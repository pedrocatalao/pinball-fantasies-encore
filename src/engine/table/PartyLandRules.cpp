// Party Land's rules: what happens when the ball rolls into or hits each thing on the table
// (TABLE1.PRG, cs:1104 to cs:2f9a). Routines are named after what they do, and say where they
// are in the program.
#include "engine/table/PartyLand.h"

namespace encore {

using namespace party;

void PartyLand::addBonus(u16 amount) {
  u16 times = W(var::bonusTimes);
  do addScore(var::bonus, amount);
  while (times-- > 1);
}

void PartyLand::scored() {
  B(var::scoredThisBall) = 0xff;
  W(var::idleFrames) = 0;
  if (W(var::idleShowing) == 0xff) {
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
  return W(var::lightStates + lights::partyP) == 0xffff && W(var::lightStates + lights::partyR) == 0xffff &&
         lightState(lights::partyY) == 0xff;
}

void PartyLand::checkPartyAll() {
  if (!partyAllLit()) return;
  if (B(var::modeRunning) == 0xff) {
    B(var::pendingModeHit) = 0xff;
    return;
  }
  happyHour();
}

void PartyLand::happyHour() {
  award(awards::happyHour);
  B(var::musicNext) = 0x2b;
  for (u8 light = lights::partyP; light <= lights::partyY; ++light) clearLight(light);
  if (B(var::demonJackpot) != 0xff && B(var::demonJackpotTimed) != 0xff) blink(lights::demonJackpot, B(var::lightPhaseDemon), 0x0e);
  B(var::demonJackpotTimed) = 0xff;
  B(var::modeHit) = 0xff;
  blink(lights::happyHour, 0, 8);
  B(var::modeRunning) = 0xff;
  B(var::pendingMode) = 0xff;
}

void PartyLand::checkCrazyAll(bool carry) {
  if (!carry) return;
  if (B(var::modeRunning) == 0xff) {
    B(var::pendingModeRamp) = 0xff;
    return;
  }
  megaLaugh();
}

void PartyLand::megaLaugh() {
  award(awards::megaLaugh);
  B(var::musicNext) = 0x19;
  if (B(var::demonJackpot) != 0xff && B(var::demonJackpotTimed) != 0xff) blink(lights::demonJackpot, B(var::lightPhaseDemon), 0x0e);
  B(var::demonJackpotTimed) = 0xff;
  B(var::modeRamp) = 0xff;
  blink(lights::megaLaugh, 0, 8);
  B(var::modeRunning) = 0xff;
  B(var::pendingMode) = 0xff;
}

void PartyLand::duckScore() {
  effect(sounds::hitDuck);
  addScore(var::score, points::k7510);
  modeCountHit();
  addBonus(points::k750);
  B(var::scoredThisBall) = 0xff;
}

/// cs:156d: true when all three drop targets are down.
bool PartyLand::duckAll() {
  if (lightState(lights::duckDrop0) != 0 || lightState(lights::duckDrop1) != 0 || lightState(lights::duckDrop2) != 0) return false;
  award(awards::duckAll);
  B(var::duckHit) = 0xff;
  B(var::duckHit + 1) = 0xff;
  for (u8 light : {lights::duck0, lights::duck1, lights::duck2}) stopBlink(light);
  for (u8 light : {lights::duck0, lights::duck1, lights::duck2}) blink(light, 0, 2);
  addTimer(timers::duckAllUnblink);
  if (B(var::snackLit, B(var::snack)) != 0xff) {
    if (B(var::snack) == 1) {
      u8 start = static_cast<u8>(B(var::lightPhaseSnack) + 8);
      if (start >= 16) start = static_cast<u8>(start - 16);
      blink(lights::snack1, start, 8);
      B(var::snackLit + 1) = 0xff;
    } else if (B(var::snack) > 1) {
      blink(lights::snack2, B(var::lightPhaseSnack), 8);
      B(var::snackLit + 2) = 0xff;
    } else {
      blink(lights::snack0, B(var::lightPhaseSnack), 8);
      B(var::snackLit) = 0xff;
    }
  }
  if (++B(var::snack) == 3) B(var::snack) = 0;
  return true;
}

bool PartyLand::crazyLetter() {
  incJackpot();
  B(var::crazyShown) = award(B(var::crazyFromArcade) == 0xff ? awards::arcadeCrazy : awards::orbitCrazy) ? 1 : 0;
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
  if (lightState(lights::partyP) == 0xff) return;
  award(awards::partyP);
  setLight(lights::partyP);
  checkPartyAll();
}

/// What the arcade gives. The original goes by a count of the frames played, through a list
/// of 128 (ds:05b4), and gives it a frame later. Amended: one of the six by chance proper,
/// each as likely as the others, and at once.
void PartyLand::arcadePickReward() {
  if (!amended) return addTimer(W(var::arcadePrizes, B(var::arcadeList, static_cast<u16>(B(var::frames) >> 1))));
  static constexpr u16 kPrizes[6] = {code::arcadeSideExtraBall, code::arcadeCrazy, code::arcade1M, code::arcade5M, code::arcade500k, code::arcadeNoScore};
  const u16 running = W(var::timerSlot);
  addTimer(kPrizes[chance(6, 0)]);
  W(var::timerSlot) = bx;
  call(nativeW(bx));  // (each of them ends itself)
  W(var::timerSlot) = running;
}

void PartyLand::arcadeNotShown() {
  W(var::dropZoneDelay) = 0x0a;
  addTimer(timers::dropZoneStart);
  endTimer();
}

bool PartyLand::demon5M() {
  if (B(var::demon5M) != 0xff) return false;
  award(awards::demon5M);
  stopBlink(lights::demon5M);
  lightOff(lights::demon5M);
  B(var::demon5M) = 0;
  W(var::demonTimeout) = B(var::modeRunning) == 0xff ? 0x0f : 0xa0;
  return true;
}

bool PartyLand::demonExtraBall() {
  if (B(var::demonExtraBall) != 0xff) return false;
  stopBlink(lights::demonExtraBall);
  lightOff(lights::demonExtraBall);
  setLight(lights::extraBall);
  ++B(var::extraBalls);
  award(awards::demonExtraBall);
  B(var::demonExtraBall) = 0;
  W(var::demonTimeout) = B(var::modeRunning) == 0xff ? 0x0f : 0x140;
  return true;
}

bool PartyLand::demonJackpot() {
  if (B(var::demonJackpot) != 0xff && B(var::demonJackpotTimed) != 0xff) return false;
  B(var::demonJackpot) = 0;
  B(var::demonJackpotTimed) = 0;
  stopBlink(lights::demonJackpot);
  lightOff(lights::demonJackpot);
  music(tunes::jackpot);
  startScript(B(var::modeRunning) != 0xff ? scripts::jackpot : B(var::modeHit) != 0xff ? scripts::jackpotModeRamp : scripts::jackpotModeHit);
  addScore(var::score, var::jackpot);
  for (u16 i = 0; i < 12; ++i) B(var::jackpot, i) = B(var::jackpotStart, i);
  W(var::demonTimeout) = 0x19a;
  return true;
}

void PartyLand::partyT() {
  if (lightState(lights::partyT) == 0xff) return;
  setLight(lights::partyT);
  award(awards::partyT);
  checkPartyAll();
}

bool PartyLand::pukeScore() {
  addScore(var::score, points::k20070);
  scored();
  addBonus(points::k1000);
  incJackpot();
  return lightState(lights::puke0) == 0xff && lightState(lights::puke3) == 0xff && lightState(lights::puke1) == 0xff &&
         lightState(lights::puke2) == 0xff;
}

/// cs:28f5: all four lanes lit.
void PartyLand::pukeAll() {
  auto later = [](u8 start, u8 half) { start = static_cast<u8>(start + half); return start >= half * 2 ? static_cast<u8>(start - half * 2) : start; };
  blink(lights::puke0, B(var::lightPhasePuke), 2);
  blink(lights::puke2, B(var::lightPhasePuke), 2);
  blink(lights::puke3, later(B(var::lightPhasePuke), 2), 2);
  blink(lights::puke1, later(B(var::lightPhasePuke), 2), 2);
  addTimer(timers::pukeUnblinkAll);
  ++W(var::demonReward);
  if (W(var::demonReward) == 1) {
    if (B(var::demon5M) != 0xff) {
      blink(lights::demon5M, B(var::lightPhaseDemon), 0x0e);
      B(var::demon5M) = 0xff;
    }
  } else if (W(var::demonReward) == 2) {
    if (B(var::demonExtraBall) != 0xff) {
      blink(lights::demonExtraBall, later(B(var::lightPhaseDemon), 0x0e), 0x0e);
      B(var::demonExtraBall) = 0xff;
    }
  } else if (W(var::demonReward) == 3 && B(var::demonJackpot) != 0xff) {
    if (B(var::demonJackpotTimed) != 0xff) blink(lights::demonJackpot, B(var::lightPhaseDemon), 0x0e);
    B(var::demonJackpot) = 0xff;
  }
  if (lightState(lights::partyY) != 0xff) {
    setLight(lights::partyY);
    award(awards::partyY);
    checkPartyAll();
  }
}

void PartyLand::partyR() {
  if (lightState(lights::partyR) != 0xff) {
    setLight(lights::partyR);
    award(awards::partyR);
    checkPartyAll();
  }
  cycloneX5End();
}

void PartyLand::modeCountHit() {
  if (B(var::modeHit) != 0xff) return;
  addScore(var::modeHitScore, points::k1000000);
  B(var::modeScored) = 0xff;
}

void PartyLand::modeCountRamp() {
  if (B(var::modeRamp) != 0xff) return;
  addScore(var::modeRampScore, points::k5000000);
  B(var::modeScored) = 0xff;
}

void PartyLand::sideExtraBall() {
  clearLight(lights::sideExtraBall);
  award(awards::sideExtraBall);
  setLight(lights::extraBall);
  ++B(var::extraBalls);
  B(var::blockDrain) = 0xff;
  addTimer(timers::sideExtraBallFinish);
}

void PartyLand::modeCheck() {
  const bool nine = B(var::countdownTens) == 0x2a && B(var::countdownUnits) == 0x37 + 2,
             nought = B(var::countdownTens) == 0x2a && B(var::countdownUnits) == 0x37;
  if (nine && B(var::demonJackpot) != 0xff && B(var::demonJackpotTimed) == 0xff) {
    stopBlink(lights::demonJackpot);
    blink(lights::demonJackpot, 0, 2);
  }
  if (!nought) return;
  const bool first = B(var::modeHit) == 0xff;
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
  B(var::musicNext) = 1;
  stopBlink(lights::demonJackpot);
  lightOff(lights::demonJackpot);
  B(var::demonJackpotTimed) = 0;
  if (B(var::demonJackpot) == 0xff) blink(lights::demonJackpot, B(var::lightPhaseDemon), 0x0e);
  if (first) {
    if (B(var::pendingModeRamp) == 0xff) {
      B(var::pendingModeRamp) = 0;
      addTimer(timers::megaLaugh);
    }
  } else if (B(var::pendingModeHit) == 0xff) {
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
    if (B(var::modeRunning) != 0xff && !countTo(counters::dropZoneStart, W(var::dropZoneDelay))) return;
    startDropZone();
    endTimer();
  });
  after(timers::dropZoneWait, counters::dropZoneWait, 0x1e, [this] {
    blink(lights::dropZone0, 0, 7);
    blink(lights::dropZone1, 0, 7);
    addTimer(timers::dropZoneRelease);
    endTimer();
  });
  after(timers::dropZoneRelease, counters::dropZoneRelease, 0x1b, [this] {  // out of the hole, upwards on the ramps' side
    W(var::lookAt) = 0xffff;
    const u16 speed = chance(0x80, W(var::loopCounter) & 0x7f);
    hole();
    B(var::layer) = 0xff;
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
    W(var::dropZoneScroll) -= 5;
    if (W(var::dropZoneScroll).s() < 0) {
      W(var::lookAt) = 0;
      endTimer();
    } else {
      W(var::lookAt) = W(var::dropZoneScroll);
    }
  });
  bind(routines::arcadeButton, [this] {  // the arcade's button
    if (B(var::arcadeButtonHit) == 0xff) return;
    effect(sounds::arcadeButton);
    B(var::arcadeButtonHit) = 0xff;
    addTimer(timers::resetArcadeButton);
    if (B(var::arcadeOpen) == 0xff) return;
    B(var::arcadeOpen) = 0xff;
    blink(lights::arcade0, 0, 0x0c);
    blink(lights::arcade1, 0, 0x0c);
  });
  after(timers::resetArcadeButton, counters::resetArcadeButton, 0x14, [this] {
    B(var::arcadeButtonHit) = 0;
    endTimer();
  });

  // the three drop targets, the ducks
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
      if (B(t.lit) != 0xff || B(t.down) == 0xff) return;
      B(t.down) = 0xff;
      clearLight(static_cast<u8>(t.light));
      addTimer(t.fall);
      duckScore();
      if (duckAll()) return;
      addTimer(t.flashTimer);
      blink(static_cast<u8>(t.flash), 0, 3);
      CB(t.flashCount) = 0;
    });
    bind(t.flashTimer, [this, t] {
      if (CB(t.flashCount) == 0x0d) {
        stopBlink(static_cast<u8>(t.flash));
        B(t.down) = 0;
        dropTimer();
      }
      ++CB(t.flashCount);
    });
    after(t.fall, t.counter, 0x14, [this, t] {  // it goes down: no longer in the ball's way
      patchMask(at::wallsGround, t.at, t.shape, t.width, 0x0f);
      endTimer();
    });
  }
  after(timers::duckAllUnblink, counters::duckAllUnblink, 0x47, [this] {  // and all three stand again
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

  bind(routines::orbitTopLeft, [this] {  // the top of the left orbit: the right orbit made
    if (W(var::lastTrigger) != routines::orbitTopRight) return;
    if (W(var::timeoutSpringLoop) != 0) {
      W(var::timeoutSpringLoop) = 0;
      return;
    }
    if (B(var::orbitRightBlinking) == 0xff) return;
    incJackpot();
    modeCountRamp();
    if (W(var::timeoutPartyT) != 0) partyT();
    W(var::timeoutPartyT) = 0x258;
    W(var::timeoutPartyPr) = 0x12c;
    ++B(var::orbitRightCycle);
    if (B(var::orbitRightCycle) < 2) {
      stopBlink(lights::orbit250k);
      lightOn(lights::orbit250k);
      blink(lights::orbit500k, 0, 9);
      award(awards::orbit250k);
    } else if (B(var::orbitRightCycle) == 2) {
      stopBlink(lights::orbit500k);
      lightOn(lights::orbit500k);
      blink(lights::orbit750k, 0, 9);
      award(awards::orbit500k);
    } else {
      B(var::orbitRightCycle) = 0;
      stopBlink(lights::orbit750k);
      blink(lights::orbit750k, 0, 2);
      blink(lights::orbit500k, 0, 2);
      blink(lights::orbit250k, 0, 2);
      award(awards::orbit750k);
      addTimer(timers::orbitRightUnblink);
      B(var::orbitRightBlinking) = 0xff;
    }
    if (B(var::rightOrbitMultiBonus) == 0xff) {
      B(var::rightOrbitMultiBonus) = 0;
      stopBlink(lights::rightOrbitMultiBonus);
      lightOff(lights::rightOrbitMultiBonus);
      struct Step { u16 lit; u8 light; u16 prize, times; };
      for (const Step s : {Step{var::lightStates + lights::bonusX2, lights::bonusX2, awards::orbitMb2, 2},
                           {var::lightStates + lights::bonusX4, lights::bonusX4, awards::orbitMb4, 4},
                           {var::lightStates + lights::bonusX6, lights::bonusX6, awards::orbitMb6, 6},
                           {var::lightStates + lights::bonusX8, lights::bonusX8, awards::orbitMb8, 8}}) {
        if (B(s.lit) == 0xff) continue;
        setLight(s.light);
        award(s.prize);
        W(var::bonusTimes) = s.times;
        B(var::bonusMultiplier) += 2;
        if (B(var::bonusMultiplier) == 3) --B(var::bonusMultiplier);
        if (++B(text::multiplier) != 0x39) ++B(text::multiplier);
        break;
      }
    }
    if (B(var::rightOrbitHoldBonus) == 0xff) {
      B(var::rightOrbitHoldBonus) = 0;
      stopBlink(lights::rightOrbitHoldBonus);
      lightOff(lights::rightOrbitHoldBonus);
      award(awards::orbitHoldBonus);
      B(var::holdBonus) = 0xff;
    }
    if (B(var::rightOrbitDoubleBonus) == 0xff) {
      B(var::rightOrbitDoubleBonus) = 0;
      stopBlink(lights::rightOrbitDoubleBonus);
      lightOff(lights::rightOrbitDoubleBonus);
      award(awards::orbitDoubleBonus);
      addScore(var::bonus, var::bonus);
    }
  });
  after(timers::orbitRightUnblink, counters::orbitRightUnblink, 0x78, [this] {
    for (u8 light : {lights::orbit750k, lights::orbit500k, lights::orbit250k}) stopBlink(light);
    blink(lights::orbit250k, 0, 9);
    B(var::orbitRightBlinking) = 0;
    endTimer();
  });
  bind(routines::orbitTopRight, [this] {  // the top of the right orbit: the left orbit made
    if (W(var::lastTrigger) != routines::orbitTopLeft || B(var::madBlinking) == 0xff) return;
    incJackpot();
    modeCountRamp();
    if (W(var::timeoutPartyT) != 0) partyT();
    W(var::timeoutPartyT) = 0x258;
    if (lightState(lights::mad0) != 0xff) {
      award(awards::orbitMad0);
      blink(lights::mad0, 0, 2);
      addTimer(timers::madUnblink0);
    } else if (lightState(lights::mad1) != 0xff) {
      award(awards::orbitMad1);
      blink(lights::mad1, 0, 2);
      addTimer(timers::madUnblink1);
    } else {
      lightState(lights::mad0) = 0;
      lightState(lights::mad1) = 0;
      blink(lights::mad0, 0, 2);
      blink(lights::mad1, 0, 2);
      blink(lights::mad2, 0, 2);
      addTimer(timers::madAllUnblink);
      B(var::madBlinking) = 0xff;
      award(awards::orbitMad2);
      B(var::crazyFromArcade) = 0;
      checkCrazyAll(crazyLetter());
    }
  });
  after(timers::madUnblink0, counters::madUnblink0, 0x0e, [this] {
    stopBlink(lights::mad0);
    lightState(lights::mad0) = 0xff;
    endTimer();
  });
  after(timers::madUnblink1, counters::madUnblink1, 0x0e, [this] {
    stopBlink(lights::mad1);
    lightState(lights::mad1) = 0xff;
    endTimer();
  });
  after(timers::madAllUnblink, counters::madAllUnblink, 0x78, [this] {
    for (u8 light : {lights::mad0, lights::mad1, lights::mad2}) stopBlink(light);
    B(var::madBlinking) = 0;
    endTimer();
  });

  bind(routines::secret, [this] {  // the secret
    B(var::scriptDone) = 0;
    award(awards::secret);
    incJackpot();
    blink(lights::cycloneX5, 0, 6);
    B(var::cycloneX5) = 0xff;
    addTimer(timers::secretDrop);
    hole();
  });
  bind(routines::secretTilt, [this] {  // the same, tilted
    hole();
    startDropZone();
    stopBlink(lights::cycloneX5);
  });
  bind(timers::secretDrop, [this] {
    if (B(var::scriptDone) != 0xff) return;
    addTimer(timers::cycloneX5Blink);
    startDropZone();
    endTimer();
  });
  after(timers::cycloneX5Blink, counters::cycloneX5Blink, 0x1e0, [this] {
    stopBlink(lights::cycloneX5);
    if (B(var::cycloneX5) != 0) {
      blink(lights::cycloneX5, 0, 2);
      addTimer(timers::cycloneX5End);
    } else {
      lightOff(lights::cycloneX5);
    }
    endTimer();
  });
  after(timers::cycloneX5End, counters::cycloneX5End, 0x78, [this] {
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
      B(var::awardQuiet) = 0xff;
      partyP();
    } else if (W(var::timeoutPartyPr) != 0) {
      partyP();
    }
    if (lightState(lights::tunnel3M) == 0xff) {
      award(awards::tunnel5M);
      W(var::timeoutTunnel) = 0x5a0;
    } else if (lightState(lights::tunnel1M) == 0xff) {
      award(awards::tunnel3M);
      W(var::timeoutTunnel) = 0x5a0;
      stopBlink(lights::tunnel3M);
      setLight(lights::tunnel3M);
      blink(lights::tunnel5M, 0, 8);
    } else {
      award(awards::tunnel1M);
      W(var::timeoutTunnel) = 0x2d0;
      stopBlink(lights::tunnel1M);
      setLight(lights::tunnel1M);
      blink(lights::tunnel3M, 0, 8);
    }
    W(var::tunnelDelay) = 0x82;
    startDropZoneScroll();
    addTimer(timers::tunnelDropZoneStart);
    if (B(var::modeRunning) != 0xff) addTimer(timers::tunnelFreeze);
    B(var::awardQuiet) = 0;
  });
  bind(timers::tunnelDropZoneStart, [this] {
    if (B(var::modeRunning) != 0xff && !countTo(counters::tunnelDropZoneStart, W(var::tunnelDelay))) return;
    W(var::lookAt) = 0xffff;
    startDropZone();
    endTimer();
  });
  after(timers::tunnelFreeze, counters::tunnelFreeze, 2, [this] {
    hole();
    endTimer();
  });
  bind(routines::tunnelTilt, [this] {  // the tunnel, tilted
    hole();
    startDropZone();
  });
  bind(routines::arcade, [this] {  // the arcade
    B(var::frames) += 0x15;
    modeCountRamp();
    if (B(var::tilted) == 0xff || B(var::arcadeOpen) != 0xff) {
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
    W(var::jingleTimeout) = 0x19;
    if (!award(awards::arcade)) {
      B(var::arcadeReady) = 0xff;
      arcadePickReward();
      B(var::musicKept) = B(var::musicNext);
    } else {
      B(var::musicNext) = 0x3e;
      // (amended: the arcade's jingle keeps its own priority while it plays)
      if (!amended) B(var::musicPriority) = 1;
      B(var::jingleEndedRules) = 0;
      B(var::jingleEnded) = 0;
      addTimer(timers::arcadePickReward);
    }
    startDropZoneScroll();
    hole();
  });
  bind(timers::arcadePickReward, [this] {
    if (B(var::modeRunning) != 0xff && B(var::arcadeReady) != 0xff) return;
    if (B(var::tilted) == 0xff) {
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
    W(var::dropZoneDelay) = 0xa0;
    addTimer(timers::dropZoneStart);
    endTimer();
  });
  bind(code::arcadeCrazy, [this] {
    B(var::crazyFromArcade) = 0xff;
    checkCrazyAll(crazyLetter());
    if (B(var::crazyShown) == 0) return arcadeNotShown();
    B(var::musicNext) = B(var::musicKept);
    if (!amended) B(var::musicPriority) = 0;  // (amended: a prize's jingle keeps its priority)
    W(var::arcadeTimeoutSoft) = 0x8c;
    W(var::arcadeTimeoutHard) = 0xb4;
    B(var::arcadeJingleDone) = 0;
    addTimer(timers::arcadeDropZoneStart);
    endTimer();
  });
  bind(code::arcade1M, [=] { prize(awards::arcade1M, 0x78, 0x96); });
  bind(code::arcade5M, [=] { prize(awards::arcade5M, 0x6e, 0x8c); });
  bind(code::arcade500k, [=] { prize(awards::arcade500k, 0x2d, 0x46); });
  bind(code::arcadeNoScore, [this] {
    if (award(awards::arcadeNoScore)) {
      B(var::musicNext) = B(var::musicKept);
      if (!amended) B(var::musicPriority) = 0;
    }
    W(var::dropZoneDelay) = 0x2d;
    addTimer(timers::dropZoneStart);
    endTimer();
  });
  bind(timers::arcadeDropZoneStart, [this] {
    bool done = --W(var::arcadeTimeoutHard) == 0;
    if (amended) {
      // The original lets the ball go when the prize's jingle is over and its time on the
      // display is up. Amended: when its time is up, once the jingle has been heard to play;
      // a long jingle no longer keeps the ball back.
      if (!done && B(var::modeRunning) != 0xff) {
        if (W(var::arcadeTimeoutSoft) != 0) --W(var::arcadeTimeoutSoft);
        if (B(var::musicRepeats) == 0 && B(var::arcadeJingleDone) != 0xff) return;
        B(var::arcadeJingleDone) = 0xff;
        if (W(var::arcadeTimeoutSoft) != 0) return;
      }
    } else if (!done) {
      if (W(var::arcadeTimeoutSoft) != 0) --W(var::arcadeTimeoutSoft);
      if (B(var::arcadeJingleDone) != 0xff) {
        if (B(var::modeRunning) == 0xff) done = true;
        else if (B(var::jingleEndedRules) != 0xff) return;
      }
      if (!done) {
        B(var::arcadeJingleDone) = 0xff;
        if (W(var::arcadeTimeoutSoft) != 0) return;
      }
    }
    B(var::jingleEndedRules) = 0;
    W(var::arcadeTimeoutSoft) = 0xffff;
    W(var::arcadeTimeoutHard) = 0x0a;
    startDropZone();
    endTimer();
  });

  bind(routines::rampSnack, [this] {  // the snack ramp
    if (B(var::inSnack) == 0xff) return;
    B(var::inSnack) = 0xff;
    addScore(var::score, points::k50000);
    scored();
    addBonus(points::k5000);
    modeCountRamp();
    if (B(var::snackLit + 2) == 0xff) {
      award(awards::snack2);
      incJackpot();
      ++B(var::popcorns);
    } else if (B(var::snackLit + 1) == 0xff) {
      award(awards::snack1);
      incJackpot();
    } else if (B(var::snackLit) == 0xff) {
      award(awards::snack0);
      incJackpot();
    } else {
      award(awards::snackNope);
    }
    for (u8 light : {lights::snack0, lights::snack1, lights::snack2}) {
      stopBlink(light);
      lightOff(light);
    }
    if (B(var::snackLit + 2) == 0xff) {
      if (lightState(lights::partyA) != 0xff) {
        setLight(lights::partyA);
        award(awards::partyA);
        checkPartyAll();
      }
      if (B(var::popcorns) <= 1) {
        if (B(var::rightOrbitHoldBonus) != 0xff) {
          B(var::rightOrbitHoldBonus) = 0xff;
          u8 start = static_cast<u8>(B(var::lightPhaseOrbit) + 0x0c);
          if (start >= 0x18) start = static_cast<u8>(start - 0x18);
          blink(lights::rightOrbitHoldBonus, start, 0x0c);
        }
      } else if (B(var::rightOrbitDoubleBonus) != 0xff) {
        B(var::rightOrbitDoubleBonus) = 0xff;
        addTimer(timers::doubleBonusBlink);
        blink(lights::rightOrbitDoubleBonus, B(var::lightPhaseOrbit), 0x0c);
      }
    }
    B(var::snackLit) = 0;
    B(var::snackLit + 1) = 0;
    B(var::snackLit + 2) = 0;
    addTimer(timers::snacksWait);
  });
  after(timers::doubleBonusBlink, counters::doubleBonusBlink, 0x1e0, [this] {
    if (B(var::rightOrbitDoubleBonus) == 0xff) {
      stopBlink(lights::rightOrbitDoubleBonus);
      blink(lights::rightOrbitDoubleBonus, 0, 2);
      addTimer(timers::doubleBonusEnd);
    }
    endTimer();
  });
  after(timers::doubleBonusEnd, counters::doubleBonusEnd, 0x78, [this] {
    if (B(var::rightOrbitDoubleBonus) == 0xff) {
      stopBlink(lights::rightOrbitDoubleBonus);
      B(var::rightOrbitDoubleBonus) = 0;
    }
    endTimer();
  });
  auto held = [this] {  // the ball kept at the top of the left ramp
    B(var::ballHidden) = 0xff;
    placeBall(spots::snacks);
    B(var::layer) = 0xff;
  };
  bind(timers::snacksWait, [=, this] {
    held();
    if (!countTo(counters::snacksWait, 0x28)) return;
    addTimerNow(timers::snacksRelease);
    endTimer();
  });
  bind(timers::snacksRelease, [=, this] {
    held();
    if (B(var::modeRunning) != 0xff && !countTo(counters::snacksRelease, 0x5a)) return;
    effect(sounds::snacksRelease);
    held();
    B(var::ballHidden) = 0;
    W(var::ballVy) = 0xf63c;
    addTimer(timers::snacksFinish);
    endTimer();
  });
  after(timers::snacksFinish, counters::snacksFinish, 0x3c, [this] {
    B(var::inSnack) = 0;
    endTimer();
  });
  bind(routines::orbitEntryRight, [this] { W(var::timeoutSpringLoop) = 0; });  // the way into the right orbit
  bind(routines::enter, [this] {  // the ball came back down the lane: it is given again
    if (W(var::lastTrigger) != routines::plungerGo) return;
    B(var::playersMayJoin) = 0;
    music(tunes::main);
    B(var::musicNext) = 1;
    startScript(scripts::enter);
    B(var::ballUntouched) = 0;
    B(var::specialPlungerEvent) = 0;
    B(var::partyOn) = 0;
  });
  bind(routines::demon, [this] {  // the demon
    if (B(var::inDemon) == 0xff) return;
    B(var::inDemon) = 0xff;
    placeBall(spots::demon);
    B(var::ballHidden) = 0xff;
    CB(cvar::demonPrizes) = 0;
    modeCountRamp();
    if (demon5M()) ++CB(cvar::demonPrizes);
    if (demonExtraBall()) ++CB(cvar::demonPrizes);
    if (demonJackpot()) ++CB(cvar::demonPrizes);
    if (CB(cvar::demonPrizes) == 0) {
      award(awards::demon250k);
      W(var::demonTimeout) = 0x55;
    }
    addTimer(timers::demonBlink);
  });
  bind(timers::demonBlink, [this] {
    if (!countTo(counters::demonBlink, W(var::demonTimeout))) return;
    blink(lights::demonHead, 0, 7);
    addTimer(timers::demonRelease);
    endTimer();
  });
  after(timers::demonRelease, counters::demonRelease, 0x1b, [this] {
    stopBlink(lights::demonHead);
    effect(sounds::issueBall);
    B(var::ballHidden) = 0;
    placeBall(spots::demon);
    W(var::ballVy) = 0x627;
    W(var::ballVx) = 0xfdc1;
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
      if (lightState(lights::sideExtraBall) == 0xff) return sideExtraBall();
      effect(sounds::rollover);
      addScore(var::score, points::k50030);
      scored();
    });
  after(timers::sideExtraBallFinish, counters::sideExtraBallFinish, 0x258, [this] {
    B(var::blockDrain) = 0;
    endTimer();
  });
  bind(routines::plungerGo, [this] {  // the ball leaves the plunger lane
    W(var::timeoutSkillShot) = 0x12c;
    B(var::ballOnPlunger) = 0;
    W(var::timeoutSpringLoop) = 0x78;
  });
  bind(routines::plungerBottom, [this] { B(var::ballOnPlunger) = 0xff; });
  bind(routines::skyrideTop, [this] {  // the top of the skyride
    if (W(var::lastTrigger) != routines::skyrideRamp) return;
    modeCountRamp();
    raise(gates::skyride);
    if (W(var::timeoutPartyT) != 0) partyT();
    W(var::timeoutPartyT) = 0x258;
    ++W(var::skyride);
    if (W(var::skyride) >= 3) {
      award(awards::skyride2);
      blink(lights::skyride0, 0, 2);
      blink(lights::skyride1, 0, 2);
      blink(lights::skyride2, 0, 2);
      addTimer(timers::skyrideUnblink);
      W(var::skyride) = 0;
      if (B(var::rightOrbitMultiBonus) != 0xff && lightState(lights::bonusX8) != 0xff) {
        B(var::rightOrbitMultiBonus) = 0xff;
        blink(lights::rightOrbitMultiBonus, B(var::lightPhaseOrbit), 0x0c);
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
  after(timers::skyrideUnblink, counters::skyrideUnblink, 0x78, [this] {
    for (u8 light : {lights::skyride0, lights::skyride1, lights::skyride2}) stopBlink(light);
    endTimer();
  });
  bind(routines::skyrideRamp, [this] { drop(gates::skyride); });  // up the skyride

  // the four lanes at the top
  struct Lane { u16 roll, lit; u8 light; u16 timer, counter; };
  for (const Lane l : {Lane{routines::skyridePuke0, var::lightStates + lights::puke0, lights::puke0, timers::pukeUnblink0, counters::pukeUnblink0},
                       {routines::skyridePuke3, var::lightStates + lights::puke3, lights::puke3, timers::pukeUnblink3, counters::pukeUnblink3},
                       {routines::skyridePuke1, var::lightStates + lights::puke1, lights::puke1, timers::pukeUnblink1, counters::pukeUnblink1},
                       {routines::skyridePuke2, var::lightStates + lights::puke2, lights::puke2, timers::pukeUnblink2, counters::pukeUnblink2}}) {
    bind(l.roll, [this, l] {
      effect(sounds::rollover);
      if (B(l.lit) != 0) return;
      B(l.lit) = 0xff;
      if (pukeScore()) return pukeAll();
      B(var::flipperLockPuke) = 0xff;
      blink(l.light, 0, 2);
      addTimer(l.timer);
    });
    after(l.timer, l.counter, 0x0d, [this, l] {
      stopBlink(l.light);
      B(var::flipperLockPuke) = 0;
      endTimer();
    });
  }
  after(timers::pukeUnblinkAll, counters::pukeUnblinkAll, 0x64, [this] {
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
      B(var::awardQuiet) = 0xff;
      partyR();
    } else if (W(var::timeoutPartyPr) != 0) {
      partyR();
    }
    if (B(var::cycloneX5) == 0xff) {
      W(var::cyclones) += 5;
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
  after(timers::happyHour, counters::happyHour, 0x190, [this] {
    happyHour();
    endTimer();
  });
  after(timers::megaLaugh, counters::megaLaugh, 0x190, [this] {
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
    B(var::specialPlungerEvent) = 0xff;
    B(text::playerDigit1) = static_cast<u8>(B(var::player) + 0x37);
    wait1();
    nextStep(4);
  });
  bind(routines::setupShootAgain, [=, this] {  // shoot again
    B(text::playerDigit2) = static_cast<u8>(B(var::player) + 0x37);
    wait1();
    nextStep(4);
  });
  bind(code::haltTask, [] {});
  bind(routines::setSpecialPlungerEvent, [=, this] {
    B(var::specialPlungerEvent) = 0xff;
    W(var::taskState) = 1;
    W(var::task) = code::haltTask;
    nextStep(4);
  });
  bind(routines::noop, [=, this] {
    wait1();
    nextStep(4);
  });
  bind(routines::arcadeReady, [=, this] {  // the arcade's script has got to its prize
    B(var::arcadeReady) = 0xff;
    wait1();
    nextStep(4);
  });
  bind(routines::waitWhileGameStarting, [=, this] {  // wait while the game is starting
    W(var::task) = code::gameStartingTask;
    W(var::taskState) = 0x2c;
    nextStep(4);
  });
  bind(code::gameStartingTask, [this] { si = B(var::playersMayJoin) == 0xff ? 0x22 : 0; });
  bind(routines::jccScoreZero, [=, this] {  // go elsewhere if a score is nought
    bool nought = true;
    for (u16 i = 0; i < 12; ++i)
      if (nativeB(static_cast<u16>(arg(2) + i)) != 0) nought = false;
    bx = nought ? arg(4) : static_cast<u16>(bx + 6);
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
      nextStep(4);
      return;
    }
    bx = arg(2);
    W(var::nextStep) = bx;
    call(nativeW(bx));
  });
  bind(routines::multiplyBonus, [=, this] {  // the bonus times its multiplier
    for (u16 i = 0; i < 12; ++i) B(var::bonusOnce, i) = B(var::bonus, i);
    for (u8 n = static_cast<u8>(B(var::bonusMultiplier) - 1); n > 0; --n) addScore(var::bonus, var::bonusOnce);
    B(text::bonusX) = static_cast<u8>(B(var::bonusMultiplier) + 0x37);
    wait1();
    nextStep(4);
  });
  bind(routines::accBonusModeRamp, [=, this] {  // MEGA LAUGH's score into the bonus
    wait1();
    addScore(var::bonus, var::modeRampScore);
    nextStep(2);
  });
  bind(routines::accBonusModeHit, [=, this] {  // HAPPY HOUR's
    wait1();
    addScore(var::bonus, var::modeHitScore);
    nextStep(2);
  });
  bind(routines::accBonusCyclones, [=, this] {  // the cyclones'
    for (u16 i = 0; i < 12; ++i) B(var::cycloneBonus, i) = 0;
    wait1();
    u32 times = W(var::cyclones);
    if (times == 0) times = 0x10000;  // as the original's loop does with a count of nought
    for (u32 i = 0; i < times; ++i) addScore(var::cycloneBonus, points::k100000);
    addScore(var::bonus, var::cycloneBonus);
    nextStep(2);
  });
  bind(routines::secretDropStep, [=, this] {  // the secret's script lets the ball go on
    B(var::scriptDone) = 0xff;
    wait1();
    nextStep(4);
  });
  bind(routines::extraBall, [=, this] {  // an extra ball
    wait1();
    ++B(var::extraBalls);
    setLight(lights::extraBall);
    nextStep(4);
  });
}

}  // namespace encore
