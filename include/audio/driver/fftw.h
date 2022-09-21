/**
 * \file
 * \brief  Class to support using FFTW3
 */

#ifndef INCLUDE_AUDIO_DRIVER_FFTW_H_
#define INCLUDE_AUDIO_DRIVER_FFTW_H_

#include <fftw3.h>

#include <memory>
#include <vector>

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
  struct RealDeleter {
    void operator()(double *p) const { fftw_free(p); }
  };

  struct ComplexDeleter {
    void operator()(fftw_complex *p) const { fftw_free(p); }
  };

  using FFTReal = std::unique_ptr<double, RealDeleter>;
  using FFTComplex = std::unique_ptr<fftw_complex, ComplexDeleter>;

  /**
   * @brief Audio frequency analysis
   */
  struct FreqAnalysis {
    int buffer_size;                    // FFTbassbufferSize;
    fftw_plan plan_left, plan_right;    // p_bass_l, p_bass_r;
    FFTComplex out_left, out_right;     // *out_bass_l, *out_bass_r;
    FFTReal multiplier;                 // *bass_multiplier;
    FFTReal in_raw_left, in_raw_right;  // *in_bass_r_raw, *in_bass_l_raw;
    FFTReal in_left, in_right;          // *in_bass_r, *in_bass_l;
  };

  /* ******************************************************************************************** */
  //! Private methods
 private:
  void CreateHannWindow(FreqAnalysis &analysis);
  void CreateFftwStructure(FreqAnalysis &analysis);
  void CreateBuffers();
  void CalculateFreqs();

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
  FreqAnalysis bass_, mid_, treble_;

  //! Input buffer
  double input_size;           //!< input_buffer_size
  std::vector<double> input_;  //!< input_buffer

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
