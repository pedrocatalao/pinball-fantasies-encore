# Building, options and layout

## Building

The game needs CMake 3.24 or later, a C++20 compiler, SDL3 and OpenGL 4.1. Nothing else: the
pictures are decoded by this project's own PNG reader, and every OpenGL function past 1.1 is
asked of the driver through SDL, so there is no image library and no loader to install.

**macOS** (Xcode command-line tools, `brew install sdl3`; for a build that runs on both Apple
Silicon and Intel, point `-DENCORE_SDL3_FRAMEWORK=` at the official universal `SDL3.framework`
and set `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`):

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build
open "build/Pinball Fantasies.app"
```

The tests that play whole games need the game's files, from `ENCORE_DATA` or a `FANTASY` folder
beside the project, and are skipped without them; `ENCORE_REQUIRE_DATA=1` makes their absence a
failure instead, as CI has it (see [own-engine.md](own-engine.md#the-game)).

**Linux** (SDL3 from your distribution, or built from source, plus `libgl1-mesa-dev`):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
"./build/Pinball Fantasies" --data /path/to/FANTASY
```

**Windows** (Visual Studio 2022, and the SDL3 development files unpacked somewhere):

```bat
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH=C:\SDL3-3.4.16\cmake
cmake --build build --config RelWithDebInfo
```

On macOS the shaders and pictures go inside the application bundle; elsewhere they are copied
next to the executable, which is where the game looks for them. Settings and high scores live
in the folder SDL keeps for the platform, under `Encore/Pinball Fantasies`
(`~/Library/Application Support/...`, `~/.local/share/...`, `%APPDATA%\...`).

## Command line

| Option | Effect |
| --- | --- |
| `--data <dir>` | folder with the game files |
| `--table <1-4>` | open a table directly |
| `--skip-intro` | go straight to the table menu |
| `--res normal\|high\|full\|tall` | screen mode: 320x240, 320x350, or the whole table at once, with 320x350's pixels (full) or square ones for a screen turned on its side (tall) |
| `--crt`, `--no-crt` | CRT look (scanlines, shadow mask, glow); remembered |
| `--hd`, `--no-hd` | the remastered pictures, or the originals; remembered |
| `--trail`, `--no-trail` | the fading ghosts behind the ball; remembered |
| `--hd-dir <dir>` | HD pictures to use instead of the ones fetched from the server; `--hd-dir assets/hd` shows changes to them at once |
| `--smooth` | soften the one pixel that straddles two source pixels; steadies scrolling |
| `--square-pixels` | show the picture unstretched instead of filling a 4:3 screen |
| `--fullscreen`, `--scale <n>` | window options; fullscreen is otherwise as it was left last time |
| `--screenshot <file>`, `--screenshot-frame <n>` | render one frame to a PNG and quit |
| `--stats` | log, once a second, how long each frame takes |
| `<file.RPL> ...` | play recordings one after another, then stay on the last one's table (dropping one on the program or its window does the same) |
| `--video <dir>` | film the recordings instead: a silent clip of each into `<dir>`, made by ffmpeg |
| `--video-from <s>`, `--video-seconds <n>` | which part of each recording is filmed (default from 12 s, for 9 s) |
| `--verbose` | debug logging |
| `--verbose` | log every step, not only what matters |
| `--help` | the options, and what they do |

## Tools

| Tool | Purpose |
| --- | --- |
| `encore-play <dir> <table> <frames> <seed> [out.png] [out.wav]` | plays games with no window by keys pressed at random, twice and from their recording, and says whether all three came out the same |
| `encore-play <dir> --replay <file.RPL>`, `--verify <file.RPL>` | plays a recording again; or checks it as the server does, and answers in JSON |
| `encore-front <dir> <frames> <out prefix> [--menu <table>] [--keys ...]` | the slides and the menu with no window, frames of it written as pictures |
| `encore-music <dir> <module> <seconds> <out.wav>` | a module of the game's played through the sound driver of ours, into a sound file |
| `encore-extract <dir> <out>` | writes each table's playfield as the original draws it, with every lamp lit and with every lamp out |
| `encore-oracle-table <dir> <table> <frames> ...` | the engine against the original's own program, frame by frame ([own-engine.md](own-engine.md)) |
| `tools/hd_import.py <name>=<picture> ...` | prepares redrawn pictures (trims, resizes to 3x the original) into `assets/hd` |
| `tools/hd_unlit.py <lit.png> <unlit.png> <table dir>` | derives a playfield's lights-off picture from its lights-on one, using the original's lamps (needs `encore-extract` output) |
| `tools/mkico.py <icon.png> <out.ico>` | the Windows icon, compiled into the `.exe`; run once when the icon changes, and commit `packaging/windows/pinball.ico` |
| `tools/mkappicon.py <icon.png> <out.h>` | the window's own icon for the taskbar on Linux and Windows; run once when the icon changes, and commit `src/platform/AppIcon.h` |

## Layout

| Path | Purpose |
| --- | --- |
| `src/engine/table` | The tables: the engine the four share, and each one's rules, written from the game's own programs ([own-engine.md](own-engine.md)) |
| `src/engine/audio` | The sound driver the tables and the menu talk to, playing their music modules |
| `src/engine/view` | A table's screen, and what the high-resolution pictures need to know of it |
| `src/engine/game` | A table as the game plays it, recordings of games, and the slides and the menu |
| `src/engine/data`, `src/engine/sim` | Reading a table's files: its pictures, masks and sounds |
| `src/game` | Application shell and the options and high-score files |
| `assets/hd` | Redrawn, high-resolution pictures drawn in place of the originals: the intro's slides, the menu's side panel, table banners and high-score heading, each table's playfield lit and unlit, the flippers and the ball; and, per table, what hides the ball on the playfield and on the ramps (`hides_ball<n>_playfield.png`, `hides_ball<n>_ramps.png`: black hides it, white shows it, pure red is clear plastic it is seen through greyed) |
| `assets/app` | The application icon, built into `icon.icns` at build time |
| `src/gfx`, `shaders` | Indexed framebuffer, palette, OpenGL renderer; `post.frag` presents the picture, `crt-lottes.frag` gives the CRT look, and the shaders are hot-reloaded |
| `src/platform` | SDL3 window, audio device, finding the game files, fetching over HTTP |
| `src/core`, `src/data` | Types, files, PNG, deflate and zip, SHA-256, IFF pictures, the game-version check |
| `tests` | Pure-logic tests, and tests that play full games when the game files are present |
| `docs` | Notes from the reverse-engineering work |
