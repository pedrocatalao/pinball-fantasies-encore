#pragma once
// A table program's memory, and the means to write its routines again (docs/own-engine.md).
//
// The original keeps all a table knows in its data segment, with a few variables among its
// code. A Program starts from those bytes, as they are in the player's TABLEn.PRG, and the
// engine's routines read and write them where the original's do.
//
// The four programs hold the same engine at different addresses. The engine's code is written
// once, against Party Land's addresses, and every access goes through the table's map (found
// by lining the programs up: re/align.py, Maps.inc). A table's own rules are written against
// its own addresses, with the *Native accessors.
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/Error.h"
#include "core/Types.h"
#include "engine/Values.h"

namespace encore {

/// Party Land's names for places in its program (re/symbols/table1.txt).
namespace at {
#include "engine/table/Names.inc"
}

/// A DOS program's header (an .EXE's, which TABLEn.PRG is): where its fields are, in bytes.
namespace mz {
constexpr std::size_t leastBytes = 0x200;  ///< a header's page: a program is no shorter
constexpr std::size_t relocations = 6;  ///< word: how many segment values DOS is to adjust
constexpr std::size_t headerParagraphs = 8;  ///< word: the header's size
constexpr std::size_t relocationTable = 0x18;  ///< word: where their places are listed,
constexpr std::size_t relocationSize = 4;  ///< each an offset and a segment
constexpr std::size_t paragraph = 16;  ///< bytes of a paragraph, which a segment value counts in
constexpr std::size_t segmentBytes = 0x10000;
}  // namespace mz

/// The 8086's instructions, as far as the engine reads the program's own: the ones its
/// pictures that are code are made of.
namespace x86 {
constexpr u8 ret = 0xc3;
constexpr u8 movByte = 0x88;  ///< mov r/m8, r8
constexpr u8 regAh = 0x20;  ///< in its ModR/M byte, AH as the register (AL without it)
constexpr u8 atBx = 0x07, atBxDisp8 = 0x47, atBxDisp16 = 0x87;  ///< [bx], [bx+d8], [bx+d16]
constexpr u8 modDisp8 = 1;  ///< a ModR/M byte's mode for an offset of one byte
}  // namespace x86

class Program {
 public:
  /// A 16-bit variable in the program's memory, little-endian as the original has it.
  class Word {
   public:
    explicit Word(u8* p) : p_(p) {}
    operator u16() const { return static_cast<u16>(p_[0] | (p_[1] << 8)); }
    i16 s() const { return static_cast<i16>(static_cast<u16>(*this)); }
    Word& operator=(u16 v) {
      p_[0] = static_cast<u8>(v);
      p_[1] = static_cast<u8>(v >> 8);
      return *this;
    }
    Word& operator=(const Word& o) { return *this = static_cast<u16>(o); }
    Word& operator+=(u16 v) { return *this = static_cast<u16>(*this + v); }
    Word& operator-=(u16 v) { return *this = static_cast<u16>(*this - v); }
    Word& operator++() { return *this += 1; }
    Word& operator--() { return *this -= 1; }

   private:
    u8* p_;
  };

  static constexpr int kTables = 4;
  /// `prg` is the whole TABLEn.PRG; `table` 0 to 3.
  Program(ByteView prg, int table);
  virtual ~Program() = default;

  int table() const { return table_; }

  // --- variables, by Party Land's address (plus an index, for tables of them)
  u8& B(u16 a, u16 index = 0) { return ds_[static_cast<u16>(data(a) + index)]; }
  Word W(u16 a, u16 index = 0) { return Word(&ds_[static_cast<u16>(data(a) + index)]); }
  /// The same for the variables the program keeps among its code.
  u8& CB(u16 a, u16 index = 0) { return cs_[static_cast<u16>(codeData(a) + index)]; }
  Word CW(u16 a) { return Word(&cs_[codeData(a)]); }

  // --- by this table's own address: what a pointer read from memory points at
  u8& nativeB(u16 a) { return ds_[a]; }
  Word nativeW(u16 a) { return Word(&ds_[a]); }

  /// And the variables among the code, by this table's own address.
  u8& nativeCB(u16 a) { return cs_[a]; }
  Word nativeCW(u16 a) { return Word(&cs_[a]); }

  /// The program's other segments (pictures of the display's animations, the score's digits),
  /// by this table's own segment value: the byte at an offset in one.
  /// The data segment's 64 KB are kept apart from the rest, and a table may have another
  /// segment begin inside them (the Gameshow's map of where the ball is hidden does): what is
  /// there is the same memory.
  u8& farB(u16 nativeSegment, u16 offset) {
    const std::size_t at = (std::size_t{static_cast<u16>(nativeSegment - kLoadSegment)} * mz::paragraph + offset) % image_.size();
    if (const std::size_t d = at - std::size_t{dataSegment_} * mz::paragraph; d < mz::segmentBytes) return ds_[d];
    return image_[at];
  }
  Word farW(u16 nativeSegment, u16 offset) { return Word(&farB(nativeSegment, offset)); }
  /// The segment the program counts as loaded at. Segment values it keeps in its memory are
  /// as DOS would have made them there; the referee loads it at the same place.
  static constexpr u16 kLoadSegment = 0x0810;
  /// One of Party Land's segment values (as its listing has them), as this table has it in
  /// memory.
  u16 S(u16 segment) const;

  /// A number an instruction of Party Land's carries, as this table's own instruction has it:
  /// the byte or word `offset` bytes into the instruction at Party Land's address. The engine
  /// is one for the four tables, but each was built with its own numbers in places: how many
  /// lights, which colours the display's dots are, where its best scores are kept.
  u8 kb(u16 instruction, int offset) const { return cs_[static_cast<u16>(F(instruction) + offset)]; }
  /// The offset an instruction of the form "[register + offset]" carries (a table's address),
  /// as this table's instruction has it, whether it wrote it in one byte or two.
  u16 koffset(u16 instruction) const {
    const u8 modrm = kb(instruction, 1);
    return (modrm >> 6) == x86::modDisp8 ? static_cast<u16>(static_cast<i8>(kb(instruction, 2))) : kw(instruction, 2);
  }
  u16 kw(u16 instruction, int offset) const { return static_cast<u16>(kb(instruction, offset) | (kb(instruction, offset + 1) << 8)); }

  /// Party Land's address of some data, as this table has it: to keep as a pointer.
  u16 A(u16 a) const { return data(a); }
  /// Party Land's address of a routine, as this table has it: to keep as a pointer.
  u16 F(u16 a) const;
  /// Whether this table has that routine of Party Land's at all.
  bool has(u16 a) const;

  /// Runs the routine at an address of this table's (a pointer read from its memory).
  void call(u16 native);
  /// The routines written (by this table's addresses) that nothing has called yet: for the
  /// tests, to know what a comparison with the original did not reach.
  std::vector<u16> neverCalled() const;
  /// Says which function is the routine at Party Land's address `a`, in every table.
  void bind(u16 a, std::function<void()> fn);
  void bindNative(u16 native, std::function<void()> fn) { routines_[native] = std::move(fn); }

  /// The whole data segment, for the tools that compare it with the original's.
  const std::vector<u8>& memory() const { return ds_; }
  std::vector<u8>& memory() { return ds_; }
  const std::vector<u8>& codeMemory() const { return cs_; }
  std::vector<u8>& codeMemory() { return cs_; }
  /// The whole program as loaded, from its load segment on: for the same tools.
  std::vector<u8>& image() { return image_; }
  /// Segment values as the program's listing has them.
  u16 dataSegment() const { return dataSegment_; }
  /// For reports: Party Land's address for one of this table's, or kNone.
  u16 partyLandData(u16 native) const;

  /// For finding a mistake: a place in the data segment; each routine reached through a
  /// pointer that changes it is said on stderr.
  int debugWatch = -1;
  /// debugWatch's value that has each bounce and each score added said on stderr instead.
  static constexpr int kDebugTrace = -2;

  // What the original passes between routines in registers, for those reached through
  // pointers: a script's place in BX, a task's answer in SI, and so on.
  u16 ax = 0, bx = 0, cx = 0, dx = 0, si = 0, di = 0, bp = 0;

 protected:
  u16 data(u16 a) const;
  u16 codeData(u16 a) const;

  int table_;
  std::vector<u8> ds_, cs_;
  std::vector<u8> image_;  ///< the whole program as loaded, for its other segments
  u16 dataSegment_ = 0;

 private:
  std::unordered_map<u16, std::function<void()>> routines_;
  std::unordered_set<u16> called_;
};

}  // namespace encore
