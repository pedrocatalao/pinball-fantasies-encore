# Graphics: screen layout, animation formats, dot-matrix display

Recovered from the original binary. Working decoders that produced verified images live in
`re/agent_gfx/` (`dmd2055.py` for the display bank, `flippers.py` for the table animations):
local reverse-engineering scratch, not in the repository, as everything under `re/` but its tools.

## Screen layout

The game runs in an unchained VGA mode with a row pitch of **84 bytes, that is 336 pixels across
four planes**. Only the left 320 pixels are shown.

| Rows | Content |
| --- | --- |
| 0 to 32 | dot-matrix display, 33 rows |
| 33 onward | the scrolling window onto the 320x576 playfield |

**On screen the order is reversed.** A hardware line-compare makes the display jump back to the
start of video memory partway down the screen, so the dot matrix appears at the **bottom** and the
playfield above it. Wanting the dot matrix at the bottom is in fact why the original used that
trick; the Amiga version has it at the top.

In the shipped high-resolution mode the screen is 350 lines at about 70 Hz, giving 33 display rows
plus 317 playfield rows. The alternative is Mode X at 320x240 and 60 Hz, where every physics
constant is scaled by 5/6.

A VRAM address maps to a pixel as `row = addr / 84`, `byteColumn = addr % 84`,
`x = byteColumn * 4 + plane`, `y = row - 33` in playfield coordinates.

## Table animations: delta lists over a pixel dictionary

This is how the original animates flippers, drop targets and lamp inserts without redrawing
regions. Two blocks work together.

**The dictionary** lives in the small 1760-byte block (segment 0x8273 in table 1). It holds **437
entries stored plane-major**: entry *i* is the four bytes at offsets *i*, *437+i*, *874+i* and
*1311+i*. Each entry is therefore one group of four horizontally adjacent pixels, one per plane.

**The delta blocks** live in the large table block (segment 0x0c0e in table 1). Each block is:

```
word   count
9 words of header in total, the first being count
count pairs of:
    word  offset      VRAM offset relative to the animation's base address
    word  dictEntry   dictionary address; index = value - 0xd4f4
```

Applying a block writes each dictionary entry's four pixels at the decoded position. Blocks exist
in forward and backward variants, so an animation can be played in either direction and returns
exactly to its starting image, which the flipper decoder verifies.

The flipper records carry the animation fields alongside the physics fields:

| Offset | Meaning |
| --- | --- |
| +0x30 | base of the backward copy lists |
| +0x36 | base of the forward copy lists, plus eight bytes per flipper index |
| +0x38 | stride between consecutive frame lists |

Rebuilding a flipper's frames: start from the playfield cropped to the flipper's rectangle, then
for each step apply that step's copy list on top of the previous frame. A copy list is a count
word, then **eight more words of bookkeeping**, then the entries, so the first entry is at offset
0x12. Each entry's destination is a video offset relative to the flipper's own corner, giving
`x = (destination % 84) * 4` and `y = destination / 84`, and its source names a group by address,
so the group index is `source - 0xd4f4`.

The copy lists live in the table's large animation block and the pixel groups in the small block
before the plunger. Both are addressed from the start of their own block. The number of groups
differs per table: 437, 42, 55 and 30.

This yields 21 frames for each main flipper and fewer for the upper one, matching the original.

## Dot-matrix display bank

The sprite bank block (segment 0x2055 in table 1, about 62 KB) is the **dot-matrix animation
library**. It begins with a directory of sequences:

```
sequence header (3 words):  loopStart, count, endOffset
followed by pairs of:       word frameOffset, word durationInFrames
```

The number of pairs is `endOffset / 4`, plus one when the repeat count exceeds one. `loopStart`
divided by four gives the pair the sequence loops back to.

Each frame is a run-length delta over the display buffer, encoded per plane for **planes 0 and 2
only**, which is what gives the display its two-pixel dot pitch:

```
word count
then count bytes, starting from cursor 0xa7:
    byte & 1        -> cursor += byte >> 1, light this dot
    byte >> 1 == 0x7f -> cursor += 0x7f, touch nothing
    otherwise       -> cursor += byte >> 1, clear this dot
```

The cursor addresses the 84-byte rows directly, so `row = cursor / 84` and `x = (cursor % 84) * 4 +
plane`. Frames accumulate: each one is a difference against the previous image, not a full picture.

Decoding the whole bank yields the Party Land display vocabulary: HAPPY HOUR, CYCLONE, PARTY,
MEGA LAUGH, MILLION, JACKPOT, the 2X through 8X BONUS multipliers, EXTRA BALL, the slot-machine
reels and a set of zooming digit animations.

## Ball: a compiled sprite

The ball's pixels are **not stored anywhere in the file**. The original holds a routine whose
instructions carry each pixel's colour as an immediate operand, a technique known as a compiled
sprite, which was the fastest way to draw through the plane-switching of an unchained video mode.
Decoding that routine recovers the image.

Every pixel is written by one `mov byte [si + displacement], colour`, in one of two encodings:

| Bytes | Meaning |
| --- | --- |
| `C6 44 dd cc` | byte displacement |
| `C6 84 lo hi cc` | word displacement |

The displacement is a video memory offset, so `row = displacement / 84` and
`column = displacement % 84`, and the pixel's x is `column * 4 + plane`. The writes come in four
sections, one per plane, separated by the code that selects the next plane; those breaks are far
wider than the gaps within a section, which is how the decoder tells them apart.

The result is a **15 by 15 shaded sphere of 177 pixels**, lit from the top left. Every table draws
the identical silhouette; only the shading colours differ, because each table has its own palette.
The upper layer reuses the same image, shifting only the occlusion lookup.

Three further 320x576 masks are copied into off-screen video memory at start-up and used to occlude
the ball where it passes under ramps, and their low nibbles double as the gravity-zone map
described in the physics notes. The ball graphics block serves two roles at once: a sequential
save area for the background behind the ball, and a position-addressed copy of the occlusion mask.

## Plunger

A 230-byte block holds the plunger as **10 by 23 chunky pixels**, one byte per pixel. It is drawn
at x = 304, and the pull counter slides it down the screen by `(pull / 2) - 3` rows, so drawing the
plunger back moves it downward.

## Camera

The playfield window is **317 rows** in the 350-line mode and 207 rows in the 320x240 mode. The
camera keeps the ball 130 rows below the top of the window, 75 in the smaller mode, and eases
toward that target rather than snapping:

```
target = clamp(ballY - lead, 0, 576 - windowHeight)
cameraPosition += (target - cameraRow) * scrollSpeed / 4      // position in 1/16 rows
```

`scrollSpeed` is the configuration value, 9 soft, 11 medium, 20 hard, with 40 available from a
function key; a larger number makes the view snappier. After the ease, a rubber band bounds the
remaining error: the camera may not sit more than 130 rows ahead of its target, nor more than 170
behind. The nudge lift is added to the final row, which is what makes the screen shake. The
original realised all of this by writing the CRTC start address, so no pixels were ever copied.

## Palette

Each playfield strip carries a 256-colour map and sixteen colour-cycling ranges in Deluxe Paint
format. The engine does not use the ranges: the colours shown are the 256 the table program itself
sets in the video card's palette, frame by frame (`Engine::colours`, read by
`TableScreen::colours`), cycling included.

## Presenting the picture

The playfield screen is 320 by 350 and the original filled a 4:3 display with it, so its pixels
are 1.458 times wider than they are tall. That shape is authentic, and the alternative 320 by 240
mode has square pixels instead.

Scaling that to a modern window needs care. An earlier version of the presentation shader sampled
at pixel boundaries rather than pixel centres, so linear filtering blended neighbouring pixels
across almost the whole screen and the picture looked soft. Sampling the centre of the source pixel
each output pixel falls inside gives a result identical to nearest-neighbour, and only the thin
band straddling an edge is blended. Measured on a 1920 by 1440 drawable, horizontal edges come out
with no blending at all, because the horizontal scale happens to be exactly six.

## Lights

The playfield picture never changes. Only four things on it move between frames: the lights,
the flippers, the ball and the plunger.

Lights are not drawn at all. **Each light owns a short run of palette entries**, and switching it
on or off just rewrites those colours: full strength when lit, halved when not. That is the whole
mechanism, and it is why a table with dozens of blinking inserts costs the original nothing per
frame.

Each table has an array of pointers to light records. A record is one byte giving the first
palette entry the light owns, one byte giving how many entries it covers, then that many colour
triples. The components are stored on a 0 to 95 scale, expanded to the palette's range by
multiplying by 162 and shifting right six places.

| Table | Pointer array | Lights |
| --- | --- | --- |
| Party Land | 0x12bd | 56 |
| Speed Devils | 0x0fdd | 67 |
| Billion Dollar Gameshow | 0x0d8b | 38 |
| Stones 'n Bones | 0x11d0 | 44 |

Most lights cover one or two entries. A few cover many: one light on Stones 'n Bones spans
seventeen, because its ghost artwork is recoloured wholesale.

Rendering a frame is therefore: copy the table's own palette, then for every light write its
colours at full or half strength. About four percent of the playfield's pixels are under light
control.

## Dot-matrix display

A grid of **160 by 16 dots** shown in the 320 by 33 pixel band at the bottom of the screen. Each
dot is a single pixel, placed on every other column and every other row, which is what leaves the
dark gaps between them. The band's background is black, and two palette entries carry the lit and
unlit dot colours, so the original can blink the whole display by rewriting one entry.

There are four fonts, 5, 8, 11 and 13 dots high. Each holds forty glyphs in the order
`0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ?()-`, stored as one byte per row with the leftmost dot at
bit 0x80. Characters are seven dots wide in a cell of eight, the last dot being the gap. There are
no lowercase letters, which is why every message in the game is in capitals.

Font addresses in each table's data segment:

| Table | 5 high | 8 high | 11 high | 13 high |
| --- | --- | --- | --- | --- |
| Party Land | 0x6710 | 0x65d0 | 0x6410 | 0x6200 |
| Speed Devils | 0x67a0 | 0x6660 | 0x64a0 | 0x6290 |
| Billion Dollar Gameshow | 0x5ff0 | 0x5eb0 | 0x5cf0 | 0x5ae0 |
| Stones 'n Bones | 0x6d00 | 0x6bc0 | 0x6a00 | 0x67f0 |

The two dot colours sit either side of the light pointer array: the lit colour six bytes before
it, the unlit colour immediately after its last entry.

The original also holds the same glyphs a second and third time as compiled code, for drawing
scores and long scrolling messages quickly. Those are redundant for us; the plain font tables give
the same shapes.

## Menu font

The two character strips the menu keeps off screen are the front-end font: 640x28 and 640x29
pictures holding **40 glyphs in a grid of 20 columns by 2 rows**, one cell every 32 pixels
horizontally and 14 rows vertically, with about 17 pixels of ink per glyph. The order is the
ten digits, the twenty-six letters, and four symbols. The two strips are the same glyphs in
two shades, which is how the menu highlights a line.
