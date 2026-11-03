#pragma once
// ProTracker "M.K." module parser (4 channels, 31 samples, 64-row patterns).
#include <array>
#include <filesystem>
#include <optional>

#include "core/Types.h"

namespace encore {

/// Where things are in a module of 31 samples. One of 15, the oldest
/// kind, has no mark, and its song comes 16 samples' records sooner.
namespace modRec {
constexpr std::size_t title = 0;
constexpr std::size_t samples = 20;        ///< the samples' records, one after another
constexpr std::size_t song = 950;          ///< songRec, after the 31st sample's record
constexpr std::size_t mark = 1080;         ///< four letters: "M.K." and the like
constexpr std::size_t patterns = 1084;
}  // namespace modRec

/// A sample's record. Its words are big-endian, and count words.
namespace sampleRec {
constexpr std::size_t size = 30;
constexpr std::size_t length = 22;
constexpr std::size_t finetune = 24, volume = 25;
constexpr std::size_t loopStart = 26, loopLength = 28;
}  // namespace sampleRec

/// The song: how many places it has, where it starts
/// again, and the pattern at each of 128 places.
namespace songRec {
constexpr std::size_t length = 0, restart = 1, order = 2;
constexpr std::size_t size = 130;
}  // namespace songRec

/// The effects a note can carry, by number.
namespace modEffect {
constexpr u8 arpeggio = 0x0;
constexpr u8 portamentoUp = 0x1, portamentoDown = 0x2;
constexpr u8 tonePortamento = 0x3;
constexpr u8 vibrato = 0x4;
constexpr u8 tonePortamentoVolumeSlide = 0x5, vibratoVolumeSlide = 0x6;
constexpr u8 setSampleOffset = 0x9;
constexpr u8 volumeSlide = 0xa;
constexpr u8 positionJump = 0xb;
constexpr u8 setVolume = 0xc;
constexpr u8 patternBreak = 0xd;
constexpr u8 extended = 0xe;    ///< which one, in the top four bits of its number
constexpr u8 retrigNote = 0x9;  ///< extended's, in those four bits
constexpr u8 setSpeed = 0xf;
}  // namespace modEffect

struct ModSample {
  std::string name;
  u32 length = 0;      ///< bytes
  i8 finetune = 0;     ///< -8..7
  u8 volume = 64;
  u32 loopStart = 0;   ///< bytes
  u32 loopLength = 0;  ///< bytes; <= 2 means no loop
  std::vector<i8> data;
  bool loops() const { return loopLength > 2; }
};

struct ModNote {
  u8 sample = 0;   ///< 1..31, 0 = none
  u16 period = 0;  ///< Amiga period, 0 = none
  u8 effect = 0;   ///< 0..15 (modEffect)
  u8 param = 0;
};

struct ModFile {
  std::string title;
  std::array<ModSample, 31> samples;
  u8 songLength = 0;
  u8 restart = 0;
  std::array<u8, 128> order{};
  int patternCount = 0;
  std::vector<ModNote> notes;  ///< patternCount * 64 rows * 4 channels

  const ModNote& note(int pattern, int row, int channel) const {
    return notes[(static_cast<std::size_t>(pattern) * 64 + row) * 4 + channel];
  }

  static ModFile load(const std::filesystem::path& path);
  static ModFile parse(ByteView bytes, std::string name);
};

}  // namespace encore
