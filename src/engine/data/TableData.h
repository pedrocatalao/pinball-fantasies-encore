#pragma once
// Everything a table needs, pulled out of TABLEn.PRG: the 320x576 playfield picture,
// its palette and colour cycles, the collision and occlusion masks, flipper frames,
// sprite banks, the data segment (tables and initial variables) and the message strings.
#include <filesystem>
#include <map>

#include "data/IffImage.h"
#include "engine/data/MzImage.h"
#include "gfx/Palette.h"
#include "engine/sim/Mask.h"

namespace encore {

struct TableData {
  static constexpr int kWidth = 320;
  static constexpr int kHeight = 576;
  static constexpr int kStrips = 4;  ///< the playfield's picture comes in four

  int index = 0;                  ///< 0..3
  std::string name;               ///< "Party Land", ...
  MzImage image;                  ///< keeps the raw executable bytes alive
  Bytes playfield;                ///< 320*576 palette indices (four strips joined)
  std::vector<Rgb> palette;       ///< the 256-colour palette in use
  std::vector<ColorRange> colorRanges;
  std::array<IffImage, kStrips> strips;

  // Collision masks (1 = solid).
  Mask walls;      ///< static playfield
  Mask dynamic;    ///< bumpers, slingshots, targets
  Mask ramps;      ///< upper level / ramps
  // Ball occlusion masks, copied to VRAM by the original at startup (in file order).
  Mask occlusionA, occlusionB, occlusionC;
  // Flipper frame stacks, in the order the flipper records reference them.
  /// Raw flipper frame stacks. The blocks are padded, so their size does not reveal the
  /// row pitch; each flipper record carries the geometry and builds its own Mask.
  std::vector<ByteView> flipperBlocks;

  ByteView code;          ///< code segment
  ByteView dataSegment;   ///< variables + tables
  ByteView sharedCode;    ///< second code segment
  ByteView spriteBank;    ///< segment "2055" in TABLE1
  ByteView ballGraphics;  ///< segment "2f93" in TABLE1
  ByteView rampMask;      ///< segment "358b" in TABLE1
  /// Where the ball may not be drawn, because playfield features pass over it. One map per
  /// layer, held inside the ball-graphics block and spilling into the one after it.
  Mask occlusionGround, occlusionOverhead;
  ByteView frameTable;    ///< segment "0c0e" in TABLE1
  ByteView smallImageA;   ///< the four-pixel dictionary the table animations index into
  ByteView plungerImage;  ///< 10 x 23 chunky pixels of the plunger

  static TableData load(const std::filesystem::path& prg, int index);
  /// The same from the file's bytes; `name` is for saying what is wrong with them.
  static TableData parse(Bytes prg, const std::string& name, int index);
};

}  // namespace encore
