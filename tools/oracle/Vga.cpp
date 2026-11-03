#include "Vga.h"

namespace oracle {
namespace {

/// The sequencer's memory mode in mode 13h (chain-4 on, odd/even off, all 256 KB) and in the
/// others; the chain-4 bit.
constexpr u8 kMemoryMode13h = 0x0e, kMemoryModeOther = 0x06, kChain4 = 0x08;
/// The graphics controller's mode: 256 colours, read mode 1, and the write mode's two bits.
constexpr u8 kShift256 = 0x40, kReadMode1 = 0x08;
/// The write modes.
constexpr int kWriteRotated = 0, kWriteLatches = 1, kWriteColour = 2;  // 3: the bit mask from the value
/// The data rotate register: the count in bits 0 to 2, then the function in bits 3 and 4.
constexpr int kFunctionAnd = 1, kFunctionOr = 2, kFunctionXor = 3;  // 0 puts the value as it is
/// Input status 1: the display off (a line's blank, or the retrace) and the vertical retrace.
constexpr u8 kDisplayOff = 0x01, kInRetrace = 0x09;
}  // namespace

Vga::Vga() {
  for (auto& p : planes) p.assign(0x10000, 0);
  seq[seqReg::mapMask] = 0x0f;
  gc[gcReg::bitMask] = 0xff;
}

void Vga::setMode(u8 mode) {
  for (auto& p : planes) p.assign(0x10000, 0);
  for (u8& v : gc) v = 0;
  gc[gcReg::bitMask] = 0xff;
  seq[seqReg::mapMask] = 0x0f;
  seq[seqReg::memoryMode] = mode == kMode13h ? kMemoryMode13h : kMemoryModeOther;  // chain-4 in mode 13h
  gc[gcReg::mode] = mode == kMode13h ? kShift256 : 0;
  for (u8& v : crtc) v = 0;
  // the line compare at its last, 0x3ff: no split
  crtc[crtcReg::lineCompare] = 0xff;
  crtc[crtcReg::overflow] = kLineCompare8;
  crtc[crtcReg::maxScanLine] = kLineCompare9;
}

u8 Vga::read(u32 offset) {
  offset &= 0xffff;
  if (seq[seqReg::memoryMode] & kChain4) return planes[offset & 3][offset & 0xfffc];  // chained
  for (int p = 0; p < 4; ++p) latch_[p] = planes[p][offset];
  if (gc[gcReg::mode] & kReadMode1) {  // read mode 1: colour compare
    u8 result = 0xff;
    for (int p = 0; p < 4; ++p) {
      if (!(gc[gcReg::colourDontCare] & (1 << p))) continue;
      const u8 want = (gc[gcReg::colourCompare] & (1 << p)) ? 0xff : 0x00;
      result &= static_cast<u8>(~(latch_[p] ^ want));
    }
    return result;
  }
  return latch_[gc[gcReg::readMap] & 3];
}

void Vga::write(u32 offset, u8 value) {
  offset &= 0xffff;
  if (seq[seqReg::memoryMode] & kChain4) {
    if (seq[seqReg::mapMask] & (1 << (offset & 3))) planes[offset & 3][offset & 0xfffc] = value;
    return;
  }
  const int mode = gc[gcReg::mode] & 3;
  const u8 mask = gc[gcReg::bitMask];
  const int rot = gc[gcReg::dataRotate] & 7;
  const int func = (gc[gcReg::dataRotate] >> 3) & 3;
  auto logical = [&](u8 v, u8 latch) -> u8 {
    switch (func) {
      case kFunctionAnd: return v & latch;
      case kFunctionOr: return v | latch;
      case kFunctionXor: return v ^ latch;
      default: return v;
    }
  };
  for (int p = 0; p < 4; ++p) {
    if (!(seq[seqReg::mapMask] & (1 << p))) continue;
    u8 out = 0;
    switch (mode) {
      case kWriteRotated: {
        u8 v = static_cast<u8>((value >> rot) | (value << (8 - rot)));
        if (gc[gcReg::enableSetReset] & (1 << p)) v = (gc[gcReg::setReset] & (1 << p)) ? 0xff : 0x00;
        v = logical(v, latch_[p]);
        out = static_cast<u8>((v & mask) | (latch_[p] & ~mask));
        break;
      }
      case kWriteLatches: out = latch_[p]; break;
      case kWriteColour: {
        u8 v = (value & (1 << p)) ? 0xff : 0x00;
        v = logical(v, latch_[p]);
        out = static_cast<u8>((v & mask) | (latch_[p] & ~mask));
        break;
      }
      default: {
        const u8 rotated = static_cast<u8>((value >> rot) | (value << (8 - rot)));
        const u8 m = rotated & mask;
        const u8 v = (gc[gcReg::setReset] & (1 << p)) ? 0xff : 0x00;
        out = static_cast<u8>((v & m) | (latch_[p] & ~m));
        break;
      }
    }
    planes[p][offset] = out;
  }
}

u8 Vga::portIn(u16 port) {
  switch (port) {
    case vgaPort::seqData: return seq[seqIndex_ & 7];
    case vgaPort::gcData: return gc[gcIndex_ & 15];
    case vgaPort::crtcData: return crtc[crtcIndex_ & 31];
    case vgaPort::dacData: {
      const u8 v = dac[(dacRead_ * 3 + dacReadStep_) % 768];
      if (++dacReadStep_ == 3) { dacReadStep_ = 0; ++dacRead_; }
      return v;
    }
    case vgaPort::status: {
      attrFlip_ = false;
      // Programs wait on these two bits. Within a frame the answers go: retrace for a while
      // (bits 0 and 3), then picture lines, each one read as display then blank.
      const unsigned n = statusReads_++;
      if (n < 8u) return kInRetrace;
      // After many reads with no new frame, another retrace comes, so that a program that
      // waits for one outside the frame's callbacks is never stuck.
      const unsigned m = (n - 8u) % 1024u;
      if (m >= 1000u) return kInRetrace;
      return (m & 1) ? kDisplayOff : 0x00;
    }
    default: return kOpenBus;
  }
}

void Vga::portOut(u16 port, u8 value) {
  switch (port) {
    case vgaPort::seqIndex: seqIndex_ = value; break;
    case vgaPort::seqData: seq[seqIndex_ & 7] = value; break;
    case vgaPort::gcIndex: gcIndex_ = value; break;
    case vgaPort::gcData: gc[gcIndex_ & 15] = value; break;
    case vgaPort::crtcIndex: crtcIndex_ = value; break;
    case vgaPort::crtcData: crtc[crtcIndex_ & 31] = value; break;
    case vgaPort::attr:
      if (!attrFlip_) attrIndex_ = value;
      else attr[attrIndex_ & 31] = value;
      attrFlip_ = !attrFlip_;
      break;
    case vgaPort::dacReadIndex: dacRead_ = value; dacReadStep_ = 0; break;
    case vgaPort::dacWriteIndex: dacWrite_ = value; dacWriteStep_ = 0; break;
    case vgaPort::dacData:
      dac[(dacWrite_ * 3 + dacWriteStep_) % 768] = value & 0x3f;
      if (++dacWriteStep_ == 3) { dacWriteStep_ = 0; ++dacWrite_; }
      break;
    default: break;
  }
}

std::vector<u8> Vga::picture(int height) const {
  std::vector<u8> out(static_cast<std::size_t>(320 * height));
  const int split = lineCompare();
  const int scan = (crtc[crtcReg::maxScanLine] & 0x1f) + 1;  // screen lines per picture row
  u32 start = startAddress();
  const u32 pitch = crtc[crtcReg::offset] ? crtc[crtcReg::offset] * 2u : 80u;  // bytes of each plane per row
  int row = 0;
  bool below = false;
  for (int y = 0; y < height; ++y, ++row) {
    if (!below && y * scan > split) {  // below the split, memory restarts at 0
      below = true;
      start = 0;
      row = 0;
    }
    const u32 base = start + static_cast<u32>(row) * pitch;
    for (int x = 0; x < 320; ++x) out[static_cast<std::size_t>(y * 320 + x)] = planes[x & 3][(base + x / 4) & 0xffff];
  }
  return out;
}

}  // namespace oracle
