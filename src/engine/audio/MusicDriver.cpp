#include "engine/audio/MusicDriver.h"

#include <utility>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <vector>

#include "engine/data/ModFile.h"

namespace encore {
namespace {

/// The notes the driver knows, and the finetunes, sixteen rows of the 36 periods: a finetune's
/// row of them, from the highest note's period (kHighestPeriod) to the lowest's.
constexpr int kNotes = 36, kFinetunes = 16;
constexpr int kHighestPeriod = 0x71, kLowestPeriod = 0x358;

constexpr u16 kPeriods[kFinetunes * kNotes] = {
#include "engine/audio/DriverPeriods.inc"
};

/// The vibrato's wave (ds:69e2): half a sine, thirty-two steps; the other half is the same
/// below nought, which the top bit of the place in the wave says.
constexpr int kSineSteps = 32, kSineNegative = 0x80;
constexpr u8 kSine[kSineSteps] = {0x00, 0x18, 0x31, 0x4a, 0x61, 0x78, 0x8d, 0xa1, 0xb4, 0xc5, 0xd4, 0xe0, 0xeb, 0xf4, 0xfa, 0xfd,
                          0xff, 0xfd, 0xfa, 0xf4, 0xeb, 0xe0, 0xd4, 0xc5, 0xb4, 0xa1, 0x8d, 0x78, 0x61, 0x4a, 0x31, 0x18};

/// The Amiga's clock, as the driver has it (cs:1001): a period is so many of its beats.
constexpr u32 kClock = 0x361f0f;

/// A module of 31 samples, as the driver tells one (cs:077a): the sum of its mark's two
/// words, "M.K." or "FLT4".
constexpr u16 kMarkMK = 0x5c98, kMarkFLT4 = 0x809a;

/// The places in the song are seven bits.
constexpr int kPlaceBits = 0x7f;
/// A sample shorter than this is none (cs:0964 leaves it a silent byte).
constexpr int kShortestSample = 3;
/// A loop of no more than two bytes is none: the sample plays once, to its end.
constexpr int kNoLoop = 2;
/// The channels heard on the left: the first two (the others on the right).
constexpr int kLeftChannels = 2;
/// A volume as the module gives it, up to 64 (the channel keeps it in 256ths: kTopVolume).
constexpr int kMaxVolume = 0x40, kTopVolume = 0x4000;
/// An effect's number holds two numbers, in its byte's two halves.
constexpr int kHighHalf = 0xf0, kLowHalf = 0x0f;
/// A tick is a fiftieth of a second.
constexpr double kTickRate = 50.0;
/// The mixing: a channel's volume is out of 64, a sample byte out of 128, and four channels
/// share the whole; the master volume is out of 255.
constexpr float kVolumeSteps = 64.0f, kSampleTop = 128.0f, kChannelShare = 0.25f, kMasterTop = 255.0f;
/// The balanced mixing eases a note's loudness over a millisecond and a half.
constexpr double kEaseSeconds = 0.0015;

/// The balanced mixing reads a sample between its bytes from the eight around the place: a
/// sinc, narrowed a little below the sample's own top frequency and shaped by a Blackman-Harris
/// window, for each of 1024 places between two bytes. Each set of eight adds up to one, so
/// nothing is louder or softer than the bytes themselves.
constexpr int kTaps = 8, kPhaseBits = 10;
/// The balance between the two ways of reading a sample: so much of the nearest byte, as the
/// driver takes it (its bite, and its grit), the rest from between the bytes (smooth).
constexpr float kNearest = 0.5f;
/// How much of a channel is heard on its own side: 1 is the driver's, all of it.
constexpr float kOwnSide = 0.85f;
/// The balanced tone: so many decibels more below the bass's corner and above the treble's.
constexpr double kBassDb = 3.0, kBassHz = 150.0, kTrebleDb = 3.0, kTrebleHz = 6000.0;

/// A shelving filter of the RBJ cookbook, slope 1: low or high, so many dB from a corner.
void shelve(float& b0, float& b1, float& b2, float& a1, float& a2, bool low, double db, double hz, int rate) {
  constexpr double pi = 3.14159265358979323846;
  const double a = std::pow(10.0, db / 40), w = 2 * pi * hz / rate, cw = std::cos(w);
  const double alpha = std::sin(w) / 2 * std::sqrt(2.0), root = 2 * std::sqrt(a) * alpha;
  double n0, n1, n2, d0, d1, d2;
  if (low) {
    n0 = a * ((a + 1) - (a - 1) * cw + root), n1 = 2 * a * ((a - 1) - (a + 1) * cw), n2 = a * ((a + 1) - (a - 1) * cw - root);
    d0 = (a + 1) + (a - 1) * cw + root, d1 = -2 * ((a - 1) + (a + 1) * cw), d2 = (a + 1) + (a - 1) * cw - root;
  } else {
    n0 = a * ((a + 1) + (a - 1) * cw + root), n1 = -2 * a * ((a - 1) + (a + 1) * cw), n2 = a * ((a + 1) + (a - 1) * cw - root);
    d0 = (a + 1) - (a - 1) * cw + root, d1 = 2 * ((a - 1) - (a + 1) * cw), d2 = (a + 1) - (a - 1) * cw - root;
  }
  b0 = static_cast<float>(n0 / d0), b1 = static_cast<float>(n1 / d0), b2 = static_cast<float>(n2 / d0);
  a1 = static_cast<float>(d1 / d0), a2 = static_cast<float>(d2 / d0);
}
const std::vector<float>& sincTable() {
  static const std::vector<float> table = [] {
    constexpr double pi = 3.14159265358979323846, cutoff = 0.95;
    std::vector<float> t(std::size_t{1} << kPhaseBits << 3);
    for (int p = 0; p < (1 << kPhaseBits); ++p) {
      const double f = static_cast<double>(p) / (1 << kPhaseBits);
      double w[kTaps], sum = 0;
      for (int k = 0; k < kTaps; ++k) {
        const double x = (k - kTaps / 2 + 1) - f;  // from the place to this byte
        const double s = x == 0 ? 1.0 : std::sin(pi * x * cutoff) / (pi * x * cutoff);
        const double n = (x + kTaps / 2.0) / kTaps;  // 0 to 1 across the eight
        const double window = 0.35875 - 0.48829 * std::cos(2 * pi * n) + 0.14128 * std::cos(4 * pi * n) -
                              0.01168 * std::cos(6 * pi * n);
        w[k] = s * window;
        sum += w[k];
      }
      for (int k = 0; k < kTaps; ++k) t[static_cast<std::size_t>(p * kTaps + k)] = static_cast<float>(w[k] / sum);
    }
    return t;
  }();
  return table;
}

u16 big(ByteView b, std::size_t at) { return static_cast<u16>((b[at] << 8) | b[at + 1]); }

}  // namespace

MusicDriver::MusicDriver(int outputRate) : rate_(outputRate) {
  shelve(bass_.b0, bass_.b1, bass_.b2, bass_.a1, bass_.a2, true, kBassDb, kBassHz, rate_);
  shelve(treble_.b0, treble_.b1, treble_.b2, treble_.a1, treble_.a2, false, kTrebleDb, kTrebleHz, rate_);
}

float MusicDriver::Shelf::run(float x, int side) {
  const float y = b0 * x + b1 * x1[side] + b2 * x2[side] - a1 * y1[side] - a2 * y2[side];
  x2[side] = x1[side], x1[side] = x, y2[side] = y1[side], y1[side] = y;
  return y;
}

void MusicDriver::tone(float* out, std::size_t frames) {
  for (std::size_t i = 0; i < frames * 2; ++i) {
    const int side = static_cast<int>(i & 1);
    out[i] = treble_.run(bass_.run(out[i], side), side);
  }
}

/// cs:071f: the module read in. Its notes are kept three bytes each: the note as a number
/// (1 to 36, found by its period in the first row of the table; a period that is not there
/// loses the whole note), the sample, the effect and its number.
bool MusicDriver::load(ByteView mod) {
  std::lock_guard lock(mutex_);
  loaded_ = false;
  if (mod.size() < modRec::patterns) return false;
  // "M.K." and "FLT4" say thirty-one samples, lengths of loops in words (cs:077a)
  const u16 mark = static_cast<u16>((mod[modRec::mark] | (mod[modRec::mark + 1] << 8)) +
                                    (mod[modRec::mark + 2] | (mod[modRec::mark + 3] << 8)));
  const bool modern = mark == kMarkMK || mark == kMarkFLT4;
  const std::size_t count = modern ? modRec::sampleCount : modRec::oldSampleCount;
  const std::size_t header = modRec::samples + count * sampleRec::size;
  if (mod.size() < header + songRec::size + (modern ? modRec::markSize : 0)) return false;
  length_ = mod[header + songRec::length];
  restart_ = mod[header + songRec::restart] & kPlaceBits;
  if (restart_ >= length_) restart_ = 0;
  u8 patterns = 0;
  for (std::size_t i = 0; i < modRec::places; ++i) {
    order_[i] = mod[header + songRec::order + i];
    patterns = std::max(patterns, order_[i]);
  }
  ++patterns;
  const std::size_t notes = header + songRec::size + (modern ? modRec::markSize : 0);
  if (mod.size() < notes + std::size_t{patterns} * modRec::patternSize) return false;
  cells_.assign(std::size_t{patterns} * sdr::patternBytes, 0);
  for (std::size_t i = 0; i < std::size_t{patterns} * (modRec::rows * modRec::channels); ++i) {
    const u8* in = &mod[notes + i * modRec::noteSize];
    const u16 period = static_cast<u16>(((in[0] & noteRec::lowHalf) << 8) | in[noteRec::periodLow]);
    u8 note = 0;
    bool known = period == 0;
    for (u8 n = 0; n < kNotes && !known; ++n)
      if (kPeriods[n] == period) {
        note = static_cast<u8>(n + 1);
        known = true;
      }
    if (!known) continue;
    const u8 sample = static_cast<u8>(((in[0] & noteRec::highHalf) | (in[noteRec::effect] >> 4)) & sdr::sampleBits);
    u8* out = &cells_[i * sdr::noteBytes];
    out[0] = static_cast<u8>(note | (sample << sdr::sampleShift));
    out[1] = static_cast<u8>((sample >> 2) | ((in[noteRec::effect] & noteRec::lowHalf) << 3));  // (the word's high byte)
    out[2] = in[noteRec::param];
  }
  // the samples (cs:0964)
  samples_.fill(Sample{});
  sampleData_.assign(modRec::sampleCount, {});
  std::size_t at = notes + std::size_t{patterns} * modRec::patternSize;
  for (std::size_t s = 0; s < count; ++s) {
    const std::size_t h = modRec::samples + s * sampleRec::size;
    Sample& out = samples_[s];
    const u16 length = static_cast<u16>(big(mod, h + sampleRec::length) << 1);
    if (length < kShortestSample) continue;  // (nothing: it is left a silent byte)
    out.table = static_cast<u16>((mod[h + sampleRec::finetune] & sampleRec::finetuneBits) * kNotes);
    out.volume = mod[h + sampleRec::volume];
    u16 loopStart = big(mod, h + sampleRec::loopStart), loopLength = big(mod, h + sampleRec::loopLength);
    if (modern) {
      loopStart = static_cast<u16>(loopStart << 1);
      loopLength = static_cast<u16>(loopLength << 1);
    }
    if (loopLength == 0) loopLength = 1;
    out.length = length;
    out.loopStart = loopStart;
    out.loopEnd = static_cast<u16>(loopStart + loopLength);
    auto& data = sampleData_[s];
    data.assign(std::size_t{length} + 1, 0);
    const std::size_t have = at < mod.size() ? std::min<std::size_t>(length, mod.size() - at) : 0;
    if (have) std::memcpy(data.data(), &mod[at], have);
    data[length] = data[out.loopStart < length ? out.loopStart : 0];  // (cs:0a2d: the byte past the end is the loop's first)
    out.data = data.data();
    at += length;
  }
  position_ = 0;
  rowAt_ = std::size_t{order_[0]} * sdr::patternBytes;
  pendingRow_ = 0;
  speed_ = kDefaultSpeed;
  ticksLeft_ = 1;
  ch_.fill(Channel{});
  voices_.fill(Voice{});
  asked_.clear();
  loaded_ = true;
  return true;
}

u32 MusicDriver::stepFor(int period) const {
  if (period <= 0) return 0;
  const u32 perSecond = kClock / static_cast<u32>(period);  // whole sample bytes a second, as the driver's first division
  return static_cast<u32>((u64{perSecond} << 16) / static_cast<u32>(rate_));
}

int MusicDriver::periodAt(u16 index) const { return index < std::size(kPeriods) ? kPeriods[index] : kHighestPeriod; }

/// int66::start (cs:061a), as far as the music goes: it goes on from where it was, the next
/// row at the next tick.
u8 MusicDriver::start() {
  std::lock_guard lock(mutex_);
  playing_ = loaded_;
  if (std::exchange(held_, false)) return 0;
  ticksLeft_ = 1;
  return 0;
}

/// int66::stop (cs:023c, cs:1417): the card is stopped and every channel falls silent.
void MusicDriver::stop() {
  std::lock_guard lock(mutex_);
  playing_ = false;
  if (holdOnStop) {  // this version's pause: everything as it is, to go on from
    held_ = true;
    return;
  }
  for (Channel& c : ch_) {
    c.volume = 0;
    c.end = c.loopStart = c.loops = c.position = 0;
  }
  voices_.fill(Voice{});
}

/// int66::setMasterVolume (cs:02f8).
void MusicDriver::volume(u16 level) {
  std::lock_guard lock(mutex_);
  // cs:1584: the level goes to the sound card's own mixer as a byte, 0x100 becoming its 0xff:
  // so 0x100 is all of it, which is what the game sends when it is not fading
  master_ = static_cast<u8>(level - (level >> 8));
}

/// int66::jump (cs:0201): the place to go to once the row now due has been played. The
/// answer is the place the music was at.
u8 MusicDriver::jump(u16 position) {
  std::lock_guard lock(mutex_);
  const u16 was = position_;
  position_ = static_cast<u16>(position - 1);
  pendingRow_ = 1;
  placed_ = true;
  ticksLeft_ = 1;
  return static_cast<u8>(was);
}

/// int66::playSfx (cs:01db): a note on a channel, with a volume if one is given.
/// (Here it waits for its moment in the sound: render.)
void MusicDriver::effect(u8 sample, u8 note, u8 volume, u8 channel) {
  std::lock_guard lock(mutex_);
  if (!loaded_ || channel < 1 || channel > modRec::channels) return;
  asked_.push_back({sample, note, volume, channel, stamp_});
}

void MusicDriver::stampTime(double seconds) {
  std::lock_guard lock(mutex_);
  stamp_ = seconds;
}

/// The driver's own note on the channel; what it would cut off, if it was another effect still
/// sounding, plays on to its end on a voice of its own (the oldest such voice gives way). The
/// same effect again starts over, as in the original: a bonus counted, a bumper hit and hit
/// again, would otherwise pile up into a blur.
void MusicDriver::startEffect(const Asked& a) {
  Channel& c = ch_[a.channel - 1u];
  const bool sounding = c.data && c.position < c.end && c.loops <= kNoLoop;  // (one that loops would never end)
  const bool another = a.sample != 0 && a.sample <= modRec::sampleCount && c.data != samples_[a.sample - 1u].data;
  if (another && a.note != 0 && c.byEffect && sounding) {
    Voice* to = &voices_[0];
    for (Voice& v : voices_) {
      if (!v.c.data) {
        to = &v;
        break;
      }
      if (v.since < to->since) to = &v;
    }
    *to = {c, a.channel <= kLeftChannels, ++voicesStarted_};
  }
  trigger(c, a.sample, a.note, a.volume ? modEffect::setVolume : 0, a.volume);
  c.byEffect = true;
}

/// cs:0e3e: a note begins, or an effect on the one that plays.
void MusicDriver::trigger(Channel& c, u8 sample, u8 note, u8 effect, u8 param) {
  if (effect != modEffect::tonePortamento && effect != modEffect::tonePortamentoVolumeSlide) {
    if (sample != 0 && sample <= modRec::sampleCount) {  // cs:1026
      const Sample& s = samples_[sample - 1u];
      c.data = s.data;
      c.volume = static_cast<u16>(s.volume << 8);
      c.table = s.table;
      c.loopStart = s.loopStart;
      c.offset = 0;
      c.loops = s.loopEnd;
      c.end = s.loopEnd;
      if (static_cast<u16>(s.loopEnd - s.loopStart) <= kNoLoop) {
        c.loops = 1;
        c.end = s.length;
      }
    }
    if (note != 0) {  // cs:0fe4
      c.note = static_cast<u16>(c.table + note - 1);
      c.period = static_cast<i16>(periodAt(c.note));
      c.step = stepFor(c.period);
      c.position = c.offset;
      c.started = true;
      c.vibratoAt = 0;
    }
  }
  // cs:107b
  if (c.vibrated) c.step = stepFor(c.period);
  c.vibrated = false;
  c.tick = Channel::Tick::None;
  switch (effect & kLowHalf) {
    case modEffect::arpeggio:  // the note and two above it in turn
      if (param == 0) break;
      c.arpeggio1 = stepFor(periodAt(static_cast<u16>(c.note + (param >> 4))));
      c.arpeggio2 = stepFor(periodAt(static_cast<u16>(c.note + (param & kLowHalf))));
      c.tick = Channel::Tick::Arpeggio;
      break;
    case modEffect::portamentoUp:  // the pitch slides up
      c.target = kHighestPeriod;
      c.portamento = static_cast<i16>(-param);
      c.tick = Channel::Tick::Portamento;
      break;
    case modEffect::portamentoDown:  // or down
      c.target = kLowestPeriod;
      c.portamento = param;
      c.tick = Channel::Tick::Portamento;
      break;
    case modEffect::tonePortamento:  // or to the note named
      if (param != 0) c.portamento = param;
      if (note != 0) {
        c.note = static_cast<u16>(c.table + note - 1);
        c.target = static_cast<i16>(periodAt(c.note));
        if (sample != 0 && sample <= modRec::sampleCount) c.volume = static_cast<u16>(samples_[sample - 1u].volume << 8);
      }
      if (c.period >= c.target) c.portamento = static_cast<i16>(-c.portamento);
      c.tick = Channel::Tick::Portamento;
      break;
    case modEffect::vibrato:
      c.vibrated = true;
      if (param & kHighHalf) c.vibratoSpeed = static_cast<u8>((param & kHighHalf) >> 2);
      if (param & kLowHalf) c.vibratoDepth = param & kLowHalf;
      c.tick = Channel::Tick::Vibrato;
      break;
    case modEffect::vibratoVolumeSlide:
      c.tick = Channel::Tick::VibratoSlide;
      c.slide = static_cast<i16>(((param >> 4) ? (param >> 4) : -(param & kLowHalf)) * 256);
      break;
    case modEffect::setSampleOffset:  // from further into the sample
      c.offset = static_cast<u16>(param << 8);
      if (rowSample_ != 0) {
        c.position = c.offset;
        c.started = true;
      }
      break;
    case modEffect::volumeSlide:
      c.tick = Channel::Tick::Slide;
      c.slide = static_cast<i16>(((param >> 4) ? (param >> 4) : -(param & kLowHalf)) * 256);
      break;
    case modEffect::positionJump:  // the music jumps: the table is asked where to (cs:12c0)
      if (pendingRow_ != 0) break;
      position_ = static_cast<u16>((position_ & 0xff00) | (conductor ? conductor->jump(param) : param));
      --position_;
      pendingRow_ = 1;
      placed_ = true;
      break;
    case modEffect::setVolume:
      c.volume = static_cast<u16>(std::min<u8>(param, kMaxVolume) << 8);
      break;
    case modEffect::patternBreak:  // on to the next pattern, at a row of it
      pendingRow_ = static_cast<u8>(param + 1);
      break;
    case modEffect::extended:
      if ((param >> 4) != modEffect::retrigNote) break;
      c.retrigger = c.retriggerLeft = param & kLowHalf;
      c.tick = Channel::Tick::Retrigger;
      break;
    case modEffect::setSpeed:  // so many ticks to a row, whatever the number
      speed_ = ticksLeft_ = param;
      break;
    default:
      break;
  }
}

void MusicDriver::vibrato(Channel& c) {  // cs:121c
  c.vibratoAt = static_cast<u8>(c.vibratoAt + c.vibratoSpeed);
  int by = (kSine[(c.vibratoAt >> 2) & (kSineSteps - 1)] * c.vibratoDepth) >> 7;
  if (c.vibratoAt & kSineNegative) by = -by;
  c.step = stepFor(c.period + by);
}

void MusicDriver::slideVolume(Channel& c) {  // cs:129e
  const int v = c.volume + c.slide;
  c.volume = static_cast<u16>(std::clamp(v, 0, kTopVolume));
}

void MusicDriver::everyTick(Channel& c) {
  switch (c.tick) {
    case Channel::Tick::Arpeggio: {  // cs:1118
      const u32 now = c.step;
      c.step = c.arpeggio2;
      c.arpeggio2 = c.arpeggio1;
      c.arpeggio1 = now;
      break;
    }
    case Channel::Tick::Portamento: {  // cs:11a5
      if (c.target == 0) break;
      int p = c.period + c.portamento;
      if (c.portamento >= 0 ? p >= c.target : p <= c.target) p = c.target;
      c.period = static_cast<i16>(p);
      c.step = c.arpeggio1 = c.arpeggio2 = stepFor(p);
      break;
    }
    case Channel::Tick::Vibrato:
      vibrato(c);
      break;
    case Channel::Tick::VibratoSlide:
      vibrato(c);
      slideVolume(c);
      break;
    case Channel::Tick::Slide:
      slideVolume(c);
      break;
    case Channel::Tick::Retrigger:  // cs:131d
      if (--c.retriggerLeft == 0) {
        c.retriggerLeft = c.retrigger;
        c.position = 0;
      }
      break;
    case Channel::Tick::None:
      break;
  }
}

/// cs:0dac: a row's four notes, and then on to the next row, or to wherever a jump says.
void MusicDriver::playRow() {
  if (cells_.empty()) return;
  for (std::size_t n = 0; n < modRec::channels; ++n) {
    const u8* cell = &cells_[(rowAt_ + n * sdr::noteBytes) % cells_.size()];
    const u16 packed = static_cast<u16>(cell[0] | (cell[1] << 8));
    rowSample_ = static_cast<u8>((packed >> sdr::sampleShift) & sdr::sampleBits);
    trigger(ch_[n], rowSample_, static_cast<u8>(packed & sdr::noteBits), static_cast<u8>((packed >> sdr::effectShift) & sdr::effectBits), cell[2]);
    if (rowSample_ != 0 || (packed & sdr::noteBits) != 0) ch_[n].byEffect = false;  // the music's own note now
  }
  if (pendingRow_ == 0) {
    rowAt_ += sdr::rowBytes;
    if (rowAt_ % sdr::patternBytes != 0) return;
  }
  ++position_;
  while (position_ >= length_) {
    position_ = restart_;
    if (restart_ >= length_) break;
  }
  if (conductor && !placed_) position_ = conductor->next(static_cast<u8>(position_));
  placed_ = false;
  const u8 row = pendingRow_ ? static_cast<u8>(pendingRow_ - 1) : 0;
  rowAt_ = std::size_t{order_[position_ & kPlaceBits]} * sdr::patternBytes + static_cast<u8>(row << 2) * unsigned{sdr::noteBytes};
  pendingRow_ = 0;
}

/// cs:0d20: a fiftieth of a second.
void MusicDriver::tick() {
  ++ticks_;
  if (--ticksLeft_ != 0) {
    for (Channel& c : ch_) everyTick(c);
    return;
  }
  ticksLeft_ = speed_;
  playRow();
}

void MusicDriver::mixVoice(Channel& c, bool left, float* out, std::size_t frames, float master) {
  if (!c.data) return;
  if (balanced_.load(std::memory_order_relaxed)) return mixVoiceBalanced(c, left, out, frames, master);
  const float gain = static_cast<float>(c.volume >> 8) / kVolumeSteps / kSampleTop * kChannelShare * master;
  for (std::size_t i = 0; i < frames; ++i) {
    if (c.position >= c.end) {
      if (c.loops <= kNoLoop) return;  // it has played out
      c.position = static_cast<u16>(c.position - c.end + c.loopStart);
      if (c.position >= c.end) c.position = c.loopStart;
    }
    const float s = static_cast<float>(c.data[c.position]) * gain;  // (the byte it is at, as the driver takes it)
    out[i * 2 + (left ? 0 : 1)] += s;
    const u32 moved = u32{c.fraction} + (c.step & 0xffff);
    c.fraction = static_cast<u16>(moved);
    const u32 position = u32{c.position} + (c.step >> 16) + (moved >> 16);
    c.position = position > 0xffff ? 0xffff : static_cast<u16>(position);
  }
}

/// The balanced mixing of one voice (setBalanced): the same steps through the sample as the
/// driver's, read between the bytes, eased, and spread over both sides.
void MusicDriver::mixVoiceBalanced(Channel& c, bool left, float* out, std::size_t frames, float master) {
  const bool loops = c.loops > kNoLoop && c.end > c.loopStart;
  // the byte at a place before or past the sample's end: its loop, or silence
  auto at = [&](int i) -> float {
    if (i < 0) return 0.0f;
    if (i >= c.end) {
      if (!loops) return 0.0f;
      const int span = c.end - c.loopStart;
      i = c.loopStart + (i - c.end) % span;
    }
    return static_cast<float>(c.data[i]);
  };
  const float wanted = static_cast<float>(c.volume >> 8) / kVolumeSteps / kSampleTop * kChannelShare * master;
  if (std::exchange(c.started, false)) c.level = 0;
  const float ease = static_cast<float>(1.0 - std::exp(-1.0 / (kEaseSeconds * rate_)));
  // mostly on its own side, a little on the other (the driver: all on its own side)
  const float toLeft = left ? kOwnSide : 1.0f - kOwnSide, toRight = 1.0f - toLeft;
  const std::vector<float>& sinc = sincTable();
  for (std::size_t i = 0; i < frames; ++i) {
    if (c.position >= c.end) {
      if (!loops) return;  // it has played out
      c.position = static_cast<u16>(c.position - c.end + c.loopStart);
      if (c.position >= c.end) c.position = c.loopStart;
    }
    const float* taps = &sinc[static_cast<std::size_t>(c.fraction >> (16 - kPhaseBits)) * kTaps];
    float smooth = 0;
    for (int t = 0; t < kTaps; ++t) smooth += taps[t] * at(static_cast<int>(c.position) + t - kTaps / 2 + 1);
    float s = smooth + (static_cast<float>(c.data[c.position]) - smooth) * kNearest;
    c.level += (wanted - c.level) * ease;
    s *= c.level;
    out[i * 2] += s * toLeft;
    out[i * 2 + 1] += s * toRight;
    const u32 moved = u32{c.fraction} + (c.step & 0xffff);
    c.fraction = static_cast<u16>(moved);
    const u32 position = u32{c.position} + (c.step >> 16) + (moved >> 16);
    c.position = position > 0xffff ? 0xffff : static_cast<u16>(position);
  }
}

void MusicDriver::mix(float* out, std::size_t frames) {
  const float master = static_cast<float>(master_) / kMasterTop;
  for (std::size_t n = 0; n < modRec::channels; ++n) mixVoice(ch_[n], n < kLeftChannels, out, frames, master);
  for (Voice& v : voices_) {
    mixVoice(v.c, v.left, out, frames, master);
    if (v.c.data && v.c.position >= v.c.end) v.c.data = nullptr;  // played out: free again
  }
}

void MusicDriver::render(float* out, int frames) {
  const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
  renderAt(out, frames, now);
}

/// `now`: when the card asked, in seconds of the steady clock; below nought, every effect
/// waiting is heard at once.
void MusicDriver::renderAt(float* out, int frames, double now) {
  std::lock_guard lock(mutex_);
  std::size_t left = frames > 0 ? static_cast<std::size_t>(frames) : 0;
  float* const begin = out;
  std::fill_n(out, left * 2, 0.0f);
  // stopped, or held by this version's pause: nothing sounds, and nothing moves on
  if (!loaded_ || !playing_ || held_) {
    for (const Asked& a : asked_) startEffect(a);
    asked_.clear();
    return;
  }
  // Where in this helping each effect waiting falls: one helping after its moment, so that
  // those asked for since the last helping spread over this one as they were asked. One
  // later than that (a frame run late) is heard at once; one not yet due waits for the next.
  const double helping = static_cast<double>(left) / rate_;
  auto due = [&](const Asked& a) -> std::size_t {
    if (a.when < 0 || now < 0) return 0;
    const double at = std::round((a.when + helping - now) * rate_);
    return at <= 0 ? 0 : static_cast<std::size_t>(at);
  };
  std::size_t done = 0;
  while (left != 0) {
    // the effects due by now, in the order asked
    while (!asked_.empty() && due(asked_.front()) <= done) {
      startEffect(asked_.front());
      asked_.erase(asked_.begin());
    }
    if (tickFrames_ < 1.0) {
      if (conductor) {
        if (const int place = conductor->interrupt(); place >= 0) {
          // int66::jump lets the row now due be played first, and goes to the new place
          // after it: with six ticks to a row, an eighth of a second later. Here the new
          // place's first row is the one played at this tick, so a jingle is heard at once.
          position_ = static_cast<u16>(place);
          while (position_ >= length_ && length_ != 0) position_ = restart_ < length_ ? restart_ : 0;
          rowAt_ = std::size_t{order_[position_ & kPlaceBits]} * sdr::patternBytes;
          pendingRow_ = 0;
          placed_ = false;
          ticksLeft_ = 1;
        }
      }
      tick();
      tickFrames_ += rate_ / kTickRate;
    }
    std::size_t step = std::min(left, static_cast<std::size_t>(tickFrames_));
    if (!asked_.empty()) step = std::min(step, std::max<std::size_t>(due(asked_.front()) - done, 1));
    mix(out, step);
    out += step * 2;
    left -= step;
    done += step;
    tickFrames_ -= static_cast<double>(step);
  }
  if (balanced_.load(std::memory_order_relaxed)) tone(begin, static_cast<std::size_t>(out - begin) / 2);
  // Overlapping effects (and the tone) can add up past full scale: kept within it.
  for (float* s = begin; s != out; ++s) *s = std::clamp(*s, -1.0f, 1.0f);
}

void MusicDriver::pass(double seconds) {
  passed_ += seconds * rate_;
  const int frames = static_cast<int>(passed_);
  passed_ -= frames;
  unheard_.resize(static_cast<std::size_t>(frames) * 2);
  renderAt(unheard_.data(), frames, -1);
}

}  // namespace encore
