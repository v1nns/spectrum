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
#include <string_view>

#include "audio/base/decoder.h"
#include "model/application_error.h"
#include "model/song.h"
#include "model/volume.h"

namespace driver {

/**
 * @brief Decode and equalize audio samples using FFmpeg libraries
 */
class FFmpeg final : public Decoder {
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
  //! Internal operations
 private:
  error::Code OpenInputStream(const std::string& filepath);
  error::Code ConfigureDecoder();
  error::Code ConfigureFilters();

  //! These are ffmpeg-specific filters
  error::Code CreateFilterAbufferSrc();
  error::Code CreateFilterVolume();
  error::Code CreateFilterAformat();
  error::Code CreateFilterAbufferSink();
  error::Code CreateFilterEqualizer(const std::string& name, const model::AudioFilter& filter);

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

  /* ******************************************************************************************** */
 public:
  /**
   * @brief Open file as input stream and check for codec compatibility for decoding
   * @param audio_info (In/Out) In case of success, this is filled with detailed audio information
   * @return error::Code Application error code
   */
  error::Code OpenFile(model::Song& audio_info) override;

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

  static constexpr int kChannels = 2;                                 //!< Output number of channels
  static constexpr int kSampleRate = 44100;                           //!< Output sample rate
  static constexpr AVSampleFormat kSampleFormat = AV_SAMPLE_FMT_S16;  //!< Output sample format

  //! All filters used from AVFilter library
  static constexpr char kFilterAbufferSrc[] = "abuffer";
  static constexpr char kFilterVolume[] = "volume";
  static constexpr char kFilterAformat[] = "aformat";
  static constexpr char kFilterEqualizer[] = "equalizer";
  static constexpr char kFilterAbufferSink[] = "abuffersink";

  static constexpr int kDefaultFilterCount =
      4;  //!< Number of filters without considering equalizer filters
  static constexpr int kResponseSize = 64;  //!< Response message size from AVFilter command

  /* ******************************************************************************************** */
  //! Utilities

  struct SampleFmtInfo {
    std::string_view name;  //! Short name
    int bits;               //! Bit depth
    int planar;  //! For planar sample formats, each audio channel is in a separate data plane, and
                 //! linesize is the buffer size, in bytes, for a single plane.
    enum AVSampleFormat altform;  //! Associated value from AVSampleFormat
  };

  /**
   * @brief Utilitary table with detailed info from FFmpeg AVSampleFormat (bit depth specially)
   */
  static constexpr std::array<SampleFmtInfo, AV_SAMPLE_FMT_NB> sample_fmt_info{{
      {"ut8", 8, 0, AV_SAMPLE_FMT_U8},
      {"s16", 16, 0, AV_SAMPLE_FMT_S16},
      {"s32", 32, 0, AV_SAMPLE_FMT_S32},
      {"flt", 32, 0, AV_SAMPLE_FMT_FLT},
      {"dbl", 64, 0, AV_SAMPLE_FMT_DBL},
      {"u8p", 8, 1, AV_SAMPLE_FMT_U8P},
      {"s16p", 16, 1, AV_SAMPLE_FMT_S16P},
      {"s32p", 32, 1, AV_SAMPLE_FMT_S32P},
      {"fltp", 32, 1, AV_SAMPLE_FMT_FLTP},
      {"dblp", 64, 1, AV_SAMPLE_FMT_DBLP},
      {"s64", 64, 0, AV_SAMPLE_FMT_S64},
      {"s64p", 64, 1, AV_SAMPLE_FMT_S64P},
  }};

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
    Frame frame_filtered;  //!< Frame received from filtergraph

    error::Code err_code;  //!< Error code for decoding and equalizing audio
    bool keep_playing;     //!< Control flag for playing audio
    bool reset_filters;    //!< Control flag for resetting filter graph

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
    bool CheckAllocations() const { return packet && frame_decoded && frame_filtered; }
  };

  /**
   * @brief Receive decoded frame and send it to be processed by filter chain (filtergraph), if
   * everything is fine, send output buffer to Player API callback
   *
   * @param samples Maximum number of samples to send to Audio Player API callback
   * @param callback Audio Player API callback
   */
  void ProcessFrame(int samples, AudioCallback& callback);

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

  FilterGraph filter_graph_;      //!< Directed graph of connected filters
  FilterContext buffersrc_ctx_;   //!< Input buffer for audio frames in the filter chain
  FilterContext buffersink_ctx_;  //!< Output buffer from filter chain

  using FilterName = std::string;
  std::map<FilterName, model::AudioFilter, std::less<>> audio_filters_;  //!< Equalization filters

  DecodingData shared_context_;  //!< Shared context for decoding and equalizing audio data
};

}  // namespace driver
#endif  // INCLUDE_AUDIO_DRIVER_FFMPEG_H_
