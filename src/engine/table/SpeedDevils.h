#pragma once
// Speed Devils' own rules (TABLE2.PRG, cs:0000 to cs:2799), on the engine the tables share.
// Addresses here are this table's own, and named below. The names are those of pfr's rewrite
// of the game (github.com/wanda-phi/pfr: src/table/speed.rs and the binds in its assets) where
// it has the same thing, and the engine's (Names.inc) for what the engine shares; each routine
// still says where it is in the program ("cs:2281").
#include <initializer_list>

#include "engine/table/Flow.h"

namespace encore {

/// Speed Devils' places and numbers, by its own addresses.
namespace speed {

/// The lights, by their numbers.
namespace lights {
constexpr u8 pitLoopExtraBall = 0x01;
constexpr u8 pitStopHoldBonus = 0x02;
constexpr u8 pitStopSuperJackpot = 0x03;
constexpr u8 offroadMultiBonus = 0x04;
constexpr u8 miniRampJump = 0x05;
constexpr u8 pit = 0x06;  ///< the three PIT lanes, 0x06 to 0x08
constexpr u8 pitStopGoal = 0x09;
constexpr u8 carPartLit = 0x0a;  ///< the five car parts to be taken, 0x0a to 0x0e; by pfr's numbers 0 to 4:
constexpr u8 carPartLit0 = 0x0b, carPartLit1 = 0x0d, carPartLit2 = 0x0e, carPartLit3 = 0x0c, carPartLit4 = 0x0a;
constexpr u8 miniRampJackpot = 0x0f;
constexpr u8 bur = 0x10;  ///< the three BUR targets, 0x10 to 0x12
constexpr u8 nin = 0x13;  ///< the three NIN targets, 0x13 to 0x15
constexpr u8 gear = 0x16;  ///< the four GEAR lights, 0x16 to 0x19: the jump, the off road ramp, BUR, NIN
constexpr u8 gearNum = 0x1a;  ///< the six gears, 0x1a to 0x1f
constexpr u8 place = 0x20;  ///< the ten places in the race, 0x20 to 0x29
constexpr u8 bonus = 0x2a;  ///< the bonus multipliers 2 to 9: 0x2a to 0x31
constexpr u8 carPart = 0x32;  ///< the five car parts taken, 0x32 to 0x36; by pfr's numbers 0 to 4:
constexpr u8 carPart0 = 0x32, carPart1 = 0x35, carPart2 = 0x34, carPart3 = 0x33, carPart4 = 0x36;
constexpr u8 extraBall = 0x37;
constexpr u8 speed = 0x38;  ///< the twelve lights of the speed, 0x38 to 0x43
}  // namespace lights

/// Variables in the data segment. Flags are ff or 0; "12 digits" is a number kept a digit to a
/// byte, as addScore adds them.
namespace var {
// --- the table's own
constexpr u16 miles = 0x0056;  ///< word: trips up the miles ramps (pfr: num_cyclone)
constexpr u16 milesDigits = 0x0058;  ///< 12 digits: the same number (pfr: NumCyclone, CycloneIncr)
constexpr u16 milesBonus = 0x0064;  ///< 12 digits: what they add to the bonus, at the end of the ball
constexpr u16 matchWon = 0x0070;
constexpr u16 extraBalls = 0x0071;  ///< balls owed
constexpr u16 lightPhasePlace = 0x007a;  ///< where the blinking lights begin, 0 to 0x1d
constexpr u16 playerRecords = 0x00b3;  ///< the eight players' records, rec::size apart (see rec)
constexpr u16 bonusMultiplier = 0x095d;  ///< 1 to 9
constexpr u16 bonusOnce = 0x095e;  ///< 12 digits: the bonus as it was, before the multiplier
constexpr u16 jackpot = 0x096a;  ///< 12 digits each, from here on
constexpr u16 jackpotStep = 0x0976;
constexpr u16 jackpotStart = 0x0982;
constexpr u16 blinkBur = 0x1067;  ///< one for each BUR target: its light blinks
constexpr u16 blinkNin = 0x106a;  ///< one for each NIN target
constexpr u16 timeoutPit2 = 0x106d;  ///< words: frames until each PIT lane's light stops blinking
constexpr u16 timeoutPit1 = 0x106f;
constexpr u16 timeoutPit0 = 0x1071;
constexpr u16 timeoutPitAll = 0x1073;  ///< word: and all three's
constexpr u16 timeoutJackpot = 0x1075;  ///< word: frames in which the mini ramp's jackpot stays lit
constexpr u16 mbPending = 0x1077;  ///< multipliers lit at the off road ramp, not yet collected
constexpr u16 mbActive = 0x1078;  ///< and those collected
constexpr u16 curPlace = 0x1079;  ///< word: the places taken in the race
constexpr u16 maxPlace = 0x107b;  ///< word: the places lit to be taken
constexpr u16 timeoutMilesRight = 0x107d;  ///< words: frames in which the other miles ramp overtakes
constexpr u16 timeoutMilesLeft = 0x107f;
constexpr u16 pedalMetal1 = 0x1081;  ///< words: all three ff, the jump puts the pedal to the metal;
constexpr u16 pedalMetal0 = 0x1083;  ///< an overtaking sets these two,
constexpr u16 pedalMetalJump = 0x1085;  ///< and the jump this one
constexpr u16 curGear = 0x1087;  ///< word: the gears lit
constexpr u16 timeoutGearBlink = 0x1089;  ///< word: frames that the six gears blink, all of them lit
constexpr u16 curSpeed = 0x108b;  ///< word: the speed's lights lit
constexpr u16 carMods = 0x10a1;  ///< word: the car parts lit to be taken so far
constexpr u16 offroadAt = 0x10a3;  ///< word: the miles that start the next off road
constexpr u16 jumpAt = 0x10a5;  ///< word: the miles that light the next jump
constexpr u16 modeHit = 0x10a7;  ///< the timed modes: the one that counts targets (off road),
constexpr u16 modeRamp = 0x10a8;  ///< and the one that counts ramps (turbo)
constexpr u16 holdBonus = 0x10aa;  ///< the bonus is kept for the next ball
constexpr u16 specialPlungerEvent = 0x10ad;  ///< the ball was put back in the plunger lane
constexpr u16 partyOn = 0x10ae;  ///< ff: the tune a ball opens with is not played (Flow::serveMusic)
constexpr u16 modeHitScore = 0x1373;  ///< 12 digits
constexpr u16 modeRampScore = 0x1424;  ///< 12 digits

// --- the engine's, as this table has them (Party Land's names, where it has them)
constexpr u16 musicNext = 0x2119;
constexpr u16 musicAfter = 0x211b;
constexpr u16 musicRepeats = 0x211c;
constexpr u16 ballVx = 0x2cfc;
constexpr u16 ballVy = 0x2cfe;
constexpr u16 ballHidden = 0x2d3a;  ///< ff: the ball held still, and out of the physics
constexpr u16 lastTrigger = 0x331c;  ///< the routine of the roll trigger the ball last crossed
constexpr u16 layer = 0x331e;  ///< 0 playfield, ff ramps
constexpr u16 lookAtSoft = 0x334d;
constexpr u16 musicPriority = 0x3351;
constexpr u16 ballOnPlunger = 0x3352;
constexpr u16 flipperPressed = 0x3353;
constexpr u16 bonus = 0x3361;  ///< 12 digits
constexpr u16 bonusTimes = 0x3379;  ///< word: the bonus is added this many times
constexpr u16 betweenBalls = 0x3396;
constexpr u16 flippersLive = 0x3397;
constexpr u16 scoredThisBall = 0x33a6;
constexpr u16 modeScored = 0x33a7;
constexpr u16 ballUntouched = 0x33a8;
constexpr u16 modeRunning = 0x33aa;
constexpr u16 playersMayJoin = 0x33ab;
constexpr u16 nextStep = 0x33ad;
constexpr u16 task = 0x33af;
constexpr u16 taskState = 0x33b1;
constexpr u16 countdownHeld = 0x33bf;  ///< the mode's count-down waits (a jackpot is being shown)
constexpr u16 lightStates = 0x361d;  ///< by light number: ff lit
constexpr u16 countdownTens = 0x378a;  ///< the count-down's two digits, as characters
constexpr u16 countdownUnits = 0x378b;
constexpr u16 idleFrames = 0x378d;  ///< word: frames since anything scored
constexpr u16 idleShowing = 0x378f;  ///< word: ff while the display shows the best scores for it
constexpr u16 player = 0x37b1;  ///< the one playing, from 1
constexpr u16 score = 0x464d;  ///< 12 digits
constexpr u16 scoreDrawn = 0x4671;  ///< 12: the score's digits as the display has them
constexpr u16 scoreDrawnGroups = 0x467d;
constexpr u16 pitStopHeld = 0x10a9;  ///< ff would keep the pit stop from timing; only compared and cleared (nothing in TABLE2.PRG sets it)
constexpr u16 keptUnread = 0x10ab;  ///< kept from ball to ball and cleared, never read
}  // namespace var

/// Where in a player's record (var::playerRecords) each thing is kept from ball to ball.
namespace rec {
constexpr u16 size = 0x5c;
constexpr u16 score = 0x00;  ///< 12 digits
constexpr u16 bonus = 0x0c;  ///< 12 digits
constexpr u16 milesDigits = 0x18;  ///< 12 digits
constexpr u16 miles = 0x24;  ///< word
constexpr u16 curSpeed = 0x26;  ///< word
constexpr u16 curGear = 0x28;  ///< word
constexpr u16 jumpAt = 0x2a;  ///< word
constexpr u16 offroadAt = 0x2c;  ///< word
constexpr u16 curPlace = 0x2e;  ///< word
constexpr u16 maxPlace = 0x30;  ///< word
constexpr u16 pitStopGoal = 0x32;  ///< a light
constexpr u16 gear = 0x33;  ///< four
constexpr u16 carPartLit = 0x37;  ///< five
constexpr u16 carPart = 0x3c;  ///< five
constexpr u16 carMods = 0x41;  ///< word
constexpr u16 keptUnread = 0x43;  ///< var::keptUnread
constexpr u16 modeRampScore = 0x44;  ///< 12 digits
constexpr u16 modeHitScore = 0x50;  ///< 12 digits
}  // namespace rec

/// The car parts, as the table at ds:108d has them: four bytes each, the award and the light.
namespace carParts {
constexpr u16 table = 0x108d;
constexpr u16 size = 4;
constexpr u16 award = 0, light = 2;
}  // namespace carParts

/// Numbers to add, 12 digits each, by their value.
namespace points {
constexpr u16 k1 = 0x098e;
constexpr u16 k100000 = 0x099a;
constexpr u16 k5000000 = 0x09a6;
constexpr u16 k7510 = 0x09b2;
constexpr u16 k7520 = 0x09be;
constexpr u16 k550 = 0x09ca;
constexpr u16 k570 = 0x09d6;
constexpr u16 k100000b = 0x09e2;  ///< the same number again
}  // namespace points

/// The awards (Engine::award: a tune, a score, a bonus and a script), pfr's effects.
namespace awards {
constexpr u16 turboRamp = 0x0393;
constexpr u16 milesToJump = 0x03af;
constexpr u16 milesToFirstOffroad = 0x03cc;
constexpr u16 milesToExtraBall = 0x03e9;
constexpr u16 milesToOffroad = 0x0406;
constexpr u16 superJackpot = 0x0423;
constexpr u16 jackpot = 0x043f;
constexpr u16 superJackpotGoal = 0x045b;
constexpr u16 holdBonus = 0x0477;
constexpr u16 extraGear = 0x0493;
constexpr u16 extraBall = 0x04af;
constexpr u16 milesExtraBall = 0x04cb;
constexpr u16 jump = 0x04ea;
constexpr u16 milesJump = 0x0506;
constexpr u16 car0 = 0x0522;
constexpr u16 car1 = 0x053e;
constexpr u16 car2 = 0x055a;
constexpr u16 car3 = 0x0576;
constexpr u16 car4 = 0x0592;
constexpr u16 gear = 0x05ae;
constexpr u16 pedalMetal = 0x05ca;
constexpr u16 overtake = 0x05e6;
constexpr u16 overtakeFinal = 0x0602;
constexpr u16 turbo = 0x061e;
constexpr u16 drained = 0x063a;
constexpr u16 laneOuter = 0x0657;
constexpr u16 laneInner = 0x0673;
constexpr u16 pit = 0x0690;
constexpr u16 pitAll = 0x06ad;
constexpr u16 offroadExit = 0x06c9;
constexpr u16 rampOffroad = 0x06e6;
constexpr u16 million = 0x0704;
constexpr u16 miles = 0x0721;  ///< for speed 0 to 11, each 0x1d on (pfr: SpeedMiles0 to 11)
constexpr u16 milesSize = 0x1d;
constexpr u16 mb = 0x087d;  ///< the bonus multiplier 2, then 3 to 9, each 0x1c on (pfr: SpeedMb2 to Mb9)
constexpr u16 mbSize = 0x1c;
}  // namespace awards

/// The sound effects' records.
namespace sounds {
constexpr u16 ballDrained = 0x09fe;
constexpr u16 ejectPit = 0x0a0e;
constexpr u16 inlane = 0x0a1a;
constexpr u16 hitTarget = 0x0a2a;
}  // namespace sounds

/// The tunes' records: the place in the song, then two bytes more.
namespace tunes {
constexpr u16 gameStart = 0x0a41;  ///< and the plunger's
constexpr u16 main = 0x0a44;
constexpr u16 attract = 0x0a47;
constexpr u16 warnTilt = 0x0a4a;
constexpr u16 tilt = 0x0a4d;
constexpr u16 modeOverRamp = 0x0aa1;
constexpr u16 modeHit = 0x0aa7;
constexpr u16 modeOverHit = 0x0aaa;
}  // namespace tunes

/// Display scripts the rules start.
namespace scripts {
constexpr u16 main = 0x12a4;
constexpr u16 modeHit = 0x12bc;  ///< off road
constexpr u16 modeRampContinue = 0x137f;  ///< turbo's count-down, after a jackpot
constexpr u16 modeRamp = 0x138f;  ///< turbo
constexpr u16 shootAgain = 0x157e;  ///< a ball owed is served
constexpr u16 showScore = 0x17fa;
constexpr u16 tilt = 0x1d1b;
}  // namespace scripts

/// Display texts the rules write into.
namespace text {
constexpr u16 milesToJump = 0x1924;  ///< "XXX LITES THE JUMP"
constexpr u16 milesToOffroad = 0x1937;  ///< "XXX LITES OFF ROAD"
constexpr u16 miles = 0x19b5;  ///< "XXX MILES"
constexpr u16 matchPlayers = 0x1d41;  ///< each player's match digit, two characters apart
constexpr u16 matchWinners = 0x1d53;  ///< the same, the others rubbed out
constexpr u16 partyOnPlayer = 0x1d92;  ///< "PARTY ON PLAYER X"'s digit
constexpr u16 shootAgainPlayer = 0x1da9;  ///< "SHOOT AGAIN PLAYER X"'s
constexpr u16 bonusTimes = 0x1dd1;  ///< "BONUS X 8"'s digit
constexpr u16 multiplier = 0x2047;  ///< "YOU HAVE 8X BONUS"'s digit
constexpr u16 playerA = 0x2058;  ///< the player's and the ball's digits, in one text
constexpr u16 ballA = 0x2062;
constexpr u16 playerB = 0x2097;  ///< and in another, from which they are copied
constexpr u16 ballB = 0x209e;
}  // namespace text

/// Where the ball is put.
struct Spot {
  u16 x, y;
};
namespace spots {
constexpr Spot drained{0x118, 0x230};
constexpr Spot pit{0x100, 0x29};
}  // namespace spots

/// A routine the table runs every frame (Engine::addTimer), and the counter it counts frames in.
struct Timer {
  u16 routine, counter;
};
namespace timers {
constexpr Timer drainSfx{0x01f1, 0x3661};
constexpr Timer unblinkBurAll{0x1436, 0x3677};
constexpr Timer unblinkNinAll{0x14d9, 0x3679};
constexpr Timer unblinkGearAll{0x16b6, 0x367b};
constexpr u16 turbo = 0x18be;  ///< these two wait for the mode running to be over
constexpr u16 offroad = 0x1903;
constexpr Timer pitStop{0x19d1, 0x367d};  ///< the pit holds the ball,
constexpr Timer pitStopLong{0x1a29, 0x367f};  ///< longer for the super jackpot at the goal,
constexpr Timer pitEject{0x1a81, 0x3681};  ///< and lets it go
constexpr Timer pitDone{0x1b38, 0x3683};
constexpr Timer unblinkCar{0x1bd9, 0x3685};
constexpr Timer unblinkGear1{0x1cb4, 0x3687};
constexpr Timer resetSuperJackpot{0x1ea9, 0x3689};
constexpr Timer unblinkGear0{0x1ec7, 0x368b};
constexpr Timer unblinkBur0{0x120b, 0x366b};  ///< pfr: SpeedUnblinkBur
constexpr Timer unblinkBur1{0x1263, 0x366d};  ///< pfr: SpeedUnblinkBur
constexpr Timer unblinkBur2{0x12bb, 0x366f};  ///< pfr: SpeedUnblinkBur
constexpr Timer unblinkNin0{0x1313, 0x3671};  ///< pfr: SpeedUnblinkNin
constexpr Timer unblinkNin1{0x136b, 0x3673};  ///< pfr: SpeedUnblinkNin
constexpr Timer unblinkNin2{0x13c3, 0x3675};  ///< pfr: SpeedUnblinkNin
}  // namespace timers

/// Routines the rules point at: the display's tasks.
namespace code {
constexpr u16 gameStartingTask = 0x25aa;  ///< the script waits while the game is starting
constexpr u16 waitTask = 0x4a84;
constexpr u16 nothingTask = 0x4acb;
}  // namespace code

/// The routines the table binds, each by its place in the code (cs:). pfr's names, its
/// RollTrigger, HitTrigger and UopKind without the table's prefix, where it has them (with
/// Trigger or Step added where timers:: has the name already); the table's hooks, that the
/// engine calls, by Names.inc's names; else after what they do. The rules also look back at
/// some of the roll triggers (var::lastTrigger).
namespace routines {
// --- the hooks
constexpr u16 tableStart = 0x0004;
constexpr u16 tableNewGame = 0x0008;
constexpr u16 tableAttractMusic = 0x0149;
constexpr u16 tableGameMusic = 0x0162;
constexpr u16 tableDrained = 0x0175;
constexpr u16 tableTilt = 0x1078;
constexpr u16 tableTiltWarning = 0x1097;
constexpr u16 tableFlipperPressed = 0x109e;
constexpr u16 tableFrame = 0x241a;
constexpr u16 tableBumper = 0x2512;
constexpr u16 tableModeSecond = 0x2711;
constexpr u16 idleWhileUntouched = 0x0c70;  ///< a timer: no idle count until the ball is touched
// --- roll triggers
constexpr u16 plungerExit = 0x11d1;
constexpr u16 pitStopTrigger = 0x193e;
constexpr u16 rampOffroad = 0x1c04;
constexpr u16 pitLoopJump = 0x1d1f;
constexpr u16 pitLoopPre = 0x1ec5;  ///< on the way into the pit loop
constexpr u16 jumpPre = 0x1ec6;  ///< on the way up the jump
constexpr u16 laneOuter = 0x212f;
constexpr u16 laneInner = 0x2136;
constexpr u16 offroadExit = 0x2151;
constexpr u16 enter = 0x2158;
constexpr u16 plungerGo = 0x218d;
constexpr u16 plungerBottom = 0x2194;
constexpr u16 rampMilesRight = 0x2231;
constexpr u16 rampMilesLeft = 0x224e;
// --- the steps of the display's scripts
constexpr u16 startTurbo = 0x0602;
constexpr u16 checkTurboCont = 0x0618;
constexpr u16 clearFlagMode = 0x06f6;
constexpr u16 match = 0x0711;
constexpr u16 setupPartyOn = 0x2535;
constexpr u16 setupShootAgain = 0x2550;
constexpr u16 bell = 0x256b;  ///< (the original rings the PC's bell here)
constexpr u16 setSpecialPlungerEvent = 0x2578;
constexpr u16 noop = 0x258c;
constexpr u16 waitWhileGameStarting = 0x259b;
constexpr u16 jccScoreZero = 0x25bd;
constexpr u16 multiplyBonus = 0x2602;
constexpr u16 jccNoBonusMult = 0x2644;
constexpr u16 jump = 0x2668;
constexpr u16 accBonusModeHit = 0x2674;
constexpr u16 accBonusModeRamp = 0x269e;
constexpr u16 accBonusCyclones = 0x26c8;  ///< (here, the miles)
constexpr u16 extraBall = 0x277e;
constexpr u16 bur0 = 0x11de;  ///< pfr: HitTrigger SpeedBur(0)
constexpr u16 bur1 = 0x1236;  ///< pfr: HitTrigger SpeedBur(1)
constexpr u16 bur2 = 0x128e;  ///< pfr: HitTrigger SpeedBur(2)
constexpr u16 nin0 = 0x12e6;  ///< pfr: HitTrigger SpeedNin(0)
constexpr u16 nin1 = 0x133e;  ///< pfr: HitTrigger SpeedNin(1)
constexpr u16 nin2 = 0x1396;  ///< pfr: HitTrigger SpeedNin(2)
constexpr u16 pit2 = 0x1ee3;  ///< pfr: RollTrigger SpeedPit(2)
constexpr u16 pit1 = 0x1f4f;  ///< pfr: RollTrigger SpeedPit(1)
constexpr u16 pit0 = 0x1fc0;  ///< pfr: RollTrigger SpeedPit(0)
}  // namespace routines

/// Variables the program keeps among its code.
namespace cvar {
constexpr u16 matchFrames = 0x07e0;  ///< frames left of this digit
constexpr u16 matchPace = 0x07e2;  ///< frames each digit stands
constexpr u16 matchLength = 0x0866;  ///< (a word in an instruction) the digits the match runs through
}  // namespace cvar

/// The display's characters: the digits are 0x37 on.
constexpr u8 kDigit0 = 0x37;
constexpr u8 kBlank = 0x2a;

}  // namespace speed

class SpeedDevils : public Flow {
 public:
  explicit SpeedDevils(ByteView prg);

 private:
  u8& b(u16 a) { return nativeB(a); }
  Word w(u16 a) { return nativeW(a); }
  void sfx(u16 record) { sound->effect(b(static_cast<u16>(record + soundRec::sample)), b(static_cast<u16>(record + soundRec::note)), 0, static_cast<u8>(b(static_cast<u16>(record + soundRec::channel)) + 1)); }
  u8& lightState(u8 light) { return b(static_cast<u16>(speed::var::lightStates + light)); }
  /// Where in a blink's period a light begins that is to be out of step with its neighbour.
  static u8 later(u8 start, u8 half) {
    start = static_cast<u8>(start + half);
    return start >= half * 2 ? static_cast<u8>(start - half * 2) : start;
  }
  /// A row of lights shown as a row of flags has them: lit or out, and not blinking.
  void row(u16 flags, u16 count, u8 firstLight);
  void scored();
  void addBonus(u16 amount);
  void all(std::initializer_list<u8> which, void (SpeedDevils::*what)(u8));
  void stop(u8 light) { stopBlink(light); }
  void on(u8 light) { lightOn(light); }
  void off(u8 light) { lightOff(light); }
  void clear(u8 light) { clearLight(light); }
  void flash(u8 light, u8 half) { blink(light, 0, half); }
  using Flow::placeBall;
  void placeBall(speed::Spot at) { placeBall(at.x, at.y); }
  void hold();  ///< the ball kept out of sight in the pit lane

  void savePlayer(u16 player) override;   // cs:0f9e
  void restorePlayer() override;          // cs:0c84
  void clearBall() override;              // cs:004c
  bool extraBallOwed() override { return b(speed::var::extraBalls) != 0; }
  void matchPace() override { nativeCW(speed::cvar::matchFrames) = nativeCW(speed::cvar::matchPace) = high() ? 0x0d : 0x0b; }
  u16 matchLength() override { return kw(speed::cvar::matchLength, 1); }
  void gameOverLights() override {}
  u16 matchRepeated(u16 digit) override { return digit; }
  void matchWon() override { b(speed::var::matchWon) = 0xff; }
  bool restoresAtTurn() override { return false; }
  void ballShown() override { b(speed::text::ballA) = b(speed::text::ballB); }
  void playerShown() override { b(speed::text::playerA) = b(speed::text::playerB); }
  bool redrawNameWhileWaiting() override { return false; }
  void sameBallAgain() override;

  void bindRules();
  void bindSteps();
  void resetJackpot();  // cs:0217
  void incJackpot() { addScore(speed::var::jackpot, speed::var::jackpotStep); }  // cs:0225
  bool hitBur();        // cs:1534
  bool hitNin();        // cs:15b9
  bool gear();          // cs:1638
  void nextGear();      // cs:16f0
  void gearBlinkOver(); // cs:1748
  void addPlaces();     // cs:17ad
  void gainPlace();     // cs:17f1
  void goal();          // cs:186a
  void doTurbo();       // cs:18db
  void offroad();       // cs:18ed
  void doOffroad();     // cs:1920
  void carModsAll();    // cs:1b4f
  bool rollPit();       // cs:2031
  void pedalMetal();    // cs:219b
  void lightCarPart();  // cs:2201
  void overtake();      // cs:226b
  void bumpMiles();     // cs:2281
  void modeCountHit();  // cs:24de
  void modeCountRamp(); // cs:24f8
  void everyFrame();    // cs:241a
  void threeDigits(u16 value, u16 at, bool blankHundreds);
  u16 pitWait_ = 0x14;  ///< how long the pit keeps the ball this time
};

}  // namespace encore
