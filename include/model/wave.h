/**
 * \file
 * \brief  Class to read data samples from a WAVE file.
 */

#ifndef INCLUDE_MODEL_WAVE_H_
#define INCLUDE_MODEL_WAVE_H_

#include <cstdint>

#include "error_table.h"
#include "model/song.h"

class WaveFormat : public Song {
 public:
  using Song::Song;

  /**
   * @brief Parse only the header metadata from a given sound file
   *
   * @param full_path Path where song is located
   * @return Value Error code from operation
   */
  error::Value ParseHeaderInfo(const std::string& full_path) override;

  /**
   * @brief Parse raw data from a given sound file (this is only possible after parsing header info)
   * @return Value Error code from operation
   */
  error::Value ParseData() override;

  /* ******************************************************************************************** */
 private:
  // Based on canonical WAVE format from this link: http://soundfile.sapp.org/doc/WaveFormat
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
    uint16_t NumChannels;    // Number of channels 1=Mono 2=Stereo
    uint32_t SampleRate;     // Sampling Frequency in Hz (8000, 44100,...)
    uint32_t ByteRate;       // bytes per second
    uint16_t BlockAlign;     // 2=16-bit mono, 4=16-bit stereo
    uint16_t BitsPerSample;  // Number of bits per sample (8 bits, 16 bits,...)

    /* "data" sub-chunk */
    uint8_t Subchunk2ID[4];  // "data" string
    uint32_t Subchunk2Size;  // Sampled data length
  };

 private:
  wave_header_t header_;  //!< Header from WAVE file
};

#endif  // INCLUDE_MODEL_WAVE_H_