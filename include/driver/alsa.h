/**
 * \file
 * \brief  Class for ALSA driver support
 */

#ifndef INCLUDE_DRIVER_ALSA_H_
#define INCLUDE_DRIVER_ALSA_H_

namespace driver {

/**
 * @brief TODO:...
 */
class AlsaSound {
 public:
  /**
   * @brief Construct a new AlsaSound object
   */
  AlsaSound();

  /**
   * @brief Destroy the AlsaSound object
   */
  virtual ~AlsaSound() = default;
};

}  // namespace driver
#endif  // INCLUDE_DRIVER_ALSA_H_