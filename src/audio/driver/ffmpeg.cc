#include "audio/driver/ffmpeg.h"

#include <libavutil/error.h>

#include <iomanip>
#include <iterator>

#include "util/logger.h"

namespace driver {

static void log_callback(void *, int level, const char *fmt, va_list vargs) {
  va_list ap_copy;
  va_copy(ap_copy, vargs);

  size_t len = vsnprintf(0, 0, fmt, ap_copy);
  va_end(ap_copy);

  std::string message("", len + 1);  // need space for NUL
  vsnprintf(&message[0], len + 1, fmt, vargs);

  message.resize(len - 1);  // remove the NUL + \n
  LOG("[LOG_CALLBACK] LEVEL:", level, " MESSAGE:", message);
}

/* ********************************************************************************************** */

FFmpeg::FFmpeg(bool verbose) {
  LOG("Initialize FFmpeg with verbose logging=", verbose);

#if LIBAVUTIL_VERSION_MAJOR > 56
  ch_layout_.reset(new AVChannelLayout{});
  // Set output channel layout to stereo (2-channel)
  av_channel_layout_default(ch_layout_.get(), 2);
#endif

  if (verbose) {
    av_log_set_level(AV_LOG_WARNING);
    av_log_set_callback(log_callback);
  } else {
    av_log_set_level(AV_LOG_QUIET);
  }
}

/* ********************************************************************************************** */

bool FFmpeg::ContainsAudioStream(const util::File &file) {
  LOG("Check for audio stream on file=", std::quoted(file.string()));
  AVFormatContext *ptr = nullptr;

  // Open input stream from given file
  int result = avformat_open_input(&ptr, file.c_str(), nullptr, nullptr);
  if (result < 0) {
    ERROR("Cannot open file as input stream, error=", av_err2str(result));
    return false;
  }

  // Get stream information from parsed input
  FormatContext input_stream(std::move(ptr));
  result = avformat_find_stream_info(input_stream.get(), nullptr);

  if (result < 0) {
    ERROR("Cannot find stream information in the opened input, error=", av_err2str(result));
    return false;
  }

#if LIBAVFORMAT_VERSION_MAJOR > 58
  const AVCodec *codec = nullptr;
#else
  AVCodec *codec = nullptr;
#endif

  // Check for available audio stream
  if (int stream_index =
          av_find_best_stream(input_stream.get(), AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
      stream_index < 0 || !codec) {
    ERROR("Cannot find audio stream in the specified file");
    return false;
  }

  return true;
}

/* ********************************************************************************************** */

error::Code FFmpeg::OpenInputStream(const model::Song &audio_info) {
  AVFormatContext *ptr = nullptr;
  AVDictionary *options = nullptr;

  std::string url;

  if (audio_info.stream_info.has_value()) {
    LOG("Song contains streaming information, attempt to decode as audio stream");

    std::string http_header;
    for (const auto &[key, value] : audio_info.stream_info->http_header) {
      http_header += key + ":" + value + ";";
    }

    // Set up HTTP header and a few parameters for reconnection (to avoid any network-related issue)
    av_dict_set(&options, "headers", http_header.c_str(), 0);
    av_dict_set(&options, "reconnect", "1", 0);
    av_dict_set(&options, "reconnect_streamed", "1", 0);
    av_dict_set(&options, "reconnect_delay_max", "10", 0);

    LOG("Open input stream from url=", std::quoted(audio_info.stream_info->base_url));
    url = audio_info.stream_info->streaming_url;
  } else if (!audio_info.filepath.empty()) {
    LOG("Open input stream from filepath=", std::quoted(audio_info.filepath.string()));
    url = audio_info.filepath;
  }

  int result = avformat_open_input(&ptr, url.c_str(), nullptr, &options);
  if (options) av_dict_free(&options);
  if (result < 0) {
    ERROR("Cannot open input stream, error=", av_err2str(result));
    return error::kFileNotSupported;
  }

  input_stream_.reset(std::move(ptr));

  result = avformat_find_stream_info(input_stream_.get(), nullptr);
  if (result < 0) {
    ERROR("Cannot find stream info about opened input, error=", av_err2str(result));
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

  const AVCodecParameters *parameters = input_stream_->streams[stream_index_]->codecpar;
  decoder_ = CodecContext{avcodec_alloc_context3(codec)};

  int result = avcodec_parameters_to_context(decoder_.get(), parameters);
  if (result < 0) {
    ERROR("Cannot create audio decoder, error=", av_err2str(result));
    return error::kUnknownError;
  }

  // Force to use stereo as channel layout
#if LIBAVUTIL_VERSION_MAJOR > 56
  decoder_->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
#else
  decoder_->channel_layout = AV_CH_LAYOUT_STEREO;
#endif

  result = avcodec_open2(decoder_.get(), codec, nullptr);
  if (result < 0) {
    ERROR("Cannot initialize audio decoder, error=", av_err2str(result));
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
  for (const auto &[name, filter] : audio_filters_) {
    result = CreateFilterEqualizer(name, filter);
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

  std::string ch_layout(64, ' ');

// Get channel layout description
#if LIBAVUTIL_VERSION_MAJOR > 56
  // Set filter options through the AVOptions API
  av_channel_layout_describe(&decoder_->ch_layout, ch_layout.data(), ch_layout.size());
#else
  // Set filter options through the AVOptions API
  av_get_channel_layout_string(ch_layout.data(), (int)ch_layout.size(), decoder_->channels,
                               decoder_->channel_layout);
#endif

  av_opt_set(buffersrc_ctx_.get(), "channel_layout", ch_layout.data(), AV_OPT_SEARCH_CHILDREN);
  av_opt_set(buffersrc_ctx_.get(), "sample_fmt", av_get_sample_fmt_name(decoder_->sample_fmt),
             AV_OPT_SEARCH_CHILDREN);
  av_opt_set_q(buffersrc_ctx_.get(), "time_base", (AVRational){1, decoder_->sample_rate},
               AV_OPT_SEARCH_CHILDREN);
  av_opt_set_int(buffersrc_ctx_.get(), "sample_rate", decoder_->sample_rate,
                 AV_OPT_SEARCH_CHILDREN);

  // Initialize filter
  if (int result = avfilter_init_str(buffersrc_ctx_.get(), nullptr); result < 0) {
    ERROR("Cannot initialize the abuffer filter, error=", av_err2str(result));
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
  if (int result = avfilter_init_str(volume_ctx, nullptr); result < 0) {
    ERROR("Cannot initialize the volume filter, error=", av_err2str(result));
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

  std::string ch_layout(64, ' ');

// Get channel layout description
#if LIBAVUTIL_VERSION_MAJOR > 56
  av_channel_layout_describe(&decoder_->ch_layout, ch_layout.data(), ch_layout.size());
#else
  av_get_channel_layout_string(ch_layout.data(), (int)ch_layout.size(), decoder_->channels,
                               decoder_->channel_layout);
#endif

  // Set filter options through the AVOptions API
  av_opt_set(aformat_ctx, "channel_layout", ch_layout.data(), AV_OPT_SEARCH_CHILDREN);
  av_opt_set(aformat_ctx, "sample_fmts", av_get_sample_fmt_name(AV_SAMPLE_FMT_S16),
             AV_OPT_SEARCH_CHILDREN);
  av_opt_set_int(aformat_ctx, "sample_rate", kSampleRate, AV_OPT_SEARCH_CHILDREN);

  // Initialize filter
  if (int result = avfilter_init_str(aformat_ctx, nullptr); result < 0) {
    ERROR("Cannot initialize the aformat filter, error=", av_err2str(result));
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
  if (int result = avfilter_init_str(buffersink_ctx_.get(), nullptr); result < 0) {
    ERROR("Cannot initialize the abuffersink instance, error=", av_err2str(result));
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
    ERROR("Cannot initialize the equalizer filter (", name, "), error=", av_err2str(result));
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
  for (const auto &[name, filter] : audio_filters_) {
    filters_to_link.push_back(avfilter_graph_get_filter(filter_graph_.get(), name.c_str()));
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
    ERROR("Cannot connect filters in the linear chain, error=", av_err2str(result));
    return error::kUnknownError;
  }

  // Configure the graph
  result = avfilter_graph_config(filter_graph_.get(), nullptr);
  if (result < 0) {
    ERROR("Cannot configure the filter graph for equalization, error=", av_err2str(result));
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

error::Code FFmpeg::Open(model::Song &audio_info) {
  LOG("Open file/url and attempt to decode as audio stream");
  auto clean_up_and_return = [&](error::Code error_code) {
    ClearCache();
    return error_code;
  };

  error::Code result = OpenInputStream(audio_info);
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
  int64_t song_duration = (input_stream_->duration / AV_TIME_BASE);

  // Read audio raw data from input stream
  while (av_read_frame(input_stream_.get(), packet) >= 0 && shared_context_.KeepDecoding()) {
    // If not the same stream index, we should not try to decode it
    if (packet->stream_index != stream_index_) {
      av_packet_unref(packet);
      continue;
    }

    // Send packet to decoder
    if (auto result = avcodec_send_packet(decoder_.get(), packet); result < 0) {
      // It is not actually an error, this kind of situation may happen when seek frame is used
      if (result == AVERROR_INVALIDDATA && shared_context_.position == song_duration) {
        break;
      }

      ERROR("Cannot decode song, error=", av_err2str(result));
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
  // Decoding
  input_stream_.reset();
  decoder_.reset();
  stream_index_ = 0;

  // Filters
  buffersrc_ctx_.reset();
  buffersink_ctx_.reset();

  // Custom data for audio filters
  audio_filters_.clear();

  // Clear internal structure used for sharing context
  shared_context_ = DecodingData{};

  // Main structure to handle all created filters
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

  // Set filter option
  if (std::string response(kResponseSize, ' ');
      avfilter_graph_send_command(filter_graph_.get(), kFilterVolume, "volume", volume.c_str(),
                                  response.data(), kResponseSize, AV_OPT_SEARCH_CHILDREN)) {
    ERROR("Cannot set new value for volume filter, error=", response);
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

model::Volume FFmpeg::GetVolume() const { return volume_; }

/* ********************************************************************************************** */

error::Code FFmpeg::UpdateFilters(const model::EqualizerPreset &filters) {
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

void FFmpeg::ProcessFrame(int samples, AudioCallback &callback) {
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

    // Check if EQ has updated or song position has changed
    if (shared_context_.reset_filters || shared_context_.position != old_position) {
      seek_frame = shared_context_.position != old_position;
      break;
    }
  }

  // Check if got some critical error
  if (result < 0 && result != AVERROR(EAGAIN) && result != AVERROR_EOF) {
    ERROR("Cannot pull data from audio filtergraph, error=", av_err2str(result));
    shared_context_.err_code = error::kDecodeFileFailed;
  }

  // Seek new position in song
  if (shared_context_.KeepDecoding() && seek_frame) {
    // Clear internal buffers
    shared_context_.ClearFrames();
    avcodec_flush_buffers(decoder_.get());

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
