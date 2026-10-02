#pragma once
// A dense 2-D array indexed (x, y), like the column-major arrays pfr uses for bitmaps.
#include <vector>

#include "core/Types.h"

namespace pfr {

template <typename T>
class Grid {
 public:
  Grid() = default;
  Grid(int w, int h, T fill = T{}) : w_(w), h_(h), data_(static_cast<std::size_t>(w) * h, fill) {}
  int width() const { return w_; }
  int height() const { return h_; }
  bool empty() const { return data_.empty(); }
  T& operator()(int x, int y) { return data_[static_cast<std::size_t>(y) * w_ + x]; }
  const T& operator()(int x, int y) const { return data_[static_cast<std::size_t>(y) * w_ + x]; }
  bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < w_ && y < h_; }
  /// A copy of the rectangle at (x, y).
  Grid slice(int x, int y, int w, int h) const {
    Grid r(w, h);
    for (int j = 0; j < h; ++j)
      for (int i = 0; i < w; ++i) r(i, j) = (*this)(x + i, y + j);
    return r;
  }
  void paste(int x, int y, const Grid& src) {
    for (int j = 0; j < src.h_; ++j)
      for (int i = 0; i < src.w_; ++i) (*this)(x + i, y + j) = src(i, j);
  }
  const std::vector<T>& raw() const { return data_; }
  bool operator==(const Grid&) const = default;

 private:
  int w_ = 0, h_ = 0;
  std::vector<T> data_;
};

using Grid8 = Grid<u8>;

}  // namespace pfr
