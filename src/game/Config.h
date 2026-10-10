#pragma once
// Options and high scores, stored in the DOS game's own formats (PINBALL.CFG, TABLEn.HI)
// so they stay interchangeable with the original.
#include <array>
#include <filesystem>

#include "core/Bcd.h"

namespace encore {

enum class ScrollSpeed : u8 { Hard, Medium, Soft };
inline i16 rawScrollSpeed(ScrollSpeed s) { return s == ScrollSpeed::Hard ? 20 : s == ScrollSpeed::Medium ? 11 : 9; }

/// Normal and High are the original's screens; Full and Tall are this version's, the whole table
/// at once: Full with High's pixels, Tall with square ones (for a screen turned on its side).
enum class Resolution : u8 { Normal, High, Full, Tall };

/// Table slope. Low and High are the original's. Higher is this remake's addition: the
/// same step again beyond High, with stronger flippers and plunger to match.
enum class Angle : u8 { Low, High, Higher };
inline Angle nextAngle(Angle a) { return a == Angle::High ? Angle::Higher : a == Angle::Higher ? Angle::Low : Angle::High; }

struct Options {
  u8 balls = 5;  // the original's setup says 3; five is the game most people want
  Angle angle = Angle::High;
  ScrollSpeed scrollSpeed = ScrollSpeed::Medium;
  Resolution resolution = Resolution::Normal;
  bool noMusic = false;
  bool mono = false;
  // This version's own, kept beside PINBALL.CFG (sfx.txt, hd.txt, dotmatrix.txt) rather than in
  // it: the driver's own mixing rather than the remastered one, the 1994 pictures rather than
  // the HD, and the dot display above the table rather than below it (as the Amiga's).
  bool originalSound = false;
  bool originalPictures = false;
  bool dotMatrixTop = false;
};

struct HighScore {
  Bcd score;
  std::array<u8, 3> name{};
};
using HighScores = std::array<HighScore, 4>;

struct Config {
  Options options;
  std::array<HighScores, 4> highScores;

  static Config defaults();
  /// Reads PINBALL.CFG and TABLEn.HI from `dir`, or failing that from `fallbackDir` (the
  /// game folder, so a player's DOS high scores carry over), keeping defaults otherwise.
  static Config load(const std::filesystem::path& dir, const std::filesystem::path& fallbackDir = {});
  static void saveOptions(const std::filesystem::path& dir, const Options& o);
  static void saveHighScores(const std::filesystem::path& dir, int table, const HighScores& s);
};

}  // namespace encore
