// The open table format: JSON that reads back as written, and every table read from its
// executable, written out and read back in, arriving exactly as it left.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "Test.h"
#include "assets/TableAssetsIo.h"
#include "core/File.h"
#include "core/Json.h"
#include "data/GameVersion.h"
#include "data/OpenGame.h"
#include "game/Config.h"
#include "table/Table.h"
#include "intro/IntroAssetsIo.h"

using namespace pfr;

namespace {

std::filesystem::path dataDir() {
  if (const char* d = std::getenv("ENCORE_DATA")) return d;
  return std::filesystem::path(ENCORE_SOURCE_DIR) / ".." / ".." / "FANTASY";
}

bool haveData() {
  static const bool ok = std::filesystem::exists(dataDir()) && unsupportedGameFiles(dataDir()).empty();
  return ok;
}

}  // namespace

TEST(json_reads_back_what_it_writes) {
  Json j = Json::object();
  j.set("n", -32768).set("big", 4294967295LL).set("yes", true).set("none", Json());
  j.set("text", "quote \" backslash \\ newline \n");
  j.set("bytes", Json::bytes({0x41, 0x94, 0x00, 0xff, 0x7f}));
  j.set("nested", Json::array().push(Json::array().push(1).push(2)).push(Json::object().set("k", "v")));
  const Json back = Json::parse(j.dump());
  CHECK_EQ(back.at("n").integer(), -32768);
  CHECK_EQ(back.at("big").integer(), 4294967295LL);
  CHECK(back.at("yes").boolean());
  CHECK(back.at("none").isNull());
  CHECK_EQ(back.at("text").string(), std::string("quote \" backslash \\ newline \n"));
  CHECK(back.at("bytes").toBytes() == (std::vector<u8>{0x41, 0x94, 0x00, 0xff, 0x7f}));
  CHECK_EQ(back.at("nested").at(0).at(1).integer(), 2);
  CHECK_EQ(back.at("nested").at(1).at("k").string(), std::string("v"));
  CHECK_EQ(back.dump(), j.dump());

  bool refused = false;
  try { Json::parse("{\"a\": 1.5}"); } catch (const DataError&) { refused = true; }
  CHECK(refused);
}

TEST(tables_survive_the_open_format_unchanged) {
  if (!haveData()) {
    std::printf("  (skipped: no supported game files at %s)\n", dataDir().string().c_str());
    return;
  }
  for (int t = 0; t < 4; ++t) {
    const auto prg = file::readAll(dataDir() / ("TABLE" + std::to_string(t + 1) + ".PRG"));
    CHECK(prg.has_value());
    if (!prg) continue;
    const TableAssets a = TableAssets::load(*prg, t);
    const auto dir = std::filesystem::temp_directory_path() / ("encore-open-table" + std::to_string(t + 1));
    std::filesystem::remove_all(dir);
    TableAssets b;
    try {
      saveTableAssets(a, dir);
      b = loadTableAssets(dir);
    } catch (const std::exception& e) {
      std::printf("  FAIL table %d: %s\n", t + 1, e.what());
      ++test::failures();
      continue;
    }

    // Each part on its own, so a failure says which part did not come back.
#define SAME(field)                                                                 \
  do {                                                                              \
    if (!(a.field == b.field)) {                                                    \
      std::printf("  FAIL table %d: %s did not come back the same\n", t + 1, #field); \
      ++test::failures();                                                           \
    }                                                                               \
  } while (0)
    SAME(table); SAME(mainBoard); SAME(palette); SAME(spring); SAME(ball); SAME(occmaps); SAME(physmaps);
    SAME(physmapPatches); SAME(ramps); SAME(ballOutline); SAME(ballOutlineByAngle); SAME(lights);
    SAME(attractLights); SAME(dmPalette); SAME(dmFonts); SAME(flippers); SAME(lightBinds); SAME(dmTower);
    SAME(transitionsDown); SAME(transitionsUp); SAME(bumpers); SAME(rollTriggers); SAME(rollTriggersTilt);
    SAME(hitTriggers); SAME(jingleBinds); SAME(sfxBinds); SAME(positionJingleStart); SAME(scripts);
    SAME(msgs); SAME(anims); SAME(animFrames); SAME(scriptBinds); SAME(cheats); SAME(effects);
    SAME(sineTable); SAME(scoreJackpotInit); SAME(scoreJackpotIncr); SAME(scoreModeHitIncr);
    SAME(scoreModeRampIncr); SAME(issueBallPos); SAME(issueBallReleasePos);
#undef SAME
    CHECK(a == b);
    std::printf("  table %d: %zu script steps, %zu messages, %zu flippers\n", t + 1, a.scripts.size(), a.msgs.size(), a.flippers.size());
  }
}

TEST(intro_survives_the_open_format_unchanged) {
  if (!haveData()) {
    std::printf("  (skipped: no supported game files at %s)\n", dataDir().string().c_str());
    return;
  }
  const auto prg = file::readAll(dataDir() / "INTRO.PRG");
  CHECK(prg.has_value());
  if (!prg) return;
  const IntroAssets a = IntroAssets::load(*prg);
  const auto dir = std::filesystem::temp_directory_path() / "encore-open-intro";
  std::filesystem::remove_all(dir);
  IntroAssets b;
  try {
    saveIntroAssets(a, dir);
    b = loadIntroAssets(dir);
  } catch (const std::exception& e) {
    std::printf("  FAIL intro: %s\n", e.what());
    ++test::failures();
    return;
  }
  CHECK(a.slides == b.slides);
  CHECK(a.left == b.left);
  CHECK(a.tables == b.tables);
  CHECK(a.hiscoresLq == b.hiscoresLq && a.hiscoresHq == b.hiscoresHq);
  CHECK(a.fontLq == b.fontLq && a.fontHq == b.fontHq);
  CHECK(a.textPages == b.textPages);
  CHECK(a.leftTextMenu == b.leftTextMenu && a.leftTextOptions == b.leftTextOptions);
  CHECK(a.warpTable == b.warpTable && a.warpFrames == b.warpFrames);
  CHECK(a == b);
  std::printf("  intro: %zu slides, %zu text pages\n", a.slides.size(), a.textPages.size());
}

namespace {

/// The same scripted game on whatever table it is given -- start, plunge every ball, flip when
/// the ball comes down on a flipper -- written down frame by frame: where the ball is, the
/// score, and every trigger the table reports.
std::vector<std::string> playAndRecord(Table& t, int frames) {
  std::vector<std::string> events, log;
  t.trace = &events;
  std::vector<float> audio(1600);
  int plungeAt = -1, prevY = 0;
  bool l = false, r = false;
  for (int f = 0; f < frames; ++f) {
    if (f == 30) t.handleKey(Key::Enter, true), t.handleKey(Key::Enter, false);
    const auto p = t.ballPos();
    if (!t.inAttract() && p[0] >= 290 && p[1] >= 515 && plungeAt < 0) {
      plungeAt = f;
      t.handleKey(Key::ArrowDown, true);
    }
    if (plungeAt >= 0 && f == plungeAt + 40) t.handleKey(Key::ArrowDown, false);
    if (plungeAt >= 0 && f > plungeAt + 200 && p[1] < 500) plungeAt = -1;
    const bool zone = p[1] > prevY && p[1] > 485 && p[1] < 545;
    prevY = p[1];
    const bool wl = zone && p[0] < 150, wr = zone && p[0] >= 130 && p[0] < 290;
    if (wl != l) t.handleKey(Key::ShiftLeft, l = wl);
    if (wr != r) t.handleKey(Key::ShiftRight, r = wr);
    t.runFrame();
    t.player().render(audio.data(), 800);
    const auto score = t.scoreMain().toAscii();
    std::string line = std::to_string(p[0]) + "," + std::to_string(p[1]) + " " + std::string(score.begin(), score.end());
    for (const auto& e : events) line += " | " + e;
    events.clear();
    log.push_back(std::move(line));
  }
  return log;
}

}  // namespace

TEST(converted_tables_play_the_same_game) {
  if (!haveData()) {
    std::printf("  (skipped: no supported game files at %s)\n", dataDir().string().c_str());
    return;
  }
  const auto dir = std::filesystem::temp_directory_path() / "encore-open-game";
  try {
    convertGame(dataDir(), dir);
  } catch (const std::exception& e) {
    std::printf("  FAIL converting: %s\n", e.what());
    ++test::failures();
    return;
  }
  CHECK(OpenGame::isIn(dir));
  const OpenGame game = OpenGame::at(dir);
  constexpr int kFrames = 6000;  // a hundred seconds of play
  for (int t = 0; t < 4; ++t) {
    const std::string n = std::to_string(t + 1);
    const auto prg = file::readAll(dataDir() / ("TABLE" + n + ".PRG"));
    const auto mod = file::readAll(dataDir() / ("TABLE" + n + ".MOD"));
    const auto openMod = file::readAll(game.tableMusic[static_cast<std::size_t>(t)]);
    CHECK(prg && mod && openMod && *mod == *openMod);
    if (!prg || !mod || !openMod) continue;
    Table fromDos(*prg, *mod, Config::defaults(), t, 1234);
    Table fromOpen(loadTableAssets(game.tables[static_cast<std::size_t>(t)]), *openMod, Config::defaults(), t, 1234);
    const auto a = playAndRecord(fromDos, kFrames);
    const auto b = playAndRecord(fromOpen, kFrames);
    std::size_t f = 0;
    while (f < a.size() && a[f] == b[f]) ++f;
    if (f < a.size()) {
      std::printf("  FAIL table %d: the games part at frame %zu\n    dos:  %s\n    open: %s\n", t + 1, f, a[f].c_str(), b[f].c_str());
      ++test::failures();
    } else {
      // The highest score reached, and how many triggers fired, to show the game was played.
      std::string peak;
      std::size_t triggers = 0;
      for (const auto& line : a) {
        const std::string score = line.substr(line.find(' ') + 1, 12);
        if (peak.empty() || score > peak) peak = score;
        for (std::size_t at = line.find(" | "); at != std::string::npos; at = line.find(" | ", at + 3)) ++triggers;
      }
      std::printf("  table %d: %d frames identical; best score %s, %zu triggers\n", t + 1, kFrames, peak.c_str(), triggers);
    }
  }
}
