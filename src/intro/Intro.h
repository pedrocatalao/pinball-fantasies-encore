#pragma once
// The opening slideshow and the table chooser with its text, high-score and options pages
// (translated from pfr's src/intro.rs). Drawn on a 640x480 palette screen, or 640x960
// when the full-height table mode is chosen.
#include <functional>
#include <memory>
#include <optional>

#include "game/Config.h"
#include "gfx/HdLayer.h"
#include "intro/IntroAssets.h"
#include "sound/Player.h"
#include "table/Keys.h"

namespace pfr {

struct IntroAction {
  enum class Kind : u8 { None, OpenTable, SaveOptions, Quit } kind = Kind::None;
  int table = 0;  ///< OpenTable: 0..3
};

class Intro {
 public:
  /// `returningFrom`: the table just left (skips the slideshow), or -1 at start-up.
  Intro(IntroAssets assets, ByteView module, const Config& config, int returningFrom);
  /// Straight from INTRO.PRG, as the tests do.
  Intro(ByteView prg, ByteView module, const Config& config, int returningFrom);

  IntroAction runFrame();
  void handleKey(Key key, bool pressed);
  /// `hd`, if given, receives where each original picture was drawn, for replacements.
  void render(u8* pixels, Rgb* palette, HdFrame* hd = nullptr) const;

  int width() const { return 640; }
  int height() const { return vertical() ? 960 : 480; }
  const Options& options() const { return config_.options; }
  Player& player() { return *player_; }

 private:
  enum class Press : u8 { None, Table, Options, Enter, Space, Escape, Up, Down, Option };
  enum class Next : u8 { SkipToTables, SkipToText, Options, Table };
  enum class StateKind : u8 {
    Slide, InitDelay, Left, TablesGap, TablesWarpIn, Tables, TablesWarpOut, TablesFadeOut, TextGap, TextFadeIn,
    Text, TextFadeOut, OptionsGap, OptionsFadeIn, Options, OptionsFadeOut, FadeOut,
  };
  enum class SlideStage : u8 { Gap, FadeIn, Show, FadeOut };
  enum class LeftKind : u8 { None, Image, ImageOut, TextIn, Text, TextOut };

  struct State {
    StateKind kind = StateKind::Slide;
    u16 n = 0;                 ///< the state's counter (for Options: the cursor)
    std::size_t slide = 0;
    SlideStage stage = SlideStage::Gap;
    Next next = Next::SkipToTables;
    IntroAction action;        ///< what a fade-out finishes with
  };

  bool vertical() const { return config_.options.resolution == Resolution::Full; }
  void clearLeft(u8* data, int num) const;
  void renderLeftText(u8* data, int baseY, int num, bool isOptions) const;
  void unclearLeft(u8* data, int num) const;
  void renderLeft(u8* data, Rgb* pal, int offset) const;
  void renderTable(u8* data, Rgb* pal, const std::function<bool(int)>& f, int table, int base, bool flip) const;
  void renderTables(u8* data, Rgb* pal, const std::function<bool(int)>& f) const;
  void renderChar(u8* data, const IntroImage& font, u8 chr, int x, int y) const;
  void renderLine(u8* data, const IntroImage& font, const std::vector<u8>& line, int y) const;
  void renderHiScores(u8* data, const IntroImage& font, int table, int y) const;
  void renderText(u8* data, Rgb* pal, bool lq) const;
  void renderOptions(u8* data, Rgb* pal, bool lq, std::optional<u8> cursor) const;
  void nextPage();
  void hdUse(HdPicture p, const IntroImage& img) const;
  void hdMark(int pos, HdPicture p, int x8, int y8, u16 flags) const;
  void hdClear(int pos) const;
  void handleOption(u8 which);

  std::unique_ptr<Player> player_;
  IntroAssets assets_;
  Config config_;
  State state_;
  std::size_t textPage_ = 0;
  Press key_ = Press::None;
  int keyTable_ = 0;
  u8 keyOption_ = 0;
  LeftKind left_ = LeftKind::None;
  u16 leftN_ = 0;
  bool leftOptions_ = false;
  bool leftIsOptions_ = false;
  mutable HdFrame* hd_ = nullptr;  ///< set only while render() runs
};

}  // namespace pfr
