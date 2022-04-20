#include <math.h>

#include <chrono>
#include <cmath>
#include <fstream>
#include <ftxui/dom/elements.hpp>
#include <iterator>
#include <string>
#include <thread>
#include <vector>

#include "fftw3.h"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"  // for ScreenInteractive
#include "ftxui/dom/canvas.hpp"

#define PI 3.1415926535897

struct wave_header_t {
  /* RIFF Chunk Descriptor */
  uint8_t RIFF[4];     // RIFF Header Magic header
  uint32_t ChunkSize;  // RIFF Chunk Size
  uint8_t WAVE[4];     // WAVE Header

  /* "FMT" sub-chunk */
  uint8_t Subchunk1ID[4];  // FMT header
  uint32_t Subchunk1Size;  // Size of the FMT chunk
  uint16_t AudioFormat;    // PCM = 1 (i.e. Linear quantization).
                           // Values other than 1 indicate some form of compression.
  uint16_t NumChannels;    // Number of channels 1=Mono 2=Sterio
  uint32_t SampleRate;     // Sampling Frequency in Hz (8000, 44100,...)
  uint32_t ByteRate;       // bytes per second
  uint16_t BlockAlign;     // 2=16-bit mono, 4=16-bit stereo
  uint16_t BitsPerSample;  // Number of bits per sample (8 bits, 16 bits,...)

  /* "data" sub-chunk */
  uint8_t Subchunk2ID[4];  // "data" string
  uint32_t Subchunk2Size;  // Sampled data length
};

std::vector<std::vector<double>> ParseData(wave_header_t &header_) {
  //   std::string full_path{"/home/vinicius/projects/music-analyzer/africa-toto.wav"};
  //   std::string full_path{"/home/vinicius/projects/music-analyzer/another1k.wav"};
  std::string full_path{"/home/vinicius/projects/music-analyzer/beach.wav"};
  std::ifstream file(full_path, std::ios::binary);

  if (!file.good()) {
    // std::cout << "ERROR: this doesn't seem to be a valid .WAV file" << std::endl;
    return std::vector<std::vector<double>>({});
  }

  file.unsetf(std::ios::skipws);
  file.read((char *)&header_, sizeof(wave_header_t));

  std::string chunkId(header_.RIFF, header_.RIFF + 4);
  std::string format(header_.WAVE, header_.WAVE + 4);
  std::string subchunk1Id(header_.Subchunk1ID, header_.Subchunk1ID + 4);

  if (chunkId != "RIFF" || format != "WAVE" || subchunk1Id != "fmt ") {
    // std::cout << "file not supported" << std:endl;
    return std::vector<std::vector<double>>({});
  }

  if (header_.AudioFormat != 1) {
    // std::cout << "ERROR: this is a compressed .WAV file and this library does not support
    // decoding them at present" << std::endl;
    return std::vector<std::vector<double>>({});
  }

  if (header_.NumChannels < 1 || header_.NumChannels > 2) {
    // std::cout << "ERROR: this WAV file seems to be neither mono nor stereo (perhaps multi-track,
    // or corrupted?)" << std::endl;
    return std::vector<std::vector<double>>({});
  }

  if ((header_.ByteRate !=
       (header_.NumChannels * header_.SampleRate * header_.BitsPerSample) / 8)) {
    // std::cout << "ERROR: the header data in this WAV file seems to be inconsistent" << std::endl;
    return std::vector<std::vector<double>>({});
  }

  file.seekg(sizeof(wave_header_t));

  std::istream_iterator<uint8_t> begin(file), end;
  std::vector<uint8_t> raw_data(begin, end);

  int num_bytes_per_sample = header_.BitsPerSample / 8;
  int num_samples = header_.Subchunk2Size / (header_.NumChannels * num_bytes_per_sample);

  std::vector<std::vector<double>> data(header_.NumChannels);

  for (int i = 0; i < num_samples; i++) {
    for (int channel = 0; channel < header_.NumChannels; channel++) {
      int index = (header_.BlockAlign * i) + channel * num_bytes_per_sample;

      switch (header_.BitsPerSample) {
        case 8: {
          int32_t sampleAsInt = (int32_t)raw_data[index];
          double sample = (double)(sampleAsInt - 128) / (double)128.;

          data[channel].push_back(sample);
        } break;
        case 16: {
          int16_t sampleAsInt = (raw_data[index + 1] << 8) | raw_data[index];
          double sample = (double)sampleAsInt / (double)32768.;

          data[channel].push_back(sample);
        } break;
        case 24: {
          int32_t sampleAsInt = 0;
          sampleAsInt = (raw_data[index + 2] << 16) | (raw_data[index + 1] << 8) | raw_data[index];

          // if the 24th bit is set, this is a negative number in 24-bit world
          // so make sure sign is extended to the 32 bit float
          if (sampleAsInt & 0x800000) sampleAsInt = sampleAsInt | ~0xFFFFFF;

          double sample = (double)sampleAsInt / (double)8388608.;

          data[channel].push_back(sample);
        } break;
        default:
          // ERROR!
          break;
      }
    }
  }

  return data;
}

class Graph {
 public:
  explicit Graph(std::vector<double> *y, int num_items)
      : logspace_(), y_(y), num_items_(num_items), m_bar_heights() {
    logspace_.reserve(500);
  }

  void GenLogspace(double width) {
    const double HZ_MIN = 20, HZ_MAX = 20000;

    // Calculate number of extra bins needed between 0 HZ and HZ_MIN
    const size_t left_bins =
        (log10(HZ_MIN) - width * log10(HZ_MIN)) / (log10(HZ_MIN) - log10(HZ_MAX));
    // Generate logspaced frequencies
    logspace_.resize(width);
    const double log_scale = log10(HZ_MAX) / (left_bins + logspace_.size() - 1);
    for (size_t i = left_bins; i < logspace_.size() + left_bins; ++i) {
      logspace_[i - left_bins] = pow(10, i * log_scale);
    }
  }

  double Interpolate(size_t x, size_t h_idx) {
    const double x_next = m_bar_heights[h_idx].first;
    const double h_next = m_bar_heights[h_idx].second;

    double dh = 0;
    if (h_idx == 0) {
      // no data points on left, linear extrap
      if (h_idx < m_bar_heights.size() - 1) {
        const double x_next2 = m_bar_heights[h_idx + 1].first;
        const double h_next2 = m_bar_heights[h_idx + 1].second;
        dh = (h_next2 - h_next) / (x_next2 - x_next);
      }
      return h_next - dh * (x_next - x);
    } else if (h_idx == 1) {
      // one data point on left, linear interp
      const double x_prev = m_bar_heights[h_idx - 1].first;
      const double h_prev = m_bar_heights[h_idx - 1].second;
      dh = (h_next - h_prev) / (x_next - x_prev);
      return h_next - dh * (x_next - x);
    } else if (h_idx < m_bar_heights.size() - 1) {
      // two data points on both sides, cubic interp see
      // https://en.wikipedia.org/wiki/Cubic_Hermite_spline#Interpolation_on_an_arbitrary_interval
      const double x_prev2 = m_bar_heights[h_idx - 2].first;
      const double h_prev2 = m_bar_heights[h_idx - 2].second;
      const double x_prev = m_bar_heights[h_idx - 1].first;
      const double h_prev = m_bar_heights[h_idx - 1].second;
      const double x_next2 = m_bar_heights[h_idx + 1].first;
      const double h_next2 = m_bar_heights[h_idx + 1].second;

      const double m0 = (h_prev - h_prev2) / (x_prev - x_prev2);
      const double m1 = (h_next2 - h_next) / (x_next2 - x_next);
      const double t = (x - x_prev) / (x_next - x_prev);
      const double h00 = 2 * t * t * t - 3 * t * t + 1;
      const double h10 = t * t * t - 2 * t * t + t;
      const double h01 = -2 * t * t * t + 3 * t * t;
      const double h11 = t * t * t - t * t;

      return h00 * h_prev + h10 * (x_next - x_prev) * m0 + h01 * h_next +
             h11 * (x_next - x_prev) * m1;
    }

    // less than two data points on right, no interp, should never happen unless VERY low DFT size
    return h_next;
  }

  double Bin2Hz(size_t bin) { return bin * 44100 / 32768; }

  std::vector<int> operator()(int width, int height) {
    std::vector<int> output;
    m_bar_heights.clear();
    GenLogspace(width);

    const double DYNAMIC_RANGE = 100 - 10, GAIN = 10;

    size_t cur_bin = 0;
    while (cur_bin < num_items_ && Bin2Hz(cur_bin) < logspace_[0]) ++cur_bin;
    for (size_t x = 0; x < width; ++x) {
      double bar_height = 0;

      // accumulate bins
      size_t count = 0;
      // check right bound
      while (cur_bin < num_items_ && Bin2Hz(cur_bin) < logspace_[x]) {
        // check left bound if not first index
        if (x == 0 || Bin2Hz(cur_bin) >= logspace_[x - 1]) {
          bar_height += y_->at(cur_bin);
          ++count;
        }
        ++cur_bin;
      }

      if (count == 0) continue;

      // average bins
      bar_height /= count;

      // log scale bar heights
      bar_height = (20 * log10(bar_height) + DYNAMIC_RANGE + GAIN) / DYNAMIC_RANGE;
      // Scale bar height between 0 and height
      bar_height = bar_height > 0 ? bar_height * height : 0;
      bar_height = bar_height > height ? height : bar_height;

      m_bar_heights.emplace_back(x, bar_height);
    }

    size_t h_idx = 0;
    for (size_t x = 0; x < width; ++x) {
      const size_t i = m_bar_heights[h_idx].first;
      const double bar_height = m_bar_heights[h_idx].second;
      double h = 0;

      if (x == i) {
        // this data point exists
        h = bar_height;
        if (h_idx < m_bar_heights.size() - 1) ++h_idx;
      } else {
        // data point does not exist, need to interpolate
        h = Interpolate(x, h_idx);
      }

      output.emplace_back(h);
    }

    return output;
  }

  std::vector<double> logspace_;
  std::vector<double> *y_;
  int num_items_;
  std::vector<std::pair<size_t, double>> m_bar_heights;
};

/* ********************************************************************************************** */

double magnitude(double real, double imag) { return sqrt((real * real) + (imag * imag)); }

/* ********************************************************************************************** */

void print_screen(Graph &my_graph, double index) {
  using namespace ftxui;
  static std::string reset_position;

  int seconds = (index - 1) / 44100 + 1;

  auto document = hbox({
      vbox({
          text("duration: " + std::to_string(seconds)) | bold,
          graph(std::ref(my_graph)),
      }) | flex |
          border,
  });

  auto screen = Screen::Create(Dimension::Full(), Dimension::Full());
  Render(screen, document);

  std::cout << reset_position << screen.ToString() << std::flush;
  reset_position = screen.ResetPosition();
}

/* ********************************************************************************************** */

int main() {
  // parse header file and fill data
  wave_header_t header_{};
  auto main_data = ParseData(header_);

  if (main_data.at(0).size() == 0) return 1;

  // here i'm considering a mono audio (only one channel)
  std::vector<double> data(main_data.at(0).begin(), main_data.at(0).end());
  //   std::vector<double> data(main_data.at(0).size());
  //   if (header_.NumChannels > 1) {
  //     for (size_t n = 0; n != main_data.at(0).size(); ++n) {  // stereo -> mono
  //       data.push_back(double(((double)main_data.at(0)[n] + (double)main_data.at(1)[n]) / 2.0));
  //     }
  //   }

  // calculate audio duration
  const int duration = header_.Subchunk2Size / header_.ByteRate;

  // get the number of samples matching a second
  const int num_samples = header_.Subchunk2Size / (header_.NumChannels * header_.BitsPerSample / 8);

  // get the nearest power of 2 from sample rate, for example: Fs = 44100, means window = 32768
  //   const int window = std::pow(2, std::floor(log(header_.SampleRate) / log(2)));
  const int window = 1024;
  const int result_window = window / 2 + 1;

  // fft
  std::vector<double> fft_input(window);
  std::vector<double> mag(result_window);

  fftw_complex *fft_out;
  fftw_plan fft;

  fft_out = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * result_window);
  fft = fftw_plan_dft_r2c_1d(window, fft_input.data(), fft_out, FFTW_ESTIMATE);

  // create hanning window
  double hanning_win[window];  // hanning window
  for (int n = 0; n < window; n++) {
    hanning_win[n] = 0.5 * (1 - cos(2 * PI * n / window));
  }

  using namespace std::chrono_literals;

  const auto sleep_time = .05s;

  //   std::vector<double> spectrum(result_window);
  Graph my_graph(&mag, result_window);

  double time_period = 0.5;                               // 500ms
  int max_chunk_size = header_.SampleRate * time_period;  // 22050
  //   int subchunks_per_time =
  //       std::ceil((max_chunk_size - 1) / window + 1);  //(22050 - 1) / 32768 + 1 => 2;

  double index = 0;
  while (index < num_samples) {
    // Check if index is bigger than vector size
    if ((index + window) > data.size()) {
      index = data.size() - window;
    }

    int aux = 0;
    fft_input.clear();
    std::transform(data.begin() + index, data.begin() + index + window,
                   std::back_inserter(fft_input),
                   [&](const double &v) mutable { return v * hanning_win[aux++]; });

    fftw_execute(fft);

    for (int n = 0; n < result_window; ++n) {
      mag[n] = magnitude(fft_out[n][0], fft_out[n][1]) / (result_window);  // normalize
      // int freq = (n * header_.SampleRate) / window;
    }

    print_screen(my_graph, index);
    std::this_thread::sleep_for(sleep_time);

    // std::fill(mag.begin(), mag.end(), 0);
    // print_screen(my_graph, index);
    // std::this_thread::sleep_for(sleep_time);

    index += window;
  }

  fftw_free(fft_out);
  fftw_destroy_plan(fft);

  fftw_cleanup();

  return 0;
}
