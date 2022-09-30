/**
 * \file
 * \brief Interface class for audio analyzer support
 */

#ifndef INCLUDE_AUDIO_BASE_ANALYZER_H_
#define INCLUDE_AUDIO_BASE_ANALYZER_H_

#include "model/application_error.h"

namespace driver {

/**
 * @brief Common interface to execute frequency analysis on audio data
 */
class Analyzer {
 public:
  /**
   * @brief Construct a new Analyzer object
   */
  Analyzer() = default;

  /**
   * @brief Destroy the Analyzer object
   */
  virtual ~Analyzer() = default;

  /* ******************************************************************************************** */
  //! Public API

  /**
   * @brief Initialize internal structures for audio analysis
   *
   * @param output_size Size for output vector from Execute
   */
  virtual error::Code Init(int output_size) = 0;

  /**
   * @brief Run FFT on input vector to get information about audio in the frequency domain
   *
   * @param in Input vector with audio raw data (signal amplitude)
   * @param size Input vector size
   * @param out Output vector where each entry represents a frequency bar
   */
  virtual error::Code Execute(double *in, int size, double *out) = 0;

  /**
   * @brief Get internal buffer size
   *
   * @return Maximum size for input vector
   */
  virtual int GetBufferSize() = 0;

  /**
   * @brief Get output buffer size
   *
   * @return Size for output vector (considering number of bars multiplied per number of channels)
   */
  virtual int GetOutputSize() = 0;
};

}  // namespace driver
#endif  // INCLUDE_AUDIO_BASE_ANALYZER_H_
