// Party Land's rules: what happens when the ball rolls into or hits each thing on the table
// (TABLE1.PRG, cs:1104 to cs:2f9a). Routines are named after pfr's (src/table/party.rs) where
// it has them, and say where they are in the program.
#include "engine/table/PartyLand.h"

namespace encore {

using namespace party;

void PartyLand::addBonus(u16 amount) {
  u16 times = W(var::bonusTimes);
  do addScore(var::bonus, amount);
  while (times-- > 1);
}

void PartyLand::scored() {
  B(var::scoredThisBall) = kSet;
  W(var::idleFrames) = 0;
  if (W(var::idleShowing) == kSet) {
    startScript(scripts::main);
    W(var::idleShowing) = 0;
  }
}

/// The screen is taken up to the top of the table, from where it is (cs:1217, cs:1228). The
/// two timers that do it start a frame or two after the ball has been put in its hole up
/// there, and until then the screen would set off after the ball. Amended: it is held where
/// it is from this moment, and goes up from this frame's turn of the timers on.
void PartyLand::startDropZoneScroll() {
  if (!amended) return addTimer(timers::dropZoneScrollStart);
  W(var::dropZoneScroll) = W(var::screenTop);
  W(var::lookAt) = W(var::screenTop);
  addTimer(timers::dropZoneScroll);
}

void PartyLand::startDropZone() {
  hole();
  startDropZoneScroll();
  addTimer(timers::dropZoneWait);
  W(var::dropZoneScroll) = W(var::screenTop);
}

bool PartyLand::partyAllLit() {
  return W(var::lightStates + lights::partyP) == kSetPair && W(var::lightStates + lights::partyR) == kSetPair &&
         lightState(lights::partyY) == kSet;
}

void PartyLand::checkPartyAll() {
  if (!partyAllLit()) return;
  if (B(var::modeRunning) == kSet) {
    B(var::pendingModeHit) = kSet;
    return;
  }
  happyHour();
}

void PartyLand::happyHour() {
  award(awards::happyHour);
  B(var::musicNext) = songAt::happyHour;
  for (u8 light = lights::partyP; light <= lights::partyY; ++light) clearLight(light);
  if (B(var::demonJackpot) != kSet && B(var::demonJackpotTimed) != kSet) blink(lights::demonJackpot, B(var::lightPhaseDemon), blinks::demon);
  B(var::demonJackpotTimed) = kSet;
  B(var::modeHit) = kSet;
  blink(lights::happyHour, 0, blinks::tunnel);
  B(var::modeRunning) = kSet;
  B(var::pendingMode) = kSet;
}

void PartyLand::checkCrazyAll(bool carry) {
  if (!carry) return;
  if (B(var::modeRunning) == kSet) {
    B(var::pendingModeRamp) = kSet;
    return;
  }
  megaLaugh();
}

void PartyLand::megaLaugh() {
  award(awards::megaLaugh);
  B(var::musicNext) = songAt::megaLaugh;
  if (B(var::demonJackpot) != kSet && B(var::demonJackpotTimed) != kSet) blink(lights::demonJackpot, B(var::lightPhaseDemon), blinks::demon);
  B(var::demonJackpotTimed) = kSet;
  B(var::modeRamp) = kSet;
  blink(lights::megaLaugh, 0, blinks::tunnel);
  B(var::modeRunning) = kSet;
  B(var::pendingMode) = kSet;
}

void PartyLand::duckScore() {
  effect(sounds::hitDuck);
  addScore(var::score, points::k7510);
  modeCountHit();
  addBonus(points::k750);
  B(var::scoredThisBall) = kSet;
}

/// cs:156d: true when all three drop targets are down.
bool PartyLand::duckAll() {
  if (lightState(lights::duckDrop0) != 0 || lightState(lights::duckDrop1) != 0 || lightState(lights::duckDrop2) != 0) return false;
  award(awards::duckAll);
  B(var::duckHit) = kSet;
  B(var::duckHit + 1) = kSet;
  for (u8 light : {lights::duck0, lights::duck1, lights::duck2}) stopBlink(light);
  for (u8 light : {lights::duck0, lights::duck1, lights::duck2}) blink(light, 0, blinks::flash);
  addTimer(timers::duckAllUnblink);
  if (B(var::snackLit, B(var::snack)) != kSet) {
    if (B(var::snack) == 1) {
      u8 start = static_cast<u8>(B(var::lightPhaseSnack) + blinks::snack);
      if (start >= blinks::snackPeriod) start = static_cast<u8>(start - blinks::snackPeriod);
      blink(lights::snack1, start, blinks::snack);
      B(var::snackLit + 1) = kSet;
    } else if (B(var::snack) > 1) {
      blink(lights::snack2, B(var::lightPhaseSnack), blinks::snack);
      B(var::snackLit + 2) = kSet;
    } else {
      blink(lights::snack0, B(var::lightPhaseSnack), blinks::snack);
      B(var::snackLit) = kSet;
    }
  }
  if (++B(var::snack) == counts::snacks) B(var::snack) = 0;
  return true;
}

bool PartyLand::crazyLetter() {
  incJackpot();
  B(var::crazyShown) = award(B(var::crazyFromArcade) == kSet ? awards::arcadeCrazy : awards::orbitCrazy) ? 1 : 0;
  if (!nextOfRow(var::crazyRow)) return false;
  for (u8 light : {lights::crazy4, lights::crazy3, lights::crazy2, lights::crazy1, lights::crazy0}) clearLight(light);
  return true;
}

void PartyLand::cycloneX5End() {
  stopBlink(lights::cycloneX5);
  lightOff(lights::cycloneX5);
  B(var::cycloneX5) = 0;
}

void PartyLand::partyP() {
  if (lightState(lights::partyP) == kSet) return;
  award(awards::partyP);
  setLight(lights::partyP);
  checkPartyAll();
}

/// What the arcade gives. The original goes by a count of the frames played, through a list
/// of 128 (ds:05b4), and gives it a frame later. Amended: one of the six by chance proper,
/// each as likely as the others, and at once.
void PartyLand::arcadePickReward() {
  if (!amended) return addTimer(W(var::arcadePrizes, B(var::arcadeList, static_cast<u16>(B(var::frames) >> 1))));
  static constexpr u16 kPrizes[counts::arcadePrizes] = {code::arcadeSideExtraBall, code::arcadeCrazy, code::arcade1M, code::arcade5M, code::arcade500k, code::arcadeNoScore};
  const u16 running = W(var::timerSlot);
  addTimer(kPrizes[chance(counts::arcadePrizes, 0)]);
  W(var::timerSlot) = bx;
  call(nativeW(bx));  // (each of them ends itself)
  W(var::timerSlot) = running;
}

void PartyLand::arcadeNotShown() {
  W(var::dropZoneDelay) = times::arcadeNotShown;
  addTimer(timers::dropZoneStart);
  endTimer();
}

bool PartyLand::demon5M() {
  if (B(var::demon5M) != kSet) return false;
  award(awards::demon5M);
  stopBlink(lights::demon5M);
  lightOff(lights::demon5M);
  B(var::demon5M) = 0;
  W(var::demonTimeout) = B(var::modeRunning) == kSet ? times::demonInMode : times::demon5M;
  return true;
}

bool PartyLand::demonExtraBall() {
  if (B(var::demonExtraBall) != kSet) return false;
  stopBlink(lights::demonExtraBall);
  lightOff(lights::demonExtraBall);
  setLight(lights::extraBall);
  ++B(var::extraBalls);
  award(awards::demonExtraBall);
  B(var::demonExtraBall) = 0;
  W(var::demonTimeout) = B(var::modeRunning) == kSet ? times::demonInMode : times::demonExtraBall;
  return true;
}

bool PartyLand::demonJackpot() {
  if (B(var::demonJackpot) != kSet && B(var::demonJackpotTimed) != kSet) return false;
  B(var::demonJackpot) = 0;
  B(var::demonJackpotTimed) = 0;
  stopBlink(lights::demonJackpot);
  lightOff(lights::demonJackpot);
  music(tunes::jackpot);
  startScript(B(var::modeRunning) != kSet ? scripts::jackpot : B(var::modeHit) != kSet ? scripts::jackpotModeRamp : scripts::jackpotModeHit);
  addScore(var::score, var::jackpot);
  for (u16 i = 0; i < kScoreDigits; ++i) B(var::jackpot, i) = B(var::jackpotStart, i);
  W(var::demonTimeout) = times::demonJackpot;
  return true;
}

void PartyLand::partyT() {
  if (lightState(lights::partyT) == kSet) return;
  setLight(lights::partyT);
  award(awards::partyT);
  checkPartyAll();
}

bool PartyLand::pukeScore() {
  addScore(var::score, points::k20070);
  scored();
  addBonus(points::k1000);
  incJackpot();
  return lightState(lights::puke0) == kSet && lightState(lights::puke3) == kSet && lightState(lights::puke1) == kSet &&
         lightState(lights::puke2) == kSet;
}

/// cs:28f5: all four lanes lit.
void PartyLand::pukeAll() {
  auto later = [](u8 start, u8 half) { start = static_cast<u8>(start + half); return start >= half * 2 ? static_cast<u8>(start - half * 2) : start; };
  blink(lights::puke0, B(var::lightPhasePuke), blinks::puke);
  blink(lights::puke2, B(var::lightPhasePuke), blinks::puke);
  blink(lights::puke3, later(B(var::lightPhasePuke), blinks::puke), blinks::puke);
  blink(lights::puke1, later(B(var::lightPhasePuke), blinks::puke), blinks::puke);
  addTimer(timers::pukeUnblinkAll);
  ++W(var::demonReward);
  if (W(var::demonReward) == 1) {
    if (B(var::demon5M) != kSet) {
      blink(lights::demon5M, B(var::lightPhaseDemon), blinks::demon);
      B(var::demon5M) = kSet;
    }
  } else if (W(var::demonReward) == 2) {
    if (B(var::demonExtraBall) != kSet) {
      blink(lights::demonExtraBall, later(B(var::lightPhaseDemon), blinks::demon), blinks::demon);
      B(var::demonExtraBall) = kSet;
    }
  } else if (W(var::demonReward) == 3 && B(var::demonJackpot) != kSet) {
    if (B(var::demonJackpotTimed) != kSet) blink(lights::demonJackpot, B(var::lightPhaseDemon), blinks::demon);
    B(var::demonJackpot) = kSet;
  }
  if (lightState(lights::partyY) != kSet) {
    setLight(lights::partyY);
    award(awards::partyY);
    checkPartyAll();
  }
}

void PartyLand::partyR() {
  if (lightState(lights::partyR) != kSet) {
    setLight(lights::partyR);
    award(awards::partyR);
    checkPartyAll();
  }
  cycloneX5End();
}

void PartyLand::modeCountHit() {
  if (B(var::modeHit) != kSet) return;
  addScore(var::modeHitScore, points::k1000000);
  B(var::modeScored) = kSet;
}

void PartyLand::modeCountRamp() {
  if (B(var::modeRamp) != kSet) return;
  addScore(var::modeRampScore, points::k5000000);
  B(var::modeScored) = kSet;
}

void PartyLand::sideExtraBall() {
  clearLight(lights::sideExtraBall);
  award(awards::sideExtraBall);
  setLight(lights::extraBall);
  ++B(var::extraBalls);
  B(var::blockDrain) = kSet;
  addTimer(timers::sideExtraBallFinish);
}

void PartyLand::modeCheck() {
  const bool nine = B(var::countdownTens) == glyph::blank && B(var::countdownUnits) == glyph::digit0 + 2,
             nought = B(var::countdownTens) == glyph::blank && B(var::countdownUnits) == glyph::digit0;
  if (nine && B(var::demonJackpot) != kSet && B(var::demonJackpotTimed) == kSet) {
    stopBlink(lights::demonJackpot);
    blink(lights::demonJackpot, 0, blinks::flash);
  }
  if (!nought) return;
  const bool first = B(var::modeHit) == kSet;
  if (first) {
    W(var::taskState) = 0;
    B(var::modeRunning) = 0;
    B(var::modeHit) = 0;
  } else {
    B(var::modeRunning) = 0;
    B(var::modeRamp) = 0;
    W(var::taskState) = 0;
  }
  const u8 light = first ? lights::happyHour : lights::megaLaugh;
  stopBlink(light);
  lightOff(light);
  award(first ? awards::happyHourEnd : awards::megaLaughEnd);
  B(var::musicNext) = songAt::main;
  stopBlink(lights::demonJackpot);
  lightOff(lights::demonJackpot);
  B(var::demonJackpotTimed) = 0;
  if (B(var::demonJackpot) == kSet) blink(lights::demonJackpot, B(var::lightPhaseDemon), blinks::demon);
  if (first) {
    if (B(var::pendingModeRamp) == kSet) {
      B(var::pendingModeRamp) = 0;
      addTimer(timers::megaLaugh);
    }
  } else if (B(var::pendingModeHit) == kSet) {
    B(var::pendingModeHit) = 0;
    addTimer(timers::happyHour);
  }
}

void PartyLand::bindRules() {
  // a timer that waits `limit` frames and then does something once
  auto after = [this](u16 at, u16 counter, u16 limit, std::function<void()> then) {
    bind(at, [this, counter, limit, then] {
      if (countTo(counter, limit)) then();
    });
  };
  bind(routines::tableModeSecond, [this] { modeCheck(); });

  bind(timers::dropZoneStart, [this] {
    if (B(var::modeRunning) != kSet && !countTo(counters::dropZoneStart, W(var::dropZoneDelay))) return;
    startDropZone();
    endTimer();
  });
  after(timers::dropZoneWait, counters::dropZoneWait, times::dropZoneWait, [this] {
    blink(lights::dropZone0, 0, blinks::dropZone);
    blink(lights::dropZone1, 0, blinks::dropZone);
    addTimer(timers::dropZoneRelease);
    endTimer();
  });
  after(timers::dropZoneRelease, counters::dropZoneRelease, times::dropZoneRelease, [this] {  // out of the hole, upwards on the ramps' side
    W(var::lookAt) = kNone;
    const u16 speed = chance(speeds::dropZoneChoices, W(var::loopCounter) & speeds::dropZoneMask);
    hole();
    B(var::layer) = layers::ramps;
    W(var::ballVy) = speed;
    effect(sounds::issueBall);
    stopBlink(lights::dropZone0);
    stopBlink(lights::dropZone1);
    endTimer();
  });
  bind(timers::dropZoneScrollStart, [this] {
    addTimer(timers::dropZoneScroll);
    W(var::dropZoneScroll) = W(var::screenTop);
    endTimer();
  });
  bind(timers::dropZoneScroll, [this] {  // the screen goes up to the top of the table
    W(var::dropZoneScroll) -= counts::dropZoneScrollStep;
    if (W(var::dropZoneScroll).s() < 0) {
      W(var::lookAt) = 0;
      endTimer();
    } else {
      W(var::lookAt) = W(var::dropZoneScroll);
    }
  });
  bind(routines::arcadeButton, [this] {  // the arcade's button
    if (B(var::arcadeButtonHit) == kSet) return;
    effect(sounds::arcadeButton);
    B(var::arcadeButtonHit) = kSet;
    addTimer(timers::resetArcadeButton);
    if (B(var::arcadeOpen) == kSet) return;
    B(var::arcadeOpen) = kSet;
    blink(lights::arcade0, 0, blinks::arcade);
    blink(lights::arcade1, 0, blinks::arcade);
  });
  after(timers::resetArcadeButton, counters::resetArcadeButton, times::resetArcadeButton, [this] {
    B(var::arcadeButtonHit) = 0;
    endTimer();
  });

  // the three drop targets, the ducks (pfr: PartyDuck(0) to (2))
  struct Target { u16 hit, lit, down, light, fall, counter, shape, at, width, flash, flashCount, flashTimer; };
  for (const Target t : {Target{routines::duck0, var::lightStates + lights::duckDrop0, var::duckHit, lights::duckDrop0, timers::duckDrop0,
                                counters::duckDrop0, gates::duck0.dropped, gates::duck0.at, gates::duck0.width, lights::duck0,
                                cvar::duckUnblink0, code::duckUnblink0},
                         Target{routines::duck1, var::lightStates + lights::duckDrop1, var::duckHit + 1, lights::duckDrop1, timers::duckDrop1,
                                counters::duckDrop1, gates::duck1.dropped, gates::duck1.at, gates::duck1.width, lights::duck1,
                                cvar::duckUnblink1, code::duckUnblink1},
                         Target{routines::duck2, var::lightStates + lights::duckDrop2, var::duckHit + 2, lights::duckDrop2, timers::duckDrop2,
                                counters::duckDrop2, gates::duck2.dropped, gates::duck2.at, gates::duck2.width, lights::duck2,
                                cvar::duckUnblink2, code::duckUnblink2}}) {
    bind(t.hit, [this, t] {
      if (B(t.lit) != kSet || B(t.down) == kSet) return;
      B(t.down) = kSet;
      clearLight(static_cast<u8>(t.light));
      addTimer(t.fall);
      duckScore();
      if (duckAll()) return;
      addTimer(t.flashTimer);
      blink(static_cast<u8>(t.flash), 0, blinks::duck);
      CB(t.flashCount) = 0;
    });
    bind(t.flashTimer, [this, t] {
      if (CB(t.flashCount) == times::duckUnblink) {
        stopBlink(static_cast<u8>(t.flash));
        B(t.down) = 0;
        dropTimer();
      }
      ++CB(t.flashCount);
    });
    after(t.fall, t.counter, times::duckDrop, [this, t] {  // it goes down: no longer in the ball's way
      patchMask(at::wallsGround, t.at, t.shape, t.width, counts::duckRows);
      endTimer();
    });
  }
  after(timers::duckAllUnblink, counters::duckAllUnblink, times::duckAllUnblink, [this] {  // and all three stand again
    for (u8 light : {lights::duck0, lights::duck1, lights::duck2}) stopBlink(light);
    setLight(lights::duckDrop0);
    raise(gates::duck0);
    lightOff(lights::duck0);
    setLight(lights::duckDrop1);
    raise(gates::duck1);
    lightOff(lights::duck1);
    setLight(lights::duckDrop2);
    raise(gates::duck2);
    lightOff(lights::duck2);
    effect(sounds::raiseDucks);
    B(var::duckHit) = 0;
    B(var::duckHit + 1) = 0;
    B(var::duckHit + 2) = 0;
    endTimer();
  });

  bind(routines::orbitTopLeft, [this] {  // the top of the left orbit: the right orbit made (pfr's party_orbit_right)
    if (W(var::lastTrigger) != routines::orbitTopRight) return;
    if (W(var::timeoutSpringLoop) != 0) {
      W(var::timeoutSpringLoop) = 0;
      return;
    }
    if (B(var::orbitRightBlinking) == kSet) return;
    incJackpot();
    modeCountRamp();
    if (W(var::timeoutPartyT) != 0) partyT();
    W(var::timeoutPartyT) = times::partyT;
    W(var::timeoutPartyPr) = times::partyPr;
    ++B(var::orbitRightCycle);
    if (B(var::orbitRightCycle) < 2) {
      stopBlink(lights::orbit250k);
      lightOn(lights::orbit250k);
      blink(lights::orbit500k, 0, blinks::orbit);
      award(awards::orbit250k);
    } else if (B(var::orbitRightCycle) == 2) {
      stopBlink(lights::orbit500k);
      lightOn(lights::orbit500k);
      blink(lights::orbit750k, 0, blinks::orbit);
      award(awards::orbit500k);
    } else {
      B(var::orbitRightCycle) = 0;
      stopBlink(lights::orbit750k);
      blink(lights::orbit750k, 0, blinks::flash);
      blink(lights::orbit500k, 0, blinks::flash);
      blink(lights::orbit250k, 0, blinks::flash);
      award(awards::orbit750k);
      addTimer(timers::orbitRightUnblink);
      B(var::orbitRightBlinking) = kSet;
    }
    if (B(var::rightOrbitMultiBonus) == kSet) {
      B(var::rightOrbitMultiBonus) = 0;
      stopBlink(lights::rightOrbitMultiBonus);
      lightOff(lights::rightOrbitMultiBonus);
      struct Step { u16 lit; u8 light; u16 prize, times; };
      for (const Step s : {Step{var::lightStates + lights::bonusX2, lights::bonusX2, awards::orbitMb2, 2},
                           {var::lightStates + lights::bonusX4, lights::bonusX4, awards::orbitMb4, 4},
                           {var::lightStates + lights::bonusX6, lights::bonusX6, awards::orbitMb6, 6},
                           {var::lightStates + lights::bonusX8, lights::bonusX8, awards::orbitMb8, 8}}) {
        if (B(s.lit) == kSet) continue;
        setLight(s.light);
        award(s.prize);
        W(var::bonusTimes) = s.times;
        B(var::bonusMultiplier) += 2;
        if (B(var::bonusMultiplier) == 3) --B(var::bonusMultiplier);
        if (++B(text::multiplier) != glyph::digit0 + 2) ++B(text::multiplier);
        break;
      }
    }
    if (B(var::rightOrbitHoldBonus) == kSet) {
      B(var::rightOrbitHoldBonus) = 0;
      stopBlink(lights::rightOrbitHoldBonus);
      lightOff(lights::rightOrbitHoldBonus);
      award(awards::orbitHoldBonus);
      B(var::holdBonus) = kSet;
    }
    if (B(var::rightOrbitDoubleBonus) == kSet) {
      B(var::rightOrbitDoubleBonus) = 0;
      stopBlink(lights::rightOrbitDoubleBonus);
      lightOff(lights::rightOrbitDoubleBonus);
      award(awards::orbitDoubleBonus);
      addScore(var::bonus, var::bonus);
    }
  });
  after(timers::orbitRightUnblink, counters::orbitRightUnblink, times::orbitRightUnblink, [this] {
    for (u8 light : {lights::orbit750k, lights::orbit500k, lights::orbit250k}) stopBlink(light);
    blink(lights::orbit250k, 0, blinks::orbit);
    B(var::orbitRightBlinking) = 0;
    endTimer();
  });
  bind(routines::orbitTopRight, [this] {  // the top of the right orbit: the left orbit made (pfr's party_orbit_left)
    if (W(var::lastTrigger) != routines::orbitTopLeft || B(var::madBlinking) == kSet) return;
    incJackpot();
    modeCountRamp();
    if (W(var::timeoutPartyT) != 0) partyT();
    W(var::timeoutPartyT) = times::partyT;
    if (lightState(lights::mad0) != kSet) {
      award(awards::orbitMad0);
      blink(lights::mad0, 0, blinks::flash);
      addTimer(timers::madUnblink0);
    } else if (lightState(lights::mad1) != kSet) {
      award(awards::orbitMad1);
      blink(lights::mad1, 0, blinks::flash);
      addTimer(timers::madUnblink1);
    } else {
      lightState(lights::mad0) = 0;
      lightState(lights::mad1) = 0;
      blink(lights::mad0, 0, blinks::flash);
      blink(lights::mad1, 0, blinks::flash);
      blink(lights::mad2, 0, blinks::flash);
      addTimer(timers::madAllUnblink);
      B(var::madBlinking) = kSet;
      award(awards::orbitMad2);
      B(var::crazyFromArcade) = 0;
      checkCrazyAll(crazyLetter());
    }
  });
  after(timers::madUnblink0, counters::madUnblink0, times::madUnblink, [this] {
    stopBlink(lights::mad0);
    lightState(lights::mad0) = kSet;
    endTimer();
  });
  after(timers::madUnblink1, counters::madUnblink1, times::madUnblink, [this] {
    stopBlink(lights::mad1);
    lightState(lights::mad1) = kSet;
    endTimer();
  });
  after(timers::madAllUnblink, counters::madAllUnblink, times::madAllUnblink, [this] {
    for (u8 light : {lights::mad0, lights::mad1, lights::mad2}) stopBlink(light);
    B(var::madBlinking) = 0;
    endTimer();
  });

  bind(routines::secret, [this] {  // the secret
    B(var::scriptDone) = 0;
    award(awards::secret);
    incJackpot();
    blink(lights::cycloneX5, 0, blinks::cycloneX5);
    B(var::cycloneX5) = kSet;
    addTimer(timers::secretDrop);
    hole();
  });
  bind(routines::secretTilt, [this] {  // the same, tilted
    hole();
    startDropZone();
    stopBlink(lights::cycloneX5);
  });
  bind(timers::secretDrop, [this] {
    if (B(var::scriptDone) != kSet) return;
    addTimer(timers::cycloneX5Blink);
    startDropZone();
    endTimer();
  });
  after(timers::cycloneX5Blink, counters::cycloneX5Blink, times::cycloneX5Blink, [this] {
    stopBlink(lights::cycloneX5);
    if (B(var::cycloneX5) != 0) {
      blink(lights::cycloneX5, 0, blinks::flash);
      addTimer(timers::cycloneX5End);
    } else {
      lightOff(lights::cycloneX5);
    }
    endTimer();
  });
  after(timers::cycloneX5End, counters::cycloneX5End, times::cycloneX5End, [this] {
    cycloneX5End();
    endTimer();
  });
  bind(routines::tunnel, [this] {  // the tunnel
    incJackpot();
    modeCountRamp();
    if (W(var::timeoutSkillShot) != 0) {
      incJackpot();
      W(var::timeoutSkillShot) = 0;
      addScore(var::tunnelSkillShot, points::k1000000);
      addScore(var::score, var::tunnelSkillShot);
      award(awards::tunnelSkillShot);
      B(var::awardQuiet) = kSet;
      partyP();
    } else if (W(var::timeoutPartyPr) != 0) {
      partyP();
    }
    if (lightState(lights::tunnel3M) == kSet) {
      award(awards::tunnel5M);
      W(var::timeoutTunnel) = times::tunnel;
    } else if (lightState(lights::tunnel1M) == kSet) {
      award(awards::tunnel3M);
      W(var::timeoutTunnel) = times::tunnel;
      stopBlink(lights::tunnel3M);
      setLight(lights::tunnel3M);
      blink(lights::tunnel5M, 0, blinks::tunnel);
    } else {
      award(awards::tunnel1M);
      W(var::timeoutTunnel) = times::tunnelHalf;
      stopBlink(lights::tunnel1M);
      setLight(lights::tunnel1M);
      blink(lights::tunnel3M, 0, blinks::tunnel);
    }
    W(var::tunnelDelay) = times::tunnelDelay;
    startDropZoneScroll();
    addTimer(timers::tunnelDropZoneStart);
    if (B(var::modeRunning) != kSet) addTimer(timers::tunnelFreeze);
    B(var::awardQuiet) = 0;
  });
  bind(timers::tunnelDropZoneStart, [this] {
    if (B(var::modeRunning) != kSet && !countTo(counters::tunnelDropZoneStart, W(var::tunnelDelay))) return;
    W(var::lookAt) = kNone;
    startDropZone();
    endTimer();
  });
  after(timers::tunnelFreeze, counters::tunnelFreeze, times::tunnelFreeze, [this] {
    hole();
    endTimer();
  });
  bind(routines::tunnelTilt, [this] {  // the tunnel, tilted
    hole();
    startDropZone();
  });
  bind(routines::arcade, [this] {  // the arcade
    B(var::frames) += times::arcadeEntry;
    modeCountRamp();
    if (B(var::tilted) == kSet || B(var::arcadeOpen) != kSet) {
      startDropZone();
      return;
    }
    B(var::arcadeOpen) = 0;
    stopBlink(lights::arcade0);
    stopBlink(lights::arcade1);
    lightOff(lights::arcade0);
    lightOff(lights::arcade1);
    B(var::musicKept) = B(var::musicNext);
    B(var::arcadeReady) = 0;
    W(var::jingleTimeout) = times::arcadeJingle;
    if (!award(awards::arcade)) {
      B(var::arcadeReady) = kSet;
      arcadePickReward();
      B(var::musicKept) = B(var::musicNext);
    } else {
      B(var::musicNext) = songAt::silence;
      // (amended: the arcade's jingle keeps its own priority while it plays)
      if (!amended) B(var::musicPriority) = priorities::arcade;
      B(var::jingleEndedRules) = 0;
      B(var::jingleEnded) = 0;
      addTimer(timers::arcadePickReward);
    }
    startDropZoneScroll();
    hole();
  });
  bind(timers::arcadePickReward, [this] {
    if (B(var::modeRunning) != kSet && B(var::arcadeReady) != kSet) return;
    if (B(var::tilted) == kSet) {
      startDropZone();
      endTimer();
      return;
    }
    // (amended: after the arcade the music starts again from the beginning of the table's tune,
    // not from where it was when the ball went in)
    if (amended) B(var::musicKept) = kb(at::musicOnPlaceInsn, 4);
    B(var::musicNext) = B(var::musicKept);
    // (amended: the prize's jingle is not cut short by one of less weight: the priority
    // stays, and a jingle still to be repeated is)
    if (!amended) {
      B(var::musicRepeats) = 1;
      B(var::musicPriority) = 0;
    } else if (B(var::musicRepeats) == 0) {
      B(var::musicRepeats) = 1;
    }
    arcadePickReward();
    endTimer();
  });
  // what the arcade gives, one of these by chance (the table at ds:0634)
  auto prize = [this](u16 record, u16 show, u16 hold) {
    if (!award(record)) return arcadeNotShown();
    B(var::musicNext) = B(var::musicKept);
    if (!amended) B(var::musicPriority) = 0;  // (amended: a prize's jingle keeps its priority)
    W(var::arcadeTimeoutSoft) = show;
    W(var::arcadeTimeoutHard) = hold;
    B(var::arcadeJingleDone) = 0;
    addTimer(timers::arcadeDropZoneStart);
    endTimer();
  };
  bind(code::arcadeSideExtraBall, [this] {
    setLight(lights::sideExtraBall);
    if (!award(awards::arcadeSideExtraBall)) return arcadeNotShown();
    B(var::musicNext) = B(var::musicKept);
    if (!amended) B(var::musicPriority) = 0;  // (amended: a prize's jingle keeps its priority)
    W(var::dropZoneDelay) = times::arcadeSideExtraBall;
    addTimer(timers::dropZoneStart);
    endTimer();
  });
  bind(code::arcadeCrazy, [this] {
    B(var::crazyFromArcade) = kSet;
    checkCrazyAll(crazyLetter());
    if (B(var::crazyShown) == 0) return arcadeNotShown();
    B(var::musicNext) = B(var::musicKept);
    if (!amended) B(var::musicPriority) = 0;  // (amended: a prize's jingle keeps its priority)
    W(var::arcadeTimeoutSoft) = times::arcadeCrazyShow;
    W(var::arcadeTimeoutHard) = times::arcadeCrazyHold;
    B(var::arcadeJingleDone) = 0;
    addTimer(timers::arcadeDropZoneStart);
    endTimer();
  });
  bind(code::arcade1M, [=] { prize(awards::arcade1M, times::arcade1MShow, times::arcade1MHold); });
  bind(code::arcade5M, [=] { prize(awards::arcade5M, times::arcade5MShow, times::arcade5MHold); });
  bind(code::arcade500k, [=] { prize(awards::arcade500k, times::arcade500kShow, times::arcade500kHold); });
  bind(code::arcadeNoScore, [this] {
    if (award(awards::arcadeNoScore)) {
      B(var::musicNext) = B(var::musicKept);
      if (!amended) B(var::musicPriority) = 0;
    }
    W(var::dropZoneDelay) = times::arcadeNoScore;
    addTimer(timers::dropZoneStart);
    endTimer();
  });
  bind(timers::arcadeDropZoneStart, [this] {
    bool done = --W(var::arcadeTimeoutHard) == 0;
    if (amended) {
      // The original lets the ball go when the prize's jingle is over and its time on the
      // display is up. Amended: when its time is up, once the jingle has been heard to play;
      // a long jingle no longer keeps the ball back.
      if (!done && B(var::modeRunning) != kSet) {
        if (W(var::arcadeTimeoutSoft) != 0) --W(var::arcadeTimeoutSoft);
        if (B(var::musicRepeats) == 0 && B(var::arcadeJingleDone) != kSet) return;
        B(var::arcadeJingleDone) = kSet;
        if (W(var::arcadeTimeoutSoft) != 0) return;
      }
    } else if (!done) {
      if (W(var::arcadeTimeoutSoft) != 0) --W(var::arcadeTimeoutSoft);
      if (B(var::arcadeJingleDone) != kSet) {
        if (B(var::modeRunning) == kSet) done = true;
        else if (B(var::jingleEndedRules) != kSet) return;
      }
      if (!done) {
        B(var::arcadeJingleDone) = kSet;
        if (W(var::arcadeTimeoutSoft) != 0) return;
      }
    }
    B(var::jingleEndedRules) = 0;
    W(var::arcadeTimeoutSoft) = times::arcadeSoftReset;
    W(var::arcadeTimeoutHard) = times::arcadeHardReset;
    startDropZone();
    endTimer();
  });

  bind(routines::rampSnack, [this] {  // the snack ramp
    if (B(var::inSnack) == kSet) return;
    B(var::inSnack) = kSet;
    addScore(var::score, points::k50000);
    scored();
    addBonus(points::k5000);
    modeCountRamp();
    if (B(var::snackLit + 2) == kSet) {
      award(awards::snack2);
      incJackpot();
      ++B(var::popcorns);
    } else if (B(var::snackLit + 1) == kSet) {
      award(awards::snack1);
      incJackpot();
    } else if (B(var::snackLit) == kSet) {
      award(awards::snack0);
      incJackpot();
    } else {
      award(awards::snackNope);
    }
    for (u8 light : {lights::snack0, lights::snack1, lights::snack2}) {
      stopBlink(light);
      lightOff(light);
    }
    if (B(var::snackLit + 2) == kSet) {
      if (lightState(lights::partyA) != kSet) {
        setLight(lights::partyA);
        award(awards::partyA);
        checkPartyAll();
      }
      if (B(var::popcorns) <= 1) {
        if (B(var::rightOrbitHoldBonus) != kSet) {
          B(var::rightOrbitHoldBonus) = kSet;
          u8 start = static_cast<u8>(B(var::lightPhaseOrbit) + blinks::orbitBonus);
          if (start >= blinks::orbitPeriod) start = static_cast<u8>(start - blinks::orbitPeriod);
          blink(lights::rightOrbitHoldBonus, start, blinks::orbitBonus);
        }
      } else if (B(var::rightOrbitDoubleBonus) != kSet) {
        B(var::rightOrbitDoubleBonus) = kSet;
        addTimer(timers::doubleBonusBlink);
        blink(lights::rightOrbitDoubleBonus, B(var::lightPhaseOrbit), blinks::orbitBonus);
      }
    }
    B(var::snackLit) = 0;
    B(var::snackLit + 1) = 0;
    B(var::snackLit + 2) = 0;
    addTimer(timers::snacksWait);
  });
  after(timers::doubleBonusBlink, counters::doubleBonusBlink, times::doubleBonusBlink, [this] {
    if (B(var::rightOrbitDoubleBonus) == kSet) {
      stopBlink(lights::rightOrbitDoubleBonus);
      blink(lights::rightOrbitDoubleBonus, 0, blinks::flash);
      addTimer(timers::doubleBonusEnd);
    }
    endTimer();
  });
  after(timers::doubleBonusEnd, counters::doubleBonusEnd, times::doubleBonusEnd, [this] {
    if (B(var::rightOrbitDoubleBonus) == kSet) {
      stopBlink(lights::rightOrbitDoubleBonus);
      B(var::rightOrbitDoubleBonus) = 0;
    }
    endTimer();
  });
  auto held = [this] {  // the ball kept at the top of the left ramp
    B(var::ballHidden) = kSet;
    placeBall(spots::snacks);
    B(var::layer) = layers::ramps;
  };
  bind(timers::snacksWait, [=, this] {
    held();
    if (!countTo(counters::snacksWait, times::snacksWait)) return;
    addTimerNow(timers::snacksRelease);
    endTimer();
  });
  bind(timers::snacksRelease, [=, this] {
    held();
    if (B(var::modeRunning) != kSet && !countTo(counters::snacksRelease, times::snacksRelease)) return;
    effect(sounds::snacksRelease);
    held();
    B(var::ballHidden) = 0;
    W(var::ballVy) = speeds::snacksRelease;
    addTimer(timers::snacksFinish);
    endTimer();
  });
  after(timers::snacksFinish, counters::snacksFinish, times::snacksFinish, [this] {
    B(var::inSnack) = 0;
    endTimer();
  });
  bind(routines::orbitEntryRight, [this] { W(var::timeoutSpringLoop) = 0; });  // the way into the right orbit
  bind(routines::enter, [this] {  // the ball came back down the lane: it is given again
    if (W(var::lastTrigger) != routines::plungerGo) return;
    B(var::playersMayJoin) = 0;
    music(tunes::main);
    B(var::musicNext) = songAt::main;
    startScript(scripts::enter);
    B(var::ballUntouched) = 0;
    B(var::specialPlungerEvent) = 0;
    B(var::partyOn) = 0;
  });
  bind(routines::demon, [this] {  // the demon
    if (B(var::inDemon) == kSet) return;
    B(var::inDemon) = kSet;
    placeBall(spots::demon);
    B(var::ballHidden) = kSet;
    CB(cvar::demonPrizes) = 0;
    modeCountRamp();
    if (demon5M()) ++CB(cvar::demonPrizes);
    if (demonExtraBall()) ++CB(cvar::demonPrizes);
    if (demonJackpot()) ++CB(cvar::demonPrizes);
    if (CB(cvar::demonPrizes) == 0) {
      award(awards::demon250k);
      W(var::demonTimeout) = times::demonNothing;
    }
    addTimer(timers::demonBlink);
  });
  bind(timers::demonBlink, [this] {
    if (!countTo(counters::demonBlink, W(var::demonTimeout))) return;
    blink(lights::demonHead, 0, blinks::dropZone);
    addTimer(timers::demonRelease);
    endTimer();
  });
  after(timers::demonRelease, counters::demonRelease, times::demonRelease, [this] {
    stopBlink(lights::demonHead);
    effect(sounds::issueBall);
    B(var::ballHidden) = 0;
    placeBall(spots::demon);
    W(var::ballVy) = speeds::demonVy;
    W(var::ballVx) = speeds::demonVx;
    B(var::inDemon) = 0;
    endTimer();
  });
  for (u16 at : {routines::laneInnerOther, routines::laneInner})  // the inlanes
    bind(at, [this] {
      award(awards::rollInner);
      effect(sounds::inlane);
    });
  for (u16 at : {routines::laneOuterOther, routines::laneOuter})  // the outlanes
    bind(at, [this] {
      if (lightState(lights::sideExtraBall) == kSet) return sideExtraBall();
      effect(sounds::rollover);
      addScore(var::score, points::k50030);
      scored();
    });
  after(timers::sideExtraBallFinish, counters::sideExtraBallFinish, times::sideExtraBallFinish, [this] {
    B(var::blockDrain) = 0;
    endTimer();
  });
  bind(routines::plungerGo, [this] {  // the ball leaves the plunger lane
    W(var::timeoutSkillShot) = times::skillShot;
    B(var::ballOnPlunger) = 0;
    W(var::timeoutSpringLoop) = times::springLoop;
  });
  bind(routines::plungerBottom, [this] { B(var::ballOnPlunger) = kSet; });
  bind(routines::skyrideTop, [this] {  // the top of the skyride
    if (W(var::lastTrigger) != routines::skyrideRamp) return;
    modeCountRamp();
    raise(gates::skyride);
    if (W(var::timeoutPartyT) != 0) partyT();
    W(var::timeoutPartyT) = times::partyT;
    ++W(var::skyride);
    if (W(var::skyride) >= counts::skyride) {
      award(awards::skyride2);
      blink(lights::skyride0, 0, blinks::flash);
      blink(lights::skyride1, 0, blinks::flash);
      blink(lights::skyride2, 0, blinks::flash);
      addTimer(timers::skyrideUnblink);
      W(var::skyride) = 0;
      if (B(var::rightOrbitMultiBonus) != kSet && lightState(lights::bonusX8) != kSet) {
        B(var::rightOrbitMultiBonus) = kSet;
        blink(lights::rightOrbitMultiBonus, B(var::lightPhaseOrbit), blinks::orbitBonus);
        award(awards::skyrideLitMb);
      }
    } else if (W(var::skyride) == 1) {
      lightOn(lights::skyride0);
      award(awards::skyride0);
    } else {
      award(awards::skyride1);
      lightOn(lights::skyride1);
    }
  });
  after(timers::skyrideUnblink, counters::skyrideUnblink, times::skyrideUnblink, [this] {
    for (u8 light : {lights::skyride0, lights::skyride1, lights::skyride2}) stopBlink(light);
    endTimer();
  });
  bind(routines::skyrideRamp, [this] { drop(gates::skyride); });  // up the skyride

  // the four lanes at the top (pfr: PartySkyridePuke(0), (3), (1), (2))
  struct Lane { u16 roll, lit; u8 light; u16 timer, counter; };
  for (const Lane l : {Lane{routines::skyridePuke0, var::lightStates + lights::puke0, lights::puke0, timers::pukeUnblink0, counters::pukeUnblink0},
                       {routines::skyridePuke3, var::lightStates + lights::puke3, lights::puke3, timers::pukeUnblink3, counters::pukeUnblink3},
                       {routines::skyridePuke1, var::lightStates + lights::puke1, lights::puke1, timers::pukeUnblink1, counters::pukeUnblink1},
                       {routines::skyridePuke2, var::lightStates + lights::puke2, lights::puke2, timers::pukeUnblink2, counters::pukeUnblink2}}) {
    bind(l.roll, [this, l] {
      effect(sounds::rollover);
      if (B(l.lit) != 0) return;
      B(l.lit) = kSet;
      if (pukeScore()) return pukeAll();
      B(var::flipperLockPuke) = kSet;
      blink(l.light, 0, blinks::flash);
      addTimer(l.timer);
    });
    after(l.timer, l.counter, times::pukeUnblink, [this, l] {
      stopBlink(l.light);
      B(var::flipperLockPuke) = 0;
      endTimer();
    });
  }
  after(timers::pukeUnblinkAll, counters::pukeUnblinkAll, times::pukeUnblinkAll, [this] {
    for (u8 light : {lights::puke0, lights::puke3, lights::puke1, lights::puke2}) stopBlink(light);
    for (u8 light : {lights::puke0, lights::puke3, lights::puke1, lights::puke2}) clearLight(light);
    endTimer();
  });
  bind(routines::dummy0, [] {});
  bind(routines::dummy1, [] {});
  bind(routines::rampCyclone, [this] {  // the cyclone ramp
    modeCountRamp();
    if (W(var::timeoutSkillShot) != 0) {
      incJackpot();
      incJackpot();
      if (!amended) award(awards::cycloneSkillShot);  // (the original gives the skill shot's award twice; amended: once)
      addScore(var::cycloneSkillShot, points::k1000000);
      addScore(var::score, var::cycloneSkillShot);
      award(awards::cycloneSkillShot);
      B(var::awardQuiet) = kSet;
      partyR();
    } else if (W(var::timeoutPartyPr) != 0) {
      partyR();
    }
    if (B(var::cycloneX5) == kSet) {
      W(var::cyclones) += counts::cyclonesX5;
      addScore(var::cyclonesDigits, points::k5);
      addScore(var::cycloneBonus, points::k500000);
      award(awards::cycloneX5);
      cycloneX5End();
      cycloneX5End();
    } else {
      do {
        ++W(var::cyclones);
        addScore(var::cyclonesDigits, points::k1);
        addScore(var::cycloneBonus, points::k100000);
      } while (W(var::cyclones) == 1);
      award(awards::cyclone);
      cycloneX5End();
    }
    B(var::awardQuiet) = 0;
  });
  after(timers::happyHour, counters::happyHour, times::modeStart, [this] {
    happyHour();
    endTimer();
  });
  after(timers::megaLaugh, counters::megaLaugh, times::modeStart, [this] {
    megaLaugh();
    endTimer();
  });
}

/// The steps Party Land adds to the display's scripts (cs:2bf4 on).
void PartyLand::bindSteps() {
  auto arg = [this](u16 n) { return nativeW(static_cast<u16>(bx + n)); };
  auto wait1 = [this] {
    W(var::taskState) = 1;
    W(var::task) = code::waitTask;
  };
  bind(routines::setupPartyOn, [=, this] {  // PARTY ON: the ball is served again
    B(var::specialPlungerEvent) = kSet;
    B(text::playerDigit1) = static_cast<u8>(B(var::player) + glyph::digit0);
    wait1();
    nextStep(steps::withWord);
  });
  bind(routines::setupShootAgain, [=, this] {  // shoot again
    B(text::playerDigit2) = static_cast<u8>(B(var::player) + glyph::digit0);
    wait1();
    nextStep(steps::withWord);
  });
  bind(code::haltTask, [] {});
  bind(routines::setSpecialPlungerEvent, [=, this] {
    B(var::specialPlungerEvent) = kSet;
    W(var::taskState) = 1;
    W(var::task) = code::haltTask;
    nextStep(steps::withWord);
  });
  bind(routines::noop, [=, this] {
    wait1();
    nextStep(steps::withWord);
  });
  bind(routines::arcadeReady, [=, this] {  // the arcade's script has got to its prize
    B(var::arcadeReady) = kSet;
    wait1();
    nextStep(steps::withWord);
  });
  bind(routines::waitWhileGameStarting, [=, this] {  // wait while the game is starting
    W(var::task) = code::gameStartingTask;
    W(var::taskState) = tasks::gameStartingFirst;
    nextStep(steps::withWord);
  });
  bind(code::gameStartingTask, [this] { si = B(var::playersMayJoin) == kSet ? tasks::gameStartingStill : 0; });
  bind(routines::jccScoreZero, [=, this] {  // go elsewhere if a score is nought
    bool nought = true;
    for (u16 i = 0; i < kScoreDigits; ++i)
      if (nativeB(static_cast<u16>(arg(steps::arg1) + i)) != 0) nought = false;
    bx = nought ? arg(steps::arg2) : static_cast<u16>(bx + steps::withTwoWords);
    if (nativeW(bx) == 0) {
      W(var::nextStep) = 0;
      return;
    }
    W(var::nextStep) = bx;
    call(nativeW(bx));
  });
  bind(routines::jccNoBonusMult, [=, this] {  // go elsewhere if there is no multiplier
    if (B(var::bonusMultiplier) > 1) {
      wait1();
      nextStep(steps::withWord);
      return;
    }
    bx = arg(steps::arg1);
    W(var::nextStep) = bx;
    call(nativeW(bx));
  });
  bind(routines::multiplyBonus, [=, this] {  // the bonus times its multiplier
    for (u16 i = 0; i < kScoreDigits; ++i) B(var::bonusOnce, i) = B(var::bonus, i);
    for (u8 n = static_cast<u8>(B(var::bonusMultiplier) - 1); n > 0; --n) addScore(var::bonus, var::bonusOnce);
    B(text::bonusX) = static_cast<u8>(B(var::bonusMultiplier) + glyph::digit0);
    wait1();
    nextStep(steps::withWord);
  });
  bind(routines::accBonusModeRamp, [=, this] {  // MEGA LAUGH's score into the bonus
    wait1();
    addScore(var::bonus, var::modeRampScore);
    nextStep(steps::bare);
  });
  bind(routines::accBonusModeHit, [=, this] {  // HAPPY HOUR's
    wait1();
    addScore(var::bonus, var::modeHitScore);
    nextStep(steps::bare);
  });
  bind(routines::accBonusCyclones, [=, this] {  // the cyclones'
    for (u16 i = 0; i < kScoreDigits; ++i) B(var::cycloneBonus, i) = 0;
    wait1();
    u32 times = W(var::cyclones);
    if (times == 0) times = 0x10000;  // as the original's loop does with a count of nought
    for (u32 i = 0; i < times; ++i) addScore(var::cycloneBonus, points::k100000);
    addScore(var::bonus, var::cycloneBonus);
    nextStep(steps::bare);
  });
  bind(routines::secretDropStep, [=, this] {  // the secret's script lets the ball go on
    B(var::scriptDone) = kSet;
    wait1();
    nextStep(steps::withWord);
  });
  bind(routines::extraBall, [=, this] {  // an extra ball
    wait1();
    ++B(var::extraBalls);
    setLight(lights::extraBall);
    nextStep(steps::withWord);
  });
}

}  // namespace encore
