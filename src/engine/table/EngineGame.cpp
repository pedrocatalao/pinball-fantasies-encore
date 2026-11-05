// A game's comings and goings: starting one, the ball's beginning, scores.
#include <cstdio>

#include "engine/table/Engine.h"

namespace encore {
namespace {

constexpr u8 kRowEnd = 0xff;  ///< ends a row of lights (nextOfRow)

}  // namespace

/// cs:3881
void Engine::newGame() {
  CB(at::keysOn) = kSet;
  B(at::enterAttract) = 0;
  B(at::ball) = 1;
  B(at::ballDigit) = static_cast<u8>(B(at::ball) + 0x37);
  B(at::player) = 1;
  B(at::playerDigit) = static_cast<u8>(B(at::player) + 0x37);
  B(at::codeChanged) = kSet;
  // The original adds up a stretch of its own code here, to see that nobody has changed it,
  // and only then lets players be added and the high scores stand.
  u8 sum = 0;
  for (u16 a = F(at::checkedFrom); a != F(at::checkedTo); ++a) sum = static_cast<u8>(sum + cs_[a]);
  if (static_cast<u8>(sum - 2) == cs_[static_cast<u16>(F(at::checkedSum) + 2)]) {
    B(at::playersMayJoin) = kSet;
    B(at::codeChanged) = 0;
    B(at::scriptEnded) = kSet;
  }
  B(at::gotTopScore) = 0;
  B(at::inGameStart) = kSet;
}

/// cs:37ea
void Engine::beginBall() {
  W(at::inTrigger) = 0;
  B(at::ballOnPlunger) = kSet;
  B(at::flipperPressed) = 0;
  W(at::runOnce) = F(at::nothing);
  B(at::jingleEnded) = kSet;
  B(at::jingleEndedRules) = kSet;
  B(at::awardQuiet) = 0;
  B(at::betweenBalls) = 0;
  B(at::modeRunning) = 0;
  B(at::countdownHeld) = 0;
  B(at::scriptEnded) = kSet;
  stopBlinks();
  for (u16 i = 0, slots = kw(at::timerSlotsInsn, 1); i < slots; ++i) W(at::timers, static_cast<u16>(i * 2)) = F(at::nothing);  // cs:3870
  for (u16 i = 0; i < 0x32; ++i) W(at::counters, static_cast<u16>(i * 2)) = 0;          // cs:37dd
  if (B(at::specialPlungerEvent) == kSet) return;
  B(at::flashing) = 0;
  displaySteady();
  if (B(at::inGameStart) == kSet) {
    B(at::inGameStart) = 0;
    return;
  }
  for (u16 i = 0; i < 12; ++i) B(at::scoreDrawn, i) = display::scoreNotDrawn;
  W(at::scoreDrawnGroups) = 0;
  startScript(A(at::scriptMain));
}

void Engine::toAttract() {
  CB(at::quitAsked) = 0;
  CB(at::keysOn) = kSet;
  B(at::attract) = kSet;
}

/// cs:5867: every light out, at once and not through the queue.
void Engine::lightsOut() {
  if (B(at::lightsReady) != kSet) return;
  for (u16 light = 1, lights = kw(at::lightCountOutInsn, 1); light <= lights; ++light) {
    B(at::lightStates, light) = 0;
    u16 record = W(at::lightRecords, static_cast<u16>((light - 1) * 2));
    B(at::lightsOutInterrupted) = 0;
    std::size_t colour = std::size_t{nativeB(record++)} * 3;
    const u8 count = nativeB(record++);
    for (u8 i = 0; i < count; ++i, ++colour) dac_[colour % 768] = (nativeB(record++) >> 1) & 0x3f;
  }
}

/// A tilt puts every light out. In the original that is lightsOut, which also forgets which
/// were lit, so the lights a player keeps from ball to ball are lost with the ball. Amended:
/// they only go dark, and what was lit is still the player's when the ball is over.
void Engine::lightsDark() {
  if (!amended) return lightsOut();
  if (B(at::lightsReady) != kSet) return;
  for (u16 light = 1, lights = kw(at::lightCountOutInsn, 1); light <= lights; ++light) {
    u16 record = W(at::lightRecords, static_cast<u16>((light - 1) * 2));
    B(at::lightsOutInterrupted) = 0;
    std::size_t colour = std::size_t{nativeB(record++)} * 3;
    const u8 count = nativeB(record++);
    for (u8 i = 0; i < count; ++i, ++colour) dac_[colour % 768] = (nativeB(record++) >> 1) & 0x3f;
  }
}

bool Engine::music(u16 record) {
  if (record == 0) return true;
  const u8 place = nativeB(static_cast<u16>(record + tuneRec::place)), repeats = nativeB(static_cast<u16>(record + tuneRec::repeats)),
           priority = nativeB(static_cast<u16>(record + tuneRec::priority));
  if (priority < B(at::musicPriority)) return false;
  B(at::musicPriority) = priority;
  const u8 before = B(at::musicRepeats);
  B(at::musicRepeats) = repeats;
  const u8 answer = sound->jump(place);
  B(at::musicRepeats) = repeats;
  B(at::jingleEnded) = 0;
  B(at::jingleEndedRules) = 0;
  if (static_cast<i8>(before) > 0) return true;
  if (answer == kb(at::musicRefusedInsn, 1)) return true;
  B(at::musicNext) = answer;
  return true;
}

void Engine::addScore(u16 to, u16 amount) {
  if (debugWatch == kDebugTrace) {
    std::fprintf(stderr, "[ours] addScore to %04x from %04x:", to, amount);
    for (int i = 0; i < 12; ++i) std::fprintf(stderr, "%d", nativeB(static_cast<u16>(amount + i)));
    std::fprintf(stderr, "\n");
  }
  // Digit by digit from the right, as the original's "add, then adjust to a decimal digit".
  u8 carry = 0;
  for (int i = 11; i >= 0; --i) {
    const u8 a = nativeB(static_cast<u16>(to + i)), b = nativeB(static_cast<u16>(amount + i));
    u8 sum = static_cast<u8>(a + b + carry);
    const bool half = (a & 0x0f) + (b & 0x0f) + carry > 0x0f;
    carry = 0;
    if ((sum & 0x0f) > 9 || half) {
      sum = static_cast<u8>(sum + 6);
      carry = 1;
    }
    nativeB(static_cast<u16>(to + i)) = sum & 0x0f;
  }
}

bool Engine::award(u16 record) {
  bool refused = false;
  const u16 first = nativeW(record);
  record = static_cast<u16>(record + 2);
  if (first != 0) {
    if (static_cast<u8>(first) == B(at::tuneDrained) || (B(at::awardQuiet) != kSet && B(at::modeRunning) != kSet)) refused = !music(first);
  } else {
    refused = nativeB(record++) < B(at::musicPriority);
  }
  W(at::idleFrames) = 0;
  if (W(at::idleShowing) == kSet) {
    startScript(A(at::scriptMain));
    W(at::idleShowing) = 0;
  }
  addScore(A(at::score), record);
  B(at::scoredThisBall) = kSet;
  addScore(A(at::bonus), static_cast<u16>(record + awardRec::bonus));
  const u16 script = nativeW(static_cast<u16>(record + awardRec::script));
  if (refused || script == 0 || B(at::awardQuiet) == kSet || B(at::modeRunning) == kSet) {
    B(at::scriptDone) = kSet;
    return false;
  }
  startScript(script);
  return true;
}

bool Engine::awardAlways(u16 record) {
  const u16 first = nativeW(record);
  record = static_cast<u16>(record + 2);
  if (first != 0) music(first);
  else ++record;
  W(at::idleFrames) = 0;
  if (W(at::idleShowing) == kSet) {
    startScript(A(at::scriptMain));
    W(at::idleShowing) = 0;
  }
  addScore(A(at::score), record);
  B(at::scoredThisBall) = kSet;
  addScore(A(at::bonus), static_cast<u16>(record + awardRec::bonus));
  const u16 script = nativeW(static_cast<u16>(record + awardRec::script));
  if (script == 0) return false;
  startScript(script);
  return true;
}

bool Engine::nextOfRow(u16 row) {
  const u8 n = nativeB(row);
  setLight(nativeB(static_cast<u16>(row + 1 + n)));
  if (nativeB(static_cast<u16>(row + 2 + n)) == kRowEnd) {
    nativeB(row) = 0;
    return true;
  }
  ++nativeB(row);
  return false;
}

void Engine::placeBall(u16 x, u16 y) {
  W(at::ballX) = x;
  W(at::ballY) = y;
  B(at::layer) = 0;
  const u32 fx = u32{x} * 0x400, fy = u32{y} * 0x400;
  W(at::ballXFixed) = static_cast<u16>(fx);
  W(at::ballXFixed, 2) = static_cast<u16>(fx >> 16);
  W(at::ballYFixed) = static_cast<u16>(fy);
  W(at::ballYFixed, 2) = static_cast<u16>(fy >> 16);
  W(at::ballVy) = 0;
  W(at::ballVx) = 0;
  spinDue_ = true;
}

void Engine::patchMask(u16 segment, u16 at, u16 shape, u16 width, u16 rows) { copyShape(S(segment), at, shape, width, rows, width); }

void Engine::copyShape(u16 native, u16 at, u16 shape, u16 width, u16 rows, u16 step) {
  for (u16 r = 0; r < rows; ++r, at = static_cast<u16>(at + 0x28), shape = static_cast<u16>(shape + step))
    for (u16 x = 0; x < width; ++x) {
      const u8 v = nativeB(static_cast<u16>(shape + x));
      farB(native, static_cast<u16>(at + x)) = v;
    }
}

void Engine::bindGame() {
  bind(at::musicOffTimer, [this] {  // the music turned off: silence, as soon as it may
    B(at::musicPriority) = 0;
    music(A(at::tuneSilence));
    endTimer();
  });
  // the words that can be typed while no game is played, each with what it shows (cs:6b01 on)
  struct Word { u16 at, script; };
  for (const Word w : {Word{at::cheatTech, at::scriptTech}, {at::cheatTsp, at::scriptTsp}, {at::cheatJohan, at::scriptJohan},
                       {at::cheatDaniel, at::scriptDaniel}, {at::cheatGabriel, at::scriptGabriel}, {at::cheatCheat, at::scriptCheat},
                       {at::cheatEarthquake, at::scriptEarthquake}, {at::cheatRobban, at::scriptRobban}, {at::cheatStein, at::scriptStein},
                       {at::cheatGreet, at::scriptGreet}, {at::cheatSnail, at::scriptSnail}, {at::cheatExtraBalls, at::scriptExtraBalls},
                       {at::cheatFairPlay, at::scriptFairPlay}}) {
    bind(w.at, [this, w] {
      B(at::enterAttract) = kSet;
      if (w.at == at::cheatEarthquake) B(at::noTilt) = kSet;
      startScript(A(w.script));
      if (w.at == at::cheatSnail) B(at::keys) |= keyBits::oneStep;
      if (w.at == at::cheatExtraBalls) B(at::balls) = 5;
      if (w.at == at::cheatFairPlay) {  // and back to as it was
        B(at::keys) &= static_cast<u8>(~keyBits::oneStep);
        B(at::noTilt) = 0;
        B(at::balls) = 3;
      }
    });
  }
  bind(at::setStartKeysActiveTimer, [this] {  // players may be added again a moment after the last
    if (!countTo(A(at::setStartKeysActiveCounter), 0x0f)) return;
    B(at::playersMayJoin) = kSet;
    endTimer();
  });
}

}  // namespace encore
