#include "engine/game/TableGame.h"

#include <algorithm>

#include "core/Log.h"
#include "engine/table/Gameshow.h"
#include "engine/table/PartyLand.h"
#include "engine/table/SpeedDevils.h"
#include "engine/table/StonesNBones.h"

namespace encore {
namespace {

std::unique_ptr<Engine> make(ByteView prg, int table) {
  switch (table) {
    case 0: return std::make_unique<PartyLand>(prg);
    case 1: return std::make_unique<SpeedDevils>(prg);
    case 2: return std::make_unique<Gameshow>(prg);
    default: return std::make_unique<StonesNBones>(prg);
  }
}

/// A key as the keyboard of the original's day says it: a code, after 0xe0 for the keys that
/// came later. Nought for a key the table has no use for.
struct Scancode {
  u8 code = 0;
  bool extended = false;
};

Scancode scancode(Key k) {
  // the letters, A to Z
  static constexpr u8 kLetters[26] = {0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
                                      0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c};
  if (k >= Key::A && k <= Key::Z) return {kLetters[static_cast<int>(k) - static_cast<int>(Key::A)]};
  if (k >= Key::F1 && k <= Key::F8) return {static_cast<u8>(0x3b + static_cast<int>(k) - static_cast<int>(Key::F1))};
  // (this version takes 1 to 8 for F1 to F8: a game for that many players)
  if (k >= Key::Digit1 && k <= Key::Digit8) return {static_cast<u8>(0x3b + static_cast<int>(k) - static_cast<int>(Key::Digit1))};
  switch (k) {
    case Key::ShiftLeft: return {0x2a};
    case Key::ShiftRight: return {0x36};
    case Key::ControlLeft: return {0x1d};
    case Key::ControlRight: return {0x1d, true};
    case Key::AltLeft: return {0x38};
    case Key::AltRight: return {0x38, true};
    case Key::Space: return {0x39};
    case Key::Enter: return {0x1c};
    case Key::Escape: return {0x01};
    case Key::ArrowDown: return {0x50, true};
    case Key::ArrowUp: return {0x48, true};
    case Key::ArrowLeft: return {0x4b, true};
    case Key::ArrowRight: return {0x4d, true};
    default: return {};
  }
}

/// The best scores as the table keeps them, and as TABLEn.HI does: four of twelve digits,
/// three letters and a byte of nothing.
std::array<u8, 0x40> packed(const HighScores& scores) {
  std::array<u8, 0x40> out{};
  for (std::size_t i = 0; i < 4; ++i) {
    std::copy(scores[i].score.digits.begin(), scores[i].score.digits.end(), out.begin() + i * 0x10);
    std::copy(scores[i].name.begin(), scores[i].name.end(), out.begin() + i * 0x10 + 12);
  }
  return out;
}

bool same(const Options& a, const Options& b) {
  return a.balls == b.balls && a.angle == b.angle && a.scrollSpeed == b.scrollSpeed && a.resolution == b.resolution &&
         a.noMusic == b.noMusic && a.mono == b.mono;
}

bool same(const HighScores& a, const HighScores& b) {
  for (std::size_t i = 0; i < 4; ++i)
    if (a[i].score != b[i].score || a[i].name != b[i].name) return false;
  return true;
}

}  // namespace

TableGame::TableGame(ByteView prg, ByteView module, int table, const Setup& setup)
    : options_(setup.options), music_(48000) {
  engine_ = make(prg, table);
  if (setup.picture) {
    screen_ = std::make_unique<TableScreen>(Bytes(prg.begin(), prg.end()), table);
    screen_->keepTopRow = true;
    screen_->artBehindPlunger = true;
  }
  if (!music_.load(module)) throw DataError("the table's music is not a module");
  music_.holdOnStop = true;
  link_ = std::make_unique<TableMusic>(*engine_, music_);
  music_.conductor = link_.get();
  engine_->sound = link_.get();
  engine_->pollCallsMusic = false;
  engine_->refuseFewerPlayers = true;
  engine_->servesAlike = true;
  engine_->keysFirst = true;
  engine_->stepsTogether = true;
  engine_->wholeGains = true;
  engine_->amended = true;
  engine_->seedChance(setup.seed);
  if (screen_) screen_->attach(*engine_);

  Engine::Options o;
  o.fiveBalls = options_.balls == 5;
  o.lowAngle = options_.angle == Angle::Low;
  o.scrolling = static_cast<u8>(options_.scrollSpeed);
  o.musicOff = options_.noMusic;
  // The original has two sets of speeds: one for its screen of 350 rows, which the card shows
  // 70 times a second, and one for its screen of 240, shown 60 times. This version shows every
  // screen 60 times a second, so the table is always started for that one, whatever size of
  // picture is asked for: the size is then only a matter of what is drawn.
  o.highResolution = false;
  o.mono = options_.mono;
  const auto best = packed(setup.highScores);
  engine_->start(o, best);
  if (options_.angle == Angle::Higher) engine_->setAngle(2);
  // what the table this one's game was started on had that the game would not be the same without
  const Recording::Carry& carry = setup.carry;
  if (carry.balls != 0) engine_->B(0x33dd) = carry.balls;
  if (carry.noTilt) engine_->B(0x372a) = 0xff;
  if (carry.otherSteps) engine_->B(at::keys) ^= 4;
  if (carry.scrollPos != 0xffff) {
    engine_->W(0x3383) = carry.scrollPos;
    engine_->W(0x2f04) = carry.scrollAt;
  }
  if (screen_) screen_->started(*engine_);
  // (where the screen was on the table the game was started from, the fraction of a row too)
  camera_.pos = static_cast<u16>(carry.scrollPos != 0xffff ? carry.scrollPos : std::max(cameraTop(), 0));
  camera_.raw = static_cast<i16>(carry.scrollPos != 0xffff ? carry.scrollAt : camera_.pos << 4);

  recording_.table = table;
  recording_.seed = setup.seed;
  recording_.options = options_;
  recording_.highScores = setup.highScores;
  recording_.carry = carry;
  recording_.carry.balls = engine_->B(0x33dd);
  bestAtStart_ = savedScores_ = highScores();
  saved_ = options();
  link_->give();
}

TableGame::~TableGame() { music_.conductor = nullptr; }

Bcd TableGame::score(int player) const {
  Bcd s;
  const u16 at = static_cast<u16>(engine_->A(0x021a) + player * engine_->kw(0x05d4, 1));
  for (u16 i = 0; i < 12; ++i) s.digits[i] = engine_->nativeB(static_cast<u16>(at + i));
  return s;
}

HighScores TableGame::highScores() const {
  HighScores h{};
  const u16 at = engine_->kw(0x6377, 1);
  for (u16 i = 0; i < 4; ++i) {
    for (u16 d = 0; d < 12; ++d) h[i].score.digits[d] = engine_->nativeB(static_cast<u16>(at + i * 0x10 + d));
    for (u16 c = 0; c < 3; ++c) h[i].name[c] = engine_->nativeB(static_cast<u16>(at + i * 0x10 + 12 + c));
  }
  return h;
}

Options TableGame::options() const {
  Options o = options_;
  o.angle = engine_->angle() == 0 ? Angle::Low : engine_->angle() == 2 ? Angle::Higher : Angle::High;
  o.noMusic = engine_->musicIsOff();
  return o;
}

bool TableGame::optionsChanged() { return std::exchange(optionsChanged_, false); }
bool TableGame::highScoresChanged() { return std::exchange(scoresChanged_, false); }

Recording::Carry TableGame::carryOver() const {
  Recording::Carry c;
  c.noTilt = engine_->B(0x372a) == 0xff;
  c.otherSteps = (engine_->B(at::keys) & 4) != 0;
  c.balls = engine_->B(0x33dd);
  c.scrollPos = camera_.pos;
  c.scrollAt = static_cast<u16>(camera_.raw);
  return c;
}

bool TableGame::startsGame(Key key) const {
  if (!waiting() || left() || asking_) return false;
  if (key != Key::Enter && !(key >= Key::F1 && key <= Key::F8) && !(key >= Key::Digit1 && key <= Key::Digit8)) return false;
  // the table takes keys, more players may join, and it is not asking whether to be left
  return engine_->CB(0x3475) != 0 && engine_->B(0x33e3) != 0 && engine_->CB(0x3195) != 0xff;
}

bool TableGame::askingName() const { return engine_->W(0x33e7) == engine_->F(0x0609); }

void TableGame::key(Key key, bool down) {
  if (left()) return;
  if (recording_.games.empty())  // (a recording is of one game)
    recording_.events.push_back({frames_, down ? Recording::Event::Kind::KeyDown : Recording::Event::Kind::KeyUp, static_cast<u32>(key)});
  if (key == Key::ArrowUp) up_ = down;
  if (key == Key::ArrowDown) down_ = down;
  if (asking_) {  // this version's question, which the table knows nothing of
    if (!down) return;
    if (key == Key::Y) sendOnline_ = true;
    if (key == Key::Y || key == Key::N) {
      asking_ = false;
      answered_ = true;
      engine_->write("");
    }
    return;
  }
  // The original's pause ends with any key but escape. Here it ends with P; escape asks
  // whether to leave, as there; and other keys change the options, or do nothing.
  if (down && engine_->isPaused() && !engine_->asksToQuit() && key != Key::P && key != Key::Escape) return pausedKey(key);
  const Scancode s = scancode(key);
  if (s.code == 0) return;
  if (s.extended) engine_->key(0xe0);
  engine_->key(static_cast<u8>(s.code | (down ? 0 : 0x80)));
  link_->give();
}

/// The music as the table sees it this frame: the sound card's, or a recording's when one is
/// played back; noted in this table's own recording whenever it has moved on by itself.
void TableGame::syncMusic() {
  const u32 before = link_->view();
  if (playback_) {
    const auto& events = playback_->events;
    for (; playbackAt_ < events.size() && events[playbackAt_].frame <= frames_; ++playbackAt_)
      if (events[playbackAt_].frame == frames_ && !events[playbackAt_].isKey()) link_->setView(events[playbackAt_].value);
  } else {
    link_->take();
  }
  if (link_->view() != before && recording_.games.empty())
    recording_.events.push_back({frames_, Recording::Event::Kind::Music, link_->view()});
}

void TableGame::pausedKey(Key key) {
  switch (key) {
    case Key::A:
      engine_->setAngle(engine_->angle() == 1 ? 2 : engine_->angle() == 2 ? 0 : 1);
      engine_->write(engine_->angle() == 0 ? "ANGLE LOW" : engine_->angle() == 2 ? "ANGLE HIGHER" : "ANGLE HIGH");
      break;
    case Key::S:
      options_.scrollSpeed = static_cast<ScrollSpeed>((static_cast<int>(options_.scrollSpeed) + 1) % 3);
      engine_->setScrolling(static_cast<u8>(options_.scrollSpeed));
      engine_->write(options_.scrollSpeed == ScrollSpeed::Hard ? "SCROLLING HARD" : options_.scrollSpeed == ScrollSpeed::Soft ? "SCROLLING SOFT" : "SCROLLING MEDIUM");
      break;
    case Key::M:
      engine_->toggleMusic();
      engine_->write(engine_->musicIsOff() ? "MUSIC OFF" : "MUSIC ON");
      break;
    case Key::R:
      // (a recording played back keeps to the viewer's size of screen, whatever its player chose)
      if (!playback_) {
        options_.resolution = static_cast<Resolution>((static_cast<int>(options_.resolution) + 1) % 4);
        // the new size of screen looks where it is told to, or where the ball is
        camera_.pos = static_cast<u16>(std::clamp(camera_.said ? *camera_.said : engine_->W(at::ballY).s() - cameraLead(), 0, cameraTop()));
        camera_.raw = static_cast<i16>(camera_.pos << 4);
      }
      engine_->write("RESOLUTION CHANGED");
      break;
    case Key::F7:
      lamps_ = (lamps_ + 1) % 3;
      return;
    default: return;
  }
  pauseFrames_ = 0;
  link_->give();
}

int TableGame::screenHeight() const {
  return wholeTable() ? TableData::kHeight + TableScreen::kDisplayRows
                      : TableScreen::height(options_.resolution == Resolution::High);
}

int TableGame::viewRows() const { return screenHeight() - TableScreen::kDisplayRows; }

int TableGame::viewTop() const {
  // (the whole table on the screen still jumps when the table is shaken)
  const int lift = engine_->W(at::nudgeLift).s();
  if (wholeTable()) return lift;
  return std::min<int>(camera_.pos, cameraTop()) + lift;
}

int TableGame::cameraLead() const { return options_.resolution == Resolution::Normal ? 75 : options_.resolution == Resolution::High ? 130 : 0; }

TableGame::Sight TableGame::look() const {
  Engine& e = *engine_;
  return {waiting(), e.ballParked() ? 525 : e.W(at::ballY).s(), e.W(0x3385), e.W(0x2f02)};
}

/// Where the screen looks this frame: after the ball, drifting while nobody plays, or moved
/// by hand while paused. `s` is the table as the frame began.
void TableGame::aim(const Sight& s) {
  const u16 said = engine_->W(0x3383);
  if (engine_->isPaused()) {
    if (!engine_->asksToQuit() && cameraTop() > 0 && up_ != down_) {
      const int p = std::clamp(static_cast<int>(camera_.pos) + (down_ ? 4 : 0) - (up_ ? 4 : 0), 0, cameraTop());
      camera_.pos = static_cast<u16>(p);
      camera_.raw = static_cast<i16>(p << 4);
    }
    return;
  }
  if (s.waiting) {
    if (cameraTop() <= 0) camera_.pos = 0;
    else {
      if (camera_.pos == 0) camera_.up = false;
      else if (camera_.pos >= cameraTop()) camera_.up = true;
      camera_.pos = static_cast<u16>(camera_.up ? camera_.pos - 1 : camera_.pos + 1);
      camera_.raw = static_cast<i16>(camera_.pos << 4);
    }
    camera_.said.reset();
    saidBefore_ = said;
    return;
  }
  follow(s);
  // Party Land takes the screen up from where it is after its holes (ds:3383, counted from
  // where the table's own screen was): this one goes up by as much from where it is, at once.
  const u16 before = std::exchange(saidBefore_, said);
  if (said == 0xffff) {
    camera_.said.reset();
    return;
  }
  if (before == 0xffff || !camera_.said) camera_.said = camera_.pos + static_cast<i16>(said) - static_cast<i16>(s.shown);
  else camera_.said = *camera_.said + static_cast<i16>(said) - static_cast<i16>(before);
  camera_.pos = static_cast<u16>(std::clamp(*camera_.said, 0, std::max(cameraTop(), 0)));
  camera_.raw = static_cast<i16>(camera_.pos << 4);
}

/// The screen follows the ball, or the row the table says to follow instead (ds:3385): so
/// far each frame towards it as the scrolling option has it, and never so far behind that the
/// ball is off the screen.
void TableGame::follow(const Sight& s) {
  const int top = cameraTop();
  if (top <= 0) {
    camera_.pos = 0;
    return;
  }
  const int lead = cameraLead();
  int target = std::clamp(s.ballY - lead, 0, top);
  if (camera_.said) target = std::clamp(*camera_.said, 0, top);
  else if (s.follow != 0xffff) {
    // the Gameshow's wheel, or the bottom of the table when a ball is lost
    const int row = static_cast<i16>(s.follow);
    target = row == 0x10e ? (options_.resolution == Resolution::High ? 220 : 270) : row >= 0x171 ? top : std::clamp(row, 0, top);
  }
  const i16 speed = options_.scrollSpeed == ScrollSpeed::Hard ? 20 : options_.scrollSpeed == ScrollSpeed::Medium ? 11 : 9;
  i16 delta = static_cast<i16>(target - (camera_.raw >> 4));
  camera_.raw = static_cast<i16>(camera_.raw + ((delta * speed) >> 2));
  delta = static_cast<i16>(target - (camera_.raw >> 4));
  if (delta <= -lead) camera_.raw = static_cast<i16>(camera_.raw + ((delta + lead) << 4));
  else if (delta >= lead + 40) camera_.raw = static_cast<i16>(camera_.raw + ((delta - lead - 40) << 4));
  camera_.pos = static_cast<u16>(std::max(camera_.raw >> 4, 0));
}

void TableGame::frame() {
  if (left()) return;
  syncMusic();
  ++frames_;
  if (recording_.games.empty()) recording_.frames = frames_;
  if (engine_->exited()) {
    // cs:3a11: the table is left: its picture and its music fade away, in 128 frames
    leaving_ -= 2;
    if (leaving_ >= 0 && (leaving_ & 0x0f) == 0) music_.volume(static_cast<u16>(leaving_));
    return;
  }
  if (asking_) return;  // the table waits for the answer
  if (std::exchange(answered_, false)) return;  // and takes it in this frame; it goes on from the next
  // where the screen looks is worked out from the table as the frame begins (the ball where
  // it was), once the frame's keys have been taken: a pause pressed now holds it already
  const Sight sight = look();
  // what the display shows as the frame begins (TableScreen draws it from the first 33 rows of
  // the video card's memory, 0x54 bytes of each plane a row)
  constexpr std::size_t kDisplayBytes = std::size_t{TableScreen::kDisplayRows} * 0x54;
  for (std::size_t plane = 0; plane < 4; ++plane)
    std::copy_n(engine_->videoMemory()[plane].begin(), kDisplayBytes, display_[plane].begin());
  try {
    engine_->frame();
  } catch (const std::exception& e) {
    // A routine of the original's that was not written, or one led astray: the table is left
    // rather than played on from nobody knows what.
    failure_ = e.what();
    log::error("table " + std::to_string(table() + 1) + ", frame " + std::to_string(frames_) + ": " + failure_);
  }
  if (engine_->exited() && engine_->CB(0x3732) == 0xff) {
    // Left from the waiting table's question: the display's script goes straight on to its
    // next step, whose first frame draws a part of it over the question, and the table stops
    // there. The question is what is shown while the table fades away.
    for (std::size_t plane = 0; plane < 4; ++plane)
      std::copy_n(display_[plane].begin(), kDisplayBytes, engine_->videoMemory()[plane].begin());
  }
  link_->give();
  // the picture shows the ball and the flippers where the frame leaves them
  if (!engine_->exited()) engine_->showAsNow();
  if (screen_) screen_->follow(*engine_);
  aim(sight);
  if (engine_->isPaused()) {
    if (!engine_->asksToQuit()) {
      // what the display says while paused, in turn
      if (!wasPaused_) pauseFrames_ = 0;
      ++pauseFrames_;
      if (pauseFrames_ == 120) engine_->write("P TO UNPAUSE");
      else if (pauseFrames_ == 240) engine_->write("ASMR FOR OPTIONS");
      else if (pauseFrames_ == 360) {
        engine_->write("GAME PAUSED");
        pauseFrames_ = 0;
      }
    }
    wasPaused_ = true;
  } else {
    wasPaused_ = false;
  }

  // In a game of one player, the initials for a best score are in and have been shown (the
  // display's second with them, cs:06c0, is over): this version then asks whether the game is
  // to be sent online, and the table waits for the answer.
  const u16 wait = engine_->W(0x33e7);
  if (wait != lastWait_ && lastWait_ == engine_->F(0x06c0) && players() == 1 && failure_.empty()) {
    asking_ = true;
    engine_->write("SEND ONLINE \\Y OR N]");
  }
  lastWait_ = wait;

  const bool now = !waiting() && failure_.empty() && !engine_->exited();
  // How the recorded game is played (the first: a recording is of one game). The cheats are
  // typed only while no game is played, so they are as the game starts: no tilt, the other
  // pace, or more balls than the options give.
  if (now && recording_.games.empty()) {
    if (!playing_)
      cheats_ = engine_->B(0x372a) == 0xff || (engine_->B(at::keys) & 4) != 0 ||
                engine_->B(0x33dd) > options_.balls;
    gentlest_ = gentlest_ < 0 ? engine_->angle() : std::min(gentlest_, engine_->angle());
  }
  // (a table left in the middle of a game leaves no game to keep)
  if (playing_ && !now && !engine_->exited() && recording_.games.empty()) {
    Recording::Game g;
    g.endFrame = frames_;
    // (the last ball's number is one past the balls there are, once it has been played)
    g.abandoned = !failure_.empty() || engine_->B(0x33dc) <= engine_->B(0x33dd);
    for (int p = 0; p < players(); ++p) g.scores.push_back(score(p));
    const HighScores best = highScores();
    if (!g.scores.empty())
      for (std::size_t entry = 0; entry < 4 && g.initials[0] == 0; ++entry) {
        if (best[entry].score != g.scores[0]) continue;
        bool before = false;
        for (const HighScore& old : bestAtStart_) before |= old.score == best[entry].score && old.name == best[entry].name;
        if (!before) g.initials = best[entry].name;
        // (the table keeps a space typed as its own mark for one)
        for (u8& c : g.initials) c = c == '*' ? ' ' : c;
      }
    recording_.games.push_back(std::move(g));
    bestAtStart_ = best;
  }
  playing_ = now;

  if (const Options o = options(); !same(o, saved_)) {
    saved_ = o;
    optionsChanged_ = true;
  }
  if (const HighScores h = highScores(); !same(h, savedScores_)) {
    savedScores_ = h;
    scoresChanged_ = true;
  }
}

void TableGame::draw(u8* frame, Rgb* colours, HdFrame* hd) const {
  if (!screen_) return;
  if (hd && options_.mono) {  // the pictures drawn again are in colour: not for a screen in greys
    hd->reset(TableScreen::kWidth, screenHeight());
    hd = nullptr;
  }
  TableScreen::View view;
  view.height = screenHeight();
  view.top = viewTop();
  view.lamps = lamps_;
  view.ballTrail = ballTrail;
  screen_->draw(*engine_, frame, view, hd);
  screen_->colours(*engine_, colours, lamps_);
  if (engine_->exited()) {  // cs:4f3c: every colour by how bright the table still is
    const int level = std::max(leaving_, 0);
    for (std::size_t i = 0; i < 256; ++i)
      colours[i] = Rgb{static_cast<u8>(colours[i].r * level >> 8), static_cast<u8>(colours[i].g * level >> 8), static_cast<u8>(colours[i].b * level >> 8)};
    if (hd) {
      hd->fade.fill(static_cast<float>(level) / 256.0f);
      hd->spriteTint = static_cast<float>(level) / 256.0f;
    }
  }
}

void TableGame::sound(float* out, int frames) {
  music_.render(out, frames);
  if (silent) std::fill_n(out, static_cast<std::size_t>(frames) * 2, 0.0f);
}

}  // namespace encore
