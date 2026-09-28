#pragma once
// Everything a table needs, read from its TABLEn.PRG: artwork, collision maps, triggers,
// lights, sounds, dot-matrix fonts and the table's script. A translation of pfr's
// src/assets/table*.rs; addresses are those of the versions pfr supports (checked by
// DataLocator through their SHA-256).
#include <array>
#include <initializer_list>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "assets/Bcd.h"
#include "assets/Exe.h"
#include "assets/Grid.h"

namespace pfr {

enum class DmFont : u8 { H5, H8, H11, H13 };
inline int dmFontHeight(DmFont f) {
  switch (f) {
    case DmFont::H5: return 5;
    case DmFont::H8: return 8;
    case DmFont::H11: return 11;
    case DmFont::H13: return 13;
  }
  return 13;
}

#include "assets/GameTables.inc"

enum class Layer : u8 { Ground, Overhead };
enum class FlipperSide : u8 { Left, Right };

/// Inclusive rectangle, as the game stores them.
struct TRect {
  i16 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
  bool contains(i16 x, i16 y) const { return x >= x0 && x <= x1 && y >= y0 && y <= y1; }
  bool operator==(const TRect&) const = default;
};

struct Jingle {
  u8 position = 0, repeat = 0, priority = 0;
  bool operator==(const Jingle&) const = default;
};
struct Sfx {
  u8 sample = 0, period = 0, channel = 0;
  bool operator==(const Sfx&) const = default;
};

struct Bumper {
  bool isKicker = false;
  TRect rect;
  Sfx sfx;
  Bcd score;
  bool operator==(const Bumper&) const = default;
};

struct HitTriggerArea {
  TRect rect;
  HitTrigger kind;
  u8 arg;
  bool operator==(const HitTriggerArea&) const = default;
};
struct RollTriggerArea {
  TRect rect;
  RollTrigger kind;
  u8 arg;
  bool operator==(const RollTriggerArea&) const = default;
};

struct PhysmapPatch {
  Layer layer = Layer::Ground;
  int x = 0, y = 0;
  Grid8 raised, dropped;
  bool operator==(const PhysmapPatch&) const = default;
};

struct Ramp {
  std::array<i16, 2> accel{}, accelHires{};
  bool operator==(const Ramp&) const = default;
};

struct BallOutlinePixel {
  i16 x = 0, y = 0;
  u16 angle = 0;
  u8 quad = 0, idx = 0;
  bool isBot = false, isRight = false;
  bool operator==(const BallOutlinePixel&) const = default;
};

struct Light {
  u8 baseIndex = 0;
  std::vector<Rgb> colors;
  bool operator==(const Light&) const = default;
};
struct AttractLight {
  u16 ctrReset = 0, ctrOff = 0, ctrOn = 0;
  u8 light = 0;
  bool operator==(const AttractLight&) const = default;
};
struct DmPalette {
  u8 indexOff = 0, indexOn = 0;
  Rgb colorOff, colorOn;
  bool operator==(const DmPalette&) const = default;
};

struct Flipper {
  FlipperSide side = FlipperSide::Left;
  int rectX = 0, rectY = 0;
  std::vector<Grid8> physmap;  ///< ground-layer patch per angle step
  std::vector<Grid8> gfx;      ///< artwork per angle step
  TRect ballBbox;
  i16 originX = 0, originY = 0;
  bool isVertical = false;
  u16 quantumMax = 0;
  i16 posMax = 0, accelPress = 0, accelRelease = 0, speedPressStart = 0;
  bool operator==(const Flipper&) const = default;
};

struct DmCoord {
  i16 x = 0, y = 0;
  bool operator==(const DmCoord&) const = default;
};

struct ScriptScoreRef {
  ScriptScore kind = ScriptScore::Bonus;
  u8 arg = 0;   ///< HighScore index
  Bcd value;    ///< Const value
  bool operator==(const ScriptScoreRef&) const = default;
};

/// Special characters in dot-matrix messages, substituted at print time.
namespace special_chars {
inline constexpr u8 kHighScores = 0x80;
inline constexpr u8 kBonusMultL = 0x90;
inline constexpr u8 kBonusMultR = 0x92;
inline constexpr u8 kCurPlayer = 0x94;
inline constexpr u8 kCurBall = 0x95;
inline constexpr u8 kTotalPlayers = 0x96;
inline constexpr u8 kNumCyclones = 0x98;
inline constexpr u8 kNumCyclonesTarget = 0x9c;
inline constexpr u8 kNumCyclonesTargetL = 0xa0;
}  // namespace special_chars

/// One script instruction. Which fields are meaningful depends on `kind`.
struct Uop {
  UopKind kind = UopKind::End;
  u16 value = 0;        ///< delay, repeat count, blink rate, tower-hunt argument, timeout, music position
  u16 target = 0;       ///< jump target (index into TableAssets::scripts)
  ScriptScoreRef score;
  DmFont font = DmFont::H13;
  bool flag = false;    ///< DmPrintScore: centred; DmState: on
  DmCoord coord;
  u16 msg = 0;          ///< index into TableAssets::msgs
  i16 end = 0;          ///< scroll end
  u8 anim = 0;          ///< index into TableAssets::anims
  Sfx sfx;
  u8 volume = 0;
  Jingle jingle;
  u8 time = 0;          ///< mode timer, seconds
  bool operator==(const Uop&) const = default;
};

struct DmAnim {
  u16 repeats = 0;
  std::size_t restart = 0, numFrames = 0;
  std::vector<std::pair<u8, u16>> frames;  ///< frame index, duration
  bool operator==(const DmAnim&) const = default;
};
using DmAnimFrame = std::vector<std::pair<DmCoord, bool>>;

struct Cheat {
  std::string keys;
  u16 script = 0;
  CheatEffect effect = CheatEffect::None;
  bool operator==(const Cheat&) const = default;
};

struct Effect {
  std::optional<Jingle> jingle;  ///< none: silent, with `silentPriority`
  u8 silentPriority = 0;
  Bcd scoreMain, scoreBonus;
  std::optional<u16> script;
  bool operator==(const Effect&) const = default;
};

struct TableAssets {
  static TableAssets load(ByteView prg, int table);

  int table = 0;  ///< 0..3

  Grid8 mainBoard;  ///< 320x576 palette indices
  std::vector<Rgb> palette;
  Grid8 spring;     ///< 10x23
  Grid8 ball;       ///< 15x15, 0 = transparent
  std::array<Grid8, 2> occmaps;   ///< per layer, 1 = ball hidden
  std::array<Grid8, 2> physmaps;  ///< per layer: bits 0-2 material planes, bits 4-7 ramp
  std::array<std::optional<PhysmapPatch>, static_cast<std::size_t>(PhysmapBind::Count)> physmapPatches;
  std::vector<Ramp> ramps;
  std::vector<BallOutlinePixel> ballOutline;
  std::vector<std::array<i16, 2>> ballOutlineByAngle;

  std::vector<Light> lights;
  std::vector<AttractLight> attractLights;
  DmPalette dmPalette;
  std::array<std::map<u8, std::vector<u8>>, 4> dmFonts;
  std::vector<Flipper> flippers;
  std::array<std::vector<u8>, static_cast<std::size_t>(LightBind::Count)> lightBinds;
  std::optional<Grid<u8>> dmTower;  ///< Stones 'n Bones: 160x167 bitmap

  std::vector<TRect> transitionsDown, transitionsUp;
  std::vector<Bumper> bumpers;
  std::array<std::vector<RollTriggerArea>, 2> rollTriggers, rollTriggersTilt;
  std::vector<HitTriggerArea> hitTriggers;

  std::array<std::optional<Jingle>, static_cast<std::size_t>(JingleBind::Count)> jingleBinds;
  std::array<std::optional<Sfx>, static_cast<std::size_t>(SfxBind::Count)> sfxBinds;
  u8 positionJingleStart = 0;

  std::vector<Uop> scripts;
  std::vector<std::vector<u8>> msgs;
  std::vector<DmAnim> anims;
  std::vector<DmAnimFrame> animFrames;
  std::array<std::optional<u16>, static_cast<std::size_t>(ScriptBind::Count)> scriptBinds;
  std::vector<Cheat> cheats;
  std::array<std::optional<Effect>, static_cast<std::size_t>(EffectBind::Count)> effects;

  std::array<i16, 0xa00> sineTable{};

  Bcd scoreJackpotInit, scoreJackpotIncr, scoreModeHitIncr, scoreModeRampIncr;
  std::array<i16, 2> issueBallPos{}, issueBallReleasePos{};

  bool operator==(const TableAssets&) const = default;

  const Jingle& jingle(JingleBind b) const;
  const std::optional<Sfx>& sfx(SfxBind b) const { return sfxBinds[static_cast<std::size_t>(b)]; }
  const std::vector<u8>& lightsOf(LightBind b) const { return lightBinds[static_cast<std::size_t>(b)]; }
  const std::optional<Effect>& effect(EffectBind b) const { return effects[static_cast<std::size_t>(b)]; }
};

}  // namespace pfr
