/**
 * \file
 * \brief  Class to read data samples from a WAVE file.
 */

#ifndef INCLUDE_WAVE_H_
#define INCLUDE_WAVE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "sound/song.h"

// Based on canonical WAVE format from this link:
// http://soundfile.sapp.org/doc/WaveFormat
struct wave_header_t {
  /* RIFF Chunk Descriptor */
  uint8_t RIFF[4];     // RIFF Header Magic header
  uint32_t ChunkSize;  // RIFF Chunk Size
  uint8_t WAVE[4];     // WAVE Header

  /* "fmt" sub-chunk */
  uint8_t fmt[4];          // FMT header
  uint32_t Subchunk1Size;  // Size of the fmt chunk
  uint16_t AudioFormat;    // Audio format 1=PCM,6=mulaw,7=alaw,     257=IBM
                           // Mu-Law, 258=IBM A-Law, 259=ADPCM
  uint16_t NumOfChan;      // Number of channels 1=Mono 2=Sterio
  uint32_t SamplesPerSec;  // Sampling Frequency in Hz
  uint32_t bytesPerSec;    // bytes per second
  uint16_t blockAlign;     // 2=16-bit mono, 4=16-bit stereo
  uint16_t bitsPerSample;  // Number of bits per sample

  /* "data" sub-chunk */
  uint8_t Subchunk2ID[4];  // "data" string
  uint32_t Subchunk2Size;  // Sampled data length
};

/* ********************************************************************************************** */
class WaveFormat : public Song {
 public:
  using Song::Song;

  /* ******************************************************************************************** */

  /**
   * @brief Parse a given sound file to get its info
   *
   * @param full_path Text containing path where file is located
   * @return int Error code from operation
   */
  int ParseFromFile(const std::string& full_path) override;

  /**
   * @brief Get the Formatted Stats from parsed sound file
   *
   * @return std::vector<std::string> Text splitted in lines
   */
  std::vector<std::string> GetFormattedStats() override;

  /* ******************************************************************************************** */
 private:
  wave_header_t header_;  //!< Header from WAVE file
};

#endif  // INCLUDE_WAVE_H_