#include "engine/audio/MusicDriver.h"

#include <utility>

#include <algorithm>
#include <cstring>

namespace encore {
namespace {

constexpr u16 kPeriods[16 * 36] = {
#include "engine/audio/DriverPeriods.inc"
};

/// The vibrato's wave (ds:69e2): half a sine, thirty-two steps.
constexpr u8 kSine[32] = {0x00, 0x18, 0x31, 0x4a, 0x61, 0x78, 0x8d, 0xa1, 0xb4, 0xc5, 0xd4, 0xe0, 0xeb, 0xf4, 0xfa, 0xfd,
                          0xff, 0xfd, 0xfa, 0xf4, 0xeb, 0xe0, 0xd4, 0xc5, 0xb4, 0xa1, 0x8d, 0x78, 0x61, 0x4a, 0x31, 0x18};

/// The Amiga's clock, as the driver has it (cs:1001): a period is so many of its beats.
constexpr u32 kClock = 0x361f0f;

u16 big(ByteView b, std::size_t at) { return static_cast<u16>((b[at] << 8) | b[at + 1]); }

}  // namespace

MusicDriver::MusicDriver(int outputRate) : rate_(outputRate) {}

/// cs:071f: the module read in. Its notes are kept three bytes each: the note as a number
/// (1 to 36, found by its period in the first row of the table; a period that is not there
/// loses the whole note), the sample, the effect and its number.
bool MusicDriver::load(ByteView mod) {
  std::lock_guard lock(mutex_);
  loaded_ = false;
  if (mod.size() < 0x43c) return false;
  // "M.K." and one other mark say thirty-one samples, lengths of loops in words (cs:077a)
  const u16 mark = static_cast<u16>((mod[0x438] | (mod[0x439] << 8)) + (mod[0x43a] | (mod[0x43b] << 8)));
  const bool modern = mark == 0x5c98 || mark == 0x809a;
  const std::size_t count = modern ? 31 : 15;
  const std::size_t header = 20 + count * 30;
  if (mod.size() < header + 130 + (modern ? 4 : 0)) return false;
  length_ = mod[header];
  restart_ = mod[header + 1] & 0x7f;
  if (restart_ >= length_) restart_ = 0;
  u8 patterns = 0;
  for (std::size_t i = 0; i < 128; ++i) {
    order_[i] = mod[header + 2 + i];
    patterns = std::max(patterns, order_[i]);
  }
  ++patterns;
  const std::size_t notes = header + 130 + (modern ? 4 : 0);
  if (mod.size() < notes + std::size_t{patterns} * 0x400) return false;
  cells_.assign(std::size_t{patterns} * 0x300, 0);
  for (std::size_t i = 0; i < std::size_t{patterns} * 0x100; ++i) {
    const u8* in = &mod[notes + i * 4];
    const u16 period = static_cast<u16>(((in[0] & 0x0f) << 8) | in[1]);
    u8 note = 0;
    bool known = period == 0;
    for (u8 n = 0; n < 36 && !known; ++n)
      if (kPeriods[n] == period) {
        note = static_cast<u8>(n + 1);
        known = true;
      }
    if (!known) continue;
    const u8 sample = static_cast<u8>(((in[0] & 0xf0) | (in[2] >> 4)) & 0x1f);
    u8* out = &cells_[i * 3];
    out[0] = static_cast<u8>(note | (sample << 6));
    out[1] = static_cast<u8>((sample >> 2) | ((in[2] & 0x0f) << 3));
    out[2] = in[3];
  }
  // the samples (cs:0964)
  samples_.fill(Sample{});
  sampleData_.assign(31, {});
  std::size_t at = notes + std::size_t{patterns} * 0x400;
  for (std::size_t s = 0; s < count; ++s) {
    const std::size_t h = 20 + s * 30;
    Sample& out = samples_[s];
    const u16 length = static_cast<u16>(big(mod, h + 22) << 1);
    if (length < 3) continue;  // (nothing: it is left a silent byte)
    out.table = static_cast<u16>((mod[h + 24] & 0x0f) * 36);
    out.volume = mod[h + 25];
    u16 loopStart = big(mod, h + 26), loopLength = big(mod, h + 28);
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
  rowAt_ = std::size_t{order_[0]} * 0x300;
  pendingRow_ = 0;
  speed_ = 6;
  ticksLeft_ = 1;
  ch_.fill(Channel{});
  voices_.fill(Voice{});
  loaded_ = true;
  return true;
}

u32 MusicDriver::stepFor(int period) const {
  if (period <= 0) return 0;
  const u32 perSecond = kClock / static_cast<u32>(period);  // whole sample bytes a second, as the driver's first division
  return static_cast<u32>((u64{perSecond} << 16) / static_cast<u32>(rate_));
}

int MusicDriver::periodAt(u16 index) const { return index < std::size(kPeriods) ? kPeriods[index] : 113; }

/// Function 0x04 (cs:061a), as far as the music goes: it goes on from where it was, the next
/// row at the next tick.
u8 MusicDriver::start() {
  std::lock_guard lock(mutex_);
  playing_ = loaded_;
  if (std::exchange(held_, false)) return 0;
  ticksLeft_ = 1;
  return 0;
}

/// Function 0x0f (cs:023c, cs:1417): the card is stopped and every channel falls silent.
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

void MusicDriver::volume(u16 level) {
  std::lock_guard lock(mutex_);
  // cs:1584: the level goes to the sound card's own mixer as a byte, 0x100 becoming its 0xff:
  // so 0x100 is all of it, which is what the game sends when it is not fading
  master_ = static_cast<u8>(level - (level >> 8));
}

/// Function 0x10 (cs:0201): the place to go to once the row now due has been played. The
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

/// Function 0x11 (cs:01db): a note on a channel, with a volume if one is given.
void MusicDriver::effect(u8 sample, u8 note, u8 volume, u8 channel) {
  std::lock_guard lock(mutex_);
  if (!loaded_ || channel < 1 || channel > 4) return;
  startEffect(sample, note, volume, channel);
}

/// The driver's own note on the channel; what it would cut off, if it was another effect still
/// sounding, plays on to its end on a voice of its own (the oldest such voice gives way). The
/// same effect again starts over, as in the original: a bonus counted, a bumper hit and hit
/// again, would otherwise pile up into a blur.
void MusicDriver::startEffect(u8 sample, u8 note, u8 volume, u8 channel) {
  Channel& c = ch_[channel - 1u];
  const bool sounding = c.data && c.position < c.end && c.loops <= 2;  // (one that loops would never end)
  const bool another = sample != 0 && sample <= 31 && c.data != samples_[sample - 1u].data;
  if (another && note != 0 && c.byEffect && sounding) {
    Voice* to = &voices_[0];
    for (Voice& v : voices_) {
      if (!v.c.data) {
        to = &v;
        break;
      }
      if (v.since < to->since) to = &v;
    }
    *to = {c, channel <= 2, ++voicesStarted_};
  }
  trigger(c, sample, note, volume ? 0x0c : 0, volume);
  c.byEffect = true;
}

/// cs:0e3e: a note begins, or an effect on the one that plays.
void MusicDriver::trigger(Channel& c, u8 sample, u8 note, u8 effect, u8 param) {
  if (effect != 3 && effect != 5) {
    if (sample != 0 && sample <= 31) {  // cs:1026
      const Sample& s = samples_[sample - 1u];
      c.data = s.data;
      c.volume = static_cast<u16>(s.volume << 8);
      c.table = s.table;
      c.loopStart = s.loopStart;
      c.offset = 0;
      c.loops = s.loopEnd;
      c.end = s.loopEnd;
      if (static_cast<u16>(s.loopEnd - s.loopStart) <= 2) {
        c.loops = 1;
        c.end = s.length;
      }
    }
    if (note != 0) {  // cs:0fe4
      c.note = static_cast<u16>(c.table + note - 1);
      c.period = static_cast<i16>(periodAt(c.note));
      c.step = stepFor(c.period);
      c.position = c.offset;
      c.vibratoAt = 0;
    }
  }
  // cs:107b
  if (c.vibrated) c.step = stepFor(c.period);
  c.vibrated = false;
  c.tick = Channel::Tick::None;
  switch (effect & 0x0f) {
    case 0x0:  // the note and two above it in turn
      if (param == 0) break;
      c.arpeggio1 = stepFor(periodAt(static_cast<u16>(c.note + (param >> 4))));
      c.arpeggio2 = stepFor(periodAt(static_cast<u16>(c.note + (param & 0x0f))));
      c.tick = Channel::Tick::Arpeggio;
      break;
    case 0x1:  // the pitch slides up
      c.target = 0x71;
      c.portamento = static_cast<i16>(-param);
      c.tick = Channel::Tick::Portamento;
      break;
    case 0x2:  // or down
      c.target = 0x358;
      c.portamento = param;
      c.tick = Channel::Tick::Portamento;
      break;
    case 0x3:  // or to the note named
      if (param != 0) c.portamento = param;
      if (note != 0) {
        c.note = static_cast<u16>(c.table + note - 1);
        c.target = static_cast<i16>(periodAt(c.note));
        if (sample != 0 && sample <= 31) c.volume = static_cast<u16>(samples_[sample - 1u].volume << 8);
      }
      if (c.period >= c.target) c.portamento = static_cast<i16>(-c.portamento);
      c.tick = Channel::Tick::Portamento;
      break;
    case 0x4:
      c.vibrated = true;
      if (param & 0xf0) c.vibratoSpeed = static_cast<u8>((param & 0xf0) >> 2);
      if (param & 0x0f) c.vibratoDepth = param & 0x0f;
      c.tick = Channel::Tick::Vibrato;
      break;
    case 0x6:
      c.tick = Channel::Tick::VibratoSlide;
      c.slide = static_cast<i16>(((param >> 4) ? (param >> 4) : -(param & 0x0f)) * 256);
      break;
    case 0x9:  // from further into the sample
      c.offset = static_cast<u16>(param << 8);
      if (rowSample_ != 0) c.position = c.offset;
      break;
    case 0xa:
      c.tick = Channel::Tick::Slide;
      c.slide = static_cast<i16>(((param >> 4) ? (param >> 4) : -(param & 0x0f)) * 256);
      break;
    case 0xb:  // the music jumps: the table is asked where to (cs:12c0)
      if (pendingRow_ != 0) break;
      position_ = static_cast<u16>((position_ & 0xff00) | (conductor ? conductor->jump(param) : param));
      --position_;
      pendingRow_ = 1;
      placed_ = true;
      break;
    case 0xc:
      c.volume = static_cast<u16>(std::min<u8>(param, 0x40) << 8);
      break;
    case 0xd:  // on to the next pattern, at a row of it
      pendingRow_ = static_cast<u8>(param + 1);
      break;
    case 0xe:
      if ((param >> 4) != 9) break;
      c.retrigger = c.retriggerLeft = param & 0x0f;
      c.tick = Channel::Tick::Retrigger;
      break;
    case 0xf:  // so many ticks to a row, whatever the number
      speed_ = ticksLeft_ = param;
      break;
    default:
      break;
  }
}

void MusicDriver::vibrato(Channel& c) {  // cs:121c
  c.vibratoAt = static_cast<u8>(c.vibratoAt + c.vibratoSpeed);
  int by = (kSine[(c.vibratoAt >> 2) & 0x1f] * c.vibratoDepth) >> 7;
  if (c.vibratoAt & 0x80) by = -by;
  c.step = stepFor(c.period + by);
}

void MusicDriver::slideVolume(Channel& c) {  // cs:129e
  const int v = c.volume + c.slide;
  c.volume = static_cast<u16>(std::clamp(v, 0, 0x4000));
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
  for (std::size_t n = 0; n < 4; ++n) {
    const u8* cell = &cells_[(rowAt_ + n * 3) % cells_.size()];
    const u16 packed = static_cast<u16>(cell[0] | (cell[1] << 8));
    rowSample_ = static_cast<u8>((packed >> 6) & 0x1f);
    trigger(ch_[n], rowSample_, static_cast<u8>(packed & 0x3f), static_cast<u8>((packed >> 11) & 0x0f), cell[2]);
    if (rowSample_ != 0 || (packed & 0x3f) != 0) ch_[n].byEffect = false;  // the music's own note now
  }
  if (pendingRow_ == 0) {
    rowAt_ += 12;
    if (rowAt_ % 0x300 != 0) return;
  }
  ++position_;
  while (position_ >= length_) {
    position_ = restart_;
    if (restart_ >= length_) break;
  }
  if (conductor && !placed_) position_ = conductor->next(static_cast<u8>(position_));
  placed_ = false;
  const u8 row = pendingRow_ ? static_cast<u8>(pendingRow_ - 1) : 0;
  rowAt_ = std::size_t{order_[position_ & 0x7f]} * 0x300 + static_cast<u8>(row << 2) * 3u;
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
  const float gain = static_cast<float>(c.volume >> 8) / 64.0f / 128.0f * 0.25f * master;
  for (std::size_t i = 0; i < frames; ++i) {
    if (c.position >= c.end) {
      if (c.loops <= 2) return;  // it has played out
      c.position = static_cast<u16>(c.position - c.end + c.loopStart);
      if (c.position >= c.end) c.position = c.loopStart;
    }
    const float s = static_cast<float>(c.data[c.position]) * gain;  // (the byte it is at, as the driver takes it)
    if (mono_) {
      out[i * 2] += s * 0.5f;
      out[i * 2 + 1] += s * 0.5f;
    } else {
      out[i * 2 + (left ? 0 : 1)] += s;
    }
    const u32 moved = u32{c.fraction} + (c.step & 0xffff);
    c.fraction = static_cast<u16>(moved);
    const u32 position = u32{c.position} + (c.step >> 16) + (moved >> 16);
    c.position = position > 0xffff ? 0xffff : static_cast<u16>(position);
  }
}

void MusicDriver::mix(float* out, std::size_t frames) {
  const float master = static_cast<float>(master_) / 255.0f;
  for (std::size_t n = 0; n < 4; ++n) mixVoice(ch_[n], n < 2, out, frames, master);
  for (Voice& v : voices_) {
    mixVoice(v.c, v.left, out, frames, master);
    if (v.c.data && v.c.position >= v.c.end) v.c.data = nullptr;  // played out: free again
  }
}

void MusicDriver::render(float* out, int frames) {
  std::lock_guard lock(mutex_);
  std::size_t left = frames > 0 ? static_cast<std::size_t>(frames) : 0;
  float* const begin = out;
  std::fill_n(out, left * 2, 0.0f);
  // stopped, or held by this version's pause: nothing sounds, and nothing moves on
  if (!loaded_ || !playing_ || held_) return;
  while (left != 0) {
    if (tickFrames_ < 1.0) {
      if (conductor) {
        if (const int place = conductor->interrupt(); place >= 0) {
          // Function 0x10 lets the row now due be played first, and goes to the new place
          // after it: with six ticks to a row, an eighth of a second later. Here the new
          // place's first row is the one played at this tick, so a jingle is heard at once.
          position_ = static_cast<u16>(place);
          while (position_ >= length_ && length_ != 0) position_ = restart_ < length_ ? restart_ : 0;
          rowAt_ = std::size_t{order_[position_ & 0x7f]} * 0x300;
          pendingRow_ = 0;
          placed_ = false;
          ticksLeft_ = 1;
        }
      }
      tick();
      tickFrames_ += rate_ / 50.0;
    }
    const std::size_t now = std::min(left, static_cast<std::size_t>(tickFrames_));
    mix(out, now);
    out += now * 2;
    left -= now;
    tickFrames_ -= static_cast<double>(now);
  }
  // Overlapping effects can add up past full scale: kept within it.
  for (float* s = begin; s != out; ++s) *s = std::clamp(*s, -1.0f, 1.0f);
}

void MusicDriver::pass(double seconds) {
  passed_ += seconds * rate_;
  const int frames = static_cast<int>(passed_);
  passed_ -= frames;
  unheard_.resize(static_cast<std::size_t>(frames) * 2);
  render(unheard_.data(), frames);
}

}  // namespace encore
