# How the engine is written

The engine is written from the original game's own programs, and from nothing else. This is
how, and how it is checked.

## The rule

Code is written from the disassembly of the game's files (`re/fantasy`, made by
`python3 re/disasm.py <the game's folder>`; it stays on the machine, like everything else made
from the game's files) and from our own notes. Every address, constant and rule in it says
where in the original it comes from: a comment "cs:1234" is a place in the program's listing.

## The referee

`encore-oracle` (tools/oracle) runs the game's own table programs in a small machine of ours:
an 80186, the video card's planes and registers, the DOS and launcher calls the tables make,
and the silent sound driver (NOSOUND.SDR) done by hand from its listing. It is a test tool and
is never part of the game. What it does with the same keys is what the engine must do.

## The tables

The original keeps everything a table knows in one data segment, and its scripts, lists and
tables are data in that segment too. The engine keeps the same memory: a table starts from
the bytes of the program's data segment, and each routine of the original is written again by
hand as a C++ function that reads and writes it (`src/engine/table`: `PartyLand`,
`SpeedDevils`, `Gameshow`, `StonesNBones`, on the `Flow` they share and the `Engine` under
it). That has three consequences:

- **Nothing is transcribed.** Scripts, trigger lists, light tables and starting values are read
  from the player's own files, where the original reads them.
- **Everything can be checked.** After every frame the engine's memory is compared with the
  referee's, byte for byte; a difference names the variable and the frame.
- **One engine, four tables.** The four programs hold the same engine at different addresses.
  Names (`re/symbols`) are given to Party Land's addresses as they are understood, and
  `re/align.py` carries them to the other three.

```bash
build/encore-oracle-table <the game's folder> <table 1-4> 100000 --all --start --wild --rough --flip <seed>
```

`--flip` plays a game at random; `--wild` adds nudges, pause, typed letters and more players;
`--rough` throws the ball into the table's own trigger zones, shakes the table until it tilts,
switches lights the rules remember, and starts the best scores at nought so that initials are
asked for. All of it is done alike to the original and to ours. `--picture` compares the
screens too, dot for dot. `ENCORE_COVER=1` lists the routines of ours a run never reached.

One thing the original does is not followed past the point where it goes wrong: while more
players may still join, the start keys also let the number of players be put *below* the player
whose turn it is. The original then counts players on past the eighth and reads and writes past
their records, and what follows is whatever was in that memory. The test stops there, and the
game refuses that key.

## The game

`TableGame` (`src/engine/game`) joins a table with the sound driver written from SB16.SDR
(`src/engine/audio/MusicDriver`) and with its picture (`src/engine/view/TableScreen`), takes
keys, and adds what this version has that the original has not: the options changed while
paused, the steeper angle, the whole table on one screen, what the high-resolution pictures
need, and the question whether a best score goes online. A game is recorded as its start, its
keys and what it saw of its music (`Recording`) and plays again to the same end:

```bash
build/encore-play <the game's folder> <table 1-4> 60000 <seed>     # games by keys at random, twice, and from their recording
build/encore-play <the game's folder> --verify <file.RPL>          # as the server checks one
```

The online scores are checked by `encore-play` built from the newest code on main, and the
games it checks were played on released versions. So a recording must keep playing to the same
end on whatever comes after it: a change to how games play is one main must not take, unless it
comes with a new recording format (`Recording::kFormat`, which the server also has to accept)
and the older formats still play as they did. The guard is `kept_recordings_play_again_exactly`,
over the games in `tests/recordings` (players' games from the online board among them: adding a
few now and then keeps the guard wide), and CI runs it on all three systems with the game's
table files fetched from the server as the score checking does; with them asked for
(`ENCORE_REQUIRE_DATA`) and missing, the tests fail rather than pass by being skipped.

## What this version changes on purpose

The engine by itself is the original's, to the byte: that is what the referee checks. The game
then turns on a handful of switches for this version's own decisions. They are listed, each
with what the original does instead, in [differences-from-the-original.md](differences-from-the-original.md).

## The slides and the menu

`Front` (`src/engine/game`) is INTRO.PRG's one long routine written the way the original has
it: it draws into a model of the video card's memory and is left wherever the original waits
for the next frame. The question out of the manual that the original asks is not asked.
`encore-front` writes frames of it as pictures.

With the whole table on one screen chosen in the options (full or tall), the menu's screen is as tall as that
one (640 by 960): the slides stand in the middle of it, the panel on the left is drawn out to
its length with its picture and its text both there, and beside it are all four tables'
banners on one page and all four lists of best scores on one (`Front::drawTallMenu`). The
card's memory and the routine are as ever; only what is made of them for the screen differs.
In high resolution the panel's plain stretch is `left_repeat.png`, a strip repeated down it; a
set of pictures without one has a single line of `left.png` drawn out there instead.
