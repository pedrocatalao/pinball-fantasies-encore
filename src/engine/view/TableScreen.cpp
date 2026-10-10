#include "engine/view/TableScreen.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace encore {

TableScreen::TableScreen(const std::filesystem::path& prg, int table) : data_(TableData::load(prg, table)) {
  ball_ = BallSprite::decode(data_.code);
}

TableScreen::TableScreen(Bytes prg, int table) : data_(TableData::parse(std::move(prg), "TABLE" + std::to_string(table + 1) + ".PRG", table)) {
  ball_ = BallSprite::decode(data_.code);
}

namespace {

/// Marks everything a shape encloses, by flooding what is outside it instead.
void fillEnclosed(u8* mask, int w, int h) {
  std::vector<u8> outside(static_cast<std::size_t>(w) * h, 0);
  std::vector<int> queue;
  auto visit = [&](int x, int y) {
    const std::size_t p = static_cast<std::size_t>(y) * w + x;
    if (mask[p] || outside[p]) return;
    outside[p] = 1;
    queue.push_back(static_cast<int>(p));
  };
  for (int x = 0; x < w; ++x) visit(x, 0), visit(x, h - 1);
  for (int y = 0; y < h; ++y) visit(0, y), visit(w - 1, y);
  while (!queue.empty()) {
    const int p = queue.back();
    queue.pop_back();
    const int x = p % w, y = p / w;
    if (x > 0) visit(x - 1, y);
    if (x + 1 < w) visit(x + 1, y);
    if (y > 0) visit(x, y - 1);
    if (y + 1 < h) visit(x, y + 1);
  }
  for (std::size_t p = 0; p < outside.size(); ++p)
    if (!outside[p]) mask[p] = 1;
}

}  // namespace

void TableScreen::attach(Engine& engine) {
  picture_ = data_.playfield;
  // (the picture's first row is rubbed out as the table starts: cs:31b5)
  if (!keepTopRow) std::fill_n(picture_.begin(), kWidth, u8{0});
  engine.onFlipperDrawn = [this, &engine](u16 record, u16 was, u16 now) { turnFlipper(engine, picture_, record, was, now); };

  // Each flipper's shape at every step of its travel, from the pictures of it that the ball is
  // stopped by: taken now, before the table's start-up adds the wall round it to them (cs:3d81).
  art_.clear();
  const u16 pictures[3] = {engine.S(0x4c54), engine.S(0x4fc2), engine.S(0x4eb6)};
  int which = 0;
  for (u16 f = engine.A(0x6950); which < 3 && engine.nativeB(f) != 0; f = static_cast<u16>(f + 0x3c), ++which) {
    FlipperArt art;
    const int bytes = engine.nativeW(static_cast<u16>(f + 0x06)) * 2;
    art.record = f;
    art.x = engine.nativeW(static_cast<u16>(f + 0x02));
    art.y = engine.nativeW(static_cast<u16>(f + 0x04));
    art.w = bytes * 8;
    art.h = engine.nativeW(static_cast<u16>(f + 0x08));
    art.steps = engine.nativeW(static_cast<u16>(f + 0x20)) + 1;
    const std::size_t pixels = static_cast<std::size_t>(art.w) * art.h;
    art.shape.assign(static_cast<std::size_t>(art.steps) * pixels, 0);
    for (int q = 0; q < art.steps; ++q)
      for (int y = 0; y < art.h; ++y)
        for (int x = 0; x < art.w; ++x)
          if (engine.farB(pictures[which], static_cast<u16>(q * bytes * art.h + y * bytes + (x >> 3))) & (0x80 >> (x & 7)))
            art.shape[static_cast<std::size_t>(q) * pixels + static_cast<std::size_t>(y) * art.w + x] = 1;
    art_.push_back(std::move(art));
  }
}

/// Once the table has started, what the pictures drawn again at high resolution need of it is
/// worked out: what hides the ball and how much, each flipper taken out of its artwork, and
/// which lamp each dot of the playfield belongs to.
void TableScreen::started(Engine& e) {
  buildCover(e);
  buildFlipperArt(e);
  buildLampAreas(e);
}

void TableScreen::follow(Engine& e) {
  // (how far from where the engine changed over, in dots, and for how long: a ball that rolls
  // on under another ramp is soon taken as under it, at whatever speed it goes)
  constexpr int kFarthest = 48, kLongest = 60;
  const Engine::Shown& shown = e.shown();
  if (!shown.ramps && drawnOnRamps_ && !hides_[0].empty()) {
    if (stillOnRamps_ == 0) changedAt_ = {shown.ballX, shown.ballY};
    const int dx = shown.ballX - changedAt_[0], dy = shown.ballY - changedAt_[1];
    // (the dots of the ball the playfield's map hides and the ramps' does not: the ramp left)
    bool under = stillOnRamps_ < kLongest && dx * dx + dy * dy <= kFarthest * kFarthest;
    if (under) {
      under = false;
      for (int y = 0; y < ball_.height && !under; ++y)
        for (int x = 0; x < ball_.width && !under; ++x) {
          const int px = shown.ballX + x, py = shown.ballY + y;
          if (!ball_.covers(x, y) || px < 0 || px >= kWidth || py < 0 || py >= TableData::kHeight) continue;
          const std::size_t i = static_cast<std::size_t>(py) * kWidth + px;
          under = hides_[0][i] && !hides_[1][i];
        }
    }
    if (under) {
      ++stillOnRamps_;
      return;
    }
  }
  drawnOnRamps_ = shown.ramps;
  stillOnRamps_ = 0;
}

/// What the artwork covers of the ball. The original hides the ball dot by dot, and where
/// something is to be seen through (the criss-cross rail on Stones 'n Bones) hides every
/// other dot; for a picture of the ball at any size that is told as how much of the ball
/// each dot hides. Artwork that covers a spot and the ones around it is solid, and hides the
/// ball outright, as the original does. Only where the cover is dithered against what is
/// behind it does the share of the dots around decide how much shows through: averaging
/// everywhere would leave a rim of the ball visible wherever a solid cover ends, since a dot
/// within two of that edge averages below its own value.
void TableScreen::buildCover(Engine& e) {
  const u16 maps = e.S(0x2f94);
  for (std::size_t layer = 0; layer < 2; ++layer) {
    const u16 map = layer ? 0x5f80 : 0x0100;
    Bytes hides(static_cast<std::size_t>(kWidth) * TableData::kHeight);
    for (int y = 0; y < TableData::kHeight; ++y)
      for (int x = 0; x < kWidth; ++x)
        hides[static_cast<std::size_t>(y) * kWidth + x] = (e.farB(maps, static_cast<u16>(map + y * 42 + (x >> 3))) & (0x80 >> (x & 7))) ? 1 : 0;
    auto at = [&](int x, int y) { return hides[static_cast<std::size_t>(y) * kWidth + x] != 0; };
    hides_[layer] = hides;
    cover_[layer].assign(hides.size(), 0);
    for (int y = 0; y < TableData::kHeight; ++y)
      for (int x = 0; x < kWidth; ++x) {
        int near = 0;
        if (x > 0) near += at(x - 1, y);
        if (x < kWidth - 1) near += at(x + 1, y);
        if (y > 0) near += at(x, y - 1);
        if (y < TableData::kHeight - 1) near += at(x, y + 1);
        int cover = 255;
        if (!at(x, y) || near < 3) {
          int covered = 0, counted = 0;
          for (int ny = y - 2; ny <= y + 2; ++ny)
            for (int nx = x - 2; nx <= x + 2; ++nx) {
              if (nx < 0 || nx >= kWidth || ny < 0 || ny >= TableData::kHeight) continue;
              covered += at(nx, ny);
              ++counted;
            }
          cover = covered * 255 / counted;
        }
        cover_[layer][static_cast<std::size_t>(y) * kWidth + x] = static_cast<u8>(cover);
      }
  }
}

/// Takes each flipper out of its artwork, so it can be drawn turned to any angle.
///
/// The original draws a flipper as one of its pictures of the flipper's rectangle, each
/// holding the flipper and the playfield behind it. Which dots are the flipper is not a
/// question the pictures can answer on their own -- the hub looks the same in all of them --
/// but the table already knows: the shape the ball is stopped by at each step. That shape,
/// widened to take in the outline drawn around it, is the flipper; the playfield behind it is
/// what the other pictures show in its place, and the angle of a step is the direction of the
/// far end of its shape from what it turns about.
void TableScreen::buildFlipperArt(Engine& e) {
  // the flipper's pictures, got as the original gets them: step by step from the one at rest
  Bytes scratch = data_.playfield;
  for (FlipperArt& art : art_) {
    const int w = art.w, h = art.h, steps = art.steps;
    const std::size_t pixels = static_cast<std::size_t>(w) * h;
    std::vector<Bytes> drawn(static_cast<std::size_t>(steps));
    for (int q = 0; q < steps; ++q) {
      if (q > 0) turnFlipper(e, scratch, art.record, static_cast<u16>(q - 1), static_cast<u16>(q));
      drawn[static_cast<std::size_t>(q)].resize(pixels);
      for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
          drawn[static_cast<std::size_t>(q)][static_cast<std::size_t>(y) * w + x] =
              art.x + x < kWidth && art.y + y < TableData::kHeight ? scratch[static_cast<std::size_t>(art.y + y) * kWidth + art.x + x] : 0;
    }
    for (int q = steps - 1; q > 0; --q) turnFlipper(e, scratch, art.record, static_cast<u16>(q), static_cast<u16>(q - 1));
    art.atRest = drawn[0];

    Bytes& coveredAt = art.shape;
    art.background.assign(pixels, 0);
    art.covered.assign(pixels, 0);
    art.rest.assign(pixels, 0);
    std::vector<u8> known(pixels, 0);
    for (std::size_t p = 0; p < pixels; ++p) {
      std::array<u8, 256> count{};
      u8 commonest = 0;
      for (std::size_t q = 0; q < static_cast<std::size_t>(steps); ++q) {
        if (coveredAt[q * pixels + p]) {
          art.covered[p] = 1;
          continue;
        }
        const u8 here = drawn[q][p];
        known[p] = 1;
        if (++count[here] > count[commonest]) commonest = here;
      }
      art.background[p] = commonest;
    }
    // The playfield is never seen under the hub, so the nearest of it is spread inwards.
    for (int pass = 0; pass < std::max(w, h); ++pass) {
      bool spread = false;
      for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
          const std::size_t p = static_cast<std::size_t>(y) * w + x;
          if (known[p]) continue;
          const std::array<int, 4> around = {x > 0 ? static_cast<int>(p) - 1 : -1, x + 1 < w ? static_cast<int>(p) + 1 : -1,
                                             y > 0 ? static_cast<int>(p) - w : -1, y + 1 < h ? static_cast<int>(p) + w : -1};
          for (const int n : around)
            if (n >= 0 && known[static_cast<std::size_t>(n)] == 1) {
              art.background[p] = art.background[static_cast<std::size_t>(n)];
              known[p] = 2;  // filled in this pass; usable in the next one
              spread = true;
              break;
            }
        }
      for (u8& k : known)
        if (k == 2) k = 1;
      if (!spread) break;
    }

    // The artwork draws an outline around the flipper that the ball's shape of it does not have.
    for (std::size_t q = 0; q < static_cast<std::size_t>(steps); ++q) {
      u8* covered = &coveredAt[q * pixels];
      std::vector<u8> grown(covered, covered + pixels);
      for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
          const std::size_t p = static_cast<std::size_t>(y) * w + x;
          if (covered[p] || drawn[q][p] == art.background[p]) continue;
          for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx) {
              const int nx = x + dx, ny = y + dy;
              if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
              if (covered[static_cast<std::size_t>(ny) * w + nx]) grown[p] = 1;
            }
        }
      std::copy(grown.begin(), grown.end(), covered);
      fillEnclosed(covered, w, h);
      for (std::size_t p = 0; p < pixels; ++p) {
        if (!covered[p]) continue;
        art.covered[p] = 1;
        if (q == 0) art.rest[p] = 1;
      }
    }

    // What the flipper turns about. The table's own place for it is where the ball is made
    // to bounce, which for the upper bats is a few dots from where their artwork hinges, so
    // the artwork's own axis is taken from the pictures: two steps of one rigid shape, the turn
    // between them is the difference of their long ways through, and the axis is its fixed point.
    auto moments = [&](const u8* mask) {
      double cx = 0, cy = 0;
      int n = 0;
      for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
          if (mask[static_cast<std::size_t>(y) * w + x]) cx += x, cy += y, ++n;
      if (!n) return std::array<double, 3>{0, 0, 0};
      cx /= n;
      cy /= n;
      double xx = 0, yy = 0, xy = 0;
      for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
          if (mask[static_cast<std::size_t>(y) * w + x]) {
            xx += (x - cx) * (x - cx);
            yy += (y - cy) * (y - cy);
            xy += (x - cx) * (y - cy);
          }
      return std::array<double, 3>{cx, cy, 0.5 * std::atan2(2 * xy, xx - yy)};
    };
    double ox = static_cast<i16>(e.nativeW(static_cast<u16>(art.record + 0x12))) - art.x;
    double oy = static_cast<i16>(e.nativeW(static_cast<u16>(art.record + 0x14))) - art.y;
    if (steps > 1) {
      const auto m0 = moments(&coveredAt[0]), m1 = moments(&coveredAt[static_cast<std::size_t>(steps - 1) * pixels]);
      double turn = m1[2] - m0[2];
      while (turn > 1.57079633) turn -= 3.14159265;
      while (turn < -1.57079633) turn += 3.14159265;
      // A shape that hardly turns says little about where its axis is; then the table's place stands.
      if (std::abs(turn) > 0.17453293) {  // ten degrees
        const double c = std::cos(turn), s = std::sin(turn);
        const double bx = m1[0] - (c * m0[0] - s * m0[1]), by = m1[1] - (s * m0[0] + c * m0[1]);
        const double d = (1 - c) * (1 - c) + s * s;
        const double ax = ((1 - c) * bx - s * by) / d, ay = (s * bx + (1 - c) * by) / d;
        // ... and neither does a fit that lands somewhere the hinge cannot be.
        if (std::hypot(ax - ox, ay - oy) < 8 && ax >= 0 && ay >= 0 && ax < w && ay < h) {
          ox = ax;
          oy = ay;
        }
      }
    }
    art.axisX = static_cast<float>(ox);
    art.axisY = static_cast<float>(oy);
    art.angle.clear();
    for (std::size_t q = 0; q < static_cast<std::size_t>(steps); ++q) {
      const u8* covered = &coveredAt[q * pixels];
      double far = 0;
      for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
          if (covered[static_cast<std::size_t>(y) * w + x]) far = std::max(far, std::hypot(x - ox, y - oy));
      // The direction of the flipper's far end from its axis, which is its angle.
      double sx = 0, sy = 0;
      for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
          if (!covered[static_cast<std::size_t>(y) * w + x]) continue;
          if (std::hypot(x - ox, y - oy) < 0.85 * far) continue;
          sx += x - ox;
          sy += y - oy;
        }
      art.angle.push_back(static_cast<float>(std::atan2(sy, sx)));
    }
  }
}

/// Where a flipper is now, in radians from its resting position. The table moves a flipper
/// smoothly and only picks a picture by steps of 55, so the angle follows the steps either side.
float TableScreen::flipperAngle(Engine& e, const FlipperArt& art) const {
  const int pos = std::max<int>(static_cast<i16>(e.nativeW(static_cast<u16>(art.record + 0x1c))), 0);
  const std::size_t q = std::min<std::size_t>(static_cast<std::size_t>(pos / 55), art.angle.size() - 1);
  const std::size_t next = std::min<std::size_t>(q + 1, art.angle.size() - 1);
  const float frac = static_cast<float>(pos % 55) / 55.0f;
  float step = art.angle[next] - art.angle[q];
  if (step > 3.14159265f) step -= 6.2831853f;  // the right flipper's angles cross half a turn
  if (step < -3.14159265f) step += 6.2831853f;
  float angle = art.angle[q] + step * frac - art.angle[0];
  if (angle > 3.14159265f) angle -= 6.2831853f;
  if (angle < -3.14159265f) angle += 6.2831853f;
  return angle;
}

/// Which lamp each dot of the playfield belongs to, for the high-resolution pictures.
///
/// A lamp is a few of the picture's colours, so its dots are the ones drawn in them. Where the
/// artwork dithers a lamp against what is behind it (the criss-cross rail on Stones 'n Bones
/// lays a chequerboard over the green lamps), the lamp only owns every other dot, which a
/// high-resolution picture would show as a coarse chequerboard of lit and unlit. So a gap
/// with three or four neighbours of one lamp is taken into it: that closes a dither without
/// spilling past an edge, where a dot has one or two such neighbours.
void TableScreen::buildLampAreas(Engine& e) {
  const int w = kWidth, h = TableData::kHeight;
  lampAreas_.assign(static_cast<std::size_t>(w) * h, 0);
  std::array<u8, 256> ofColour{};
  lamps_.clear();
  for (u16 light = 1, lights = e.kw(0x5912, 1); light <= lights && light < 256; ++light) {
    const u16 record = e.W(0x12bd, static_cast<u16>((light - 1) * 2));
    const u16 first = e.nativeB(record), colours = static_cast<u16>(e.nativeB(static_cast<u16>(record + 1)) / 3);
    for (u16 i = 0; i < colours && first + i < 256; ++i) ofColour[first + i] = static_cast<u8>(light);
    lamps_.push_back(record);
  }
  for (std::size_t p = 0; p < lampAreas_.size(); ++p) lampAreas_[p] = ofColour[data_.playfield[p]];

  std::vector<u8> filled = lampAreas_;
  for (int y = 1; y < h - 1; ++y)
    for (int x = 1; x < w - 1; ++x) {
      const std::size_t p = static_cast<std::size_t>(y) * w + x;
      if (lampAreas_[p]) continue;
      const u8 around[4] = {lampAreas_[p - 1], lampAreas_[p + 1], lampAreas_[p - w], lampAreas_[p + w]};
      for (const u8 lamp : around) {
        if (!lamp) continue;
        int same = 0;
        for (const u8 other : around) same += other == lamp;
        if (same >= 3) {
          filled[p] = lamp;
          break;
        }
      }
    }
  lampAreas_ = std::move(filled);

  // What a lamp encloses belongs to it, whatever colour it is drawn in: the words and numbers
  // inside an insert are not in the lamp's colours, but they light and go out with it.
  //
  // A lamp's ring is often a dot short here and there, which would let the outside leak into
  // the middle of it, so what is outside is found against the lamps grown by a dot all round.
  // That grown edge is then peeled off again, leaving what a lamp really encloses.
  constexpr std::size_t kLargestInside = 600;
  const auto at = [&](int x, int y) { return static_cast<std::size_t>(y) * w + x; };
  std::vector<u8> grown(lampAreas_.size(), 0);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      if (!lampAreas_[at(x, y)]) continue;
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx)
          if (x + dx >= 0 && y + dy >= 0 && x + dx < w && y + dy < h) grown[at(x + dx, y + dy)] = 1;
    }
  std::vector<u8> outside(lampAreas_.size(), 0);
  std::vector<int> queue;
  auto flood = [&](int x, int y) {
    if (grown[at(x, y)] || outside[at(x, y)]) return;
    outside[at(x, y)] = 1;
    queue.push_back(static_cast<int>(at(x, y)));
  };
  for (int x = 0; x < w; ++x) flood(x, 0), flood(x, h - 1);
  for (int y = 0; y < h; ++y) flood(0, y), flood(w - 1, y);
  while (!queue.empty()) {
    const int p = queue.back();
    queue.pop_back();
    const int x = p % w, y = p / w;
    if (x > 0) flood(x - 1, y);
    if (x + 1 < w) flood(x + 1, y);
    if (y > 0) flood(x, y - 1);
    if (y + 1 < h) flood(x, y + 1);
  }
  // Inside a lamp: not the lamp itself, not outside it, and not the dot of slack that
  // growing the lamps left around them.
  std::vector<u8> inside(lampAreas_.size(), 0);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      if (lampAreas_[at(x, y)] || outside[at(x, y)]) continue;
      bool touchesOutside = false;
      for (int dy = -1; dy <= 1 && !touchesOutside; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
          const int nx = x + dx, ny = y + dy;
          if (nx >= 0 && ny >= 0 && nx < w && ny < h && outside[at(nx, ny)]) {
            touchesOutside = true;
            break;
          }
        }
      if (!touchesOutside) inside[at(x, y)] = 1;
    }
  std::vector<u8> seen(lampAreas_.size(), 0);
  std::vector<int> enclosed;
  for (int sy = 0; sy < h; ++sy)
    for (int sx = 0; sx < w; ++sx) {
      if (!inside[at(sx, sy)] || seen[at(sx, sy)]) continue;
      enclosed.clear();
      queue.assign(1, static_cast<int>(at(sx, sy)));
      seen[at(sx, sy)] = 1;
      std::array<std::size_t, 256> around{};
      while (!queue.empty()) {
        const int p = queue.back();
        queue.pop_back();
        enclosed.push_back(p);
        const int x = p % w, y = p / w;
        for (int dy = -1; dy <= 1; ++dy)
          for (int dx = -1; dx <= 1; ++dx) {
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            const u8 lamp = lampAreas_[at(nx, ny)];
            if (lamp) {
              ++around[lamp];
            } else if (inside[at(nx, ny)] && !seen[at(nx, ny)]) {
              seen[at(nx, ny)] = 1;
              queue.push_back(static_cast<int>(at(nx, ny)));
            }
          }
      }
      if (enclosed.size() > kLargestInside) continue;
      // Whichever lamp surrounds it most.
      const auto lamp = static_cast<u8>(std::max_element(around.begin() + 1, around.end()) - around.begin());
      if (!around[lamp]) continue;
      for (const int p : enclosed) lampAreas_[static_cast<std::size_t>(p)] = lamp;
    }

  // Finally every lamp grows a little, so that what switches is the whole of it and a touch
  // more. Outside a lamp the two pictures are the same, so the extra costs nothing; inside, it
  // makes sure nothing of the lamp is left behind by an edge that does not quite line up.
  constexpr int kGrow = 2;
  for (int pass = 0; pass < kGrow; ++pass) {
    std::vector<u8> wider = lampAreas_;
    for (int y = 0; y < h; ++y)
      for (int x = 0; x < w; ++x) {
        if (lampAreas_[at(x, y)]) continue;
        std::array<u8, 9> near{};
        std::size_t count = 0;
        for (int dy = -1; dy <= 1; ++dy)
          for (int dx = -1; dx <= 1; ++dx) {
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            if (const u8 lamp = lampAreas_[at(nx, ny)]) near[count++] = lamp;
          }
        if (!count) continue;
        // Whichever lamp is nearest on the most sides.
        std::array<std::size_t, 256> tally{};
        u8 best = 0;
        for (std::size_t i = 0; i < count; ++i)
          if (++tally[near[i]] > tally[best]) best = near[i];
        wider[at(x, y)] = best;
      }
    lampAreas_ = std::move(wider);
  }
}

/// cs:550e, cs:69aa: the original turns a flipper by copying, from one way it stands to the
/// next, only the groups of four dots that change: for every step there is a list of where in
/// the picture a group goes and where in the video card's memory it is kept, and another
/// for the step back. Turning several steps at once it leaves out of each list what a later
/// one will draw over, by a count kept for it, and not always all of that: so the same is
/// done here, list by list, on the picture kept for the screen.
void TableScreen::turnFlipper(Engine& e, Bytes& picture, u16 record, u16 was, u16 now) const {
  auto w = [&](u16 o) { return e.nativeW(static_cast<u16>(record + o)); };
  const u16 lists = e.S(0x0c0f);
  const auto& video = e.videoMemory();
  const std::size_t base = static_cast<std::size_t>(w(0x04)) * 0x54 + (w(0x02) >> 2);  // in groups of four dots, 84 to a row
  const u16 stride = w(0x38);
  u16 list = now > was ? static_cast<u16>(was * stride + w(0x36)) : static_cast<u16>(w(0x30) - was * stride);
  int steps = now > was ? now - was : was - now;
  while (steps > 0) {
    int left = steps > 9 ? 9 : steps;
    steps -= left;
    for (; left > 0; --left, list = static_cast<u16>(list + stride)) {
      const u16 count = e.farW(lists, static_cast<u16>(list + (left - 1) * 2));
      for (u16 i = 0; i < count; ++i) {
        const u16 entry = static_cast<u16>(list + 0x12 + i * 4);
        const std::size_t to = base + e.farW(lists, entry);
        const u16 from = e.farW(lists, static_cast<u16>(entry + 2));
        const std::size_t row = to / 0x54, column = to % 0x54;
        if (row >= TableData::kHeight || column >= 80) continue;
        for (std::size_t plane = 0; plane < 4; ++plane) picture[row * kWidth + column * 4 + plane] = video[plane][from];
      }
    }
  }
}

/// Which lamps are lit: a lamp's colours are as the table gives them when it is lit and half
/// that when it is out (cs:5728, cs:5747), and the colours the video card now has say which.
void TableScreen::lampsLit(Engine& e, int lamps, std::array<bool, 256>& lit) const {
  lit.fill(false);
  const auto& dac = e.colours();
  for (std::size_t l = 0; l < lamps_.size(); ++l) {
    const u16 record = lamps_[l];
    const u16 colour = e.nativeB(record);
    int best = 0;
    bool on = false;
    for (u16 part = 0; part < 3 && e.nativeB(static_cast<u16>(record + 1)) >= 3; ++part) {
      const int full = e.nativeB(static_cast<u16>(record + 2 + part)) & 0x3f, half = full >> 1;
      if (full - half <= best) continue;
      best = full - half;
      on = dac[colour * 3u + part] * 2 > full + half;
    }
    lit[l + 1] = lamps == 1 ? true : lamps == 2 ? false : on;
  }
}

void TableScreen::colours(Engine& e, Rgb* out, int lamps) const {
  std::array<u8, 768> dac = e.colours();
  if (lamps != 0)
    for (u16 light = 1, lights = e.kw(0x5912, 1); light <= lights; ++light) {
      u16 record = e.W(0x12bd, static_cast<u16>((light - 1) * 2));
      u16 at = static_cast<u16>(e.nativeB(record++) * 3);
      const u8 bytes = e.nativeB(record++);
      for (u8 i = 0; i < bytes && at < 768; ++i, ++at) {
        const u8 full = e.nativeB(record++) & 0x3f;
        dac[at] = lamps == 1 ? full : full >> 1;
      }
    }
  auto wide = [](u8 v) { return static_cast<u8>((v << 2) | (v >> 4)); };  // 0-63 as 0-255
  for (std::size_t i = 0; i < 256; ++i) out[i] = Rgb{wide(dac[i * 3]), wide(dac[i * 3 + 1]), wide(dac[i * 3 + 2])};
}

void TableScreen::draw(Engine& e, u8* frame, const View& v, HdFrame* hd) const {
  const int height = v.height, view = height - kDisplayRows, top = v.top;
  std::memset(frame, 0, static_cast<std::size_t>(kWidth) * static_cast<std::size_t>(height));
  for (int y = 0; y < view; ++y) {
    const int row = top + y;
    if (row < 0 || row >= TableData::kHeight || picture_.empty()) continue;
    std::memcpy(frame + static_cast<std::size_t>(y) * kWidth, picture_.data() + static_cast<std::size_t>(row) * kWidth, kWidth);
  }
  const Engine::Shown& shown = e.shown();
  const bool waiting = e.B(0x3713) == 0xff;  // no game: the original has no ball to show
  if (hd && !lampAreas_.empty()) {
    // Every dot of the window is a dot of the playfield's picture: the unlit one, with how lit
    // the spot is alongside, which the renderer blends the lit picture in by. What a lamp
    // covers on screen is decided by the pictures, which differ only where the lamp is, so
    // this only has to say how lit the place is -- and it is taken over the dots around,
    // because the original's own edge is a staircase and a sharp reading of it would show
    // through as a jagged rim.
    hd->reset(kWidth, height);
    const auto off = static_cast<u16>(static_cast<int>(HdPicture::Playfield1Off) + data_.index);
    const auto on = static_cast<u16>(static_cast<int>(HdPicture::Playfield1On) + data_.index);
    hd->used |= (1u << off) | (1u << on);
    hd->size[off] = hd->size[on] = {static_cast<u16>(kWidth), static_cast<u16>(TableData::kHeight)};
    std::array<bool, 256> lampLit;
    lampsLit(e, v.lamps, lampLit);
    for (int y = 0; y < view; ++y) {
      const int row = top + y;
      if (row < 0 || row >= TableData::kHeight) continue;
      HdPixel* out = hd->map.data() + static_cast<std::size_t>(y) * kWidth;
      for (int x = 0; x < kWidth; ++x) {
        int around = 0;
        for (int ny = std::max(row - 1, 0); ny <= std::min(row + 1, TableData::kHeight - 1); ++ny)
          for (int nx = std::max(x - 1, 0); nx <= std::min(x + 1, kWidth - 1); ++nx)
            around += lampLit[lampAreas_[static_cast<std::size_t>(ny) * kWidth + nx]];
        const u16 lit = static_cast<u16>(around * HdFrame::kLitMax / 9);
        out[x] = {static_cast<u16>(x * 8), static_cast<u16>(row * 8), static_cast<u16>(off | (lit << HdFrame::kLitShift)), 0};
      }
    }
    // The renderer draws each flipper turned to its angle. One with a picture of its own has
    // the playfield's picture behind it; one cut out of the original artwork has the
    // original's playfield there instead, as the other pictures of it show it, and the
    // replacement picture steps aside wherever the flipper may reach.
    for (std::size_t f = 0; f < art_.size(); ++f) {
      const FlipperArt& art = art_[f];
      if (hd->ownSprites & (1u << f)) continue;
      for (int fy = 0; fy < art.h; ++fy) {
        const int y = art.y + fy - top;
        if (y < 0 || y >= view) continue;
        for (int fx = 0; fx < art.w && art.x + fx < kWidth; ++fx) {
          const std::size_t p = static_cast<std::size_t>(fy) * art.w + fx, to = static_cast<std::size_t>(y) * kWidth + art.x + fx;
          frame[to] = art.background[p];
          if (art.covered[p]) hd->map[to].picture = 0;
        }
      }
    }
  } else {
    hd = nullptr;
  }
  auto put = [&](int x, int row, u8 colour) {
    const int y = row - top;
    if (x >= 0 && x < kWidth && y >= 0 && y < view) frame[static_cast<std::size_t>(y) * kWidth + static_cast<std::size_t>(x)] = colour;
  };
  // the plunger (cs:6600): its picture moves down as it is pulled, and what it leaves is dark
  {
    const int width = e.kw(0x65e9, 1), rows = e.kw(0x6660, 1);
    const int at = e.kw(0x662a, 1) - 0x0ad4;  // where in the video card's memory, past the display
    const int row0 = at / 0x54, x0 = (at % 0x54) * 4;
    const int pull = static_cast<i8>((e.B(0x23a5) >> 1) - 3);
    const int down = pull < 0 ? 0 : pull, skip = pull < 0 ? -pull : 0;
    const auto plunger = static_cast<u16>(HdPicture::Plunger);
    if (hd) {
      hd->used |= 1u << plunger;
      hd->size[plunger] = {static_cast<u16>(width), static_cast<u16>(data_.plungerImage.size() / static_cast<std::size_t>(width))};
    }
    auto mark = [&](int x, int row, int px, int py) {  // a dot of the plunger's own picture, or of none
      const int y = row - top;
      if (!hd || x < 0 || x >= kWidth || y < 0 || y >= view) return;
      HdPixel& m = hd->map[static_cast<std::size_t>(y) * kWidth + static_cast<std::size_t>(x)];
      m = px < 0 ? HdPixel{} : HdPixel{static_cast<u16>(px * 8), static_cast<u16>(py * 8), plunger, 0};
    };
    // (with the pictures drawn again, what the plunger leaves is the playfield's picture)
    for (int y = 0; y < down; ++y)
      for (int x = 0; x < width; ++x)
        put(x0 + x, row0 + y, artBehindPlunger ? picture_[static_cast<std::size_t>(row0 + y) * kWidth + static_cast<std::size_t>(x0 + x)] : u8{0});
    for (int y = 0; y < rows - down; ++y)
      for (int x = 0; x < width; ++x) {
        const std::size_t i = static_cast<std::size_t>((skip + y) * width + x);
        if (i >= data_.plungerImage.size()) continue;
        put(x0 + x, row0 + down + y, data_.plungerImage[i]);
        mark(x0 + x, row0 + down + y, x, skip + y);
      }
  }
  // the ball, but for the dots of it that something on the table passes over (cs:95b0)
  if (ball_.valid() && !hd) {
    const u16 maps = e.S(0x2f94);
    const u16 map = drawnOnRamps_ ? 0x5f80 : 0x0100;
    for (int y = 0; y < ball_.height; ++y)
      for (int x = 0; x < ball_.width; ++x) {
        if (!ball_.covers(x, y)) continue;
        const int px = shown.ballX + x, py = shown.ballY + y;
        // (a ball leaving by the bottom is still drawn in the few rows past the picture that a
        // shaken table shows)
        if (px < 0 || px >= kWidth || py < 0 || py >= TableData::kHeight + 4) continue;
        if (e.farB(maps, static_cast<u16>(map + py * 42 + (px >> 3))) & (0x80 >> (px & 7))) continue;
        put(px, py, ball_.at(x, y));
      }
  }
  if (hd) {
    for (std::size_t f = 0; f < art_.size(); ++f) {
      const FlipperArt& art = art_[f];
      const auto rectW = static_cast<float>(art.w), rectH = static_cast<float>(art.h);
      HdSprite s;
      s.picture = static_cast<u16>(f);
      s.pivotFrameX = static_cast<float>(art.x) + art.axisX;
      s.pivotFrameY = static_cast<float>(art.y - top) + art.axisY;
      s.pivotSpriteX = art.axisX / rectW;
      s.pivotSpriteY = art.axisY / rectH;
      s.scaleX = 1.0f / rectW;
      s.scaleY = 1.0f / rectH;
      s.angle = flipperAngle(e, art);
      s.clipTop = 0;
      s.clipBottom = static_cast<float>(view);
      hd->sprites.push_back(s);
    }
    // The ball goes last, so it passes in front of the flippers, and a faint trail of where it
    // has just been goes immediately before it. The trail follows the ball's own steps rather
    // than the frames, which is what makes it flow rather than step.
    if (ball_.valid() && !waiting) {
      // (a ball in play is drawn lifted with a shaken table, as the original draws it: cs:4140)
      const float lift = e.B(at::ballHidden) == 0xff ? 0.0f : static_cast<float>(e.W(at::nudgeLift).s());
      const float bx = static_cast<float>(e.W(at::ballX).s()), by = static_cast<float>(e.W(at::ballY).s()) + lift;
      // What the artwork covers of the ball is said wherever the ball is drawn, the trail
      // included, and a little wider than the ball itself: a dot's cover is read smoothly,
      // from the four around it, and without that the outermost ring of the ball would read
      // half its cover from dots nothing was said of.
      const std::size_t layer = drawnOnRamps_ ? 1 : 0;
      hd->ballLayer = static_cast<u8>(layer);
      constexpr int kMargin = 3;
      auto cover = [&](float fx, float fy) {
        const int px = static_cast<int>(std::lround(fx)), py = static_cast<int>(std::lround(fy));
        for (int oy = -kMargin; oy < 15 + kMargin; ++oy) {
          const int sy = py + oy, y = sy - top;
          if (y < 0 || y >= view || sy < 0 || sy >= TableData::kHeight) continue;
          for (int ox = -kMargin; ox < 15 + kMargin; ++ox) {
            const int x = px + ox;
            if (x < 0 || x >= kWidth) continue;
            const std::size_t p = static_cast<std::size_t>(sy) * kWidth + x;
            HdPixel& m = hd->map[static_cast<std::size_t>(y) * kWidth + x];
            m.flags = static_cast<u16>((m.flags & 0xff) | (cover_[layer][p] << HdPixel::kCoverShift));
            if (hides_[layer][p]) m.flags |= HdPixel::kHidesBall;
          }
        }
      };
      auto ball = [&](float x, float y, float opacity) {
        HdSprite s;
        s.picture = HdSprite::kBall;
        s.pivotFrameX = x + 7.5f;
        s.pivotFrameY = y - static_cast<float>(top) + 7.5f;
        s.pivotSpriteX = s.pivotSpriteY = 0.5f;
        s.scaleX = s.scaleY = 1.0f / 15.0f;
        s.clipTop = 0;
        s.clipBottom = static_cast<float>(view);
        s.hiddenBy = HdPixel::kHidesBall;
        s.opacity = opacity;
        hd->sprites.push_back(s);
      };
      // (where the ball was before each of its last fourteen steps: the newest kept is where it
      // is now, which the ball itself covers)
      const auto& steps = e.steps();
      const std::size_t kept = e.stepsKept(), length = kept > 0 ? kept - 1 : 0;
      for (std::size_t i = 0; v.ballTrail && i < length; ++i) {
        const Engine::Step& was = steps[Engine::kSteps - kept + i];
        const float x = static_cast<float>(was.x) / 1024.0f, y = static_cast<float>(was.y) / 1024.0f + lift;
        // Strongest just behind the ball, fading away towards the oldest step.
        const float recent = static_cast<float>(i + 1) / static_cast<float>(length);
        const float away = std::hypot(x - bx, y - by);
        // Nothing where the ball has hardly moved, so that a ball at rest keeps to itself, and
        // the faster it goes the more of a trail it leaves.
        if (away < 0.4f) continue;
        cover(x, y);
        ball(x, y, 0.22f * recent * recent * std::min(away / 2.5f, 1.0f));
      }
      cover(bx, by);
      ball(bx, by, 1.0f);
    }
  }

  // the display, which the video card shows below the playfield though it comes first in its
  // memory: a dot is a byte of the first or the third plane, every other column
  const auto& video = e.videoMemory();
  for (int y = 0; y < kDisplayRows; ++y) {
    u8* out = frame + static_cast<std::size_t>(view + y) * kWidth;
    for (int x = 0; x < kWidth; ++x) out[x] = video[static_cast<std::size_t>(x & 3)][static_cast<std::size_t>(y * 0x54 + (x >> 2))];
  }
}

std::vector<bool> TableScreen::flipperIsLeft(Engine& e) const {
  std::vector<bool> left;
  for (const FlipperArt& art : art_) left.push_back(e.nativeB(art.record) == 2);
  return left;
}

std::vector<Cutout> TableScreen::flipperPictures(Engine& e) const {
  std::vector<Cutout> out;
  std::array<Rgb, 256> colour;
  colours(e, colour.data());
  for (const FlipperArt& art : art_) {
    Cutout c;
    c.width = art.w;
    c.height = art.h;
    c.rgba.assign(static_cast<std::size_t>(c.width * c.height * 4), 0);
    for (std::size_t i = 0; i < art.rest.size() && i < art.atRest.size(); ++i) {
      const Rgb rgb = colour[art.atRest[i]];
      c.rgba[i * 4] = rgb.r, c.rgba[i * 4 + 1] = rgb.g, c.rgba[i * 4 + 2] = rgb.b, c.rgba[i * 4 + 3] = art.rest[i] ? 0xff : 0;
    }
    out.push_back(std::move(c));
  }
  return out;
}

Cutout TableScreen::ballPicture(Engine& e) const {
  Cutout c;
  if (!ball_.valid()) return c;
  std::array<Rgb, 256> colour;
  colours(e, colour.data());
  c.width = ball_.width;
  c.height = ball_.height;
  c.rgba.assign(static_cast<std::size_t>(c.width * c.height * 4), 0);
  for (int y = 0; y < c.height; ++y)
    for (int x = 0; x < c.width; ++x) {
      if (!ball_.covers(x, y)) continue;
      const std::size_t i = static_cast<std::size_t>(y * c.width + x);
      const Rgb rgb = colour[ball_.at(x, y)];
      c.rgba[i * 4] = rgb.r, c.rgba[i * 4 + 1] = rgb.g, c.rgba[i * 4 + 2] = rgb.b, c.rgba[i * 4 + 3] = 0xff;
    }
  return c;
}

}  // namespace encore
