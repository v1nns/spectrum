# spectrum

Introducing yet another music player for audiophiles and tech enthusiasts alike:
spectrum, a console-based music player written in C++ that will simplify the
way you experience your favorite tunes!

Immerse yourself in the sound with the powerful equalizer, allowing you to
fine-tune every aspect of your music to your exact specifications. With
real-time adjustments, you can easily adjust the bass, treble, and mid-range to
create a personalized sound that perfectly matches your mood.

But that's not all - spectrum also features an audio visualizer that takes your
listening experience to the next level. Watch as the sound waves come to life in
a stunning display of color and movement, adding a mesmerizing visual element to
your music.

With an intuitive user interface and lightning-fast performance, this music
player is the perfect addition to any audiophile's collection. Whether you're a
casual listener or a serious music lover, our console-based music player will
exceed your expectations.

## Features

- Simple and intuitive terminal user interface;
- Plays music in any format;
- Basic playback controls such as play, pause, stop, and skip;
- Displays information about the currently playing track;
- Audio spectrum visualizer and equalizer.

---

## Installation

To build spectrum, you need to have a C++ compiler installed on your system.

```bash
# Package dependencies (on Ubuntu)
sudo apt install build-essential libasound2-dev libavcodec-dev \
     libavfilter-dev libavformat-dev libfftw3-dev libswresample-dev

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

---

## Contributing

Contributions are always welcome! If you find any bugs or have suggestions for
new features, please open an issue or submit a pull request.

## License

This project is licensed under the MIT License. See the LICENSE file for
details.
