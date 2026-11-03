// The ball: its sub-steps (probe, bounce, flippers and nudge, move), and what follows them
// each frame. All of it works on the program's own variables and collision masks.
#include <cstdio>
#include <utility>

#include "engine/table/Engine.h"

namespace encore {
namespace {

/// The 44 points round the ball that are tested, and the angle each stands for, in 2048ths of
/// a turn from pointing right, clockwise (cs:8866 on, where each is its own instructions).
struct ProbePoint {
  i8 dx, dy;
  i16 angle;
};
constexpr ProbePoint kProbes[44] = {
    {8, 0, 0x000},  {8, 1, 0x029},   {8, 2, 0x050},   {7, 3, 0x084},   {7, 4, 0x0a9},   {6, 5, 0x0e2},
    {5, 6, 0x11e},  {4, 7, 0x157},   {3, 7, 0x17c},   {2, 8, 0x1b0},   {1, 8, 0x1d7},   {0, 8, 0x200},
    {-1, 8, 0x229}, {-2, 8, 0x250},  {-3, 7, 0x284},  {-4, 7, 0x2a9},  {-5, 6, 0x2e2},  {-6, 5, 0x31e},
    {-7, 4, 0x357}, {-7, 3, 0x37c},  {-8, 2, 0x3b0},  {-8, 1, 0x3d7},  {-8, 0, 0x400},  {-8, -1, 0x429},
    {-8, -2, 0x450},{-7, -3, 0x484}, {-7, -4, 0x4a9}, {-6, -5, 0x4e2}, {-5, -6, 0x51e}, {-4, -7, 0x557},
    {-3, -7, 0x57c},{-2, -8, 0x5b0}, {-1, -8, 0x5d7}, {0, -8, 0x600},  {1, -8, 0x629},  {2, -8, 0x650},
    {3, -7, 0x684}, {4, -7, 0x6a9},  {5, -6, 0x6e2},  {6, -5, 0x71e},  {7, -4, 0x757},  {7, -3, 0x77c},
    {8, -2, 0x7b0}, {8, -1, 0x7d7},
};
/// The order they are tested in: rows from the top down. The last to touch decides what the
/// ball is taken to have hit.
constexpr u8 kProbeOrder[44] = {35, 34, 33, 32, 31, 37, 36, 30, 29, 38, 28, 39, 27, 40, 26,
                                41, 25, 42, 24, 43, 23, 0,  22, 1,  21, 2,  20, 3,  19, 4,
                                18, 5,  17, 6,  16, 7,  8,  14, 15, 9,  10, 11, 12, 13};

i16 clampTo(i16 v, i16 low, i16 highest) { return v < low ? low : v > highest ? highest : v; }

}  // namespace

/// cs:87c0
void Engine::physicsSteps() {
  if (CB(at::physicsBusy) == 0xff) return;
  CB(at::physicsBusy) = 0xff;
  const bool twice = !((B(at::keys) & B(at::inMidFrame)) & 4);
  for (int i = 0; i < (twice ? 2 : 1); ++i) {
    if (B(at::ballHidden) == 0xff) {
      spinDue_ = false;  // (put away, not sent off: it leaves later with the spin it has)
      stepsKept_ = 0;
      nudgeAndFlippers();
      stampFlippers();
      continue;
    }
    if (spinDue_ && drawsChance()) {
      // the spin of a ball put somewhere, drawn as it sets off from there
      const u16 spin = chance(0x400, 0);
      W(at::spin) = (spin & 1) ? static_cast<u16>(-spin) : spin;
    }
    spinDue_ = false;
    if (probeBall()) bounce();
    nudgeAndFlippers();
    integrate();
    stampFlippers();
  }
  CB(at::physicsBusy) = 0;
}

bool Engine::maskBit(u16 partyLandSegment, int x, int y) {
  // As the original, nothing stops a place beyond an edge: it is a dot of the row above or
  // below, or of what lies beside the mask.
  const u16 column = static_cast<u16>(x);
  return farB(S(partyLandSegment), static_cast<u16>(y * 0x28 + (column >> 3))) & (0x80 >> (column & 7));
}

u8 Engine::copiedMask(int which, u16 offset) {
  // The original keeps pictures over parts of these copies, and so takes what the ball
  // touches there for something else than it is. Amended: the masks themselves are read.
  if (amended) return farB(S(which == 0 ? at::marksRamps : which == 1 ? at::slopesRamps : at::slopesGround), offset);
  const auto& plane = video_[offset & 3];
  if (which == 0) return plane[static_cast<u16>(W(at::marksRampsCopy) + (offset >> 2))];
  if (which == 1) return plane[static_cast<u16>(W(at::slopesRampsCopy) + (offset >> 2))];
  // the playfield's is in two pieces: its top rows beside the display, the rest after the others
  if (offset < video::groundTopBytes) return plane[static_cast<u16>(((offset >> 4) + 1) * 0x50 + (offset >> 2) + video::picture)];
  return plane[static_cast<u16>(W(at::slopesGroundCopy) + (offset >> 2) - (video::groundTopBytes >> 2))];
}

/// cs:8829
bool Engine::probeBall() {
  const u16 walls = S(B(at::layer) == 0 ? at::wallsGround : at::wallsRamps);
  // The original works out the byte the ball's left edge is in once, as a number without a
  // sign, and reaches each point from there: with the ball at the table's left edge that is
  // 8 KB further on in the mask's segment, not the row's own start.
  const u32 left = static_cast<u16>(W(at::ballX) - 1);
  const int cy = W(at::ballY).s() + W(at::nudgeLift).s() + 7;
  u16 sum = 0;
  int count = 0, lowerHalf = 0, quadrants = 0, last = -1;
  for (u8 index : kProbeOrder) {
    const ProbePoint& p = kProbes[index];
    const u32 x = left + static_cast<u32>(p.dx + 8);
    if (!(farB(walls, static_cast<u16>((cy + p.dy) * 0x28 + (x >> 3))) & (0x80 >> (x & 7)))) continue;
    sum = static_cast<u16>(sum + p.angle);
    ++count;
    // (the point due left, at 0x400, is counted with the quarter below it: cs:8a2f)
    if (p.angle <= 0x400) ++lowerHalf;
    quadrants |= p.angle == 0x400 ? 2 : 1 << (p.angle >> 9);
    last = index;
  }
  // Touches either side of "pointing right" would average to pointing left.
  if (quadrants == 0x9 || quadrants == 0xb || quadrants == 0xd) sum = static_cast<u16>(sum + (lowerHalf << 11));
  if (count == 0) return false;
  B(at::contactProbes) = static_cast<u8>(count);
  W(at::contactAngle) = static_cast<u16>((sum / count) & 0x7ff);

  // What was hit: the three planes under the last point that touched.
  const int mx = W(at::probePoints, static_cast<u16>(last * 4)).s() + W(at::ballX).s() - 1;
  int my = W(at::probePoints + 2, static_cast<u16>(last * 4)).s() + W(at::ballY).s() - 1;
  if (my >= 0x240) return false;
  my += W(at::nudgeLift).s();
  u8 material = 0;
  const u16 place = static_cast<u16>(my * 0x28 + (static_cast<u16>(mx) >> 3));
  const u8 bit = static_cast<u8>(0x80 >> (mx & 7));
  if (B(at::layer) != 0xff) {
    if (farB(S(at::marksGround), place) & bit) material |= 1;
    if (farB(S(at::wallsGround), place) & bit) material |= 2;
    if (copiedMask(2, place) & bit) material |= 4;
  } else {
    if (farB(S(at::wallsRamps), place) & bit) material |= 2;
    if (copiedMask(0, place) & bit) material |= 1;
    if (copiedMask(1, place) & bit) material |= 4;
  }
  B(at::touchedMaterial) = material;

  // Where: the point of the 44 nearest the mean angle.
  // (an angle just short of the full turn rounds to a forty-fifth point, which the original
  // reads from whatever follows the list; amended: that is the first point again)
  u16 nearest = static_cast<u16>(((u32{0x580} * W(at::contactAngle) + 0x8000) >> 16) * 4);
  if (amended) nearest = static_cast<u16>(nearest % (44 * 4));
  const u16 x = static_cast<u16>(W(at::probePoints, nearest) + W(at::ballX)), y = static_cast<u16>(W(at::probePoints + 2, nearest) + W(at::ballY));
  W(at::contactX) = x;
  W(at::contactY) = y;
  W(at::flipperVx) = 0;
  W(at::flipperVy) = 0;
  B(at::material) = material;
  for (u16 i = 0; i < 10; ++i) B(at::materialGain, i) = B(at::materials, static_cast<u16>(material * 16 + i));

  if (material == 7 || material == 3) {  // a bumper's plastic, a kicker's rubber: which one?
    if (B(at::tilted) == 0xff) return true;
    for (u16 e = material == 7 ? A(at::bumpers) : A(at::kickers); nativeW(e) != 0xffff; e = static_cast<u16>(e + zoneRec::size)) {
      if (x < nativeW(static_cast<u16>(e + zoneRec::xMin))) continue;
      if (y < nativeW(static_cast<u16>(e + zoneRec::yMin))) return true;  // as the original: above one ends the search
      if (x > nativeW(static_cast<u16>(e + zoneRec::xMax)) || y > nativeW(static_cast<u16>(e + zoneRec::yMax))) continue;
      W(at::bumperRecord) = nativeW(static_cast<u16>(e + zoneRec::then));
      B(at::bumperPending) = 0xff;
      return true;
    }
    return true;
  }
  if (material != 2) return true;

  // A flipper: how fast its surface is moving where the ball touches it.
  for (u16 f = A(at::flippers); nativeB(f) != 0; f = static_cast<u16>(f + flipperRec::size)) {
    auto w = [&](u16 o) { return nativeW(static_cast<u16>(f + o)); };
    if (x < w(flipperRec::ballXMin) || x > w(flipperRec::ballXMax) || y < w(flipperRec::ballYMin) || y > w(flipperRec::ballYMax)) continue;
    i16 dx = static_cast<i16>(x - w(flipperRec::originX));
    const bool right = nativeB(f) == 1;
    if (right ? dx >= 0 : dx < 0) return true;  // the wrong side of its pivot
    i16 dy = static_cast<i16>(y - w(flipperRec::originY));
    i16 extra = 0;
    if (w(flipperRec::upright) & 0xff) {  // a flipper that stands upright
      std::swap(dx, dy);
      extra = static_cast<i16>(dy >> 1);
      if (extra < 0) extra = static_cast<i16>(-extra);
    } else {
      if (right) {
        dx = static_cast<i16>(-dx);
        dy = static_cast<i16>(-dy);
      }
      extra = dy < 0 ? static_cast<i16>(-dy) : dy;
      extra = static_cast<i16>(extra >> 2);
    }
    dx = static_cast<i16>(-(dx + extra));
    const i16 speed = static_cast<i16>(w(flipperRec::speed));
    W(at::flipperVy) = static_cast<u16>(speed * dx);
    W(at::flipperVx) = static_cast<u16>(speed * dy);
    if (angle_ == 2) {  // this version's steeper table: a quarter more push
      W(at::flipperVy) = static_cast<u16>(W(at::flipperVy).s() * 5 / 4);
      W(at::flipperVx) = static_cast<u16>(W(at::flipperVx).s() * 5 / 4);
    }
    return true;
  }
  return true;
}

/// cs:8e95
void Engine::bounce() {
  if (debugWatch == -2)
    std::fprintf(stderr, "[ours] bounce at (%d,%d) speed (%d,%d) angle %03x probes %d material %d contact (%d,%d) nudge %d\n", W(at::ballX).s(),
                 W(at::ballY).s(), W(at::ballVx).s(), W(at::ballVy).s(), static_cast<unsigned>(W(at::contactAngle)), B(at::contactProbes),
                 B(at::material), W(at::contactX).s(), W(at::contactY).s(), W(at::nudgeLift).s());
  const i16 lowest = W(at::speedMin).s(), highest = W(at::speedMax).s();
  const i16 vx = clampTo(static_cast<i16>(W(at::ballVx) + W(at::flipperVx)), lowest, highest);
  const i16 vy = clampTo(static_cast<i16>(W(at::ballVy) + W(at::flipperVy) + W(at::tableVelocity)), lowest, highest);
  const u16 angle = W(at::contactAngle);

  // Turned to the surface: along its normal, and along it. (The sines are of 14 bits; the
  // shifts leave twice the true speeds, which turning back undoes.)
  const u16 back = static_cast<u16>(((0x800 - angle) & 0x7ff) * 2);
  i32 cosine = W(at::cosines, back).s(), sine = W(at::sines, back).s();
  i16 normal = static_cast<i16>(static_cast<u32>((cosine * vx - sine * vy) << 3) >> 16);
  i16 along = static_cast<i16>(static_cast<u32>((sine * vx + cosine * vy) << 3) >> 16);
  if (normal <= 0) {  // moving away from it already
    B(at::bumperPending) = 0;
    return;
  }
  normal = static_cast<i16>(-normal);

  bool kicked = false;
  if (normal >= W(at::minBounceSpeed).s()) {
    normal = 0;  // too slow to bounce
  } else {
    i16 glance = static_cast<i16>((16 * i32{along}) / normal);
    if (glance < 0) glance = static_cast<i16>(-glance);
    if (static_cast<u16>(glance) >= W(at::maxBounceAngle)) {
      normal = 0;  // too glancing
    } else if (B(at::bumperPending) != 0) {
      if (B(at::material) != 3) {  // a bumper always throws the ball back
        normal = static_cast<i16>(normal + W(at::bumperSpeedBoost));
        kicked = true;
      } else if (normal <= W(at::kickerSpeedThreshold).s()) {  // a kicker only when hit hard enough
        normal = static_cast<i16>(normal + W(at::kickerSpeedBoost));
        kicked = true;
      }
    }
  }
  if (!kicked) B(at::bumperPending) = 0;

  normal = static_cast<i16>(normal - static_cast<i16>((i32{normal} * 256) / W(at::bounceFactor).s()));
  // How much the ball's spin and its speed along the surface trade. The two gains are scaled
  // by the softness of the hit in 16-bit registers, so steel's wrap round for all but the
  // hardest hits, and are then signed divisors; a gain of zero leaves the dividend's low word.
  i16 gain = W(at::materialGain).s(), spinGain = W(at::materialSpinGain).s();
  i32 alongWhole = 0;
  if (wholeGains) {
    i32 wide = gain, wideSpin = spinGain;
    if (normal >= -1023) {
      const i32 soft = (-normal >> 6) + 1;
      wide *= soft;
      wideSpin *= soft;
    }
    const i32 slip = (W(at::spin).s() + W(at::tableVelocity).s() - along) * 256;
    alongWhole = along + (wide != 0 ? slip / wide : slip);
    W(at::spin) -= static_cast<u16>(static_cast<i16>(wideSpin != 0 ? slip / wideSpin : slip));
    alongWhole = alongWhole * 0x800 / 0x801;
  } else {
  if (normal >= -1023) {
    const u16 soft = static_cast<u16>((static_cast<i16>(-normal) >> 6) + 1);
    gain = static_cast<i16>(static_cast<u16>(soft * static_cast<u16>(gain)));
    spinGain = static_cast<i16>(static_cast<u16>(soft * static_cast<u16>(spinGain)));
  }
  const i32 slip = i32{static_cast<i16>(W(at::spin) + W(at::tableVelocity) - along)} * 256;
  along = static_cast<i16>(along + (gain != 0 ? static_cast<i16>(slip / gain) : static_cast<i16>(slip)));
  W(at::spin) -= static_cast<u16>(spinGain != 0 ? static_cast<i16>(slip / spinGain) : static_cast<i16>(slip));
  along = static_cast<i16>((i32{0x800} * along) / 0x801);
  alongWhole = along;
  }

  // Turned back.
  cosine = W(at::cosines, static_cast<u16>(angle * 2)).s();
  sine = W(at::sines, static_cast<u16>(angle * 2)).s();
  i16 nvx = static_cast<i16>(static_cast<u32>((cosine * normal - sine * alongWhole) << 1) >> 16);
  i16 nvy = static_cast<i16>(static_cast<u32>((sine * normal + cosine * alongWhole) << 1) >> 16);
  nvx = static_cast<i16>(nvx - W(at::flipperVx));
  nvy = static_cast<i16>(nvy - W(at::flipperVy) - W(at::tableVelocity));
  W(at::ballVx) = static_cast<u16>(clampTo(nvx, lowest, highest));
  W(at::ballVy) = static_cast<u16>(clampTo(nvy, lowest, highest));

  if (B(at::contactProbes) >= 6) {  // deep in: a quarter of a dot back out along the normal
    auto push = [this](u16 fixed, i32 unit) {
      const i32 by = static_cast<i16>(static_cast<u32>(i32{-1024} * unit) >> 16);
      const u32 v = (u32{W(fixed)} | (u32{W(fixed, 2)} << 16)) + static_cast<u32>(by);
      W(fixed) = static_cast<u16>(v);
      W(fixed, 2) = static_cast<u16>(v >> 16);
    };
    push(at::ballXFixed, cosine);
    push(at::ballYFixed, sine);
  }
}

/// cs:9133
void Engine::nudgeAndFlippers() {
  if (B(at::spaceHeld) != 0) {  // the space bar is held: the table rises
    W(at::tableVelocity) = W(at::pushSpeedAttack);
    W(at::nudgeAccumulator) += W(at::tableVelocity);
    if (W(at::nudgeAccumulator) > 0x800) {
      W(at::tableVelocity) = 0;
      W(at::nudgeAccumulator) = 0x800;
    }
  } else {
    W(at::tableVelocity) = W(at::pushSpeedRelease);
    W(at::nudgeAccumulator) += W(at::tableVelocity);
    if (W(at::nudgeAccumulator).s() < 0) {
      W(at::tableVelocity) = 0;
      W(at::nudgeAccumulator) = 0;
    }
  }
  W(at::nudgeLift) = W(at::nudgeAccumulator) >> 9;

  for (u16 f = A(at::flippers);; f = static_cast<u16>(f + flipperRec::size)) {
    auto w = [&](u16 o) { return nativeW(static_cast<u16>(f + o)); };
    const u8 side = nativeB(f);
    if (side != 1 && side != 2) return;
    const bool held = (B(at::keys) & (side == 2 ? 2 : 1)) && B(at::flippersLive) != 0;
    i16 speed = static_cast<i16>(w(flipperRec::speed) + w(held ? flipperRec::accelHeld : flipperRec::accelLetGo));
    if (held && speed > w(flipperRec::speedHeldMax).s()) speed = w(flipperRec::speedHeldMax).s();
    w(flipperRec::speed) = static_cast<u16>(speed);
    const u16 position = static_cast<u16>(w(flipperRec::position) - speed);
    w(flipperRec::position) = position;
    i16 picture = nativeW(static_cast<u16>(A(at::flipperQuanta) + position * 2)).s();
    if (picture == 0) {
      w(flipperRec::speed) = 0;
      w(flipperRec::position) = 0;
    }
    if (picture >= w(flipperRec::lastPicture).s()) {
      w(flipperRec::speed) = 0;
      w(flipperRec::position) = w(flipperRec::lastPosition);
      picture = w(flipperRec::lastPicture).s();
    }
    w(flipperRec::picture) = static_cast<u16>(picture);
  }
}

/// cs:908f
void Engine::integrate() {
  auto move = [this](u16 fixed, u16 speed) {
    const i32 v = static_cast<i32>(u32{W(fixed)} | (u32{W(fixed, 2)} << 16)) + W(speed).s();
    W(fixed) = static_cast<u16>(v);
    W(fixed, 2) = static_cast<u16>(static_cast<u32>(v) >> 16);
    return static_cast<u16>(v / 0x400);
  };
  W(at::ballY) = move(at::ballYFixed, at::ballVy);
  if (W(at::ballY).s() >= 0x240) B(at::ballLost) = 0xff;
  W(at::ballX) = move(at::ballXFixed, at::ballVx);
  for (std::size_t i = 1; i < kSteps; ++i) steps_[i - 1] = steps_[i];
  if (stepsKept_ < kSteps) ++stepsKept_;
  steps_[kSteps - 1] = {static_cast<i32>(u32{W(at::ballXFixed)} | (u32{W(at::ballXFixed, 2)} << 16)),
                        static_cast<i32>(u32{W(at::ballYFixed)} | (u32{W(at::ballYFixed, 2)} << 16))};
  W(at::ballVy) += W(at::gravityY);
  W(at::ballVx) += W(at::gravityX);
  const i16 spin = W(at::spin).s();
  if (spin > 0) W(at::spin) = static_cast<u16>(spin - 2 > 0 ? spin - 2 : 0);
  else if (spin < 0) W(at::spin) = static_cast<u16>(spin + 2 < 0 ? spin + 2 : 0);
}

/// cs:9106: a flipper is part of the wall. While the ball is near one, the picture of where
/// it now stands is put into the collision mask (cs:3d36).
void Engine::stampFlippers() {
  const u16 x = W(at::ballX), y = W(at::ballY);
  for (u16 f = A(at::flippers); nativeB(f) != 0; f = static_cast<u16>(f + flipperRec::size)) {
    auto w = [&](u16 o) { return nativeW(static_cast<u16>(f + o)); };
    // (the original only does so while the ball is within a box round the flipper, though the
    // ball's edge reaches the flipper from a little outside it, where it then meets the
    // flipper as it last stood; amended: the mask always has the flipper where it is)
    if (!amended && (x < w(flipperRec::ballXMin) || x > w(flipperRec::ballXMax) || y < w(flipperRec::ballYMin) || y > w(flipperRec::ballYMax))) continue;
    const u16 picture = w(flipperRec::picture);
    if (static_cast<u8>(picture) == nativeB(static_cast<u16>(f + flipperRec::pictureStamped))) continue;
    nativeB(static_cast<u16>(f + flipperRec::pictureStamped)) = static_cast<u8>(picture);
    const u16 width = static_cast<u16>(w(flipperRec::width) * 2), rows = w(flipperRec::rows), from = w(flipperRec::masks), walls = S(at::wallsGround);
    u16 source = static_cast<u16>(w(flipperRec::maskStep) * picture);
    u16 to = static_cast<u16>(w(flipperRec::y) * 0x28 + (w(flipperRec::x) >> 3));
    for (u16 r = 0; r < rows; ++r, to = static_cast<u16>(to + 0x28))
      for (u16 b = 0; b < width; ++b) {
        const u8 v = farB(from, source++);
        farB(walls, static_cast<u16>(to + b)) = v;
      }
  }
}

/// cs:59aa
void Engine::afterSteps() {
  if (W(at::tiltCounter) != 0) --W(at::tiltCounter);
  bumperEvent();
  pickGravity();
  changeLayer();
  if (B(at::ballLost) == 0xff && B(at::betweenBalls) != 0xff) {
    call(F(at::tableDrained));
    lostParked_ = true;
  }
}

/// cs:5ace: a bumper or kicker threw the ball: its sound and its score.
void Engine::bumperEvent() {
  if (B(at::bumperPending) == 0) return;
  B(at::bumperPending) = 0;
  const u16 record = W(at::bumperRecord);
  addScore(A(at::score), static_cast<u16>(record + 2));
  B(at::scoredThisBall) = 0xff;
  const u16 noise = nativeW(record);
  sound->effect(nativeB(static_cast<u16>(noise + soundRec::sample)), nativeB(static_cast<u16>(noise + soundRec::note)), 0,
                static_cast<u8>(nativeB(static_cast<u16>(noise + soundRec::channel)) + 1));
  call(F(at::tableBumper));
}

/// cs:59d9: the pull on the ball where it is. The planes that are not about solidity carry,
/// for each stretch of eight dots, which of the table's slopes it lies on.
void Engine::pickGravity() {
  u16 at = static_cast<u16>((static_cast<u16>(W(at::ballX) + 8) >> 3) + (W(at::ballY) + 8) * 0x28 - 1);
  const bool ground = B(at::layer) != 0xff;
  if (amended) {
    // The original reads the slopes from copies of the masks it keeps in the video card's
    // memory, over parts of which it also keeps pictures, and looks for a stretch with nothing
    // solid in it as the masks are at that moment, a flipper's picture and all. Amended: the
    // slopes are a property of the table, the same wherever the flippers stand.
    // (a lost ball is put away wherever each table has room for it, and the next one would set
    // off with the pull of that place; amended: with the pull of the bottom of the table,
    // where it was lost)
    const int x = (lostParked_ ? 280 : W(at::ballX).s()) + 8, y = (lostParked_ ? 525 : W(at::ballY).s()) + 8;
    if (x < 0 || x >= 320 || y < 0 || y >= 0x240) return;
    const u8 slope = slopes_[ground ? 0 : 1][static_cast<std::size_t>(y * 0x28 + (x >> 3))];
    if (slope >= cs_[static_cast<u16>(F(at::slopeCount) + 2)]) return;
    W(at::gravityX) = nativeW(static_cast<u16>(koffset(high() ? 0x5a39 : 0x5a2a) + slope * 4));
    W(at::gravityY) = nativeW(static_cast<u16>(koffset(high() ? 0x5a40 : 0x5a31) + slope * 4));
    return;
  }
  for (int i = 0; i < 3; ++i, ++at) {
    u8 solid = 0, slope = 0;
    if (ground) {
      solid = farB(S(at::marksGround), at) | farB(S(at::wallsGround), at);
      slope = copiedMask(2, at) & 0x0f;
    } else {
      solid = copiedMask(0, at) | farB(S(at::wallsRamps), at);
      slope = copiedMask(1, at) & 0x0f;
    }
    if (solid != 0) continue;
    if (slope >= cs_[static_cast<u16>(F(at::slopeCount) + 2)]) return;  // more than the table has
    W(at::gravityX) = nativeW(static_cast<u16>(koffset(high() ? 0x5a39 : 0x5a2a) + slope * 4));
    W(at::gravityY) = nativeW(static_cast<u16>(koffset(high() ? 0x5a40 : 0x5a31) + slope * 4));
    return;
  }
}

/// cs:5d24: ramps have no height: the ball is on one or the other of two sets of masks, and
/// changes over inside certain rectangles. Which list is looked at depends on where it is now.
void Engine::changeLayer() {
  const u16 x = static_cast<u16>(W(at::ballX) + 8), y = static_cast<u16>(W(at::ballY) + 8 + W(at::nudgeLift));
  for (u16 e = B(at::layer) == 0xff ? A(at::transitionsDown) : A(at::transitionsUp); nativeW(e) != 0xffff; e = static_cast<u16>(e + 8)) {
    if (x < nativeW(static_cast<u16>(e + zoneRec::xMin)) || y < nativeW(static_cast<u16>(e + zoneRec::yMin)) || x > nativeW(static_cast<u16>(e + zoneRec::xMax)) || y > nativeW(static_cast<u16>(e + zoneRec::yMax)))
      continue;
    B(at::layer) = static_cast<u8>(~B(at::layer));
    return;
  }
}

/// cs:5989
void Engine::runRules() {
  rollTriggers();
  hitTriggers();
  call(F(at::tableFlipperPressed));
  if (std::exchange(nudgeDue_, false)) {
    freeTrappedBall();
    const u8 key = B(at::lastKey);
    nudgeKey();
    B(at::lastKey) = key;
  }
  if (amended) {  // (how long the ball in play has stood still, as of this frame: for freeTrappedBall)
    const i16 x = W(at::ballX).s(), y = W(at::ballY).s();
    restFrames_ = B(at::ballHidden) != 0xff && x == restX_ && y == restY_ ? restFrames_ + 1 : 0;
    restX_ = x, restY_ = y;
  }
  displayFlash();
  call(W(at::runOnce));
  W(at::runOnce) = F(at::nothing);
  runTimers();
  runBlinks();
}

/// (amended) A ball that had stood still for a second in one of the table's traps (traps_) is
/// set rolling by a shake: the shake lifts it as anywhere, and it goes off sideways as well.
/// Only a ball resting there (as of the frame before: the shake has moved it by now), not one
/// rolling through; and as a ball resting there never left before, no game that was ever
/// finished plays any differently.
void Engine::freeTrappedBall() {
  if (restFrames_ < 60) return;
  for (const Trap& t : traps_) {
    if (restX_ < t.left || restX_ > t.right || restY_ < t.top || restY_ > t.bottom) continue;
    W(at::ballVx) = static_cast<u16>(t.vx);
    restFrames_ = 0;
    return;
  }
}

/// cs:5d70: rectangles the ball rolls into, each with the routine to run when it does.
void Engine::rollTriggers() {
  const u16 x = static_cast<u16>(W(at::ballX) + 8), y = static_cast<u16>(W(at::ballY) + 8 + W(at::nudgeLift));
  const bool ramps = B(at::layer) == 0xff;
  u16 e = ramps ? A(at::rollTriggersRamps) : A(at::rollTriggersGround);
  if (B(at::tilted) == 0xff) e = ramps ? A(at::rollTriggersRampsTilted) : A(at::rollTriggersGroundTilted);
  for (; nativeW(e) != 0; e = static_cast<u16>(e + zoneRec::size)) {
    if (x < nativeW(static_cast<u16>(e + zoneRec::xMin)) || y < nativeW(static_cast<u16>(e + zoneRec::yMin)) || x > nativeW(static_cast<u16>(e + zoneRec::xMax)) || y > nativeW(static_cast<u16>(e + zoneRec::yMax)))
      continue;
    const u16 routine = nativeW(static_cast<u16>(e + zoneRec::then));
    if (W(at::inTrigger) == routine) return;  // still in the one it was in
    W(at::inTrigger) = routine;
    call(routine);
    W(at::lastTrigger) = W(at::inTrigger);
    return;
  }
  W(at::inTrigger) = 0;
}

/// cs:5cb9: rectangles in which touching something solid counts, on the playfield only.
void Engine::hitTriggers() {
  if (B(at::tilted) == 0xff) return;
  if (W(at::contactX) == 0 && W(at::contactY) == 0) return;
  const u16 x = W(at::contactX), y = static_cast<u16>(W(at::contactY) + W(at::nudgeLift));
  W(at::contactX) = 0;
  W(at::contactY) = 0;
  if (B(at::layer) == 0xff) return;
  for (u16 e = A(at::hitTriggerList); nativeW(e) != 0; e = static_cast<u16>(e + zoneRec::size)) {
    if (x < nativeW(static_cast<u16>(e + zoneRec::xMin)) || y < nativeW(static_cast<u16>(e + zoneRec::yMin)) || x > nativeW(static_cast<u16>(e + zoneRec::xMax)) || y > nativeW(static_cast<u16>(e + zoneRec::yMax)))
      continue;
    call(nativeW(static_cast<u16>(e + zoneRec::then)));
    return;
  }
}

/// The plunger (cs:5e0d while it is pulled or at rest, cs:5e6a when it is let go).
void Engine::bindPlunger() {
  auto shown = [this] {  // cs:6600, less its drawing
    const i8 v = static_cast<i8>((B(at::plungerPull) >> 1) - 3);
    B(at::plungerShown) = v < 0 ? 0 : static_cast<u8>(v);
  };
  bind(at::plungerPulling, [=, this] {
    if (B(at::plungerHeld) != 0 && B(at::plungerPull) < 0x20) {
      ++B(at::plungerPull);
      shown();
    }
  });
  bind(at::plungerLetGo, [=, this] {
    if (const u8 pull = B(at::plungerPull); pull != 0) {
      if (B(at::ballOnPlunger) != 0) {  // a ball waits on it
        i16 speed = static_cast<i16>((high() ? -166 : -138) * pull - chance(0x100, W(at::loopCounter) & 0xff));
        if (angle_ == 2) speed = static_cast<i16>(speed * 5 / 4);  // this version's steeper table
        if (B(at::loopCounter, 2) != 0xff) {
          W(at::ballVy) = static_cast<u16>(speed);
          W(at::ballVx) = 0;
        }
        W(at::spin) = chance(0x10, W(at::loopCounter) & 0x0f);
      }
      const u8 volume = static_cast<u8>((pull << 6) / 0x20);
      sound->effect(B(at::soundSpringUp), B(at::soundSpringUp + 1), volume, static_cast<u8>(B(at::soundSpringUp + 3) + 1));
      B(at::plungerPull) = 0;
      shown();
    }
    W(at::plungerRoutine) = F(at::plungerPulling);
  });
}

}  // namespace encore
