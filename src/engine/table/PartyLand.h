#pragma once
// Party Land's own rules (TABLE1.PRG, cs:0000 to cs:2f9a), on the engine the tables share.
// Party Land is the table the engine is written against: its addresses are the engine's own
// (the map is the identity), and the rules read them through B(), W() and the like. Its own are
// named below, what the engine shares in Names.inc; each routine still says where it is in the
// program ("cs:1126").
#include "engine/table/Flow.h"

namespace encore {

/// Party Land's places and numbers, by its own addresses.
namespace party {

/// The lights, by their numbers.
namespace lights {
constexpr u8 puke0 = 0x01;  ///< the four lanes at the top, which the flippers move along
constexpr u8 puke1 = 0x04;
constexpr u8 puke2 = 0x05;
constexpr u8 puke3 = 0x02;
constexpr u8 dropZone0 = 0x03;  ///< blink while the ball waits in the drop zone, at the top
constexpr u8 mad0 = 0x06;  ///< the left orbit's three
constexpr u8 arcade0 = 0x07;  ///< the arcade is open
constexpr u8 mad1 = 0x08;
constexpr u8 mad2 = 0x09;
constexpr u8 tunnel5M = 0x0a;  ///< the tunnel's prizes: the one that blinks comes next
constexpr u8 cycloneX5 = 0x0b;
constexpr u8 tunnel3M = 0x0c;
constexpr u8 skyride1 = 0x0d;
constexpr u8 tunnel1M = 0x0e;
constexpr u8 skyride2 = 0x0f;
constexpr u8 duck0 = 0x10;  ///< blink when a duck goes down
constexpr u8 snack1 = 0x11;
constexpr u8 duck1 = 0x12;
constexpr u8 orbit750k = 0x13;  ///< the right orbit's prizes
constexpr u8 snack2 = 0x14;
constexpr u8 demonHead = 0x15;
constexpr u8 skyride0 = 0x16;
constexpr u8 snack0 = 0x17;
constexpr u8 duck2 = 0x18;
constexpr u8 orbit500k = 0x19;
constexpr u8 orbit250k = 0x1a;
constexpr u8 demon5M = 0x1b;
constexpr u8 crazy4 = 0x1c;  ///< CRAZY: lit in the order of the row at var::crazyRow
constexpr u8 rightOrbitMultiBonus = 0x1d;
constexpr u8 demonExtraBall = 0x1e;
constexpr u8 crazy3 = 0x1f;
constexpr u8 happyHour = 0x20;
constexpr u8 rightOrbitHoldBonus = 0x21;
constexpr u8 crazy2 = 0x22;
constexpr u8 demonJackpot = 0x23;
constexpr u8 rightOrbitDoubleBonus = 0x24;
constexpr u8 megaLaugh = 0x25;
constexpr u8 crazy1 = 0x26;
constexpr u8 sideExtraBall = 0x27;
constexpr u8 crazy0 = 0x29;
constexpr u8 partyP = 0x2a;  ///< PARTY, 0x2a to 0x2e
constexpr u8 partyA = 0x2b;
constexpr u8 partyR = 0x2c;
constexpr u8 partyT = 0x2d;
constexpr u8 partyY = 0x2e;
constexpr u8 bonusX2 = 0x2f;  ///< the bonus multipliers
constexpr u8 bonusX8 = 0x30;
constexpr u8 bonusX4 = 0x31;
constexpr u8 bonusX6 = 0x32;
constexpr u8 extraBall = 0x33;  ///< a ball is owed
constexpr u8 duckDrop0 = 0x34;  ///< lit while each duck stands
constexpr u8 duckDrop1 = 0x35;
constexpr u8 duckDrop2 = 0x36;
constexpr u8 arcade1 = 0x37;
constexpr u8 dropZone1 = 0x38;
}  // namespace lights

/// Variables in the data segment. Flags are ff or 0; "12 digits" is a number kept a digit to a
/// byte, as addScore adds them.
namespace var {
// --- the table's own
constexpr u16 lightPhaseSnack = 0x0086;  ///< where the snack lights begin to blink, 0 to 0x0f
constexpr u16 snackLit = 0x0087;  ///< one for each of the three snack lights
constexpr u16 snack = 0x008a;  ///< the snack the ducks light next, 0 to 2
constexpr u16 duckHit = 0x008b;  ///< one for each duck: down
constexpr u16 arcadeOpen = 0x008e;
constexpr u16 frames = 0x0093;  ///< counts the frames played (the arcade's prize goes by it)
constexpr u16 arcadeButtonHit = 0x0094;  ///< the arcade's button was just hit
constexpr u16 inSnack = 0x0095;  ///< the ball is on the snack ramp
constexpr u16 popcorns = 0x0096;
constexpr u16 rightOrbitMultiBonus = 0x0097;  ///< the right orbit's lit prizes
constexpr u16 rightOrbitHoldBonus = 0x0098;
constexpr u16 rightOrbitDoubleBonus = 0x0099;
constexpr u16 cycloneX5 = 0x009a;
constexpr u16 lightPhaseOrbit = 0x009b;  ///< the same for the right orbit's prizes, 0 to 0x17
constexpr u16 skyride = 0x009c;  ///< word: the skyride's next prize, 0 to 2
constexpr u16 demonReward = 0x009e;  ///< word: the times all four top lanes were made
constexpr u16 demon5M = 0x00a0;  ///< the demon's lit prizes
constexpr u16 demonExtraBall = 0x00a1;
constexpr u16 demonJackpot = 0x00a2;
constexpr u16 demonJackpotTimed = 0x00a3;  ///< the jackpot lit by a mode, for as long as it runs
constexpr u16 lightPhasePuke = 0x00a4;  ///< 0 to 3
constexpr u16 lightPhaseDemon = 0x00a5;  ///< 0 to 0x1b
constexpr u16 demonTimeout = 0x00a6;  ///< word: frames the demon holds the ball
constexpr u16 timeoutTunnel = 0x00a8;  ///< words: frames left of what they light
constexpr u16 timeoutSkillShot = 0x00aa;
constexpr u16 tunnelDelay = 0x00ac;  ///< word: frames until the ball goes on from the tunnel
constexpr u16 cyclones = 0x00ae;  ///< word
constexpr u16 cyclonesDigits = 0x00b0;  ///< 12 digits: the same number
constexpr u16 cycloneBonus = 0x00bc;  ///< 12 digits: what they add to the bonus
constexpr u16 orbitRightCycle = 0x00c8;  ///< the right orbit's next prize, 0 to 2
constexpr u16 orbitRightBlinking = 0x00c9;
constexpr u16 madBlinking = 0x00ca;
constexpr u16 pendingModeHit = 0x00cb;  ///< a mode waits for the one running to be over
constexpr u16 pendingModeRamp = 0x00cc;
constexpr u16 flipperLockPuke = 0x00cf;  ///< the flippers do not move the top lanes' lights along
constexpr u16 blockDrain = 0x00d2;  ///< a lost ball is not taken as lost (the side lane's extra ball)
constexpr u16 arcadeReady = 0x00d3;  ///< the arcade's script has got to its prize
constexpr u16 inDemon = 0x00d4;
constexpr u16 arcadeTimeoutSoft = 0x00d5;  ///< word: frames the arcade's prize is shown at least
constexpr u16 arcadeJingleDone = 0x00d7;
constexpr u16 arcadeTimeoutHard = 0x00d8;  ///< word: frames it holds the ball at most
constexpr u16 crazyShown = 0x00da;  ///< 1: the last CRAZY letter's award showed something
constexpr u16 crazyFromArcade = 0x00db;  ///< the letter is the arcade's prize
constexpr u16 tunnelSkillShot = 0x00dc;  ///< 12 digits each, from here on
constexpr u16 cycloneSkillShot = 0x00e8;
constexpr u16 modeHitScore = 0x00f4;
constexpr u16 modeRampScore = 0x0100;
constexpr u16 jackpot = 0x010c;
constexpr u16 jackpotStep = 0x0118;
constexpr u16 jackpotStart = 0x0124;
constexpr u16 bonusMultiplier = 0x01fc;  ///< 1, 2, 4, 6, 8: the bonus counted this many times at the end
constexpr u16 bonusOnce = 0x01fd;  ///< 12 digits: the bonus as it was, before the multiplier
constexpr u16 players = 0x0209;  ///< the eight players' records, rec::size apart (not at::players, how many)
constexpr u16 timeoutSpringLoop = 0x05a9;  ///< words: frames left
constexpr u16 timeoutPartyPr = 0x05ab;  ///< in which the tunnel and the cyclone light P and R
constexpr u16 timeoutPartyT = 0x05ad;  ///< in which the skyride and the orbits light T
constexpr u16 dropZoneDelay = 0x05af;  ///< word: frames until the ball goes on from the drop zone
constexpr u16 modeHit = 0x05b1;  ///< the timed modes: HAPPY HOUR counts the ducks,
constexpr u16 modeRamp = 0x05b2;  ///< and MEGA LAUGH the ramps
constexpr u16 arcadeList = 0x05b4;  ///< 128 bytes: which prize, by the count of frames
constexpr u16 arcadePrizes = 0x0634;  ///< words: the arcade's six prizes' routines
constexpr u16 crazyRow = 0x06ce;  ///< the next CRAZY letter, then the row of their lights

// --- the engine's (Names.inc)
using at::awardQuiet;
using at::ballHidden;
using at::ballOnPlunger;
using at::ballUntouched;
using at::ballVx;
using at::ballVy;
using at::betweenBalls;
using at::bonus;
using at::countdownTens;
using at::countdownUnits;
using at::extraBalls;  ///< balls owed
using at::flipperPressed;
using at::flippersLive;
using at::gotHighScore;  ///< (here: ff, the game ended to the best scores' tune)
using at::holdBonus;
using at::idleFrames;
using at::idleShowing;
using at::jingleEnded;  ///< (here: the tune came to its jump, cs:3a6a)
using at::jingleEndedRules;
using at::jingleTimeout;
using at::lastTrigger;
using at::layer;
using at::lightStates;
using at::lookAt;
using at::lookAtSoft;
using at::loopCounter;
using at::matchWon;
using at::modeRunning;
using at::musicAfter;
using at::musicKept;
using at::musicNext;
using at::musicPriority;
using at::musicRepeats;
using at::nextStep;
using at::partyOn;  ///< (here: the ball came back unscored, and is served again)
using at::pendingMode;
using at::player;
using at::playersMayJoin;
using at::sadJingle;  ///< (here: ff, the game ended to the game over's own tune)
using at::score;
using at::scoredThisBall;
using at::screenTop;
using at::scriptDone;  ///< (here, also: the secret's script lets the ball go on)
using at::specialPlungerEvent;
using at::task;
using at::taskState;
using at::tilted;
using at::timerSlot;
// --- the engine's, that Names.inc does not name
constexpr u16 dropZoneScroll = 0x3387;  ///< word: the row the screen is taken up to, a step at a time
constexpr u16 bonusTimes = 0x33b1;  ///< word: the bonus is added this many times
constexpr u16 modeScored = 0x33df;
}  // namespace var

/// Where in a player's record (var::players) each thing is kept from ball to ball.
namespace rec {
constexpr u16 size = 0x74;
constexpr u16 lights = 0x00;  ///< kKeptLights, one byte each
constexpr u16 score = 0x11;  ///< 12 digits
constexpr u16 bonus = 0x1d;  ///< 12 digits
constexpr u16 tunnelSkillShot = 0x29;  ///< 12 digits
constexpr u16 cycloneSkillShot = 0x35;  ///< 12 digits
constexpr u16 cyclonesDigits = 0x59;  ///< 12 digits
constexpr u16 cycloneBonus = 0x65;  ///< 12 digits
constexpr u16 cyclones = 0x71;  ///< word
constexpr u16 crazyRow = 0x73;
}  // namespace rec

/// Numbers to add, 12 digits each, by their value.
namespace points {
constexpr u16 k1 = 0x0130;
constexpr u16 k5 = 0x013c;
constexpr u16 k500000 = 0x0154;
constexpr u16 k100000 = 0x0160;
constexpr u16 k1000000 = 0x016c;
constexpr u16 k5000000 = 0x0178;
constexpr u16 k7510 = 0x0184;
constexpr u16 k750 = 0x0190;
constexpr u16 k20070 = 0x019c;
constexpr u16 k1000 = 0x01a8;
constexpr u16 k50030 = 0x01b4;
constexpr u16 k50000 = 0x01c0;
constexpr u16 k5000 = 0x01cc;
}  // namespace points

/// The awards (Engine::award: a tune, a score, a bonus and a script).
namespace awards {
constexpr u16 arcadeSideExtraBall = 0x0640;  ///< the arcade's six prizes
constexpr u16 arcade5M = 0x065d;
constexpr u16 arcade1M = 0x0679;
constexpr u16 arcade500k = 0x0695;
constexpr u16 arcadeNoScore = 0x06b1;
constexpr u16 drained = 0x06d5;
constexpr u16 arcade = 0x06f1;
constexpr u16 partyP = 0x070d;
constexpr u16 partyA = 0x0729;
constexpr u16 partyR = 0x0745;
constexpr u16 partyT = 0x0761;
constexpr u16 partyY = 0x077d;
constexpr u16 tunnel1M = 0x0799;
constexpr u16 tunnel3M = 0x07b5;
constexpr u16 tunnel5M = 0x07d1;
constexpr u16 demon250k = 0x07ed;  ///< the demon, with nothing lit
constexpr u16 snackNope = 0x0809;
constexpr u16 orbit250k = 0x0825;
constexpr u16 orbit500k = 0x0841;
constexpr u16 orbit750k = 0x085d;
constexpr u16 demon5M = 0x0879;
constexpr u16 duckAll = 0x0895;
constexpr u16 snack0 = 0x08b1;
constexpr u16 snack1 = 0x08cd;
constexpr u16 snack2 = 0x08e9;
constexpr u16 skyrideLitMb = 0x0921;
constexpr u16 orbitMb2 = 0x093e;
constexpr u16 orbitMb4 = 0x095a;
constexpr u16 orbitMb6 = 0x0976;
constexpr u16 orbitMb8 = 0x0992;
constexpr u16 orbitHoldBonus = 0x09ae;
constexpr u16 orbitDoubleBonus = 0x09ca;
constexpr u16 demonExtraBall = 0x09e6;
constexpr u16 sideExtraBall = 0x0a02;
constexpr u16 orbitCrazy = 0x0a1e;
constexpr u16 arcadeCrazy = 0x0a3a;
constexpr u16 orbitMad0 = 0x0a56;
constexpr u16 orbitMad1 = 0x0a72;
constexpr u16 orbitMad2 = 0x0a8e;
constexpr u16 skyride0 = 0x0aab;
constexpr u16 skyride1 = 0x0ac7;
constexpr u16 skyride2 = 0x0ae3;
constexpr u16 cyclone = 0x0aff;
constexpr u16 cycloneX5 = 0x0b1b;
constexpr u16 secret = 0x0b37;
constexpr u16 cycloneSkillShot = 0x0b54;
constexpr u16 tunnelSkillShot = 0x0b70;
constexpr u16 rollInner = 0x0b8c;
constexpr u16 happyHour = 0x0ba9;
constexpr u16 happyHourEnd = 0x0bc5;
constexpr u16 megaLaugh = 0x0be1;
constexpr u16 megaLaughEnd = 0x0bfd;
}  // namespace awards

/// The sound effects' records.
namespace sounds {
constexpr u16 hitDuck = 0x0c19;
constexpr u16 raiseDucks = 0x0c1d;
constexpr u16 ballDrained = 0x0c31;
constexpr u16 issueBall = 0x0c35;
constexpr u16 snacksRelease = 0x0c41;
constexpr u16 rollover = 0x0c45;
constexpr u16 inlane = 0x0c49;
constexpr u16 arcadeButton = 0x0c5d;
}  // namespace sounds

/// The tunes' records: the place in the song, then two bytes more.
namespace tunes {
constexpr u16 plunger = 0x0c6f;
constexpr u16 main = 0x0c72;
constexpr u16 attract = 0x0c75;
constexpr u16 warnTilt = 0x0c78;
constexpr u16 tilt = 0x0c7b;
constexpr u16 gameStart = 0x0c87;
constexpr u16 jackpot = 0x0ca2;
}  // namespace tunes

/// Display scripts the rules start.
namespace scripts {
constexpr u16 partyOn = 0x1477;  ///< the ball came back unscored
constexpr u16 enter = 0x148d;
constexpr u16 jackpot = 0x1647;
constexpr u16 jackpotModeHit = 0x1651;
constexpr u16 jackpotModeRamp = 0x166f;
constexpr u16 main = 0x1acc;
constexpr u16 tilt = 0x1bf0;
}  // namespace scripts

/// Display texts the rules write into.
namespace text {
constexpr u16 matchPlayers = 0x1c16;  ///< each player's match digit, two characters apart
constexpr u16 matchWinners = 0x1c28;  ///< the same, the others rubbed out
constexpr u16 playerDigit1 = 0x1cf9;  ///< the player's digit, in PARTY ON's text
constexpr u16 playerDigit2 = 0x1d91;  ///< and in "SHOOT AGAIN PLAYER 1"
constexpr u16 bonusX = 0x1db9;  ///< "BONUS X8"'s digit
constexpr u16 multiplier = 0x2238;  ///< the last digit of "YOU HAVE XX BONUS"
}  // namespace text

/// A piece of a collision mask that comes and goes: where it goes in
/// which mask, the shape of it raised and dropped, and its size.
struct Gate {
  u16 segment;  ///< the mask's: the playfield's, or the ramps'
  u16 at, raised, dropped, width, rows;
};
namespace gates {
constexpr Gate duck0{at::wallsGround, 0x2b5a, 0x68b0, 0x68d0, 2, 0x0f};  ///< raised: the duck stands
constexpr Gate duck1{at::wallsGround, 0x2e2b, 0x68f0, 0x6910, 2, 0x0f};
constexpr Gate duck2{at::wallsGround, 0x30fc, 0x6940, 0x6930, 1, 0x0f};
/// The top of the left ramp, the skyride's gate: its shape raised is the mask's own, kept at
/// start-up.
constexpr Gate skyride{at::wallsRamps, 0x0266, 0x1356, 0x1332, 2, 0x12};
}  // namespace gates

/// Where the ball is put.
struct Spot {
  u16 x, y;
};
namespace spots {
constexpr Spot dropZone{0x0f, 0x2f};  ///< out of sight, at the top of the table
constexpr Spot snacks{0x03, 0xfd};  ///< the top of the left ramp
constexpr Spot demon{0x101, 0x136};
}  // namespace spots

/// Routines the table runs every frame (Engine::addTimer).
namespace timers {
constexpr u16 partyOn = 0x02ba;
constexpr u16 drainSfx = 0x02f1;
constexpr u16 dropZoneStart = 0x1104;
constexpr u16 dropZoneWait = 0x1175;
constexpr u16 dropZoneRelease = 0x119f;
constexpr u16 dropZoneScrollStart = 0x1217;
constexpr u16 dropZoneScroll = 0x1228;
constexpr u16 resetArcadeButton = 0x1397;
constexpr u16 duckDrop0 = 0x1419;
constexpr u16 duckDrop1 = 0x1499;
constexpr u16 duckDrop2 = 0x1519;
constexpr u16 duckAllUnblink = 0x164b;
constexpr u16 orbitRightUnblink = 0x1880;
constexpr u16 madUnblink0 = 0x1900;
constexpr u16 madUnblink1 = 0x1940;
constexpr u16 madAllUnblink = 0x19a1;
constexpr u16 secretDrop = 0x1aaf;
constexpr u16 cycloneX5Blink = 0x1ac7;
constexpr u16 cycloneX5End = 0x1b04;
constexpr u16 tunnelDropZoneStart = 0x1c0f;
constexpr u16 tunnelFreeze = 0x1c36;
constexpr u16 arcadePickReward = 0x1da2;
constexpr u16 arcadeDropZoneStart = 0x1f32;
constexpr u16 doubleBonusBlink = 0x20e1;
constexpr u16 doubleBonusEnd = 0x2111;
constexpr u16 snacksWait = 0x2137;
constexpr u16 snacksRelease = 0x218f;
constexpr u16 snacksFinish = 0x2246;
constexpr u16 demonBlink = 0x242a;
constexpr u16 demonRelease = 0x244c;
constexpr u16 sideExtraBallFinish = 0x2560;
constexpr u16 skyrideUnblink = 0x268f;
constexpr u16 pukeUnblink0 = 0x270f;
constexpr u16 pukeUnblink3 = 0x276c;
constexpr u16 pukeUnblink1 = 0x27c9;
constexpr u16 pukeUnblink2 = 0x2826;
constexpr u16 pukeUnblinkAll = 0x28bb;
constexpr u16 happyHour = 0x2f40;  ///< these two start a mode that waited
constexpr u16 megaLaugh = 0x2f55;
}  // namespace timers

/// The counters those of them that wait count their frames in (Engine::countTo), by the
/// same names.
namespace counters {
constexpr u16 partyOn = 0x35ca;
constexpr u16 drainSfx = 0x35cc;
constexpr u16 dropZoneStart = 0x35d6;
constexpr u16 dropZoneWait = 0x35d8;
constexpr u16 dropZoneRelease = 0x35da;
constexpr u16 resetArcadeButton = 0x35dc;
constexpr u16 duckDrop0 = 0x35de;
constexpr u16 duckDrop1 = 0x35e0;
constexpr u16 duckDrop2 = 0x35e2;
constexpr u16 duckAllUnblink = 0x35e4;
constexpr u16 orbitRightUnblink = 0x35e6;
constexpr u16 madUnblink0 = 0x35e8;
constexpr u16 madUnblink1 = 0x35ea;
constexpr u16 madAllUnblink = 0x35ec;
constexpr u16 cycloneX5Blink = 0x35ee;
constexpr u16 cycloneX5End = 0x35f0;
constexpr u16 tunnelDropZoneStart = 0x35f2;
constexpr u16 tunnelFreeze = 0x35f4;
constexpr u16 doubleBonusBlink = 0x35f6;
constexpr u16 doubleBonusEnd = 0x35f8;
constexpr u16 snacksWait = 0x35fa;
constexpr u16 snacksRelease = 0x35fc;
constexpr u16 snacksFinish = 0x35fe;
constexpr u16 demonBlink = 0x3600;
constexpr u16 demonRelease = 0x3602;
constexpr u16 sideExtraBallFinish = 0x3604;
constexpr u16 skyrideUnblink = 0x3606;
constexpr u16 pukeUnblink0 = 0x3608;
constexpr u16 pukeUnblink3 = 0x360a;
constexpr u16 pukeUnblink1 = 0x360c;
constexpr u16 pukeUnblink2 = 0x360e;
constexpr u16 pukeUnblinkAll = 0x3610;
constexpr u16 happyHour = 0x3612;
constexpr u16 megaLaugh = 0x3614;
}  // namespace counters

/// Routines the rules point at: the arcade's prizes, the display's tasks, and the engine's own.
namespace code {
constexpr u16 duckUnblink0 = 0x13f6;  ///< timers that count in cvar::duckUnblink*
constexpr u16 duckUnblink1 = 0x1476;
constexpr u16 duckUnblink2 = 0x14f6;
constexpr u16 arcadeSideExtraBall = 0x1dfa;
constexpr u16 arcadeCrazy = 0x1e27;
constexpr u16 arcade1M = 0x1e62;
constexpr u16 arcade5M = 0x1e95;
constexpr u16 arcade500k = 0x1ec8;
constexpr u16 arcadeNoScore = 0x1f0a;
constexpr u16 gameStartingTask = 0x2c75;  ///< the script waits while the game is starting
constexpr u16 waitTask = at::waitTask;
constexpr u16 haltTask = 0x52cc;  ///< does nothing: the script waits for good
}  // namespace code

/// The routines the table binds, each by its place in the code (cs:): the triggers and the
/// script's steps after what they are (with Trigger or Step added where timers:: has the name
/// already); the table's hooks, that the
/// engine calls, and the engine's own routines bound here, by Names.inc's names; else after
/// what they do. The rules also look back at some of the roll triggers (var::lastTrigger).
namespace routines {
// --- the hooks (Names.inc)
using at::tableStart;
using at::tableNewGame;
using at::tableAttractMusic;
using at::tableGameMusic;
using at::tableDrained;
using at::tableTilt;
using at::tableTiltWarning;
using at::tableFlipperPressed;
using at::tableFrame;
using at::tableBumper;
using at::tableModeSecond;
using at::waitJingleTask;  ///< the engine's, which Party Land binds as its own
// --- hit triggers
constexpr u16 arcadeButton = 0x134c;
// --- roll triggers
constexpr u16 orbitTopLeft = 0x16b6;
constexpr u16 orbitTopRight = 0x18af;
constexpr u16 secret = 0x1a0e;
constexpr u16 secretTilt = 0x1a6c;
constexpr u16 tunnel = 0x1b29;
constexpr u16 tunnelTilt = 0x1c9c;
constexpr u16 arcade = 0x1cda;
constexpr u16 rampSnack = 0x1f92;
constexpr u16 orbitEntryRight = 0x225d;
constexpr u16 enter = 0x2264;
constexpr u16 demon = 0x2299;
constexpr u16 laneInnerOther = 0x24bc;  ///< the other inlane
constexpr u16 laneInner = 0x24d7;
constexpr u16 laneOuterOther = 0x24f2;  ///< and the outlane
constexpr u16 laneOuter = 0x2577;
constexpr u16 plungerGo = 0x25c2;
constexpr u16 plungerBottom = 0x25d5;
constexpr u16 skyrideTop = 0x25dc;
constexpr u16 skyrideRamp = 0x26ca;
constexpr u16 rampCyclone = 0x29e9;
// --- the steps of the display's scripts
constexpr u16 setupPartyOn = 0x2bf4;
constexpr u16 setupShootAgain = 0x2c14;
constexpr u16 setSpecialPlungerEvent = 0x2c2f;
constexpr u16 noop = 0x2c43;
constexpr u16 arcadeReady = 0x2c52;
constexpr u16 waitWhileGameStarting = 0x2c66;
constexpr u16 jccScoreZero = 0x2c88;
constexpr u16 jccNoBonusMult = 0x2ccd;
constexpr u16 multiplyBonus = 0x2cfd;
constexpr u16 accBonusModeRamp = 0x2d3f;
constexpr u16 accBonusModeHit = 0x2d69;
constexpr u16 accBonusCyclones = 0x2d93;
constexpr u16 secretDropStep = 0x2f6a;
constexpr u16 extraBall = 0x2f7f;
constexpr u16 dummy0 = 0x29e7;  ///< cs:29e7: a routine that does nothing
constexpr u16 dummy1 = 0x29e8;  ///< cs:29e8: a roll trigger that does nothing
constexpr u16 duck0 = 0x13ae;
constexpr u16 duck1 = 0x142e;
constexpr u16 duck2 = 0x14ae;
constexpr u16 skyridePuke0 = 0x26ce;
constexpr u16 skyridePuke3 = 0x272b;
constexpr u16 skyridePuke1 = 0x2788;
constexpr u16 skyridePuke2 = 0x27e5;
}  // namespace routines

/// Variables the program keeps among its code.
namespace cvar {
constexpr u16 duckUnblink0 = 0x13f5;
constexpr u16 duckUnblink1 = 0x1475;
constexpr u16 duckUnblink2 = 0x14f5;
constexpr u16 demonPrizes = 0x2298;  ///< the demon's lit prizes collected
}  // namespace cvar
}  // namespace party

class PartyLand : public Flow {
 public:
  explicit PartyLand(ByteView prg);

 private:
  u8& lightState(u8 light) { return B(static_cast<u16>(party::var::lightStates + light)); }
  using Flow::placeBall;
  void placeBall(party::Spot at) { placeBall(at.x, at.y); }
  void raise(party::Gate gate) { patchMask(gate.segment, gate.at, gate.raised, gate.width, gate.rows); }
  void drop(party::Gate gate) { patchMask(gate.segment, gate.at, gate.dropped, gate.width, gate.rows); }

  void newGameTable();    // cs:000b
  void clearScores();     // cs:0054
  void savePlayer(u16 player) override;   // cs:0e83
  void restorePlayer() override;          // cs:0d48
  void clearBall() override;              // cs:00aa
  void everyFrame();      // cs:2ab1
  void drained();         // cs:0215
  void bindRules();       // PartyLandRules.cpp
  void bindSteps();

  /// The screen is taken up to the drop zone (cs:1217, cs:1228).
  void startDropZoneScroll();
  void hole() { placeBall(party::spots::dropZone); }   ///< the ball put away, out of sight
  void startDropZone();   // cs:1126
  bool partyAllLit();     // cs:1249
  void checkPartyAll();   // cs:126c
  void happyHour();       // cs:1287
  void megaLaugh();       // cs:1302
  void checkCrazyAll(bool carry);  // cs:12ea
  void duckScore();       // cs:152e
  bool duckAll();         // cs:156d
  bool crazyLetter();     // cs:19c7
  void cycloneX5End();    // cs:1b19
  void partyP();          // cs:1c82
  void arcadePickReward();  // cs:1ddb
  void arcadeNotShown();  // cs:1efb
  bool demon5M();         // cs:232a
  bool demonExtraBall();  // cs:2364
  bool demonJackpot();    // cs:23a7
  void incJackpot() { addScore(party::var::jackpot, party::var::jackpotStep); }  // cs:2420
  void partyT();          // cs:26b0
  bool pukeScore();       // cs:2842
  void pukeAll();         // cs:28f5
  void partyR();          // cs:2a94
  void modeCountHit();    // cs:2b9d
  void modeCountRamp();   // cs:2bb7
  void sideExtraBall();   // cs:2540
  void modeCheck();       // cs:2ddc
  /// The bonus goes up by an amount, once for each of its multiplier (cs:154e and its like).
  void addBonus(u16 amount);
  /// The usual end of a score: the display goes back to showing it.
  void scored();
};

}  // namespace encore
