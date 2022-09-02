/**
 * \file
 * \brief  Class for decoding audio using FFmpeg libraries
 */

#ifndef INCLUDE_AUDIO_DRIVER_FFMPEG_H_
#define INCLUDE_AUDIO_DRIVER_FFMPEG_H_

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

#include <memory>
#include <string>

#include "audio/base/decoder.h"
#include "model/application_error.h"
#include "model/song.h"

namespace driver {

/**
 * @brief Decode and resample audio samples using FFmpeg libraries
 */
class FFmpeg : public Decoder {
 public:
  /**
   * @brief Construct a new FFmpeg object
   */
  FFmpeg();

  /**
   * @brief Destroy the FFmpeg object
   */
  virtual ~FFmpeg() = default;

  /* ******************************************************************************************** */
  //! Internal operations
 private:
  error::Code OpenInputStream(const std::string& filepath);
  error::Code ConfigureDecoder();
  error::Code ConfigureResampler();

  void FillAudioInformation(model::Song* audio_info);

  /* ******************************************************************************************** */
 public:
  /**
   * @brief Open file as input stream and check for codec compatibility for decoding
   * @param audio_info (In/Out) In case of success, this is filled with detailed audio information
   * @return error::Code Application error code
   */
  error::Code OpenFile(model::Song* audio_info) override;

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

  /* ******************************************************************************************** */
  //! Custom declarations with deleters
 private:
  struct ChannelLayoutDeleter {
    void operator()(AVChannelLayout* p) const { av_channel_layout_uninit(p); }
  };

  struct FormatContextDeleter {
    void operator()(AVFormatContext* p) const { avformat_close_input(&p); }
  };

  struct CodecContextDeleter {
    void operator()(AVCodecContext* p) const { avcodec_free_context(&p); }
  };

  struct SwrContextDeleter {
    void operator()(SwrContext* p) const { swr_free(&p); }
  };

  struct PacketDeleter {
    void operator()(AVPacket* p) const { av_packet_free(&p); }
  };

  struct FrameDeleter {
    void operator()(AVFrame* p) const { av_frame_free(&p); }
  };

  struct DataBufferDeleter {
    void operator()(uint8_t* p) const { free(p); }
  };

  using ChannelLayout = std::unique_ptr<AVChannelLayout, ChannelLayoutDeleter>;

  using FormatContext = std::unique_ptr<AVFormatContext, FormatContextDeleter>;
  using CodecContext = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
  using CustomSwrContext = std::unique_ptr<SwrContext, SwrContextDeleter>;

  using Packet = std::unique_ptr<AVPacket, PacketDeleter>;
  using Frame = std::unique_ptr<AVFrame, FrameDeleter>;

  using DataBuffer = std::unique_ptr<uint8_t, DataBufferDeleter>;

  /* ******************************************************************************************** */
  //! Default Constants

  static constexpr int kChannels = 2;
  static constexpr int kSampleRate = 44100;
  static constexpr AVSampleFormat kSampleFormat = AV_SAMPLE_FMT_S16;

  /* ******************************************************************************************** */
  //! Utilities

  struct SampleFmtInfo {
    char name[8];  //! Short name
    int bits;      //! Bit depth
    int planar;  //! For planar sample formats, each audio channel is in a separate data plane, and
                 //! linesize is the buffer size, in bytes, for a single plane.
    enum AVSampleFormat altform;  //! Associated value from AVSampleFormat
  };

  /**
   * @brief Utilitary table with detailed info from FFmpeg AVSampleFormat (bit depth specially)
   */
  static constexpr SampleFmtInfo sample_fmt_info[AV_SAMPLE_FMT_NB] = {
      {"ut8", 8, 0, AV_SAMPLE_FMT_U8},     {"s16", 16, 0, AV_SAMPLE_FMT_S16},
      {"s32", 32, 0, AV_SAMPLE_FMT_S32},   {"flt", 32, 0, AV_SAMPLE_FMT_FLT},
      {"dbl", 64, 0, AV_SAMPLE_FMT_DBL},   {"u8p", 8, 1, AV_SAMPLE_FMT_U8P},
      {"s16p", 16, 1, AV_SAMPLE_FMT_S16P}, {"s32p", 32, 1, AV_SAMPLE_FMT_S32P},
      {"fltp", 32, 1, AV_SAMPLE_FMT_FLTP}, {"dblp", 64, 1, AV_SAMPLE_FMT_DBLP},
      {"s64", 64, 0, AV_SAMPLE_FMT_S64},   {"s64p", 64, 1, AV_SAMPLE_FMT_S64P},
  };

  /* ******************************************************************************************** */
  //! Variables

  ChannelLayout ch_layout_;  //!< Default channel layout to use on decoding

  FormatContext input_stream_;  //!< Input stream from file
  CodecContext decoder_;        //!< Specific codec compatible with the input stream
  CustomSwrContext resampler_;  //!< Resample audio data to desired sample format and rate

  int stream_index_;  //!< Audio stream index read in input stream
};

}  // namespace driver
#endif  // INCLUDE_AUDIO_DRIVER_FFMPEG_H_
