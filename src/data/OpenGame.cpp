#include "data/OpenGame.h"

#include <string>
#include <system_error>

#include "assets/OpenFormat.h"
#include "assets/TableAssetsIo.h"
#include "core/Error.h"
#include "core/File.h"
#include "core/Log.h"
#include "data/GameFiles.h"
#include "data/GameVersion.h"
#include "intro/IntroAssetsIo.h"

namespace pfr {
namespace {

namespace fs = std::filesystem;
constexpr int kFormat = 1;

void copyFile(const fs::path& from, const fs::path& to) {
  std::error_code ec;
  fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
  if (ec) throw DataError("cannot copy " + from.string() + " to " + to.string() + ": " + ec.message());
}

}  // namespace

OpenGame OpenGame::at(const fs::path& dir) {
  OpenGame g;
  g.directory = dir;
  g.intro = dir / "intro";
  g.introMusic = dir / "intro" / "intro.mod";
  g.menuMusic = dir / "intro" / "menu.mod";
  for (std::size_t i = 0; i < 4; ++i) {
    g.tables[i] = dir / ("table" + std::to_string(i + 1));
    g.tableMusic[i] = g.tables[i] / "music.mod";
  }
  return g;
}

bool OpenGame::isIn(const fs::path& dir) {
  std::error_code ec;
  if (!fs::exists(dir / "game.json", ec)) return false;
  try {
    return open::readJson(dir / "game.json").at("format").integer() == kFormat;
  } catch (const DataError&) {
    return false;
  }
}

void convertGame(const fs::path& dosDir, const fs::path& outDir) {
  const auto bad = unsupportedGameFiles(dosDir);
  if (!bad.empty()) {
    std::string names;
    for (const auto& n : bad) names += " " + n;
    throw DataError(dosDir.string() + " holds a different release of the game (" + names.substr(1) + " differ)");
  }
  const GameFiles dos = GameFiles::fromDirectory(dosDir);

  std::error_code ec;
  const fs::path part = outDir.parent_path() / (outDir.filename().string() + ".part");
  fs::remove_all(part, ec);
  const OpenGame g = OpenGame::at(part);

  const auto read = [](const fs::path& p) {
    auto b = file::readAll(p);
    if (!b) throw DataError("cannot read " + p.string());
    return *b;
  };

  log::info("converting the intro");
  saveIntroAssets(IntroAssets::load(read(dos.intro)), g.intro);
  copyFile(dos.introMusic, g.introMusic);
  copyFile(dos.menuMusic, g.menuMusic);
  for (std::size_t i = 0; i < 4; ++i) {
    log::info("converting table " + std::to_string(i + 1));
    saveTableAssets(TableAssets::load(read(dos.tables[i]), static_cast<int>(i)), g.tables[i]);
    copyFile(dos.tableMusic[i], g.tableMusic[i]);
  }
  // Options and high scores, where the DOS game has left them, to be taken up the first time.
  for (const std::string name : {"PINBALL.CFG", "TABLE1.HI", "TABLE2.HI", "TABLE3.HI", "TABLE4.HI"})
    if (const auto p = file::findCaseInsensitive(dosDir, name)) copyFile(*p, part / name);

  // Written last: a folder without it is not a converted game.
  Json manifest = Json::object();
  manifest.set("format", kFormat);
  manifest.set("source", "Pinball Fantasies, MS-DOS release, 1994");
  Json tables = Json::array();
  for (std::size_t i = 0; i < 4; ++i) tables.push("table" + std::to_string(i + 1));
  manifest.set("tables", tables);
  open::writeJson(part / "game.json", manifest);

  fs::remove_all(outDir, ec);
  fs::rename(part, outDir, ec);
  if (ec) throw DataError("cannot move the converted game into " + outDir.string() + ": " + ec.message());
  log::info("converted game written to " + outDir.string());
}

}  // namespace pfr
