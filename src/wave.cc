#include "wave.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

void PrintStats(const int length, const wave_header_t& hdr) {
  std::cout << "File is                    :" << length << " bytes."
            << std::endl;
  std::cout << "RIFF header                :" << hdr.RIFF[0] << hdr.RIFF[1]
            << hdr.RIFF[2] << hdr.RIFF[3] << std::endl;
  std::cout << "WAVE header                :" << hdr.WAVE[0] << hdr.WAVE[1]
            << hdr.WAVE[2] << hdr.WAVE[3] << std::endl;
  std::cout << "FMT                        :" << hdr.fmt[0] << hdr.fmt[1]
            << hdr.fmt[2] << hdr.fmt[3] << std::endl;
  std::cout << "Data size                  :" << hdr.ChunkSize << std::endl;

  // Display the sampling Rate from the header
  std::cout << "Sampling Rate              :" << hdr.SamplesPerSec << std::endl;
  std::cout << "Number of bits used        :" << hdr.bitsPerSample << std::endl;
  std::cout << "Number of channels         :" << hdr.NumOfChan << std::endl;
  std::cout << "Number of bytes per second :" << hdr.bytesPerSec << std::endl;
  std::cout << "Data length                :" << hdr.Subchunk2Size << std::endl;
  std::cout << "Audio Format               :" << hdr.AudioFormat << std::endl;
  // Audio format 1=PCM,6=mulaw,7=alaw, 257=IBM Mu-Law, 258=IBM A-Law,
  // 259=ADPCM

  std::cout << "Block align                :" << hdr.blockAlign << std::endl;
  std::cout << "Data string                :" << hdr.Subchunk2ID[0]
            << hdr.Subchunk2ID[1] << hdr.Subchunk2ID[2] << hdr.Subchunk2ID[3]
            << std::endl;
}

int Read() {
  const char* filePath =
      "/home/vinicius/projects/music-analyzer/africa-toto.wav";
  FILE* wavFile = fopen(filePath, "r");
  if (wavFile == nullptr) {
    fprintf(stderr, "Unable to open wave file: %s\n", filePath);
    return 1;
  }

  // Read the header
  wave_header_t wavHeader;
  int headerSize = sizeof(wave_header_t), filelength = 0;

  size_t bytesRead = fread(&wavHeader, 1, headerSize, wavFile);
  std::cout << "Header Read " << bytesRead << " bytes." << std::endl;

  //   if (bytesRead > 0) {
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

  //     while ((bytesRead = fread(buffer, sizeof buffer[0],
  //                               BUFFER_SIZE / (sizeof buffer[0]), wavFile)) >
  //                               0) {
  //       /** DO SOMETHING WITH THE WAVE DATA HERE **/
  //       // std::cout << "Read " << bytesRead << " bytes." << std::endl;
  //     }
  //     delete[] buffer;
  //     buffer = nullptr;
  //   }

  filelength = GetFileSize(wavFile);
  PrintStats(filelength, wavHeader);

  fclose(wavFile);
  return 0;
}

// find the file size
int GetFileSize(FILE* inFile) {
  int fileSize = 0;
  fseek(inFile, 0, SEEK_END);

  fileSize = ftell(inFile);

  fseek(inFile, 0, SEEK_SET);
  return fileSize;
}