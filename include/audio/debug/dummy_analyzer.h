/**
 * \file
 * \brief Dummy class for audio analyzer support
 */

#ifndef INCLUDE_AUDIO_DEBUG_DUMMY_ANALYZER_H_
#define INCLUDE_AUDIO_DEBUG_DUMMY_ANALYZER_H_

#include "audio/base/analyzer.h"
#include "model/application_error.h"

namespace driver {

/**
 * @brief Dummy implementation
 */
class DummyAnalyzer : public Analyzer {
 public:
  /**
   * @brief Construct a new Analyzer object
   */
  DummyAnalyzer() = default;

  /**
   * @brief Destroy the Analyzer object
   */
  virtual ~DummyAnalyzer() = default;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Initialize internal structures for audio analysis
   *
   * @param output_size Size for output vector from Execute
   */
  error::Code Init(int output_size) override {
    output_size_ = output_size;
    return error::kSuccess;
  }

  /**
   * @brief Run FFT on input vector to get information about audio in the frequency domain
   *
   * @param in Input vector with audio raw data (signal amplitude)
   * @param size Input vector size
   * @param out Output vector where each entry represents a frequency bar
   */
  error::Code Execute(double *in, int size, double *out) override { return error::kSuccess; }

  /**
   * @brief Get internal buffer size
   *
   * @return Maximum size for input vector
   */
  int GetBufferSize() override { return kBufferSize; }

  /**
   * @brief Get output buffer size
   *
   * @return Size for output vector (considering number of bars multiplied per number of channels)
   */
  int GetOutputSize() override { return output_size_; }

  /* *********************************************************************************************/
  //! Default Constants
 private:
  static constexpr int kBufferSize = 1024;  //!< Base size for buffers

  /* ******************************************************************************************** */
  //! Variables
 private:
  int output_size_;  //!< Maximum output size from audio analysis
};

}  // namespace driver
#endif  // INCLUDE_AUDIO_DEBUG_DUMMY_ANALYZER_H_
