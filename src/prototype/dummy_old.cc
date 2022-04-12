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
  std::string full_path{"/home/vinicius/projects/music-analyzer/another1k.wav"};
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
  int num_samples = header_.Subchunk2Size / num_bytes_per_sample;

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
          int32_t sampleAsInt = (raw_data[index + 1] << 8) | raw_data[index];
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
  explicit Graph(double *y, int num_items, int size) {
    y_ = y;
    num_items_ = num_items;
    size_ = size;
  }
  std::vector<int> operator()(int width, int height) const {
    std::vector<int> output(num_items_ / 2);

    // Given the window width, it is necessary to downsample data in order to plot it.
    // To solve this, I'm using Largest-Triangle-Three-Buckets (LTTB), based from here:
    // https://skemman.is/bitstream/1946/15343/3/SS_MSthesis.pdf
    // const int bucket_size = size_ / width;

    // for (int i = 0; i < size_; ++i) {
    //   double v = y_[i];
    //   output[i] = static_cast<int>(v);
    // }
    for (int i = 0; i < num_items_ / 2; i++) {
      double v = y_[i * size_];
      output[i] = static_cast<int>(v > 0 ? v : 0);
    }
    return output;
  }
  double *y_;
  int num_items_, size_;
};

int main() {
  wave_header_t header_{};
  auto main_data = ParseData(header_);
  std::vector<double> data(main_data.at(0).begin(), main_data.at(0).end());

  // calculate audio duration
  const int duration = header_.Subchunk2Size / header_.ByteRate;

  // this will give the number of samples matching a second
  const int num_samples = header_.Subchunk2Size / (header_.NumChannels * header_.BitsPerSample / 8);

  const int win_len = (num_samples / duration) / 2;  // match 500ms
  int size_f = header_.SampleRate / win_len;

  //   fftw_complex *fft_in, *fft_out;
  //   fftw_plan fft;
  //   fft_in = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * win_len);
  //   fft_out = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * win_len);
  //   fft = fftw_plan_dft_1d(win_len, fft_in, fft_out, FFTW_BACKWARD, FFTW_ESTIMATE_PATIENT);  //
  //   FFT

  // fft
  double real[win_len];
  double imag[win_len];
  double mag[win_len];
  double angle[win_len] = {0};

  double samples[win_len];
  fftw_complex *fft_out;
  fftw_plan fft;

  fft_out = (fftw_complex *)fftw_malloc(sizeof(fftw_complex) * win_len);
  fft = fftw_plan_dft_r2c_1d(win_len, samples, fft_out, FFTW_ESTIMATE);

  double hanning_win[win_len];  // hanning window
  for (int n = 0; n < win_len; n++) {
    hanning_win[n] = 0.5 * (1 - cos(2 * PI * n / win_len));
  }

  using namespace ftxui;
  using namespace std::chrono_literals;

  const auto sleep_time = .5s;
  std::string reset_position;
  Graph my_graph(mag, win_len, size_f);

  for (int i = 0, offset = 0; i < duration; offset += win_len, i++) {
    for (int n = 0; n < win_len; n++) {
      // double input_temp = wav.data[offset*ana_len +n];
      double input_temp = data.at(offset + n);
      //   fft_in[n][0] = input_temp * hanning_win[n];
      //   fft_in[n][1] = 0;
      samples[n] = input_temp * hanning_win[n];
    }

    fftw_execute(fft);

    mag[0] = sqrt(real[0] * real[0] + imag[0] * imag[0]) / win_len;

    for (int n = 1; n < win_len / 2; n++) {
      real[n] = fft_out[n][0];
      imag[n] = fft_out[n][1];
      // Notice the different between real() and abs() func
      mag[n] = sqrt(real[n * size_f] * real[n * size_f] + imag[n * size_f] * imag[n * size_f]);
      // Notice：angle(x+yj) = actan2(y,x);
      angle[n] = atan2(imag[n], real[n]);
    }

    auto document = hbox({
        vbox({
            graph(std::ref(my_graph)),
        }) | flex |
            border,
    });

    auto screen = Screen::Create(Dimension::Full(), Dimension::Full());
    Render(screen, document);
    std::cout << reset_position;
    screen.Print();
    reset_position = screen.ResetPosition();

    std::this_thread::sleep_for(sleep_time);
  }

  //   fftw_free(fft_in);
  fftw_free(fft_out);
  fftw_destroy_plan(fft);

  fftw_cleanup();

  return 0;
}
