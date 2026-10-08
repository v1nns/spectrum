#include "audio/driver/ffmpeg.h"

#include <libavutil/error.h>

#include <cstdio>
#include <iomanip>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "util/logger.h"

namespace driver {

namespace {

/**
 * @brief Get description for an error code from FFmpeg (used instead of av_err2str, as that macro
 * relies on a C feature not accepted by every C++ compiler)
 * @param code Error code
 * @return Error description
 */
std::string ErrorToString(int code) {
  char buffer[AV_ERROR_MAX_STRING_SIZE] = {0};
  av_strerror(code, buffer, sizeof(buffer));

  return std::string{buffer};
}

}  // namespace

static void log_callback(void*, int level, const char* fmt, va_list vargs) {
  // Custom callback receives messages from every level, so filter them here
  if (level > av_log_get_level()) return;

  // Map FFmpeg level to application level
  const util::LogLevel log_level = level <= AV_LOG_ERROR     ? util::LogLevel::Error
                                   : level <= AV_LOG_WARNING ? util::LogLevel::Warning
                                                             : util::LogLevel::Debug;

  if (!util::Logger::GetInstance().IsEnabled(log_level)) return;

  va_list ap_copy;
  va_copy(ap_copy, vargs);
  int len = vsnprintf(nullptr, 0, fmt, ap_copy);
  va_end(ap_copy);

  if (len <= 0) return;

  std::string message(static_cast<std::size_t>(len) + 1, '\0');  // need space for NUL
  vsnprintf(message.data(), message.size(), fmt, vargs);
  message.resize(static_cast<std::size_t>(len));

  // Messages usually end with a line break, which is already added by logger
  while (!message.empty() && (message.back() == '\n' || message.back() == '\r')) message.pop_back();

  LOG_LEVEL(log_level, "[ffmpeg] ", message);
}

/* ********************************************************************************************** */

FFmpeg::FFmpeg(bool verbose) {
  LOG("Initialize FFmpeg with verbose logging=", verbose);
  INFO("FFmpeg version=", av_version_info());

#if LIBAVUTIL_VERSION_MAJOR > 56
  ch_layout_.reset(new AVChannelLayout{});
  // Set output channel layout to stereo (2-channel)
  av_channel_layout_default(ch_layout_.get(), 2);
#endif

  // Warnings and errors from FFmpeg are always logged (and everything else with verbose logging)
  av_log_set_level(verbose ? AV_LOG_VERBOSE : AV_LOG_WARNING);
  av_log_set_callback(log_callback);
}

/* ********************************************************************************************** */

bool FFmpeg::ContainsAudioStream(const util::File& file) {
  LOG("Check for audio stream on file=", std::quoted(file.string()));
  AVFormatContext* ptr = nullptr;

  // Open input stream from given file
  int result = avformat_open_input(&ptr, file.c_str(), nullptr, nullptr);
  if (result < 0) {
    ERROR("Cannot open file as input stream, error=", ErrorToString(result));
    return false;
  }

  // Get stream information from parsed input
  FormatContext input_stream(std::move(ptr));
  result = avformat_find_stream_info(input_stream.get(), nullptr);

  if (result < 0) {
    ERROR("Cannot find stream information in the opened input, error=", ErrorToString(result));
    return false;
  }

#if LIBAVFORMAT_VERSION_MAJOR > 58
  const AVCodec* codec = nullptr;
#else
  AVCodec* codec = nullptr;
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

error::Code FFmpeg::OpenInputStream(const model::Song& audio_info) {
  AVFormatContext* ptr = nullptr;
  AVDictionary* options = nullptr;

  std::string url;

  if (audio_info.stream_info.has_value()) {
    LOG("Song contains streaming information, attempt to decode as audio stream");

    std::string http_header;
    for (const auto& [key, value] : audio_info.stream_info->http_header) {
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
    ERROR("Cannot open input stream, error=", ErrorToString(result));
    return error::kFileNotSupported;
  }

  input_stream_.reset(std::move(ptr));

  result = avformat_find_stream_info(input_stream_.get(), nullptr);
  if (result < 0) {
    ERROR("Cannot find stream info about opened input, error=", ErrorToString(result));
    return error::kFileNotSupported;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::ConfigureDecoder() {
  LOG("Configure audio decoder for opened input stream");

#if LIBAVFORMAT_VERSION_MAJOR > 58
  const AVCodec* codec = nullptr;
#else
  AVCodec* codec = nullptr;
#endif

  // select the audio stream
  stream_index_ = av_find_best_stream(input_stream_.get(), AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);

  if (stream_index_ < 0 || !codec) {
    ERROR("Cannot find audio decoder to specified file");
    return error::kFileNotSupported;
  }

  const AVCodecParameters* parameters = input_stream_->streams[stream_index_]->codecpar;
  decoder_ = CodecContext{avcodec_alloc_context3(codec)};

  int result = avcodec_parameters_to_context(decoder_.get(), parameters);
  if (result < 0) {
    ERROR("Cannot create audio decoder, error=", ErrorToString(result));
    return error::kUnknownError;
  }

  // Keep the input channel layout, filter graph is responsible for converting it to stereo output.
  // Overriding it here would make abuffer expect a layout different from the decoded frames
#if LIBAVUTIL_VERSION_MAJOR <= 56
  // Old API may not fill a channel layout (e.g. WAV without channel mask), so use default one
  if (decoder_->channel_layout == 0) {
    decoder_->channel_layout =
        static_cast<uint64_t>(av_get_default_channel_layout(decoder_->channels));
  }
#endif

  result = avcodec_open2(decoder_.get(), codec, nullptr);
  if (result < 0) {
    ERROR("Cannot initialize audio decoder, error=", ErrorToString(result));
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
  for (const auto& [name, filter] : audio_filters_) {
    result = CreateFilterEqualizer(name, filter);
    if (result != error::kSuccess) return result;
  }

  // Create and configure asplit filter, to split audio between playback and analysis
  result = CreateFilterAsplit();
  if (result != error::kSuccess) return result;

  // Create and configure aformat filter for both branches (output format is the one expected by
  // playback, while analysis always expects the same format)
  result = CreateFilterAformat(kAformatPlayback, static_cast<int>(output_format_.sample_rate),
                               output_format_.sample_format == model::SampleFormat::S16
                                   ? AV_SAMPLE_FMT_S16
                                   : AV_SAMPLE_FMT_S32);
  if (result != error::kSuccess) return result;

  result = CreateFilterAformat(kAformatAnalysis, kAnalysisSampleRate, kAnalysisSampleFormat);
  if (result != error::kSuccess) return result;

  // Create and configure abuffersink filter for both branches
  for (const char* name : {kSinkPlayback, kSinkAnalysis}) {
    result = CreateFilterAbufferSink(name);
    if (result != error::kSuccess) {
      return result;
    }
  }

  buffersink_ctx_.reset(avfilter_graph_get_filter(filter_graph_.get(), kSinkPlayback));
  analysis_sink_ctx_.reset(avfilter_graph_get_filter(filter_graph_.get(), kSinkAnalysis));

  // Link all filters
  result = ConnectFilters();

  return result;
}

/* ********************************************************************************************** */

error::Code FFmpeg::CreateFilterAbufferSrc() {
  LOG("Create abuffer filter");

  // Find abuffer filter
  const AVFilter* abuffersrc = avfilter_get_by_name(kFilterAbufferSrc);

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
  av_get_channel_layout_string(ch_layout.data(), static_cast<int>(ch_layout.size()),
                               decoder_->channels, decoder_->channel_layout);
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
    ERROR("Cannot initialize the abuffer filter, error=", ErrorToString(result));
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::CreateFilterVolume() {
  LOG("Create volume filter with value=", volume_);

  // Find volume filter
  const AVFilter* volume = avfilter_get_by_name(kFilterVolume);

  if (!volume) {
    ERROR("Cannot find the volume filter");
    return error::kUnknownError;
  }

  // Create an instance of volume filter
  AVFilterContext* volume_ctx =
      avfilter_graph_alloc_filter(filter_graph_.get(), volume, kFilterVolume);

  if (!volume_ctx) {
    ERROR("Cannot allocate the volume instance");
    return error::kUnknownError;
  }

  // Set filter option using decibel scale for perceptually accurate volume control
  av_opt_set(volume_ctx, "volume", model::to_string_db(volume_).c_str(), AV_OPT_SEARCH_CHILDREN);

  // Initialize filter
  if (int result = avfilter_init_str(volume_ctx, nullptr); result < 0) {
    ERROR("Cannot initialize the volume filter, error=", ErrorToString(result));
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::CreateFilterAformat(const char* name, int sample_rate,
                                        AVSampleFormat sample_format) {
  LOG("Create aformat filter with name=", name, " sample rate=", sample_rate,
      " sample format=", av_get_sample_fmt_name(sample_format));

  // Find aformat filter
  const AVFilter* aformat = avfilter_get_by_name(kFilterAformat);

  if (!aformat) {
    ERROR("Cannot find the aformat filter");
    return error::kUnknownError;
  }

  // Create an instance of aformat filter, it ensures that the output is of the format we want
  AVFilterContext* aformat_ctx = avfilter_graph_alloc_filter(filter_graph_.get(), aformat, name);

  if (!aformat_ctx) {
    ERROR("Cannot allocate the aformat instance");
    return error::kUnknownError;
  }

  std::string ch_layout(64, ' ');

// Get output channel layout description (always stereo, regardless of the input layout)
#if LIBAVUTIL_VERSION_MAJOR > 56
  av_channel_layout_describe(ch_layout_.get(), ch_layout.data(), ch_layout.size());
#else
  av_get_channel_layout_string(ch_layout.data(), static_cast<int>(ch_layout.size()), 2,
                               AV_CH_LAYOUT_STEREO);
#endif

  std::string rate = std::to_string(sample_rate);

  // Set filter options through the AVOptions API (as lists with a single value, in string format,
  // which is accepted by all FFmpeg versions)
  if (int result =
          av_opt_set(aformat_ctx, "channel_layouts", ch_layout.data(), AV_OPT_SEARCH_CHILDREN);
      result < 0) {
    ERROR("Cannot set channel layout for the aformat filter, error=", ErrorToString(result));
    return error::kUnknownError;
  }

  if (int result = av_opt_set(aformat_ctx, "sample_fmts", av_get_sample_fmt_name(sample_format),
                              AV_OPT_SEARCH_CHILDREN);
      result < 0) {
    ERROR("Cannot set sample format for the aformat filter, error=", ErrorToString(result));
    return error::kUnknownError;
  }

  if (int result = av_opt_set(aformat_ctx, "sample_rates", rate.c_str(), AV_OPT_SEARCH_CHILDREN);
      result < 0) {
    ERROR("Cannot set sample rate for the aformat filter, error=", ErrorToString(result));
    return error::kUnknownError;
  }

  // Initialize filter
  if (int result = avfilter_init_str(aformat_ctx, nullptr); result < 0) {
    ERROR("Cannot initialize the aformat filter, error=", ErrorToString(result));
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::CreateFilterAsplit() {
  LOG("Create asplit filter");

  // Find asplit filter
  const AVFilter* asplit = avfilter_get_by_name(kFilterAsplit);

  if (!asplit) {
    ERROR("Cannot find the asplit filter");
    return error::kUnknownError;
  }

  // Create an instance of asplit filter, it duplicates audio for both playback and analysis
  AVFilterContext* asplit_ctx =
      avfilter_graph_alloc_filter(filter_graph_.get(), asplit, kFilterAsplit);

  if (!asplit_ctx) {
    ERROR("Cannot allocate the asplit instance");
    return error::kUnknownError;
  }

  // Initialize filter with two outputs
  if (const int result = avfilter_init_str(asplit_ctx, "outputs=2"); result < 0) {
    ERROR("Cannot initialize the asplit filter, error=", ErrorToString(result));
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::CreateFilterAbufferSink(const char* name) {
  LOG("Create abuffersink filter with name=", name);

  // Find abuffersink filter
  const AVFilter* abuffersink = avfilter_get_by_name(kFilterAbufferSink);

  if (!abuffersink) {
    ERROR("Cannot find the abuffersink filter");
    return error::kUnknownError;
  }

  // Create an instance of abuffersink filter, it will be used to get filtered data out of the graph
  AVFilterContext* sink_ctx = avfilter_graph_alloc_filter(filter_graph_.get(), abuffersink, name);

  if (!sink_ctx) {
    ERROR("Cannot allocate the abuffersink instance");
    return error::kUnknownError;
  }

  // This filter takes no options
  if (const int result = avfilter_init_str(sink_ctx, nullptr); result < 0) {
    ERROR("Cannot initialize the abuffersink instance, error=", ErrorToString(result));
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::CreateFilterEqualizer(const std::string& name,
                                          const model::AudioFilter& filter) {
  // Find equalizer filter
  const AVFilter* equalizer = avfilter_get_by_name(kFilterEqualizer);

  if (!equalizer) {
    ERROR("Cannot find the equalizer filter");
    return error::kUnknownError;
  }

  // Create an instance of equalizer filter
  AVFilterContext* equalizer_ctx =
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
    ERROR("Cannot initialize the equalizer filter (", name, "), error=", ErrorToString(result));
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

error::Code FFmpeg::ConnectFilters() {
  LOG("Connect all filters");

  // Find existing instance of filters
  AVFilterContext* volume_ctx = avfilter_graph_get_filter(filter_graph_.get(), kFilterVolume);
  AVFilterContext* asplit_ctx = avfilter_graph_get_filter(filter_graph_.get(), kFilterAsplit);
  AVFilterContext* aformat_playback =
      avfilter_graph_get_filter(filter_graph_.get(), kAformatPlayback);
  AVFilterContext* aformat_analysis =
      avfilter_graph_get_filter(filter_graph_.get(), kAformatAnalysis);

  // Main chain: abuffer -> equalizer filters -> asplit
  std::vector<AVFilterContext*> main_chain;
  main_chain.reserve(kDefaultFilterCount + audio_filters_.size());

  main_chain.push_back(buffersrc_ctx_.get());

  for (const auto& [name, filter] : audio_filters_) {
    main_chain.push_back(avfilter_graph_get_filter(filter_graph_.get(), name.c_str()));
  }

  main_chain.push_back(asplit_ctx);

  // Link a sequence of filters, using the given output pad from the first filter
  auto link_chain = [](const std::vector<AVFilterContext*>& chain, unsigned first_pad) {
    int result = 0;
    unsigned pad = first_pad;

    for (auto it = chain.begin(); std::next(it) != chain.end() && result >= 0; ++it, pad = 0) {
      result = avfilter_link(*it, pad, *std::next(it), 0);
    }

    return result;
  };

  // Split audio in two branches, as audio analysis must not be affected by volume (otherwise,
  // muting audio would also clear spectrum visualizer):
  //  - playback: asplit -> volume -> aformat -> abuffersink
  //  - analysis: asplit -> aformat -> abuffersink
  static constexpr unsigned kPlaybackPad = 0;
  static constexpr unsigned kAnalysisPad = 1;

  int result = link_chain(main_chain, 0);

  if (result >= 0) {
    result =
        link_chain({asplit_ctx, volume_ctx, aformat_playback, buffersink_ctx_.get()}, kPlaybackPad);
  }

  if (result >= 0) {
    result = link_chain({asplit_ctx, aformat_analysis, analysis_sink_ctx_.get()}, kAnalysisPad);
  }

  if (result < 0) {
    ERROR("Cannot connect filters, error=", ErrorToString(result));
    return error::kUnknownError;
  }

  // Configure the graph
  result = avfilter_graph_config(filter_graph_.get(), nullptr);
  if (result < 0) {
    ERROR("Cannot configure the filter graph for equalization, error=", ErrorToString(result));
    return error::kUnknownError;
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

bool FFmpeg::IsOnlyAudioStream() const {
  for (unsigned int i = 0; i < input_stream_->nb_streams; i++) {
    const AVStream* stream = input_stream_->streams[i];

    const bool is_picture = (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) != 0;
    if (static_cast<int>(i) != stream_index_ && !is_picture) return false;
  }

  return true;
}

/* ********************************************************************************************** */

void FFmpeg::FillAudioInformation(model::Song& audio_info) {
  LOG("Fill song structure with audio information");

  // use this to get all metadata associated to this audio file
  //   const AVDictionaryEntry *tag = nullptr;
  //   while ((tag = av_dict_get(input_stream_->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
  //     LOG("key=", tag->key," value=", tag->value);
  const AVDictionaryEntry* tag = nullptr;

  // Get track name
  // Note: each lookup must search the whole dictionary (prev=nullptr), as tags may be in any order
  tag = av_dict_get(input_stream_->metadata, "title", nullptr, AV_DICT_IGNORE_SUFFIX);
  if (tag) audio_info.title = std::string{tag->value};

  // Get artist name
  tag = av_dict_get(input_stream_->metadata, "artist", nullptr, AV_DICT_IGNORE_SUFFIX);
  if (tag) audio_info.artist = std::string{tag->value};

  const AVCodecParameters* audio_stream = input_stream_->streams[stream_index_]->codecpar;

#if LIBAVUTIL_VERSION_MAJOR > 56
  audio_info.num_channels = static_cast<uint16_t>(audio_stream->ch_layout.nb_channels);
#else
  audio_info.num_channels = static_cast<uint16_t>(audio_stream->channels);
#endif
  audio_info.sample_rate = static_cast<uint32_t>(audio_stream->sample_rate);

  // Not every codec informs bit rate in its stream (e.g. FLAC and Opus). In this case, keep the one
  // already known (from streaming information), or use the one from the whole file when there is
  // nothing else in it besides this audio stream (and pictures, like an album cover)
  if (audio_stream->bit_rate > 0) {
    audio_info.bit_rate = static_cast<uint32_t>(audio_stream->bit_rate);
  } else if (audio_info.bit_rate == 0 && input_stream_->bit_rate > 0 && IsOnlyAudioStream()) {
    audio_info.bit_rate = static_cast<uint32_t>(input_stream_->bit_rate);
  }

  // Use bit depth from source (e.g. 16 or 24 bits for FLAC/WAV), not from the decoded sample format
  // (which is 32 bits float for lossy codecs like MP3). For lossy codecs, it stays 0 (not
  // applicable)
  audio_info.bit_depth = static_cast<uint32_t>(audio_stream->bits_per_raw_sample > 0
                                                   ? audio_stream->bits_per_raw_sample
                                                   : audio_stream->bits_per_coded_sample);
  audio_info.duration = static_cast<uint32_t>(input_stream_->duration / AV_TIME_BASE);
}

/* ********************************************************************************************** */

error::Code FFmpeg::Open(model::Song& audio_info) {
  LOG("Open file/url and attempt to decode as audio stream");
  auto clean_up_and_return = [&](error::Code error_code) {
    ClearCache();
    return error_code;
  };

  error::Code result = OpenInputStream(audio_info);
  if (result != error::kSuccess) return clean_up_and_return(result);

  result = ConfigureDecoder();
  if (result != error::kSuccess) return clean_up_and_return(result);

  // At this point, we can get detailed information about the song (filters are configured only
  // when output format is informed, as it may depend on this information)
  FillAudioInformation(audio_info);

  return result;
}

/* ********************************************************************************************** */

error::Code FFmpeg::SetOutputFormat(const model::AudioFormat& format) {
  LOG("Set output format=", format);
  output_format_ = format;

  if (!decoder_) {
    ERROR("Cannot set output format without opening a song");
    return error::kUnknownError;
  }

  // While decoding, filtergraph is in use: it is created again before processing the next frame,
  // and samples still held by it are discarded (as they are not in the new format)
  if (shared_context_.packet) {
    shared_context_.reset_filters = true;
    shared_context_.format_changed = true;
    return error::kSuccess;
  }

  if (error::Code result = ConfigureFilters(); result != error::kSuccess) {
    ClearCache();
    return result;
  }

  return error::kSuccess;
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
      .frame_analysis{Frame(av_frame_alloc())},
      .err_code = error::kSuccess,
      .keep_playing = true,
      .reset_filters = false,
      .format_changed = false,
  };

  if (!shared_context_.CheckAllocations()) {
    ERROR("Cannot allocate internal structures to decode song");
    return error::kUnknownError;
  }

  AVPacket* packet = shared_context_.packet.get();
  AVFrame* frame = shared_context_.frame_decoded.get();
  int64_t song_duration = (input_stream_->duration / AV_TIME_BASE);

  // Read audio raw data from input stream. Once it ends, flush what is still buffered internally,
  // and in case of seeking to a new position in the meantime, get back to reading
  do {
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

        ERROR("Cannot decode song, error=", ErrorToString(result));
        return error::kDecodeFileFailed;
      }

      // Receive frames from decoder
      while (avcodec_receive_frame(decoder_.get(), frame) >= 0 && shared_context_.KeepDecoding()) {
        // Note that AVPacket.pts is in AVStream.time_base units, not AVCodecContext.time_base units
        shared_context_.position = packet->pts / shared_context_.time_base.den;

        // UI sent event to update audio filters with new parameters, so it is necessary to reset it
        if (shared_context_.reset_filters) {
          shared_context_.reset_filters = false;
          shared_context_.format_changed = false;

          // Old filtergraph was already released, so there is nothing left to process this frame
          if (ConfigureFilters() != error::kSuccess) {
            ERROR("Cannot reconfigure filtergraph with updated audio filters");
            shared_context_.err_code = error::kEqualizerFailed;
            break;
          }
        }

        // Pass decoded frame to be processed by filtergraph. And in case of error while processing
        // frame, shared_context_.KeepDecoding() will return false, so do not worry about it
        ProcessFrame(samples, callback);

        shared_context_.ClearFrames();
      }

      shared_context_.ClearPacket();
    }
  } while (Flush(samples, callback));

  return shared_context_.err_code;
}

/* ********************************************************************************************** */

bool FFmpeg::Flush(int samples, AudioCallback& callback) {
  AVFrame* frame = shared_context_.frame_decoded.get();
  int64_t old_position = shared_context_.position;

  // Seeking is handled by ProcessFrame, here it only matters to know that it has happened
  auto keep_flushing = [this, old_position]() {
    return shared_context_.position == old_position && shared_context_.KeepDecoding();
  };

  // Nothing to flush when decoding was stopped (by user or by some error)
  if (!keep_flushing()) return false;

  // Enter draining mode, to receive frames that are still buffered by decoder
  avcodec_send_packet(decoder_.get(), nullptr);

  while (keep_flushing() && avcodec_receive_frame(decoder_.get(), frame) >= 0) {
    ProcessFrame(samples, callback);
    shared_context_.ClearFrames();
  }

  // Signal end of stream to filtergraph, to pull the last samples (as they are not enough to fill
  // an entire buffer, they are still held by sink)
  if (keep_flushing()) ProcessFrame(samples, callback, true);

  bool seek_frame = shared_context_.position != old_position;
  return seek_frame && shared_context_.KeepDecoding();
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
  analysis_sink_ctx_.reset();

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
  std::string volume = model::to_string_db(volume_);
  LOG("Found volume filter, update value to ", volume);

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

error::Code FFmpeg::UpdateFilters(const model::EqualizerPreset& filters) {
  LOG("Update audio filters in the internal structure");

  // Clear internal structure
  audio_filters_.clear();

  for (const auto& filter : filters) {
    if (filter.frequency == 0 || filter.Q == 0) {
      ERROR("Zeroed filter is not permitted");
      return error::kEqualizerFailed;
    }

    std::string name{filter.GetName()};
    audio_filters_[name] = filter;
  }

  // In case that music is playing, must reset filter graph
  if (filter_graph_) shared_context_.reset_filters = true;

  return error::kSuccess;
}

/* ********************************************************************************************** */

void FFmpeg::ProcessFrame(int samples, AudioCallback& callback, bool flush) {
  // Filtergraph still creates samples in the previous output format, so do not use it anymore
  if (shared_context_.format_changed) return;

  // Get source and sinks
  AVFilterContext* source = buffersrc_ctx_.get();
  AVFilterContext* sink = buffersink_ctx_.get();
  AVFilterContext* analysis_sink = analysis_sink_ctx_.get();

  // Get allocated pointer for frames (decoded and filtered)
  AVFrame* decoded = shared_context_.frame_decoded.get();
  AVFrame* filtered = shared_context_.frame_filtered.get();
  AVFrame* analysis = shared_context_.frame_analysis.get();

  // Push the audio data from decoded frame into the filtergraph (or signal end of stream instead)
  if (av_buffersrc_add_frame_flags(source, flush ? nullptr : decoded, AV_BUFFERSRC_FLAG_KEEP_REF) <
      0) {
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
    // Pull the same audio from analysis branch (not affected by volume), which may have a different
    // number of samples, as its sample rate is not always the same one from output format. When
    // they are not available yet, they are sent along with the next ones
    const auto analysis_samples = static_cast<int>(
        av_rescale_rnd(filtered->nb_samples, kAnalysisSampleRate,
                       static_cast<int64_t>(output_format_.sample_rate), AV_ROUND_UP));

    const bool has_analysis =
        av_buffersink_get_samples(analysis_sink, analysis, analysis_samples) >= 0;

    // Send filtered audio data to Player
    shared_context_.keep_playing =
        callback(static_cast<void*>(filtered->data[0]), filtered->nb_samples,
                 has_analysis ? static_cast<void*>(analysis->data[0]) : nullptr,
                 has_analysis ? analysis->nb_samples : 0, shared_context_.position);

    // Clear frames from filtergraph
    av_frame_unref(filtered);
    av_frame_unref(analysis);

    // Check if EQ has updated or song position has changed (when flushing, there is no next frame
    // to apply updated EQ, so just keep pulling)
    if ((shared_context_.reset_filters && !flush) || shared_context_.format_changed ||
        shared_context_.position != old_position) {
      seek_frame = shared_context_.position != old_position;
      break;
    }
  }

  // Check if got some critical error
  if (result < 0 && result != AVERROR(EAGAIN) && result != AVERROR_EOF) {
    ERROR("Cannot pull data from audio filtergraph, error=", ErrorToString(result));
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

    // Filtergraph does not accept frames after end of stream, so it must be created again
    if (flush) shared_context_.reset_filters = true;
  }
}

}  // namespace driver
