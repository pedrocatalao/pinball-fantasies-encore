#pragma once
// A table as the game plays it: the table's program written again (engine/table), the sound
// driver it talks to (engine/audio) and the picture of it (engine/view), put together and
// given keys. Everything that happens on it follows from how it was set up and from the keys
// it was given at each frame, so a game played on it can be played again (Recording.h).
//
// It also does what this version adds to a table: the options changed while the game is
// paused, the steeper angle, the whole table on one screen, the lamps all lit or all out to
// look at the artwork, and the question whether a best score is to be sent online.
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/Keys.h"
#include "engine/audio/MusicDriver.h"
#include "engine/game/Recording.h"
#include "engine/game/TableMusic.h"
#include "engine/table/Engine.h"
#include "engine/view/TableScreen.h"
#include "game/Config.h"

namespace encore {

class TableGame {
 public:
  struct Setup {
    Options options;
    HighScores highScores{};
    /// What the table's chance is begun from (Engine::seedChance).
    u64 seed = 0;
    Recording::Carry carry;
    bool picture = true;  ///< false for a table nobody will look at
  };

  /// `prg` and `module` are the bytes of TABLEn.PRG and TABLEn.MOD; `table` 0 to 3.
  TableGame(ByteView prg, ByteView module, int table, const Setup& setup);
  ~TableGame();

  int table() const { return engine_->table(); }
  void key(Key key, bool down);
  /// One frame of the game: a sixtieth of a second of it.
  void frame();
  /// The music is taken from this recording, frame by frame, and not from the sound card's
  /// playing of it: for a game played again. None: the card's again.
  void playBack(const Recording* recording) { playback_ = recording; }
  u32 frames() const { return frames_; }

  /// No game is being played: the table waits for one.
  bool waiting() const { return engine_->B(0x3713) == 0xff; }
  /// Whether this key, pressed now, would start a game.
  bool startsGame(Key key) const;
  bool paused() const { return engine_->isPaused(); }
  /// The table was left: back to the menu.
  bool left() const { return (engine_->exited() && leaving_ <= 0) || !failure_.empty(); }
  /// What went wrong, if the table stopped because something did.
  const std::string& failure() const { return failure_; }
  int players() const { return engine_->B(0x3716); }
  int player() const { return engine_->B(0x371a); }
  int ball() const { return engine_->B(0x33dc); }
  Bcd score(int player) const;

  /// How the game this table recorded was really played, as the engine had it from its first
  /// frame to its last, whatever the recording's header says: whether a cheat was on (a word
  /// typed before the start), and the gentlest angle it was played at (the angle can be changed
  /// while paused). Until a game has started, no cheat and no angle (-1).
  bool playedWithCheats() const { return cheats_; }
  int gentlestAngle() const { return gentlest_; }

  /// The options as they now are, the ones changed while paused among them.
  Options options() const;
  HighScores highScores() const;
  /// True once after each change, for whoever keeps them.
  bool optionsChanged();
  bool highScoresChanged();
  /// What a game started from this table takes over from it.
  Recording::Carry carryOver() const;

  /// Everything since this table was made: for playing it again.
  const Recording& recording() const { return recording_; }
  /// The first player, asked after typing initials for a best score, wants the game sent.
  bool sendOnline() const { return sendOnline_; }
  bool askingOnline() const { return asking_; }
  /// Initials are being typed for a best score.
  bool askingName() const;

  /// The screen: 320 across and this many rows, each dot one of 256 colours.
  int screenHeight() const;
  /// The whole table on the screen at once, which then never scrolls (Full and Tall).
  bool wholeTable() const { return options_.resolution == Resolution::Full || options_.resolution == Resolution::Tall; }
  /// The rows of the table on it: how many, and the first.
  int viewRows() const;
  int viewTop() const;
  /// `hd`: also what the pictures drawn again at high resolution need (gfx/HdLayer.h).
  void draw(u8* frame, Rgb* colours, HdFrame* hd = nullptr) const;
  std::vector<Cutout> flipperPictures() const { return screen_ ? screen_->flipperPictures(*engine_) : std::vector<Cutout>{}; }
  std::vector<bool> flipperIsLeft() const { return screen_ ? screen_->flipperIsLeft(*engine_) : std::vector<bool>{}; }
  Cutout ballPicture() const { return screen_ ? screen_->ballPicture(*engine_) : Cutout{}; }
  bool ballTrail = true;
  /// The lamps as the game has them (0), all lit (1) or all out (2): for looking at the artwork.
  void showLamps(int how) { lamps_ = how; }

  /// The next of the sound, for the sound card (48000 a second, left and right), which moves
  /// the music on by asking; from its thread.
  void sound(float* out, int frames);
  /// With no sound card, a frame's worth of the music is played to nobody instead.
  void noSound() { music_.pass(1.0 / 60); }
  /// The moment the next frame belongs to, in seconds of the steady clock: its sounds are
  /// heard a fixed time after it (MusicDriver::stampTime).
  void stampSound(double seconds) { music_.stampTime(seconds); }
  bool silent = false;  ///< plays, but hands out silence

  Engine& engine() { return *engine_; }
  /// The music as the table sees it (TableMusic::view).
  u32 musicView() const { return link_->view(); }
  MusicDriver& music() { return music_; }

 private:
  struct Sight {
    bool waiting;
    int ballY;
    u16 follow, shown;  ///< the row the table says to follow instead of the ball (ds:3385), and where its own screen is (ds:2f02)
  };
  Sight look() const;
  void aim(const Sight& s);
  void follow(const Sight& s);
  void pausedKey(Key key);
  void note();
  void syncMusic();

  Options options_;
  MusicDriver music_;
  std::unique_ptr<Engine> engine_;
  std::unique_ptr<TableMusic> link_;  ///< what the table knows of its music
  const Recording* playback_ = nullptr;
  std::size_t playbackAt_ = 0;
  std::unique_ptr<TableScreen> screen_;
  u32 frames_ = 0;
  bool playing_ = false;
  bool cheats_ = false;  ///< playedWithCheats()
  int gentlest_ = -1;    ///< gentlestAngle()
  HighScores bestAtStart_{};
  Recording recording_;
  std::string failure_;
  // what this version adds
  Options saved_;             ///< as last told to whoever keeps them
  HighScores savedScores_{};
  bool optionsChanged_ = false, scoresChanged_ = false;
  bool asking_ = false, answered_ = false, sendOnline_ = false;
  u16 lastWait_ = 0;          ///< the display's wait a frame ago, to see the initials' end come
  int leaving_ = 0x100;       ///< the table being left: how bright it still is, of 256 (cs:3a11)
  int lamps_ = 0;             ///< 0 as the game has them, 1 all lit, 2 all out
  /// Where the screen looks: this version's own following of the ball, the same for every
  /// size of screen (the table's own, cs:4018, is for its one size and is left to the rules).
  struct Camera {
    i16 raw = 0;                ///< the first row shown, in sixteenths
    u16 pos = 0;                ///< and whole
    std::optional<int> said;    ///< a place the table said to look at instead of the ball
    bool up = true;             ///< which way the table drifts while nobody plays
  } camera_;
  int cameraTop() const { return TableData::kHeight - viewRows(); }  ///< the last row it can start at
  int cameraLead() const;      ///< how far down the screen the ball is kept
  int pauseFrames_ = 0;       ///< how long the pause's display has shown what it shows
  bool wasPaused_ = false;
  bool up_ = false, down_ = false;
  std::array<std::array<u8, 33 * 0x54>, 4> display_{};  ///< the display as the frame began
  u16 saidBefore_ = 0xffff;  ///< where the table said to look a frame ago, if it did
};

}  // namespace encore
