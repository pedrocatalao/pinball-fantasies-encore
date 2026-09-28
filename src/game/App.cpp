#include "game/App.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>

#include "core/Error.h"
#include "core/File.h"
#include "core/Log.h"
#include "core/Png.h"
#include "assets/TableAssetsIo.h"
#include "game/Fantasy.h"
#include "intro/IntroAssetsIo.h"
#include "platform/DataLocator.h"
#include "platform/ImageFile.h"

namespace pfr {
namespace {

constexpr double kFrame = 1.0 / 60.0;  ///< both screens run 60 frames a second, as in pfr

/// The table's 240- and 350-line screens fill a 4:3 display, so their pixels are not
/// square. The full-height mode keeps the 350-line pixel shape and shows the whole table.
double tablePixelAspect(int height) {
  const int shaped = height > 350 ? 350 : height;
  return (4.0 / 3.0) / (320.0 / shaped);
}


/// The font picture, as slots into the handful of colours it is drawn with.
AskFont loadAskFont(const std::filesystem::path& path) {
  AskFont font;
  const auto image = loadImageFile(path);
  if (!image) {
    log::error("cannot read the letters in " + path.string());
    return font;
  }
  font.width = image->width;
  font.height = image->height;
  font.index.resize(static_cast<std::size_t>(font.width) * font.height);
  for (std::size_t i = 0; i < font.index.size(); ++i) {
    const Rgb c{image->pixels[i * 4], image->pixels[i * 4 + 1], image->pixels[i * 4 + 2]};
    std::size_t slot = 0;
    while (slot < font.colors.size() &&
           (font.colors[slot].r != c.r || font.colors[slot].g != c.g || font.colors[slot].b != c.b))
      ++slot;
    // The picture is drawn with a few colours, as the original's font was; anything past
    // what a bank of the palette holds takes the nearest slot already known, the first.
    if (slot == font.colors.size()) {
      if (font.colors.size() >= 16) slot = 0;
      else font.colors.push_back(c);
    }
    font.index[i] = static_cast<u8>(slot);
  }
  return font;
}

/// One letter of the intro's font, whose cells are 18 x 14 and are shown twice as tall.
/// It holds capitals, digits and four marks, and nothing else is drawn.
void putChar(Framebuffer& fb, const AskFont& font, u8 chr, int x, int y, u8 bank) {
  int idx = -1;
  if (chr >= '0' && chr <= '9') idx = chr - '0';
  else if (chr >= 'A' && chr <= 'Z') idx = chr - 'A' + 10;
  else if (chr == '.') idx = 36;
  else if (chr == ':') idx = 37;
  else if (chr == '-') idx = 38;
  else if (chr == '>') idx = 39;
  if (idx < 0) return;
  const int fx = idx % 20 * 32, fy = idx / 20 * 14;
  for (int cy = 0; cy < 14; ++cy)
    for (int cx = 0; cx < 18; ++cx) {
      if (fx + cx >= font.width || fy + cy >= font.height) continue;
      const u8 v = static_cast<u8>(font.index[static_cast<std::size_t>(fy + cy) * font.width + fx + cx] | bank);
      fb.put(x + cx, y + cy * 2, v);
      fb.put(x + cx, y + cy * 2 + 1, v);
    }
}

constexpr int kLetterW = 18, kLetterH = 28;

void putText(Framebuffer& fb, const AskFont& font, std::string_view text, int x, int y, u8 bank = 0x10) {
  for (std::size_t i = 0; i < text.size(); ++i)
    putChar(fb, font, static_cast<u8>(text[i]), x + static_cast<int>(i) * kLetterW, y, bank);
}

/// Middled in the frame, so a line reads the same whatever the window is doing.
void putTextCentred(Framebuffer& fb, const AskFont& font, std::string_view text, int y, u8 bank = 0x10) {
  putText(fb, font, text, (fb.width() - static_cast<int>(text.size()) * kLetterW) / 2, y, bank);
}

/// A solid arrow pointing at what is chosen, in one colour of its own: the font's own marks
/// are as dark as the letters and go unseen on a dark screen.
void putArrow(Framebuffer& fb, int x, int y, u8 index) {
  constexpr int kHeight = 20, kWidth = 12;
  for (int row = 0; row < kHeight; ++row) {
    const int from = row < kHeight / 2 ? row : kHeight - 1 - row;  // narrowing to the point
    fb.fillRect(Rect{x, y + row, kWidth * from * 2 / kHeight + 1, 1}, index);
  }
}

std::filesystem::path executableDir() {
  const char* base = SDL_GetBasePath();
  return base ? std::filesystem::path(base) : std::filesystem::current_path();
}

/// The keys the game understands, from SDL key codes (the original layout: Shift, Ctrl or
/// Alt for the flippers, Space to nudge, Down to pull the plunger, F1-F4 for the tables).
Key keyFor(SDL_Keycode k) {
  switch (k) {
    case SDLK_LSHIFT: return Key::ShiftLeft;
    case SDLK_RSHIFT: return Key::ShiftRight;
    case SDLK_LCTRL: return Key::ControlLeft;
    case SDLK_RCTRL: return Key::ControlRight;
    case SDLK_LALT: return Key::AltLeft;
    case SDLK_RALT: return Key::AltRight;
    case SDLK_SPACE: return Key::Space;
    case SDLK_DOWN: return Key::ArrowDown;
    case SDLK_UP: return Key::ArrowUp;
    case SDLK_LEFT: return Key::ArrowLeft;
    case SDLK_RIGHT: return Key::ArrowRight;
    case SDLK_RETURN:
    case SDLK_KP_ENTER: return Key::Enter;
    case SDLK_ESCAPE: return Key::Escape;
    default: break;
  }
  if (k >= SDLK_F1 && k <= SDLK_F8) return static_cast<Key>(static_cast<int>(Key::F1) + static_cast<int>(k - SDLK_F1));
  if (k >= SDLK_1 && k <= SDLK_8) return static_cast<Key>(static_cast<int>(Key::Digit1) + static_cast<int>(k - SDLK_1));
  if (k >= SDLK_A && k <= SDLK_Z) return static_cast<Key>(static_cast<int>(Key::A) + static_cast<int>(k - SDLK_A));
  return Key::None;
}

}  // namespace

App::App(AppOptions options) : options_(std::move(options)), frame_(640, 480) {}

bool App::init() {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
    log::error(std::string("SDL_Init: ") + SDL_GetError());
    return false;
  }
  saveDir_ = preferencesDir();

  // Prefer the shaders in the source tree while developing, so edits take effect at once.
  const std::filesystem::path source = std::filesystem::path(ENCORE_SOURCE_DIR) / "shaders";
  shaderDir_ = std::filesystem::exists(source) ? source : executableDir() / "shaders";

  if (!window_.create("Pinball Fantasies: Encore!", 640 * std::max(1, options_.windowScale) / 2,
                      480 * std::max(1, options_.windowScale) / 2))
    return false;
  if (options_.fullscreen) window_.setFullscreen(true);
  // The window's context is current from here, so the driver can be asked for its functions.
  if (!loadGlFunctions()) {
    log::error("this computer's OpenGL is missing what the renderer needs (4.1 core)");
    return false;
  }
  if (!renderer_.init(shaderDir_, 640, 480, 1.0)) return false;
  renderer_.setSmoothEdges(options_.smoothEdges);
  {
    const auto saved = file::readAll(saveDir_ / "crt.txt");
    setCrt(options_.crt.value_or(saved && !saved->empty() && (*saved)[0] == '1'));
  }
  // A trial: files offered on the first run and unpacked where this version keeps what is
  // its own -- the same folder as PINBALL.CFG, the high scores and the remembered settings,
  // whatever the platform calls it. The archive holds an hd folder, which is the first place
  // the replacement pictures are looked for, so what is fetched now is used by the loading
  // below. Turning the offer down ends the game.
  // Off for now: set to true to put the question at start again.
  constexpr bool kOfferDownloadAtStart = false;
  if (kOfferDownloadAtStart) {
    // The letters come with this version, not from the original, so the question can be put
    // before a single game file is there.
    askFont_ = loadAskFont(hdPicturePath("font.png"));
    // Without letters there is no way to put the question, and no question means no offer.
    if (askFont_.width == 0) log::error("the letters could not be read; not offering the download");
    if (askFont_.width != 0 && !downloadFantasyOnce(
            saveDir_, [this] { return askToDownload(); },
            [this](double seconds) { drawWaiting(seconds, "DOWNLOADING GAME FILES"); }))
      return false;
  }
  // The game is read in the open format only, from one game folder. A copy of the DOS release
  // -- the one given with --data, or the one in FANTASY -- is converted into it the first time
  // and not read again; --data may also name a converted game directly. This is looked at only
  // once the download above has had its say, so that a first start can fetch what it offers
  // before anything is asked for.
  {
    // A build made from the source keeps it in the project's own game folder, which git
    // ignores, the way it reads the pictures from assets/hd; any other build keeps it beside
    // the options.
    const std::filesystem::path project(ENCORE_SOURCE_DIR);
    std::error_code ec;
    const std::filesystem::path converted =
        std::filesystem::exists(project / "CMakeLists.txt", ec) ? project / "game" : saveDir_ / "game";
    std::optional<std::filesystem::path> open;
    if (options_.dataDir && OpenGame::isIn(*options_.dataDir))
      open = *options_.dataDir;
    else if (!options_.dataDir && OpenGame::isIn(converted))
      open = converted;
    else if (const auto dos = locateGameData(options_.dataDir)) {
      try {
        convertGame(*dos, converted);
        open = converted;
      } catch (const DataError& e) {
        log::error(e.what());
        reportError(std::string("The game files could not be converted.\n\n") + e.what());
        return false;
      }
    }
    if (!open) {
      log::error("the Pinball Fantasies data files were not found");
      reportMissingGameData();
      return false;
    }
    game_ = OpenGame::at(*open);
  }
  log::info("game data: " + game_.directory.string());
  // Options and high scores live in this version's own folder; the first time, they are
  // taken from the converted game, which brought them along from the DOS folder if it had them.
  config_ = Config::load(saveDir_, game_.directory);
  if (options_.resolution) config_.options.resolution = *options_.resolution;

  loadHdPictures();
  {
    const auto saved = file::readAll(saveDir_ / "hd.txt");
    renderer_.setHdEnabled(options_.hd.value_or(!saved || saved->empty() || (*saved)[0] != '0'));
    if (options_.hd) setHd(*options_.hd);
  }
  {
    const auto saved = file::readAll(saveDir_ / "trail.txt");
    ballTrail_ = options_.trail.value_or(!saved || saved->empty() || (*saved)[0] != '0');
    if (options_.trail) setBallTrail(*options_.trail);
  }
  audio_.open(48000);

  if (options_.table >= 1 && options_.table <= 4)
    openTable(options_.table - 1);
  else
    openIntro(options_.skipIntro ? 0 : -1);
  return true;
}

/// The CRT look on or off, remembered for next time.
void App::setCrt(bool on) {
  renderer_.setCrt(on);
  const char c = on ? '1' : '0';
  file::writeAll(saveDir_ / "crt.txt", ByteView(reinterpret_cast<const u8*>(&c), 1));
  log::info(std::string("CRT look ") + (on ? "on" : "off"));
}

/// High-resolution replacements for the intro's pictures, named after HdPicture
/// (slide1.png ... slide5.png, left.png, table1.png ... table4.png, hiscores.png). Each file
/// must show the whole original picture, edge to edge, at any size. The application carries
/// its own (assets/hd), and only --hd-dir puts another set in their place. What the download
/// leaves in the preferences folder is not read: fetching it is the experiment for now, and
/// choosing between sets comes later.
std::filesystem::path App::hdPicturePath(const std::string& name) const {
  if (options_.hdDir && std::filesystem::exists(*options_.hdDir / name)) return *options_.hdDir / name;
  const std::filesystem::path source = std::filesystem::path(ENCORE_SOURCE_DIR) / "assets" / "hd";
  return (std::filesystem::exists(source) ? source : executableDir() / "hd") / name;
}

void App::loadHdPictures() {
  int count = 0;
  for (std::size_t i = 1; i < HdFrame::kCount; ++i) {
    const auto p = static_cast<HdPicture>(i);
    const auto path = hdPicturePath(std::string(hdPictureName(p)) + ".png");
    if (!std::filesystem::exists(path)) continue;
    const auto image = loadImageFile(path);
    if (!image) {
      log::error("cannot read " + path.string());
      continue;
    }
    renderer_.setHdPicture(p, image->width, image->height, image->pixels.data());
    ++count;
  }
  if (count) log::info("replacement pictures: " + std::to_string(count));
}

/// The flippers drawn at any angle: a picture of each on its own, if there is one, and
/// otherwise the flipper cut out of the original artwork.
void App::loadFlipperPictures(int table) {
  renderer_.clearSpritePictures();
  const auto cutOut = table_->flipperPictures();
  const auto sides = table_->flipperSides();
  ownFlipperPictures_ = 0;
  std::array<int, 2> seen{};
  int own = 0;
  for (std::size_t f = 0; f < cutOut.size(); ++f) {
    const bool left = sides[f] == FlipperSide::Left;
    const int nth = ++seen[left ? 0 : 1];
    const std::string name = "flipper" + std::to_string(table + 1) + (left ? "_left" : "_right") +
                             (nth > 1 ? std::to_string(nth) : "") + ".png";
    const auto path = hdPicturePath(name);
    std::optional<RgbaImage> picture;
    if (std::filesystem::exists(path)) {
      picture = loadImageFile(path);
      if (!picture) log::error("cannot read " + path.string());
    }
    if (picture) {
      renderer_.setSpritePicture(f, picture->width, picture->height, picture->pixels.data());
      ownFlipperPictures_ = static_cast<u8>(ownFlipperPictures_ | (1u << f));
      ++own;
    } else {
      renderer_.setSpritePicture(f, cutOut[f].width, cutOut[f].height, cutOut[f].rgba.data());
    }
  }
  if (own) log::info("flipper pictures: " + std::to_string(own) + " of " + std::to_string(cutOut.size()));

  // The ball: its own picture if there is one, and otherwise the original's.
  std::optional<RgbaImage> ball;
  for (const std::string& name : {"ball" + std::to_string(table + 1) + ".png", std::string("ball.png")}) {
    const auto path = hdPicturePath(name);
    if (!std::filesystem::exists(path)) continue;
    ball = loadImageFile(path);
    if (!ball) log::error("cannot read " + path.string());
    break;
  }
  if (ball) {
    renderer_.setSpritePicture(HdSprite::kBall, ball->width, ball->height, ball->pixels.data());
    log::info("ball picture: " + std::to_string(ball->width) + "x" + std::to_string(ball->height));
  } else {
    const auto original = table_->ballPicture();
    renderer_.setSpritePicture(HdSprite::kBall, original.width, original.height, original.rgba.data());
  }
}

/// The replacement pictures on or off, remembered for next time.
void App::setHd(bool on) {
  renderer_.setHdEnabled(on);
  const char c = on ? '1' : '0';
  file::writeAll(saveDir_ / "hd.txt", ByteView(reinterpret_cast<const u8*>(&c), 1));
  log::info(std::string("replacement pictures ") + (on ? "on" : "off"));
}

/// The ball's trail on or off, remembered for next time.
void App::setBallTrail(bool on) {
  ballTrail_ = on;
  const char c = on ? '1' : '0';
  file::writeAll(saveDir_ / "trail.txt", ByteView(reinterpret_cast<const u8*>(&c), 1));
  log::info(std::string("ball trail ") + (on ? "on" : "off"));
}

void App::resizeFrame(int width, int height, double pixelAspect) {
  if (frame_.width() != width || frame_.height() != height) frame_ = Framebuffer(width, height);
  renderer_.setPixelAspect(options_.squarePixels ? 1.0 : pixelAspect);
}

void App::openIntro(int returningFrom) {
  audio_.setSource({});
  table_.reset();
  // The slideshow plays to the intro's music; coming back from a table the menu has its own.
  const auto mod = file::readAll(returningFrom < 0 ? game_.introMusic : game_.menuMusic);
  if (!mod) throw DataError("cannot read the intro's music in " + game_.intro.string());
  intro_ = std::make_unique<Intro>(loadIntroAssets(game_.intro), *mod, config_, returningFrom);
  resizeFrame(intro_->width(), intro_->height(), 1.0);
  audio_.setSource([p = &intro_->player()](float* out, int frames) { p->render(out, frames); });
}

void App::openTable(int index) {
  audio_.setSource({});
  intro_.reset();
  const auto i = static_cast<std::size_t>(index);
  const auto mod = file::readAll(game_.tableMusic[i]);
  if (!mod) throw DataError("cannot read the music in " + game_.tables[i].string());
  const u64 seed = static_cast<u64>(std::chrono::steady_clock::now().time_since_epoch().count());
  table_ = std::make_unique<Table>(loadTableAssets(game_.tables[i]), *mod, config_, index, seed);
  resizeFrame(320, table_->screenHeight(), tablePixelAspect(table_->screenHeight()));
  audio_.setSource([p = &table_->player()](float* out, int frames) { p->render(out, frames); });
  loadFlipperPictures(index);
  log::info("opened table " + std::to_string(index + 1));
}

void App::handleKey(const SDL_Event& e) {
  if (e.type != SDL_EVENT_KEY_DOWN && e.type != SDL_EVENT_KEY_UP) return;
  if (e.key.repeat) return;
  if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F && (e.key.mod & SDL_KMOD_GUI)) {
    window_.setFullscreen(!window_.fullscreen());
    return;
  }
  if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F9) {
    setCrt(!renderer_.crt());
    return;
  }
  if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F10) {
    if (renderer_.hasHdPictures()) setHd(!renderer_.hdEnabled());
    return;
  }
  // While paused, beside the lamps on F7: the ball's trail, for looking at it either way.
  if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F8 && table_ && table_->paused()) {
    setBallTrail(!ballTrail_);
    return;
  }
  const Key k = keyFor(e.key.key);
  if (k == Key::None) return;
  const bool down = e.type == SDL_EVENT_KEY_DOWN;
  if (table_) table_->handleKey(k, down);
  else if (intro_) intro_->handleKey(k, down);
}

void App::update(double dt) {
  clock_ += dt;
  while (clock_ >= kFrame) {
    clock_ -= kFrame;
    if (intro_) {
      const IntroAction a = intro_->runFrame();
      switch (a.kind) {
        case IntroAction::Kind::OpenTable:
          config_.options = intro_->options();
          openTable(a.table);
          break;
        case IntroAction::Kind::SaveOptions:
          config_.options = intro_->options();
          Config::saveOptions(saveDir_, config_.options);
          resizeFrame(intro_->width(), intro_->height(), 1.0);
          break;
        case IntroAction::Kind::Quit: running_ = false; return;
        case IntroAction::Kind::None: break;
      }
    } else if (table_) {
      const TableAction a = table_->runFrame();
      const int index = table_->tableIndex();
      switch (a.kind) {
        case TableAction::Kind::SaveOptions:
          config_.options = table_->options();
          Config::saveOptions(saveDir_, config_.options);
          break;
        case TableAction::Kind::SaveHighScores:
          config_.highScores[static_cast<std::size_t>(index)] = table_->highScores();
          Config::saveHighScores(saveDir_, index, table_->highScores());
          break;
        case TableAction::Kind::Quit:
          config_.options = table_->options();
          openIntro(index);
          return;
        case TableAction::Kind::None: break;
      }
      if (table_) resizeFrame(320, table_->screenHeight(), tablePixelAspect(table_->screenHeight()));
    }
  }
}

/// The offer of the pictures, put in the game's own window rather than in a box of the
/// system's: two lines and a choice, answered with the arrow keys and enter, or with Y and
/// N, or turned down with escape.
bool App::askToDownload() {
  bool yes = true;
  for (;;) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) return false;
      if (event.type != SDL_EVENT_KEY_DOWN) continue;
      switch (event.key.key) {
        case SDLK_LEFT: case SDLK_RIGHT: case SDLK_UP: case SDLK_DOWN: case SDLK_TAB: yes = !yes; break;
        case SDLK_Y: return true;
        case SDLK_N: case SDLK_ESCAPE: return false;
        case SDLK_RETURN: case SDLK_KP_ENTER: case SDLK_SPACE: return yes;
        default: break;
      }
    }
    frame_.clear(0);
    palette_[0] = Rgb{0x10, 0x10, 0x18};
    palette_[1] = Rgb{0xff, 0xc8, 0x50};  // the arrow
    // Three banks of the font's own colours: as they are for the question, lifted towards
    // white for what is chosen, sunk towards the background for what is not. The first
    // colour is what fills a letter's cell, and it stays the screen's own: lifting it too
    // would put a pale block behind the word instead of leaving letters on a dark screen.
    for (std::size_t i = 0; i < askFont_.colors.size(); ++i) {
      const Rgb c = askFont_.colors[i];
      const auto lift = [](u8 v) { return static_cast<u8>(v + (0xff - v) * 3 / 5); };
      const auto sink = [](u8 v) { return static_cast<u8>(v * 2 / 5); };
      palette_[0x10 + i] = c;
      palette_[0x20 + i] = i == 0 ? palette_[0] : Rgb{lift(c.r), lift(c.g), lift(c.b)};
      palette_[0x30 + i] = i == 0 ? palette_[0] : Rgb{sink(c.r), sink(c.g), sink(c.b)};
    }
    const int top = frame_.height() / 2 - 3 * kLetterH;
    putTextCentred(frame_, askFont_, "YOU LEGALLY OWN", top);
    putTextCentred(frame_, askFont_, "PINBALL FANTASIES", top + kLetterH + 6);
    putTextCentred(frame_, askFont_, "TO PLAY ENCORE", top + kLetterH*2 + 12);
    const int row = top + 4 * (kLetterH + 6);
    const int left = (frame_.width() - 11 * kLetterW) / 2;
    putText(frame_, askFont_, "YES", left, row, yes ? 0x20 : 0x30);
    putText(frame_, askFont_, "NO", left + 7 * kLetterW, row, yes ? 0x30 : 0x20);
    putArrow(frame_, (yes ? left : left + 7 * kLetterW) - kLetterW - 6, row + kLetterH / 2 - 10, 1);
    int w = 0, h = 0;
    window_.drawableSize(w, h);
    renderer_.setPalette(palette_);
    renderer_.draw(frame_, w, h, 0.0);
    window_.swap();
    SDL_Delay(16);
  }
}

/// While a set of pictures is being fetched: a line of text and a ring of dots turning, in
/// the game's own window, so the wait does not look like a hang. The game has drawn nothing
/// yet at this point, so the palette is this drawing's own.
void App::drawWaiting(double seconds, std::string_view line) {
  constexpr int kDots = 8, kSize = 10;
  constexpr double kRadius = 44.0;
  frame_.clear(0);
  palette_[0] = Rgb{0x10, 0x10, 0x18};
  for (std::size_t i = 0; i < askFont_.colors.size(); ++i) palette_[0x10 + i] = askFont_.colors[i];
  putTextCentred(frame_, askFont_, line, frame_.height() / 2 - 3 * kLetterH);
  const int cx = frame_.width() / 2, cy = frame_.height() / 2 + kLetterH;
  for (int i = 0; i < kDots; ++i) {
    const double angle = (seconds * 1.5 + static_cast<double>(i) / kDots) * 2.0 * 3.14159265358979;
    const double fade = 1.0 - static_cast<double>(i) / kDots;
    const auto shade = [&](int full) { return static_cast<u8>(0x18 + (full - 0x18) * fade * fade); };
    palette_[static_cast<std::size_t>(i) + 1] = Rgb{shade(0xff), shade(0xc0), shade(0x40)};
    const Rect dot{cx + static_cast<int>(std::cos(angle) * kRadius) - kSize / 2,
                   cy + static_cast<int>(std::sin(angle) * kRadius) - kSize / 2, kSize, kSize};
    frame_.fillRect(dot, static_cast<u8>(i + 1));
  }
  int w = 0, h = 0;
  window_.drawableSize(w, h);
  renderer_.setPalette(palette_);
  renderer_.draw(frame_, w, h, std::max(0.0, seconds));
  window_.swap();
}

void App::render(double now) {
  std::array<Rgb, 256> colors{};
  // Only when the replacements will really be drawn: the table leaves the flippers out of the
  // frame for the renderer to put back, so with them off it must draw everything itself.
  HdFrame* const hd = renderer_.hasHdPictures() && renderer_.hdEnabled() ? &hd_ : nullptr;
  hd_.ownSprites = ownFlipperPictures_;
  hd_.ballTrail = ballTrail_;
  if (table_)
    table_->render(frame_.data(), colors.data(), hd);
  else if (intro_)
    intro_->render(frame_.data(), colors.data(), hd);
  palette_.set(0, std::vector<Rgb>(colors.begin(), colors.end()));
  int w = 0, h = 0;
  window_.drawableSize(w, h);
  renderer_.setPalette(palette_);
  const auto beforeDraw = std::chrono::steady_clock::now();
  renderer_.draw(frame_, w, h, now, &hd_);
  if (options_.screenshot && ++frameCounter_ >= options_.screenshotFrame) {
    std::vector<u8> rgb(static_cast<std::size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    writeRgbPng(*options_.screenshot, rgb.data(), w, h, true);
    log::info("screenshot written to " + options_.screenshot->string());
    running_ = false;
  }
  const auto beforeSwap = std::chrono::steady_clock::now();
  window_.swap();
  if (options_.stats) {
    using ms = std::chrono::duration<double, std::milli>;
    stats_.draw += ms(beforeSwap - beforeDraw).count();
    stats_.wait += ms(std::chrono::steady_clock::now() - beforeSwap).count();
  }
}

int App::run() {
  try {
    if (!init()) return 1;
    using clock = std::chrono::steady_clock;
    const auto start = clock::now();
    auto last = start;
    double reloadTimer = 0;
    while (running_) {
      SDL_Event e;
      while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) running_ = false;
        // Nothing in the game is played with the mouse, so the pointer keeps out of the way
        // while it is over the window, and comes back when it leaves or the window does.
        if (e.type == SDL_EVENT_WINDOW_MOUSE_ENTER || e.type == SDL_EVENT_WINDOW_FOCUS_GAINED) SDL_HideCursor();
        if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE || e.type == SDL_EVENT_WINDOW_FOCUS_LOST) SDL_ShowCursor();
        handleKey(e);
      }
      const auto nowT = clock::now();
      const double dt = std::min(0.1, std::chrono::duration<double>(nowT - last).count());
      last = nowT;
      const auto beforeUpdate = clock::now();
      update(dt);
      if (options_.stats) {
        using ms = std::chrono::duration<double, std::milli>;
        stats_.update += ms(clock::now() - beforeUpdate).count();
        stats_.worst = std::max(stats_.worst, dt * 1000);
        stats_.seconds += dt;
        if (++stats_.frames >= 60 && stats_.seconds > 0) {
          const double n = stats_.frames;
          log::info("stats: " + std::to_string(n / stats_.seconds).substr(0, 5) + " frames a second, update " +
                    std::to_string(stats_.update / n).substr(0, 4) + " ms, draw " +
                    std::to_string(stats_.draw / n).substr(0, 4) + " ms, waiting for the screen " +
                    std::to_string(stats_.wait / n).substr(0, 4) + " ms, longest frame " +
                    std::to_string(stats_.worst).substr(0, 5) + " ms");
          stats_ = {};
        }
      }
      reloadTimer += dt;
      if (reloadTimer > 1.0) {
        reloadTimer = 0;
        renderer_.pollShaderReload();
      }
      render(std::chrono::duration<double>(nowT - start).count());
    }
  } catch (const std::exception& e) {
    // Whatever went wrong, it is said out loud rather than ending the program in silence.
    log::error(e.what());
    reportError(e.what());
    audio_.close();
    SDL_Quit();
    return 1;
  }
  audio_.setSource({});
  audio_.close();
  SDL_Quit();
  return 0;
}

}  // namespace pfr
