#include "engine/data/TableSound.h"

#include <algorithm>

#include "engine/table/Engine.h"

namespace encore {
namespace {

/// Where each record is in a table's data segment, after pfr's binds (assets/table/sound.rs).
/// Its SfxBind FlipperPress, BallDrained, SpringUp and RaiseHitTargets are flipper, drained,
/// spring and raiseTargets here, and its JingleBind Drained and GameStart drainedJingle and
/// gameStartJingle.
struct SoundAddresses {
  u16 flipper, drained, issueBall, spring, rollInner, tickBonus, gameStart, raiseTargets;
  u16 silence, plunger, main, attract, warnTilt, tilt, gameOverSad, gameOverHighScore;
  u16 drainedJingle, gameStartJingle, matchStart, matchWin;
};
constexpr int kTableCount = 4;
constexpr SoundAddresses kTables[kTableCount] = {
    // TABLE1.PRG, Party Land
    {.flipper = 0x0c2d, .drained = 0x0c31, .issueBall = 0x0c35, .spring = 0x0c3d, .rollInner = 0x0c49,
     .tickBonus = 0x0c61, .gameStart = 0x0c65, .raiseTargets = 0x0c1d,
     .silence = 0x0c6c, .plunger = 0x0c6f, .main = 0x0c72, .attract = 0x0c75, .warnTilt = 0x0c78, .tilt = 0x0c7b,
     .gameOverSad = 0x0c7e, .gameOverHighScore = 0x0c81, .drainedJingle = 0x0c84, .gameStartJingle = 0x0c87,
     .matchStart = 0x0c90, .matchWin = 0x0c93},
    // TABLE2.PRG, Speed Devils (pfr's MatchStart and MatchWin are 0x0a62 and 0x0a65)
    {.flipper = 0x09fa, .drained = 0x09fe, .issueBall = 0x0a02, .spring = 0x0a0a, .rollInner = 0x0a1a,
     .tickBonus = 0x0a36, .gameStart = 0x0a12, .raiseTargets = 0x09f2,
     .silence = 0x0a3e, .plunger = 0x0a41, .main = 0x0a44, .attract = 0x0a47, .warnTilt = 0x0a4a, .tilt = 0x0a4d,
     .gameOverSad = 0x0a50, .gameOverHighScore = 0x0a53, .drainedJingle = 0x0a56, .gameStartJingle = 0x0a41,
     .matchStart = 0x0a59, .matchWin = 0x0a5c},
    // TABLE3.PRG, Billion Dollar Gameshow (pfr's Silence is 0x08ad)
    {.flipper = 0x086d, .drained = 0x0871, .issueBall = 0x0875, .spring = 0x087d, .rollInner = 0x0889,
     .tickBonus = 0x08a5, .gameStart = 0x08b9, .raiseTargets = 0x0865,
     .silence = 0x08cf, .plunger = 0x08de, .main = 0x08d2, .attract = 0x08e1, .warnTilt = 0x08b0, .tilt = 0x08b3,
     .gameOverSad = 0x08d5, .gameOverHighScore = 0x08d8, .drainedJingle = 0x08b6, .gameStartJingle = 0x08de,
     .matchStart = 0x08c3, .matchWin = 0x08c6},
    // TABLE4.PRG, Stones 'n Bones (pfr's MatchStart is 0x0910, and it binds no RaiseHitTargets)
    {.flipper = 0x08b8, .drained = 0x08bc, .issueBall = 0x08c0, .spring = 0x08c8, .rollInner = 0x08d4,
     .tickBonus = 0x08e4, .gameStart = 0x08e8, .raiseTargets = 0x08b4,
     .silence = 0x08ec, .plunger = 0x08f2, .main = 0x08f5, .attract = 0x08f8, .warnTilt = 0x08fb, .tilt = 0x08fe,
     .gameOverSad = 0x0901, .gameOverHighScore = 0x0904, .drainedJingle = 0x0907, .gameStartJingle = 0x08ef,
     .matchStart = 0x090a, .matchWin = 0x090d},
};

/// A sound effect's record has a byte between the note and the channel that no routine reads;
/// it is nought in every record (pfr's reading asserts it).
constexpr u16 soundRecSpare = 2;
/// The records' sizes: a tune's three bytes, a sound effect's four.
constexpr std::size_t kTuneRecSize = 3, kSoundRecSize = 4;
/// What an effect can be: a sample of the module's 31, a note of the driver's 36, on one of
/// the four channels (counted from nought here).
constexpr u8 kSamples = 31, kNotes = 36, kLastChannel = 3;

Jingle readJingle(ByteView d, std::size_t at) {
  if (at + kTuneRecSize > d.size()) return {};
  return {d[at + tuneRec::place], d[at + tuneRec::repeats], d[at + tuneRec::priority]};
}

}  // namespace

namespace {
bool plausible(const Sfx& s) {
  return s.sample >= 1 && s.sample <= kSamples && s.note >= 1 && s.note <= kNotes && s.channel <= kLastChannel;
}
}  // namespace

Sfx readSfx(ByteView d, std::size_t at) {
  if (at + kSoundRecSize > d.size()) return {};
  // The third byte is always zero in a well-formed record.
  if (d[at + soundRecSpare] != 0) return {};
  return {d[at + soundRec::sample], d[at + soundRec::note], d[at + soundRec::channel]};
}

/// The recorded addresses are not always exact for every copy of the game, so accept a
/// record found just either side of the expected one.
Sfx readSfxNear(ByteView d, std::size_t at) {
  if (const Sfx exact = readSfx(d, at); plausible(exact)) return exact;
  for (int delta : {-1, 1, -2, 2}) {
    if (delta < 0 && at < static_cast<std::size_t>(-delta)) continue;
    const Sfx s = readSfx(d, at + static_cast<std::size_t>(delta));
    if (plausible(s)) return s;
  }
  return {};
}

TableSounds extractSounds(ByteView d, int tableIndex) {
  const SoundAddresses& a = kTables[std::clamp(tableIndex, 0, kTableCount - 1)];
  TableSounds s;
  s.flipper = readSfxNear(d, a.flipper);
  s.drained = readSfxNear(d, a.drained);
  s.issueBall = readSfxNear(d, a.issueBall);
  s.spring = readSfxNear(d, a.spring);
  s.rollInner = readSfxNear(d, a.rollInner);
  s.tickBonus = readSfxNear(d, a.tickBonus);
  s.gameStart = readSfxNear(d, a.gameStart);
  s.raiseTargets = readSfxNear(d, a.raiseTargets);
  s.silence = readJingle(d, a.silence);
  s.plunger = readJingle(d, a.plunger);
  s.main = readJingle(d, a.main);
  s.attract = readJingle(d, a.attract);
  s.warnTilt = readJingle(d, a.warnTilt);
  s.tilt = readJingle(d, a.tilt);
  s.gameOverSad = readJingle(d, a.gameOverSad);
  s.gameOverHighScore = readJingle(d, a.gameOverHighScore);
  s.drainedJingle = readJingle(d, a.drainedJingle);
  s.gameStartJingle = readJingle(d, a.gameStartJingle);
  s.matchStart = readJingle(d, a.matchStart);
  s.matchWin = readJingle(d, a.matchWin);
  return s;
}

}  // namespace encore
