#include "sound/wave.h"

#include <stdio.h>  // for fclose, fopen, fprintf, fread, size_t

#include <algorithm>  // for max
#include <cassert>    // for assert
#include <fstream>    // for operator<<, basic_ostream, endl, basi...
#include <iterator>
#include <sstream>  // for ostringstream

#include "error/error_table.h"  // for kSuccess

/* ********************************************************************************************** */

int WaveFormat::ParseFromFile(const std::string& full_path) {
  filename_ = full_path;
  std::ifstream file(full_path, std::ios::binary);

  if (!file.good()) {
    // std::cout << "ERROR: this doesn't seem to be a valid .WAV file" << std::endl;
    return -1;
  }

  file.unsetf(std::ios::skipws);
  file.read((char*)&header_, sizeof(wave_header_t));

  std::string chunkId(header_.RIFF, header_.RIFF + 4);
  std::string format(header_.WAVE, header_.WAVE + 4);
  std::string subchunk1Id(header_.Subchunk1ID, header_.Subchunk1ID + 4);

  if (chunkId != "RIFF" || format != "WAVE" || subchunk1Id != "FMT") {
    // std::cout << "file not supported" << std:endl;
    return -1;
  }

  if (header_.AudioFormat != 1) {
    // std::cout << "ERROR: this is a compressed .WAV file and this library does not support
    // decoding them at present" << std::endl;
    return -1;
  }

  if (header_.NumChannels < 1 || header_.NumChannels > 2) {
    // std::cout << "ERROR: this WAV file seems to be neither mono nor stereo (perhaps multi-track,
    // or corrupted?)" << std::endl;
    return -1;
  }

  if ((header_.ByteRate !=
       (header_.NumChannels * header_.SampleRate * header_.BitsPerSample) / 8)) {
    // std::cout << "ERROR: the header data in this WAV file seems to be inconsistent" << std::endl;
    return -1;
  }

  file.seekg(sizeof(wave_header_t));

  // calculate audio duration
  //   const int duration = (header_.ChunkSize - 36) /
  //    (header_.SampleRate * header_.NumChannels * (header_.BitsPerSample / 8));

  std::istream_iterator<uint8_t> begin(file), end;
  std::vector<uint8_t> raw_data(begin, end);

  int num_samples = header_.Subchunk2Size;
  int num_bytes_per_sample = header_.BitsPerSample / 8;

  std::vector<std::vector<double>> data(header_.NumChannels);

  for (int sample = 0; sample < num_samples; sample++) {
    for (int channel = 0; channel < header_.NumChannels; channel++) {
      int index = (header_.BlockAlign * sample) + channel * num_bytes_per_sample;

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
