#pragma once
// One pinball table in play: physics, rules, scripts, lights, dot matrix and sound. A
// translation of pfr's src/table.rs and src/table/*.rs; the member functions are spread
// over several .cpp files the same way (TablePhysics, TableScript, TableTasks, TableGame,
// and one file per table's rules).
#include <array>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "assets/TableAssets.h"
#include "game/Config.h"
#include "gfx/HdLayer.h"
#include "sound/Player.h"
#include "table/Keys.h"
#include "table/TableStates.h"

namespace pfr {

/// A roll trigger with its argument, compared as a pair like pfr's enum.
struct RollTriggerId {
  RollTrigger kind;
  u8 arg = 0;
  bool operator==(const RollTriggerId&) const = default;
};

enum class TaskKind : u8 {
  SetStartKeysActive, PartyOn, IssueBall, IssueBallFinish, IssueBallRelease, IssueBallSfx, IssueBallRaiseSfx,
  DrainSfx, GameOver,
  PartyDropZoneStart, PartyDropZoneWait, PartyDropZoneRelease, PartyDropZoneScroll, PartyResetArcadeButton,
  PartyOrbitRightUnblink, PartyMadUnblink, PartyMadAllUnblink, PartySecretDrop, PartyCycloneX5Blink,
  PartyCycloneX5End, PartyTunnelFreeze, PartyArcadePickReward, PartyArcadeDropZoneStart, PartyDoubleBonusBlink,
  PartyDoubleBonusEnd, PartySnacksRelease, PartySnacksFinish, PartyDemonBlink, PartyDemonRelease,
  PartySideExtraBallFinish, PartySkyrideUnblink, PartyPukeUnblink, PartyPukeUnblinkAll, PartyDuckDrop,
  PartyDuckUnblink, PartyDuckAllUnblink, PartyHappyHour, PartyMegaLaugh,
  SpeedUnblinkBur, SpeedUnblinkBurAll, SpeedUnblinkNin, SpeedUnblinkNinAll, SpeedUnblinkGear, SpeedUnblinkGearAll,
  SpeedOffroad, SpeedTurbo, SpeedPitStop, SpeedUnblinkCar, SpeedResetSuperJackpot,
  ShowResetDropCenter, ShowResetDropLeft, ShowUnblinkDollar, ShowUnblinkDollarAll, ShowVaultEject,
  ShowBillionRelease, ShowSpinWheelEnd, ShowGivePrize, ShowCashpot, ShowCashpotEject,
  StonesUnblinkStone, StonesUnblinkBone, StonesUnblinkStonesBones, StonesUnblinkKey, StonesUnblinkKeyAll,
  StonesResetSuperJackpot, StonesTowerEject, StonesTowerEjectNow, StonesWellEject, StonesVaultEject,
  StonesUnblinkGhosts, StonesModeHit, StonesModeRamp, StonesRaiseKickback, StonesUnblinkRip, StonesUnblinkRipAll,
  StonesScreamExtra,
};

/// A delayed action. `a`/`b` hold the variant's arguments (index, delay, timeouts); `flag`
/// the boolean of PartyArcadeDropZoneStart.
struct Task {
  TaskKind kind;
  u16 a = 0, b = 0;
  bool flag = false;
  u16 timer = 0;
};

/// What the frame asks the application to do.
struct TableAction {
  enum class Kind : u8 { None, SaveOptions, SaveHighScores, Quit } kind = Kind::None;
};

class Table {
 public:
  Table(TableAssets assets, ByteView module, const Config& config, int table, u64 seed);
  /// Straight from the table's DOS executable, as the tests and tools do.
  Table(ByteView prg, ByteView module, const Config& config, int table, u64 seed);
  ~Table();

  TableAction runFrame();
  void handleKey(Key key, bool pressed);
  /// Draws the visible screen: `height` rows of 320 palette indices, plus the palette.
  /// `hd`, if given, receives where the playfield was drawn, for a replacement picture.
  void render(u8* pixels, Rgb* palette, HdFrame* hd = nullptr) const;

  struct SpritePicture {
    int width = 0, height = 0;
    Bytes rgba;
  };
  /// The flippers cut out of the artwork, for drawing them turned to their angle.
  std::vector<SpritePicture> flipperPictures() const;
  /// The ball as the original draws it, for when there is no picture of its own.
  SpritePicture ballPicture() const;
  /// Which side each flipper is on, in the same order.
  std::vector<FlipperSide> flipperSides() const;

  int screenHeight() const;
  const Options& options() const { return options_; }
  const HighScores& highScores() const { return highScores_; }
  int tableIndex() const { return assets_.table; }
  Player& player() { return *player_; }

  // Read-only views for tools and tests.
  std::array<i16, 2> ballPos() const { return {static_cast<i16>(ball_.posHires[0] >> 10), static_cast<i16>(ball_.posHires[1] >> 10)}; }
  bool ballOverhead() const { return ball_.layer == Layer::Overhead; }
  const Bcd& scoreMain() const { return scoreMain_; }
  bool inAttract() const { return inAttract_; }
  bool paused() const { return kbdState_ == KbdState::Paused || kbdState_ == KbdState::PausedConfirmQuit; }
  const TableAssets& assets() const { return assets_; }
  const std::array<std::array<bool, 160>, 16>& dotMatrix() const { return dm_.pixels; }
  u8 currentBall() const { return curBall_; }
  /// Tools can collect a line per trigger fired.
  std::vector<std::string>* trace = nullptr;

 private:
  struct Ball {
    Layer layer = Layer::Ground;
    std::array<i32, 2> posHires{};
    std::array<i16, 2> speed{}, accel{0, 8};
    bool frozen = true;
    i16 rotation = 0, maxSpeed = 0;
    std::array<i16, 2> pos() const { return {static_cast<i16>(posHires[0] >> 10), static_cast<i16>(posHires[1] >> 10)}; }
    void setPos(std::array<i16, 2> p) { posHires = {i32{p[0]} << 10, i32{p[1]} << 10}; }
  };
  /// Where the ball was at each physics step just gone, for the trail behind it. The physics
  /// moves the ball four times a frame, so these are finer than the frames the screen shows.
  static constexpr std::size_t kTrail = 14;
  std::array<std::array<float, 2>, kTrail> ballTrail_{};
  std::size_t trailNext_ = 0, trailLength_ = 0;

  struct Push {
    i16 offsetF9 = 0, speed = 0, speedAttack = 0, speedRelease = 0;
    void frame(bool state);
    i16 offset() const { return static_cast<i16>(offsetF9 >> 9); }
  };
  struct FlipperState {
    i16 pos = 0, speed = 0;
    u16 quantum = 0, prevQuantum = 1;
    i16 accelPress = 0, accelRelease = 0, speedPressStart = 0;
  };
  struct Scroll {
    u16 pos = 0;
    i16 rawPosF4 = 0, speed = 0;
    u16 windowHeight = 0;
    std::optional<u16> targetSpecial;
    i16 ballTarget = 0;
    bool attractUp = true;
    void setResolution(Resolution r, std::optional<i16> ballY);
    void update(i16 ballY);
    void attractFrame();
    void setSpecialTargetNow(u16 target);
  };
  struct LightState {
    bool lit = false, state = false;
    bool blinking = false;
    u8 ctr = 0, ctrOff = 0, ctrReset = 0;
  };
  struct DotMatrix {
    using Pixels = std::array<std::array<bool, 160>, 16>;
    Pixels pixels{}, saved{};
    bool state = true;
    std::optional<std::pair<u16, u16>> blink;  ///< timer, period
    void clear() { pixels = {}; }
    void stopBlink() { state = true; blink.reset(); }
    void startBlink(u16 period) { state = true; blink = {period, period}; }
    void blinkFrame();
  };
  struct Collision {
    std::array<i16, 2> flipperSpeed{};
    u16 angle = 0;
    std::size_t material = 0;
    u16 cnt = 0;
  };
  struct Material {
    i16 unk0, unk2, bounceFactor, minBounceSpeed, maxBounceAngle;
  };

  enum class KbdState : u8 { Main, ConfirmQuit, Paused, PausedConfirmQuit, GetName };

  enum class ScriptTaskKind : u8 {
    Placeholder, Default, Delay, Halt, ConfirmQuit, WaitJingle, WaitWhileGameStarting, AccBonus, Mode, DmClear,
    DmWipeDown, DmWipeRight, DmWipeDownStriped, DmMsgScroll, DmLongMsg, DmAnim, DmTowerHunt, Match, MatchStones,
    RecordHighScores, RecordHighScoresGetName, RecordHighScoresFinish,
  };
  /// The script's current wait, with the fields its kind needs.
  struct ScriptTask {
    ScriptTaskKind kind = ScriptTaskKind::Placeholder;
    u16 count = 0;       ///< Delay time; wipe/long-message position; match count; tower-hunt position
    u16 target = 0;      ///< scroll/tower-hunt target; frames reload; high-score place
    i16 pos = 0;         ///< scroll/long-message coordinate; match frames
    bool down = false;
    u16 msg = 0;
    u8 anim = 0;
    std::size_t frameIdx = 0;
    u16 delay = 0, repeats = 0;
    i8 frame = 0;        ///< AccBonus
    std::size_t digitIdx = 0;
    Bcd score;
    ScriptScore mode = ScriptScore::ModeHit;
    u8 digit = 0;
  };

  friend class TableTest;

  // --- table.rs
  void pause();
  void unpause();
  void toggleMusic();
  void pauseOptionAngle();
  void pauseOptionScrolling();
  void pauseOptionMusic();
  void pauseOptionResolution();
  void pauseConfirmQuit();

  // --- physics.rs
  void physicsFrame();
  std::optional<Collision> physicsCheckCollision();
  void physicsNewDir(const Collision& c);
  void ballMove();
  void springRelease();
  void flippersMove();
  void flippersPhysmapUpdate();
  void physmapPatch(Layer layer, int x, int y, const Grid8& src);
  void dropPhysmap(PhysmapBind bind);
  void raisePhysmap(PhysmapBind bind);
  void ballGravity();
  std::array<i16, 2> ballCenter() const;
  void checkTransitions();
  void scoreBumper();
  void ballTeleportFreeze(Layer layer, std::array<i16, 2> pos);
  void ballTeleport(Layer layer, std::array<i16, 2> pos, std::array<i16, 2> speed);
  i16 angleBoost(i32 v) const;
  i16 speedFix(i16 v) const { return hifps_ ? v : static_cast<i16>(i32{v} * 5 / 6); }

  // --- script.rs
  bool runScriptTask(ScriptTask& t);
  void scriptFrame();
  void startScript(ScriptBind bind);
  void startScriptRaw(u16 pos);
  Bcd scriptScore(const ScriptScoreRef& s) const;
  void runUop(u16 pos);
  void checkTopScore();
  void resetIdle();

  // --- dm.rs
  u8 dmSubChar(u8 chr) const;
  void dmPutChar(DmFont font, DmCoord pos, u8 chr);
  void dmPutBcd(DmFont font, DmCoord pos, const Bcd& num, bool center);
  void dmPuts(DmFont font, DmCoord pos, const std::vector<u8>& msg);
  void dmPuts(DmFont font, DmCoord pos, std::string_view msg);
  void dmAnimFrame(u8 frame);

  // --- tasks.rs
  void addTask(TaskKind kind, u16 a = 0, u16 b = 0, bool flag = false);
  void tasksFrame();
  bool runTask(Task& t);
  u16 taskDelay(const Task& t) const;
  bool runPartyTask(Task& t);
  bool runSpeedTask(Task& t);
  bool runShowTask(Task& t);
  bool runStonesTask(Task& t);

  // --- lights.rs
  void lightsAttractFrame();
  void lightsReset();
  void lightsTilt();
  void lightsBlinkFrame();
  void setLightState(u8 light, bool state);
  void lightBlink(LightBind bind, u8 idx, u8 halfPeriod, u8 phase);
  void lightSet(LightBind bind, u8 idx, bool state);
  void lightSetAll(LightBind bind, bool state);
  bool lightState(LightBind bind, u8 idx) const;
  bool lightAllLit(LightBind bind) const;
  bool lightAllUnlit(LightBind bind) const;
  void lightRotate(LightBind bind);
  u8 lightSequence(LightBind bind);
  template <std::size_t N>
  std::array<bool, N> lightSave(LightBind bind) const;
  template <std::size_t N>
  void lightLoad(LightBind bind, const std::array<bool, N>& data);

  // --- game.rs
  void initGame();
  void resetPlayerState();
  void initBall();
  void issueBall();
  void issueBallFinish();
  void issueBallRelease();
  void abortGame();
  void score(const Bcd& main, const Bcd& bonus);
  void scorePremult(const Bcd& main, const Bcd& bonus);
  void effectForceRaw(const Effect& e);
  bool effectRaw(const Effect& e);
  void effectForce(EffectBind bind);
  bool effect(EffectBind bind);
  void enter();
  void incrJackpot();
  void extraBall();
  void addCyclone(u8 cnt);
  void matchDone(u8 digit);
  bool runAccBonus(ScriptTask& t);
  bool runMatch(ScriptTask& t);
  bool runMatchStones(ScriptTask& t);

  // --- player.rs
  void loadCurPlayer();
  void saveCurPlayer();

  // --- mode.rs
  void modeCountHit();
  void modeCountRamp();
  bool modeFrame(ScriptScore score);

  // --- sound.rs
  void playSfxBind(SfxBind bind, u8 volume = 0x40);
  bool playJingleBind(JingleBind bind);
  bool playJingleBindForce(JingleBind bind);
  bool playJingleBindSilence(JingleBind bind);
  void setMusicSilence();
  void setMusicPlunger();
  void setMusicMain();
  void playJinglePlunger();

  // --- cheat.rs
  void handleCheat(u8 chr);

  // --- triggers.rs
  void doHitTriggers();
  void doRollTriggers();
  void doRollTrigger(RollTriggerId t);

  // --- per table
  void partyFrame();
  void partyFlipperPressed();
  void partyModeCheck();
  void partyDrained();
  void partyStartDropZoneScroll();
  void partyStartDropZone();
  void partyParty(u8 which);
  void partyCheckPartyAll();
  void partyHappyHour();
  bool partyCrazyLetter(EffectBind effect);
  void partyMegaLaugh();
  void partyArcadeButton();
  void partyHitDuck(u8 which);
  void partyOrbitRight();
  void partyOrbitLeft();
  void partySecret();
  void partySecretTilt();
  void partyTunnel();
  void partyTunnelTilt();
  void partyArcade();
  void partyArcadePickReward();
  void partyRampSnack();
  void partyDemon();
  void partyLaneOuter();
  void partySkyrideTop();
  void partyPuke(u8 which);
  void partyRampCyclone();

  void speedFrame();
  void speedFlipperPressed();
  void speedDrained();
  void speedModeCheck();
  void speedHitBur(u8 which);
  void speedHitNin(u8 which);
  bool speedGear(u8 which);
  void speedGoal();
  void speedDoTurbo();
  void speedOffroad();
  void speedDoOffroad();
  void speedPitStop();
  void speedCarMod(u8 which);
  void speedRampOffroad();
  void speedRampJump();
  void speedPitLoop();
  void speedRollPit(u8 which);
  void speedOvertake();
  void speedBumpMiles();
  void speedLoadFixup();

  void showFrame();
  void showFlipperPressed();
  void showDrained();
  void showModeCheck();
  void showHitCenter(u8 which);
  void showHitLeft(u8 which);
  void showHitDollar(u8 which);
  void showVault();
  void showWheelTick();
  void showGivePrize();
  Bcd showWheelScore() const;
  void showCashpot();
  void showCashpotEject();
  void showRampRight();
  void showLitPrize(u8 which);
  void showRampLoop();
  void showOrbitLeft();
  void showOrbitRight();
  void showRampSkills();
  void showRampTop();

  void stonesFrame();
  void stonesFlipperPressed();
  void stonesDrained();
  void stonesModeCheck();
  void stonesStonesBonesAll();
  void stonesHitStone(u8 which);
  void stonesHitBone(u8 which);
  void stonesRollKeyEntry();
  void stonesRollKey(u8 which);
  void stonesTower();
  void stonesTowerTilt();
  void stonesEndMode();
  void stonesTowerCheckClose();
  void stonesTowerOpen();
  void stonesTowerEject();
  void stonesWell();
  void stonesWellTilt();
  void stonesVault();
  void stonesRampTop();
  void stonesRollRip(u8 which);
  void stonesRampScreams();
  void stonesRampLeftToLane();
  void stonesRampLeftToVault();
  void stonesIncrVault();
  void stonesIncrWell();
  void stonesIncrTowerBonus();
  void stonesLoadFixup();

  int rand(int n) { return std::uniform_int_distribution<int>(0, n - 1)(rng_); }
  const Jingle& jingle(JingleBind b) const { return assets_.jingle(b); }

  // ---- state (same names as pfr, camelCased) ----
  TableAssets assets_;
  std::shared_ptr<TableSequencer> sequencer_;
  std::unique_ptr<Player> player_;
  Options options_;
  HighScores highScores_;
  bool hifps_ = false;
  std::mt19937_64 rng_;
  Scroll scroll_;
  std::vector<LightState> lights_;
  std::vector<u16> attractCtr_;
  Push push_;
  u8 springPos_ = 0;
  DotMatrix dm_;
  // script
  u16 scriptPos_ = 0;
  ScriptTask scriptTask_;
  u16 timerIdle_ = 718;
  bool needDefaultBg_ = false, inIdle_ = false, enterAttract_ = true;
  u16 repeatCnt_ = 0;
  std::vector<Task> tasks_;
  Ball ball_;
  // cheats
  bool cheatNoTilt_ = false, cheatSlowdown_ = false;
  std::string cheatBuf_;
  std::vector<FlipperState> flippers_;
  std::array<Grid8, 2> physmaps_;
  std::array<Material, 8> materials_{};
  i16 kickerSpeedThreshold_ = 0, kickerSpeedBoost_ = 0, bumperSpeedBoost_ = 0;
  std::array<u16, 36> matchTiming_{};

  bool inAttract_ = true, inGameStart_ = true, inPlunger_ = true, atSpring_ = false, inDrain_ = false, drained_ = false;
  bool gotTopScore_ = false, partyOn_ = false, specialPlungerEvent_ = false;
  std::optional<u8> matchDigit_;
  bool ballScoredPoints_ = false, tilted_ = false;
  u16 tiltCounter_ = 0;
  bool silenceEffect_ = false, timerStop_ = false, blockDrain_ = false, gotHighScore_ = false,
       flushHighScores_ = false;
  std::vector<u8> nameBuf_;

  bool inMode_ = false, inModeHit_ = false, inModeRamp_ = false;
  bool pendingMode_ = false, pendingModeHit_ = false, pendingModeRamp_ = false;
  u8 modeTimeoutFrames_ = 0, modeTimeoutSecs_ = 0;

  KbdState kbdState_ = KbdState::Main;
  u16 pauseCycle_ = 0;
  /// Debugging, while paused: every lamp forced on or off, and scrolling by hand.
  enum class LampOverride : u8 { None, AllOn, AllOff };
  LampOverride lampOverride_ = LampOverride::None;
  int scrollKey_ = 0;  ///< -1 up, 1 down, while the arrow is held
  void buildLampAreas() const;
  struct FlipperArt {
    Grid8 background;          ///< the artwork with the flipper taken out
    std::vector<u8> covered;   ///< per pixel: the flipper reaches it at some angle
    std::vector<u8> rest;      ///< per pixel: the flipper covers it at rest
    std::vector<float> angle;  ///< radians, per step
    float axisX = 0, axisY = 0;  ///< what it turns about, in the rect's pixels
  };
  void buildFlipperArt() const;
  float flipperAngle(std::size_t f) const;
  mutable std::vector<FlipperArt> flipperArt_;
  mutable std::vector<u8> lampAreas_;  ///< per playfield pixel: the lamp it belongs to, plus 1
  bool optionChanged_ = false;
  std::array<bool, 2> flipperState_{};
  bool flipperPressed_ = false, flippersEnabled_ = false, spaceState_ = false, spacePressed_ = false;
  bool springDownState_ = false, springReleased_ = false;
  bool startKeysActive_ = true;
  std::optional<u8> startKey_;
  bool quitting_ = false;
  u16 fade_ = 0x100;

  u8 curPlayer_ = 1, totalPlayers_ = 1, curBall_ = 1, totalBalls_ = 3, extraBalls_ = 0;
  u8 bonusMultEarly_ = 1, bonusMultLate_ = 1;
  std::vector<PlayerState> players_;

  Bcd scoreMain_, scoreBonus_, scoreJackpot_, scoreModeHit_, scoreModeRamp_, scoreRaisingMillions_;
  u16 numCyclone_ = 0, numCycloneTarget_ = 0;
  Bcd bcdNumCyclone_, scoreCycloneBonus_;
  bool holdBonus_ = false;

  std::optional<std::array<i16, 2>> hitPos_;
  std::optional<std::size_t> hitBumper_;
  std::optional<RollTriggerId> rollTrigger_, prevRollTrigger_;

  PartyState party_;
  SpeedState speed_;
  ShowState show_;
  StonesState stones_;
};

}  // namespace pfr
