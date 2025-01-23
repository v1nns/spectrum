<h1 align="center">
  <br>
  :headphones: spectrum
  <br>
</h1>

<h4 align="center">A simple and intuitive console-based music player written in C++</h4>

https://github.com/v1nns/spectrum/assets/22479290/5ab537cf-34d6-4627-8d66-4f7128cd6915

Introducing yet another music player for tech enthusiasts that will simplify the way you experience your favorite tunes! Immerse yourself in the sound with the powerful equalizer, allowing you to fine-tune every aspect of the music to your exact specifications, perfectly matching your mood.

With an intuitive user interface and lightning-fast performance, this music player is the perfect addition to any audiophile's collection. Whether you're a casual listener or a serious music lover, this console-based music player will exceed your expectations.

## Features :speech_balloon:

- Simple and intuitive terminal user interface;
- Plays music in any format;
- Basic playback controls such as play, pause, stop, and skip;
- Displays information about the currently playing track;
- Audio spectrum visualizer;
- Audio equalizer;
- Fetch song lyrics;
- Support for playlists.

## Installation :floppy_disk:

### AUR (using yay)

If you're using Arch Linux or any derivative, you can install spectrum using yay, a popular AUR helper:

   ```bash
   # Install the latest version
   yay -S spectrum-git
   ```

### Flatpak

To install Spectrum using Flatpak:

1. Add the Flathub repository (if not already added):
   ```bash
   flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
   ```

2. Install spectrum:
   ```bash
   flatpak install flathub io.github.v1nns.spectrum
   ```

## Development :memo:

To build spectrum, you need to have a C++ compiler installed on your system.

```bash
# Package dependencies (on Ubuntu)
sudo apt install build-essential libasound2-dev libavcodec-dev \
     libavfilter-dev libavformat-dev libfftw3-dev libswresample-dev \
     libcurl4-openssl-dev libxml++2.6-dev

# Clone repository
git clone https://github.com/v1nns/spectrum.git
cd spectrum

# Generate build system in the build directory
cmake -S . -B build

# Build executable
cmake --build build

# Install to /usr/local/bin/ (optional)
sudo cmake --install build

# OR just execute it
./build/src/spectrum
```

To ensure that any new implementation won't impact the existing one, you may execute unit tests to check that. To enable unit testing, you should compile with the following settings:

```bash
# Generate build system for testing/debugging
cmake -S . -B build -DENABLE_TESTS=ON -DCMAKE_BUILD_TYPE=Debug -G Ninja

# Execute unit tests
cmake --build build && ./build/test/test

# For manual testing, you may take a look in the log file
cmake --build build && ./build/src/spectrum -l /tmp/log.txt
```

## Credits :placard:

This software uses the following open source packages:

- [FFmpeg](https://ffmpeg.org/)
- [FFTW](https://www.fftw.org/)
- [curl](https://curl.se/)
- [libxml++](https://libxmlplusplus.github.io/libxmlplusplus/)
- [FTXUI](https://github.com/ArthurSonzogni/FTXUI)
- [cava](https://github.com/karlstav/cava) <sup>(visualizer is based on cava implementation)</sup>
- [json](https://github.com/nlohmann/json)

## Contributing

Contributions are always welcome! If you find any bugs or have suggestions for new features, please open an issue or submit a pull request.

## License

This project is licensed under the MIT License. See the LICENSE file for details.
