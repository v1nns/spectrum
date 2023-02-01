#include "audio/driver/ffmpeg.h"

#include <iomanip>

#include "util/logger.h"

namespace driver {

static void log_callback(void *ptr, int level, const char *fmt, va_list vargs) {
  va_list ap_copy;
  va_copy(ap_copy, vargs);

  size_t len = vsnprintf(0, 0, fmt, ap_copy);

  std::string message("", len + 1);  // need space for NUL
  vsnprintf(&message[0], len + 1, fmt, vargs);

  message.resize(len - 1);  // remove the NUL + \n
  LOG("[LOG_CALLBACK] ", message);
}

FFmpeg::FFmpeg()
    : input_stream_{},
      decoder_{},
      stream_index_{},
      volume_{1.f},
      filter_graph_{},
      buffersrc_ctx_{},
      buffersink_ctx_{},
      audio_filters_{} {
#if LIBAVUTIL_VERSION_MAJOR > 56
  ch_layout_.reset(new AVChannelLayout{});
  // Set output channel layout to stereo (2-channel)
  av_channel_layout_default(ch_layout_.get(), 2);
#endif

  // TODO: Control this with a parameter
  //   av_log_set_level(AV_LOG_QUIET);

  av_log_set_level(AV_LOG_ERROR);
  av_log_set_callback(log_callback);
}

/* ********************************************************************************************** */

error::Code FFmpeg::OpenInputStream(const std::string &filepath) {
  LOG("Open input stream from filepath=", std::quoted(filepath));
  AVFormatContext *ptr = nullptr;

  if (avformat_open_input(&ptr, filepath.c_str(), nullptr, nullptr) < 0) {
    ERROR("Cannot open input stream");
    return error::kFileNotSupported;
  }

  input_stream_.reset(std::move(ptr));

  if (avformat_find_stream_info(input_stream_.get(), nullptr) < 0) {
    ERROR("Cannot find stream info about opened input");
    return error::kFileNotSupported;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::ConfigureDecoder() {
  LOG("Configure audio decoder for opened input stream");
  const AVCodec *codec = nullptr;
  AVCodecParameters *parameters = nullptr;

  // select the audio stream
  stream_index_ = av_find_best_stream(input_stream_.get(), AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);

  if (stream_index_ < 0 || !codec) {
    ERROR("Cannot find audio decoder to specified file");
    return error::kFileNotSupported;
  }

  parameters = input_stream_->streams[stream_index_]->codecpar;
  decoder_ = CodecContext{avcodec_alloc_context3(codec)};

  if (!decoder_ || avcodec_parameters_to_context(decoder_.get(), parameters) < 0) {
    ERROR("Cannot create audio decoder");
    return error::kUnknownError;
  }

#if LIBAVUTIL_VERSION_MAJOR > 56
  // Force to use stereo as channel layout
  if (!codec->ch_layouts) {
    av_channel_layout_copy(&decoder_->ch_layout, ch_layout_.get());
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
    ERROR("Cannot initialize audio decoder");
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::ConfigureFilters() {
  LOG("Configure filter chain");

  // Create a new filtergraph, which will contain all the filters
  filter_graph_.reset(avfilter_graph_alloc());
  if (!filter_graph_) {
    ERROR("Unable to create filter graph");
    return error::kUnknownError;
  }

  // Create and configure abuffer filter
  error::Code result = CreateFilterAbufferSrc();
  if (result != error::kSuccess) return result;

  // Create and configure volume filter
  result = CreateFilterVolume();
  if (result != error::kSuccess) return result;

  // Create and configure all equalizer filters
  for (const auto &entry : audio_filters_) {
    result = CreateFilterEqualizer(entry.first, entry.second);
    if (result != error::kSuccess) return result;
  }

  // Create and configure aformat filter
  result = CreateFilterAformat();
  if (result != error::kSuccess) return result;

  // Create and configure abuffersink filter
  result = CreateFilterAbufferSink();
  if (result != error::kSuccess) return result;

  // Link all filters in a linear chain
  result = ConnectFilters();

  return result;
}

/* ********************************************************************************************** */

error::Code FFmpeg::CreateFilterAbufferSrc() {
  LOG("Create abuffer filter");

  // Find abuffer filter
  const AVFilter *abuffersrc = avfilter_get_by_name("abuffer");

  if (!abuffersrc) {
    ERROR("Cannot find the abuffer filter");
    return error::kUnknownError;
  }

  // Create an instance of abuffer filter, it will be used for feeding data into the filter graph
  buffersrc_ctx_.reset(avfilter_graph_alloc_filter(filter_graph_.get(), abuffersrc, "src"));

  if (!buffersrc_ctx_) {
    ERROR("Cannot allocate the buffersrc instance");
    return error::kUnknownError;
  }

  char ch_layout[64];

  // Set filter options through the AVOptions API
  av_channel_layout_describe(&decoder_->ch_layout, ch_layout, sizeof(ch_layout));
  av_opt_set(buffersrc_ctx_.get(), "channel_layout", ch_layout, AV_OPT_SEARCH_CHILDREN);
  av_opt_set(buffersrc_ctx_.get(), "sample_fmt", av_get_sample_fmt_name(decoder_->sample_fmt),
             AV_OPT_SEARCH_CHILDREN);
  av_opt_set_q(buffersrc_ctx_.get(), "time_base", (AVRational){1, decoder_->sample_rate},
               AV_OPT_SEARCH_CHILDREN);
  av_opt_set_int(buffersrc_ctx_.get(), "sample_rate", decoder_->sample_rate,
                 AV_OPT_SEARCH_CHILDREN);

  // Initialize filter
  if (avfilter_init_str(buffersrc_ctx_.get(), nullptr) < 0) {
    ERROR("Cannot initialize the abuffer filter");
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::CreateFilterVolume() {
  LOG("Create volume filter");

  // Find volume filter
  const AVFilter *volume = avfilter_get_by_name(kFilterVolume);

  if (!volume) {
    ERROR("Cannot find the volume filter");
    return error::kUnknownError;
  }

  // Create an instance of volume filter
  AVFilterContext *volume_ctx =
      avfilter_graph_alloc_filter(filter_graph_.get(), volume, kFilterVolume);

  if (!volume_ctx) {
    ERROR("Cannot allocate the volume instance");
    return error::kUnknownError;
  }

  // Set filter option
  av_opt_set(volume_ctx, "volume", model::to_string(volume_).c_str(), AV_OPT_SEARCH_CHILDREN);

  // Initialize filter
  if (avfilter_init_str(volume_ctx, nullptr) < 0) {
    ERROR("Cannot initialize the volume filter");
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::CreateFilterAformat() {
  LOG("Create aformat filter");

  // Find aformat filter
  const AVFilter *aformat = avfilter_get_by_name(kFilterAformat);

  if (!aformat) {
    ERROR("Cannot find the aformat filter");
    return error::kUnknownError;
  }

  // Create an instance of aformat filter, it ensures that the output is of the format we want
  AVFilterContext *aformat_ctx =
      avfilter_graph_alloc_filter(filter_graph_.get(), aformat, kFilterAformat);

  if (!aformat_ctx) {
    ERROR("Cannot allocate the aformat instance");
    return error::kUnknownError;
  }

  char ch_layout[64];

  // Set filter options
  av_channel_layout_describe(ch_layout_.get(), ch_layout, sizeof(ch_layout));
  av_opt_set(aformat_ctx, "channel_layout", ch_layout, AV_OPT_SEARCH_CHILDREN);
  av_opt_set(aformat_ctx, "sample_fmts", av_get_sample_fmt_name(AV_SAMPLE_FMT_S16),
             AV_OPT_SEARCH_CHILDREN);
  av_opt_set_int(aformat_ctx, "sample_rate", kSampleRate, AV_OPT_SEARCH_CHILDREN);

  // Initialize filter
  if (avfilter_init_str(aformat_ctx, nullptr) < 0) {
    ERROR("Cannot initialize the aformat filter");
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::CreateFilterAbufferSink() {
  LOG("Create abuffersink filter");

  // Find abuffersink filter
  const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");

  if (!abuffersink) {
    ERROR("Cannot find the abuffersink filter");
    return error::kUnknownError;
  }

  // Create an instance of abuffersink filter, it will be used to get filtered data out of the graph
  buffersink_ctx_.reset(avfilter_graph_alloc_filter(filter_graph_.get(), abuffersink, "sink"));

  if (!buffersink_ctx_) {
    ERROR("Cannot allocate the abuffersink instance");
    return error::kUnknownError;
  }

  // This filter takes no options
  if (avfilter_init_str(buffersink_ctx_.get(), nullptr) < 0) {
    ERROR("Cannot initialize the abuffersink instance");
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::CreateFilterEqualizer(const std::string &name,
                                          const model::AudioFilter &filter) {
  LOG("Create new equalizer filter");

  // Find equalizer filter
  const AVFilter *equalizer = avfilter_get_by_name("equalizer");

  if (!equalizer) {
    ERROR("Cannot find the equalizer filter");
    return error::kUnknownError;
  }

  // Create an instance of equalizer filter
  AVFilterContext *equalizer_ctx =
      avfilter_graph_alloc_filter(filter_graph_.get(), equalizer, name.c_str());

  if (!equalizer_ctx) {
    ERROR("Cannot allocate the equalizer instance");
    return error::kUnknownError;
  }

  // Set filter options
  av_opt_set_double(equalizer_ctx, "frequency", filter.frequency, AV_OPT_SEARCH_CHILDREN);
  av_opt_set(equalizer_ctx, "width_type", "q", AV_OPT_SEARCH_CHILDREN);
  av_opt_set_double(equalizer_ctx, "width", filter.Q, AV_OPT_SEARCH_CHILDREN);
  av_opt_set_double(equalizer_ctx, "gain", filter.gain, AV_OPT_SEARCH_CHILDREN);
  av_opt_set(equalizer_ctx, "transform", "dii", AV_OPT_SEARCH_CHILDREN);
  av_opt_set(equalizer_ctx, "precision", "auto", AV_OPT_SEARCH_CHILDREN);

  // Initialize filter
  if (avfilter_init_str(equalizer_ctx, nullptr) < 0) {
    ERROR("Cannot initialize the equalizer filter");
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::ConnectFilters() {
  LOG("Connect all filters in a linear chain");
  AVFilterContext *volume_ctx = avfilter_graph_get_filter(filter_graph_.get(), kFilterVolume);
  AVFilterContext *aformat_ctx = avfilter_graph_get_filter(filter_graph_.get(), kFilterAformat);

  // Connect the filters, in this case the filters just form a linear chain
  int err = avfilter_link(buffersrc_ctx_.get(), 0, volume_ctx, 0);

  if (audio_filters_.size() > 0) {
    AVFilterContext *previous_ctx = volume_ctx;

    for (const auto &entry : audio_filters_) {
      AVFilterContext *filter_ctx =
          avfilter_graph_get_filter(filter_graph_.get(), entry.first.c_str());

      if (err >= 0) err = avfilter_link(previous_ctx, 0, filter_ctx, 0);
      previous_ctx = filter_ctx;
    }

    if (err >= 0) err = avfilter_link(previous_ctx, 0, aformat_ctx, 0);

  } else {
    if (err >= 0) err = avfilter_link(volume_ctx, 0, aformat_ctx, 0);
  }

  if (err >= 0) err = avfilter_link(aformat_ctx, 0, buffersink_ctx_.get(), 0);
  if (err < 0) {
    ERROR("Error connecting filters");
    return error::kUnknownError;
  }

  // Configure the graph
  err = avfilter_graph_config(filter_graph_.get(), nullptr);
  if (err < 0) {
    ERROR("Error configuring the filter graph");
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

void FFmpeg::FillAudioInformation(model::Song *audio_info) {
  LOG("Fill song structure with audio information");

  // use this to get all metadata associated to this audio file
  //   const AVDictionaryEntry *tag = nullptr;
  //   while ((tag = av_dict_get(input_stream_->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
  //     LOG("key=", tag->key," value=", tag->value);
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

  result = ConfigureFilters();
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

  // Allocate internal decoding structure
  DecodingData data{
      .time_base = input_stream_->streams[stream_index_]->time_base,
      .position = 0,
      .packet{av_packet_alloc()},
      .frame_decoded{av_frame_alloc()},
      .frame_filtered{av_frame_alloc()},
      .result = error::kSuccess,
      .keep_playing = true,
  };

  if (!data.packet || !data.frame_decoded || !data.frame_filtered) {
    ERROR("Cannot allocate internal structures to decode song");
    return error::kUnknownError;
  }

  AVPacket *packet = data.packet.get();
  AVFrame *frame = data.frame_decoded.get();

  // Read audio raw data from input stream
  while (av_read_frame(input_stream_.get(), packet) >= 0 && data.KeepDecoding()) {
    // If not the same stream index, we should not try to decode it
    if (packet->stream_index != stream_index_) {
      av_packet_unref(packet);
      continue;
    }

    // Send packet to decoder
    if (avcodec_send_packet(decoder_.get(), packet) < 0) {
      ERROR("Cannot decode song");
      return error::kDecodeFileFailed;
    }

    // Receive frames from decoder
    while (avcodec_receive_frame(decoder_.get(), frame) >= 0 && data.KeepDecoding()) {
      // Note that AVPacket.pts is in AVStream.time_base units, not AVCodecContext.time_base units
      data.position = packet->pts / data.time_base.den;

      // Pass decoded frame to be processed by filtergraph. And in case of error while processing
      // frame, data.KeepDecoding will return false, so do not worry about it
      ProcessFrame(samples, callback, data);

      data.ClearFrames();
    }

    data.ClearPacket();
  }

  return data.result;
}

/* ********************************************************************************************** */

void FFmpeg::ClearCache() {
  LOG("Clear internal cache");
  input_stream_.reset();
  decoder_.reset();
  buffersrc_ctx_.reset();
  buffersink_ctx_.reset();
  audio_filters_.clear();

  filter_graph_.reset();

  stream_index_ = 0;
}

/* ********************************************************************************************** */

error::Code FFmpeg::SetVolume(model::Volume value) {
  LOG("Set volume to new value=", value);
  volume_ = value;

  // Filtergraph is not created yet and there is no need to do anything further
  if (!filter_graph_) return error::kSuccess;

  // Otherwise, it means that some music is playing, so we gotta update the running filtergraph
  LOG("Found volume filter, update value");
  std::string volume = model::to_string(volume_);
  std::string response(kResponseSize, ' ');

  // Set filter option
  if (avfilter_graph_send_command(filter_graph_.get(), kFilterVolume, "volume", volume.c_str(),
                                  response.data(), kResponseSize, AV_OPT_SEARCH_CHILDREN)) {
    ERROR("Cannot set new value for volume filter, returned:", response);
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

model::Volume FFmpeg::GetVolume() const { return volume_; }

/* ********************************************************************************************** */

error::Code FFmpeg::UpdateFilters(const std::vector<model::AudioFilter> &filters) {
  LOG("Update audio filters in the internal structure");

  for (const auto &filter : filters) {
    if (filter.frequency == 0 || filter.Q == 0) {
      ERROR("Zeroed filter is not permitted");
      return error::kUnknownError;
    }

    std::string name{filter.GetName()};
    audio_filters_[name] = filter;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

void FFmpeg::ProcessFrame(int samples, AudioCallback callback, DecodingData &data) {
  // Get source and sink
  AVFilterContext *source = buffersrc_ctx_.get();
  AVFilterContext *sink = buffersink_ctx_.get();

  // Get allocated pointer for frames (decoded and filtered)
  AVFrame *decoded = data.frame_decoded.get();
  AVFrame *filtered = data.frame_filtered.get();

  // Push the audio data from decoded frame into the filtergraph
  if (av_buffersrc_add_frame_flags(source, decoded, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
    ERROR("Cannot feed audio filtergraph");
    data.result = error::kDecodeFileFailed;
    return;
  }

  int result;
  bool seek_frame = false;
  int64_t old_position = data.position;

  // Pull filtered audio from the filtergraph
  while ((result = av_buffersink_get_samples(sink, filtered, samples)) >= 0) {
    // Send filtered audio data to Player
    data.keep_playing = callback((void *)filtered->data[0], filtered->nb_samples, data.position);

    // Clear frame from filtergraph
    av_frame_unref(filtered);

    // Updated song cursor position
    if (data.position != old_position) {
      seek_frame = true;
      break;
    }
  }

  // Check if got some critical error
  if (result < 0 && result != AVERROR(EAGAIN) && result != AVERROR_EOF) {
    ERROR("Cannot pull data from audio filtergraph");
    data.result = error::kDecodeFileFailed;
  }

  // Seek new position in song
  if (data.KeepDecoding() && seek_frame) {
    // Clear internal buffers
    data.ClearFrames();
    avcodec_flush_buffers(decoder_.get());

    // Recalculate new position
    int64_t target = av_rescale_q(data.position * AV_TIME_BASE, AV_TIME_BASE_Q, data.time_base);

    // Seek new frame
    if (av_seek_frame(input_stream_.get(), stream_index_, target, AVSEEK_FLAG_BACKWARD) < 0) {
      ERROR("Cannot seek frame in song");
      data.result = error::kSeekFrameFailed;
    }

    seek_frame = false;
  }
}

}  // namespace driver
