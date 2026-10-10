// Whole games on the tables, against the real game files (looked for as GameDir.h says, and
// skipped when they are not there).
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>

#include "GameDir.h"
#include "Test.h"
#include "core/File.h"
#include "data/GameVersion.h"
#include "engine/game/Front.h"
#include "engine/game/TableGame.h"

using namespace encore;

namespace {

bool haveData() {
  static const bool ok = test::haveTables();
  if (!ok) std::printf("  (skipped: no supported table files at %s)\n", test::gameDir().string().c_str());
  return ok;
}

/// The menu's files too (INTRO.PRG, MOD2.MOD), for the tests of the menu.
bool haveMenu() {
  static const bool ok = test::haveMenu();
  if (!ok) std::printf("  (skipped: no supported menu files at %s)\n", test::gameDir().string().c_str());
  return ok;
}

Bytes read(const std::string& name) {
  const auto path = file::findCaseInsensitive(test::gameDir(), name);
  return path ? file::readAll(*path).value_or(Bytes{}) : Bytes{};
}

/// One game from the key that starts it, played by keys at random, the same for a seed: the
/// table is made, the game started at once (after `typedFirst`, typed a letter at a time, a
/// space for each *), and it is played to its end or for `frames`.
Recording play(int table, int frames, unsigned seed, const HighScores& best = Config::defaults().highScores[0],
               const std::function<void(TableGame&)>& each = {}, std::string_view typedFirst = {}) {
  TableGame::Setup setup;
  setup.options.balls = 3;
  setup.highScores = best;
  setup.seed = static_cast<u64>(seed);
  setup.picture = false;
  const std::string n = std::to_string(table + 1);
  TableGame game(read("TABLE" + n + ".PRG"), read("TABLE" + n + ".MOD"), table, setup);
  unsigned rng = seed * 2654435761u + 1;
  auto random = [&] { rng = rng * 1664525u + 1013904223u; return rng >> 16; };
  bool left = false, right = false, pulled = false;
  for (const char c : typedFirst) {
    const Key k = c == '*' ? Key::Space : static_cast<Key>(static_cast<int>(Key::A) + (c - 'A'));
    game.key(k, true), game.frame(), game.key(k, false), game.frame();
  }
  game.key(Key::Enter, true), game.key(Key::Enter, false);
  for (int f = 0; f < frames && game.recording().games.empty() && !game.left(); ++f) {
    if (each) each(game);
    if (!game.askingName() && !game.askingOnline()) {
      if (random() % 23 == 0) game.key(Key::ShiftLeft, left = !left);
      if (random() % 23 == 0) game.key(Key::ShiftRight, right = !right);
      if (!pulled && f % 300 == 100) game.key(Key::ArrowDown, pulled = true);
      else if (pulled && random() % 40 == 0) game.key(Key::ArrowDown, pulled = false);
    }
    game.frame();
    game.noSound();
  }
  return game.recording();
}

}  // namespace

TEST(every_table_plays_a_game_to_its_end) {
  if (!haveData()) return;
  for (int table = 0; table < 4; ++table) {
    const Recording r = play(table, 60000, 3);
    CHECK(r.games.size() == 1);
    if (r.games.empty()) continue;
    CHECK(!r.games[0].abandoned);
    CHECK(!r.games[0].scores.empty() && !r.games[0].scores[0].isZero());
  }
}

TEST(the_same_keys_give_the_same_game) {
  if (!haveData()) return;
  const Recording a = play(1, 60000, 5), b = play(1, 60000, 5);
  CHECK(a.games == b.games);
  CHECK(a.events == b.events);
  CHECK(a.frames == b.frames);
}

TEST(recorded_games_play_again_exactly) {
  if (!haveData()) return;
  for (int table = 0; table < 4; ++table) {
    const Recording played = play(table, 60000, 7);
    const auto loaded = Recording::load(played.save());
    CHECK(loaded.has_value());
    if (!loaded) continue;
    const std::string n = std::to_string(table + 1);
    const Recording again = replay(read("TABLE" + n + ".PRG"), read("TABLE" + n + ".MOD"), *loaded);
    CHECK(again.games == played.games);
    CHECK(again.events == played.events);
  }
}

// Games played and kept by earlier versions (tests/recordings), among them players' games from
// the online board: each still plays again to the game it was, score, last frame and every
// event. The score checking is built from the newest code, and checks games sent by released
// versions: a change that plays any of these differently is one it must not have.
TEST(kept_recordings_play_again_exactly) {
  if (!haveData()) return;
  const auto dir = std::filesystem::path(ENCORE_SOURCE_DIR) / "tests" / "recordings";
  int played = 0;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.path().extension() != ".RPL") continue;
    const auto data = file::readAll(entry.path());
    const auto kept = data ? Recording::load(*data) : std::nullopt;
    CHECK(kept.has_value());
    if (!kept) continue;
    const std::string n = std::to_string(kept->table + 1);
    const Recording again = replay(read("TABLE" + n + ".PRG"), read("TABLE" + n + ".MOD"), *kept);
    std::printf("  %s\n", entry.path().filename().string().c_str());
    CHECK(again.games == kept->games);
    CHECK(again.events == kept->events);
    CHECK(again.frames == kept->frames);
    ++played;
  }
  CHECK(played == 16);
}

// A best score asks for initials and then whether to send the game online; both answers are
// keys, so the recording has them and plays them again, initials and all.
TEST(initials_and_the_online_question_are_recorded) {
  if (!haveData()) return;
  bool typed = false, asked = false;
  int letters = 0, wait = 0;
  const Recording played = play(1, 60000, 5, HighScores{}, [&](TableGame& t) {
    if (t.askingName() && letters < 3 && ++wait % 10 == 0) {  // (a letter at a time, as fingers do)
      static constexpr Key kName[3] = {Key::R, Key::D, Key::X};
      t.key(kName[letters], true), t.key(kName[letters], false);
      typed = ++letters == 3;
    }
    if (t.askingOnline() && !asked) {
      t.key(Key::Y, true), t.key(Key::Y, false);
      asked = true;
    }
  });
  CHECK(typed);
  CHECK(asked);
  CHECK(played.games.size() == 1);
  if (played.games.empty()) return;
  CHECK((played.games[0].initials == std::array<u8, 3>{'R', 'D', 'X'}));
  const Recording again = replay(read("TABLE2.PRG"), read("TABLE2.MOD"), *Recording::load(played.save()));
  CHECK(again.games == played.games);
}

// A game played with a cheat that makes it easier is not counted: no tilt, the other pace, or
// more balls than the options give.
TEST(cheated_games_are_not_counted) {
  if (!haveData()) return;
  const Bytes prg = read("TABLE2.PRG"), mod = read("TABLE2.MOD");
  const Recording honest = play(1, 60000, 5);
  CHECK(honest.games.size() == 1);
  CHECK(verify(prg, mod, honest).ok);
  Recording pace = honest, earthquake = honest, extra = honest;
  pace.carry.otherSteps = true;
  earthquake.carry.noTilt = true;
  extra.carry.balls = 5;
  for (const Recording* r : {&pace, &earthquake, &extra}) {
    const Verdict v = verify(prg, mod, *r);
    CHECK(!v.ok);
    CHECK(v.reason == "played with cheats");
  }
}

// The same cheats typed into the recording itself, before the key that starts the game, as a
// recording made by hand could have them: the header says nothing of them, but the table has
// them as the game starts.
TEST(cheats_typed_before_the_start_are_not_counted) {
  if (!haveData()) return;
  const Bytes prg = read("TABLE2.PRG"), mod = read("TABLE2.MOD");
  for (const std::string_view word : {"EARTHQUAKE", "SNAIL", "EXTRA*BALLS"}) {
    const Recording typed = play(1, 60000, 5, Config::defaults().highScores[0], {}, word);
    CHECK(typed.games.size() == 1);
    CHECK(!typed.cheated());
    const Verdict v = verify(prg, mod, typed);
    CHECK(!v.ok);
    CHECK(v.reason == "played with cheats");
  }
  // (a word typed and taken back is no cheat)
  CHECK(verify(prg, mod, play(1, 60000, 5, Config::defaults().highScores[0], {}, "EARTHQUAKEFAIR*PLAY")).ok);
}

// The angle can be changed while paused; a game counts as played at the gentlest it had.
TEST(a_game_counts_at_the_gentlest_angle_it_had) {
  if (!haveData()) return;
  const Bytes prg = read("TABLE2.PRG"), mod = read("TABLE2.MOD");
  // keys pressed a few frames apart, from the frame given
  auto changes = [](int from, std::vector<Key> keys) {
    return [from, keys, f = 0](TableGame& t) mutable {
      const int at = f++ - from;
      if (at >= 0 && at % 10 == 0 && static_cast<std::size_t>(at / 10) < keys.size()) {
        const Key k = keys[static_cast<std::size_t>(at / 10)];
        t.key(k, true), t.key(k, false);
      }
    };
  };
  const Recording honest = play(1, 60000, 5);
  CHECK(honest.games.size() == 1);
  if (honest.games.empty()) return;
  CHECK_EQ(verify(prg, mod, honest).angle, 1);  // high, as the options have it
  // (a third of the way into the game)
  const int during = static_cast<int>(honest.games[0].endFrame / 3);
  // high, paused for higher and then low, and back to high
  const Recording low = play(1, 60000, 5, Config::defaults().highScores[0],
                             changes(during, {Key::P, Key::A, Key::A, Key::P, Key::P, Key::A, Key::P}));
  CHECK(low.games.size() == 1);
  CHECK(low.options.angle == Angle::High);
  const Verdict v = verify(prg, mod, low);
  CHECK(v.ok);
  CHECK_EQ(v.angle, 0);
  // high, and then higher: high is the gentlest
  const Recording higher = play(1, 60000, 5, Config::defaults().highScores[0], changes(during, {Key::P, Key::A, Key::P}));
  CHECK_EQ(verify(prg, mod, higher).angle, 1);
}

// A flipper's replacement picture turns about the point its own artwork hinges on, which is
// not always where the table has the ball bounce off it: the upper bats of Party Land and
// Speed Devils are the lower bats' pictures, hinged several dots from there.
TEST(flipper_pictures_turn_about_the_artwork_hinge) {
  if (!haveData()) return;
  struct Expected { int table, flipper; float x, y; };
  // Fractions of the flipper's rectangle, measured on the tables' own flipper pictures.
  for (const Expected& e : {Expected{0, 0, 15.14f / 64, 26.17f / 53}, Expected{0, 2, 6.23f / 48, 7.53f / 51},
                            Expected{1, 1, 43.86f / 64, 26.17f / 53}, Expected{1, 2, 43.60f / 64, 26.00f / 53},
                            Expected{3, 0, 15.14f / 64, 26.17f / 53}}) {
    const std::string n = std::to_string(e.table + 1);
    TableGame game(read("TABLE" + n + ".PRG"), read("TABLE" + n + ".MOD"), e.table, {});
    std::vector<u8> pixels(320 * (576 + 33));
    std::vector<Rgb> colours(256);
    HdFrame hd;
    game.draw(pixels.data(), colours.data(), &hd);
    CHECK(hd.sprites.size() > static_cast<std::size_t>(e.flipper));
    if (hd.sprites.size() <= static_cast<std::size_t>(e.flipper)) continue;
    const HdSprite& s = hd.sprites[static_cast<std::size_t>(e.flipper)];
    CHECK(std::abs(s.pivotSpriteX - e.x) < 0.02f);
    CHECK(std::abs(s.pivotSpriteY - e.y) < 0.02f);
  }
}

TEST(the_menu_starts_a_table) {
  if (!haveMenu()) return;
  Front front(read("INTRO.PRG"), read("MOD2.MOD"), Config::defaults(), 0);
  bool opened = false;
  for (int f = 0; f < 1200 && !opened; ++f) {
    if (f == 600) front.key(Key::F3, true);
    const Front::Action a = front.frame();
    front.noSound();
    opened = a.kind == Front::Action::Kind::OpenTable && a.table == 2;
  }
  CHECK(opened);
}

// With the whole table on one screen chosen, the menu is as tall as that screen and has all
// four tables' banners on its first page: there is a picture in each quarter of it.
TEST(the_tall_menu_shows_four_tables) {
  if (!haveMenu()) return;
  Config config = Config::defaults();
  config.options.resolution = Resolution::Full;
  Front front(read("INTRO.PRG"), read("MOD2.MOD"), config, 0);
  CHECK(front.height() == 960);
  for (int f = 0; f < 250; ++f) front.frame(), front.noSound();
  std::vector<u8> pixels(static_cast<std::size_t>(Front::kWidth) * 960);
  std::vector<Rgb> colours(256);
  front.draw(pixels.data(), colours.data());
  for (int quarter = 0; quarter < 4; ++quarter) {
    int banner = 0;
    for (int y = quarter * 240 + 60; y < quarter * 240 + 200; ++y)
      for (int x = 200; x < 560; ++x) banner += pixels[static_cast<std::size_t>(y) * Front::kWidth + static_cast<std::size_t>(x)] >= 0x40 + quarter * 16;
    CHECK(banner > 20000);
  }
}

// The tall screen shows the whole table, with the dot matrix under it, and never scrolls; it is
// kept in PINBALL.CFG, and left out of the recordings, which every viewer sees in their own size.
TEST(the_tall_screen_shows_the_whole_table) {
  if (!haveData()) return;
  TableGame::Setup setup;
  setup.options.resolution = Resolution::Tall;
  TableGame game(read("TABLE1.PRG"), read("TABLE1.MOD"), 0, setup);
  CHECK(game.screenHeight() == 576 + 33);
  for (int f = 0; f < 600; ++f) {  // the ball served and played up the table, unshaken
    game.frame();
    CHECK(game.viewTop() == 0);
  }
  const auto again = Recording::load(game.recording().save());
  CHECK(again && again->options.resolution == Resolution::Normal);
  const auto dir = std::filesystem::temp_directory_path() / "encore-tall-test";
  std::filesystem::create_directories(dir);
  Config::saveOptions(dir, setup.options);
  CHECK(Config::load(dir).options.resolution == Resolution::Tall);
  std::filesystem::remove_all(dir);
}

// Leaving a table from the pause is not a cut: its picture fades away over 128 frames, as
// the original's does, and only then is the table left.
TEST(a_table_left_fades_out) {
  if (!haveData()) return;
  TableGame game(read("TABLE1.PRG"), read("TABLE1.MOD"), 0, {});
  std::vector<u8> pixels(320 * 350);
  std::vector<Rgb> colours(256);
  auto brightness = [&] {
    game.draw(pixels.data(), colours.data());
    long sum = 0;
    for (const Rgb& c : colours) sum += c.r + c.g + c.b;
    return sum;
  };
  auto tap = [&](Key k) { game.key(k, true), game.key(k, false); };
  auto run = [&](int frames) {
    for (int f = 0; f < frames; ++f) game.frame(), game.noSound();
  };
  tap(Key::Enter);
  run(200);
  const long full = brightness();
  tap(Key::P);
  run(5);
  tap(Key::Escape);
  run(5);
  tap(Key::Y);
  run(5);
  CHECK(!game.left());
  run(60);
  const long half = brightness();
  CHECK(!game.left());
  CHECK(half < full * 3 / 4 && half > full / 8);
  run(70);
  CHECK(game.left());
}
