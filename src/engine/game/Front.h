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

class Front {
 public:
  static constexpr int kWidth = 640, kHeight = 480;

  /// `prg` is INTRO.PRG; `module` is INTRO.MOD the first time and MOD2.MOD after a table.
  /// `returningFrom` is the table come back from (0 to 3), or -1 the first time, which shows
  /// the slides. `scores` are the four tables' best scores, for its pages of them.
  Front(ByteView prg, ByteView module, const Config& config, int returningFrom);
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

  /// ARTWORK, AUDIO and DOT MATRIX as they are now, whatever the options page last said (keys
  /// change them anywhere): the page shows them so the next time it opens.
  void setLooks(bool originalSound, bool originalPictures, bool dotMatrixTop) {
    options_.originalSound = saved_.originalSound = originalSound;
    options_.originalPictures = saved_.originalPictures = originalPictures;
    options_.dotMatrixTop = saved_.dotMatrixTop = dotMatrixTop;
  }
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
  u8& ds(u16 at) { return image_[0x800 + at]; }
  u16 dsw(u16 at) { return static_cast<u16>(ds(at) | (ds(static_cast<u16>(at + 1)) << 8)); }
  void setw(u16 at, u16 v) {
    ds(at) = static_cast<u8>(v);
    ds(static_cast<u16>(at + 1)) = static_cast<u8>(v >> 8);
  }
  u8& farByte(u16 segment, u32 offset) { return image_[(std::size_t{segment} * 16 + offset) % image_.size()]; }
  u8 cs(u16 at) const { return image_[std::size_t{0x364c} * 16 + at]; }

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
  void showOption(int row);                                  ///< cs:4101
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
  u16 scrollerText_ = 0x288e, scrollerNow_ = 0x288e;  ///< cs:2981, cs:2983
  bool scrollerShowsText_ = false; ///< cs:2985: 29df when true, 29fa when not
  u16 drawAt_ = 0, fontAt_ = 0;    ///< cs:2cee, cs:2cf0
  u16 pageAt_ = 2;                 ///< cs:362d
  i16 idle_ = 0;                   ///< cs:1d47
  bool wantOptions_ = false;       ///< cs:486d
  int optionRow_ = 0;              ///< cs:3f4b
  /// The options page's text: the original's (ds:4e40), with this version's two more options
  /// under its six, which the original's place has no room for. A line of an option is 0x18
  /// letters, its word from the 0x10th.
  std::vector<u8> optionsPage_;
  u8* optionWord(int row) { return &optionsPage_[static_cast<std::size_t>(0x1e + row * 0x18)]; }
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
