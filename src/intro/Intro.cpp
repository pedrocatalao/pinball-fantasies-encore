#include "intro/Intro.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

#include "assets/TableAssets.h"

namespace pfr {

namespace {

constexpr int kW = 640;

void fadePal(Rgb* dst, const std::vector<Rgb>& src, Rgb color, int num, int den) {
  for (std::size_t i = 0; i < src.size() && i < 256; ++i) {
    auto mix = [&](u8 a, u8 b) { return static_cast<u8>((a * num + b * (den - num)) / den); };
    dst[i] = {mix(src[i].r, color.r), mix(src[i].g, color.g), mix(src[i].b, color.b)};
  }
}

void scalePal(Rgb* pal, int from, int to, int num, int den) {
  for (int i = from; i < to; ++i)
    pal[i] = {static_cast<u8>(pal[i].r * num / den), static_cast<u8>(pal[i].g * num / den),
              static_cast<u8>(pal[i].b * num / den)};
}

std::vector<u8> bytes(std::string_view s) { return {s.begin(), s.end()}; }

}  // namespace

Intro::Intro(ByteView prg, ByteView module, const Config& config, int returningFrom)
    : Intro(IntroAssets::load(prg), module, config, returningFrom) {}

Intro::Intro(IntroAssets assets, ByteView module, const Config& config, int returningFrom)
    : assets_(std::move(assets)), config_(config) {
  Mod mod = Mod::load(module);
  const u8 wrap = static_cast<u8>(mod.positions.size());
  player_ = std::make_unique<Player>(std::move(mod), std::make_shared<SimpleSequencer>(wrap));
  if (returningFrom >= 0) {
    state_.kind = StateKind::InitDelay;
    textPage_ = returningFrom < 2 ? 0 : 1;
  }
}

// ---- drawing -------------------------------------------------------------------------------

void Intro::clearLeft(u8* data, int num) const {
  for (int y = 0; y < num; ++y) {
    const int yy = 95 + y;
    for (int x = 8; x < 120; ++x) {
      data[yy * 2 * kW + x] = data[(yy * 2 + 1) * kW + x] = 0x2;
      hdClear(yy * 2 * kW + x);
      hdClear((yy * 2 + 1) * kW + x);
    }
  }
}

void Intro::renderLeftText(u8* data, int baseY, int num, bool isOptions) const {
  const auto& text = isOptions ? assets_.leftTextOptions : assets_.leftTextMenu;
  for (std::size_t ty = 0; ty < text.size(); ++ty)
    for (std::size_t tx = 0; tx < text[ty].size(); ++tx) {
      if (static_cast<int>(ty * 12 + tx) >= num) continue;
      const int y = baseY + 9 * static_cast<int>(ty), x = 16 + 8 * static_cast<int>(tx);
      const u8 chr = text[ty][tx] & 0x7f;
      for (int cy = 0; cy < 8; ++cy) {
        const u8 line = kCgaFont[chr][cy];
        for (int dx = 0; dx < 8; ++dx)
          if (line & (0x80 >> dx)) {
            data[(y + cy) * 2 * kW + x + dx] = data[((y + cy) * 2 + 1) * kW + x + dx] = 0;
            hdClear((y + cy) * 2 * kW + x + dx);
            hdClear(((y + cy) * 2 + 1) * kW + x + dx);
          }
      }
    }
}

void Intro::unclearLeft(u8* data, int num) const {
  for (int y = 90 - num; y < 90; ++y) {
    const int yy = 95 + y;
    for (int x = 8; x < 120; ++x) {
      data[yy * 2 * kW + x] = data[(yy * 2 + 1) * kW + x] = assets_.left.data(x, yy);
      hdMark(yy * 2 * kW + x, HdPicture::Left, x * 8, yy * 8, HdFrame::kHalfY);
      hdMark((yy * 2 + 1) * kW + x, HdPicture::Left, x * 8, yy * 8 + 4, HdFrame::kHalfY);
    }
  }
}

void Intro::renderLeft(u8* data, Rgb* pal, int offset) const {
  for (std::size_t i = 0; i < 16 && i < assets_.left.cmap.size(); ++i) pal[i] = assets_.left.cmap[i];
  const int lw = assets_.left.data.width();
  hdUse(HdPicture::Left, assets_.left);
  if (vertical()) {
    for (int y = 0; y < 960; ++y) {
      const int sy = y <= 373 ? y / 2 : y <= 854 ? 186 : (y - 480) / 2;
      for (int x = offset; x < lw; ++x) {
        data[y * kW + x - offset] = assets_.left.data(x, sy);
        hdMark(y * kW + x - offset, HdPicture::Left, x * 8, sy * 8 + (y & 1) * 4, HdFrame::kHalfY);
      }
    }
    if (offset == 0)
      renderLeftText(data, 187 + 70, 120,
                     state_.kind == StateKind::Options || state_.kind == StateKind::OptionsFadeIn ||
                         state_.kind == StateKind::OptionsFadeOut);
    return;
  }
  for (int y = 0; y < 480; ++y)
    for (int x = offset; x < lw; ++x) {
      data[y * kW + x - offset] = assets_.left.data(x, y / 2);
      hdMark(y * kW + x - offset, HdPicture::Left, x * 8, y * 4, HdFrame::kHalfY);
    }
  switch (left_) {
    case LeftKind::None:
    case LeftKind::Image: break;
    case LeftKind::ImageOut: clearLeft(data, leftN_); break;
    case LeftKind::TextIn:
      clearLeft(data, 90);
      renderLeftText(data, 97, leftN_, leftOptions_);
      break;
    case LeftKind::Text:
      clearLeft(data, 90);
      renderLeftText(data, 97, 120, leftOptions_);
      break;
    case LeftKind::TextOut:
      clearLeft(data, 90);
      renderLeftText(data, 97, 120, leftOptions_);
      unclearLeft(data, leftN_);
      break;
  }
}

void Intro::renderTable(u8* data, Rgb* pal, const std::function<bool(int)>& f, int table, int base, bool flip) const {
  const int palBase = 0x10 * (table + 1);
  const IntroImage& img = assets_.tables[static_cast<std::size_t>(table)];
  for (std::size_t i = 0; i < 16 && i < img.cmap.size(); ++i) pal[static_cast<std::size_t>(palBase) + i] = img.cmap[i];
  const auto picture = static_cast<HdPicture>(static_cast<int>(HdPicture::Table1) + table);
  hdUse(picture, img);
  for (int y = 0; y < 95; ++y) {
    if (!f(flip ? 94 - y : y)) continue;
    for (int x = 0; x < 440; ++x) {
      const int p = (base + y) * 2 * kW + 160 + x;
      data[p] = data[p + kW] = static_cast<u8>(img.data(x, y) | palBase);
      hdMark(p, picture, x * 8, y * 8, HdFrame::kHalfY);
      hdMark(p + kW, picture, x * 8, y * 8 + 4, HdFrame::kHalfY);
    }
  }
}

void Intro::renderTables(u8* data, Rgb* pal, const std::function<bool(int)>& f) const {
  if (vertical()) {
    renderTable(data, pal, f, 0, 12, false);
    renderTable(data, pal, f, 1, 132, false);
    renderTable(data, pal, f, 2, 252, true);
    renderTable(data, pal, f, 3, 372, true);
  } else if (textPage_ % 2 == 0) {
    renderTable(data, pal, f, 0, 10, false);
    renderTable(data, pal, f, 1, 135, true);
  } else {
    renderTable(data, pal, f, 2, 10, false);
    renderTable(data, pal, f, 3, 135, true);
  }
}

void Intro::renderChar(u8* data, const IntroImage& font, u8 chr, int x, int y) const {
  int idx;
  if (chr >= '0' && chr <= '9')
    idx = chr - '0';
  else if (chr >= 'A' && chr <= 'Z')
    idx = chr - 'A' + 10;
  else if (chr == '.')
    idx = 36;
  else if (chr == ':')
    idx = 37;
  else if (chr == '-')
    idx = 38;
  else if (chr == '>')
    idx = 39;
  else
    return;
  const int fx = idx % 20 * 32, fy = idx / 20 * 14;
  for (int cy = 0; cy < 14; ++cy)
    for (int cx = 0; cx < 18; ++cx) {
      if (fx + cx >= font.data.width() || fy + cy >= font.data.height()) continue;
      const int p = (y + cy) * 2 * kW + x + cx;
      data[p] = data[p + kW] = static_cast<u8>(font.data(fx + cx, fy + cy) | 0x10);
      hdClear(p);
      hdClear(p + kW);
    }
}

void Intro::renderLine(u8* data, const IntroImage& font, const std::vector<u8>& line, int y) const {
  const int sx = 164 + (24 - static_cast<int>(line.size())) * 9;
  for (std::size_t tx = 0; tx < line.size(); ++tx) renderChar(data, font, line[tx], sx + static_cast<int>(tx) * 18, y);
}

void Intro::renderHiScores(u8* data, const IntroImage& font, int table, int y) const {
  static constexpr const char* kNames[4] = {" PARTY LAND ", " SPEED DEVILS ", " BILLION DOLLAR ", " STONES N BONES "};
  renderLine(data, font, bytes(kNames[table]), y);
  const HighScores& hs = config_.highScores[static_cast<std::size_t>(table)];
  for (std::size_t i = 0; i < hs.size(); ++i) {
    std::vector<u8> line(24, ' ');
    line[2] = static_cast<u8>('1' + i);
    line[3] = '.';
    std::copy(hs[i].name.begin(), hs[i].name.end(), line.begin() + 5);
    line[9] = '-';
    const auto ascii = hs[i].score.toAscii();
    std::copy(ascii.begin(), ascii.end(), line.begin() + 11);
    renderLine(data, font, line, y + static_cast<int>(i + 1) * 18);
  }
}

void Intro::renderText(u8* data, Rgb* pal, bool lq) const {
  const IntroImage& font = lq ? assets_.fontLq : assets_.fontHq;
  const IntroImage& hiscores = lq ? assets_.hiscoresLq : assets_.hiscoresHq;
  for (std::size_t i = 0; i < 16 && i < font.cmap.size(); ++i) pal[0x10 + i] = font.cmap[i];
  const TextPage& page = assets_.textPages[textPage_];
  if (page.hiScores) {
    hdUse(HdPicture::HiScores, hiscores);
    for (int y = 0; y < hiscores.data.height(); ++y)
      for (int x = 0; x < hiscores.data.width(); ++x) {
        const int p = y * kW * 2 + x + 184;
        data[p] = data[p + kW] = static_cast<u8>(hiscores.data(x, y) | 0x10);
        hdMark(p, HdPicture::HiScores, x * 8, y * 8, HdFrame::kHalfY);
        hdMark(p + kW, HdPicture::HiScores, x * 8, y * 8 + 4, HdFrame::kHalfY);
      }
    if (vertical()) {
      for (int t = 0; t < 4; ++t) renderHiScores(data, font, t, 42 + t * 108);
    } else {
      renderHiScores(data, font, page.tables34 ? 2 : 0, 42);
      renderHiScores(data, font, page.tables34 ? 3 : 1, 150);
    }
  } else {
    const int base = vertical() ? 120 : 0;
    for (std::size_t ty = 0; ty < page.lines.size(); ++ty)
      renderLine(data, font, page.lines[ty], 14 + static_cast<int>(ty) * 18 + base);
  }
}

void Intro::renderOptions(u8* data, Rgb* pal, bool lq, std::optional<u8> cursor) const {
  const int pitch = vertical() ? 36 : 18;
  const IntroImage& font = lq ? assets_.fontLq : assets_.fontHq;
  for (std::size_t i = 0; i < 16 && i < font.cmap.size(); ++i) pal[0x10 + i] = font.cmap[i];
  auto padded = [](std::string_view s) {
    std::vector<u8> v(s.begin(), s.end());
    v.resize(24, ' ');
    return v;
  };
  std::vector<std::vector<u8>> lines = {bytes("OPTIONS MENU"),     {},
                                        padded(" BALLS:"),          padded(" ANGLE:"),
                                        padded(" SCROLLING:"),      padded(" INGAME MUSIC:"),
                                        padded(" RESOLUTION:"),     padded(" COLOR MODE:"),
                                        {},                         bytes(" SAVE AND EXIT ")};
  auto put = [](std::vector<u8>& line, std::string_view s) { std::copy(s.begin(), s.end(), line.begin() + 16); };
  const Options& o = config_.options;
  lines[2][16] = static_cast<u8>('0' + o.balls);
  put(lines[3], o.angle == Angle::Low ? "LOW" : o.angle == Angle::High ? "HIGH" : "HIGHER");
  put(lines[4], o.scrollSpeed == ScrollSpeed::Hard ? "HARD" : o.scrollSpeed == ScrollSpeed::Medium ? "MEDIUM" : "SOFT");
  put(lines[5], o.noMusic ? "OFF" : "ON");
  put(lines[6], o.resolution == Resolution::Normal ? "NORMAL" : o.resolution == Resolution::High ? "HIGH" : "FULL");
  put(lines[7], o.mono ? "MONO" : "COLOR");
  // The option lines end in spaces up to their value; trim trailing blanks so they centre as in the original.
  for (std::size_t i = 2; i <= 7; ++i)
    while (!lines[i].empty() && lines[i].back() == ' ') lines[i].pop_back();
  for (std::size_t i = 2; i <= 7; ++i) lines[i].resize(std::max<std::size_t>(lines[i].size(), 22), ' ');
  for (std::size_t ty = 0; ty < lines.size(); ++ty) renderLine(data, font, lines[ty], 14 + static_cast<int>(ty) * pitch);
  if (cursor) {
    const int pos = *cursor == 6 ? 9 : *cursor + 2;
    renderChar(data, font, '>', 175, 14 + pos * pitch);
  }
}

void Intro::hdUse(HdPicture p, const IntroImage& img) const {
  if (!hd_) return;
  const auto i = static_cast<std::size_t>(p);
  hd_->size[i] = {static_cast<u16>(img.data.width()), static_cast<u16>(img.data.height())};
  hd_->used |= 1u << i;
}

void Intro::hdMark(int pos, HdPicture p, int x8, int y8, u16 flags) const {
  if (hd_) hd_->map[static_cast<std::size_t>(pos)] = {static_cast<u16>(x8), static_cast<u16>(y8),
                                                      static_cast<u16>(static_cast<u16>(p) | flags), 0};
}

void Intro::hdClear(int pos) const {
  if (hd_) hd_->map[static_cast<std::size_t>(pos)].picture = 0;
}

// ---- logic ---------------------------------------------------------------------------------

void Intro::nextPage() {
  if (vertical()) {
    switch (textPage_) {
      case 0: case 1: textPage_ = 2; break;
      case 2: textPage_ = 3; break;
      case 3: textPage_ = 4; break;
      case 4: case 5: textPage_ = 6; break;
      case 6: textPage_ = 7; break;
      default: textPage_ = 0; break;
    }
  } else {
    ++textPage_;
  }
  if (textPage_ >= assets_.textPages.size()) textPage_ = 0;
}

void Intro::handleOption(u8 which) {
  Options& o = config_.options;
  switch (which) {
    case 0: o.balls = o.balls == 3 ? 5 : 3; break;
    case 1: o.angle = nextAngle(o.angle); break;
    case 2:
      o.scrollSpeed = o.scrollSpeed == ScrollSpeed::Hard     ? ScrollSpeed::Medium
                      : o.scrollSpeed == ScrollSpeed::Medium ? ScrollSpeed::Soft
                                                             : ScrollSpeed::Hard;
      break;
    case 3: o.noMusic = !o.noMusic; break;
    case 4:
      o.resolution = o.resolution == Resolution::Normal ? Resolution::High
                     : o.resolution == Resolution::High ? Resolution::Full
                                                        : Resolution::Normal;
      break;
    case 5: o.mono = !o.mono; break;
    default:
      state_ = State{};
      state_.kind = StateKind::OptionsFadeOut;
      break;
  }
}

IntroAction Intro::runFrame() {
  using K = StateKind;
  switch (left_) {
    case LeftKind::None: break;
    case LeftKind::Image:
      if (++leftN_ >= 480) left_ = LeftKind::ImageOut, leftN_ = 0;
      break;
    case LeftKind::ImageOut:
      leftN_ = static_cast<u16>(leftN_ + 3);
      if (leftN_ >= 90) left_ = LeftKind::TextIn, leftN_ = 0, leftOptions_ = leftIsOptions_;
      break;
    case LeftKind::TextIn:
      if (++leftN_ >= 120) left_ = LeftKind::Text, leftN_ = 0;
      break;
    case LeftKind::Text:
      if (++leftN_ >= 480) left_ = LeftKind::TextOut, leftN_ = 0;
      break;
    case LeftKind::TextOut:
      if (++leftN_ >= 90) left_ = LeftKind::Image, leftN_ = 0;
      break;
  }
  State& s = state_;
  auto go = [&](K k) {
    s = State{};
    s.kind = k;
  };
  switch (s.kind) {
    case K::Slide: {
      const Slide& slide = assets_.slides[s.slide];
      switch (s.stage) {
        case SlideStage::Gap:
          if (++s.n >= slide.gapFrames) s.stage = SlideStage::FadeIn, s.n = 0;
          break;
        case SlideStage::FadeIn:
          if (++s.n >= slide.fadeInFrames) s.stage = SlideStage::Show, s.n = 0;
          break;
        case SlideStage::Show:
          if (player_->ticks() >= slide.fadeOutTick || key_ == Press::Space) s.stage = SlideStage::FadeOut, s.n = 0;
          break;
        case SlideStage::FadeOut:
          if (++s.n >= slide.fadeOutFrames) {
            ++s.slide;
            if (s.slide == assets_.slides.size() || key_ == Press::Space) {
              go(K::InitDelay);
              if (key_ == Press::Space) key_ = Press::None;
            } else {
              s.stage = assets_.slides[s.slide].gapFrames != 0 ? SlideStage::Gap : SlideStage::FadeIn;
              s.n = 0;
            }
          }
          break;
      }
      break;
    }
    case K::InitDelay:
      if (++s.n >= 11) {
        go(K::Left);
        s.n = 128;
      }
      break;
    case K::Left:
      if (s.n != 0) {
        s.n = static_cast<u16>(s.n - 8);
      } else {
        go(K::TablesGap);
        left_ = LeftKind::Image;
        leftN_ = 0;
      }
      break;
    case K::TablesGap:
      if (++s.n >= 20) go(K::TablesWarpIn);
      break;
    case K::TablesWarpIn:
      if (++s.n >= assets_.warpFrames) go(K::Tables);
      break;
    case K::TablesFadeOut:
    case K::FadeOut:
      player_->setMasterVolume(0x100u * (80u - s.n) / 80u);
      if (s.n >= 80) return s.action;
      ++s.n;
      break;
    case K::Tables: {
      ++s.n;
      const Press k = key_;
      key_ = Press::None;
      if (k == Press::Table) {
        go(K::TablesFadeOut);
        s.action = {IntroAction::Kind::OpenTable, keyTable_};
      } else if (k == Press::Options) {
        go(K::TablesWarpOut);
        s.next = Next::Options;
      } else if (k == Press::Space) {
        go(K::TablesWarpOut);
        s.next = Next::SkipToText;
      } else if (k == Press::Enter) {
        go(K::TablesWarpOut);
        s.next = Next::SkipToTables;
      } else if (k == Press::Escape) {
        go(K::TablesFadeOut);
        s.action = {IntroAction::Kind::Quit, 0};
      } else if (s.n >= 540) {
        go(K::TablesWarpOut);
        s.next = Next::SkipToText;
      }
      break;
    }
    case K::TablesWarpOut:
      if (++s.n >= assets_.warpFrames) {
        switch (s.next) {
          case Next::SkipToTables:
            nextPage();
            go(K::TablesGap);
            break;
          case Next::SkipToText: go(K::TextGap); break;
          case Next::Options:
            go(K::OptionsGap);
            leftIsOptions_ = true;
            break;
          case Next::Table: break;
        }
      }
      break;
    case K::TextGap:
      if (++s.n >= 5) go(K::TextFadeIn);
      break;
    case K::TextFadeIn:
      if (++s.n >= 20) go(K::Text);
      break;
    case K::Text: {
      ++s.n;
      const Press k = key_;
      key_ = Press::None;
      const int table = keyTable_;
      if (k == Press::Table) {
        go(K::TextFadeOut);
        s.next = Next::Table;
        s.action = {IntroAction::Kind::OpenTable, table};
      } else if (k == Press::Options) {
        go(K::TextFadeOut);
        s.next = Next::Options;
      } else if (k == Press::Enter || k == Press::Space || k == Press::Escape || s.n >= 420) {
        go(K::TextFadeOut);
        s.next = Next::SkipToTables;
      }
      break;
    }
    case K::TextFadeOut:
      if (++s.n >= 20) {
        switch (s.next) {
          case Next::SkipToTables:
            nextPage();
            go(K::TablesGap);
            break;
          case Next::Options:
            nextPage();
            go(K::OptionsGap);
            leftIsOptions_ = true;
            break;
          case Next::Table: {
            const IntroAction a = s.action;
            go(K::FadeOut);
            s.action = a;
            break;
          }
          case Next::SkipToText: break;
        }
      }
      break;
    case K::OptionsGap:
      if (++s.n >= 5) go(K::OptionsFadeIn);
      break;
    case K::OptionsFadeIn:
      if (++s.n >= 40) go(K::Options);
      break;
    case K::Options: {
      const Press k = key_;
      key_ = Press::None;
      if (k == Press::Enter || k == Press::Space)
        handleOption(static_cast<u8>(s.n));
      else if (k == Press::Escape)
        go(K::OptionsFadeOut);
      else if (k == Press::Up)
        s.n = s.n == 0 ? 6 : static_cast<u16>(s.n - 1);
      else if (k == Press::Down)
        s.n = s.n == 6 ? 0 : static_cast<u16>(s.n + 1);
      else if (k == Press::Option)
        handleOption(keyOption_);
      break;
    }
    case K::OptionsFadeOut:
      if (++s.n >= 40) {
        go(K::TablesGap);
        leftIsOptions_ = false;
        return {IntroAction::Kind::SaveOptions, 0};
      }
      break;
  }
  return {};
}

void Intro::handleKey(Key key, bool pressed) {
  if (!pressed) return;
  if ((key >= Key::F1 && key <= Key::F4) || (key >= Key::Digit1 && key <= Key::Digit4)) {
    key_ = Press::Table;
    keyTable_ = key >= Key::F1 && key <= Key::F4 ? static_cast<int>(key) - static_cast<int>(Key::F1)
                                                 : static_cast<int>(key) - static_cast<int>(Key::Digit1);
  } else if (key == Key::F5 || key == Key::Digit5) {
    key_ = Press::Options;
  } else if (key == Key::Escape) {
    key_ = Press::Escape;
  } else if (key == Key::Enter) {
    key_ = Press::Enter;
  } else if (key == Key::Space) {
    key_ = Press::Space;
  } else if (key == Key::ArrowDown) {
    key_ = Press::Down;
  } else if (key == Key::ArrowUp) {
    key_ = Press::Up;
  }
}

void Intro::render(u8* data, Rgb* pal, HdFrame* hd) const {
  using K = StateKind;
  const State& s = state_;
  const int h = height();
  std::fill(data, data + static_cast<std::size_t>(kW) * h, 0);
  std::fill(pal, pal + 256, Rgb{});
  hd_ = hd;
  if (hd) hd->reset(kW, h);
  // The fades below mix every colour towards black (or white) by the same amount; the
  // replacement pictures follow them.
  auto fade = [&](int num, int den, Rgb color = {}) {
    if (hd) hd->fade.fill(static_cast<float>(num) / static_cast<float>(den)), hd->fadeColor = color;
  };
  // The text pages fade only their own colours (0x10 to 0x1f), which the heading uses.
  auto fadeText = [&](int num, int den) {
    if (hd) hd->fade[static_cast<std::size_t>(HdPicture::HiScores)] = static_cast<float>(num) / static_cast<float>(den);
  };
  switch (s.kind) {
    case K::Slide: {
      const int base = vertical() ? 240 : 0;
      const Slide& slide = assets_.slides[s.slide];
      const Grid8& img = slide.image.data;
      const bool doubled = img.width() == 320;
      const auto picture = static_cast<HdPicture>(static_cast<std::size_t>(HdPicture::Slide1) + s.slide);
      const bool replaceable = s.slide < 5;
      if (replaceable) hdUse(picture, slide.image);
      const int step = doubled ? 4 : 8;
      const u16 flags = doubled ? HdFrame::kHalfX | HdFrame::kHalfY : 0;
      for (int y = 0; y < 480; ++y)
        for (int x = 0; x < kW; ++x) {
          const int sx = doubled ? x / 2 : x, sy = doubled ? y / 2 : y;
          if (sx < img.width() && sy < img.height()) {
            data[x + (base + y) * kW] = img(sx, sy);
            if (replaceable) hdMark(x + (base + y) * kW, picture, x * step, y * step, flags);
          }
        }
      const auto& cmap = slide.image.cmap;
      switch (s.stage) {
        case SlideStage::Gap: fade(0, 1); break;
        case SlideStage::FadeIn: {
          const Rgb from = slide.fadeFromWhite ? Rgb{0xff, 0xff, 0xff} : Rgb{};
          fadePal(pal, cmap, from, s.n, slide.fadeInFrames);
          fade(s.n, slide.fadeInFrames, from);
          break;
        }
        case SlideStage::Show: std::copy(cmap.begin(), cmap.begin() + std::min<std::size_t>(cmap.size(), 256), pal); break;
        case SlideStage::FadeOut:
          fadePal(pal, cmap, Rgb{}, slide.fadeOutFrames - s.n, slide.fadeOutFrames);
          fade(slide.fadeOutFrames - s.n, slide.fadeOutFrames);
          break;
      }
      break;
    }
    case K::InitDelay: break;
    case K::Left: renderLeft(data, pal, s.n); break;
    case K::TablesGap:
    case K::TextGap:
    case K::OptionsGap: renderLeft(data, pal, 0); break;
    case K::TablesWarpIn:
      renderLeft(data, pal, 0);
      renderTables(data, pal, [&](int i) { return assets_.warpTable[static_cast<std::size_t>(i)] < s.n; });
      break;
    case K::Tables:
      renderLeft(data, pal, 0);
      renderTables(data, pal, [](int) { return true; });
      break;
    case K::TablesWarpOut:
      renderLeft(data, pal, 0);
      renderTables(data, pal, [&](int i) { return assets_.warpTable[static_cast<std::size_t>(94 - i)] >= s.n; });
      break;
    case K::TablesFadeOut: {
      renderLeft(data, pal, 0);
      renderTables(data, pal, [](int) { return true; });
      const std::vector<Rgb> o(pal, pal + 256);
      fadePal(pal, o, Rgb{}, 80 - s.n, 80);
      fade(80 - s.n, 80);
      break;
    }
    case K::TextFadeIn:
      renderLeft(data, pal, 0);
      renderText(data, pal, true);
      scalePal(pal, 0x10, 0x20, s.n, 20);
      fadeText(s.n, 20);
      break;
    case K::Text:
      renderLeft(data, pal, 0);
      renderText(data, pal, false);
      break;
    case K::TextFadeOut:
      renderLeft(data, pal, 0);
      renderText(data, pal, true);
      scalePal(pal, 0x10, 0x20, 19 - s.n, 20);
      fadeText(19 - s.n, 20);
      break;
    case K::OptionsFadeIn:
      renderLeft(data, pal, 0);
      renderOptions(data, pal, true, std::nullopt);
      scalePal(pal, 0x10, 0x20, s.n, 40);
      break;
    case K::Options:
      renderLeft(data, pal, 0);
      renderOptions(data, pal, false, static_cast<u8>(s.n));
      break;
    case K::OptionsFadeOut:
      renderLeft(data, pal, 0);
      renderOptions(data, pal, true, std::nullopt);
      scalePal(pal, 0x10, 0x20, 39 - s.n, 40);
      break;
    case K::FadeOut: {
      renderLeft(data, pal, 0);
      const std::vector<Rgb> o(pal, pal + 256);
      fadePal(pal, o, Rgb{}, 80 - s.n, 80);
      fade(80 - s.n, 80);
      break;
    }
  }
  hd_ = nullptr;
}

}  // namespace pfr
