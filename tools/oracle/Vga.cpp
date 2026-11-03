#include "Vga.h"

namespace oracle {
namespace {

/// The map mask with every plane on, and a byte with every bit (of the bit mask; of a plane's
/// eight dots).
constexpr u8 kAllPlanes = 0x0f, kAllBits = 0xff;
/// The sequencer's memory mode in mode 13h (chain-4 on, odd/even off, all 256 KB) and in the
/// others; the chain-4 bit.
constexpr u8 kMemoryMode13h = 0x0e, kMemoryModeOther = 0x06, kChain4 = 0x08;
/// The graphics controller's mode: 256 colours, read mode 1, and the write mode's two bits.
constexpr u8 kShift256 = 0x40, kReadMode1 = 0x08;
constexpr int kWriteModeMask = 3;
/// The write modes.
constexpr int kWriteRotated = 0, kWriteLatches = 1, kWriteColour = 2;  // 3: the bit mask from the value
/// The data rotate register: the count in bits 0 to 2, then the function in bits 3 and 4.
constexpr int kRotateMask = 7, kFunctionShift = 3, kFunctionMask = 3;
constexpr int kFunctionAnd = 1, kFunctionOr = 2, kFunctionXor = 3;  // 0 puts the value as it is
/// The scan lines of each row of the picture, less one, in the maximum scan line register.
constexpr int kScanLinesMask = 0x1f;
/// A plane's number (in the chained mode, the address's low two bits pick it; the rest is the
/// place in it), an offset in a plane's 64 KB.
constexpr int kPlaneMask = 3, kChainAddress = 0xfffc, kOffsetMask = 0xffff;
/// Input status 1: the display off (a line's blank, or the retrace) and the vertical retrace.
constexpr u8 kDisplayOff = 0x01, kInRetrace = 0x09;
/// The answers a frame's reads of it get: kRetraceReads in the retrace, then lines; after
/// kLineReads of those with no new frame, another retrace, and so on every kStatusCycle.
constexpr unsigned kRetraceReads = 8, kLineReads = 1000, kStatusCycle = 1024;
/// The bytes of each plane in a row, unless the offset register says otherwise.
constexpr u32 kRowBytes = 80u;

}  // namespace

Vga::Vga() {
  for (auto& p : planes) p.assign(kPlaneBytes, 0);
  seq[seqReg::mapMask] = kAllPlanes;
  gc[gcReg::bitMask] = kAllBits;
}

void Vga::setMode(u8 mode) {
  for (auto& p : planes) p.assign(kPlaneBytes, 0);
  for (u8& v : gc) v = 0;
  gc[gcReg::bitMask] = kAllBits;
  seq[seqReg::mapMask] = kAllPlanes;
  seq[seqReg::memoryMode] = mode == kMode13h ? kMemoryMode13h : kMemoryModeOther;  // chain-4 in mode 13h
  gc[gcReg::mode] = mode == kMode13h ? kShift256 : 0;
  for (u8& v : crtc) v = 0;
  // the line compare at its last, 0x3ff: no split
  crtc[crtcReg::lineCompare] = 0xff;
  crtc[crtcReg::overflow] = kLineCompare8;
  crtc[crtcReg::maxScanLine] = kLineCompare9;
}

u8 Vga::read(u32 offset) {
  offset &= kOffsetMask;
  if (seq[seqReg::memoryMode] & kChain4) return planes[offset & kPlaneMask][offset & kChainAddress];  // chained
  for (int p = 0; p < kPlanes; ++p) latch_[p] = planes[p][offset];
  if (gc[gcReg::mode] & kReadMode1) {  // read mode 1: colour compare
    u8 result = kAllBits;
    for (int p = 0; p < kPlanes; ++p) {
      if (!(gc[gcReg::colourDontCare] & (1 << p))) continue;
      const u8 want = (gc[gcReg::colourCompare] & (1 << p)) ? kAllBits : 0x00;
      result &= static_cast<u8>(~(latch_[p] ^ want));
    }
    return result;
  }
  return latch_[gc[gcReg::readMap] & kPlaneMask];
}

void Vga::write(u32 offset, u8 value) {
  offset &= kOffsetMask;
  if (seq[seqReg::memoryMode] & kChain4) {
    if (seq[seqReg::mapMask] & (1 << (offset & kPlaneMask))) planes[offset & kPlaneMask][offset & kChainAddress] = value;
    return;
  }
  const int mode = gc[gcReg::mode] & kWriteModeMask;
  const u8 mask = gc[gcReg::bitMask];
  const int rot = gc[gcReg::dataRotate] & kRotateMask;
  const int func = (gc[gcReg::dataRotate] >> kFunctionShift) & kFunctionMask;
  auto logical = [&](u8 v, u8 latch) -> u8 {
    switch (func) {
      case kFunctionAnd: return v & latch;
      case kFunctionOr: return v | latch;
      case kFunctionXor: return v ^ latch;
      default: return v;
    }
  };
  for (int p = 0; p < kPlanes; ++p) {
    if (!(seq[seqReg::mapMask] & (1 << p))) continue;
    u8 out = 0;
    switch (mode) {
      case kWriteRotated: {
        u8 v = static_cast<u8>((value >> rot) | (value << (8 - rot)));
        if (gc[gcReg::enableSetReset] & (1 << p)) v = (gc[gcReg::setReset] & (1 << p)) ? kAllBits : 0x00;
        v = logical(v, latch_[p]);
        out = static_cast<u8>((v & mask) | (latch_[p] & ~mask));
        break;
      }
      case kWriteLatches: out = latch_[p]; break;
      case kWriteColour: {
        u8 v = (value & (1 << p)) ? kAllBits : 0x00;
        v = logical(v, latch_[p]);
        out = static_cast<u8>((v & mask) | (latch_[p] & ~mask));
        break;
      }
      default: {
        const u8 rotated = static_cast<u8>((value >> rot) | (value << (8 - rot)));
        const u8 m = rotated & mask;
        const u8 v = (gc[gcReg::setReset] & (1 << p)) ? kAllBits : 0x00;
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
      const u8 v = dac[(dacRead_ * kColourBytes + dacReadStep_) % kDacBytes];
      if (++dacReadStep_ == kColourBytes) { dacReadStep_ = 0; ++dacRead_; }
      return v;
    }
    case vgaPort::status: {
      attrFlip_ = false;
      // Programs wait on these two bits. Within a frame the answers go: retrace for a while
      // (bits 0 and 3), then picture lines, each one read as display then blank.
      const unsigned n = statusReads_++;
      if (n < kRetraceReads) return kInRetrace;
      // After many reads with no new frame, another retrace comes, so that a program that
      // waits for one outside the frame's callbacks is never stuck.
      const unsigned m = (n - kRetraceReads) % kStatusCycle;
      if (m >= kLineReads) return kInRetrace;
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
      dac[(dacWrite_ * kColourBytes + dacWriteStep_) % kDacBytes] = value & kDacMax;
      if (++dacWriteStep_ == kColourBytes) { dacWriteStep_ = 0; ++dacWrite_; }
      break;
    default: break;
  }
}

std::vector<u8> Vga::picture(int height) const {
  std::vector<u8> out(static_cast<std::size_t>(kWidth * height));
  const int split = lineCompare();
  const int scan = (crtc[crtcReg::maxScanLine] & kScanLinesMask) + 1;  // screen lines per picture row
  u32 start = startAddress();
  const u32 pitch = crtc[crtcReg::offset] ? crtc[crtcReg::offset] * 2u : kRowBytes;  // bytes of each plane per row
  int row = 0;
  bool below = false;
  for (int y = 0; y < height; ++y, ++row) {
    if (!below && y * scan > split) {  // below the split, memory restarts at 0
      below = true;
      start = 0;
      row = 0;
    }
    const u32 base = start + static_cast<u32>(row) * pitch;
    for (int x = 0; x < kWidth; ++x) out[static_cast<std::size_t>(y * kWidth + x)] = planes[x & 3][(base + x / 4) & kOffsetMask];
  }
  return out;
}

}  // namespace oracle
