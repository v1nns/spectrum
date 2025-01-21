#include "view/element/text_animation.h"

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
      // Here comes the magic
      text += text.front();
      text.erase(text.begin());

      // Notify UI
      cb_update();
    }
  });
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
