/**
 * \file
 * \brief  Class for decoding audio using FFMPEG libraries
 */

#ifndef INCLUDE_AUDIO_DRIVER_DECODER_H_
#define INCLUDE_AUDIO_DRIVER_DECODER_H_

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

#include <memory>
#include <string>

#include "model/application_error.h"

namespace driver {

/**
 * @brief TODO:...
 */
class Decoder {
 public:
  /**
   * @brief Construct a new Decoder object
   */
  Decoder();

  /**
   * @brief Destroy the Decoder object
   */
  virtual ~Decoder() = default;

  /* ******************************************************************************************** */
  //! Internal operations
 private:
  error::Code OpenInputStream(const std::string& filepath);
  error::Code ConfigureDecoder();
  error::Code ConfigureResampler();

  /* ******************************************************************************************** */
 public:
  /**
   * @brief Open a file as input stream to parse it and check for codec compatibility for decoding
   * @param filepath Full path to file
   * @return error::Code Application error code
   */
  error::Code OpenFile(const std::string& filepath);

  error::Code Decode(int samples, std::function<void(void*, int, int)> callback);

  /* ******************************************************************************************** */
  //! Custom declarations with deleters
 private:
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

  using FormatContext = std::unique_ptr<AVFormatContext, FormatContextDeleter>;
  using CodecContext = std::unique_ptr<AVCodecContext, CodecContextDeleter>;
  using CustomSwrContext = std::unique_ptr<SwrContext, SwrContextDeleter>;

  using Packet = std::unique_ptr<AVPacket, PacketDeleter>;
  using Frame = std::unique_ptr<AVFrame, FrameDeleter>;

  /* ******************************************************************************************** */
  //! Default Constants

  static constexpr int kChannels = 2;
  static constexpr int kSampleRate = 44100;
  static constexpr AVSampleFormat kSampleFormat = AV_SAMPLE_FMT_S16;
  static constexpr int kChannelLayout = AV_CH_LAYOUT_STEREO;

  /* ******************************************************************************************** */
  //! Variables

  FormatContext input_stream_;
  CodecContext decoder_;
  CustomSwrContext resampler_;

  int stream_index_;
};

}  // namespace driver
#endif  // INCLUDE_AUDIO_DRIVER_DECODER_H_