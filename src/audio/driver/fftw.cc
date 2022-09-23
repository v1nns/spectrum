#include "audio/driver/fftw.h"

#include <cmath>
#include <cstring>
#include <iostream>

namespace driver {

FFTW::FFTW()
    : bass_{},
      mid_{},
      treble_{},
      input_size_{},
      input_{},
      previous_output_{},
      memory_{},
      peak_{},
      fall_{},
      cut_off_freq_{},
      bass_cut_off_{},
      treble_cut_off_{},
      lower_cut_off_per_bar_{},
      upper_cut_off_per_bar_{},
      equalizer_{},
      frame_rate_{},
      frame_skip_{},
      sensitivity_{},
      sens_init{} {}

/* ********************************************************************************************** */

error::Code FFTW::Init() {
  frame_rate_ = 75;
  sensitivity_ = 1;
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

  // Calculate cutoff frequencies and equalize result
  CalculateFreqs();

  return error::kUnknownError;
}

/* ********************************************************************************************** */

error::Code FFTW::Execute(double* in, int size, double* out) {
  int silence = 1;

  // Use raw data to fill input
  FillInputBuffer(in, size, silence);

  // Fill the bass, mid and treble buffers
  ApplyFft(bass_);
  ApplyFft(mid_);
  ApplyFft(treble_);

  // Separate frequency bands
  SeparateFreqBands(out);

  // Smoothing results with sensitivity adjustment
  SmoothingResults(out, silence);

  return error::kUnknownError;
}

/* ********************************************************************************************** */

void FFTW::CreateHannWindow(FreqAnalysis& analysis) {
  analysis.multiplier.reset(fftw_alloc_real(analysis.buffer_size));

  for (int i = 0; i < analysis.buffer_size; i++) {
    analysis.multiplier.get()[i] = 0.5 * (1 - std::cos(2 * M_PI * i / (analysis.buffer_size - 1)));
  }
}

/* ********************************************************************************************** */

void FFTW::CreateFftwStructure(FreqAnalysis& analysis) {
  analysis.in_raw_left.reset(fftw_alloc_real(analysis.buffer_size));
  analysis.in_raw_right.reset(fftw_alloc_real(analysis.buffer_size));

  analysis.in_left.reset(fftw_alloc_real(analysis.buffer_size));
  analysis.in_right.reset(fftw_alloc_real(analysis.buffer_size));

  analysis.out_left.reset(fftw_alloc_complex(analysis.buffer_size / 2 + 1));
  analysis.out_right.reset(fftw_alloc_complex(analysis.buffer_size / 2 + 1));

  fftw_plan p = fftw_plan_dft_r2c_1d(analysis.buffer_size, analysis.in_left.get(),
                                     analysis.out_left.get(), FFTW_MEASURE);
  analysis.plan_left.reset(p);

  p = fftw_plan_dft_r2c_1d(analysis.buffer_size, analysis.in_right.get(), analysis.out_right.get(),
                           FFTW_MEASURE);
  analysis.plan_right.reset(p);

  memset(analysis.in_raw_left.get(), 0, sizeof(double) * analysis.buffer_size);
  memset(analysis.in_raw_right.get(), 0, sizeof(double) * analysis.buffer_size);

  memset(analysis.in_left.get(), 0, sizeof(double) * analysis.buffer_size);
  memset(analysis.in_right.get(), 0, sizeof(double) * analysis.buffer_size);

  memset(*analysis.out_left, 0, (analysis.buffer_size / 2 + 1) * sizeof(fftw_complex));
  memset(*analysis.out_right, 0, (analysis.buffer_size / 2 + 1) * sizeof(fftw_complex));
}

/* ********************************************************************************************** */

void FFTW::CreateBuffers() {
  input_size_ = bass_.buffer_size * kNumberChannels;
  input_ = std::vector<double>(input_size_, 0);

  fall_ = std::vector<int>(kNumberBars * kNumberChannels, 0);
  memory_ = std::vector<double>(kNumberBars * kNumberChannels, 0);
  peak_ = std::vector<double>(kNumberBars * kNumberChannels, 0);
  previous_output_ = std::vector<double>(kNumberBars * kNumberChannels, 0);

  cut_off_freq_ = std::vector<float>(kNumberBars + 1, 0);
  equalizer_ = std::vector<double>(kNumberBars + 1, 0);

  lower_cut_off_per_bar_ = std::vector<int>(kNumberBars + 1);
  upper_cut_off_per_bar_ = std::vector<int>(kNumberBars + 1);
}

/* ********************************************************************************************** */

void FFTW::CalculateFreqs() {
  // Use lower cut off frequencies, to give a better resolution while keeping the responsiveness
  int bass_reference = 100;
  int treble_reference = 500;

  // Calculate frequency constant (used to distribute bars across the frequency band)
  double frequency_constant =
      log10((float)kLowCutOff / (float)kHighCutOff) / (1 / ((float)kNumberBars + 1) - 1);

  float relative_cut_off[treble_.buffer_size];

  bass_cut_off_ = -1;
  treble_cut_off_ = -1;
  int first_bar = 1;
  int first_treble_bar = 0;
  int bar_buffer[kNumberBars + 1];

  for (int n = 0; n < kNumberBars + 1; n++) {
    double bar_distribution_coefficient = frequency_constant * (-1);
    bar_distribution_coefficient += ((float)n + 1) / ((float)kNumberBars + 1) * frequency_constant;
    cut_off_freq_[n] = kHighCutOff * pow(10, bar_distribution_coefficient);

    if (n > 0) {
      if (cut_off_freq_[n - 1] >= cut_off_freq_[n] && cut_off_freq_[n - 1] > bass_reference)
        cut_off_freq_[n] = cut_off_freq_[n - 1] + (cut_off_freq_[n - 1] - cut_off_freq_[n - 2]);
    }

    // Nyquist frequency
    relative_cut_off[n] = cut_off_freq_[n] / ((float)kSampleRate / 2);

    // Numbers that come out of the FFT are very high, so the equalizer is used to "normalize" them
    // by dividing with also a very huge number
    equalizer_[n] = pow(cut_off_freq_[n], 1);
    equalizer_[n] /= pow(2, 18);
    equalizer_[n] /= log2(bass_.buffer_size);

    if (cut_off_freq_[n] < bass_reference) {
      // BASS
      bar_buffer[n] = 1;
      lower_cut_off_per_bar_[n] = relative_cut_off[n] * ((float)bass_.buffer_size / 2);
      bass_cut_off_++;
      treble_cut_off_++;
      if (bass_cut_off_ > 0) first_bar = 0;

      if (lower_cut_off_per_bar_[n] > bass_.buffer_size / 2) {
        lower_cut_off_per_bar_[n] = bass_.buffer_size / 2;
      }
    } else if (cut_off_freq_[n] > bass_reference && cut_off_freq_[n] < treble_reference) {
      // MID
      bar_buffer[n] = 2;
      lower_cut_off_per_bar_[n] = relative_cut_off[n] * ((float)mid_.buffer_size / 2);
      treble_cut_off_++;
      if ((treble_cut_off_ - bass_cut_off_) == 1) {
        first_bar = 1;
        if (n > 0) {
          upper_cut_off_per_bar_[n - 1] = relative_cut_off[n] * ((float)bass_.buffer_size / 2);
        }
      } else {
        first_bar = 0;
      }

      if (lower_cut_off_per_bar_[n] > mid_.buffer_size / 2) {
        lower_cut_off_per_bar_[n] = mid_.buffer_size / 2;
      }
    } else {
      // TREBLE
      bar_buffer[n] = 3;
      lower_cut_off_per_bar_[n] = relative_cut_off[n] * ((float)treble_.buffer_size / 2);
      first_treble_bar++;
      if (first_treble_bar == 1) {
        first_bar = 1;
        if (n > 0) {
          upper_cut_off_per_bar_[n - 1] = relative_cut_off[n] * ((float)mid_.buffer_size / 2);
        }
      } else {
        first_bar = 0;
      }

      if (lower_cut_off_per_bar_[n] > treble_.buffer_size / 2) {
        lower_cut_off_per_bar_[n] = treble_.buffer_size / 2;
      }
    }

    if (n > 0) {
      if (!first_bar) {
        upper_cut_off_per_bar_[n - 1] = lower_cut_off_per_bar_[n] - 1;

        // Pushing the spectrum up if the exponential function gets "clumped" in the bass and
        // calculating new cut off frequencies
        if (lower_cut_off_per_bar_[n] <= lower_cut_off_per_bar_[n - 1]) {
          // Check if there is room for more first
          int room_for_more = 0;

          if (bar_buffer[n] == 1) {
            if (lower_cut_off_per_bar_[n - 1] + 1 < bass_.buffer_size / 2 + 1) room_for_more = 1;
          } else if (bar_buffer[n] == 2) {
            if (lower_cut_off_per_bar_[n - 1] + 1 < mid_.buffer_size / 2 + 1) room_for_more = 1;
          } else if (bar_buffer[n] == 3) {
            if (lower_cut_off_per_bar_[n - 1] + 1 < treble_.buffer_size / 2 + 1) room_for_more = 1;
          }

          if (room_for_more) {
            // Push the spectrum up
            lower_cut_off_per_bar_[n] = lower_cut_off_per_bar_[n - 1] + 1;
            upper_cut_off_per_bar_[n - 1] = lower_cut_off_per_bar_[n] - 1;

            // Calculate new cut off frequency
            switch (bar_buffer[n]) {
              case 1:
                relative_cut_off[n] =
                    (float)(lower_cut_off_per_bar_[n]) / ((float)bass_.buffer_size / 2);
                break;
              case 2:
                relative_cut_off[n] =
                    (float)(lower_cut_off_per_bar_[n]) / ((float)mid_.buffer_size / 2);
                break;
              case 3:
                relative_cut_off[n] =
                    (float)(lower_cut_off_per_bar_[n]) / ((float)treble_.buffer_size / 2);
                break;
            }

            cut_off_freq_[n] = relative_cut_off[n] * ((float)kSampleRate / 2);
          }
        }
      } else {
        if (upper_cut_off_per_bar_[n - 1] <= lower_cut_off_per_bar_[n - 1])
          upper_cut_off_per_bar_[n - 1] = lower_cut_off_per_bar_[n - 1] + 1;
      }
    }
  }
}

/* ********************************************************************************************** */

void FFTW::FillInputBuffer(double* in, int& size, int& silence) {
  if (size > input_size_) size = input_size_;

  if (size > 0) {
    frame_rate_ -= frame_rate_ / 64;
    frame_rate_ += (double)((float)(kSampleRate * kNumberChannels * frame_skip_) / size) / 64;
    frame_skip_ = 1;

    // Shifting input buffer
    for (uint16_t n = input_size_ - 1; n >= size; n--) {
      input_[n] = input_[n - size];
    }

    // Fill the input buffer
    for (uint16_t n = 0; n < size; n++) {
      input_[size - n - 1] = in[n];
      if (in[n]) {
        silence = 0;
      }
    }
  } else {
    frame_skip_++;
  }
}

/* ********************************************************************************************** */

void FFTW::ApplyFft(FreqAnalysis& analysis) {
  for (int i = 0; i < analysis.buffer_size; i++) {
    analysis.in_raw_right.get()[i] = input_[i * 2];
    analysis.in_raw_left.get()[i] = input_[i * 2 + 1];
  }

  // Hann Window
  for (int j = 0; j < analysis.buffer_size; j++) {
    analysis.in_left.get()[j] = analysis.multiplier.get()[j] * analysis.in_raw_left.get()[j];
    analysis.in_right.get()[j] = analysis.multiplier.get()[j] * analysis.in_raw_right.get()[j];
  }

  fftw_execute(analysis.plan_left.get());
  fftw_execute(analysis.plan_right.get());
}

/* ********************************************************************************************** */

void FFTW::SeparateFreqBands(double* out) {
  for (int n = 0; n < kNumberBars; n++) {
    double temp_l = 0;
    double temp_r = 0;

    // Add upp FFT values within bands
    for (int i = lower_cut_off_per_bar_[n]; i <= upper_cut_off_per_bar_[n]; i++) {
      if (n <= bass_cut_off_) {
        temp_l += hypot(bass_.out_left.get()[i][0], bass_.out_left.get()[i][1]);
        temp_r += hypot(bass_.out_right.get()[i][0], bass_.out_right.get()[i][1]);

      } else if (n > bass_cut_off_ && n <= treble_cut_off_) {
        temp_l += hypot(mid_.out_left.get()[i][0], mid_.out_left.get()[i][1]);
        temp_r += hypot(mid_.out_right.get()[i][0], mid_.out_right.get()[i][1]);

      } else if (n > treble_cut_off_) {
        temp_l += hypot(treble_.out_left.get()[i][0], treble_.out_left.get()[i][1]);
        temp_r += hypot(treble_.out_right.get()[i][0], treble_.out_right.get()[i][1]);
      }
    }

    // Getting average multiply with equalizer
    temp_l /= upper_cut_off_per_bar_[n] - lower_cut_off_per_bar_[n] + 1;
    temp_l *= equalizer_[n];
    out[n] = temp_l;

    temp_r /= upper_cut_off_per_bar_[n] - lower_cut_off_per_bar_[n] + 1;
    temp_r *= equalizer_[n];
    out[n + kNumberBars] = temp_r;
  }
}

/* ********************************************************************************************** */

void FFTW::SmoothingResults(double* out, int silence) {
  // Applying sensitivity adjustment
  for (int n = 0; n < kNumberBars * kNumberChannels; n++) {
    out[n] *= sensitivity_;
  }

  // Smoothing based on frame rate
  int overshoot = 0;
  double gravity_mod = pow((60 / frame_rate_), 2.5) * 1.54 / kNoiseReduction;

  if (gravity_mod < 1) gravity_mod = 1;

  for (int n = 0; n < kNumberBars * kNumberChannels; n++) {
    // Falloff
    if (out[n] < previous_output_[n]) {
      out[n] = peak_[n] * (1000 - (fall_[n] * fall_[n] * gravity_mod)) / 1000;

      if (out[n] < 0) out[n] = 0;
      fall_[n]++;
    } else {
      peak_[n] = out[n];
      fall_[n] = 0;
    }
    previous_output_[n] = out[n];

    // Integral
    out[n] = memory_[n] * kNoiseReduction + out[n];
    memory_[n] = out[n];

    double diff = 1000 - out[n];
    if (diff < 0) diff = 0;
    double div = 1 / (diff + 1);
    memory_[n] = memory_[n] * (1 - div / 20);

    // Check if we overshoot target height
    if (out[n] > 1000) {
      overshoot = 1;
    }
    out[n] /= 1000;
  }

  // Calculating automatic sensitivity adjustment
  if (overshoot) {
    sensitivity_ = sensitivity_ * 0.98;
    sens_init = 0;
  } else {
    if (!silence) {
      sensitivity_ = sensitivity_ * 1.001;
      if (sens_init) sensitivity_ = sensitivity_ * 1.1;
    }
  }
}

}  // namespace driver
