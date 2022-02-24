#include "sound/wave.h"

#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>

#include "error_code.h"

// find the file size
int GetFileSize(FILE* in_file) {
  int size = 0;
  fseek(in_file, 0, SEEK_END);

  size = ftell(in_file);

  fseek(in_file, 0, SEEK_SET);
  return size;
}

bool WaveFormat::ParseFromFile(const std::string& full_path) {
  filename_ = full_path;
  file_ = fopen(filename_.c_str(), "r");

  if (file_ == nullptr) {
    fprintf(stderr, "Unable to open wave file: %s\n", filename_.c_str());
    return 1;
  }

  // Read the header
  int size = sizeof(wave_header_t);
  size_t bytes_read = fread(&header_, 1, size, file_);
  std::cout << "Header read " << bytes_read << " bytes." << std::endl;

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

  length_ = GetFileSize(file_);

  fclose(file_);
  return true;
}

void WaveFormat::PrintStats() {
  const uint8_t max_columns = 27;
  std::cout << std::setw(max_columns) << std::left << "File is";
  std::cout << ": " << length_ << " bytes." << std::endl;

  std::cout << std::setw(max_columns) << std::left << "RIFF header";
  std::cout << ": " << header_.RIFF[0] << header_.RIFF[1] << header_.RIFF[2]
            << header_.RIFF[3] << std::endl;

  std::cout << std::setw(max_columns) << std::left << "WAVE header";
  std::cout << ": " << header_.WAVE[0] << header_.WAVE[1] << header_.WAVE[2]
            << header_.WAVE[3] << std::endl;

  std::cout << std::setw(max_columns) << std::left << "FMT";
  std::cout << ": " << header_.fmt[0] << header_.fmt[1] << header_.fmt[2]
            << header_.fmt[3] << std::endl;

  std::cout << std::setw(max_columns) << std::left << "Data size";
  std::cout << ": " << header_.ChunkSize << std::endl;

  // Display the sampling Rate from the header
  std::cout << std::setw(max_columns) << std::left << "Sampling Rate";
  std::cout << ": " << header_.SamplesPerSec << std::endl;

  std::cout << std::setw(max_columns) << std::left << "Number of bits used";
  std::cout << ": " << header_.bitsPerSample << std::endl;

  std::cout << std::setw(max_columns) << std::left << "Number of channels";
  std::cout << ": " << header_.NumOfChan << std::endl;

  std::cout << std::setw(max_columns) << std::left
            << "Number of bytes per second";
  std::cout << ": " << header_.bytesPerSec << std::endl;

  std::cout << std::setw(max_columns) << std::left << "Data length";
  std::cout << ": " << header_.Subchunk2Size << std::endl;

  std::cout << std::setw(max_columns) << std::left << "Audio Format";
  std::cout << ": " << header_.AudioFormat << std::endl;
  // Audio format 1=PCM,6=mulaw,7=alaw, 257=IBM Mu-Law, 258=IBM A-Law,
  // 259=ADPCM

  std::cout << std::setw(max_columns) << std::left << "Block align";
  std::cout << ": " << header_.blockAlign << std::endl;

  std::cout << std::setw(max_columns) << std::left << "Data string";
  std::cout << ": " << header_.Subchunk2ID[0] << header_.Subchunk2ID[1]
            << header_.Subchunk2ID[2] << header_.Subchunk2ID[3] << std::endl;
}
