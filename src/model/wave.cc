#include "model/wave.h"

#include <stdio.h>  // for fclose, fopen, fprintf, fread, size_t

#include <algorithm>  // for max
#include <cassert>    // for assert
#include <fstream>    // for operator<<, basic_ostream, endl, basi...
#include <iostream>
#include <iterator>
#include <sstream>  // for ostringstream

#include "model/application_error.h"  // for Errors

namespace model {

int WaveFormat::ParseHeaderInfo(const std::string& full_path) {
  filename_ = full_path;
  file_ = std::ifstream(full_path, std::ios::binary);

  if (!file_.good()) {
    return error::kInvalidFile;
  }

  file_.unsetf(std::ios::skipws);
  file_.read((char*)&header_, sizeof(wave_header_t));

  std::string chunkId(header_.RIFF, header_.RIFF + 4);
  std::string format(header_.WAVE, header_.WAVE + 4);
  std::string subchunk1Id(header_.Subchunk1ID, header_.Subchunk1ID + 4);

  // Given sub-string is uppercase, search for it in the given text
  auto find_substr = [](std::string text, std::string to_search) {
    std::transform(text.begin(), text.end(), text.begin(), ::toupper);
    return text.find(to_search) != std::string::npos;
  };

  if (!find_substr(chunkId, "RIFF") || !find_substr(format, "WAVE") ||
      !find_substr(subchunk1Id, "FMT")) {
    return error::kFileNotSupported;
  }

  if (header_.AudioFormat != 1) {
    return error::kFileCompressionNotSupported;
  }

  if (header_.NumChannels < 1 || header_.NumChannels > 2) {
    return error::kUnknownNumOfChannels;
  }

  if ((header_.ByteRate !=
       (header_.NumChannels * header_.SampleRate * header_.BitsPerSample) / 8)) {
    return error::kInconsistentHeaderInfo;
  }

  // After parsing successfully, fill audio information
  info_ = AudioData{
      .file_format = "WAV",
      .num_channels = header_.NumChannels,
      .sample_rate = header_.SampleRate,
      .bit_rate = header_.ByteRate * 8,
      .bit_depth = header_.BitsPerSample,
  };

  file_.seekg(sizeof(wave_header_t));
  return error::kSuccess;
}

/* ********************************************************************************************** */

int WaveFormat::ParseData() {
  std::istream_iterator<uint8_t> begin(file_), end;
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
          return error::kCorruptedData;
          break;
      }
    }

    // TODO: pass this to player, or maybe return a vector from this method
  }

  return error::kSuccess;
}

}