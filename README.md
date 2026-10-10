<p align="center"><img src="docs/readme-header.png" alt="Pinball Fantasies: Encore!" width="820"></p>

# Pinball Fantasies: Encore!

[![macOS](https://github.com/pedrocatalao/pinball-fantasies-encore/actions/workflows/macos.yml/badge.svg)](https://github.com/pedrocatalao/pinball-fantasies-encore/actions/workflows/macos.yml)
[![Linux](https://github.com/pedrocatalao/pinball-fantasies-encore/actions/workflows/linux.yml/badge.svg)](https://github.com/pedrocatalao/pinball-fantasies-encore/actions/workflows/linux.yml)
[![Windows](https://github.com/pedrocatalao/pinball-fantasies-encore/actions/workflows/windows.yml/badge.svg)](https://github.com/pedrocatalao/pinball-fantasies-encore/actions/workflows/windows.yml)

*Pinball Fantasies*, the 1994 PC classic by Digital Illusions, running natively on macOS,
Windows and Linux. All four tables, with the original rules and physics, and redrawn
high-resolution artwork you can switch on and off at any time.

## Download

<p align="center">
<a href="https://github.com/pedrocatalao/pinball-fantasies-encore/releases/latest/download/pinball-fantasies-encore-macos-universal.zip"><img src="https://img.shields.io/badge/macOS-Universal-66a8c2?style=for-the-badge&labelColor=14232b&color=66a8c2&logoColor=white&logo=apple" alt="Download for macOS (Universal)"></a>
<a href="https://github.com/pedrocatalao/pinball-fantasies-encore/releases/latest/download/pinball-fantasies-encore-windows-x64.zip"><img src="https://img.shields.io/badge/Windows-x64-66a8c2?style=for-the-badge&labelColor=14232b&color=66a8c2&logoColor=white&logo=data%3Aimage%2Fsvg%2Bxml%3Bbase64%2CPHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCIgZmlsbD0id2hpdGUiPjxwYXRoIGQ9Ik0yIDMuNWw4LjUtMS4ydjguM0gyek0xMS41IDIuMkwyMiAuOHY5LjhIMTEuNXpNMiAxMS42aDguNXY4LjNMMiAxOC43ek0xMS41IDExLjZIMjJ2OS43bC0xMC41LTEuNHoiLz48L3N2Zz4%3D" alt="Download for Windows (x64)"></a>
<a href="https://github.com/pedrocatalao/pinball-fantasies-encore/releases/latest/download/pinball-fantasies-encore-linux-x86_64.tar.gz"><img src="https://img.shields.io/badge/Linux-x86__64-66a8c2?style=for-the-badge&labelColor=14232b&color=66a8c2&logoColor=white&logo=linux" alt="Download for Linux (x86_64)"></a>
<a href="https://github.com/pedrocatalao/pinball-fantasies-encore/releases/latest/download/pinball-fantasies-encore-linux-arm64.tar.gz"><img src="https://img.shields.io/badge/Linux-arm64-66a8c2?style=for-the-badge&labelColor=14232b&color=66a8c2&logoColor=white&logo=linux" alt="Download for Linux (arm64)"></a>
</p>

Unzip it and run it, there's nothing to install.

- **macOS and Windows:** the game isn't signed, so the first time your system will warn you
  before opening it. See [first run on macOS and Windows](#first-run-on-macos-and-windows) below.
- **Linux:** `./install.sh` in the unpacked folder adds the game to your app menu with its icon
  (everything goes under `~/.local`). It's optional, the game runs from the folder too.
- Older versions are on the [releases page](https://github.com/pedrocatalao/pinball-fantasies-encore/releases).

Found a problem? Please [open an issue](https://github.com/pedrocatalao/pinball-fantasies-encore/issues).

## It needs the original game files

Nothing from the original game is included here. The game reads everything from the 1994 DOS
files: the pictures, the collision maps, the table scripts, the music and the sound effects.

It needs exactly the 1994 disk release, since it reads data from fixed places in those files.
Every file is checked when the game starts, and other releases are refused.
[docs/game-files.md](docs/game-files.md) lists the files and their checksums.

The first time it runs, once you confirm you own a legal copy of the game, it downloads the right files for you from a preservation website.
They're kept in a `FANTASY` folder here:

| System | Folder |
| --- | --- |
| macOS | `~/Library/Application Support/Encore/Pinball Fantasies/FANTASY` |
| Linux | `~/.local/share/Encore/Pinball Fantasies/FANTASY` |
| Windows | `%APPDATA%\Encore\Pinball Fantasies\FANTASY` |

## What's in it

**It plays like the original.** Since 1.1.0 Encore runs on its own engine, written from
scratch from the DOS game's code: physics, table rules, dot matrix, music, intro and menu.
Nothing is emulated. While building it, every frame was checked against the original
program running side by side, so the tables behave exactly as they did in 1994. There are a few
small changes on purpose, all listed in [differences from the original](docs/differences-from-the-original.md),
and [the engine notes](docs/own-engine.md) explain how it was made.

**Remastered artwork, not an upscale.** Every playfield, flipper, ball, slide and banner was
redrawn by hand at high resolution, following the original's shapes, colours and lamps. No
filters, no AI. Because the new pictures line up exactly with the old ones, F10 swaps between
them at any moment, even in the middle of a ball.

The redrawn pictures aren't in the download. The first time you run the game it offers to
fetch them (about 30 MB), and later it offers updates when there are new ones. Say no and you
get the original pictures.

**Balanced sound.** The same music and sound effects, halfway between the 1994 sound driver and a
smooth modern player: less metallic edge on the samples but still crisp, stereo that's a little
easier on headphones, and no clicks. AUDIO in the options (or the 0 key) switches back to the
original sound.

**Four screen sizes,** under Resolution in the options or with R in the pause:

| Size | What you see |
| --- | --- |
| Normal | 240 rows, as in 1994: the screen follows the ball |
| High | 350 rows, as in 1994: more of the table, still following the ball |
| Full | the whole table at once, flat pixels, for a normal screen |
| Tall | the whole table with square pixels, for a widescreen monitor turned 90° |

**Online high scores.** When you get a high score, the game asks if you want to send it. The
server replays the game from its recording before it counts, so every score on
[thebestpinball.com](https://thebestpinball.com) was really played.

**Recordings.** Every game you play is saved in a `replays` folder next to your high scores.
Drop a `.RPL` file on the game window to watch it, in your own screen size. Escape stops it.
You can download the recordings of the online scores from the website and watch them too.

<p align="center"><img src="docs/readme-compare.png" alt="Stones 'n' Bones, the 1994 picture and the remastered one, a line sweeping between them" width="720"></p>
<p align="center"><em>Stones 'n' Bones: the 1994 artwork on the left of the line, the redrawn one on the right.</em></p>

## Controls

The original key layout.

**In the menu**

| Key | What it does |
| --- | --- |
| <kbd>F1</kbd> – <kbd>F4</kbd>* | pick a table |
| <kbd>F5</kbd>* | options |
| <kbd>Esc</kbd> | quit |

\* Or the number keys <kbd>1</kbd> – <kbd>5</kbd>.

**Playing**

| Key | What it does |
| --- | --- |
| <kbd>Enter</kbd> | start a game, press again before launching to add a player |
| <kbd>F1</kbd> – <kbd>F8</kbd>* | start a game for that many players |
| <kbd>Shift</kbd> <kbd>Ctrl</kbd> <kbd>Alt</kbd> | flippers, left and right (any of the three) |
| <kbd>↓</kbd> | pull the plunger, let go to launch |
| <kbd>Space</kbd> | nudge the table (too much and you tilt) |
| <kbd>M</kbd> | music on or off (stays that way for the next games too) |
| <kbd>P</kbd> | pause |
| <kbd>Esc</kbd> | give up the game, with the ball still at the plunger. With no game on, leave the table (<kbd>Y</kbd> to confirm) |

\* Or the number keys <kbd>1</kbd> – <kbd>8</kbd>.

**Paused**

| Key | What it does |
| --- | --- |
| <kbd>A</kbd> | angle: low, high or higher (steeper, with stronger flippers) |
| <kbd>S</kbd> | scrolling: hard, medium or soft |
| <kbd>R</kbd> | resolution: normal, high, full or tall |
| <kbd>M</kbd> | music on or off |
| <kbd>D</kbd> | dot matrix above the table (like the Amiga) or below it (DOT MATRIX in the options) |
| <kbd>↑</kbd> <kbd>↓</kbd> | scroll the table by hand |
| <kbd>F7</kbd> | lamps: all on, all off, back to normal |
| <kbd>F8</kbd> | ball trail on or off |
| <kbd>P</kbd> | back to the game |
| <kbd>Esc</kbd> | give up the game (<kbd>Y</kbd> to confirm) |

**Any time**

| Key | What it does |
| --- | --- |
| <kbd>F9</kbd> | CRT look on or off |
| <kbd>F10</kbd> | redrawn or original pictures (ARTWORK in the options) |
| <kbd>0</kbd> | balanced or original sound (AUDIO in the options) |
| <kbd>F11</kbd> | full screen or window (on a Mac, <kbd>⌘</kbd> <kbd>F</kbd> too) |

## First run on macOS and Windows

The game isn't signed with a paid developer certificate, so the first time you open it your
system will stop and ask. You only need to do this once.

- **Windows:** when it says "Windows protected your PC", click **More info**, then **Run anyway**.
- **macOS:** when it says the app can't be opened, go to **System Settings → Privacy &
  Security**, scroll down and click **Open Anyway**.

On macOS you can also do it from a terminal instead:

```bash
xattr -dr com.apple.quarantine "Pinball Fantasies.app"
```

## Building it yourself

You need CMake, a C++20 compiler and SDL3, on macOS, Linux or Windows.
[docs/building.md](docs/building.md) has the steps, the command-line options, the extra tools
and a map of the source.

## Licence

The code is under the [GNU GPL, version 3 or later](LICENSE). The redrawn artwork in `assets`
is under CC BY-SA 4.0, which suits pictures better. [NOTICE.md](NOTICE.md) has the details.

*Pinball Fantasies* belongs to its owners, and this project isn't affiliated with them. No file
from the original game is included here or in the downloads.
