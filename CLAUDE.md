# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

spectrum is a console (TUI) music player in C++17: FTXUI for the UI, FFmpeg for decoding, ALSA for playback, FFTW3 for spectrum analysis, curl + libxml++ for lyrics, and yt-dlp (via embedded Python) for YouTube streams/playlists.

## Build, test, lint

```bash
# Full build (needs ALSA, FFmpeg, FFTW3, curl, libxml++, python3-embed, and yt-dlp on PATH — configure fails without yt-dlp)
cmake -S . -B build
cmake --build build
./build/src/spectrum -l /tmp/log.txt [-v]    # -l writes a log file, -v is verbose logging

# Tests (GoogleTest/GMock fetched via FetchContent)
cmake -S . -B build -DENABLE_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -G Ninja
cmake --build build && ./build/test/test
./build/test/test --gtest_filter='SuiteName.TestName'          # single test
ctest --output-on-failure --test-dir build/test -R SuiteName    # or via ctest (as CI does)

# Build without external AV deps (CI "debug" job). Uses Dummy* stubs from include/debug/.
# Note: tests are NOT built when SPECTRUM_DEBUG=ON.
cmake -S . -B build -DSPECTRUM_DEBUG=ON -DCMAKE_BUILD_TYPE=Debug

# Static analysis (matches CI)
cppcheck --max-ctu-depth=3 --enable=all --inline-suppr \
  --suppress=missingInclude --suppress=syntaxError \
  --suppress=unmatchedSuppression --suppress=preprocessorErrorDirective \
  --language=c++ --std=c++17 src
```

Other CMake options: `ENABLE_COVERAGE`, `ENABLE_INSTALL`, `DISABLE_POPULATE` (use system FTXUI/nlohmann_json instead of FetchContent, for Flatpak).

Source files are listed explicitly — no globbing:
- New `.cc` files must be added to `src/CMakeLists.txt` (`spectrum_lib`). Files depending on ALSA/FFmpeg/FFTW/curl/libxml/Python (anything in `audio/driver/` or `web/driver/`) go in the `if(NOT SPECTRUM_DEBUG)` block.
- New test files must be added to `test/CMakeLists.txt`.
- The executable builds with `-Wall -Wextra -Wshadow -Wconversion -Werror`; tests with `-Wall -Werror`.

## Architecture

Three layers, wired together in `src/main.cc`:

- **`view/`** (`interface::` namespace) — `Terminal` owns the FTXUI blocks (`Sidebar`, `FileInfo`, `MainContent`, `MediaPlayer`) plus dialogs. `view/base/` holds the framework (`Block`, `Dialog`, `EventDispatcher`, `CustomEvent`, `keybinding`); `view/element/` holds reusable widgets (menus, tabs, dialogs, focus controller).
- **`middleware/`** — `MediaController` bridges UI ↔ audio player and runs the FFT analysis thread feeding the spectrum visualizer.
- **`audio/`** — `Player` runs the audio loop thread using abstract `Playback`, `Decoder`, `Analyzer` interfaces (`audio/base/`) implemented by ALSA/FFmpeg/FFTW drivers (`audio/driver/`). `audio/lyric/` fetches lyrics.
- **`web/`** — `UrlFetcher`, `HtmlParser`, `StreamFetcher` interfaces (`web/base/`) with curl / libxml++ / yt-dlp drivers (`web/driver/`, `driver::` namespace).
- **`model/`** — plain data shared by all layers (`Song`, `Playlist`, `Volume`, `AudioFilter`, error codes, ...).

**Event flow**: UI blocks call `GetDispatcher()->SendEvent(CustomEvent::...)`; `Terminal` routes the event either to the audio thread (through `audio::Notifier`, implemented by `MediaController`) or to other blocks. Audio-thread callbacks go back through `interface::Notifier` (also implemented by `MediaController`), which forwards them to `Terminal`. `CustomEvent` identifiers are partitioned by direction: `FromAudioThreadToInterface` (50000+), `FromInterfaceToAudioThread` (60000+), `FromInterfaceToInterface` (70000+); payload is a `std::variant`.

## Conventions

- **Factories**: major classes expose `static Create(...)` with private constructors (plus an `Init()` step) rather than public constructors.
- **Dependency injection**: `Create()` takes optional raw pointers for drivers (ownership is transferred); `nullptr` means "use the real driver" (or the `Dummy*` stub under `SPECTRUM_DEBUG`). `Player::Create` also takes `asynchronous` — pass `false` in tests.
- **Tests**: mocks live in `test/mock/` (`*Mock`), shared fixtures in `test/general/` (`sync_testing.h` provides `TestSyncer`/`RunAsyncTest` for step-synchronized multi-threaded tests). `ENABLE_TESTS` turns on `friend class ::TestName;` declarations in production headers so tests can reach private members; test classes live in an anonymous namespace.
- **Style**: Google C++ style, 100-column limit (`.clang-format`). Header guards `INCLUDE_<DIR>_<FILE>_H_`. Doxygen `/** */` comments on public APIs.
- Namespaces: `audio`, `interface`, `middleware`, `driver`, `model`, `util`, `web`.
