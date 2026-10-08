# Where the game differs from the original, on purpose

The engine was first written to match the DOS game exactly: its memory was compared with the
original program's, byte for byte, after every frame, over hundreds of runs of a hundred
thousand frames on each table (see [own-engine.md](own-engine.md), "The referee"). With its
switches off it still does, and that check is still run.

Everything below was decided afterwards, knowingly. The reason is the same for all of it: the
games already played and kept, and the scores already sent online, were recorded with the
legacy engine, and they must play again to the same score. So where the legacy engine played
differently from the original, this version plays as the legacy engine did. Each difference is
behind a switch in `Engine` (`keysFirst`, `stepsTogether`, `wholeGains`, `servesAlike`,
`amended`, `seedChance`), which `TableGame` turns on; the place in the code says what the
original does there.

The proof that it holds is in `tests/recordings`: eight games recorded by the legacy engine,
on all four tables and by several players, which the test `kept_recordings_play_again_exactly`
plays again to the same score, the same last frame and the same events. Every game on the
online high-score list at the time of writing (17) was played again the same way.

## The frame

| This version | The original |
|---|---|
| Keys are read before the frame is played: a key acts in the frame it was pressed before. | Keys are read after the frame's two callbacks: a key acts one frame later. |
| The ball's four moves of a frame all come first, the rules after them. | Two moves, then the rules, then two more part way down the frame. |
| A shake of the table is counted after the frame's moves: a tilt takes the flippers from the next frame. | Counted when the key is read. |
| Every ball is served alike. | The first ball of a game is kept back half a second longer, while more players may join. |
| A timer started by another timer waits for the next frame. | It runs in the same frame or the next, depending on where it found a free place in the list. |
| A table left from the question asked while nobody plays fades with the question on its display. | The display's script goes on to its next step as the answer is read, and the table stops part way through that step's first frame of drawing, which is what fades. |
| A P or an M pressed while a ball is being lost is dropped. | It is kept, and pauses the game or switches the music when the next ball comes. |

## Chance

| This version | The original |
|---|---|
| A generator begun from a number each game is given; one number drawn at each moment the game asks. | A count of the turns of the program's own loop, which goes as fast as the machine does. |
| The ball's spin is drawn when it sets off from where it was put. | Drawn afresh all the while no ball is in play. |
| Party Land's arcade gives one of its six prizes with equal chance. | By a count of the frames played, through a list of 128. |
| The Gameshow's wheel stops where the generator says. | Where the loop's count says. |

## The ball

| This version | The original |
|---|---|
| A soft hit on steel trades spin and speed as the material's numbers say. | The numbers are scaled in 16 bits and overflow for all but the hardest hits, so the trade is larger and can change sign. |
| Slopes and materials are read from the table's own masks. | From copies in the video card's memory, parts of which the program also keeps pictures over: near the top of the ramps the ball can meet the wrong material or slope. |
| The slope under the ball is a property of the table. | Looked for in the nearest stretch with nothing solid in it at that moment, a flipper's shape included. |
| A flipper's shape is in the collision mask wherever the flipper stands. | Only updated while the ball is inside a box round the flipper; the ball's edge can meet the flipper as it last stood. |
| A contact angle just short of the full turn uses the first of the 44 points round the ball. | It reads a 45th point from whatever follows the list, so a bumper or slingshot there does not kick. |
| A lost ball leaves the next one the pull of the bottom of the table. | The pull of wherever that table puts a lost ball away. |

## The rules

**All tables**

| This version | The original |
|---|---|
| The sound of a new ball coming up is heard 45 frames after the ball is asked for. | 50 frames on three of the tables, 45 on Stones 'n Bones. |
| Half a second before the next ball. | Half a second on Party Land and Stones 'n Bones, a whole second on Speed Devils and the Gameshow. |
| The match starts from a digit drawn by chance and never shows the same digit twice running (the next one up is shown). | Speed Devils may repeat a digit; the Gameshow shows nine minus it. |
| The match begins in the frame its step is reached. | One frame later. |
| After the match the music is silent until the next game's music. | The Gameshow names a place past the end of its music, so its music starts again from the top. |
| A piece of music a display script asks for returns, when it is over, to where the music was when it began. | To the place the table had last been told to go back to. |
| A tilt puts the lights out but does not forget them: the lights a player keeps from ball to ball are still theirs after a tilted ball. | A tilt forgets every light, so those are lost with the ball. |

**Party Land**

| This version | The original |
|---|---|
| The arcade's prize is given at once. | A frame later. |
| After the arcade the music starts again from the beginning of the table's tune. | It goes on from where it was when the ball went in. |
| The ball leaves the arcade when the prize's time on the display is up, once its jingle has been heard to play. | When the jingle is over and the time is up. |
| The jingles of a lost ball, of the arcade and of its prizes keep their priority while they play. | Their priority is put back to nothing (or to one) as soon as they start, so a lesser jingle can cut in. |
| A flipper pressed while the four lane lights are held is lost. | It is kept, and moves the lights when they are let go. |
| A flipper pressed while the flippers are dead does not count for the lights (all tables). | It counts. |
| The match runs through 22 digits. | 23 at this screen speed. |
| The skill shot's award is given once. | Twice. |
| When the ball is put in its hole at the top (arcade, tunnel, secret), the screen is held where it is from that moment and taken up from that frame on. | The screen is taken up by timers that start a frame or two later; until then it sets off after the ball. |
| After the side lane's extra ball, a ball that drains is not taken as lost for ten seconds. | For ten seconds, or until the award's jingle is over, whichever comes first. |

**Speed Devils**

| This version | The original |
|---|---|
| Both rows of three targets score 7,510 and 550 of bonus. | The right row scores 7,520 and 570. |
| A bonus earned is added as many times as the bonus multiplier stands at, and the whole is multiplied again when it is counted. | Added once; multiplied only when counted. |
| The pit keeps the ball a third of a second, or a second and a third while a mode's clock runs. | A third of a second and then a second more, unless a mode's clock runs. |
| The four lights towards the next gear start afresh with every ball. | They are kept from ball to ball. |

**Billion Dollar Gameshow**

| This version | The original |
|---|---|
| A ball let out of the vault or off the wheel keeps the spin it came with. | The same, but the spin had been drawn afresh all the while it was held. |
| For the second that the pair of door targets flashes after both were hit, another hit on either counts as both again. | It counts as one. |

**Stones 'n Bones**

| This version | The original |
|---|---|
| A lone player is given the same two seconds between balls as several players. | A lone player's wait there is cut to two frames. |

## The music and the sounds

| This version | The original |
|---|---|
| The table's sound effects are played a semitone higher. | At the note the table names. |
| An effect cut off by a different one plays on to its end beside it, on a voice of its own, so two effects can sound at once: a flipper no longer silences a slingshot. The same effect again (a bonus being counted, a bumper hit twice) starts over as in the original. | The table has one channel for its effects, and each new effect stops the one playing. |
| Each effect is heard a fixed time after the frame that asked for it: one helping of sound (512 samples, about 11 ms) later, at its place within that helping, so every effect is equally late and a frame run late does not bunch its sounds together. | An effect starts with the next stretch of sound the driver mixes, up to a tick (20 ms) after it was asked for. |
| A jingle starts at the very next tick of the music. | The row of music in progress is played out first: an eighth of a second later. |
| The table reads the music through a view taken at the start of each frame, which recordings note. | The sound card's interrupt changes the table's memory whenever it comes. |

## The picture

| This version | The original |
|---|---|
| The ball and the flippers are drawn where the frame leaves them (a ball put away out of play, where it was last drawn). | Each is drawn at a moment of its own in the frame, some before the ball's last moves and some after, so either can show a step behind. |
| Where the plunger has moved down from, the artwork shows. | The plunger leaves a dark line there, seen on Speed Devils as each ball rolls onto it. |
| The table's top row of dots is the artwork's. | The program rubs it out as the table starts, and a black line shows whenever the screen is at the top. |

## Speeds and screen sizes

| This version | The original |
|---|---|
| Every table runs at the speeds of the screen shown 60 times a second; the size of the picture (normal, high, full, tall) changes only what is drawn. | The 350-row screen is shown 70 times a second and has its own set of speeds. |
| Two more sizes show the whole table and the dot matrix at once, and never scroll: full, with the 350-row screen's pixels, wider than tall; and tall, with square pixels, as the 240-row screen has them, which nearly fills a wide screen turned on its side. | Two sizes of screen, both following the ball up and down the table. |
| The screen follows the ball by a rule of this version's own, the same for every size of screen: towards the ball as fast as the scrolling option says, held where the table says (the Gameshow's wheel, the bottom of the table when a ball is lost), taken up from where it is after Party Land's holes, and drifting up and down the table while nobody plays, starting upwards. A game takes over where the screen was on the table it was started from. | The program's own following, made for its one size of screen, from which the larger screens can only be scaled. |

## Known small differences from the legacy engine

These do not change a score or the ball's path:

- At the start of a ball on Speed Devils the place the music would go back to is set differently
  for a frame; nothing is waiting to go back at that moment, so it is never heard.
- One recording made before a correction to the legacy engine (a bonus counted twice after a
  top score on the match ball) plays again as the corrected legacy engine plays it, not as it
  was recorded.
