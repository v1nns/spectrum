#include "audio/driver/ffmpeg.h"

#include <iomanip>

#include "util/logger.h"

namespace driver {

FFmpeg::FFmpeg() : input_stream_{}, decoder_{}, resampler_{}, stream_index_{}, volume_{1.f} {
#if LIBAVUTIL_VERSION_MAJOR > 56
  ch_layout_.reset(new AVChannelLayout{});
  // Set output channel layout to stereo (2-channel)
  av_channel_layout_default(ch_layout_.get(), 2);
#endif

  // TODO: Control this with a parameter
  av_log_set_level(AV_LOG_QUIET);
}

/* ********************************************************************************************** */

error::Code FFmpeg::OpenInputStream(const std::string &filepath) {
  LOG("Open input stream from filepath=", std::quoted(filepath));
  AVFormatContext *ptr = nullptr;

  if (avformat_open_input(&ptr, filepath.c_str(), nullptr, nullptr) < 0) {
    ERROR("Could not open input stream");
    return error::kFileNotSupported;
  }

  input_stream_.reset(std::move(ptr));

  if (avformat_find_stream_info(input_stream_.get(), nullptr) < 0) {
    ERROR("Could not find stream info about opened input");
    return error::kFileNotSupported;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::ConfigureDecoder() {
  LOG("Configure audio decoder for opened input stream");
  const AVCodec *codec = nullptr;
  AVCodecParameters *parameters = nullptr;

  for (int i = 0; i < input_stream_->nb_streams; i++) {
    parameters = input_stream_->streams[i]->codecpar;
    if (parameters->codec_type == AVMEDIA_TYPE_AUDIO) {
      LOG("Found audio stream from input with index=", i);
      stream_index_ = i;
      codec = avcodec_find_decoder(parameters->codec_id);

      break;
    }
  }

  if (!codec) {
    ERROR("Could not find audio decoder to specified file");
    return error::kFileNotSupported;
  }

  decoder_ = CodecContext{avcodec_alloc_context3(codec)};

  if (!decoder_ || avcodec_parameters_to_context(decoder_.get(), parameters) < 0) {
    ERROR("Could not create audio decoder");
    return error::kUnknownError;
  }

#if LIBAVUTIL_VERSION_MAJOR > 56
  // Force to use stereo as channel layout
  if (!codec->ch_layouts) {
    auto dummy = (AVChannelLayout)AV_CHANNEL_LAYOUT_STEREO;
    av_channel_layout_copy(&decoder_->ch_layout, &dummy);
  }
#else
  if (decoder_->channel_layout == 0) {
    decoder_->channel_layout = AV_CH_LAYOUT_STEREO;
    // switch (decoder_->channels) {
    //   case 1:
    //     decoder_->channel_layout = AV_CH_LAYOUT_MONO;
    //     break;
    //   case 2:
    //     decoder_->channel_layout = AV_CH_LAYOUT_STEREO;
    //     break;
    //   default:
    //     return error::kUnknownError;
    // }
  }
#endif

  if (avcodec_open2(decoder_.get(), codec, nullptr) < 0) {
    ERROR("Could not initialize audio decoder");
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::ConfigureResampler() {
  LOG("Configure audio resampler");

#if LIBAVUTIL_VERSION_MAJOR > 56
  SwrContext *dummy{};
  swr_alloc_set_opts2(&dummy, ch_layout_.get(), kSampleFormat, kSampleRate, &decoder_->ch_layout,
                      decoder_->sample_fmt, decoder_->sample_rate, 0, nullptr);

  resampler_.reset(std::move(dummy));
#else
  resampler_ = CustomSwrContext{swr_alloc_set_opts(
      nullptr, kChannelLayout, kSampleFormat, kSampleRate, decoder_->channel_layout,
      decoder_->sample_fmt, decoder_->sample_rate, 0, nullptr)};
#endif

  if (!resampler_ || swr_init(resampler_.get()) < 0) {
    ERROR("Could not initialize audio resampler");
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::ConfigureVolume() {
  LOG("Configure filter to control audio volume");
  // TODO: implement

  return error::kSuccess;
}

/* ********************************************************************************************** */

void FFmpeg::FillAudioInformation(model::Song *audio_info) {
  LOG("Fill song structure with audio information");

  // use this to get all metadata associated to this audio file
  //   const AVDictionaryEntry *tag = nullptr;
  //   while ((tag = av_dict_get(input_stream_->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
  //     printf("%s=%s\n", tag->key, tag->value);
  const AVDictionaryEntry *tag = nullptr;

  // Get track name
  tag = av_dict_get(input_stream_->metadata, "title", tag, AV_DICT_IGNORE_SUFFIX);
  if (tag) audio_info->title = std::string{tag->value};

  // Get artist name
  tag = av_dict_get(input_stream_->metadata, "artist", tag, AV_DICT_IGNORE_SUFFIX);
  if (tag) audio_info->artist = std::string{tag->value};

  const AVCodecParameters *audio_stream = input_stream_->streams[stream_index_]->codecpar;

#if LIBAVUTIL_VERSION_MAJOR > 56
  audio_info->num_channels = (uint16_t)audio_stream->ch_layout.nb_channels;
#else
  audio_info->num_channels = (uint16_t)audio_stream->channels;
#endif
  audio_info->sample_rate = (uint32_t)audio_stream->sample_rate;
  audio_info->bit_rate = (uint32_t)audio_stream->bit_rate;
  audio_info->bit_depth = (uint32_t)sample_fmt_info[audio_stream->format].bits;
  audio_info->duration = (uint32_t)(input_stream_->duration / AV_TIME_BASE);
}

/* ********************************************************************************************** */

error::Code FFmpeg::OpenFile(model::Song *audio_info) {
  LOG("Open file and try to decode as song");
  auto clean_up_and_return = [&](error::Code error_code) {
    ClearCache();
    return error_code;
  };

  error::Code result = OpenInputStream(audio_info->filepath);
  if (result != error::kSuccess) return clean_up_and_return(result);

  result = ConfigureDecoder();
  if (result != error::kSuccess) return clean_up_and_return(result);

  result = ConfigureResampler();
  if (result != error::kSuccess) return clean_up_and_return(result);

  // At this point, we can get detailed information about the song
  FillAudioInformation(audio_info);

  return result;
}

/* ********************************************************************************************** */

error::Code FFmpeg::Decode(int samples, AudioCallback callback) {
  LOG("Decode song using maximum sample=", samples);

#if LIBAVUTIL_VERSION_MAJOR > 56
  int channels = decoder_->ch_layout.nb_channels;
#else
  int channels = decoder_->channels;
#endif

  int max_buffer_size =
      av_samples_get_buffer_size(nullptr, channels, samples, decoder_->sample_fmt, 1);

  Packet packet = Packet{av_packet_alloc()};
  Frame frame = Frame{av_frame_alloc()};

  if (!packet || !frame) {
    ERROR("Could not allocate internal structures to decode song");
    return error::kUnknownError;
  }

  DataBuffer allocated_buffer = DataBuffer{(uint8_t *)av_malloc(max_buffer_size)};
  uint8_t *buffer = allocated_buffer.get();

  // Control flags
  bool continue_decoding = true;

  // Flag to seek frame based on value informed by callback (in this case, set by player)
  bool seek_frame = false;
  int64_t position = 0;

  while (av_read_frame(input_stream_.get(), packet.get()) >= 0 && continue_decoding) {
    if (packet->stream_index != stream_index_) {
      av_packet_unref(packet.get());
      continue;
    }

    if (avcodec_send_packet(decoder_.get(), packet.get()) < 0) {
      ERROR("Could not resample song");
      return error::kDecodeFileFailed;
    }

    while (avcodec_receive_frame(decoder_.get(), frame.get()) >= 0 && continue_decoding) {
      // Note that AVPacket.pts is in AVStream.time_base units, not AVCodecContext.time_base units
      position = packet->pts / input_stream_->streams[stream_index_]->time_base.den;
      int64_t old_position = position;

      int samples_size = swr_convert(resampler_.get(), &buffer, samples,
                                     (const uint8_t **)(frame->data), frame->nb_samples);

      while (samples_size > 0 && continue_decoding) {
        continue_decoding = callback(buffer, max_buffer_size, samples_size, position);
        samples_size = swr_convert(resampler_.get(), &buffer, samples, nullptr, 0);

        if (position != old_position) seek_frame = true;
      }

      if (seek_frame) {
        avcodec_flush_buffers(decoder_.get());
        int64_t target = av_rescale_q(position * AV_TIME_BASE, AV_TIME_BASE_Q,
                                      input_stream_->streams[stream_index_]->time_base);

        if (av_seek_frame(input_stream_.get(), stream_index_, target, AVSEEK_FLAG_BACKWARD) < 0) {
          ERROR("Could not seek frame in song");
          return error::kSeekFrameFailed;
        }

        seek_frame = false;
      }

      av_frame_unref(frame.get());
    }

    av_packet_unref(packet.get());
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

void FFmpeg::ClearCache() {
  LOG("Clear internal cache");
  input_stream_.reset();
  decoder_.reset();
  resampler_.reset();

  stream_index_ = 0;
}

/* ********************************************************************************************** */

error::Code FFmpeg::SetVolume(model::Volume value) {
  volume_ = value;
  return error::kSuccess;
}

/* ********************************************************************************************** */

model::Volume FFmpeg::GetVolume() const { return volume_; }

}  // namespace driver
