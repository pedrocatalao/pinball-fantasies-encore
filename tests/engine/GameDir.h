#pragma once
// Where the tests find the game's files: ENCORE_DATA, or the FANTASY folder beside the project.
// None of them is in the repository: without them the tests that need them are skipped, unless
// ENCORE_REQUIRE_DATA is set (as CI does), when the table files missing fail the run instead.
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "data/GameVersion.h"

namespace test {
inline std::filesystem::path gameDir() {
  if (const char* d = std::getenv("ENCORE_DATA")) return d;
  return std::filesystem::path(ENCORE_SOURCE_DIR) / ".." / ".." / "FANTASY";
}

/// The game's files of the supported version that are missing or not that version: all of them,
/// or only the tables' (TABLE1.PRG ... TABLE4.MOD, all a game or a recording needs; the menu's
/// INTRO.PRG and MOD2.MOD are left out).
inline std::vector<std::string> missingGameFiles(bool menuToo) {
  auto bad = encore::unsupportedGameFiles(gameDir());
  if (!menuToo)
    std::erase_if(bad, [](const std::string& name) { return name == "INTRO.PRG" || name == "MOD2.MOD"; });
  return bad;
}
inline bool haveTables() { return missingGameFiles(false).empty(); }
inline bool haveMenu() { return missingGameFiles(true).empty(); }
}  // namespace test
