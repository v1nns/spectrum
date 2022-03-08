#include "sound/wave.h"

#include <cassert>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>

#include "error_code.h"

/* ********************************************************************************************** */

int WaveFormat::ParseFromFile(const std::string& full_path) {
  filename_ = full_path;
  file_ = fopen(filename_.c_str(), "r");

  if (file_ == nullptr) {
    fprintf(stderr, "Unable to open wave file: %s\n", filename_.c_str());
    return ERR_GENERIC;
  }

  // Read the header
  int size = sizeof(wave_header_t);
  size_t bytes_read = fread(&header_, 1, size, file_);
  assert(bytes_read > 0);

  //   if (bytes_read > 0) {
  //     // Read the data
  //     uint16_t bytesPerSample =
  //         wavHeader.bitsPerSample / 8;  // Number of bytes per sample
  //     uint64_t numSamples =
  //         wavHeader.ChunkSize /
  //         bytesPerSample;  // How many samples are in the wav file?
  //     static const uint16_t BUFFER_SIZE = 4096;
  //     int8_t* buffer = new int8_t[BUFFER_SIZE];
  //     std::cout << "WAVE file has " << numSamples << " samples." <<
  //     std::endl;

  //     while ((bytes_read = fread(buffer, sizeof buffer[0],
  //                               BUFFER_SIZE / (sizeof buffer[0]), wavFile)) >
  //                               0) {
  //       /** DO SOMETHING WITH THE WAVE DATA HERE **/
  //       // std::cout << "Read " << bytes_read << " bytes." << std::endl;
  //     }
  //     delete[] buffer;
  //     buffer = nullptr;
  //   }

  fclose(file_);
  return ERR_OK;
}

/* ********************************************************************************************** */

std::vector<std::string> WaveFormat::GetFormattedStats() {
  std::vector<std::string> output{};
  std::ostringstream s;

  s << "RIFF header: " << header_.RIFF[0] << header_.RIFF[1] << header_.RIFF[2] << header_.RIFF[3]
    << std::endl;
  output.push_back(s.str());

  s.str("");
  s << "WAVE header: " << header_.WAVE[0] << header_.WAVE[1] << header_.WAVE[2] << header_.WAVE[3]
    << std::endl;
  output.push_back(s.str());

  s.str("");
  s << "FMT: " << header_.fmt[0] << header_.fmt[1] << header_.fmt[2] << header_.fmt[3] << std::endl;
  output.push_back(s.str());

  s.str("");
  s << "Data size: " << header_.ChunkSize << std::endl;
  output.push_back(s.str());

  // Display the sampling Rate from the header
  s.str("");
  s << "Sampling Rate: " << header_.SamplesPerSec << std::endl;
  output.push_back(s.str());

  s.str("");
  s << "Number of bits used: " << header_.bitsPerSample << std::endl;
  output.push_back(s.str());

  s.str("");
  s << "Number of channels: " << header_.NumOfChan << std::endl;
  output.push_back(s.str());

  s.str("");
  s << "Number of bytes per second: " << header_.bytesPerSec << std::endl;
  output.push_back(s.str());

  s.str("");
  s << "Data length: " << header_.Subchunk2Size << std::endl;
  output.push_back(s.str());

  s.str("");
  // Audio format 1=PCM,6=mulaw,7=alaw, 257=IBM Mu-Law, 258=IBM A-Law, 259=ADPCM
  s << "Audio Format: " << header_.AudioFormat << std::endl;
  output.push_back(s.str());

  s.str("");
  s << "Block align: " << header_.blockAlign << std::endl;
  output.push_back(s.str());

  s.str("");
  s << "Data string: " << header_.Subchunk2ID[0] << header_.Subchunk2ID[1] << header_.Subchunk2ID[2]
    << header_.Subchunk2ID[3] << std::endl;
  output.push_back(s.str());

  return output;
}
