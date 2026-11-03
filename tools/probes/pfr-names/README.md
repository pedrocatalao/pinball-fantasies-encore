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
