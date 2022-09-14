#include "audio/driver/fftw.h"

#include <cmath>
#include <cstring>

namespace driver {

FFTW::FFTW() : bass_{}, mid_{}, treble_{} {}

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

  return error::kUnknownError;
}

}  // namespace driver
