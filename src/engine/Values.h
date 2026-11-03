#pragma once
// Values the game's programs use alike, named once for the engine, the tables, the menu and the
// sound: what a flag holds when set, and what says "none" or ends a list. (Addresses are named
// elsewhere: Names.inc, and each table's header.)
#include "core/Types.h"

namespace encore {

/// A flag's value when set; a clear flag is 0.
constexpr u8 kSet = 0xff;
/// A word that says "none": no place to go to, no row to look at, the end of a list.
constexpr u16 kNone = 0xffff;
}  // namespace encore
