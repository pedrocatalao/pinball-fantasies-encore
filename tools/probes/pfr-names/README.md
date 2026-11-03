# Probes behind the names from pfr

The rules of each table, and the engine under them, were written with the original's addresses
and values as numbers (`b(0x0229)`, `award(0x0688)`). [pfr](https://github.com/wanda-phi/pfr)'s
rewrite of the game in Rust has names for most of them (`tower_extra_ball`,
`EffectBind::StonesTowerExtraBall`), and those names are carried into our code. A rename must not
change what a single routine does, or the recordings in `tests/recordings`, and the online
scores, would stop playing to the same end. These scripts are how each rename is checked. They
are not part of the build; they run from the repository's root, after a build of the tools,
against the 1994 game's files (`DATA`, by default `build/data`).

| Probe                | The question it answers                                                                                                                                             | Run                                                                                                                                                                               |
|----------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `objcmp.sh`          | Does a source file compile to the same machine code as before? (symbols, addresses and PC-relative offsets left out)                                                | `tools/probes/pfr-names/objcmp.sh $PWD build/obj-after.txt src/engine/table/Gameshow.cpp`                                                                                         |
| `fncmp.py`           | Of two `objcmp.sh` outputs, which function bodies differ, whatever order they come in?                                                                              | `python3 tools/probes/pfr-names/fncmp.py build/obj-before.txt build/obj-after.txt`                                                                                                |
| `fnpair.py`          | And what changed in each of those: how many lines, how many of them not padding?                                                                                    | `python3 tools/probes/pfr-names/fnpair.py build/obj-before.txt build/obj-after.txt`                                                                                               |
| `equiv.sh`           | Does the table do the same? The referee over 16 seeds and modes (and 3 with the screen compared), the table's recordings verified, 3 random games, the engine tests | `tools/probes/pfr-names/equiv.sh $PWD build/equiv-after 4`                                                                                                                        |
| `addr_map.cpp`       | For a table's own address: Party Land's address for it (the engine's map) and the bytes there                                                                       | `g++ -std=c++20 -O1 -Isrc tools/probes/pfr-names/addr_map.cpp build/libencore_engine.a build/libencore_core.a -o build/addr_map && build/addr_map "$DATA/TABLE4.PRG" 4 0275 3cea` |
| `hdrcmp.sh`          | Does a file that includes a renamed header, and is not renamed itself, compile the same with the old header and the new?                                            | `tools/probes/pfr-names/hdrcmp.sh $PWD main engine/table/SpeedDevils.h tools/oracle/table.cpp -Itools/oracle`                                                                     |
| `genproof.sh`        | With the engine's numbers made names, does `re/gensymbols.py` still write the same `Names.inc` and `Maps.inc`?                                                      | `NASM_BIN=<folder with ndisasm> tools/probes/pfr-names/genproof.sh $PWD HEAD`                                                                                                     |
| `unreached.py`       | Which of the engine's named routines does no referee run reach (Party Land: the names are its addresses)?                                                           | `python3 tools/probes/pfr-names/unreached.py $PWD build/equiv-after` (folders `-1` to `-4` from `equiv.sh`)                                                                       |
| `pfr_coverage.py`    | Which of pfr's names for the game's things (its binding enums) does our code not carry?                                                                             | `python3 tools/probes/pfr-names/pfr_coverage.py $PWD ~/compartilhado/Pinball/pfr`                                                                                                 |
| `datacmp.sh`         | Do a file's data sections (tables of constants) hold the same bytes as at a ref? (`objcmp.sh` compares only code)                                                   | `tools/probes/pfr-names/datacmp.sh $PWD HEAD src/engine/data/TableSound.cpp`                                                                                                      |
| `soundbytes.cpp`     | Where `TableSound`'s record addresses and pfr's differ, what bytes are at each?                                                                                     | see its first lines                                                                                                                                                               |
| `objall.sh`          | Do two trees compile every `.cpp` of `src/engine`, and the other files named, to the same machine code?                                                             | `tools/probes/pfr-names/objall.sh <tree before> <tree after> <outdir> [more .cpp]`                                                                                                |
| `datacmp-trees.sh`   | Do files outside `src/` (the referee's) hold the same data sections in two checked-out trees?                                                                       | `tools/probes/pfr-names/datacmp-trees.sh <tree before> <tree after> tools/oracle/Cpu.cpp`                                                                                         |
| `count_literals.py`  | How many numbers did a value-naming change replace, by kind of name?                                                                                                | see its first lines                                                                                                                                                               |
| `count_remaining.py` | How many numbers are still written as numbers in the game's code, at a revision?                                                                                    | `python3 tools/probes/pfr-names/count_remaining.py main HEAD`                                                                                                                     |

The method, for each file renamed: `objcmp.sh` and `equiv.sh` on the code before (rebuild the
tools first), the same after, then `fncmp.py` on the two disassemblies and `diff -r` on the two
`equiv.sh` folders. Every function body the compiler emits the same is the same routine; the
few that differ are read one by one (`fnpair.py`); and `equiv.sh` covers what the first two
cannot, such as a table of records read in a different order.

Two things to know when reading the outputs:

- **The referee is not clean at the start.** On every table, before any rename, it reports a
  handful of bytes that differ from the original's (on Stones 'n Bones: `2798`, `38a1`, `38a6`,
  `38a7` at frame 1153, which are Party Land's `230d`, `3389`, `338e`, `338f`). A rename is
  checked by its output being *the same* before and after, not by its being empty.
- **One line of the engine tests changes by itself**: "heard from N of 512" in
  `tests/engine/test_effects.cpp` comes from an unseeded draw (318, 319 and 319 on three runs of
  the same binary).

## Results

Measured on this branch, each commit against the one before it, with `objall.sh` (every `.cpp`
of `src/engine`, the referee, the tools, the tests, `src/game/App.cpp` and `src/main.cpp`: 45
files, about 2,387 function bodies) and `datacmp.sh` / `datacmp-trees.sh` (the data of every
`.cpp` the commit changes).

| commit                                            | machine code                    | data                   |
|---------------------------------------------------|---------------------------------|------------------------|
| test: probes                                      | the same                        | no code                |
| refactor(re): Party Land's addresses              | the same                        | no code                |
| refactor(engine): names for addresses and records | the same                        | the same               |
| refactor: game, view and referee use the names    | the same                        | the same               |
| refactor(stones): addresses                       | 6 bodies differ, 1 more (below) | packed tighter (below) |
| refactor(speed): addresses                        | the same                        | the same               |
| refactor(gameshow): addresses                     | 1 body differs (below)          | the same               |
| refactor(party): addresses                        | the same                        | the same               |
| refactor(sound): functions and records            | the same                        | the same               |
| refactor(menu): INTRO.PRG's addresses             | the same                        | the same               |
| refactor(engine): values                          | the same                        | the same               |
| refactor(tables): values                          | the same                        | the same               |
| refactor: values in game, view, sound, data       | the same                        | the same               |
| refactor(menu): values                            | the same                        | the same               |
| refactor(oracle): the referee's values            | the same                        | the same               |

Every commit compiles. The seven bodies that differ, read one by one (`fnpair.py`):

- **Stones 'n Bones** (six, and `stonesAndBones`, a helper that replaces the list of nine lights
  written out at each place): the helper's two callers; `bindRules` and its `after`, which take a
  `Timer` (a routine and its counter) instead of two numbers; `restorePlayer`, where the ghost's
  address `0x305 + (n - 1) * 7` is folded as `0x2fe + n * 7`, the same number; and two that
  differ only in padding. Its data holds the same constants, packed tighter (32 bytes less
  padding), and `after`'s lambda's type name, which now names `stones::Timer`.
- **Gameshow** (one): `wheelTick` passes `cond ? timers::givePrize : timers::spinWheelEnd.routine`
  to `addTimer`; with names of type `u16` in place of `int` literals GCC lays out the two
  branches the other way round. The same two routines on the same condition.

**Behaviour** (`equiv.sh`, `main` against this branch, each table): the referee over 19 runs,
the recordings, three random games and the engine tests give the same output on all four tables,
but for the one line that changes by itself.

**`re/gensymbols.py`** (`genproof.sh`, against `main`): it writes this branch's `Names.inc`, and
the same `Maps.inc` with two lines more, `ENCORE_MAP_TARGET(0x2316, ...)` and `(0x2ef6, ...)`.
Those are `nudgeLift` and `gravityX`, which the engine already wrote as names on `main`, so their
numbers were never gathered: data addresses taken for routines, as others of the map's lines
gathered from numbers already are, and read only by `F()` and `has()`, which nothing asks for
data. Without the change that gathers the `at::` names, a regeneration would lose 13 lines (among
them `jump`'s, which Speed Devils and the Gameshow bind). `Maps.inc` itself is not regenerated
here: this machine's nasm (2.16.03) does not give the committed one.

**Coverage** (`pfr_coverage.py`): the 549 names of pfr's binding enums (but its `Dummy`) are all
in `src/engine/table`, by name, as a series' base or cited in a comment.

**What is still a number** (`count_remaining.py`, `src/engine` and `tools/oracle`, outside
definitions, comments and strings, not 0, 1 or 2): 10,079 on `main`, 1,253 here. They are
arithmetic (shifts, masks of bits, the 2 of a word, `% 10`), data written as data (the referee's
per-table addresses in `Symbols.h`, `TableSound`'s records, the menu's banners and letters),
ordinals (`case 0` to `case 8`), pfr's own literals in the bounce, and a few a name would
compile otherwise: GCC narrows a division by a literal to the operand's width, not one by a
name, and a name passed by reference (`vector::assign`) or in a `?:` changes the code; those
keep the literal, with a comment.

**Found on the way**, left as they are:

- Where pfr's rules and the original's differ, the code keeps the original's (Party Land:
  `partyR()` ends the cyclone's X5 before it is checked; the popcorns count with no cap;
  `duckAll` sets only the first two flags).
- `TableSound` reads four records at other places than pfr: Speed Devils' match tunes (ours
  0x0a59 and 0x0a5c, pfr's 0x0a62 and 0x0a65), the Gameshow's silence (0x08cf, pfr's 0x08ad),
  Stones 'n Bones' match start (0x090a, pfr's 0x0910, where the table's own rules play it).
  pfr's bytes look like the records (`soundbytes.cpp`). Only `test_sound.cpp` reads these.
- Comments corrected from pfr: the ball *leaves* the plunger lane at Stones 'n Bones' 0x1738 and
  the Gameshow's cs:1077; Speed Devils' rows of three are the BUR and NIN targets, and cs:2281
  counts miles; the Gameshow's "loop" in `countSkills` is the skills ramp; SB16.SDR's function
  0x18 turns the timer's callbacks on and off; a flipper's pieces are the flippers', not the
  plunger's.
