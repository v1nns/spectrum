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
- Choose audio output device, playing each song with its own sample rate when device supports it (not converting it);
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

#### When YouTube refuses to answer

After too many requests, YouTube may refuse new ones and ask for a login to prove that they do not come from a bot. It usually goes away by itself after some time, but spectrum can also ask yt-dlp to send the cookies from your browser, where you are already logged in. To allow it, add this to `settings.json` (see [Files and settings](#files-and-settings)), using the browser as [expected by yt-dlp](https://github.com/yt-dlp/yt-dlp#filesystem-options) (`BROWSER[+KEYRING][:PROFILE][::CONTAINER]`):

```json
{
  "stream": {
    "cookies_from_browser": "firefox"
  }
}
```

Cookies are read by yt-dlp (never by spectrum), and only after a request is refused; from then on, they are sent by every request until spectrum is closed. A few things to know before using it:

- these cookies are your login, and yt-dlp warns that an account used like this may be flagged (or even banned) by YouTube, so prefer a secondary account in a separate browser profile (e.g. `firefox:music`);
- Firefox is the simplest choice on Linux, as browsers based on Chromium keep cookies encrypted by the system keyring, which yt-dlp must be able to reach.

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
| <kbd>O</kbd> | Choose audio output device |
| <kbd>q</kbd> | Quit |

### Remote control

A running `spectrum` may be controlled from another terminal (or from a script, a window manager keybinding, etc.) with `spectrum -r <command>`, for example `spectrum -r next`. Available commands:

| Command | Action |
| --- | --- |
| `play-pause` | Play selected song, or pause/resume the current one |
| `play` / `pause` | Same as above, but without toggling (useful for scripts) |
| `play <target>` | Play a file (followed by the other ones from its directory), a directory, a YouTube URL or a saved playlist (by its name) |
| `stop` | Stop current song |
| `previous` / `next` | Skip to previous / next song |
| `seek-forward` / `seek-backward` | Seek forward / backward |
| `seek <position>` | Seek to a position (`90` or `1:30`), or by some seconds from the current one (`+10` or `-10`) |
| `volume-up` / `volume-down` / `mute` | Volume up / down / mute |
| `volume <level>` | Set volume (from `0` to `100`), or change it (`+5` or `-5`) |
| `repeat` / `shuffle` | Change repeat mode (off, all, one) / toggle shuffle |
| `repeat <off\|all\|one>` / `shuffle <on\|off>` | Set repeat mode / shuffle, without toggling |
| `quit` | Exit from the running instance |
| `status` | Print what is playing (nothing changes on player) |
| `subscribe` | Same as `status`, but keeps running and prints it again every time something changes |

A command and its value may be written as separate words or as a single one, so `spectrum -r volume 50` is the same as `spectrum -r "volume 50"`.

By default, `status` is printed as JSON in a single line, useful for scripts and status bars:

```bash
$ spectrum -r status
{"artist":"NIKITO","duration":123,"muted":false,"output_bit_depth":32,"output_device":"default","output_sample_rate":44100,"position":42,"repeat":"off","shuffle":false,"state":"playing","title":"Bounce","volume":80}
```

Fields starting with `output` tell how the song is being played: the device in use and the format of audio sent to it, which may not be the same one from song (they are empty when nothing is playing).

Use `-f <text>` to print it as text instead, where each field name between braces is replaced by its value (`state` is `playing`, `paused` or `stopped`; `position` and `duration` are printed as time):

```bash
$ spectrum -r status -f "{artist} - {title} [{position}/{duration}]"
NIKITO - Bounce [00:42/02:03]
```

With `subscribe`, a status bar does not need to ask for status from time to time: one line is printed right away and another one on every change (while playing, that is once per second, because of `position`), until the running instance exits. It also accepts `-f <text>`, and then a line is printed only when that text changes.

When more than one instance is running, only the first one started receives the commands.

### Files and settings

Playlists (`playlists.json`) and settings (`settings.json`) are saved in `$XDG_CONFIG_HOME/spectrum`, or `~/.config/spectrum` when that is not set. Log is written to `~/.cache/spectrum/spectrum.log` (use `-l <path>` to change it, and `-v` for verbose messages).

Almost everything in `settings.json` is saved by spectrum itself, right when it is changed in the interface, so there is usually no need to edit this file (and it does not even exist until something is changed). This is all it may contain:

```json
{
  "interface": {
    "theme": "tokyo-night"
  },
  "player": {
    "volume": 80,
    "device": "default",
    "repeat": "off",
    "shuffle": false
  },
  "equalizer": {
    "preset": "Custom",
    "custom": [3, 2, 0, 0, -1, 0, 0, 1, 2, 4]
  },
  "visualizer": {
    "animation": "horizontal-mirror",
    "bar_width": 2
  },
  "stream": {
    "cookies_from_browser": "firefox"
  }
}
```

| Setting | Value | Default | Changed with |
| --- | --- | --- | --- |
| `interface.theme` | `tokyo-night`, `catppuccin-mocha`, `gruvbox-dark`, `nord`, `dracula`, `catppuccin-latte` or `terminal` (colors from your terminal) | `tokyo-night` | <kbd>t</kbd> |
| `player.volume` | Percentage, from `0` to `100` (mute is not saved) | `100` | <kbd>+</kbd> / <kbd>-</kbd>, or `-r volume` |
| `player.device` | Name of an audio output device from ALSA, or an empty text to let spectrum choose (default device from system, or the first one available) | empty | <kbd>O</kbd> |
| `player.repeat` | `off`, `all` or `one` | `off` | <kbd>R</kbd>, or `-r repeat` |
| `player.shuffle` | `true` or `false` | `false` | <kbd>x</kbd>, or `-r shuffle` |
| `equalizer.preset` | Preset in use: `Custom`, `Flat`, `Acoustic`, `Bass Boost`, `Classical`, `Dance`, `Electronic`, `Hip-Hop`, `Jazz`, `Loudness`, `Pop`, `Rock`, `Treble Boost` or `Vocal` | `Custom` | <kbd>a</kbd> (apply) on equalizer |
| `equalizer.custom` | Gains (in dB, from `-12` to `12`) from preset `Custom`, one for each of its ten frequencies (32, 64, 125, 250, 500, 1k, 2k, 4k, 8k and 16k Hz) | all `0` | <kbd>a</kbd> (apply) or <kbd>r</kbd> (reset) on equalizer |
| `visualizer.animation` | `horizontal-mirror`, `vertical-mirror`, `mono`, `horizontal-mirror-no-space`, `vertical-mirror-no-space`, `mono-no-space`, `line`, `line-mirror`, `line-filled` or `line-filled-mirror` (a number saved by older versions is still accepted) | `horizontal-mirror` | <kbd>a</kbd> on visualizer |
| `visualizer.bar_width` | Columns used by each bar, from `1` to `4` | `2` | <kbd>,</kbd> / <kbd>.</kbd> |
| `stream.cookies_from_browser` | Browser that yt-dlp reads cookies from (see [When YouTube refuses to answer](#when-youtube-refuses-to-answer)) | not set | only by editing this file |

Any setting may be missing, and a value that is not valid is ignored (default is used instead). Settings are read only when spectrum starts, so edit this file while it is not running. When the file itself cannot be read as JSON, it is copied to `settings.json.bak` before spectrum saves a new one.

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
