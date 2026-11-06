#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "audio/driver/ffmpeg.h"
#include "model/application_error.h"
#include "model/song.h"
#include "util/logger.h"

namespace {

/**
 * @brief Tests with FFmpeg class (using files written to a temporary directory)
 */
class FFmpegTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() { util::Logger::GetInstance().Configure(); }

  void TearDown() override { std::filesystem::remove(path); }

  /**
   * @brief Write a short silent Ogg file (with FLAC audio), keeping tags in its audio stream (as
   * Ogg does, unlike formats that keep them for the whole file, like MP3)
   * @return true if file was written, otherwise false
   */
  bool WriteOggWithStreamTags(const std::string& artist, const std::string& title) const {
    constexpr int kSampleRate = 44100;

    AVFormatContext* output = nullptr;
    if (avformat_alloc_output_context2(&output, nullptr, "ogg", path.c_str()) < 0) return false;

    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_FLAC);
    AVCodecContext* encoder = avcodec_alloc_context3(codec);
    encoder->sample_rate = kSampleRate;
    encoder->sample_fmt = AV_SAMPLE_FMT_S16;
    encoder->time_base = AVRational{1, kSampleRate};
    av_channel_layout_default(&encoder->ch_layout, 1);

    AVStream* stream = avformat_new_stream(output, nullptr);
    AVFrame* frame = av_frame_alloc();
    AVPacket* packet = av_packet_alloc();
    bool written = false;

    if (avcodec_open2(encoder, codec, nullptr) >= 0 &&
        avcodec_parameters_from_context(stream->codecpar, encoder) >= 0) {
      stream->time_base = encoder->time_base;
      av_dict_set(&stream->metadata, "artist", artist.c_str(), 0);
      av_dict_set(&stream->metadata, "title", title.c_str(), 0);

      frame->nb_samples = encoder->frame_size;
      frame->format = encoder->sample_fmt;
      frame->sample_rate = encoder->sample_rate;
      av_channel_layout_copy(&frame->ch_layout, &encoder->ch_layout);
      frame->pts = 0;

      if (avio_open(&output->pb, path.c_str(), AVIO_FLAG_WRITE) >= 0 &&
          avformat_write_header(output, nullptr) >= 0 && av_frame_get_buffer(frame, 0) >= 0) {
        av_samples_set_silence(frame->data, 0, frame->nb_samples, 1, encoder->sample_fmt);

        avcodec_send_frame(encoder, frame);
        avcodec_send_frame(encoder, nullptr);

        while (avcodec_receive_packet(encoder, packet) == 0) {
          av_packet_rescale_ts(packet, encoder->time_base, stream->time_base);
          packet->stream_index = stream->index;
          av_interleaved_write_frame(output, packet);
        }

        written = av_write_trailer(output) >= 0;
      }
    }

    av_packet_free(&packet);
    av_frame_free(&frame);
    avcodec_free_context(&encoder);
    if (output->pb) avio_closep(&output->pb);
    avformat_free_context(output);

    return written;
  }

  //! Temporary file written by test
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "spectrum_ffmpeg_test.ogg";
};

/* ********************************************************************************************** */

TEST_F(FFmpegTest, ReadTagsFromAudioStream) {
  ASSERT_TRUE(WriteOggWithStreamTags("Twin Tribes", "Monolith"));

  driver::FFmpeg decoder(/*verbose=*/false);
  model::Song song{.filepath = path};

  ASSERT_EQ(decoder.Open(song), error::kSuccess);
  EXPECT_EQ(song.artist, "Twin Tribes");
  EXPECT_EQ(song.title, "Monolith");

  decoder.ClearCache();
}

}  // namespace
