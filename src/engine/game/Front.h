#pragma once
// What comes before the tables: the slides the game opens with the first time, and the menu
// the tables are chosen from, with its pages of best scores and credits and its options.
// Written from INTRO.PRG (re/fantasy/INTRO_seg364c.lst; a comment "cs:1234" is a place in it).
//
// The original is one long routine that draws into the video card's memory and waits for the
// next frame wherever it has to; it is written here the same way, as a routine that is left
// at each of those waits and gone back into a frame later, drawing into a small model of the
// card: its four planes of memory, its colours, where the screen starts in that memory.
#include <array>
#include <coroutine>
#include <exception>
#include <string>
#include <vector>

#include "core/Keys.h"
#include "engine/audio/MusicDriver.h"
#include "game/Config.h"
#include "gfx/HdLayer.h"

namespace encore {

/// INTRO.PRG's places and numbers, by its own addresses, named after what the code does with
/// them.
namespace intro {

/// The program's segments: its own two, and the pictures', each a picture file as it was.
namespace seg {
constexpr u16 data = 0x0080;
constexpr u16 code = 0x364c;
constexpr u16 logo0Upper = 0x3b41;  ///< the first slide, in two halves
constexpr u16 logo0Lower = 0x4285;
constexpr u16 logo1Upper = 0x4d6a;  ///< the second
constexpr u16 logo1Lower = 0x4653;
constexpr u16 logo2Upper = 0x11c2;  ///< the third
constexpr u16 logo2Lower = 0x17b2;
constexpr u16 presents = 0x10a6;  ///< the fourth
constexpr u16 pflogo = 0x0a05;  ///< the fifth, the game's name
constexpr u16 hiscoresLq = 0x3301;  ///< the heading of the pages of best scores, while they fade
constexpr u16 hiscoresHq = 0x3499;  ///< and while they are shown
}  // namespace seg

/// Variables and texts in the data segment.
namespace var {
constexpr u16 logo1Colours = 0x0300;  ///< the second slide's colours, copied here (the first's at 0)
constexpr u16 logo0Palette = 0x060b;  ///< word: where the first slide's colours were copied to
constexpr u16 logo1Palette = 0x060d;  ///< word: and the second's
constexpr u16 hiscoresLq = 0x09df;  ///< the headings of the pages of best scores, kept (50 bytes by 40 rows of each plane)
constexpr u16 hiscoresHq = 0x291f;
constexpr u16 balls = 0x49a3;  ///< the options, as PINBALL.CFG has them: 1 for five balls
constexpr u16 angle = 0x49a4;  ///< 1 low (and this version's 2, higher)
constexpr u16 scrollSpeed = 0x49a5;
constexpr u16 noMusic = 0x49a6;
constexpr u16 resolution = 0x49a7;
constexpr u16 mono = 0x49a8;
constexpr u16 pageTaken = 0x4bff;  ///< pages less two: with pageAt_ moved on, the one just taken
constexpr u16 pages = 0x4c01;  ///< words: the pages shown in place of the banners; the first the
                               ///< best scores of the two tables shown, which an ffff after it
                               ///< stands for; 0 ends the list
constexpr u16 textPages = 0x4c03;  ///< the list past that first
constexpr u16 optionsText = 0x4e40;  ///< the page of options: its heading, its six and saving
constexpr u16 optionsSave = 0x4edf;  ///< its line for saving
constexpr u16 optionsTextEnd = 0x4ef8;
constexpr u16 scoresTables12 = 0x4efc;  ///< the page of best scores of tables 1 and 2
constexpr u16 scoresTable1 = 0x4f31;  ///< where table 1's first best score is written (its initials), a line on to each next
constexpr u16 scoresTable2 = 0x4fc1;
constexpr u16 scoresTables34 = 0x501c;  ///< and of tables 3 and 4
constexpr u16 scoresTable3 = 0x5051;
constexpr u16 scoresTable4 = 0x50e1;
constexpr u16 logo0Cmap = 0x5271;  ///< words: where each slide's colours are in its segment
constexpr u16 logo1Cmap = 0x5273;
constexpr u16 pflogoCmap = 0x5275;
constexpr u16 logo2Cmap = 0x5279;
constexpr u16 presentsCmap = 0x527b;
constexpr u16 black = 0x527d;  ///< 0x300 bytes: every colour black, which the slides fade from and to
constexpr u16 white = 0x557d;  ///< and white, which the second and third come out of
constexpr u16 tablePair = 0x588e;  ///< word: 2 while the banners are of tables 3 and 4
constexpr u16 pictureRows = 0x5890;  ///< words, one for each of pictures: the row it is unpacked at
constexpr u16 pictureSegments = 0x589e;  ///< its segment
constexpr u16 pictureCmaps = 0x58ac;  ///< where its colours are in that segment, once unpacked
constexpr u16 leavePalette = 0x58ba;  ///< the colours the menu fades out from: the bottom part's,
constexpr u16 leavePaletteTop = 0x58ea;  ///< then the top part's
constexpr u16 warpTable = 0x597a;  ///< the banners' rows drawn in, a frame's after another, ff after each; ff ff ends
constexpr u16 glyphs = 0x59ed;  ///< words: where each letter is in the card's memory, by its code; ffff for none
}  // namespace var

/// In a line of best scores: where the score's twelve digits begin, after the initials.
/// Where in video memory the screen starts for the second and the third slides.
namespace screenStart {
constexpr u16 logo1 = 0x4d30, logo2 = 0x99c0;
}  // namespace screenStart

/// The options' words, each ending in a nought.
namespace optionWords {
constexpr u16 balls3 = 0x4df6;
constexpr u16 balls5 = 0x4df8;
constexpr u16 normal = 0x4dfa;
constexpr u16 high = 0x4e01;
constexpr u16 low = 0x4e08;
constexpr u16 scrollSpeeds = 0x4e24;  ///< words: where HARD, MEDIUM and SOFT are
constexpr u16 on = 0x4e2a;
constexpr u16 off = 0x4e2e;
constexpr u16 color = 0x4e32;
constexpr u16 mono = 0x4e38;
constexpr u16 mark = 0x4e3e;  ///< ">", beside the option chosen
}  // namespace optionWords

/// The menu's pictures, by their place in the lists of them (var::pictureRows and the next two).
namespace pictures {
constexpr u16 left = 0x00;  ///< the panel on the left
constexpr u16 fontHq = 0x02;  ///< the letters
constexpr u16 fontLq = 0x04;  ///< the letters while a page fades
constexpr u16 table1 = 0x06;  ///< the four banners
constexpr u16 table2 = 0x08;
constexpr u16 table3 = 0x0a;
constexpr u16 table4 = 0x0c;
}  // namespace pictures

/// The texts on the panel on the left, in the code segment: ten lines of twelve letters each.
namespace code {
constexpr u16 leftTextMenu = 0x288e;
constexpr u16 leftTextOptions = 0x2906;
}  // namespace code

/// Colours the program keeps for its own.
namespace palette {
constexpr u8 bottom = 0x00;  ///< sixteen for the screen's bottom part, and for all of it
constexpr u8 top = 0x10;  ///< sixteen for the top part, while the palette is split
constexpr u8 letterLight = 0x01;  ///< the letters' three colours
constexpr u8 letterMid = 0x0c;
constexpr u8 letterDark = 0x0e;
}  // namespace palette

}  // namespace intro

class Front {
 public:
  static constexpr int kWidth = 640, kHeight = 480;

  /// `prg` is INTRO.PRG; `module` is INTRO.MOD the first time and MOD2.MOD after a table.
  /// `returningFrom` is the table come back from (0 to 3), or -1 the first time, which shows
  /// the slides. `scores` are the four tables' best scores, for its pages of them.
  /// `portableLock` says why PORTABLE cannot be changed, if it cannot, and `portableError`
  /// that the last change of it failed (shown as its value the next time the options open).
  Front(ByteView prg, ByteView module, const Config& config, int returningFrom,
        PortableLock portableLock = PortableLock::None, bool portableError = false);
  ~Front();
  Front(const Front&) = delete;
  Front& operator=(const Front&) = delete;

  void key(Key key, bool down);

  struct Action {
    enum class Kind { None, OpenTable, SaveOptions, Quit } kind = Kind::None;
    int table = 0;  ///< 0 to 3
  };
  /// One frame, a sixtieth of a second.
  Action frame();
  const Options& options() const { return options_; }

  /// The screen: 640 across and height() down, each dot one of 256 colours.
  /// With the whole table on one screen asked for in the options, the screen is as tall as
  /// that one, two of the usual: the slides stand in the middle of it, and the menu is laid
  /// out down it, with all four tables on one page and all four lists of best scores on one.
  void draw(u8* frame, Rgb* colours, HdFrame* hd = nullptr) const;
  bool tall() const { return options_.resolution == Resolution::Full || options_.resolution == Resolution::Tall; }
  /// There is a picture of the panel's plain part to repeat down the tall menu
  /// (HdPicture::LeftRepeat), this many dots across and down; without one, a line of the
  /// panel's own picture is drawn out instead.
  void setPanelStrip(int width, int height) { stripWidth_ = width, stripHeight_ = height; }
  int height() const { return tall() ? 2 * kHeight : kHeight; }

  /// ARTWORK, AUDIO, DOT MATRIX and PORTABLE as they are now, whatever the options page last
  /// said (keys change them anywhere, and a move can fail): the page shows them so the next
  /// time it opens.
  void setLooks(bool originalSound, bool originalPictures, bool dotMatrixTop, bool portable,
                PortableLock portableLock, bool portableError);
  /// The options were opened with PORTABLE: ERROR shown: it is not to be shown again.
  bool portableErrorShown() const { return portableErrorShown_; }
  MusicDriver& music() { return music_; }
  void sound(float* out, int frames) { music_.render(out, frames); }
  void noSound() { music_.pass(1.0 / 60); }

 private:
  // --- a routine that can wait for the next frame, and call others that can
  struct Task {
    struct promise_type {
      std::coroutine_handle<> parent;
      std::exception_ptr error;
      Task get_return_object() { return Task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
      std::suspend_always initial_suspend() noexcept { return {}; }
      struct Final {
        bool await_ready() noexcept { return false; }
        std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
          return h.promise().parent ? h.promise().parent : std::noop_coroutine();
        }
        void await_resume() noexcept {}
      };
      Final final_suspend() noexcept { return {}; }
      void return_void() {}
      void unhandled_exception() { error = std::current_exception(); }
    };
    std::coroutine_handle<promise_type> handle;
    Task() = default;
    explicit Task(std::coroutine_handle<promise_type> h) : handle(h) {}
    Task& operator=(Task&& o) noexcept {
      if (handle) handle.destroy();
      handle = o.handle;
      o.handle = {};
      return *this;
    }
    Task(Task&& o) noexcept : handle(o.handle) { o.handle = {}; }
    Task(const Task&) = delete;
    ~Task() {
      if (handle) handle.destroy();
    }
    bool await_ready() const noexcept { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> parent) noexcept {
      handle.promise().parent = parent;
      return handle;
    }
    void await_resume() {
      if (handle.promise().error) std::rethrow_exception(handle.promise().error);
    }
  };
  struct NextFrame {
    Front* front;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) noexcept { front->waiting_ = h; }
    void await_resume() const noexcept {}
  };
  NextFrame nextFrame() { return NextFrame{this}; }

  // --- the program's memory
  u8& ds(u16 at) { return image_[intro::seg::data * 16 + at]; }
  u16 dsw(u16 at) { return static_cast<u16>(ds(at) | (ds(static_cast<u16>(at + 1)) << 8)); }
  void setw(u16 at, u16 v) {
    ds(at) = static_cast<u8>(v);
    ds(static_cast<u16>(at + 1)) = static_cast<u8>(v >> 8);
  }
  u8& farByte(u16 segment, u32 offset) { return image_[(std::size_t{segment} * 16 + offset) % image_.size()]; }
  u8 cs(u16 at) const { return image_[std::size_t{intro::seg::code} * 16 + at]; }

  // --- the video card
  enum class Mode { Chunky320, Planar480, Planar240 };
  u8 peek(u16 at);                 ///< reads a byte, and keeps all four planes' for a copy
  void poke(u16 at, u8 value);     ///< writes as the card is set to write
  void copy(u16 to, u16 from, u16 count);   ///< rep movsb within the card's memory
  void fill(u16 to, u16 count, u8 value = 0);  ///< rep stosb
  void writeMode(int mode);        ///< cs:2cd7 (1: copies of all planes) and cs:2ce6 (0)
  void setStart(u16 at) { start_ = at; }
  void setDac(int first, const u8* values, int bytes);
  void clearVideo();

  // --- the routines
  Task main();
  Task slides();
  Task menu();
  Task fade(int frames, u16 fromSegment, u16 from, u16 toSegment, u16 to, int bytes);
  Task leave(int table);           ///< cs:1e18, cs:1d49: the long fade out
  Task waitTicks(u32 ticks);
  Task banners(u16 which);         ///< cs:2572
  Task rubOutBanners();            ///< cs:25bc
  Task page();                     ///< cs:3218
  Task optionsMenu();              ///< cs:43f4
  Task chooseOptions();            ///< cs:3f4d
  Task say(u16 y, u16 x, u16 text);  ///< cs:42de
  Task turnOptions(int page);      ///< the other page of options in place of the one shown
  Task closePage();                ///< cs:3529
  void frameCallback();            ///< cs:2bb2
  void midFrameCallback();         ///< cs:2821
  void scroller();                 ///< cs:2987
  u16 find(u16 segment, const char* tag);
  u16 unpackPlanar(u16 segment, u16 row, bool setColours, HdPicture is = HdPicture::None);   ///< cs:4880
  u16 unpackChunky(u16 segment, u16 row, u16 rows);          ///< cs:4a39, cs:4db0
  void sendColours(u8 first, u16 picture);                   ///< cs:1ed1
  void glyph(u16 y, u16 x, u8 letter);                       ///< cs:2c37
  void text(u16 y, u16 page) { text(y, &ds(page)); }        ///< cs:2cf2
  void text(u16 y, const u8* page);
  void string(u16 y, u16 x, u16 text);                       ///< cs:43cf
  void heading(u16 from, u16 to);                            ///< cs:13ea
  void forget(u16 at, u8 dots = 0xff);                       ///< those dots are no longer a picture's
  void threeColours(int times, int of);                      ///< the letters' three colours, dimmed
  void rubOutText(bool both);                                ///< cs:2684, cs:26b1
  void optionText(int row, u16& text);                       ///< cs:41b2 and its like
  void changeOption(int row);
  u8 takeKey() {
    const u8 k = key_;
    key_ = 0;
    return k;
  }

  Bytes image_;                    ///< the program as loaded
  MusicDriver music_;
  Options options_, saved_;
  int returningFrom_;
  std::array<std::vector<u8>, 4> planes_;
  std::array<u8, 4> latch_{};
  u8 planeMask_ = 0x0f, readPlane_ = 0, setReset_ = 0, enableSetReset_ = 0;
  int writeMode_ = 0;
  Mode mode_ = Mode::Chunky320;
  u16 start_ = 0;
  u16 shownWidth_ = 640;           ///< dots of each row the card shows; the menu opens sideways
  std::array<u8, 768> dac_{};
  u8 colourSelect_ = 0;
  bool selectBits_ = false;        ///< the upper colour bits come from the colour select
  u8 selectTop_ = 0, selectBottom_ = 0;
  std::array<u8, 768> fadeBuffer_{};  ///< cs:20df
  /// For the pictures drawn again at high resolution: which picture each dot of the card's
  /// memory is a dot of, and which dot (picture << 20 | across << 10 | down); 0 for none.
  /// Copies within the memory take it along, and whatever is drawn over a dot takes it away.
  std::vector<u32> from_;
  int slide_ = 0;        ///< which of the five slides is on the screen, or 0
  float level_ = 1.0f;   ///< how far the screen is faded in
  float textLevel_ = 1.0f;  ///< and the pages' letters, with which their heading comes and goes
  bool fadeWhite_ = false;  ///< the fade is out of white, not black

  Task task_;
  std::coroutine_handle<> waiting_;
  Action action_;
  bool done_ = false;
  u8 key_ = 0;                     ///< the last key pressed, until it is asked for
  bool skip_ = false;              ///< cs:365a
  // variables among the code
  bool splitPalette_ = false;      ///< cs:2818
  u16 openWidth_ = 0;              ///< cs:2888
  u16 clock_ = 0;                  ///< cs:288c
  bool slideIn_ = false;           ///< cs:2c32
  u16 slideAt_ = 0;                ///< cs:2c33
  bool scrollerOn_ = false;        ///< cs:2980
  i16 scrollerCount_ = 0x1e0;      ///< cs:297e
  u16 scrollerText_ = intro::code::leftTextMenu, scrollerNow_ = intro::code::leftTextMenu;  ///< cs:2981, cs:2983
  bool scrollerShowsText_ = false; ///< cs:2985: 29df when true, 29fa when not
  u16 drawAt_ = 0, fontAt_ = 0;    ///< cs:2cee, cs:2cf0
  u16 pageAt_ = 2;                 ///< cs:362d
  i16 idle_ = 0;                   ///< cs:1d47
  bool wantOptions_ = false;       ///< cs:486d
  int optionRow_ = 0;              ///< cs:3f4b
  /// The options' two pages of text: the original's (ds:4e40), its six options and a line more
  /// to turn to the other, which has this version's options, as the original's has no room for
  /// them. A line of an option is 0x18 letters, its word from the 0x10th; the first is the third
  /// line, after the heading and an empty one, on both pages.
  std::array<std::vector<u8>, 2> optionsPages_;
  int optionsShown_ = 0;           ///< which of them
  static constexpr int kFirstOptions = 6;  ///< the options on the first page; the rest on the other
  u8* optionWord(int row) {
    const int page = row < kFirstOptions ? 0 : 1, line = row - (page ? kFirstOptions : 0);
    return &optionsPages_[static_cast<std::size_t>(page)][static_cast<std::size_t>(0x1e + line * 0x18)];
  }
  void writeOptionWord(int row);   ///< cs:404a: an option's word into its page's text
  PortableLock portableLock_ = PortableLock::None;
  bool portableError_ = false, portableErrorShown_ = false;
  bool optionsDone_ = false;       ///< cs:4265
  int launch_ = -1;                ///< a table asked for while a page was shown
  // for the tall screen: what the part beside the panel shows, and which rows of the
  // banners have been drawn in
  enum class Showing { Banners, Page, Scores } showing_ = Showing::Banners;
  std::array<bool, 96> bannerTop_{}, bannerBottom_{};
  int stripWidth_ = 0, stripHeight_ = 0;
  bool lastWasScores_ = false;     ///< the page shown last was one of best scores
  void drawScreen(u8* frame, HdFrame* hd, int top) const;
  void drawTallMenu(u8* frame, HdFrame* hd) const;
  u16 lastVolume_ = 0;
};

}  // namespace encore
