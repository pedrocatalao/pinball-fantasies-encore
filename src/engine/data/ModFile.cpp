#include "engine/data/ModFile.h"

#include <algorithm>
#include <cstring>

#include "core/Error.h"
#include "core/File.h"

namespace encore {

ModFile ModFile::load(const std::filesystem::path& path) {
  auto bytes = file::readAll(path);
  if (!bytes) throw DataError("cannot read " + path.string());
  return parse(*bytes, path.filename().string());
}

ModFile ModFile::parse(ByteView b, std::string name) {
  if (b.size() < modRec::patterns) throw DataError(name + ": too small for a ProTracker module");
  ModFile mod;
  auto text = [&](std::size_t off, std::size_t len) {
    std::string s(reinterpret_cast<const char*>(b.data() + off), len);
    const auto nul = s.find('\0');
    if (nul != std::string::npos) s.resize(nul);
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
  };
  mod.title = text(modRec::title, modRec::titleSize);
  const char* tag = reinterpret_cast<const char*>(b.data() + modRec::mark);
  if (std::memcmp(tag, "M.K.", modRec::markSize) != 0 && std::memcmp(tag, "M!K!", modRec::markSize) != 0 &&
      std::memcmp(tag, "FLT4", modRec::markSize) != 0)
    throw DataError(name + ": not a 4-channel ProTracker module");

  for (int i = 0; i < modRec::sampleCount; ++i) {
    const std::size_t o = modRec::samples + static_cast<std::size_t>(i) * sampleRec::size;
    ModSample& s = mod.samples[i];
    s.name = text(o, sampleRec::nameSize);
    s.length = rd16be(b, o + sampleRec::length) * 2u;
    const u8 ft = b[o + sampleRec::finetune] & sampleRec::finetuneBits;
    s.finetune = static_cast<i8>(ft >= 8 ? static_cast<int>(ft) - 16 : ft);
    s.volume = std::min<u8>(b[o + sampleRec::volume], sampleRec::maxVolume);
    s.loopStart = rd16be(b, o + sampleRec::loopStart) * 2u;
    s.loopLength = rd16be(b, o + sampleRec::loopLength) * 2u;
  }
  mod.songLength = std::min<u8>(b[modRec::song + songRec::length], modRec::places);
  mod.restart = b[modRec::song + songRec::restart];
  std::memcpy(mod.order.data(), b.data() + (modRec::song + songRec::order), modRec::places);
  int maxPattern = 0;
  for (u8 p : mod.order) maxPattern = std::max<int>(maxPattern, p);
  mod.patternCount = maxPattern + 1;

  const std::size_t patternBytes = static_cast<std::size_t>(mod.patternCount) * modRec::patternSize;
  if (modRec::patterns + patternBytes > b.size()) throw DataError(name + ": truncated pattern data");
  mod.notes.resize(static_cast<std::size_t>(mod.patternCount) * modRec::rows * modRec::channels);
  for (std::size_t i = 0; i < mod.notes.size(); ++i) {
    const std::size_t o = modRec::patterns + i * modRec::noteSize;
    ModNote& n = mod.notes[i];
    n.sample = static_cast<u8>((b[o] & noteRec::highHalf) | (b[o + noteRec::effect] >> 4));
    n.period = static_cast<u16>(((b[o] & noteRec::lowHalf) << 8) | b[o + noteRec::periodLow]);
    n.effect = b[o + noteRec::effect] & noteRec::lowHalf;
    n.param = b[o + noteRec::param];
  }
  std::size_t pos = modRec::patterns + patternBytes;
  for (ModSample& s : mod.samples) {
    const std::size_t take = std::min<std::size_t>(s.length, b.size() > pos ? b.size() - pos : 0);
    s.data.resize(s.length, 0);
    for (std::size_t i = 0; i < take; ++i) s.data[i] = static_cast<i8>(b[pos + i]);
    pos += s.length;
    if (s.loopStart + s.loopLength > s.length) {
      s.loopStart = std::min(s.loopStart, s.length);
      s.loopLength = s.length - s.loopStart;
    }
  }
  return mod;
}

}  // namespace encore
