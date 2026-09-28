#include "assets/TableAssets.h"

#include <algorithm>
#include <array>
#include <map>
#include <utility>

#include "data/IffImage.h"

namespace pfr {

namespace {

using Physmaps = std::array<Grid8, 2>;

std::size_t li(Layer l) { return static_cast<std::size_t>(l); }

void check(bool ok, const char* what) {
  if (!ok) throw DataError(std::string("unexpected table data: ") + what);
}

Rgb fixupColorA(u8 r, u8 g, u8 b) {
  auto f = [](u8 v) { return static_cast<u8>((v * 0xa2) >> 6); };
  return {f(r), f(g), f(b)};
}
Rgb fixupColorB(u8 r, u8 g, u8 b) {
  auto f = [](u8 v) { return static_cast<u8>((v << 2) | (v >> 4)); };
  return {f(r), f(g), f(b)};
}

TRect readRect(const Exe& exe, u16 off) {
  return {exe.dataWordS(off), exe.dataWordS(static_cast<u16>(off + 2)), exe.dataWordS(static_cast<u16>(off + 4)),
          exe.dataWordS(static_cast<u16>(off + 6))};
}

Jingle readJingle(const Exe& exe, u16 off) {
  return {exe.dataByte(off), exe.dataByte(static_cast<u16>(off + 1)), exe.dataByte(static_cast<u16>(off + 2))};
}

Sfx readSfx(const Exe& exe, u16 off) {
  check(exe.dataByte(static_cast<u16>(off + 2)) == 0, "sfx record");
  return {exe.dataByte(off), exe.dataByte(static_cast<u16>(off + 1)), exe.dataByte(static_cast<u16>(off + 3))};
}

// ---- graphics -----------------------------------------------------------------------

void extractMainBoard(const Exe& exe, int table, TableAssets& a) {
  static constexpr u16 kSegs[4][4] = {{0x5224, 0x5947, 0x617b, 0x6a9c},
                                     {0x5054, 0x5820, 0x5fe4, 0x6791},
                                     {0x4c96, 0x5221, 0x5a4b, 0x632d},
                                     {0x4ba1, 0x5480, 0x5d87, 0x66c2}};
  a.mainBoard = Grid8(320, 576);
  for (int s = 0; s < 4; ++s) {
    const auto img = decodeIff(exe.segment(kSegs[table][s]));
    check(img && img->width == 320 && img->height >= 144, "playfield strip");
    for (int y = 0; y < 144; ++y)
      for (int x = 0; x < 320; ++x) a.mainBoard(x, s * 144 + y) = img->at(x, y);
    if (s == 3) a.palette = img->palette;
  }
  a.palette.resize(256);
}

void extractLights(const Exe& exe, int table, TableAssets& a) {
  static constexpr u16 kOff[4] = {0x12bd, 0xfdc, 0xd8b, 0x11d0};
  static constexpr u16 kNum[4] = {56, 67, 38, 44};
  const u16 tableOff = kOff[table], num = kNum[table];
  for (u16 i = 0; i < num; ++i) {
    const u16 off = exe.dataWord(static_cast<u16>(tableOff + i * 2));
    Light l;
    l.baseIndex = exe.dataByte(off);
    const u16 cnt = exe.dataByte(static_cast<u16>(off + 1));
    for (u16 c = 0; c < cnt; ++c) {
      const u16 p = static_cast<u16>(off + 2 + c * 3);
      l.colors.push_back(fixupColorA(exe.dataByte(p), exe.dataByte(static_cast<u16>(p + 1)),
                                     exe.dataByte(static_cast<u16>(p + 2))));
    }
    if (table == 0 && i == 0x27) l.colors.clear();
    a.lights.push_back(std::move(l));
  }
  static constexpr u8 kIndexOff[4] = {0x60, 0x62, 0x72, 0xe7};
  static constexpr u8 kIndexOn[4] = {0xf2, 0x80, 0x99, 0x4f};
  DmPalette& p = a.dmPalette;
  p.indexOff = kIndexOff[table];
  p.indexOn = kIndexOn[table];
  const u16 dm0 = static_cast<u16>(tableOff - 6);
  check(exe.dataByte(dm0) == p.indexOn && exe.dataByte(static_cast<u16>(dm0 + 1)) == 1, "dot-matrix colour");
  p.colorOn = fixupColorA(exe.dataByte(static_cast<u16>(dm0 + 2)), exe.dataByte(static_cast<u16>(dm0 + 3)),
                          exe.dataByte(static_cast<u16>(dm0 + 4)));
  const u16 dm1 = static_cast<u16>(tableOff + num * 2);
  check(exe.dataByte(dm1) == p.indexOn && exe.dataByte(static_cast<u16>(dm1 + 1)) == 3, "dot-matrix colour");
  p.colorOff = fixupColorB(exe.dataByte(static_cast<u16>(dm1 + 2)), exe.dataByte(static_cast<u16>(dm1 + 3)),
                           exe.dataByte(static_cast<u16>(dm1 + 4)));
}

void extractOccmaps(const Exe& exe, int table, TableAssets& a) {
  static constexpr u16 kSeg[4] = {0x2f94, 0x2cd8, 0x1f0f, 0x287b};
  static constexpr u16 kOff[2] = {0x580, 0x6400};
  for (int l = 0; l < 2; ++l) {
    Grid8 g(320, 576);
    for (int y = 0; y < 576; ++y)
      for (int x = 0; x < 320; ++x)
        g(x, y) = (exe.byte(kSeg[table], static_cast<u16>(kOff[l] + x / 8 + y * 40)) >> (7 - x % 8)) & 1;
    a.occmaps[static_cast<std::size_t>(l)] = std::move(g);
  }
}

void extractSpring(const Exe& exe, int table, TableAssets& a) {
  static constexpr u16 kSeg[4] = {0x82e2, 0x7e48, 0x7b0d, 0x7f4f};
  const ByteView s = exe.segment(kSeg[table]);
  a.spring = Grid8(10, 23);
  for (int y = 0; y < 23; ++y)
    for (int x = 0; x < 10; ++x) a.spring(x, y) = s[static_cast<std::size_t>(y * 10 + x)];
}

/// The ball is drawn by unrolled code; its colours are immediate operands.
void extractBall(const Exe& exe, int table, TableAssets& a) {
  static constexpr u16 kBase[4] = {0x95b0, 0x8da0, 0x8830, 0x9d40};
  a.ball = Grid8(15, 15);
  u16 pos = static_cast<u16>(kBase[table] + 0x57);
  int plane = 0;
  auto disp = [&](u8 modrm8, u8 modrm16, u8 modrm0) -> u16 {
    const u8 m = exe.codeByte(static_cast<u16>(pos + 1));
    (void)modrm0;
    if (m == modrm8) {
      const u16 v = exe.codeByte(static_cast<u16>(pos + 2));
      pos = static_cast<u16>(pos + 3);
      return v;
    }
    check(m == modrm16, "ball sprite code");
    const u16 v = exe.codeWord(static_cast<u16>(pos + 2));
    pos = static_cast<u16>(pos + 4);
    return v;
  };
  for (;;) {
    const u8 op = exe.codeByte(pos);
    if (op == 0x26) {
      // test es:[di+disp], ah ; jnz skip
      switch (exe.codeByte(static_cast<u16>(pos + 2))) {
        case 0x27: pos = static_cast<u16>(pos + 3); break;
        case 0x67: pos = static_cast<u16>(pos + 4); break;
        case 0xa7: pos = static_cast<u16>(pos + 5); break;
        default: check(false, "ball sprite code");
      }
      check(exe.codeByte(pos) == 0x75, "ball sprite code");
      pos = static_cast<u16>(pos + 2);
      check(exe.codeByte(pos) == 0x8a, "ball sprite code");
      const u16 poff = disp(0x44, 0x84, 0);
      check(exe.codeByte(pos) == 0xaa, "ball sprite code");
      pos = static_cast<u16>(pos + 1);
      check(exe.codeByte(pos) == 0xc6, "ball sprite code");
      disp(0x44, 0x84, 0);
      const u8 pix = exe.codeByte(pos);
      pos = static_cast<u16>(pos + 1);
      const int py = poff / 84, px = poff % 84 * 4 + plane;
      if (px < 15 && py < 15) a.ball(px, py) = pix;
    } else if (op == 0xd0 && exe.codeByte(static_cast<u16>(pos + 1)) == 0xcc) {
      pos = static_cast<u16>(pos + 20);
    } else if (op == 0xd0 && exe.codeByte(static_cast<u16>(pos + 1)) == 0xc1) {
      pos = static_cast<u16>(pos + 0x2e);
      ++plane;
    } else {
      check(op == 0x5a, "ball sprite code");
      break;
    }
  }
}

// ---- physics ------------------------------------------------------------------------

void extractPhysmaps(const Exe& exe, int table, TableAssets& a) {
  static constexpr u16 kPlanes[4][2][3] = {
      {{0x4114, 0x3b74, 0x7194}, {0x7734, 0x46b4, 0x7cd4}},
      {{0x3e58, 0x38b8, 0x6d5d}, {0x72fd, 0x43f8, 0x789d}},
      {{0x308f, 0x2aef, 0x6a1f}, {0x6fbf, 0x362f, 0x755f}},
      {{0x39fb, 0x345b, 0x6e67}, {0x7407, 0x3f9b, 0x79a7}},
  };
  for (int l = 0; l < 2; ++l) {
    const u16 s0 = kPlanes[table][l][0], s1 = kPlanes[table][l][1], s2 = kPlanes[table][l][2];
    Grid8 g(320, 576);
    for (int y = 0; y < 576; ++y)
      for (int x = 0; x < 320; ++x) {
        const u16 off = static_cast<u16>(x / 8 + y * 40);
        const int sh = 7 - x % 8;
        const u8 bit0 = (exe.byte(s0, off) >> sh) & 1;
        const u8 bit1 = (exe.byte(s1, off) >> sh) & 1;
        const u8 bit2 = (exe.byte(s2, off) >> sh) & 1;
        u8 val = static_cast<u8>(bit2 << 2 | bit1 << 1 | bit0);
        // A byte with no plane 0/1 bits holds a ramp index in plane 2; the ball uses the
        // nearest such byte around it, or keeps its acceleration (0xf) if there is none.
        auto zone = [&](u16 o) { return exe.byte(s0, o) == 0 && exe.byte(s1, o) == 0; };
        if (off != 0 && zone(static_cast<u16>(off - 1)))
          val = static_cast<u8>(val | exe.byte(s2, static_cast<u16>(off - 1)) << 4);
        else if (zone(off))
          val = static_cast<u8>(val | exe.byte(s2, off) << 4);
        else if (zone(static_cast<u16>(off + 1)))
          val = static_cast<u8>(val | exe.byte(s2, static_cast<u16>(off + 1)) << 4);
        else
          val |= 0xf0;
        g(x, y) = val;
      }
    a.physmaps[static_cast<std::size_t>(l)] = std::move(g);
  }
}

std::pair<int, int> xlatPhysmapAddr(u16 addr) { return {addr % 0x28 * 8, addr / 0x28}; }

Grid8 physmapRect(const Physmaps& pm, Layer l, int x, int y, int widthBytes, int height) {
  return pm[li(l)].slice(x, y, widthBytes * 8, height);
}

/// Replaces the solid bit with a stored bitmap (`skip3`: rows hold all three planes).
Grid8 physmapRectPatched(const Exe& exe, const Physmaps& pm, Layer l, int x, int y, int widthBytes, int height,
                         u16 off, bool skip3) {
  Grid8 r = physmapRect(pm, l, x, y, widthBytes, height);
  for (int j = 0; j < height; ++j)
    for (int bx = 0; bx < widthBytes; ++bx) {
      const u8 byte = exe.dataByte(static_cast<u16>(off + (skip3 ? j * 3 + 1 : j) * widthBytes + bx));
      for (int dx = 0; dx < 8; ++dx) {
        const u8 bit = (byte >> (7 - dx)) & 1;
        u8& v = r(bx * 8 + dx, j);
        v = static_cast<u8>((v & ~2) | bit << 1);
      }
    }
  return r;
}

/// ORs a stored bitmap into the solid bit (flipper shapes).
Grid8 physmapRectPatchedOr(const Exe& exe, const Physmaps& pm, Layer l, int x, int y, int widthBytes, int height,
                           u16 seg, u16 off) {
  Grid8 r = physmapRect(pm, l, x, y, widthBytes, height);
  for (int j = 0; j < height; ++j)
    for (int bx = 0; bx < widthBytes; ++bx) {
      const u8 byte = exe.byte(seg, static_cast<u16>(off + j * widthBytes + bx));
      for (int dx = 0; dx < 8; ++dx) r(bx * 8 + dx, j) |= static_cast<u8>(((byte >> (7 - dx)) & 1) << 1);
    }
  return r;
}

PhysmapPatch patchRaw(const Exe& exe, const Physmaps& pm, Layer l, u16 addr, int w, int h, u16 offRaised,
                      u16 offDropped, bool skip3) {
  const auto [x, y] = xlatPhysmapAddr(addr);
  return {l, x, y, physmapRectPatched(exe, pm, l, x, y, w, h, offRaised, skip3),
          physmapRectPatched(exe, pm, l, x, y, w, h, offDropped, skip3)};
}

PhysmapPatch patchFormatted(const Exe& exe, const Physmaps& pm, Layer l, u16 off) {
  return patchRaw(exe, pm, l, exe.dataWord(static_cast<u16>(off + 4)), exe.dataWord(static_cast<u16>(off + 6)),
                  exe.dataWord(static_cast<u16>(off + 8)), exe.dataWord(off), exe.dataWord(static_cast<u16>(off + 2)),
                  true);
}

void extractPhysmapPatches(const Exe& exe, int table, TableAssets& a) {
  const Physmaps& pm = a.physmaps;
  auto set = [&](PhysmapBind b, PhysmapPatch p) { a.physmapPatches[static_cast<std::size_t>(b)] = std::move(p); };
  const Layer G = Layer::Ground, O = Layer::Overhead;
  switch (table) {
    case 0: {
      const auto [x, y] = xlatPhysmapAddr(0x266);
      set(PhysmapBind::PartyGateSkyride,
          {O, x, y, physmapRect(pm, O, x, y, 2, 18), physmapRectPatched(exe, pm, O, x, y, 2, 18, 0x1332, false)});
      set(PhysmapBind::PartyHitDuck0, patchRaw(exe, pm, G, 0x2b5a, 2, 15, 0x68b0, 0x68d0, false));
      set(PhysmapBind::PartyHitDuck1, patchRaw(exe, pm, G, 0x2e2b, 2, 15, 0x68f0, 0x6910, false));
      set(PhysmapBind::PartyHitDuck2, patchRaw(exe, pm, G, 0x30fc, 1, 15, 0x6940, 0x6930, false));
      break;
    }
    case 2:
      set(PhysmapBind::ShowGatePlunger, patchRaw(exe, pm, G, 0x2fcb, 2, 34, 0x6280, 0x6230, false));
      set(PhysmapBind::ShowGateRampRight, patchRaw(exe, pm, O, 0x198e, 4, 20, 0x6480, 0x6390, true));
      set(PhysmapBind::ShowGateVaultEntry, patchRaw(exe, pm, O, 0x0f00, 3, 25, 0x61e0, 0x6190, false));
      set(PhysmapBind::ShowGateVaultExit, patchRaw(exe, pm, G, 0x4fd8, 4, 14, 0x6620, 0x6570, true));
      set(PhysmapBind::ShowHitCenter0, patchRaw(exe, pm, G, 0x2389, 2, 16, 0x6370, 0x62d0, false));
      set(PhysmapBind::ShowHitCenter1, patchRaw(exe, pm, G, 0x26a9, 1, 16, 0x6360, 0x62f0, false));
      set(PhysmapBind::ShowHitLeft0, patchRaw(exe, pm, G, 0x2994, 1, 16, 0x6350, 0x6300, false));
      set(PhysmapBind::ShowHitLeft1, patchRaw(exe, pm, G, 0x2cb3, 2, 16, 0x6330, 0x6310, false));
      break;
    case 3:
      set(PhysmapBind::StonesGateKickback, patchFormatted(exe, pm, G, 0x1265));
      set(PhysmapBind::StonesGateTowerEntry, patchFormatted(exe, pm, G, 0x123d));
      set(PhysmapBind::StonesGateRampTower, patchFormatted(exe, pm, G, 0x1233));
      set(PhysmapBind::StonesGateRampLeft0, patchFormatted(exe, pm, O, 0x1247));
      set(PhysmapBind::StonesGateRampLeft1, patchFormatted(exe, pm, O, 0x1251));
      set(PhysmapBind::StonesGateRampLeft2, patchFormatted(exe, pm, O, 0x125b));
      break;
    default: break;
  }
}

void extractSineTable(const Exe& exe, int table, TableAssets& a) {
  static constexpr u16 kOff[4] = {0x4600, 0x4690, 0x3ee0, 0x4bf0};
  for (u16 i = 0; i < 0xa00; ++i) {
    const u16 w = exe.dataWord(static_cast<u16>(kOff[table] + i * 2));
    a.sineTable[i] = static_cast<i16>(static_cast<u16>((w >> 8) | (w << 8)));
  }
}

void extractRamps(const Exe& exe, int table, TableAssets& a) {
  static constexpr u16 kOff[4] = {0x5e, 0x7b, 0x233, 0xca2};
  static constexpr u16 kOffHi[4] = {0x72, 0x97, 0x24b, 0xcd2};
  static constexpr u16 kNum[4] = {4, 6, 5, 11};
  for (u16 i = 0; i < kNum[table]; ++i) {
    Ramp r;
    r.accel = {exe.dataWordS(static_cast<u16>(kOff[table] + i * 4)), exe.dataWordS(static_cast<u16>(kOff[table] + i * 4 + 2))};
    r.accelHires = {exe.dataWordS(static_cast<u16>(kOffHi[table] + i * 4)),
                    exe.dataWordS(static_cast<u16>(kOffHi[table] + i * 4 + 2))};
    a.ramps.push_back(r);
  }
}

/// The 44 collision probes around the ball, read from the unrolled probing code.
void extractBallOutline(const Exe& exe, int table, TableAssets& a) {
  static constexpr u16 kRange[4][2] = {{0x8866, 0x8c34}, {0x8056, 0x8424}, {0x7ae6, 0x7eb4}, {0x8ff6, 0x93c4}};
  u16 pos = kRange[table][0];
  const u16 end = kRange[table][1];
  int byteOff = 0;
  while (pos != end) {
    if (exe.codeByte(pos) == 0x26) {
      check(exe.codeByte(static_cast<u16>(pos + 1)) == 0x8b, "ball outline code");
      switch (exe.codeByte(static_cast<u16>(pos + 2))) {
        case 0x04: byteOff = 0; pos = static_cast<u16>(pos + 3); break;
        case 0x44: byteOff = exe.codeByte(static_cast<u16>(pos + 3)); pos = static_cast<u16>(pos + 4); break;
        case 0x84: byteOff = exe.codeWord(static_cast<u16>(pos + 3)); pos = static_cast<u16>(pos + 5); break;
        default: check(false, "ball outline code");
      }
      byteOff += 1;
      check(exe.codeByte(pos) == 0xd3 && exe.codeByte(static_cast<u16>(pos + 1)) == 0xc0, "ball outline code");
      pos = static_cast<u16>(pos + 2);
    }
    check(exe.codeByte(pos) == 0xa8, "ball outline code");
    const u8 bit = exe.codeByte(static_cast<u16>(pos + 1));
    int bitPos = 0;
    while (bitPos < 8 && (0x80 >> bitPos) != bit) ++bitPos;
    check(bitPos < 8, "ball outline bit");
    BallOutlinePixel p;
    p.y = static_cast<i16>(byteOff / 0x28);
    p.x = static_cast<i16>((byteOff % 0x28) * 8 + bitPos - 7);
    pos = static_cast<u16>(pos + 2);
    check(exe.codeByte(pos) == 0x74, "ball outline code");
    const u16 curEnd = static_cast<u16>(pos + 2 + exe.codeByte(static_cast<u16>(pos + 1)));
    pos = static_cast<u16>(pos + 2);
    check(exe.codeByte(static_cast<u16>(pos + 1)) == 0xc5, "ball outline code");
    if (exe.codeByte(pos) == 0x81) {
      p.angle = exe.codeWord(static_cast<u16>(pos + 2));
      pos = static_cast<u16>(pos + 4);
    } else {
      check(exe.codeByte(pos) == 0x83, "ball outline code");
      p.angle = exe.codeByte(static_cast<u16>(pos + 2));
      pos = static_cast<u16>(pos + 3);
    }
    check(exe.codeByte(pos) == 0x83 && exe.codeByte(static_cast<u16>(pos + 1)) == 0xcf, "ball outline code");
    p.quad = exe.codeByte(static_cast<u16>(pos + 2));
    pos = static_cast<u16>(pos + 3);
    check(exe.codeByte(pos) == 0xb5, "ball outline code");
    p.idx = exe.codeByte(static_cast<u16>(pos + 1));
    pos = static_cast<u16>(pos + 2);
    p.isBot = exe.codeByte(static_cast<u16>(pos + 1)) == 0xc6;
    pos = static_cast<u16>(pos + 2);
    p.isRight = exe.codeByte(static_cast<u16>(pos + 1)) == 0xc7;
    pos = static_cast<u16>(pos + 2);
    check(pos == curEnd, "ball outline code");
    a.ballOutline.push_back(p);
  }
  auto sorted = a.ballOutline;
  std::stable_sort(sorted.begin(), sorted.end(), [](const auto& l, const auto& r) { return l.angle < r.angle; });
  for (const auto& p : sorted) a.ballOutlineByAngle.push_back({p.x, p.y});
}

void extractTransitions(const Exe& exe, int table, TableAssets& a) {
  static constexpr u16 kOff[4][2] = {{0xec5, 0xf17}, {0xc55, 0xc97}, {0xad8, 0xb2a}, {0xc1e, 0xc70}};
  for (int k = 0; k < 2; ++k) {
    auto& list = k == 0 ? a.transitionsDown : a.transitionsUp;
    for (u16 off = kOff[table][k]; exe.dataWord(off) != 0xffff; off = static_cast<u16>(off + 8))
      list.push_back(readRect(exe, off));
  }
}

void extractBumpers(const Exe& exe, int table, TableAssets& a) {
  static constexpr u16 kOff[4][2] = {{0xcf3, 0xd1b}, {0xaad, 0xadf}, {0x924, 0x94c}, {0x96a, 0x99c}};
  for (int k = 0; k < 2; ++k)
    for (u16 pos = kOff[table][k]; exe.dataWord(pos) != 0xffff; pos = static_cast<u16>(pos + 10)) {
      const u16 ptr = exe.dataWord(static_cast<u16>(pos + 8));
      a.bumpers.push_back({k == 1, readRect(exe, pos), readSfx(exe, exe.dataWord(ptr)),
                           exe.dataBcd(static_cast<u16>(ptr + 2))});
    }
}

std::vector<RollTriggerArea> rollTriggerList(const Exe& exe, int table, u16 pos) {
  std::vector<RollTriggerArea> res;
  while (exe.dataWord(pos) != 0) {
    const TRect rect = readRect(exe, pos);
    const u16 ptr = exe.dataWord(static_cast<u16>(pos + 8));
    pos = static_cast<u16>(pos + 10);
    const RollTriggerHandler* h = nullptr;
    for (const auto& e : kRollTriggerHandlers)
      if (e.table == table && e.ptr == ptr) h = &e;
    check(h != nullptr, "unknown roll trigger handler");
    res.push_back({rect, h->kind, h->arg});
  }
  return res;
}

void extractRollTriggers(const Exe& exe, int table, TableAssets& a) {
  static constexpr u16 kOff[4][4] = {
      {0xd9b, 0xe29, 0xea3, 0xec3}, {0xb91, 0xc29, 0xc53, 0xc53}, {0x9e0, 0xa78, 0xaca, 0xad6}, {0xa5e, 0xb14, 0xbac, 0xbcc}};
  a.rollTriggers[0] = rollTriggerList(exe, table, kOff[table][0]);
  a.rollTriggers[1] = rollTriggerList(exe, table, kOff[table][1]);
  a.rollTriggersTilt[0] = rollTriggerList(exe, table, kOff[table][2]);
  a.rollTriggersTilt[1] = rollTriggerList(exe, table, kOff[table][3]);
}

void extractHitTriggers(const Exe& exe, int table, TableAssets& a) {
  static constexpr u16 kOff[4] = {0xd71, 0xb51, 0x9a2, 0xa00};
  for (u16 pos = kOff[table]; exe.dataWord(pos) != 0; pos = static_cast<u16>(pos + 10)) {
    const u16 ptr = exe.dataWord(static_cast<u16>(pos + 8));
    const HitTriggerHandler* h = nullptr;
    for (const auto& e : kHitTriggerHandlers)
      if (e.table == table && e.ptr == ptr) h = &e;
    check(h != nullptr, "unknown hit trigger handler");
    a.hitTriggers.push_back({readRect(exe, pos), h->kind, h->arg});
  }
}

// ---- dot matrix -----------------------------------------------------------------------

void extractDmFonts(const Exe& exe, int table, TableAssets& a) {
  static constexpr u16 kOff[4][4] = {{0x6710, 0x65d0, 0x6410, 0x6200},
                                    {0x67a0, 0x6660, 0x64a0, 0x6290},
                                    {0x5ff0, 0x5eb0, 0x5cf0, 0x5ae0},
                                    {0x6d00, 0x6bc0, 0x6a00, 0x67f0}};
  static constexpr char kChars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ?()-";
  for (int f = 0; f < 4; ++f) {
    const int h = dmFontHeight(static_cast<DmFont>(f));
    auto& font = a.dmFonts[static_cast<std::size_t>(f)];
    for (int i = 0; kChars[i]; ++i) {
      const ByteView b = exe.dataBytes(static_cast<u16>(kOff[table][f] + i * h), static_cast<std::size_t>(h));
      font[static_cast<u8>(kChars[i])] = std::vector<u8>(b.begin(), b.end());
    }
    font['_'] = std::vector<u8>(static_cast<std::size_t>(h), 0);
  }
}

void extractDmTower(const Exe& exe, TableAssets& a) {
  Grid<u8> t(160, 167);
  for (int y = 0; y < 167; ++y)
    for (int x = 0; x < 160; ++x) {
      const u8 byte = exe.byte(0x49ff, static_cast<u16>(y * 40 + x / 4));
      t(x, y) = ((byte << (2 * (x % 4))) & 0x80) != 0;
    }
  a.dmTower = std::move(t);
}

// ---- flippers ---------------------------------------------------------------------------

void extractFlippers(const Exe& exe, int table, TableAssets& a) {
  struct Cfg { u16 off; std::vector<u16> physSegs; u16 gfxSeg, glutSeg, glutLen; };
  const Cfg cfgs[4] = {{0x6950, {0x4c54, 0x4fc2, 0x4eb6}, 0xc0f, 0x8274, 0x6d4 / 4},
                       {0x6940, {0x4998, 0x4df2, 0x4bfa}, 0xb8e, 0x7e3d, 0xa8 / 4},
                       {0x66d0, {0x3bcf, 0x3f2a, 0x3e31}, 0xb37, 0x7aff, 0xdc / 4},
                       {0x7360, {0x453b, 0x479d}, 0xc88, 0x7f47, 0x78 / 4}};
  const Cfg& c = cfgs[table];
  for (std::size_t i = 0; i < c.physSegs.size(); ++i) {
    const u16 f = static_cast<u16>(c.off + i * 0x3c);
    auto w = [&](int o) { return exe.dataWord(static_cast<u16>(f + o)); };
    auto ws = [&](int o) { return exe.dataWordS(static_cast<u16>(f + o)); };
    Flipper fl;
    const int width = w(0x06) * 16, height = w(0x08);
    const u16 physStride = static_cast<u16>(w(0x18) / 3);
    check((width / 8) * height == physStride, "flipper record");
    fl.quantumMax = w(0x20);
    fl.rectX = w(0x02);
    fl.rectY = w(0x04);
    fl.gfx.push_back(a.mainBoard.slice(fl.rectX, fl.rectY, width, height));
    const u16 copyList = static_cast<u16>(w(0x36) + i * 8);
    const u16 copyStride = w(0x38);
    for (u16 q = 0; q < fl.quantumMax; ++q) {
      Grid8 img = fl.gfx.back();
      const u16 cl = static_cast<u16>(copyList + copyStride * q);
      const u16 cnt = exe.word(c.gfxSeg, cl);
      for (u16 j = 0; j < cnt; ++j) {
        const u16 o = static_cast<u16>(cl + 0x12 + j * 4);
        const u16 dst = exe.word(c.gfxSeg, o);
        const u16 src = static_cast<u16>(exe.word(c.gfxSeg, static_cast<u16>(o + 2)) - 0xd4f4);
        const int dx = (dst % 0x54) * 4, dy = dst / 0x54;
        check(dx < width && dy < height, "flipper copy list");
        for (int k = 0; k < 4; ++k) img(dx + k, dy) = exe.byte(c.glutSeg, static_cast<u16>(src + c.glutLen * k));
      }
      fl.gfx.push_back(std::move(img));
    }
    switch (exe.dataByte(f)) {
      case 1: fl.side = FlipperSide::Right; break;
      case 2: fl.side = FlipperSide::Left; break;
      default: check(false, "flipper side");
    }
    for (u16 q = 0; q <= fl.quantumMax; ++q)
      fl.physmap.push_back(physmapRectPatchedOr(exe, a.physmaps, Layer::Ground, fl.rectX, fl.rectY, width / 8, height,
                                                c.physSegs[i], static_cast<u16>(q * physStride)));
    fl.ballBbox = {ws(0x0a), ws(0x0e), ws(0x0c), ws(0x10)};
    fl.originX = ws(0x12);
    fl.originY = ws(0x14);
    check(w(0x16) == 0 || w(0x16) == 0xffff, "flipper orientation");
    fl.isVertical = w(0x16) == 0xffff;
    fl.posMax = ws(0x22);
    fl.accelPress = static_cast<i16>(-ws(0x24));
    fl.accelRelease = static_cast<i16>(-ws(0x26));
    fl.speedPressStart = static_cast<i16>(-ws(0x28));
    a.flippers.push_back(std::move(fl));
  }
}

// ---- lights -------------------------------------------------------------------------------

void extractAttractLights(const Exe& exe, int table, TableAssets& a) {
  static constexpr u16 kOff[4] = {0xf41, 0xcb9, 0xb54, 0xf1a};
  for (u16 pos = kOff[table]; exe.dataWord(pos) != 0xffff; pos = static_cast<u16>(pos + 10)) {
    check(exe.dataWord(pos) == 0, "attract light record");
    AttractLight l;
    l.ctrReset = exe.dataWord(static_cast<u16>(pos + 2));
    l.ctrOff = static_cast<u16>(l.ctrReset + exe.dataWord(static_cast<u16>(pos + 4)));
    l.ctrOn = static_cast<u16>(l.ctrOff + exe.dataWord(static_cast<u16>(pos + 6)));
    l.light = static_cast<u8>(exe.dataWord(static_cast<u16>(pos + 8)) - 1);
    a.attractLights.push_back(l);
  }
}

void extractLightBinds(int table, TableAssets& a) {
  for (const auto& s : kLightBinds)
    if (s.table == table)
      for (u8 l : s.lights) a.lightBinds[static_cast<std::size_t>(s.bind)].push_back(static_cast<u8>(l - 1));
}

// ---- sound ------------------------------------------------------------------------------------

void extractSoundBinds(const Exe& exe, int table, TableAssets& a) {
  for (const auto& j : kJingleBindAddrs)
    if (j.table == table) a.jingleBinds[static_cast<std::size_t>(j.bind)] = readJingle(exe, j.addr);
  for (const auto& s : kSfxBindAddrs)
    if (s.table == table) a.sfxBinds[static_cast<std::size_t>(s.bind)] = readSfx(exe, s.addr);
  static constexpr u8 kStart[4] = {0x06, 0x0a, 0x07, 0x0a};
  a.positionJingleStart = kStart[table];
}

// ---- script -------------------------------------------------------------------------------------

DmCoord dmAddrToXy(u16 addr, int plane) {
  int x = addr % 0x54, y = addr / 0x54;
  if (y & 1) {
    y += 1;
    x -= 0x54;
  }
  return {static_cast<i16>(x * 2 + plane), static_cast<i16>(y / 2 - 1)};
}

struct ScriptBuilder {
  const Exe& exe;
  int table;
  TableAssets& a;
  std::map<u16, u16> msgByAddr, animByAddr, frameByAddr, uopByAddr;

  u16 msg(u16 off, bool isLong) {
    if (auto it = msgByAddr.find(off); it != msgByAddr.end()) return it->second;
    std::vector<u8> m;
    const u16 highScoreOff = table == 3 ? 0xa6 : 0x16;
    for (u16 pos = off;; ++pos) {
      const u8 byte = exe.dataByte(pos);
      if ((byte == 0 && !isLong) || (byte == 0xff && isLong)) break;
      u8 chr;
      if (isLong) {
        chr = byte == 1 ? '_' : byte == '^' ? '-' : byte;
      } else if (byte == 0x2a) {
        chr = '_';
      } else if (byte >= 0x37 && byte <= 0x40) {
        chr = static_cast<u8>(byte - 7);
      } else if (byte == 0x20 || byte == 0x21 || byte == 0x2d || (byte >= 0x41 && byte <= 0x5a)) {
        chr = byte;
      } else if (byte >= 0x5b && byte <= 0x5e) {
        chr = static_cast<u8>("?()-"[byte - 0x5b]);
      } else {
        throw DataError("unknown dot-matrix character");
      }
      if (pos >= highScoreOff && pos < highScoreOff + 0x40) {
        const int idx = (pos - highScoreOff) / 0x10, cidx = (pos - highScoreOff) % 0x10;
        check(cidx >= 12 && cidx < 15, "high score name reference");
        chr = static_cast<u8>(special_chars::kHighScores + idx * 3 + (cidx - 12));
      }
      if (const int s = specialAt(pos); s >= 0) chr = static_cast<u8>(s);
      m.push_back(chr);
    }
    if (table == 2 && off == 0x128f) {
      m.resize(m.size() - 2);
      for (u8 k = 0; k < 3; ++k) m.push_back(static_cast<u8>(special_chars::kNumCyclonesTargetL + k));
    }
    const u16 id = static_cast<u16>(a.msgs.size());
    a.msgs.push_back(std::move(m));
    msgByAddr[off] = id;
    return id;
  }

  /// Message bytes the code overwrites with a live value before printing.
  int specialAt(u16 pos) const {
    struct S { u8 t; u16 p; u8 c; };
    using namespace special_chars;
    static constexpr S kSpecial[] = {
        {0, 0x1cf9, kCurPlayer}, {1, 0x1d92, kCurPlayer}, {2, 0x1a7c, kCurPlayer}, {0, 0x1d91, kCurPlayer},
        {1, 0x1da9, kCurPlayer}, {2, 0x1a93, kCurPlayer}, {3, 0x2408, kCurPlayer}, {2, 0x1aa8, kCurPlayer},
        {3, 0x1716, kCurPlayer}, {0, 0x2249, kCurPlayer}, {1, 0x2058, kCurPlayer}, {2, 0x1d09, kCurPlayer},
        {3, 0x26b4, kCurPlayer}, {0, 0x2288, kCurPlayer}, {1, 0x2097, kCurPlayer}, {2, 0x1d48, kCurPlayer},
        {3, 0x26f3, kCurPlayer},
        {0, 0x2253, kCurBall}, {1, 0x2062, kCurBall}, {2, 0x1d13, kCurBall}, {3, 0x26be, kCurBall},
        {0, 0x228f, kCurBall}, {1, 0x209e, kCurBall}, {2, 0x1d4f, kCurBall}, {3, 0x26fa, kCurBall},
        {0, 0x1d9b, kTotalPlayers}, {1, 0x1db3, kTotalPlayers}, {2, 0x1ab2, kTotalPlayers}, {3, 0x2412, kTotalPlayers},
        {0, 0x1db9, kBonusMultL}, {1, 0x1dd1, kBonusMultL}, {2, 0x1ad0, kBonusMultL}, {3, 0x2424, kBonusMultL},
        {2, 0x1ad1, kBonusMultL + 1}, {3, 0x2425, kBonusMultL + 1},
        {0, 0x2237, kBonusMultR}, {1, 0x2046, kBonusMultR}, {2, 0x1cf7, kBonusMultR}, {3, 0x26a2, kBonusMultR},
        {0, 0x2238, kBonusMultR + 1}, {1, 0x2047, kBonusMultR + 1}, {2, 0x1cf8, kBonusMultR + 1},
        {3, 0x26a3, kBonusMultR + 1},
        {1, 0x19b5, kNumCyclones}, {3, 0x1e40, kNumCyclones}, {1, 0x19b6, kNumCyclones + 1},
        {3, 0x1e41, kNumCyclones + 1}, {1, 0x19b7, kNumCyclones + 2}, {3, 0x1e42, kNumCyclones + 2},
        {1, 0x1924, kNumCyclonesTarget}, {1, 0x1937, kNumCyclonesTarget}, {3, 0x1eb0, kNumCyclonesTarget},
        {3, 0x1ec5, kNumCyclonesTarget}, {1, 0x1925, kNumCyclonesTarget + 1}, {1, 0x1938, kNumCyclonesTarget + 1},
        {3, 0x1eb1, kNumCyclonesTarget + 1}, {3, 0x1ec6, kNumCyclonesTarget + 1},
        {1, 0x1926, kNumCyclonesTarget + 2}, {1, 0x1939, kNumCyclonesTarget + 2},
        {3, 0x1eb2, kNumCyclonesTarget + 2}, {3, 0x1ec7, kNumCyclonesTarget + 2},
    };
    for (const S& s : kSpecial)
      if (s.t == table && s.p == pos) return s.c;
    return -1;
  }

  u8 anim(u16 off) {
    if (auto it = animByAddr.find(off); it != animByAddr.end()) return static_cast<u8>(it->second);
    static constexpr u16 kSeg[4] = {0x2056, 0x1f6f, 0x418c, 0x1d8f};
    const u16 seg = kSeg[table];
    DmAnim an;
    an.repeats = exe.word(seg, static_cast<u16>(off - 4));
    const u16 numFrames = static_cast<u16>(exe.word(seg, static_cast<u16>(off - 2)) / 4);
    an.restart = an.repeats == 1 ? 0 : exe.word(seg, static_cast<u16>(off - 6)) / 4;
    const u16 realFrames = an.repeats == 1 ? numFrames : static_cast<u16>(numFrames + 1);
    an.numFrames = numFrames;
    for (u16 i = 0; i < realFrames; ++i) {
      const u16 foff = exe.word(seg, static_cast<u16>(off + i * 4));
      const u16 num = exe.word(seg, static_cast<u16>(off + i * 4 + 2));
      u16 frameId;
      if (auto it = frameByAddr.find(foff); it != frameByAddr.end()) {
        frameId = it->second;
      } else {
        DmAnimFrame frame;
        u16 fpos = foff;
        for (int plane = 0; plane < 2; ++plane) {
          u16 dpos = 0xa7;
          const u16 cnt = exe.word(seg, fpos);
          fpos = static_cast<u16>(fpos + 2);
          for (u16 k = 0; k < cnt; ++k) {
            const u8 byte = exe.byte(seg, fpos);
            fpos = static_cast<u16>(fpos + 1);
            dpos = static_cast<u16>(dpos + (byte >> 1));
            if (byte != 0xfe) frame.push_back({dmAddrToXy(dpos, plane), (byte & 1) != 0});
          }
        }
        frameId = static_cast<u16>(a.animFrames.size());
        a.animFrames.push_back(std::move(frame));
        frameByAddr[foff] = frameId;
      }
      an.frames.push_back({static_cast<u8>(frameId), num});
    }
    const u16 id = static_cast<u16>(a.anims.size());
    a.anims.push_back(std::move(an));
    animByAddr[off] = id;
    return static_cast<u8>(id);
  }

  ScriptScoreRef score(u16 ptr) const {
    for (const auto& s : kScoreRefs)
      if (s.table == table && s.ptr == ptr) {
        ScriptScoreRef r{s.kind, s.arg, {}};
        if (s.kind == ScriptScore::Const) r.value = exe.dataBcd(ptr);
        return r;
      }
    throw DataError("unknown score reference");
  }

  UopKind kind(u16 ptr, DmFont& font, bool& centered) const {
    if (ptr == 0) return UopKind::End;
    for (const auto& u : kUopHandlers)
      if (u.table == table && u.ptr == ptr) {
        font = u.font;
        centered = u.centered;
        return u.kind;
      }
    throw DataError("unknown script instruction");
  }

  void run() {
    std::vector<std::pair<u16, u16>> relocs;
    for (const auto& range : kScriptRanges) {
      if (range.table != table) continue;
      bool wasEnd = true;
      u16 pos = range.begin;
      auto word = [&](int o) { return exe.dataWord(static_cast<u16>(pos + o)); };
      while (pos != range.end) {
        const u16 cur = static_cast<u16>(a.scripts.size());
        uopByAddr[pos] = cur;
        Uop u;
        u.kind = kind(exe.dataWord(pos), u.font, u.flag);
        pos = static_cast<u16>(pos + 2);
        wasEnd = false;
        int argBytes = 0;
        switch (u.kind) {
          case UopKind::End: wasEnd = true; break;
          case UopKind::Delay:
          case UopKind::DelayIfMultiplayer:
          case UopKind::DmBlink:
          case UopKind::DmTowerHunt:
          case UopKind::SetJingleTimeout:
          case UopKind::SetMusic:
            u.value = word(0);
            argBytes = 2;
            break;
          case UopKind::Jump:
          case UopKind::JccNoBonusMult:
          case UopKind::FinalScoreLoop:
            relocs.push_back({cur, word(0)});
            argBytes = 2;
            break;
          case UopKind::JccScoreZero:
            u.score = score(word(0));
            relocs.push_back({cur, word(2)});
            argBytes = 4;
            break;
          case UopKind::RepeatSetup:
            u.value = word(0);
            argBytes = 4;
            break;
          case UopKind::RepeatLoop:
            u.value = word(0);
            relocs.push_back({cur, word(2)});
            argBytes = 4;
            break;
          case UopKind::DmState:
            u.flag = word(0) != 0;
            argBytes = 2;
            break;
          case UopKind::DmClear:
          case UopKind::DmWipeDown:
          case UopKind::DmWipeRight:
          case UopKind::DmWipeDownStriped:
          case UopKind::IssueBall:
          case UopKind::AccBonusCyclones:
          case UopKind::AccBonusModeHit:
          case UopKind::AccBonusModeRamp:
          case UopKind::AccBonus:
          case UopKind::CheckTopScore:
          case UopKind::NextBallIfMatched:
          case UopKind::NextBall:
          case UopKind::CheckMatch:
          case UopKind::GameOver:
          case UopKind::SpeedStartTurbo:
            break;
          case UopKind::DmAnim:
            u.anim = anim(word(0));
            argBytes = 2;
            break;
          case UopKind::DmPuts: {
            u16 m = word(0), dpos = word(2);
            if (dpos == 0x14e) {
              m = static_cast<u16>(m + 2);
              dpos = static_cast<u16>(dpos + 8);
            }
            u.msg = msg(m, false);
            u.coord = dmAddrToXy(dpos, 0);
            argBytes = 4;
            break;
          }
          case UopKind::DmPrintScore:
            u.score = score(word(0));
            u.coord = dmAddrToXy(word(2), 0);
            argBytes = 4;
            break;
          case UopKind::DmBigScore:
            u.kind = UopKind::DmPrintScore;
            u.font = DmFont::H13;
            u.flag = false;
            u.coord = {-16, 1};
            u.score = score(word(0));
            argBytes = 2;
            break;
          case UopKind::DmMsgScrollUp:
          case UopKind::DmMsgScrollDown:
            u.msg = msg(word(0), false);
            u.end = static_cast<i16>(word(2));
            argBytes = 4;
            break;
          case UopKind::DmLongMsg:
            u.msg = msg(word(0), true);
            argBytes = 2;
            break;
          case UopKind::PlaySfx: {
            u.sfx = readSfx(exe, word(0));
            const u16 vol = word(2);
            u.volume = static_cast<u8>(vol == 0 ? 0x40 : vol);
            argBytes = 4;
            break;
          }
          case UopKind::PlayJingle:
            u.jingle = readJingle(exe, word(0));
            argBytes = 2;
            break;
          case UopKind::ModeContinue:
          case UopKind::ModeStart:
          case UopKind::ModeStartOrContinue:
            u.time = static_cast<u8>(word(0) * 10 + word(2));
            u.score = score(word(4));
            argBytes = 6;
            break;
          default:
            // Every other instruction takes one word that the game ignores.
            argBytes = 2;
            break;
        }
        pos = static_cast<u16>(pos + argBytes);
        a.scripts.push_back(u);
      }
      check(wasEnd, "script range does not end with End");
    }
    for (const auto& [at, target] : relocs) {
      auto it = uopByAddr.find(target);
      check(it != uopByAddr.end(), "script jump target");
      a.scripts[at].target = it->second;
    }
  }

  u16 at(u16 addr) const {
    auto it = uopByAddr.find(addr);
    check(it != uopByAddr.end(), "script address");
    return it->second;
  }
};

void extractScripts(const Exe& exe, int table, TableAssets& a) {
  ScriptBuilder b{exe, table, a, {}, {}, {}, {}};
  b.run();
  for (const auto& s : kScriptBinds)
    if (s.table == table) a.scriptBinds[static_cast<std::size_t>(s.bind)] = b.at(s.addr);

  const CheatAddrs& c = kCheatAddrs[table];
  const std::pair<const char*, std::pair<u16, CheatEffect>> cheats[] = {
      {"JOHAN", {c.johan, CheatEffect::None}},         {"TECH", {c.tech, CheatEffect::None}},
      {"TSP", {c.tsp, CheatEffect::None}},             {"DANIEL", {c.daniel, CheatEffect::None}},
      {"GABRIEL", {c.gabriel, CheatEffect::None}},     {"CHEAT", {c.cheat, CheatEffect::None}},
      {"EARTHQUAKE", {c.earthquake, CheatEffect::Tilt}}, {"EXTRA BALLS", {c.extra_balls, CheatEffect::Balls}},
      {"SNAIL", {c.snail, CheatEffect::Slowdown}},     {"FAIR PLAY", {c.fair_play, CheatEffect::Reset}},
      {"ROBBAN", {c.robban, CheatEffect::None}},       {"STEIN", {c.stein, CheatEffect::None}},
      {"GREET", {c.greet, CheatEffect::None}},
  };
  for (const auto& [keys, v] : cheats) a.cheats.push_back({keys, b.at(v.first), v.second});

  for (const auto& e : kEffectAddrs) {
    if (e.table != table) continue;
    u16 off = e.addr;
    Effect eff;
    const u16 jingle = exe.dataWord(off);
    off = static_cast<u16>(off + 2);
    if (jingle == 0) {
      eff.silentPriority = exe.dataByte(off);
      off = static_cast<u16>(off + 1);
    } else {
      eff.jingle = readJingle(exe, jingle);
    }
    const u16 script = exe.dataWord(static_cast<u16>(off + 24));
    if (script != 0) eff.script = b.at(script);
    eff.scoreMain = exe.dataBcd(off);
    eff.scoreBonus = exe.dataBcd(static_cast<u16>(off + 12));
    a.effects[static_cast<std::size_t>(e.bind)] = eff;
  }
}

}  // namespace

const Jingle& TableAssets::jingle(JingleBind b) const {
  const auto& j = jingleBinds[static_cast<std::size_t>(b)];
  if (!j) throw DataError(std::string("table has no jingle ") + kJingleBindNames[static_cast<std::size_t>(b)]);
  return *j;
}

TableAssets TableAssets::load(ByteView prg, int table) {
  TableAssets a;
  a.table = table;
  Exe exe = Exe::load(prg);
  check(exe.codeByte(static_cast<u16>(exe.ip + 0xe)) == 0xb8, "entry code");
  exe.ds = exe.codeWord(static_cast<u16>(exe.ip + 0xf));

  extractLights(exe, table, a);
  extractAttractLights(exe, table, a);
  extractLightBinds(table, a);
  extractMainBoard(exe, table, a);
  extractOccmaps(exe, table, a);
  extractSpring(exe, table, a);
  extractBall(exe, table, a);
  extractPhysmaps(exe, table, a);
  extractPhysmapPatches(exe, table, a);
  extractSineTable(exe, table, a);
  extractDmFonts(exe, table, a);
  if (table == 3) extractDmTower(exe, a);
  extractFlippers(exe, table, a);
  extractRamps(exe, table, a);
  extractBallOutline(exe, table, a);
  extractSoundBinds(exe, table, a);
  extractScripts(exe, table, a);

  static const Bcd kJackpotInit[4] = {Bcd::of("10000000"), Bcd::of("5000000"), Bcd::of("10000000"), Bcd::of("10000000")};
  static const Bcd kJackpotIncr[4] = {Bcd::of("50000"), Bcd::of("100000"), Bcd::of("100000"), Bcd::of("100000")};
  static const Bcd kModeHitIncr[4] = {Bcd::of("1000000"), Bcd::of("100000"), Bcd::of("500000"), Bcd::of("1000000")};
  static const Bcd kModeRampIncr[4] = {Bcd::of("5000000"), Bcd::of("5000000"), Bcd::of("1000000"), Bcd::of("5000000")};
  a.scoreJackpotInit = kJackpotInit[table];
  a.scoreJackpotIncr = kJackpotIncr[table];
  a.scoreModeHitIncr = kModeHitIncr[table];
  a.scoreModeRampIncr = kModeRampIncr[table];
  static constexpr i16 kIssue[4][2] = {{282, 530}, {285, 530}, {284, 530}, {280, 525}};
  static constexpr i16 kRelease[4][2] = {{297, 530}, {300, 530}, {299, 530}, {295, 525}};
  a.issueBallPos = {kIssue[table][0], kIssue[table][1]};
  a.issueBallReleasePos = {kRelease[table][0], kRelease[table][1]};

  extractTransitions(exe, table, a);
  extractBumpers(exe, table, a);
  extractRollTriggers(exe, table, a);
  extractHitTriggers(exe, table, a);
  return a;
}

}  // namespace pfr
