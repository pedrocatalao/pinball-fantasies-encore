#pragma once
// 8-bit indexed framebuffer: the game draws exactly like the original (palette indices),
// the renderer converts to RGB on the GPU. Keeping the indexed surface is what makes
// palette effects and later shaders straightforward.
#include "core/Types.h"

namespace encore {

class Framebuffer {
 public:
  Framebuffer() = default;
  Framebuffer(int width, int height);

  int width() const { return width_; }
  int height() const { return height_; }
  u8* row(int y) { return pixels_.data() + static_cast<std::size_t>(y) * width_; }
  const u8* row(int y) const { return pixels_.data() + static_cast<std::size_t>(y) * width_; }
  const u8* data() const { return pixels_.data(); }
  u8* data() { return pixels_.data(); }
  std::size_t size() const { return pixels_.size(); }

  void clear(u8 index = 0);
  void put(int x, int y, u8 index) {
    if (x >= 0 && y >= 0 && x < width_ && y < height_) pixels_[static_cast<std::size_t>(y) * width_ + x] = index;
  }
  void fillRect(Rect r, u8 index);

 private:
  int width_ = 0;
  int height_ = 0;
  Bytes pixels_;
};

}  // namespace encore
