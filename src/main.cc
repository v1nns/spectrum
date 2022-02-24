#include "error_code.h"
#include "sound/wave.h"
#include "ui/terminal.h"

#include <unistd.h>

void testWaveParser() {
  WaveFormat song;
  bool result = song.ParseFromFile(
      "/home/vinicius/projects/music-analyzer/africa-toto.wav");
  if (result) {
    song.PrintStats();
  }
}

int main() {
  Terminal term;
  term.Init();

  while (term.Tick()) {
    usleep(200);
  }

  term.Cleanup();
  return ERR_OK;
}