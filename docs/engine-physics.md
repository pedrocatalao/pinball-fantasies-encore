# Ball physics and flippers (TABLE1.PRG, Party Land)

Source of truth: the 1994 binary. Addresses are `cs:` offsets in the code segment (`re/TABLE1_cs.lst`)
or `ds:` offsets in the data segment (file offset 0x19d40 + off). Units are table pixels in a
320x576 space, y downwards. Facts marked LIKELY are inferred rather than traced.

## Execution model

Physics does not run from the main loop. The main loop only reads keys and idles. Two callbacks are
registered with the sound driver at `cs:0x5fcb`, and the driver fires them from a PIT interrupt
phase-locked to the vertical retrace:

| Callback | Registered | Fires at | Work |
| --- | --- | --- | --- |
| #1 `cs:0x41c1` | int 66h fn 0x0B, priority 0x64 | start of frame | 2 physics sub-steps, sensors, events, display |
| #2 `cs:0x55d6` | int 66h fn 0x0C, priority 0xC8, scanline 0x108 or 0xAE | mid-frame | scroll, 1 or 2 sub-steps, ball draw |

They strictly alternate (`cs:0x4446`). Per frame that is **3 sub-steps in 350-line mode, 4 in
240-line mode**. Video mode depends on config byte 4 (`ds:0x3556`): set means 320x350 at about
70 Hz, clear means Mode X 320x240 at about 60 Hz with every constant scaled by 5/6.

So the simulation runs at **210 sub-steps per second** in the shipped configuration.

## Ball state

| Address | Meaning |
| --- | --- |
| `ds:0x2ee2` / `0x2ee4` | x / y, integer, top-left of the 16x16 sprite; collision centre is (x+7, y+7) |
| `ds:0x2ee6` / `0x2eea` | x / y in 22.10 fixed point |
| `ds:0x2eee` / `0x2ef0` | vx / vy, signed 16-bit, units of 1/1024 pixel per sub-step |
| `ds:0x2ef6` / `0x2ef8` | gravity gy / gx for this frame, from the zone map |
| `ds:0x2ee0` | spin (surface speed), decays by 2 per sub-step toward zero |
| `ds:0x331c` | layer: 0 lower, 0xFF upper |
| `ds:0x2ef2` / `0x2ef4` | last contact point |
| `ds:0x2edc` | material class of the last hit |
| `ds:0x6890` / `0x6892` | surface normal angle (0..0x7FF) / number of probes that hit |
| `ds:0x6894` / `0x6896` | extra relative velocity from a moving flipper |
| `ds:0x2316` / `0x2318` / `0x231a` | nudge accumulator / pixel lift (0..4) / table velocity |
| `ds:0x2f2c` | 0xFF means the ball is hidden and physics is skipped |
| `ds:0x239e` | set when y >= 576, the ball is lost |

Ball radius is 8 pixels. The sprite is drawn at (x, y+0x21).

## Integration (`cs:0x907F`), one sub-step

```
y_fixed += vy;  y = y_fixed / 1024      (truncating)
if y >= 576: ball lost
x_fixed += vx;  x = x_fixed / 1024
vy += gy;  vx += gx
spin moves toward 0 by 2
```

There is no air drag. The only velocity clamp is inside the collision response, at +/-4100.

## Gravity zones (`cs:0x59D4`)

Gravity is not constant. The low nibble of the occlusion masks encodes a zone number per byte
column, and the code walks three byte columns at the ball's row, taking the first that is not
solid. Lower layer reads mask 7193, upper layer reads 7cd3.

| Zone | 350-line (gx, gy) | 240-line (gx, gy) |
| --- | --- | --- |
| 0 default | 0, 10 | 0, 8 |
| 1 | 2, 14 | 2, 11 |
| 2 | -2, 14 | -2, 11 |
| 3 | -4, 16 | -4, 13 |

Zone 0 gravity of 10 units is about 441 pixels per second squared. The zones tilt gravity sideways
in the plunger lane and on the ramps, which is how the original makes a ball roll around a curve.

## Collision detection (`cs:0x8819`)

44 probe pixels on a Bresenham circle of radius 8 around (x+7, y+7), tested against the wall mask
of the current layer. Each probe that lands on a solid pixel contributes its exact angle, in units
of 1/2048 of a turn.

| Index | Offset from centre | Angle | Index | Offset | Angle |
| --- | --- | --- | --- | --- | --- |
| 0 | (8, 0) | 0x000 | 22 | (-8, 0) | 0x400 |
| 11 | (0, 8) | 0x200 | 33 | (0, -8) | 0x600 |

The full 44-entry table is at `ds:0x67e0` (and in `re/agent_physics/probes.py`, local
reverse-engineering scratch that is not in the repository).

The surface normal is the mean angle of the hit probes, with a wrap correction when the hits
straddle angle zero. Fewer than one effective hit means no collision.

### Material classes

The pixel under the last hit probe is read from three masks, giving a class number:

| Class | Meaning | Restitution | Notes |
| --- | --- | --- | --- |
| 2 | wall | 0.36 | fast if normal speed > 300 |
| 3 | rubber | 0.707 | bumpers, slingshots, drop targets, posts |
| 6 | marked wall | 0.431 | outer and ramp walls |
| 7 | marked rubber | 0.36 | |

The per-class constants live at `ds:0x231d` as eight records of 16 bytes (w0..w4):

| Class | w0 | w1 | w2 | w3 | w4 |
| --- | --- | --- | --- | --- | --- |
| 2 | 1792 | 448 | 400 | -600 | 18 |
| 3 | 896 | 224 | 875 | -200 | 38 |
| 6 | 10000 | 2500 | 450 | -700 | 38 |
| 7 | 10000 | 2500 | 400 | -500 | 38 |

Global constants at `ds:0x68a2`: velocity clamp -4100 and +4100, bumper kick -7000, slingshot kick
-2000, slingshot minimum -300, nudge lift +600, nudge return -200.

## Collision response (`cs:0x8E85`)

A sine table of 2560 entries at `ds:0x4600` holds sin(i * 2pi / 2048) * 16384; cosine is the same
table offset by 512.

```
v += flipperVelocity + (0, tableVelocity);  clamp
rotate into the normal frame:  vn = 2*(v . normal),  vt = 2*(v . tangent)
if vn <= 0: no collision this step
vn = -vn
if vn >= w3                     -> too slow, vn = 0
else if |16*vt/vn| >= w4        -> too glancing, vn = 0
else if an element event is set -> add the bumper or slingshot kick
vn -= vn * 256 / w2                                  (restitution, also scales the kick)
k = (vn >= -1023) ? ((-vn >> 6) + 1) : 1
d = spin + tableVelocity - vt
vt  += d * 256 / (w0 * k)
spin -= d * 256 / (w1 * k)
vt = vt * 2048 / 2049                                (tangential loss)
rotate back, halve, subtract the flipper and table velocities, clamp
if hitCount >= 6: push the ball 0.25 px out along the normal
```

For a wall, the spin coupling works out as vt += (spin - vt)/7 with spin losing four sevenths of
the difference.

## Flippers

Three records of 0x3C bytes at `ds:0x6950`.

| Field | Left | Right | Upper left | Meaning |
| --- | --- | --- | --- | --- |
| +0x00 | 2 | 1 | 2 | 2 = driven by the left key, 1 = right key |
| +0x02 | 80, 510 | 160, 510 | 16, 227 | mask origin |
| +0x06 | 4 | 4 | 3 | words per row, so 64 or 48 pixels wide |
| +0x08 | 53 | 53 | 51 | rows per frame |
| +0x0A | 0,142,400,576 | 143,320,400,576 | 0,160,0,399 | bounding box |
| +0x12 | 95, 536 | 204, 536 | 27, 234 | pivot |
| +0x16 | 0 | 0 | -1 | vertical flag, swaps dx and dy in the impulse |
| +0x20 | 20 | 20 | 13 | highest frame index |
| +0x24 | -7, 4, -68 | -7, 4, -68 | -8, 4, -68 | angular step up, step down, initial rising speed |
| +0x3A | 0x4c53 | 0x4fc1 | 0x4eb5 | mask segment holding the frames |

Motion, per sub-step: while the key is held the angular speed accumulates by -7 down to a floor of
-68; when released it accumulates by +4. Position changes by minus the speed, and the frame index
is position divided by 55. A flipper rises in 11 sub-steps and falls in 23.

Frame angles measured from the pivot: the left flipper sweeps +34.8 degrees down to -28.9 degrees
over 21 frames, about 3.2 degrees per frame. The right flipper mirrors it. The upper flipper
sweeps 76.9 to 34.9 degrees over 14 frames.

Collision with a flipper is ordinary wall collision: when the ball is inside the bounding box, the
current frame's bitmap is stamped into the wall mask (`cs:0x3D31`). At rest the flipper is simply
part of the wall.

The impulse (`cs:0x8DDF`) uses the offset from the pivot to the contact point. For the left
flipper, contacts left of the pivot are ignored. The effective flipper velocity is a rigid rotation
plus a fudge term: `(-omega * dy, omega * (dx + |dy|/4))`. With the angular speed between 68 and
138 position units per sub-step, a contact near the tip produces 3000 to 6000 units, which the
clamp caps at 4100, about 840 pixels per second.

## Bumpers, slingshots and other elements

There is no circle list. A mask hit of class 7 looks its contact point up in a rectangle list at
`ds:0xcf3`, and class 3 uses `ds:0xd1b`:

| Element | Rectangle | Event record |
| --- | --- | --- |
| Pop bumper 1 | (211,252)-(235,276) | `ds:0xd39` |
| Pop bumper 2 | (268,263)-(292,287) | `ds:0xd47` |
| Pop bumper 3 | (185,283)-(209,307) | `ds:0xd55` |
| Left slingshot | (50,415)-(80,470) | `ds:0xd63` |
| Right slingshot | (219,415)-(249,470) | `ds:0xd63` |

A match sets the pending event, and the response adds the kick along the normal, but only for hits
that are fast enough and not too glancing. There is no cooldown. Once per frame the event is
dispatched to the score and sound routines.

Contact-triggered elements use a second rectangle list at `ds:0xd71`, and position sensors on the
ball centre use `ds:0xd9b` for the lower layer and `ds:0xe29` for the upper, with separate lists
used while tilted.

### Drop targets

Drop targets are patches stamped into the wall mask. Patterns at `ds:0x68b0` onward are copied into
mask 3b73 at three places: (144,277) 16x15, (152,295), and (160,313) 8 wide. The raised pattern is
a 3-pixel diagonal bar plus a 2-pixel wall bar; the dropped pattern keeps only the wall bar. The
upper-level gate works the same way on mask 46b3 at (112,15), 16x18.

## Plunger, ball loss, nudge and tilt

The ball is served hidden at (282,530), then after an 80-frame timer placed at (297,530) with
vx = +10, and zone 3 gravity rolls it onto the plunger. Holding the down arrow increases a pull
counter up to 32. On release the launch velocity is `vy = -138 * pull - random(0..255)` in
350-line mode, or -118 per unit in 240-line mode, with a small random spin. The sound effect volume
is proportional to the pull.

The ball is lost when y reaches 576. Outlanes are ordinary sensor rectangles; there is no generic
kickback in the physics layer.

Nudging with the space bar raises a table velocity of +600 units while held, decaying by 200 when
released, and lifts the whole table under the ball by up to 4 pixels. Each nudge adds 60 to a tilt
counter that decays by 1 per frame: above 60 shows a warning, above 120 tilts, which kills the
flippers and suppresses all bumper, slingshot and contact events.

## Layers

Ramps have no height model. Instead the ball moves between two sets of masks, and the illusion of
climbing comes from the gravity-zone map plus the separate mask set.

The layer flag inverts when the ball centre, taken as (x + 8, y + 8 + nudge lift), enters one of
two lists of rectangles. **Which list is consulted depends on the layer the ball is already on**,
not on the direction of the change: the first list is used while the ball is on the ramp layer,
the second while it is on the playfield. Getting this the wrong way round makes the ball fail to
leave the shooter lane, because the lane's exit is a diagonal deflector that exists only on the
playfield mask, while the lane itself runs straight up the ramp mask.

The two lists sit back to back in the data segment, each ended by a word of -1, with eight bytes
per rectangle. They are found by structure, since no two tables put them at the same address:

| Table | While on the ramp layer | While on the playfield |
| --- | --- | --- |
| Party Land | 0xec5, 10 zones | 0xf17, 5 zones |
| Speed Devils | 0xc54, 8 zones | 0xc96, 4 zones |
| Billion Dollar Gameshow | 0xad8, 10 zones | 0xb2a, 5 zones |
| Stones 'n Bones | 0xc1e, 10 zones | 0xc70, 6 zones |

## Reimplementation summary

```
radius 8, three sub-steps per 70 Hz frame, clamp 4100
frame:
  callback 1: decay tilt; dispatch events; pick gravity zone; 2 sub-steps; sensors; timers
  callback 2: scroll; 1 sub-step; draw
sub-step:
  probe 44 points -> hit count, normal angle, material class, contact point, flipper velocity
  if hit: bounce
  update nudge and flipper angles
  integrate
  stamp the flipper bitmap if the ball is near it
```

## Finding these structures in each of the four binaries

The four table programs were compiled separately, so every structure sits at a different
address in each one. Hard-coding Party Land's addresses works only for Party Land; each
structure can instead be found by evidence, as an earlier model of the ball in this project did
(the engine now needs none of it: it runs each table's own code on its own data):

| Structure | How it is found |
| --- | --- |
| Sine table | By its contents, which mathematics fixes exactly |
| Shared limits | At a constant distance of 0x22a2 from the sine table, which holds in all four binaries |
| Material records | By structure: eight 16-byte records, five signed words then six zero bytes, with the signs the collision code needs. Unique in every binary, and the values turn out to be identical across all four |
| Gravity tables | From the instruction sequence of the table-angle option, which loads both table addresses into BX before walking them |
| Flipper records | From the set-up code that writes each record's frame block, which also reveals the three frame-block references the records themselves do not contain |

Resolved addresses per table:

| | Party Land | Speed Devils | Billion Dollar | Stones 'n Bones |
| --- | --- | --- | --- | --- |
| Sine table | 0x4600 | 0x4690 | 0x3ee0 | 0x4bf0 |
| Materials | 0x231d | 0x212b | 0x1ddd | 0x27a7 |
| Gravity, 350-line | 0x72 | 0x96 | 0x24b | 0xcd2 |
| Flipper records | 0x6950 | 0x6940 | 0x66d0 | 0x7360 |
| Flippers | 3 | 3 | 3 | 2 |

Gravity zones differ per table, as expected, since they are what steers the ball around
each table's ramps:

| Table | Zone 0 | Zone 1 | Zone 2 | Zone 3 |
| --- | --- | --- | --- | --- |
| Party Land | 0, 10 | 2, 14 | -2, 14 | -4, 16 |
| Speed Devils | 0, 10 | 0, 15 | 0, 25 | -1, 10 |
| Billion Dollar | 0, 10 | 4, 12 | 0, 14 | 2, 9 |
| Stones 'n Bones | 0, 10 | -10, 5 | 0, -10 | 5, 0 |

Every table shares the same lower flipper geometry, pivots at (95, 536) and (204, 536) with
21 frames. The third flipper differs: Party Land has an upper left flipper at (27, 234),
Speed Devils an upper right one at (188, 198), Billion Dollar Gameshow one at (286, 182),
and Stones 'n Bones has none.

## Physics maps

Each layer is described by a physics map the same size as the playfield, stored as **three bit
planes**. The planes are not a plain three-bit image:

- **Plane 1 alone says whether a pixel is solid.** Every collision test reads this plane.
- For a solid pixel, the three planes combine into a material: 2 for flippers and gates, which are
  patched in at run time, 3 for the rubber of kickers, 6 for the steel of most of the playfield,
  and 7 for the plastic of bumpers.
- For a pixel that is not solid, the same storage is reused at lower resolution to describe ramps.
  When the *byte* containing the pixel is zero in planes 0 and 1, the byte in plane 2 is the ramp
  index; otherwise the bytes either side are tried. The index selects the incline to apply, with
  index 0 meaning level ground.

The six mask blocks in a table are therefore the two layers' three planes each, and the engine's
plane assignment was confirmed to match the original exactly.

## Triggers

A trigger is a rectangle paired with the address of the routine the original runs for it, ten
bytes per entry, ending at a zero word. There are two kinds:

- **Hit triggers** fire when the ball collides with something solid inside the rectangle.
- **Roll triggers** fire as the ball's centre enters the rectangle, once per entry.

Roll triggers come in four lists: one per layer, plus a separate pair used once the table has
tilted, which is how tilting silences most of the table. Bumpers and kickers are not triggers;
they have their own lists carrying a score and a sound.

| Table | Hit | Roll, playfield | Roll, ramps |
| --- | --- | --- | --- |
| Party Land | 4 | 14 | 12 |
| Speed Devils | 6 | 15 | 4 |
| Billion Dollar Gameshow | 6 | 15 | 8 |
| Stones 'n Bones | 9 | 18 | 15 |

The handler address identifies the element. Party Land's four hit triggers are the arcade button
and the three ducks; its roll triggers include both orbits, the tunnel, the secret passage and the
inner and outer lanes.

**A caution on recorded addresses.** The published addresses for these lists do not all transfer
to a given copy of the game. Party Land, Billion Dollar Gameshow and Stones 'n Bones matched, while
Speed Devils' lists sat one byte earlier, and its light table one byte later. The reader therefore
looks either side of the expected address and takes the longest list that reads cleanly.

## Ball occlusion

Two further bitmaps per table, one per layer, mark where the ball must not be drawn because a
playfield feature passes over it. They live inside the ball-graphics block at offsets 0x580 and
0x6400, the second running past the end of that block into the next. Roughly a third of the
playfield is covered on the ground layer, and far less on the ramps, where only the rails hide the
ball.

## Bumpers, kickers and scoring

Bumpers and kickers are not triggers. They have their own two lists, ten bytes an entry: a
rectangle and a pointer to a record holding the sound to play and the score to award. The lists end
with a word of 0xffff. A bumper is recognised by the plastic material and always throws the ball
back; a kicker is rubber and only fires when the hit is firm enough.

| Table | Bumpers | Kickers | Highest award |
| --- | --- | --- | --- |
| Party Land | 3 | 2 | 1,000 |
| Speed Devils | 4 | 2 | 1,030 |
| Billion Dollar Gameshow | 3 | 2 | 1,000 |
| Stones 'n Bones | 3 | 2 | 1,000 |

Scores are twelve decimal digits, one per byte, which is also the format the high-score files use,
so a score moves between them unchanged.

## Layer changes made by rollover handlers

Most layer changes come from the two transition-zone lists, but two rollovers change the
layer from their handler instead, and the collision maps alone make those paths look like
dead ends:

- **Stones 'n Bones**, handler `0x1738` at the end of the top rail: the ball goes to the
  playfield layer and drops into the key lanes. On the ramp layer the rail is a closed pocket.
- **Speed Devils**, handler `0x11d1`: the ball goes to the playfield layer and its vertical
  speed is zeroed, so it does not fall back into the plunger lane. (A cracked re-release of
  the game has this handler at `0x11bb`; the engine now only accepts the original release.)

## Physics-map patches (gates, drop targets)

A patch record is five words in the data segment: raised shape, dropped shape, position (an
offset into the 40-byte-pitch plane, so `x = (addr % 40) * 8`, `y = addr / 40`), width in
bytes, height in rows. Each shape stores its rows as three interleaved planes; only the second
(solid) plane is written into the map. Stones 'n Bones: Kickback `0x1265`, TowerEntry `0x123d`,
RampTower `0x1233` (playfield layer), RampLeft0/1/2 `0x1247`/`0x1251`/`0x125b` (ramp layer).
A new game raises TowerEntry and Kickback. The engine applies each patch where the table's
own rules do, in `src/engine/table`.
