// Windows wants its own entry point, which this header provides. On macOS the same header
// hands the application to SDL's Cocoa loop, which then asks it to quit the moment it starts,
// so it stays where it is needed.
#ifdef _WIN32
#include <SDL3/SDL_main.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <utility>

#include "core/Log.h"
#include "game/App.h"

namespace {

void usage() {
  std::puts("Pinball Fantasies: Encore!\n"
            "  --data <dir>     folder with the original game files (INTRO.PRG, TABLE1.PRG, ...)\n"
            "  --table <1-4>    open a table directly instead of the menu\n"
            "  --fullscreen     start in fullscreen\n"
            "  --skip-intro     go straight to the table chooser\n"
            "  --square-pixels  do not stretch the picture to the original 4:3 shape\n"
            "  --smooth         soften pixel edges, which steadies the picture while scrolling\n"
            "  --crt            CRT look: scanlines, shadow mask, glow (F9 switches it); remembered\n"
            "  --no-crt         the original crisp pixels\n"
            "  --hd, --no-hd    the remastered pictures, or the originals (F10); remembered\n"
            "  --trail, --no-trail  the fading ghosts behind the ball (F8 while paused); remembered\n"
            "  --hd-dir <dir>   HD pictures to use instead of the ones fetched from the server (assets/hd, say)\n"
            "  --res <mode>     screen mode: normal (320x240), high (320x350), full (whole table) or tall\n"
            "                   (whole table with square pixels, for a screen turned on its side)\n"
            "  --scale <n>      window scale factor (default 3)\n"
            "  --screenshot <f> render a frame to a PNG file and quit\n"
            "  --screenshot-frame <n>  which frame to capture (default 30)\n"
            "  --stats          log how long each frame takes\n"
            "  <file.RPL> ...   play recordings one after another, then stay on the last one's table\n"
            "                   (also by dropping one on the program or its window)\n"
            "  --video <dir>    film the recordings instead: a silent clip of each into <dir>, by ffmpeg\n"
            "  --video-from <s>, --video-seconds <n>  which part of each (default from 12 s, for 9 s)\n"
            "  --verbose        debug logging\n");
}

}  // namespace

int main(int argc, char** argv) {
  encore::AppOptions options;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
    if (a == "--data") options.dataDir = next();
    else if (a == "--table") options.table = std::atoi(next());
    else if (a == "--fullscreen") options.fullscreen = true;
    else if (a == "--skip-intro") options.skipIntro = true;
    else if (a == "--square-pixels") options.squarePixels = true;
    else if (a == "--smooth") options.smoothEdges = true;
    else if (a == "--crt") options.crt = true;
    else if (a == "--no-crt") options.crt = false;
    else if (a == "--hd") options.hd = true;
    else if (a == "--no-hd") options.hd = false;
    else if (a == "--trail") options.trail = true;
    else if (a == "--no-trail") options.trail = false;
    else if (a == "--hd-dir") options.hdDir = next();
    else if (a == "--res") {
      const std::string m = next();
      options.resolution = m == "normal" ? encore::Resolution::Normal
                           : m == "full"   ? encore::Resolution::Full
                           : m == "tall"   ? encore::Resolution::Tall
                                           : encore::Resolution::High;
    }
    else if (a == "--scale") options.windowScale = std::max(1, std::atoi(next()));
    else if (a == "--screenshot") options.screenshot = next();
    else if (a == "--screenshot-frame") options.screenshotFrame = std::max(1, std::atoi(next()));
    else if (a == "--stats") options.stats = true;
    else if (a == "--verbose") encore::log::setMinimumLevel(encore::log::Level::Debug);
    else if (a == "--help" || a == "-h") { usage(); return 0; }
    else if (a.rfind("-psn", 0) == 0) { /* macOS launch services */ }
    else if (a == "--video") options.video = next();
    else if (a == "--video-from") options.videoFrom = std::max(0, std::atoi(next()));
    else if (a == "--video-seconds") options.videoSeconds = std::max(1, std::atoi(next()));
    else if (std::error_code ec; a[0] != '-' && std::filesystem::is_regular_file(a, ec)) options.replays.push_back(a);
    else { usage(); return 2; }
  }
  return encore::App(std::move(options)).run();
}
