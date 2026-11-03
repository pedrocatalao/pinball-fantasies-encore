#pragma once
// The engine the four tables share, written again routine by routine from Party Land's
// program (docs/own-engine.md). A comment "cs:1234" is where the routine is in TABLE1.PRG.
#include <array>
#include <optional>
#include <random>
#include <string_view>
#include <utility>
#include <vector>

#include "engine/table/Program.h"

namespace encore {

/// A flipper's record (at::flippers, one after another, a side of 0 ends them), by where each
/// thing is in it.
namespace flipperRec {
constexpr u16 size = 0x3c;
constexpr u16 side = 0x00;  ///< byte: 1 the right flipper's key, 2 the left's
constexpr u8 right = 1, left = 2;  ///< its sides
constexpr int third = 2;  ///< the third, which not every table has
constexpr u16 pictureStamped = 0x01;  ///< byte: the picture last put into the collision mask
constexpr u16 x = 0x02, y = 0x04;  ///< where its box is on the playfield
constexpr u16 width = 0x06;  ///< in sixteens of dots
constexpr u16 rows = 0x08;
constexpr u16 ballXMin = 0x0a, ballXMax = 0x0c, ballYMin = 0x0e, ballYMax = 0x10;
constexpr u16 originX = 0x12, originY = 0x14;  ///< what it turns round
constexpr u16 upright = 0x16;  ///< ff: it stands upright
constexpr u16 maskStep = 0x18;  ///< bytes of its mask from one picture to the next
constexpr u16 speed = 0x1a, position = 0x1c, picture = 0x1e;  ///< as it moves
constexpr u16 lastPicture = 0x20;
constexpr u16 lastPosition = 0x22;
constexpr u16 accelHeld = 0x24, accelLetGo = 0x26;  ///< added to the speed each step
constexpr u16 speedHeldMax = 0x28;  ///< the speed held does not pass
constexpr u16 pictureDrawn = 0x2c;  ///< the picture as last drawn
constexpr u16 backLists = 0x30;  ///< the lists that turn it back, walked down from here
constexpr u16 drawnAt = 0x34;  ///< where in video memory it was last drawn
constexpr u16 lists = 0x36;  ///< and those that turn it on, each listSize long
constexpr u16 listSize = 0x38;
constexpr u16 masks = 0x3a;  ///< the segment of its pictures for the collision mask
}  // namespace flipperRec

/// The video card's memory as the tables lay it out: the display at the top, then the table's
/// picture, 0x54 bytes a row (80 shown and 4 spare), then the plunger and the masks' copies.
namespace video {
constexpr u16 picture = 0x0ad4;  ///< where the table's picture begins
constexpr u16 pictureEnd = 0xc7d4;  ///< and where it ends
constexpr u16 firstRowSpare = 0x0b24;  ///< the spare bytes at the end of the picture's first row
}  // namespace video

/// A box on the playfield and what it leads to: an entry of the bumpers', the kickers' and the
/// triggers' lists (an x of ffff ends them).
namespace zoneRec {
constexpr u16 size = 10;
constexpr u16 xMin = 0, yMin = 2, xMax = 4, yMax = 6;
constexpr u16 then = 8;  ///< the routine to run, or (a bumper's, a kicker's) its award
constexpr u16 boxOnly = 8;  ///< the size of an entry of the ramps' transitions, which have the box alone
}  // namespace zoneRec

/// A light that blinks (at::blinks, one slot after another).
namespace blinkRec {
constexpr u16 size = 4;
constexpr u16 light = 0;  ///< 0: the slot is free
constexpr u16 count = 1;  ///< from where in the period it began
constexpr u16 halfPeriod = 2, period = 3;
}  // namespace blinkRec

/// An entry of the lights' show while nobody plays (at::attractLightList, an ffff count ends it).
namespace attractRec {
constexpr u16 size = 10;
constexpr u16 count = 0, restart = 2;  ///< words: the count, and where it starts again
constexpr u16 offAt = 4, onAt = 6;  ///< words: the counts at which the light goes out and comes on
constexpr u16 light = 8;
}  // namespace attractRec

/// A light's record (at::lightRecords): its first palette entry, how many bytes of colour,
/// then the colours lit, three bytes to an entry.
namespace lightRec {
constexpr u16 firstColour = 0, bytes = 1, colours = 2;
}  // namespace lightRec

/// A sound effect's record: the sample, its note, then the channel less one at 3.
namespace soundRec {
constexpr u16 sample = 0, note = 1, channel = 3;
}  // namespace soundRec

/// A tune's record: where in the song, how many times, at what priority.
namespace tuneRec {
constexpr u16 place = 0, repeats = 1, priority = 2;
}  // namespace tuneRec

/// An award's record (Engine::award), from after its tune: the score, the bonus (12 digits
/// each) and the display's script.
namespace awardRec {
constexpr u16 score = 0, bonus = 12, script = 24;
}  // namespace awardRec

/// A slope of the table (cs:30e9): the pull on the ball across it and down it, a word each.
namespace slopeRec {
constexpr u16 size = 4;
constexpr u16 pullX = 0, pullY = 2;
constexpr u16 lowAngle = 3;  ///< taken from the pull down every slope when the table lies low
constexpr u8 number = 0x0f;  ///< a slope's number, in the low bits of a byte of the slopes' mask
}  // namespace slopeRec

/// One of the best scores (at::bestScoresInsn): its twelve digits, then the three letters of
/// a name, and a byte to spare.
namespace bestScoreRec {
constexpr u16 size = 0x10;
}  // namespace bestScoreRec

/// The keys, by the code the keyboard sends as each goes down (cs:3e44). As a key comes up,
/// its code comes again with `released` set.
namespace scancodes {
constexpr u8 escape = 0x01;
constexpr u8 p = 0x19;  ///< pause
constexpr u8 enter = 0x1c;  ///< one player more
constexpr u8 ctrl = 0x1d;
constexpr u8 leftShift = 0x2a;
constexpr u8 m = 0x32;  ///< the music off and on
constexpr u8 rightShift = 0x36;
constexpr u8 alt = 0x38;
constexpr u8 space = 0x39;  ///< the nudge
constexpr u8 f1 = 0x3b, f8 = 0x42;  ///< a game for one to eight players
constexpr u8 f9 = 0x43, f10 = 0x44, f11 = 0x57, f12 = 0x58;  ///< how fast the screen follows
constexpr u8 pageUp = 0x49, pageDown = 0x51;  ///< (after `extended`) the mid-frame steps, while no game is played
constexpr u8 down = 0x50;  ///< (after `extended`) the plunger
constexpr u8 extended = 0xe0;  ///< comes before an arrow's code, and the right-hand alt's and ctrl's
constexpr u8 released = 0x80;  ///< the bit set in a key's code as it comes up
constexpr u8 up(u8 code) { return static_cast<u8>(code | released); }
constexpr u8 none = 0xff;  ///< at::lastKey: no key waits to be read
constexpr u8 keyDownMark = 0x0f;  ///< what at::keyDown is set to as a key goes down
}  // namespace scancodes

/// The bits of at::keys.
namespace keyBits {
constexpr u8 rightFlipper = 1, leftFlipper = 2;
constexpr u8 oneStep = 4;  ///< the ball is moved once, not twice, in the mid-frame callback
}  // namespace keyBits

/// What F11 and F12 leave in at::scrollKey, for the program's loop to act on.
namespace scrollKey {
constexpr u8 f11 = 0x0b, f12 = 0x0c;
}  // namespace scrollKey

/// The materials, by the three bits the masks give where the ball touches:
/// a mark, a wall, a slope's mask.
namespace material {
constexpr u8 markBit = 1, wallBit = 2, slopeBit = 4;
constexpr u8 flipper = 2;
constexpr u8 kicker = 3;   ///< MATERIAL_KICKER
constexpr u8 bumper = 7;   ///< MATERIAL_BUMPER
constexpr u16 size = 16;  ///< a material's record (at::materials)
constexpr u16 minBounceSpeed = 6;  ///< the speed in it
}  // namespace material

/// Which set of masks the ball is on (at::layer).
namespace layers {
constexpr u8 playfield = 0, ramps = 0xff;  ///< the ball's layer
}  // namespace layers

/// The copies of the masks the original keeps in video memory (Engine::copiedMask).
namespace maskCopy {
constexpr int rampsMarks = 0, rampsSlopes = 1, groundSlopes = 2;
}  // namespace maskCopy

/// Angles, in 2048ths of a turn from pointing right, clockwise.
namespace ballAngles {
/// The quarters a touch was in, a bit each (from pointing right on); three sets lie either
/// side of pointing right.
constexpr int q1 = 1, q2 = 2, q3 = 4, q4 = 8;
}  // namespace ballAngles

/// What a task of the display's leaves in SI (at::taskState) besides a count of frames:
/// nought when it is done.
namespace taskState {
constexpr u16 waiting = 0xff;  ///< not done: waiting on the music or a sound
constexpr u16 countingBonus = 0x4b;  ///< not done: what the bonus's count leaves
}  // namespace taskState

/// The display, as it lies in the first and third planes of video memory: 16 rows of 160
/// dots, each byte a dot, the two planes by turns. Its places are offsets in a plane, from
/// the top of video memory, whose first row it does not use.
namespace display {
constexpr u16 top = 0xa8;  ///< where its first row begins
constexpr u16 beforeTop = 0xa7;  ///< the byte before it, which the animations' dots and the wipe to the right count from
constexpr u16 end = 0xb28;  ///< past its last row
constexpr u16 rightEnd = 0xf8;  ///< past the end of its first row
constexpr u16 line = 0x150;  ///< a line of the 13-dot font, a row down from the top
constexpr u16 paused = 0x162;  ///< GAME PAUSED, in the middle of that line
constexpr u16 score = 0xc8;  ///< the score in its large digits
constexpr u16 bigScore = 0xa0;  ///< the same, further left, for the script's dmBigScore
constexpr u16 bonus = 0x488;  ///< the bonus as it is counted, in the 8-dot font
constexpr u16 bonusClear = 0x498;  ///< and what is cleared when it is
constexpr u16 matchDigits = 0x540;  ///< the match's digits, 0 to 9, eight bytes apart
constexpr u16 countdownSeconds = 0x240;  ///< a mode's count-down, in the 11-dot font
constexpr u16 countdownScore = 0x158;  ///< and its score beside it, in the 13-dot
constexpr u16 longMsgPlace = 0x2a0;  ///< the long message's place, as the program starts (cs:6e7c)
constexpr u16 stripedEnd = 0x348;  ///< the striped wipe: four rows, each band of four cleared together
constexpr u16 commaMark = 0xa1b;  ///< the mark between the thousands, from the score's place less display::score
constexpr u8 notDrawn = 0xcc;  ///< at::numberDrawn: a digit no digit is, so that all are drawn afresh
constexpr u8 scoreNotDrawn = 0x12;  ///< at::scoreDrawn: the same
constexpr u8 unchanged = 0xff;  ///< at::numberShown: the digit is left as it is
constexpr u8 longMsgEnd = 0xff;  ///< ends a long message
constexpr u8 animSkip = 0x7f;  ///< an animation's byte that only moves on
}  // namespace display

/// An animation of the display's (cs:6fb5), by where things are from its first frame: before
/// it, where to go back to, how many rounds and where the frames end; each frame how long it
/// stays, then where its dots are.
namespace animRec {
constexpr u16 back = 6, rounds = 4, end = 2;  ///< before the first frame
constexpr u16 frameTime = 2;  ///< in a frame, after where its dots are
}  // namespace animRec

/// The display's four fonts (setFont), by their height.
namespace font {
constexpr int h13 = 0, h11 = 1, h8 = 2, h5 = 3;
}  // namespace font

/// The map masks that pick a plane of the video card (the long messages' letters are drawn in
/// the first and the third by turns).
namespace planeMask {
constexpr u8 first = 1, third = 4;
}  // namespace planeMask

/// What the sound driver's status (int 66h, function 0x15) says, by its bits.
namespace driverStatus {
constexpr u8 effectSounding = 8;
constexpr u8 silent = 0x0a;  ///< what the silent driver always answers
}  // namespace driverStatus

/// How quickly the screen follows the ball, as PINBALL.CFG says it.
namespace scrollOption {
constexpr u8 hard = 0, medium = 1, soft = 2;
}  // namespace scrollOption

/// The angles the table may lie at (Engine::setAngle).
namespace tableAngle {
constexpr int low = 0, high = 1, steeper = 2;
}  // namespace tableAngle

/// The tables, as Program numbers them.
namespace tableId {
constexpr int partyLand = 0, speedDevils = 1, gameshow = 2, stonesNBones = 3;
}  // namespace tableId

/// What a table asks of the sound driver (the original's int 66h). The default is the game's
/// own silent driver, NOSOUND.SDR: it plays nothing and answers 0.
class SoundDriver {
 public:
  virtual ~SoundDriver() = default;
  /// Function 0x11 (int66::playSfx): a sample of the module (1-31) at a note (1-36), on a channel (1-4).
  virtual void effect(u8 sample, u8 note, u8 volume, u8 channel) { (void)sample, (void)note, (void)volume, (void)channel; }
  /// Function 0x10 (int66::jump): go to a place in the song at the next tick.
  virtual u8 jump(u16 position) { (void)position; return 0; }
  /// Function 0x06 (int66::setMasterVolume): the volume, 0x100 for all of it.
  virtual void volume(u16 level) { (void)level; }
  /// Function 0x0f (int66::stop): stop the music.
  virtual void stop() {}
  /// Function 0x04 (int66::start): start it.
  virtual u8 start() { return 0; }
  /// Function 0x15 (int66::status): what is playing; bit 3 is set while an effect still sounds. The silent
  /// driver always answers that one does.
  virtual u8 status() { return driverStatus::silent; }
};

class Engine : public Program {
 public:
  Engine(ByteView prg, int table);

  /// The options, as PINBALL.CFG has them.
  struct Options {
    bool fiveBalls = false;
    bool lowAngle = false;
    u8 scrolling = scrollOption::medium;      ///< 0 hard, 1 medium, 2 soft
    bool musicOff = false;
    bool highResolution = true;
    bool mono = false;
  };
  /// The program's start-up (cs:2f9b), up to where its own loop begins: once, before the
  /// first frame. `bestScores` is the table's TABLEn.HI, 64 bytes, if there is one.
  void start(const Options& options, ByteView bestScores = {});

  /// A key goes down or up, as the keyboard says it: a scancode, bit 7 set for up
  /// (cs:3e44, the table's keyboard interrupt).
  void key(u8 scancode);
  /// One video frame, as the driver's two callbacks run it (cs:41c6, cs:55db), then the
  /// program's own loop (cs:35fd) `loopsPerFrame` times.
  void frame();
  int loopsPerFrame = 8;
  /// The original's loop reads the keys in whatever time the frame's two callbacks leave it,
  /// so a key acts in the frame after the one it was pressed before. With this, the loop's
  /// turns come first, and a key acts in the frame it was pressed before.
  bool keysFirst = false;
  /// The original moves the ball twice at the start of the frame and twice part way down it,
  /// with the table's rules in between. With this, all the frame's moves come first and the
  /// rules after them: what the rules see is where the ball is when the frame is drawn.
  bool stepsTogether = false;
  /// How much of a hit goes into the ball's spin depends on what was hit and how softly. The
  /// original scales those two numbers in 16 bits, where steel's no longer fit for all but
  /// the hardest hits, and what is left of them is what it divides by. With this they are
  /// scaled in full, so a soft touch on steel trades as little as the numbers mean it to.
  bool wholeGains = false;
  /// This version's amendments to the tables' rules, each said where it is made. Without
  /// this the rules are the original's to the byte.
  bool amended = false;
  /// As the silent driver: the music's callback is called every time the driver is polled.
  bool pollCallsMusic = true;
  /// The original lets the number of players be put below the player whose turn it is while
  /// more may still join, and then goes astray (docs/own-engine.md). With this, such a key
  /// is taken for no key.
  bool refuseFewerPlayers = false;
  /// The original takes its chance from the count of its own loop's turns (ds:33ed), which
  /// goes as fast as the machine does. With this, chance is a generator's instead, begun
  /// from `seed`: one number each time the game asks for one, and nothing in between, so the
  /// same game comes of the same seed on any machine.
  void seedChance(u64 seed) { generator_.emplace(seed); }
  /// The original keeps a first ball back half a second longer than the others, while more
  /// players may still be added. With this, every ball is served alike.
  bool servesAlike = false;
  /// A ball was lost and the next has not been put on the plunger yet.
  bool ballParked() const { return lostParked_; }
  /// The program asked to end.
  bool exited() const { return exited_; }
  /// Where the moving things were when the original last drew them (which is not always
  /// where they are now: some are drawn before a frame's steps and some after).
  struct Shown {
    i16 ballX = 0, ballY = 0;
    bool ramps = false;              ///< the ball is on the ramps: other things hide it there
    std::array<u16, 3> flipper{};    ///< which of its pictures each flipper shows
  };
  const Shown& shown() const { return shown_; }
  /// The ball and the flippers taken as shown where they now are, as the frame ends: for a
  /// picture that is drawn once a frame is over. (The flippers' note of the picture last drawn
  /// is moved on too, which nothing but the drawing reads.)
  /// (A ball put away out of play only when the original draws it there, and none while no
  /// game is played.)
  void showAsNow() {
    for (int which = 0; which < 3; ++which) drawFlipper(which);
    if (!std::exchange(ballDrawn_, false) && (B(at::ballHidden) == kSet || B(at::attract) == kSet)) return;
    shown_.ballX = W(at::ballX).s();
    shown_.ballY = static_cast<i16>(W(at::ballY) + (B(at::ballHidden) == kSet ? 0 : W(at::nudgeLift)));
    shown_.ramps = B(at::layer) != 0;
  }
  /// A flipper is drawn standing another way than it was last drawn: its record, and which
  /// of its pictures it showed and shows now. The original gets from one to the other by
  /// lists of changes, and what is left on the screen depends on the way taken.
  std::function<void(u16 record, u16 was, u16 now)> onFlipperDrawn;
  /// The row of the picture at the top of the screen.
  u16 screenRow() const { return screenRow_; }

  // --- what this version lets be changed while a table is played. The original takes its
  // options once, as it starts; these put right what its start-up made of them.
  /// How steeply the table lies: 0 low, 1 high, and 2 steeper than the original has it. At
  /// that one the pull down the table is as much greater again as high is over low, and so
  /// that the top of the table can still be reached the flippers' push, the plunger and the
  /// fastest the ball may go are a quarter more.
  void setAngle(int angle);
  int angle() const { return angle_; }
  /// How quickly the screen follows the ball: 0 hard, 1 medium, 2 soft.
  void setScrolling(u8 scrolling);
  void toggleMusic() { musicKey(); }
  bool musicIsOff() { return B(at::musicOff) != 0; }
  /// The game is paused (the original then waits for a key and does nothing else).
  bool isPaused() const { return pause_ != Pause::No; }
  bool asksToQuit() const { return pause_ == Pause::YesOrNo; }
  /// A line on the display, in the letters the pause is told in: capitals, digits and
  /// spaces, and \ and ] for brackets. What the display showed is not kept: for while the
  /// game is paused, which keeps it and puts it back.
  void write(std::string_view text);
  /// Where the ball was after each of its last steps (there are four to a frame), the newest
  /// last: in 1024ths of a dot.
  struct Step {
    i32 x = 0, y = 0;
  };
  static constexpr std::size_t kSteps = 15;
  const std::array<Step, kSteps>& steps() const { return steps_; }
  /// How many of them are of the ball now in play (the last ones).
  std::size_t stepsKept() const { return stepsKept_; }

  /// The video card's memory, four planes of 64 KB, as far as the table's logic depends on
  /// it. The display is at its start: in the first and third planes, 168 bytes a row, each
  /// byte a dot, 0xf2 lit and 0x60 not. Past the picture are the copies of three masks that the
  /// original reads the ramps' slopes and materials from, and, over parts of those, the
  /// pictures it keeps there to draw with: what it reads there, the engine must read too.
  const std::array<std::vector<u8>, 4u>& videoMemory() const { return video_; }
  std::array<std::vector<u8>, 4u>& videoMemory() { return video_; }

  /// The video card's colours as the table has set them: 256 of red, green, blue, 0-63.
  const std::array<u8, 768>& colours() const { return dac_; }
  std::array<u8, 768>& colours() { return dac_; }

  SoundDriver* sound = &silent_;

 protected:
  // --- the frame (EngineFrame.cpp)
  void frameCallback();
  void midFrameCallback();
  void frameSteps();
  void mainLoop();
  void drawBall();          // cs:4140
  void drawFlipper(int which);  // cs:550e
  void attractFrame();      // cs:6011
  void attractMidFrame();   // cs:60c5
  void scroll();            // cs:4018
  u8 musicCallback(u8 al);  // cs:3a6a
  void keyExtended(u8 al);  // cs:3f8a
  void typeCheat();         // cs:3549
  void playersKey();        // cs:33f9
  void pauseKey();          // cs:31cd
  void paused();            // the same routine's waits for a key, a frame at a time
  void saveDisplay();       // cs:4a6a
  void restoreDisplay();    // cs:4b2a
  void message(u16 nativeText, u16 at);  // cs:4aae: the display cleared, and a line on it
  void nudgeKey();          // cs:3478
  void musicKey();          // cs:34e3

  // --- the ball, and what follows its sub-steps (EnginePhysics.cpp)
  void physicsSteps();      // cs:87c0
  bool probeBall();         // cs:8829
  void bounce();            // cs:8e95
  void nudgeAndFlippers();  // cs:9133
  void integrate();         // cs:908f
  void stampFlippers();     // cs:9106
  void afterSteps();        // cs:59aa
  void bumperEvent();       // cs:5ace
  void pickGravity();       // cs:59d9
  void changeLayer();       // cs:5d24
  void runRules();          // cs:5989
  void rollTriggers();      // cs:5d70
  void hitTriggers();       // cs:5cb9
  void bindPlunger();
  /// One dot of a collision mask (a segment of the program's, 40 bytes a row).
  bool maskBit(u16 partyLandSegment, int x, int y);
  /// A byte of one of the three mask copies in video memory, by its place in the mask
  /// (cs:5a48, cs:5a80): 0 the ramps' marks, 1 the ramps' slopes, 2 the playfield's slopes.
  u8 copiedMask(int which, u16 offset);

  // --- timers: up to 50 routines run once a frame (cs:5b0b, cs:5b2a, cs:576a)
  void addTimer(u16 native);
  /// The same, for one that is the rest of the timer that starts it: it has its first turn in
  /// this frame (see runTimers).
  void addTimerNow(u16 native) {
    startsNow_ = true;
    addTimer(native);
    startsNow_ = false;
  }
  void runTimers();
  void endTimer();
  /// cs:5777: counts the word at `counter` up to `limit`; true, and back to 0, when there.
  bool countTo(u16 nativeCounter, u16 limit);

  // --- lights, which are colours of the picture (EngineLights.cpp)
  void queueColours(u16 nativeRecord, bool half);
  void lightOn(u8 light);      // cs:5728
  void lightOff(u8 light);     // cs:5747
  void setLight(u8 light);     // cs:5788
  void clearLight(u8 light);   // cs:5795
  void blink(u8 light, u8 start, u8 halfPeriod);  // cs:57a2
  void stopBlink(u8 light);    // cs:57c7
  void stopBlinks();           // cs:57e1
  void runBlinks();            // cs:57f0
  void flushColours();         // cs:5918
  void attractLights();        // cs:622d
  void displayFlash();         // cs:4c7d
  void displayNormal();        // cs:4cbd
  void displayInverse();       // cs:4cd4
  void displaySteady();        // cs:4d34

  // --- the display (EngineDisplay.cpp): scripts of steps, each a routine and its arguments
  void bindDisplay();
  void displayStep();               // cs:444c
  void startScript(u16 native);     // cs:44b0
  void runStep(u16 native);         // cs:44c3
  void nextStep(u16 size);          // cs:5344 and its like: the step after this one
  void drawChar(u8 c, u16& at);     // cs:6ccd
  void drawText(u16 nativeText, u16 at);    // cs:6ca5
  void drawNumber(u16 nativeDigits, u16 at);  // cs:6c0f
  void drawCommas(u16 nativeDigits, u16 base);  // cs:6d6f
  void forgetNumber();              // cs:6bbc
  void drawScore(u16 nativeDigits, u16 at);   // the tables' second code segment
  void fillDisplay(u16 at, u16 width, u16 rows);  // cs:49eb
  void setFont(int which);          // 0 to 3: 13, 11, 8 and 5 dots high
  /// Runs one of the original's pictures that are code: a row of "store this register there".
  void compiledPicture(const u8* code, std::size_t size, u16 start, u16 base, int plane, u8 litDot, u8 unlitDot);
  /// The two values a dot of the display has in video memory, which differ from table to table.
  u8 lit() const { return kb(at::dotLitInsn, 1); }
  u8 unlit() const { return kb(at::dotUnlitInsn, 1); }
  u8& dot(int plane, u16 offset) { return video_[static_cast<std::size_t>(plane * 2)][offset]; }

  // --- a game's comings and goings (EngineGame.cpp)
  void bindGame();
  void newGame();              // cs:3881
  void beginBall();            // cs:37ea
  void toAttract();            // cs:5fff
  void lightsOut();            // cs:5867
  void lightsDark();  ///< at a tilt
  /// cs:5c3f: asks for a piece of the music (place, repeats, priority); false if something
  /// more important is playing.
  bool music(u16 nativeRecord);
  void addScore(u16 nativeTo, u16 nativeAmount);  // cs:6a5e: twelve digits, one to a byte
  void placeBall(u16 x, u16 y);
  /// cs:5b9f: an award: its music if nothing more important plays, its score and bonus, and
  /// its script on the display. True if the script was started.
  bool award(u16 nativeRecord);
  /// cs:5b40: the same, whatever else is playing or showing.
  bool awardAlways(u16 nativeRecord);
  /// cs:5c98: lights the next of a row of lights, one more each time; true when that was
  /// the last, and the row starts again.
  bool nextOfRow(u16 nativeRow);
  /// The timer whose turn it is gives up its slot without being counted off (as cs:140b).
  void dropTimer() { nativeW(W(at::timerSlot)) = F(at::nothing); }
  /// Copies a shape into a collision mask (cs:5f5a): `width` bytes a row for `rows` rows.
  void patchMask(u16 partyLandSegment, u16 at, u16 nativeShape, u16 width, u16 rows);
  /// The same by the table's own segment; `step` is how far apart the shape's rows are.
  void copyShape(u16 nativeSegment, u16 at, u16 nativeShape, u16 width, u16 rows, u16 step);

  /// A routine not written yet: says so, with its place.
  void todo(u16 partyLandAddress) { call(F(partyLandAddress)); }
  void effect(u16 record);  ///< plays the four-byte effect record at Party Land's address

  bool high() { return B(at::highResolution) == kSet; }
  /// A number below `n` by chance: the generator's next, or `counted`, which is what the
  /// original makes of its count of loops at this place.
  u16 chance(u16 n, u16 counted) {
    if (!generator_) return counted;
    return static_cast<u16>((*generator_)() % n);
  }
  bool drawsChance() const { return generator_.has_value(); }

  std::array<u8, 768> dac_{};
  std::array<std::vector<u8>, 4u> video_;
  SoundDriver silent_;
  bool exited_ = false;
  enum class Pause { No, AnyKey, YesOrNo } pause_ = Pause::No;
  u16 screenRow_ = 0;
  Shown shown_;
  int angle_ = tableAngle::high;
  std::array<i16, 2> speedLimits_{};  ///< as the start-up left them, while the steeper angle has them greater
  std::array<Step, kSteps> steps_{};
  std::size_t stepsKept_ = 0;
  std::optional<std::mt19937_64> generator_;
  std::array<std::vector<u8>, 2> slopes_;  ///< the slope at each stretch of eight dots, playfield and ramps
  std::array<bool, 64> waiting_{};     ///< the timers started in this frame's turn, to be run from the next
  std::array<bool, 64> startedNow_{};  ///< and the ones to be run in this turn yet
  bool startsNow_ = false;             ///< a timer started now is one of the latter
  bool timersRunning_ = false;
  bool lostParked_ = false;  ///< a ball was lost and the next has not been put on the plunger yet
  /// (amended) Where a ball can come to rest and never leave, as a table says (a box of the
  /// ball's places, and the speed across a shake gives it there): with whole gains (wholeGains) a ball
  /// balanced on a steel edge stays balanced, where the original's sums for a soft touch shake
  /// it off at the first nudge. A shake while it rests in one sets it rolling (freeTrappedBall).
  struct Trap {
    i16 left, top, right, bottom;
    i16 vx;
  };
  std::vector<Trap> traps_;
  int restFrames_ = 0;       ///< (amended) frames the ball in play has stood still
  i16 restX_ = 0, restY_ = 0;
  void freeTrappedBall();
  bool nudgeDue_ = false;
  bool ballDrawn_ = false;  ///< the original drew the ball in this frame  ///< the table was shaken: counted when the frame's rules are run
  bool spinDue_ = false;  ///< the ball was put somewhere: its spin is drawn when it next moves
};

}  // namespace encore
