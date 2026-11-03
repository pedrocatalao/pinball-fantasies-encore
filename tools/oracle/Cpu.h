#pragma once
// A processor for the referee (docs/own-engine.md): the 16-bit real-mode instructions the
// game's programs use, which are the 8086's and the 80186's. It knows nothing of the machine
// around it: memory, ports and interrupts are the Bus's.
#include <cstdint>
#include <functional>
#include <map>
#include <vector>
#include <stdexcept>
#include <string>

namespace oracle {

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;

/// The instructions, by their first byte. Where eight in a row take a register in their low
/// three bits (inc, dec, push, pop, xchg, mov), the name is the first of them, and a register
/// (Cpu::Reg, or Cpu::Reg8 for the bytes) is added to it.
namespace op {
constexpr u8 pushEs = 0x06, popEs = 0x07, pushCs = 0x0e, pushSs = 0x16, popSs = 0x17, pushDs = 0x1e, popDs = 0x1f;
constexpr u8 segEs = 0x26, segCs = 0x2e, segSs = 0x36, segDs = 0x3e;  ///< prefixes: the segment to use
constexpr u8 daa = 0x27, das = 0x2f, aaa = 0x37, aas = 0x3f;
constexpr u8 inc = 0x40, dec = 0x48, push = 0x50, pop = 0x58;  ///< + a register
constexpr u8 pusha = 0x60, popa = 0x61;
constexpr u8 pushImm16 = 0x68, imulImm16 = 0x69, pushImm8 = 0x6a, imulImm8 = 0x6b;
constexpr u8 insb = 0x6c, insw = 0x6d, outsb = 0x6e, outsw = 0x6f;
/// The sixteen conditional jumps, each the one before it with the condition turned round.
constexpr u8 jo = 0x70, jno = 0x71, jb = 0x72, jnb = 0x73, jz = 0x74, jnz = 0x75, jbe = 0x76, ja = 0x77;
constexpr u8 js = 0x78, jns = 0x79, jp = 0x7a, jnp = 0x7b, jl = 0x7c, jnl = 0x7d, jle = 0x7e, jg = 0x7f;
/// The arithmetic on a register or memory with a number; the operation is in the ModR/M byte.
constexpr u8 aluRm8Imm = 0x80, aluRm16Imm = 0x81, aluRm8Imm82 = 0x82, aluRm16Imm8 = 0x83;
constexpr u8 testRm8 = 0x84, testRm16 = 0x85, xchgRm8 = 0x86, xchgRm16 = 0x87;
constexpr u8 movRm8Reg = 0x88, movRm16Reg = 0x89, movReg8Rm = 0x8a, movReg16Rm = 0x8b;
constexpr u8 movRmSeg = 0x8c, lea = 0x8d, movSegRm = 0x8e, popRm = 0x8f;
constexpr u8 nop = 0x90, xchgAx = 0x90;  ///< xchgAx + a register; with ax itself it is the nop
constexpr u8 cbw = 0x98, cwd = 0x99, callFar = 0x9a, wait = 0x9b, pushf = 0x9c, popf = 0x9d, sahf = 0x9e, lahf = 0x9f;
constexpr u8 movAlMem = 0xa0, movAxMem = 0xa1, movMemAl = 0xa2, movMemAx = 0xa3;
constexpr u8 movsb = 0xa4, movsw = 0xa5, cmpsb = 0xa6, cmpsw = 0xa7, testAlImm = 0xa8, testAxImm = 0xa9;
constexpr u8 stosb = 0xaa, stosw = 0xab, lodsb = 0xac, lodsw = 0xad, scasb = 0xae, scasw = 0xaf;
constexpr u8 movReg8Imm = 0xb0, movReg16Imm = 0xb8;  ///< + a register
constexpr u8 shiftRm8Imm = 0xc0, shiftRm16Imm = 0xc1, retImm = 0xc2, ret = 0xc3, les = 0xc4, lds = 0xc5;
constexpr u8 movRm8Imm = 0xc6, movRm16Imm = 0xc7, enter = 0xc8, leave = 0xc9, retfImm = 0xca, retf = 0xcb;
constexpr u8 int3 = 0xcc, intImm = 0xcd, iret = 0xcf;
constexpr u8 shiftRm8By1 = 0xd0, shiftRm16By1 = 0xd1, shiftRm8ByCl = 0xd2, shiftRm16ByCl = 0xd3;
constexpr u8 aam = 0xd4, aad = 0xd5, xlat = 0xd7;
constexpr u8 loopne = 0xe0, loope = 0xe1, loop = 0xe2, jcxz = 0xe3;
constexpr u8 inAlImm = 0xe4, inAxImm = 0xe5, outImmAl = 0xe6, outImmAx = 0xe7;
constexpr u8 call = 0xe8, jmp = 0xe9, jmpFar = 0xea, jmpShort = 0xeb;
constexpr u8 inAlDx = 0xec, inAxDx = 0xed, outDxAl = 0xee, outDxAx = 0xef;
constexpr u8 lock = 0xf0, repne = 0xf2, rep = 0xf3;  ///< prefixes
constexpr u8 hlt = 0xf4, cmc = 0xf5, unaryRm8 = 0xf6, unaryRm16 = 0xf7;
constexpr u8 clc = 0xf8, stc = 0xf9, cli = 0xfa, sti = 0xfb, cld = 0xfc, std_ = 0xfd;  ///< std_: the name std is taken
constexpr u8 incDecRm8 = 0xfe, groupRm16 = 0xff;  ///< inc, dec; and inc, dec, call, jmp, push
}  // namespace op

struct CpuError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

/// What a port nothing answers reads as: every bit set.
constexpr u8 kOpenBus = 0xff;

class Bus {
 public:
  virtual ~Bus() = default;
  virtual u8 read(u32 address) = 0;
  virtual void write(u32 address, u8 value) = 0;
  virtual u8 portIn(u16 port) = 0;
  virtual void portOut(u16 port, u8 value) = 0;
  /// A software interrupt. True if the machine answered it itself; false to go through the
  /// vector in memory, as the processor would.
  virtual bool interrupt(u8 number) = 0;
};

class Cpu {
 public:
  enum Reg { AX, CX, DX, BX, SP, BP, SI, DI };
  /// The byte registers as the instructions number them: the low halves, then the high.
  enum Reg8 { AL, CL, DL, BL, AH, CH, DH, BH };
  enum Seg { ES, CS, SS, DS };
  enum Flag : u16 { CF = 1, PF = 4, AF = 0x10, ZF = 0x40, SF = 0x80, TF = 0x100, IF = 0x200, DF = 0x400, OF = 0x800 };

  explicit Cpu(Bus& bus) : bus_(bus) {}

  u16 r[8]{};
  u16 s[4]{};
  u16 ip = 0;
  u16 flags = 2;
  bool halted = false;
  /// Called when the instruction at a place (segment << 16 | offset) is about to run.
  std::map<u32, std::function<void()>> watch;
  /// Set by a watch to leave that instruction for later: step() returns without running it.
  bool pause = false;
  /// If set: every offset in segment `coverageSegment` an instruction began at is marked.
  std::vector<bool>* coverage = nullptr;
  u16 coverageSegment = 0;
  std::uint64_t executed = 0;

  u8 lo(Reg x) const { return static_cast<u8>(r[x]); }
  u8 hi(Reg x) const { return static_cast<u8>(r[x] >> 8); }
  void setLo(Reg x, u8 v) { r[x] = static_cast<u16>((r[x] & 0xff00) | v); }
  void setHi(Reg x, u8 v) { r[x] = static_cast<u16>((r[x] & 0x00ff) | (v << 8)); }
  bool flag(Flag f) const { return flags & f; }
  void setFlag(Flag f, bool on) { flags = static_cast<u16>(on ? flags | f : flags & ~f); }

  u8 read8(u16 seg, u16 off) { return bus_.read((u32{seg} << 4) + off); }
  u16 read16(u16 seg, u16 off) { return static_cast<u16>(read8(seg, off) | (read8(seg, static_cast<u16>(off + 1)) << 8)); }
  void write8(u16 seg, u16 off, u8 v) { bus_.write((u32{seg} << 4) + off, v); }
  void write16(u16 seg, u16 off, u16 v) {
    write8(seg, off, static_cast<u8>(v));
    write8(seg, static_cast<u16>(off + 1), static_cast<u8>(v >> 8));
  }
  void push(u16 v) { r[SP] = static_cast<u16>(r[SP] - 2); write16(s[SS], r[SP], v); }
  u16 pop() { const u16 v = read16(s[SS], r[SP]); r[SP] = static_cast<u16>(r[SP] + 2); return v; }

  /// One instruction (a repeated string instruction counts as one).
  void step();
  /// Up to `limit` instructions; stops early at a hlt.
  void run(std::uint64_t limit);
  /// Calls a far routine that ends with retf, as an interrupt handler of the machine would:
  /// every register is as it was afterwards. Throws if it has not returned after `limit`.
  /// Returns what the routine left in AX.
  u16 callFar(u16 seg, u16 off, std::uint64_t limit = 50'000'000);
  /// The same for a routine that ends with iret (an interrupt vector).
  u16 callInterrupt(u16 seg, u16 off, std::uint64_t limit = 50'000'000);
  /// The vector in memory.
  void raise(u8 number);

  std::string where() const;
  /// Where the last instructions were, oldest first, for when something goes wrong.
  std::string history() const;

 private:
  struct Operand {
    bool isReg = false;
    int reg = 0;
    u16 seg = 0, off = 0;
  };
  u8 fetch8() { return read8(s[CS], ip++); }
  u16 fetch16() { const u16 v = read16(s[CS], ip); ip = static_cast<u16>(ip + 2); return v; }
  Operand modrm(u8 m);
  u8 get8(const Operand& o);
  u16 get16(const Operand& o);
  void set8(const Operand& o, u8 v);
  void set16(const Operand& o, u16 v);
  u8 reg8(int n) const { return n < 4 ? lo(static_cast<Reg>(n)) : hi(static_cast<Reg>(n - 4)); }
  void setReg8(int n, u8 v) { n < 4 ? setLo(static_cast<Reg>(n), v) : setHi(static_cast<Reg>(n - 4), v); }
  u16 dataSeg(Seg normal) const { return segOverride_ >= 0 ? s[segOverride_] : s[normal]; }

  void setSzp8(u8 v);
  void setSzp16(u16 v);
  u8 alu8(int op, u8 a, u8 b);
  u16 alu16(int op, u16 a, u16 b);
  u8 shift8(int op, u8 v, int count);
  u16 shift16(int op, u16 v, int count);
  u8 inc8(u8 v, bool dec);
  u16 inc16(u16 v, bool dec);
  void string(u8 opcode);
  bool condition(int cc) const;
  u16 callRoutine(u16 seg, u16 off, bool withFlags, std::uint64_t limit);
  [[noreturn]] void bad(u8 opcode);

  Bus& bus_;
  static constexpr int kNoOverride = -1;
  static constexpr int kNoRep = 0, kRepe = 1, kRepne = 2;  ///< rep_: none, repe (or rep), repne
  int segOverride_ = kNoOverride;
  int rep_ = kNoRep;
  u16 startIp_ = 0;
  static constexpr int kHistory = 48;
  u32 history_[kHistory]{};
  int historyAt_ = 0;
};

}  // namespace oracle
