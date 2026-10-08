/**
 * \file
 * \brief  Class for rendering audio output device picker
 */

#ifndef INCLUDE_VIEW_ELEMENT_DEVICE_PICKER_H_
#define INCLUDE_VIEW_ELEMENT_DEVICE_PICKER_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/box.hpp"
#include "model/audio_device.h"
#include "util/file_handler.h"
#include "view/base/element.h"
#include "view/base/event_dispatcher.h"

namespace interface {

/**
 * @brief Picker to choose audio output device, shown over all blocks: device is used by audio
 * player right after choosing it (even with a song playing) and saved to be restored on next run.
 * Besides keyboard, mouse may be used: wheel moves selection, click selects a device and
 * double-click chooses it
 */
class DevicePicker : public Element {
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
   * @brief Renders the component
   * @return Element Built element based on internal state
   */
  ftxui::Element Render() override;

  /**
   * @brief Handles an event (from mouse/keyboard)
   * @param event Received event from screen
   * @return true if event was handled, otherwise false
   */
  bool OnEvent(const ftxui::Event& event) override;

  /**
   * @brief Indicates if picker is visible
   * @return true if picker is visible, otherwise false
   */
  bool IsVisible() const { return visible_; }

  /**
   * @brief Set picker as visible, with device in use selected (or the first entry, when it is not
   * available anymore)
   * @param devices Output devices available on audio player
   */
  void Open(const model::AudioDevices& devices);

  /* ******************************************************************************************** */
  //! Internal operations
 private:
  //! Ask audio player to use selected device and save it, so it is restored on next run
  void Apply();

  //! Select next/previous entry (if any)
  void Move(bool next);

  //! Close picker, using selected device
  void Choose();

  //! Get index of entry rendered at the position of mouse cursor (if any)
  std::optional<size_t> GetEntryAt(const ftxui::Mouse& mouse) const;

  /* ******************************************************************************************** */
  //! Mouse handling (called by Element, only when mouse cursor is over picker)

  //! Move selection
  void HandleWheel(const ftxui::Mouse::Button& button) override;

  //! Select entry under mouse cursor
  void HandleClick(ftxui::Event& event) override;

  //! Select entry under mouse cursor and choose it
  void HandleDoubleClick(ftxui::Event& event) override;

  /* ******************************************************************************************** */
  //! Variables

  std::weak_ptr<EventDispatcher> dispatcher_;        //!< Send chosen device to audio player
  std::shared_ptr<util::FileHandler> file_handler_;  //!< Load/save device in settings

  //! Entries to choose from, where the first one is always to not choose any device (as audio
  //! player does it)
  model::AudioDevices devices_;

  size_t selected_ = 0;  //!< Index of selected entry
  std::string current_;  //!< Name of device in use (empty when it was not chosen by user)
  bool visible_ = false;

  std::vector<ftxui::Box> boxes_;  //!< Single box for each entry rendered (to handle mouse)
};

}  // namespace interface
#endif  // INCLUDE_VIEW_ELEMENT_DEVICE_PICKER_H_
