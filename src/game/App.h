#pragma once
// Application shell: owns the window, renderer and audio, and runs either the intro/menu
// screen or a table, both at the original's 60 frames a second.
#include <cstdio>
#include <filesystem>
#include <future>
#include <initializer_list>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "data/GameFiles.h"
#include "game/Art.h"
#include "game/Release.h"
#include "game/Config.h"
#include "game/Online.h"
#include "gfx/Framebuffer.h"
#include "gfx/Palette.h"
#include "gfx/Renderer.h"
#include "engine/game/Front.h"
#include "platform/AudioDevice.h"
#include "platform/Window.h"
#include "core/Keys.h"
#include "engine/game/TableGame.h"

union SDL_Event;

namespace encore {

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
  std::optional<std::filesystem::path> hdDir;  ///< where replacement pictures are read from, instead of the fetched set
  int windowScale = 3;
  bool stats = false;                    ///< log how long each frame takes
  std::optional<std::filesystem::path> screenshot;  ///< render one frame, save it, quit
  std::vector<std::filesystem::path> replays;  ///< recordings to play one after another, as dropped on the program
  /// Film the recordings instead, a clip of each, into this folder (ffmpeg makes the files).
  std::optional<std::filesystem::path> video;
  int videoFrom = 12, videoSeconds = 9;  ///< which seconds of each recording are filmed
  int screenshotFrame = 30;
};

/// The letters the screens before the game are written with. They are the intro's own,
/// kept as a picture beside the application (font.png, from assets/hd, 20 cells of 32 x 14 per
/// row) so that they can be read before any file of the original game is, or even found, and
/// before any HD picture is fetched. The fetched set carries them too, and its copy is preferred.
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
  bool askYesNo(std::span<const std::string_view> lines);
  bool askYesNo(std::initializer_list<std::string_view> lines) { return askYesNo(std::span(lines.begin(), lines.size())); }
  void drawWaiting(double seconds, std::string_view line, std::string_view detail = {});
  bool offerArt();
  bool offerRelease();
  void openIntro(int returningFrom);
  void openTable(int index, const encore::Recording* recording = nullptr);
  bool openReplay(const std::filesystem::path& path);
  bool recordingOver();
  void captureFrame(int width, int height);
  void endClip();
  void newGame();
  void saveRecording();
  bool windowEvent(const SDL_Event& e);
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
  std::filesystem::path artDir_;  ///< the fetched HD pictures in use, or empty for none
  std::future<std::optional<ArtSet>> artCheck_;  ///< the server's current set, being asked for
  std::future<std::optional<ReleaseInfo>> releaseCheck_;  ///< the newest release, being asked for
  GameFiles files_;
  Config config_;
  Window window_;
  Renderer renderer_;
  AudioDevice audio_;
  Framebuffer frame_;
  AskFont askFont_;  ///< the letters for the screens shown before the game
  Palette palette_;
  HdFrame hd_;
  std::array<int, 2> panelStrip_{};  ///< the size of the picture repeated down the tall menu's panel, if there is one
  u8 ownFlipperPictures_ = 0;  ///< bit per flipper with a picture of its own
  bool ballTrail_ = true;      ///< the fading ghosts behind the ball
  std::unique_ptr<encore::Front> intro_;
  std::unique_ptr<encore::TableGame> table_;
  int tableIndex_ = 0;  ///< which table is open
  struct Stats {
    double update = 0, draw = 0, wait = 0, worst = 0, seconds = 0;
    int frames = 0;
  } stats_;
  double clock_ = 0;
  double now_ = 0;  ///< when the frames being run were due, in seconds of the steady clock, as of now
  int frameCounter_ = 0;
  Bytes tablePrg_, tableMod_;  ///< the open table's files, for a table of its own per game
  bool recordingSaved_ = false;  ///< the open table's game has been kept
  std::unique_ptr<ScoreSender> sender_;
  // A recording being played: the table's keys come from it until its last frame.
  std::optional<encore::Recording> replay_;
  std::size_t replayNext_ = 0;  ///< its next event
  u32 replayFrame_ = 0;         ///< frames of it played
  bool replaying_ = false;
  bool fromReplay_ = false;     ///< the table is the recording's, until a game of one's own
  std::size_t nextReplay_ = 0;  ///< of AppOptions::replays
  // Filming (AppOptions::video): frames go to ffmpeg as they are drawn, a clip per recording.
  std::FILE* clip_ = nullptr;
  int clips_ = 0, clipWidth_ = 0, clipHeight_ = 0;
  std::vector<u8> clipFrame_;
  bool running_ = true;
  bool sound_ = false;            ///< a sound card is playing the music
};

}  // namespace encore
