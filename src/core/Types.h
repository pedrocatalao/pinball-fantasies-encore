#pragma once
// Fundamental types shared by every module.
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pfr {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using Bytes = std::vector<u8>;
using ByteView = std::span<const u8>;

/// Little-endian readers used by all binary loaders (x86 data).
inline u16 rd16le(ByteView b, std::size_t off) { return static_cast<u16>(b[off] | (b[off + 1] << 8)); }
inline u32 rd32le(ByteView b, std::size_t off) {
  return static_cast<u32>(b[off]) | (static_cast<u32>(b[off + 1]) << 8) | (static_cast<u32>(b[off + 2]) << 16) |
         (static_cast<u32>(b[off + 3]) << 24);
}
/// Big-endian readers (IFF and ProTracker data are Amiga-native).
inline u16 rd16be(ByteView b, std::size_t off) { return static_cast<u16>((b[off] << 8) | b[off + 1]); }
inline u32 rd32be(ByteView b, std::size_t off) {
  return (static_cast<u32>(b[off]) << 24) | (static_cast<u32>(b[off + 1]) << 16) |
         (static_cast<u32>(b[off + 2]) << 8) | static_cast<u32>(b[off + 3]);
}

struct Point {
  int x = 0;
  int y = 0;
};

struct Rect {
  int x = 0, y = 0, w = 0, h = 0;
  bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
};

struct Rgb {
  u8 r = 0, g = 0, b = 0;
  bool operator==(const Rgb&) const = default;
};

}  // namespace pfr
