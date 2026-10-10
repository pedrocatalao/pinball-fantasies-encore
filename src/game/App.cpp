#include "game/App.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <ctime>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>

#include "core/Error.h"
#include "core/File.h"
#include "core/Log.h"
#include "core/Png.h"
#include "game/Art.h"
#include "game/Fantasy.h"
#include "game/Release.h"
#include "platform/DataLocator.h"
#include "platform/ImageFile.h"

namespace encore {
namespace {

constexpr double kFrame = 1.0 / 60.0;  ///< the menu and the tables both run 60 frames a second

/// The table's 240- and 350-line screens fill a 4:3 display, so their pixels are not
/// square. The full-height mode keeps the 350-line pixel shape and shows the whole table; the
/// tall one shows it with square pixels, as the 240-line screen has them.
double tablePixelAspect(const encore::TableGame& table) {
  if (table.options().resolution == Resolution::Tall) return 1.0;
  const int shaped = std::min(table.screenHeight(), 350);
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
/// It holds capitals, digits and five marks (the ? added for this version), and nothing else is drawn.
void putChar(Framebuffer& fb, const AskFont& font, u8 chr, int x, int y, u8 bank) {
  int idx = -1;
  if (chr >= '0' && chr <= '9') idx = chr - '0';
  else if (chr >= 'A' && chr <= 'Z') idx = chr - 'A' + 10;
  else if (chr == '.') idx = 36;
  else if (chr == ':') idx = 37;
  else if (chr == '-') idx = 38;
  else if (chr == '>') idx = 39;
  else if (chr == '?') idx = 40;
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

/// Whether there is something at the path; what cannot be looked at is not there. On Windows a
/// drive with no card or disc in it answers "not ready", and the build's own folder, looked for
/// first, can be on such a drive (D:, on GitHub's builders).
bool present(const std::filesystem::path& path) {
  std::error_code ec;
  return std::filesystem::exists(path, ec);
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
  // Who this is, for the system: on Linux the identifier is the window's app id, which is how
  // the desktop matches the window to its menu entry and icon (see packaging/linux).
  SDL_SetAppMetadata("Pinball Fantasies: Encore!", nullptr, "org.encore.pinball-fantasies");
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
    log::error(std::string("SDL_Init: ") + SDL_GetError());
    return false;
  }
  saveDir_ = preferencesDir();
  shaderDir_ = executableDir() / "shaders";
  // Whether the server has newer HD pictures is asked at once, so that the answer is usually
  // there by the time the question could be put.
  if (!options_.hdDir && !options_.video && !options_.screenshot)
    artCheck_ = std::async(std::launch::async, [] {
      std::string error;
      auto set = fetchArtSet(&error);
      if (!set) log::info("HD pictures: no word from the server (" + error + ")");
      return set;
    });
  // And, for a release, whether there is a newer one.
  if (!thisRelease().empty() && !options_.video && !options_.screenshot)
    releaseCheck_ = std::async(std::launch::async, [] {
      std::string error;
      auto release = fetchLatestRelease(&error);
      if (!release) log::info("new release: no word from the server (" + error + ")");
      return release;
    });

  if (!window_.create("Pinball Fantasies: Encore!", 640 * std::max(1, options_.windowScale) / 2,
                      480 * std::max(1, options_.windowScale) / 2))
    return false;
  // Fullscreen or in a window, as it was left last time, unless told otherwise.
  {
    const auto saved = file::readAll(saveDir_ / "fullscreen.txt");
    if (!options_.video && (options_.fullscreen || (saved && !saved->empty() && (*saved)[0] == '1'))) {
      // macOS slides into fullscreen over about a second, stretching whatever the window last
      // showed to the shape of the screen as it goes: the first frames of the intro would be
      // seen pulled wide and then snap back. So the window shows black while it goes, and
      // nothing is drawn until it has arrived; the intro then starts at its final size.
      glClearColor(0, 0, 0, 1);
      glClear(GL_COLOR_BUFFER_BIT);
      window_.swap();
      window_.setFullscreen(true);
      const auto before = std::chrono::steady_clock::now();
      SDL_SyncWindow(window_.handle());
      const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - before);
      log::info("fullscreen reached in " + std::to_string(waited.count()) + " ms");
    }
  }
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
  // whatever the platform calls it. Turning the offer down ends the game.
  {
    // The letters come with this version, not from the original, so the question can be put
    // before a single game file is there, or any HD picture. The fetched set has them too, so
    // that they can change without a release: once there, they are used instead (and --hd-dir's
    // before either). If those cannot be read, the application's own still can.
    const std::filesystem::path own = executableDir() / "font.png";
    std::filesystem::path letters = own;
    if (options_.hdDir && present(*options_.hdDir / "font.png")) letters = *options_.hdDir / "font.png";
    else if (const auto fetched = installedArt(saveDir_); fetched && present(fetched->dir / "font.png"))
      letters = fetched->dir / "font.png";
    askFont_ = loadAskFont(letters);
    if (askFont_.width == 0 && letters != own) askFont_ = loadAskFont(own);
    // Without letters there is no way to put the question, and no question means no offer.
    if (askFont_.width == 0) log::error("the letters could not be read; not offering the download");
    if (askFont_.width != 0 && !downloadFantasyOnce(
            saveDir_, [this] { return askToDownload(); },
            [this](double seconds) { drawWaiting(seconds, "DOWNLOADING GAME FILES"); }))
      return false;
  }
  // The game files are looked for only once the download above has had its say, so that a
  // first start can fetch what it offers before anything of the original is asked for.
  const auto dataDir = locateGameData(options_.dataDir);
  if (!dataDir) {
    log::error("the Pinball Fantasies data files were not found");
    reportMissingGameData();
    return false;
  }
  files_ = GameFiles::fromDirectory(*dataDir);
  log::info("game data: " + files_.directory.string());
  // Options and high scores live in this version's own folder, never in the game folder.
  config_ = Config::load(saveDir_, files_.directory);
  if (options_.resolution) config_.options.resolution = *options_.resolution;

  if (!offerArt()) return false;
  // Gone to fetch the new release: the game ends here, as when it is quit.
  if (!offerRelease()) {
    running_ = false;
    return true;
  }
  loadHdPictures();
  {
    const auto saved = file::readAll(saveDir_ / "hd.txt");
    renderer_.setHdEnabled(options_.hd.value_or(!saved || saved->empty() || (*saved)[0] != '0'));
    if (options_.hd) setHd(*options_.hd);
  }
  {
    const auto saved = file::readAll(saveDir_ / "sfx.txt");
    balancedSound_ = !saved || saved->empty() || (*saved)[0] != '0';
  }
  {
    const auto saved = file::readAll(saveDir_ / "trail.txt");
    ballTrail_ = options_.trail.value_or(!saved || saved->empty() || (*saved)[0] != '0');
    if (options_.trail) setBallTrail(*options_.trail);
  }
  sound_ = audio_.open(48000);
  // Whatever could not be sent last time goes now.
  sender_ = std::make_unique<ScoreSender>(saveDir_);
  sender_->send();

  if (!options_.replays.empty() && openReplay(options_.replays[0])) {
    nextReplay_ = 1;
  } else if (options_.table >= 1 && options_.table <= 4) {
    openTable(options_.table - 1);
  } else {
    openIntro(options_.skipIntro ? 0 : -1);
  }
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
/// must show the whole original picture, edge to edge, at any size. They come from --hd-dir
/// when it is given (assets/hd, to see changes to them at once), and otherwise from the set
/// fetched from the server (game/Art.h); with neither, the original pictures are shown.
std::filesystem::path App::hdPicturePath(const std::string& name) const {
  if (options_.hdDir) return *options_.hdDir / name;
  return artDir_.empty() ? std::filesystem::path() : artDir_ / name;
}

void App::loadHdPictures() {
  int count = 0;
  for (std::size_t i = 1; i < HdFrame::kCount; ++i) {
    const auto p = static_cast<HdPicture>(i);
    const auto path = hdPicturePath(std::string(hdPictureName(p)) + ".png");
    if (!present(path)) continue;
    const auto image = loadImageFile(path);
    if (!image) {
      log::error("cannot read " + path.string());
      continue;
    }
    renderer_.setHdPicture(p, image->width, image->height, image->pixels.data());
    if (p == HdPicture::LeftRepeat) panelStrip_ = {image->width, image->height};
    ++count;
  }
  if (count) log::info("replacement pictures: " + std::to_string(count));
}

/// The flippers drawn at any angle: a picture of each on its own, if there is one, and
/// otherwise the flipper cut out of the original artwork.
void App::loadFlipperPictures(int table) {
  renderer_.clearSpritePictures();
  const auto cutOut = table_->flipperPictures();
  const auto sides = table_->flipperIsLeft();
  ownFlipperPictures_ = 0;
  std::array<int, 2> seen{};
  int own = 0;
  for (std::size_t f = 0; f < cutOut.size(); ++f) {
    const bool left = sides[f];
    const int nth = ++seen[left ? 0 : 1];
    const std::string name = "flipper" + std::to_string(table + 1) + (left ? "_left" : "_right") +
                             (nth > 1 ? std::to_string(nth) : "") + ".png";
    const auto path = hdPicturePath(name);
    std::optional<RgbaImage> picture;
    if (present(path)) {
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

  // What hides the ball, drawn to the replacement playfield, if there is a picture of it: on
  // the playfield and on the ramps, each on its own. Black hides the ball, white does not, and
  // pure red is clear plastic, which shows it greyed.
  renderer_.clearCoverPictures();
  for (int layer = 0; layer < 2; ++layer) {
    const auto path = hdPicturePath("hides_ball" + std::to_string(table + 1) + (layer ? "_ramps" : "_playfield") + ".png");
    if (!present(path)) continue;
    const auto cover = loadImageFile(path);
    if (!cover) {
      log::error("cannot read " + path.string());
      continue;
    }
    renderer_.setCoverPicture(layer, cover->width, cover->height, cover->pixels.data(), TableData::kWidth, TableData::kHeight);
    log::info("what hides the ball: " + path.filename().string());
  }

  // The ball: its own picture if there is one, and otherwise the original's.
  std::optional<RgbaImage> ball;
  for (const std::string& name : {"ball" + std::to_string(table + 1) + ".png", std::string("ball.png")}) {
    const auto path = hdPicturePath(name);
    if (!present(path)) continue;
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
  showLooks();
}

/// This version's mixing of the music and the sounds, or the driver's, remembered for next time.
void App::setBalancedSound(bool on) {
  balancedSound_ = on;
  const char c = on ? '1' : '0';
  file::writeAll(saveDir_ / "sfx.txt", ByteView(reinterpret_cast<const u8*>(&c), 1));
  log::info(std::string("sound ") + (on ? "balanced" : "as the original's driver mixes it"));
  showLooks();
}

void App::showLooks() {
  config_.options.originalSound = !balancedSound_;
  config_.options.originalPictures = !renderer_.hdEnabled();
  if (intro_) {
    intro_->setLooks(config_.options.originalSound, config_.options.originalPictures);
    intro_->music().setBalanced(balancedSound_);
  }
  if (table_) table_->music().setBalanced(balancedSound_);
}

/// The ball's trail on or off, remembered for next time.
void App::setBallTrail(bool on) {
  ballTrail_ = on;
  const char c = on ? '1' : '0';
  file::writeAll(saveDir_ / "trail.txt", ByteView(reinterpret_cast<const u8*>(&c), 1));
  log::info(std::string("ball trail ") + (on ? "on" : "off"));
}

void App::resizeFrame(int width, int height, double pixelAspect, int displayRows) {
  if (frame_.width() != width || frame_.height() != height) frame_ = Framebuffer(width, height);
  renderer_.setPixelAspect(options_.squarePixels ? 1.0 : pixelAspect);
  renderer_.setDisplayRows(displayRows);
}

void App::openIntro(int returningFrom) {
  audio_.setSource({});
  table_.reset();
  replaying_ = fromReplay_ = false;
  // The slideshow plays to INTRO.MOD; coming back from a table the menu plays MOD2.MOD.
  const auto prg = file::readAll(files_.intro);
  const auto mod = file::readAll(returningFrom < 0 ? files_.introMusic : files_.menuMusic);
  if (!prg || !mod) throw DataError("cannot read INTRO.PRG or its music");
  showLooks();  // (into the options the menu is made with)
  intro_ = std::make_unique<encore::Front>(*prg, *mod, config_, returningFrom);
  showLooks();
  intro_->setPanelStrip(panelStrip_[0], panelStrip_[1]);
  resizeFrame(encore::Front::kWidth, intro_->height(), 1.0);
  audio_.setSource([f = intro_.get()](float* out, int frames) { f->sound(out, frames); });
}

/// A table to play on; with `recording`, the table that recording was played on, which then
/// plays it back.
void App::openTable(int index, const encore::Recording* recording) {
  audio_.setSource({});
  intro_.reset();
  table_.reset();
  const auto prg = file::readAll(files_.tables[static_cast<std::size_t>(index)]);
  const auto mod = file::readAll(files_.tableMusic[static_cast<std::size_t>(index)]);
  if (!prg || !mod) throw DataError("cannot read the table files");
  tablePrg_ = *prg;
  tableMod_ = *mod;
  tableIndex_ = index;
  encore::TableGame::Setup setup;
  if (recording) {
    // played as it was, and seen as this player sees every table
    setup.options = recording->options;
    setup.options.resolution = config_.options.resolution;
    setup.highScores = recording->highScores;
    setup.seed = recording->seed;
    setup.carry = recording->carry;
  } else {
    setup.options = config_.options;
    setup.highScores = config_.highScores[static_cast<std::size_t>(index)];
    setup.seed = static_cast<u64>(std::chrono::steady_clock::now().time_since_epoch().count());
  }
  table_ = std::make_unique<encore::TableGame>(tablePrg_, tableMod_, index, setup);
  showLooks();
  if (recording) table_->playBack(recording);
  // A game played back is the recording's, not one to keep or send.
  recordingSaved_ = recording != nullptr;
  replaying_ = fromReplay_ = recording != nullptr;
  replayNext_ = 0;
  replayFrame_ = 0;
  resizeFrame(320, table_->screenHeight(), tablePixelAspect(*table_), encore::TableScreen::kDisplayRows);
  audio_.setSource([t = table_.get()](float* out, int frames) { t->sound(out, frames); });
  loadFlipperPictures(index);
  log::info("opened table " + std::to_string(index + 1));
}

/// Plays a recording, dropped on the program or its window or named on its command line, on
/// the table it was played on. Once it is over the table stays, for a game of one's own.
bool App::openReplay(const std::filesystem::path& path) {
  const auto data = file::readAll(path);
  auto recording = data ? encore::Recording::load(*data) : std::nullopt;
  if (!recording) {
    log::error("not a recording this version can play: " + path.string());
    return false;
  }
  audio_.setSource({});
  table_.reset();  // before the recording it may be playing goes
  replay_ = std::move(recording);
  openTable(replay_->table, &*replay_);
  if (options_.video) table_->silent = true;  // filmed in silence
  log::info("playing " + path.filename().string());
  return true;
}

/// A recording has played to its end, or as far as it is filmed: on to the next one, if there
/// is one; true when the table is no longer the one that was playing.
bool App::recordingOver() {
  replaying_ = false;
  if (table_) table_->playBack(nullptr);
  if (clip_) endClip();
  if (nextReplay_ < options_.replays.size()) {
    if (!openReplay(options_.replays[nextReplay_++])) return recordingOver();
    return true;
  }
  if (options_.video) {
    running_ = false;
    return true;
  }
  log::info("the recording is over; the table is yours");
  return false;
}

/// A drawn frame, to the clip being filmed: raw pixels down a pipe to ffmpeg, which makes the
/// file, so every frame is in it however long the drawing took.
void App::captureFrame(int width, int height) {
  if (!clip_) {
    std::error_code ec;
    std::filesystem::create_directories(*options_.video, ec);
    const auto out = *options_.video / ("clip" + std::to_string(++clips_) + ".mp4");
    const std::string command = "ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgb24 -s " + std::to_string(width) +
                                "x" + std::to_string(height) + " -r 60 -i - -vf vflip -c:v libx264 -crf 16 " +
                                "-pix_fmt yuv420p \"" + out.string() + "\"";
#ifdef _WIN32
    clip_ = _popen(command.c_str(), "wb");
#else
    clip_ = popen(command.c_str(), "w");
#endif
    if (!clip_) {
      log::error("cannot start ffmpeg to film");
      running_ = false;
      return;
    }
    clipWidth_ = width;
    clipHeight_ = height;
    log::info("filming " + out.string());
  }
  if (width != clipWidth_ || height != clipHeight_) return;  // the window changed size: not this frame
  clipFrame_.resize(static_cast<std::size_t>(width) * height * 3);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, clipFrame_.data());
  std::fwrite(clipFrame_.data(), 1, clipFrame_.size(), clip_);
}

void App::endClip() {
#ifdef _WIN32
  _pclose(clip_);
#else
  pclose(clip_);
#endif
  clip_ = nullptr;
}

/// Every game is played on a table of its own, made as the key that starts it is pressed, so
/// that its recording depends on nothing played before it. The new table takes over what the
/// old one had that the game would play differently without: the options and high scores as
/// they are now, any cheats typed while it waited, and where its screen was looking.
void App::newGame() {
  const int index = tableIndex_;
  encore::TableGame::Setup setup;
  setup.options = config_.options;
  setup.highScores = config_.highScores[static_cast<std::size_t>(index)];
  setup.carry = table_->carryOver();
  if (fromReplay_) {
    // After a recording, one's own options and high scores, and none of its cheats.
    setup.carry.noTilt = setup.carry.otherSteps = false;
    setup.carry.balls = 0;
    fromReplay_ = false;
  } else {
    setup.options = table_->options();
    setup.highScores = table_->highScores();
  }
  setup.seed = static_cast<u64>(std::chrono::steady_clock::now().time_since_epoch().count());
  audio_.setSource({});
  table_ = std::make_unique<encore::TableGame>(tablePrg_, tableMod_, index, setup);
  showLooks();
  audio_.setSource([t = table_.get()](float* out, int frames) { t->sound(out, frames); });
  recordingSaved_ = false;
}

/// The game just over, kept beside the high scores in replays/, named by Replay::fileName.
void App::saveRecording() {
  const std::time_t now = std::time(nullptr);
  char stamp[32];
  std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M", std::localtime(&now));
  const encore::Recording& r = table_->recording();
  auto path = saveDir_ / "replays" / r.fileName(stamp);
  // Two games ending in the same minute with the same score keep both.
  for (int n = 2; present(path); ++n)
    path = saveDir_ / "replays" / r.fileName(std::string(stamp) + "-" + std::to_string(n));
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (!file::writeAll(path, r.save())) {
    log::error("cannot write " + path.string());
    return;
  }
  log::info("recorded: " + path.string());
  if (table_->sendOnline() && sender_) {
    sender_->queue(path);
    sender_->send();
  }
}

/// What belongs to the window rather than to the game, on every screen, the questions before the
/// game included: fullscreen on F11 (and Command+F on a Mac, as Windows keeps Windows+F for
/// itself), and fullscreen remembered for next time however it came about -- the key, or the
/// system's own button on the title bar. True when the event was one of these.
bool App::windowEvent(const SDL_Event& e) {
  if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat &&
      (e.key.key == SDLK_F11 || (e.key.key == SDLK_F && (e.key.mod & SDL_KMOD_GUI)))) {
    window_.setFullscreen(!window_.fullscreen());
    return true;
  }
  if (e.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN || e.type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN) {
    const bool on = e.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN;
    if (window_.fullscreen() != on) window_.setFullscreen(on);
    const char c = on ? '1' : '0';
    file::writeAll(saveDir_ / "fullscreen.txt", ByteView(reinterpret_cast<const u8*>(&c), 1));
    return true;
  }
  return false;
}

void App::handleKey(const SDL_Event& e) {
  if (e.type != SDL_EVENT_KEY_DOWN && e.type != SDL_EVENT_KEY_UP) return;
  if (e.key.repeat) return;
  if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F9) {
    setCrt(!renderer_.crt());
    return;
  }
  if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F10) {
    if (renderer_.hasHdPictures()) setHd(!renderer_.hdEnabled());
    return;
  }
  // and the sound's, beside them: 0, which nothing in the game uses
  if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_0) {
    setBalancedSound(!balancedSound_);
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
  if (table_ && replaying_) {
    // The recording plays the table; Escape stops it and goes back to the menu.
    if (down && k == Key::Escape) openIntro(tableIndex_);
    return;
  }
  if (table_ && down && table_->startsGame(k)) newGame();
  if (table_) table_->key(k, down);
  else if (intro_) intro_->key(k, down);
}

void App::update(double dt) {
  clock_ += dt;
  while (clock_ >= kFrame) {
    clock_ -= kFrame;
    if (intro_) {
      using Kind = encore::Front::Action::Kind;
      const encore::Front::Action a = intro_->frame();
      // (without a sound card nothing else moves the music on, and the pictures wait for it)
      if (!sound_) intro_->noSound();
      switch (a.kind) {
        case Kind::OpenTable:
          config_.options = intro_->options();
          openTable(a.table);
          break;
        case Kind::SaveOptions:
          config_.options = intro_->options();
          Config::saveOptions(saveDir_, config_.options);
          // and this version's two, as the page left them
          if (config_.options.originalSound == balancedSound_) setBalancedSound(!config_.options.originalSound);
          if (config_.options.originalPictures == renderer_.hdEnabled()) setHd(!config_.options.originalPictures);
          break;
        case Kind::Quit: running_ = false; return;
        case Kind::None: break;
      }
    } else if (table_) {
      if (replaying_) {
        const auto& events = replay_->events;
        for (; replayNext_ < events.size() && events[replayNext_].frame == replayFrame_; ++replayNext_)
          if (events[replayNext_].isKey()) table_->key(events[replayNext_].key(), events[replayNext_].down());
      }
      // the moment this frame was due, which its sounds keep to however late it is run
      table_->stampSound(now_ - clock_);
      table_->frame();
      if (replaying_) {
        ++replayFrame_;
        const bool filmed =
            options_.video && replayFrame_ >= static_cast<u32>((options_.videoFrom + options_.videoSeconds) * 60);
        if ((replayFrame_ >= replay_->frames || filmed) && recordingOver()) return;
      }
      if (!sound_) table_->noSound();
      if (!recordingSaved_ && !table_->recording().games.empty()) {
        saveRecording();
        recordingSaved_ = true;
      }
      const int index = tableIndex_;
      // A recording's table has the recording's options and high scores: none are kept.
      if (table_->optionsChanged() && !fromReplay_) {
        config_.options = table_->options();
        showLooks();  // (the table's copy is from when it opened)
        Config::saveOptions(saveDir_, config_.options);
      }
      if (table_->highScoresChanged() && !fromReplay_) {
        config_.highScores[static_cast<std::size_t>(index)] = table_->highScores();
        Config::saveHighScores(saveDir_, index, table_->highScores());
      }
      if (table_->left()) {
        if (!fromReplay_) config_.options = table_->options();
        showLooks();
        openIntro(index);
        return;
      }
      if (table_) resizeFrame(320, table_->screenHeight(), tablePixelAspect(*table_), encore::TableScreen::kDisplayRows);
    }
  }
}

/// The offer of the pictures, put in the game's own window rather than in a box of the
/// system's: two lines and a choice, answered with the arrow keys and enter, or with Y and
/// N, or turned down with escape.
bool App::askToDownload() {
  return askYesNo({"YOU LEGALLY OWN", "PINBALL FANTASIES", "TO PLAY ENCORE?"});
}

/// A question in the intro's letters, a line or more of it, with YES and NO under it: the
/// arrows or Tab move between them, Enter or Space takes the one chosen, Y and N answer at
/// once, Escape (or closing the window) is no.
bool App::askYesNo(std::span<const std::string_view> lines) {
  bool yes = true;
  for (;;) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) return false;
      if (windowEvent(event) || event.type != SDL_EVENT_KEY_DOWN) continue;
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
    // A short question sits a little above the middle; a long one is centred, to fit.
    const int lineH = kLetterH + 6;
    const int top = lines.size() <= 3 ? frame_.height() / 2 - 3 * kLetterH
                                      : (frame_.height() - static_cast<int>(lines.size() + 2) * lineH) / 2;
    int y = top;
    for (const auto line : lines) {
      putTextCentred(frame_, askFont_, line, y);
      y += kLetterH + 6;
    }
    const int row = y + kLetterH + 6;
    const int left = (frame_.width() - 11 * kLetterW) / 2;
    putText(frame_, askFont_, "YES", left, row, yes ? 0x20 : 0x30);
    putText(frame_, askFont_, "NO", left + 7 * kLetterW, row, yes ? 0x30 : 0x20);
    putArrow(frame_, (yes ? left : left + 7 * kLetterW) - kLetterW - 6, row + kLetterH / 2 - 10, 1);
    int w = 0, h = 0;
    window_.drawableSize(w, h);
    renderer_.setPalette(palette_);
    renderer_.draw(frame_, w, h);
    window_.swap();
    SDL_Delay(16);
  }
}

/// While a set of pictures is being fetched: a line of text and a ring of dots turning, in
/// the game's own window, so the wait does not look like a hang. The game has drawn nothing
/// yet at this point, so the palette is this drawing's own.
void App::drawWaiting(double seconds, std::string_view line, std::string_view detail) {
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
  if (!detail.empty()) putTextCentred(frame_, askFont_, detail, cy + static_cast<int>(kRadius) + kLetterH);
  int w = 0, h = 0;
  window_.drawableSize(w, h);
  renderer_.setPalette(palette_);
  renderer_.draw(frame_, w, h);
  window_.swap();
}

namespace {

/// Bytes as whole megabytes for the screens, rounded up: 1 MB at the least.
std::string megabytes(u64 bytes) { return std::to_string(std::max<u64>(1, (bytes + 1048575) / 1048576)) + " MB"; }

}  // namespace

/// The HD pictures: the set fetched before, if there is one, and the server's newer set when
/// there is one and the player wants it. The first time, nothing is there and the question is
/// whether to fetch the pictures at all; after that, whether to fetch the new version. Turned
/// down, a version is not offered again. False only when the window was closed meanwhile.
bool App::offerArt() {
  if (options_.hdDir) return true;
  auto have = installedArt(saveDir_);
  if (have) artDir_ = have->dir;
  if (!artCheck_.valid() || askFont_.width == 0) return true;

  // Usually the answer is in already; if not, it is waited for a little, and otherwise asked
  // for again at the next start.
  const auto start = std::chrono::steady_clock::now();
  while (artCheck_.wait_for(std::chrono::milliseconds(16)) != std::future_status::ready) {
    const double waited = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    if (waited > 3.0) {
      log::info("HD pictures: the server is slow to answer; asking again next time");
      return true;
    }
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) return false;
      windowEvent(event);
    }
    if (waited > 0.3) drawWaiting(waited, "LOOKING FOR REMASTERED ARTWORK");
  }
  const auto set = artCheck_.get();
  if (!set) return true;
  if (set->format > kArtFormat) {
    log::info("HD pictures: version " + std::to_string(set->version) + " needs a newer game");
    return true;
  }
  if (have && have->set.version >= set->version) return true;

  // A version with nothing new to download (a picture taken out, say) is not worth a question:
  // it is put in place from the pictures there are.
  const u64 bytes = artBytesToFetch(*set, have);
  if (bytes == 0) {
    ArtProgress progress;
    std::string error;
    if (fetchArt(saveDir_, *set, have, progress, &error)) {
      if (auto now = installedArt(saveDir_)) artDir_ = now->dir;
    } else {
      log::error("HD pictures: version " + std::to_string(set->version) + " not put in place: " + error);
    }
    return true;
  }
  if (declinedArt(saveDir_) >= set->version) return true;

  const bool wanted = have ? askYesNo({"UPDATE THE", "REMASTERED ARTWORK?", megabytes(bytes)})
                           : askYesNo({"DOWNLOAD THE", "REMASTERED ARTWORK?", megabytes(bytes)});
  if (!wanted) {
    declineArt(saveDir_, set->version);
    log::info("HD pictures: version " + std::to_string(set->version) + " turned down");
    return true;
  }

  // Fetched in the background while the screen shows how far it has got; Escape stops it, and
  // the pictures there were before stay.
  ArtProgress progress;
  progress.total = bytes;
  std::string error;
  bool fetched = false;
  std::atomic<bool> finished{false};
  std::thread worker([&] {
    fetched = fetchArt(saveDir_, *set, have, progress, &error);
    finished = true;
  });
  bool closed = false;
  const auto began = std::chrono::steady_clock::now();
  while (!finished) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) closed = true;
      if (windowEvent(event)) continue;
      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) progress.cancel = true;
      if (closed) progress.cancel = true;
    }
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
    const u64 done = progress.done;
    drawWaiting(seconds, "DOWNLOADING REMASTERED ARTWORK",
                progress.cancel ? std::string("STOPPING") : (std::to_string(done / 1048576) + " OF " + megabytes(bytes)));
    SDL_Delay(16);
  }
  worker.join();
  if (closed) return false;
  if (!fetched) {
    log::error("HD pictures: version " + std::to_string(set->version) + " not fetched: " + error);
    return true;
  }
  if (auto now = installedArt(saveDir_)) artDir_ = now->dir;
  return true;
}

/// A release newer than this one: what it brings, and whether to open its page. Asked once for
/// each version, whatever the answer; the game does not update itself. False when the page was
/// opened, which ends the game, so that the new version can be put in its place.
bool App::offerRelease() {
  if (!releaseCheck_.valid() || askFont_.width == 0) return true;
  // The question about the pictures has usually given it all the time it needs.
  if (releaseCheck_.wait_for(std::chrono::seconds(1)) != std::future_status::ready) return true;
  const auto release = releaseCheck_.get();
  if (!release || !newerVersion(release->version, thisRelease())) return true;
  const std::string offered = offeredRelease(saveDir_);
  if (!offered.empty() && !newerVersion(release->version, offered)) return true;

  std::vector<std::string> lines{"NEW VERSION " + release->version};
  if (!release->lines.empty()) {
    lines.emplace_back();
    lines.insert(lines.end(), release->lines.begin(), release->lines.end());
  }
  lines.emplace_back();
  lines.emplace_back("OPEN THE DOWNLOAD PAGE?");
  std::vector<std::string_view> views(lines.begin(), lines.end());
  const bool open = askYesNo(views);
  rememberOffered(saveDir_, release->version);
  log::info("new release " + release->version + (open ? ": opening its page, and ending" : ": not now"));
  if (!open) return true;
  // If no browser could be asked to open it, the game carries on rather than just vanishing.
  if (!SDL_OpenURL(release->url.c_str())) {
    log::error(std::string("cannot open the page: ") + SDL_GetError());
    return true;
  }
  return false;
}

void App::render() {
  std::array<Rgb, 256> colors{};
  // Only when the replacements will really be drawn: the table leaves the flippers out of the
  // frame for the renderer to put back, so with them off it must draw everything itself.
  HdFrame* const hd = renderer_.hasHdPictures() && renderer_.hdEnabled() ? &hd_ : nullptr;
  hd_.ownSprites = ownFlipperPictures_;
  hd_.ballTrail = ballTrail_;
  // The frame drawn into is as big as the screen is now, not as it was after the last frame
  // of the game: a resolution changed in the pause menu takes effect at once, and with the
  // display faster than the game the screen is drawn again before another frame has run.
  if (table_)
    resizeFrame(320, table_->screenHeight(), tablePixelAspect(*table_), encore::TableScreen::kDisplayRows);
  else if (intro_)
    resizeFrame(encore::Front::kWidth, intro_->height(), 1.0);
  if (table_) {
    table_->ballTrail = ballTrail_;
    table_->draw(frame_.data(), colors.data(), hd);
  }
  else if (intro_)
    intro_->draw(frame_.data(), colors.data(), hd);
  palette_.set(0, std::vector<Rgb>(colors.begin(), colors.end()));
  int w = 0, h = 0;
  window_.drawableSize(w, h);
  renderer_.setPalette(palette_);
  const auto beforeDraw = std::chrono::steady_clock::now();
  renderer_.draw(frame_, w, h, &hd_);
  if (options_.video && replaying_ && replayFrame_ >= static_cast<u32>(options_.videoFrom * 60)) captureFrame(w, h);
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
        if (windowEvent(e)) continue;
        // Nothing in the game is played with the mouse, so the pointer keeps out of the way
        // while it is over the window, and comes back when it leaves or the window does. Moving
        // over the window hides it too, for when it was already there at the start.
        if (e.type == SDL_EVENT_WINDOW_MOUSE_ENTER || e.type == SDL_EVENT_MOUSE_MOTION ||
            (e.type == SDL_EVENT_WINDOW_FOCUS_GAINED && SDL_GetMouseFocus() == window_.handle()))
          window_.hidePointer(true);
        if (e.type == SDL_EVENT_WINDOW_MOUSE_LEAVE || e.type == SDL_EVENT_WINDOW_FOCUS_LOST) window_.hidePointer(false);
        if (e.type == SDL_EVENT_DROP_FILE && e.drop.data) openReplay(e.drop.data);
        handleKey(e);
      }
      const auto nowT = clock::now();
      // Filming, every drawn frame is one of the game's, however long it takes; and what comes
      // before the part filmed is played through without being drawn.
      const double dt = options_.video ? kFrame : std::min(0.1, std::chrono::duration<double>(nowT - last).count());
      last = nowT;
      now_ = std::chrono::duration<double>(nowT.time_since_epoch()).count();
      const auto beforeUpdate = clock::now();
      if (options_.video)
        while (running_ && replaying_ && replayFrame_ < static_cast<u32>(options_.videoFrom * 60)) update(kFrame);
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
      render();
    }
  } catch (const std::exception& e) {
    // Whatever went wrong, it is said out loud rather than ending the program in silence.
    log::error(e.what());
    reportError(e.what());
    audio_.close();
    SDL_Quit();
    return 1;
  }
  if (clip_) endClip();
  audio_.setSource({});
  audio_.close();
  SDL_Quit();
  return 0;
}

}  // namespace encore
