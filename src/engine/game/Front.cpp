#include "engine/game/Front.h"

#include <algorithm>
#include <cstring>
#include <string_view>

#include "core/Error.h"

namespace encore {

using namespace intro;

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

/// The codes of the keys the menu reads, as the launcher's keyboard routine says them (int 65h):
/// the code of a key's going down. F1 to F4 open the four tables, F5 the options.
namespace scan {
constexpr u8 escape = 0x01;
constexpr u8 enter = 0x1c;
constexpr u8 space = 0x39;
constexpr u8 f1 = 0x3b;
constexpr u8 f4 = 0x3e;
constexpr u8 f5 = 0x3f;
constexpr u8 up = 0x48;
constexpr u8 left = 0x4b;
constexpr u8 right = 0x4d;
constexpr u8 down = 0x50;
}  // namespace scan

/// The key as the launcher's keyboard routine says it. (The letters' codes, A to Z, which the
/// menu passes on but never asks for.)
u8 scancode(Key k) {
  static constexpr u8 kLetters[26] = {0x1e, 0x30, 0x2e, 0x20, 0x12, 0x21, 0x22, 0x23, 0x17, 0x24, 0x25, 0x26, 0x32,
                                      0x31, 0x18, 0x19, 0x10, 0x13, 0x1f, 0x14, 0x16, 0x2f, 0x11, 0x2d, 0x15, 0x2c};
  if (k >= Key::A && k <= Key::Z) return kLetters[static_cast<int>(k) - static_cast<int>(Key::A)];
  if (k >= Key::F1 && k <= Key::F8) return static_cast<u8>(scan::f1 + static_cast<int>(k) - static_cast<int>(Key::F1));
  // (this version takes 1 to 5 for F1 to F5 as well)
  if (k >= Key::Digit1 && k <= Key::Digit5) return static_cast<u8>(scan::f1 + static_cast<int>(k) - static_cast<int>(Key::Digit1));
  switch (k) {
    case Key::Space: return scan::space;
    case Key::Enter: return scan::enter;
    case Key::Escape: return scan::escape;
    case Key::ArrowDown: return scan::down;
    case Key::ArrowUp: return scan::up;
    case Key::ArrowLeft: return scan::left;
    case Key::ArrowRight: return scan::right;
    default: return 0;
  }
}

constexpr u16 kPage1 = 0xae24;   ///< where the menu's second page is in the card's memory
constexpr u16 kLqLetters = 0x8c0;  ///< and the letters while a page fades, this far past the others
constexpr u16 kBanners = 0x5c80;   ///< and the four banners, kept there a row of 0x37 bytes after another
constexpr u16 kBannersLower = 0x70e9;  ///< the lower of a pair, a banner's 95 rows on
constexpr u16 kTables34 = 0x28d2;  ///< the pair of tables 3 and 4, two banners on
constexpr u16 kPanelKept = 0xfb14;  ///< the panel's picture under its text, kept (cs:2b8a)
constexpr u8 kTallBanners = 0x40;  ///< this version's colours: the four banners' sixteen each, on the tall screen
constexpr u16 kHigher = 0x49b5;  ///< this version's words among the options, kept where the
constexpr u16 kFull = 0x49c0;    ///< original has a question this version does not ask (and
constexpr u16 kTall = 0x49c8;    ///< the answer's place after it)
constexpr u16 kOriginal = 0x49d0;
constexpr u16 kRemaster = 0x49da;
constexpr u16 kBalanced = 0x49e4;
constexpr u16 kBottom = 0x49ee;  ///< (past the question's words too, which are not shown either)
constexpr u16 kTop = 0x49f5;
/// PORTABLE's words, all of eight letters, since a new word is written over the old to its own
/// length: yes, no, and why it cannot be changed (PortableLock), or that changing it failed.
constexpr u16 kYes = 0x49fc, kNo = 0x4a05, kReadOnly = 0x4a0e, kNoSpace = 0x4a17, kNotHere = 0x4a20, kMoveApp = 0x4a29,
              kError = 0x4a32;
constexpr int kOptions = 10;     ///< the original's six, and ARTWORK, AUDIO, DOT MATRIX and PORTABLE

/// INTRO.PRG as a file: its MZ header, and the program after it, at least so many bytes.
constexpr int kHeaderBytes = 0x400, kImageBytes = 0x4daa0;
/// A picture's part is looked for in its first so many bytes (cs:4998).
constexpr u32 kSearchBytes = 0xc350;

/// The card's memory as the menu lays it out: 64K in each plane, rows of 80 bytes (640 dots).
constexpr int kPlaneBytes = 0x10000;
constexpr int kRowBytes = 80;
constexpr int kRows = 0xf0;           ///< the menu's rows, each shown twice
constexpr int kScreenBytes = 0x4b00;  ///< a screen of them
constexpr u16 kLatchByte = 0x13;  ///< a byte left empty: read, it fills the latches with nothing to copy
constexpr u16 kBeside = 0x14;       ///< the part of a row beside the panel: from this byte (dot 160),
constexpr u16 kBesideBytes = 0x37;  ///< so many bytes of it (440 dots, a banner's width)
constexpr u16 kBannerTop = 0x334, kBannerBottom = 0x2a44;  ///< where the two banners shown are: rows 10 and 135, beside the panel
constexpr u8 kBannerLast = 0x5e;    ///< a banner's last row: it has 95
constexpr int kBannerRows = 95;
constexpr int kBanners4 = 4;        ///< the banners, one for each table
constexpr u16 kPanelText = 0x1db2;  ///< where the panel's text is written: from row 95, byte 2 (dot 16), 14 bytes by 90 rows
constexpr int kPanelTextTop = 95, kPanelTextLeft = 2, kPanelTextBytes = 0x0e, kPanelTextRows = 0x5a;
constexpr u16 kScrollerLetters = 0x1e52;  ///< and its first letter: row 97, byte 2
constexpr int kScrollerLines = 10, kScrollerColumns = 12, kScrollerLineRows = 9;
constexpr int kRomRows = 8;          ///< a ROM letter's rows
constexpr u16 kHeadingAt = 0x17;     ///< a page of best scores' heading: from byte 0x17 of its first row,
constexpr int kHeadingBytes = 50, kHeadingRows = 40;  ///< 50 bytes (400 dots) by 40 rows
constexpr u16 kUnderHeading = 0x320;  ///< its text ten rows lower
constexpr u16 kSlideIn = 0x10;  ///< the menu comes in from the right: the screen starts so many bytes on, and moves back a byte a frame
constexpr u16 kOpenPanel = 0x82, kOpenWhole = 0x140;  ///< how wide the card shows the menu (cs:2888): the panel alone (130 dots), then all of it
constexpr u16 kClockStep = 3;  ///< what cs:288c counts up by each frame (nothing here reads it)
constexpr int kSplitRow = 0xe6;  ///< the driver's callback part way down the screen: at line 230 of the 480
constexpr int kSelectBits = 3, kSelectShift = 4;  ///< the colour select's two bits are the colours' fifth and sixth
constexpr u8 kWarpEnd = 0xff;  ///< in var::warpTable, ends a frame's rows; twice, the table
constexpr u16 kScoresPage = 0xffff;  ///< in var::pages: the best scores of the two tables shown
constexpr int kPair34 = 2;  ///< var::tablePair while the banners are of tables 3 and 4
constexpr int kTable3 = 2;  ///< the third table, counted from nought
constexpr int kTables = 4, kBestScores = 4;  ///< and each table's best scores
constexpr int kSecondName = 7;  ///< a page of best scores: the second table's name's line (the first's is the next after the heading)
constexpr u8 kFirstPlane = 0x11, kBeforeFirstPlane = 0x88;  ///< the plane a picture is unpacked into first: its bit, in both halves to be turned about
constexpr int kColourBytes = 3;
constexpr u16 kVolumeStep = 0xfff0;  ///< the volume is said to the driver when it changes by a sixteenth
constexpr Rgb kWhite{0xff, 0xff, 0xff};

/// The menu's times, in frames (pfr: intro.rs, the counts of its State).
namespace times {
constexpr int black = 3;         ///< of black, before the menu opens and as it does
constexpr int openWait = 4;      ///< the panel alone, before the rest of the menu slides in
constexpr int slideIn = 0x14;
constexpr int bannersGap = 0x14;  ///< pfr: TablesGap
constexpr int banners = 0x21c;   ///< pfr: Tables, before a page is shown in their place
constexpr int pageGap = 5;       ///< pfr: TextGap, as a page's letters are drawn dim
constexpr int pageFade = 0x14;   ///< pfr: TextFadeIn, TextFadeOut
constexpr int page = 0x1a4;      ///< pfr: Text
constexpr int optionsFade = 0x28;  ///< pfr: OptionsFadeIn
constexpr int leave = 0x50;      ///< pfr: TablesFadeOut
}  // namespace times

/// The slides (pfr: assets/intro.rs, a Slide's gap_frames, fade_in_frames, fade_out_frames and
/// fade_out_tick): frames of the fades, and the driver's ticks (fiftieths of a second of the
/// music) after which each fades out. (pfr's presents fades at tick 1.)
namespace slideTimes {
constexpr int fade = 0x14, fadeWhite = 0x0a, fadePresents = 0x08, gap = 1;
constexpr u32 start = 1, logo0 = 0x12e, logo1 = 0x26c, logo2 = 0x37b, presents = 0, pflogo = 0x5dc;
}  // namespace slideTimes

/// The rows the slides' halves are unpacked at, in the card's memory; a half is 151 rows
/// (cs:4a39, kHalfBytes) or 200 (cs:4db0, kTallHalfBytes).
namespace slideRows {
constexpr u16 logo0Upper = 0x000, logo0Lower = 0x08b, logo1Upper = 0x0f0, logo1Lower = 0x16d, logo2Upper = 0x1ec,
              logo2Lower = 0x25a, presents = 0, pflogo = 0x96;
}  // namespace slideRows
constexpr u16 kHalfBytes = 0x2f40, kTallHalfBytes = 0x3e80;
constexpr int kHalvedBytes = 0x60;  ///< the 32 colours made again at half their strength (cs:4aa2)
constexpr int kChunkySlides = 4, kLastSlide = 5;  ///< the slides of 320 by 240, and the last, of 640 by 480
constexpr int kChunkyWidth = 320, kChunkyHeight = 240;

/// The options' page: its first option's line, and the column of marks beside them (cs:3f5a):
/// where it begins in the card's memory (row 50, byte 0x15), and its width and rows.
constexpr u16 kOptionsTop = 0x32;
constexpr u16 kMarkAt = 0x0fb5, kMarkX = 0xaf, kMarkBytes = 3;
constexpr int kMarkRows = 0x8c;
constexpr u16 kWordX = 0x12 * 0x0d + 0xda;  ///< an option's word: the 16th letter of its line (cs:4155)

/// The options' values as PINBALL.CFG has them.
namespace cfg {
constexpr int fewBalls = 3, manyBalls = 5;
constexpr int angleHigh = 0, angleLow = 1, angleHigher = 2;  ///< (the last this version's)
constexpr int scrollHard = 0, scrollSoft = 2;
constexpr int resolutionNormal = 0, resolutionHigh = 1, resolutionFull = 2, resolutionTall = 3;  ///< (the last two this version's)
}  // namespace cfg

/// This version's: which picture's dot a dot of the card's memory is (Front::from_).
namespace dotOf {
constexpr int picture = 20, across = 10;  ///< where they are in the word
constexpr u32 field = 0x3ff;
}  // namespace dotOf

/// This version's tall menu, and the pictures drawn again at high resolution: the panel's
/// picture, a banner and a heading, in dots, and where things are down the tall screen.
constexpr int kPanelDots = 130, kPanelRows = 240, kBannerDots = 440, kHeadingDots = 400;
constexpr int kBesideDots = 160;  ///< where the part beside the panel begins
constexpr int kPanelTop = 186;  ///< the panel's rows shown as they are; then its plain middle,
constexpr int kStretchEnd = 427, kStretch = 240;  ///< drawn out to this row, and the rest of it this far lower
constexpr int kTallPageTop = 120;  ///< a page, in the middle
constexpr int kTallScrollerTop = 257;  ///< the panel's text, on the plain stretch
constexpr int kTallScoresTop = 24;  ///< the four tables' best scores, from here down

/// The places in a picture's chunks (IFF, big-endian), from the end of the chunk's name.
namespace iff {
constexpr u32 lengthHigh = 2u;  ///< CMAP: the low word of the chunk's length, the bytes of colours
constexpr u32 lengthLow = 3u;
constexpr u32 data = 4u;  ///< past the length: what the chunk holds
constexpr u32 widthHigh = 4u;  ///< BMHD: the picture's width in dots
constexpr u32 widthLow = 5u;
constexpr u32 heightHigh = 6u;  ///< and its height
constexpr u32 heightLow = 7u;
/// BODY, packed (ByteRun1): a byte below this, so many bytes and one more as they are; above
/// it, the next byte 257 less it times; this itself, nothing.
constexpr int skip = 0x80;
constexpr u32 tagBytes = 4u;  ///< a part's name, before it
}  // namespace iff

}  // namespace

Front::Front(ByteView prg, ByteView module, const Config& config, int returningFrom, PortableLock portableLock,
             bool portableError)
    : music_(MusicDriver::kOutputRate), options_(config.options), saved_(config.options), returningFrom_(returningFrom),
      portableLock_(portableLock), portableError_(portableError) {
  if (prg.size() < kHeaderBytes + kImageBytes) throw DataError("INTRO.PRG is not the program it should be");
  image_.assign(prg.begin() + kHeaderBytes, prg.end());
  for (auto& plane : planes_) plane.assign(kPlaneBytes, 0);
  from_.assign(kPlaneBytes * 8, 0);
  if (!music_.load(module)) throw DataError("the menu's music is not a module");
  music_.start();

  // cs:407e: the options, as PINBALL.CFG has them
  ds(var::balls) = options_.balls == cfg::manyBalls;
  ds(var::angle) = options_.angle == Angle::Low ? cfg::angleLow : options_.angle == Angle::Higher ? cfg::angleHigher : cfg::angleHigh;
  ds(var::scrollSpeed) = static_cast<u8>(options_.scrollSpeed);
  ds(var::noMusic) = options_.noMusic;
  ds(var::resolution) = static_cast<u8>(options_.resolution);
  ds(var::mono) = options_.mono;
  std::memcpy(&ds(kHigher), "HIGHER", 7);
  std::memcpy(&ds(kFull), "FULL  ", 7);
  std::memcpy(&ds(kTall), "TALL  ", 7);
  std::memcpy(&ds(kOriginal), "ORIGINAL", 9);
  std::memcpy(&ds(kRemaster), "REMASTER", 9);
  std::memcpy(&ds(kBalanced), "BALANCED", 9);
  std::memcpy(&ds(kBottom), "BOTTOM", 7);
  std::memcpy(&ds(kTop), "TOP   ", 7);
  std::memcpy(&ds(kYes), "YES     ", 9);
  std::memcpy(&ds(kNo), "NO      ", 9);
  std::memcpy(&ds(kReadOnly), "READONLY", 9);
  std::memcpy(&ds(kNoSpace), "NO SPACE", 9);
  std::memcpy(&ds(kNotHere), "NOT HERE", 9);
  std::memcpy(&ds(kMoveApp), "MOVE APP", 9);
  std::memcpy(&ds(kError), "ERROR   ", 9);
  // the first page as the original has it, its six options, with a line to turn to the other
  // before saving; the other with a heading, this version's options and a line to turn back.
  // Each has twelve lines, an empty one a nought, as the page shows twelve and no more
  const auto copy = [&](std::vector<u8>& to, u16 from, u16 end) { to.insert(to.end(), &ds(from), &ds(from) + (end - from)); };
  const auto addLine = [](std::vector<u8>& to, std::string text) {
    text.resize(pageText::lineBytes, ' ');
    to.insert(to.end(), text.begin(), text.end());
  };
  std::vector<u8>& firstPage = optionsPages_[0];
  std::vector<u8>& morePage = optionsPages_[1];
  copy(firstPage, var::optionsText, var::optionsSave);  // the heading, an empty line, the six options and an empty line
  addLine(firstPage, "  MORE OPTIONS");
  copy(firstPage, var::optionsSave, var::optionsTextEnd);  // saving, and an empty line
  for (const char c : std::string_view("MORE OPTIONS")) morePage.push_back(static_cast<u8>(c));
  morePage.insert(morePage.end(), {0, 0});
  for (const char* label : {"  ARTWORK:", "  AUDIO:", "  DOT MATRIX:", "  PORTABLE:"}) addLine(morePage, label);
  morePage.push_back(0);
  addLine(morePage, "  BACK");
  copy(morePage, var::optionsSave, var::optionsTextEnd);
  morePage.insert(morePage.end(), {0, 0});

  // cs:371f: the best scores, written into the two pages that show them
  static constexpr u16 kRows[kTables] = {var::scoresTable1, var::scoresTable2, var::scoresTable3, var::scoresTable4};
  for (std::size_t t = 0; t < kTables; ++t)
    for (u16 n = 0; n < kBestScores; ++n) {
      const HighScore& h = config.highScores[t][n];
      const u16 row = static_cast<u16>(kRows[t] + n * pageText::lineBytes);
      for (u16 i = 0; i < scoreLine::initials; ++i) ds(static_cast<u16>(row + i)) = h.name[i];
      bool begun = false;
      for (u16 i = 0; i < kScoreDigits; ++i) {
        begun |= h.score.digits[i] != 0;
        ds(static_cast<u16>(row + scoreLine::score + i)) = begun ? static_cast<u8>(h.score.digits[i] + '0') : ' ';
      }
      if (!begun) ds(static_cast<u16>(row + scoreLine::score + kScoreDigits - 1)) = '0';  // (this version: a score of nought is written)
    }
  // (this version: each table's name in the middle of its line, where the original has them all
  // begin at one place)
  for (const u16 line : {u16{var::scoresTables12 + pageText::lineBytes}, u16{var::scoresTables12 + pageText::lineBytes * kSecondName},
                          u16{var::scoresTables34 + pageText::lineBytes}, u16{var::scoresTables34 + pageText::lineBytes * kSecondName}}) {
    std::string name;
    for (u16 i = 0; i < pageText::lineBytes; ++i) name += static_cast<char>(ds(static_cast<u16>(line + i)));
    const auto first = name.find_first_not_of(' '), last = name.find_last_not_of(' ');
    if (first == std::string::npos) continue;
    name = name.substr(first, last - first + 1);
    const std::size_t before = (pageText::lineBytes - name.size()) / 2;
    for (u16 i = 0; i < pageText::lineBytes; ++i) ds(static_cast<u16>(line + i)) = i >= before && i - before < name.size() ? static_cast<u8>(name[i - before]) : ' ';
  }
  for (int row = 0; row < kOptions; ++row) writeOptionWord(row);
  task_ = main();
}

void Front::writeOptionWord(int row) {
  u16 word = 0;
  optionText(row, word);
  for (u8* at = optionWord(row); ds(word) != 0; ++word, ++at) *at = ds(word);
}

void Front::setLooks(bool originalSound, bool originalPictures, bool dotMatrixTop, bool portable, PortableLock portableLock,
                     bool portableError) {
  options_.originalSound = saved_.originalSound = originalSound;
  options_.originalPictures = saved_.originalPictures = originalPictures;
  options_.dotMatrixTop = saved_.dotMatrixTop = dotMatrixTop;
  options_.portable = saved_.portable = portable;
  portableLock_ = portableLock;
  portableError_ = portableError;
  for (int row = 6; row < kOptions; ++row) writeOptionWord(row);
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
  for (std::size_t p = 0; p < kPlanes; ++p) latch_[p] = planes_[p][at];
  return latch_[readPlane_];
}

void Front::poke(u16 at, u8 value) {
  for (std::size_t p = 0; p < kPlanes; ++p) {
    if (!(planeMask_ & (1 << p))) continue;
    if (writeMode_ == 1) planes_[p][at] = latch_[p];
    else if (enableSetReset_ & (1 << p)) planes_[p][at] = (setReset_ & (1 << p)) ? kAllDots : 0;
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
  if (mode == 1) planeMask_ = kAllPlanes;
}

void Front::setDac(int first, const u8* values, int bytes) {
  for (int i = 0; i < bytes && first * 3 + i < palette::allBytes; ++i) dac_[static_cast<std::size_t>(first * 3 + i)] = values[i] & palette::dacMax;
}

void Front::clearVideo() {
  for (auto& plane : planes_) std::fill(plane.begin(), plane.end(), u8{0});
  std::fill(from_.begin(), from_.end(), u32{0});
}

/// cs:4998: where in a picture a part of it begins, past its four letters.
u16 Front::find(u16 segment, const char* tag) {
  for (u32 at = 0; at < kSearchBytes; ++at)
    if (farByte(segment, at) == static_cast<u8>(tag[0]) && farByte(segment, at + 1) == static_cast<u8>(tag[1]) &&
        farByte(segment, at + 2) == static_cast<u8>(tag[2]) && farByte(segment, at + 3) == static_cast<u8>(tag[3]))
      return static_cast<u16>(at + iff::tagBytes);
  throw DataError(std::string("a picture of the menu's has no ") + tag);
}

/// cs:4880: a picture of sixteen colours, a row of each plane at a time, into the card's
/// memory from a row on. Answers where its colours are, which it leaves as the card takes them.
u16 Front::unpackPlanar(u16 segment, u16 row, bool setColours, HdPicture is) {
  const u16 cmap = find(segment, "CMAP");
  const u16 colours = static_cast<u16>((farByte(segment, cmap + iff::lengthHigh) << 8) | farByte(segment, cmap + iff::lengthLow));
  const u16 palette = static_cast<u16>(cmap + iff::data);
  for (u16 i = 0; i < colours; ++i) farByte(segment, static_cast<u32>(palette + i)) >>= 2;
  if (setColours) setDac(0, &farByte(segment, palette), colours);
  const u16 header = find(segment, "BMHD");
  const u16 width = static_cast<u16>(((farByte(segment, header + iff::widthHigh) << 8) | farByte(segment, header + iff::widthLow)) >> 3);
  const u16 height = static_cast<u16>((farByte(segment, header + iff::heightHigh) << 8) | farByte(segment, header + iff::heightLow));
  const u16 end = static_cast<u16>((row + height) * kRowBytes);
  u32 from = find(segment, "BODY") + iff::data;
  u16 line = static_cast<u16>(row * kRowBytes - kRowBytes);
  u8 mask = kFirstPlane;
  for (;;) {
    const std::size_t plane = mask & 1 ? 0 : mask & 2 ? 1 : mask & 4 ? 2 : 3;
    u16 at = line;
    if (mask & 1) {
      at = static_cast<u16>(at + kRowBytes);
      line = at;
    }
    if (at >= end) break;
    mask = static_cast<u8>((mask << 1) | (mask >> 7));
    for (bool rowDone = false; !rowDone;) {
      const u8 n = farByte(segment, from++);
      if (n == iff::skip) continue;
      if (n < iff::skip) {
        for (int i = 0; i <= n; ++i) planes_[plane][at++] = farByte(segment, from++);
      } else {
        const u8 v = farByte(segment, from++);
        for (int i = 0; i <= 256 - n; ++i) planes_[plane][at++] = v;
      }
      rowDone = width != 0 && at % width == 0;
    }
  }
  // which picture's dots these now are (of the menu's own picture, only the panel on its left)
  const u32 acrossTo = is == HdPicture::Left ? kPanelDots : is == HdPicture::None ? 0 : static_cast<u32>(width * 8);
  for (u32 y = 0; y < height; ++y)
    for (u32 x = 0; x < static_cast<u32>(width * 8); ++x)
      from_[(std::size_t{static_cast<u16>((row + y) * kRowBytes)} * 8 + x) % from_.size()] =
          x < acrossTo ? (static_cast<u32>(is) << dotOf::picture) | (x << dotOf::across) | y : 0;
  return palette;
}

/// cs:4a39 (and cs:4db0, which takes more rows): a picture of 256 colours, a dot to a byte,
/// into the card's memory where four dots share an address, one in each plane.
u16 Front::unpackChunky(u16 segment, u16 row, u16 bytes) {
  find(segment, "PBM ");
  const u16 cmap = find(segment, "CMAP");
  const u16 colours = static_cast<u16>((farByte(segment, cmap + iff::lengthHigh) << 8) | farByte(segment, cmap + iff::lengthLow));
  const u16 palette = static_cast<u16>(cmap + iff::data);
  // (colours 32 to 63 are made here, the first 32 at half their strength: by the one routine
  // only, cs:4aa2, which is the one that takes 151 rows)
  if (bytes == kHalfBytes)
    for (u16 i = 0; i < kHalvedBytes; ++i) farByte(segment, static_cast<u32>(palette + kHalvedBytes + i)) = farByte(segment, static_cast<u32>(palette + i)) >> 1;
  for (u16 i = 0; i < colours; ++i) farByte(segment, static_cast<u32>(palette + i)) >>= 2;
  u32 from = find(segment, "BODY") + iff::data;
  u16 at = static_cast<u16>(row * kRowBytes);
  const u16 end = static_cast<u16>(at + bytes);
  u8 mask = kBeforeFirstPlane;
  auto dot = [&](u8 v) {
    mask = static_cast<u8>((mask << 1) | (mask >> 7));
    planes_[mask & 1 ? 0 : mask & 2 ? 1 : mask & 4 ? 2 : 3][at] = v;
    if (mask & 8) ++at;
  };
  for (;;) {
    const u8 n = farByte(segment, from++);
    if (n == iff::skip) continue;
    if (n < iff::skip) {
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
  setDac(first, &farByte(dsw(static_cast<u16>(var::pictureSegments + picture)), dsw(static_cast<u16>(var::pictureCmaps + picture))), palette::setBytes);
  level_ = 1.0f;
}

/// cs:2c37: a letter of the menu's own, 24 dots by 14, from where the letters are kept in
/// the card's memory, laid over what is there.
void Front::glyph(u16 y, u16 x, u8 letter) {
  u16 from = dsw(static_cast<u16>(var::glyphs + letter * 2));
  if (from == kNone) return;
  from = static_cast<u16>(from + fontAt_);
  const u16 to = static_cast<u16>((x >> 3) + y * kRowBytes + drawAt_);
  const int shift = x & 7;
  for (auto& plane : planes_) {
    u16 s = from, d = to;
    for (int row = 0; row < menuFont::rows; ++row, s = static_cast<u16>(s + kRowBytes), d = static_cast<u16>(d + kRowBytes)) {
      u8 carry = 0;
      for (u16 b = 0; b < menuFont::bytes; ++b) {
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
  for (int line = 0; line < pageText::lines; ++line, y = static_cast<u16>(y + pageText::lineHeight)) {
    u16 left = 0;  // what the original's count has left when it finds the line's end
    for (u16 i = 0; i < pageText::lineBytes; ++i)
      if (page[i] == 0) {
        left = static_cast<u16>(pageText::lineBytes - i - 1);
        break;
      }
    u16 x = static_cast<u16>(pageText::left + ((menuFont::advance * left) >> 1));
    for (u16 n = static_cast<u16>(pageText::lineBytes - left); n > 0; --n, x = static_cast<u16>(x + menuFont::advance)) glyph(y, x, *page++);
  }
}

void Front::string(u16 y, u16 x, u16 words) {
  for (; ds(words) != 0; ++words, x = static_cast<u16>(x + menuFont::advance)) glyph(y, x, ds(words));
}

/// cs:13ea: the heading kept from a picture (50 bytes by 40 rows of each plane) put back.
void Front::heading(u16 from, u16 to) {
  for (auto& plane : planes_) {
    u16 at = to;
    for (int row = 0; row < kHeadingRows; ++row, at = static_cast<u16>(at + (kRowBytes - kHeadingBytes)))
      for (int b = 0; b < kHeadingBytes; ++b) plane[at++] = ds(from++);
  }
  for (u32 y = 0; y < kHeadingRows; ++y)
    for (u32 x = 0; x < kHeadingDots; ++x)
      from_[std::size_t{static_cast<u16>(to + y * kRowBytes)} * 8 + x] = (static_cast<u32>(HdPicture::HiScores) << dotOf::picture) | (x << dotOf::across) | y;
  writeMode_ = 0;
}

void Front::threeColours(int times, int of) {
  static constexpr u8 kColours[3][4] = {{palette::letterLight, 0x34, 0x34, 0x3e},
                                        {palette::letterMid, 0x25, 0x25, 0x2d},
                                        {palette::letterDark, 0x11, 0x11, 0x19}};
  for (const auto& c : kColours)
    for (std::size_t part = 0; part < kColourBytes; ++part)
      dac_[c[0] * 3u + part] = static_cast<u8>(((c[part + 1] * (times & 0xff)) / of) & palette::dacMax);
  textLevel_ = static_cast<float>(times < 0 ? 0 : times) / static_cast<float>(of);
}

void Front::rubOutText(bool both) {
  writeMode(1);
  peek(kLatchByte);
  for (const u16 first : {kBeside, static_cast<u16>(kPage1 + kBeside)}) {
    if (first == kBeside && !both) continue;
    u16 at = first;
    for (int row = 0; row < kRows; ++row, at = static_cast<u16>(at + kRowBytes)) fill(at, kBesideBytes);
  }
}

// ---------------------------------------------------------------------------------------
// what the sound driver calls each frame
// ---------------------------------------------------------------------------------------
void Front::frameCallback() {
  clock_ = static_cast<u16>(clock_ + kClockStep);
  if (openWidth_ != 0) shownWidth_ = static_cast<u16>((openWidth_ >> 2) * 8);
  if (splitPalette_) colourSelect_ = 1;
  if (scrollerOn_) scroller();
}

void Front::midFrameCallback() {
  if (slideIn_) {
    const u16 at = slideAt_--;
    if (at == kNone) slideIn_ = false;
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
    if (count <= -scroller::rows) {  // cs:29ea
      scrollerShowsText_ = false;
      scrollerCount_ = scroller::shown;
      return restore();
    }
    // cs:2a69: a row of the picture back
    writeMode(1);
    const i16 row = static_cast<i16>(-count - (scroller::rows - 1));
    const u16 from = static_cast<u16>(kPanelKept + row * -kPanelTextBytes);
    const u16 to = static_cast<u16>(static_cast<u16>(kPanelTextTop - row) * kRowBytes + kPanelTextLeft);
    copy(to, from, kPanelTextBytes);
    copy(static_cast<u16>(to + kPage1), from, kPanelTextBytes);
    return restore();
  }
  if (count > -scroller::rows) {  // cs:2ab7: three rows painted over
    scrollerNow_ = scrollerText_;
    writeMode_ = 0;
    planeMask_ = kAllPlanes;
    setReset_ = scroller::colour;
    enableSetReset_ = kAllPlanes;
    for (int n = scroller::rowsAFrame;; ) {
      const u16 at = static_cast<u16>(static_cast<u16>(kPanelTextTop - scrollerCount_) * kRowBytes + (kPanelTextLeft - 1));  // (a byte to the left, too)
      fill(at, kPanelTextBytes);
      fill(static_cast<u16>(at + kPage1), kPanelTextBytes);
      if (--n == 0) break;
      --scrollerCount_;
    }
    return restore();
  }
  if (count > -(scroller::rows + scroller::letters)) {  // cs:2b0f: one more letter
    writeMode_ = 0;
    planeMask_ = scroller::plane;
    enableSetReset_ = keptEnable;
    const u16 n = static_cast<u16>(-count - scroller::rows);
    // (kScrollerColumns, as a number: GCC divides a word by a number in 16 bits, by a name in 32)
    const u8 line = static_cast<u8>(n / 12), column = static_cast<u8>(n % 12);
    u16 at = static_cast<u16>(kScrollerLetters + static_cast<u16>(static_cast<u8>(line * kScrollerLineRows) * kRowBytes) + column);
    const u8 letter = cs(static_cast<u16>(scrollerNow_ + static_cast<u8>(kScrollerColumns * line) + column));
    for (int row = 0; row < kRomRows; ++row, at = static_cast<u16>(at + kRowBytes)) {
      const u8 dots = static_cast<u8>(~romRow(letter, row));
      poke(at, dots);
      poke(static_cast<u16>(at + kPage1), dots);
    }
    return restore();
  }
  scrollerShowsText_ = true;
  scrollerCount_ = scroller::shown;
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
    const bool toBlack = fromSegment == seg::data && (from == var::black || from == var::white);
    const bool fromBlack = toSegment == seg::data && (to == var::black || to == var::white);
    fadeWhite_ = (toSegment == seg::data && to == var::white) || (fromSegment == seg::data && from == var::white);
    level_ = toBlack && fromBlack ? 0.0f : toBlack ? static_cast<float>(left) / static_cast<float>(frames)
                                         : static_cast<float>(frames - left) / static_cast<float>(frames);
  }
}

Front::Task Front::waitTicks(u32 ticks) {
  for (;;) {
    if (music_.ticks() > ticks) break;
    // Space: no more slides. (The original lets go of any other key pressed here; this version
    // keeps it, so that a table asked for early is opened as soon as the menu is there.)
    if (key_ == scan::space) {
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
  for (int left = times::leave; left > 0; --left) {
    for (int i = 0; i < palette::lowBytes; ++i) {
      const unsigned a = ds(static_cast<u16>(var::black + i)) * static_cast<unsigned>(times::leave - left);
      const unsigned b = ds(static_cast<u16>(var::leavePalette + i)) * static_cast<unsigned>(left);
      fadeBuffer_[static_cast<std::size_t>(i)] = static_cast<u8>(((a + b) & 0xffff) / unsigned{times::leave});
    }
    co_await nextFrame();
    setDac(0, fadeBuffer_.data(), palette::lowBytes);
    level_ = static_cast<float>(left) / static_cast<float>(times::leave);
    const u16 volume = static_cast<u16>((left << 8) / times::leave);
    if ((volume & kVolumeStep) != lastVolume_) {
      lastVolume_ = volume & kVolumeStep;
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
  co_await waitTicks(slideTimes::start);
  co_await fade(slideTimes::fade, seg::data, dsw(var::logo0Palette), seg::data, var::black, palette::lowBytes);
  co_await waitTicks(slideTimes::logo0);
  co_await fade(slideTimes::fade, seg::data, var::black, seg::data, dsw(var::logo0Palette), palette::lowBytes);
  if (!skip_) {
    setStart(screenStart::logo1);
    slide_ = 2;
    co_await fade(slideTimes::fadeWhite, seg::data, dsw(var::logo1Palette), seg::data, var::white, palette::allBytes);
    co_await waitTicks(slideTimes::logo1);
    co_await fade(slideTimes::fade, seg::data, var::black, seg::data, dsw(var::logo1Palette), palette::allBytes);
  }
  if (!skip_) {
    co_await fade(slideTimes::gap, seg::data, var::black, seg::data, var::black, palette::allBytes);
    setStart(screenStart::logo2);
    slide_ = 3;
    co_await fade(slideTimes::fadeWhite, seg::logo2Upper, dsw(var::logo2Cmap), seg::data, var::white, palette::allBytes);
    co_await waitTicks(slideTimes::logo2);
    co_await fade(slideTimes::fade, seg::data, var::black, seg::logo2Upper, dsw(var::logo2Cmap), palette::allBytes);
  }
  if (!skip_) {
    setStart(0);
    for (auto& plane : planes_) std::fill(plane.begin(), plane.begin() + kScreenBytes, u8{0});  // (0x2580 words: the screen's 240 rows)
    fadeBuffer_.fill(0);
    setDac(0, fadeBuffer_.data(), palette::allBytes);
    slide_ = 4;
    const u16 colours = unpackChunky(seg::presents, slideRows::presents, kHalfBytes);
    setw(var::presentsCmap, colours);
    co_await fade(slideTimes::fadePresents, seg::presents, colours, seg::data, var::black, palette::lowBytes);
    co_await waitTicks(slideTimes::presents);
    co_await fade(slideTimes::fade, seg::data, var::black, seg::presents, colours, palette::lowBytes);
  }
  if (!skip_) {
    clearVideo();
    mode_ = Mode::Planar480;  // (the BIOS's mode 12h)
    start_ = 0;
    selectBits_ = false;
    co_await fade(slideTimes::gap, seg::data, var::black, seg::data, var::black, palette::lowBytes);
    slide_ = 5;
    const u16 colours = unpackPlanar(seg::pflogo, slideRows::pflogo, false);
    setw(var::pflogoCmap, colours);
    co_await fade(slideTimes::fade, seg::pflogo, colours, seg::data, var::black, palette::lowBytes);
    co_await waitTicks(slideTimes::pflogo);
    co_await fade(slideTimes::fade, seg::data, var::black, seg::pflogo, colours, palette::lowBytes);
  }
  if (skip_) {  // cs:09bd: two frames of black
    co_await fade(slideTimes::gap, seg::data, var::black, seg::data, var::black, palette::lowBytes);
    co_await fade(slideTimes::gap, seg::data, var::black, seg::data, var::black, palette::lowBytes);
  }
  slide_ = 0;
  co_await nextFrame();
  mode_ = Mode::Planar240;
  co_await nextFrame();
  fadeBuffer_.fill(0);
  setDac(0, fadeBuffer_.data(), palette::allBytes);
}

Front::Task Front::banners(u16 which) {
  for (u16 at = var::warpTable;;) {
    const u8 row = ds(at);
    if (row != kWarpEnd) {
      copy(static_cast<u16>(kBannerTop + row * kRowBytes), static_cast<u16>(kBanners + which + row * kBesideBytes), kBesideBytes);  // cs:260a
      const u8 other = static_cast<u8>(kBannerLast - row);                                                                        // cs:262c
      bannerTop_[row % 96] = bannerBottom_[other % 96] = true;
      copy(static_cast<u16>(kBannerBottom + other * kRowBytes), static_cast<u16>(kBannersLower + which + other * kBesideBytes), kBesideBytes);
      ++at;
      continue;
    }
    if (ds(++at) == kWarpEnd) break;
    co_await nextFrame();
  }
}

Front::Task Front::rubOutBanners() {
  peek(kLatchByte);
  for (u16 at = var::warpTable;;) {
    const u8 row = ds(at);
    if (row != kWarpEnd) {
      bannerTop_[static_cast<u8>(kBannerLast - row) % 96] = bannerBottom_[row % 96] = false;
      fill(static_cast<u16>(kBannerTop + static_cast<u8>(kBannerLast - row) * kRowBytes), kBesideBytes);  // cs:2654
      fill(static_cast<u16>(kBannerBottom + row * kRowBytes), kBesideBytes);                             // cs:266f
      ++at;
      continue;
    }
    if (ds(++at) == kWarpEnd) break;
    co_await nextFrame();
  }
}

Front::Task Front::closePage() {
  setStart(kPage1);
  for (int n = 0; n < times::pageFade; ++n) {
    threeColours(times::pageFade - 1 - n, times::pageFade);
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
  sendColours(palette::bottom, pictures::table1);
  splitPalette_ = false;
  colourSelect_ = 0;
  u16 words = 0;
  bool scores = false;
  for (int pass = 0; pass < 2; ++pass) {
    for (;;) {
      const u16 at = pageAt_;
      pageAt_ = static_cast<u16>(pageAt_ + 2);
      words = dsw(static_cast<u16>(at + var::pages));
      if (words == kScoresPage) words = dsw(var::pages);
      if (words != 0) break;
      pageAt_ = 2;
    }
    scores = dsw(static_cast<u16>(pageAt_ + var::pageTaken)) == kScoresPage;
    // (the tall screen has all four tables' scores on one page: the other pair's page, which
    // would come next, is passed over)
    if (!(tall() && scores && lastWasScores_)) break;
  }
  lastWasScores_ = scores;
  showing_ = scores ? Showing::Scores : Showing::Page;
  drawAt_ = kPage1;
  fontAt_ = kLqLetters;
  if (scores) {  // cs:13cb
    heading(var::hiscoresLq, static_cast<u16>(kPage1 + kHeadingAt));
    drawAt_ = static_cast<u16>(drawAt_ + kUnderHeading);
  }
  writeMode_ = 0;
  text(pageText::top, words);
  for (int n = 0; n < times::pageGap; ++n) {
    threeColours(times::pageGap - 1 - n, times::pageGap);
    co_await nextFrame();
  }
  setStart(kPage1);
  co_await nextFrame();
  drawAt_ = 0;
  fontAt_ = 0;
  if (scores) {  // cs:13ac
    heading(var::hiscoresHq, kHeadingAt);
    drawAt_ = static_cast<u16>(drawAt_ + kUnderHeading);
  }
  text(pageText::top, words);
  for (int left = times::pageFade; left > 0; --left) {
    threeColours(times::pageFade - left, times::pageFade);
    co_await nextFrame();
  }
  setStart(0);
  wantOptions_ = false;
  launch_ = -1;
  for (int left = times::page; left > 0; --left) {
    co_await nextFrame();
    const u8 k = takeKey();
    if (k == scan::escape || k == scan::enter || k == scan::space) break;
    if (k >= scan::f1 && k <= scan::f4) {
      launch_ = k - scan::f1;
      break;
    }
    if (k == scan::f5) {
      wantOptions_ = true;
      scrollerText_ = code::leftTextOptions;
      break;
    }
  }
  co_await closePage();
  showing_ = Showing::Banners;
}

void Front::optionText(int row, u16& words) {
  switch (row) {
    case 0: words = ds(var::balls) ? optionWords::balls5 : optionWords::balls3; break;
    case 1: words = ds(var::angle) == cfg::angleLow ? optionWords::low : ds(var::angle) == cfg::angleHigher ? kHigher : optionWords::high; break;
    case 2: words = dsw(static_cast<u16>(optionWords::scrollSpeeds + ds(var::scrollSpeed) * 2)); break;
    case 3: words = ds(var::noMusic) ? optionWords::off : optionWords::on; break;
    case 4:
      words = ds(var::resolution) == cfg::resolutionHigh   ? optionWords::high
              : ds(var::resolution) == cfg::resolutionFull ? kFull
              : ds(var::resolution) == cfg::resolutionTall ? kTall
                                                           : optionWords::normal;
      break;
    case 5: words = ds(var::mono) ? optionWords::mono : optionWords::color; break;
    // this version's two
    case 6: words = options_.originalPictures ? kOriginal : kRemaster; break;
    case 7: words = options_.originalSound ? kOriginal : kBalanced; break;
    case 8: words = options_.dotMatrixTop ? kTop : kBottom; break;
    default:
      words = portableError_                              ? kError
              : portableLock_ == PortableLock::ReadOnly     ? kReadOnly
              : portableLock_ == PortableLock::NoSpace      ? kNoSpace
              : portableLock_ == PortableLock::AppFolder    ? kNotHere
              : portableLock_ == PortableLock::Translocated ? kMoveApp
              : options_.portable                         ? kYes
                                                          : kNo;
      break;
  }
}

void Front::changeOption(int row) {
  switch (row) {
    case 0: ds(var::balls) ^= 1; break;
    // (the original has high and low; this version's steeper one comes after high)
    case 1: ds(var::angle) = ds(var::angle) == cfg::angleHigh ? cfg::angleHigher : ds(var::angle) == cfg::angleHigher ? cfg::angleLow : cfg::angleHigh; break;
    case 2: ds(var::scrollSpeed) = ds(var::scrollSpeed) >= cfg::scrollSoft ? cfg::scrollHard : static_cast<u8>(ds(var::scrollSpeed) + 1); break;
    case 3: ds(var::noMusic) ^= 1; break;
    // (and after the original's two sizes of screen, this version's whole table, twice)
    case 4: ds(var::resolution) = ds(var::resolution) >= cfg::resolutionTall ? cfg::resolutionNormal : static_cast<u8>(ds(var::resolution) + 1); break;
    case 5: ds(var::mono) ^= 1; break;
    case 6: options_.originalPictures = !options_.originalPictures; break;
    case 7: options_.originalSound = !options_.originalSound; break;
    case 8: options_.dotMatrixTop = !options_.dotMatrixTop; break;
    // (not while it cannot be changed, or its last change is being shown to have failed)
    case 9: if (portableLock_ == PortableLock::None && !portableError_) options_.portable = !options_.portable; break;
    default: optionsDone_ = true; break;
  }
  options_.balls = ds(var::balls) ? cfg::manyBalls : cfg::fewBalls;
  options_.angle = ds(var::angle) == cfg::angleLow ? Angle::Low : ds(var::angle) == cfg::angleHigher ? Angle::Higher : Angle::High;
  options_.scrollSpeed = static_cast<ScrollSpeed>(ds(var::scrollSpeed));
  options_.noMusic = ds(var::noMusic) != 0;
  options_.resolution = static_cast<Resolution>(ds(var::resolution));
  options_.mono = ds(var::mono) != 0;
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
    const auto rowY = [&](int row) { return static_cast<u16>((row < count ? row : row + 1) * pageText::lineHeight + kOptionsTop); };  // (the empty line before turning)
    if (moved) {  // cs:3f5a: the mark beside the row
      const u16 y = rowY(optionRow_);
      writeMode(1);
      peek(kLatchByte);
      for (const u16 at0 : {kMarkAt, static_cast<u16>(kPage1 + kMarkAt)}) {
        u16 at = at0;
        for (int row = 0; row < kMarkRows + pageText::lineHeight; ++row, at = static_cast<u16>(at + kRowBytes)) fill(at, kMarkBytes);  // (a row more)
      }
      writeMode_ = 0;
      co_await say(y, kMarkX, optionWords::mark);
      moved = false;
    }
    if (optionsDone_) co_return;
    u8 k = 0;
    do {
      k = takeKey();
      co_await nextFrame();
    } while (k == 0);
    if (k == scan::escape) co_return;
    if (k == scan::down) {
      optionRow_ = optionRow_ >= count + 1 ? 0 : optionRow_ + 1;
      moved = true;
    } else if (k == scan::up) {
      optionRow_ = optionRow_ <= 0 ? count + 1 : optionRow_ - 1;
      moved = true;
    } else if (k == scan::space || k == scan::enter) {
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
        const u16 x = kWordX;  // (cs:4155; its row times 0x2d00 is of a row already lost)
        const u16 width = static_cast<u16>(((length * menuFont::advance) >> 3) + 2);
        const u16 y = rowY(optionRow_);
        writeMode(1);
        peek(kLatchByte);
        for (const u16 page : {u16{0}, kPage1}) {
          u16 at = static_cast<u16>(y * kRowBytes + (x >> 3) + page);
          for (int r = 0; r < menuFont::rows; ++r, at = static_cast<u16>(at + kRowBytes)) fill(at, width);
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
    peek(kLatchByte);
    u16 to = static_cast<u16>(at + kBeside);
    for (int row = 0; row < kRows; ++row, to = static_cast<u16>(to + kRowBytes)) fill(to, kBesideBytes);  // (as cs:26b1)
    writeMode_ = 0;
    drawAt_ = at;
    fontAt_ = 0;
    text(pageText::top, optionsPages_[static_cast<std::size_t>(page)].data());
  }
  drawAt_ = 0;
}

/// cs:43f4: the options, in place of the banners.
Front::Task Front::optionsMenu() {
  optionsShown_ = 0;
  if (portableError_) portableErrorShown_ = true;
  scrollerText_ = code::leftTextOptions;
  co_await rubOutBanners();
  showing_ = Showing::Page;
  writeMode_ = 0;
  sendColours(palette::bottom, pictures::table1);
  splitPalette_ = false;
  colourSelect_ = 0;
  drawAt_ = kPage1;
  fontAt_ = kLqLetters;
  text(pageText::top, optionsPages_[static_cast<std::size_t>(optionsShown_)].data());
  for (int n = 0; n < times::pageGap; ++n) {
    threeColours(times::pageGap - 1 - n, times::pageGap);
    co_await nextFrame();
  }
  setStart(kPage1);
  co_await nextFrame();
  drawAt_ = 0;
  fontAt_ = 0;
  text(pageText::top, optionsPages_[static_cast<std::size_t>(optionsShown_)].data());
  co_await nextFrame();
  for (int left = times::optionsFade; left > 0; --left) {
    threeColours(times::optionsFade - left, times::optionsFade);
    co_await nextFrame();
  }
  setStart(0);
  co_await nextFrame();
  writeMode(1);
  peek(kLatchByte);
  for (u16 at = static_cast<u16>(kPage1 + kBeside), row = 0; row < kRows; ++row, at = static_cast<u16>(at + kRowBytes)) fill(at, kBesideBytes);  // cs:26b1
  writeMode_ = 0;
  drawAt_ = kPage1;
  fontAt_ = 0;
  text(pageText::top, optionsPages_[static_cast<std::size_t>(optionsShown_)].data());

  co_await chooseOptions();

  scrollerText_ = code::leftTextMenu;
  co_await nextFrame();
  setStart(0);
  co_await nextFrame();
  writeMode(1);
  peek(kLatchByte);
  for (u16 at = static_cast<u16>(kPage1 + kBeside), row = 0; row < kRows; ++row, at = static_cast<u16>(at + kRowBytes)) fill(at, kBesideBytes);
  writeMode_ = 0;
  drawAt_ = kPage1;
  fontAt_ = kLqLetters;
  text(pageText::top, optionsPages_[static_cast<std::size_t>(optionsShown_)].data());
  text(pageText::top, optionsPages_[static_cast<std::size_t>(optionsShown_)].data());
  setStart(kPage1);
  for (int n = 0; n < times::optionsFade; ++n) {
    threeColours(times::optionsFade - 1 - n, times::optionsFade);
    co_await nextFrame();
  }
  rubOutText(true);
  setStart(0);
  splitPalette_ = true;
  showing_ = Showing::Banners;
  if (options_.balls != saved_.balls || options_.angle != saved_.angle || options_.scrollSpeed != saved_.scrollSpeed ||
      options_.resolution != saved_.resolution || options_.noMusic != saved_.noMusic || options_.mono != saved_.mono ||
      options_.originalSound != saved_.originalSound || options_.originalPictures != saved_.originalPictures ||
      options_.dotMatrixTop != saved_.dotMatrixTop || options_.portable != saved_.portable) {
    saved_ = options_;
    action_.kind = Action::Kind::SaveOptions;
  }
  // (PORTABLE: ERROR was shown this once; the next time the real value is)
  if (portableError_) {
    portableError_ = false;
    writeOptionWord(kOptions - 1);
  }
}

Front::Task Front::menu() {
  clearVideo();
  mode_ = Mode::Planar240;
  co_await fade(times::black, seg::data, var::black, seg::data, var::black, palette::lowBytes);
  co_await nextFrame();
  selectBits_ = true;
  // two headings, each unpacked to the top of the screen and kept (cs:1377)
  for (const auto& [segment, keep] : {std::pair<u16, u16>{seg::hiscoresLq, var::hiscoresLq}, {seg::hiscoresHq, var::hiscoresHq}}) {
    unpackPlanar(segment, 0, false);
    u16 to = keep;
    for (auto& plane : planes_) {
      u16 at = 0;
      for (int row = 0; row < kHeadingRows; ++row, at = static_cast<u16>(at + (kRowBytes - kHeadingBytes)))
        for (int b = 0; b < kHeadingBytes; ++b) ds(to++) = plane[at++];
    }
  }
  openWidth_ = kOpenPanel;
  co_await nextFrame();
  for (u16 i = 0; i < pictures::end; i = static_cast<u16>(i + 2))  // cs:1449: the menu's picture, its letters, the four banners
    setw(static_cast<u16>(var::pictureCmaps + i),
         unpackPlanar(dsw(static_cast<u16>(var::pictureSegments + i)), dsw(static_cast<u16>(var::pictureRows + i)), false,
                      i == pictures::left     ? HdPicture::Left
                      : i >= pictures::table1 ? static_cast<HdPicture>(static_cast<int>(HdPicture::Table1) + (i - pictures::table1) / 2)
                                              : HdPicture::None));
  writeMode(1);
  {  // cs:2d3a: the banners' rows, 55 bytes of each, moved up against each other
    u16 from = kBanners + kRowBytes, to = kBanners + kBesideBytes;  // (from the second row: the first is in place)
    for (int row = 0; row < kBanners4 * kBannerRows - 1; ++row, from = static_cast<u16>(from + kRowBytes), to = static_cast<u16>(to + kBesideBytes))
      copy(to, from, kBesideBytes);
  }
  copy(kPage1, 0, kScreenBytes);  // cs:2d74: the second page
  {                               // cs:2b8a: what is under the panel's text
    u16 from = kPanelText, to = kPanelKept;
    for (int row = 0; row < kPanelTextRows; ++row, from = static_cast<u16>(from + kRowBytes), to = static_cast<u16>(to + kPanelTextBytes))
      copy(to, from, kPanelTextBytes);
  }
  setStart(kSlideIn);
  co_await fade(times::black, seg::data, var::black, seg::data, var::black, palette::lowBytes);
  auto keepColours = [this](u16 top, u16 bottom) {  // for the fade when the menu is left
    for (u16 i = 0; i < palette::setBytes; ++i) {
      ds(static_cast<u16>(var::leavePaletteTop + i)) =
          farByte(dsw(static_cast<u16>(var::pictureSegments + top)), static_cast<u32>(dsw(static_cast<u16>(var::pictureCmaps + top)) + i));
      ds(static_cast<u16>(var::leavePalette + i)) =
          farByte(dsw(static_cast<u16>(var::pictureSegments + bottom)), static_cast<u32>(dsw(static_cast<u16>(var::pictureCmaps + bottom)) + i));
    }
  };
  sendColours(palette::top, pictures::table3);
  sendColours(palette::bottom, pictures::table3);
  keepColours(pictures::table1, pictures::table2);
  splitPalette_ = true;
  openWidth_ = kOpenPanel;
  for (int n = 0; n < times::openWait; ++n) co_await nextFrame();
  slideAt_ = kSlideIn;
  slideIn_ = true;
  for (int n = 0; n < times::slideIn; ++n) co_await nextFrame();  // (the original lets go of keys here; see waitTicks)
  openWidth_ = kOpenWhole;
  writeMode(1);
  // (cs:15fa to cs:1a23 asks a question out of the manual: not here)
  scrollerOn_ = true;
  co_await rubOutBanners();
  idle_ = times::banners;

  for (;;) {  // cs:1a55
    for (int n = 0; n < times::bannersGap; ++n) {
      co_await nextFrame();
      if (key_ == scan::space) {
        takeKey();
        break;
      }
    }
    if (!(dsw(var::tablePair) & kPair34)) {
      sendColours(palette::top, pictures::table1);
      sendColours(palette::bottom, pictures::table2);
      keepColours(pictures::table1, pictures::table2);
      splitPalette_ = true;
      co_await banners(0);
    } else {
      sendColours(palette::top, pictures::table3);
      sendColours(palette::bottom, pictures::table4);
      keepColours(pictures::table3, pictures::table4);
      splitPalette_ = true;
      co_await banners(kTables34);
      idle_ = times::banners;
    }
    enum class Next { Banners, Page } next = Next::Banners;
    for (;;) {  // cs:1b3a
      co_await nextFrame();
      const u8 k = takeKey();
      if (k == scan::escape) {
        co_await leave(-1);
        co_return;
      }
      if (k >= scan::f1 && k <= scan::f4) {
        co_await leave(k - scan::f1);
        co_return;
      }
      if (k == scan::f5) {
        co_await optionsMenu();
        break;
      }
      if (k == scan::enter) {  // cs:1ba7
        setw(var::tablePair, dsw(var::tablePair) ^ kPair34);
        pageAt_ = static_cast<u16>(pageAt_ + 2);
        // (as the original: the place in the list read as an address, and set to one)
        const bool none = dsw(pageAt_) == 0;
        if (none) pageAt_ = var::textPages;
        setw(var::pages, none ? var::scoresTables12 : var::scoresTables34);
        co_await rubOutBanners();
        idle_ = times::banners;
        break;
      }
      if (k == scan::space || --idle_ == 0) {  // cs:1bde
        setw(var::tablePair, dsw(var::tablePair) ^ kPair34);
        setw(var::pages, dsw(var::tablePair) == 0 ? var::scoresTables34 : var::scoresTables12);
        idle_ = times::banners;
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
  setw(var::logo0Cmap, unpackChunky(seg::logo0Upper, slideRows::logo0Upper, kHalfBytes));
  for (u16 i = 0; i < palette::allBytes; ++i) ds(i) = farByte(seg::logo0Upper, static_cast<u32>(dsw(var::logo0Cmap) + i));
  unpackChunky(seg::logo0Lower, slideRows::logo0Lower, kHalfBytes);
  setw(var::logo1Cmap, unpackChunky(seg::logo1Upper, slideRows::logo1Upper, kTallHalfBytes));
  for (u16 i = 0; i < palette::allBytes; ++i) ds(static_cast<u16>(var::logo1Colours + i)) = farByte(seg::logo1Upper, static_cast<u32>(dsw(var::logo1Cmap) + i));
  unpackChunky(seg::logo1Lower, slideRows::logo1Lower, kHalfBytes);
  setw(var::logo2Cmap, unpackChunky(seg::logo2Upper, slideRows::logo2Upper, kTallHalfBytes));
  unpackChunky(seg::logo2Lower, slideRows::logo2Lower, kHalfBytes);

  setw(var::tablePair, 0);
  pageAt_ = 2;
  if (returningFrom_ >= kTable3) {  // back from one of the last two tables: theirs are the banners shown
    setw(var::tablePair, kPair34);
    pageAt_ = 4;
    setw(var::pages, var::scoresTables34);
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
    hd->fadeColor = fadeWhite_ ? kWhite : Rgb{};
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
  for (std::size_t i = 0; i < palette::colours; ++i) colours[i] = Rgb{wide(dac_[i * 3]), wide(dac_[i * 3 + 1]), wide(dac_[i * 3 + 2])};
  if (tall() && mode_ == Mode::Planar240) {
    // the four banners' own colours, a set of sixteen each, as bright as the screen is
    for (std::size_t t = 0; t < kBanners4; ++t) {
      const u16 picture = static_cast<u16>(pictures::table1 + t * 2);
      const u16 segment = const_cast<Front*>(this)->dsw(static_cast<u16>(var::pictureSegments + picture));
      const u16 at = const_cast<Front*>(this)->dsw(static_cast<u16>(var::pictureCmaps + picture));
      for (std::size_t i = 0; i < palette::setColours; ++i) {
        auto part = [&](std::size_t n) {
          const u8 v = const_cast<Front*>(this)->farByte(segment, static_cast<u32>(at + i * 3 + n)) & palette::dacMax;
          return static_cast<u8>(static_cast<float>(wide(v)) * level_);
        };
        colours[kTallBanners + t * palette::setColours + i] = Rgb{part(0), part(1), part(2)};
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
      const u16 row = static_cast<u16>(start_ + (y / 2) * kRowBytes);
      for (int x = 0; x < kWidth; ++x) {
        out[x] = planes_[static_cast<std::size_t>((x / 2) & 3)][static_cast<u16>(row + x / 8)];
        if (hd && slide_ >= 1 && slide_ <= kChunkySlides)
          mark(x, y, static_cast<HdPicture>(slide_), static_cast<u32>(x * 4), static_cast<u32>(y * 4), HdFrame::kHalfX | HdFrame::kHalfY, kChunkyWidth,
               kChunkyHeight);
      }
      continue;
    }
    const u16 row = static_cast<u16>(start_ + (mode_ == Mode::Planar240 ? y / 2 : y) * kRowBytes);
    const u8 upper = selectBits_ ? static_cast<u8>(((y < kSplitRow ? selectTop_ : selectBottom_) & kSelectBits) << kSelectShift) : 0;
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
        if (slide_ == kLastSlide) mark(x, y, HdPicture::Slide5, static_cast<u32>(x * 8), static_cast<u32>(y * 8), 0, kWidth, kHeight);
        continue;
      }
      const u32 dot = from_[std::size_t{at} * 8 + static_cast<std::size_t>(x & 7)];
      if (dot == 0) continue;
      const auto is = static_cast<HdPicture>(dot >> dotOf::picture);
      const bool banner = is >= HdPicture::Table1 && is <= HdPicture::Table4;
      mark(x, y, is, ((dot >> dotOf::across) & dotOf::field) * 8, (dot & dotOf::field) * 8 + static_cast<u32>(y & 1) * 4, HdFrame::kHalfY,
           is == HdPicture::Left ? kPanelDots : banner ? kBannerDots : kHeadingDots, is == HdPicture::Left ? kPanelRows : banner ? kBannerRows : kHeadingRows);
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
  auto upper = [&](int row) { return selectBits_ ? static_cast<u8>((((row * 2) < kSplitRow ? selectTop_ : selectBottom_) & kSelectBits) << kSelectShift) : u8{0}; };
  const bool settled = (start_ == 0 || start_ == kPage1) && shownWidth_ >= kBesideDots;
  static constexpr int kBanner[kBanners4] = {12, 132, 252, 372};
  for (int y = 0; y < 2 * kHeight; ++y) {
    u8* out = frame + static_cast<std::size_t>(y) * kWidth;
    const int u = y / 2;
    for (int x = 0; x < kWidth; ++x) {
      out[x] = 0;
      if (x >= shownWidth_) continue;
      // which of the card's rows this is a row of, if any
      int row = -1;
      if (x < kBesideDots) {
        row = u <= kPanelTop ? u : u <= kStretchEnd ? kPanelTop : u - kStretch;
      } else if (showing_ == Showing::Banners && settled) {
        for (int t = 0; t < kBanners4; ++t) {
          const int r = u - kBanner[t];
          if (r < 0 || r >= kBannerRows || x >= kBesideDots + kBannerDots || !(t < 2 ? bannerTop_ : bannerBottom_)[static_cast<std::size_t>(r)]) continue;
          const int bx = x - kBesideDots;
          out[x] = static_cast<u8>(kTallBanners + t * palette::setColours + dots(static_cast<u16>(kBanners + (t * kBannerRows + r) * kBesideBytes + (bx >> 3)), bx));
          if (hd)
            mark(x, y, static_cast<HdPicture>(static_cast<int>(HdPicture::Table1) + t), static_cast<u32>(bx * 8), static_cast<u32>(r * 8 + (y & 1) * 4), kBannerDots,
                 kBannerRows);
        }
        continue;
      } else if (showing_ == Showing::Scores && settled) {
        row = u < kHeadingRows ? u : -1;  // the heading; the lists are written below
      } else {
        row = u >= kTallPageTop && u < kTallPageTop + kRows ? u - kTallPageTop : -1;
      }
      if (row < 0) continue;
      const u16 at = static_cast<u16>(start_ + row * kRowBytes + x / 8);
      // under the panel's text the picture is kept aside (cs:2b8a): it is shown, not the text
      const bool kept = settled && x >= kPanelTextLeft * 8 && x < (kPanelTextLeft + kPanelTextBytes) * 8 && row >= kPanelTextTop &&
                        row < kPanelTextTop + kPanelTextRows;
      const u8 colour = kept ? dots(static_cast<u16>(kPanelKept + (row - kPanelTextTop) * kPanelTextBytes + (x >> 3) - kPanelTextLeft), x) : dots(at, x);
      const u32 dot = kept ? (static_cast<u32>(HdPicture::Left) << dotOf::picture) | (static_cast<u32>(x) << dotOf::across) | static_cast<u32>(row)
                           : from_[std::size_t{at} * 8 + static_cast<std::size_t>(x & 7)];
      if (showing_ == Showing::Scores && settled && x >= kBesideDots && static_cast<HdPicture>(dot >> dotOf::picture) != HdPicture::HiScores) continue;
      out[x] = static_cast<u8>(upper(row) | colour);
      if (!hd || dot == 0) continue;
      const auto is = static_cast<HdPicture>(dot >> dotOf::picture);
      const bool banner = is >= HdPicture::Table1 && is <= HdPicture::Table4;
      // (the panel's plain stretch is one line of its picture all the way down)
      const bool drawnOut = x < kBesideDots && u > kPanelTop && u <= kStretchEnd;
      if (drawnOut && is == HdPicture::Left && stripHeight_ > 0) {
        // the strip is as fine as the panel's picture: so many of its rows to a row of the screen
        const int perRow8 = stripWidth_ * 4 / kPanelDots, down = y - (kPanelTop + 1) * 2;
        mark(x, y, HdPicture::LeftRepeat, ((dot >> dotOf::across) & dotOf::field) * 8, static_cast<u32>((down * perRow8) % (stripHeight_ * 8)), kPanelDots,
             static_cast<u16>(stripHeight_));
        hd->map[static_cast<std::size_t>(y) * kWidth + static_cast<std::size_t>(x)].picture &= static_cast<u16>(~HdFrame::kHalfY);
        hd->rowStep[static_cast<std::size_t>(HdPicture::LeftRepeat)] = static_cast<float>(perRow8) / 8.0f;
        continue;
      }
      mark(x, y, is, ((dot >> dotOf::across) & dotOf::field) * 8, (dot & dotOf::field) * 8 + (drawnOut ? 4 : static_cast<u32>(y & 1) * 4),
           is == HdPicture::Left ? kPanelDots : banner ? kBannerDots : kHeadingDots, is == HdPicture::Left ? kPanelRows : banner ? kBannerRows : kHeadingRows,
           drawnOut ? HdFrame::kFlatY : u16{0});
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
    for (int line = 0; line < kScrollerLines; ++line)
      for (int column = 0; column < kScrollerColumns; ++column) {
        const u8 letter = cs(static_cast<u16>(scrollerText_ + kScrollerColumns * line + column));
        for (int r = 0; r < kRomRows; ++r) {
          const u8 rowDots = romRow(letter, r);
          for (int b = 0; b < 8; ++b)
            if (rowDots & (0x80 >> b)) put(kPanelTextLeft * 8 + column * 8 + b, kTallScrollerTop + line * kScrollerLineRows + r, upper(kPanelTop));
        }
      }
  }
  if (showing_ == Showing::Scores && settled) {
    // the four tables' best scores: the two pages of them, one under the other (cs:2cf2)
    int y = kTallScoresTop;
    for (const u16 first : {u16{var::scoresTables12}, u16{var::scoresTables34}}) {
      u16 page = first;
      for (int line = 0; line < pageText::lines; ++line, y += pageText::lineHeight) {
        u16 left = 0;
        for (u16 i = 0; i < pageText::lineBytes; ++i)
          if (self.ds(static_cast<u16>(page + i)) == 0) {
            left = static_cast<u16>(pageText::lineBytes - i - 1);
            break;
          }
        int x = pageText::left + ((menuFont::advance * left) >> 1);
        for (u16 n = static_cast<u16>(pageText::lineBytes - left); n > 0; --n, x += menuFont::advance) {
          const u16 from = self.dsw(static_cast<u16>(var::glyphs + self.ds(page++) * 2));
          if (from == kNone) continue;
          for (int r = 0; r < menuFont::rows; ++r)
            for (int b = 0; b < menuFont::dots; ++b)
              if (const u8 colour = dots(static_cast<u16>(from + r * kRowBytes + (b >> 3)), b); colour != 0) put(x + b, y + r, colour);
        }
      }
    }
  }
}

}  // namespace encore
