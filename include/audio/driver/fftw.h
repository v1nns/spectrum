/**
 * \file
 * \brief  Class to support using FFTW3
 */

#ifndef INCLUDE_AUDIO_DRIVER_FFTW_H_
#define INCLUDE_AUDIO_DRIVER_FFTW_H_

#include <fftw3.h>

#include <memory>

#include "model/application_error.h"

namespace driver {

/**
 * @brief Provides an interface to apply frequency analysis on audio samples by using FFT
 */
class FFTW {
 public:
  /**
   * @brief Construct a new FFTW object
   */
  FFTW();

  /**
   * @brief Destroy the FFTW object
   */
  virtual ~FFTW();

  /* ******************************************************************************************** */
  //! Public API
 public:
  error::Code Init();
  error::Code Execute(double *in, int size, double *out);

  /* ******************************************************************************************** */
  //! Custom declarations with deleters
 private:
  struct PlanDeleter {
    void operator()(fftw_plan *p) const { fftw_destroy_plan(*p); }
  };

  struct ComplexDeleter {
    void operator()(fftw_complex *p) const { fftw_free(p); }
  };

  using FFTPlan = std::unique_ptr<fftw_plan, PlanDeleter>;

  using FFTComplex = std::unique_ptr<fftw_complex, ComplexDeleter>;

  /* ******************************************************************************************** */
  //! Default Constants

  static constexpr int kBufferSize = 1024;
  static constexpr int kNumberBars = 10;
  static constexpr int kNumberChannels = 2;

  static constexpr int kLowCutOff = 50;      //!< Low frequency to cut off (in Hz)
  static constexpr int kHighCutOff = 10000;  //!< High frequency to cut off (in Hz)

  static constexpr int kSampleRate = 44100;

  static constexpr float kNoiseReduction = 0.77f;

  /* ******************************************************************************************** */
  //! Variables
 private:
  /**
   * @brief Audio frequency analysis
   */
  struct FreqAnalysis {
    int buffer_size;                     // FFTbassbufferSize;
    fftw_plan plan_left, plan_right;     // p_bass_l, p_bass_r;
    fftw_complex *out_left, *out_right;  // *out_bass_l, *out_bass_r;
    double *multiplier;                  // *bass_multiplier;
    double *in_raw_left, *in_raw_right;  // *in_bass_r_raw, *in_bass_l_raw;
    double *in_left, *in_right;          // *in_bass_r, *in_bass_l;
  };

  FreqAnalysis bass_, mid_, treble_;

  //! Input buffer
  double input_size;  // input_buffer_size
  double *input;      // input_buffer

  //! Still gotta understand
  double *prev_cava_out, *cava_mem, *cava_peak;
  int *cava_fall;

  float *cut_off_frequency;
  int bass_cut_off_bar;
  int treble_cut_off_bar;

  double *eq;
  int *FFTbuffer_lower_cut_off;
  int *FFTbuffer_upper_cut_off;

  double framerate;
  int frame_skip;

  double sens;
  int sens_init;
};

}  // namespace driver
#endif  // INCLUDE_AUDIO_DRIVER_FFTW_H_
