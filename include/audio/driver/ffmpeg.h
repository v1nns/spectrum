/**
 * \file
 * \brief  Class for decoding audio using FFmpeg libraries
 */

#ifndef INCLUDE_AUDIO_DRIVER_FFMPEG_H_
#define INCLUDE_AUDIO_DRIVER_FFMPEG_H_

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/version.h>
}

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "audio/base/decoder.h"
#include "model/application_error.h"
#include "model/song.h"
#include "model/volume.h"
#include "util/file_handler.h"

namespace driver {

/**
 * @brief Decode and equalize audio samples using FFmpeg libraries
 */
class FFmpeg final : public audio::Decoder {
 public:
  /**
   * @brief Construct a new FFmpeg object
   * @param verbose Enable verbose logging messages
   */
  explicit FFmpeg(bool verbose);

  /**
   * @brief Destroy the FFmpeg object
   */
  ~FFmpeg() override = default;

  /* ******************************************************************************************** */
  //! Public API that do not follow the instance lifecycle

  /**
   * @brief Check if file contains an available audio stream
   * @param file Full path to file
   * @return true if file contains an audio stream, false otherwise
   */
  static bool ContainsAudioStream(const util::File& file);

  /* ******************************************************************************************** */
  //! Internal operations
 private:
  error::Code OpenInputStream(const model::Song& audio_info);
  error::Code ConfigureDecoder();
  error::Code ConfigureFilters();

  //! These are ffmpeg-specific filters
  error::Code CreateFilterAbufferSrc();
  error::Code CreateFilterVolume(const char* name, const std::string& value);
  error::Code CreateFilterAformat(const char* name, int sample_rate, AVSampleFormat sample_format);
  error::Code CreateFilterAsplit();
  error::Code CreateFilterAbufferSink(const char* name);
  error::Code CreateFilterEqualizer(const std::string& name, const model::AudioFilter& filter);

  //! Get value for volume filter from playback, which also gives back what was attenuated before
  //! equalization filters (limited to what is always reduced from volume, to never amplify audio)
  std::string GetPlaybackVolume() const;

  /**
   * @brief Connect all filters created in the filtergraph as a linear chain
   * P.S. in general, this is the filter chain:
   *            _________    ________    ______________    _________    _____________
   * RAW DATA->| abuffer |->| volume |->| equalizer(s) |->| aformat |->| abuffersink |-> OUTPUT
   *            ---------    --------    --------------    ---------    -------------
   * @return error::Code Application error code
   */
  error::Code ConnectFilters();

  /**
   * @brief Extract all metadata from current song and fill the structure with it
   * @param audio_info Audio information structure
   */
  void FillAudioInformation(model::Song& audio_info);

  /**
   * @brief Check if audio stream being decoded is the only content from input (ignoring pictures
   * attached to it, like an album cover)
   * @return true if there is no other stream (like video or another audio), false otherwise
   */
  bool IsOnlyAudioStream() const;

  /* ******************************************************************************************** */
 public:
  /**
   * @brief Open song as input stream and check for codec compatibility for decoding
   * @param audio_info (In/Out) In case of success, this is filled with detailed audio information
   * @return error::Code Application error code
   */
  error::Code Open(model::Song& audio_info) override;

  /**
   * @brief Set format of audio samples sent to playback, creating the filter chain to convert
   * decoded audio to it (so it must be informed after opening song and before decoding it). When
   * it is changed while decoding, filter chain is created again before processing the next frame,
   * and samples in the previous format are not sent anymore
   * @param format Format of audio samples expected by playback
   * @return error::Code Application error code
   */
  error::Code SetOutputFormat(const model::AudioFormat& format) override;

  /**
   * @brief Decode and resample input stream to desired sample format/rate
   * @param samples Maximum value of samples
   * @param callback Pass resamples to this callback
   * @return error::Code Application error code
   */
  error::Code Decode(int samples, AudioCallback callback) override;

  /**
   * @brief After file is opened and decoded, or when some error occurs, always clear internal cache
   */
  void ClearCache() override;

  /**
   * @brief Set volume on playback stream
   *
   * @param value Desired volume (in a range between 0.f and 1.f)
   * @return error::Code Decoder error converted to application error code
   */
  error::Code SetVolume(model::Volume value) override;

  /**
   * @brief Get volume from playback stream
   * @return model::Volume Volume percentage (in a range between 0.f and 1.f)
   */
  model::Volume GetVolume() const override;

  /**
   * @brief Update audio filters in the filter chain (used for equalization)
   *
   * @param filters Audio filters
   * @return error::Code Decoder error converted to application error code
   */
  error::Code UpdateFilters(const model::EqualizerPreset& filters) override;

  /* ******************************************************************************************** */
  //! Custom declarations with deleters
 private:
  struct FormatContextDeleter {
    void operator()(AVFormatContext* p) const { avformat_close_input(&p); }
  };

  struct CodecContextDeleter {
    void operator()(AVCodecContext* p) const { avcodec_free_context(&p); }
  };

  struct PacketDeleter {
    void operator()(AVPacket* p) const {
      av_packet_unref(p);
      av_packet_free(&p);
    }
  };

  struct FrameDeleter {
    void operator()(AVFrame* p) const {
      av_frame_unref(p);
      av_frame_free(&p);
    }
  };

  struct FilterGraphDeleter {
    void operator()(AVFilterGraph* p) const { avfilter_graph_free(&p); }
  };

  struct FilterContextDeleter {
    void operator()(const AVFilterContext*) const noexcept {
      //! There is no need to do anything at all, because FilterGraphDeleter clears the resource
    }
  };

  using FormatContext = std::unique_ptr<AVFormatContext, FormatContextDeleter>;
  using CodecContext = std::unique_ptr<AVCodecContext, CodecContextDeleter>;

  using Packet = std::unique_ptr<AVPacket, PacketDeleter>;
  using Frame = std::unique_ptr<AVFrame, FrameDeleter>;

  using FilterGraph = std::unique_ptr<AVFilterGraph, FilterGraphDeleter>;
  using FilterContext = std::unique_ptr<AVFilterContext, FilterContextDeleter>;

  /* ******************************************************************************************** */
  //! Default Constants

  static constexpr int kChannels = 2;  //!< Output number of channels (playback and analysis)

  //! Format of samples sent to analysis (always the same one, no matter the output format)
  static constexpr int kAnalysisSampleRate = 44100;
  static constexpr AVSampleFormat kAnalysisSampleFormat = AV_SAMPLE_FMT_S16;

  //! All filters used from AVFilter library
  static constexpr char kFilterAbufferSrc[] = "abuffer";
  static constexpr char kFilterVolume[] = "volume";
  static constexpr char kFilterAformat[] = "aformat";
  static constexpr char kFilterEqualizer[] = "equalizer";
  static constexpr char kFilterAbufferSink[] = "abuffersink";
  static constexpr char kFilterAsplit[] = "asplit";

  //! Names for filter instances that exist in both branches from filtergraph (playback/analysis)
  static constexpr char kVolumePreamp[] = "preamp";
  static constexpr char kAformatPlayback[] = "aformat";
  static constexpr char kAformatAnalysis[] = "aformat_analysis";
  static constexpr char kSinkPlayback[] = "sink";
  static constexpr char kSinkAnalysis[] = "sink_analysis";

  static constexpr int kDefaultFilterCount =
      4;  //!< Number of filters in the main chain without considering equalizer filters
  static constexpr int kResponseSize = 64;  //!< Response message size from AVFilter command

  /* ******************************************************************************************** */
  //! Decoding

  /**
   * @brief An structure for shared use between Decode and ProcessFrame functions
   */
  struct DecodingData {
    AVRational time_base;  //!< Unit of time from input stream
    int64_t position;      //!< Current audio position

    Packet packet;         //!< Raw audio data read from input stream
    Frame frame_decoded;   //!< Frame received from decoder
    Frame frame_filtered;  //!< Frame received from filtergraph (to playback)
    Frame frame_analysis;  //!< Frame received from filtergraph (to analysis, without volume)

    error::Code err_code;  //!< Error code for decoding and equalizing audio
    bool keep_playing;     //!< Control flag for playing audio
    bool reset_filters;    //!< Control flag for resetting filter graph
    bool format_changed;   //!< Samples from current filter graph are not in output format anymore

    /**
     * @brief Clear packet content
     */
    void ClearPacket() const { av_packet_unref(packet.get()); }

    /**
     * @brief Clear content from all frames
     */
    void ClearFrames() const {
      av_frame_unref(frame_decoded.get());
      av_frame_unref(frame_filtered.get());
      av_frame_unref(frame_analysis.get());
    }

    /**
     * @brief Check condition to keep executing audio decoding operation
     * @return true for all conditions are fine to keep decoding, false otherwise
     */
    bool KeepDecoding() const { return err_code == error::kSuccess && keep_playing; }

    /**
     * @brief Check if internal structures are allocated correctly
     * @return true for correct allocation, false otherwise
     */
    [[nodiscard]] bool CheckAllocations() const {
      return packet && frame_decoded && frame_filtered && frame_analysis;
    }
  };

  /**
   * @brief Receive decoded frame and send it to be processed by filter chain (filtergraph), if
   * everything is fine, send output buffer to Player API callback
   *
   * @param samples Maximum number of samples to send to Audio Player API callback
   * @param callback Audio Player API callback
   * @param flush Signal end of stream to filtergraph (instead of sending decoded frame), to pull
   * the last samples from it
   */
  void ProcessFrame(int samples, AudioCallback& callback, bool flush = false);

  /**
   * @brief Flush frames still buffered by decoder and filtergraph after reaching the end of input
   * stream, sending them to Player API callback
   *
   * @param samples Maximum number of samples to send to Audio Player API callback
   * @param callback Audio Player API callback
   * @return true if song position has changed while flushing (so decoding must be resumed), false
   * otherwise
   */
  bool Flush(int samples, AudioCallback& callback);

  /* ******************************************************************************************** */
  //! Variables

#if LIBAVUTIL_VERSION_MAJOR > 56
  struct ChannelLayoutDeleter {
    void operator()(AVChannelLayout* p) const { av_channel_layout_uninit(p); }
  };

  using ChannelLayout = std::unique_ptr<AVChannelLayout, ChannelLayoutDeleter>;
  ChannelLayout ch_layout_;  //!< Default channel layout to use on decoding
#else
  static constexpr int kChannelLayout = AV_CH_LAYOUT_STEREO;
#endif

  FormatContext input_stream_;  //!< Input stream from file
  CodecContext decoder_;        //!< Specific codec compatible with the input stream

  int stream_index_ = 0;  //!< Audio stream index read in input stream

  model::Volume volume_ = model::Volume{1.f};  //!< Playback stream volume

  FilterGraph filter_graph_;         //!< Directed graph of connected filters
  FilterContext buffersrc_ctx_;      //!< Input buffer for audio frames in the filter chain
  FilterContext buffersink_ctx_;     //!< Output buffer from filter chain (to playback)
  FilterContext analysis_sink_ctx_;  //!< Output buffer from filter chain (to audio analysis)

  using FilterName = std::string;
  std::map<FilterName, model::AudioFilter, std::less<>> audio_filters_;  //!< Equalization filters

  //! Highest gain (in decibels) that equalization filters apply to any frequency. Audio is
  //! attenuated by it before these filters, otherwise it could be clipped by them
  double equalizer_peak_ = 0;

  DecodingData shared_context_;  //!< Shared context for decoding and equalizing audio data

  model::AudioFormat output_format_;  //!< Format of audio samples sent to playback
};

}  // namespace driver
#endif  // INCLUDE_AUDIO_DRIVER_FFMPEG_H_
