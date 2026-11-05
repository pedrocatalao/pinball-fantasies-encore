#pragma once
// The video card as the game's programs use it: four planes of 64 KB, written through the
// sequencer's and graphics controller's registers, and read back, which the tables do to
// test the ball against masks they keep off screen.
#include <array>
#include <vector>

#include "Cpu.h"

namespace oracle {

/// The card's ports (at the colour addresses, 3dx).
namespace vgaPort {
constexpr u16 first = 0x3c0, last = 0x3df;  ///< every port the card answers
constexpr u16 attr = 0x3c0;  ///< the attribute controller: an index, then its value, in turn
constexpr u16 seqIndex = 0x3c4, seqData = 0x3c5;
constexpr u16 dacReadIndex = 0x3c7, dacWriteIndex = 0x3c8, dacData = 0x3c9;
constexpr u16 gcIndex = 0x3ce, gcData = 0x3cf;
constexpr u16 crtcIndex = 0x3d4, crtcData = 0x3d5;
constexpr u16 status = 0x3da;  ///< input status 1: the retrace; reading it sets the attribute port to an index
}  // namespace vgaPort

/// The registers used, by their index: the sequencer's, the graphics controller's and the
/// CRT controller's.
namespace seqReg {
constexpr int mapMask = 2, memoryMode = 4;
}  // namespace seqReg
namespace gcReg {
constexpr int setReset = 0, enableSetReset = 1, colourCompare = 2, dataRotate = 3, readMap = 4, mode = 5, colourDontCare = 7, bitMask = 8;
}  // namespace gcReg
namespace crtcReg {
constexpr int overflow = 0x07, maxScanLine = 0x09, startHigh = 0x0c, startLow = 0x0d, offset = 0x13, lineCompare = 0x18;
}  // namespace crtcReg

class Vga {
 public:
  /// The BIOS's mode 13h: 320 by 200 in 256 colours.
  static constexpr u8 kMode13h = 0x13;
  /// Bits of the CRT controller's overflow and maximum scan line registers: bits 8 and 9 of
  /// the line compare.
  static constexpr int kLineCompare8 = 0x10, kLineCompare9 = 0x40;

  Vga();
  u8 read(u32 offset);              ///< offset into the 64 KB window at A000:0
  void write(u32 offset, u8 value);
  u8 portIn(u16 port);
  void portOut(u16 port, u8 value);
  /// A mode set through the BIOS: kMode13h puts the card in its chained 256-colour state.
  void setMode(u8 mode);
  /// Called once per frame: the retrace begins.
  void startFrame() { statusReads_ = 0; }

  std::array<std::vector<u8>, 4> planes;
  std::array<u8, 768> dac{};  ///< 6-bit components
  u8 seq[8]{}, gc[16]{}, crtc[32]{}, attr[32]{};

  u16 startAddress() const { return static_cast<u16>((crtc[crtcReg::startHigh] << 8) | crtc[crtcReg::startLow]); }
  int lineCompare() const { return crtc[crtcReg::lineCompare] | ((crtc[crtcReg::overflow] & kLineCompare8) << 4) | ((crtc[crtcReg::maxScanLine] & kLineCompare9) << 3); }
  /// The picture as shown: `height` rows of 320 colour indices, read as the unchained
  /// 256-colour mode lays them out, with the split screen the tables use for the display.
  std::vector<u8> picture(int height) const;

 private:
  u8 latch_[4]{};
  u8 seqIndex_ = 0, gcIndex_ = 0, crtcIndex_ = 0, attrIndex_ = 0;
  bool attrFlip_ = false;
  u8 dacWrite_ = 0, dacRead_ = 0;
  int dacWriteStep_ = 0, dacReadStep_ = 0;
  unsigned statusReads_ = 0;
};

}  // namespace oracle
