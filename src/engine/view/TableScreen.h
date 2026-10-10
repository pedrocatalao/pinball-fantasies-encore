#pragma once
// The table as the player sees it, drawn from what the engine knows: the window on the
// playfield's picture, the ball, the flippers, the plunger, and the display's dots below.
// The original draws all of this into the video card's memory as it goes; the engine does not
// draw, so the picture is made here, once a frame, from where things are.
//
// For the pictures drawn again at high resolution (gfx/HdLayer.h) it also says, dot by dot,
// which dot of the playfield's picture each is and how lit, and where the flippers and the
// ball are, as pictures to be turned and placed.
#include "engine/data/BallSprite.h"
#include "engine/data/TableData.h"
#include "engine/table/Engine.h"
#include "gfx/HdLayer.h"

namespace encore {

/// A picture cut from the original's, to be drawn at any size: red, green, blue and how solid.
struct Cutout {
  int width = 0, height = 0;
  Bytes rgba;
};

class TableScreen {
 public:
  static constexpr int kWidth = 320;
  static constexpr int kDisplayRows = 33;

  TableScreen(const std::filesystem::path& prg, int table);
  TableScreen(Bytes prg, int table);

  /// The screen's rows in the mode the options chose: 240, or 350.
  static int height(bool highResolution) { return highResolution ? 350 : 240; }

  /// What is to be seen of the table.
  struct View {
    int height = 350;  ///< the screen's rows: the display's 33, and above them the playfield's
    int top = 0;       ///< the row of the playfield's picture at the top of the screen
    /// 0: the lights as the game has them; 1: every one lit; 2: every one out.
    int lamps = 0;
    bool ballTrail = true;
  };
  /// One frame: `height` rows of 320 colours of the palette (see colours()). With `hd`, also
  /// what the high-resolution pictures need; the ball is then left out of the frame, to be
  /// drawn as a picture of its own.
  void draw(Engine& engine, u8* frame, const View& view, HdFrame* hd = nullptr) const;
  /// The same, looking where the original would.
  void draw(Engine& engine, u8* frame, int height) const {
    draw(engine, frame, View{height, static_cast<i16>(engine.screenRow()) - kDisplayRows});
  }
  /// The 256 colours the frame is in.
  void colours(Engine& engine, Rgb* out, int lamps = 0) const;

  /// Follows a table from before its start: the flippers are drawn into the playfield's
  /// picture as the engine says they move. The engine must outlive this.
  void attach(Engine& engine);
  /// The picture's first row as the artwork has it; the original rubs it out as the table starts.
  bool keepTopRow = false;
  /// The artwork where the plunger has moved down from, rather than the dark the original leaves.
  bool artBehindPlunger = false;

  /// Once the table has started: works out what the high-resolution pictures need.
  void started(Engine& engine);
  /// Once a frame: which of the two maps of what hides the ball the picture goes by. The
  /// engine changes the ball over from the ramps to the playfield where its walls must change,
  /// which can be a little before the ball is out from under the ramp it leaves (at the top of
  /// Stones 'n Bones' plunger rail, the ramp's middle wire would be drawn over it); the
  /// picture goes on as if on the ramps until the playfield's map hides no more of the ball
  /// than the ramps' does, or the ball has gone a little way on. Only the picture: the game is
  /// the same.
  void follow(Engine& engine);

  /// Each flipper as it lies at rest, cut out of the playfield's picture; and whether it is
  /// one of the left key's.
  std::vector<Cutout> flipperPictures(Engine& engine) const;
  std::vector<bool> flipperIsLeft(Engine& engine) const;
  Cutout ballPicture(Engine& engine) const;

  const TableData& data() const { return data_; }

 private:
  /// A flipper taken out of its artwork.
  struct FlipperArt {
    u16 record = 0;            ///< the table's record of it
    int x = 0, y = 0, w = 0, h = 0, steps = 0;  ///< its rectangle on the playfield, and how many pictures it has
    Bytes shape;               ///< per step and dot: the flipper is there
    Bytes atRest;              ///< its rectangle of the playfield with it at rest, as colours
    Bytes background;          ///< the artwork with the flipper taken out
    Bytes covered;             ///< per dot: the flipper reaches it at some angle
    Bytes rest;                ///< per dot: the flipper covers it at rest
    std::vector<float> angle;  ///< radians, per step
    float axisX = 0, axisY = 0;  ///< what it turns about, in the rectangle's dots
  };
  void turnFlipper(Engine& engine, Bytes& picture, u16 record, u16 was, u16 now) const;
  void buildCover(Engine& engine);
  void buildFlipperArt(Engine& engine);
  void buildLampAreas(Engine& engine);
  float flipperAngle(Engine& engine, const FlipperArt& art) const;
  void lampsLit(Engine& engine, int lamps, std::array<bool, 256>& lit) const;

  TableData data_;
  BallSprite ball_;
  Bytes picture_;  ///< the playfield as it now is on the screen: its picture, and the flippers as they stand
  std::array<Bytes, 2> hides_;      ///< per dot: the artwork hides the ball there, on the playfield and on the ramps
  std::array<Bytes, 2> cover_;      ///< and how much of it, 0 to 255
  bool drawnOnRamps_ = false;       ///< the picture hides the ball as on the ramps (follow())
  int stillOnRamps_ = 0;            ///< frames it has gone on doing so after the engine changed over
  std::array<int, 2> changedAt_{};  ///< where the ball was drawn when the engine changed over
  std::vector<FlipperArt> art_;
  Bytes lampAreas_;                 ///< per dot of the playfield: the lamp it belongs to, or 0
  std::vector<u16> lamps_;          ///< each lamp's record of colours in the table's memory
};

}  // namespace encore
