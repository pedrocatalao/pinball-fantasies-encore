#pragma once
// One game as it was played, from the key that started it to its game over: enough to play
// it again, with no window and no sound, and arrive at the same score.
//
// Every game is played on a table of its own, made afresh when the game is started, so it
// depends on nothing played before it. It is the same every time given the same start (the
// table, what its chance was begun from, the options, the table's high scores and what it
// took over from the table it was started on), the same keys at the same frames, and the
// music in the same state at the same frames. The music is the one thing played on the side,
// by the sound card's clock, as it was on the machines the game was made for: so what the
// game saw of it at the start of each frame is recorded too, whenever it had moved on by
// itself (engine/game/TableMusic.h).
#include <array>
#include <optional>
#include <string>
#include <vector>

#include "core/Bcd.h"
#include "core/Keys.h"
#include "core/Types.h"
#include "game/Config.h"

namespace encore {

struct Recording {
  /// The tables as recordings are named after them, eight letters each.
  static constexpr const char* kTableCodes[4] = {"PARTYLND", "SPDDEVLS", "GAMESHOW", "STONBONE"};

  /// Raised whenever a recording would no longer play back the same: a change in the file's
  /// layout, or in how the tables play.
  static constexpr u16 kFormat = 3;

  struct Event {
    enum class Kind : u8 { KeyDown, KeyUp, Music };
    u32 frame = 0;  ///< before this frame for keys, at its start for the music
    Kind kind = Kind::KeyDown;
    u32 value = 0;  ///< a Key, or the music's state as TableMusic packs it
    bool isKey() const { return kind != Kind::Music; }
    Key key() const { return static_cast<Key>(value); }
    bool down() const { return kind == Kind::KeyDown; }
    bool operator==(const Event&) const = default;
  };
  /// What a game takes over from the table it was started on, beyond the options: cheats
  /// typed while that table waited, and where its screen was looking.
  struct Carry {
    bool noTilt = false;
    bool otherSteps = false;  ///< the cheat that has the ball move at the other screen mode's pace
    u8 balls = 0;  ///< as the options or the balls cheat have it; 0: as the options have it
    u16 scrollPos = 0xffff;  ///< the first row the waiting table's screen showed; 0xffff: where a table opens
    u16 scrollAt = 0;        ///< the same in sixteenths of a row, as it was being followed
    bool operator==(const Carry&) const = default;
  };
  /// The game as it ended: the frame of its game over and each player's score.
  struct Game {
    u32 endFrame = 0;
    bool abandoned = false;  ///< quit from the pause menu before the end
    std::vector<Bcd> scores;
    std::array<u8, 3> initials{};  ///< typed for a high score by the first player; zeros if none
    bool operator==(const Game&) const = default;
  };

  int table = 0;
  u64 seed = 0;  ///< what the table's chance is begun from
  Options options;  ///< as the table opened; changed in the pause menu by keys, which are here
                    ///< (all but the size of screen, which is the viewer's: Normal here)
  HighScores highScores;  ///< the table's, which decide whether a game ends asking for a name
  Carry carry;
  u32 frames = 0;
  std::vector<Event> events;
  std::vector<Game> games;  ///< the one game, once it is over

  /// Played with a cheat that makes it easier: no tilt, another pace, or more balls than the
  /// options give. (Fewer, as "fair play" can leave, is only harder.)
  bool cheated() const { return carry.noTilt || carry.otherSteps || carry.balls > options.balls; }

  Bytes save() const;
  static std::optional<Recording> load(ByteView data);
  /// What a recording is called: FANTASY-<table>-<initials>-[<tag>-]<score>-<when>.RPL, as
  /// FANTASY-STONBONE-RDX-67108120-20261002-2153.RPL. Initials not typed are ---, a space in
  /// them is _, and the score is the first player's. `tag` is the server's, for its own.
  std::string fileName(const std::string& when, const std::string& tag = {}) const;
};

/// How a game played again was really played, as the table had it (TableGame::playedWithCheats,
/// gentlestAngle), whatever the recording's header says.
struct HowPlayed {
  bool cheats = false;
  int angle = -1;  ///< the gentlest it was played at, 0 low to 2 higher; -1 if no game started
};

/// Plays a recording again from the table's files, and returns what that recorded: for a
/// faithful recording, the same events, game and score; and, if asked, how it was played.
Recording replay(ByteView prg, ByteView module, const Recording& recording, HowPlayed* how = nullptr);

/// What a server will take.
struct VerifyLimits {
  u32 maxFrames = 60 * 60 * 60 * 3;  ///< three hours
  std::size_t maxEvents = 2'000'000;
};

/// What a server makes of a recording: one whole game, played to its end without cheats, that
/// it can play again. The game and its score are the ones the replay arrives at, not the ones
/// the recording claims.
struct Verdict {
  bool ok = false;
  std::string reason;  ///< why not, when not
  Recording replayed;     ///< as played again: its games and scores are the ones to go by
  bool claimsMatch = false;  ///< the recording's own games and scores came out the same
  /// The gentlest angle the game was played at (0 low to 2 higher): a game can be paused and
  /// the angle changed, and it counts as played at the gentlest it had.
  int angle = -1;
};
Verdict verify(ByteView prg, ByteView module, const Recording& recording, const VerifyLimits& limits = {});

}  // namespace encore
