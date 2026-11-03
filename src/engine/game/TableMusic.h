#pragma once
// Between a table and its music. The music is played by the sound card's clock, on a thread
// of its own (engine/audio/MusicDriver.h), and the table by the screen's; and the table goes
// by the music: a jingle that ends lets the next thing happen. So that a game can still be
// played again exactly, the table never looks at the music as it is, but at a view of it:
// the music as it was when the frame began, and what the table itself has done to it since.
// Whenever the view has moved on by itself between two frames, a recording notes the new one
// (Recording::Event::Kind::Music); a game played from a recording is given those instead, and
// sees the music exactly as the game that was played did, whatever the card was doing.
//
// The view is what the original keeps of its music (cs:3a6a, cs:5c3f): the place in the song,
// whether it has just been told to go there, how often the jingle is still to be repeated,
// its priority, the place to go back to after it, and whether the music is switched off.
// All but the first two are in the table's own memory, where its rules read and write them.
#include <atomic>

#include "engine/Values.h"
#include "engine/audio/MusicDriver.h"
#include "engine/table/Engine.h"

namespace encore {

class TableMusic final : public SoundDriver, public Conductor {
 public:
  TableMusic(Engine& engine, MusicDriver& driver);

  // --- the table's side
  /// (The table's effects are played a semitone above where the driver would put them: the
  /// pitch this version has had them at from the start.)
  static constexpr int kEffectsRaised = 1;
  void effect(u8 sample, u8 note, u8 volume, u8 channel) override {
    driver_.effect(sample, static_cast<u8>(note + kEffectsRaised), volume, channel);
  }
  u8 jump(u16 position) override;
  void volume(u16 level) override { driver_.volume(level); }
  void stop() override { driver_.stop(); }
  u8 start() override { return driver_.start(); }
  u8 status() override { return driver_.status(); }

  /// The view, packed into a word: the place (7 bits), told to go there (1), repeats (8),
  /// priority (8), the place to go back to (7), music off (1).
  u32 view() const;
  /// The view as a recording says it was.
  void setView(u32 packed);
  /// The music as it now is, taken into the view.
  void take() { setView(state_.load(std::memory_order_acquire)); }
  /// What the table has done to its view since it was last taken or given, done to the music.
  void give();

  // --- the music's side (Conductor)
  int interrupt() override;
  u8 next(u8 following) override;
  u8 jump(u8 place) override;

 private:
  struct State {
    // where each part is in the word
    static constexpr int kInterrupt = 0x80;
    static constexpr u32 kOff = 0x80000000u;
    u8 position = 0;
    bool interrupt = false;
    u8 repeat = 0, priority = 0, music = 0;
    bool off = false;
    State() = default;
    explicit State(u32 v)
        : position(static_cast<u8>(v & 0x7f)), interrupt((v & kInterrupt) != 0), repeat(static_cast<u8>(v >> 8)),
          priority(static_cast<u8>(v >> 16)), music(static_cast<u8>(v >> 24 & 0x7f)), off((v & kOff) != 0) {}
    u32 pack() const {
      return u32{position} | u32{interrupt} << 7 | u32{repeat} << 8 | u32{priority} << 16 |
             u32{static_cast<u8>(music & 0x7f)} << 24 | u32{off} << 31;
    }
  };

  Engine& engine_;
  MusicDriver& driver_;
  u8 position_ = 0;         ///< the view's place
  bool interrupt_ = false;  ///< and whether the music has yet to go there
  bool told_ = false;       ///< the table has sent the music somewhere since the view was last given
  u32 last_ = 0;            ///< the view when it was last taken or given
  std::atomic<u32> state_{0};
  std::atomic<u8> after_{0};  ///< the priority left when a jingle ends (ds:230c)
  u8 lastOfMusic_, silence_;  ///< with the music off, a place up to the one is played as the other (cs:3aac)
};

}  // namespace encore
