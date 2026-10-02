// Turns a copy of the 1994 DOS release into the open format the game reads: the intro and
// the four tables as pictures and JSON, each with its music.
//   encore-convert <folder with the DOS files> <folder to write>
#include <cstdio>
#include <exception>

#include "data/OpenGame.h"

int main(int argc, char** argv) {
  if (argc != 3) {
    std::fprintf(stderr, "usage: encore-convert <folder with the DOS files> <folder to write>\n");
    return 2;
  }
  try {
    pfr::convertGame(argv[1], argv[2]);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "encore-convert: %s\n", e.what());
    return 1;
  }
  return 0;
}
