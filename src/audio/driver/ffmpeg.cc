#include "audio/driver/ffmpeg.h"

#include <iomanip>
#include <iterator>

#include "util/logger.h"

namespace driver {

static void log_callback(void *ptr, int level, const char *fmt, va_list vargs) {
  va_list ap_copy;
  va_copy(ap_copy, vargs);

  size_t len = vsnprintf(0, 0, fmt, ap_copy);
  va_end(ap_copy);

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

  int result = avformat_open_input(&ptr, filepath.c_str(), nullptr, nullptr);
  if (result < 0) {
    ERROR("Cannot open input stream, error=", result);
    return error::kFileNotSupported;
  }

  input_stream_.reset(std::move(ptr));

  result = avformat_find_stream_info(input_stream_.get(), nullptr);
  if (result < 0) {
    ERROR("Cannot find stream info about opened input, error=", result);
    return error::kFileNotSupported;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::ConfigureDecoder() {
  LOG("Configure audio decoder for opened input stream");

#if LIBAVFORMAT_VERSION_MAJOR > 58
  const AVCodec *codec = nullptr;
#else
  AVCodec *codec = nullptr;
#endif

  // select the audio stream
  stream_index_ = av_find_best_stream(input_stream_.get(), AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);

  if (stream_index_ < 0 || !codec) {
    ERROR("Cannot find audio decoder to specified file");
    return error::kFileNotSupported;
  }

  AVCodecParameters *parameters = input_stream_->streams[stream_index_]->codecpar;
  decoder_ = CodecContext{avcodec_alloc_context3(codec)};

  int result = avcodec_parameters_to_context(decoder_.get(), parameters);
  if (result < 0) {
    ERROR("Cannot create audio decoderm, error=", result);
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

  result = avcodec_open2(decoder_.get(), codec, nullptr);
  if (result < 0) {
    ERROR("Cannot initialize audio decoder, error=", result);
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
  LOG("Create new equalizer filters, size=", audio_filters_.size());
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
  const AVFilter *abuffersrc = avfilter_get_by_name(kFilterAbufferSrc);

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

// Get channel layout description
#if LIBAVUTIL_VERSION_MAJOR > 56
  // Set filter options through the AVOptions API
  av_channel_layout_describe(&decoder_->ch_layout, ch_layout, sizeof(ch_layout));
#else
  // Set filter options through the AVOptions API
  av_get_channel_layout_string(ch_layout, sizeof(ch_layout), decoder_->channels,
                               decoder_->channel_layout);
#endif

  av_opt_set(buffersrc_ctx_.get(), "channel_layout", ch_layout, AV_OPT_SEARCH_CHILDREN);
  av_opt_set(buffersrc_ctx_.get(), "sample_fmt", av_get_sample_fmt_name(decoder_->sample_fmt),
             AV_OPT_SEARCH_CHILDREN);
  av_opt_set_q(buffersrc_ctx_.get(), "time_base", (AVRational){1, decoder_->sample_rate},
               AV_OPT_SEARCH_CHILDREN);
  av_opt_set_int(buffersrc_ctx_.get(), "sample_rate", decoder_->sample_rate,
                 AV_OPT_SEARCH_CHILDREN);

  // Initialize filter
  int result = avfilter_init_str(buffersrc_ctx_.get(), nullptr);
  if (result < 0) {
    ERROR("Cannot initialize the abuffer filter, error=", result);
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
  int result = avfilter_init_str(volume_ctx, nullptr);
  if (result < 0) {
    ERROR("Cannot initialize the volume filter, error=", result);
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

// Get channel layout description
#if LIBAVUTIL_VERSION_MAJOR > 56
  av_channel_layout_describe(&decoder_->ch_layout, ch_layout, sizeof(ch_layout));
#else
  av_get_channel_layout_string(ch_layout, sizeof(ch_layout), decoder_->channels,
                               decoder_->channel_layout);
#endif

  // Set filter options through the AVOptions API
  av_opt_set(aformat_ctx, "channel_layout", ch_layout, AV_OPT_SEARCH_CHILDREN);
  av_opt_set(aformat_ctx, "sample_fmts", av_get_sample_fmt_name(AV_SAMPLE_FMT_S16),
             AV_OPT_SEARCH_CHILDREN);
  av_opt_set_int(aformat_ctx, "sample_rate", kSampleRate, AV_OPT_SEARCH_CHILDREN);

  // Initialize filter
  int result = avfilter_init_str(aformat_ctx, nullptr);
  if (result < 0) {
    ERROR("Cannot initialize the aformat filter, error=", result);
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::CreateFilterAbufferSink() {
  LOG("Create abuffersink filter");

  // Find abuffersink filter
  const AVFilter *abuffersink = avfilter_get_by_name(kFilterAbufferSink);

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
  int result = avfilter_init_str(buffersink_ctx_.get(), nullptr);
  if (result < 0) {
    ERROR("Cannot initialize the abuffersink instance, error=", result);
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::CreateFilterEqualizer(const std::string &name,
                                          const model::AudioFilter &filter) {
  // Find equalizer filter
  const AVFilter *equalizer = avfilter_get_by_name(kFilterEqualizer);

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
  int result = avfilter_init_str(equalizer_ctx, nullptr);
  if (result < 0) {
    ERROR("Cannot initialize the equalizer filter (", name, "), error=", result);
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::ConnectFilters() {
  LOG("Connect all filters in a linear chain");

  // Find existing instance of filters
  AVFilterContext *volume_ctx = avfilter_graph_get_filter(filter_graph_.get(), kFilterVolume);
  AVFilterContext *aformat_ctx = avfilter_graph_get_filter(filter_graph_.get(), kFilterAformat);

  // Filters will be linked considering the ordination in this vector
  std::vector<AVFilterContext *> filters_to_link;
  filters_to_link.reserve(kDefaultFilterCount + audio_filters_.size());

  // Add both abuffer and volume filters
  filters_to_link.insert(filters_to_link.end(), {buffersrc_ctx_.get(), volume_ctx});

  // Add equalizer filters
  for (const auto &entry : audio_filters_) {
    filters_to_link.push_back(avfilter_graph_get_filter(filter_graph_.get(), entry.first.c_str()));
  }

  // Add aformat and abuffersink filters
  filters_to_link.insert(filters_to_link.end(), {aformat_ctx, buffersink_ctx_.get()});

  // Link all the filters, it will form a linear chain
  int result = 0;
  for (auto it = filters_to_link.begin(); it != filters_to_link.end() && result >= 0;) {
    auto next = std::next(it);

    if (next == filters_to_link.end()) break;

    result = avfilter_link(*it, 0, *next, 0);
    it = next;
  }

  if (result < 0) {
    ERROR("Cannot connect filters in the linear chain, error=", result);
    return error::kUnknownError;
  }

  // Configure the graph
  result = avfilter_graph_config(filter_graph_.get(), nullptr);
  if (result < 0) {
    ERROR("Cannot configure the filter graph for equalization, error=", result);
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

void FFmpeg::FillAudioInformation(model::Song &audio_info) {
  LOG("Fill song structure with audio information");

  // use this to get all metadata associated to this audio file
  //   const AVDictionaryEntry *tag = nullptr;
  //   while ((tag = av_dict_get(input_stream_->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
  //     LOG("key=", tag->key," value=", tag->value);
  const AVDictionaryEntry *tag = nullptr;

  // Get track name
  tag = av_dict_get(input_stream_->metadata, "title", tag, AV_DICT_IGNORE_SUFFIX);
  if (tag) audio_info.title = std::string{tag->value};

  // Get artist name
  tag = av_dict_get(input_stream_->metadata, "artist", tag, AV_DICT_IGNORE_SUFFIX);
  if (tag) audio_info.artist = std::string{tag->value};

  const AVCodecParameters *audio_stream = input_stream_->streams[stream_index_]->codecpar;

#if LIBAVUTIL_VERSION_MAJOR > 56
  audio_info.num_channels = (uint16_t)audio_stream->ch_layout.nb_channels;
#else
  audio_info.num_channels = (uint16_t)audio_stream->channels;
#endif
  audio_info.sample_rate = (uint32_t)audio_stream->sample_rate;
  audio_info.bit_rate = (uint32_t)audio_stream->bit_rate;
  audio_info.bit_depth = (uint32_t)sample_fmt_info[audio_stream->format].bits;
  audio_info.duration = (uint32_t)(input_stream_->duration / AV_TIME_BASE);
}

/* ********************************************************************************************** */

error::Code FFmpeg::OpenFile(model::Song &audio_info) {
  LOG("Open file and try to decode as song");
  auto clean_up_and_return = [&](error::Code error_code) {
    ClearCache();
    return error_code;
  };

  error::Code result = OpenInputStream(audio_info.filepath);
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

  // Allocate internal decoding structure
  shared_context_ = DecodingData{
      .time_base = input_stream_->streams[stream_index_]->time_base,
      .position = 0,
      .packet{Packet(av_packet_alloc())},
      .frame_decoded{Frame(av_frame_alloc())},
      .frame_filtered{Frame(av_frame_alloc())},
      .err_code = error::kSuccess,
      .keep_playing = true,
      .reset_filters = false,
  };

  if (!shared_context_.CheckAllocations()) {
    ERROR("Cannot allocate internal structures to decode song");
    return error::kUnknownError;
  }

  AVPacket *packet = shared_context_.packet.get();
  AVFrame *frame = shared_context_.frame_decoded.get();

  // Read audio raw data from input stream
  while (av_read_frame(input_stream_.get(), packet) >= 0 && shared_context_.KeepDecoding()) {
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
    while (avcodec_receive_frame(decoder_.get(), frame) >= 0 && shared_context_.KeepDecoding()) {
      // Note that AVPacket.pts is in AVStream.time_base units, not AVCodecContext.time_base units
      shared_context_.position = packet->pts / shared_context_.time_base.den;

      // UI sent event to update audio filters with new parameters, so it is necessary to reset it
      if (shared_context_.reset_filters) {
        shared_context_.err_code = ConfigureFilters();
        shared_context_.reset_filters = false;
      }

      // Pass decoded frame to be processed by filtergraph. And in case of error while processing
      // frame, shared_context_.KeepDecoding() will return false, so do not worry about it
      ProcessFrame(samples, callback);

      shared_context_.ClearFrames();
    }

    shared_context_.ClearPacket();
  }

  return shared_context_.err_code;
}

/* ********************************************************************************************** */

void FFmpeg::ClearCache() {
  LOG("Clear internal cache");
  // decoding
  input_stream_.reset();
  decoder_.reset();
  stream_index_ = 0;

  // filters
  buffersrc_ctx_.reset();
  buffersink_ctx_.reset();

  // custom data for audio filters
  audio_filters_.clear();

  // clear internal structure used for sharing context
  shared_context_ = DecodingData{};

  // main structure to handle all created filters
  filter_graph_.reset();
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
    ERROR("Cannot set new value for volume filter, error=", response);
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

model::Volume FFmpeg::GetVolume() const { return volume_; }

/* ********************************************************************************************** */

error::Code FFmpeg::UpdateFilters(const std::vector<model::AudioFilter> &filters) {
  LOG("Update audio filters in the internal structure");

  // Clear internal structure
  audio_filters_.clear();

  for (const auto &filter : filters) {
    if (filter.frequency == 0 || filter.Q == 0) {
      ERROR("Zeroed filter is not permitted");
      return error::kUnknownError;
    }

    std::string name{filter.GetName()};
    audio_filters_[name] = filter;
  }

  // In case that music is playing, must reset filter graph
  if (filter_graph_) shared_context_.reset_filters = true;

  return error::kSuccess;
}

/* ********************************************************************************************** */

void FFmpeg::ProcessFrame(int samples, AudioCallback callback) {
  // Get source and sink
  AVFilterContext *source = buffersrc_ctx_.get();
  AVFilterContext *sink = buffersink_ctx_.get();

  // Get allocated pointer for frames (decoded and filtered)
  AVFrame *decoded = shared_context_.frame_decoded.get();
  AVFrame *filtered = shared_context_.frame_filtered.get();

  // Push the audio data from decoded frame into the filtergraph
  if (av_buffersrc_add_frame_flags(source, decoded, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
    ERROR("Cannot feed audio filtergraph");
    shared_context_.err_code = error::kDecodeFileFailed;
    return;
  }

  int result;
  bool seek_frame = false;
  int64_t old_position = shared_context_.position;

  // Pull filtered audio from the filtergraph
  while ((result = av_buffersink_get_samples(sink, filtered, samples)) >= 0 &&
         shared_context_.KeepDecoding()) {
    // Send filtered audio data to Player
    shared_context_.keep_playing = callback(static_cast<void *>(filtered->data[0]),
                                            filtered->nb_samples, shared_context_.position);

    // Clear frame from filtergraph
    av_frame_unref(filtered);

    // Updated song cursor position
    if (shared_context_.position != old_position) {
      seek_frame = true;
      break;
    }

    // In case of EQ update
    if (shared_context_.reset_filters) {
      break;
    }
  }

  // Check if got some critical error
  if (result < 0 && result != AVERROR(EAGAIN) && result != AVERROR_EOF) {
    ERROR("Cannot pull data from audio filtergraph, error=", result);
    shared_context_.err_code = error::kDecodeFileFailed;
  }

  // Seek new position in song
  if (shared_context_.KeepDecoding() && seek_frame) {
    // Clear internal buffers
    shared_context_.ClearFrames();
    avcodec_flush_buffers(decoder_.get());
    seek_frame = false;

    // Recalculate new position
    int64_t target = av_rescale_q(shared_context_.position * AV_TIME_BASE, AV_TIME_BASE_Q,
                                  shared_context_.time_base);

    // Seek new frame
    if (av_seek_frame(input_stream_.get(), stream_index_, target, AVSEEK_FLAG_BACKWARD) < 0) {
      ERROR("Cannot seek frame in song");
      shared_context_.err_code = error::kSeekFrameFailed;
    }
  }
}

}  // namespace driver
