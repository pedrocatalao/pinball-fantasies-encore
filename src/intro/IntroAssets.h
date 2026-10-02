#pragma once
// Pictures, text and timing tables for the opening slideshow and the table chooser, read
// from INTRO.PRG (translated from pfr's src/assets/intro.rs).
#include <array>
#include <variant>
#include <vector>

#include "assets/Grid.h"

namespace pfr {

struct IntroImage {
  Grid8 data;
  std::vector<Rgb> cmap;
  bool operator==(const IntroImage&) const = default;
};

struct Slide {
  IntroImage image;
  u8 gapFrames = 0, fadeInFrames = 0, fadeOutFrames = 0;
  u32 fadeOutTick = 0;   ///< music tick at which the slide starts to fade
  bool fadeFromWhite = false;
  bool operator==(const Slide&) const = default;
};

/// A page of the chooser: high scores for two tables, or lines of text.
struct TextPage {
  bool hiScores = false;
  bool tables34 = false;  ///< hiScores: tables 3 and 4 rather than 1 and 2
  std::vector<std::vector<u8>> lines;
  bool operator==(const TextPage&) const = default;
};

struct IntroAssets {
  static IntroAssets load(ByteView prg);

  std::vector<Slide> slides;
  IntroImage left;
  std::array<IntroImage, 4> tables;
  IntroImage hiscoresLq, hiscoresHq, fontLq, fontHq;
  std::vector<TextPage> textPages;
  std::vector<std::vector<u8>> leftTextMenu, leftTextOptions;
  std::vector<u8> warpTable;  ///< frame at which each of the 95 rows appears
  u8 warpFrames = 0;

  bool operator==(const IntroAssets&) const = default;
};

}  // namespace pfr
