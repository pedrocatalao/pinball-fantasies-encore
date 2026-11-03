// encore-oracle-table: the engine's memory against the original's, frame by frame.
//
// The referee starts a table and runs its start-up; the engine is given the memory as that
// leaves it. Both are then given the same keys, and after every frame the engine's data
// segment, the variables among its code and its colours are compared with the original's.
//
//   encore-oracle-table <game folder> <table 1-4> <frames> [--keys <frame>:<scancode>,...]
//                       [--flip <seed>]  plays at random from frame 400 on
//                       [--wild]   with --flip: nudges, letters and more players too
//                       [--picture] the screen as ours draws it against the original's, dot for dot
//                                  (ENCORE_PICTURE=<frame> writes both there as ours.png, theirs.png)
//                       [--rough]  with --flip: now and then the ball is thrown somewhere else on
//                                  the table, and the table shaken until it tilts; the best
//                                  scores start at nought, so that every game asks for initials
//                       [--options 001010]  balls, angle, scrolling, music off, resolution, mono
//                       [--start]  the engine starts by itself, and that is compared first
//                       [--all]   goes on after a difference, taking the original's value for
//                                 it, and says each place that ever differs once
#include <cstdio>
#include <cstdlib>
#include <array>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Machine.h"
#include "core/File.h"
#include "engine/table/Engine.h"
#include "engine/table/PartyLand.h"
#include "engine/table/Gameshow.h"
#include "engine/table/SpeedDevils.h"
#include "engine/table/StonesNBones.h"
#include "engine/view/TableScreen.h"
#include "core/Png.h"

int main(int argc, char** argv) {
  if (argc < 4) {
    std::puts("usage: encore-oracle-table <game folder> <table 1-4> <frames> [--keys <frame>:<scancode>,...] [--all]");
    return 2;
  }
  const std::filesystem::path dir = argv[1];
  const int table = std::atoi(argv[2]) - 1;
  int frames = std::atoi(argv[3]);
  std::vector<std::pair<int, int>> keys;
  bool all = false;
  int flip = -1;
  bool ownStart = false, wild = false, rough = false, picture = false;
  oracle::Machine::Config config;
  encore::Engine::Options options;
  for (int i = 4; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--all")) all = true;
    else if (!std::strcmp(argv[i], "--start")) ownStart = true;
    else if (!std::strcmp(argv[i], "--wild")) wild = true;
    else if (!std::strcmp(argv[i], "--rough")) rough = true;
    else if (!std::strcmp(argv[i], "--picture")) picture = true;
    else if (!std::strcmp(argv[i], "--options") && i + 1 < argc) {  // six digits, as PINBALL.CFG's bytes
      const char* o = argv[++i];
      for (int k = 0; k < 6 && o[k]; ++k) config.bytes[k] = static_cast<oracle::u8>(o[k] - '0');
      options = {config.bytes[0] != 0, config.bytes[1] != 0, config.bytes[2], config.bytes[3] != 0, config.bytes[4] != 0, config.bytes[5] != 0};
      ownStart = true;
    }
    else if (!std::strcmp(argv[i], "--flip") && i + 1 < argc) flip = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--keys") && i + 1 < argc) {
      for (const char* p = argv[++i]; *p;) {
        char* end = nullptr;
        const long a = std::strtol(p, &end, 10);
        if (*end != ':') break;
        const long b = std::strtol(end + 1, &end, 16);
        keys.push_back({static_cast<int>(a), static_cast<int>(b)});
        p = *end == ',' ? end + 1 : end;
      }
    }
  }
  const auto prg = encore::file::readAll(dir / ("TABLE" + std::to_string(table + 1) + ".PRG"));
  if (!prg) {
    std::puts("cannot read the table's program");
    return 2;
  }
  int frame = -1;
  try {
    std::unique_ptr<encore::Engine> made;
    if (table == 0) made = std::make_unique<encore::PartyLand>(*prg);
    else if (table == 1) made = std::make_unique<encore::SpeedDevils>(*prg);
    else if (table == 2) made = std::make_unique<encore::Gameshow>(*prg);
    else made = std::make_unique<encore::StonesNBones>(*prg);
    encore::Engine& e = *made;
    oracle::Machine m(dir, table, config);
    m.setLoop(0x10, e.F(encore::at::mainLoop));
    m.loopsPerFrame = e.loopsPerFrame;
    m.boot();
    if (const char* w = std::getenv("ENCORE_WATCH")) e.debugWatch = static_cast<int>(std::strtol(w, nullptr, 16));
    if (const char* w = std::getenv("ENCORE_WATCH")) m.watchWrite = (oracle::u32{m.seg(e.dataSegment())} << 4) + static_cast<oracle::u32>(std::strtol(w, nullptr, 16));
    const oracle::u16 ds = m.seg(e.dataSegment()), cs = m.seg(0x10);
    if (const char* from = std::getenv("ENCORE_BOUNCE")) {
      const int fromFrame = std::atoi(from);
      m.cpu.watch[(oracle::u32{cs} << 16) | e.F(encore::at::bounce)] = [&, fromFrame] {
        if (frame < fromFrame) return;
        e.debugWatch = -2;
        auto w = [&](encore::u16 a) { return static_cast<oracle::i16>(m.peek16(ds, e.A(a))); };
        std::fprintf(stderr, "[original] bounce at (%d,%d) speed (%d,%d) angle %03x probes %d material %d contact (%d,%d) nudge %d (frame %d)\n",
                     w(encore::at::ballX), w(encore::at::ballY), w(encore::at::ballVx), w(encore::at::ballVy),
                     static_cast<unsigned>(m.peek16(ds, e.A(encore::at::contactAngle))), m.peek8(ds, e.A(encore::at::contactProbes)),
                     m.peek8(ds, e.A(encore::at::material)), w(encore::at::contactX), w(encore::at::contactY), w(encore::at::nudgeLift), frame);
      };
    }
    if (ownStart) {
      // The engine starts by itself, and what that leaves is compared with what the original's
      // start-up left, before it is taken over like every other difference.
      e.start(options);
      int differ = 0;
      unsigned lastAt = 0xfffffff;
      const char* lastWhat = "";
      auto say = [&](const char* what, unsigned a, unsigned ours, unsigned theirs) {
        // what the start-up leaves that is nothing to the engine: the sound driver's file
        // name, and the variables of the routines that draw
        if (!std::strcmp(what, "data") && a >= e.A(encore::at::driverFileName) && a < e.A(encore::at::driverFileName) + 13u) return;
        if (what[0] == 'c' && a >= e.F(encore::at::drawingCode) && a < e.F(encore::at::drawingCode) + 0x1a00u) return;
        ++differ;
        // only the first of a run is said
        if (!(lastWhat == what && a == lastAt + 1) && differ <= 400)
          std::printf("after start-up: %s %04x ours %02x, the original's %02x\n", what, a, ours, theirs);
        lastAt = a;
        lastWhat = what;
      };
      for (unsigned a = 0; a < 0x6c00; ++a)
        if (e.memory()[a] != m.peek8(ds, static_cast<oracle::u16>(a))) say("data", a, e.memory()[a], m.peek8(ds, static_cast<oracle::u16>(a)));
      for (unsigned a = 0; a < 0xac00; ++a)
        if (e.codeMemory()[a] != m.peek8(cs, static_cast<oracle::u16>(a))) say("code variable", a, e.codeMemory()[a], m.peek8(cs, static_cast<oracle::u16>(a)));
      for (std::size_t a = 0x100; a + 0x10000 < e.image().size(); ++a) {
        const oracle::u8 theirs = m.peek8(static_cast<oracle::u16>(encore::Program::kLoadSegment + (a >> 4)), static_cast<oracle::u16>(a & 15));
        const std::size_t dataAt = std::size_t{e.dataSegment()} * 16, codeAt = 0x100;
        if ((a >= dataAt && a < dataAt + 0x10000) || (a >= codeAt && a < codeAt + 0xac00)) continue;  // those two are kept apart
        if (e.image()[a] != theirs) say("the program's other segments, byte", static_cast<unsigned>(a), e.image()[a], theirs);
      }
      for (std::size_t p = 0; p < 4; ++p)
        for (unsigned a = 0; a < 0x10000; ++a)
          if ((a < encore::video::picture || a >= encore::video::pictureEnd || (a - encore::video::picture) % 0x54 >= 0x50) && e.videoMemory()[p][a] != m.vga.planes[p][a])
            say("video memory", a, e.videoMemory()[p][a], m.vga.planes[p][a]);
      for (unsigned a = 0; a < 768; ++a)
        if (e.colours()[a] != m.vga.dac[a]) say("colour byte", a, e.colours()[a], m.vga.dac[a]);
      std::printf(differ ? "after start-up: %d bytes differ\n" : "after start-up: the same\n", differ);
    }
    for (unsigned a = 0; a < 0x10000; ++a) {
      e.memory()[a] = m.peek8(ds, static_cast<oracle::u16>(a));
      e.codeMemory()[a] = m.peek8(cs, static_cast<oracle::u16>(a));
    }
    for (std::size_t a = 0; a < e.image().size() && a < 0x90000; ++a)
      e.image()[a] = m.peek8(static_cast<oracle::u16>(encore::Program::kLoadSegment + (a >> 4)), static_cast<oracle::u16>(a & 15));
    e.colours() = m.vga.dac;
    e.videoMemory() = m.vga.planes;

    // The data segment ends where the next begins; the code's variables are among its code.
    const unsigned dataEnd = 0x6c00, codeEnd = 0xac00;
    std::set<unsigned> seen;
    unsigned rng = static_cast<unsigned>(flip) * 2654435761u + 1;
    bool left = false, right = false;
    int shakeUntil = 0;
    unsigned throwEvery = 350;
    long pictureFrames = 0, picturePixels = 0;
    std::unique_ptr<encore::TableScreen> screen;
    if (picture) screen = std::make_unique<encore::TableScreen>(*encore::file::findCaseInsensitive(dir, "TABLE" + std::to_string(table + 1) + ".PRG"), table);
    if (screen) screen->attach(e);
    int wrongPlayer = 0;
    int reported = 0;
    long videoDiffers = 0;
    bool shown = false;
    for (frame = 0; frame < frames; ++frame) {
      for (const auto& [at, code] : keys)
        if (at == frame) {
          m.key(static_cast<oracle::u8>(code));
          e.key(static_cast<encore::u8>(code));
        }
      if (flip >= 0 && frame > 400) {  // a game played at random, the same for both
        auto random = [&] { rng = rng * 1664525u + 1013904223u; return rng >> 16; };
        auto both = [&](int code) {
          if (std::getenv("ENCORE_KEYS")) std::printf("key %02x at frame %d\n", code, frame);
          m.key(static_cast<oracle::u8>(code));
          e.key(static_cast<encore::u8>(code));
        };
        if (random() % 23 == 0) { left = !left; both(left ? 0x2a : 0xaa); }
        if (random() % 23 == 0) { right = !right; both(right ? 0x36 : 0xb6); }
        if (frame % 900 == 0) both(0x1c);
        if (frame % 900 == 5) both(0x9c);
        if (frame % 300 == 100) { both(0xe0); both(0x50); }
        if (frame % 300 == 100 + static_cast<int>(random() % 60) + 20) { both(0xe0); both(0xd0); }
        if (rough) {
          // the same change made to both memories: the two are still to stay alike
          auto poke = [&](encore::u16 partyLandAddress, unsigned value, int bytes) {
            const encore::u16 at = e.A(partyLandAddress);
            for (int i = 0; i < bytes; ++i) {
              const auto v = static_cast<oracle::u8>(value >> (8 * i));
              m.poke8(ds, static_cast<oracle::u16>(at + i), v);
              e.memory()[static_cast<encore::u16>(at + i)] = v;
            }
          };
          if (frame == 401)
            for (encore::u16 entry = 0; entry < 4; ++entry) {
              const encore::u16 at = static_cast<encore::u16>(e.kw(encore::at::bestScoresInsn, 1) + entry * 0x10);
              for (encore::u16 i = 0; i < 12; ++i) {
                m.poke8(ds, static_cast<oracle::u16>(at + i), 0);
                e.memory()[static_cast<encore::u16>(at + i)] = 0;
              }
            }
          // (a ball about to leave by the top or the sides is thrown again at once: the original
          // would draw it over the display's memory, which no game does)
          const int ballX = static_cast<oracle::i16>(m.peek16(ds, e.A(encore::at::ballX))), ballY = static_cast<oracle::i16>(m.peek16(ds, e.A(encore::at::ballY)));
          const bool leaving = ballY < 40 || ballX < 0 || ballX > 304;
          // (now and then quickly one after another, for what takes two shots in a few seconds)
          if (frame % 3000 == 0) throwEvery = std::array<unsigned, 4>{40, 120, 350, 350}[random() % 4];
          if ((leaving || random() % throwEvery == 0) && m.peek8(ds, e.A(encore::at::ballHidden)) == 0 && m.peek8(ds, e.A(encore::at::betweenBalls)) == 0) {
            unsigned x = 8 + random() % 300, y = 60 + random() % 510, layer = random() % 6 == 0 ? 0xff : 0;
            int speed = 1000;
            if (const unsigned where = random() % 4; where != 0) {
              // more often than not, into one of the table's own places: where rolling over
              // counts (on the playfield, on the ramps), or where touching something does
              const encore::u16 list = e.A(where == 1 ? encore::at::rollTriggersGround : where == 2 ? encore::at::rollTriggersRamps : encore::at::hitTriggerList);
              unsigned entries = 0;
              while (e.nativeW(static_cast<encore::u16>(list + entries * 10)) != 0) ++entries;
              if (entries != 0) {
                const encore::u16 entry = static_cast<encore::u16>(list + (random() % entries) * 10);
                auto word = [&](int i) { return static_cast<unsigned>(e.nativeW(static_cast<encore::u16>(entry + i))); };
                x = word(0) + random() % (word(4) - word(0) + 1) - 8;
                y = word(2) + random() % (word(6) - word(2) + 1) - 8;
                layer = where == 2 ? 0xff : 0;
                speed = where == 3 ? 1500 : 300;
              }
            }
            // (on the table: left of its edge the original draws the ball over what it reads the
            // slopes from, which no game gets to)
            x = static_cast<int>(x) < 0 ? 0 : x > 304 ? 304 : x;
            y = static_cast<int>(y) < 40 ? 40 : y;
            if (std::getenv("ENCORE_KEYS")) std::printf("thrown to %u,%u layer %02x at frame %d\n", x, y, layer, frame);
            poke(encore::at::ballX, x, 2);
            poke(encore::at::ballY, y, 2);
            poke(encore::at::ballXFixed, x * 0x400, 4);
            poke(encore::at::ballYFixed, y * 0x400, 4);
            poke(encore::at::ballVx, static_cast<unsigned>(static_cast<int>(random() % static_cast<unsigned>(2 * speed + 1)) - speed), 2);
            poke(encore::at::ballVy, static_cast<unsigned>(static_cast<int>(random() % static_cast<unsigned>(2 * speed + 1)) - speed), 2);
            poke(encore::at::layer, layer, 1);
          }
          // a light the rules remember as lit or not is switched: most of what the rules do
          // depends on those, and whole rows of them seldom come about by chance
          if (random() % 150 == 0 && m.peek8(ds, e.A(encore::at::betweenBalls)) == 0) {
            const unsigned light = 1 + random() % e.kw(encore::at::lightCountInsn, 1);
            const encore::u16 flag = static_cast<encore::u16>(e.A(encore::at::lightStates) + light);
            const oracle::u8 now = m.peek8(ds, flag);
            if (now == 0 || now == 0xff) {
              m.poke8(ds, flag, static_cast<oracle::u8>(~now));
              e.memory()[flag] = static_cast<encore::u8>(~now);
            }
          }
          if (random() % 9000 == 0) shakeUntil = frame + 60;
          if (frame < shakeUntil && frame % 4 == 0) both(0x39);
          if (frame < shakeUntil && frame % 4 == 2) both(0xb9);
        }
        if (wild) {  // and the rest of the keyboard: nudges, letters, more players
          if (random() % 500 == 0) both(0x39);
          if (random() % 500 == 1) both(0xb9);
          if (random() % 700 == 0) { const int letter = 0x10 + static_cast<int>(random() % 35); both(letter); both(letter | 0x80); }
          if (random() % 2500 == 0) { both(0x1c); both(0x9c); }
          if (random() % 4000 == 0) { const int f = 0x3b + static_cast<int>(random() % 4); both(f); both(f | 0x80); }
        }
      }
      // The original can go astray by itself, and then there is nothing to compare with: while
      // more players may still join, it also lets their number be put below the player whose
      // turn it is, and then counts players on past the eighth, reading and writing past their
      // records; or a routine of its own never ends.
      const char* astray = nullptr;
      std::string why;
      try {
        m.frame();
      } catch (const std::exception& ex) {
        why = ex.what();
        astray = why.c_str();
      }
      // (one past the last is where the match stops looking)
      // (and a new game's first moments still have the last game's player)
      if (m.peek8(ds, e.A(encore::at::player)) <= m.peek8(ds, e.A(encore::at::players)) + 1) wrongPlayer = 0;
      else if (m.peek8(ds, e.A(encore::at::betweenBalls)) == 0 && m.peek8(ds, e.A(encore::at::ballHidden)) == 0) ++wrongPlayer;
      if (!astray && wrongPlayer > 30) astray = "fewer players were asked for than the one whose turn it is";
      if (astray) {
        std::printf("the original goes astray at frame %d (%s)\n", frame, astray);
        frames = frame;
        break;
      }
      e.frame();
      bool differs = false;
      const std::array<std::vector<encore::u8>, 2> shownOurs = {e.videoMemory()[0], e.videoMemory()[2]};
      // What only the original's drawing uses is not kept: where it last drew the ball, and
      // the drawing routines' own variables among the code.
      auto drawing = [&](const char* what, unsigned a) {
        if (!std::strcmp(what, "data")) {
          if ((a >= e.A(encore::at::ballDrawnAt) && a < e.A(encore::at::ballDrawnAt) + 4u) || a == e.A(encore::at::ballNeverDrawn)) return true;
          // what was under the ball where it was drawn is kept at the start of a segment that,
          // in the Gameshow, begins inside the data segment's 64 KB
          if (a - (static_cast<unsigned>(static_cast<encore::u16>(e.S(encore::at::coverGround) - encore::Program::kLoadSegment)) - e.dataSegment()) * 16u < 0x100u) return true;
          // each flipper's record keeps which picture of it was last drawn, and where
          for (unsigned f = e.A(encore::at::flippers); f < e.A(encore::at::flippers) + 3u * encore::flipperRec::size; f += encore::flipperRec::size)
            if ((a >= f + encore::flipperRec::pictureDrawn && a < f + encore::flipperRec::pictureDrawn + 2u) ||
                (a >= f + encore::flipperRec::drawnAt && a < f + encore::flipperRec::drawnAt + 2u)) return true;
          return false;
        }
        if (what[0] == 'c') return a >= e.F(encore::at::drawingCode) && a < e.F(encore::at::drawingCode) + 0x1a00u;
        return false;
      };
      auto report = [&](const char* what, unsigned a, unsigned ours, unsigned theirs) {
        if (drawing(what, a)) return;
        differs = true;
        if (!seen.insert((static_cast<unsigned>(what[0] * 31 + what[1] * 7 + what[std::strlen(what) - 2]) << 20) | a).second) return;
        ++reported;
        if (std::getenv("ENCORE_VRAM")) {
          const unsigned base = m.peek16(ds, e.A(encore::at::marksRampsCopy));
          int bad = 0, first = -1;
          for (unsigned si = 0; si < 20400; ++si)
            if (m.vga.planes[si & 3][(base + (si >> 2)) & 0xffff] != m.peek8(m.seg(encore::at::marksRamps), static_cast<oracle::u16>(si))) {
              ++bad;
              if (first < 0) first = static_cast<int>(si);
            }
          std::printf("  (the copy of the ramps' marks in video memory at %04x: %d bytes differ, the first at row %d column %d)\n", base, bad,
                      first / 40, first % 40 * 8);
        }
        if (std::getenv("ENCORE_TASK"))
          std::printf("  (task %04x state %04x next step %04x; clip %d,%d; slide row %d of %d)\n", m.peek16(ds, e.A(encore::at::task)), m.peek16(ds, e.A(encore::at::taskState)),
                      m.peek16(ds, e.A(encore::at::nextStep)), m.peek16(ds, e.A(encore::at::glyphRowsCut)), m.peek16(ds, e.A(encore::at::glyphFirstRow)),
                      static_cast<oracle::i16>(m.peek16(ds, e.A(encore::at::msgScrollRow))), static_cast<oracle::i16>(m.peek16(ds, e.A(encore::at::msgScrollTarget))));
        if (std::getenv("ENCORE_BALL"))
          std::printf("  (trigger %04x, ours %04x) (ball %d,%d layer %02x nudge %d)\n", m.peek16(ds, e.A(encore::at::inTrigger)), static_cast<unsigned>(e.W(encore::at::inTrigger)), m.peek16(ds, e.A(encore::at::ballX)), m.peek16(ds, e.A(encore::at::ballY)),
                      m.peek8(ds, e.A(encore::at::layer)), m.peek16(ds, e.A(encore::at::nudgeLift)));
        if (!std::strcmp(what, "data")) {
          const encore::u16 pl = e.partyLandData(static_cast<encore::u16>(a));
          std::printf("frame %d: data %04x (Party Land's %04x) ours %02x, the original's %02x\n", frame, a, pl, ours, theirs);
        } else {
          std::printf("frame %d: %s %04x ours %02x, the original's %02x\n", frame, what, a, ours, theirs);
        }
      };
      for (unsigned a = 0; a < dataEnd; ++a) {
        const oracle::u8 theirs = m.peek8(ds, static_cast<oracle::u16>(a));
        if (e.memory()[a] != theirs) {
          report("data", a, e.memory()[a], theirs);
          if (all) e.memory()[a] = theirs;
        }
      }
      for (unsigned a = 0; a < codeEnd; ++a) {
        const oracle::u8 theirs = m.peek8(cs, static_cast<oracle::u16>(a));
        if (e.codeMemory()[a] != theirs) {
          report("code variable", a, e.codeMemory()[a], theirs);
          if (all) e.codeMemory()[a] = theirs;
        }
      }
      for (int p = 0; p < 2; ++p)
        for (unsigned a = 0; a < encore::video::picture; ++a) {  // up to where the table's picture begins
          const oracle::u8 theirs = m.vga.planes[static_cast<std::size_t>(p * 2)][a];
          encore::u8& ours = e.videoMemory()[static_cast<std::size_t>(p * 2)][a];
          if (ours != theirs) {
            // a ball that goes off the top of the table is drawn by the original over the
            // display's memory, in colours no dot of the display has: that is its drawing
            if (theirs != 0 && theirs != e.kb(encore::at::dotLitInsn, 1) && theirs != e.kb(encore::at::dotUnlitInsn, 1)) {
              ++videoDiffers;
              continue;
            }
            report(p ? "display, third plane," : "display, first plane,", a, ours, theirs);
            if (all) ours = theirs;
          }
        }
      // beside the picture: the four spare bytes of each row, where the top of one mask's copy is
      for (std::size_t p = 0; p < 4; ++p)
        for (unsigned a = encore::video::picture; a < encore::video::pictureEnd; ++a)
          if ((a - encore::video::picture) % 0x54 >= 0x50 && e.videoMemory()[p][a] != m.vga.planes[p][a]) {
            ++videoDiffers;
            if (std::getenv("ENCORE_VIDEO")) report("video memory beside the picture,", a, e.videoMemory()[p][a], m.vga.planes[p][a]);
            if (std::getenv("ENCORE_TAKE_VIDEO")) e.videoMemory()[p][a] = m.vga.planes[p][a];
          }
      // past the picture: the mask copies and the pictures kept over them
      for (std::size_t p = 0; p < 4; ++p)
        for (unsigned a = encore::video::pictureEnd; a < 0x10000; ++a)
          if (e.videoMemory()[p][a] != m.vga.planes[p][a]) {
            // The original draws the ball there as it leaves the bottom of the table, over the
            // first rows of a mask's copy, and puts back what was there when the ball moves on.
            // The engine does not draw. The bytes are counted, said only if asked
            // (ENCORE_VIDEO), and left as the engine has them unless ENCORE_TAKE_VIDEO is set.
            ++videoDiffers;
            if (std::getenv("ENCORE_VIDEO")) report("video memory past the picture,", a, e.videoMemory()[p][a], m.vga.planes[p][a]);
            if (std::getenv("ENCORE_TAKE_VIDEO")) e.videoMemory()[p][a] = m.vga.planes[p][a];
          }
      for (unsigned a = 0; a < 768; ++a)
        if (e.colours()[a] != m.vga.dac[a]) {
          report("colour byte", a, e.colours()[a], m.vga.dac[a]);
          if (all) e.colours()[a] = m.vga.dac[a];
        }
      if (picture) {
        const int height = encore::TableScreen::height(options.highResolution);
        std::vector<encore::u8> ours(static_cast<std::size_t>(320 * height));
        screen->draw(e, ours.data(), height);
        const auto theirs = m.vga.picture(height);
        long wrong = 0;
        std::size_t first = 0;
        for (std::size_t i = 0; i < ours.size(); ++i)
          if (ours[i] != theirs[i] && std::memcmp(&m.vga.dac[ours[i] * 3u], &m.vga.dac[theirs[i] * 3u], 3) != 0 && wrong++ == 0) first = i;
        if (wrong) {
          ++pictureFrames;
          picturePixels += wrong;
          if (pictureFrames <= 8)
            std::printf("frame %d: the screen differs in %ld dots, the first at %zu,%zu (ours %02x, the original's %02x); top row %d, the original's memory from %04x, split at %d\n",
                        frame, wrong, first % 320, first / 320, ours[first], theirs[first], static_cast<encore::i16>(e.screenRow()) - 33,
                        m.vga.startAddress(), m.vga.lineCompare());
        }
        if (const char* at = std::getenv("ENCORE_PICTURE"); at && frame == std::atoi(at)) {
          auto save = [&](const char* name, const encore::u8* px) {
            std::vector<encore::u8> rgb(static_cast<std::size_t>(320 * height * 3));
            for (std::size_t i = 0; i < static_cast<std::size_t>(320 * height); ++i)
              for (std::size_t c = 0; c < 3; ++c) rgb[i * 3 + c] = static_cast<encore::u8>(m.vga.dac[px[i] * 3u + c] * 255 / 63);
            encore::writeRgbPng(name, rgb.data(), 320, height);
          };
          save("ours.png", ours.data());
          save("theirs.png", theirs.data());
        }
      }
      if (const char* at = std::getenv("ENCORE_GRAVITY"); at && frame == std::atoi(at)) {
        // what the pull on the ball is read from, under where the ball is now, in both
        unsigned x = m.peek16(ds, e.A(encore::at::ballX)), y = m.peek16(ds, e.A(encore::at::ballY));
        if (const char* xy = std::getenv("ENCORE_GRAVITY_XY")) {
          int gx = 0, gy = 0;
          std::sscanf(xy, "%d,%d", &gx, &gy);
          x = static_cast<unsigned>(gx) & 0xffff;
          y = static_cast<unsigned>(gy) & 0xffff;
        }
        unsigned place = (((x + 8) & 0xffff) >> 3) + (y + 8) * 0x28 - 1;
        const unsigned copy = m.peek16(ds, e.A(encore::at::slopesGroundCopy));
        for (int i = 0; i < 3; ++i, ++place) {
          const auto o = static_cast<oracle::u16>(place);
          const unsigned v = o < encore::video::groundTopBytes ? ((o >> 4) + 1) * 0x50 + (o >> 2) + encore::video::picture : copy + (o >> 2) - (encore::video::groundTopBytes >> 2);
          std::printf("place %04x: masks %02x %02x (ours %02x %02x), slope %02x (ours %02x) at video %04x plane %d\n", o,
                      m.peek8(e.S(encore::at::marksGround), o), m.peek8(e.S(encore::at::wallsGround), o), e.farB(e.S(encore::at::marksGround), o), e.farB(e.S(encore::at::wallsGround), o),
                      m.vga.planes[o & 3][v & 0xffff], e.videoMemory()[o & 3][v & 0xffff], v & 0xffff, o & 3);
        }
      }
      if (const char* at = std::getenv("ENCORE_SEGMENTS"); at && frame == std::atoi(at)) {
        // the whole program, once: where its other segments (the masks, say) differ
        int n = 0;
        for (std::size_t a = 0x100; a + 0x10000 < e.image().size(); ++a) {
          const oracle::u8 theirs = m.peek8(static_cast<oracle::u16>(encore::Program::kLoadSegment + (a >> 4)), static_cast<oracle::u16>(a & 15));
          const std::size_t dataAt = std::size_t{e.dataSegment()} * 16;
          if ((a >= dataAt && a < dataAt + 0x10000) || a < 0x100 + 0xac00u) continue;
          if (e.image()[a] != theirs && ++n <= 12)
            std::printf("frame %d: the program's byte %05zx (segment %04zx) ours %02x, the original's %02x\n", frame, a, a >> 4, e.image()[a], theirs);
        }
        std::printf("frame %d: %d bytes of the program's other segments differ\n", frame, n);
      }
      if (differs && std::getenv("ENCORE_SHOW") && !shown) {
        // the two displays, ours above
        shown = true;
        for (int which = 0; which < 2; ++which) {
          for (int row = 0; row < 16; ++row) {
            std::string line;
            for (int x = 0; x < 160; ++x) {
              const unsigned at = 0xa8u + static_cast<unsigned>(row) * 0xa8u + static_cast<unsigned>(x / 2);
              const oracle::u8 v = which ? m.vga.planes[(x & 1) * 2][at] : shownOurs[static_cast<std::size_t>(x & 1)][at];
              line += v == e.kb(encore::at::dotLitInsn, 1) ? '#' : '.';
            }
            std::puts(line.c_str());
          }
          std::puts("");
        }
      }
      if (differs && !all) {
        std::printf("they part at frame %d\n", frame);
        return 1;
      }
      if (reported > 200) {
        std::puts("(and more)");
        return 1;
      }
    }
    std::printf(reported ? "%d places differed over %d frames\n" : "the same for %2$d frames\n", reported, frames);
    if (videoDiffers) std::printf("(the original's drawing past the picture: %ld bytes, frame by frame)\n", videoDiffers);
    if (picture) std::printf(pictureFrames ? "the screen differed in %ld frames, %ld dots in all\n" : "the screen the same in every frame\n", pictureFrames, picturePixels);
    if (std::getenv("ENCORE_COVER")) {  // what of ours the game never came to
      std::printf("never called:");
      for (const encore::u16 at : e.neverCalled()) std::printf(" %04x", at);
      std::printf("\n");
    }
    return reported ? 1 : 0;
  } catch (const std::exception& ex) {
    std::printf("stopped at frame %d: %s\n", frame, ex.what());
    return 2;
  }
}
