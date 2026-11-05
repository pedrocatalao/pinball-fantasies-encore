#include "Machine.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace oracle {
namespace {

/// Where the program's prefix is, and its size in paragraphs: the program follows it.
constexpr u16 kPsp = 0x0800;
/// The video card's 64 KB at a000:0000.
constexpr int kVideoWindow = 0xa0000, kVideoWindowEnd = 0xb0000;
/// The first segment past the memory DOS gives out.
constexpr int kMemoryTop = 0xa000;
/// In the BIOS's notes: the port of the video card's registers (a word).
constexpr int kBiosCrtcPort = 0x463;
/// Where the vectors of the services answered here point: "int n; iret" at f000:(n * 4).
constexpr int kStubs = 0xf0000;

/// The header of a program (an .EXE, here a .PRG): where in it each word is.
namespace mz {
constexpr int lastPageBytes = 0x02, pages = 0x04, relocations = 0x06, headerParagraphs = 0x08;
constexpr int minExtraParagraphs = 0x0a, ss = 0x0e, sp = 0x10, ip = 0x14, cs = 0x16, relocationTable = 0x18;
constexpr int size = 0x1c;
}  // namespace mz

/// The flags a program starts with: interrupts on.
constexpr u16 kStartFlags = 0x202;

/// The PC's other ports the programs use: the keyboard's, the interrupt controllers'.
namespace pcPort {
constexpr u16 keyboardData = 0x60, systemControl = 0x61;
constexpr u16 pic1Command = 0x20, pic1Mask = 0x21, pic2Command = 0xa0, pic2Mask = 0xa1;
}  // namespace pcPort

/// int 10h's function (in AH) that sets a mode, and int 33h's (in AX) that asks for a mouse,
/// with its answer: none.
constexpr u8 kSetMode = 0x00;
constexpr u16 kMouseReset = 0x0000, kNoMouse = 0;

/// The launcher's services (int 65h), by AX (docs/engine-system.md).
namespace int65 {
constexpr u16 state = 0x0000;        ///< AH: first run; BL: the table, 1 to 4
constexpr u16 storeConfig = 0x0100;  ///< from ES:BX, the six bytes of PINBALL.CFG
constexpr u16 readConfig = 0x0200;   ///< into ES:BX
constexpr u16 readKey = 0x0012;      ///< AL: the last key, then cleared (none here)
constexpr u16 next = 0xffff;         ///< BL: the program to run next
}  // namespace int65

/// DOS's functions (int 21h, by AH), its errors, and where a seek counts from.
namespace dosFn {
constexpr u8 print = 0x09, setVector = 0x25, version = 0x30, getVector = 0x35;
constexpr u8 create = 0x3c, open = 0x3d, close = 0x3e, read = 0x3f, write = 0x40, seek = 0x42;
constexpr u8 allocate = 0x48, free = 0x49, resize = 0x4a, exec = 0x4b, exit = 0x4c;
}  // namespace dosFn
namespace dosError {
constexpr u16 fileNotFound = 2, badHandle = 6, notEnoughMemory = 8;
}  // namespace dosError
constexpr u8 kSeekFromStart = 0, kSeekFromHere = 1;
/// DOS 5.0, as function 0x30 says it: the major number in AL, the minor in AH.
constexpr u16 kDosVersion = 0x0005;

/// The sound driver's functions (int 66h, by AL), named as the engine's int66:: names them
/// (engine/audio/MusicDriver.h) where it has them.
namespace int66 {
constexpr u8 unload = 0x00, start = 0x04, poll = 0x08, position = 0x09;
constexpr u8 frameCallback = 0x0b, lineCallback = 0x0c, releaseTimer = 0x0d;
constexpr u8 setMusicCallback = 0x13, callMusicCallback = 0x14, status = 0x15;
constexpr u8 ticks = 0x16, resetTicks = 0x17, enableCallbacks = 0x18;
constexpr u8 setMasterVolume = 0x06, stop = 0x0f, jump = 0x10, playSfx = 0x11, load = 0x12;
}  // namespace int66
/// int66::start's answer: started; and int66::status's from the silent driver (bit 3: an effect
/// still sounds).
constexpr u8 kStarted = 0, kSilentStatus = 0x0a;

std::vector<u8> slurp(const std::filesystem::path& p) {
  std::ifstream f(p, std::ios::binary);
  if (!f) throw CpuError("cannot read " + p.string());
  return std::vector<u8>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

u16 le16(const std::vector<u8>& d, std::size_t at) { return static_cast<u16>(d[at] | (d[at + 1] << 8)); }

}  // namespace

Machine::Machine(const std::filesystem::path& gameDir, int table, const Config& config)
    : dir_(gameDir), table_(table), config_(config), mem_(0x100000, 0) {
  const auto file = slurp(dir_ / ("TABLE" + std::to_string(table + 1) + ".PRG"));
  if (file.size() < mz::size || file[0] != 'M' || file[1] != 'Z') throw CpuError("not a program");
  const std::size_t header = std::size_t{le16(file, mz::headerParagraphs)} * 16;
  const std::size_t pages = le16(file, mz::pages), last = le16(file, mz::lastPageBytes);
  const std::size_t size = pages * 512 - (last ? 512 - last : 0);
  base_ = kPsp + 0x10;
  std::memcpy(&mem_[std::size_t{base_} * 16], &file[header], size - header);
  for (int i = 0; i < le16(file, mz::relocations); ++i) {
    const std::size_t entry = le16(file, mz::relocationTable) + std::size_t(i) * 4;
    const std::size_t at = std::size_t{base_} * 16 + std::size_t{le16(file, entry + 2)} * 16 + le16(file, entry);
    const u16 v = static_cast<u16>(mem_[at] | (mem_[at + 1] << 8));
    mem_[at] = static_cast<u8>(v + base_);
    mem_[at + 1] = static_cast<u8>((v + base_) >> 8);
  }
  // What the program finds below itself: its prefix, and the BIOS's notes.
  mem_[std::size_t{kPsp} * 16] = op::intImm;
  mem_[std::size_t{kPsp} * 16 + 1] = vec::terminate;
  mem_[kBiosCrtcPort] = 0xd4;  // the video card's register port, 0x3d4 (vgaPort::crtcIndex)
  mem_[kBiosCrtcPort + 1] = 0x03;
  // The services answered here have vectors too, since the program calls some through them:
  // each points at "int n; iret".
  for (u8 n : {vec::video, vec::dos, vec::mouse, vec::launcher, vec::soundDriver}) {
    const u32 stub = kStubs + n * 4u;
    mem_[stub] = op::intImm;
    mem_[stub + 1] = n;
    mem_[stub + 2] = op::iret;
    mem_[n * 4u] = static_cast<u8>(n * 4u);
    mem_[n * 4u + 1] = static_cast<u8>((n * 4u) >> 8);
    mem_[n * 4u + 2] = 0x00;  // the segment, f000
    mem_[n * 4u + 3] = 0xf0;
  }
  nextFree_ = static_cast<u16>(base_ + (size - header + 15) / 16 + le16(file, mz::minExtraParagraphs) + 1);

  cpu.s[Cpu::DS] = cpu.s[Cpu::ES] = kPsp;
  cpu.s[Cpu::SS] = static_cast<u16>(base_ + le16(file, mz::ss));
  cpu.r[Cpu::SP] = le16(file, mz::sp);
  cpu.s[Cpu::CS] = static_cast<u16>(base_ + le16(file, mz::cs));
  cpu.ip = le16(file, mz::ip);
  cpu.flags = kStartFlags;
}

u8 Machine::read(u32 a) {
  a &= 0xfffff;
  if (a >= kVideoWindow && a < kVideoWindowEnd) return vga.read(a - kVideoWindow);
  return mem_[a];
}

void Machine::write(u32 a, u8 v) {
  a &= 0xfffff;
  if (a >= kVideoWindow && a < kVideoWindowEnd) vga.write(a - kVideoWindow, v);
  else {
    if (a == watchWrite) std::fprintf(stderr, "[write] %05x = %02x at %s (frame %u)\n", a, v, cpu.where().c_str(), frames);
    mem_[a] = v;
  }
}

u8 Machine::portIn(u16 port) {
  if (port >= vgaPort::first && port <= vgaPort::last) return vga.portIn(port);
  if (port == pcPort::keyboardData) return keyPort_;
  if (port == pcPort::systemControl) return 0;
  if (port == pcPort::pic1Mask || port == pcPort::pic2Mask) return 0;
  if (log) std::fprintf(stderr, "[port] in %04x at %s\n", port, cpu.where().c_str());
  return kOpenBus;
}

void Machine::portOut(u16 port, u8 v) {
  if (port >= vgaPort::first && port <= vgaPort::last) return vga.portOut(port, v);
  if (port == pcPort::pic1Command || port == pcPort::pic1Mask || port == pcPort::pic2Command || port == pcPort::pic2Mask || port == pcPort::systemControl) return;
  if (log) std::fprintf(stderr, "[port] out %04x = %02x at %s\n", port, v, cpu.where().c_str());
}

std::string Machine::string0(u16 seg, u16 off) const {
  std::string s;
  for (int i = 0; i < 128; ++i) {
    const u8 c = peek8(seg, static_cast<u16>(off + i));
    if (!c) break;
    s += static_cast<char>(c);
  }
  return s;
}

bool Machine::interrupt(u8 n) {
  switch (n) {
    case vec::video:
      if (log) std::fprintf(stderr, "[video] ax=%04x bx=%04x\n", cpu.r[Cpu::AX], cpu.r[Cpu::BX]);
      if (cpu.hi(Cpu::AX) == kSetMode) vga.setMode(cpu.lo(Cpu::AX));
      return true;
    case vec::dos: dos(); return true;
    case vec::mouse:  // no mouse
      if (cpu.r[Cpu::AX] == kMouseReset) cpu.r[Cpu::AX] = kNoMouse;
      return true;
    case vec::launcher: launcher(); return true;
    case vec::soundDriver: driver(); return true;
    default:
      // A vector the program set itself goes through memory.
      if (peek16(0, static_cast<u16>(n * 4)) || peek16(0, static_cast<u16>(n * 4 + 2))) return false;
      throw CpuError("interrupt " + std::to_string(n) + " with nothing to answer it, at " + cpu.where());
  }
}

void Machine::launcher() {
  const u16 ax = cpu.r[Cpu::AX];
  if (log) std::fprintf(stderr, "[launcher] ax=%04x bx=%04x\n", ax, cpu.r[Cpu::BX]);
  switch (ax) {
    case int65::state:
      cpu.setHi(Cpu::AX, 0);
      cpu.setLo(Cpu::BX, static_cast<u8>(table_ + 1));
      break;
    case int65::readConfig:
      for (int i = 0; i < 6; ++i) write(u32{cpu.s[Cpu::ES]} * 16 + static_cast<u16>(cpu.r[Cpu::BX] + i), config_.bytes[i]);
      break;
    case int65::storeConfig:
      for (int i = 0; i < 6; ++i) config_.bytes[i] = peek8(cpu.s[Cpu::ES], static_cast<u16>(cpu.r[Cpu::BX] + i));
      break;
    case int65::readKey: cpu.setLo(Cpu::AX, 0); break;
    case int65::next: break;
    default: throw CpuError("launcher service " + std::to_string(ax) + " at " + cpu.where());
  }
}

void Machine::dos() {
  const u8 ah = cpu.hi(Cpu::AX);
  if (log) std::fprintf(stderr, "[dos] ax=%04x bx=%04x cx=%04x dx=%04x at %s\n", cpu.r[Cpu::AX], cpu.r[Cpu::BX],
                        cpu.r[Cpu::CX], cpu.r[Cpu::DX], cpu.where().c_str());
  switch (ah) {
    case dosFn::print: {  // a string ending in $
      std::string s;
      for (int i = 0; i < 400; ++i) {
        const u8 c = peek8(cpu.s[Cpu::DS], static_cast<u16>(cpu.r[Cpu::DX] + i));
        if (c == '$') break;
        s += static_cast<char>(c);
      }
      std::fprintf(stderr, "[the program says] %s\n", s.c_str());
      break;
    }
    case dosFn::setVector:
      mem_[cpu.lo(Cpu::AX) * 4u] = static_cast<u8>(cpu.r[Cpu::DX]);
      mem_[cpu.lo(Cpu::AX) * 4u + 1] = static_cast<u8>(cpu.r[Cpu::DX] >> 8);
      mem_[cpu.lo(Cpu::AX) * 4u + 2] = static_cast<u8>(cpu.s[Cpu::DS]);
      mem_[cpu.lo(Cpu::AX) * 4u + 3] = static_cast<u8>(cpu.s[Cpu::DS] >> 8);
      break;
    case dosFn::getVector:
      cpu.r[Cpu::BX] = peek16(0, static_cast<u16>(cpu.lo(Cpu::AX) * 4));
      cpu.s[Cpu::ES] = peek16(0, static_cast<u16>(cpu.lo(Cpu::AX) * 4 + 2));
      break;
    case dosFn::version: cpu.r[Cpu::AX] = kDosVersion; break;
    case dosFn::create: case dosFn::open: {
      const std::string name = string0(cpu.s[Cpu::DS], cpu.r[Cpu::DX]);
      File f;
      f.path = dir_ / name;
      for (const auto& e : std::filesystem::directory_iterator(dir_)) {
        std::string a = e.path().filename().string(), b = name;
        std::transform(a.begin(), a.end(), a.begin(), ::toupper);
        std::transform(b.begin(), b.end(), b.begin(), ::toupper);
        if (a == b) f.path = e.path();
      }
      if (ah == dosFn::open) {
        std::ifstream in(f.path, std::ios::binary);
        if (!in) {
          if (log) std::fprintf(stderr, "[dos] no file %s\n", name.c_str());
          cpu.r[Cpu::AX] = dosError::fileNotFound;
          setCarry(true);
          break;
        }
        f.data.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
      }
      if (log) std::fprintf(stderr, "[dos] %s %s\n", ah == dosFn::open ? "open" : "create", name.c_str());
      cpu.r[Cpu::AX] = nextHandle_;
      files_[nextHandle_++] = std::move(f);
      setCarry(false);
      break;
    }
    case dosFn::close: files_.erase(cpu.r[Cpu::BX]); setCarry(false); break;  // close; nothing is ever written to disk
    case dosFn::read: {
      auto it = files_.find(cpu.r[Cpu::BX]);
      if (it == files_.end()) { cpu.r[Cpu::AX] = dosError::badHandle; setCarry(true); break; }
      File& f = it->second;
      const std::size_t n = std::min<std::size_t>(cpu.r[Cpu::CX], f.data.size() - f.at);
      for (std::size_t i = 0; i < n; ++i) write(u32{cpu.s[Cpu::DS]} * 16 + static_cast<u16>(cpu.r[Cpu::DX] + i), f.data[f.at + i]);
      f.at += n;
      cpu.r[Cpu::AX] = static_cast<u16>(n);
      setCarry(false);
      break;
    }
    case dosFn::write: {
      auto it = files_.find(cpu.r[Cpu::BX]);
      if (it != files_.end()) it->second.written = true;
      cpu.r[Cpu::AX] = cpu.r[Cpu::CX];
      setCarry(false);
      break;
    }
    case dosFn::seek: {
      auto it = files_.find(cpu.r[Cpu::BX]);
      if (it == files_.end()) { cpu.r[Cpu::AX] = dosError::badHandle; setCarry(true); break; }
      File& f = it->second;
      const long off = static_cast<i32>((u32{cpu.r[Cpu::CX]} << 16) | cpu.r[Cpu::DX]);
      const long from = cpu.lo(Cpu::AX) == kSeekFromStart ? 0 : cpu.lo(Cpu::AX) == kSeekFromHere ? static_cast<long>(f.at) : static_cast<long>(f.data.size());
      f.at = static_cast<std::size_t>(std::clamp<long>(from + off, 0, static_cast<long>(f.data.size())));
      cpu.r[Cpu::AX] = static_cast<u16>(f.at);
      cpu.r[Cpu::DX] = static_cast<u16>(f.at >> 16);
      setCarry(false);
      break;
    }
    case dosFn::allocate: {
      const u16 want = cpu.r[Cpu::BX];
      if (u32{nextFree_} + want + 1 > kMemoryTop) {
        cpu.r[Cpu::AX] = dosError::notEnoughMemory;
        cpu.r[Cpu::BX] = static_cast<u16>(kMemoryTop - nextFree_ - 1);
        setCarry(true);
        break;
      }
      cpu.r[Cpu::AX] = nextFree_;
      nextFree_ = static_cast<u16>(nextFree_ + want + 1);
      setCarry(false);
      break;
    }
    case dosFn::free: case dosFn::resize: setCarry(false); break;
    case dosFn::exec:  // the sound driver, which is done here by hand instead
      if (log) std::fprintf(stderr, "[dos] run %s\n", string0(cpu.s[Cpu::DS], cpu.r[Cpu::DX]).c_str());
      setCarry(false);
      break;
    case dosFn::exit:
      if (log) std::fprintf(stderr, "[dos] exit %d\n", cpu.lo(Cpu::AX));
      exited_ = true;
      cpu.halted = true;
      break;
    default:
      throw CpuError("DOS call " + std::to_string(ah) + " at " + cpu.where());
  }
}

void Machine::callBack(const Callback& c) {
  // As the driver's interrupt does it: one word pushed, zero when nothing was missed.
  cpu.push(0);
  cpu.callFar(c.seg, c.off);
  cpu.pop();
}

void Machine::driver() {
  const u8 al = cpu.lo(Cpu::AX);
  if (log && al != int66::poll && al != int66::ticks)
    std::fprintf(stderr, "[driver] al=%02x bx=%04x cx=%04x dx=%04x es=%04x at %s\n", al, cpu.r[Cpu::BX], cpu.r[Cpu::CX],
                 cpu.r[Cpu::DX], cpu.s[Cpu::ES], cpu.where().c_str());
  bool plainReturn = true;
  switch (al) {
    case int66::unload:
      haveFrameCallback_ = false;
      lineCallbacks_.clear();
      break;
    case int66::start: driverStatus_ = kStarted; break;
    case int66::poll:  // the silent driver calls the music's callback every time
      if (!polling_ && (musicSeg_ || musicOff_)) {
        polling_ = true;
        cpu.callFar(musicSeg_, musicOff_);
        polling_ = false;
      }
      break;
    case int66::frameCallback:
      frameCallback_ = {0, cpu.lo(Cpu::BX), cpu.s[Cpu::ES], cpu.r[Cpu::DX]};
      haveFrameCallback_ = true;
      break;
    case int66::lineCallback: {  // one already at that line is taken out instead
      const u16 line = cpu.r[Cpu::CX];
      auto it = std::find_if(lineCallbacks_.begin(), lineCallbacks_.end(), [&](const Callback& c) { return c.scanline == line; });
      if (it != lineCallbacks_.end()) {
        lineCallbacks_.erase(it);
      } else {
        lineCallbacks_.push_back({line, cpu.lo(Cpu::BX), cpu.s[Cpu::ES], cpu.r[Cpu::DX]});
        std::stable_sort(lineCallbacks_.begin(), lineCallbacks_.end(), [](const Callback& a, const Callback& b) { return a.scanline < b.scanline; });
      }
      break;
    }
    case int66::releaseTimer:  // the timer goes back to the system
      haveFrameCallback_ = false;
      lineCallbacks_.clear();
      break;
    case int66::setMusicCallback:
      musicSeg_ = cpu.s[Cpu::ES];
      musicOff_ = cpu.r[Cpu::DX];
      break;
    case int66::callMusicCallback: {  // the callback is called with BL in AL, and what it leaves in AL is returned
      u8 result = cpu.lo(Cpu::BX);
      if (musicSeg_ || musicOff_) {
        cpu.setLo(Cpu::AX, result);
        result = static_cast<u8>(cpu.callFar(musicSeg_, musicOff_));
      }
      driverStatus_ = result;
      break;
    }
    case int66::status: driverStatus_ = kSilentStatus; break;
    case int66::ticks:  // frames counted since int66::resetTicks
      cpu.r[Cpu::AX] = static_cast<u16>(ticks_);
      cpu.r[Cpu::DX] = static_cast<u16>(ticks_ >> 16);
      plainReturn = false;
      break;
    case int66::resetTicks: ticks_ = 0; break;
    case int66::enableCallbacks: callbacksOn_ = cpu.lo(Cpu::BX) != 0; break;
    case int66::position: plainReturn = false; break;
    // These do nothing in the silent driver: volume, music position, effects, loading. (0x01, 0x02,
    // 0x03, 0x07, 0x0a and 0x0e are left as numbers: nothing here says what they are.)
    case 0x01: case 0x02: case 0x03: case int66::setMasterVolume: case 0x07: case 0x0a: case 0x0e: case int66::stop:
    case int66::jump: case int66::playSfx: case int66::load:
      break;
    default:
      break;
  }
  if (plainReturn) {
    cpu.setLo(Cpu::AX, driverStatus_ == kNoStatus ? 0 : driverStatus_);
    driverStatus_ = kNoStatus;
  }
}

void Machine::setLoop(u16 codeSegment, u16 offset) {
  loopAt_ = (u32{seg(codeSegment)} << 16) | offset;
  cpu.watch[loopAt_] = [this] {
    if (++loops_ >= loopLimit_) cpu.pause = true;
  };
}

void Machine::boot() {
  if (!loopAt_) throw CpuError("boot() needs setLoop()");
  loops_ = 0;
  loopLimit_ = 1;
  cpu.pause = false;
  for (std::uint64_t i = 0; i < 400'000'000 && !cpu.pause && !exited_; ++i) cpu.step();
  if (!cpu.pause) throw CpuError("the program never reached its loop");
  ticks_ = 0;
}

void Machine::frame() {
  // Not in the middle of something the program does with interrupts off.
  for (int i = 0; i < 100000 && !cpu.flag(Cpu::IF) && !exited_; ++i) cpu.step();
  for (u8 code : keys_) {
    keyPort_ = code;
    const u16 off = peek16(0, vec::keyboard * 4), seg = peek16(0, vec::keyboard * 4 + 2);
    if (seg || off) cpu.callInterrupt(seg, off);
  }
  keys_.clear();
  vga.startFrame();
  if (callbacksOn_ && haveFrameCallback_) {
    ++ticks_;
    callBack(frameCallback_);
    // the list may change under a callback
    for (std::size_t i = 0; i < lineCallbacks_.size(); ++i) callBack(lineCallbacks_[i]);
  }
  if (loopAt_) {
    // The loop is where boot() or the last frame left it: at its first instruction.
    // The loop is at its first instruction, which is not a turn done; unless the last frame
    // ran out of time inside a turn (the program waiting for a key), which this one will finish.
    loops_ = midTurn_ ? 0 : -1;
    loopLimit_ = loopsPerFrame;
    cpu.pause = false;
    for (std::uint64_t i = 0; i < 2'000'000 && !cpu.pause && !exited_; ++i) cpu.step();
    midTurn_ = !cpu.pause;
    loopLimit_ = 1 << 30;    // the callbacks and keys of the next frame are not counted
  } else if (!exited_) {
    cpu.run(loopBudget);
  }
  ++frames;
}

}  // namespace oracle
