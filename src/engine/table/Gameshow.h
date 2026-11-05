#pragma once
// Billion Dollar Gameshow's own rules (TABLE3.PRG, cs:0000 to cs:2228), on the engine the
// tables share. Addresses here are this table's own, and named below; what the engine shares
// has the engine's names (Names.inc). Each routine still says where it is in the program
// ("cs:1342").
#include <array>

#include "engine/Values.h"
#include "engine/table/Flow.h"

namespace encore {

/// Billion Dollar Gameshow's places and numbers, by its own addresses.
namespace show {

/// The lights, by their numbers.
namespace lights {
constexpr u8 skills = 0x01;  ///< blinks as a ball begins
constexpr u8 dollar = 0x02;  ///< the pair of targets that open the door, 0x02 and 0x03
constexpr u8 topLoop = 0x04;
constexpr u8 superJackpot = 0x05;
constexpr u8 cashpot = 0x06;
constexpr u8 dropCenter = 0x07;  ///< the two pairs of targets that drop: 0x07 and 0x08,
constexpr u8 dropLeft = 0x09;    ///< and 0x09 and 0x0a
constexpr u8 orbitExtraBall = 0x0b;
constexpr u8 cashpotX5 = 0x0c;
constexpr u8 prizeTv = 0x0d;  ///< the six prizes, in two rows of three
constexpr u8 prizeTrip = 0x0e;
constexpr u8 prizeCar = 0x0f;
constexpr u8 jackpot = 0x10;
constexpr u8 collectPrize = 0x11;
constexpr u8 spinWheel = 0x12;
constexpr u8 wheel = 0x13;  ///< the prize wheel's eight, 0x13 to 0x1a
constexpr u8 billion = 0x1b;
constexpr u8 prizeBoat = 0x1c;
constexpr u8 prizeHouse = 0x1d;
constexpr u8 prizePlane = 0x1e;
constexpr u8 extraBall = 0x1f;
constexpr u8 moneyMania = 0x20;
constexpr u8 bonus = 0x21;  ///< the six multipliers, 0x21 to 0x26
}  // namespace lights

/// Variables in the data segment. Flags are ff or 0; "12 digits" is a number kept a digit to a
/// byte, as addScore adds them.
namespace var {
// --- the table's own
constexpr u16 modeRamp = 0x005e;  ///< ff: the mode running counts ramps; 0: targets
constexpr u16 specialPlungerEvent = 0x005f;  ///< the ball was put back in the plunger lane
constexpr u16 lightPhasePrize = 0x0060;  ///< where in the blinks' common period, 0 to 0x13
constexpr u16 timeoutTopLoop = 0x0062;  ///< words: frames left of what each lit
constexpr u16 timeoutSuperJackpot = 0x0064;
constexpr u16 timeoutJackpot = 0x0066;
constexpr u16 timeoutMultiBonus = 0x0068;
constexpr u16 timeoutTv = 0x006a;  ///< frames left in which a shot lights a prize
constexpr u16 timeoutBoat = 0x006c;
constexpr u16 timeoutHouse = 0x006e;
constexpr u16 timeoutPlane = 0x0070;
constexpr u16 timeoutTrip = 0x0072;
constexpr u16 timeoutCar = 0x0074;
constexpr u16 timeoutCashpotX5 = 0x0076;
constexpr u16 wheelWait = 0x0078;  ///< word: the place in the list of the wheel's waits
constexpr u16 wheelWaitsHigh = 0x007a;  ///< words: the waits between the wheel's moves, ffff at the end, at the faster screen mode
constexpr u16 wheelWaits = 0x00d0;  ///< and at the other
constexpr u16 timeoutWheelTick = 0x0122;  ///< word: frames to the wheel's next move
constexpr u16 wheelPos = 0x0124;  ///< 0 to 7, the wheel's light lit
constexpr u16 raisingMillions = 0x0125;  ///< 12 digits: what RAISING MILLIONS shows
constexpr u16 modeScoring = 0x01b5;  ///< ff from a mode's start to the end of its count-down
constexpr u16 prizeTv = 0x01b7;  ///< the six prizes: 0 none, 1 lit, 2 taken
constexpr u16 billionLit = 0x01b8;
constexpr u16 prizeTrip = 0x01b9;
constexpr u16 prizeCar = 0x01ba;
constexpr u16 prizeHouse = 0x01bb;
constexpr u16 prizeBoat = 0x01bc;
constexpr u16 prizePlane = 0x01bd;
constexpr u16 prizeSet1 = 0x01be;  ///< ff: the first row of three taken,
constexpr u16 prizeSet2 = 0x01bf;  ///< and the second
constexpr u16 multiBonusStep = 0x01c0;  ///< 0 to 6, the multipliers given (lights::bonus on)
constexpr u16 multiBonusTimes = 0x01c1;  ///< the six multipliers: 2, 3, 4, 6, 8, 10
constexpr u16 multiBonusScripts = 0x01c7;  ///< words: the script that shows each
constexpr u16 playerRecords = 0x0263;  ///< the eight players' records, rec::size apart (see rec)
constexpr u16 skills = 0x03a3;  ///< word: trips up the skills ramp
constexpr u16 skillsDigits = 0x03a5;  ///< 12 digits: the same number
constexpr u16 skillsBonus = 0x03b1;  ///< 12 digits: what they add to the bonus, at the end of the ball
constexpr u16 matchWon = 0x03bd;
constexpr u16 jackpot = 0x0617;  ///< 12 digits, the score of awards::jackpot
constexpr u16 jackpotStart = 0x0631;  ///< 12 digits
constexpr u16 cashpot = 0x06cb;  ///< 12 digits, the score of awards::cashpot
constexpr u16 cashpotX5 = 0x06e7;  ///< 12 digits, the score of awards::cashpotX5
constexpr u16 bonusMultiplier = 0x0854;  ///< 1 to 10
constexpr u16 bonusOnce = 0x0855;  ///< 12 digits: the bonus as it was, before the multiplier
constexpr u16 modeScore = 0x15fc;  ///< 12 digits: either mode's

// --- the engine's, as this table has them (Party Land's names, where it has them)
constexpr u16 musicNext = 0x1dcb;
constexpr u16 musicAfter = 0x1dcd;
constexpr u16 musicRepeats = 0x1dce;
constexpr u16 tilted = 0x1dd2;
constexpr u16 ballVx = 0x29ae;
constexpr u16 ballVy = 0x29b0;
constexpr u16 ballHidden = 0x29ec;  ///< ff: the ball held still, and out of the physics
constexpr u16 lastTrigger = 0x2be6;  ///< word: the routine of the last roll trigger crossed
constexpr u16 layer = 0x2be8;  ///< 0 playfield, ff ramps
constexpr u16 lookAtSoft = 0x2c17;
constexpr u16 musicPriority = 0x2c1b;
constexpr u16 ballOnPlunger = 0x2c1c;
constexpr u16 flipperPressed = 0x2c1d;
constexpr u16 bonus = 0x2c2b;  ///< 12 digits
constexpr u16 betweenBalls = 0x2c60;
constexpr u16 flippersLive = 0x2c61;
constexpr u16 ballUntouched = 0x2c72;
constexpr u16 modeRunning = 0x2c74;
constexpr u16 playersMayJoin = 0x2c75;
constexpr u16 nextStep = 0x2c77;
constexpr u16 task = 0x2c79;
constexpr u16 taskState = 0x2c7b;
constexpr u16 loopCounter = 0x2c7f;  ///< the program's source of chance
constexpr u16 lightStates = 0x2e5f;  ///< by light number: ff lit
constexpr u16 countdownTens = 0x2faf;  ///< the count-down's two digits, as characters
constexpr u16 countdownUnits = 0x2fb0;
constexpr u16 idleFrames = 0x2fb2;  ///< word: frames since anything scored
constexpr u16 idleShowing = 0x2fb4;  ///< word: ff while the display shows the best scores for it
constexpr u16 player = 0x2fd6;  ///< the one playing, from 1
constexpr u16 score = 0x3e90;  ///< 12 digits
constexpr u16 scoreDrawn = 0x3eb4;  ///< 12: the score's digits as the display has them
constexpr u16 scoreDrawnGroups = 0x3ec0;
constexpr u16 clearedWithModeScoring = 0x01b6;  ///< cleared beside modeScoring; nothing in these rules sets it
}  // namespace var

/// Where in a player's record (var::playerRecords) each thing is kept from ball to ball.
namespace rec {
constexpr u16 size = 0x28;
constexpr u16 score = 0x00;  ///< 12 digits
constexpr u16 bonus = 0x0c;  ///< 12 digits
constexpr u16 prizeSet1 = 0x18;
constexpr u16 prizeSet2 = 0x19;
constexpr u16 skillsDigits = 0x1a;  ///< 12 digits
constexpr u16 skills = 0x26;  ///< word
}  // namespace rec

/// Numbers to add, 12 digits each, by their value.
namespace points {
constexpr u16 k25000 = 0x0131;
constexpr u16 wheel = 0x013d;  ///< the wheel's eight values, 25000 (with blanks, for the display) to 5000000
constexpr u16 wheel100000 = 0x0155;  ///< three of them, added on their own too
constexpr u16 wheel500000 = 0x016d;
constexpr u16 wheel1000000 = 0x0179;
constexpr u16 k7130 = 0x019d;
constexpr u16 k227830 = 0x01a9;
constexpr u16 k1 = 0x01d3;
constexpr u16 k100000 = 0x01df;
}  // namespace points

/// The awards (Engine::award: a tune, a score, a bonus and a script).
namespace awards {
constexpr u16 cashpotLock = 0x03be;
constexpr u16 billion = 0x03da;
constexpr u16 laneOuter = 0x03f6;
constexpr u16 laneInner = 0x0413;
constexpr u16 rampRight = 0x0430;
constexpr u16 rampTop = 0x044d;
constexpr u16 rampLoop = 0x046a;
constexpr u16 topEntry = 0x0487;
constexpr u16 skillsEntry = 0x04a4;
constexpr u16 rampSkills = 0x04c1;
constexpr u16 orbitLeft = 0x04de;
constexpr u16 orbitRight = 0x04fb;
constexpr u16 loopEntry = 0x0518;
constexpr u16 prizeTv = 0x0535;  ///< a prize taken
constexpr u16 prizeTrip = 0x0551;
constexpr u16 prizeCar = 0x056d;
constexpr u16 prizeBoat = 0x0589;
constexpr u16 prizeHouse = 0x05a5;
constexpr u16 prizePlane = 0x05c1;
constexpr u16 modeHit = 0x05dd;
constexpr u16 modeRamp = 0x05f9;
constexpr u16 jackpot = 0x0615;
constexpr u16 superJackpot = 0x063d;
constexpr u16 extraBall = 0x0659;
constexpr u16 raisingMillions = 0x0675;
constexpr u16 skillsToMoneyMania = 0x0691;
constexpr u16 skillsToExtraBall = 0x06ad;
constexpr u16 cashpot = 0x06c9;
constexpr u16 cashpotX5 = 0x06e5;
constexpr u16 dropCenter = 0x0701;
constexpr u16 dropLeft = 0x071e;
constexpr u16 dollar = 0x073b;
constexpr u16 dollarBoth = 0x0758;
constexpr u16 rampTopTwice = 0x0774;
constexpr u16 litTv = 0x0790;  ///< a prize lit
constexpr u16 litTrip = 0x07ac;
constexpr u16 litCar = 0x07c8;
constexpr u16 litBoat = 0x07e4;
constexpr u16 litHouse = 0x0800;
constexpr u16 litPlane = 0x081c;
constexpr u16 drained = 0x0838;
}  // namespace awards

/// The sound effects' records.
namespace sounds {
constexpr u16 raiseHitTargets = 0x0865;
constexpr u16 ballDrained = 0x0871;
constexpr u16 issueBall = 0x0875;
constexpr u16 ejectCashpot = 0x0881;
constexpr u16 rollover = 0x0885;
constexpr u16 inlane = 0x0889;
constexpr u16 hitTarget = 0x089d;
}  // namespace sounds

/// The tunes' records: the place in the song, then two bytes more.
namespace tunes {
constexpr u16 warnTilt = 0x08b0;
constexpr u16 tilt = 0x08b3;
constexpr u16 spinWheel = 0x08c0;
constexpr u16 main = 0x08d2;
constexpr u16 gameStart = 0x08de;
constexpr u16 plunger = 0x08de;  ///< the same record
constexpr u16 attract = 0x08e1;
constexpr u16 multiBonus = 0x08e8;
constexpr u16 jackpot = 0x08eb;
constexpr u16 modeEnd = 0x08fd;
constexpr u16 extraBallLit = 0x0918;
constexpr u16 prizeIncoming = 0x091e;
}  // namespace tunes

/// Display scripts the rules start.
namespace scripts {
constexpr u16 main = 0x0e9e;
constexpr u16 hintLoopRight = 0x10ef;
constexpr u16 hintLoopLeft = 0x10fd;
constexpr u16 spinWheelBlink = 0x12e7;
constexpr u16 spinWheelClear = 0x12f9;
constexpr u16 spinWheelClearHalt = 0x12ff;
constexpr u16 spinWheelScore = 0x1309;
constexpr u16 spinWheelScoreValue = 0x130b;  ///< word, in that script: the score it shows, one of points::wheel
constexpr u16 shootAgain = 0x1738;  ///< a ball owed is served
constexpr u16 showScore = 0x19dc;
constexpr u16 tilt = 0x1a0a;
}  // namespace scripts

/// Display texts the rules write into.
namespace text {
constexpr u16 maniaAt = 0x129f;  ///< MONEY MANIA AT's number, three characters
constexpr u16 matchPlayers = 0x1a2e;
constexpr u16 matchWinners = 0x1a40;
constexpr u16 playerDigit1 = 0x1a7b;  ///< the player's digit, in three texts
constexpr u16 playerDigit3 = 0x1a93;
constexpr u16 playerDigit2 = 0x1aa8;
constexpr u16 bonusX = 0x1ad0;  ///< the two digits of "BONUS X 88"
constexpr u16 multiplier = 0x1cf7;  ///< the two of "XX BONUS"
}  // namespace text

/// A piece of a collision mask that comes and goes: where in the mask, and the shape put there
/// open and shut; then its width in bytes, its rows, and the bytes from one of
/// its rows to the next in the shape. The last two keep three rows of bytes to each row of the
/// mask, of which the middle one is put there: a step of three widths, from one
/// width on.
struct Shape {
  u16 at, open, shut;
  u16 width, rows, step;
};
namespace shapes {
constexpr Shape vaultEntry{0x0f00, 0x6190, 0x61e0, 3, 0x19, 3};  ///< the ramps' mask
constexpr Shape plungerGate{0x2fcb, 0x6230, 0x6280, 2, 0x22, 2};
constexpr Shape rampRightGate{0x198e, 0x6390, 0x6480, 4, 0x14, 12};  ///< the ramps' mask
constexpr Shape vaultExit{0x4fd8, 0x6570, 0x6620, 4, 0x0e, 12};
}  // namespace shapes
/// A target that drops: where in the mask, its shape down and up, and its width.
struct TargetShape {
  u16 at, down, up, width;
};
namespace targetShapes {
constexpr TargetShape dropCenter0{0x2389, 0x62d0, 0x6370, 2};
constexpr TargetShape dropCenter1{0x26a9, 0x62f0, 0x6360, 1};
constexpr TargetShape dropLeft0{0x2994, 0x6300, 0x6350, 1};
constexpr TargetShape dropLeft1{0x2cb3, 0x6310, 0x6330, 2};
}  // namespace targetShapes

/// Where the ball is put.
struct Spot {
  u16 x, y;
};
namespace spots {
constexpr Spot drained{0x87, 0x1c};
constexpr Spot vault{4, 0x211};
constexpr Spot cashpot{0x67, 0xe9};
constexpr Spot plungerLane{0x130, 0x217};
}  // namespace spots

/// A routine the table runs every frame (Engine::addTimer), and the counter it counts frames in.
struct Timer {
  u16 routine, counter;
};
namespace timers {
constexpr Timer drainSfx{0x01d2, 0x2e86};
constexpr Timer resetDropCenter{0x0e6b, 0x2e90};
constexpr Timer resetDropLeft{0x0f21, 0x2e92};
constexpr Timer unblinkDollar0{0x1019, 0x2e94};
constexpr Timer unblinkDollar1{0x1035, 0x2e96};
constexpr Timer unblinkDollarAll{0x1051, 0x2e98};
constexpr Timer billionRelease{0x111e, 0x2e9a};
constexpr Timer vaultEject{0x11e7, 0x2e9c};
constexpr Timer spinWheelEnd{0x13a8, 0x2e9e};
constexpr u16 givePrize = 0x13f7;
constexpr Timer cashpot{0x15d8, 0x2ea0};
constexpr Timer cashpotEject{0x1643, 0x2ea2};
constexpr Timer cashpotEjectInMode{0x166a, 0x2ea4};
}  // namespace timers

/// Routines the rules point at: the display's tasks.
namespace code {
constexpr u16 matchTask = 0x06e5;
constexpr u16 gameStartingTask = 0x204c;  ///< the script waits while the game is starting
constexpr u16 waitTask = 0x4513;
constexpr u16 nothingTask = 0x455a;
}  // namespace code

/// The routines the table binds, each by its place in the code (cs:): the triggers and the
/// script's steps after what they are (with Trigger or Step added where timers:: has the name
/// already); the table's hooks, that the
/// engine calls, by Names.inc's names; else after what they do. The rules also look back at
/// some of the roll triggers (var::lastTrigger).
namespace routines {
// --- the hooks
constexpr u16 tableStart = 0x0004;
constexpr u16 tableNewGame = 0x00c6;
constexpr u16 tableAttractMusic = 0x0124;
constexpr u16 tableGameMusic = 0x013d;
constexpr u16 tableDrained = 0x0150;
constexpr u16 tableTilt = 0x0cd7;
constexpr u16 tableTiltWarning = 0x0cf6;
constexpr u16 tableFlipperPressed = 0x0cfd;
constexpr u16 tableFrame = 0x1d9d;
constexpr u16 tableBumper = 0x1f40;
constexpr u16 tableModeSecond = 0x21a4;
constexpr u16 idleWhileUntouched = 0x0bd5;  ///< a timer: no idle count until the ball is touched
// --- hit triggers
constexpr u16 dollar0 = 0x0f57;
constexpr u16 dollar1 = 0x0f95;
// --- roll triggers
constexpr u16 enter = 0x1077;
constexpr u16 vaultExit = 0x10a9;
constexpr u16 plungerGo = 0x10ad;
constexpr u16 plungerBottom = 0x10b7;
constexpr u16 vault = 0x1219;
constexpr u16 cashpotTrigger = 0x1587;
constexpr u16 rampRight = 0x1713;
constexpr u16 rampLoop = 0x1768;
constexpr u16 orbitLeft = 0x18d6;
constexpr u16 orbitRight = 0x19bd;
constexpr u16 rampSkillMark = 0x1a66;
constexpr u16 rampSkill = 0x1a67;
constexpr u16 laneOuterOther = 0x1cd9;  ///< the other outlane
constexpr u16 laneOuter = 0x1cf4;
constexpr u16 laneInnerOther = 0x1d0f;  ///< and the inlane
constexpr u16 laneInner = 0x1d2a;
constexpr u16 rampTopEntry = 0x1d45;
constexpr u16 rampSkillEntry = 0x1d4c;
constexpr u16 rampLoopEntry = 0x1d53;
constexpr u16 rampTopSecondary = 0x1d5a;
constexpr u16 rampTop = 0x1d5e;
// --- the steps of the display's scripts
constexpr u16 spinWheelEndStep = 0x0678;
constexpr u16 endMoneyMania = 0x068f;
constexpr u16 blinkMoneyMania = 0x06b6;
constexpr u16 match = 0x06d0;
constexpr u16 setupPartyOn = 0x1fe1;
constexpr u16 setupShootAgain = 0x1fff;
constexpr u16 setSpecialPlungerEvent = 0x201a;
constexpr u16 noop = 0x202e;
constexpr u16 waitWhileGameStarting = 0x203d;
constexpr u16 jccScoreZero = 0x205f;
constexpr u16 multiplyBonus = 0x20a4;
constexpr u16 jccNoBonusMult = 0x20fc;
constexpr u16 accBonusModeHit = 0x2120;
constexpr u16 jump = 0x214a;
constexpr u16 accBonusCyclones = 0x215b;  ///< (here, the trips up the skills ramp)
constexpr u16 extraBall = 0x2211;
constexpr u16 incCashpotOther = 0x1f85;  ///< cs:1f85: the cashpot up by 227,830; what calls it is not known
constexpr u16 dummy0 = 0x20a3;  ///< cs:20a3: does nothing
constexpr u16 dummy1 = 0x2155;  ///< cs:2155: does nothing
constexpr u16 stayStep = 0x2156;  ///< cs:2156: a step the script stays on
constexpr u16 dropCenter0 = 0x0deb;
constexpr u16 dropCenter1 = 0x0e2b;
constexpr u16 dropLeft0 = 0x0ea1;
constexpr u16 dropLeft1 = 0x0ee1;
}  // namespace routines

/// Variables the program keeps among its code.
namespace cvar {
constexpr u16 matchPace = 0x0762;
}  // namespace cvar

/// The table's place in the engine's maps (Flow's second argument).
constexpr int kTableIndex = 2;
/// What var::scoreDrawn's digits are set to, so that each is drawn again.
constexpr u8 kScoreUndrawn = 0x12;
/// A light's state set to 1, not ff, as the rules' own mark: a prize to collect
/// (lights::collectPrize), and the ball more and the jackpot lit to be taken.
constexpr u8 kMarked = 1;

/// A prize's flag (var::prizeTv and the five others).
namespace prize {
constexpr u8 lit = 1;  ///< lit, to be collected in the vault
constexpr u8 taken = 2;
}  // namespace prize

/// The four targets that drop, as target() numbers them.
namespace drops {
constexpr int center0 = 0, center1 = 1, left0 = 2, left1 = 3;
}  // namespace drops

/// The prize wheel: its eight places (var::wheelPos), and where each prize is on it.
namespace wheel {
constexpr u8 tv = 0, trip = 1, car = 2, plane = 4, house = 5, boat = 6;
}  // namespace wheel

/// Places in the song (var::musicNext): where the music goes on after a tune.
namespace songAt {
constexpr u8 plunger = 0x00;  ///< tunes::plunger's (and tunes::gameStart's)
constexpr u8 main = 0x01;  ///< tunes::main's
constexpr u8 afterMode = 0x03;  ///< after a mode's end: the main music past its opening
constexpr u8 afterAttract = 0x09;  ///< after the tune while nobody plays
constexpr u8 silence = 0x37;  ///< ds:08ad
}  // namespace songAt
}  // namespace show

class Gameshow : public Flow {
 public:
  explicit Gameshow(ByteView prg);

 private:
  u8& b(u16 a) { return nativeB(a); }
  Word w(u16 a) { return nativeW(a); }
  void sfx(u16 record, u8 how = 0) { sound->effect(b(static_cast<u16>(record + soundRec::sample)), b(static_cast<u16>(record + soundRec::note)), how, static_cast<u8>(b(static_cast<u16>(record + soundRec::channel)) + 1)); }
  /// Where in the blinks' common period (cs:1e15: twenty frames) a light begins: in step with
  /// the others, or half a period out.
  u8 inStep() { return b(show::var::lightPhasePrize); }
  u8 outOfStep() {
    const u8 start = static_cast<u8>(b(show::var::lightPhasePrize) + 0x0a);
    return start >= 0x14 ? static_cast<u8>(start - 0x14) : start;
  }
  void wait1() {
    w(show::var::taskState) = 1;
    w(show::var::task) = show::code::waitTask;
  }
  void copy12(u16 to, u16 from) {
    for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(to + i)) = b(static_cast<u16>(from + i));
  }
  void zero12(u16 at) {
    for (u16 i = 0; i < 12; ++i) b(static_cast<u16>(at + i)) = 0;
  }
  u8& lightState(u8 light) { return b(static_cast<u16>(show::var::lightStates + light)); }
  using Flow::placeBall;
  void placeBall(show::Spot at) { placeBall(at.x, at.y); }
  /// The ball kept out of sight somewhere.
  void hold(show::Spot at) {
    b(show::var::ballHidden) = kSet;
    placeBall(at);
  }

  void savePlayer(u16 player) override;   // cs:0c97
  void restorePlayer() override;          // cs:0be9
  void clearBall() override;              // cs:0008
  bool extraBallOwed() override { return lightState(show::lights::extraBall) == kSet; }
  void matchPace() override { nativeCW(show::cvar::matchPace) = 0x0e; }
  u16 matchLength() override { return 0x0f; }
  u16 matchRepeated(u16 digit) override { return static_cast<u16>(9 - digit); }
  void matchWon() override { b(show::var::matchWon) = kSet; }
  void gameOverLights() override {}
  bool restoresAtTurn() override { return false; }
  void afterRestore() override {
    rampRightGate(false);
    vaultExit(false);
  }
  void serveMusic() override { music(show::tunes::plunger); }
  void sameBallAgain() override;

  // --- the pieces of the collision masks that come and go (cs:0d0f to cs:0dea):
  // open is the second shape (dropped), shut the first (raised)
  static constexpr u16 kMask = 0x2aef + kLoadSegment, kUpperMask = 0x362f + kLoadSegment;  ///< the playfield's, and the ramps'
  /// The door into the vault.
  void vaultEntry(bool open) {
    copyShape(kUpperMask, show::shapes::vaultEntry.at, open ? show::shapes::vaultEntry.open : show::shapes::vaultEntry.shut, show::shapes::vaultEntry.width,
              show::shapes::vaultEntry.rows, show::shapes::vaultEntry.step);
  }
  void plungerGate(bool open) {
    copyShape(kMask, show::shapes::plungerGate.at, open ? show::shapes::plungerGate.open : show::shapes::plungerGate.shut, show::shapes::plungerGate.width,
              show::shapes::plungerGate.rows, show::shapes::plungerGate.step);
  }
  /// The four targets that drop: 0 and 1 the centre pair, 2 and 3 the left.
  void target(int which, bool down);
  void rampRightGate(bool open) {
    copyShape(kUpperMask, show::shapes::rampRightGate.at,
              static_cast<u16>((open ? show::shapes::rampRightGate.open : show::shapes::rampRightGate.shut) + show::shapes::rampRightGate.width),  // the middle of each three rows
              show::shapes::rampRightGate.width, show::shapes::rampRightGate.rows, show::shapes::rampRightGate.step);
  }
  void vaultExit(bool open) {
    copyShape(kMask, show::shapes::vaultExit.at, static_cast<u16>((open ? show::shapes::vaultExit.open : show::shapes::vaultExit.shut) + show::shapes::vaultExit.width),
              show::shapes::vaultExit.width, show::shapes::vaultExit.rows, show::shapes::vaultExit.step);
  }

  void bindRules();
  void bindSteps();
  void everyFrame();     // cs:1d9d
  void billion();        // cs:10be
  void vaultSkip();      // cs:11a0
  void wheelTick();      // cs:1342
  void cashpotLock();    // cs:1517
  void cashpotEject();   // cs:16b9
  void multiBonus();     // cs:1848
  void countSkills();    // cs:1beb
  void incJackpot() { addScore(show::var::jackpot, show::points::wheel100000); }  // cs:1f63
  void resetJackpot() { copy12(show::var::jackpot, show::var::jackpotStart); }   // cs:1f6d
  void incCashpot() { addScore(show::var::cashpot, show::points::k7130); }        // cs:1f7b
  void modeCountHit();   // cs:1f8f
  void modeCountRamp();  // cs:1fb8
  /// One of the six prizes is lit to be collected (cs:17d0 and its like): its light blinks,
  /// and with all three of its row lit the way to collect them opens.
  void litPrize(u16 flag, u16 record, u8 light, bool outOfStepLight, u16 other1, u16 other2);
  void threeDigits(u16 value, u16 at);
  void maniaAt();
  bool bothShown_ = false;  ///< the pair of door targets is flashing for both having been hit
  std::array<u8, 4> maniaAt_{};  ///< the skills ramp's message's number as the program begins it (ds:129f)
};

}  // namespace encore
