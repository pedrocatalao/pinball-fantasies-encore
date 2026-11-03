#include "engine/table/SpeedDevils.h"

namespace encore {

using namespace speed;

SpeedDevils::SpeedDevils(ByteView prg) : Flow(prg, kTableIndex) {
  bindRules();
  bindSteps();
  bindNative(routines::tableStart, [this] { lightsOut(); });
  bindNative(routines::tableNewGame, [this] {  // a new game
    lightsOut();
    stopBlinks();
    resetJackpot();
    clearBall();
    for (u16 player = 0; player < 8; ++player) savePlayer(static_cast<u16>(player * rec::size));
    b(var::extraBalls) = 0;
    b(var::matchWon) = 0;
    b(var::partyOn) = 0;
    for (u16 i = 0; i < 0x10; ++i) b(static_cast<u16>(text::matchPlayers + i)) = 0x20;
    for (u16 i = 0; i < 0x10; ++i) b(static_cast<u16>(text::matchWinners + i)) = 0x20;
  });
  bindNative(routines::tableAttractMusic, [this] {  // the music while nobody plays
    music(tunes::attract);
    b(var::musicAfter) = b(tunes::attract + tuneRec::priority);
    b(var::musicNext) = songAt::afterAttract;
  });
  bindNative(routines::tableGameMusic, [this] {  // the music a game opens with
    music(tunes::gameStart);
    b(var::musicAfter) = b(tunes::gameStart + tuneRec::priority);
    b(var::musicNext) = songAt::plunger;
  });
  bindNative(routines::tableDrained, [this] {  // the ball is lost
    b(var::betweenBalls) = kSet;
    w(var::lookAtSoft) = high() ? 0x103 : 0x171;
    placeBall(spots::drained);
    b(var::ballHidden) = kSet;
    b(var::flippersLive) = 0;
    b(var::modeRunning) = 0;
    b(var::musicPriority) = 0;
    award(awards::drained);
    addTimer(timers::drainSfx.routine);
  });
  bindNative(timers::drainSfx.routine, [this] {
    if (!countTo(timers::drainSfx.counter, 5)) return;
    sfx(sounds::ballDrained);
    endTimer();
  });
  bindNative(routines::idleWhileUntouched, [this] {
    w(var::idleFrames) = 0;
    if (b(var::ballUntouched) == 0) endTimer();
  });
  bindNative(routines::tableTilt, [this] {  // tilt
    b(var::flippersLive) = 0;
    b(var::musicNext) = songAt::afterTilt;
    music(tunes::tilt);
    startScript(scripts::tilt);
    stopBlinks();
    lightsDark();
  });
  bindNative(routines::tableTiltWarning, [this] { music(tunes::warnTilt); });
  bindNative(routines::tableFlipperPressed, [this] {  // a flipper pressed: each row of three lights, BUR, NIN and PIT, moves along one
    if (b(var::flipperPressed) != kSet) return;
    b(var::flipperPressed) = 0;
    auto rotate = [this](u16 flags) {
      const u8 first = b(flags);
      b(flags) = b(static_cast<u16>(flags + 1));
      b(static_cast<u16>(flags + 1)) = b(static_cast<u16>(flags + 2));
      b(static_cast<u16>(flags + 2)) = first;
    };
    rotate(var::lightStates + lights::bur);
    row(var::lightStates + lights::bur, 3, lights::bur);
    b(var::blinkBur) = b(var::blinkBur + 1) = b(var::blinkBur + 2) = 0;
    rotate(var::lightStates + lights::nin);
    row(var::lightStates + lights::nin, 3, lights::nin);
    b(var::blinkNin) = b(var::blinkNin + 1) = b(var::blinkNin + 2) = 0;
    if (w(var::timeoutPitAll) != 0) return;
    w(var::timeoutPit1) = 0;
    w(var::timeoutPit2) = 0;
    w(var::timeoutPit0) = 0;
    rotate(var::lightStates + lights::pit);
    row(var::lightStates + lights::pit, 3, lights::pit);
  });
  bindNative(routines::tableFrame, [this] { everyFrame(); });
  bindNative(routines::tableBumper, [this] {  // after a bumper's score
    modeCountHit();
    w(var::idleFrames) = 0;
    if (w(var::idleShowing) == kSet) {
      startScript(scripts::showScore);
      w(var::idleShowing) = 0;
    }
  });
  bindNative(routines::tableModeSecond, [this] {  // each second of a count-down: at nought the mode ends
    if (b(var::countdownTens) != 0x2a || b(var::countdownUnits) != 0x37) return;
    for (u16 mode : {var::modeRamp, var::modeHit})
      if (b(mode) == kSet) {
        b(var::musicRepeats) = 1;
        music(mode == var::modeRamp ? tunes::modeOverRamp : tunes::modeOverHit);
        b(var::musicNext) = songAt::afterMode;
      }
    b(var::modeHit) = 0;
    b(var::modeRamp) = 0;
    b(var::musicPriority) = 0;
    b(var::musicNext) = songAt::afterMode;
    b(var::musicRepeats) = 1;
  });
}

void SpeedDevils::row(u16 flags, u16 count, u8 firstLight) {
  for (u16 i = 0; i < count; ++i) {
    const u8 light = static_cast<u8>(firstLight + i);
    stopBlink(light);
    if (b(static_cast<u16>(flags + i)) == kSet) lightOn(light);
    else lightOff(light);
  }
}

void SpeedDevils::all(std::initializer_list<u8> which, void (SpeedDevils::*what)(u8)) {
  for (u8 light : which) (this->*what)(light);
}

void SpeedDevils::scored() {
  b(var::scoredThisBall) = kSet;
  w(var::idleFrames) = 0;
  if (w(var::idleShowing) == kSet) {
    startScript(scripts::showScore);
    w(var::idleShowing) = 0;
  }
}

/// The original has a number of times to add a bonus here that this table never sets, so it
/// is always once, and the bonus is multiplied only when it is counted. Amended: here too it
/// is added as many times as the multiplier now stands at.
void SpeedDevils::addBonus(u16 amount) {
  u16 times = amended ? b(var::bonusMultiplier) : w(var::bonusTimes);
  do addScore(var::bonus, amount);
  while (times-- > 1);
}

void SpeedDevils::hold() {
  b(var::ballHidden) = kSet;
  placeBall(spots::pit);
}

void SpeedDevils::sameBallAgain() {
  --b(var::extraBalls);
  w(var::idleFrames) = 0;
  for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(var::scoreDrawn + i)) = kScoreUndrawn;
  w(var::scoreDrawnGroups) = 0;
  bx = scripts::shootAgain;
  call(nativeW(bx));
}

void SpeedDevils::resetJackpot() {
  for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(var::jackpot + i)) = b(static_cast<u16>(var::jackpotStart + i));
}

void SpeedDevils::clearBall() {
  b(text::multiplier) = 0x38;
  b(text::ballA) = 0x38;
  b(text::playerA) = 0x38;
  for (u16 at : {var::score, var::bonus, var::modeHitScore, var::modeRampScore})
    for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(at + i)) = 0;
  b(var::lightPhasePlace) = 0;
  w(var::carMods) = 0;
  for (u16 a = var::blinkBur; a <= var::blinkNin + 2; ++a) b(a) = 0;  // and blinkNin
  for (u16 a : {var::timeoutPit2, var::timeoutPit1, var::timeoutPit0, var::timeoutGearBlink, var::timeoutPitAll}) w(a) = 0;
  b(var::mbPending) = 0;
  b(var::mbActive) = 0;
  for (u16 a : {var::timeoutMilesLeft, var::timeoutMilesRight, var::pedalMetal0, var::pedalMetal1, var::pedalMetalJump}) w(a) = 0;
  b(var::layer) = layers::playfield;
  b(var::holdBonus) = 0;
  b(var::keptUnread) = 0;
  b(var::bonusMultiplier) = 1;
  for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(var::milesDigits + i)) = 0;
  w(var::miles) = 0;
  w(var::jumpAt) = 0x1e;
  w(var::offroadAt) = 0x28;
  for (u16 a : {var::curSpeed, var::curGear, var::maxPlace, var::curPlace}) w(a) = 0;
  b(var::modeRunning) = 0;
  b(var::modeRamp) = 0;
  b(var::modeHit) = 0;
}

namespace {
struct Kept {
  u16 inRecord, at, bytes;
};
constexpr u16 kept(u16 field) { return static_cast<u16>(var::playerRecords + field); }
/// What a player's record holds, and where each piece lives while that player plays
/// (cs:0f9e). The last two are only ever brought back, not kept (cs:0cbc).
constexpr Kept kKept[] = {
    {kept(rec::score), var::score, 12},
    {kept(rec::bonus), var::bonus, 12},
    {kept(rec::milesDigits), var::milesDigits, 12},
    {kept(rec::miles), var::miles, 2},
    {kept(rec::jumpAt), var::jumpAt, 2},
    {kept(rec::offroadAt), var::offroadAt, 2},
    {kept(rec::curSpeed), var::curSpeed, 2},
    {kept(rec::curGear), var::curGear, 2},
    {kept(rec::maxPlace), var::maxPlace, 2},
    {kept(rec::curPlace), var::curPlace, 2},
    {kept(rec::pitStopGoal), var::lightStates + lights::pitStopGoal, 1},
    {kept(rec::gear), var::lightStates + lights::gear, 4},
    {kept(rec::carPartLit), var::lightStates + lights::carPartLit, 5},
    {kept(rec::carPart), var::lightStates + lights::carPart, 5},
    {kept(rec::keptUnread), var::keptUnread, 1},
    {kept(rec::carMods), var::carMods, 2},
};
constexpr Kept kBroughtBackOnly[] = {{kept(rec::modeHitScore), var::modeHitScore, 12}, {kept(rec::modeRampScore), var::modeRampScore, 12}};
}  // namespace

void SpeedDevils::savePlayer(u16 player) {
  for (const Kept& k : kKept)
    for (u16 i = 0; i < k.bytes; ++i) b(static_cast<u16>(player + k.inRecord + i)) = b(static_cast<u16>(k.at + i));
}

void SpeedDevils::restorePlayer() {
  const u16 player = static_cast<u16>((b(var::player) - 1) * rec::size);
  for (const Kept& k : kKept)
    for (u16 i = 0; i < k.bytes; ++i) b(static_cast<u16>(k.at + i)) = b(static_cast<u16>(player + k.inRecord + i));
  for (const Kept& k : kBroughtBackOnly)
    for (u16 i = 0; i < k.bytes; ++i) b(static_cast<u16>(k.at + i)) = b(static_cast<u16>(player + k.inRecord + i));
  // and the lights are put as that says
  if (lightState(lights::pitStopGoal) != 0) {
    blink(lights::pitStopGoal, later(b(var::lightPhasePlace), 0x0f), 0x0f);
    setLight(lights::pitStopGoal);
  }
  for (u16 i = 0; i < w(var::curGear); ++i) setLight(static_cast<u8>(lights::gearNum + i));
  // (amended: the four lights towards the next gear are not carried from ball to ball; each
  // ball earns its own)
  if (amended) for (u16 a = var::lightStates + lights::gear; a <= var::lightStates + lights::gear + 3; ++a) b(a) = 0;
  row(var::lightStates + lights::gear, 4, lights::gear);
  for (u16 i = 0; i < 5; ++i) {
    const u8 light = static_cast<u8>(lights::carPart + i);
    if (b(static_cast<u16>(var::lightStates + lights::carPart + i)) != kSet) continue;
    blink(light, (light & 1) ? later(b(var::lightPhasePlace), 0x0f) : b(var::lightPhasePlace), 0x0f);
  }
  for (u16 i = 0; i < 5; ++i)
    if (b(static_cast<u16>(var::lightStates + lights::carPartLit + i)) == kSet) blink(static_cast<u8>(lights::carPartLit + i), 0, 0x0f);
  if (lightState(lights::pitStopGoal) == kSet) lightOn(lights::pitStopGoal);
  for (u16 i = 0; i < w(var::curPlace); ++i) b(static_cast<u16>(var::lightStates + lights::place + i)) = kSet;
  row(var::lightStates + lights::place, 0x0a, lights::place);
  for (u16 n = static_cast<u16>(w(var::maxPlace) - w(var::curPlace)), light = static_cast<u16>(w(var::curPlace) + lights::place); n > 0; --n, ++light)
    blink(static_cast<u8>(light), (light & 1) ? later(b(var::lightPhasePlace), 0x0f) : b(var::lightPhasePlace), 0x0f);
  if (u16 n = w(var::curSpeed); n != 0) {
    if (n >= 0x0c) n = 0x0c;
    for (u16 i = 0; i < n; ++i) b(static_cast<u16>(var::lightStates + lights::speed + i)) = kSet;
    row(var::lightStates + lights::speed, 0x0c, lights::speed);
  }
}

bool SpeedDevils::hitBur() {
  modeCountHit();
  addScore(var::score, points::k7510);
  scored();
  addBonus(points::k550);
  sfx(sounds::hitTarget);
  if (lightState(lights::bur) != kSet || lightState(lights::bur + 1) != kSet || lightState(lights::bur + 2) != kSet) return false;
  incJackpot();
  return true;
}

bool SpeedDevils::hitNin() {
  modeCountHit();
  // (the original gives this row ten points and twenty of bonus more than the other;
  // amended: the two rows score alike)
  addScore(var::score, amended ? points::k7510 : points::k7520);
  scored();
  addBonus(amended ? points::k550 : points::k570);
  sfx(sounds::hitTarget);
  if (lightState(lights::nin) != kSet || lightState(lights::nin + 1) != kSet || lightState(lights::nin + 2) != kSet) return false;
  incJackpot();
  return true;
}

/// cs:1638: all four GEAR lights: two more places lit in the race, the next gear, and they
/// flash. True if they were all lit.
bool SpeedDevils::gear() {
  for (u16 a = var::lightStates + lights::gear; a <= var::lightStates + lights::gear + 3; ++a)
    if (b(a) != kSet) return false;
  addPlaces();
  nextGear();
  for (u16 a = var::lightStates + lights::gear; a <= var::lightStates + lights::gear + 3; ++a) b(a) = 0;
  for (u8 light = lights::gear; light <= lights::gear + 3; ++light) blink(light, 0, 2);
  addTimer(timers::unblinkGearAll.routine);
  award(awards::gear);
  return true;
}

void SpeedDevils::nextGear() {
  const u16 n = w(var::curGear);
  if (n == 5) {
    w(var::timeoutGearBlink) = 0x1e;
    for (u8 light = lights::gearNum; light <= lights::gearNum + 5; ++light) blink(light, 0, 1);
    return;
  }
  ++w(var::curGear);
  setLight(static_cast<u8>(n + lights::gearNum));
}

void SpeedDevils::gearBlinkOver() {
  w(var::curGear) = 0;
  for (u8 light = lights::gearNum; light <= lights::gearNum + 5; ++light) {
    stopBlink(light);
    clearLight(light);
  }
  if (lightState(lights::pitStopHoldBonus) == kSet) return;
  blink(lights::pitStopHoldBonus, b(var::lightPhasePlace), 0x0f);
  b(var::keptUnread) = 0;
  lightState(lights::pitStopHoldBonus) = kSet;
}

void SpeedDevils::addPlaces() {
  const u16 n = w(var::maxPlace);
  if (n == 0x0a) {
    award(awards::extraGear);
    return;
  }
  blink(static_cast<u8>(n + lights::place), b(var::lightPhasePlace), 0x0f);
  blink(static_cast<u8>(n + lights::place + 1), later(b(var::lightPhasePlace), 0x0f), 0x0f);
  w(var::maxPlace) += 2;
}

void SpeedDevils::gainPlace() {
  const u16 n = w(var::curPlace);
  if (w(var::maxPlace) <= n) return;
  ++w(var::curPlace);
  stopBlink(static_cast<u8>(n + lights::place));
  setLight(static_cast<u8>(n + lights::place));
  award(awards::overtake);
  if (w(var::curPlace) != 0x0a) return;
  blink(lights::pitStopGoal, later(b(var::lightPhasePlace), 0x0f), 0x0f);
  setLight(lights::pitStopGoal);
  blink(lights::miniRampJackpot, b(var::lightPhasePlace), 0x0f);
  setLight(lights::miniRampJackpot);
  w(var::timeoutJackpot) = 0x4b0;
  award(awards::overtakeFinal);
}

void SpeedDevils::goal() {
  for (u8 light = lights::place + 9; light >= lights::place; --light) clearLight(light);
  w(var::curPlace) = 0;
  w(var::maxPlace) = 0;
  if (b(var::modeRunning) == kSet) addTimer(timers::turbo);
  else doTurbo();
}

void SpeedDevils::doTurbo() {
  award(awards::turbo);
  b(var::modeRunning) = kSet;
  b(var::modeRamp) = kSet;
}

void SpeedDevils::offroad() {
  if (b(var::modeRunning) == kSet) addTimer(timers::offroad);
  else doOffroad();
}

void SpeedDevils::doOffroad() {
  b(var::musicPriority) = 0;
  music(tunes::modeHit);
  b(var::modeRunning) = kSet;
  startScript(scripts::modeHit);
  b(var::modeHit) = kSet;
}

void SpeedDevils::carModsAll() {
  for (u16 light = lights::carPart; light < lights::carPart + 5; ++light)
    if (b(static_cast<u16>(var::lightStates + light)) != kSet) return;
  for (u8 light = lights::carPart; light <= lights::carPart + 4; ++light) clearLight(light);
  for (u8 light = lights::carPart; light <= lights::carPart + 4; ++light) stopBlink(light);
  for (u8 light = lights::carPart; light <= lights::carPart + 4; ++light) blink(light, 0, 1);
  addTimer(timers::unblinkCar.routine);
  w(var::carMods) = 0;
}

bool SpeedDevils::rollPit() {
  modeCountHit();
  sfx(sounds::hitTarget);
  award(awards::pit);
  for (u16 light = lights::pit; light < lights::pit + 3; ++light)
    if (b(static_cast<u16>(var::lightStates + light)) != kSet) return false;
  if (w(var::timeoutPitAll) != 0) return false;
  award(awards::pitAll);
  if (static_cast<u8>(b(var::mbActive) + b(var::mbPending)) >= 8) {
    award(awards::million);
  } else if (++b(var::mbPending) == 1) {
    lightState(lights::offroadMultiBonus) = kSet;
    blink(lights::offroadMultiBonus, 0, 0x0a);
  }
  for (u8 light = lights::pit; light <= lights::pit + 2; ++light) blink(light, 0, 2);
  w(var::timeoutPitAll) = 0x28;
  return true;
}

void SpeedDevils::pedalMetal() {
  if (w(var::pedalMetal0) != kSet || w(var::pedalMetal1) != kSet || w(var::pedalMetalJump) != kSet) return;
  award(awards::pedalMetal);
  w(var::pedalMetal0) = 0;
  w(var::pedalMetal1) = 0;
  w(var::pedalMetalJump) = 0;
  const u8 light = static_cast<u8>(w(var::curSpeed) + lights::speed);
  if (light & 1) lightCarPart();
  ++w(var::curSpeed);
  if (light < lights::speed + 11) setLight(light);
  else lightOn(lights::speed + 11);
}

void SpeedDevils::lightCarPart() {
  if (w(var::carMods) == 5) return;
  const u16 n = w(var::carMods);
  ++w(var::carMods);
  const u8 light = b(static_cast<u16>(carParts::table + n * carParts::size + carParts::light));
  blink(light, b(var::lightPhasePlace), 0x0f);
  setLight(light);
}

/// cs:226b: one miles ramp soon after the other: a place taken in the race, and towards the
/// pedal to the metal.
void SpeedDevils::overtake() {
  gainPlace();
  award(awards::million);
  w(var::pedalMetal0) = kSet;
  w(var::pedalMetal1) = kSet;
}

void SpeedDevils::threeDigits(u16 value, u16 at, bool blankHundreds) {
  for (int i = 2; i >= 0; --i) {
    u16 digit = value % 10;
    value /= 10;
    if (blankHundreds && digit == 0 && i == 0) digit = (static_cast<u16>(0x2a - 0x37));
    b(static_cast<u16>(at + i)) = static_cast<u8>(digit + 0x37);
  }
}

/// cs:2281: a trip up a miles ramp. The miles are counted, with awards at ten, twenty and at
/// the two counts that move up twenty at a time, and the display's lines about them are made up.
void SpeedDevils::bumpMiles() {
  incJackpot();
  award(static_cast<u16>(awards::miles + (w(var::curSpeed) > 0x0b ? 0x0b : w(var::curSpeed)) * awards::milesSize));
  if (b(var::modeRamp) == kSet) {
    award(awards::turboRamp);
    modeCountRamp();
  }
  do {
    ++w(var::miles);
    addScore(var::milesDigits, points::k1);
  } while (w(var::miles) == 1);
  const u16 miles = w(var::miles);
  if (miles <= 0x14) {
    if (miles > 0x0a) {
      award(awards::milesToExtraBall);
      if (miles == 0x14) {
        blink(lights::pitLoopExtraBall, 0, 0x0f);
        setLight(lights::pitLoopExtraBall);
        award(awards::milesExtraBall);
      }
    }
    if (miles == 0x0a) offroad();
    else if (miles < 0x0a) award(awards::milesToFirstOffroad);
  }
  if (miles == w(var::jumpAt)) {
    w(var::jumpAt) += 0x14;
    if (lightState(lights::miniRampJump) != kSet) {
      setLight(lights::miniRampJump);
      blink(lights::miniRampJump, 0, 0x0f);
      award(awards::milesJump);
    }
  } else if (static_cast<u16>(w(var::jumpAt) - miles) < 0x0a) {
    threeDigits(w(var::jumpAt), text::milesToJump, true);
    award(awards::milesToJump);
  }
  if (static_cast<u16>(w(var::offroadAt) - w(var::miles)) <= 0x0a) {
    if (w(var::miles) == w(var::offroadAt)) {
      w(var::offroadAt) += 0x14;
      offroad();
    }
    threeDigits(w(var::offroadAt), text::milesToOffroad, true);
    award(awards::milesToOffroad);
  }
  threeDigits(w(var::miles), text::miles, false);
  for (u16 at = text::miles; b(at) == 0x37; ++at) b(at) = 0x2a;  // no noughts in front
}

void SpeedDevils::modeCountHit() {
  if (b(var::modeHit) != kSet) return;
  addScore(var::modeHitScore, points::k100000b);
  b(var::modeScored) = kSet;
}

void SpeedDevils::modeCountRamp() {
  if (b(var::modeRamp) != kSet) return;
  addScore(var::modeRampScore, points::k5000000);
  b(var::modeScored) = kSet;
}

void SpeedDevils::everyFrame() {
  if (++b(var::lightPhasePlace) == 0x1e) b(var::lightPhasePlace) = 0;
  struct Count { u16 at; void (SpeedDevils::*then)(); };
  auto lane = [this](u8 light) {
    stopBlink(light);
    lightOn(light);
    row(var::lightStates + lights::pit, 3, lights::pit);
  };
  auto down = [this](u16 at) { return w(at) != 0 && --w(at) == 0; };
  if (down(var::timeoutPit2)) lane(lights::pit + 2);
  if (down(var::timeoutPit1)) lane(lights::pit + 1);
  if (down(var::timeoutPit0)) lane(lights::pit);
  if (down(var::timeoutPitAll)) {  // cs:20d7
    for (u8 light = lights::pit; light <= lights::pit + 2; ++light) stopBlink(light);
    for (u8 light = lights::pit; light <= lights::pit + 2; ++light) clearLight(light);
    row(var::lightStates + lights::pit, 3, lights::pit);
  }
  down(var::timeoutMilesRight);
  down(var::timeoutMilesLeft);
  if (down(var::timeoutGearBlink)) gearBlinkOver();
  if (down(var::timeoutJackpot)) {  // cs:185f
    clearLight(lights::miniRampJackpot);
    stopBlink(lights::miniRampJackpot);
  }
}

void SpeedDevils::bindRules() {
  auto after = [this](u16 at, u16 counter, u16 limit, std::function<void()> then) {
    bindNative(at, [this, counter, limit, then] {
      if (countTo(counter, limit)) then();
    });
  };

  bindNative(routines::plungerExit, [this] {  // out of the lane on to the table
    w(var::ballVy) = 0;
    b(var::layer) = layers::playfield;
  });

  // the two rows of three targets, BUR and NIN
  struct Target { u16 hit, busy, lit; u8 light; u16 timer, counter; bool bur; };
  for (const Target t : {Target{routines::bur0, var::blinkBur, var::lightStates + lights::bur, lights::bur, timers::unblinkBur0.routine, timers::unblinkBur0.counter, true},
                         {routines::bur1, var::blinkBur + 1, var::lightStates + lights::bur + 1, lights::bur + 1, timers::unblinkBur1.routine, timers::unblinkBur1.counter, true},
                         {routines::bur2, var::blinkBur + 2, var::lightStates + lights::bur + 2, lights::bur + 2, timers::unblinkBur2.routine, timers::unblinkBur2.counter, true},
                         {routines::nin0, var::blinkNin, var::lightStates + lights::nin, lights::nin, timers::unblinkNin0.routine, timers::unblinkNin0.counter, false},
                         {routines::nin1, var::blinkNin + 1, var::lightStates + lights::nin + 1, lights::nin + 1, timers::unblinkNin1.routine, timers::unblinkNin1.counter, false},
                         {routines::nin2, var::blinkNin + 2, var::lightStates + lights::nin + 2, lights::nin + 2, timers::unblinkNin2.routine, timers::unblinkNin2.counter, false}}) {
    bindNative(t.hit, [this, t] {
      if (b(t.busy) == kSet) return;
      b(t.busy) = kSet;
      b(t.lit) = kSet;
      if (t.bur ? hitBur() : hitNin()) {  // all three: they flash, and a light of the four GEAR comes on
        const u8 first = t.bur ? lights::bur : lights::nin;
        const u16 busy = t.bur ? var::blinkBur : var::blinkNin;
        for (u16 i = 0; i < 3; ++i) b(static_cast<u16>(busy + i)) = kSet;
        for (u8 i = 0; i < 3; ++i) blink(static_cast<u8>(first + i), 0, 1);
        b(t.bur ? var::lightStates + lights::gear + 2 : var::lightStates + lights::gear + 3) = kSet;
        addTimer(t.bur ? timers::unblinkBurAll.routine : timers::unblinkNinAll.routine);
        if (!gear()) blink(t.bur ? lights::gear + 2 : lights::gear + 3, 0, 1);
        return;
      }
      blink(t.light, 0, 1);
      addTimer(t.timer);
    });
    after(t.timer, t.counter, 0x0a, [this, t] {  // unblinks the target's light
      stopBlink(t.light);
      if (b(t.busy) == kSet) {
        lightOn(t.light);
        b(t.busy) = 0;
      }
      endTimer();
    });
  }
  struct Rows { u16 timer, counter; u8 light, first; u16 lit, busy; };
  for (const Rows r : {Rows{timers::unblinkBurAll.routine, timers::unblinkBurAll.counter, lights::gear + 2, lights::bur, var::lightStates + lights::bur, var::blinkBur},
                       {timers::unblinkNinAll.routine, timers::unblinkNinAll.counter, lights::gear + 3, lights::nin, var::lightStates + lights::nin, var::blinkNin}}) {
    after(r.timer, r.counter, 0x28, [this, r] {
      stopBlink(r.light);
      lightOn(r.light);
      for (u16 i = 0; i < 3; ++i) b(static_cast<u16>(r.lit + i)) = 0;
      for (u8 i = 0; i < 3; ++i) stopBlink(static_cast<u8>(r.first + i));
      for (u8 i = 0; i < 3; ++i) lightOff(static_cast<u8>(r.first + i));
      for (u16 i = 0; i < 3; ++i) b(static_cast<u16>(r.busy + i)) = 0;
      endTimer();
    });
  }
  after(timers::unblinkGearAll.routine, timers::unblinkGearAll.counter, 0x2d, [this] {
    for (u8 light = lights::gear; light <= lights::gear + 3; ++light) stopBlink(light);
    for (u8 light : {u8{lights::gear + 2}, u8{lights::gear + 3}, lights::gear, u8{lights::gear + 1}}) lightOff(light);
    endTimer();
  });
  bindNative(timers::turbo, [this] {
    if (b(var::betweenBalls) != kSet) {
      if (b(var::modeRunning) == kSet) return;
      doTurbo();
    }
    endTimer();
  });
  bindNative(timers::offroad, [this] {
    if (b(var::betweenBalls) != kSet) {
      if (b(var::modeRunning) == kSet) return;
      doOffroad();
    }
    endTimer();
  });

  bindNative(routines::pitStopTrigger, [this] {  // the pit
    bool pitStop = false;
    if (lightState(lights::pitStopSuperJackpot) == kSet) {
      stopBlink(lights::pitStopSuperJackpot);
      clearLight(lights::pitStopSuperJackpot);
      if (lightState(lights::pitStopGoal) == kSet) {
        award(awards::superJackpotGoal);
        pitStop = true;
      } else {
        b(var::countdownHeld) = kSet;
        b(var::musicPriority) = 0;
        awardAlways(awards::superJackpot);
      }
    }
    if (!pitStop) {
      if (lightState(lights::pitStopGoal) == kSet) {
        stopBlink(lights::pitStopGoal);
        clearLight(lights::pitStopGoal);
        goal();
      }
      if (lightState(lights::pitStopHoldBonus) == kSet) {
        award(awards::holdBonus);
        stopBlink(lights::pitStopHoldBonus);
        clearLight(lights::pitStopHoldBonus);
        b(var::holdBonus) = kSet;
      }
    }
    // The original keeps the ball a third of a second (two and a half for the greatest
    // prize) and then a second more, unless a mode's clock is running. Amended: a third of a
    // second, or a second and a third while a mode's clock runs; two and a half for the prize.
    pitWait_ = pitStop ? 0x96 : b(var::modeRunning) == kSet ? 0x50 : 0x14;
    if (b(var::pitStopHeld) != kSet) addTimer(pitStop ? timers::pitStopLong.routine : timers::pitStop.routine);  // (nothing sets it)
  });
  bindNative(timers::pitStop.routine, [this] {
    hold();
    if (!countTo(timers::pitStop.counter, amended ? pitWait_ : 0x14)) return;
    amended ? addTimerNow(timers::pitEject.routine) : addTimer(timers::pitEject.routine);
    endTimer();
  });
  bindNative(timers::pitStopLong.routine, [this] {
    hold();
    if (!countTo(timers::pitStopLong.counter, 0x96)) return;
    amended ? addTimerNow(timers::pitEject.routine) : addTimer(timers::pitEject.routine);
    endTimer();
  });
  bindNative(timers::pitEject.routine, [this] {
    hold();
    if (!amended && b(var::modeRunning) != kSet && !countTo(timers::pitEject.counter, 0x3c)) return;
    sfx(sounds::ejectPit);
    hold();
    b(var::ballHidden) = 0;
    w(var::ballVy) = 0x320;
    w(var::ballVx) = 0xf7cc;
    addTimer(timers::pitDone.routine);
    endTimer();
  });
  after(timers::pitDone.routine, timers::pitDone.counter, 0x3c, [this] {
    b(var::pitStopHeld) = 0;
    endTimer();
  });
  after(timers::unblinkCar.routine, timers::unblinkCar.counter, 0x78, [this] {
    for (u8 light = lights::carPart; light <= lights::carPart + 4; ++light) stopBlink(light);
    endTimer();
  });

  // a car part's light taken: its light in the row of five blinks
  auto carMod = [this](u16 lit, u8 light, u16 record, u16 flag, u8 rowLight, bool outOfStep) {
    if (b(lit) != kSet) return;
    clearLight(light);
    stopBlink(light);
    award(record);
    b(flag) = kSet;
    blink(rowLight, outOfStep ? later(b(var::lightPhasePlace), 0x0f) : b(var::lightPhasePlace), 0x0f);
    carModsAll();
  };
  bindNative(routines::rampOffroad, [=, this] {  // the off road ramp
    if (b(var::modeRamp) == kSet) {
      award(awards::turboRamp);
      modeCountRamp();
    }
    award(awards::rampOffroad);
    carMod(var::lightStates + lights::carPartLit0, lights::carPartLit0, awards::car0, var::lightStates + lights::carPart0, lights::carPart0, true);
    carMod(var::lightStates + lights::carPartLit3, lights::carPartLit3, awards::car3, var::lightStates + lights::carPart3, lights::carPart3, false);
    if (lightState(lights::offroadMultiBonus) == kSet) {  // cs:1cd0
      ++b(text::multiplier);
      ++b(var::mbActive);
      ++b(var::bonusMultiplier);
      if (--b(var::mbPending) == 0) {
        clearLight(lights::offroadMultiBonus);
        stopBlink(lights::offroadMultiBonus);
      }
      u16 n = 0, record = awards::mb;
      while (n < 7 && b(static_cast<u16>(var::lightStates + lights::bonus + n)) != 0) {
        ++n;
        record = static_cast<u16>(record + awards::mbSize);
      }
      award(record);
      b(static_cast<u16>(var::lightStates + lights::bonus + n)) = kSet;
      lightOn(static_cast<u8>(n + lights::bonus));
    }
    lightState(lights::gear + 1) = kSet;
    incJackpot();
    if (gear()) return;
    blink(lights::gear + 1, 0, 1);
    addTimer(timers::unblinkGear1.routine);
  });
  after(timers::unblinkGear1.routine, timers::unblinkGear1.counter, 0x0a, [this] {
    stopBlink(lights::gear + 1);
    lightOn(lights::gear + 1);
    endTimer();
  });
  bindNative(routines::pitLoopJump, [=, this] {  // the pit loop, or the jump
    if (w(var::lastTrigger) == routines::pitLoopPre) {
      carMod(var::lightStates + lights::carPartLit2, lights::carPartLit2, awards::car2, var::lightStates + lights::carPart2, lights::carPart2, true);
      carMod(var::lightStates + lights::carPartLit4, lights::carPartLit4, awards::car4, var::lightStates + lights::carPart4, lights::carPart4, false);
      if (lightState(lights::pitLoopExtraBall) == kSet) {
        award(awards::extraBall);
        clearLight(lights::pitLoopExtraBall);
        stopBlink(lights::pitLoopExtraBall);
        setLight(lights::extraBall);
        ++b(var::extraBalls);
      }
      return;
    }
    if (w(var::lastTrigger) != routines::jumpPre) return;
    if (b(var::modeRamp) == kSet) {
      award(awards::turboRamp);
      modeCountRamp();
    }
    incJackpot();
    if (lightState(lights::miniRampJackpot) == kSet) {
      addScore(var::score, var::jackpot);
      resetJackpot();
      if (b(var::modeRamp) == kSet) {
        b(var::musicPriority) = 0;
        awardAlways(awards::jackpot);
        b(var::countdownHeld) = kSet;
      } else {
        award(awards::jackpot);
      }
      clearLight(lights::miniRampJackpot);
      stopBlink(lights::miniRampJackpot);
      setLight(lights::pitStopSuperJackpot);
      blink(lights::pitStopSuperJackpot, b(var::lightPhasePlace), 0x0f);
      addTimer(timers::resetSuperJackpot.routine);
    }
    if (lightState(lights::miniRampJump) == kSet) {
      award(awards::jump);
      clearLight(lights::miniRampJump);
      stopBlink(lights::miniRampJump);
    }
    carMod(var::lightStates + lights::carPartLit1, lights::carPartLit1, awards::car1, var::lightStates + lights::carPart1, lights::carPart1, false);
    w(var::pedalMetalJump) = kSet;
    pedalMetal();
    lightState(lights::gear) = kSet;
    incJackpot();
    if (gear()) return;
    blink(lights::gear, 0, 1);
    addTimer(timers::unblinkGear0.routine);
  });
  after(timers::resetSuperJackpot.routine, timers::resetSuperJackpot.counter, 0x4b0, [this] {
    stopBlink(lights::pitStopSuperJackpot);
    clearLight(lights::pitStopSuperJackpot);
    endTimer();
  });
  bindNative(routines::pitLoopPre, [] {});
  bindNative(routines::jumpPre, [] {});
  after(timers::unblinkGear0.routine, timers::unblinkGear0.counter, 0x0a, [this] {
    stopBlink(lights::gear);
    lightOn(lights::gear);
    endTimer();
  });

  // the three PIT lanes at the bottom
  struct Bottom { u16 roll, wait, lit; u8 light; bool lightFirst; };
  for (const Bottom l : {Bottom{routines::pit2, var::timeoutPit2, var::lightStates + lights::pit + 2, lights::pit + 2, false},
                         {routines::pit1, var::timeoutPit1, var::lightStates + lights::pit + 1, lights::pit + 1, true},
                         {routines::pit0, var::timeoutPit0, var::lightStates + lights::pit, lights::pit, true}}) {
    bindNative(l.roll, [this, l] {
      if (w(l.wait) != 0) return;
      if (l.lightFirst) lightOn(l.light);
      b(l.lit) = kSet;
      if (rollPit()) return;
      w(l.wait) = 0x14;
      blink(l.light, 0, 1);
    });
  }
  bindNative(routines::laneOuter, [this] { award(awards::laneOuter); });
  bindNative(routines::laneInner, [this] {
    sfx(sounds::inlane);
    award(awards::laneInner);
  });
  bindNative(routines::offroadExit, [this] { award(awards::offroadExit); });
  bindNative(routines::enter, [this] {  // the ball came back down the lane: it is given again
    if (w(var::lastTrigger) != routines::plungerExit) return;
    b(var::playersMayJoin) = 0;
    music(tunes::main);
    b(var::musicNext) = songAt::afterMain;
    startScript(scripts::main);
    b(var::ballUntouched) = 0;
    b(var::specialPlungerEvent) = 0;
    b(var::ballOnPlunger) = 0;
  });
  bindNative(routines::plungerGo, [this] { b(var::ballOnPlunger) = 0; });
  bindNative(routines::plungerBottom, [this] { b(var::ballOnPlunger) = kSet; });
  bindNative(routines::rampMilesRight, [this] {  // the right miles ramp
    w(var::timeoutMilesRight) = 0x186;
    if (w(var::timeoutMilesLeft) != 0) {
      w(var::timeoutMilesLeft) = 0;
      overtake();
    }
    bumpMiles();
  });
  bindNative(routines::rampMilesLeft, [this] {  // the left
    w(var::timeoutMilesLeft) = 0x186;
    if (w(var::timeoutMilesRight) != 0) {
      w(var::timeoutMilesRight) = 0;
      overtake();
    }
    bumpMiles();
  });
}

/// The steps Speed Devils adds to the display's scripts.
void SpeedDevils::bindSteps() {
  auto arg = [this](u16 n) { return nativeW(static_cast<u16>(bx + n)); };
  auto wait1 = [this] {
    w(var::taskState) = 1;
    w(var::task) = code::waitTask;
  };
  bindNative(routines::startTurbo, [this] {  // turbo begins
    stopBlink(lights::pitStopGoal);
    clearLight(lights::pitStopGoal);
    goal();
    goTo(scripts::modeRamp);
  });
  bindNative(routines::checkTurboCont, [this] {  // turbo's count-down goes on, if a jackpot held it
    if (b(var::countdownHeld) != kSet) return nextStep(4);
    b(var::countdownHeld) = 0;
    bx = scripts::modeRampContinue;
    w(var::nextStep) = bx;
    b(var::modeRamp) = kSet;
    call(nativeW(bx));
  });
  bindNative(routines::clearFlagMode, [=, this] {  // the mode is over
    wait1();
    b(var::modeRunning) = 0;
    b(var::musicPriority) = 0;
    nextStep(4);
  });
  bindNative(routines::match, [this] {  // the match begins (and its pace is set a first time)
    nativeCW(cvar::matchPace) = high() ? 0x0d : 0x0b;
    call(F(at::match));
  });
  bindNative(routines::setupPartyOn, [=, this] {
    b(text::partyOnPlayer) = static_cast<u8>(b(var::player) + 0x37);
    wait1();
    nextStep(4);
  });
  bindNative(routines::setupShootAgain, [=, this] {
    b(text::shootAgainPlayer) = static_cast<u8>(b(var::player) + 0x37);
    wait1();
    nextStep(4);
  });
  bindNative(routines::bell, [this] { nextStep(4); });  // (the original rings the PC's bell here)
  bindNative(code::nothingTask, [] {});
  bindNative(routines::setSpecialPlungerEvent, [this] {
    b(var::specialPlungerEvent) = kSet;
    w(var::taskState) = 1;
    w(var::task) = code::nothingTask;
    nextStep(4);
  });
  bindNative(routines::noop, [=, this] {
    wait1();
    nextStep(4);
  });
  bindNative(routines::waitWhileGameStarting, [this] {
    w(var::task) = code::gameStartingTask;
    w(var::taskState) = 0x2c;
    nextStep(4);
  });
  bindNative(code::gameStartingTask, [this] { si = b(var::playersMayJoin) == kSet ? 0x22 : 0; });
  bindNative(routines::jccScoreZero, [=, this] {  // go elsewhere if a score is nought
    bool nought = true;
    for (u16 i = 0; i < 12; ++i)
      if (nativeB(static_cast<u16>(arg(2) + i)) != 0) nought = false;
    bx = nought ? arg(4) : static_cast<u16>(bx + 6);
    if (nativeW(bx) == 0) {
      w(var::nextStep) = 0;
      return;
    }
    w(var::nextStep) = bx;
    call(nativeW(bx));
  });
  bindNative(routines::multiplyBonus, [=, this] {  // the bonus times its multiplier
    for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(var::bonusOnce + i)) = b(static_cast<u16>(var::bonus + i));
    for (u8 n = static_cast<u8>(b(var::bonusMultiplier) - 1); n > 0; --n) addScore(var::bonus, var::bonusOnce);
    b(text::bonusTimes) = static_cast<u8>(b(var::bonusMultiplier) + 0x37);
    wait1();
    nextStep(4);
  });
  bindNative(routines::jccNoBonusMult, [=, this] {  // go elsewhere if there is no multiplier
    if (b(var::bonusMultiplier) != 1) {
      wait1();
      nextStep(4);
      return;
    }
    goTo(arg(2));
  });
  bindNative(routines::jump, [=, this] { goTo(arg(2)); });
  bindNative(routines::accBonusModeHit, [=, this] {
    wait1();
    addScore(var::bonus, var::modeHitScore);
    nextStep(2);
  });
  bindNative(routines::accBonusModeRamp, [=, this] {
    wait1();
    addScore(var::bonus, var::modeRampScore);
    nextStep(2);
  });
  bindNative(routines::accBonusCyclones, [=, this] {  // the miles into the bonus
    for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(var::milesBonus + i)) = 0;
    wait1();
    u32 times = w(var::miles);
    if (times == 0) times = 0x10000;  // as the original's loop does with a count of nought
    for (u32 i = 0; i < times; ++i) addScore(var::milesBonus, points::k100000);
    addScore(var::bonus, var::milesBonus);
    nextStep(2);
  });
  bindNative(routines::extraBall, [=, this] {  // an extra ball
    wait1();
    setLight(lights::extraBall);
    ++b(var::extraBalls);
    nextStep(4);
  });
}

}  // namespace encore
