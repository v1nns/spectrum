#include "audio/driver/fftw.h"

#include <cmath>
#include <cstring>
#include <iostream>

namespace driver {

FFTW::FFTW()
    : bass_{},
      mid_{},
      treble_{},
      input_size{},
      input_{},
      prev_cava_out{},
      cava_mem{},
      cava_peak{},
      cava_fall{},
      cut_off_frequency{},
      bass_cut_off_bar{},
      treble_cut_off_bar{},
      eq{},
      FFTbuffer_lower_cut_off{},
      FFTbuffer_upper_cut_off{},
      framerate{},
      frame_skip{},
      sens{},
      sens_init{} {}

/* ********************************************************************************************** */

FFTW::~FFTW() {
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
  framerate = 75;
  sens = 1;
  bass_.buffer_size = kBufferSize * 8;
  mid_.buffer_size = kBufferSize * 4;
  treble_.buffer_size = kBufferSize;

  // Hann Window calculate multipliers
  CreateHannWindow(bass_);
  CreateHannWindow(mid_);
  CreateHannWindow(treble_);

  // Allocate FFTW structures
  CreateFftwStructure(bass_);
  CreateFftwStructure(mid_);
  CreateFftwStructure(treble_);

  // Create input buffers
  CreateBuffers();

  // Calculate cutoff frequencies and eq
  CalculateFreqs();

  return error::kUnknownError;
}

/* ********************************************************************************************** */

error::Code FFTW::Execute(double* in, int size, double* out) {
  std::cout << "step 1" << std::endl;
  if (size > input_size) size = input_size;

  std::cout << "step 2" << std::endl;
  int silence = 1;
  if (size > 0) {
    framerate -= framerate / 64;
    framerate += (double)((kSampleRate * kNumberChannels * frame_skip) / size) / 64;
    frame_skip = 1;

    // shifting input buffer
    for (uint16_t n = input_size - 1; n >= size; n--) {
      input_[n] = input_[n - size];
    }

    // fill the input buffer
    for (uint16_t n = 0; n < size; n++) {
      input_[size - n - 1] = in[n];
      if (in[n]) {
        silence = 0;
      }
    }
  } else {
    frame_skip++;
  }

  std::cout << "step 3" << std::endl;

  // fill the bass, mid and treble buffers
  auto fill_and_run_fft = [&](FreqAnalysis& analysis) mutable {
    std::cout << "step 3.1" << std::endl;
    for (int i = 0; i < analysis.buffer_size; i++) {
      analysis.in_raw_right.get()[i] = input_[i * 2];
      analysis.in_raw_left.get()[i] = input_[i * 2 + 1];
    }
    std::cout << "step 3.2" << std::endl;
    // Hann Window
    for (int j = 0; j < analysis.buffer_size; j++) {
      analysis.in_left.get()[j] = analysis.multiplier.get()[j] * analysis.in_raw_left.get()[j];
      analysis.in_right.get()[j] = analysis.multiplier.get()[j] * analysis.in_raw_right.get()[j];
    }
    std::cout << "step 3.3" << std::endl;
    fftw_execute(analysis.plan_left);
    fftw_execute(analysis.plan_right);
  };

  fill_and_run_fft(bass_);
  fill_and_run_fft(mid_);
  fill_and_run_fft(treble_);

  std::cout << "step 4" << std::endl;

  // process: separate frequency bands
  for (int n = 0; n < kNumberBars; n++) {
    double temp_l = 0;
    double temp_r = 0;

    // process: add upp FFT values within bands
    for (int i = FFTbuffer_lower_cut_off[n]; i <= FFTbuffer_upper_cut_off[n]; i++) {
      if (n <= bass_cut_off_bar) {
        temp_l += hypot(bass_.out_left.get()[i][0], bass_.out_left.get()[i][1]);
        temp_r += hypot(bass_.out_right.get()[i][0], bass_.out_right.get()[i][1]);

      } else if (n > bass_cut_off_bar && n <= treble_cut_off_bar) {
        temp_l += hypot(mid_.out_left.get()[i][0], mid_.out_left.get()[i][1]);
        temp_r += hypot(mid_.out_right.get()[i][0], mid_.out_right.get()[i][1]);

      } else if (n > treble_cut_off_bar) {
        temp_l += hypot(treble_.out_left.get()[i][0], treble_.out_left.get()[i][1]);
        temp_r += hypot(treble_.out_right.get()[i][0], treble_.out_right.get()[i][1]);
      }
    }

    // getting average multiply with eq
    temp_l /= FFTbuffer_upper_cut_off[n] - FFTbuffer_lower_cut_off[n] + 1;
    temp_l *= eq[n];
    out[n] = temp_l;

    temp_r /= FFTbuffer_upper_cut_off[n] - FFTbuffer_lower_cut_off[n] + 1;
    temp_r *= eq[n];
    out[n + kNumberBars] = temp_r;
  }

  std::cout << "step 5" << std::endl;

  // applying sens
  for (int n = 0; n < kNumberBars * kNumberChannels; n++) {
    out[n] *= sens;
  }

  // process [smoothing]
  int overshoot = 0;
  double gravity_mod = pow((60 / framerate), 2.5) * 1.54 / kNoiseReduction;

  if (gravity_mod < 1) gravity_mod = 1;

  std::cout << "step 6" << std::endl;

  for (int n = 0; n < kNumberBars * kNumberChannels; n++) {
    // process [smoothing]: falloff
    if (out[n] < prev_cava_out[n]) {
      out[n] = cava_peak[n] * (1000 - (cava_fall[n] * cava_fall[n] * gravity_mod)) / 1000;

      if (out[n] < 0) out[n] = 0;
      cava_fall[n]++;
    } else {
      cava_peak[n] = out[n];
      cava_fall[n] = 0;
    }
    prev_cava_out[n] = out[n];

    // process [smoothing]: integral
    out[n] = cava_mem[n] * kNoiseReduction + out[n];
    cava_mem[n] = out[n];

    double diff = 1000 - out[n];
    if (diff < 0) diff = 0;
    double div = 1 / (diff + 1);
    cava_mem[n] = cava_mem[n] * (1 - div / 20);

    // check if we overshoot target height
    if (out[n] > 1000) {
      overshoot = 1;
    }
    out[n] /= 1000;
  }

  std::cout << "step 7" << std::endl;

  // calculating automatic sense adjustment
  if (overshoot) {
    sens = sens * 0.98;
    sens_init = 0;
  } else {
    if (!silence) {
      sens = sens * 1.001;
      if (sens_init) sens = sens * 1.1;
    }
  }

  return error::kUnknownError;
}

/* ********************************************************************************************** */

void FFTW::CreateHannWindow(FreqAnalysis& analysis) {
  analysis.multiplier = FFTReal(fftw_alloc_real(analysis.buffer_size));

  for (int i = 0; i < analysis.buffer_size; i++) {
    analysis.multiplier.get()[i] = 0.5 * (1 - std::cos(2 * M_PI * i / (analysis.buffer_size - 1)));
  }
}

/* ********************************************************************************************** */

void FFTW::CreateFftwStructure(FreqAnalysis& analysis) {
  analysis.in_raw_left = FFTReal(fftw_alloc_real(analysis.buffer_size));
  analysis.in_raw_right = FFTReal(fftw_alloc_real(analysis.buffer_size));

  analysis.in_left = FFTReal(fftw_alloc_real(analysis.buffer_size));
  analysis.in_right = FFTReal(fftw_alloc_real(analysis.buffer_size));

  analysis.out_left = FFTComplex(fftw_alloc_complex(analysis.buffer_size / 2 + 1));
  analysis.out_right = FFTComplex(fftw_alloc_complex(analysis.buffer_size / 2 + 1));

  analysis.plan_left = fftw_plan_dft_r2c_1d(analysis.buffer_size, analysis.in_left.get(),
                                            analysis.out_left.get(), FFTW_MEASURE);
  analysis.plan_right = fftw_plan_dft_r2c_1d(analysis.buffer_size, analysis.in_right.get(),
                                             analysis.out_right.get(), FFTW_MEASURE);

  memset(analysis.in_raw_left.get(), 0, sizeof(double) * analysis.buffer_size);
  memset(analysis.in_raw_right.get(), 0, sizeof(double) * analysis.buffer_size);

  memset(analysis.in_left.get(), 0, sizeof(double) * analysis.buffer_size);
  memset(analysis.in_right.get(), 0, sizeof(double) * analysis.buffer_size);

  memset(*analysis.out_left, 0, (analysis.buffer_size / 2 + 1) * sizeof(fftw_complex));
  memset(*analysis.out_right, 0, (analysis.buffer_size / 2 + 1) * sizeof(fftw_complex));
}

/* ********************************************************************************************** */

void FFTW::CreateBuffers() {
  input_size = bass_.buffer_size * kNumberChannels;
  input_ = std::vector<double>(input_size, 0);

  cava_fall = (int*)malloc(sizeof(int) * kNumberBars * kNumberChannels);
  cava_mem = (double*)malloc(sizeof(double) * kNumberBars * kNumberChannels);
  cava_peak = (double*)malloc(sizeof(double) * kNumberBars * kNumberChannels);
  prev_cava_out = (double*)malloc(sizeof(double) * kNumberBars * kNumberChannels);

  memset(cava_fall, 0, sizeof(int) * kNumberBars * kNumberChannels);
  memset(cava_mem, 0, sizeof(double) * kNumberBars * kNumberChannels);
  memset(cava_peak, 0, sizeof(double) * kNumberBars * kNumberChannels);
  memset(prev_cava_out, 0, sizeof(double) * kNumberBars * kNumberChannels);

  cut_off_frequency = (float*)malloc((kNumberBars + 1) * sizeof(float));
  eq = (double*)malloc((kNumberBars + 1) * sizeof(double));

  FFTbuffer_lower_cut_off = (int*)malloc((kNumberBars + 1) * sizeof(int));
  FFTbuffer_upper_cut_off = (int*)malloc((kNumberBars + 1) * sizeof(int));
}

/* ********************************************************************************************** */

void FFTW::CalculateFreqs() {
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

    // nyquist freq
    relative_cut_off[n] = cut_off_frequency[n] / ((float)kSampleRate / 2);

    // the numbers that come out of the FFT are very high
    // the EQ is used to "normalize" them by dividing with this verry huge number
    eq[n] = pow(cut_off_frequency[n], 1);
    eq[n] /= pow(2, 18);
    eq[n] /= log2(bass_.buffer_size);

    if (cut_off_frequency[n] < bass_cut_off) {
      // BASS
      bar_buffer[n] = 1;
      FFTbuffer_lower_cut_off[n] = relative_cut_off[n] * ((float)bass_.buffer_size / 2);
      bass_cut_off_bar++;
      treble_cut_off_bar++;
      if (bass_cut_off_bar > 0) first_bar = 0;

      if (FFTbuffer_lower_cut_off[n] > bass_.buffer_size / 2) {
        FFTbuffer_lower_cut_off[n] = bass_.buffer_size / 2;
      }
    } else if (cut_off_frequency[n] > bass_cut_off && cut_off_frequency[n] < treble_cut_off) {
      // MID
      bar_buffer[n] = 2;
      FFTbuffer_lower_cut_off[n] = relative_cut_off[n] * ((float)mid_.buffer_size / 2);
      treble_cut_off_bar++;
      if ((treble_cut_off_bar - bass_cut_off_bar) == 1) {
        first_bar = 1;
        if (n > 0) {
          FFTbuffer_upper_cut_off[n - 1] = relative_cut_off[n] * ((float)bass_.buffer_size / 2);
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
      FFTbuffer_lower_cut_off[n] = relative_cut_off[n] * ((float)treble_.buffer_size / 2);
      first_treble_bar++;
      if (first_treble_bar == 1) {
        first_bar = 1;
        if (n > 0) {
          FFTbuffer_upper_cut_off[n - 1] = relative_cut_off[n] * ((float)mid_.buffer_size / 2);
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
        // bass and calculating new cut off frequencies
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
            switch (bar_buffer[n]) {
              case 1:
                relative_cut_off[n] =
                    (float)(FFTbuffer_lower_cut_off[n]) / ((float)bass_.buffer_size / 2);
                break;
              case 2:
                relative_cut_off[n] =
                    (float)(FFTbuffer_lower_cut_off[n]) / ((float)mid_.buffer_size / 2);
                break;
              case 3:
                relative_cut_off[n] =
                    (float)(FFTbuffer_lower_cut_off[n]) / ((float)treble_.buffer_size / 2);
                break;
            }

            cut_off_frequency[n] = relative_cut_off[n] * ((float)kSampleRate / 2);
          }
        }
      } else {
        if (FFTbuffer_upper_cut_off[n - 1] <= FFTbuffer_lower_cut_off[n - 1])
          FFTbuffer_upper_cut_off[n - 1] = FFTbuffer_lower_cut_off[n - 1] + 1;
      }
    }
  }
}

}  // namespace driver
