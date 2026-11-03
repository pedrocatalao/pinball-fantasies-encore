#pragma once
// ProTracker "M.K." module parser (4 channels, 31 samples, 64-row patterns).
#include <array>
#include <filesystem>
#include <optional>

#include "core/Types.h"

namespace encore {

/// Where things are in a module of 31 samples (pfr: sound/loader.rs). One of 15, the oldest
/// kind, has no mark, and its song comes 16 samples' records sooner.
namespace modRec {
constexpr std::size_t title = 0, titleSize = 20;
constexpr std::size_t samples = 20;        ///< the samples' records, one after another
constexpr std::size_t song = 950;          ///< songRec, after the 31st sample's record
constexpr std::size_t mark = 1080;         ///< four letters: "M.K." and the like
constexpr std::size_t patterns = 1084;
constexpr std::size_t patternSize = 1024;  ///< 64 rows of 4 notes, 4 bytes each
constexpr std::size_t markSize = 4;
constexpr int sampleCount = 31;     ///< samples' records; in the oldest kind,
constexpr int oldSampleCount = 15;
constexpr int places = 128;         ///< the song's places, each the number of a pattern
constexpr int rows = 64;            ///< a pattern's rows,
constexpr int channels = 4;         ///< each a note for each channel
constexpr std::size_t noteSize = 4;
}  // namespace modRec

/// A note in a pattern (pfr: sound/loader.rs): the sample's number is split between the high
/// halves of its first and third bytes; the period is twelve bits, the effect four.
namespace noteRec {
constexpr std::size_t period = 0;  ///< its top four bits in the low half, with the sample's high half above
constexpr std::size_t periodLow = 1;
constexpr std::size_t effect = 2;  ///< in the low half, the sample's low half above
constexpr std::size_t param = 3;
constexpr u8 highHalf = 0xf0, lowHalf = 0x0f;
}  // namespace noteRec

/// A sample's record (pfr's Sample). Its words are big-endian, and count words.
namespace sampleRec {
constexpr std::size_t size = 30;
constexpr std::size_t nameSize = 22;  ///< the name comes first
constexpr std::size_t length = 22;
constexpr std::size_t finetune = 24, volume = 25;
constexpr u8 finetuneBits = 0x0f;  ///< four bits of finetune, signed, in its byte
constexpr int maxVolume = 64;
constexpr std::size_t loopStart = 26, loopLength = 28;  ///< pfr: repeat
}  // namespace sampleRec

/// The song (pfr: song_len, pos_restart, positions): how many places it has, where it starts
/// again, and the pattern at each of 128 places.
namespace songRec {
constexpr std::size_t length = 0, restart = 1, order = 2;
constexpr std::size_t size = 130;
}  // namespace songRec

/// The effects a note can carry, by number (pfr: ToneEffect, VolumeEffect, MiscEffect).
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
  u8 volume = sampleRec::maxVolume;
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
  std::array<ModSample, modRec::sampleCount> samples;
  u8 songLength = 0;
  u8 restart = 0;
  std::array<u8, modRec::places> order{};
  int patternCount = 0;
  std::vector<ModNote> notes;  ///< patternCount * 64 rows * 4 channels

  const ModNote& note(int pattern, int row, int channel) const {
    return notes[(static_cast<std::size_t>(pattern) * modRec::rows + row) * modRec::channels + channel];
  }

  static ModFile load(const std::filesystem::path& path);
  static ModFile parse(ByteView bytes, std::string name);
};

}  // namespace encore
