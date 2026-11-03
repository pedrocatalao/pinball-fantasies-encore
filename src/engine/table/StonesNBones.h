#pragma once
// Stones 'n Bones' own rules (TABLE4.PRG, cs:0000 to cs:3746), on the engine the tables
// share. Addresses here are this table's own, and named below. The names are those of pfr's
// rewrite of the game (github.com/wanda-phi/pfr: src/table/stones.rs and the binds in its
// assets) where it has the same thing, and the engine's (Names.inc) for what the engine
// shares; each routine still says where it is in the program ("cs:1c68").
#include <initializer_list>

#include "engine/table/Flow.h"

namespace encore {

/// Stones 'n Bones' places and numbers, by its own addresses.
namespace stones {

/// The lights, by their numbers.
namespace lights {
constexpr u8 key = 0x01;  ///< the three KEY lanes, 0x01 to 0x03 (one blinks as a ball begins: the skill shot)
constexpr u8 rip = 0x04;  ///< the three RIP lanes, 0x04 to 0x06
constexpr u8 tower = 0x07;  ///< the way into the tower is open
constexpr u8 towerExtraBall = 0x08;  ///< the tower's prizes, collected when the ball goes in
constexpr u8 towerJackpot = 0x09;
constexpr u8 towerSuperJackpot = 0x0a;
constexpr u8 towerMillion = 0x0b;
constexpr u8 tower5M = 0x0c;
constexpr u8 towerHoldBonus = 0x0d;
constexpr u8 towerDoubleBonus = 0x0e;
constexpr u8 vaultLock = 0x0f;
constexpr u8 vaultGhost = 0x10;  ///< a ghost waits in the vault
constexpr u8 screamX2 = 0x11;
constexpr u8 screamDemon = 0x12;
constexpr u8 millionPlus = 0x13;
constexpr u8 bone = 0x14;  ///< the four BONE targets, 0x14 to 0x17
constexpr u8 wellMultiBonus = 0x18;
constexpr u8 wellLock = 0x19;
constexpr u8 stone = 0x1a;  ///< the five STONE targets, 0x1a to 0x1e
constexpr u8 ghosts = 0x1f;  ///< the eight ghosts, 0x1f to 0x26, collected in the order of ghosts::table
constexpr u8 grimReaper = 0x26;  ///< the last of them
constexpr u8 bonusX2 = 0x27;  ///< the bonus multipliers 2, 4, 6, 8, 10: 0x27 to 0x2b
constexpr u8 kickback = 0x2c;
}  // namespace lights

/// Variables in the data segment. Flags are ff or 0; "12 digits" is a number kept a digit to a
/// byte, as addScore adds them.
namespace var {
// --- the table's own
constexpr u16 screamsTarget = 0x01ea;  ///< word: the screams that light the next prize (10, 20...)
constexpr u16 screams = 0x01ec;  ///< word: trips up the SCREAMS ramp (pfr: num_cyclone)
constexpr u16 screamsDigits = 0x01ee;  ///< 12 digits: the same number (pfr: NumCyclone, CycloneIncr)
constexpr u16 screamsBonus = 0x01fa;  ///< 12 digits: what they add to the bonus, at the end of the ball
constexpr u16 matchWon = 0x0206;
constexpr u16 keyBlinking = 0x0207;  ///< all three KEY lanes made: they blink
constexpr u16 stonesBonesBlinking = 0x0208;  ///< all nine targets down: they blink
constexpr u16 ripBlinking = 0x0209;
constexpr u16 flipperLockKey = 0x020a;  ///< the flippers do not move the KEY lights along
constexpr u16 flipperLockRip = 0x020b;
constexpr u16 ghost = 0x020c;  ///< the next ghost, 0 to 7
constexpr u16 ghostActive = 0x020d;  ///< it waits in the vault
constexpr u16 timeoutTopLoop = 0x020e;  ///< word: frames in which the top ramp again is TOP MILLION
constexpr u16 towerOpen = 0x0210;
constexpr u16 keySkillShot = 0x0211;  ///< the KEY light of the skill shot, or 0
constexpr u16 skillShot = 0x0212;  ///< 12 digits: the skill shot's value
constexpr u16 kickback = 0x021e;
constexpr u16 lightPhaseTower = 0x021f;  ///< where the tower's lights begin to blink, 0 to 0x23
constexpr u16 wellMultiBonus = 0x0220;
constexpr u16 screamX2 = 0x0221;
constexpr u16 millionPlus = 0x0222;
constexpr u16 lightPhaseRight = 0x0225;  ///< the same for the left ramp's, 0 to 0x1f
constexpr u16 extraBalls = 0x0226;  ///< balls owed
constexpr u16 towerMillion = 0x0227;  ///< the tower's prizes, lit
constexpr u16 tower5M = 0x0228;
constexpr u16 towerExtraBall = 0x0229;
constexpr u16 towerJackpot = 0x022a;
constexpr u16 towerSuperJackpot = 0x022b;
constexpr u16 towerHoldBonus = 0x022c;
constexpr u16 towerDoubleBonus = 0x022d;
constexpr u16 holdBonus = 0x022e;  ///< the bonus is kept for the next ball
constexpr u16 millionPlusValue = 0x022f;  ///< 12 digits
constexpr u16 nextBonusLight = 0x023b;  ///< the next multiplier's light, lights::bonusX2 on
constexpr u16 boneBlinking = 0x023c;  ///< one for each BONE target; two unused, then...
constexpr u16 stoneBlinking = 0x0242;  ///< one for each STONE target
constexpr u16 vaultFromRamp = 0x0247;  ///< the ball came into the vault down the ramp
constexpr u16 keyTowerCycle = 0x0248;  ///< word: which prize KEY lights in the tower next
constexpr u16 awardsShown = 0x024a;  ///< awards whose script showed something, while the ball is held
constexpr u16 timeoutMultiBonus = 0x024b;  ///< words: frames left of what the left ramp lit
constexpr u16 timeoutLeftRamp = 0x024d;
constexpr u16 timeoutLoopCombo = 0x024f;
constexpr u16 loopCombo = 0x0251;  ///< 1: the left ramp made; 2: and then the screams ramp
constexpr u16 towerHunt = 0x0252;
constexpr u16 towerHuntAtEntry = 0x0253;  ///< towerHunt as the ball went into the tower
constexpr u16 towerHuntStep = 0x0254;  ///< 1 to 3, the next of the hunt's awards
constexpr u16 timeoutTowerHunt = 0x0255;  ///< word
constexpr u16 timeoutLock = 0x0257;  ///< word
constexpr u16 lockReady = 0x0259;
constexpr u16 lockVaultReady = 0x025a;
constexpr u16 lockWellReady = 0x025b;
constexpr u16 ballLocked = 0x025c;
constexpr u16 specialPlungerEvent = 0x025d;  ///< the ball was put back in the plunger lane
constexpr u16 screamDemon = 0x025e;
constexpr u16 vaultHold = 0x025f;  ///< the ball stays in the vault until a mode lets it go
constexpr u16 ghostsBlinking = 0x0260;  ///< all eight collected: they blink
constexpr u16 bonusMultiplier = 0x0261;  ///< 1, 2, 4, 6, 8, 10
constexpr u16 bonusOnce = 0x0262;  ///< 12 digits: the bonus as it was, before the multiplier
constexpr u16 towerResumeModeRamp = 0x026f;  ///< the mode running as the ball went into the tower
constexpr u16 towerResumeMode = 0x0271;
constexpr u16 inVault = 0x0272;  ///< where the ball is held
constexpr u16 inWell = 0x0273;
constexpr u16 inTower = 0x0274;
constexpr u16 jackpot = 0x0275;  ///< 12 digits each, from here on
constexpr u16 jackpotStep = 0x0281;
constexpr u16 jackpotStart = 0x028d;
constexpr u16 towerBonus = 0x0299;
constexpr u16 towerBonusStart = 0x02a5;
constexpr u16 towerBonusStep = 0x02b1;
constexpr u16 vault = 0x02bd;
constexpr u16 vaultStart = 0x02c9;
constexpr u16 vaultStep = 0x02d5;
constexpr u16 well = 0x02e1;
constexpr u16 wellStart = 0x02ed;
constexpr u16 wellStep = 0x02f9;
constexpr u16 playerRecords = 0x0d02;  ///< the eight players' records, playerSize apart (see rec)
constexpr u16 modeHit = 0x126f;  ///< the timed modes: the one that counts targets,
constexpr u16 modeRamp = 0x1270;  ///< and the one that counts ramps
constexpr u16 modeHitScore = 0x1d3b;  ///< 12 digits
constexpr u16 modeRampScore = 0x1e30;  ///< 12 digits
constexpr u16 matchNumber = 0x23d2;  ///< word: the match's digit

// --- the engine's, as this table has them (Party Land's names, where it has them)
constexpr u16 musicNext = 0x2795;
constexpr u16 musicAfter = 0x2797;
constexpr u16 musicRepeats = 0x2798;
constexpr u16 tilted = 0x279c;
constexpr u16 ballVx = 0x3378;
constexpr u16 ballVy = 0x337a;
constexpr u16 ballHidden = 0x33b6;  ///< ff: the ball held still, and out of the physics
constexpr u16 layer = 0x386e;  ///< 0 playfield, ff ramps
constexpr u16 lookAtSoft = 0x389d;
constexpr u16 musicPriority = 0x38a1;
constexpr u16 ballOnPlunger = 0x38a2;
constexpr u16 flipperPressed = 0x38a3;
constexpr u16 flashPeriod = 0x38a8;  ///< words: the whole table flashing
constexpr u16 flashCount = 0x38aa;
constexpr u16 flashPhase = 0x38ac;
constexpr u16 flashing = 0x38ae;
constexpr u16 awardQuiet = 0x38b0;  ///< ff: an award's script is not started
constexpr u16 bonus = 0x38b1;  ///< 12 digits
constexpr u16 bonusTimes = 0x38c9;  ///< word: the bonus is added this many times
constexpr u16 betweenBalls = 0x38e6;
constexpr u16 flippersLive = 0x38e7;
constexpr u16 scoredThisBall = 0x38f6;
constexpr u16 modeScored = 0x38f7;
constexpr u16 ballUntouched = 0x38f8;
constexpr u16 modeRunning = 0x38fa;
constexpr u16 playersMayJoin = 0x38fb;
constexpr u16 nextStep = 0x38fd;
constexpr u16 task = 0x38ff;
constexpr u16 taskState = 0x3901;
constexpr u16 loopCounter = 0x3905;  ///< the program's source of chance
constexpr u16 countdownHeld = 0x390f;  ///< the mode's count-down waits (the ball is in the tower)
constexpr u16 lightStates = 0x3b6d;  ///< by light number: ff lit
constexpr u16 countdownTens = 0x3cc3;  ///< the count-down's two digits, as characters
constexpr u16 countdownUnits = 0x3cc4;
constexpr u16 idleFrames = 0x3cc6;  ///< word: frames since anything scored
constexpr u16 idleShowing = 0x3cc8;  ///< word: ff while the display shows the best scores for it
constexpr u16 drawRoutine = 0x3cca;
constexpr u16 drawSource = 0x3ccc;
constexpr u16 drawAt = 0x3cce;
constexpr u16 players = 0x3ce6;  ///< how many play
constexpr u16 player = 0x3cea;  ///< the one playing, from 1
constexpr u16 glyphRowsCut = 0x4a69;
constexpr u16 glyphFirstRow = 0x4a6b;
constexpr u16 score = 0x4ba4;  ///< 12 digits
constexpr u16 scoreDrawn = 0x4bc8;  ///< 12: the score's digits as the display has them
constexpr u16 scoreDrawnGroups = 0x4bd4;
constexpr u16 setAtNewGame = 0x026e;  ///< ff at a new game; nothing in this table reads it
constexpr u16 unusedA = 0x0223, unusedB = 0x0224;  ///< cleared as a ball begins; nothing else touches them
}  // namespace var

/// Where in a player's record (var::playerRecords) each thing is kept from ball to ball.
namespace rec {
constexpr u16 size = 0x43;
constexpr u16 score = 0x00;  ///< 12 digits
constexpr u16 bonus = 0x0c;  ///< 12 digits
constexpr u16 ghost = 0x18;
constexpr u16 ghostActive = 0x19;
constexpr u16 skillShot = 0x1a;  ///< 12 digits
constexpr u16 kickback = 0x26;
constexpr u16 rip = 0x27;  ///< three lights
constexpr u16 stone = 0x2a;  ///< five
constexpr u16 bone = 0x2f;  ///< four
constexpr u16 screams = 0x33;  ///< word
constexpr u16 screamsDigits = 0x35;  ///< 12 digits
constexpr u16 screamsTarget = 0x41;  ///< word
}  // namespace rec

/// The eight ghosts, as the table at ds:0305 has them: seven bytes each, the light, the
/// routine run when the ghost is collected, its award then, and the award when it is lit.
/// (pfr: the awards StonesGhostLit0 to 7, and StonesGhost5M, StonesGhostTowerHunt,
/// StonesGhostExtraBall, StonesGhost10M, StonesGhostGhostHunter, StonesGhostDemon, StonesGhost15M
/// and StonesGhostGrimReaper, in this table's order.)
namespace ghosts {
constexpr u16 table = 0x0305;
constexpr u16 size = 7;
constexpr u16 light = 0, routine = 1, collected = 3, lit = 5;
}  // namespace ghosts

/// Numbers to add, 12 digits each, by their value.
namespace points {
constexpr u16 k0 = 0x00ee;
constexpr u16 k510 = 0x00fa;
constexpr u16 k750 = 0x0106;
constexpr u16 k1000 = 0x0112;
constexpr u16 k1010 = 0x011e;
constexpr u16 k1020 = 0x012a;
constexpr u16 k1050 = 0x0136;
constexpr u16 k1040 = 0x0142;
constexpr u16 k1080 = 0x014e;
constexpr u16 k10000 = 0x015a;
constexpr u16 k10030 = 0x0166;
constexpr u16 k10020 = 0x0172;
constexpr u16 k10060 = 0x017e;
constexpr u16 k10070 = 0x018a;
constexpr u16 k17520 = 0x0196;
constexpr u16 k27530 = 0x01a2;
constexpr u16 k500010 = 0x01ae;
constexpr u16 k500030 = 0x01ba;
constexpr u16 k1000000 = 0x01c6;
constexpr u16 k5000000 = 0x01d2;
constexpr u16 k1 = 0x089c;
constexpr u16 k100000 = 0x08a8;
}  // namespace points

/// The awards (Engine::award: a tune, a score, a bonus and a script), pfr's effects.
namespace awards {
constexpr u16 lock = 0x033d;
constexpr u16 stonesBonesAllRedundant = 0x0376;  ///< all nine down, a ghost already waiting
constexpr u16 towerHunt1 = 0x048f;
constexpr u16 towerHunt2 = 0x04ab;
constexpr u16 towerHunt3 = 0x04c7;
constexpr u16 loopCombo = 0x0537;
constexpr u16 screamsExtraBall = 0x0553;
constexpr u16 kickback = 0x056f;
constexpr u16 skillShot = 0x058c;
constexpr u16 towerOpen = 0x05a8;
constexpr u16 ghostHunter = 0x05c4;
constexpr u16 grimReaper = 0x05e0;
constexpr u16 topMillion = 0x0618;
constexpr u16 towerMillion = 0x0634;
constexpr u16 demon5M = 0x0650;
constexpr u16 tower5M = 0x066c;
constexpr u16 towerExtraBall = 0x0688;
constexpr u16 demon10M = 0x06a4;
constexpr u16 demon20M = 0x06c0;
constexpr u16 towerHoldBonus = 0x06dc;
constexpr u16 towerDoubleBonus = 0x06f8;
constexpr u16 towerJackpot = 0x0714;
constexpr u16 towerSuperJackpot = 0x0730;
constexpr u16 millionPlus = 0x074c;
constexpr u16 vault = 0x0768;
constexpr u16 well = 0x0784;
constexpr u16 towerBonus = 0x07a0;
constexpr u16 wellMultiBonus = 0x07bc;  ///< for 2, then 4 to 10, each 0x1c on (pfr: StonesWellMb2 to Mb10)
constexpr u16 wellMultiBonusSize = 0x1c;
constexpr u16 screamsToExtraBall = 0x0848;
constexpr u16 screamsTo5M = 0x0864;
constexpr u16 drained = 0x0880;
}  // namespace awards

/// The sound effects' records.
namespace sounds {
constexpr u16 ballDrained = 0x08bc;
constexpr u16 eject = 0x08cc;
constexpr u16 rollover = 0x08d0;
constexpr u16 inlane = 0x08d4;
constexpr u16 stone = 0x08d8;
constexpr u16 bone = 0x08dc;
}  // namespace sounds

/// The tunes' records: the place in the song, then two bytes more.
namespace tunes {
constexpr u16 gameStart = 0x08ef;
constexpr u16 plunger = 0x08f2;
constexpr u16 main = 0x08f5;
constexpr u16 attract = 0x08f8;
constexpr u16 warnTilt = 0x08fb;
constexpr u16 tilt = 0x08fe;
constexpr u16 matchWon = 0x090d;
constexpr u16 match = 0x0910;
constexpr u16 towerHuntEnd = 0x094f;
constexpr u16 modeEnd = 0x095b;  ///< pfr: ModeEndHit and ModeEndRamp, one record
}  // namespace tunes

/// Display scripts the rules start.
namespace scripts {
constexpr u16 main = 0x1c0f;
constexpr u16 modeHitContinue = 0x1ca3;  ///< a mode's count-down, after the tower
constexpr u16 modeRampContinue = 0x1dd9;
constexpr u16 shootAgain = 0x203e;  ///< a ball owed is served
constexpr u16 showScore = 0x22fc;
constexpr u16 tilt = 0x2368;
}  // namespace scripts

/// Display texts the rules write into.
namespace text {
constexpr u16 playerDigit1 = 0x1716;  ///< the player's digit, at the end of two texts
constexpr u16 screams = 0x1e40;  ///< "XXX SCREAMS"
constexpr u16 screamsToExtraBall = 0x1eb0;  ///< "XXX LITES EXTRA BALL"
constexpr u16 screamsTo5M = 0x1ec5;  ///< "XXX LITES 5 MILLION"
constexpr u16 matchPlayers = 0x23aa;  ///< each player's match digit, two characters apart
constexpr u16 matchWinners = 0x23bc;  ///< the same, the others rubbed out
constexpr u16 matchDigit = 0x23ce;  ///< the digit the match stands on
constexpr u16 matchBlank = 0x23d0;
constexpr u16 playerDigit2 = 0x2408;
constexpr u16 bonusTimes = 0x2424;  ///< the two digits of "88 TOTAL BONUS"
constexpr u16 multiplier = 0x26a3;  ///< "8X BONUS"'s digit
constexpr u16 playerA = 0x26b4;  ///< the player's and the ball's digits, in one text
constexpr u16 ballA = 0x26be;
constexpr u16 playerB = 0x26f3;  ///< and in another, from which they are copied
constexpr u16 ballB = 0x26fa;
}  // namespace text

/// A piece of a collision mask that comes and goes (cs:6721 to cs:6776): a record names two
/// shapes, the gate raised and the gate dropped, and where they go.
struct Gate {
  u16 segment;  ///< the mask's: the playfield's, or the ramps'
  u16 record;
};
/// Where in a gate's record each thing is (cs:6721).
namespace gateRec {
constexpr u16 raised = 0, dropped = 2;  ///< the two shapes, from where the mask's pictures begin
constexpr u16 at = 4;  ///< where in the mask it goes
constexpr u16 width = 6, rows = 8;
}  // namespace gateRec
namespace gates {
constexpr u16 playfield = 0x345b, ramps = 0x3f9b;
constexpr Gate rampTower{playfield, 0x1233};  ///< raised as the ball reaches the KEY lanes, dropped on the ramp to the tower
constexpr Gate towerEntry{playfield, 0x123d};  ///< raised: the way into the tower is shut
constexpr Gate kickback{playfield, 0x1265};  ///< raised: no kickback
constexpr Gate rampLeft0{ramps, 0x1247};  ///< the left ramp: dropped by its way to the lane, raised by its way to the vault
constexpr Gate rampLeft1{ramps, 0x1251};
constexpr Gate rampLeft2{ramps, 0x125b};
}  // namespace gates

/// Where the ball is put.
struct Spot {
  u16 x, y;
};
namespace spots {
constexpr Spot drained{0x8c, 0xd2};
constexpr Spot tower{0x8d, 0x8f};
constexpr Spot well{0x113, 0xf5};
constexpr Spot vault{2, 0x214};
constexpr Spot plungerLane{0x12c, 0x212};
}  // namespace spots

/// A routine the table runs every frame (Engine::addTimer), and the counter it counts frames in.
struct Timer {
  u16 routine, counter;
};
namespace timers {
constexpr Timer drainSfx{0x02aa, 0x3b9a};
constexpr Timer stonesBonesBlink{0x15c7, 0x3bb4};
constexpr Timer unblinkStonesBones{0x1630, 0x3bb6};
constexpr Timer unblinkKeyAll{0x1c09, 0x3bbe};
constexpr Timer resetSuperJackpot{0x202c, 0x3bc0};
constexpr Timer towerEject{0x211d, 0x3bc2};
constexpr u16 towerEjectNow = 0x21bc;
constexpr Timer wellEject{0x2352, 0x3bc4};
constexpr Timer unblinkGhosts{0x2738, 0x3bc6};
constexpr Timer vaultEject{0x2919, 0x3bc8};
constexpr Timer raiseKickback{0x2990, 0x3bca};
constexpr Timer unblinkRipAll{0x2afb, 0x3bcc};
constexpr Timer screamExtra{0x2f48, 0x3bd4};
constexpr u16 grimReaper = 0x35b5;  ///< these two wait for the mode running to be over
constexpr u16 ghostHunter = 0x35da;
constexpr Timer unblinkBone0{0x1082, 0x3ba2};  ///< pfr: StonesUnblinkBone(0)
constexpr Timer unblinkBone1{0x10fe, 0x3ba4};  ///< pfr: StonesUnblinkBone(1)
constexpr Timer unblinkBone2{0x117a, 0x3ba6};  ///< pfr: StonesUnblinkBone(2)
constexpr Timer unblinkBone3{0x11f6, 0x3ba8};  ///< pfr: StonesUnblinkBone(3)
constexpr Timer unblinkStone0{0x1272, 0x3baa};  ///< pfr: StonesUnblinkStone(0)
constexpr Timer unblinkStone1{0x12ee, 0x3bac};  ///< pfr: StonesUnblinkStone(1)
constexpr Timer unblinkStone2{0x136a, 0x3bae};  ///< pfr: StonesUnblinkStone(2)
constexpr Timer unblinkStone3{0x13e6, 0x3bb0};  ///< pfr: StonesUnblinkStone(3)
constexpr Timer unblinkStone4{0x1462, 0x3bb2};  ///< pfr: StonesUnblinkStone(4)
constexpr Timer unblinkKey0{0x18af, 0x3bb8};  ///< pfr: StonesUnblinkKey(0)
constexpr Timer unblinkKey1{0x19bc, 0x3bba};  ///< pfr: StonesUnblinkKey(1)
constexpr Timer unblinkKey2{0x1ac9, 0x3bbc};  ///< pfr: StonesUnblinkKey(2)
constexpr Timer unblinkRip0{0x2be1, 0x3bce};  ///< pfr: StonesUnblinkRip(0)
constexpr Timer unblinkRip1{0x2ca0, 0x3bd0};  ///< pfr: StonesUnblinkRip(1)
constexpr Timer unblinkRip2{0x2d5f, 0x3bd2};  ///< pfr: StonesUnblinkRip(2)
}  // namespace timers

/// Routines the rules point at: the display's tasks, and the engine's own.
namespace code {
constexpr u16 matchTask = 0x08a4;
constexpr u16 gameStartingTask = 0x3368;  ///< the script waits while the game is starting
constexpr u16 scrollTask = 0x369d;
constexpr u16 waitTask = 0x5a31;
constexpr u16 nothingTask = 0x5a78;
}  // namespace code

/// The routines the table binds, each by its place in the code (cs:). pfr's names, its
/// RollTrigger, HitTrigger and UopKind without the table's prefix, where it has them (with
/// Trigger or Step added where timers:: has the name already); the table's hooks, that the
/// engine calls, by Names.inc's names; else after what they do.
namespace routines {
// --- the hooks
constexpr u16 tableStart = 0x0004;
constexpr u16 tableNewGame = 0x0008;
constexpr u16 tableAttractMusic = 0x01f8;
constexpr u16 tableGameMusic = 0x0211;
constexpr u16 tableDrained = 0x0224;
constexpr u16 tableTilt = 0x0f58;
constexpr u16 tableTiltWarning = 0x0f77;
constexpr u16 tableFlipperPressed = 0x0f7e;
constexpr u16 tableFrame = 0x3189;
constexpr u16 tableBumper = 0x3336;
constexpr u16 tableModeSecond = 0x3461;
constexpr u16 idleWhileUntouched = 0x0ced;  ///< a timer: no idle count until the ball is touched
// --- roll triggers
constexpr u16 enter = 0x1738;
constexpr u16 keyEntry = 0x1768;
constexpr u16 rampTower = 0x1784;
constexpr u16 keyClose = 0x178e;
constexpr u16 tower = 0x1c68;
constexpr u16 towerTilt = 0x20bf;
constexpr u16 well = 0x21c2;
constexpr u16 wellTilt = 0x23bd;
constexpr u16 laneInnerLeft = 0x23ca;
constexpr u16 laneInnerRight = 0x2425;
constexpr u16 laneOuterLeft = 0x2480;
constexpr u16 laneOuterRight = 0x24c3;
constexpr u16 vault = 0x2506;
constexpr u16 plungerBottom = 0x29f7;
constexpr u16 plungerGo = 0x29fe;
constexpr u16 rampTop = 0x2a05;
constexpr u16 rampTopExit = 0x2d97;
constexpr u16 rampScreams = 0x2d9e;
constexpr u16 rampLeftToLane = 0x2ff7;
constexpr u16 rampLeftToVault = 0x30f5;
constexpr u16 rampLeftFixup0 = 0x3164;
constexpr u16 rampLeftFixup1 = 0x316b;
constexpr u16 rampLeftFixup2 = 0x3172;
constexpr u16 rampLeftFixup3 = 0x3179;
constexpr u16 vaultExit = 0x3180;
// --- what each ghost does when it is collected (ghosts::routine)
constexpr u16 ghostGrimReaper = 0x27a5;
constexpr u16 ghostHunter = 0x27e8;
constexpr u16 ghostMillions = 0x2825;  ///< 5, 10 and 15 million: nothing
constexpr u16 ghostDemon = 0x2826;
constexpr u16 ghostExtraBall = 0x286b;
constexpr u16 ghostTowerHunt = 0x289e;
// --- the steps of the display's scripts
constexpr u16 clearFlagMode = 0x075f;
constexpr u16 setFlagMode = 0x077a;
constexpr u16 setFlagModeRamp = 0x078f;
constexpr u16 clearFlagModeRamp = 0x07a3;
constexpr u16 setFlagModeHit = 0x07b7;
constexpr u16 clearFlagModeHit = 0x07cb;
constexpr u16 matchBegin = 0x07f4;  ///< (pfr's Match is cs:07df, which comes here)
constexpr u16 endMode = 0x1fd6;
constexpr u16 towerEjectStep = 0x2106;
constexpr u16 wellEjectStep = 0x233b;
constexpr u16 endGrimReaper = 0x28d8;
constexpr u16 vaultEjectStep = 0x28f3;
constexpr u16 tiltEject = 0x29b7;
constexpr u16 waitWhileGameStarting = 0x3359;
constexpr u16 jccScoreZero = 0x337b;
constexpr u16 jccNoBonusMult = 0x33bf;
constexpr u16 jump = 0x33e3;
constexpr u16 accBonusModeHit = 0x33ee;
constexpr u16 accBonusCyclones = 0x3418;  ///< (here, the screams)
constexpr u16 setupPartyOn = 0x355c;
constexpr u16 setupShootAgain = 0x3577;
constexpr u16 setSpecialPlungerEvent = 0x3592;
constexpr u16 noop = 0x35a6;
constexpr u16 multiplyBonus = 0x35ff;
constexpr u16 accBonusModeRamp = 0x3657;
constexpr u16 dmTowerHunt = 0x3681;
constexpr u16 extraBall = 0x3732;
constexpr u16 dummy0 = 0x1783;  ///< cs:1783: three roll triggers that do nothing (pfr: Dummy)
constexpr u16 dummy1 = 0x23c9;  ///< cs:23c9
constexpr u16 dummy2 = 0x29f6;  ///< cs:29f6
constexpr u16 bone0 = 0x103e;  ///< pfr: HitTrigger StonesBone(0)
constexpr u16 bone1 = 0x10ba;  ///< pfr: HitTrigger StonesBone(1)
constexpr u16 bone2 = 0x1136;  ///< pfr: HitTrigger StonesBone(2)
constexpr u16 bone3 = 0x11b2;  ///< pfr: HitTrigger StonesBone(3)
constexpr u16 stone0 = 0x122e;  ///< pfr: HitTrigger StonesStone(0)
constexpr u16 stone1 = 0x12aa;  ///< pfr: HitTrigger StonesStone(1)
constexpr u16 stone2 = 0x1326;  ///< pfr: HitTrigger StonesStone(2)
constexpr u16 stone3 = 0x13a2;  ///< pfr: HitTrigger StonesStone(3)
constexpr u16 stone4 = 0x141e;  ///< pfr: HitTrigger StonesStone(4)
constexpr u16 key0 = 0x17cd;  ///< pfr: RollTrigger StonesKey(0)
constexpr u16 key1 = 0x18da;  ///< pfr: RollTrigger StonesKey(1)
constexpr u16 key2 = 0x19e7;  ///< pfr: RollTrigger StonesKey(2)
constexpr u16 rip0 = 0x2b5a;  ///< pfr: RollTrigger StonesRip(0)
constexpr u16 rip1 = 0x2c19;  ///< pfr: RollTrigger StonesRip(1)
constexpr u16 rip2 = 0x2cd8;  ///< pfr: RollTrigger StonesRip(2)
}  // namespace routines

/// Variables the program keeps among its code.
namespace cvar {
constexpr u16 matchWaits = 0x089f;  ///< the place in the list of the match's waits
constexpr u16 matchFrames = 0x08a1;  ///< frames left of this digit
constexpr u16 matchHit = 0x08a3;  ///< ff: a player has the digit
constexpr u16 scrollRow = 0x3699;
constexpr u16 scrollTo = 0x369b;
}  // namespace cvar

/// The segment of the tall picture that rolls down the display (cs:36c0).
constexpr u16 tallPicture = 0x49ff;

/// The display's characters: the digits are 0x37 on.
constexpr u8 kDigit0 = 0x37;
constexpr u8 kBlank = 0x2a;

}  // namespace stones

class StonesNBones : public Flow {
 public:
  explicit StonesNBones(ByteView prg);

 private:
  u8& b(u16 a) { return nativeB(a); }
  Word w(u16 a) { return nativeW(a); }
  void sfx(u16 record, u8 how = 0) { sound->effect(b(static_cast<u16>(record + soundRec::sample)), b(static_cast<u16>(record + soundRec::note)), how, static_cast<u8>(b(static_cast<u16>(record + soundRec::channel)) + 1)); }
  void wait1() {
    w(stones::var::taskState) = 1;
    w(stones::var::task) = stones::code::waitTask;
  }
  void copy12(u16 to, u16 from) {
    for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(to + i)) = b(static_cast<u16>(from + i));
  }
  void zero12(u16 at) {
    for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(at + i)) = 0;
  }
  u8& lightState(u8 light) { return b(static_cast<u16>(stones::var::lightStates + light)); }
  bool isLit(u8 light) { return lightState(light) == 0xff; }
  /// Does `what` to each light, in turn.
  void all(std::initializer_list<u8> which, void (StonesNBones::*what)(u8));
  /// The same to the nine targets' lights, the four BONE then the five STONE.
  void stonesAndBones(void (StonesNBones::*what)(u8));
  void stop(u8 light) { stopBlink(light); }
  void clear(u8 light) { clearLight(light); }
  void flash(u8 light) { blink(light, 0, 2); }
  /// A light put out for good: no longer blinking, and not remembered as lit.
  void out(u8 light) {
    stopBlink(light);
    clearLight(light);
  }
  using Flow::placeBall;
  void placeBall(stones::Spot at) { placeBall(at.x, at.y); }
  /// The ball put somewhere, on the playfield or on the ramps, standing still.
  void place(stones::Spot at, u8 layer) {
    placeBall(at);
    b(stones::var::layer) = layer;
  }
  /// A score that is not an award's: the score itself, and what every score does (cs:14b1).
  void scored(u16 amount);
  /// The bonus goes up by an amount, once for each step of its multiplier (cs:14df).
  void addBonus(u16 amount);
  /// A row of three lights shown as their flags have them: lit or out, not blinking (cs:0fad).
  void row(u16 flags, u8 firstLight);
  /// Of a ghost's record in ghosts::table: the byte or word at `field`.
  u16 ghostRecord(u16 n, u16 field) { return static_cast<u16>(stones::ghosts::table + n * stones::ghosts::size + field); }
  /// The award that counted (Engine::award's answer: whether its script showed something).
  void shown(bool script) { b(stones::var::awardsShown) = static_cast<u8>(b(stones::var::awardsShown) + (script ? 1 : 0)); }

  // --- the gates (cs:6721 to cs:6776)
  void shape(u16 segment, u16 record, bool second);
  void raise(stones::Gate gate) { shape(gate.segment, gate.record, false); }
  void drop(stones::Gate gate) { shape(gate.segment, gate.record, true); }

  void savePlayer(u16 player) override;   // cs:0e77
  void restorePlayer() override;          // cs:0d01
  void clearBall() override;              // cs:0056
  bool extraBallOwed() override { return b(stones::var::extraBalls) != 0; }
  void matchWon() override { b(stones::var::matchWon) = 0xff; }
  void gameOverLights() override {}
  bool clearsScoreAtEnd() override { return false; }
  bool restoresAtTurn() override { return false; }
  void afterRestore() override {}
  void serveMusic() override { music(stones::tunes::plunger); }
  void ballShown() override { b(stones::text::ballA) = b(stones::text::ballB); }
  void playerShown() override { b(stones::text::playerA) = b(stones::text::playerB); }
  bool redrawNameWhileWaiting() override { return false; }
  void sameBallAgain() override;

  void bindMatch();
  void bindRules();
  void bindSteps();
  void everyFrame();       // cs:3189
  void tower();            // cs:1c68
  void well();             // cs:21c2
  void vault();            // cs:2506
  void boneScored();       // cs:149a
  void stoneScored();      // cs:14f8
  void stonesBonesAll();   // cs:1556
  void lightGhost();       // cs:1708
  void millionPlus();      // cs:1795
  void keyAll();           // cs:1af4
  bool towerCheckClose();  // cs:2060
  void towerEject();       // cs:2162
  void ripAll();           // cs:2a9c
  void rampScreams();      // cs:2d9e
  void screamsLightExtraBall();  // cs:2f85
  void screamsLight5M();   // cs:2fbe
  void digits(u16 value, u16 at);  // cs:2f5d
  /// cs:1d37 and its like: the way into the tower blinks, if it does not already.
  void lightTower();
  void incTowerBonus() { addScore(stones::var::towerBonus, stones::var::towerBonusStep); }  // cs:34f8
  void incVault() { addScore(stones::var::vault, stones::var::vaultStep); }                 // cs:3504
  void incWell() { addScore(stones::var::well, stones::var::wellStep); }                    // cs:3510
  void incJackpot() { addScore(stones::var::jackpot, stones::var::jackpotStep); }           // cs:351c
  void modeCountRamp();  // cs:3528
  void modeCountHit();   // cs:3542
  void scrollPicture(u16 row);  // cs:36c0
};

}  // namespace encore
