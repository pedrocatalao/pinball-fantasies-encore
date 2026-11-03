#pragma once
// Values the game's programs use alike, named once for the engine, the tables, the menu and the
// sound: what a flag holds when set, what says "none" or ends a list, the display's characters,
// and the size of a score. (Addresses are named elsewhere: Names.inc, and each table's header.)
#include "core/Types.h"

namespace encore {

/// A flag's value when set; a clear flag is 0.
constexpr u8 kSet = 0xff;
/// A word that says "none": no place to go to, no row to look at, the end of a list.
constexpr u16 kNone = 0xffff;

/// The display's characters, as its fonts number them.
namespace glyph {
constexpr u8 space = 0x20;
constexpr u8 blank = 0x2a;  ///< a place left empty, the width of a digit
constexpr u8 digit0 = 0x37;  ///< the digits are 0x37 to 0x40
constexpr u16 twoBlanks = 0x2a2a;  ///< two places left empty, as a word
}  // namespace glyph

/// The digits 0 to 9: how many there are.
constexpr u16 kDecimal = 10;

/// A score, a bonus or any number kept a digit to a byte, as addScore adds them.
constexpr u16 kScoreDigits = 12;

}  // namespace encore
