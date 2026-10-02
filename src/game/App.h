#pragma once
// Application shell: owns the window, renderer and audio, and runs either the intro/menu
// screen or a table, both at the original's 60 frames a second.
#include <filesystem>
#include <memory>
#include <optional>

#include "data/OpenGame.h"
#include "game/Config.h"
#include "gfx/Framebuffer.h"
#include "gfx/Palette.h"
#include "gfx/Renderer.h"
#include "intro/Intro.h"
#include "platform/AudioDevice.h"
#include "platform/Window.h"
#include "table/Table.h"

union SDL_Event;

namespace pfr {

struct AppOptions {
  std::optional<std::filesystem::path> dataDir;
  int table = 0;           ///< open a table directly (1..4), or 0 to start with the intro
  bool fullscreen = false;
  bool skipIntro = false;
  bool squarePixels = false;  ///< show the picture unstretched instead of the original 4:3
  bool smoothEdges = false;   ///< soften the one pixel that straddles two source pixels
  std::optional<Resolution> resolution;  ///< overrides the saved screen mode
  std::optional<bool> crt;               ///< overrides the saved CRT look
  std::optional<bool> hd;                ///< overrides the saved choice of replacement pictures
  std::optional<bool> trail;             ///< overrides the saved choice of the ball's trail
  std::optional<std::filesystem::path> hdDir;  ///< where replacement pictures are read from
  int windowScale = 3;
  bool stats = false;                    ///< log how long each frame takes
  std::optional<std::filesystem::path> screenshot;  ///< render one frame, save it, quit
  int screenshotFrame = 30;
};

/// The letters the screens before the game are written with. They are the intro's own,
/// kept as a picture in assets/hd (font.png, 20 cells of 32 x 14 per row) so that they can
/// be read before any file of the original game is, or even found.
struct AskFont {
  int width = 0, height = 0;
  Bytes index;              ///< one colour slot per pixel
  std::vector<Rgb> colors;  ///< what each slot looks like; slot 0 fills a letter's cell
};

class App {
 public:
  explicit App(AppOptions options);
  int run();

 private:
  bool init();
  void update(double dt);
  void render(double now);
  bool askToDownload();
  void drawWaiting(double seconds, std::string_view line);
  void openIntro(int returningFrom);
  void openTable(int index);
  void handleKey(const SDL_Event& e);
  void resizeFrame(int width, int height, double pixelAspect);
  void setCrt(bool on);
  void loadHdPictures();
  void loadFlipperPictures(int table);
  std::filesystem::path hdPicturePath(const std::string& name) const;
  void setHd(bool on);
  void setBallTrail(bool on);

  AppOptions options_;
  std::filesystem::path shaderDir_, saveDir_;
  OpenGame game_;  ///< the only game data read: the open format, converted once from the DOS files
  Config config_;
  Window window_;
  Renderer renderer_;
  AudioDevice audio_;
  Framebuffer frame_;
  AskFont askFont_;  ///< the letters for the screens shown before the game
  Palette palette_;
  HdFrame hd_;
  u8 ownFlipperPictures_ = 0;  ///< bit per flipper with a picture of its own
  bool ballTrail_ = true;      ///< the fading ghosts behind the ball
  std::unique_ptr<Intro> intro_;
  std::unique_ptr<Table> table_;
  struct Stats {
    double update = 0, draw = 0, wait = 0, worst = 0, seconds = 0;
    int frames = 0;
  } stats_;
  double clock_ = 0;
  int frameCounter_ = 0;
  bool running_ = true;
};

}  // namespace pfr
