#include "table/Table.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <optional>
#include <utility>

namespace pfr {

namespace {

constexpr std::array<std::array<i16, 5>, 8> kMaterials = {{
    {1792, 448, 400, 300, 38},    // 0: dummy
    {1792, 448, 400, 600, 18},    // 1: dummy
    {1792, 448, 400, 600, 18},    // 2: flipper, patch
    {896, 224, 875, 200, 38},     // 3: rubber (kickers)
    {1792, 448, 400, 300, 38},    // 4: dummy
    {30000, 7500, 1000, 400, 38}, // 5: dummy
    {10000, 2500, 450, 700, 38},  // 6: steel
    {10000, 2500, 400, 500, 38},  // 7: plastic (bumpers)
}};

constexpr std::array<u16, 36> kMatchTimingLow = {24, 23, 21, 21, 18, 16, 15, 13, 11, 9, 8, 7, 7, 6, 6, 6, 5, 5,
                                                 5,  5,  5,  4,  4,  4,  4,  4,  4,  4, 4, 4, 3, 3, 3, 3, 3, 3};
constexpr std::array<u16, 36> kMatchTimingHigh = {22, 28, 25, 25, 22, 19, 18, 15, 13, 11, 9, 9, 8, 8, 7, 7, 6, 6,
                                                  6,  6,  6,  5,  5,  5,  5,  5,  5,  4,  4, 4, 4, 4, 4, 4, 3, 3};

}  // namespace

Table::Table(ByteView prg, ByteView module, const Config& config, int table, u64 seed)
    : Table(TableAssets::load(prg, table), module, config, table, seed) {}

Table::Table(TableAssets assets, ByteView module, const Config& config, int table, u64 seed)
    : assets_(std::move(assets)),
      options_(config.options),
      highScores_(config.highScores[static_cast<std::size_t>(table)]),
      rng_(seed),
      show_(false) {
  sequencer_ = std::make_shared<TableSequencer>(jingle(JingleBind::Attract).position, assets_.positionJingleStart,
                                                jingle(JingleBind::Silence).position, options_.noMusic);
  player_ = std::make_unique<Player>(Mod::load(module), sequencer_);

  scroll_.speed = rawScrollSpeed(options_.scrollSpeed);
  scroll_.setResolution(options_.resolution, std::nullopt);
  scroll_.pos = static_cast<u16>(576 - scroll_.windowHeight);
  scroll_.rawPosF4 = 0;
  lights_.resize(assets_.lights.size());
  attractCtr_.assign(assets_.attractLights.size(), 0);
  for (const Flipper& f : assets_.flippers) {
    FlipperState s;
    s.accelPress = speedFix(f.accelPress);
    s.accelRelease = speedFix(f.accelRelease);
    s.speedPressStart = speedFix(f.speedPressStart);
    flippers_.push_back(s);
  }
  physmaps_ = assets_.physmaps;
  for (std::size_t i = 0; i < 8; ++i) {
    const auto& m = kMaterials[i];
    materials_[i] = {m[0], m[1], m[2], speedFix(m[3]), m[4]};
  }
  kickerSpeedThreshold_ = speedFix(300);
  kickerSpeedBoost_ = speedFix(2000);
  bumperSpeedBoost_ = speedFix(7000);
  matchTiming_ = hifps_ ? kMatchTimingHigh : kMatchTimingLow;
  push_.speedAttack = speedFix(600);
  push_.speedRelease = speedFix(-200);
  ball_.maxSpeed = speedFix(4100);
  totalBalls_ = options_.balls;

  ball_.setPos({280, 525});
  startScript(ScriptBind::Init);
  flippersPhysmapUpdate();
}

Table::~Table() = default;

int Table::screenHeight() const {
  switch (options_.resolution) {
    case Resolution::Normal: return 240;
    case Resolution::High: return 350;
    case Resolution::Full: return 576 + 33;
  }
  return 240;
}

// ---- pause and options (table.rs) ---------------------------------------------------------

void Table::pause() {
  dm_.saved = dm_.pixels;
  dm_.clear();
  dm_.state = true;
  dmPuts(DmFont::H13, {36, 1}, "GAME PAUSED");
  kbdState_ = KbdState::Paused;
  pauseCycle_ = 0;
  player_->pause();
}

void Table::unpause() {
  dm_.pixels = dm_.saved;
  kbdState_ = KbdState::Main;
  player_->unpause();
}

void Table::toggleMusic() {
  if (options_.noMusic) {
    options_.noMusic = false;
    sequencer_->setMusic(jingle(inPlunger_ ? JingleBind::Plunger : JingleBind::Main).position);
    sequencer_->forceEndLoop();
  } else {
    options_.noMusic = true;
    playJingleBindForce(JingleBind::Silence);
  }
  sequencer_->setNoMusic(options_.noMusic);
}

void Table::pauseOptionAngle() {
  options_.angle = nextAngle(options_.angle);
  dm_.clear();
  switch (options_.angle) {
    case Angle::Low: dmPuts(DmFont::H13, {44, 1}, "ANGLE LOW"); break;
    case Angle::High: dmPuts(DmFont::H13, {40, 1}, "ANGLE HIGH"); break;
    case Angle::Higher: dmPuts(DmFont::H13, {32, 1}, "ANGLE HIGHER"); break;
  }
  pauseCycle_ = 0;
  optionChanged_ = true;
}

void Table::pauseOptionScrolling() {
  options_.scrollSpeed = options_.scrollSpeed == ScrollSpeed::Hard     ? ScrollSpeed::Medium
                         : options_.scrollSpeed == ScrollSpeed::Medium ? ScrollSpeed::Soft
                                                                       : ScrollSpeed::Hard;
  scroll_.speed = rawScrollSpeed(options_.scrollSpeed);
  dm_.clear();
  switch (options_.scrollSpeed) {
    case ScrollSpeed::Hard: dmPuts(DmFont::H13, {24, 1}, "SCROLLING HARD"); break;
    case ScrollSpeed::Medium: dmPuts(DmFont::H13, {16, 1}, "SCROLLING MEDIUM"); break;
    case ScrollSpeed::Soft: dmPuts(DmFont::H13, {24, 1}, "SCROLLING SOFT"); break;
  }
  pauseCycle_ = 0;
  optionChanged_ = true;
}

void Table::pauseOptionMusic() {
  toggleMusic();
  dm_.clear();
  if (options_.noMusic)
    dmPuts(DmFont::H13, {44, 1}, "MUSIC OFF");
  else
    dmPuts(DmFont::H13, {48, 1}, "MUSIC ON");
  pauseCycle_ = 0;
  optionChanged_ = true;
}

void Table::pauseOptionResolution() {
  options_.resolution = options_.resolution == Resolution::Normal ? Resolution::High
                        : options_.resolution == Resolution::High ? Resolution::Full
                                                                  : Resolution::Normal;
  scroll_.setResolution(options_.resolution, inAttract_ ? std::nullopt : std::optional<i16>(ball_.pos()[1]));
  dm_.clear();
  dmPuts(DmFont::H13, {8, 1}, "RESOLUTION CHANGED");
  pauseCycle_ = 0;
  optionChanged_ = true;
}

void Table::pauseConfirmQuit() {
  dm_.clear();
  dmPuts(DmFont::H13, {0, 1}, "REALLY QUIT (Y OR N)");
  kbdState_ = KbdState::PausedConfirmQuit;
}

/// Marks every pixel a shape encloses, by flooding what is outside it instead.
static void fillEnclosed(u8* mask, int w, int h) {
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

/// Which lamp each playfield pixel belongs to, for the replacement pictures.
///
/// A lamp is a few palette colours, so its pixels are the ones drawn in them. Where the
/// artwork dithers a lamp against what is behind it (the criss-cross rail on Stones n Bones
/// lays a checkerboard over the green lamps), the lamp only owns every other pixel, which a
/// high-resolution picture would show as a coarse checkerboard of lit and unlit. So a gap
/// with three or four neighbours of one lamp is taken into it: that closes a dither without
/// spilling past an edge, where a pixel has one or two such neighbours.
void Table::buildLampAreas() const {
  const int w = assets_.mainBoard.width(), h = assets_.mainBoard.height();
  lampAreas_.assign(static_cast<std::size_t>(w) * h, 0);
  std::array<u8, 256> ofColor{};
  for (std::size_t l = 0; l < assets_.lights.size() && l < 255; ++l)
    for (std::size_t i = 0; i < assets_.lights[l].colors.size(); ++i)
      ofColor[assets_.lights[l].baseIndex + i] = static_cast<u8>(l + 1);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) lampAreas_[static_cast<std::size_t>(y) * w + x] = ofColor[assets_.mainBoard(x, y)];

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
  // A lamp's ring is often a pixel short here and there, which would let the outside leak into
  // the middle of it, so what is outside is found against the lamps grown by a pixel all round.
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

  // Inside a lamp: not the lamp itself, not outside it, and not the pixel of slack that
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

/// Takes each flipper out of its artwork, so it can be drawn turned to any angle.
///
/// The original draws a flipper by stamping one of twenty-one pictures of its rectangle, each
/// holding the flipper and the playfield behind it. Which pixels are the flipper is not a
/// question the pictures can answer on their own -- the hub looks the same in all of them --
/// but the table already knows: the collision map it stamps for each step marks the flipper's
/// own material. That shape, widened to take in the outline drawn around it, is the flipper;
/// the playfield behind it is what the other pictures show in its place, and the angle of a
/// step is the direction of the far end of its shape from the hinge.
void Table::buildFlipperArt() const {
  constexpr u8 kFlipperMaterial = 2;
  flipperArt_.clear();
  for (const Flipper& fl : assets_.flippers) {
    const int w = fl.gfx[0].width(), h = fl.gfx[0].height();
    const std::size_t steps = fl.gfx.size(), pixels = static_cast<std::size_t>(w) * h;

    std::vector<u8> coveredAt(steps * pixels, 0);
    for (std::size_t q = 0; q < steps && q < fl.physmap.size(); ++q)
      for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
          if ((fl.physmap[q](x, y) & 7) == kFlipperMaterial)
            coveredAt[q * pixels + static_cast<std::size_t>(y) * w + x] = 1;

    FlipperArt art;
    art.background = Grid8(w, h);
    art.covered.assign(pixels, 0);
    art.rest.assign(pixels, 0);
    std::vector<u8> known(pixels, 0);
    for (std::size_t p = 0; p < pixels; ++p) {
      std::array<u8, 256> count{};
      u8 commonest = 0;
      for (std::size_t q = 0; q < steps; ++q) {
        if (coveredAt[q * pixels + p]) {
          art.covered[p] = 1;
          continue;
        }
        const u8 here = fl.gfx[q].raw()[p];
        known[p] = 1;
        if (++count[here] > count[commonest]) commonest = here;
      }
      art.background(static_cast<int>(p % w), static_cast<int>(p / w)) = commonest;
    }
    // The playfield is never seen under the hub, so spread the nearest of it inwards.
    for (int pass = 0; pass < std::max(w, h); ++pass) {
      bool spread = false;
      for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
          const std::size_t p = static_cast<std::size_t>(y) * w + x;
          if (known[p]) continue;
          const std::array<int, 4> around = {x > 0 ? static_cast<int>(p) - 1 : -1,
                                             x + 1 < w ? static_cast<int>(p) + 1 : -1,
                                             y > 0 ? static_cast<int>(p) - w : -1,
                                             y + 1 < h ? static_cast<int>(p) + w : -1};
          for (const int n : around)
            if (n >= 0 && known[static_cast<std::size_t>(n)] == 1) {
              art.background(x, y) = art.background.raw()[static_cast<std::size_t>(n)];
              known[p] = 2;  // filled in this pass; usable in the next one
              spread = true;
              break;
            }
        }
      for (u8& k : known)
        if (k == 2) k = 1;
      if (!spread) break;
    }

    // The artwork draws an outline around the flipper that the collision map does not have.
    for (std::size_t q = 0; q < steps; ++q) {
      u8* covered = &coveredAt[q * pixels];
      std::vector<u8> grown(covered, covered + pixels);
      for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
          const std::size_t p = static_cast<std::size_t>(y) * w + x;
          if (covered[p] || fl.gfx[q](x, y) == art.background(x, y)) continue;
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

    // What the flipper turns about. The table's own origin for it is where the ball is made
    // to bounce, which for the upper bats is a few pixels from where their artwork hinges, so
    // the artwork's own axis is taken from the pictures: two steps of one rigid shape, the turn
    // between them is the difference of their principal axes, and the axis is its fixed point.
    auto moments = [&](const u8* mask) {
      double cx = 0, cy = 0;
      int n = 0;
      for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
          if (mask[static_cast<std::size_t>(y) * w + x]) { cx += x; cy += y; ++n; }
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
    double ox = fl.originX - fl.rectX, oy = fl.originY - fl.rectY;
    if (steps > 1) {
      const auto m0 = moments(&coveredAt[0]), m1 = moments(&coveredAt[(steps - 1) * pixels]);
      double turn = m1[2] - m0[2];
      while (turn > 1.57079633) turn -= 3.14159265;
      while (turn < -1.57079633) turn += 3.14159265;
      // A shape that hardly turns says little about where its axis is; then the origin stands.
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
    for (std::size_t q = 0; q < steps; ++q) {
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
    flipperArt_.push_back(std::move(art));
  }
}

/// Where a flipper is now, in radians from its resting position. The table moves a flipper
/// smoothly and only quantises it to pick a picture, so the angle follows the steps either side.
float Table::flipperAngle(std::size_t f) const {
  const FlipperArt& art = flipperArt_[f];
  const int pos = std::max<int>(flippers_[f].pos, 0);
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

Table::SpritePicture Table::ballPicture() const {
  SpritePicture picture{assets_.ball.width(), assets_.ball.height(), {}};
  picture.rgba.resize(static_cast<std::size_t>(picture.width) * picture.height * 4);
  for (std::size_t p = 0; p < assets_.ball.raw().size(); ++p) {
    const u8 index = assets_.ball.raw()[p];
    const Rgb c = assets_.palette[index];
    picture.rgba[p * 4] = c.r;
    picture.rgba[p * 4 + 1] = c.g;
    picture.rgba[p * 4 + 2] = c.b;
    picture.rgba[p * 4 + 3] = index ? 255 : 0;  // index 0 is what the original leaves out
  }
  return picture;
}

std::vector<Table::SpritePicture> Table::flipperPictures() const {
  if (flipperArt_.empty()) buildFlipperArt();
  std::vector<SpritePicture> out;
  for (std::size_t f = 0; f < assets_.flippers.size(); ++f) {
    const Flipper& fl = assets_.flippers[f];
    const FlipperArt& art = flipperArt_[f];
    SpritePicture picture{fl.gfx[0].width(), fl.gfx[0].height(), {}};
    picture.rgba.resize(static_cast<std::size_t>(picture.width) * picture.height * 4);
    for (std::size_t p = 0; p < art.rest.size(); ++p) {
      const Rgb c = assets_.palette[fl.gfx[0].raw()[p]];
      picture.rgba[p * 4] = c.r;
      picture.rgba[p * 4 + 1] = c.g;
      picture.rgba[p * 4 + 2] = c.b;
      picture.rgba[p * 4 + 3] = art.rest[p] ? 255 : 0;
    }
    out.push_back(std::move(picture));
  }
  return out;
}

std::vector<FlipperSide> Table::flipperSides() const {
  std::vector<FlipperSide> out;
  for (const Flipper& fl : assets_.flippers) out.push_back(fl.side);
  return out;
}

// ---- the frame ---------------------------------------------------------------------------

TableAction Table::runFrame() {
  using K = TableAction::Kind;
  if (kbdState_ == KbdState::Paused) {
    if (scrollKey_) {
      const int top = 576 - scroll_.windowHeight;
      const int p = std::clamp(static_cast<int>(scroll_.pos) + scrollKey_ * 4, 0, std::max(top, 0));
      scroll_.pos = static_cast<u16>(p);
      scroll_.rawPosF4 = static_cast<i16>(p << 4);
    }
    ++pauseCycle_;
    if (pauseCycle_ == 120) {
      dm_.clear();
      dmPuts(DmFont::H13, {32, 1}, "P TO UNPAUSE");
    } else if (pauseCycle_ == 240) {
      dm_.clear();
      dmPuts(DmFont::H13, {16, 1}, "ASMR FOR OPTIONS");
    } else if (pauseCycle_ == 360) {
      dm_.clear();
      dmPuts(DmFont::H13, {36, 1}, "GAME PAUSED");
      pauseCycle_ = 0;
    }
    if (optionChanged_) {
      optionChanged_ = false;
      return {K::SaveOptions};
    }
    return {};
  }
  if (kbdState_ == KbdState::PausedConfirmQuit) return {};
  if (quitting_) {
    if (fade_ != 0) fade_ = static_cast<u16>(fade_ - 2);
    player_->setMasterVolume(fade_);
    return fade_ == 0 ? TableAction{K::Quit} : TableAction{};
  }

  if (inAttract_) {
    scroll_.attractFrame();
    lightsAttractFrame();
    dm_.blinkFrame();
    if (startKey_) {
      const u8 players = *startKey_;
      startKey_.reset();
      totalPlayers_ = players;
      players_.assign(players, PlayerState{});
      startScript(ScriptBind::GameStart);
      playSfxBind(SfxBind::GameStart);
      inAttract_ = false;
      initGame();
      const Jingle& start = jingle(JingleBind::GameStart);
      const Jingle& plunger = jingle(options_.noMusic ? JingleBind::Silence : JingleBind::Plunger);
      sequencer_->playJingle(start, true, plunger.position);
      issueBall();
      addTask(TaskKind::SetStartKeysActive);
    }
  } else {
    scroll_.update(ball_.pos()[1]);
    if (startKey_) {
      const u8 players = *startKey_;
      startKey_.reset();
      totalPlayers_ = players;
      players_.assign(players, PlayerState{});
      startScript(ScriptBind::GameStartPlayers);
      playSfxBind(SfxBind::GameStart);
      addTask(TaskKind::SetStartKeysActive);
    }
    if (!cheatSlowdown_) physicsFrame();
    physicsFrame();
    physicsFrame();
    physicsFrame();
    if (tiltCounter_ != 0) --tiltCounter_;
    scoreBumper();
    ballGravity();
    checkTransitions();
    if (drained_ && !inDrain_) {
      ballTeleportFreeze(Layer::Ground, {280, 525});
      flippersEnabled_ = false;
      inMode_ = inModeHit_ = inModeRamp_ = false;
      if (!blockDrain_) {
        inDrain_ = true;
        switch (assets_.table) {
          case 0: partyDrained(); break;
          case 1: speedDrained(); break;
          case 2: showDrained(); break;
          default: stonesDrained(); break;
        }
      }
    }
    switch (assets_.table) {
      case 0: partyFrame(); break;
      case 1: speedFrame(); break;
      case 2: showFrame(); break;
      default: stonesFrame(); break;
    }
    doRollTriggers();
    doHitTriggers();
    if (flipperPressed_) {
      flipperPressed_ = false;
      switch (assets_.table) {
        case 0: partyFlipperPressed(); break;
        case 1: speedFlipperPressed(); break;
        case 2: showFlipperPressed(); break;
        default: stonesFlipperPressed(); break;
      }
    }
    if (spacePressed_) {
      spacePressed_ = false;
      if (!cheatNoTilt_ && !inPlunger_ && !drained_ && !tilted_) {
        tiltCounter_ = static_cast<u16>(tiltCounter_ + 60);
        if (tiltCounter_ > 120) {
          tilted_ = true;
          flippersEnabled_ = false;
          playJingleBindSilence(JingleBind::Tilt);
          startScript(ScriptBind::Tilt);
          lightsTilt();
          party_.secretDropRelease = true;
        } else if (tiltCounter_ > 60) {
          playJingleBind(JingleBind::WarnTilt);
        }
      }
    }
    dm_.blinkFrame();
    tasksFrame();
    lightsBlinkFrame();
    if (springReleased_ && springPos_ != 0) {
      springRelease();
      springReleased_ = false;
    } else if (springDownState_ && springPos_ < 0x20) {
      ++springPos_;
    }
  }
  scriptFrame();
  if (flushHighScores_) {
    flushHighScores_ = false;
    return {K::SaveHighScores};
  }
  if (optionChanged_) {
    optionChanged_ = false;
    return {K::SaveOptions};
  }
  return {};
}

void Table::handleKey(Key key, bool pressed) {
  auto flipperKey = [&](FlipperSide side) {
    const std::size_t s = static_cast<std::size_t>(side);
    if (pressed && flippersEnabled_ && !flipperState_[s]) {
      flipperPressed_ = true;
      playSfxBind(SfxBind::FlipperPress);
    }
    flipperState_[s] = pressed;
  };
  if (key == Key::ShiftLeft || key == Key::ControlLeft || key == Key::AltLeft) flipperKey(FlipperSide::Left);
  if (key == Key::ShiftRight || key == Key::ControlRight || key == Key::AltRight) flipperKey(FlipperSide::Right);
  if (key == Key::Space) {
    if (pressed && !spaceState_) spacePressed_ = true;
    spaceState_ = pressed;
  }
  const bool paused = kbdState_ == KbdState::Paused || kbdState_ == KbdState::PausedConfirmQuit;
  if (key == Key::ArrowDown && !paused) {
    springDownState_ = pressed;
    if (!pressed) springReleased_ = true;
  }
  // Debugging: while paused, the arrows scroll the table by hand.
  if (key == Key::ArrowUp || key == Key::ArrowDown) {
    const int dir = key == Key::ArrowUp ? -1 : 1;
    if (pressed && paused)
      scrollKey_ = dir;
    else if (scrollKey_ == dir)
      scrollKey_ = 0;
  }
  if (!pressed) return;
  const u8 chr = keyChar(key);

  switch (kbdState_) {
    case KbdState::Main:
      if (startKeysActive_ && (inAttract_ || atSpring_)) {
        if (key >= Key::F1 && key <= Key::F8)
          startKey_ = static_cast<u8>(static_cast<int>(key) - static_cast<int>(Key::F1) + 1);
        else if (key >= Key::Digit1 && key <= Key::Digit8)
          startKey_ = static_cast<u8>(static_cast<int>(key) - static_cast<int>(Key::Digit1) + 1);
        else if (key == Key::Enter) {
          if (inAttract_)
            startKey_ = 1;
          else if (totalPlayers_ < 8)
            startKey_ = static_cast<u8>(totalPlayers_ + 1);
        }
        if (startKey_) startKeysActive_ = false;
      }
      if (inAttract_) {
        if (chr) handleCheat(chr);
        if (key == Key::Escape) {
          kbdState_ = KbdState::ConfirmQuit;
          startScript(ScriptBind::ConfirmQuit);
        }
      } else if (!inDrain_) {
        if (key == Key::Escape && atSpring_) {
          abortGame();
        } else if (key == Key::M) {
          toggleMusic();
          optionChanged_ = true;
        } else if (key == Key::P) {
          pause();
        }
      }
      break;
    case KbdState::ConfirmQuit:
      if (key == Key::Y) {
        quitting_ = true;
        kbdState_ = KbdState::Main;
      } else if (key == Key::N) {
        kbdState_ = KbdState::Main;
      }
      break;
    case KbdState::Paused:
      switch (key) {
        case Key::M: pauseOptionMusic(); break;
        case Key::R: pauseOptionResolution(); break;
        case Key::S: pauseOptionScrolling(); break;
        case Key::A: pauseOptionAngle(); break;
        case Key::P: unpause(); break;
        // Debugging: every lamp on, then every lamp off, then as the game has them.
        case Key::F7:
          lampOverride_ = lampOverride_ == LampOverride::None    ? LampOverride::AllOn
                          : lampOverride_ == LampOverride::AllOn ? LampOverride::AllOff
                                                                 : LampOverride::None;
          break;
        case Key::Escape: pauseConfirmQuit(); break;
        default: break;
      }
      break;
    case KbdState::PausedConfirmQuit:
      if (key == Key::Y) {
        dm_.pixels = dm_.saved;
        quitting_ = true;
        kbdState_ = KbdState::Main;
      } else {
        unpause();
      }
      break;
    case KbdState::GetName:
      if (chr && nameBuf_.size() < 3) nameBuf_.push_back(chr);
      break;
  }
}

void Table::render(u8* data, Rgb* pal, HdFrame* hd) const {
  for (std::size_t i = 0; i < 256; ++i) pal[i] = assets_.palette[i];
  auto lampLit = [&](std::size_t l) {
    return lampOverride_ == LampOverride::None ? lights_[l].lit : lampOverride_ == LampOverride::AllOn;
  };
  for (std::size_t l = 0; l < assets_.lights.size(); ++l) {
    const Light& light = assets_.lights[l];
    for (std::size_t i = 0; i < light.colors.size(); ++i) {
      const Rgb c = light.colors[i];
      pal[light.baseIndex + i] = lampLit(l) ? c : Rgb{static_cast<u8>(c.r / 2), static_cast<u8>(c.g / 2), static_cast<u8>(c.b / 2)};
    }
  }
  pal[assets_.dmPalette.indexOn] = dm_.state ? assets_.dmPalette.colorOn : assets_.dmPalette.colorOff;
  const int height = screenHeight() == 576 + 33 ? 576 : screenHeight() - 33;
  const int springPos = springPos_ / 2;

  // Replacement pictures: every playfield pixel takes the lit picture where it belongs to a
  // lamp that is on, the unlit one elsewhere; whatever is drawn over it keeps the original.
  // Only in colour: the mono mode greys the palette, which the pictures do not follow.
  if (hd) hd->reset(320, screenHeight());
  if (options_.mono) hd = nullptr;
  const auto hdOn = static_cast<u16>(static_cast<int>(HdPicture::Playfield1On) + assets_.table);
  const auto hdOff = static_cast<u16>(static_cast<int>(HdPicture::Playfield1Off) + assets_.table);
  if (hd) {
    if (lampAreas_.empty()) buildLampAreas();
    if (flipperArt_.empty()) buildFlipperArt();
    hd->spriteTint = static_cast<float>(fade_) / 256.0f;
    for (const u16 p : {hdOn, hdOff}) {
      hd->size[p] = {320, 576};
      hd->used |= 1u << p;
    }
    hd->fade.fill(static_cast<float>(fade_) / 256.0f);
  }
  const auto [bx, by0] = ball_.pos();
  const int by = ball_.frozen ? by0 : by0 + push_.offset();
  for (int y = 0; y < height; ++y) {
    const int sy = y + scroll_.pos + push_.offset();
    u8* row = data + static_cast<std::size_t>(y) * 320;
    if (sy >= 576)
      std::fill(row, row + 320, 0);
    else
      for (int x = 0; x < 320; ++x) {
        row[x] = assets_.mainBoard(x, sy);
        if (hd) {
          // The unlit picture, with how lit this spot is alongside: the renderer blends the lit
          // picture in by it. What a lamp covers on screen is decided by the pictures, which
          // differ only where the lamp is, so this only has to say how lit the place is -- and
          // it is taken over the pixels around, because the original's own edge is a staircase
          // and a sharp reading of it would show through as a jagged rim.
          int around = 0;
          for (int ny = std::max(sy - 1, 0); ny <= std::min(sy + 1, 575); ++ny)
            for (int nx = std::max(x - 1, 0); nx <= std::min(x + 1, 319); ++nx) {
              const u8 near = lampAreas_[static_cast<std::size_t>(ny) * 320 + nx];
              around += near && lampLit(near - 1);
            }
          const u16 lit = static_cast<u16>(around * HdFrame::kLitMax / 9);
          hd->map[static_cast<std::size_t>(y) * 320 + x] = {
              static_cast<u16>(x * 8), static_cast<u16>(sy * 8),
              static_cast<u16>(hdOff | (lit << HdFrame::kLitShift)), 0};
        }
      }
    if (sy >= 556 && sy < 556 + 17) {
      const int springY = sy - 553;
      if (springY >= springPos) {
        for (int sx = 0; sx < 10; ++sx) row[sx + 304] = assets_.spring(sx, springY - springPos);
        // The plunger has a picture of its own: every pixel of it drawn here says which of the
        // original's it is, so the replacement slides down with it as it is pulled.
        if (hd) {
          const auto p = static_cast<u16>(HdPicture::Plunger);
          hd->size[p] = {static_cast<u16>(assets_.spring.width()), static_cast<u16>(assets_.spring.height())};
          hd->used |= 1u << p;
          for (int sx = 0; sx < 10; ++sx)
            hd->map[static_cast<std::size_t>(y) * 320 + sx + 304] = {
                static_cast<u16>(sx * 8), static_cast<u16>((springY - springPos) * 8), p, 0};
        }
      }
    }
    for (std::size_t f = 0; f < assets_.flippers.size(); ++f) {
      const Flipper& fl = assets_.flippers[f];
      const Grid8& gfx = fl.gfx[flippers_[f].quantum];
      if (sy >= fl.rectY && sy - fl.rectY < gfx.height()) {
        const int fy = sy - fl.rectY;
        if (hd) {
          // The renderer draws the flipper turned to its angle, so the frame keeps the
          // playfield behind it, and the replacement picture steps aside where it may reach.
          const FlipperArt& art = flipperArt_[f];
          for (int fx = 0; fx < gfx.width(); ++fx) {
            row[fx + fl.rectX] = art.background(fx, fy);
            // Without a flipper picture of its own the replacement playfield still has the
            // flipper painted into it, so the artwork behind it has to show instead.
            if (!(hd->ownSprites & (1u << f)) && art.covered[static_cast<std::size_t>(fy) * gfx.width() + fx])
              hd->map[static_cast<std::size_t>(y) * 320 + fx + fl.rectX].picture = 0;
          }
        } else {
          for (int fx = 0; fx < gfx.width(); ++fx) row[fx + fl.rectX] = gfx(fx, fy);
        }
      }
    }
    // With replacement pictures the renderer draws the ball, and the cover over it is recorded
    // afterwards, for the trail as well as for the ball.
    if (!inAttract_ && !hd && sy >= by && sy < by + 15) {
      const int ballY = sy - by;
      for (int ballX = 0; ballX < 15; ++ballX) {
        const u8 pix = assets_.ball(ballX, ballY);
        const int x = ballX + bx;
        if (x < 0 || x >= 320) continue;
        const Grid8& occmap = assets_.occmaps[static_cast<std::size_t>(ball_.layer)];
        if (pix == 0 || (sy < 576 && occmap(x, sy) != 0)) continue;
        row[x] = pix;
      }
    }
  }
  if (hd)
    for (std::size_t f = 0; f < assets_.flippers.size() && f < HdSprite::kBall; ++f) {
      const Flipper& fl = assets_.flippers[f];
      const FlipperArt& art = flipperArt_[f];
      const float rectW = static_cast<float>(fl.gfx[0].width()), rectH = static_cast<float>(fl.gfx[0].height());
      hd->sprites.push_back({static_cast<u16>(f), static_cast<float>(fl.rectX) + art.axisX,
                             static_cast<float>(fl.rectY - scroll_.pos - push_.offset()) + art.axisY,
                             art.axisX / rectW, art.axisY / rectH, 1.0f / rectW, 1.0f / rectH,
                             flipperAngle(f), 0.0f, static_cast<float>(height), 0});
    }
  // The ball goes last, so it passes in front of the flippers, and a faint trail of where it
  // has just been goes immediately before it. The trail follows the physics steps rather than
  // the frames, which is what makes it flow rather than step.
  if (hd && !inAttract_) {
    const float top = static_cast<float>(scroll_.pos + push_.offset());
    // What the artwork covers of the ball, as the share of the pixels around each one: a
    // dithered cover then comes out as transparency rather than a chequerboard of holes. It is
    // recorded wherever the ball is drawn, the trail included, so that a ghost passing under a
    // ramp goes behind it exactly as the ball does.
    const Grid8& occmap = assets_.occmaps[static_cast<std::size_t>(ball_.layer)];
    // A pixel's cover is read smoothly, from the four around it, so it has to be recorded a
    // little wider than the ball itself: without that the outermost ring of the ball reads
    // half its cover from pixels nothing was written to, and a thin rim of it shows through
    // whatever is in front.
    constexpr int kMargin = 3;
    auto cover = [&](float fx, float fy) {
      const int px = static_cast<int>(std::lround(fx)), py = static_cast<int>(std::lround(fy));
      for (int oy = -kMargin; oy < 15 + kMargin; ++oy) {
        const int sy = py + oy, y = sy - static_cast<int>(top);
        if (y < 0 || y >= height || sy < 0 || sy >= 576) continue;
        for (int ox = -kMargin; ox < 15 + kMargin; ++ox) {
          const int x = px + ox;
          if (x < 0 || x >= 320) continue;
          // Artwork that covers this spot and the ones around it is solid, and hides the ball
          // outright, as the original does. Only where the cover is dithered against what is
          // behind it -- the criss-cross rail on Stones n Bones -- does the share of the
          // pixels around decide how much shows through. Averaging everywhere would leave a
          // rim of the ball visible wherever a solid cover ends, since a pixel within two of
          // that edge averages below its own value.
          int near = 0;
          if (x > 0) near += occmap(x - 1, sy) != 0;
          if (x < 319) near += occmap(x + 1, sy) != 0;
          if (sy > 0) near += occmap(x, sy - 1) != 0;
          if (sy < 575) near += occmap(x, sy + 1) != 0;
          int cover = 255;
          if (!occmap(x, sy) || near < 3) {
            int covered = 0, counted = 0;
            for (int ny = sy - 2; ny <= sy + 2; ++ny)
              for (int nx = x - 2; nx <= x + 2; ++nx) {
                if (nx < 0 || nx >= 320 || ny < 0 || ny >= 576) continue;
                covered += occmap(nx, ny) != 0;
                ++counted;
              }
            cover = covered * 255 / counted;
          }
          HdPixel& m = hd->map[static_cast<std::size_t>(y) * 320 + x];
          m.flags = static_cast<u16>((m.flags & 0xff) | (cover << HdPixel::kCoverShift));
          if (occmap(x, sy)) m.flags |= HdPixel::kHidesBall;
        }
      }
    };
    auto ball = [&](float x, float y, float opacity) {
      hd->sprites.push_back({HdSprite::kBall, x + 7.5f, y - top + 7.5f, 0.5f, 0.5f, 1.0f / 15.0f, 1.0f / 15.0f, 0.0f,
                             0.0f, static_cast<float>(height), HdPixel::kHidesBall, opacity});
    };
    for (std::size_t i = 0; hd->ballTrail && i < trailLength_; ++i) {
      const auto& p = ballTrail_[(trailNext_ + kTrail - trailLength_ + i) % kTrail];
      // Strongest just behind the ball, fading away towards the oldest step.
      const float recent = static_cast<float>(i + 1) / static_cast<float>(trailLength_);
      const float away = std::hypot(p[0] - static_cast<float>(bx), p[1] - static_cast<float>(by));
      // Nothing where the ball has hardly moved, so that a ball at rest keeps to itself, and
      // the faster it goes the more of a trail it leaves.
      if (away < 0.4f) continue;
      cover(p[0], p[1]);
      ball(p[0], p[1], 0.22f * recent * recent * std::min(away / 2.5f, 1.0f));
    }
    cover(static_cast<float>(bx), static_cast<float>(by));
    ball(static_cast<float>(bx), static_cast<float>(by), 1.0f);
  }
  const int fullHeight = height + 33;
  for (int y = height; y < fullHeight; ++y) std::fill(data + static_cast<std::size_t>(y) * 320, data + static_cast<std::size_t>(y + 1) * 320, 0);
  for (int y = 0; y < 16; ++y) {
    u8* row = data + static_cast<std::size_t>(2 + 2 * y + height) * 320;
    for (int x = 0; x < 160; ++x)
      row[x * 2] = dm_.pixels[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] ? assets_.dmPalette.indexOn
                                                                                          : assets_.dmPalette.indexOff;
  }
  if (options_.mono)
    for (std::size_t i = 0; i < 256; ++i) {
      const u8 m = static_cast<u8>((pal[i].r + pal[i].g + pal[i].b) / 3);
      pal[i] = {m, m, m};
    }
  if (fade_ != 0x100)
    for (std::size_t i = 0; i < 256; ++i)
      pal[i] = {static_cast<u8>(pal[i].r * fade_ >> 8), static_cast<u8>(pal[i].g * fade_ >> 8),
                static_cast<u8>(pal[i].b * fade_ >> 8)};
}

// ---- scrolling (scroll.rs) ---------------------------------------------------------------

void Table::Scroll::setResolution(Resolution r, std::optional<i16> ballY) {
  switch (r) {
    case Resolution::Normal: windowHeight = 240 - 33; ballTarget = 75; break;
    case Resolution::High: windowHeight = 350 - 33; ballTarget = 130; break;
    case Resolution::Full: windowHeight = 576; ballTarget = 0; break;
  }
  u16 p;
  if (targetSpecial)
    p = *targetSpecial;
  else if (ballY)
    p = *ballY < ballTarget ? 0 : static_cast<u16>(*ballY - ballTarget);
  else
    p = 0;
  pos = std::min<u16>(p, static_cast<u16>(576 - windowHeight));
  rawPosF4 = static_cast<i16>(pos << 4);
}

void Table::Scroll::update(i16 ballY) {
  if (windowHeight == 576) {
    pos = 0;
    return;
  }
  const u16 target = targetSpecial ? *targetSpecial
                     : ballY < ballTarget ? 0
                                          : std::min<u16>(static_cast<u16>(ballY - ballTarget), static_cast<u16>(576 - windowHeight));
  i16 delta = static_cast<i16>(target - (rawPosF4 >> 4));
  rawPosF4 = static_cast<i16>(rawPosF4 + ((delta * speed) >> 2));
  delta = static_cast<i16>(target - (rawPosF4 >> 4));
  if (delta <= -ballTarget)
    rawPosF4 = static_cast<i16>(rawPosF4 + ((delta + ballTarget) << 4));
  else if (delta >= ballTarget + 40)
    rawPosF4 = static_cast<i16>(rawPosF4 + ((delta - ballTarget - 40) << 4));
  pos = static_cast<u16>(rawPosF4 >> 4);
}

void Table::Scroll::attractFrame() {
  if (windowHeight == 576) {
    pos = 0;
    return;
  }
  if (pos == 0)
    attractUp = false;
  else if (pos == 576 - windowHeight)
    attractUp = true;
  pos = static_cast<u16>(attractUp ? pos - 1 : pos + 1);
  rawPosF4 = static_cast<i16>(pos << 4);
}

void Table::Scroll::setSpecialTargetNow(u16 target) {
  targetSpecial = target;
  if (windowHeight != 576) {
    rawPosF4 = static_cast<i16>(target << 4);
    pos = target;
  }
}

void Table::Push::frame(bool state) {
  if (state) {
    speed = speedAttack;
    offsetF9 = static_cast<i16>(offsetF9 + speed);
    if (offsetF9 > 0x800) {
      speed = 0;
      offsetF9 = 0x800;
    }
  } else {
    speed = speedRelease;
    offsetF9 = static_cast<i16>(offsetF9 + speed);
    if (offsetF9 < 0) {
      speed = 0;
      offsetF9 = 0;
    }
  }
}

void Table::DotMatrix::blinkFrame() {
  if (!blink) return;
  if (--blink->first == 0) {
    blink->first = blink->second;
    state = !state;
  }
}

// ---- lights (lights.rs) --------------------------------------------------------------------

void Table::lightsAttractFrame() {
  for (std::size_t i = 0; i < attractCtr_.size(); ++i) {
    u16& ctr = attractCtr_[i];
    ++ctr;
    const AttractLight& a = assets_.attractLights[i];
    if (ctr == a.ctrOff) {
      lights_[a.light].lit = false;
    } else if (ctr == a.ctrOn) {
      lights_[a.light].lit = true;
      ctr = a.ctrReset;
    }
  }
}

void Table::setLightState(u8 light, bool state) { lights_[light] = LightState{state, state}; }

void Table::lightsReset() {
  for (std::size_t i = 0; i < lights_.size(); ++i) setLightState(static_cast<u8>(i), false);
}

void Table::lightsTilt() {
  for (LightState& l : lights_) {
    l.lit = false;
    l.blinking = false;
  }
}

void Table::lightsBlinkFrame() {
  for (LightState& l : lights_) {
    if (!l.blinking) continue;
    if (l.ctr == 0 || l.ctr == l.ctrReset) {
      l.lit = true;
      l.ctr = 0;
    } else if (l.ctr == l.ctrOff) {
      l.lit = false;
    }
    ++l.ctr;
  }
}

void Table::lightBlink(LightBind bind, u8 idx, u8 halfPeriod, u8 phase) {
  LightState& l = lights_[assets_.lightsOf(bind)[idx]];
  l.blinking = true;
  l.ctr = phase;
  l.ctrOff = halfPeriod;
  l.ctrReset = static_cast<u8>(halfPeriod * 2);
}

void Table::lightSet(LightBind bind, u8 idx, bool state) { setLightState(assets_.lightsOf(bind)[idx], state); }

void Table::lightSetAll(LightBind bind, bool state) {
  for (u8 l : assets_.lightsOf(bind)) setLightState(l, state);
}

bool Table::lightState(LightBind bind, u8 idx) const { return lights_[assets_.lightsOf(bind)[idx]].state; }

bool Table::lightAllLit(LightBind bind) const {
  for (u8 l : assets_.lightsOf(bind))
    if (!lights_[l].state) return false;
  return true;
}

bool Table::lightAllUnlit(LightBind bind) const {
  for (u8 l : assets_.lightsOf(bind))
    if (lights_[l].state) return false;
  return true;
}

void Table::lightRotate(LightBind bind) {
  const auto& ls = assets_.lightsOf(bind);
  std::vector<bool> states;
  for (u8 l : ls) states.push_back(lights_[l].state);
  for (std::size_t i = 0; i < ls.size(); ++i) setLightState(ls[i], states[(i + 1) % ls.size()]);
}

u8 Table::lightSequence(LightBind bind) {
  const auto& ls = assets_.lightsOf(bind);
  for (std::size_t i = 0; i < ls.size(); ++i)
    if (!lights_[ls[i]].state) {
      setLightState(ls[i], true);
      return static_cast<u8>(i);
    }
  return static_cast<u8>(ls.size());
}

// ---- sound (sound.rs) -----------------------------------------------------------------------

void Table::playSfxBind(SfxBind bind, u8 volume) {
  if (const auto& s = assets_.sfx(bind)) player_->playSfx(*s, volume);
}

bool Table::playJingleBind(JingleBind bind) { return sequencer_->playJingle(jingle(bind), false, std::nullopt); }
bool Table::playJingleBindForce(JingleBind bind) { return sequencer_->playJingle(jingle(bind), true, std::nullopt); }
bool Table::playJingleBindSilence(JingleBind bind) {
  return sequencer_->playJingle(jingle(bind), false, jingle(JingleBind::Silence).position);
}
void Table::setMusicSilence() { sequencer_->setMusic(jingle(JingleBind::Silence).position); }
void Table::setMusicPlunger() {
  sequencer_->setMusic(jingle(options_.noMusic ? JingleBind::Silence : JingleBind::Plunger).position);
}
void Table::setMusicMain() {
  sequencer_->setMusic(jingle(options_.noMusic ? JingleBind::Silence : JingleBind::Main).position);
}
void Table::playJinglePlunger() {
  const Jingle& j = jingle(options_.noMusic ? JingleBind::Silence : JingleBind::Plunger);
  sequencer_->playJingle(j, false, j.position);
}

// ---- cheats (cheat.rs) -------------------------------------------------------------------------

void Table::handleCheat(u8 chr) {
  cheatBuf_.push_back(static_cast<char>(chr));
  bool foundPrefix = false;
  for (const Cheat& c : assets_.cheats) {
    if (cheatBuf_ == c.keys) {
      cheatBuf_.clear();
      switch (c.effect) {
        case CheatEffect::None: break;
        case CheatEffect::Tilt: cheatNoTilt_ = true; break;
        case CheatEffect::Slowdown: cheatSlowdown_ = true; break;
        case CheatEffect::Balls: totalBalls_ = 5; break;
        case CheatEffect::Reset:
          cheatNoTilt_ = false;
          cheatSlowdown_ = false;
          totalBalls_ = 3;
          break;
        default: break;
      }
      startScriptRaw(c.script);
      enterAttract_ = true;
      return;
    }
    if (c.keys.starts_with(cheatBuf_)) foundPrefix = true;
  }
  if (!foundPrefix) cheatBuf_ = std::string(1, static_cast<char>(chr));
}

}  // namespace pfr
