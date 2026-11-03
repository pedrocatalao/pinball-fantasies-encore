#pragma once
// The referee's machine (docs/own-engine.md): just enough of a PC for one of the game's table
// programs to run as it did in 1994: memory, the video card, DOS's file and memory calls, the
// launcher's services (int 65h) and the sound driver's (int 66h). The sound driver is the
// game's own silent one, NOSOUND.SDR, done here by hand from its listing: it plays nothing and
// calls the table back at fixed points of every video frame, which is the table's clock.
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "Cpu.h"
#include "Vga.h"

namespace oracle {

/// The program's code segment, as its listing has it (relative to where it is loaded).
constexpr u16 kCodeSegment = 0x10;

/// The interrupts the machine answers, by number, and the keyboard's.
namespace vec {
constexpr u8 keyboard = 0x09, video = 0x10, terminate = 0x20, dos = 0x21, mouse = 0x33;
constexpr u8 launcher = 0x65, soundDriver = 0x66;  ///< PINBALL.EXE's services, and the sound driver's
}  // namespace vec

/// Keys as the keyboard says them: the code of a key going down, and with `released` set,
/// going up.
namespace scan {
constexpr u8 released = 0x80;
constexpr u8 extended = 0xe0;  ///< comes before the code of a key the first keyboards had not
constexpr u8 enter = 0x1c, enterUp = 0x9c;
constexpr u8 leftShift = 0x2a, leftShiftUp = 0xaa;  ///< the left flipper
constexpr u8 rightShift = 0x36, rightShiftUp = 0xb6;  ///< the right flipper
constexpr u8 space = 0x39, spaceUp = 0xb9;  ///< a nudge
constexpr u8 f1 = 0x3b;
constexpr u8 down = 0x50, downUp = 0xd0;  ///< the plunger (after `extended`, the arrow's)
}  // namespace scan

class Machine : public Bus {
 public:
  /// The options as PINBALL.CFG holds them: balls (0 three, 1 five), angle, scrolling,
  /// music off, resolution (0 normal, 1 high), mono.
  struct Config {
    u8 bytes[6] = {0, 0, 1, 0, 1, 0};
  };

  Machine(const std::filesystem::path& gameDir, int table, const Config& config);

  /// One video frame: the keys given since the last one, the driver's callbacks in the order
  /// of their scanlines, then the program's own loop for a while.
  void frame();
  /// Where the program's own loop begins (an offset in its code segment). Given this, the
  /// machine keeps to a fixed timetable, so that the same keys always give the same game:
  /// boot() runs the program's start-up to the first arrival there, with no callbacks; each
  /// frame() then ends when the loop has come round `loopsPerFrame` times.
  void setLoop(u16 codeSegment, u16 offset);
  void boot();
  int loopsPerFrame = 8;
  /// A key goes down or up, as the keyboard says it: a scancode, with bit 7 set for up.
  void key(u8 scancode) { keys_.push_back(scancode); }
  bool exited() const { return exited_; }

  u8 peek8(u16 seg, u16 off) const { return mem_[((u32{seg} << 4) + off) & 0xfffff]; }
  void poke8(u16 seg, u16 off, u8 v) { mem_[((u32{seg} << 4) + off) & 0xfffff] = v; }
  u16 peek16(u16 seg, u16 off) const { return static_cast<u16>(peek8(seg, off) | (peek8(seg, static_cast<u16>(off + 1)) << 8)); }
  /// A segment value of the program's as its listing has it (relative to where it is loaded).
  u16 seg(u16 relative) const { return static_cast<u16>(base_ + relative); }

  Cpu cpu{*this};
  Vga vga;
  bool log = false;            ///< every service call, to stderr
  static constexpr u32 kNowhere = 0xffffffff;
  u32 watchWrite = kNowhere;   ///< a place in memory: who writes there is said on stderr
  std::uint64_t loopBudget = 20000;  ///< instructions of the program's own loop per frame
  u32 frames = 0;

  // Bus
  u8 read(u32 address) override;
  void write(u32 address, u8 value) override;
  u8 portIn(u16 port) override;
  void portOut(u16 port, u8 value) override;
  bool interrupt(u8 number) override;

 private:
  /// What the sound driver's next answer is when nothing has set one.
  static constexpr u8 kNoStatus = 0xff;

  struct Callback {
    u16 scanline = 0;
    u8 priority = 0;
    u16 seg = 0, off = 0;
  };
  void dos();
  void driver();
  void launcher();
  void callBack(const Callback& c);
  std::string string0(u16 seg, u16 off) const;
  void setCarry(bool on) { cpu.setFlag(Cpu::CF, on); }

  std::filesystem::path dir_;
  int table_ = 0;
  Config config_;
  std::vector<u8> mem_;
  u16 base_ = 0;       ///< the segment the program's image is loaded at
  u16 nextFree_ = 0;   ///< for DOS's memory calls
  bool exited_ = false;
  std::vector<u8> keys_;
  u8 keyPort_ = 0;

  // files, by handle
  struct File {
    std::filesystem::path path;
    std::vector<u8> data;
    std::size_t at = 0;
    bool written = false;
  };
  std::map<u16, File> files_;
  u16 nextHandle_ = 5;

  // the sound driver's state
  bool callbacksOn_ = true;
  bool haveFrameCallback_ = false;
  Callback frameCallback_;
  std::vector<Callback> lineCallbacks_;  ///< sorted by scanline
  u16 musicSeg_ = 0, musicOff_ = 0;      ///< the callback given by int66::setMusicCallback
  u8 driverStatus_ = kNoStatus;          ///< what the next call returns in AL, once
  u32 ticks_ = 0;
  bool polling_ = false;
  u32 loopAt_ = 0;
  int loops_ = 0, loopLimit_ = 1 << 30;
  bool midTurn_ = false;
};

}  // namespace oracle
