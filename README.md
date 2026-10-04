<h1 align="center">
  <br>
  :headphones: spectrum
  <br>
</h1>

<h4 align="center">A simple and intuitive console-based music player written in C++</h4>

https://github.com/v1nns/spectrum/assets/22479290/5ab537cf-34d6-4627-8d66-4f7128cd6915

Introducing yet another music player for tech enthusiasts that will simplify the way you experience your favorite tunes! Immerse yourself in the sound with a powerful equalizer, allowing you to fine-tune every aspect of the music to your exact specifications, perfectly matching your mood.

With an intuitive user interface and lightning-fast performance, this music player is the perfect addition to any audiophile's collection. Whether you're a casual listener or a serious music lover, this console-based music player will exceed your expectations.

## Features :speech_balloon:

- Simple and intuitive terminal user interface;
- Support any song format;
- Basic playback controls such as play, pause, stop, and skip;
- Seek time position in the song;
- Display technical information about the current song;
- Audio spectrum visualizer;
- Audio equalizer;
- Fetch song lyrics;
- Playlists with local files and songs from YouTube (including whole YouTube playlists);
- Repeat and shuffle modes;
- Color themes (Tokyo Night, Catppuccin Mocha and Latte, Gruvbox Dark, Nord, Dracula, or the colors from your terminal);

## Installation :floppy_disk:

### AUR (using yay)

If you're using Arch Linux or any derivative, you can install spectrum through yay (this is a popular AUR helper):

   ```bash
   # Install the latest version
   yay -S spectrum-git
   ```

### Flatpak (still in progress)

To install spectrum using Flatpak:

1. Add the Flathub repository:
   ```bash
   flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
   ```

2. Install spectrum:
   ```bash
   flatpak install flathub io.github.v1nns.spectrum
   ```

## Usage :musical_keyboard:

Run `spectrum` to start listing files from the current directory, or `spectrum -d <path>` to start from another one. Everything is controlled by keyboard (mouse also works), and <kbd>F12</kbd> shows all keybindings, starting from the ones related to what is focused.

### Playlists and YouTube

To play songs from YouTube, [yt-dlp](https://github.com/yt-dlp/yt-dlp) must be installed and found in `PATH` (it is optional, and only used for that).

1. Press <kbd>F2</kbd> to show playlists, then <kbd>c</kbd> to create one (<kbd>o</kbd> modifies and <kbd>d</kbd> deletes the selected one);
2. Add songs to it:
   - from local files: navigate in the files list and press <kbd>Space</kbd> on each song;
   - from YouTube: press <kbd>F2</kbd>, paste the URL of a video (or of a whole playlist, to import all its songs) and press <kbd>Return</kbd>;
3. Press <kbd>r</kbd> to give it a name, and <kbd>s</kbd> to save;
4. Back in the playlists list, press <kbd>Return</kbd> on a playlist (or on a song from it) to play.

### Most used keys

| Key | Action |
| --- | --- |
| <kbd>F12</kbd> | Show help with all keybindings |
| <kbd>F1</kbd> / <kbd>F2</kbd> | Show files / playlists |
| <kbd>1</kbd> / <kbd>2</kbd> / <kbd>3</kbd> | Show visualizer / equalizer / lyrics |
| <kbd>/</kbd> | Search in the focused list |
| <kbd>p</kbd> / <kbd>s</kbd> | Pause or resume / stop |
| <kbd>&lt;</kbd> / <kbd>&gt;</kbd> | Skip to previous / next song |
| <kbd>f</kbd> / <kbd>b</kbd> | Seek forward / backward |
| <kbd>+</kbd> / <kbd>-</kbd> / <kbd>m</kbd> | Volume up / down / mute |
| <kbd>R</kbd> / <kbd>x</kbd> | Change repeat mode (off, all, one) / toggle shuffle |
| <kbd>a</kbd> / <kbd>z</kbd> | Choose visualizer animation / toggle fullscreen |
| <kbd>t</kbd> | Choose theme |
| <kbd>q</kbd> | Quit |

### Files

Playlists and settings (volume, theme and visualizer animation) are saved in `$XDG_CONFIG_HOME/spectrum`, or `~/.config/spectrum` when that is not set. Log is written to `~/.cache/spectrum/spectrum.log` (use `-l <path>` to change it, and `-v` for verbose messages).

## Development :memo:

To build spectrum, you need a C++ compiler installed on your system along with another dependencies that are listed below:

```bash
# Package dependencies (on Ubuntu)
sudo apt install build-essential libasound2-dev libavcodec-dev \
     libavfilter-dev libavformat-dev libfftw3-dev libswresample-dev \
     libcurl4-openssl-dev libxml++2.6-dev

# Optional: install yt-dlp to play songs from YouTube (only needed at runtime, found in PATH)
sudo apt install yt-dlp

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

To ensure that any new implementation won't impact the existing one, you should check that by running all unit tests. To enable unit testing, you may configure using the following settings:

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
- [yt-dlp](https://github.com/yt-dlp/yt-dlp) <sup>(optional)</sup>

## Contributing

Contributions are always welcome! If you find any bugs or have suggestions for new features, please open an issue or submit a pull request.

## License

This project is licensed under the MIT License. See the LICENSE file for details.
