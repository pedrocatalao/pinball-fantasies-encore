#include "Cpu.h"

#include <cstdio>

namespace oracle {
namespace {

/// The eight arithmetic operations, as the instructions number them (bits 3 to 5 of the
/// opcode in 00 to 3f; the ModR/M byte's middle field after 80 to 83).
constexpr int kAdd = 0, kOr = 1, kAdc = 2, kSbb = 3, kAnd = 4, kSub = 5, kXor = 6, kCmp = 7;
/// The six forms each of those comes in (the opcode's low three bits): memory or a register
/// with a register, a register with memory or a register, the accumulator with a number (5, a
/// word, is the switch's default).
constexpr int kRmReg8 = 0, kRmReg16 = 1, kRegRm8 = 2, kRegRm16 = 3, kAccImm8 = 4;
/// The eight shifts and rotations (the ModR/M byte's middle field after c0, c1, d0 to d3); the
/// last, 7 (sar), is the switch's default.
constexpr int kRol = 0, kRor = 1, kRcl = 2, kRcr = 3, kShl = 4, kShr = 5, kSal = 6;
/// After f6 and f7 (1 is a second test; 7, idiv, is the switch's default).
constexpr int kTest = 0, kTest1 = 1, kNot = 2, kNeg = 3, kMul = 4, kImul = 5, kDiv = 6;
/// After fe (the first two only) and ff.
constexpr int kIncRm = 0, kDecRm = 1, kCallRm = 2, kCallFarRm = 3, kJmpRm = 4, kJmpFarRm = 5, kPushRm = 6;
/// What a conditional jump tests, from bits 1 to 3 of its opcode (7, less or equal, is the
/// switch's default); bit 0 turns it round.
constexpr int kOverflow = 0, kBelow = 1, kZero = 2, kBelowOrZero = 3, kSign = 4, kParity = 5, kLess = 6;
constexpr int kConditionNot = 1;
/// The ModR/M byte's top two bits: memory with no displacement, a byte's, a word's; or a register.
constexpr int kModNoDisp = 0, kModDisp8 = 1, kModDisp16 = 2, kModReg = 3;
/// Its low three, for memory: the registers the address is made of (with a mod of 0, kRmBp is
/// a word that follows instead).
constexpr int kRmBxSi = 0, kRmBxDi = 1, kRmBpSi = 2, kRmBpDi = 3, kRmSi = 4, kRmDi = 5, kRmBp = 6;
/// Below this segment are the vectors and the BIOS's notes, where no program's code is.
constexpr int kLowestCodeSegment = 0x50;
/// The breakpoint's interrupt (int3).
constexpr u8 kBreakpoint = 3;

bool parity(u8 v) {
  v ^= v >> 4;
  v ^= v >> 2;
  v ^= v >> 1;
  return !(v & 1);
}

}  // namespace

std::string Cpu::where() const {
  char b[64];
  std::snprintf(b, sizeof b, "%04x:%04x", s[CS], startIp_);
  return b;
}

std::string Cpu::history() const {
  std::string out;
  char b[16];
  for (int i = 0; i < kHistory; ++i) {
    const u32 h = history_[(historyAt_ + i) % kHistory];
    std::snprintf(b, sizeof b, "%04x:%04x ", h >> 16, h & 0xffff);
    out += b;
  }
  return out;
}

void Cpu::bad(u8 opcode) {
  char b[96];
  std::snprintf(b, sizeof b, "instruction %02x not understood at %s", opcode, where().c_str());
  throw CpuError(b);
}

Cpu::Operand Cpu::modrm(u8 m) {
  Operand o;
  const int mod = m >> 6, rm = m & 7;
  if (mod == kModReg) {
    o.isReg = true;
    o.reg = rm;
    return o;
  }
  u16 off = 0;
  Seg seg = DS;
  switch (rm) {
    case kRmBxSi: off = static_cast<u16>(r[BX] + r[SI]); break;
    case kRmBxDi: off = static_cast<u16>(r[BX] + r[DI]); break;
    case kRmBpSi: off = static_cast<u16>(r[BP] + r[SI]); seg = SS; break;
    case kRmBpDi: off = static_cast<u16>(r[BP] + r[DI]); seg = SS; break;
    case kRmSi: off = r[SI]; break;
    case kRmDi: off = r[DI]; break;
    case kRmBp:
      if (mod == kModNoDisp) off = fetch16();
      else { off = r[BP]; seg = SS; }
      break;
    default: off = r[BX]; break;
  }
  if (mod == kModDisp8) off = static_cast<u16>(off + static_cast<i8>(fetch8()));
  else if (mod == kModDisp16) off = static_cast<u16>(off + fetch16());
  o.seg = dataSeg(seg);
  o.off = off;
  return o;
}

u8 Cpu::get8(const Operand& o) { return o.isReg ? reg8(o.reg) : read8(o.seg, o.off); }
u16 Cpu::get16(const Operand& o) { return o.isReg ? r[o.reg] : read16(o.seg, o.off); }
void Cpu::set8(const Operand& o, u8 v) { o.isReg ? setReg8(o.reg, v) : write8(o.seg, o.off, v); }
void Cpu::set16(const Operand& o, u16 v) {
  if (o.isReg) r[o.reg] = v;
  else write16(o.seg, o.off, v);
}

void Cpu::setSzp8(u8 v) {
  setFlag(ZF, v == 0);
  setFlag(SF, v & 0x80);
  setFlag(PF, parity(v));
}
void Cpu::setSzp16(u16 v) {
  setFlag(ZF, v == 0);
  setFlag(SF, v & 0x8000);
  setFlag(PF, parity(static_cast<u8>(v)));
}

// op: kAdd to kCmp
u8 Cpu::alu8(int op, u8 a, u8 b) {
  u32 res = 0;
  const u32 c = flag(CF) ? 1 : 0;
  switch (op) {
    case kAdd: res = u32{a} + b; break;
    case kAdc: res = u32{a} + b + c; break;
    case kSbb: res = u32{a} - b - c; break;
    case kSub: case kCmp: res = u32{a} - b; break;
    case kOr: res = a | b; break;
    case kAnd: res = a & b; break;
    default: res = a ^ b; break;
  }
  const u8 v = static_cast<u8>(res);
  if (op == kOr || op == kAnd || op == kXor) {
    setFlag(CF, false);
    setFlag(OF, false);
    setFlag(AF, false);
  } else {
    setFlag(CF, res & 0x100);
    setFlag(AF, (a ^ b ^ v) & 0x10);
    const bool sub = op == kSbb || op == kSub || op == kCmp;
    setFlag(OF, sub ? ((a ^ b) & (a ^ v) & 0x80) : (~(a ^ b) & (a ^ v) & 0x80));
  }
  setSzp8(v);
  return v;
}

u16 Cpu::alu16(int op, u16 a, u16 b) {
  u32 res = 0;
  const u32 c = flag(CF) ? 1 : 0;
  switch (op) {
    case kAdd: res = u32{a} + b; break;
    case kAdc: res = u32{a} + b + c; break;
    case kSbb: res = u32{a} - b - c; break;
    case kSub: case kCmp: res = u32{a} - b; break;
    case kOr: res = a | b; break;
    case kAnd: res = a & b; break;
    default: res = a ^ b; break;
  }
  const u16 v = static_cast<u16>(res);
  if (op == kOr || op == kAnd || op == kXor) {
    setFlag(CF, false);
    setFlag(OF, false);
    setFlag(AF, false);
  } else {
    setFlag(CF, res & 0x10000);
    setFlag(AF, (a ^ b ^ v) & 0x10);
    const bool sub = op == kSbb || op == kSub || op == kCmp;
    setFlag(OF, sub ? ((a ^ b) & (a ^ v) & 0x8000) : (~(a ^ b) & (a ^ v) & 0x8000));
  }
  setSzp16(v);
  return v;
}

u8 Cpu::inc8(u8 v, bool dec) {
  const bool c = flag(CF);
  const u8 res = alu8(dec ? kSub : kAdd, v, 1);
  setFlag(CF, c);
  return res;
}
u16 Cpu::inc16(u16 v, bool dec) {
  const bool c = flag(CF);
  const u16 res = alu16(dec ? kSub : kAdd, v, 1);
  setFlag(CF, c);
  return res;
}

// op: kRol to 7 (sar). The 80186 takes the count modulo 32.
u8 Cpu::shift8(int op, u8 v, int count) {
  count &= 31;
  if (!count) return v;
  bool c = flag(CF);
  for (int i = 0; i < count; ++i) {
    const u8 before = v;
    switch (op) {
      case kRol: c = v & 0x80; v = static_cast<u8>((v << 1) | (c ? 1 : 0)); break;
      case kRor: c = v & 1; v = static_cast<u8>((v >> 1) | (c ? 0x80 : 0)); break;
      case kRcl: { const bool n = v & 0x80; v = static_cast<u8>((v << 1) | (c ? 1 : 0)); c = n; break; }
      case kRcr: { const bool n = v & 1; v = static_cast<u8>((v >> 1) | (c ? 0x80 : 0)); c = n; break; }
      case kShl: case kSal: c = v & 0x80; v = static_cast<u8>(v << 1); break;
      case kShr: c = v & 1; v = static_cast<u8>(v >> 1); break;
      default: c = v & 1; v = static_cast<u8>((v >> 1) | (v & 0x80)); break;  // sar: the sign bit kept
    }
    // The overflow flag is defined for a count of one; this leaves it as the last step's.
    if (op == kRol || op == kRcl || op == kShl || op == kSal) setFlag(OF, ((v & 0x80) != 0) != c);
    else if (op == kRor || op == kRcr) setFlag(OF, ((v ^ (v << 1)) & 0x80) != 0);
    else if (op == kShr) setFlag(OF, before & 0x80);
    else setFlag(OF, false);
  }
  setFlag(CF, c);
  if (op >= kShl) setSzp8(v);
  return v;
}

u16 Cpu::shift16(int op, u16 v, int count) {
  count &= 31;
  if (!count) return v;
  bool c = flag(CF);
  for (int i = 0; i < count; ++i) {
    const u16 before = v;
    switch (op) {
      case kRol: c = v & 0x8000; v = static_cast<u16>((v << 1) | (c ? 1 : 0)); break;
      case kRor: c = v & 1; v = static_cast<u16>((v >> 1) | (c ? 0x8000 : 0)); break;
      case kRcl: { const bool n = v & 0x8000; v = static_cast<u16>((v << 1) | (c ? 1 : 0)); c = n; break; }
      case kRcr: { const bool n = v & 1; v = static_cast<u16>((v >> 1) | (c ? 0x8000 : 0)); c = n; break; }
      case kShl: case kSal: c = v & 0x8000; v = static_cast<u16>(v << 1); break;
      case kShr: c = v & 1; v = static_cast<u16>(v >> 1); break;
      default: c = v & 1; v = static_cast<u16>((v >> 1) | (v & 0x8000)); break;  // sar: the sign bit kept
    }
    if (op == kRol || op == kRcl || op == kShl || op == kSal) setFlag(OF, ((v & 0x8000) != 0) != c);
    else if (op == kRor || op == kRcr) setFlag(OF, ((v ^ (v << 1)) & 0x8000) != 0);
    else if (op == kShr) setFlag(OF, before & 0x8000);
    else setFlag(OF, false);
  }
  setFlag(CF, c);
  if (op >= kShl) setSzp16(v);
  return v;
}

bool Cpu::condition(int cc) const {
  bool v = false;
  switch (cc >> 1) {
    case kOverflow: v = flag(OF); break;
    case kBelow: v = flag(CF); break;
    case kZero: v = flag(ZF); break;
    case kBelowOrZero: v = flag(CF) || flag(ZF); break;
    case kSign: v = flag(SF); break;
    case kParity: v = flag(PF); break;
    case kLess: v = flag(SF) != flag(OF); break;
    default: v = flag(ZF) || flag(SF) != flag(OF); break;  // less or equal
  }
  return (cc & kConditionNot) ? !v : v;
}

void Cpu::string(u8 opcode) {
  const bool word = opcode & 1;
  const u16 delta = static_cast<u16>(flag(DF) ? (word ? -2 : -1) : (word ? 2 : 1));
  const u16 src = dataSeg(DS);
  auto once = [&] {
    switch (opcode & 0xfe) {
      case op::movsb:
        if (word) write16(s[ES], r[DI], read16(src, r[SI]));
        else write8(s[ES], r[DI], read8(src, r[SI]));
        r[SI] = static_cast<u16>(r[SI] + delta);
        r[DI] = static_cast<u16>(r[DI] + delta);
        break;
      case op::cmpsb:
        if (word) alu16(kCmp, read16(src, r[SI]), read16(s[ES], r[DI]));
        else alu8(kCmp, read8(src, r[SI]), read8(s[ES], r[DI]));
        r[SI] = static_cast<u16>(r[SI] + delta);
        r[DI] = static_cast<u16>(r[DI] + delta);
        break;
      case op::stosb:
        if (word) write16(s[ES], r[DI], r[AX]);
        else write8(s[ES], r[DI], lo(AX));
        r[DI] = static_cast<u16>(r[DI] + delta);
        break;
      case op::lodsb:
        if (word) r[AX] = read16(src, r[SI]);
        else setLo(AX, read8(src, r[SI]));
        r[SI] = static_cast<u16>(r[SI] + delta);
        break;
      case op::scasb:
        if (word) alu16(kCmp, r[AX], read16(s[ES], r[DI]));
        else alu8(kCmp, lo(AX), read8(s[ES], r[DI]));
        r[DI] = static_cast<u16>(r[DI] + delta);
        break;
      case op::insb:
        write8(s[ES], r[DI], bus_.portIn(r[DX]));
        if (word) write8(s[ES], static_cast<u16>(r[DI] + 1), bus_.portIn(static_cast<u16>(r[DX] + 1)));
        r[DI] = static_cast<u16>(r[DI] + delta);
        break;
      default:  // op::outsb
        bus_.portOut(r[DX], read8(src, r[SI]));
        if (word) bus_.portOut(static_cast<u16>(r[DX] + 1), read8(src, static_cast<u16>(r[SI] + 1)));
        r[SI] = static_cast<u16>(r[SI] + delta);
        break;
    }
  };
  if (!rep_) {
    once();
    return;
  }
  const bool compares = (opcode & 0xfe) == op::cmpsb || (opcode & 0xfe) == op::scasb;
  while (r[CX]) {
    once();
    --r[CX];
    if (compares && flag(ZF) != (rep_ == kRepe)) break;
  }
}

void Cpu::raise(u8 number) {
  push(flags);
  push(s[CS]);
  push(ip);
  setFlag(IF, false);
  setFlag(TF, false);
  ip = read16(0, static_cast<u16>(number * 4));
  s[CS] = read16(0, static_cast<u16>(number * 4 + 2));
}

void Cpu::run(std::uint64_t limit) {
  halted = false;
  for (std::uint64_t i = 0; i < limit && !halted; ++i) step();
}

u16 Cpu::callRoutine(u16 seg, u16 off, bool withFlags, std::uint64_t limit) {
  // The return address is one no program is at, which is how the return is recognised.
  constexpr u16 kSeg = 0xffff, kOff = 0xfff0;
  u16 savedR[8], savedS[4];
  for (int i = 0; i < 8; ++i) savedR[i] = r[i];
  for (int i = 0; i < 4; ++i) savedS[i] = s[i];
  const u16 savedIp = ip, savedFlags = flags;
  const bool savedHalted = halted;
  if (withFlags) {
    push(flags);
    setFlag(IF, false);
  }
  push(kSeg);
  push(kOff);
  s[CS] = seg;
  ip = off;
  halted = false;
  std::uint64_t n = 0;
  while (!(s[CS] == kSeg && ip == kOff)) {
    if (++n > limit) throw CpuError("a routine called at " + where() + " did not return");
    if (halted) halted = false;
    step();
  }
  const u16 result = r[AX];
  for (int i = 0; i < 8; ++i) r[i] = savedR[i];
  for (int i = 0; i < 4; ++i) s[i] = savedS[i];
  ip = savedIp;
  flags = savedFlags;
  halted = savedHalted;
  return result;
}

u16 Cpu::callFar(u16 seg, u16 off, std::uint64_t limit) { return callRoutine(seg, off, false, limit); }
u16 Cpu::callInterrupt(u16 seg, u16 off, std::uint64_t limit) { return callRoutine(seg, off, true, limit); }

void Cpu::step() {
  startIp_ = ip;
  if (s[CS] < kLowestCodeSegment) throw CpuError("the program went astray, to " + where());
  if (!watch.empty()) {
    if (auto it = watch.find((u32{s[CS]} << 16) | ip); it != watch.end()) {
      pause = false;
      it->second();
      if (pause) return;
    }
  }
  if (coverage && s[CS] == coverageSegment) (*coverage)[ip] = true;
  history_[historyAt_] = (u32{s[CS]} << 16) | ip;
  historyAt_ = (historyAt_ + 1) % kHistory;
  segOverride_ = kNoOverride;
  rep_ = kNoRep;
  ++executed;
  u8 op = fetch8();
  for (;;) {  // prefixes
    if (op == op::segEs) segOverride_ = ES;
    else if (op == op::segCs) segOverride_ = CS;
    else if (op == op::segSs) segOverride_ = SS;
    else if (op == op::segDs) segOverride_ = DS;
    else if (op == op::rep) rep_ = kRepe;
    else if (op == op::repne) rep_ = kRepne;
    else if (op != op::lock) break;
    op = fetch8();
  }

  // The eight arithmetic operations in their six forms each.
  if (op < op::inc && (op & 7) < 6) {
    const int alu = op >> 3;
    switch (op & 7) {
      case kRmReg8: { const u8 m = fetch8(); const Operand o = modrm(m); const u8 v = alu8(alu, get8(o), reg8((m >> 3) & 7)); if (alu != kCmp) set8(o, v); break; }
      case kRmReg16: { const u8 m = fetch8(); const Operand o = modrm(m); const u16 v = alu16(alu, get16(o), r[(m >> 3) & 7]); if (alu != kCmp) set16(o, v); break; }
      case kRegRm8: { const u8 m = fetch8(); const Operand o = modrm(m); const int g = (m >> 3) & 7; const u8 v = alu8(alu, reg8(g), get8(o)); if (alu != kCmp) setReg8(g, v); break; }
      case kRegRm16: { const u8 m = fetch8(); const Operand o = modrm(m); const int g = (m >> 3) & 7; const u16 v = alu16(alu, r[g], get16(o)); if (alu != kCmp) r[g] = v; break; }
      case kAccImm8: { const u8 v = alu8(alu, lo(AX), fetch8()); if (alu != kCmp) setLo(AX, v); break; }
      default: { const u16 v = alu16(alu, r[AX], fetch16()); if (alu != kCmp) r[AX] = v; break; }  // the accumulator with a word
    }
    return;
  }

  switch (op) {
    case op::pushEs: push(s[ES]); break;
    case op::popEs: s[ES] = pop(); break;
    case op::pushCs: push(s[CS]); break;
    case op::pushSs: push(s[SS]); break;
    case op::popSs: s[SS] = pop(); break;
    case op::pushDs: push(s[DS]); break;
    case op::popDs: s[DS] = pop(); break;
    case op::daa: {
      u8 al = lo(AX);
      const u8 old = al;
      const bool oldC = flag(CF);
      if ((al & 0x0f) > 9 || flag(AF)) { al = static_cast<u8>(al + 6); setFlag(AF, true); } else setFlag(AF, false);
      if (old > 0x99 || oldC) { al = static_cast<u8>(al + 0x60); setFlag(CF, true); } else setFlag(CF, false);
      setLo(AX, al);
      setSzp8(al);
      break;
    }
    case op::das: {
      u8 al = lo(AX);
      const u8 old = al;
      const bool oldC = flag(CF);
      if ((al & 0x0f) > 9 || flag(AF)) { al = static_cast<u8>(al - 6); setFlag(AF, true); } else setFlag(AF, false);
      if (old > 0x99 || oldC) { al = static_cast<u8>(al - 0x60); setFlag(CF, true); } else setFlag(CF, false);
      setLo(AX, al);
      setSzp8(al);
      break;
    }
    case op::aaa:
      if ((lo(AX) & 0x0f) > 9 || flag(AF)) {
        r[AX] = static_cast<u16>(r[AX] + 0x106);
        setFlag(AF, true);
        setFlag(CF, true);
      } else {
        setFlag(AF, false);
        setFlag(CF, false);
      }
      setLo(AX, lo(AX) & 0x0f);
      break;
    case op::aas:
      if ((lo(AX) & 0x0f) > 9 || flag(AF)) {
        r[AX] = static_cast<u16>(r[AX] - 6);
        setHi(AX, static_cast<u8>(hi(AX) - 1));
        setFlag(AF, true);
        setFlag(CF, true);
      } else {
        setFlag(AF, false);
        setFlag(CF, false);
      }
      setLo(AX, lo(AX) & 0x0f);
      break;
    case op::inc + AX: case op::inc + CX: case op::inc + DX: case op::inc + BX:
    case op::inc + SP: case op::inc + BP: case op::inc + SI: case op::inc + DI:
      r[op & 7] = inc16(r[op & 7], false);
      break;
    case op::dec + AX: case op::dec + CX: case op::dec + DX: case op::dec + BX:
    case op::dec + SP: case op::dec + BP: case op::dec + SI: case op::dec + DI:
      r[op & 7] = inc16(r[op & 7], true);
      break;
    case op::push + AX: case op::push + CX: case op::push + DX: case op::push + BX:
    case op::push + BP: case op::push + SI: case op::push + DI:
      push(r[op & 7]);
      break;
    case op::push + SP: push(r[SP]); break;  // the 80186 and later push the value before the push
    case op::pop + AX: case op::pop + CX: case op::pop + DX: case op::pop + BX:
    case op::pop + SP: case op::pop + BP: case op::pop + SI: case op::pop + DI:
      r[op & 7] = pop();
      break;
    case op::pusha: {
      const u16 sp = r[SP];
      for (int i = 0; i < 8; ++i) push(i == SP ? sp : r[i]);
      break;
    }
    case op::popa:
      for (int i = 7; i >= 0; --i) {
        const u16 v = pop();
        if (i != SP) r[i] = v;
      }
      break;
    case op::pushImm16: push(fetch16()); break;
    case op::pushImm8: push(static_cast<u16>(static_cast<i8>(fetch8()))); break;
    case op::imulImm16: case op::imulImm8: {  // imul reg, r/m, immediate
      const u8 m = fetch8();
      const Operand o = modrm(m);
      const i32 a = static_cast<i16>(get16(o));
      const i32 b = op == op::imulImm16 ? static_cast<i16>(fetch16()) : static_cast<i8>(fetch8());
      const i32 p = a * b;
      r[(m >> 3) & 7] = static_cast<u16>(p);
      const bool over = p != static_cast<i16>(p);
      setFlag(CF, over);
      setFlag(OF, over);
      break;
    }
    case op::insb: case op::insw: case op::outsb: case op::outsw: string(op); break;
    case op::jo: case op::jno: case op::jb: case op::jnb: case op::jz: case op::jnz: case op::jbe: case op::ja:
    case op::js: case op::jns: case op::jp: case op::jnp: case op::jl: case op::jnl: case op::jle: case op::jg: {
      const i8 d = static_cast<i8>(fetch8());
      if (condition(op & 15)) ip = static_cast<u16>(ip + d);
      break;
    }
    case op::aluRm8Imm: case op::aluRm8Imm82: {
      const u8 m = fetch8();
      const Operand o = modrm(m);
      const int alu = (m >> 3) & 7;
      const u8 v = alu8(alu, get8(o), fetch8());
      if (alu != kCmp) set8(o, v);
      break;
    }
    case op::aluRm16Imm: case op::aluRm16Imm8: {
      const u8 m = fetch8();
      const Operand o = modrm(m);
      const int alu = (m >> 3) & 7;
      const u16 a = get16(o);
      const u16 b = op == op::aluRm16Imm ? fetch16() : static_cast<u16>(static_cast<i8>(fetch8()));
      const u16 v = alu16(alu, a, b);
      if (alu != kCmp) set16(o, v);
      break;
    }
    case op::testRm8: { const u8 m = fetch8(); const Operand o = modrm(m); alu8(kAnd, get8(o), reg8((m >> 3) & 7)); break; }
    case op::testRm16: { const u8 m = fetch8(); const Operand o = modrm(m); alu16(kAnd, get16(o), r[(m >> 3) & 7]); break; }
    case op::xchgRm8: { const u8 m = fetch8(); const Operand o = modrm(m); const int g = (m >> 3) & 7; const u8 v = get8(o); set8(o, reg8(g)); setReg8(g, v); break; }
    case op::xchgRm16: { const u8 m = fetch8(); const Operand o = modrm(m); const int g = (m >> 3) & 7; const u16 v = get16(o); set16(o, r[g]); r[g] = v; break; }
    case op::movRm8Reg: { const u8 m = fetch8(); const Operand o = modrm(m); set8(o, reg8((m >> 3) & 7)); break; }
    case op::movRm16Reg: { const u8 m = fetch8(); const Operand o = modrm(m); set16(o, r[(m >> 3) & 7]); break; }
    case op::movReg8Rm: { const u8 m = fetch8(); const Operand o = modrm(m); setReg8((m >> 3) & 7, get8(o)); break; }
    case op::movReg16Rm: { const u8 m = fetch8(); const Operand o = modrm(m); r[(m >> 3) & 7] = get16(o); break; }
    case op::movRmSeg: { const u8 m = fetch8(); const Operand o = modrm(m); set16(o, s[(m >> 3) & 3]); break; }
    case op::lea: { const u8 m = fetch8(); const Operand o = modrm(m); r[(m >> 3) & 7] = o.off; break; }
    case op::movSegRm: { const u8 m = fetch8(); const Operand o = modrm(m); s[(m >> 3) & 3] = get16(o); break; }
    case op::popRm: { const u8 m = fetch8(); const u16 v = pop(); const Operand o = modrm(m); set16(o, v); break; }
    case op::nop: break;
    case op::xchgAx + CX: case op::xchgAx + DX: case op::xchgAx + BX: case op::xchgAx + SP:
    case op::xchgAx + BP: case op::xchgAx + SI: case op::xchgAx + DI: {
      const u16 v = r[AX];
      r[AX] = r[op & 7];
      r[op & 7] = v;
      break;
    }
    case op::cbw: r[AX] = static_cast<u16>(static_cast<i8>(lo(AX))); break;
    case op::cwd: r[DX] = (r[AX] & 0x8000) ? 0xffff : 0; break;
    case op::callFar: {
      const u16 off = fetch16(), seg = fetch16();
      push(s[CS]);
      push(ip);
      s[CS] = seg;
      ip = off;
      break;
    }
    case op::wait: break;
    case op::pushf: push(flags); break;
    case op::popf: flags = static_cast<u16>((pop() & 0x0fd5) | 2); break;
    case op::sahf: flags = static_cast<u16>((flags & 0xff00) | (hi(AX) & 0xd5) | 2); break;
    case op::lahf: setHi(AX, static_cast<u8>(flags)); break;
    case op::movAlMem: setLo(AX, read8(dataSeg(DS), fetch16())); break;
    case op::movAxMem: r[AX] = read16(dataSeg(DS), fetch16()); break;
    case op::movMemAl: write8(dataSeg(DS), fetch16(), lo(AX)); break;
    case op::movMemAx: write16(dataSeg(DS), fetch16(), r[AX]); break;
    case op::movsb: case op::movsw: case op::cmpsb: case op::cmpsw: case op::stosb: case op::stosw: case op::lodsb:
    case op::lodsw: case op::scasb: case op::scasw:
      string(op);
      break;
    case op::testAlImm: alu8(kAnd, lo(AX), fetch8()); break;
    case op::testAxImm: alu16(kAnd, r[AX], fetch16()); break;
    case op::movReg8Imm + AL: case op::movReg8Imm + CL: case op::movReg8Imm + DL: case op::movReg8Imm + BL:
    case op::movReg8Imm + AH: case op::movReg8Imm + CH: case op::movReg8Imm + DH: case op::movReg8Imm + BH:
      setReg8(op & 7, fetch8());
      break;
    case op::movReg16Imm + AX: case op::movReg16Imm + CX: case op::movReg16Imm + DX: case op::movReg16Imm + BX:
    case op::movReg16Imm + SP: case op::movReg16Imm + BP: case op::movReg16Imm + SI: case op::movReg16Imm + DI:
      r[op & 7] = fetch16();
      break;
    case op::shiftRm8Imm: case op::shiftRm8By1: case op::shiftRm8ByCl: {
      const u8 m = fetch8();
      const Operand o = modrm(m);
      const int count = op == op::shiftRm8Imm ? fetch8() : op == op::shiftRm8By1 ? 1 : lo(CX);
      set8(o, shift8((m >> 3) & 7, get8(o), count));
      break;
    }
    case op::shiftRm16Imm: case op::shiftRm16By1: case op::shiftRm16ByCl: {
      const u8 m = fetch8();
      const Operand o = modrm(m);
      const int count = op == op::shiftRm16Imm ? fetch8() : op == op::shiftRm16By1 ? 1 : lo(CX);
      set16(o, shift16((m >> 3) & 7, get16(o), count));
      break;
    }
    case op::retImm: { const u16 n = fetch16(); ip = pop(); r[SP] = static_cast<u16>(r[SP] + n); break; }
    case op::ret: ip = pop(); break;
    case op::les: case op::lds: {
      const u8 m = fetch8();
      const Operand o = modrm(m);
      if (o.isReg) bad(op);
      r[(m >> 3) & 7] = read16(o.seg, o.off);
      s[op == op::les ? ES : DS] = read16(o.seg, static_cast<u16>(o.off + 2));
      break;
    }
    case op::movRm8Imm: { const u8 m = fetch8(); const Operand o = modrm(m); set8(o, fetch8()); break; }
    case op::movRm16Imm: { const u8 m = fetch8(); const Operand o = modrm(m); set16(o, fetch16()); break; }
    case op::enter: {
      const u16 size = fetch16();
      const int level = fetch8() & 31;
      push(r[BP]);
      const u16 frame = r[SP];
      for (int i = 1; i < level; ++i) {
        r[BP] = static_cast<u16>(r[BP] - 2);
        push(read16(s[SS], r[BP]));
      }
      if (level) push(frame);
      r[BP] = frame;
      r[SP] = static_cast<u16>(r[SP] - size);
      break;
    }
    case op::leave: r[SP] = r[BP]; r[BP] = pop(); break;
    case op::retfImm: { const u16 n = fetch16(); ip = pop(); s[CS] = pop(); r[SP] = static_cast<u16>(r[SP] + n); break; }
    case op::retf: ip = pop(); s[CS] = pop(); break;
    case op::int3: if (!bus_.interrupt(kBreakpoint)) raise(kBreakpoint); break;
    case op::intImm: { const u8 n = fetch8(); if (!bus_.interrupt(n)) raise(n); break; }
    case op::iret: ip = pop(); s[CS] = pop(); flags = static_cast<u16>((pop() & 0x0fd5) | 2); break;
    case op::aam: {
      const u8 base = fetch8();
      if (!base) bad(op);
      const u8 al = lo(AX);
      setHi(AX, static_cast<u8>(al / base));
      setLo(AX, static_cast<u8>(al % base));
      setSzp8(lo(AX));
      break;
    }
    case op::aad: {
      const u8 base = fetch8();
      const u8 al = static_cast<u8>(lo(AX) + hi(AX) * base);
      r[AX] = al;
      setSzp8(al);
      break;
    }
    case op::xlat: setLo(AX, read8(dataSeg(DS), static_cast<u16>(r[BX] + lo(AX)))); break;
    case op::loopne: case op::loope: case op::loop: {
      const i8 d = static_cast<i8>(fetch8());
      --r[CX];
      if (r[CX] && (op == op::loop || flag(ZF) == (op == op::loope))) ip = static_cast<u16>(ip + d);
      break;
    }
    case op::jcxz: { const i8 d = static_cast<i8>(fetch8()); if (!r[CX]) ip = static_cast<u16>(ip + d); break; }
    case op::inAlImm: setLo(AX, bus_.portIn(fetch8())); break;
    case op::inAxImm: { const u16 p = fetch8(); r[AX] = static_cast<u16>(bus_.portIn(p) | (bus_.portIn(static_cast<u16>(p + 1)) << 8)); break; }
    case op::outImmAl: bus_.portOut(fetch8(), lo(AX)); break;
    case op::outImmAx: { const u16 p = fetch8(); bus_.portOut(p, lo(AX)); bus_.portOut(static_cast<u16>(p + 1), hi(AX)); break; }
    case op::call: { const u16 d = fetch16(); push(ip); ip = static_cast<u16>(ip + d); break; }
    case op::jmp: { const u16 d = fetch16(); ip = static_cast<u16>(ip + d); break; }
    case op::jmpFar: { const u16 off = fetch16(), seg = fetch16(); s[CS] = seg; ip = off; break; }
    case op::jmpShort: { const i8 d = static_cast<i8>(fetch8()); ip = static_cast<u16>(ip + d); break; }
    case op::inAlDx: setLo(AX, bus_.portIn(r[DX])); break;
    case op::inAxDx: r[AX] = static_cast<u16>(bus_.portIn(r[DX]) | (bus_.portIn(static_cast<u16>(r[DX] + 1)) << 8)); break;
    case op::outDxAl: bus_.portOut(r[DX], lo(AX)); break;
    case op::outDxAx: bus_.portOut(r[DX], lo(AX)); bus_.portOut(static_cast<u16>(r[DX] + 1), hi(AX)); break;
    case op::hlt: halted = true; break;
    case op::cmc: setFlag(CF, !flag(CF)); break;
    case op::unaryRm8: {
      const u8 m = fetch8();
      const Operand o = modrm(m);
      const u8 v = get8(o);
      switch ((m >> 3) & 7) {
        case kTest: case kTest1: alu8(kAnd, v, fetch8()); break;
        case kNot: set8(o, static_cast<u8>(~v)); break;
        case kNeg: { const u8 n = alu8(kSub, 0, v); set8(o, n); setFlag(CF, v != 0); break; }
        case kMul: { r[AX] = static_cast<u16>(lo(AX) * v); const bool c = hi(AX) != 0; setFlag(CF, c); setFlag(OF, c); break; }
        case kImul: { const i16 p = static_cast<i16>(static_cast<i8>(lo(AX)) * static_cast<i8>(v)); r[AX] = static_cast<u16>(p); const bool c = p != static_cast<i8>(p); setFlag(CF, c); setFlag(OF, c); break; }
        case kDiv: {
          if (!v || r[AX] / v > UINT8_MAX) throw CpuError("division overflow at " + where());
          const u16 a = r[AX];
          setLo(AX, static_cast<u8>(a / v));
          setHi(AX, static_cast<u8>(a % v));
          break;
        }
        default: {  // idiv
          const i16 a = static_cast<i16>(r[AX]);
          const i16 d = static_cast<i8>(v);
          if (!d || a / d > INT8_MAX || a / d < INT8_MIN) throw CpuError("division overflow at " + where());
          setLo(AX, static_cast<u8>(a / d));
          setHi(AX, static_cast<u8>(a % d));
          break;
        }
      }
      break;
    }
    case op::unaryRm16: {
      const u8 m = fetch8();
      const Operand o = modrm(m);
      const u16 v = get16(o);
      switch ((m >> 3) & 7) {
        case kTest: case kTest1: alu16(kAnd, v, fetch16()); break;
        case kNot: set16(o, static_cast<u16>(~v)); break;
        case kNeg: { const u16 n = alu16(kSub, 0, v); set16(o, n); setFlag(CF, v != 0); break; }
        case kMul: { const u32 p = u32{r[AX]} * v; r[AX] = static_cast<u16>(p); r[DX] = static_cast<u16>(p >> 16); const bool c = r[DX] != 0; setFlag(CF, c); setFlag(OF, c); break; }
        case kImul: { const i32 p = i32{static_cast<i16>(r[AX])} * static_cast<i16>(v); r[AX] = static_cast<u16>(p); r[DX] = static_cast<u16>(static_cast<u32>(p) >> 16); const bool c = p != static_cast<i16>(p); setFlag(CF, c); setFlag(OF, c); break; }
        case kDiv: {
          const u32 a = (u32{r[DX]} << 16) | r[AX];
          if (!v || a / v > UINT16_MAX) throw CpuError("division overflow at " + where());
          r[AX] = static_cast<u16>(a / v);
          r[DX] = static_cast<u16>(a % v);
          break;
        }
        default: {  // idiv
          const i32 a = static_cast<i32>((u32{r[DX]} << 16) | r[AX]);
          const i32 d = static_cast<i16>(v);
          if (!d || a / d > INT16_MAX || a / d < INT16_MIN) throw CpuError("division overflow at " + where());
          r[AX] = static_cast<u16>(a / d);
          r[DX] = static_cast<u16>(a % d);
          break;
        }
      }
      break;
    }
    case op::clc: setFlag(CF, false); break;
    case op::stc: setFlag(CF, true); break;
    case op::cli: setFlag(IF, false); break;
    case op::sti: setFlag(IF, true); break;
    case op::cld: setFlag(DF, false); break;
    case op::std_: setFlag(DF, true); break;
    case op::incDecRm8: {
      const u8 m = fetch8();
      const Operand o = modrm(m);
      const int sub = (m >> 3) & 7;
      if (sub > kDecRm) bad(op);
      set8(o, inc8(get8(o), sub == kDecRm));
      break;
    }
    case op::groupRm16: {
      const u8 m = fetch8();
      const Operand o = modrm(m);
      switch ((m >> 3) & 7) {
        case kIncRm: set16(o, inc16(get16(o), false)); break;
        case kDecRm: set16(o, inc16(get16(o), true)); break;
        case kCallRm: { const u16 t = get16(o); push(ip); ip = t; break; }
        case kCallFarRm: { if (o.isReg) bad(op); const u16 off = read16(o.seg, o.off), seg = read16(o.seg, static_cast<u16>(o.off + 2)); push(s[CS]); push(ip); s[CS] = seg; ip = off; break; }
        case kJmpRm: ip = get16(o); break;
        case kJmpFarRm: { if (o.isReg) bad(op); const u16 off = read16(o.seg, o.off), seg = read16(o.seg, static_cast<u16>(o.off + 2)); s[CS] = seg; ip = off; break; }
        case kPushRm: push(get16(o)); break;
        default: bad(op);
      }
      break;
    }
    default: bad(op);
  }
}

}  // namespace oracle
