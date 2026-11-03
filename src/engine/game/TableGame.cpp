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

/// The codes of the keys the table reads (int 9), as a key goes down; the same with the top
/// bit set as it comes up. The keys that came later have the code of an older one after 0xe0.
namespace scan {
constexpr u8 escape = 0x01;
constexpr u8 enter = 0x1c;
constexpr u8 control = 0x1d;
constexpr u8 shiftLeft = 0x2a;
constexpr u8 shiftRight = 0x36;
constexpr u8 alt = 0x38;
constexpr u8 space = 0x39;
constexpr u8 f1 = 0x3b;  ///< F1 to F8 one after another
constexpr u8 up = 0x48;
constexpr u8 left = 0x4b;
constexpr u8 right = 0x4d;
constexpr u8 down = 0x50;
constexpr int released = 0x80;
constexpr u8 extended = 0xe0;
}  // namespace scan

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
  if (k >= Key::F1 && k <= Key::F8) return {static_cast<u8>(scan::f1 + static_cast<int>(k) - static_cast<int>(Key::F1))};
  // (this version takes 1 to 8 for F1 to F8: a game for that many players)
  if (k >= Key::Digit1 && k <= Key::Digit8) return {static_cast<u8>(scan::f1 + static_cast<int>(k) - static_cast<int>(Key::Digit1))};
  switch (k) {
    case Key::ShiftLeft: return {scan::shiftLeft};
    case Key::ShiftRight: return {scan::shiftRight};
    case Key::ControlLeft: return {scan::control};
    case Key::ControlRight: return {scan::control, true};
    case Key::AltLeft: return {scan::alt};
    case Key::AltRight: return {scan::alt, true};
    case Key::Space: return {scan::space};
    case Key::Enter: return {scan::enter};
    case Key::Escape: return {scan::escape};
    case Key::ArrowDown: return {scan::down, true};
    case Key::ArrowUp: return {scan::up, true};
    case Key::ArrowLeft: return {scan::left, true};
    case Key::ArrowRight: return {scan::right, true};
    default: return {};
  }
}

/// The best scores as the table keeps them, and as TABLEn.HI does: four of twelve digits,
/// three letters and a byte of nothing.
namespace bestRec {
constexpr int count = 4;
constexpr int size = 0x10;
constexpr int name = 12;  ///< after the score's digits
constexpr int nameSize = 3;
}  // namespace bestRec

std::array<u8, bestRec::count * bestRec::size> packed(const HighScores& scores) {
  std::array<u8, bestRec::count * bestRec::size> out{};
  for (std::size_t i = 0; i < bestRec::count; ++i) {
    std::copy(scores[i].score.digits.begin(), scores[i].score.digits.end(), out.begin() + i * bestRec::size);
    std::copy(scores[i].name.begin(), scores[i].name.end(), out.begin() + i * bestRec::size + bestRec::name);
  }
  return out;
}

/// The options' balls: five, or three (PINBALL.CFG's 1 or 0).
constexpr int kFiveBalls = 5;
/// The engine's angles (Engine::setAngle): the original's two, and this version's steeper one.
namespace angles {
constexpr int low = 0, high = 1, higher = 2;
}  // namespace angles
constexpr int kScrollSpeeds = 3, kResolutions = 4;
/// at::keys' bit 2: one step of the ball in the mid-frame callback, the other screen mode's
/// pace (the cheat Recording::Carry::otherSteps).
constexpr int kOtherSteps = 4;
/// The screen's place on the table is followed in sixteenths of a row.
constexpr int kSixteenths = 4;
/// How far down the screen the ball is kept, in the original's two sizes of screen.
constexpr int kLeadNormal = 75, kLeadHigh = 130;
/// The ball is taken to be this far down the table while it is lost and parked.
constexpr int kParkedRow = 525;
/// Rows the screen is moved by hand each frame, while paused.
constexpr int kHandRows = 4;
/// Rows the table says to follow: the Gameshow's wheel, and from this one on, its bottom; and
/// where the screen looks for the wheel, in the original's two sizes of screen.
constexpr int kWheelRow = 0x10e, kBottomRows = 0x171, kWheelHigh = 220, kWheelNormal = 270;
/// How fast the screen follows (pfr: ScrollSpeed's raw), and the rows the ball may get ahead of
/// where it is kept before the screen is pulled along.
constexpr int kScrollHard = 20, kScrollMedium = 11, kScrollSoft = 9;
constexpr int kAhead = 40;
/// The table being left: so much darker each frame, and the music's volume said each sixteenth.
constexpr int kLeaveStep = 2, kVolumeEvery = 0x0f;
/// The pause's display says each of its three things for two seconds.
constexpr int kPauseSays = 120;
constexpr int kColours = 256;

bool same(const Options& a, const Options& b) {
  return a.balls == b.balls && a.angle == b.angle && a.scrollSpeed == b.scrollSpeed && a.resolution == b.resolution &&
         a.noMusic == b.noMusic && a.mono == b.mono;
}

bool same(const HighScores& a, const HighScores& b) {
  for (std::size_t i = 0; i < bestRec::count; ++i)
    if (a[i].score != b[i].score || a[i].name != b[i].name) return false;
  return true;
}

}  // namespace

TableGame::TableGame(ByteView prg, ByteView module, int table, const Setup& setup)
    : options_(setup.options), music_(MusicDriver::kOutputRate) {
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
  o.fiveBalls = options_.balls == kFiveBalls;
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
  if (options_.angle == Angle::Higher) engine_->setAngle(angles::higher);
  // what the table this one's game was started on had that the game would not be the same without
  const Recording::Carry& carry = setup.carry;
  if (carry.balls != 0) engine_->B(at::balls) = carry.balls;
  if (carry.noTilt) engine_->B(at::noTilt) = kSet;
  if (carry.otherSteps) engine_->B(at::keys) ^= kOtherSteps;
  if (carry.scrollPos != kNone) {
    engine_->W(at::lookAt) = carry.scrollPos;
    engine_->W(at::scrollFixed) = carry.scrollAt;
  }
  if (screen_) screen_->started(*engine_);
  // (where the screen was on the table the game was started from, the fraction of a row too)
  camera_.pos = static_cast<u16>(carry.scrollPos != kNone ? carry.scrollPos : std::max(cameraTop(), 0));
  camera_.raw = static_cast<i16>(carry.scrollPos != kNone ? carry.scrollAt : camera_.pos << kSixteenths);

  recording_.table = table;
  recording_.seed = setup.seed;
  recording_.options = options_;
  recording_.highScores = setup.highScores;
  recording_.carry = carry;
  recording_.carry.balls = engine_->B(at::balls);
  bestAtStart_ = savedScores_ = highScores();
  saved_ = options();
  link_->give();
}

TableGame::~TableGame() { music_.conductor = nullptr; }

Bcd TableGame::score(int player) const {
  Bcd s;
  const u16 at = static_cast<u16>(engine_->A(at::playerRecords) + player * engine_->kw(at::playerSizeInsn, 1));
  for (u16 i = 0; i < kScoreDigits; ++i) s.digits[i] = engine_->nativeB(static_cast<u16>(at + i));
  return s;
}

HighScores TableGame::highScores() const {
  HighScores h{};
  const u16 at = engine_->kw(at::bestScoresInsn, 1);
  for (u16 i = 0; i < bestRec::count; ++i) {
    for (u16 d = 0; d < kScoreDigits; ++d) h[i].score.digits[d] = engine_->nativeB(static_cast<u16>(at + i * bestRec::size + d));
    for (u16 c = 0; c < bestRec::nameSize; ++c) h[i].name[c] = engine_->nativeB(static_cast<u16>(at + i * bestRec::size + bestRec::name + c));
  }
  return h;
}

Options TableGame::options() const {
  Options o = options_;
  o.angle = engine_->angle() == angles::low ? Angle::Low : engine_->angle() == angles::higher ? Angle::Higher : Angle::High;
  o.noMusic = engine_->musicIsOff();
  return o;
}

bool TableGame::optionsChanged() { return std::exchange(optionsChanged_, false); }
bool TableGame::highScoresChanged() { return std::exchange(scoresChanged_, false); }

Recording::Carry TableGame::carryOver() const {
  Recording::Carry c;
  c.noTilt = engine_->B(at::noTilt) == kSet;
  c.otherSteps = (engine_->B(at::keys) & kOtherSteps) != 0;
  c.balls = engine_->B(at::balls);
  c.scrollPos = camera_.pos;
  c.scrollAt = static_cast<u16>(camera_.raw);
  return c;
}

bool TableGame::startsGame(Key key) const {
  if (!waiting() || left() || asking_) return false;
  if (key != Key::Enter && !(key >= Key::F1 && key <= Key::F8) && !(key >= Key::Digit1 && key <= Key::Digit8)) return false;
  // the table takes keys, more players may join, and it is not asking whether to be left
  return engine_->CB(at::keysOn) != 0 && engine_->B(at::playersMayJoin) != 0 && engine_->CB(at::quitAsked) != kSet;
}

bool TableGame::askingName() const { return engine_->W(at::task) == engine_->F(at::highScoreName); }

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
  if (s.extended) engine_->key(scan::extended);
  engine_->key(static_cast<u8>(s.code | (down ? 0 : scan::released)));
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

void TableGame::say(std::string_view text) {
  engine_->write(text);
  pauseFrames_ = 0;
}

void TableGame::pausedKey(Key key) {
  switch (key) {
    case Key::A:
      engine_->setAngle(engine_->angle() == angles::high ? angles::higher : engine_->angle() == angles::higher ? angles::low : angles::high);
      engine_->write(engine_->angle() == angles::low ? "ANGLE LOW" : engine_->angle() == angles::higher ? "ANGLE HIGHER" : "ANGLE HIGH");
      break;
    case Key::S:
      options_.scrollSpeed = static_cast<ScrollSpeed>((static_cast<int>(options_.scrollSpeed) + 1) % kScrollSpeeds);
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
        options_.resolution = static_cast<Resolution>((static_cast<int>(options_.resolution) + 1) % kResolutions);
        // the new size of screen looks where it is told to, or where the ball is
        camera_.pos = static_cast<u16>(std::clamp(camera_.said ? *camera_.said : engine_->W(at::ballY).s() - cameraLead(), 0, cameraTop()));
        camera_.raw = static_cast<i16>(camera_.pos << kSixteenths);
      }
      engine_->write("RESOLUTION CHANGED");
      break;
    case Key::F7:
      lamps_ = (lamps_ + 1) % TableScreen::kLampModes;
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

int TableGame::cameraLead() const {
  return options_.resolution == Resolution::Normal ? kLeadNormal : options_.resolution == Resolution::High ? kLeadHigh : 0;
}

TableGame::Sight TableGame::look() const {
  Engine& e = *engine_;
  return {waiting(), e.ballParked() ? kParkedRow : e.W(at::ballY).s(), e.W(at::lookAtSoft), e.W(at::screenTop)};
}

/// Where the screen looks this frame: after the ball, drifting while nobody plays, or moved
/// by hand while paused. `s` is the table as the frame began.
void TableGame::aim(const Sight& s) {
  const u16 said = engine_->W(at::lookAt);
  if (engine_->isPaused()) {
    if (!engine_->asksToQuit() && cameraTop() > 0 && up_ != down_) {
      const int p = std::clamp(static_cast<int>(camera_.pos) + (down_ ? kHandRows : 0) - (up_ ? kHandRows : 0), 0, cameraTop());
      camera_.pos = static_cast<u16>(p);
      camera_.raw = static_cast<i16>(p << kSixteenths);
    }
    return;
  }
  if (s.waiting) {
    if (cameraTop() <= 0) camera_.pos = 0;
    else {
      if (camera_.pos == 0) camera_.up = false;
      else if (camera_.pos >= cameraTop()) camera_.up = true;
      camera_.pos = static_cast<u16>(camera_.up ? camera_.pos - 1 : camera_.pos + 1);
      camera_.raw = static_cast<i16>(camera_.pos << kSixteenths);
    }
    camera_.said.reset();
    saidBefore_ = said;
    return;
  }
  follow(s);
  // Party Land takes the screen up from where it is after its holes (ds:3383, counted from
  // where the table's own screen was): this one goes up by as much from where it is, at once.
  const u16 before = std::exchange(saidBefore_, said);
  if (said == kNone) {
    camera_.said.reset();
    return;
  }
  if (before == kNone || !camera_.said) camera_.said = camera_.pos + static_cast<i16>(said) - static_cast<i16>(s.shown);
  else camera_.said = *camera_.said + static_cast<i16>(said) - static_cast<i16>(before);
  camera_.pos = static_cast<u16>(std::clamp(*camera_.said, 0, std::max(cameraTop(), 0)));
  camera_.raw = static_cast<i16>(camera_.pos << kSixteenths);
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
  else if (s.follow != kNone) {
    // the Gameshow's wheel, or the bottom of the table when a ball is lost
    const int row = static_cast<i16>(s.follow);
    target = row == kWheelRow ? (options_.resolution == Resolution::High ? kWheelHigh : kWheelNormal) : row >= kBottomRows ? top : std::clamp(row, 0, top);
  }
  const i16 speed = options_.scrollSpeed == ScrollSpeed::Hard ? kScrollHard : options_.scrollSpeed == ScrollSpeed::Medium ? kScrollMedium : kScrollSoft;
  i16 delta = static_cast<i16>(target - (camera_.raw >> kSixteenths));
  camera_.raw = static_cast<i16>(camera_.raw + ((delta * speed) >> 2));
  delta = static_cast<i16>(target - (camera_.raw >> kSixteenths));
  if (delta <= -lead) camera_.raw = static_cast<i16>(camera_.raw + ((delta + lead) << kSixteenths));
  else if (delta >= lead + kAhead) camera_.raw = static_cast<i16>(camera_.raw + ((delta - lead - kAhead) << kSixteenths));
  camera_.pos = static_cast<u16>(std::max(camera_.raw >> kSixteenths, 0));
}

void TableGame::frame() {
  if (left()) return;
  syncMusic();
  ++frames_;
  if (recording_.games.empty()) recording_.frames = frames_;
  if (engine_->exited()) {
    // cs:3a11: the table is left: its picture and its music fade away, in 128 frames
    leaving_ -= kLeaveStep;
    if (leaving_ >= 0 && (leaving_ & kVolumeEvery) == 0) music_.volume(static_cast<u16>(leaving_));
    return;
  }
  if (asking_) return;  // the table waits for the answer
  if (std::exchange(answered_, false)) return;  // and takes it in this frame; it goes on from the next
  // where the screen looks is worked out from the table as the frame begins (the ball where
  // it was), once the frame's keys have been taken: a pause pressed now holds it already
  const Sight sight = look();
  // what the display shows as the frame begins (TableScreen draws it from the first 33 rows of
  // the video card's memory, 0x54 bytes of each plane a row)
  constexpr std::size_t kDisplayBytes = std::size_t{TableScreen::kDisplayRows} * TableScreen::kRowBytes;
  for (std::size_t plane = 0; plane < TableScreen::kPlanes; ++plane)
    std::copy_n(engine_->videoMemory()[plane].begin(), kDisplayBytes, display_[plane].begin());
  try {
    engine_->frame();
  } catch (const std::exception& e) {
    // A routine of the original's that was not written, or one led astray: the table is left
    // rather than played on from nobody knows what.
    failure_ = e.what();
    log::error("table " + std::to_string(table() + 1) + ", frame " + std::to_string(frames_) + ": " + failure_);
  }
  if (engine_->exited() && engine_->CB(at::quit) == kSet) {
    // Left from the waiting table's question: the display's script goes straight on to its
    // next step, whose first frame draws a part of it over the question, and the table stops
    // there. The question is what is shown while the table fades away.
    for (std::size_t plane = 0; plane < TableScreen::kPlanes; ++plane)
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
      if (pauseFrames_ == kPauseSays) engine_->write("P TO UNPAUSE");
      else if (pauseFrames_ == 2 * kPauseSays) engine_->write("ASMRD FOR OPTIONS");
      else if (pauseFrames_ == 3 * kPauseSays) {
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
  const u16 wait = engine_->W(at::task);
  if (wait != lastWait_ && lastWait_ == engine_->F(at::highScoreNameDone) && players() == 1 && failure_.empty()) {
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
      cheats_ = engine_->B(at::noTilt) == kSet || (engine_->B(at::keys) & kOtherSteps) != 0 ||
                engine_->B(at::balls) > options_.balls;
    gentlest_ = gentlest_ < 0 ? engine_->angle() : std::min(gentlest_, engine_->angle());
  }
  // (a table left in the middle of a game leaves no game to keep)
  if (playing_ && !now && !engine_->exited() && recording_.games.empty()) {
    Recording::Game g;
    g.endFrame = frames_;
    // (the last ball's number is one past the balls there are, once it has been played)
    g.abandoned = !failure_.empty() || engine_->B(at::ball) <= engine_->B(at::balls);
    for (int p = 0; p < players(); ++p) g.scores.push_back(score(p));
    const HighScores best = highScores();
    if (!g.scores.empty())
      for (std::size_t entry = 0; entry < bestRec::count && g.initials[0] == 0; ++entry) {
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
  view.displayTop = dotMatrixTop;
  screen_->draw(*engine_, frame, view, hd);
  screen_->colours(*engine_, colours, lamps_);
  if (engine_->exited()) {  // cs:4f3c: every colour by how bright the table still is
    const int level = std::max(leaving_, 0);
    for (std::size_t i = 0; i < kColours; ++i)
      colours[i] = Rgb{static_cast<u8>(colours[i].r * level >> 8), static_cast<u8>(colours[i].g * level >> 8), static_cast<u8>(colours[i].b * level >> 8)};
    if (hd) {
      hd->fade.fill(static_cast<float>(level) / static_cast<float>(kFullLevel));
      hd->spriteTint = static_cast<float>(level) / static_cast<float>(kFullLevel);
    }
  }
}

void TableGame::sound(float* out, int frames) {
  music_.render(out, frames);
  if (silent) std::fill_n(out, static_cast<std::size_t>(frames) * 2, 0.0f);
}

}  // namespace encore
