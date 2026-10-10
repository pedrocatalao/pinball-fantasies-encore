// A game's comings and goings: starting one, the ball's beginning, scores.
#include <cstdio>

#include "engine/table/Engine.h"

namespace encore {

/// cs:3881
void Engine::newGame() {
  CB(0x3475) = 0xff;
  B(0x372c) = 0;
  B(0x33dc) = 1;  // the ball
  B(0x228f) = static_cast<u8>(B(0x33dc) + 0x37);
  B(0x371a) = 1;  // the player
  B(0x2288) = static_cast<u8>(B(0x371a) + 0x37);
  B(0x33ef) = 0xff;
  // The original adds up a stretch of its own code here, to see that nobody has changed it,
  // and only then lets players be added and the high scores stand.
  u8 sum = 0;
  for (u16 a = F(0x3030); a != F(0x305a); ++a) sum = static_cast<u8>(sum + cs_[a]);
  if (static_cast<u8>(sum - 2) == cs_[static_cast<u16>(F(0x38c8) + 2)]) {
    B(0x33e3) = 0xff;
    B(0x33ef) = 0;
    B(0x33f9) = 0xff;
  }
  B(0x33f1) = 0;
  B(0x33f2) = 0xff;
}

/// cs:37ea
void Engine::beginBall() {
  W(0x3316) = 0;
  B(0x338a) = 0xff;
  B(0x338b) = 0;
  W(0x338c) = F(0x69fc);
  B(0x338e) = 0xff;
  B(0x338f) = 0xff;
  B(0x3398) = 0;
  B(0x33ce) = 0;
  B(0x33e2) = 0;
  B(0x33f7) = 0;
  B(0x33f9) = 0xff;
  stopBlinks();
  for (u16 i = 0, slots = kw(0x3873, 1); i < slots; ++i) W(0x331b, static_cast<u16>(i * 2)) = F(0x69fc);  // cs:3870
  for (u16 i = 0; i < 0x32; ++i) W(0x35ca, static_cast<u16>(i * 2)) = 0;          // cs:37dd
  if (B(0x00d0) == 0xff) return;
  B(0x3396) = 0;
  displaySteady();
  if (B(0x33f2) == 0xff) {
    B(0x33f2) = 0;
    return;
  }
  for (u16 i = 0; i < 12; ++i) B(0x45da, i) = 0x12;
  W(0x45e6) = 0;
  startScript(A(0x1acc));
}

void Engine::toAttract() {
  CB(0x3195) = 0;
  CB(0x3475) = 0xff;
  B(0x3713) = 0xff;
}

/// cs:5867: every light out, at once and not through the queue.
void Engine::lightsOut() {
  if (B(0x2f2b) != 0xff) return;
  for (u16 light = 1, lights = kw(0x588e, 1); light <= lights; ++light) {
    B(0x3591, light) = 0;
    u16 record = W(0x12bd, static_cast<u16>((light - 1) * 2));
    B(0x354e) = 0;
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
  if (B(0x2f2b) != 0xff) return;
  for (u16 light = 1, lights = kw(0x588e, 1); light <= lights; ++light) {
    u16 record = W(0x12bd, static_cast<u16>((light - 1) * 2));
    B(0x354e) = 0;
    std::size_t colour = std::size_t{nativeB(record++)} * 3;
    const u8 count = nativeB(record++);
    for (u8 i = 0; i < count; ++i, ++colour) dac_[colour % 768] = (nativeB(record++) >> 1) & 0x3f;
  }
}

bool Engine::music(u16 record) {
  if (record == 0) return true;
  const u8 place = nativeB(record), repeats = nativeB(static_cast<u16>(record + 1)), priority = nativeB(static_cast<u16>(record + 2));
  if (priority < B(0x3389)) return false;
  B(0x3389) = priority;
  const u8 before = B(0x230d);
  B(0x230d) = repeats;
  const u8 answer = sound->jump(place);
  B(0x230d) = repeats;
  B(0x338e) = 0;
  B(0x338f) = 0;
  if (static_cast<i8>(before) > 0) return true;
  if (answer == kb(0x5c87, 1)) return true;
  B(0x230a) = answer;
  return true;
}

void Engine::addScore(u16 to, u16 amount) {
  if (debugWatch == -2) {
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
    if (static_cast<u8>(first) == B(0x0c84) || (B(0x3398) != 0xff && B(0x33e2) != 0xff)) refused = !music(first);
  } else {
    refused = nativeB(record++) < B(0x3389);
  }
  W(0x36f6) = 0;
  if (W(0x36f8) == 0xff) {
    startScript(A(0x1acc));
    W(0x36f8) = 0;
  }
  addScore(A(0x45b6), record);
  B(0x33de) = 0xff;
  addScore(A(0x3399), static_cast<u16>(record + 12));
  const u16 script = nativeW(static_cast<u16>(record + 24));
  if (refused || script == 0 || B(0x3398) == 0xff || B(0x33e2) == 0xff) {
    B(0x33f8) = 0xff;
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
  W(0x36f6) = 0;
  if (W(0x36f8) == 0xff) {
    startScript(A(0x1acc));
    W(0x36f8) = 0;
  }
  addScore(A(0x45b6), record);
  B(0x33de) = 0xff;
  addScore(A(0x3399), static_cast<u16>(record + 12));
  const u16 script = nativeW(static_cast<u16>(record + 24));
  if (script == 0) return false;
  startScript(script);
  return true;
}

bool Engine::nextOfRow(u16 row) {
  const u8 n = nativeB(row);
  setLight(nativeB(static_cast<u16>(row + 1 + n)));
  if (nativeB(static_cast<u16>(row + 2 + n)) == 0xff) {
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
  bind(0x353b, [this] {  // the music turned off: silence, as soon as it may
    B(0x3389) = 0;
    music(A(0x0c6c));
    endTimer();
  });
  // the words that can be typed while no game is played, each with what it shows (cs:6b01 on)
  struct Word { u16 at, script; };
  for (const Word w : {Word{0x6b01, 0x439c}, {0x6b0d, 0x43dc}, {0x6b19, 0x43d4}, {0x6b25, 0x43c4}, {0x6b31, 0x43cc}, {0x6b3d, 0x43a4},
                       {0x6b49, 0x43e4}, {0x6b5a, 0x43ac}, {0x6b66, 0x43b4}, {0x6b72, 0x43bc}, {0x6b7e, 0x43ec}, {0x6b8f, 0x43f4},
                       {0x6ba0, 0x43fc}}) {
    bind(w.at, [this, w] {
      B(0x372c) = 0xff;
      if (w.at == 0x6b49) B(0x372a) = 0xff;  // no tilt
      startScript(A(w.script));
      if (w.at == 0x6b7e) B(at::keys) |= 4;
      if (w.at == 0x6b8f) B(0x33dd) = 5;     // five balls
      if (w.at == 0x6ba0) {                  // and back to as it was
        B(at::keys) &= 0xfb;
        B(0x372a) = 0;
        B(0x33dd) = 3;
      }
    });
  }
  bind(0x61e9, [this] {  // players may be added again a moment after the last
    if (!countTo(A(0x3616), 0x0f)) return;
    B(0x33e3) = 0xff;
    endTimer();
  });
}

}  // namespace encore
