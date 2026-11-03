#include "engine/table/Gameshow.h"

#include <algorithm>

namespace encore {

using namespace show;

Gameshow::Gameshow(ByteView prg) : Flow(prg, kTableIndex) {
  for (u16 i = 0; i < counts::maniaAtKept; ++i) maniaAt_[i] = b(static_cast<u16>(text::maniaAt + i));
  bindRules();
  bindSteps();
  bindNative(routines::tableStart, [this] { lightsOut(); });
  bindNative(routines::tableNewGame, [this] {  // a new game
    b(var::matchWon) = 0;
    zero12(var::skillsDigits);
    w(var::skills) = 0;
    b(var::prizeSet1) = 0;
    b(var::prizeSet2) = 0;
    lightsOut();
    stopBlinks();
    clearBall();
    for (u16 player = 0; player < kMaxPlayers; ++player) savePlayer(static_cast<u16>(player * rec::size));
    resetJackpot();
    for (u16 i = 0; i < counts::matchText; ++i) b(static_cast<u16>(text::matchPlayers + i)) = glyph::space;
    for (u16 i = 0; i < counts::matchText; ++i) b(static_cast<u16>(text::matchWinners + i)) = glyph::space;
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
    w(var::lookAtSoft) = high() ? lookAt::drainedHigh : lookAt::drainedLow;
    placeBall(spots::drained);
    b(var::ballHidden) = kSet;
    b(var::flippersLive) = 0;
    b(var::modeRunning) = 0;
    b(var::musicPriority) = 0;
    award(awards::drained);
    b(var::musicNext) = songAt::silence;
    addTimer(timers::drainSfx.routine);
  });
  bindNative(timers::drainSfx.routine, [this] {
    if (!countTo(timers::drainSfx.counter, times::drainSfx)) return;
    sfx(sounds::ballDrained);
    endTimer();
  });
  bindNative(routines::idleWhileUntouched, [this] {
    w(var::idleFrames) = 0;
    if (b(var::ballUntouched) == 0) endTimer();
  });
  bindNative(routines::tableTilt, [this] {  // tilt
    b(var::flippersLive) = 0;
    music(tunes::tilt);
    b(var::musicNext) = songAt::silence;
    startScript(scripts::tilt);
    stopBlinks();
    lightsDark();
  });
  bindNative(routines::tableTiltWarning, [this] { music(tunes::warnTilt); });
  bindNative(routines::tableFlipperPressed, [this] {  // a flipper pressed
    if (b(var::flipperPressed) == kSet) b(var::flipperPressed) = 0;
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
    if (b(var::countdownTens) != glyph::blank || b(var::countdownUnits) != glyph::digit0) return;
    for (u16 mode : {var::modeScoring, var::clearedWithModeScoring})  // (nothing in these rules sets the second)
      if (b(mode) == kSet) {
        b(var::musicRepeats) = 1;
        music(tunes::modeEnd);
        b(var::musicNext) = songAt::afterMode;
      }
    b(var::clearedWithModeScoring) = 0;
    b(var::modeScoring) = 0;
    b(var::musicPriority) = 0;
    b(var::musicNext) = songAt::afterMode;
    b(var::musicRepeats) = 1;
  });
}

void Gameshow::target(int which, bool down) {
  static constexpr TargetShape kPieces[] = {targetShapes::dropCenter0, targetShapes::dropCenter1, targetShapes::dropLeft0, targetShapes::dropLeft1};
  const TargetShape& p = kPieces[which];
  copyShape(kMask, p.at, down ? p.down : p.up, p.width, targetShapes::rows, p.width);
}

void Gameshow::clearBall() {
  zero12(var::score);
  zero12(var::bonus);
  zero12(var::modeScore);
  b(var::layer) = layers::playfield;
  copy12(var::cashpot, points::wheel500000);
  for (u16 a : {var::prizeTv, var::prizeTrip, var::prizeCar, var::prizeBoat, var::prizePlane, var::prizeHouse}) b(a) = 0;
  for (u16 a : {var::timeoutTv, var::timeoutTrip, var::timeoutCar, var::timeoutBoat, var::timeoutPlane, var::timeoutHouse}) w(a) = 0;
  b(var::billionLit) = 0;
  b(var::multiBonusStep) = 0;
  b(var::bonusMultiplier) = 1;
  w(text::multiplier) = kSpaceDigit1;  // " 1"
  stopBlinks();
  lightsOut();
  vaultEntry(false);
  for (int i = 0; i < drops::count; ++i) {
    setLight(static_cast<u8>(lights::dropCenter + i));
    target(i, false);
  }
  stopBlink(lights::skills);
  blink(lights::skills, 0, blinks::skillShot);
}

void Gameshow::savePlayer(u16 player) {
  copy12(static_cast<u16>(player + var::playerRecords + rec::score), var::score);
  copy12(static_cast<u16>(player + var::playerRecords + rec::bonus), var::bonus);
  copy12(static_cast<u16>(player + var::playerRecords + rec::skillsDigits), var::skillsDigits);
  w(static_cast<u16>(player + var::playerRecords + rec::skills)) = w(var::skills);
  b(static_cast<u16>(player + var::playerRecords + rec::prizeSet1)) = b(var::prizeSet1);
  b(static_cast<u16>(player + var::playerRecords + rec::prizeSet2)) = b(var::prizeSet2);
}

void Gameshow::restorePlayer() {
  const u16 player = static_cast<u16>((b(var::player) - 1) * rec::size);
  copy12(var::score, static_cast<u16>(player + var::playerRecords + rec::score));
  copy12(var::bonus, static_cast<u16>(player + var::playerRecords + rec::bonus));
  copy12(var::skillsDigits, static_cast<u16>(player + var::playerRecords + rec::skillsDigits));
  w(var::skills) = w(static_cast<u16>(player + var::playerRecords + rec::skills));
  b(var::prizeSet1) = b(static_cast<u16>(player + var::playerRecords + rec::prizeSet1));
  b(var::prizeSet2) = b(static_cast<u16>(player + var::playerRecords + rec::prizeSet2));
  if (amended) maniaAt();
  if (b(var::prizeSet1) == 0) return;
  b(var::prizeTv) = b(var::prizeTrip) = b(var::prizeCar) = prize::taken;
  for (u8 light : {lights::prizeTv, lights::prizeTrip, lights::prizeCar}) setLight(light);
  if (b(var::prizeSet2) == 0) return;
  b(var::prizeBoat) = b(var::prizeHouse) = b(var::prizePlane) = prize::taken;
  for (u8 light : {lights::prizeBoat, lights::prizeHouse, lights::prizePlane}) setLight(light);
}

void Gameshow::sameBallAgain() {
  w(var::idleFrames) = 0;
  for (u16 i = 0; i < kScoreDigits; ++i) b(static_cast<u16>(var::scoreDrawn + i)) = kScoreUndrawn;
  w(var::scoreDrawnGroups) = 0;
  bx = scripts::shootAgain;
  call(nativeW(bx));
}

void Gameshow::modeCountHit() {
  if (b(var::modeRunning) != kSet || b(var::modeScoring) != kSet || b(var::modeRamp) == kSet) return;
  addScore(var::modeScore, points::wheel500000);
}

void Gameshow::modeCountRamp() {
  if (b(var::modeRunning) != kSet || b(var::modeScoring) != kSet || b(var::modeRamp) == 0) return;
  addScore(var::modeScore, points::wheel1000000);
}

void Gameshow::threeDigits(u16 value, u16 at) {
  for (int i = 2; i >= 0; --i) {
    u16 digit = value % 10;
    value /= 10;
    if (digit == 0 && i == 0) digit = kBlankLessDigit0;
    b(static_cast<u16>(at + i)) = static_cast<u8>(digit + glyph::digit0);
  }
}

/// (amended) The skills ramp's message, MONEY MANIA AT, with the number this player's own trips
/// up it make: cs:1beb writes the next one into it past the twelfth trip, but it is one message
/// for all the players and the original never writes it back, so a player could be shown the
/// number of whoever passed twelve last.
void Gameshow::maniaAt() {
  for (u16 i = 0; i < counts::maniaAtKept; ++i) b(static_cast<u16>(text::maniaAt + i)) = maniaAt_[i];
  const u16 trips = w(var::skills);
  if (trips <= skills::unwrittenTo) return;
  // as cs:1beb last wrote it: at this trip, or at the one before it on a sixth
  const u16 next = static_cast<u16>(trips % skills::every != 0 ? (trips / skills::every + 1) * skills::every : trips);
  threeDigits(std::min<u16>(next, skills::lastNarrow), text::maniaAt);  // (a hundred or more is written one place on,
  if (next >= skills::hundred) threeDigits(next, text::maniaAt + 1);  // over the end of the one before it)
}

void Gameshow::litPrize(u16 flag, u16 record, u8 light, bool outOfStepLight, u16 other1, u16 other2) {
  b(flag) = prize::lit;
  award(record);
  blink(light, outOfStepLight ? outOfStep() : inStep(), blinks::half);
  if (b(other1) == 0 || b(other2) == 0) return;
  blink(lights::collectPrize, outOfStep(), blinks::half);
  lightState(lights::collectPrize) = kMarked;
  vaultEntry(true);
}

/// cs:10be: into the vault with the billion lit: it is collected.
void Gameshow::billion() {
  b(var::billionLit) = 0;
  award(awards::billion);
  stopBlink(lights::billion);
  blink(lights::billion, 0, blinks::billion);
  addTimer(timers::billionRelease.routine);
  hold(spots::vault);
}

/// cs:11a0: into the vault in a mode, or tilted: the ball is only let out again.
void Gameshow::vaultSkip() {
  hold(spots::vault);
  addTimer(timers::vaultEject.routine);
}

/// cs:1342: the prize wheel moves on one, slower each time, until its list of waits ends.
void Gameshow::wheelTick() {
  w(var::wheelWait) += 2;
  const u16 wait = nativeW(w(var::wheelWait));
  if (wait == kNone) {
    startScript(scripts::spinWheelBlink);
    addTimer(lightState(lights::collectPrize) == kMarked ? timers::givePrize : timers::spinWheelEnd.routine);
    return;
  }
  w(var::timeoutWheelTick) = wait;
  clearLight(static_cast<u8>(lights::wheel + b(var::wheelPos)));
  b(var::wheelPos) = static_cast<u8>((b(var::wheelPos) + 1) & wheel::mask);
  setLight(static_cast<u8>(lights::wheel + b(var::wheelPos)));
  w(scripts::spinWheelScoreValue) = static_cast<u16>(kScoreDigits * b(var::wheelPos) + points::wheel);
  startScript(scripts::spinWheelScore);
}

/// cs:1517: into the cashpot with both rows of prizes taken: the billion is lit, and the ball
/// goes back to the plunger lane.
void Gameshow::cashpotLock() {
  setLight(lights::cashpot);
  blink(lights::billion, inStep(), blinks::half);
  award(awards::cashpotLock);
  b(var::musicNext) = songAt::plunger;
  placeBall(spots::plungerLane);
  w(var::ballVx) = speeds::plungerLaneX;
  b(var::ballHidden) = 0;
  b(var::musicPriority) = 0;
  b(var::musicNext) = songAt::plunger;
  vaultEntry(true);
  b(var::billionLit) = kSet;
}

void Gameshow::cashpotEject() {
  clearLight(lights::cashpot);
  sfx(sounds::ejectCashpot);
  placeBall(spots::cashpot);
  w(var::ballVy) = speeds::cashpotEjectY;
  w(var::ballVx) = speeds::cashpotEjectX;
  b(var::ballHidden) = 0;
}

/// cs:1848: the bonus multiplier goes up a step, or, with nothing else to give, a count does.
void Gameshow::multiBonus() {
  if (w(var::timeoutMultiBonus) == 0) {
    if (w(var::timeoutCar) == 0) return;
    ++b(var::raisingMillions + kScoreMillions);  // a million more,
    ++b(awards::raisingMillions + kAwardMillions);  // and in the award's own score
    award(awards::raisingMillions);
    return;
  }
  if (b(var::multiBonusStep) == counts::multiBonusSteps) return;
  ++b(var::multiBonusStep);
  const u16 step = static_cast<u16>(b(var::multiBonusStep) - 1);
  if (b(var::musicPriority) <= b(tunes::multiBonus + tuneRec::priority) && b(var::modeRunning) != kSet) {
    startScript(nativeW(static_cast<u16>(var::multiBonusScripts + step * 2)));
    music(tunes::multiBonus);
  }
  u8 times = b(static_cast<u16>(var::multiBonusTimes + step));
  b(var::bonusMultiplier) = times;
  b(text::multiplier) = glyph::space;
  if (times == 0x0a) {
    b(text::multiplier) = glyph::digit0 + 1;
    times = 0;
  }
  b(text::multiplier + 1) = static_cast<u8>(times + glyph::digit0);
  setLight(static_cast<u8>(b(var::multiBonusStep) + lights::bonus - 1));
}

/// cs:1beb: a trip up the skills ramp is counted, with something at every sixth.
void Gameshow::countSkills() {
  do {
    addScore(var::skillsDigits, points::k1);
    ++w(var::skills);
  } while (w(var::skills) == 1);
  const u8 sixes = static_cast<u8>(w(var::skills) / skills::every), over = static_cast<u8>(w(var::skills) % skills::every);
  if (over != 0) {
    if (sixes == 1) {
      award(awards::skillsToExtraBall);
      return;
    }
    if (sixes > 1) {
      const u16 next = static_cast<u16>((sixes + 1) * skills::every);
      threeDigits(next, next >= skills::hundred ? text::maniaAt + 1 : text::maniaAt);
    }
    award(awards::skillsToMoneyMania);
    return;
  }
  if (sixes == 2) {
    music(tunes::extraBallLit);
    blink(lights::orbitExtraBall, outOfStep(), blinks::half);
    lightState(lights::orbitExtraBall) = kMarked;
    return;
  }
  const bool second = sixes != 1 && (sixes & 1);
  award(second ? awards::modeRamp : awards::modeHit);
  b(var::modeScoring) = kSet;
  b(var::modeRunning) = kSet;
  b(var::modeRamp) = second ? kSet : 0;
  setLight(lights::moneyMania);
}

void Gameshow::everyFrame() {
  auto down = [this](u16 at) { return w(at) != 0 && --w(at) == 0; };
  auto hurry = [this](u16 at, u8 light, u8 half) {
    if (w(at) != times::hurry) return;
    stopBlink(light);
    blink(light, 0, half);
  };
  if (down(var::timeoutSuperJackpot)) {
    stopBlink(lights::superJackpot);
    clearLight(lights::superJackpot);
  }
  hurry(var::timeoutSuperJackpot, lights::superJackpot, blinks::flash);
  if (down(var::timeoutJackpot)) {
    stopBlink(lights::jackpot);
    clearLight(lights::jackpot);
  }
  hurry(var::timeoutJackpot, lights::jackpot, blinks::flash);
  if (++b(var::lightPhasePrize) == blinks::period) b(var::lightPhasePrize) = 0;
  if (down(var::timeoutWheelTick)) wheelTick();
  for (u16 at : {var::timeoutMultiBonus, var::timeoutTv, var::timeoutBoat, var::timeoutHouse, var::timeoutPlane, var::timeoutTrip, var::timeoutCar}) down(at);
  if (down(var::timeoutCashpotX5)) {
    stopBlink(lights::cashpotX5);
    clearLight(lights::cashpotX5);
  }
  hurry(var::timeoutCashpotX5, lights::cashpotX5, blinks::flash);
  if (down(var::timeoutTopLoop)) {
    stopBlink(lights::topLoop);
    lightOff(lights::topLoop);
  }
  hurry(var::timeoutTopLoop, lights::topLoop, blinks::hurryTopLoop);
}

void Gameshow::bindRules() {
  auto after = [this](u16 at, u16 counter, u16 limit, std::function<void()> then) {
    bindNative(at, [this, counter, limit, then] {
      if (countTo(counter, limit)) then();
    });
  };

  // the two pairs of targets that drop (pfr: ShowCenter(0), (1), ShowLeft(0), (1))
  struct Drop { u16 hit, flag, other, record; int piece; u8 light; u16 timer; };
  for (const Drop d : {Drop{routines::dropCenter0, var::lightStates + lights::dropCenter, var::lightStates + lights::dropCenter + 1, awards::dropCenter, drops::center0, lights::dropCenter,
                            timers::resetDropCenter.routine},
                       {routines::dropCenter1, var::lightStates + lights::dropCenter + 1, var::lightStates + lights::dropCenter, awards::dropCenter, drops::center1, lights::dropCenter + 1,
                        timers::resetDropCenter.routine},
                       {routines::dropLeft0, var::lightStates + lights::dropLeft, var::lightStates + lights::dropLeft + 1, awards::dropLeft, drops::left0, lights::dropLeft,
                        timers::resetDropLeft.routine},
                       {routines::dropLeft1, var::lightStates + lights::dropLeft + 1, var::lightStates + lights::dropLeft, awards::dropLeft, drops::left1, lights::dropLeft + 1,
                        timers::resetDropLeft.routine}}) {
    bindNative(d.hit, [this, d] {
      modeCountHit();
      if (b(d.flag) == 0) return;
      award(d.record);
      sfx(sounds::hitTarget);
      target(d.piece, true);
      clearLight(d.light);
      if (b(d.other) == 0) addTimer(d.timer);
    });
  }
  after(timers::resetDropCenter.routine, timers::resetDropCenter.counter, times::resetDropCenter, [this] {
    sfx(sounds::raiseHitTargets);
    setLight(lights::dropCenter);
    setLight(lights::dropCenter + 1);
    target(drops::center0, false);
    target(drops::center1, false);
    endTimer();
  });
  after(timers::resetDropLeft.routine, timers::resetDropLeft.counter, times::resetDropLeft, [this] {
    sfx(sounds::raiseHitTargets);
    setLight(lights::dropLeft);
    setLight(lights::dropLeft + 1);
    target(drops::left0, false);
    target(drops::left1, false);
    endTimer();
  });

  // the pair of targets that open the door when both are hit
  auto both = [this] {
    blink(lights::dollar, 0, blinks::flash);
    blink(lights::dollar + 1, 0, blinks::flash);
    if (!(amended && bothShown_)) addTimer(timers::unblinkDollarAll.routine);
    bothShown_ = true;
    award(awards::dollarBoth);
    stopBlink(lights::spinWheel);
    blink(lights::spinWheel, outOfStep(), blinks::half);
    lightState(lights::spinWheel) = kSet;
    vaultEntry(true);
  };
  bindNative(routines::dollar0, [=, this] {
    modeCountHit();
    sfx(sounds::hitTarget);
    // (amended: for the second that the pair flashes after both were hit, another hit on
    // either counts as both again)
    if (lightState(lights::dollar + 1) == kSet || (amended && bothShown_)) return both();
    blink(lights::dollar, 0, blinks::dollar);
    lightState(lights::dollar) = kSet;
    addTimer(timers::unblinkDollar0.routine);
    award(awards::dollar);
  });
  bindNative(routines::dollar1, [=, this] {
    modeCountHit();
    sfx(sounds::hitTarget);
    if (lightState(lights::dollar) == kSet || (amended && bothShown_)) return both();
    blink(lights::dollar + 1, 0, blinks::dollar);
    lightState(lights::dollar + 1) = kSet;
    addTimer(timers::unblinkDollar1.routine);
    award(awards::dollar);
  });
  after(timers::unblinkDollar0.routine, timers::unblinkDollar0.counter, times::unblinkDollar, [this] {
    stopBlink(lights::dollar);
    setLight(lights::dollar);
    endTimer();
  });
  after(timers::unblinkDollar1.routine, timers::unblinkDollar1.counter, times::unblinkDollar, [this] {
    stopBlink(lights::dollar + 1);
    setLight(lights::dollar + 1);
    endTimer();
  });
  after(timers::unblinkDollarAll.routine, timers::unblinkDollarAll.counter, times::unblinkDollarAll, [this] {
    bothShown_ = false;
    stopBlink(lights::dollar);
    clearLight(lights::dollar);
    stopBlink(lights::dollar + 1);
    clearLight(lights::dollar + 1);
    endTimer();
  });

  bindNative(routines::enter, [this] {  // the ball leaves the plunger lane for the table
    if (w(var::lastTrigger) != routines::plungerGo) return;  // (only up from the plunger lane)
    plungerGate(false);
    b(var::playersMayJoin) = 0;
    music(tunes::main);
    b(var::musicNext) = songAt::main;
    startScript(scripts::main);
    b(var::ballUntouched) = 0;
    b(var::specialPlungerEvent) = 0;
  });
  bindNative(routines::vaultExit, [this] { vaultExit(false); });  // out of the vault
  bindNative(routines::plungerGo, [this] {  // up the plunger lane
    b(var::ballOnPlunger) = 0;
    plungerGate(true);
  });
  bindNative(routines::plungerBottom, [this] { b(var::ballOnPlunger) = kSet; });  // at the foot of the plunger lane

  after(timers::billionRelease.routine, timers::billionRelease.counter, times::billionRelease, [this] {
    stopBlink(lights::billion);
    for (u8 light : {lights::billion, lights::prizeTv, lights::prizeTrip, lights::prizeCar, lights::prizeBoat, lights::prizeHouse, lights::prizePlane}) clearLight(light);
    for (u16 a : {var::prizeTv, var::prizeTrip, var::prizeCar, var::prizeBoat, var::prizePlane, var::prizeHouse, var::prizeSet1, var::prizeSet2}) b(a) = 0;
    w(var::ballVy) = speeds::vaultEject;
    sfx(sounds::issueBall);
    b(var::ballHidden) = 0;
    endTimer();
  });
  after(timers::vaultEject.routine, timers::vaultEject.counter, times::vaultEject, [this] {
    w(var::ballVy) = speeds::vaultEject;
    sfx(sounds::issueBall);
    b(var::ballHidden) = 0;
    endTimer();
  });
  bindNative(routines::vault, [this] {  // through the door into the vault: the prize wheel turns
    vaultExit(true);
    if (b(var::modeRunning) == kSet || b(var::tilted) == kSet) return vaultSkip();
    if (b(var::billionLit) == kSet) return billion();
    b(var::ballHidden) = kSet;
    if (lightState(lights::collectPrize) == 0) vaultEntry(false);
    placeBall(spots::vault);
    music(tunes::spinWheel);
    w(var::wheelWait) = high() ? var::wheelWaitsHigh : var::wheelWaits;
    w(var::timeoutWheelTick) = nativeW(w(var::wheelWait));
    startScript(scripts::spinWheelClearHalt);
    clearLight(static_cast<u8>(lights::wheel + b(var::wheelPos)));
    u8 start = static_cast<u8>(w(var::loopCounter));
    if (drawsChance() && lightState(lights::collectPrize) != kMarked) {
      // by chance proper: where the wheel is to stop, counted back by the moves it will make
      u16 moves = 0;
      for (u16 at = w(var::wheelWait); nativeW(at) != kNone; at = static_cast<u16>(at + 2)) ++moves;
      start = static_cast<u8>(chance(wheel::places, 0) - moves);
    }
    if (lightState(lights::collectPrize) == kMarked) {  // a prize is to be collected: the wheel is made to stop on it
      struct Prize { u16 flag; u8 start; };
      for (const Prize p : {Prize{var::prizeTv, wheel::tv}, {var::prizeTrip, wheel::trip}, {var::prizeCar, wheel::car}, {var::prizeBoat, wheel::boat}, {var::prizeHouse, wheel::house},
                            {var::prizePlane, wheel::plane}})
        if (b(p.flag) == prize::lit) {
          start = p.start;
          break;
        }
    }
    if (high()) start = static_cast<u8>(start - wheel::backHigh);
    b(var::wheelPos) = start & wheel::mask;
    w(var::lookAtSoft) = high() ? lookAt::vaultHigh : lookAt::vaultLow;
  });
  after(timers::spinWheelEnd.routine, timers::spinWheelEnd.counter, times::spinWheelEnd, [this] {
    w(var::lookAtSoft) = kNone;
    // (the wheel's first value has blanks in it, for the display: a plain 25000 is added)
    if (w(scripts::spinWheelScoreValue) == points::wheel) w(scripts::spinWheelScoreValue) = points::k25000;
    addScore(var::score, w(scripts::spinWheelScoreValue));
    startScript(scripts::spinWheelClear);
    b(var::ballHidden) = 0;
    w(var::ballVy) = speeds::spinWheelEnd;
    stopBlink(lights::spinWheel);
    clearLight(lights::spinWheel);
    endTimer();
  });
  bindNative(timers::givePrize, [this] {  // the next prize of the row is collected
    auto collect = [this](u16 flag, u8 light, u16 record) {
      stopBlink(light);
      setLight(light);
      if (record == awards::prizeCar || record == awards::prizePlane) {
        stopBlink(lights::collectPrize);
        clearLight(lights::collectPrize);
      }
      b(flag) = prize::taken;
      b(var::musicPriority) = 0;
      award(record);
    };
    if (b(var::prizeSet1) != kSet) {
      if (b(var::prizeTv) != prize::taken) collect(var::prizeTv, lights::prizeTv, awards::prizeTv);
      else if (b(var::prizeTrip) != prize::taken) collect(var::prizeTrip, lights::prizeTrip, awards::prizeTrip);
      else {
        collect(var::prizeCar, lights::prizeCar, awards::prizeCar);
        blink(lights::jackpot, inStep(), blinks::half);
        lightState(lights::jackpot) = kMarked;
        w(var::timeoutJackpot) = times::jackpot;
        b(var::prizeSet1) = kSet;
        vaultEntry(false);
      }
    } else if (b(var::prizeBoat) != prize::taken) {
      collect(var::prizeBoat, lights::prizeBoat, awards::prizeBoat);
    } else if (b(var::prizeHouse) != prize::taken) {
      collect(var::prizeHouse, lights::prizeHouse, awards::prizeHouse);
    } else {
      collect(var::prizePlane, lights::prizePlane, awards::prizePlane);
      b(var::prizeSet2) = kSet;
      vaultEntry(false);
    }
    endTimer();
  });

  bindNative(routines::cashpotTrigger, [this] {  // the cashpot
    b(var::ballHidden) = kSet;
    if (b(var::modeRunning) == kSet) {
      b(var::ballHidden) = kSet;
      setLight(lights::cashpot);
      addTimer(timers::cashpotEjectInMode.routine);
      return;
    }
    if (b(var::prizeSet2) == kSet) return cashpotLock();
    incJackpot();
    if (w(var::timeoutCashpotX5) == 0) {
      award(awards::cashpot);
    } else {
      w(var::timeoutCashpotX5) = times::cashpotX5Taken;
      for (int i = 0; i < counts::cashpotX5; ++i) addScore(var::cashpotX5, var::cashpot);
      award(awards::cashpotX5);
    }
    addTimer(timers::cashpot.routine);
  });
  bindNative(timers::cashpot.routine, [this] {
    hold(spots::cashpot);
    if (!countTo(timers::cashpot.counter, times::cashpot)) return;
    setLight(lights::cashpot);
    addTimer(timers::cashpotEject.routine);
    zero12(var::cashpotX5);
    endTimer();
  });
  after(timers::cashpotEject.routine, timers::cashpotEject.counter, times::cashpotEject, [this] {
    cashpotEject();
    endTimer();
  });
  bindNative(timers::cashpotEjectInMode.routine, [this] {
    placeBall(spots::cashpot);
    if (!countTo(timers::cashpotEjectInMode.counter, times::cashpotEjectInMode)) return;
    cashpotEject();
    endTimer();
  });

  bindNative(routines::rampRight, [this] {  // the right ramp
    incCashpot();
    modeCountRamp();
    award(awards::rampRight);
    w(var::timeoutCar) = times::prizeShot;
    rampRightGate(true);
    if (w(var::timeoutJackpot) != 0) {
      w(var::timeoutJackpot) = 1;
      award(awards::jackpot);
      resetJackpot();
      w(var::timeoutSuperJackpot) = times::superJackpot;
      blink(lights::superJackpot, 0, blinks::half);
      music(tunes::jackpot);
    }
    w(var::timeoutTv) = times::prizeShot;
  });
  bindNative(routines::rampLoop, [this] {  // the loop ramp
    incCashpot();
    incJackpot();
    modeCountRamp();
    award(awards::rampLoop);
    if (w(var::timeoutSuperJackpot) != 0) {
      award(awards::superJackpot);
      w(var::timeoutSuperJackpot) = 1;
    }
    w(var::timeoutCashpotX5) = times::cashpotX5;
    stopBlink(lights::cashpotX5);
    blink(lights::cashpotX5, outOfStep(), blinks::half);
    if (w(var::timeoutCar) != 0 && b(var::prizeCar) < prize::lit) {
      w(var::timeoutCar) = 0;
      return litPrize(var::prizeCar, awards::litCar, lights::prizeCar, false, var::prizeTrip, var::prizeTv);
    }
    if (b(var::prizePlane) == 0 && w(var::timeoutCar) != 0 && b(var::prizeSet1) == kSet) {
      w(var::timeoutCar) = 0;
      if (b(var::prizeSet2) != kSet) {
        if (b(var::prizePlane) != 0) return;
        if (b(var::modeRunning) != kSet) startScript(scripts::hintLoopLeft);
        w(var::timeoutPlane) = times::prizeHint;
        return;
      }
    }
    multiBonus();
  });
  bindNative(routines::orbitLeft, [this] {  // the left orbit
    award(awards::orbitLeft);
    if (w(var::lastTrigger) != routines::orbitRight) return;  // (in from the right one)
    if (w(var::timeoutBoat) != 0 && b(var::prizeBoat) == 0) {
      w(var::timeoutBoat) = 0;
      return litPrize(var::prizeBoat, awards::litBoat, lights::prizeBoat, false, var::prizeHouse, var::prizePlane);
    }
    if (w(var::timeoutHouse) == 0 || b(var::prizeHouse) != 0) return;
    litPrize(var::prizeHouse, awards::litHouse, lights::prizeHouse, true, var::prizePlane, var::prizeBoat);
  });
  bindNative(routines::orbitRight, [this] {  // the right orbit
    modeCountRamp();
    award(awards::orbitRight);
    if (w(var::lastTrigger) != routines::orbitLeft) return;  // (in from the left one)
    if (lightState(lights::orbitExtraBall) != 0) {  // the ball more that was lit
      award(awards::extraBall);
      stopBlink(lights::orbitExtraBall);
      clearLight(lights::orbitExtraBall);
      setLight(lights::extraBall);
    }
    if (w(var::timeoutPlane) != 0 && b(var::prizePlane) == 0) {
      w(var::timeoutPlane) = 0;
      return litPrize(var::prizePlane, awards::litPlane, lights::prizePlane, false, var::prizeHouse, var::prizeBoat);
    }
    rampRightGate(false);
    w(var::timeoutMultiBonus) = times::multiBonus;
    w(var::timeoutTrip) = times::prizeShot;
  });
  bindNative(routines::rampSkillMark, [] {});  // the skills ramp's mark
  bindNative(routines::rampSkill, [this] {  // the skills ramp
    award(awards::rampSkills);
    if (w(var::lastTrigger) != routines::rampSkillMark) return;  // (only straight after its mark)
    incJackpot();
    incCashpot();
    modeCountRamp();
    countSkills();
    if (w(var::timeoutTv) != 0) {
      if (b(var::prizeSet2) == kSet) return;
      if (b(var::prizeSet1) == kSet) {
        if (b(var::prizeBoat) != 0) return;
        if (b(var::modeRunning) != kSet) startScript(scripts::hintLoopRight);
        w(var::timeoutBoat) = times::prizeHint;
        return;
      }
      if (b(var::prizeTv) == 0) {
        w(var::timeoutTv) = 0;
        return litPrize(var::prizeTv, awards::litTv, lights::prizeTv, false, var::prizeCar, var::prizeTrip);
      }
    }
    if (w(var::timeoutTrip) == 0 || b(var::prizeSet2) == kSet) return;
    if (b(var::prizeSet1) == kSet) {
      if (b(var::prizeHouse) != 0) return;
      if (b(var::modeRunning) != kSet) startScript(scripts::hintLoopRight);
      w(var::timeoutHouse) = times::prizeHint;
      return;
    }
    w(var::timeoutTrip) = 0;
    if (b(var::prizeTrip) != 0) return;
    litPrize(var::prizeTrip, awards::litTrip, lights::prizeTrip, true, var::prizeCar, var::prizeTv);
  });

  for (u16 at : {routines::laneOuterOther, routines::laneOuter})  // the outlanes
    bindNative(at, [this] {
      award(awards::laneOuter);
      sfx(sounds::rollover, volumes::outlane);
    });
  for (u16 at : {routines::laneInnerOther, routines::laneInner})  // the inlanes
    bindNative(at, [this] {
      award(awards::laneInner);
      sfx(sounds::inlane);
    });
  bindNative(routines::rampTopEntry, [this] { award(awards::topEntry); });
  bindNative(routines::rampSkillEntry, [this] { award(awards::skillsEntry); });
  bindNative(routines::rampLoopEntry, [this] { award(awards::loopEntry); });
  bindNative(routines::rampTopSecondary, [this] { incJackpot(); });
  bindNative(routines::rampTop, [this] {  // the top ramp
    incCashpot();
    award(awards::rampTop);
    const bool already = w(var::timeoutTopLoop) != 0;
    w(var::timeoutTopLoop) = times::topLoop;
    if (already) award(awards::rampTopTwice);
    if (lightState(lights::collectPrize) == 1) music(tunes::prizeIncoming);
    stopBlink(lights::topLoop);
    blink(lights::topLoop, inStep(), blinks::half);
  });
  bindNative(routines::incCashpotOther, [this] { addScore(var::cashpot, points::k227830); });
}

/// The steps the Gameshow adds to the display's scripts.
void Gameshow::bindSteps() {
  auto arg = [this](u16 n) { return nativeW(static_cast<u16>(bx + n)); };
  bindNative(routines::spinWheelEndStep, [this] {  // the wheel stops
    wait1();
    const u16 step = bx;
    addTimer(timers::spinWheelEnd.routine);
    bx = step;
    nextStep(steps::withWord);
  });
  bindNative(routines::endMoneyMania, [this] {  // a mode is over
    wait1();
    b(var::modeRunning) = 0;
    stopBlink(lights::moneyMania);
    clearLight(lights::moneyMania);
    b(var::musicPriority) = 0;
    nextStep(steps::withWord);
  });
  bindNative(routines::blinkMoneyMania, [this] {  // MONEY MANIA blinks
    wait1();
    blink(lights::moneyMania, 0, blinks::moneyMania);
    nextStep(steps::withWord);
  });
  bindNative(routines::match, [this] {  // the match begins
    w(var::task) = code::matchTask;
    w(var::taskState) = 1;
    b(var::player) = 1;
    nextStep(steps::withWord);
    if (amended) {  // in this frame, not the next, as on the other tables
      si = 1;
      call(code::matchTask);
      w(var::taskState) = si;
    }
  });
  bindNative(routines::setupPartyOn, [this] {
    b(text::playerDigit1) = b(text::playerDigit2) = static_cast<u8>(b(var::player) + glyph::digit0);
    wait1();
    nextStep(steps::withWord);
  });
  bindNative(routines::setupShootAgain, [this] {
    b(text::playerDigit3) = static_cast<u8>(b(var::player) + glyph::digit0);
    wait1();
    nextStep(steps::withWord);
  });
  bindNative(code::nothingTask, [] {});
  bindNative(routines::setSpecialPlungerEvent, [this] {
    b(var::specialPlungerEvent) = kSet;
    w(var::taskState) = 1;
    w(var::task) = code::nothingTask;
    nextStep(steps::withWord);
  });
  bindNative(routines::noop, [this] {
    wait1();
    nextStep(steps::withWord);
  });
  bindNative(routines::waitWhileGameStarting, [this] {
    w(var::task) = code::gameStartingTask;
    w(var::taskState) = tasks::gameStartingFirst;
    nextStep(steps::withWord);
  });
  bindNative(code::gameStartingTask, [this] { si = b(var::playersMayJoin) == kSet ? tasks::gameStartingStill : 0; });
  bindNative(routines::jccScoreZero, [=, this] {  // go elsewhere if a score is nought
    bool nought = true;
    for (u16 i = 0; i < kScoreDigits; ++i)
      if (nativeB(static_cast<u16>(arg(steps::arg1) + i)) != 0) nought = false;
    bx = nought ? arg(steps::arg2) : static_cast<u16>(bx + steps::withTwoWords);
    if (nativeW(bx) == 0) {
      w(var::nextStep) = 0;
      return;
    }
    w(var::nextStep) = bx;
    call(nativeW(bx));
  });
  bindNative(routines::dummy0, [] {});
  bindNative(routines::multiplyBonus, [this] {  // the bonus times its multiplier
    copy12(var::bonusOnce, var::bonus);
    for (u8 n = static_cast<u8>(b(var::bonusMultiplier) - 1); n > 0; --n) addScore(var::bonus, var::bonusOnce);
    b(text::bonusX) = glyph::digit0 + 1;
    b(text::bonusX + 1) = glyph::digit0;
    if (const u8 digit = static_cast<u8>(b(var::bonusMultiplier) + glyph::digit0); digit < kPastDigit9) {
      b(text::bonusX) = digit;
      b(text::bonusX + 1) = glyph::space;
    }
    wait1();
    nextStep(steps::withWord);
  });
  bindNative(routines::jccNoBonusMult, [=, this] {  // go elsewhere if there is no multiplier
    if (b(var::bonusMultiplier) == 1) return goTo(arg(steps::arg1));
    wait1();
    nextStep(steps::withWord);
  });
  bindNative(routines::accBonusModeHit, [this] {  // the mode's score into the bonus
    wait1();
    addScore(var::bonus, var::modeScore);
    nextStep(steps::bare);
  });
  bindNative(routines::jump, [=, this] { goTo(arg(steps::arg1)); });
  bindNative(routines::dummy1, [] {});
  bindNative(routines::stayStep, [this] { w(var::nextStep) = bx; });
  bindNative(routines::accBonusCyclones, [this] {  // the trips up the skills ramp into the bonus
    zero12(var::skillsBonus);
    wait1();
    u32 times = w(var::skills);
    if (times == 0) times = 0x10000;  // as the original's loop does with a count of nought
    for (u32 i = 0; i < times; ++i) addScore(var::skillsBonus, points::k100000);
    addScore(var::bonus, var::skillsBonus);
    nextStep(steps::bare);
  });
  bindNative(routines::extraBall, [this] {  // a ball more
    wait1();
    setLight(lights::extraBall);
    nextStep(steps::withWord);
  });
}

}  // namespace encore
