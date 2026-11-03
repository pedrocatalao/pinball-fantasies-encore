#include "engine/game/TableMusic.h"

#include <utility>

namespace encore {

TableMusic::TableMusic(Engine& engine, MusicDriver& driver)
    : engine_(engine), driver_(driver), lastOfMusic_(engine.kb(at::musicLastPlaceInsn, 1)), silence_(engine.kb(at::silencePlaceInsn, 1)) {}

/// Function 0x10 as the table sees it: the answer is the place the music was at.
u8 TableMusic::jump(u16 position) {
  const u8 was = position_;
  position_ = static_cast<u8>(position) & State::kPlaceBits;
  interrupt_ = true;
  told_ = true;
  return was;
}

u32 TableMusic::view() const {
  State s;
  s.position = position_;
  s.interrupt = interrupt_;
  s.repeat = engine_.B(at::musicRepeats);
  s.priority = engine_.B(at::musicPriority);
  s.music = engine_.B(at::musicNext);
  s.off = engine_.B(at::musicOff) != 0;
  return s.pack();
}

void TableMusic::setView(u32 packed) {
  const State s(packed);
  position_ = s.position;
  interrupt_ = s.interrupt;
  if (engine_.B(at::musicRepeats) != 0 && s.repeat == 0) {  // the jingle has ended (cs:3a6a)
    engine_.B(at::jingleEnded) = kSet;
    engine_.B(at::jingleEndedRules) = kSet;
  }
  engine_.B(at::musicRepeats) = s.repeat;
  engine_.B(at::musicPriority) = s.priority;
  engine_.B(at::musicNext) = s.music;
  if ((engine_.B(at::musicOff) != 0) != s.off) engine_.B(at::musicOff) = s.off ? kSet : 0;
  last_ = view();
}

void TableMusic::give() {
  after_.store(engine_.B(at::musicAfter), std::memory_order_relaxed);
  const State now(view()), was(last_);
  const bool told = std::exchange(told_, false);
  if (now.pack() == was.pack() && !told) return;
  last_ = now.pack();
  u32 value = state_.load(std::memory_order_acquire);
  for (;;) {
    State s(value);
    if (told) {
      s.position = now.position;
      s.interrupt = true;
    }
    if (now.repeat != was.repeat) s.repeat = now.repeat;
    if (now.priority != was.priority) s.priority = now.priority;
    if (now.music != was.music) s.music = now.music;
    if (now.off != was.off) s.off = now.off;
    if (state_.compare_exchange_weak(value, s.pack(), std::memory_order_release, std::memory_order_acquire)) return;
  }
}

int TableMusic::interrupt() {
  u32 value = state_.load(std::memory_order_acquire);
  for (;;) {
    State s(value);
    if (!s.interrupt) return -1;
    s.interrupt = false;
    if (state_.compare_exchange_weak(value, s.pack(), std::memory_order_release, std::memory_order_acquire)) return s.position;
  }
}

u8 TableMusic::next(u8 following) {
  u32 value = state_.load(std::memory_order_acquire);
  for (;;) {
    State s(value);
    if (s.interrupt) return following;  // it is about to be sent elsewhere anyway
    s.position = following & State::kPlaceBits;
    if (state_.compare_exchange_weak(value, s.pack(), std::memory_order_release, std::memory_order_acquire)) return following;
  }
}

/// cs:3a6a, on the music's own state.
u8 TableMusic::jump(u8 place) {
  u32 value = state_.load(std::memory_order_acquire);
  for (;;) {
    State s(value);
    if (s.interrupt) return place;
    u8 to = place;
    const u8 left = static_cast<u8>(s.repeat - 1);
    if (left == 0) {  // the jingle has run out: back to the music
      s.priority = after_.load(std::memory_order_relaxed);
      s.repeat = 0;
      to = s.music;
    } else if (left == 0xff || left == 0x7f) {  // below zero, as the original's signed test has it
      s.repeat = 0;
    } else {
      s.repeat = left;
    }
    if (s.off && to <= lastOfMusic_) to = silence_;
    s.position = to & State::kPlaceBits;
    if (state_.compare_exchange_weak(value, s.pack(), std::memory_order_release, std::memory_order_acquire)) return to;
  }
}

}  // namespace encore
