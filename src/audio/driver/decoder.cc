#include "audio/driver/decoder.h"

namespace driver {

Decoder::Decoder() : input_stream_{}, decoder_{}, resampler_{}, stream_index_{} {}

/* ********************************************************************************************** */

error::Code Decoder::OpenInputStream(const std::string &filepath) {
  AVFormatContext *ptr = nullptr;

  if (avformat_open_input(&ptr, filepath.c_str(), nullptr, nullptr) < 0) {
    return error::kFileNotSupported;
  }

  input_stream_.reset(std::move(ptr));

  if (avformat_find_stream_info(input_stream_.get(), nullptr) < 0) {
    error::kFileNotSupported;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Decoder::ConfigureDecoder() {
  AVCodec *codec = nullptr;
  AVCodecParameters *parameters = nullptr;

  for (int i = 0; i < input_stream_->nb_streams; i++) {
    parameters = input_stream_->streams[i]->codecpar;
    if (parameters->codec_type == AVMEDIA_TYPE_AUDIO) {
      stream_index_ = i;
      codec = avcodec_find_decoder(parameters->codec_id);

      if (!codec) {
        return error::kUnknownError;
      }

      break;
    }
  }

  decoder_ = CodecContext{avcodec_alloc_context3(codec)};

  if (!decoder_ || avcodec_parameters_to_context(decoder_.get(), parameters) < 0) {
    return error::kUnknownError;
  }

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

  if (avcodec_open2(decoder_.get(), codec, nullptr) < 0) {
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code Decoder::ConfigureResampler() {
  resampler_ = CustomSwrContext{swr_alloc_set_opts(
      nullptr, kChannelLayout, kSampleFormat, kSampleRate, decoder_->channel_layout,
      decoder_->sample_fmt, decoder_->sample_rate, 0, nullptr)};

  if (!resampler_ || swr_init(resampler_.get()) < 0) {
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

void Decoder::FillAudioInformation(model::Song *audio_info) {
  //   .artist = "",
  //   .title = "",
  audio_info->num_channels = (uint16_t)decoder_->channels,
  audio_info->sample_rate = (uint32_t)decoder_->sample_rate,
  audio_info->bit_rate = (uint32_t)input_stream_->bit_rate,
  audio_info->bit_depth = (uint32_t)decoder_->bits_per_raw_sample,
  audio_info->duration = (uint32_t)(input_stream_->duration / AV_TIME_BASE);
}

/* ********************************************************************************************** */

error::Code Decoder::OpenFile(model::Song *audio_info) {
  error::Code result;

  if (result = OpenInputStream(audio_info->filepath) != error::kSuccess) {
    return result;
  }

  if (result = ConfigureDecoder() != error::kSuccess) {
    return result;
  }

  result = ConfigureResampler();

  if (result == error::kSuccess) {
    // At this point, we can get the whole information about the song
    FillAudioInformation(audio_info);
  }

  return result;
}

/* ********************************************************************************************** */

error::Code Decoder::Decode(int samples, std::function<bool(void *, int, int)> callback) {
  int max_buffer_size =
      av_samples_get_buffer_size(nullptr, decoder_->channels, samples, decoder_->sample_fmt, 1);

  Packet packet = Packet{av_packet_alloc()};
  Frame frame = Frame{av_frame_alloc()};

  if (!packet || !frame) {
    return error::kUnknownError;
  }

  DataBuffer allocated_buffer = DataBuffer{(uint8_t *)av_malloc(max_buffer_size)};
  uint8_t *buffer = allocated_buffer.get();

  bool continue_decoding = true;

  while (av_read_frame(input_stream_.get(), packet.get()) >= 0 && continue_decoding) {
    if (packet->stream_index != stream_index_) {
      continue;
    }

    if (avcodec_send_packet(decoder_.get(), packet.get()) < 0) {
      return error::kUnknownError;
    }

    while (avcodec_receive_frame(decoder_.get(), frame.get()) >= 0 && continue_decoding) {
      int out_samples = swr_convert(resampler_.get(), &buffer, samples,
                                    (const uint8_t **)(frame->data), frame->nb_samples);

      while (out_samples > 0 && continue_decoding) {
        continue_decoding = callback(buffer, max_buffer_size, out_samples);
        out_samples = swr_convert(resampler_.get(), &buffer, samples, nullptr, 0);
      }

      av_frame_unref(frame.get());
    }

    av_packet_unref(packet.get());
  }

  return error::kSuccess;
}

}  // namespace driver