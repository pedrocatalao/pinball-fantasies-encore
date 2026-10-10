#include "engine/game/Front.h"

#include <algorithm>
#include <cstring>
#include <string_view>

#include "core/Error.h"

namespace encore {
namespace {

/// The letters the PC keeps in its ROM, eight dots square, which the menu's scroller is
/// written in (the original reads them at F000:FA6E): the ones its two texts use. A row is a
/// byte, its lowest bit the leftmost dot.
struct RomLetter {
  char letter;
  u8 rows[8];
};
constexpr RomLetter kRomLetters[] = {
    {'\'', {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00}}, {'-', {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00}},
    {'1', {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00}},  {'2', {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00}},
    {'3', {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00}},  {'4', {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00}},
    {'5', {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00}},  {'A', {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00}},
    {'B', {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00}},  {'C', {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00}},
    {'D', {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00}},  {'E', {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00}},
    {'F', {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00}},  {'G', {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00}},
    {'H', {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00}},  {'I', {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}},
    {'J', {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00}},  {'K', {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00}},
    {'L', {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00}},  {'M', {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00}},
    {'N', {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00}},  {'O', {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00}},
    {'P', {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00}},  {'Q', {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00}},
    {'R', {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00}},  {'S', {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00}},
    {'T', {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00}},  {'U', {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00}},
    {'V', {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00}},  {'W', {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00}},
    {'X', {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00}},  {'Y', {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00}},
    {'Z', {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00}},
};

/// A row of a ROM letter with its highest bit the leftmost dot, as the ROM has it.
u8 romRow(u8 letter, int row) {
  for (const RomLetter& l : kRomLetters)
    if (static_cast<u8>(l.letter) == letter) {
      u8 out = 0;
      for (int bit = 0; bit < 8; ++bit)
        if (l.rows[row] & (1 << bit)) out = static_cast<u8>(out | (0x80 >> bit));
      return out;
    }
  return 0;
}

/// The key as the launcher's keyboard routine says it (int 65h): the code of its going down.
u8 scancode(Key k) {
  static constexpr u8 kLetters[26] = {0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
                                      0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c};
  if (k >= Key::A && k <= Key::Z) return kLetters[static_cast<int>(k) - static_cast<int>(Key::A)];
  if (k >= Key::F1 && k <= Key::F8) return static_cast<u8>(0x3b + static_cast<int>(k) - static_cast<int>(Key::F1));
  // (this version takes 1 to 5 for F1 to F5 as well)
  if (k >= Key::Digit1 && k <= Key::Digit5) return static_cast<u8>(0x3b + static_cast<int>(k) - static_cast<int>(Key::Digit1));
  switch (k) {
    case Key::Space: return 0x39;
    case Key::Enter: return 0x1c;
    case Key::Escape: return 0x01;
    case Key::ArrowDown: return 0x50;
    case Key::ArrowUp: return 0x48;
    case Key::ArrowLeft: return 0x4b;
    case Key::ArrowRight: return 0x4d;
    default: return 0;
  }
}

constexpr u16 kPage1 = 0xae24;   ///< where the menu's second page is in the card's memory
constexpr u16 kHigher = 0x49b5;  ///< this version's words among the options, kept where the
constexpr u16 kFull = 0x49c0;    ///< original has a question this version does not ask (and
constexpr u16 kTall = 0x49c8;    ///< the answer's place after it)
constexpr u16 kOriginal = 0x49d0;
constexpr u16 kRemaster = 0x49da;
constexpr u16 kBalanced = 0x49e4;
constexpr u16 kBottom = 0x49ee;  ///< (past the question's words too, which are not shown either)
constexpr u16 kTop = 0x49f5;
constexpr int kOptions = 9;      ///< the original's six, and ARTWORK, AUDIO and DOT MATRIX

}  // namespace

Front::Front(ByteView prg, ByteView module, const Config& config, int returningFrom)
    : music_(48000), options_(config.options), saved_(config.options), returningFrom_(returningFrom) {
  if (prg.size() < 0x400 + 0x4daa0) throw DataError("INTRO.PRG is not the program it should be");
  image_.assign(prg.begin() + 0x400, prg.end());
  for (auto& plane : planes_) plane.assign(0x10000, 0);
  from_.assign(0x10000 * 8, 0);
  if (!music_.load(module)) throw DataError("the menu's music is not a module");
  music_.start();

  // cs:407e: the options, as PINBALL.CFG has them
  ds(0x49a3) = options_.balls == 5;
  ds(0x49a4) = options_.angle == Angle::Low ? 1 : options_.angle == Angle::Higher ? 2 : 0;
  ds(0x49a5) = static_cast<u8>(options_.scrollSpeed);
  ds(0x49a6) = options_.noMusic;
  ds(0x49a7) = static_cast<u8>(options_.resolution);
  ds(0x49a8) = options_.mono;
  std::memcpy(&ds(kHigher), "HIGHER", 7);
  std::memcpy(&ds(kFull), "FULL  ", 7);
  std::memcpy(&ds(kTall), "TALL  ", 7);
  std::memcpy(&ds(kOriginal), "ORIGINAL", 9);
  std::memcpy(&ds(kRemaster), "REMASTER", 9);
  std::memcpy(&ds(kBalanced), "BALANCED", 9);
  std::memcpy(&ds(kBottom), "BOTTOM", 7);
  std::memcpy(&ds(kTop), "TOP   ", 7);
  // the first page as the original has it, its six options, with a line to turn to the other
  // before saving; the other with a heading, this version's options and a line to turn back.
  // Each has twelve lines, an empty one a nought, as the page shows twelve and no more
  const auto copy = [&](std::vector<u8>& to, u16 from, u16 end) { to.insert(to.end(), &ds(from), &ds(from) + (end - from)); };
  const auto addLine = [](std::vector<u8>& to, std::string text) {
    text.resize(0x18, ' ');
    to.insert(to.end(), text.begin(), text.end());
  };
  std::vector<u8>& firstPage = optionsPages_[0];
  std::vector<u8>& morePage = optionsPages_[1];
  copy(firstPage, 0x4e40, 0x4edf);  // the heading, an empty line, the six options and an empty line
  addLine(firstPage, "  MORE OPTIONS");
  copy(firstPage, 0x4edf, 0x4ef8);  // saving, and an empty line
  for (const char c : std::string_view("MORE OPTIONS")) morePage.push_back(static_cast<u8>(c));
  morePage.insert(morePage.end(), {0, 0});
  for (const char* label : {"  ARTWORK:", "  AUDIO:", "  DOT MATRIX:"}) addLine(morePage, label);
  morePage.push_back(0);
  addLine(morePage, "  BACK");
  copy(morePage, 0x4edf, 0x4ef8);
  morePage.insert(morePage.end(), {0, 0, 0});

  // cs:371f: the best scores, written into the two pages that show them
  static constexpr u16 kRows[4] = {0x4f31, 0x4fc1, 0x5051, 0x50e1};
  for (std::size_t t = 0; t < 4; ++t)
    for (u16 n = 0; n < 4; ++n) {
      const HighScore& h = config.highScores[t][n];
      const u16 row = static_cast<u16>(kRows[t] + n * 0x18);
      for (u16 i = 0; i < 3; ++i) ds(static_cast<u16>(row + i)) = h.name[i];
      bool begun = false;
      for (u16 i = 0; i < 12; ++i) {
        begun |= h.score.digits[i] != 0;
        ds(static_cast<u16>(row + 6 + i)) = begun ? static_cast<u8>(h.score.digits[i] + 0x30) : ' ';
      }
      if (!begun) ds(static_cast<u16>(row + 6 + 11)) = '0';  // (this version: a score of nought is written)
    }
  // (this version: each table's name in the middle of its line, where the original has them all
  // begin at one place)
  for (const u16 line : {u16{0x4efc + 0x18}, u16{0x4efc + 0x18 * 7}, u16{0x501c + 0x18}, u16{0x501c + 0x18 * 7}}) {
    std::string name;
    for (u16 i = 0; i < 0x18; ++i) name += static_cast<char>(ds(static_cast<u16>(line + i)));
    const auto first = name.find_first_not_of(' '), last = name.find_last_not_of(' ');
    if (first == std::string::npos) continue;
    name = name.substr(first, last - first + 1);
    const std::size_t before = (0x18 - name.size()) / 2;
    for (u16 i = 0; i < 0x18; ++i) ds(static_cast<u16>(line + i)) = i >= before && i - before < name.size() ? static_cast<u8>(name[i - before]) : ' ';
  }
  for (int row = 0; row < kOptions; ++row) {  // cs:404a: the options' words into their page
    u16 word = 0;
    optionText(row, word);
    for (u8* at = optionWord(row); ds(word) != 0; ++word, ++at) *at = ds(word);
  }
  task_ = main();
}

Front::~Front() = default;

void Front::key(Key key, bool down) {
  if (!down) return;
  if (const u8 code = scancode(key); code != 0) key_ = code;
}

Front::Action Front::frame() {
  if (done_) return {};
  frameCallback();
  if (waiting_) {
    const auto go = std::exchange(waiting_, {});
    go.resume();
  } else {
    task_.handle.resume();
  }
  if (task_.handle.done()) {
    done_ = true;
    if (task_.handle.promise().error) std::rethrow_exception(task_.handle.promise().error);
  }
  selectTop_ = splitPalette_ ? 1 : colourSelect_;
  midFrameCallback();
  selectBottom_ = colourSelect_;
  return std::exchange(action_, {});
}

// ---------------------------------------------------------------------------------------
// the video card
// ---------------------------------------------------------------------------------------
u8 Front::peek(u16 at) {
  for (std::size_t p = 0; p < 4; ++p) latch_[p] = planes_[p][at];
  return latch_[readPlane_];
}

void Front::poke(u16 at, u8 value) {
  for (std::size_t p = 0; p < 4; ++p) {
    if (!(planeMask_ & (1 << p))) continue;
    if (writeMode_ == 1) planes_[p][at] = latch_[p];
    else if (enableSetReset_ & (1 << p)) planes_[p][at] = (setReset_ & (1 << p)) ? 0xff : 0;
    else planes_[p][at] = value;
  }
  if (writeMode_ != 1) forget(at);
}

void Front::forget(u16 at, u8 dots) {
  for (int bit = 0; bit < 8; ++bit)
    if (dots & (0x80 >> bit)) from_[std::size_t{at} * 8 + static_cast<std::size_t>(bit)] = 0;
}

void Front::copy(u16 to, u16 from, u16 count) {
  for (u16 i = 0; i < count; ++i) {
    poke(static_cast<u16>(to + i), peek(static_cast<u16>(from + i)));
    if (writeMode_ == 1)
      for (std::size_t bit = 0; bit < 8; ++bit)
        from_[std::size_t{static_cast<u16>(to + i)} * 8 + bit] = from_[std::size_t{static_cast<u16>(from + i)} * 8 + bit];
  }
}

void Front::fill(u16 to, u16 count, u8 value) {
  for (u16 i = 0; i < count; ++i) {
    poke(static_cast<u16>(to + i), value);
    forget(static_cast<u16>(to + i));
  }
}

void Front::writeMode(int mode) {
  writeMode_ = mode;
  if (mode == 1) planeMask_ = 0x0f;
}

void Front::setDac(int first, const u8* values, int bytes) {
  for (int i = 0; i < bytes && first * 3 + i < 768; ++i) dac_[static_cast<std::size_t>(first * 3 + i)] = values[i] & 0x3f;
}

void Front::clearVideo() {
  for (auto& plane : planes_) std::fill(plane.begin(), plane.end(), u8{0});
  std::fill(from_.begin(), from_.end(), u32{0});
}

/// cs:4998: where in a picture a part of it begins, past its four letters.
u16 Front::find(u16 segment, const char* tag) {
  for (u32 at = 0; at < 0xc350; ++at)
    if (farByte(segment, at) == static_cast<u8>(tag[0]) && farByte(segment, at + 1) == static_cast<u8>(tag[1]) &&
        farByte(segment, at + 2) == static_cast<u8>(tag[2]) && farByte(segment, at + 3) == static_cast<u8>(tag[3]))
      return static_cast<u16>(at + 4);
  throw DataError(std::string("a picture of the menu's has no ") + tag);
}

/// cs:4880: a picture of sixteen colours, a row of each plane at a time, into the card's
/// memory from a row on. Answers where its colours are, which it leaves as the card takes them.
u16 Front::unpackPlanar(u16 segment, u16 row, bool setColours, HdPicture is) {
  const u16 cmap = find(segment, "CMAP");
  const u16 colours = static_cast<u16>((farByte(segment, cmap + 2u) << 8) | farByte(segment, cmap + 3u));
  const u16 palette = static_cast<u16>(cmap + 4);
  for (u16 i = 0; i < colours; ++i) farByte(segment, static_cast<u32>(palette + i)) >>= 2;
  if (setColours) setDac(0, &farByte(segment, palette), colours);
  const u16 header = find(segment, "BMHD");
  const u16 width = static_cast<u16>(((farByte(segment, header + 4u) << 8) | farByte(segment, header + 5u)) >> 3);
  const u16 height = static_cast<u16>((farByte(segment, header + 6u) << 8) | farByte(segment, header + 7u));
  const u16 end = static_cast<u16>((row + height) * 80);
  u32 from = find(segment, "BODY") + 4u;
  u16 line = static_cast<u16>(row * 80 - 80);
  u8 mask = 0x11;
  for (;;) {
    const std::size_t plane = mask & 1 ? 0 : mask & 2 ? 1 : mask & 4 ? 2 : 3;
    u16 at = line;
    if (mask & 1) {
      at = static_cast<u16>(at + 80);
      line = at;
    }
    if (at >= end) break;
    mask = static_cast<u8>((mask << 1) | (mask >> 7));
    for (bool rowDone = false; !rowDone;) {
      const u8 n = farByte(segment, from++);
      if (n == 0x80) continue;
      if (n < 0x80) {
        for (int i = 0; i <= n; ++i) planes_[plane][at++] = farByte(segment, from++);
      } else {
        const u8 v = farByte(segment, from++);
        for (int i = 0; i <= 256 - n; ++i) planes_[plane][at++] = v;
      }
      rowDone = width != 0 && at % width == 0;
    }
  }
  // which picture's dots these now are (of the menu's own picture, only the panel on its left)
  const u32 acrossTo = is == HdPicture::Left ? 130 : is == HdPicture::None ? 0 : static_cast<u32>(width * 8);
  for (u32 y = 0; y < height; ++y)
    for (u32 x = 0; x < static_cast<u32>(width * 8); ++x)
      from_[(std::size_t{static_cast<u16>((row + y) * 80)} * 8 + x) % from_.size()] =
          x < acrossTo ? (static_cast<u32>(is) << 20) | (x << 10) | y : 0;
  return palette;
}

/// cs:4a39 (and cs:4db0, which takes more rows): a picture of 256 colours, a dot to a byte,
/// into the card's memory where four dots share an address, one in each plane.
u16 Front::unpackChunky(u16 segment, u16 row, u16 bytes) {
  find(segment, "PBM ");
  const u16 cmap = find(segment, "CMAP");
  const u16 colours = static_cast<u16>((farByte(segment, cmap + 2u) << 8) | farByte(segment, cmap + 3u));
  const u16 palette = static_cast<u16>(cmap + 4);
  // (colours 32 to 63 are made here, the first 32 at half their strength: by the one routine
  // only, cs:4aa2, which is the one that takes 151 rows)
  if (bytes == 0x2f40)
    for (u16 i = 0; i < 0x60; ++i) farByte(segment, static_cast<u32>(palette + 0x60 + i)) = farByte(segment, static_cast<u32>(palette + i)) >> 1;
  for (u16 i = 0; i < colours; ++i) farByte(segment, static_cast<u32>(palette + i)) >>= 2;
  u32 from = find(segment, "BODY") + 4u;
  u16 at = static_cast<u16>(row * 80);
  const u16 end = static_cast<u16>(at + bytes);
  u8 mask = 0x88;
  auto dot = [&](u8 v) {
    mask = static_cast<u8>((mask << 1) | (mask >> 7));
    planes_[mask & 1 ? 0 : mask & 2 ? 1 : mask & 4 ? 2 : 3][at] = v;
    if (mask & 8) ++at;
  };
  for (;;) {
    const u8 n = farByte(segment, from++);
    if (n == 0x80) continue;
    if (n < 0x80) {
      for (int i = 0; i <= n; ++i) dot(farByte(segment, from++));
    } else {
      const u8 v = farByte(segment, from++);
      for (int i = 0; i <= 256 - n; ++i) dot(v);
    }
    if (at >= end) break;
  }
  return palette;
}

void Front::sendColours(u8 first, u16 picture) {
  setDac(first, &farByte(dsw(static_cast<u16>(0x589e + picture)), dsw(static_cast<u16>(0x58ac + picture))), 0x30);
  level_ = 1.0f;
}

/// cs:2c37: a letter of the menu's own, 24 dots by 14, from where the letters are kept in
/// the card's memory, laid over what is there.
void Front::glyph(u16 y, u16 x, u8 letter) {
  u16 from = dsw(static_cast<u16>(0x59ed + letter * 2));
  if (from == 0xffff) return;
  from = static_cast<u16>(from + fontAt_);
  const u16 to = static_cast<u16>((x >> 3) + y * 80 + drawAt_);
  const int shift = x & 7;
  for (auto& plane : planes_) {
    u16 s = from, d = to;
    for (int row = 0; row < 14; ++row, s = static_cast<u16>(s + 80), d = static_cast<u16>(d + 80)) {
      u8 carry = 0;
      for (u16 b = 0; b < 3; ++b) {
        const u8 v = plane[static_cast<u16>(s + b)];
        plane[static_cast<u16>(d + b)] |= static_cast<u8>((v >> shift) | carry);
        forget(static_cast<u16>(d + b), static_cast<u8>((v >> shift) | carry));
        carry = shift ? static_cast<u8>(v << (8 - shift)) : 0;
      }
    }
  }
}

/// cs:2cf2: a page of twelve lines, each of up to 24 letters and set in the middle.
void Front::text(u16 y, const u8* page) {
  for (int line = 0; line < 12; ++line, y = static_cast<u16>(y + 0x12)) {
    u16 left = 0;  // what the original's count has left when it finds the line's end
    for (u16 i = 0; i < 0x18; ++i)
      if (page[i] == 0) {
        left = static_cast<u16>(0x18 - i - 1);
        break;
      }
    u16 x = static_cast<u16>(0xa4 + ((0x12 * left) >> 1));
    for (u16 n = static_cast<u16>(0x18 - left); n > 0; --n, x = static_cast<u16>(x + 0x12)) glyph(y, x, *page++);
  }
}

void Front::string(u16 y, u16 x, u16 words) {
  for (; ds(words) != 0; ++words, x = static_cast<u16>(x + 0x12)) glyph(y, x, ds(words));
}

/// cs:13ea: the heading kept from a picture (50 bytes by 40 rows of each plane) put back.
void Front::heading(u16 from, u16 to) {
  for (auto& plane : planes_) {
    u16 at = to;
    for (int row = 0; row < 40; ++row, at = static_cast<u16>(at + 0x1e))
      for (int b = 0; b < 50; ++b) plane[at++] = ds(from++);
  }
  for (u32 y = 0; y < 40; ++y)
    for (u32 x = 0; x < 400; ++x)
      from_[std::size_t{static_cast<u16>(to + y * 80)} * 8 + x] = (static_cast<u32>(HdPicture::HiScores) << 20) | (x << 10) | y;
  writeMode_ = 0;
}

void Front::threeColours(int times, int of) {
  static constexpr u8 kColours[3][4] = {{0x01, 0x34, 0x34, 0x3e}, {0x0c, 0x25, 0x25, 0x2d}, {0x0e, 0x11, 0x11, 0x19}};
  for (const auto& c : kColours)
    for (std::size_t part = 0; part < 3; ++part)
      dac_[c[0] * 3u + part] = static_cast<u8>(((c[part + 1] * (times & 0xff)) / of) & 0x3f);
  textLevel_ = static_cast<float>(times < 0 ? 0 : times) / static_cast<float>(of);
}

void Front::rubOutText(bool both) {
  writeMode(1);
  peek(0x13);
  for (const u16 first : {u16{0x14}, static_cast<u16>(kPage1 + 0x14)}) {
    if (first == 0x14 && !both) continue;
    u16 at = first;
    for (int row = 0; row < 0xf0; ++row, at = static_cast<u16>(at + 80)) fill(at, 0x37);
  }
}

// ---------------------------------------------------------------------------------------
// what the sound driver calls each frame
// ---------------------------------------------------------------------------------------
void Front::frameCallback() {
  clock_ = static_cast<u16>(clock_ + 3);
  if (openWidth_ != 0) shownWidth_ = static_cast<u16>((openWidth_ >> 2) * 8);
  if (splitPalette_) colourSelect_ = 1;
  if (scrollerOn_) scroller();
}

void Front::midFrameCallback() {
  if (slideIn_) {
    const u16 at = slideAt_--;
    if (at == 0xffff) slideIn_ = false;
    else setStart(at);
  }
  if (splitPalette_) colourSelect_ = 0;
}

/// cs:2987: the panel on the left. It shows its picture for eight seconds; then the picture
/// is painted over from the top, a text is written on it letter by letter, and after another
/// eight seconds the picture comes back from the bottom.
void Front::scroller() {
  if (--scrollerCount_ > 0) return;
  const auto keptLatch = latch_;
  const u8 keptMask = planeMask_, keptSet = setReset_, keptEnable = enableSetReset_, keptRead = readPlane_;
  const int keptMode = writeMode_;
  auto restore = [&] {
    latch_ = keptLatch;
    planeMask_ = keptMask, setReset_ = keptSet, enableSetReset_ = keptEnable, readPlane_ = keptRead;
    writeMode_ = keptMode;
  };
  const i16 count = scrollerCount_;
  if (scrollerShowsText_) {
    if (count <= -0x5a) {  // cs:29ea
      scrollerShowsText_ = false;
      scrollerCount_ = 0x1e0;
      return restore();
    }
    // cs:2a69: a row of the picture back
    writeMode(1);
    const i16 row = static_cast<i16>(-count - 0x59);
    const u16 from = static_cast<u16>(0xfb14 + row * -14);
    const u16 to = static_cast<u16>(static_cast<u16>(0x5f - row) * 80 + 2);
    copy(to, from, 14);
    copy(static_cast<u16>(to + kPage1), from, 14);
    return restore();
  }
  if (count > -0x5a) {  // cs:2ab7: three rows painted over
    scrollerNow_ = scrollerText_;
    writeMode_ = 0;
    planeMask_ = 0x0f;
    setReset_ = 2;
    enableSetReset_ = 0x0f;
    for (int n = 3;; ) {
      const u16 at = static_cast<u16>(static_cast<u16>(0x5f - scrollerCount_) * 80 + 1);
      fill(at, 14);
      fill(static_cast<u16>(at + kPage1), 14);
      if (--n == 0) break;
      --scrollerCount_;
    }
    return restore();
  }
  if (count > -210) {  // cs:2b0f: one more letter
    writeMode_ = 0;
    planeMask_ = 2;
    enableSetReset_ = keptEnable;
    const u16 n = static_cast<u16>(-count - 0x5a);
    const u8 line = static_cast<u8>(n / 12), column = static_cast<u8>(n % 12);
    u16 at = static_cast<u16>(0x1e52 + static_cast<u16>(static_cast<u8>(line * 9) * 0x50) + column);
    const u8 letter = cs(static_cast<u16>(scrollerNow_ + static_cast<u8>(12 * line) + column));
    for (int row = 0; row < 8; ++row, at = static_cast<u16>(at + 80)) {
      const u8 dots = static_cast<u8>(~romRow(letter, row));
      poke(at, dots);
      poke(static_cast<u16>(at + kPage1), dots);
    }
    return restore();
  }
  scrollerShowsText_ = true;
  scrollerCount_ = 0x1e0;
  restore();
}

// ---------------------------------------------------------------------------------------
// the routine itself
// ---------------------------------------------------------------------------------------
Front::Task Front::fade(int frames, u16 fromSegment, u16 from, u16 toSegment, u16 to, int bytes) {
  // cs:200e, cs:209a: each colour part way between the two, by how many frames are left
  for (int left = frames; left > 0; --left) {
    for (int i = 0; i < bytes; ++i) {
      const unsigned a = farByte(fromSegment, static_cast<u32>(from + i)) * static_cast<unsigned>((frames - left) & 0xff);
      const unsigned b = farByte(toSegment, static_cast<u32>(to + i)) * static_cast<unsigned>(left);
      fadeBuffer_[static_cast<std::size_t>(i)] = static_cast<u8>(((a + b) & 0xffff) / static_cast<unsigned>(frames));
    }
    co_await nextFrame();
    setDac(0, fadeBuffer_.data(), bytes);
    // (0x527d is the black the slides fade from and to, and 0x557d the white the second and
    // third come out of)
    const bool toBlack = fromSegment == 0x80 && (from == 0x527d || from == 0x557d);
    const bool fromBlack = toSegment == 0x80 && (to == 0x527d || to == 0x557d);
    fadeWhite_ = (toSegment == 0x80 && to == 0x557d) || (fromSegment == 0x80 && from == 0x557d);
    level_ = toBlack && fromBlack ? 0.0f : toBlack ? static_cast<float>(left) / static_cast<float>(frames)
                                         : static_cast<float>(frames - left) / static_cast<float>(frames);
  }
}

Front::Task Front::waitTicks(u32 ticks) {
  for (;;) {
    if (music_.ticks() > ticks) break;
    // Space: no more slides. (The original lets go of any other key pressed here; this version
    // keeps it, so that a table asked for early is opened as soon as the menu is there.)
    if (key_ == 0x39) {
      takeKey();
      skip_ = true;
      break;
    }
    co_await nextFrame();
  }
}

/// cs:1e18 and cs:1d49: everything fades for eighty frames, the music with it, and the
/// program ends: for a table, or for good.
Front::Task Front::leave(int table) {
  for (int left = 0x50; left > 0; --left) {
    for (int i = 0; i < 0xc0; ++i) {
      const unsigned a = ds(static_cast<u16>(0x527d + i)) * static_cast<unsigned>(0x50 - left);
      const unsigned b = ds(static_cast<u16>(0x58ba + i)) * static_cast<unsigned>(left);
      fadeBuffer_[static_cast<std::size_t>(i)] = static_cast<u8>(((a + b) & 0xffff) / 0x50u);
    }
    co_await nextFrame();
    setDac(0, fadeBuffer_.data(), 0xc0);
    level_ = static_cast<float>(left) / 80.0f;
    const u16 volume = static_cast<u16>((left << 8) / 0x50);
    if ((volume & 0xfff0) != lastVolume_) {
      lastVolume_ = volume & 0xfff0;
      music_.volume(volume);
    }
  }
  clearVideo();
  action_.kind = table < 0 ? Action::Kind::Quit : Action::Kind::OpenTable;
  action_.table = table < 0 ? 0 : table;
}

Front::Task Front::slides() {
  setStart(0);
  slide_ = 1;
  level_ = 0;
  co_await waitTicks(1);
  co_await fade(0x14, 0x80, dsw(0x60b), 0x80, 0x527d, 0xc0);
  co_await waitTicks(0x12e);
  co_await fade(0x14, 0x80, 0x527d, 0x80, dsw(0x60b), 0xc0);
  if (!skip_) {
    setStart(0x4d30);
    slide_ = 2;
    co_await fade(0x0a, 0x80, dsw(0x60d), 0x80, 0x557d, 0x300);
    co_await waitTicks(0x26c);
    co_await fade(0x14, 0x80, 0x527d, 0x80, dsw(0x60d), 0x300);
  }
  if (!skip_) {
    co_await fade(1, 0x80, 0x527d, 0x80, 0x527d, 0x300);
    setStart(0x99c0);
    slide_ = 3;
    co_await fade(0x0a, 0x11c2, dsw(0x5279), 0x80, 0x557d, 0x300);
    co_await waitTicks(0x37b);
    co_await fade(0x14, 0x80, 0x527d, 0x11c2, dsw(0x5279), 0x300);
  }
  if (!skip_) {
    setStart(0);
    for (auto& plane : planes_) std::fill(plane.begin(), plane.begin() + 0x4b00, u8{0});  // (0x2580 words: the screen's 240 rows)
    fadeBuffer_.fill(0);
    setDac(0, fadeBuffer_.data(), 0x300);
    slide_ = 4;
    const u16 colours = unpackChunky(0x10a6, 0, 0x2f40);
    setw(0x527b, colours);
    co_await fade(0x08, 0x10a6, colours, 0x80, 0x527d, 0xc0);
    co_await waitTicks(0);
    co_await fade(0x14, 0x80, 0x527d, 0x10a6, colours, 0xc0);
  }
  if (!skip_) {
    clearVideo();
    mode_ = Mode::Planar480;  // (the BIOS's mode 12h)
    start_ = 0;
    selectBits_ = false;
    co_await fade(1, 0x80, 0x527d, 0x80, 0x527d, 0xc0);
    slide_ = 5;
    const u16 colours = unpackPlanar(0x0a05, 0x96, false);
    setw(0x5275, colours);
    co_await fade(0x14, 0x0a05, colours, 0x80, 0x527d, 0xc0);
    co_await waitTicks(0x5dc);
    co_await fade(0x14, 0x80, 0x527d, 0x0a05, colours, 0xc0);
  }
  if (skip_) {  // cs:09bd
    co_await fade(1, 0x80, 0x527d, 0x80, 0x527d, 0xc0);
    co_await fade(1, 0x80, 0x527d, 0x80, 0x527d, 0xc0);
  }
  slide_ = 0;
  co_await nextFrame();
  mode_ = Mode::Planar240;
  co_await nextFrame();
  fadeBuffer_.fill(0);
  setDac(0, fadeBuffer_.data(), 0x300);
}

Front::Task Front::banners(u16 which) {
  for (u16 at = 0x597a;;) {
    const u8 row = ds(at);
    if (row != 0xff) {
      copy(static_cast<u16>(0x334 + row * 0x50), static_cast<u16>(0x5c80 + which + row * 0x37), 0x37);  // cs:260a
      const u8 other = static_cast<u8>(0x5e - row);                                                      // cs:262c
      bannerTop_[row % 96] = bannerBottom_[other % 96] = true;
      copy(static_cast<u16>(0x2a44 + other * 0x50), static_cast<u16>(0x70e9 + which + other * 0x37), 0x37);
      ++at;
      continue;
    }
    if (ds(++at) == 0xff) break;
    co_await nextFrame();
  }
}

Front::Task Front::rubOutBanners() {
  peek(0x13);
  for (u16 at = 0x597a;;) {
    const u8 row = ds(at);
    if (row != 0xff) {
      bannerTop_[static_cast<u8>(0x5e - row) % 96] = bannerBottom_[row % 96] = false;
      fill(static_cast<u16>(0x334 + static_cast<u8>(0x5e - row) * 0x50), 0x37);  // cs:2654
      fill(static_cast<u16>(0x2a44 + row * 0x50), 0x37);                         // cs:266f
      ++at;
      continue;
    }
    if (ds(++at) == 0xff) break;
    co_await nextFrame();
  }
}

Front::Task Front::closePage() {
  setStart(kPage1);
  for (int n = 0; n < 0x14; ++n) {
    threeColours(0x13 - n, 0x14);
    co_await nextFrame();
  }
  rubOutText(true);
  setStart(0);
  splitPalette_ = true;
}

/// cs:3218: a page of text in place of the banners: the best scores of the two tables just
/// shown, or the next of the pages about the game.
Front::Task Front::page() {
  co_await rubOutBanners();
  writeMode_ = 0;
  sendColours(0, 6);
  splitPalette_ = false;
  colourSelect_ = 0;
  u16 words = 0;
  bool scores = false;
  for (int pass = 0; pass < 2; ++pass) {
    for (;;) {
      const u16 at = pageAt_;
      pageAt_ = static_cast<u16>(pageAt_ + 2);
      words = dsw(static_cast<u16>(at + 0x4c01));
      if (words == 0xffff) words = dsw(0x4c01);
      if (words != 0) break;
      pageAt_ = 2;
    }
    scores = dsw(static_cast<u16>(pageAt_ + 0x4bff)) == 0xffff;
    // (the tall screen has all four tables' scores on one page: the other pair's page, which
    // would come next, is passed over)
    if (!(tall() && scores && lastWasScores_)) break;
  }
  lastWasScores_ = scores;
  showing_ = scores ? Showing::Scores : Showing::Page;
  drawAt_ = kPage1;
  fontAt_ = 0x8c0;
  if (scores) {  // cs:13cb
    heading(0x09df, 0xae3b);
    drawAt_ = static_cast<u16>(drawAt_ + 0x320);
  }
  writeMode_ = 0;
  text(0x0e, words);
  for (int n = 0; n < 5; ++n) {
    threeColours(4 - n, 5);
    co_await nextFrame();
  }
  setStart(kPage1);
  co_await nextFrame();
  drawAt_ = 0;
  fontAt_ = 0;
  if (scores) {  // cs:13ac
    heading(0x291f, 0x17);
    drawAt_ = static_cast<u16>(drawAt_ + 0x320);
  }
  text(0x0e, words);
  for (int left = 0x14; left > 0; --left) {
    threeColours(0x14 - left, 0x14);
    co_await nextFrame();
  }
  setStart(0);
  wantOptions_ = false;
  launch_ = -1;
  for (int left = 0x1a4; left > 0; --left) {
    co_await nextFrame();
    const u8 k = takeKey();
    if (k == 0x01 || k == 0x1c || k == 0x39) break;
    if (k >= 0x3b && k <= 0x3e) {
      launch_ = k - 0x3b;
      break;
    }
    if (k == 0x3f) {
      wantOptions_ = true;
      scrollerText_ = 0x2906;
      break;
    }
  }
  co_await closePage();
  showing_ = Showing::Banners;
}

void Front::optionText(int row, u16& words) {
  switch (row) {
    case 0: words = ds(0x49a3) ? 0x4df8 : 0x4df6; break;
    case 1: words = ds(0x49a4) == 1 ? 0x4e08 : ds(0x49a4) == 2 ? kHigher : 0x4e01; break;
    case 2: words = dsw(static_cast<u16>(0x4e24 + ds(0x49a5) * 2)); break;
    case 3: words = ds(0x49a6) ? 0x4e2e : 0x4e2a; break;
    case 4: words = ds(0x49a7) == 1 ? 0x4e01 : ds(0x49a7) == 2 ? kFull : ds(0x49a7) == 3 ? kTall : 0x4dfa; break;
    case 5: words = ds(0x49a8) ? 0x4e38 : 0x4e32; break;
    // this version's two
    case 6: words = options_.originalPictures ? kOriginal : kRemaster; break;
    case 7: words = options_.originalSound ? kOriginal : kBalanced; break;
    default: words = options_.dotMatrixTop ? kTop : kBottom; break;
  }
}

void Front::changeOption(int row) {
  switch (row) {
    case 0: ds(0x49a3) ^= 1; break;
    // (the original has high and low; this version's steeper one comes after high)
    case 1: ds(0x49a4) = ds(0x49a4) == 0 ? 2 : ds(0x49a4) == 2 ? 1 : 0; break;
    case 2: ds(0x49a5) = ds(0x49a5) >= 2 ? 0 : static_cast<u8>(ds(0x49a5) + 1); break;
    case 3: ds(0x49a6) ^= 1; break;
    // (and after the original's two sizes of screen, this version's whole table, twice)
    case 4: ds(0x49a7) = ds(0x49a7) >= 3 ? 0 : static_cast<u8>(ds(0x49a7) + 1); break;
    case 5: ds(0x49a8) ^= 1; break;
    case 6: options_.originalPictures = !options_.originalPictures; break;
    case 7: options_.originalSound = !options_.originalSound; break;
    case 8: options_.dotMatrixTop = !options_.dotMatrixTop; break;
    default: optionsDone_ = true; break;
  }
  options_.balls = ds(0x49a3) ? 5 : 3;
  options_.angle = ds(0x49a4) == 1 ? Angle::Low : ds(0x49a4) == 2 ? Angle::Higher : Angle::High;
  options_.scrollSpeed = static_cast<ScrollSpeed>(ds(0x49a5));
  options_.noMusic = ds(0x49a6) != 0;
  options_.resolution = static_cast<Resolution>(ds(0x49a7));
  options_.mono = ds(0x49a8) != 0;
}

/// cs:42de: a word written on both pages, each while the other is shown.
Front::Task Front::say(u16 y, u16 x, u16 words) {
  co_await nextFrame();
  setStart(0);
  co_await nextFrame();
  drawAt_ = kPage1;
  fontAt_ = 0;
  string(y, x, words);
  co_await nextFrame();
  setStart(kPage1);
  co_await nextFrame();
  drawAt_ = 0;
  string(y, x, words);
}

/// cs:3f4d: up and down among the options, enter or space to change one. Under a page's options
/// (this version's) a line to turn to the other page, then saving.
Front::Task Front::chooseOptions() {
  optionsDone_ = false;
  optionRow_ = 0;
  for (bool moved = true;;) {
    // the page's options, among all of them, and the rows: theirs, turning, saving
    const int first = optionsShown_ ? kFirstOptions : 0, count = optionsShown_ ? kOptions - kFirstOptions : kFirstOptions;
    const auto rowY = [&](int row) { return static_cast<u16>((row < count ? row : row + 1) * 0x12 + 0x32); };  // (the empty line before turning)
    if (moved) {  // cs:3f5a: the mark beside the row
      const u16 y = rowY(optionRow_);
      writeMode(1);
      peek(0x13);
      for (const u16 at0 : {u16{0x0fb5}, u16{0xbdd9}}) {
        u16 at = at0;
        for (int row = 0; row < 0x8c + 0x12; ++row, at = static_cast<u16>(at + 80)) fill(at, 3);  // (a row more)
      }
      writeMode_ = 0;
      co_await say(y, 0xaf, 0x4e3e);
      moved = false;
    }
    if (optionsDone_) co_return;
    u8 k = 0;
    do {
      k = takeKey();
      co_await nextFrame();
    } while (k == 0);
    if (k == 0x01) co_return;
    if (k == 0x50) {
      optionRow_ = optionRow_ >= count + 1 ? 0 : optionRow_ + 1;
      moved = true;
    } else if (k == 0x48) {
      optionRow_ = optionRow_ <= 0 ? count + 1 : optionRow_ - 1;
      moved = true;
    } else if (k == 0x39 || k == 0x1c) {
      if (optionRow_ == count) {  // the other page: the mark on its first option, or back on turning
        co_await turnOptions(1 - optionsShown_);
        optionRow_ = optionsShown_ ? 0 : kFirstOptions;
        moved = true;
        continue;
      }
      const int row = optionRow_ < count ? first + optionRow_ : kOptions;
      changeOption(row);
      if (row < kOptions) {  // cs:4101: the new word into the page's text, and onto the screen
        u16 words = 0;
        optionText(row, words);
        u16 length = 0;
        for (u8* at = optionWord(row); ds(static_cast<u16>(words + length)) != 0; ++length)
          at[length] = ds(static_cast<u16>(words + length));
        const u16 x = 0x12 * 0x0d + 0xda;  // (cs:4155; its row times 0x2d00 is of a row already lost)
        const u16 width = static_cast<u16>(((length * 0x12) >> 3) + 2);
        const u16 y = rowY(optionRow_);
        writeMode(1);
        peek(0x13);
        for (const u16 page : {u16{0}, kPage1}) {
          u16 at = static_cast<u16>(y * 80 + (x >> 3) + page);
          for (int r = 0; r < 14; ++r, at = static_cast<u16>(at + 80)) fill(at, width);
        }
        writeMode_ = 0;
        co_await say(y, x, words);
      }
    }
  }
}

/// This version's: the other page of options written in place of the one shown, on each page of
/// the card while the other is shown, as a word is (cs:42de).
Front::Task Front::turnOptions(int page) {
  optionsShown_ = page;
  for (const auto& [shown, at] : {std::pair{u16{0}, kPage1}, std::pair{kPage1, u16{0}}}) {
    co_await nextFrame();
    setStart(shown);
    co_await nextFrame();
    writeMode(1);
    peek(0x13);
    u16 to = static_cast<u16>(at + 0x14);
    for (int row = 0; row < 0xf0; ++row, to = static_cast<u16>(to + 80)) fill(to, 0x37);  // (as cs:26b1)
    writeMode_ = 0;
    drawAt_ = at;
    fontAt_ = 0;
    text(0x0e, optionsPages_[static_cast<std::size_t>(page)].data());
  }
  drawAt_ = 0;
}

/// cs:43f4: the options, in place of the banners.
Front::Task Front::optionsMenu() {
  optionsShown_ = 0;
  scrollerText_ = 0x2906;
  co_await rubOutBanners();
  showing_ = Showing::Page;
  writeMode_ = 0;
  sendColours(0, 6);
  splitPalette_ = false;
  colourSelect_ = 0;
  drawAt_ = kPage1;
  fontAt_ = 0x8c0;
  text(0x0e, optionsPages_[static_cast<std::size_t>(optionsShown_)].data());
  for (int n = 0; n < 5; ++n) {
    threeColours(4 - n, 5);
    co_await nextFrame();
  }
  setStart(kPage1);
  co_await nextFrame();
  drawAt_ = 0;
  fontAt_ = 0;
  text(0x0e, optionsPages_[static_cast<std::size_t>(optionsShown_)].data());
  co_await nextFrame();
  for (int left = 0x28; left > 0; --left) {
    threeColours(0x28 - left, 0x28);
    co_await nextFrame();
  }
  setStart(0);
  co_await nextFrame();
  writeMode(1);
  peek(0x13);
  for (u16 at = static_cast<u16>(kPage1 + 0x14), row = 0; row < 0xf0; ++row, at = static_cast<u16>(at + 80)) fill(at, 0x37);  // cs:26b1
  writeMode_ = 0;
  drawAt_ = kPage1;
  fontAt_ = 0;
  text(0x0e, optionsPages_[static_cast<std::size_t>(optionsShown_)].data());

  co_await chooseOptions();

  scrollerText_ = 0x288e;
  co_await nextFrame();
  setStart(0);
  co_await nextFrame();
  writeMode(1);
  peek(0x13);
  for (u16 at = static_cast<u16>(kPage1 + 0x14), row = 0; row < 0xf0; ++row, at = static_cast<u16>(at + 80)) fill(at, 0x37);
  writeMode_ = 0;
  drawAt_ = kPage1;
  fontAt_ = 0x8c0;
  text(0x0e, optionsPages_[static_cast<std::size_t>(optionsShown_)].data());
  text(0x0e, optionsPages_[static_cast<std::size_t>(optionsShown_)].data());
  setStart(kPage1);
  for (int n = 0; n < 0x28; ++n) {
    threeColours(0x27 - n, 0x28);
    co_await nextFrame();
  }
  rubOutText(true);
  setStart(0);
  splitPalette_ = true;
  showing_ = Showing::Banners;
  if (options_.balls != saved_.balls || options_.angle != saved_.angle || options_.scrollSpeed != saved_.scrollSpeed ||
      options_.resolution != saved_.resolution || options_.noMusic != saved_.noMusic || options_.mono != saved_.mono ||
      options_.originalSound != saved_.originalSound || options_.originalPictures != saved_.originalPictures ||
      options_.dotMatrixTop != saved_.dotMatrixTop) {
    saved_ = options_;
    action_.kind = Action::Kind::SaveOptions;
  }
}

Front::Task Front::menu() {
  clearVideo();
  mode_ = Mode::Planar240;
  co_await fade(3, 0x80, 0x527d, 0x80, 0x527d, 0xc0);
  co_await nextFrame();
  selectBits_ = true;
  // two headings, each unpacked to the top of the screen and kept (cs:1377)
  for (const auto& [segment, keep] : {std::pair<u16, u16>{0x3301, 0x09df}, {0x3499, 0x291f}}) {
    unpackPlanar(segment, 0, false);
    u16 to = keep;
    for (auto& plane : planes_) {
      u16 at = 0;
      for (int row = 0; row < 40; ++row, at = static_cast<u16>(at + 0x1e))
        for (int b = 0; b < 50; ++b) ds(to++) = plane[at++];
    }
  }
  openWidth_ = 0x82;
  co_await nextFrame();
  for (u16 i = 0; i < 14; i = static_cast<u16>(i + 2))  // cs:1449: the menu's picture, its letters, the four banners
    setw(static_cast<u16>(0x58ac + i), unpackPlanar(dsw(static_cast<u16>(0x589e + i)), dsw(static_cast<u16>(0x5890 + i)), false,
                                                    i == 0 ? HdPicture::Left : i >= 6 ? static_cast<HdPicture>(static_cast<int>(HdPicture::Table1) + (i - 6) / 2)
                                                                                      : HdPicture::None));
  writeMode(1);
  {  // cs:2d3a: the banners' rows, 55 bytes of each, moved up against each other
    u16 from = 0x5cd0, to = 0x5cb7;
    for (int row = 0; row < 0x17b; ++row, from = static_cast<u16>(from + 0x50), to = static_cast<u16>(to + 0x37)) copy(to, from, 0x37);
  }
  copy(kPage1, 0, 0x4b00);  // cs:2d74: the second page
  {                         // cs:2b8a: what is under the panel's text
    u16 from = 0x1db2, to = 0xfb14;
    for (int row = 0; row < 0x5a; ++row, from = static_cast<u16>(from + 0x50), to = static_cast<u16>(to + 0x0e)) copy(to, from, 0x0e);
  }
  setStart(0x10);
  co_await fade(3, 0x80, 0x527d, 0x80, 0x527d, 0xc0);
  auto keepColours = [this](u16 top, u16 bottom) {  // for the fade when the menu is left
    for (u16 i = 0; i < 0x30; ++i) {
      ds(static_cast<u16>(0x58ea + i)) = farByte(dsw(static_cast<u16>(0x589e + top)), static_cast<u32>(dsw(static_cast<u16>(0x58ac + top)) + i));
      ds(static_cast<u16>(0x58ba + i)) = farByte(dsw(static_cast<u16>(0x589e + bottom)), static_cast<u32>(dsw(static_cast<u16>(0x58ac + bottom)) + i));
    }
  };
  sendColours(0x10, 0x0a);
  sendColours(0x00, 0x0a);
  keepColours(6, 8);
  splitPalette_ = true;
  openWidth_ = 0x82;
  for (int n = 0; n < 4; ++n) co_await nextFrame();
  slideAt_ = 0x10;
  slideIn_ = true;
  for (int n = 0; n < 0x14; ++n) co_await nextFrame();  // (the original lets go of keys here; see waitTicks)
  openWidth_ = 0x140;
  writeMode(1);
  // (cs:15fa to cs:1a23 asks a question out of the manual: not here)
  scrollerOn_ = true;
  co_await rubOutBanners();
  idle_ = 0x21c;

  for (;;) {  // cs:1a55
    for (int n = 0; n < 0x14; ++n) {
      co_await nextFrame();
      if (key_ == 0x39) {
        takeKey();
        break;
      }
    }
    if (!(dsw(0x588e) & 2)) {
      sendColours(0x10, 6);
      sendColours(0x00, 8);
      keepColours(6, 8);
      splitPalette_ = true;
      co_await banners(0);
    } else {
      sendColours(0x10, 0x0a);
      sendColours(0x00, 0x0c);
      keepColours(0x0a, 0x0c);
      splitPalette_ = true;
      co_await banners(0x28d2);
      idle_ = 0x21c;
    }
    enum class Next { Banners, Page } next = Next::Banners;
    for (;;) {  // cs:1b3a
      co_await nextFrame();
      const u8 k = takeKey();
      if (k == 0x01) {
        co_await leave(-1);
        co_return;
      }
      if (k >= 0x3b && k <= 0x3e) {
        co_await leave(k - 0x3b);
        co_return;
      }
      if (k == 0x3f) {
        co_await optionsMenu();
        break;
      }
      if (k == 0x1c) {  // cs:1ba7
        setw(0x588e, dsw(0x588e) ^ 2);
        pageAt_ = static_cast<u16>(pageAt_ + 2);
        const bool none = dsw(pageAt_) == 0;
        if (none) pageAt_ = 0x4c03;
        setw(0x4c01, none ? 0x4efc : 0x501c);
        co_await rubOutBanners();
        idle_ = 0x21c;
        break;
      }
      if (k == 0x39 || --idle_ == 0) {  // cs:1bde
        setw(0x588e, dsw(0x588e) ^ 2);
        setw(0x4c01, dsw(0x588e) == 0 ? 0x501c : 0x4efc);
        idle_ = 0x21c;
        next = Next::Page;
        break;
      }
    }
    if (next == Next::Page) {  // cs:1a3b
      scrollerOn_ = true;
      co_await page();
      if (launch_ >= 0) {
        co_await leave(launch_);
        co_return;
      }
      if (wantOptions_) {
        wantOptions_ = false;
        co_await optionsMenu();
      }
    }
  }
}

Front::Task Front::main() {
  // cs:23df: the first three slides, two pictures each, into one tall screen
  setw(0x5271, unpackChunky(0x3b41, 0x000, 0x2f40));
  for (u16 i = 0; i < 0x300; ++i) ds(i) = farByte(0x3b41, static_cast<u32>(dsw(0x5271) + i));
  unpackChunky(0x4285, 0x08b, 0x2f40);
  setw(0x5273, unpackChunky(0x4d6a, 0x0f0, 0x3e80));
  for (u16 i = 0; i < 0x300; ++i) ds(static_cast<u16>(0x300 + i)) = farByte(0x4d6a, static_cast<u32>(dsw(0x5273) + i));
  unpackChunky(0x4653, 0x16d, 0x2f40);
  setw(0x5279, unpackChunky(0x11c2, 0x1ec, 0x3e80));
  unpackChunky(0x17b2, 0x25a, 0x2f40);

  setw(0x588e, 0);
  pageAt_ = 2;
  if (returningFrom_ >= 2) {  // back from one of the last two tables: theirs are the banners shown
    setw(0x588e, 2);
    pageAt_ = 4;
    setw(0x4c01, 0x501c);
  }
  if (returningFrom_ < 0) {
    co_await slides();
  } else {
    co_await nextFrame();
    mode_ = Mode::Planar240;
    co_await nextFrame();
  }
  co_await menu();
}

// ---------------------------------------------------------------------------------------
// the screen
// ---------------------------------------------------------------------------------------
void Front::draw(u8* frame, Rgb* colours, HdFrame* hd) const {
  if (hd) {
    hd->reset(kWidth, height());
    hd->fade.fill(level_);
    hd->fade[static_cast<std::size_t>(HdPicture::HiScores)] = level_ * textLevel_;
    hd->fadeColor = fadeWhite_ ? Rgb{0xff, 0xff, 0xff} : Rgb{};
  }
  if (!tall()) {
    drawScreen(frame, hd, 0);
  } else if (mode_ != Mode::Planar240) {  // a slide: in the middle
    std::fill_n(frame, static_cast<std::size_t>(kWidth) * static_cast<std::size_t>(height()), u8{0});
    drawScreen(frame, hd, kHeight / 2);
  } else {
    drawTallMenu(frame, hd);
  }
  auto wide = [](u8 v) { return static_cast<u8>((v << 2) | (v >> 4)); };
  for (std::size_t i = 0; i < 256; ++i) colours[i] = Rgb{wide(dac_[i * 3]), wide(dac_[i * 3 + 1]), wide(dac_[i * 3 + 2])};
  if (tall() && mode_ == Mode::Planar240) {
    // the four banners' own colours, a set of sixteen each from 0x40 on, as bright as the screen is
    for (std::size_t t = 0; t < 4; ++t) {
      const u16 picture = static_cast<u16>(6 + t * 2);
      const u16 segment = const_cast<Front*>(this)->dsw(static_cast<u16>(0x589e + picture));
      const u16 at = const_cast<Front*>(this)->dsw(static_cast<u16>(0x58ac + picture));
      for (std::size_t i = 0; i < 16; ++i) {
        auto part = [&](std::size_t n) {
          const u8 v = const_cast<Front*>(this)->farByte(segment, static_cast<u32>(at + i * 3 + n)) & 0x3f;
          return static_cast<u8>(static_cast<float>(wide(v)) * level_);
        };
        colours[0x40 + t * 16 + i] = Rgb{part(0), part(1), part(2)};
      }
    }
  }
}

void Front::drawScreen(u8* frame, HdFrame* hd, int top) const {
  auto mark = [&](int x, int y, HdPicture is, u32 across8, u32 down8, u16 halves, u16 width, u16 height) {
    const auto id = static_cast<std::size_t>(is);
    hd->map[static_cast<std::size_t>(y + top) * kWidth + static_cast<std::size_t>(x)] =
        HdPixel{static_cast<u16>(across8), static_cast<u16>(down8), static_cast<u16>(id | halves), 0};
    hd->used |= 1u << id;
    hd->size[id] = {width, height};
  };
  for (int y = 0; y < kHeight; ++y) {
    u8* out = frame + static_cast<std::size_t>(y + top) * kWidth;
    if (mode_ == Mode::Chunky320) {
      const u16 row = static_cast<u16>(start_ + (y / 2) * 80);
      for (int x = 0; x < kWidth; ++x) {
        out[x] = planes_[static_cast<std::size_t>((x / 2) & 3)][static_cast<u16>(row + x / 8)];
        if (hd && slide_ >= 1 && slide_ <= 4)
          mark(x, y, static_cast<HdPicture>(slide_), static_cast<u32>(x * 4), static_cast<u32>(y * 4), HdFrame::kHalfX | HdFrame::kHalfY, 320, 240);
      }
      continue;
    }
    const u16 row = static_cast<u16>(start_ + (mode_ == Mode::Planar240 ? y / 2 : y) * 80);
    // (the driver's callback part way down the screen is at line 230 of the 480)
    const u8 upper = selectBits_ ? static_cast<u8>(((y < 0xe6 ? selectTop_ : selectBottom_) & 3) << 4) : 0;
    for (int x = 0; x < kWidth; ++x) {
      if (x >= shownWidth_) {
        out[x] = 0;
        continue;
      }
      const u16 at = static_cast<u16>(row + x / 8);
      const int bit = 7 - (x & 7);
      const int colour = ((planes_[0][at] >> bit) & 1) | (((planes_[1][at] >> bit) & 1) << 1) | (((planes_[2][at] >> bit) & 1) << 2) |
                         (((planes_[3][at] >> bit) & 1) << 3);
      out[x] = static_cast<u8>(upper | colour);
      if (!hd) continue;
      if (mode_ == Mode::Planar480) {
        if (slide_ == 5) mark(x, y, HdPicture::Slide5, static_cast<u32>(x * 8), static_cast<u32>(y * 8), 0, 640, 480);
        continue;
      }
      const u32 dot = from_[std::size_t{at} * 8 + static_cast<std::size_t>(x & 7)];
      if (dot == 0) continue;
      const auto is = static_cast<HdPicture>(dot >> 20);
      const bool banner = is >= HdPicture::Table1 && is <= HdPicture::Table4;
      mark(x, y, is, ((dot >> 10) & 0x3ff) * 8, (dot & 0x3ff) * 8 + static_cast<u32>(y & 1) * 4, HdFrame::kHalfY,
           is == HdPicture::Left ? 130 : banner ? 440 : 400, is == HdPicture::Left ? 240 : banner ? 95 : 40);
    }
  }
}

/// The menu down a screen twice as tall. Its rows are counted as the card counts the menu's,
/// each shown twice: 480 of them. The panel on the left is drawn out to that length (its top,
/// a stretch of its plain middle, its foot), with its picture and its text both there all the
/// time; beside it are all four banners, or all four lists of best scores, or the page the
/// card has, set in the middle.
void Front::drawTallMenu(u8* frame, HdFrame* hd) const {
  Front& self = *const_cast<Front*>(this);
  auto mark = [&](int x, int y, HdPicture is, u32 across8, u32 down8, u16 width, u16 height, u16 more = 0) {
    const auto id = static_cast<std::size_t>(is);
    hd->map[static_cast<std::size_t>(y) * kWidth + static_cast<std::size_t>(x)] =
        HdPixel{static_cast<u16>(across8), static_cast<u16>(down8), static_cast<u16>(id | HdFrame::kHalfY | more), 0};
    hd->used |= 1u << id;
    hd->size[id] = {width, height};
  };
  auto dots = [&](u16 at, int x) {
    const int bit = 7 - (x & 7);
    return static_cast<u8>(((planes_[0][at] >> bit) & 1) | (((planes_[1][at] >> bit) & 1) << 1) | (((planes_[2][at] >> bit) & 1) << 2) |
                           (((planes_[3][at] >> bit) & 1) << 3));
  };
  auto upper = [&](int row) { return selectBits_ ? static_cast<u8>((((row * 2) < 0xe6 ? selectTop_ : selectBottom_) & 3) << 4) : u8{0}; };
  const bool settled = (start_ == 0 || start_ == kPage1) && shownWidth_ >= 160;
  static constexpr int kBanner[4] = {12, 132, 252, 372};
  for (int y = 0; y < 2 * kHeight; ++y) {
    u8* out = frame + static_cast<std::size_t>(y) * kWidth;
    const int u = y / 2;
    for (int x = 0; x < kWidth; ++x) {
      out[x] = 0;
      if (x >= shownWidth_) continue;
      // which of the card's rows this is a row of, if any
      int row = -1;
      if (x < 160) {
        row = u <= 186 ? u : u <= 427 ? 186 : u - 240;
      } else if (showing_ == Showing::Banners && settled) {
        for (int t = 0; t < 4; ++t) {
          const int r = u - kBanner[t];
          if (r < 0 || r >= 95 || x >= 160 + 440 || !(t < 2 ? bannerTop_ : bannerBottom_)[static_cast<std::size_t>(r)]) continue;
          const int bx = x - 160;
          out[x] = static_cast<u8>(0x40 + t * 16 + dots(static_cast<u16>(0x5c80 + (t * 95 + r) * 0x37 + (bx >> 3)), bx));
          if (hd) mark(x, y, static_cast<HdPicture>(static_cast<int>(HdPicture::Table1) + t), static_cast<u32>(bx * 8), static_cast<u32>(r * 8 + (y & 1) * 4), 440, 95);
        }
        continue;
      } else if (showing_ == Showing::Scores && settled) {
        row = u < 40 ? u : -1;  // the heading; the lists are written below
      } else {
        row = u >= 120 && u < 360 ? u - 120 : -1;
      }
      if (row < 0) continue;
      const u16 at = static_cast<u16>(start_ + row * 80 + x / 8);
      // under the panel's text the picture is kept aside (cs:2b8a): it is shown, not the text
      const bool kept = settled && x >= 16 && x < 128 && row >= 95 && row < 185;
      const u8 colour = kept ? dots(static_cast<u16>(0xfb14 + (row - 95) * 14 + (x >> 3) - 2), x) : dots(at, x);
      const u32 dot = kept ? (static_cast<u32>(HdPicture::Left) << 20) | (static_cast<u32>(x) << 10) | static_cast<u32>(row)
                           : from_[std::size_t{at} * 8 + static_cast<std::size_t>(x & 7)];
      if (showing_ == Showing::Scores && settled && x >= 160 && static_cast<HdPicture>(dot >> 20) != HdPicture::HiScores) continue;
      out[x] = static_cast<u8>(upper(row) | colour);
      if (!hd || dot == 0) continue;
      const auto is = static_cast<HdPicture>(dot >> 20);
      const bool banner = is >= HdPicture::Table1 && is <= HdPicture::Table4;
      // (the panel's plain stretch is one line of its picture all the way down)
      const bool drawnOut = x < 160 && u > 186 && u <= 427;
      if (drawnOut && is == HdPicture::Left && stripHeight_ > 0) {
        // the strip is as fine as the panel's picture: so many of its rows to a row of the screen
        const int perRow8 = stripWidth_ * 4 / 130, down = y - 187 * 2;
        mark(x, y, HdPicture::LeftRepeat, ((dot >> 10) & 0x3ff) * 8, static_cast<u32>((down * perRow8) % (stripHeight_ * 8)), 130,
             static_cast<u16>(stripHeight_));
        hd->map[static_cast<std::size_t>(y) * kWidth + static_cast<std::size_t>(x)].picture &= static_cast<u16>(~HdFrame::kHalfY);
        hd->rowStep[static_cast<std::size_t>(HdPicture::LeftRepeat)] = static_cast<float>(perRow8) / 8.0f;
        continue;
      }
      mark(x, y, is, ((dot >> 10) & 0x3ff) * 8, (dot & 0x3ff) * 8 + (drawnOut ? 4 : static_cast<u32>(y & 1) * 4),
           is == HdPicture::Left ? 130 : banner ? 440 : 400, is == HdPicture::Left ? 240 : banner ? 95 : 40, drawnOut ? HdFrame::kFlatY : u16{0});
    }
  }
  auto put = [&](int x, int u, u8 index) {
    if (x < 0 || x >= kWidth || x >= shownWidth_ || u < 0 || u >= kHeight) return;
    for (int half = 0; half < 2; ++half) {
      const std::size_t at = static_cast<std::size_t>(u * 2 + half) * kWidth + static_cast<std::size_t>(x);
      frame[at] = index;
      if (hd) hd->map[at].picture = 0;
    }
  };
  if (settled) {
    // the panel's text, on the plain stretch below its picture (ten lines of twelve letters)
    for (int line = 0; line < 10; ++line)
      for (int column = 0; column < 12; ++column) {
        const u8 letter = cs(static_cast<u16>(scrollerText_ + 12 * line + column));
        for (int r = 0; r < 8; ++r) {
          const u8 rowDots = romRow(letter, r);
          for (int b = 0; b < 8; ++b)
            if (rowDots & (0x80 >> b)) put(16 + column * 8 + b, 257 + line * 9 + r, upper(186));
        }
      }
  }
  if (showing_ == Showing::Scores && settled) {
    // the four tables' best scores: the two pages of them, one under the other (cs:2cf2)
    int y = 24;
    for (const u16 first : {u16{0x4efc}, u16{0x501c}}) {
      u16 page = first;
      for (int line = 0; line < 12; ++line, y += 0x12) {
        u16 left = 0;
        for (u16 i = 0; i < 0x18; ++i)
          if (self.ds(static_cast<u16>(page + i)) == 0) {
            left = static_cast<u16>(0x18 - i - 1);
            break;
          }
        int x = 0xa4 + ((0x12 * left) >> 1);
        for (u16 n = static_cast<u16>(0x18 - left); n > 0; --n, x += 0x12) {
          const u16 from = self.dsw(static_cast<u16>(0x59ed + self.ds(page++) * 2));
          if (from == 0xffff) continue;
          for (int r = 0; r < 14; ++r)
            for (int b = 0; b < 24; ++b)
              if (const u8 colour = dots(static_cast<u16>(from + r * 80 + (b >> 3)), b); colour != 0) put(x + b, y + r, colour);
        }
      }
    }
  }
}

}  // namespace encore
