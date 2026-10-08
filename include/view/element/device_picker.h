/**
 * \file
 * \brief  Class for rendering audio output device picker
 */

#ifndef INCLUDE_VIEW_ELEMENT_DEVICE_PICKER_H_
#define INCLUDE_VIEW_ELEMENT_DEVICE_PICKER_H_

#include <cstddef>
#include <memory>
#include <string>

#include "model/audio_device.h"
#include "util/file_handler.h"
#include "view/base/event_dispatcher.h"
#include "view/element/picker.h"

namespace interface {

/**
 * @brief Picker to choose audio output device: device is used by audio player right after choosing
 * it (even with a song playing) and saved to be restored on next run
 */
class DevicePicker : public Picker {
 public:
  /**
   * @brief Construct a new DevicePicker object, with the device saved in settings (if any) as the
   * one in use, as audio player starts with it
   * @param dispatcher Event dispatcher to send chosen device to audio player
   * @param file_handler Utility handler to load/save device in settings
   */
  DevicePicker(const std::shared_ptr<EventDispatcher>& dispatcher,
               const std::shared_ptr<util::FileHandler>& file_handler);

  /**
   * @brief Destroy DevicePicker object
   */
  ~DevicePicker() override = default;

  /**
   * @brief Set picker as visible, with device in use selected (or the first entry, when it is not
   * available anymore)
   * @param devices Output devices available on audio player
   */
  void Open(const model::AudioDevices& devices);

  /* ******************************************************************************************** */
  //! Internal operations
 private:
  //! Ask audio player to use selected device and save it, so it is restored on next run (nothing
  //! is done while selection moves, as changing device may interrupt song for a moment)
  void OnChoose(size_t index) override;

  /* ******************************************************************************************** */
  //! Variables

  std::weak_ptr<EventDispatcher> dispatcher_;        //!< Send chosen device to audio player
  std::shared_ptr<util::FileHandler> file_handler_;  //!< Load/save device in settings

  std::string current_;  //!< Name of device in use (empty when it was not chosen by user)
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_DEVICE_PICKER_H_
