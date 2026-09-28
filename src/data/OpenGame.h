#pragma once
// The game in the open format: one folder holding the intro and every table, each with its
// music, which is all the engine reads. A copy of the DOS release is turned into one once,
// by convertGame, and is not looked at again.
#include <array>
#include <filesystem>
#include <optional>
#include <string>

namespace pfr {

struct OpenGame {
  std::filesystem::path directory;
  std::filesystem::path intro;                      ///< a folder, for loadIntroAssets
  std::array<std::filesystem::path, 4> tables;      ///< folders, for loadTableAssets
  std::array<std::filesystem::path, 4> tableMusic;  ///< module files
  std::filesystem::path introMusic, menuMusic;

  /// The layout inside `dir`, whether or not anything is there yet.
  static OpenGame at(const std::filesystem::path& dir);
  /// Whether `dir` holds a converted game in the format this version reads.
  static bool isIn(const std::filesystem::path& dir);
};

/// Converts the DOS release in `dosDir` into `outDir`: the intro and four tables, their
/// music, and the options and high scores when the folder has them. The release is checked
/// first, and nothing is written for any other one. The folder is written beside its final
/// place and moved there at the end, so a conversion that stops halfway leaves nothing that
/// looks finished. Throws DataError on anything that goes wrong.
void convertGame(const std::filesystem::path& dosDir, const std::filesystem::path& outDir);

}  // namespace pfr
