#include "engine/table/PartyLand.h"

namespace encore {

using namespace party;

namespace {

/// The lights each player keeps from ball to ball, in the order they are kept (cs:0d58).
constexpr u8 kKeptLights[] = {lights::puke0,  lights::puke3,  lights::puke1,  lights::puke2,  lights::mad0,   lights::mad1,
                              lights::mad2,   lights::crazy0, lights::crazy1, lights::crazy2, lights::crazy3, lights::crazy4,
                              lights::partyP, lights::partyA, lights::partyR, lights::partyT, lights::partyY};

}  // namespace

PartyLand::PartyLand(ByteView prg) : Flow(prg, kTableIndex) {
  // (amended) The top of the table, right of the tower: a soft shot that dies there balances on
  // the steel edge, at (187,15) or (183,14), and a shake sends it on to the right, down the
  // side, as the original's sums would.
  traps_.push_back({182, 12, 190, 17, 128});
  bind(routines::tableNewGame, [this] { newGameTable(); });
  bind(routines::tableGameMusic, [this] {  // the music a game opens with
    music(tunes::gameStart);
    B(var::musicAfter) = B(tunes::plunger + tuneRec::priority);  // (the plunger's tune's, not the game start's)
    B(var::musicNext) = songAt::plunger;
  });
  bindRules();
  bindSteps();
  bind(routines::tableStart, [this] {  // at start-up: the top of the left ramp's mask is kept, for putting back
    u16 to = gates::skyride.raised;
    for (u16 row = 0; row < gates::skyride.rows; ++row)
      for (u16 b = 0; b < gates::skyride.width; ++b) B(to++) = farB(S(at::wallsRamps), static_cast<u16>(gates::skyride.at + row * counts::maskRowBytes + b));
    lightsOut();
  });
  bind(routines::tableAttractMusic, [this] {  // the music while nobody plays
    if (B(var::sadJingle) != kSet && B(var::gotHighScore) != kSet) {
      music(tunes::attract);
      B(var::musicAfter) = B(tunes::attract + tuneRec::priority);
      B(var::musicNext) = songAt::afterAttract;
      B(var::musicRepeats) = 1;
    }
    B(var::sadJingle) = 0;
  });
  bind(routines::tableTilt, [this] {  // tilt
    B(var::flippersLive) = 0;
    music(tunes::tilt);
    B(var::musicNext) = songAt::silence;
    startScript(scripts::tilt);
    stopBlinks();
    lightsDark();
    B(var::scriptDone) = kSet;
  });
  bind(routines::tableTiltWarning, [this] { music(tunes::warnTilt); });  // the warning before it
  bind(timers::partyOn, [this] {
    if (!countTo(counters::partyOn, times::partyOn)) return;
    B(var::specialPlungerEvent) = kSet;
    serve();
  });
  bind(timers::drainSfx, [this] {
    if (!countTo(counters::drainSfx, times::drainSfx)) return;
    effect(sounds::ballDrained);
    endTimer();
  });
  bind(routines::waitJingleTask, [this] {  // until the music has come round (cs:52cd; Party Land's has a flag of its own in it)
    si = tasks::waiting;
    if (B(var::jingleEnded) != kSet) return;
    // (after the side lane's extra ball a lost ball is not taken as lost for ten seconds, or,
    // in the original, until this wait finds the jingle over; amended: the ten seconds)
    if (!amended) B(var::blockDrain) = 0;
    si = 0;
  });
  bind(routines::tableFrame, [this] { everyFrame(); });
  bind(routines::tableDrained, [this] { drained(); });
  bind(routines::tableFlipperPressed, [this] {  // a flipper pressed: the four lane lights move along one (cs:0fcc)
    if (B(var::flipperPressed) != kSet) return;
    // (the original keeps a press made while the lights are held, and moves them when they
    // are let go; amended: such a press is lost)
    if (amended) B(var::flipperPressed) = 0;
    if (B(var::flipperLockPuke) == kSet) return;
    B(var::flipperPressed) = 0;
    const u8 fourth = lightState(lights::puke2);
    auto put = [this](u8 light, u8 state) { state == kSet ? setLight(light) : clearLight(light); };
    put(lights::puke2, lightState(lights::puke3));
    put(lights::puke3, lightState(lights::puke0));
    put(lights::puke0, lightState(lights::puke1));
    put(lights::puke1, fourth);
  });
  bind(routines::tableBumper, [this] {  // after a bumper's score (cs:2bd1)
    modeCountHit();
    W(var::idleFrames) = 0;
    if (W(var::idleShowing) == kSet) {
      startScript(scripts::main);
      W(var::idleShowing) = 0;
    }
  });
}

void PartyLand::newGameTable() {
  B(var::crazyRow) = 0;
  lightsOut();
  stopBlinks();
  for (u16 i = 0; i < kScoreDigits; ++i) B(var::jackpot, i) = B(var::jackpotStart, i);
  clearScores();
  clearBall();
  for (u16 player = 0; player < kMaxPlayers; ++player) savePlayer(static_cast<u16>(player * rec::size));
  B(var::extraBalls) = 0;
  B(var::matchWon) = 0;
  B(var::partyOn) = 0;
  for (u16 i = 0; i < counts::matchText; ++i) B(text::matchPlayers, i) = glyph::space;
  for (u16 i = 0; i < counts::matchText; ++i) B(text::matchWinners, i) = glyph::space;
}

void PartyLand::clearScores() {
  for (u16 at : {var::score, var::bonus, var::cyclonesDigits, var::cycloneBonus, var::tunnelSkillShot, var::cycloneSkillShot})
    for (u16 i = 0; i < kScoreDigits; ++i) B(at, i) = 0;
  W(var::cyclones) = 0;
  for (u16 i = 0; i < counts::matchText; ++i) B(text::matchPlayers, i) = glyph::space;
}

void PartyLand::clearBall() {
  B(text::multiplier) = glyph::digit0 + 1;
  for (u16 i = 0; i < kScoreDigits; ++i) B(var::modeHitScore, i) = 0;
  for (u16 i = 0; i < kScoreDigits; ++i) B(var::modeRampScore, i) = 0;
  B(var::layer) = layers::playfield;
  for (u16 a : {var::lightPhaseSnack, var::lightPhaseOrbit, var::lightPhasePuke, var::lightPhaseDemon, var::snack, var::snackLit,
                u16{var::snackLit + 1}, u16{var::snackLit + 2}, var::arcadeOpen, var::rightOrbitMultiBonus})
    B(a) = 0;
  W(var::bonusTimes) = 1;
  for (u16 a : {var::rightOrbitHoldBonus, var::rightOrbitDoubleBonus, var::cycloneX5, var::demon5M, var::demonExtraBall, var::demonJackpot,
                var::demonJackpotTimed, var::duckHit, u16{var::duckHit + 1}, u16{var::duckHit + 2}, var::arcadeButtonHit,
                var::inSnack, var::orbitRightBlinking, var::madBlinking})
    B(a) = 0;
  W(var::timeoutSpringLoop) = 0;
  B(var::flipperLockPuke) = 0;
  B(var::inDemon) = 0;
  W(var::timeoutTunnel) = 0;
  W(var::timeoutPartyT) = 0;
  W(var::timeoutPartyPr) = 0;
  B(var::popcorns) = 0;
  W(var::skyride) = 0;
  W(var::demonReward) = 0;
  B(var::orbitRightCycle) = 0;
  B(var::modeHit) = 0;
  B(var::modeRamp) = 0;
  B(var::holdBonus) = 0;
  B(var::scriptDone) = kSet;
  stopBlinks();
  lightsOut();
  setLight(lights::duckDrop0);
  setLight(lights::duckDrop1);
  setLight(lights::duckDrop2);
  blink(lights::tunnel1M, 0, blinks::tunnel);
  blink(lights::orbit250k, 0, blinks::orbit);
  flushColours();
  // the three drop targets stand
  patchMask(gates::duck0.segment, gates::duck0.at, gates::duck0.raised, gates::duck0.width, gates::duck0.rows);
  patchMask(gates::duck1.segment, gates::duck1.at, gates::duck1.raised, gates::duck1.width, gates::duck1.rows);
  patchMask(gates::duck2.segment, gates::duck2.at, gates::duck2.raised, gates::duck2.width, gates::duck2.rows);
  B(var::bonusMultiplier) = 1;
}

void PartyLand::savePlayer(u16 player) {
  u16 i = 0;
  for (u8 light : kKeptLights) nativeB(static_cast<u16>(player + var::players + rec::lights + i++)) = B(var::lightStates, light) == kSet ? kSet : 0;
  flushColours();
  nativeW(static_cast<u16>(player + var::players + rec::cyclones)) = W(var::cyclones);
  nativeB(static_cast<u16>(player + var::players + rec::crazyRow)) = B(var::crazyRow);
  struct Kept { u16 at, from; };
  for (const Kept k : {Kept{var::players + rec::score, var::score}, {var::players + rec::bonus, var::bonus},
                       {var::players + rec::tunnelSkillShot, var::tunnelSkillShot}, {var::players + rec::cycloneSkillShot, var::cycloneSkillShot},
                       {var::players + rec::cyclonesDigits, var::cyclonesDigits}, {var::players + rec::cycloneBonus, var::cycloneBonus}})
    for (u16 b = 0; b < kScoreDigits; ++b) nativeB(static_cast<u16>(player + k.at + b)) = B(k.from, b);
}

void PartyLand::restorePlayer() {
  const u16 player = static_cast<u16>((B(var::player) - 1) * rec::size);
  u16 i = 0;
  for (u8 light : kKeptLights) {
    if (nativeB(static_cast<u16>(player + var::players + rec::lights + i++)) == kSet) setLight(light);
    else clearLight(light);
  }
  flushColours();
  W(var::cyclones) = nativeW(static_cast<u16>(player + var::players + rec::cyclones));
  B(var::crazyRow) = nativeB(static_cast<u16>(player + var::players + rec::crazyRow));
  struct Kept { u16 at, to; };
  for (const Kept k : {Kept{var::players + rec::score, var::score}, {var::players + rec::bonus, var::bonus},
                       {var::players + rec::tunnelSkillShot, var::tunnelSkillShot}, {var::players + rec::cycloneSkillShot, var::cycloneSkillShot},
                       {var::players + rec::cyclonesDigits, var::cyclonesDigits}, {var::players + rec::cycloneBonus, var::cycloneBonus}})
    for (u16 b = 0; b < kScoreDigits; ++b) B(k.to, b) = nativeB(static_cast<u16>(player + k.at + b));
}

void PartyLand::everyFrame() {
  if (B(var::betweenBalls) == kSet) return;
  ++B(var::frames);
  for (u16 a : {var::timeoutSkillShot, var::timeoutPartyT, var::timeoutPartyPr, var::timeoutSpringLoop})
    if (W(a) != 0) --W(a);
  struct Wheel { u16 at; u8 size; };
  for (const Wheel w : {Wheel{var::lightPhaseSnack, blinks::snackPeriod}, {var::lightPhaseOrbit, blinks::orbitPeriod}, {var::lightPhasePuke, blinks::pukePeriod}, {var::lightPhaseDemon, blinks::demonPeriod}})
    if (++B(w.at) == w.size) B(w.at) = 0;
  if (W(var::timeoutTunnel) == 0) return;
  --W(var::timeoutTunnel);
  if (W(var::timeoutTunnel) == times::tunnelHalf) {
    stopBlink(lights::tunnel5M);
    lightOff(lights::tunnel5M);
    lightState(lights::tunnel3M) = 0;
    blink(lights::tunnel3M, 0, blinks::tunnel);
  } else if (W(var::timeoutTunnel) == 0) {
    stopBlink(lights::tunnel3M);
    lightOff(lights::tunnel3M);
    lightState(lights::tunnel1M) = 0;
    blink(lights::tunnel1M, 0, blinks::tunnel);
  }
}

void PartyLand::drained() {
  B(var::ballHidden) = kSet;
  W(var::lookAtSoft) = high() ? lookAt::drainedHigh : lookAt::drainedLow;
  placeBall(spots::dropZone);
  B(var::flippersLive) = 0;
  B(var::modeRunning) = 0;
  B(var::modeHit) = 0;
  B(var::modeRamp) = 0;
  if (B(var::blockDrain) == kSet) return;
  B(var::betweenBalls) = kSet;
  B(var::musicPriority) = 0;
  if (B(var::scoredThisBall) == 0) {  // nothing was scored: the ball is given again
    startScript(scripts::partyOn);
    music(tunes::plunger);
    B(var::partyOn) = kSet;
    addTimer(timers::partyOn);
    return;
  }
  award(awards::drained);
  // (amended: the lost ball's jingle keeps its priority while it plays, so nothing cuts into it)
  if (!amended) B(var::musicPriority) = 0;
  B(var::musicNext) = songAt::silence;
  addTimer(timers::drainSfx);
}

}  // namespace encore
