#include "audio/driver/fftw.h"

#include <cmath>
#include <cstring>

namespace driver {

FFTW::FFTW()
    : bass_{},
      mid_{},
      treble_{},
      input_size{},
      input{},
      prev_cava_out{},
      cava_mem{},
      cava_peak{},
      cava_fall{},
      cut_off_frequency{},
      bass_cut_off_bar{},
      treble_cut_off_bar{},
      eq{},
      FFTbuffer_lower_cut_off{},
      FFTbuffer_upper_cut_off{} {}

/* ********************************************************************************************** */

FFTW::~FFTW() {
  auto free_freqs = [&](FreqAnalysis& dummy) mutable {
    free(dummy.multiplier);
    free(dummy.in_raw_left);
    free(dummy.in_raw_right);
    free(dummy.in_left);
    free(dummy.in_right);
  };

  free_freqs(bass_);
  free_freqs(mid_);
  free_freqs(treble_);

  free(input);

  free(prev_cava_out);
  free(cava_mem);
  free(cava_peak);
  free(cava_fall);

  free(cut_off_frequency);

  free(eq);

  free(FFTbuffer_lower_cut_off);
  free(FFTbuffer_upper_cut_off);
}

/* ********************************************************************************************** */

error::Code FFTW::Init() {
  bass_.buffer_size = kBufferSize * 8;
  mid_.buffer_size = kBufferSize * 4;
  treble_.buffer_size = kBufferSize;

  // Hann Window calculate multipliers
  auto create_hann = [&](double* array, int size) mutable {
    array = (double*)malloc(size * sizeof(double));

    for (int i = 0; i < size; i++) {
      array[i] = 0.5 * (1 - std::cos(2 * M_PI * i / (size - 1)));
    }
  };

  create_hann(bass_.multiplier, bass_.buffer_size);
  create_hann(mid_.multiplier, mid_.buffer_size);
  create_hann(treble_.multiplier, treble_.buffer_size);

  // Allocate FFTW structures
  auto create_fftw = [&](FreqAnalysis& analysis) mutable {
    analysis.in_raw_left = fftw_alloc_real(analysis.buffer_size);
    analysis.in_raw_right = fftw_alloc_real(analysis.buffer_size);

    analysis.in_left = fftw_alloc_real(analysis.buffer_size);
    analysis.in_right = fftw_alloc_real(analysis.buffer_size);

    analysis.out_left = fftw_alloc_complex(analysis.buffer_size / 2 + 1);
    analysis.out_right = fftw_alloc_complex(analysis.buffer_size / 2 + 1);

    analysis.plan_left = fftw_plan_dft_r2c_1d(analysis.buffer_size, analysis.in_left,
                                              analysis.out_left, FFTW_MEASURE);
    analysis.plan_right = fftw_plan_dft_r2c_1d(analysis.buffer_size, analysis.in_right,
                                               analysis.out_right, FFTW_MEASURE);

    memset(analysis.in_raw_left, 0, sizeof(double) * analysis.buffer_size);
    memset(analysis.in_raw_right, 0, sizeof(double) * analysis.buffer_size);

    memset(analysis.in_left, 0, sizeof(double) * analysis.buffer_size);
    memset(analysis.in_right, 0, sizeof(double) * analysis.buffer_size);

    memset(analysis.out_left, 0, (analysis.buffer_size / 2 + 1) * sizeof(fftw_complex));
    memset(analysis.out_right, 0, (analysis.buffer_size / 2 + 1) * sizeof(fftw_complex));
  };

  create_fftw(bass_);
  create_fftw(mid_);
  create_fftw(treble_);

  input_size = bass_.buffer_size * kNumberChannels;
  input = (double*)malloc(input_size * sizeof(double));

  memset(input, 0, sizeof(double) * input_size);

  memset(cava_fall, 0, sizeof(int) * kNumberBars * kNumberChannels);
  memset(cava_mem, 0, sizeof(double) * kNumberBars * kNumberChannels);
  memset(cava_peak, 0, sizeof(double) * kNumberBars * kNumberChannels);
  memset(prev_cava_out, 0, sizeof(double) * kNumberBars * kNumberChannels);

  cut_off_frequency = (float*)malloc((kNumberBars + 1) * sizeof(float));
  eq = (double*)malloc((kNumberBars + 1) * sizeof(double));
  FFTbuffer_lower_cut_off = (int*)malloc((kNumberBars + 1) * sizeof(int));
  FFTbuffer_upper_cut_off = (int*)malloc((kNumberBars + 1) * sizeof(int));

  // process: calculate cutoff frequencies and eq
  int bass_cut_off = 100;
  int treble_cut_off = 500;

  // calculate frequency constant (used to distribute bars across the frequency band)
  double frequency_constant =
      log10((float)kLowCutOff / (float)kHighCutOff) / (1 / ((float)kNumberBars + 1) - 1);

  float relative_cut_off[treble_.buffer_size];

  bass_cut_off_bar = -1;
  treble_cut_off_bar = -1;
  int first_bar = 1;
  int first_treble_bar = 0;
  int bar_buffer[kNumberBars + 1];

  for (int n = 0; n < kNumberBars + 1; n++) {
    double bar_distribution_coefficient = frequency_constant * (-1);
    bar_distribution_coefficient += ((float)n + 1) / ((float)kNumberBars + 1) * frequency_constant;
    cut_off_frequency[n] = kHighCutOff * pow(10, bar_distribution_coefficient);

    if (n > 0) {
      if (cut_off_frequency[n - 1] >= cut_off_frequency[n] &&
          cut_off_frequency[n - 1] > bass_cut_off)
        cut_off_frequency[n] =
            cut_off_frequency[n - 1] + (cut_off_frequency[n - 1] - cut_off_frequency[n - 2]);
    }

    relative_cut_off[n] = cut_off_frequency[n] / (kSampleRate / 2);
    // remember nyquist!, per my calculations this should be rate/2
    // and nyquist freq in M/2 but testing shows it is not...
    // or maybe the nq freq is in M/4

    eq[n] = pow(cut_off_frequency[n], 1);

    // the numbers that come out of the FFT are verry high
    // the EQ is used to "normalize" them by dividing with this verry huge number
    eq[n] /= pow(2, 18);

    eq[n] /= log2(bass_.buffer_size);

    if (cut_off_frequency[n] < bass_cut_off) {
      // BASS
      bar_buffer[n] = 1;
      FFTbuffer_lower_cut_off[n] = relative_cut_off[n] * (bass_.buffer_size / 2);
      bass_cut_off_bar++;
      treble_cut_off_bar++;
      if (bass_cut_off_bar > 0) first_bar = 0;

      if (FFTbuffer_lower_cut_off[n] > bass_.buffer_size / 2) {
        FFTbuffer_lower_cut_off[n] = bass_.buffer_size / 2;
      }
    } else if (cut_off_frequency[n] > bass_cut_off && cut_off_frequency[n] < treble_cut_off) {
      // MID
      bar_buffer[n] = 2;
      FFTbuffer_lower_cut_off[n] = relative_cut_off[n] * (mid_.buffer_size / 2);
      treble_cut_off_bar++;
      if ((treble_cut_off_bar - bass_cut_off_bar) == 1) {
        first_bar = 1;
        if (n > 0) {
          FFTbuffer_upper_cut_off[n - 1] = relative_cut_off[n] * (bass_.buffer_size / 2);
        }
      } else {
        first_bar = 0;
      }

      if (FFTbuffer_lower_cut_off[n] > mid_.buffer_size / 2) {
        FFTbuffer_lower_cut_off[n] = mid_.buffer_size / 2;
      }
    } else {
      // TREBLE
      bar_buffer[n] = 3;
      FFTbuffer_lower_cut_off[n] = relative_cut_off[n] * (treble_.buffer_size / 2);
      first_treble_bar++;
      if (first_treble_bar == 1) {
        first_bar = 1;
        if (n > 0) {
          FFTbuffer_upper_cut_off[n - 1] = relative_cut_off[n] * (mid_.buffer_size / 2);
        }
      } else {
        first_bar = 0;
      }

      if (FFTbuffer_lower_cut_off[n] > treble_.buffer_size / 2) {
        FFTbuffer_lower_cut_off[n] = treble_.buffer_size / 2;
      }
    }

    if (n > 0) {
      if (!first_bar) {
        FFTbuffer_upper_cut_off[n - 1] = FFTbuffer_lower_cut_off[n] - 1;

        // pushing the spectrum up if the exponential function gets "clumped" in the
        // bass and caluclating new cut off frequencies
        if (FFTbuffer_lower_cut_off[n] <= FFTbuffer_lower_cut_off[n - 1]) {
          // check if there is room for more first
          int room_for_more = 0;

          if (bar_buffer[n] == 1) {
            if (FFTbuffer_lower_cut_off[n - 1] + 1 < bass_.buffer_size / 2 + 1) room_for_more = 1;
          } else if (bar_buffer[n] == 2) {
            if (FFTbuffer_lower_cut_off[n - 1] + 1 < mid_.buffer_size / 2 + 1) room_for_more = 1;
          } else if (bar_buffer[n] == 3) {
            if (FFTbuffer_lower_cut_off[n - 1] + 1 < treble_.buffer_size / 2 + 1) room_for_more = 1;
          }

          if (room_for_more) {
            // push the spectrum up
            FFTbuffer_lower_cut_off[n] = FFTbuffer_lower_cut_off[n - 1] + 1;
            FFTbuffer_upper_cut_off[n - 1] = FFTbuffer_lower_cut_off[n] - 1;

            // calculate new cut off frequency
            if (bar_buffer[n] == 1)
              relative_cut_off[n] =
                  (float)(FFTbuffer_lower_cut_off[n]) / ((float)bass_.buffer_size / 2);
            else if (bar_buffer[n] == 2)
              relative_cut_off[n] =
                  (float)(FFTbuffer_lower_cut_off[n]) / ((float)mid_.buffer_size / 2);
            else if (bar_buffer[n] == 3)
              relative_cut_off[n] =
                  (float)(FFTbuffer_lower_cut_off[n]) / ((float)treble_.buffer_size / 2);

            cut_off_frequency[n] = relative_cut_off[n] * ((float)kSampleRate / 2);
          }
        }
      } else {
        if (FFTbuffer_upper_cut_off[n - 1] <= FFTbuffer_lower_cut_off[n - 1])
          FFTbuffer_upper_cut_off[n - 1] = FFTbuffer_lower_cut_off[n - 1] + 1;
      }
    }
  }

  return error::kUnknownError;
}

}  // namespace driver
