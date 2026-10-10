#include "gfx/Framebuffer.h"

#include <algorithm>
#include <cstring>

namespace encore {

Framebuffer::Framebuffer(int width, int height)
    : width_(width), height_(height), pixels_(static_cast<std::size_t>(width) * height, 0) {}

void Framebuffer::clear(u8 index) { std::fill(pixels_.begin(), pixels_.end(), index); }

void Framebuffer::fillRect(Rect r, u8 index) {
  const int x0 = std::max(r.x, 0), y0 = std::max(r.y, 0);
  const int x1 = std::min(r.x + r.w, width_), y1 = std::min(r.y + r.h, height_);
  for (int y = y0; y < y1; ++y) std::memset(row(y) + x0, index, static_cast<std::size_t>(std::max(0, x1 - x0)));
}

}  // namespace encore
