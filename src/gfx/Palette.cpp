#include "gfx/Palette.h"

#include <algorithm>

namespace encore {

Palette::Palette() = default;

void Palette::set(std::size_t first, const std::vector<Rgb>& rgb) {
  for (std::size_t i = 0; i < rgb.size() && first + i < colors_.size(); ++i) colors_[first + i] = rgb[i];
}

void Palette::quantizeTo6Bit() {
  for (auto& c : colors_) {
    c.r = static_cast<u8>((c.r >> 2) * 255 / 63);
    c.g = static_cast<u8>((c.g >> 2) * 255 / 63);
    c.b = static_cast<u8>((c.b >> 2) * 255 / 63);
  }
}

}  // namespace encore
