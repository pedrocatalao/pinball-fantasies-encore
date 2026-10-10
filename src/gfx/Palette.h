#pragma once
#include <array>

#include "core/Types.h"
#include "data/IffImage.h"

namespace encore {

/// 256-entry VGA palette (8-bit per channel in our representation; the original used 6-bit DACs).
class Palette {
 public:
  Palette();
  const std::array<Rgb, 256>& colors() const { return colors_; }
  Rgb& operator[](std::size_t i) { return colors_[i]; }
  const Rgb& operator[](std::size_t i) const { return colors_[i]; }
  void set(std::size_t first, const std::vector<Rgb>& rgb);
  /// Copies the palette in 6-bit VGA precision (x/4*4), matching what the DAC displayed.
  void quantizeTo6Bit();

 private:
  std::array<Rgb, 256> colors_{};
};

}  // namespace encore
