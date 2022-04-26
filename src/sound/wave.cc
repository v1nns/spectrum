#include "sound/wave.h"

#include <stdio.h>  // for fclose, fopen, fprintf, fread, size_t

#include <algorithm>  // for max
#include <cassert>    // for assert
#include <fstream>    // for operator<<, basic_ostream, endl, basi...
#include <iterator>
#include <sstream>  // for ostringstream

#include "error_table.h"  // for Errors

/* ********************************************************************************************** */

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

  if (chunkId != "RIFF" || format != "WAVE" || subchunk1Id != "FMT") {
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
          // TODO: return error!
          break;
      }
    }

    // TODO: pass this to player, or maybe return a vector from this method
  }

  return error::kSuccess;
}

/* ********************************************************************************************** */

std::vector<std::string> WaveFormat::GetFormattedStats() {
  std::vector<std::string> output{};
  std::ostringstream s;

  s.str("");
  s << "Data size: " << header_.ChunkSize << std::endl;
  output.push_back(s.str());

  // Display the sampling Rate from the header
  s.str("");
  s << "Sampling Rate: " << header_.SampleRate << std::endl;
  output.push_back(s.str());

  s.str("");
  s << "Number of bits used: " << header_.BitsPerSample << std::endl;
  output.push_back(s.str());

  s.str("");
  s << "Number of channels: " << header_.NumChannels << std::endl;
  output.push_back(s.str());

  s.str("");
  s << "Number of bytes per second: " << header_.ByteRate << std::endl;
  output.push_back(s.str());

  s.str("");
  s << "Data length: " << header_.Subchunk2Size << std::endl;
  output.push_back(s.str());

  s.str("");
  // Audio format 1=PCM,6=mulaw,7=alaw, 257=IBM Mu-Law, 258=IBM A-Law, 259=ADPCM
  s << "Audio Format: " << header_.AudioFormat << std::endl;
  output.push_back(s.str());

  s.str("");
  s << "Block align: " << header_.BlockAlign << std::endl;
  output.push_back(s.str());

  return output;
}
