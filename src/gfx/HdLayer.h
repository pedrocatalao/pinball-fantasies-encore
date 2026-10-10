#pragma once
// Replacement pictures in high resolution. A screen that draws one of the original pictures
// also records, for every pixel it drew from it, which picture it was and where in it. The
// renderer then draws the replacement over exactly those pixels at the window's resolution,
// so whatever the screen does with the picture (warps, slides, text drawn over it) still
// applies, and everything else stays as the original drew it.
#include <array>
#include <vector>

#include "core/Types.h"

namespace encore {

enum class HdPicture : u16 {
  None,
  Slide1, Slide2, Slide3, Slide4, Slide5,  // the opening slideshow
  Left,                                    // the menu's side panel
  Table1, Table2, Table3, Table4,          // the menu's table banners
  HiScores,                                // the high-score pages' heading
  // Each table's playfield with every lamp lit, and with every lamp off.
  Playfield1On, Playfield2On, Playfield3On, Playfield4On,
  Playfield1Off, Playfield2Off, Playfield3Off, Playfield4Off,
  Plunger,                                 // the plunger, the same on every table
  LeftRepeat,                              // a strip of the side panel's plain part, repeated down the tall menu
  Count,
};

/// The file name (without .png) a replacement is read from.
inline const char* hdPictureName(HdPicture p) {
  static constexpr const char* kNames[] = {"", "slide1", "slide2", "slide3", "slide4", "slide5",
                                           "left", "table1", "table2", "table3", "table4", "hiscores",
                                           "playfield1_on", "playfield2_on", "playfield3_on", "playfield4_on",
                                           "playfield1_off", "playfield2_off", "playfield3_off", "playfield4_off",
                                           "plunger", "left_repeat"};
  return kNames[static_cast<std::size_t>(p)];
}

/// One screen pixel: the position of its top-left corner in the original picture, in eighths
/// of a pixel, and the picture. The picture's bits 8 and 9 say that a screen pixel covers half
/// a picture pixel across or down (the picture is drawn doubled in that direction).
struct HdPixel {
  static constexpr u16 kHidesBall = 1;  ///< the artwork here covers the ball (a ramp over it)
  /// How much of the artwork covers the ball, 0 to 255, in the flags' high byte. The original
  /// hides the ball pixel by pixel, and dithers the cover where it is meant to be seen through
  /// (the criss-cross rail on Stones n Bones); as a fraction that reads as transparency
  /// instead, which is what a picture of the ball at any size needs.
  static constexpr int kCoverShift = 8;
  u16 x8 = 0, y8 = 0, picture = 0, flags = 0;
};

/// A flipper turned to its angle, or the ball: one picture drawn over the scene. The sprites
/// of a frame are drawn in order, so the ball goes last and passes in front of the flippers.
struct HdSprite {
  static constexpr u16 kBall = 3;  ///< the picture slot the ball uses; the flippers take 0 to 2
  u16 picture = 0;                           ///< which of the frame's sprite pictures
  float pivotFrameX = 0, pivotFrameY = 0;    ///< the hinge, in screen pixels
  float pivotSpriteX = 0, pivotSpriteY = 0;  ///< the hinge, as a fraction of the picture
  float scaleX = 1, scaleY = 1;              ///< fractions of the picture per screen pixel
  float angle = 0;                           ///< radians away from the resting position
  float clipTop = 0, clipBottom = 0;         ///< the screen rows it may be drawn in
  u16 hiddenBy = 0;                          ///< HdPixel flags that keep it from being drawn
  float opacity = 1;                         ///< 1 = solid; less for the ball's trail
};

struct HdFrame {
  static constexpr u16 kHalfX = 0x100, kHalfY = 0x200;
  /// The dot is one line of the picture drawn out downwards: every screen row of it shows the
  /// same line. (Only for pictures without a lit version, whose bits this shares.)
  static constexpr u16 kFlatY = 0x400;
  /// How lit a playfield pixel is, in the picture's top bits: the renderer blends between the
  /// lit and unlit pictures by it, read smoothly between screen pixels, so a lamp's edge does
  /// not step along the original's pixels.
  static constexpr int kLitShift = 10;
  static constexpr u16 kLitMax = 0x3f;
  static constexpr std::size_t kCount = static_cast<std::size_t>(HdPicture::Count);

  int width = 0, height = 0;
  std::vector<HdPixel> map;                       ///< width x height, top row first
  std::array<std::array<u16, 2>, kCount> size{};  ///< each picture's original size
  u32 used = 0;                                   ///< bit per picture drawn this frame
  /// Per picture: 0, or how many of its rows a screen row covers when it is a strip repeated
  /// downwards (its place down the strip is then counted in the picture's own rows).
  std::array<float, kCount> rowStep{};
  std::array<float, kCount> fade{};               ///< per picture: 1 = as drawn, 0 = all fadeColor
  Rgb fadeColor{};
  std::vector<HdSprite> sprites;                  ///< drawn over the pictures, in order
  float spriteTint = 1.0f;                        ///< the screen's fade, applied to the sprites
  u8 ownSprites = 0;                              ///< bit per flipper with a picture of its own, not cut from the artwork
  bool ballTrail = true;                          ///< draw the fading ghosts behind the ball
  u8 ballLayer = 0;                               ///< the ball is on the playfield (0) or the ramps (1)

  void reset(int w, int h) {
    width = w;
    height = h;
    map.assign(static_cast<std::size_t>(w) * h, HdPixel{});
    used = 0;
    fade.fill(1.0f);
    rowStep.fill(0.0f);
    fadeColor = {};
    sprites.clear();
    spriteTint = 1.0f;
  }
};

}  // namespace encore
