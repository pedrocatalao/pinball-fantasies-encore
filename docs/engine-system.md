# System layer: processes, sound driver, input, timing, menus

Addresses: `INTRO cs:` = `re/INTRO_cs.lst`, `T1 cs:` = `re/TABLE1_cs.lst`, `EXE:` = file offset in
PINBALL.EXE. Driver offsets refer to the *unpacked* driver images, because every `.SDR` file is an
EXEPACK-compressed executable; `re/unexepack.py` unpacks them. The listings and dumps under `re/`
are made from your own copy of the game by the tools there (`re/disasm.py`), and are not in the
repository: they are derived from the game's files, which are never redistributed.

## Process structure

```
PINBALL.EXE stays resident and installs int 9, int 65h and int 24h
  loop:
    exec INTRO.PRG      -> nonzero exit quits to DOS
    table = launcher variable; 0 quits
    exec TABLEn.PRG     -> nonzero exit quits, zero loops back to the intro
```

Both the intro and each table launch the sound driver named in SOUND.CFG themselves. The driver
goes resident and installs int 66h; each program unloads it again on exit.

TIMER.BIN is a standalone CPU speed benchmark, not used by the game itself.

## int 65h, launcher services, dispatched on AX

| AX | In | Out | Meaning |
| --- | --- | --- | --- |
| 0x0000 | | AH = first-run flag, BL = table number | query launcher state |
| 0xFFFF | BL = table 1..4, or 0 to quit | | request the next program |
| 0x0100 | ES:BX = 6 bytes | | store the configuration |
| 0x0200 | ES:BX = buffer | filled | read the configuration |
| 0x0012 | | AL = last key scancode, then cleared | read and clear the key latch |

The launcher's own int 9 records make codes only and does not chain to the BIOS.

## int 66h, sound driver services, dispatched on AL

| AL | Inputs | Meaning |
| --- | --- | --- |
| 0x00 | | shut down and unload the driver |
| 0x04 | BX = buffer size in 1/50 s ticks, CX = mixing rate | start module playback; AL returns 0 ok, 1 out of memory, 2 no sound card |
| 0x06 | CX = volume, 0x400 is unity | set master volume, used for fades |
| 0x08 | | poll and top up the mixer |
| 0x0B | ES:DX = callback, BL = priority | install the frame callback and calibrate the timer |
| 0x0C | CX = scanline, BL = priority, ES:DX = callback | add a mid-frame callback |
| 0x0F | | stop the music and free the module |
| 0x10 | BX = song position + 1 | jump to a song position at the next tick |
| 0x11 | DL = channel 1..4, CL = sample 1..31, BL = note 1..36, BH = volume | **play a sound effect** |
| 0x12 | DS:DX = filename | load a MOD file |
| 0x13 | ES:DX = callback | called when the music executes a position jump |
| 0x16 | | read the 50 Hz tick counter |

Note numbering is 1-based, where 1 is C-1 at Amiga period 856. Sample numbering is 1-based as in
the module's instrument list.

### Timing

The frame clock is the VGA vertical retrace. The driver measures the PIT rate against the retrace,
then schedules one-shot PIT interrupts per registered scanline, so each callback fires exactly once
per video frame at its scanline. The BIOS 18.2 Hz tick is displaced while this runs.

Music tempo is independent of video: the mixer tick is the sample rate divided by 50, so **50 Hz**,
paced by DMA consumption. Tempo commands setting beats per minute are ignored; only speed in
ticks per row is honoured.

If a callback returns 0x3039 it is telling the interrupt handler that the mixer has already been
serviced.

## Sound effects in Party Land

Effect records are four bytes (sample, note, 0, channel-1) at `ds:0xc19` onward, all on channel 4.
Relevant samples in TABLE1.MOD:

| Sample | Name | Used for |
| --- | --- | --- |
| 22 | BRICKNEDGANG | drop target falling |
| 23 | BRICKORUPP | targets raised, ramp eject |
| 24 | BUMPER | pop bumpers, three pitches |
| 25 | FLIPPERUPP | flipper press |
| 28 | NEWBALL2 | ball serve and kick-out |
| 29 | SIDOBUMPER | slingshots |
| 30 | UPPSKUTARE | plunger launch, volume proportional to pull |

The flipper effect fires from the keyboard interrupt at `T1 cs:0x3EE0` and `0x3F1E`, and only when
the flippers are enabled. The plunger effect at `cs:0x5EDC` scales its volume by twice the pull
counter.

## Keyboard

Tables install their own int 9. Keys are read as raw scancodes.

| Key | Action |
| --- | --- |
| Left Shift, Left Ctrl, Left Alt | left flipper |
| Right Shift, Right Ctrl, Right Alt | right flipper |
| Space | nudge, held; each press adds 60 to the tilt counter |
| Down arrow | pull the plunger while held, launch on release |
| F1 to F8 | start a game with that many players |
| Enter | add a player, up to 8 |
| P | pause; music stops and any key resumes |
| M | toggle in-game music |
| Esc | quit prompt while paused, and used during name entry |
| F9 to F12 | scrolling presets |

A scancode-to-ASCII table at `ds:0x3656` covers the letter rows only, with space mapping to an
end-of-name marker.

## PINBALL.CFG, six bytes

| Byte | Option | Values | Effect in a table |
| --- | --- | --- | --- |
| 0 | balls | 0 = three, 1 = five | balls per game |
| 1 | angle | 0 = high, 1 = low | adjusts four physics records |
| 2 | scrolling | 0 hard, 1 medium, 2 soft | scroll follow speed 20 / 11 / 9 |
| 3 | in-game music | 0 on, 1 off | |
| 4 | resolution | 0 normal, 1 high | 320x240 at 60 Hz with 5/6-scaled constants, or 320x350 at 70 Hz |
| 5 | colour | 0 colour, 1 mono | grey palette |

The file currently in the game folder reads five balls, low angle, medium scrolling, music on,
high resolution, colour.

This version writes two values the original never does: angle 2 for its steeper "higher" (which
the DOS game reads as high), and resolution 2 or 3 for the whole table on one screen (full and
tall); see `src/game/Config.cpp`.

## SOUND.CFG

Bytes 0 to 12 are the driver filename. Byte 14 selects the sound card base port, byte 17 selects
the mixing rate from a table where index 4 is 44000 Hz. The current file selects port 0x220 at
44000 Hz.

## High scores

`TABLEn.HI` is read when a table starts and rewritten when it exits. The intro only reads all four
for display and never writes them.

## Intro and menu flow

The intro runs only on the first launch in the original; the remake plays it on every launch and
`--skip-intro` goes straight to the chooser.

Its three logo screens are packed into **one tall 320-wide buffer** and shown by moving the display
window, never by redrawing. Six pictures are placed into that buffer, and the window then sits at
three positions:

| Buffer row | Screen |
| --- | --- |
| 0 | 21st Century Entertainment |
| 247 | Digital Illusions |
| 492 | Frontline Design |

Each screen takes the palette of the picture that fills it, and the pictures pair up into exactly
three palette groups, one per screen. After the logos come a title card and the wide Pinball
Fantasies logo in the 16-colour mode.

Timing is driven by the music, not by frames: the original held each screen until the replayer's
tick counter, running at fifty ticks a second, reached 302, 620 and 891, and ran the closing logo
out to tick 1500, which is thirty seconds. Any key skips the whole sequence.

**Known issue.** The 21st Century screen shows colour speckles. Its picture uses palette entries 32
to 63 for an effect, and those entries are placeholders in the stored colour map, black and green,
which the original fills in at run time. The eagle's own shading ramp, entries 0 to 18, is correct,
so only scattered pixels are wrong. The other two logo screens and the title are exact.

The menu offers F1 to F4 to launch a table, F5 for options, space to switch the high-score page,
enter to cycle the information pages, and escape to quit. Launching fades the palette and the
volume over 80 frames, unloads the driver, writes PINBALL.CFG and exits.

The original also contains a manual-lookup copy protection keyed off a CMOS byte and two bytes
stored inside INTRO.MOD, which explains why that file has a modern modification date in this
installation. The remake does not reproduce it.

## How a table names its sounds

A table carries no sound files. Everything it can play lives in its own music module, and the
table's code just names positions and notes.

**Music and jingles** are three-byte records: a song position to jump to, a repeat count, and a
priority. Every table has a silence, a plunger tune for while the ball waits in the lane, a main
tune for play, an attract tune, tilt warning and tilt, two game-over tunes, a drain jingle, and
the match sequence. A jingle interrupts the background music and the player returns to it
afterwards. Priority decides which of two simultaneous events is heard: a quieter one is silenced
and only its score is awarded.

**Sound effects** are four-byte records: a sample number, a note index, a zero, and a channel.
They play a single note without stopping the music, and they always use the fourth channel, which
is why the background music and jingles are written for three. The attract tune is the exception
and may use all four, since no effects can happen there.

Party Land's flipper effect names sample 25, which the module calls `FLIPPERUPP`, at note 22.
Every table's eight common effects were checked against the samples its module actually contains.

Bumpers and slingshots do not use the shared effects. Their element records point at their own,
which is how the three pop bumpers get three different pitches.
