/**
 * \file
 * \brief Dummy class for decoder support
 */

#ifndef INCLUDE_DEBUG_DUMMY_DECODER_H_
#define INCLUDE_DEBUG_DUMMY_DECODER_H_

#include <functional>

#include "audio/base/decoder.h"
#include "model/application_error.h"
#include "model/audio_filter.h"
#include "model/song.h"
#include "model/volume.h"
#include "util/file_handler.h"

namespace driver {

/**
 * @brief Dummy implementation
 */
class DummyDecoder : public Decoder {
 public:
  /**
   * @brief Construct a new Decoder object
   */
  DummyDecoder() = default;

  /**
   * @brief Destroy the Decoder object
   */
  virtual ~DummyDecoder() = default;

  /* ******************************************************************************************** */
  //! Public API that do not follow the instance lifecycle

  /**
   * @brief Check if file contains an available audio stream
   * @param file Full path to file
   * @return true if file contains an audio stream, false otherwise
   */
  static inline bool ContainsAudioStream(const util::File& file) { return true; }

  /* ******************************************************************************************** */
  //! Public API for Decoder

  /**
   * @brief Function invoked after resample is available.
   * (for better understanding: take a look at Audio Loop from Player, and also Playback class)
   */
  using AudioCallback = std::function<bool(void*, int, int64_t&)>;

  /**
   * @brief Open file as input stream and check for codec compatibility for decoding
   * @param audio_info (In/Out) In case of success, this is filled with detailed audio information
   * @return error::Code Application error code
   */
  error::Code OpenFile(model::Song& audio_info) override {
    audio_info = model::Song{.artist = "Dummy artist",
                             .title = "Dummy title",
                             .num_channels = 2,
                             .sample_rate = 44100,
                             .bit_rate = 320000,
                             .bit_depth = 32,
                             .duration = 120};

    return error::kSuccess;
  }

  /**
   * @brief Decode and resample input stream to desired sample format/rate
   * @param samples Maximum value of samples
   * @param callback Pass resamples to this callback
   * @return error::Code Application error code
   */
  error::Code Decode(int samples, AudioCallback callback) override {
    callback((void*)nullptr, 0, position_);
    return error::kSuccess;
  }

  /**
   * @brief After file is opened and decoded, or when some error occurs, always clear internal cache
   */
  void ClearCache() override {}

  /* ******************************************************************************************** */
  //! Public API for Equalizer

  /**
   * @brief Set volume on playback stream
   *
   * @param value Desired volume (in a range between 0.f and 1.f)
   * @return error::Code Decoder error converted to application error code
   */
  error::Code SetVolume(model::Volume value) override {
    volume_ = value;
    return error::kSuccess;
  }

  /**
   * @brief Get volume from playback stream
   * @return model::Volume Volume percentage (in a range between 0.f and 1.f)
   */
  model::Volume GetVolume() const override { return volume_; }

  /**
   * @brief Update audio filters in the filter chain (used for equalization)
   *
   * @param filters Audio filters
   * @return error::Code Decoder error converted to application error code
   */
  error::Code UpdateFilters(const model::EqualizerPreset& filters) override {
    return error::kSuccess;
  }

  /* ******************************************************************************************** */
  //! Variables
 private:
  model::Volume volume_;  //!< Playback stream volume
  int64_t position_;      //!< Audio position
};

}  // namespace driver
#endif  // INCLUDE_DEBUG_DUMMY_DECODER_H_
