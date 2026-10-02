#include "view/element/text_animation.h"

#include "ftxui/screen/string.hpp"

namespace interface {

TextAnimation::~TextAnimation() {
  // Ensure that thread will be stopped
  Stop();
}

/* ********************************************************************************************** */

void TextAnimation::Start(const std::string& entry) {
  // Append an empty space for better aesthetics
  text = entry + " ";
  enabled = true;

  thread = std::thread([this] {
    using namespace std::chrono_literals;
    std::unique_lock lock(mutex);

    // Run the animation every 0.2 seconds while enabled is true
    while (!notifier.wait_for(lock, 0.2s, [this] { return enabled == false; })) {
      // Here comes the magic: move first character to the end (as a whole glyph, otherwise a
      // multi-byte character would be split, taking more than one step to move)
      const auto glyphs = ftxui::Utf8ToGlyphs(text);
      const size_t first = glyphs.empty() ? 1 : glyphs.front().size();

      text += text.substr(0, first);
      text.erase(0, first);

      // Notify UI
      cb_update();
    }
  });
}

/* ********************************************************************************************** */

std::string TextAnimation::GetText() const {
  std::scoped_lock lock(mutex);
  return text;
}

/* ********************************************************************************************** */

void TextAnimation::Stop() {
  if (enabled) {
    Notify();
    Exit();
  }
}

/* ********************************************************************************************** */

void TextAnimation::Notify() {
  std::scoped_lock lock(mutex);
  enabled = false;
}

/* ********************************************************************************************** */

void TextAnimation::Exit() {
  notifier.notify_one();
  thread.join();
}

}  // namespace interface
