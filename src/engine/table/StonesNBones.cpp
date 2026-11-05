#include "engine/table/StonesNBones.h"

namespace encore {

using namespace stones;

StonesNBones::StonesNBones(ByteView prg) : Flow(prg, kTableIndex) {
  bindMatch();
  bindRules();
  bindSteps();
  bindNative(routines::tableStart, [this] { lightsOut(); });
  bindNative(routines::tableNewGame, [this] {  // a new game
    lightsOut();
    stopBlinks();
    copy12(var::jackpot, var::jackpotStart);
    clearBall();
    for (u16 player = 0; player < 8; ++player) savePlayer(static_cast<u16>(player * rec::size));
    b(var::extraBalls) = 0;
    b(var::matchWon) = 0;
    b(var::setAtNewGame) = kSet;
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
    addTimer(timers::drainSfx.routine);
    b(var::modeRunning) = 0;
    out(lights::grimReaper);
    b(var::musicPriority) = 0;
    award(awards::drained);
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
    music(tunes::tilt);
    b(var::musicNext) = songAt::silence;
    startScript(scripts::tilt);
    stopBlinks();
    lightsDark();
  });
  bindNative(routines::tableTiltWarning, [this] { music(tunes::warnTilt); });
  bindNative(routines::tableFlipperPressed, [this] {  // a flipper pressed: the KEY and the RIP lights each move along one
    if (b(var::flipperPressed) != kSet) return;
    b(var::flipperPressed) = 0;
    auto rotate = [this](u16 flags) {
      const u8 first = b(flags);
      b(flags) = b(static_cast<u16>(flags + 1));
      b(static_cast<u16>(flags + 1)) = b(static_cast<u16>(flags + 2));
      b(static_cast<u16>(flags + 2)) = first;
    };
    if (b(var::flipperLockKey) != kSet) {
      rotate(var::lightStates + lights::key);
      row(var::lightStates + lights::key, lights::key);
    }
    if (b(var::flipperLockRip) == kSet) return;
    rotate(var::lightStates + lights::rip);
    row(var::lightStates + lights::rip, lights::rip);
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
        music(tunes::modeEnd);
        b(var::musicNext) = songAt::afterMode;
      }
    b(var::modeHit) = 0;
    b(var::modeRamp) = 0;
    b(var::musicPriority) = 0;
    b(var::musicNext) = songAt::afterMode;
    b(var::musicRepeats) = 1;
  });
}

void StonesNBones::all(std::initializer_list<u8> which, void (StonesNBones::*what)(u8)) {
  for (u8 light : which) (this->*what)(light);
}

void StonesNBones::stonesAndBones(void (StonesNBones::*what)(u8)) {
  all({lights::bone, lights::bone + 1, lights::bone + 2, lights::bone + 3, lights::stone, lights::stone + 1, lights::stone + 2,
       lights::stone + 3, lights::stone + 4},
      what);
}

void StonesNBones::scored(u16 amount) {
  addScore(var::score, amount);
  b(var::scoredThisBall) = kSet;
  w(var::idleFrames) = 0;
  if (w(var::idleShowing) == kSet) {
    startScript(scripts::showScore);
    w(var::idleShowing) = 0;
  }
}

void StonesNBones::addBonus(u16 amount) {
  u16 times = w(var::bonusTimes);
  do addScore(var::bonus, amount);
  while (times-- > 1);
}

void StonesNBones::row(u16 flags, u8 firstLight) {
  for (u16 i = 0; i < 3; ++i) {
    const u8 light = static_cast<u8>(firstLight + i);
    stopBlink(light);
    if (b(static_cast<u16>(flags + i)) == kSet) lightOn(light);
    else lightOff(light);
  }
}

void StonesNBones::shape(u16 segment, u16 record, bool second) {
  const u16 width = w(static_cast<u16>(record + gateRec::width));
  copyShape(static_cast<u16>(segment + kLoadSegment), w(static_cast<u16>(record + gateRec::at)),
            static_cast<u16>(w(static_cast<u16>(record + (second ? gateRec::dropped : gateRec::raised))) + width), width, w(static_cast<u16>(record + gateRec::rows)), static_cast<u16>(width * 3));
}

void StonesNBones::lightTower() {
  if (b(var::towerOpen) == kSet) return;
  stopBlink(lights::tower);
  blink(lights::tower, 0, 0x14);
  b(var::towerOpen) = kSet;
}

void StonesNBones::modeCountRamp() {
  if (b(var::modeRamp) != kSet) return;
  addScore(var::modeRampScore, points::k5000000);
  b(var::modeScored) = kSet;
}

void StonesNBones::modeCountHit() {
  if (b(var::modeHit) != kSet) return;
  addScore(var::modeHitScore, points::k1000000);
  b(var::modeScored) = kSet;
}

void StonesNBones::clearBall() {
  b(text::multiplier) = 0x38;
  b(text::ballA) = 0x38;
  b(text::playerA) = 0x38;
  zero12(var::score);
  zero12(var::bonus);
  zero12(var::screamsDigits);
  copy12(var::modeHitScore, points::k0);
  copy12(var::modeRampScore, points::k0);
  for (u16 i = 0; i < 11; ++i) b(static_cast<u16>(var::boneBlinking + i)) = 0;  // and stoneBlinking
  zero12(var::skillShot);
  copy12(var::millionPlusValue, points::k0);
  b(var::layer) = layers::playfield;
  b(var::holdBonus) = 0;
  b(var::keyBlinking) = 0;
  b(var::stonesBonesBlinking) = 0;
  b(var::flipperLockKey) = kSet;
  b(var::flipperLockRip) = 0;
  w(var::timeoutTopLoop) = 0;
  b(var::ghost) = 0;
  b(var::ghostActive) = 0;
  b(var::towerOpen) = 0;
  raise(gates::towerEntry);
  raise(gates::kickback);
  copy12(var::towerBonus, var::towerBonusStart);
  copy12(var::vault, var::vaultStart);
  copy12(var::well, var::wellStart);
  w(var::screamsTarget) = 0x0a;
  w(var::screams) = 0;
  // the skill shot: one of the three KEY lights, by chance
  const u8 first = static_cast<u8>(chance(3, static_cast<u8>(w(var::loopCounter)) % 3) + lights::key);
  b(var::keySkillShot) = first;
  blink(first, 0, 1);
  for (u16 a : {var::kickback, var::wellMultiBonus, var::screamX2, var::millionPlus, var::unusedA, var::unusedB, var::lightPhaseRight,
                var::towerMillion, var::tower5M, var::lightPhaseTower, var::towerExtraBall, var::towerJackpot, var::towerSuperJackpot,
                var::towerHoldBonus, var::towerDoubleBonus, var::holdBonus})
    b(a) = 0;
  b(var::nextBonusLight) = lights::bonusX2;
  b(var::vaultFromRamp) = 0;
  w(var::keyTowerCycle) = 0;
  b(var::modeRamp) = 0;
  b(var::modeHit) = 0;
  w(var::timeoutMultiBonus) = 0;
  w(var::timeoutLeftRamp) = 0;
  w(var::timeoutLoopCombo) = 0;
  b(var::loopCombo) = 0;
  b(var::towerHunt) = 0;
  b(var::towerHuntStep) = 0;
  w(var::timeoutTowerHunt) = 0;
  w(var::timeoutLock) = 0;
  for (u16 a : {var::lockReady, var::lockVaultReady, var::lockWellReady, var::ballLocked, var::screamDemon, var::vaultHold, var::ghostsBlinking})
    b(a) = 0;
  b(var::bonusMultiplier) = 1;
  b(var::inVault) = 0;
  b(var::inWell) = 0;
  b(var::inTower) = 0;
}

namespace {
/// The lights a player keeps from ball to ball, and where in the players' table each is.
struct KeptLight {
  u8 light;
  u16 inRecord;
};
constexpr u16 kept(u16 field) { return static_cast<u16>(var::playerRecords + field); }
constexpr KeptLight kKeptLights[] = {
    {lights::stone, kept(rec::stone)},         {lights::stone + 1, kept(rec::stone + 1)}, {lights::stone + 2, kept(rec::stone + 2)},
    {lights::stone + 3, kept(rec::stone + 3)}, {lights::stone + 4, kept(rec::stone + 4)}, {lights::bone, kept(rec::bone)},
    {lights::bone + 1, kept(rec::bone + 1)},   {lights::bone + 2, kept(rec::bone + 2)},   {lights::bone + 3, kept(rec::bone + 3)},
    {lights::rip, kept(rec::rip)},             {lights::rip + 1, kept(rec::rip + 1)},     {lights::rip + 2, kept(rec::rip + 2)}};
}  // namespace

void StonesNBones::savePlayer(u16 player) {
  copy12(static_cast<u16>(player + kept(rec::score)), var::score);
  copy12(static_cast<u16>(player + kept(rec::bonus)), var::bonus);
  copy12(static_cast<u16>(player + kept(rec::skillShot)), var::skillShot);
  for (const KeptLight& k : kKeptLights) b(static_cast<u16>(player + k.inRecord)) = isLit(k.light) ? kSet : 0;
  w(static_cast<u16>(player + kept(rec::screamsTarget))) = w(var::screamsTarget);
  w(static_cast<u16>(player + kept(rec::screams))) = w(var::screams);
  copy12(static_cast<u16>(player + kept(rec::screamsDigits)), var::screamsDigits);
  b(static_cast<u16>(player + kept(rec::kickback))) = b(var::kickback);
  b(static_cast<u16>(player + kept(rec::ghost))) = b(var::ghost);
  b(static_cast<u16>(player + kept(rec::ghostActive))) = b(var::ghostActive);
}

void StonesNBones::restorePlayer() {
  const u16 player = static_cast<u16>((b(var::player) - 1) * rec::size);
  copy12(var::score, static_cast<u16>(player + kept(rec::score)));
  copy12(var::bonus, static_cast<u16>(player + kept(rec::bonus)));
  copy12(var::skillShot, static_cast<u16>(player + kept(rec::skillShot)));
  flushColours();
  int n = 0;
  for (const KeptLight& k : kKeptLights) {
    if (b(static_cast<u16>(player + k.inRecord)) == kSet) setLight(k.light);
    else clearLight(k.light);
    if (++n == 5) flushColours();  // (the colours waiting to be set go out part way through)
  }
  flushColours();
  b(var::kickback) = b(static_cast<u16>(player + kept(rec::kickback)));
  if (b(var::kickback) == kSet) {
    stopBlink(lights::kickback);
    blink(lights::kickback, 0, 0x12);
    drop(gates::kickback);
  }
  w(var::screamsTarget) = w(static_cast<u16>(player + kept(rec::screamsTarget)));
  w(var::screams) = w(static_cast<u16>(player + kept(rec::screams)));
  copy12(var::screamsDigits, static_cast<u16>(player + kept(rec::screamsDigits)));
  b(var::ghost) = b(static_cast<u16>(player + kept(rec::ghost)));
  for (u8 n2 = b(var::ghost); n2 > 0; --n2) setLight(b(ghostRecord(n2 - 1, ghosts::light)));  // the ghosts collected
  b(var::ghostActive) = b(static_cast<u16>(player + kept(rec::ghostActive)));
  if (b(var::ghostActive) == kSet) {
    blink(b(ghostRecord(b(var::ghost), ghosts::light)), 0, 0x20);
    stopBlink(lights::vaultGhost);
    blink(lights::vaultGhost, 0, 0x12);
  }
}

void StonesNBones::sameBallAgain() {
  --b(var::extraBalls);
  w(var::idleFrames) = 0;
  for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(var::scoreDrawn + i)) = kScoreUndrawn;
  w(var::scoreDrawnGroups) = 0;
  bx = scripts::shootAgain;
  call(nativeW(bx));
}

/// The match here counts a digit down, slower and slower... by a list of waits (ds:0000, or
/// ds:0048 at the faster screen mode), not by chance at every step.
void StonesNBones::bindMatch() {
  auto plain = [this] {
    setFont(fonts::dots5);
    w(var::glyphRowsCut) = 0;
    w(var::glyphFirstRow) = 0;
  };
  bindNative(routines::matchBegin, [=, this] {
    b(var::musicRepeats) = 1;
    music(tunes::match);
    b(var::musicNext) = songAt::silence;
    w(var::task) = code::matchTask;
    for (u16 player = 0; player < b(var::players); ++player) {
      // the tens of each player's score (the units are always nought)
      const u16 digit = static_cast<u16>(0x2000 | static_cast<u8>(b(static_cast<u16>(kept(rec::score) + 0x0a + player * rec::size)) + 0x37));
      w(static_cast<u16>(text::matchPlayers + player * 2)) = digit;
      w(static_cast<u16>(text::matchWinners + player * 2)) = digit;
    }
    plain();
    drawText(text::matchPlayers, dmAt::matchPlayers);
    w(var::matchNumber) = chance(10, static_cast<u16>(w(var::loopCounter) % 10));
    nativeCW(cvar::matchWaits) = high() ? var::matchWaitsHigh : var::matchWaitsLow;
    si = 0x24;
    nativeCW(cvar::matchFrames) = nativeW(nativeCW(cvar::matchWaits));
    w(var::taskState) = si;
  });
  bindNative(code::matchTask, [=, this] {
    u16 left = si;
    plain();
    drawText(text::matchBlank, static_cast<u16>(dmAt::matchDigits + (w(var::matchNumber) << 3)));  // the last digit rubbed out
    if (--nativeCW(cvar::matchFrames) == 0) {
      nativeCW(cvar::matchFrames) = nativeW(nativeCW(cvar::matchWaits));
      nativeCW(cvar::matchWaits) += 2;
      --left;
      if (--w(var::matchNumber) == 0xffff) w(var::matchNumber) = 9;
    }
    const u16 digit = w(var::matchNumber);
    b(text::matchDigit) = static_cast<u8>(digit + 0x37);
    plain();
    w(var::drawSource) = text::matchDigit;
    w(var::drawAt) = static_cast<u16>(dmAt::matchDigits + (digit << 3));
    w(var::drawRoutine) = F(at::drawText);
    si = left;
    if (left != 0) return;
    // the last one stands: who has it?
    nativeCB(cvar::matchHit) = 0;
    for (u16 player = 0; player < b(var::players); ++player) {
      if (b(static_cast<u16>(text::matchPlayers + player * 2)) == static_cast<u8>(digit + 0x37)) nativeCB(cvar::matchHit) = kSet;
      else w(static_cast<u16>(text::matchWinners + player * 2)) = 0x2a2a;
    }
    if (nativeCB(cvar::matchHit) != kSet) return;
    w(var::flashPeriod) = 3;
    w(var::flashCount) = 3;
    w(var::flashPhase) = kSet;
    b(var::flashing) = kSet;
    plain();
    drawText(text::matchWinners, dmAt::matchPlayers);
    b(var::musicPriority) = 0;
    music(tunes::matchWon);
    b(var::musicRepeats) = 1;
    b(var::musicPriority) = 0;
    b(var::musicNext) = songAt::silence;
  });
}

void StonesNBones::boneScored() {
  modeCountHit();
  sfx(sounds::bone);
  scored(points::k27530);
  addBonus(points::k510);
}

void StonesNBones::stoneScored() {
  modeCountHit();
  sfx(sounds::stone, 0x28);
  scored(points::k17520);
  addBonus(points::k750);
}

/// cs:1556: all nine targets are down: the next ghost waits in the vault.
void StonesNBones::stonesBonesAll() {
  incJackpot();
  b(var::stonesBonesBlinking) = kSet;
  for (u16 i = 0; i < 11; ++i) b(static_cast<u16>(var::boneBlinking + i)) = 0;
  stonesAndBones(&StonesNBones::stop);
  if (b(var::ghostActive) != kSet) award(w(ghostRecord(b(var::ghost), ghosts::lit)));
  else award(awards::stonesBonesAllRedundant);
  lightGhost();
  addTimer(timers::stonesBonesBlink.routine);
}

void StonesNBones::lightGhost() {
  if (b(var::ghostActive) == kSet) return;
  b(var::ghostActive) = kSet;
  blink(b(ghostRecord(b(var::ghost), ghosts::light)), 0, 0x20);
  blink(lights::vaultGhost, 0, 0x12);
}

/// cs:1795: the ramp to the tower shuts behind the ball, and MILLION PLUS, if lit, is collected.
void StonesNBones::millionPlus() {
  raise(gates::rampTower);
  if (b(var::millionPlus) != kSet) return;
  addScore(var::millionPlusValue, points::k1000000);
  addScore(var::score, var::millionPlusValue);
  award(awards::millionPlus);
  out(lights::millionPlus);
  b(var::millionPlus) = 0;
}

/// cs:1af4: all three KEY lights: the next of the tower's prizes, and the way in opens.
void StonesNBones::keyAll() {
  incVault();
  incJackpot();
  b(var::keyBlinking) = kSet;
  b(var::flipperLockKey) = kSet;
  b(var::keySkillShot) = 0;
  all({lights::key, lights::key + 1, lights::key + 2}, &StonesNBones::stop);
  all({lights::key, lights::key + 1, lights::key + 2}, &StonesNBones::flash);
  auto prize = [this](u16 flag, u8 light) {
    if (b(flag) == kSet) return;
    blink(light, b(var::lightPhaseTower), 0x12);
    b(flag) = kSet;
  };
  const u16 times = w(var::keyTowerCycle);
  if (times == 0) prize(var::towerMillion, lights::towerMillion);
  else if (times == 1 || times >= 4) prize(var::tower5M, lights::tower5M);
  else if (times == 2) prize(var::towerDoubleBonus, lights::towerDoubleBonus);
  else prize(var::towerHoldBonus, lights::towerHoldBonus);
  ++w(var::keyTowerCycle);
  addTimer(timers::unblinkKeyAll.routine);
  if (b(var::towerOpen) != kSet) {
    award(awards::towerOpen);
    b(var::towerOpen) = kSet;
    stopBlink(lights::tower);
    blink(lights::tower, 0, 0x14);
  }
  drop(gates::towerEntry);
}

/// cs:2060: with none of the seven prizes waiting, the way into the tower closes. True if one is.
bool StonesNBones::towerCheckClose() {
  for (u16 flag : {var::towerExtraBall, var::towerSuperJackpot, var::towerJackpot, var::towerHoldBonus, var::towerDoubleBonus, var::tower5M,
                   var::towerMillion})
    if (b(flag) == kSet) return true;
  b(var::towerOpen) = 0;
  raise(gates::towerEntry);
  out(lights::tower);
  return false;
}

void StonesNBones::towerEject() {
  sfx(sounds::eject);
  b(var::ballHidden) = 0;
  place(spots::tower, layers::ramps);
  w(var::ballVy) = 0xf2fb;
  w(var::ballVx) = 0;
  b(var::inTower) = 0;
}

/// cs:1c68: the ball goes into the tower, where the prizes are given, each that is lit in its turn.
void StonesNBones::tower() {
  place(spots::tower, layers::ramps);
  b(var::ballHidden) = kSet;
  b(var::inTower) = kSet;
  modeCountRamp();
  incJackpot();
  b(var::countdownHeld) = kSet;
  b(var::towerResumeMode) = 0;
  if (b(var::modeRunning) == kSet) {
    b(var::towerResumeMode) = kSet;
    b(var::towerResumeModeRamp) = b(var::modeRamp);
  }
  b(var::towerHuntAtEntry) = b(var::towerHunt);
  raise(gates::towerEntry);
  drop(gates::rampTower);
  b(var::towerOpen) = 0;
  out(lights::tower);
  b(var::awardsShown) = 0;
  if (b(var::towerHunt) == kSet && b(var::towerHuntStep) >= 1 && b(var::towerHuntStep) <= 3) {
    const u8 step = b(var::towerHuntStep);
    const bool script = award(step == 1 ? awards::towerHunt1 : step == 2 ? awards::towerHunt2 : awards::towerHunt3);
    if (b(var::musicNext) != songAt::towerHuntLast) ++b(var::musicNext);
    shown(script);
    b(var::awardQuiet) = kSet;
    b(var::towerHuntStep) = step == 3 ? 0 : static_cast<u8>(step + 1);
    drop(gates::towerEntry);
    lightTower();
  }
  if (b(var::towerSuperJackpot) == kSet) {
    b(var::towerSuperJackpot) = 0;
    out(lights::towerSuperJackpot);
    shown(awardAlways(awards::towerSuperJackpot));
    b(var::awardQuiet) = kSet;
  }
  if (b(var::towerJackpot) == kSet) {
    b(var::towerJackpot) = 0;
    out(lights::towerJackpot);
    shown(awardAlways(awards::towerJackpot));
    addScore(var::score, var::jackpot);
    copy12(var::jackpot, var::jackpotStart);
    blink(lights::towerSuperJackpot, b(var::lightPhaseTower), 0x12);
    b(var::towerSuperJackpot) = kSet;
    if (b(var::towerOpen) != kSet) {
      b(var::towerOpen) = kSet;
      stopBlink(lights::tower);
      blink(lights::tower, 0, 0x14);
    }
    drop(gates::towerEntry);
    addTimer(timers::resetSuperJackpot.routine);
    b(var::awardQuiet) = kSet;
  }
  if (b(var::towerExtraBall) == kSet) {
    b(var::towerExtraBall) = 0;
    out(lights::towerExtraBall);
    shown(award(awards::towerExtraBall));
    ++b(var::extraBalls);
    b(var::awardQuiet) = kSet;
  }
  if (b(var::towerDoubleBonus) == kSet) {
    b(var::towerDoubleBonus) = 0;
    out(lights::towerDoubleBonus);
    shown(award(awards::towerDoubleBonus));
    addScore(var::bonus, var::bonus);  // the bonus doubled
    b(var::awardQuiet) = kSet;
  }
  if (b(var::towerHoldBonus) == kSet) {
    b(var::towerHoldBonus) = 0;
    out(lights::towerHoldBonus);
    shown(award(awards::towerHoldBonus));
    b(var::holdBonus) = kSet;
    b(var::awardQuiet) = kSet;
  }
  if (b(var::tower5M) == kSet) {
    b(var::tower5M) = 0;
    out(lights::tower5M);
    shown(award(awards::tower5M));
    b(var::awardQuiet) = kSet;
  }
  if (b(var::towerMillion) == kSet) {
    b(var::towerMillion) = 0;
    out(lights::towerMillion);
    shown(award(awards::towerMillion));
    b(var::awardQuiet) = kSet;
  } else {
    shown(award(awards::towerBonus));
    addScore(var::score, var::towerBonus);
    copy12(var::towerBonus, var::towerBonusStart);
    if (b(var::towerHunt) == kSet) {
      drop(gates::towerEntry);
      stopBlink(lights::tower);
      blink(lights::tower, 0, 0x14);
      b(var::towerOpen) = kSet;
    }
  }
  if (b(var::awardsShown) == 0) addTimer(timers::towerEject.routine);
  b(var::awardQuiet) = 0;
}

/// cs:21c2: the ball goes into the well.
void StonesNBones::well() {
  if (isLit(lights::wellLock)) {  // a ball is locked there already
    b(var::ballHidden) = kSet;
    addTimer(timers::wellEject.routine);
    return;
  }
  b(var::ballHidden) = kSet;
  placeBall(spots::well);
  b(var::inWell) = kSet;
  modeCountRamp();
  incJackpot();
  b(var::awardsShown) = 0;
  addScore(var::score, var::well);
  if (b(var::lockWellReady) == kSet) {  // locked: a new ball in the plunger lane
    b(var::ballLocked) = kSet;
    b(var::musicPriority) = 0;
    const bool script = award(awards::lock);
    b(var::awardQuiet) = kSet;
    shown(script);
    placeBall(spots::plungerLane);
    w(var::ballVx) = 0x0a;
    b(var::ballHidden) = 0;
    b(var::awardQuiet) = kSet;
    b(var::specialPlungerEvent) = kSet;
    b(var::musicNext) = songAt::plunger;
    b(var::lockWellReady) = 0;
    stopBlink(lights::wellLock);
    setLight(lights::wellLock);
  }
  shown(award(awards::well));
  if (b(var::wellMultiBonus) != 0) {
    b(var::wellMultiBonus) = 0;
    out(lights::wellMultiBonus);
    if (b(var::nextBonusLight) <= lights::bonusX2 + 4) {  // the multiplier up: 2, then 4 to 10
      if (b(var::bonusMultiplier) == 1) ++b(var::bonusMultiplier);
      else b(var::bonusMultiplier) += 2;
      setLight(b(var::nextBonusLight));
      shown(award(static_cast<u16>(awards::wellMultiBonus + awards::wellMultiBonusSize * static_cast<u8>(b(var::nextBonusLight) - lights::bonusX2))));
      ++b(var::nextBonusLight);
    }
  }
  if (b(var::awardsShown) == 0) addTimer(timers::wellEject.routine);
  b(var::awardQuiet) = 0;
  b(text::multiplier) = static_cast<u8>(b(var::bonusMultiplier) + 0x37);
}

/// cs:2506: the ball goes into the vault.
void StonesNBones::vault() {
  b(var::awardsShown) = 0;
  if (b(var::vaultFromRamp) != kSet) {  // in by the kickback's lane, which uses the kickback up
    out(lights::kickback);
    b(var::kickback) = 0;
  }
  b(var::ballHidden) = kSet;
  placeBall(spots::vault);
  if (b(var::tilted) == kSet || lightState(lights::vaultLock) != 0) {
    addTimer(timers::vaultEject.routine);
    return;
  }
  b(var::inVault) = kSet;
  incJackpot();
  if (b(var::lockVaultReady) == kSet) {  // locked: a new ball in the plunger lane
    b(var::ballLocked) = kSet;
    b(var::musicPriority) = 0;
    const bool script = award(awards::lock);
    b(var::awardQuiet) = kSet;
    shown(script);
    b(var::ballHidden) = 0;
    placeBall(spots::plungerLane);
    w(var::ballVx) = 0x0a;
    b(var::awardQuiet) = kSet;
    b(var::specialPlungerEvent) = kSet;
    b(var::musicNext) = songAt::plunger;
    b(var::lockVaultReady) = 0;
    stopBlink(lights::vaultLock);
    setLight(lights::vaultLock);
  }
  if (b(var::ghostActive) == kSet) {  // the ghost waiting is collected
    b(var::ghostActive) = 0;
    out(lights::vaultGhost);
    const u8 ghost = b(var::ghost);
    const u8 light = b(ghostRecord(ghost, ghosts::light));
    stopBlink(light);
    setLight(light);
    const u16 its = w(ghostRecord(ghost, ghosts::collected));
    if (b(var::modeRunning) == kSet && its == awards::grimReaper) {
      addTimer(timers::grimReaper);
    } else if (b(var::modeRunning) == kSet && its == awards::ghostHunter) {
      b(var::vaultHold) = kSet;
      addTimer(timers::ghostHunter);
    } else {
      shown(award(its));
    }
    call(w(ghostRecord(ghost, ghosts::routine)));
    if (++b(var::ghost) >= 8) {  // all eight: they blink, and begin again
      b(var::ghost) = 0;
      flushColours();
      b(var::ghostsBlinking) = kSet;
      all({lights::ghosts, lights::ghosts + 1, lights::ghosts + 2, lights::ghosts + 3, lights::ghosts + 4, lights::ghosts + 5,
           lights::ghosts + 6, lights::ghosts + 7},
          &StonesNBones::flash);
      addTimer(timers::unblinkGhosts.routine);
    }
  }
  modeCountRamp();
  addScore(var::score, var::vault);
  if (b(var::awardsShown) != 0) b(var::awardQuiet) = kSet;
  const bool script = award(awards::vault);
  b(var::awardQuiet) = 0;
  shown(script);
  if (b(var::awardsShown) == 0) addTimer(timers::vaultEject.routine);
}

/// cs:2a9c: all three RIP lights: the kickback.
void StonesNBones::ripAll() {
  b(var::ripBlinking) = kSet;
  b(var::flipperLockRip) = kSet;
  all({lights::rip, lights::rip + 1, lights::rip + 2}, &StonesNBones::stop);
  all({lights::rip, lights::rip + 1, lights::rip + 2}, &StonesNBones::flash);
  addTimer(timers::unblinkRipAll.routine);
  if (b(var::kickback) != kSet) {
    b(var::kickback) = kSet;
    blink(lights::kickback, 0, 0x12);
    drop(gates::kickback);
  }
  award(awards::kickback);
}

void StonesNBones::digits(u16 value, u16 at) {
  for (int i = 2; i >= 0; --i) {
    b(static_cast<u16>(at + i)) = static_cast<u8>(value % 10 + 0x37);
    value /= 10;
  }
  // no noughts in front (and, as the original, none after either if they are all noughts)
  for (u16 n = 0; n < 0xffff && b(at) == 0x37; ++n, ++at) b(at) = 0x2a;
}

void StonesNBones::screamsLightExtraBall() {
  if (b(var::towerExtraBall) == kSet) return;
  b(var::towerExtraBall) = kSet;
  blink(lights::towerExtraBall, b(var::lightPhaseTower), 0x12);
  drop(gates::towerEntry);
  b(var::towerOpen) = kSet;
  stopBlink(lights::tower);
  blink(lights::tower, 0, 0x14);
  award(awards::screamsExtraBall);
}

void StonesNBones::screamsLight5M() {
  if (b(var::tower5M) == kSet) return;
  b(var::tower5M) = kSet;
  blink(lights::tower5M, b(var::lightPhaseTower), 0x12);
  drop(gates::towerEntry);
  b(var::towerOpen) = kSet;
  stopBlink(lights::tower);
  blink(lights::tower, 0, 0x14);
  award(awards::towerOpen);
}

/// cs:2d9e: a trip up the SCREAMS ramp.
void StonesNBones::rampScreams() {
  sfx(sounds::rollover);
  scored(points::k10060);
  addBonus(points::k1050);
  if (b(var::screamDemon) == kSet) {  // the demon, worth more for each ball locked
    out(lights::screamDemon);
    w(var::timeoutLock) = 1;
    b(var::screamDemon) = 0;
    const u16 both = static_cast<u16>(lightState(lights::vaultLock) + lightState(lights::wellLock));
    if (both == 0) award(awards::demon5M);
    else if (both == kSet) award(awards::demon10M);
    else if (both == 2 * kSet) award(awards::demon20M);
    clearLight(lights::vaultLock);
    clearLight(lights::wellLock);
    b(var::awardQuiet) = kSet;
  }
  if (w(var::timeoutLoopCombo) == 0) b(var::loopCombo) = 0;
  else if (b(var::loopCombo) == 1) b(var::loopCombo) = 2;
  else if (b(var::loopCombo) != 2) b(var::loopCombo) = 0;
  modeCountRamp();
  incWell();
  do {
    ++w(var::screams);
    addScore(var::screamsDigits, points::k1);
    if (b(var::screamX2) != 0) {
      b(var::screamX2) = 0;
      out(lights::screamX2);
      addTimer(timers::screamExtra.routine);
    }
  } while (w(var::screams) == 1);
  if (w(var::screamsTarget) != w(var::screams)) {
    const bool many = w(var::screams) > 0x0a;
    digits(w(var::screamsTarget), many ? text::screamsTo5M : text::screamsToExtraBall);
    digits(w(var::screams), text::screams);
    award(many ? awards::screamsTo5M : awards::screamsToExtraBall);
    b(var::awardQuiet) = 0;
    return;
  }
  if (w(var::screams) == 0x0a) {
    screamsLightExtraBall();
  } else {
    screamsLight5M();
    digits(w(var::screams), text::screams);
    award(awards::screamsTo5M);
  }
  w(var::screamsTarget) += 0x0a;
  b(var::awardQuiet) = 0;
}

void StonesNBones::everyFrame() {
  if (b(var::betweenBalls) == kSet || b(var::countdownHeld) == kSet) return;
  if (w(var::timeoutTopLoop) != 0) --w(var::timeoutTopLoop);
  if (b(var::ballLocked) != kSet && w(var::timeoutLock) >= 1) {
    if (w(var::timeoutLock) == 1) {
      b(var::lockReady) = 0;
      if (b(var::lockVaultReady) != 0) {
        b(var::lockVaultReady) = 0;
        out(lights::vaultLock);
      }
      if (b(var::lockWellReady) != 0) {
        b(var::lockWellReady) = 0;
        out(lights::wellLock);
      }
    }
    --w(var::timeoutLock);
  }
  if (++b(var::lightPhaseRight) == 0x20) b(var::lightPhaseRight) = 0;
  if (++b(var::lightPhaseTower) == 0x24) b(var::lightPhaseTower) = 0;
  auto hurry = [this](u16 flag, u8 light) {
    if (b(flag) == 0) return;
    stopBlink(light);
    blink(light, 0, 1);
  };
  auto over = [this](u16 flag, u8 light) {
    if (b(flag) == 0) return;
    b(flag) = 0;
    out(light);
  };
  if (w(var::timeoutLeftRamp) != 0) {
    if (--w(var::timeoutLeftRamp) == 0) {
      over(var::millionPlus, lights::millionPlus);
      over(var::screamX2, lights::screamX2);
    }
    if (w(var::timeoutLeftRamp) == 0x5a) {
      hurry(var::millionPlus, lights::millionPlus);
      hurry(var::screamX2, lights::screamX2);
    }
  }
  if (w(var::timeoutMultiBonus) != 0) {
    if (--w(var::timeoutMultiBonus) == 0) over(var::wellMultiBonus, lights::wellMultiBonus);
    if (w(var::timeoutMultiBonus) == 0x5a) hurry(var::wellMultiBonus, lights::wellMultiBonus);
  }
  if (w(var::timeoutLoopCombo) != 0 && --w(var::timeoutLoopCombo) == 0) b(var::loopCombo) = 0;
  if (w(var::timeoutTowerHunt) != 0 && --w(var::timeoutTowerHunt) == 0) {
    b(var::towerHunt) = 0;
    b(var::towerHuntStep) = 0;
    music(tunes::towerHuntEnd);
    b(var::musicNext) = b(tunes::main);
  }
}

void StonesNBones::bindRules() {
  auto after = [this](Timer timer, u16 limit, std::function<void()> then) {
    bindNative(timer.routine, [this, counter = timer.counter, limit, then] {
      if (countTo(counter, limit)) then();
    });
  };

  // the nine targets, BONE in a row of four and STONE in a row of five, each with the timer
  // that unblinks its light
  struct Target { u16 hit, blinking; u8 light; u16 timer, counter; bool stone; };
  for (const Target t : {Target{routines::bone0, var::boneBlinking, lights::bone, timers::unblinkBone0.routine, timers::unblinkBone0.counter, false},
                         {routines::bone1, var::boneBlinking + 1, lights::bone + 1, timers::unblinkBone1.routine, timers::unblinkBone1.counter, false},
                         {routines::bone2, var::boneBlinking + 2, lights::bone + 2, timers::unblinkBone2.routine, timers::unblinkBone2.counter, false},
                         {routines::bone3, var::boneBlinking + 3, lights::bone + 3, timers::unblinkBone3.routine, timers::unblinkBone3.counter, false},
                         {routines::stone0, var::stoneBlinking, lights::stone, timers::unblinkStone0.routine, timers::unblinkStone0.counter, true},
                         {routines::stone1, var::stoneBlinking + 1, lights::stone + 1, timers::unblinkStone1.routine, timers::unblinkStone1.counter, true},
                         {routines::stone2, var::stoneBlinking + 2, lights::stone + 2, timers::unblinkStone2.routine, timers::unblinkStone2.counter, true},
                         {routines::stone3, var::stoneBlinking + 3, lights::stone + 3, timers::unblinkStone3.routine, timers::unblinkStone3.counter, true},
                         {routines::stone4, var::stoneBlinking + 4, lights::stone + 4, timers::unblinkStone4.routine, timers::unblinkStone4.counter, true}}) {
    bindNative(t.hit, [this, t] {
      if (b(var::ghostsBlinking) == kSet || b(var::stonesBonesBlinking) == kSet || b(t.blinking) == kSet) return;
      b(t.blinking) = kSet;
      if (t.stone) stoneScored();
      else boneScored();
      lightState(t.light) = kSet;
      for (u8 light : {lights::bone, u8{lights::bone + 1}, u8{lights::bone + 2}, u8{lights::bone + 3}, lights::stone, u8{lights::stone + 1},
                       u8{lights::stone + 2}, u8{lights::stone + 3}, u8{lights::stone + 4}})
        if (!isLit(light)) {
          addTimer(t.timer);
          blink(t.light, 0, 2);
          return;
        }
      stonesBonesAll();
    });
    bindNative(t.timer, [this, t] {  // unblinks the target's light
      if (b(var::stonesBonesBlinking) != kSet) {
        if (!countTo(t.counter, 0x0a)) return;
        stopBlink(t.light);
        setLight(t.light);
      } else {
        stopBlink(t.light);
      }
      b(t.blinking) = 0;
      endTimer();
    });
  }
  after(timers::stonesBonesBlink, 2, [this] {
    stonesAndBones(&StonesNBones::flash);
    addTimer(timers::unblinkStonesBones.routine);
    endTimer();
  });
  after(timers::unblinkStonesBones, 0x46, [this] {
    stonesAndBones(&StonesNBones::stop);
    stonesAndBones(&StonesNBones::clear);
    b(var::stonesBonesBlinking) = 0;
    endTimer();
  });

  bindNative(routines::enter, [this] {  // the ball leaves the plunger lane for the table
    startScript(scripts::main);
    b(var::layer) = layers::playfield;
    b(var::specialPlungerEvent) = 0;
    b(var::playersMayJoin) = 0;
    b(var::musicPriority) = 0;
    music(tunes::main);
    b(var::musicNext) = songAt::main;
    b(var::ballUntouched) = 0;
  });
  bindNative(routines::keyEntry, [this] {  // up the ramp to the KEY lanes
    millionPlus();
    b(var::ballLocked) = 0;
    addScore(var::score, points::k10000);
    addScore(var::bonus, points::k1000);
  });
  bindNative(routines::dummy0, [] {});
  bindNative(routines::rampTower, [this] {  // on the ramp to the tower
    drop(gates::rampTower);
    modeCountRamp();
  });
  bindNative(routines::keyClose, [this] { drop(gates::rampTower); });

  // the three KEY lanes
  struct Lane { u16 roll; u8 light; u16 timer, counter; };
  for (const Lane l : {Lane{routines::key0, lights::key, timers::unblinkKey0.routine, timers::unblinkKey0.counter}, {routines::key1, lights::key + 1, timers::unblinkKey1.routine, timers::unblinkKey1.counter},
                       {routines::key2, lights::key + 2, timers::unblinkKey2.routine, timers::unblinkKey2.counter}}) {
    bindNative(l.roll, [this, l] {
      if (b(var::keyBlinking) == kSet) return;
      sfx(sounds::rollover);
      scored(points::k10060);
      addBonus(points::k1010);
      lightState(l.light) = kSet;
      if (b(var::keySkillShot) == l.light) {  // the one that was blinking: the skill shot
        stopBlink(l.light);
        addScore(var::skillShot, points::k1000000);
        addScore(var::score, var::skillShot);
        award(awards::skillShot);
        incVault();
        incTowerBonus();
        incJackpot();
        incWell();
      }
      if (b(var::keySkillShot) != 0) {
        b(var::flipperLockKey) = 0;
        for (u8 other = lights::key; other <= lights::key + 2; ++other)
          if (other != l.light) stopBlink(other);
        for (u8 other = lights::key; other <= lights::key + 2; ++other)
          if (other != l.light) clearLight(other);
      }
      b(var::keySkillShot) = 0;
      if (isLit(lights::key) && isLit(lights::key + 1) && isLit(lights::key + 2)) return keyAll();
      addTimer(l.timer);
      blink(l.light, 0, 2);
      b(var::flipperLockKey) = kSet;
    });
    bindNative(l.timer, [this, l] {  // unblinks the lane's light
      if (b(var::keyBlinking) != kSet) {
        if (!countTo(l.counter, 0x0a)) return;
        stopBlink(l.light);
        setLight(l.light);
        b(var::flipperLockKey) = 0;
      }
      endTimer();
    });
  }
  after(timers::unblinkKeyAll, 0x46, [this] {
    all({lights::key, lights::key + 1, lights::key + 2}, &StonesNBones::stop);
    all({lights::key, lights::key + 1, lights::key + 2}, &StonesNBones::clear);
    b(var::keyBlinking) = 0;
    b(var::flipperLockKey) = 0;
    endTimer();
  });

  bindNative(routines::tower, [this] { tower(); });
  after(timers::resetSuperJackpot, 0x30c, [this] {
    if (b(var::towerSuperJackpot) != 0) {
      out(lights::towerSuperJackpot);
      b(var::towerSuperJackpot) = 0;
      drop(gates::towerEntry);
      towerCheckClose();
    }
    endTimer();
  });
  bindNative(routines::towerTilt, [this] {  // into the tower, tilted
    place(spots::tower, layers::ramps);
    b(var::ballHidden) = kSet;
    addTimer(timers::towerEject.routine);
  });
  after(timers::towerEject, 0x0a, [this] {
    b(var::countdownHeld) = 0;
    if (b(var::towerResumeMode) == kSet) {  // a mode was running: its count-down goes on
      u16 script = scripts::modeRampContinue;
      if (b(var::towerResumeModeRamp) != kSet) {
        script = scripts::modeHitContinue;
        b(var::modeHit) = kSet;
      } else {
        b(var::modeRamp) = kSet;
      }
      startScript(script);
    }
    towerEject();
    endTimer();
  });
  bindNative(timers::towerEjectNow, [this] {
    towerEject();
    endTimer();
  });
  bindNative(routines::well, [this] { well(); });
  after(timers::wellEject, 0x0a, [this] {
    sfx(sounds::eject);
    b(var::ballHidden) = 0;
    placeBall(spots::well);
    w(var::ballVy) = 0x682;
    w(var::ballVx) = 0xfd66;
    b(var::inWell) = 0;
    endTimer();
  });
  bindNative(routines::wellTilt, [this] {  // into the well, tilted
    b(var::inWell) = 0;
    addTimer(timers::wellEject.routine);
  });
  bindNative(routines::dummy1, [] {});
  for (u16 at : {routines::laneInnerLeft, routines::laneInnerRight})  // the inlanes
    bindNative(at, [this] {
      sfx(sounds::inlane);
      scored(points::k10070);
      addBonus(points::k1080);
    });
  bindNative(routines::laneOuterLeft, [this] {  // the outlanes
    sfx(sounds::rollover);
    scored(points::k500010);
  });
  bindNative(routines::laneOuterRight, [this] {
    sfx(sounds::rollover);
    scored(points::k500030);
  });
  bindNative(routines::vault, [this] { vault(); });
  after(timers::unblinkGhosts, 0xf0, [this] {
    all({lights::ghosts, lights::ghosts + 1, lights::ghosts + 2, lights::ghosts + 3, lights::ghosts + 4, lights::ghosts + 5, lights::ghosts + 6,
         lights::ghosts + 7},
        &StonesNBones::stop);
    flushColours();
    all({lights::ghosts, lights::ghosts + 1, lights::ghosts + 2, lights::ghosts + 3, lights::ghosts + 4, lights::ghosts + 5, lights::ghosts + 6,
         lights::ghosts + 7},
        &StonesNBones::clear);
    flushColours();
    b(var::ghostsBlinking) = 0;
    endTimer();
  });

  // what each ghost does when it is collected (ghosts::routine, the table at ds:0305)
  auto lightTowerJackpot = [this] {
    if (b(var::towerJackpot) != kSet) {
      b(var::towerJackpot) = kSet;
      blink(lights::towerJackpot, b(var::lightPhaseTower), 0x12);
      drop(gates::towerEntry);
    }
    lightTower();
  };
  bindNative(routines::ghostGrimReaper, [=, this] {  // the grim reaper
    b(var::musicRepeats) = 0;
    lightTowerJackpot();
  });
  bindNative(routines::ghostHunter, lightTowerJackpot);  // the ghost hunter
  bindNative(routines::ghostMillions, [] {});  // 5, 10 and 15 million
  bindNative(routines::ghostDemon, [this] {  // the demon: both locks ready
    b(var::lockReady) = kSet;
    w(var::timeoutLock) = 0x834;
    b(var::lockVaultReady) = kSet;
    b(var::lockWellReady) = kSet;
    b(var::ballLocked) = 0;
    stopBlink(lights::vaultLock);
    stopBlink(lights::wellLock);
    blink(lights::vaultLock, 0, 0x12);
    blink(lights::wellLock, 0, 0x12);
    b(var::screamDemon) = kSet;
    blink(lights::screamDemon, 0, 0x12);
  });
  bindNative(routines::ghostExtraBall, [this] {  // the extra ball, in the tower
    if (b(var::towerExtraBall) == kSet) return;
    b(var::towerExtraBall) = kSet;
    blink(lights::towerExtraBall, b(var::lightPhaseTower), 0x12);
    drop(gates::towerEntry);
    b(var::towerOpen) = kSet;
    stopBlink(lights::tower);
    blink(lights::tower, 0, 0x14);
  });
  bindNative(routines::ghostTowerHunt, [this] {  // the tower hunt
    drop(gates::towerEntry);
    b(var::towerHuntStep) = 1;
    b(var::towerHunt) = kSet;
    w(var::timeoutTowerHunt) = 0x960;
    b(var::musicNext) = songAt::towerHunt;
    lightTower();
  });

  after(timers::vaultEject, 0x0a, [this] {
    sfx(sounds::eject);
    b(var::ballHidden) = 0;
    drop(gates::kickback);
    placeBall(spots::vault);
    w(var::ballVy) = 0xf4c0;
    w(var::ballVx) = 0;
    b(var::inVault) = 0;
    addTimer(timers::raiseKickback.routine);
    endTimer();
  });
  after(timers::raiseKickback, 0x1e, [this] {
    if (b(var::kickback) != kSet) raise(gates::kickback);
    b(var::vaultFromRamp) = 0;
    endTimer();
  });
  bindNative(routines::dummy2, [] {});
  bindNative(routines::plungerBottom, [this] { b(var::ballOnPlunger) = kSet; });
  bindNative(routines::plungerGo, [this] { b(var::ballOnPlunger) = 0; });
  bindNative(routines::rampTop, [this] {  // the top of the top ramp
    modeCountRamp();
    incVault();
    if (w(var::timeoutLoopCombo) != 0 && b(var::loopCombo) == 2) award(awards::loopCombo);
    b(var::loopCombo) = 0;
    const bool again = w(var::timeoutTopLoop) != 0;
    w(var::timeoutTopLoop) = 0x12c;
    if (again) {
      award(awards::topMillion);
      return;
    }
    sfx(sounds::rollover);
    scored(points::k10030);
    addBonus(points::k1020);
  });
  after(timers::unblinkRipAll, 0x46, [this] {
    all({lights::rip, lights::rip + 1, lights::rip + 2}, &StonesNBones::stop);
    all({lights::rip, lights::rip + 1, lights::rip + 2}, &StonesNBones::clear);
    b(var::ripBlinking) = 0;
    b(var::flipperLockRip) = 0;
    endTimer();
  });

  // the three RIP lanes
  for (const Lane l : {Lane{routines::rip0, lights::rip, timers::unblinkRip0.routine, timers::unblinkRip0.counter}, {routines::rip1, lights::rip + 1, timers::unblinkRip1.routine, timers::unblinkRip1.counter},
                       {routines::rip2, lights::rip + 2, timers::unblinkRip2.routine, timers::unblinkRip2.counter}}) {
    bindNative(l.roll, [this, l] {
      if (b(var::ripBlinking) == kSet) return;
      sfx(sounds::rollover);
      scored(points::k10070);
      addBonus(points::k1080);
      lightState(l.light) = kSet;
      if (isLit(lights::rip) && isLit(lights::rip + 1) && isLit(lights::rip + 2)) return ripAll();
      addTimer(l.timer);
      blink(l.light, 0, 2);
      b(var::flipperLockRip) = kSet;
    });
    bindNative(l.timer, [this, l] {  // unblinks the lane's light
      if (b(var::ripBlinking) != kSet) {
        if (!countTo(l.counter, 0x14)) return;
        stopBlink(l.light);
        setLight(l.light);
      } else {
        stopBlink(l.light);
      }
      b(var::flipperLockRip) = 0;
      endTimer();
    });
  }
  bindNative(routines::rampTopExit, [this] { w(var::timeoutTopLoop) = 0x12c; });  // down from the top ramp
  bindNative(routines::rampScreams, [this] { rampScreams(); });
  after(timers::screamExtra, 2, [this] {  // SCREAM X2: the trip counted again
    rampScreams();
    endTimer();
  });
  bindNative(routines::rampLeftToLane, [this] {  // the left ramp, to the lane
    if (b(var::tilted) != kSet) {
      modeCountRamp();
      incVault();
      sfx(sounds::rollover);
      scored(points::k10030);
      addBonus(points::k1040);
      auto light = [this](u16 flag, u8 which) {
        if (b(flag) == kSet) return false;
        b(flag) = kSet;
        stopBlink(which);
        blink(which, b(var::lightPhaseRight), 0x10);
        return true;
      };
      light(var::millionPlus, lights::millionPlus);
      light(var::screamX2, lights::screamX2);
      w(var::timeoutLeftRamp) = 0x1c2;
      if (b(var::nextBonusLight) <= lights::bonusX2 + 4 && light(var::wellMultiBonus, lights::wellMultiBonus)) w(var::timeoutMultiBonus) = 0x23a;
      b(var::loopCombo) = 1;
      w(var::timeoutLoopCombo) = high() ? 0x3a8 : 0x30c;
    }
    drop(gates::rampLeft0);
  });
  bindNative(routines::rampLeftToVault, [this] {  // the left ramp, to the vault
    if (b(var::tilted) != kSet) {
      modeCountRamp();
      sfx(sounds::rollover);
      scored(points::k10020);
      addBonus(points::k1010);
    }
    raise(gates::rampLeft0);
  });
  bindNative(routines::rampLeftFixup0, [this] { drop(gates::rampLeft1); });
  bindNative(routines::rampLeftFixup1, [this] { raise(gates::rampLeft1); });
  bindNative(routines::rampLeftFixup2, [this] { drop(gates::rampLeft2); });
  bindNative(routines::rampLeftFixup3, [this] { raise(gates::rampLeft2); });
  bindNative(routines::vaultExit, [this] {  // out of the ramp into the vault
    b(var::vaultFromRamp) = kSet;
    incVault();
  });
  for (u16 at : {timers::grimReaper, timers::ghostHunter})
    bindNative(at, [this, at] {  // an award that waits for the mode that is running to be over
      if (b(var::modeRunning) == kSet) return;
      if (b(var::betweenBalls) != kSet) {
        b(var::vaultHold) = kSet;
        award(at == timers::grimReaper ? awards::grimReaper : awards::ghostHunter);
      }
      endTimer();
    });
}

/// cs:36c0: sixteen rows of a tall picture (a bit a dot, every other bit) on the display.
void StonesNBones::scrollPicture(u16 row) {
  const u16 picture = tallPicture + kLoadSegment;
  const u8 on = lit(), off = unlit();
  for (int plane = 0; plane < 2; ++plane) {
    const u8 first = plane == 0 ? 0x80 : 0x20, second = plane == 0 ? 0x08 : 0x02;
    u16 from = static_cast<u16>(row * 0x28), to = 0xa8;
    for (int r = 0; r < 0x10; ++r, to = static_cast<u16>(to + 0x58))
      for (int i = 0; i < 0x28; ++i) {
        const u8 bits = farB(picture, from++);
        dot(plane, to++) = (bits & first) ? on : off;
        dot(plane, to++) = (bits & second) ? on : off;
      }
  }
}

/// The steps Stones 'n Bones adds to the display's scripts.
void StonesNBones::bindSteps() {
  auto arg = [this](u16 n) { return nativeW(static_cast<u16>(bx + n)); };
  auto simple = [this](u16 at, std::function<void()> what) {
    bindNative(at, [this, what] {
      wait1();
      what();
      nextStep(4);
    });
  };
  simple(routines::clearFlagMode, [this] {  // the mode is over
    b(var::modeRunning) = 0;
    b(var::musicPriority) = 0;
  });
  simple(routines::setFlagMode, [this] { b(var::modeRunning) = kSet; });  // a mode begins
  simple(routines::setFlagModeRamp, [this] { b(var::modeRamp) = kSet; });
  simple(routines::clearFlagModeRamp, [this] { b(var::modeRamp) = 0; });
  simple(routines::setFlagModeHit, [this] { b(var::modeHit) = kSet; });
  simple(routines::clearFlagModeHit, [this] { b(var::modeHit) = 0; });
  bindNative(routines::endMode, [this] {  // the mode is over: the jackpots with it
    const u16 step = bx;
    out(lights::towerJackpot);
    b(var::towerJackpot) = 0;
    out(lights::towerSuperJackpot);
    b(var::towerSuperJackpot) = 0;
    if (b(var::towerHuntAtEntry) != kSet && !towerCheckClose()) {
      b(var::towerOpen) = 0;
      raise(gates::towerEntry);
      out(lights::tower);
    }
    wait1();
    bx = step;
    nextStep(4);
  });
  bindNative(routines::towerEjectStep, [this] {  // the ball out of the tower
    const u16 step = bx;
    addTimer(timers::towerEject.routine);
    wait1();
    bx = step;
    nextStep(4);
  });
  bindNative(routines::wellEjectStep, [this] {  // out of the well
    const u16 step = bx;
    addTimer(timers::wellEject.routine);
    wait1();
    bx = step;
    nextStep(4);
  });
  simple(routines::endGrimReaper, [this] { out(lights::grimReaper); });
  bindNative(routines::vaultEjectStep, [this] {  // out of the vault, unless a mode holds it there
    const u16 step = bx;
    if (b(var::vaultHold) != kSet) addTimer(timers::vaultEject.routine);
    b(var::vaultHold) = 0;
    wait1();
    bx = step;
    nextStep(4);
  });
  bindNative(routines::tiltEject, [this] {  // the ball is let go from wherever it is held
    const u16 step = bx;
    if (b(var::inVault) == kSet) addTimer(timers::vaultEject.routine);
    else if (b(var::inTower) == kSet) addTimer(timers::towerEjectNow);
    wait1();
    bx = step;
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
  bindNative(routines::jccNoBonusMult, [=, this] {  // go elsewhere if there is no multiplier
    if (b(var::bonusMultiplier) == 1) return goTo(arg(2));
    wait1();
    nextStep(4);
  });
  bindNative(routines::jump, [=, this] { goTo(arg(2)); });
  bindNative(routines::accBonusModeHit, [this] {
    wait1();
    addScore(var::bonus, var::modeHitScore);
    nextStep(2);
  });
  bindNative(routines::accBonusCyclones, [this] {  // the screams into the bonus
    zero12(var::screamsBonus);
    wait1();
    u32 times = w(var::screams);
    if (times == 0) times = 0x10000;  // as the original's loop does with a count of nought
    for (u32 i = 0; i < times; ++i) addScore(var::screamsBonus, points::k100000);
    addScore(var::bonus, var::screamsBonus);
    nextStep(2);
  });
  simple(routines::setupPartyOn, [this] { b(text::playerDigit1) = static_cast<u8>(b(var::player) + 0x37); });
  simple(routines::setupShootAgain, [this] { b(text::playerDigit2) = static_cast<u8>(b(var::player) + 0x37); });
  bindNative(code::nothingTask, [] {});
  bindNative(routines::setSpecialPlungerEvent, [this] {
    b(var::specialPlungerEvent) = kSet;
    w(var::taskState) = 1;
    w(var::task) = code::nothingTask;
    nextStep(4);
  });
  simple(routines::noop, [] {});
  bindNative(routines::multiplyBonus, [this] {  // the bonus times its multiplier
    copy12(var::bonusOnce, var::bonus);
    for (u8 n = static_cast<u8>(b(var::bonusMultiplier) - 1); n > 0; --n) addScore(var::bonus, var::bonusOnce);
    b(text::bonusTimes) = 0x37 + 1;  // "10"
    b(text::bonusTimes + 1) = 0x37;
    if (const u8 digit = static_cast<u8>(b(var::bonusMultiplier) + 0x37); digit < 0x41) {
      b(text::bonusTimes) = digit;
      b(text::bonusTimes + 1) = 0x20;
    }
    wait1();
    nextStep(4);
  });
  bindNative(routines::accBonusModeRamp, [this] {
    wait1();
    addScore(var::bonus, var::modeRampScore);
    nextStep(2);
  });
  bindNative(routines::dmTowerHunt, [=, this] {  // a tall picture rolls down the display to a row of it
    nativeCW(cvar::scrollTo) = arg(2);
    nativeCW(cvar::scrollRow) = 0x98;
    w(var::task) = code::scrollTask;
    nextStep(4);
  });
  bindNative(code::scrollTask, [this] {
    --nativeCW(cvar::scrollRow);
    scrollPicture(nativeCW(cvar::scrollRow));
    si = nativeCW(cvar::scrollRow) == nativeCW(cvar::scrollTo) ? 0 : 1;
  });
  simple(routines::extraBall, [this] { ++b(var::extraBalls); });
}

}  // namespace encore
