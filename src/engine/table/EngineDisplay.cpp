// The dot-matrix display. What it shows is a script: a list of steps, each the address of a
// routine and its arguments. A step sets a task going (a routine called every frame until it
// answers zero) and often a drawing to be done once; when the task is done the next step runs.
#include <cstdio>

#include "engine/table/Engine.h"

namespace encore {
namespace {

constexpr u16 kRow = 0xa8;  // bytes from one row of dots to the next
}  // namespace

void Engine::compiledPicture(const u8* code, std::size_t size, u16 start, u16 base, int plane, u8 litDot, u8 unlitDot) {
  // AL holds a lit dot and AH an unlit one; BX is where the picture goes.
  for (std::size_t at = start; at + 1 < size;) {
    const u8 op = code[at];
    if (op == x86::ret) return;
    if (op != x86::movByte) break;
    const u8 m = code[at + 1];
    const u8 value = (m & x86::regAh) ? unlitDot : litDot;
    u16 disp = 0;
    switch (m & static_cast<u8>(~x86::regAh)) {
      case x86::atBx: at += 2; break;
      case x86::atBxDisp8: disp = static_cast<u16>(static_cast<i8>(code[at + 2])); at += 3; break;
      case x86::atBxDisp16: disp = static_cast<u16>(code[at + 2] | (code[at + 3] << 8)); at += 4; break;
      default: throw DataError("a picture of the display's is not as expected");
    }
    dot(plane, static_cast<u16>(base + disp)) = value;
  }
  throw DataError("a picture of the display's is not as expected");
}

void Engine::fillDisplay(u16 at, u16 width, u16 rows) {
  for (u16 r = 0; r < rows; ++r, at = static_cast<u16>(at + kRow))
    for (u16 x = 0; x < width; ++x) {
      dot(0, static_cast<u16>(at + x)) = unlit();
      dot(1, static_cast<u16>(at + x)) = unlit();
    }
}

/// The four fonts: their height in rows, and what takes the drawing place back to the top of
/// the next letter (cs:4b5f and its like).
void Engine::setFont(int which) {
  static constexpr u16 kHeight[4] = {0x0d, 0x0b, 0x08, 0x05};
  static constexpr u16 kBack[4] = {0xf77c, 0xf8cc, 0xfac4, 0xfcbc};
  static constexpr u16 kGlyphs[4] = {at::font13, at::font11, at::font8, at::font5};
  W(at::fontHeight) = kHeight[which];
  W(at::fontNext) = kBack[which];
  W(at::font) = A(kGlyphs[which]);
}

/// cs:6ccd: one letter at `at`, which is left where the original leaves DI.
void Engine::drawChar(u8 c, u16& at) {
  if (c == 0x20) {
    at = static_cast<u16>(at - W(at::fontNext) + 4);
    return;
  }
  c = static_cast<u8>(c - 0x37);
  u16 glyph = c == static_cast<u8>(0x2a - 0x37) ? A(at::blankGlyph) : static_cast<u16>(W(at::font) + c * B(at::fontHeight));
  const u16 rows = static_cast<u16>(W(at::fontHeight) - W(at::glyphRowsCut));
  glyph = static_cast<u16>(glyph + W(at::glyphFirstRow));
  // A row is a byte: its bits, from the top one down, are the dots from left to right, which
  // lie alternately in the two planes.
  u16 row = at;
  for (u16 r = 0; r < rows; ++r, row = static_cast<u16>(row + kRow)) {
    const u8 bits = nativeB(static_cast<u16>(glyph + r));
    for (int d = 0; d < 8; ++d)
      dot(d & 1, static_cast<u16>(row + d / 2)) = (bits & (0x80 >> d)) ? lit() : unlit();
  }
  at = row;
}

void Engine::drawText(u16 text, u16 at) {
  u8 c = nativeB(text);
  do {
    u16 place = at;
    drawChar(c, place);
    at = static_cast<u16>(at + 4);
    c = nativeB(++text);
  } while (c != 0);
}

void Engine::forgetNumber() {
  for (u16 i = 0; i < 12; ++i) B(at::numberDrawn, i) = display::notDrawn;
  B(at::numberStarted) = 0;
  W(at::numberGroups) = 0;
}

/// cs:6c0f: a number of twelve digits, one to a byte. Digits that have not changed since it
/// was last drawn are left alone, as are the zeros in front.
void Engine::drawNumber(u16 digits, u16 at) {
  if (B(at::numberCentred) == kSet) {  // set to the left: move back two for each zero in front
    B(at::numberCentred) = 0;
    u16 scanned = 0;
    while (scanned < 12 && nativeB(static_cast<u16>(digits + scanned++)) == 0) {}
    at = static_cast<u16>(at - scanned * 2);
  }
  for (u16 i = 0; i < 12; ++i) {
    u8 d = nativeB(static_cast<u16>(digits + i));
    bool draw = true;
    if (d == 0 && B(at::numberStarted) != kSet) draw = false;
    else if (d != 0) B(at::numberStarted) = kSet;
    if (draw) {
      const u8 before = B(at::numberDrawn, i);
      B(at::numberDrawn, i) = d;
      if (before == d) draw = false;
    }
    B(at::numberShown, i) = draw ? d : display::unchanged;
  }
  B(at::numberStarted) = 0;
  drawCommas(digits, static_cast<u16>(at - W(at::fontNext) + 0xf6d4));
  for (u16 i = 0; i < 12; ++i) {
    const u8 d = B(at::numberShown, i);
    if (d == display::unchanged) {
      at = static_cast<u16>(at + 4);
    } else {
      drawChar(static_cast<u8>(d + 0x37), at);
      at = static_cast<u16>(at + W(at::fontNext));
    }
  }
}

/// cs:6d6f: the marks between the thousands, as many as the number needs.
void Engine::drawCommas(u16 digits, u16 base) {
  for (u16 groups = 3;; --groups) {
    if (groups == 0) return;
    const u16 pair = static_cast<u16>(nativeW(digits) + nativeB(static_cast<u16>(digits + 2)));
    const u16 sum = static_cast<u16>((nativeW(digits) & 0xff00) | (pair & 0xff));
    digits = static_cast<u16>(digits + 3);
    if (sum == 0) continue;
    const bool erase = sum >= 0x0a00;
    const bool same = groups == W(at::numberGroups);
    W(at::numberGroups) = groups;
    if (same) return;
    u16 at = static_cast<u16>(base + display::commaMark - display::score);
    for (u16 n = groups; n > 0; --n, at = static_cast<u16>(at - 0x0c)) {
      const u8 v = (n == 1 && erase) ? unlit() : lit();
      dot(1, at) = v;
      dot(1, static_cast<u16>(at + kRow)) = v;
      dot(0, static_cast<u16>(at + 1)) = v;
      dot(0, static_cast<u16>(at + kRow)) = v;
    }
    return;
  }
}

/// The routine at the start of the tables' second code segment: the score in its large
/// digits, which are pictures that are code. Only digits that have changed are drawn.
void Engine::drawScore(u16 digits, u16 at) {
  const u16 scoreDigits = digits, scoreAt = at;
  const u16 segment = static_cast<u16>(cs_[static_cast<u16>(F(at::bigScoreTask) - 2)] | (cs_[static_cast<u16>(F(at::bigScoreTask) - 1)] << 8));
  const u8* code = &farB(segment, 0);
  u16 count = 0x0c;
  while (nativeB(digits) == 0) {  // not the zeros in front, but always the last digit
    at = static_cast<u16>(at + 4);
    ++digits;
    if (--count == 1) break;
  }
  // (that segment's own notes of these three, and its two tables of where each digit's
  // picture is, are where its own code says)
  auto its = [code](std::size_t at) { return static_cast<u16>(code[at] | (code[at + 1] << 8)); };
  // where in that segment: its three notes, and for each plane the table of the digits' pictures
  // and what is added to an entry of it
  constexpr std::size_t countAt = 0x81, placeAt = 0x85, digitsAt = 0x89, litDotAt = 0x8c, unlitDotAt = 0x8d;
  constexpr std::size_t picturesEven = 0xc8, baseEven = 0xd4, picturesOdd = 0x119, baseOdd = 0x125;
  nativeW(its(countAt)) = count;
  nativeW(its(placeAt)) = at;
  nativeW(its(digitsAt)) = digits;
  for (int plane = 0; plane < 2; ++plane) {
    u16 place = at;
    for (u16 i = 0; i < count; ++i, place = static_cast<u16>(place + 4)) {
      const u8 d = nativeB(static_cast<u16>(digits + i));
      const u8 before = B(at::scoreDrawn, i);
      if (plane == 1) B(at::scoreDrawn, i) = d;
      if (d == before) continue;
      const u16 entry = static_cast<u16>(((d + '0') & 0xff) * 2);  // by the digit's ASCII code
      const u16 start = plane == 0 ? static_cast<u16>(nativeW(static_cast<u16>(its(picturesEven) + entry)) + its(baseEven))
                                   : static_cast<u16>(nativeW(static_cast<u16>(its(picturesOdd) + entry)) + its(baseOdd));
      compiledPicture(code, 0x1480u, start, place, plane, code[litDotAt], code[unlitDotAt]);
    }
  }
  // cs:0135 of that segment: the marks between the thousands
  for (u16 groups = 3;; --groups) {
    if (groups == 0) return;
    const u16 sum = static_cast<u16>((nativeW(scoreDigits + (3 - groups) * 3) & 0xff00) |
                                     ((nativeW(scoreDigits + (3 - groups) * 3) + nativeB(static_cast<u16>(scoreDigits + (3 - groups) * 3 + 2))) & 0xff));
    if (sum == 0) continue;
    const bool same = groups == W(at::scoreDrawnGroups);
    W(at::scoreDrawnGroups) = groups;
    if (same) return;
    u16 mark = static_cast<u16>(scoreAt + display::commaMark - display::score);
    for (u16 n = groups; n > 0; --n, mark = static_cast<u16>(mark - 0x0c)) {
      dot(1, mark) = lit();
      dot(1, static_cast<u16>(mark + kRow)) = lit();
      dot(0, static_cast<u16>(mark + 1)) = lit();
      dot(0, static_cast<u16>(mark + kRow)) = lit();
    }
    return;
  }
}

// ---------------------------------------------------------------------------------------
// the script
// ---------------------------------------------------------------------------------------

/// cs:444c: once a frame: the task, then the drawing the step asked for, if any.
void Engine::displayStep() {
  B(at::skipTask) = 0;
  si = W(at::taskState);
  call(W(at::task));
  W(at::taskState) = si;
  if (si == 0) {
    W(at::task) = F(at::defaultTask);
    if (const u16 next = W(at::nextStep); next != 0) runStep(next);
    else displaySteady();
    B(at::scriptEnded) = kSet;
  }
  ax = W(at::drawSource);
  di = W(at::drawAt);
  call(W(at::drawRoutine));
  W(at::drawRoutine) = F(at::nothing);
}

void Engine::startScript(u16 native) {
  displaySteady();
  B(at::scriptEnded) = 0;
  W(at::idleFrames) = 0;
  B(at::needDefaultBg) = kSet;
  runStep(native);
}

void Engine::runStep(u16 native) {
  for (u16 i = 0; i < 12; ++i) B(at::scoreDrawn, i) = display::scoreNotDrawn;  // the score is drawn afresh
  W(at::scoreDrawnGroups) = 0;
  bx = native;
  call(nativeW(native));
}

void Engine::nextStep(u16 size) {
  bx = static_cast<u16>(bx + size);
  if (nativeW(bx) == 0) bx = 0;
  W(at::nextStep) = bx;
}

void Engine::bindDisplay() {
  auto arg = [this](u16 n) { return nativeW(static_cast<u16>(bx + n)); };
  auto wait = [this](u16 frames) {  // the usual task: count down
    W(at::taskState) = frames;
    W(at::task) = F(at::waitTask);
  };

  // --- tasks: SI comes in as what the task left last time, and goes out zero when done
  bind(at::waitTask, [this] { --si; });
  bind(at::haltTask, [this] { si = 1; });  // for ever
  bind(at::bigScoreTask, [this] {  // the score, once
    drawScore(si, display::bigScore);
    for (u16 i = 0; i < 12; ++i) B(at::scoreDrawn, i) = display::scoreNotDrawn;
    W(at::scoreDrawnGroups) = 0;
    si = 0;
  });
  bind(at::defaultTask, [this] {  // nothing in particular to show (cs:535b)
    si = 0;
    if (CB(at::keysOn) == 0) return;
    if (W(at::idleFrames) == 0x2d0) {
      if (B(at::ballOnPlunger) != kSet) {
        W(at::idleShowing) = kSet;
        B(at::idlePlayerDigit) = B(at::playerDigit);
        B(at::idleBallDigit) = B(at::ballDigit);
        startScript(A(at::scriptGameIdle));
      }
    } else if (B(at::enterAttract) == kSet) {
      B(at::enterAttract) = 0;
      startScript(A(at::scriptAttract));
      si = 0;
      return;
    } else if (B(at::attract) == kSet) {
      startScript(A(at::scriptGameOver));
    } else if (B(at::ballUntouched) != kSet && B(at::needDefaultBg) == kSet) {
      B(at::needDefaultBg) = 0;
      runStep(A(at::scriptMain));
    }
    // cs:5406: has the score passed the best on the table?
    if (B(at::ballUntouched) != kSet && B(at::modeRunning) != kSet && B(at::gotTopScore) != kSet) {
      for (u16 i = 0; i < 12; ++i) {
        const u8 best = nativeB(static_cast<u16>(kw(at::bestScoreTopInsn, 1) + i)), score = B(at::score, i);
        if (score < best) break;
        if (score > best) {
          B(at::gotTopScore) = kSet;
          startScript(A(at::scriptTopScoreIngame));
          B(at::needDefaultBg) = kSet;
          break;
        }
      }
    }
    ++W(at::idleFrames);
    drawScore(A(at::score), display::score);
    si = 0;
  });
  bind(at::confirmQuitTask, [this] {  // waits for Y or N
    si = 1;
    u8 key = B(at::lastKey);
    if (key == scancodes::none) return;
    B(at::lastKey) = scancodes::none;
    key = B(at::scancodeLetters, key);
    if (key == 0) return;
    if (key == 'Y') CB(at::quit) = kSet;
    CB(at::quitAsked) = 0;
    CB(at::keysOn) = kSet;
    si = 0;
  });

  bind(at::waitJingleTask, [this] {  // until the music has come round
    si = taskState::waiting;
    if (B(at::jingleEnded) == kSet) si = 0;
  });
  bind(at::waitJingleTimeoutTask, [this] {  // until an effect has ended, but no longer than so many frames
    si = taskState::waiting;
    if (sound->status() & driverStatus::effectSounding) {
      if (--W(at::jingleTimeout) == 0) si = 0;
    } else if (B(at::jingleEnded) == kSet) {
      si = 0;
    }
  });

  // --- steps
  bind(at::checkTopScore, [this] {  // has the score passed the table's best? then say so first
    if (B(at::gotTopScore) != kSet) {
      for (u16 i = 0; i < 12; ++i) {
        const u8 best = nativeB(static_cast<u16>(kw(at::bestScoreTopInsn, 1) + i)), score = B(at::score, i);
        if (score < best) break;
        if (score > best) {
          B(at::gotTopScore) = kSet;
          bx = A(at::scriptTopScoreInterball);
          call(nativeW(bx));
          return;
        }
      }
    }
    bx = static_cast<u16>(bx + 2);
    call(nativeW(bx));
  });
  bind(at::waitJingle, [this] {
    W(at::task) = F(at::waitJingleTask);
    nextStep(4);
  });
  bind(at::waitJingleTimeout, [this] {
    W(at::task) = F(at::waitJingleTimeoutTask);
    nextStep(4);
  });
  bind(at::playJingle, [=, this] {  // a piece of music, come what may
    wait(1);
    B(at::musicPriority) = 0;
    // (the original has the music go back afterwards to the place it was told to before;
    // amended: when no other piece is playing, back to where the music is now, as any other)
    if (!amended || static_cast<i8>(B(at::musicRepeats)) > 0) B(at::musicRepeats) = 1;
    music(arg(2));
    nextStep(4);
  });
  bind(at::jump, [=, this] {  // go on from another place in the script
    bx = arg(2);
    W(at::nextStep) = bx;
    call(nativeW(bx));
  });
  bind(at::dmBigScore, [=, this] {  // the score
    W(at::taskState) = arg(2);
    W(at::task) = F(at::bigScoreTask);
    nextStep(4);
  });
  bind(at::dmBlink, [=, this] {  // the display flashes, this many frames each way
    const u16 period = arg(2);
    wait(1);
    W(at::flashPeriod) = period;
    W(at::flashCount) = period;
    W(at::flashPhase) = kSet;
    B(at::flashing) = kSet;
    nextStep(4);
  });
  // a number at a place, in each font, and set to the left
  struct NumberStep { u16 step, draw; bool left; };
  for (const NumberStep n : {NumberStep{at::dmPrintScore11, at::drawNumber11, false}, {at::dmPrintScore13, at::drawNumber13, false},
                             {at::dmPrintScore8, at::drawNumber8, false}, {at::dmPrintScore8Centred, at::drawNumber8, true},
                             {at::dmPrintScore13Centred, at::drawNumber13, true}, {at::dmPrintScore5, at::drawNumber5, false}}) {
    bind(n.step, [=, this] {
      if (n.left) B(at::numberCentred) = kSet;
      wait(1);
      W(at::glyphFirstRow) = 0;
      W(at::numberDigits) = arg(2);
      W(at::numberAt) = arg(4);
      W(at::drawRoutine) = F(n.draw);
      nextStep(6);
    });
  }
  struct NumberDraw { u16 at; int font; };
  for (const NumberDraw n : {NumberDraw{at::drawNumber8, font::h8}, {at::drawNumber11, font::h11}, {at::drawNumber13, font::h13}, {at::drawNumber5, font::h5}}) {
    bind(n.at, [=, this] {
      forgetNumber();
      setFont(n.font);
      drawNumber(W(at::numberDigits), W(at::numberAt));
    });
  }
  bind(at::playSfx, [=, this] {  // a sound
    wait(1);
    const u16 record = arg(2);
    sound->effect(nativeB(static_cast<u16>(record + soundRec::sample)), nativeB(static_cast<u16>(record + soundRec::note)), static_cast<u8>(arg(4)),
                  static_cast<u8>(nativeB(static_cast<u16>(record + soundRec::channel)) + 1));
    nextStep(6);
  });
  bind(at::setMusic, [=, this] {
    wait(1);
    B(at::musicNext) = static_cast<u8>(arg(2));
    nextStep(4);
  });
  // text at a place, in each font
  struct TextStep { u16 step; int font; };
  for (const TextStep t : {TextStep{at::dmPuts13, font::h13}, {at::dmPuts11, font::h11}, {at::dmPuts8, font::h8}, {at::dmPuts5, font::h5}}) {
    bind(t.step, [=, this] {
      wait(1);
      setFont(t.font);
      W(at::glyphRowsCut) = 0;
      W(at::glyphFirstRow) = 0;
      W(at::drawSource) = arg(2);
      W(at::drawAt) = arg(4);
      W(at::drawRoutine) = F(at::drawText);
      nextStep(6);
    });
  }
  bind(at::drawText, [this] { drawText(ax, di); });
  bind(at::dmStopBlink, [=, this] {
    wait(1);
    displaySteady();
    nextStep(4);
  });
  bind(at::dmState, [=, this] {
    wait(1);
    if (arg(2) != 0) displayNormal();
    else displayInverse();
    nextStep(4);
  });
  bind(at::setJingleTimeout, [=, this] {
    wait(1);
    W(at::jingleTimeout) = arg(2);
    nextStep(4);
  });
  bind(at::delay, [=, this] {  // wait
    wait(arg(2));
    nextStep(4);
  });
  bind(at::delayIfMultiplayer, [=, this] {  // wait, but not long for one player
    // (amended: on Stones 'n Bones a lone player is given the time as well, to read what the
    // display says between balls)
    wait(B(at::players) == 1 && !(amended && table() == tableId::stonesNBones) ? 2 : arg(2));
    nextStep(4);
  });
  bind(at::halt, [=, this] {
    W(at::taskState) = arg(2);
    W(at::task) = F(at::haltTask);
    nextStep(4);
  });
  bind(at::confirmQuit, [=, this] {
    W(at::taskState) = arg(2);
    W(at::task) = F(at::confirmQuitTask);
    nextStep(4);
  });

  // --- the big letters that run across the display (cs:6e9c). They are pictures that are
  // code, drawn afresh each frame half a dot further left: by turns in each plane.
  bind(at::dmLongMsg, [=, this] {
    if (W(at::taskState) != 0) B(at::skipTask) = kSet;
    W(at::task) = F(at::longMsgTask);
    W(at::taskState) = arg(2);
    nextStep(4);
  });
  bind(at::longMsgTask, [this] {
    for (int twice = 0; twice < 2; ++twice) {
      if (nativeB(static_cast<u16>(si + 0x14)) == display::longMsgEnd) {  // the end of the text
        W(at::longMsgText) += 1;
        si = 0;
        return;
      }
      struct Pass { u16 mask, at, table, base; };
      for (const Pass pass : {Pass{at::longMsgMaskB, at::longMsgAtB, at::longFontOddIndex, at::longFontOdd},
                              Pass{at::longMsgMaskA, at::longMsgAtA, at::longFontEvenIndex, at::longFontEven}}) {
        const int plane = CB(pass.mask) == planeMask::first ? 0 : 1;
        u16 place = CW(pass.at);
        for (u16 i = 0; i < 0x15; ++i, place = static_cast<u16>(place + 4)) {
          const u8 c = nativeB(static_cast<u16>(si + i));
          compiledPicture(cs_.data(), cs_.size(), static_cast<u16>(W(pass.table, static_cast<u16>(c * 2)) + A(pass.base)), place, plane,
                          kb(at::longFontColoursInsn, 1), kb(at::longFontColoursInsn, 2));
        }
      }
      CB(at::longMsgMaskA) ^= planeMask::first | planeMask::third;
      CB(at::longMsgMaskB) ^= planeMask::first | planeMask::third;
      if (CB(at::longMsgMaskB) == planeMask::first) CW(at::longMsgAtA) -= 1;
      else CW(at::longMsgAtB) -= 1;
      if (--CB(at::longMsgEighths) == 0) {  // eight half-dots: on to the next letter
        CB(at::longMsgEighths) = 8;
        ++si;
        CW(at::longMsgAtA) = kRow;
        CW(at::longMsgAtB) = kRow;
      }
      W(at::longMsgText) = si;
      W(at::longMsgPlace) = W(at::longMsgPlace);
    }
  });

  // --- an animation from the table's bank of them (cs:6fb5). Before its frames come where to
  // go back to, how many times round, and where the frames end; each frame is how long it
  // stays and, for each plane, a list of dots to light or put out.
  // (its three counters and its note of where it is are in the bank itself, at places each
  // table's own code names)
  bind(at::dmAnim, [=, this] {
    if (W(at::taskState) != 0) B(at::skipTask) = kSet;
    const u16 bank = S(at::spriteBank), anim = arg(2);
    farW(bank, kw(at::animEntryInsn, 2)) = 0;
    farW(bank, kw(at::animFramesLeftInsn, 2)) = 1;
    farW(bank, kw(at::animRoundsLeftInsn, 2)) = farW(bank, static_cast<u16>(anim - animRec::rounds));
    W(at::task) = F(at::animTask);
    W(at::taskState) = anim;
    nextStep(4);
  });
  bind(at::animTask, [this] {
    const u16 bank = S(at::spriteBank);
    const u16 entryAt = kw(at::animEntryInsn, 2), framesLeft = kw(at::animFramesLeftInsn, 2), roundsLeft = kw(at::animRoundsLeftInsn, 2);
    farW(bank, kw(at::animStateInsn, 2)) = si;
    if (--farW(bank, framesLeft) != 0) return;
    const u16 entry = farW(bank, entryAt);
    if (entry == farW(bank, static_cast<u16>(si - animRec::end))) {
      if (--farW(bank, roundsLeft) == 0) {
        si = 0;
        farW(bank, entryAt) = 0;
        farW(bank, framesLeft) = 0;
        return;
      }
      farW(bank, entryAt) = farW(bank, static_cast<u16>(si - animRec::back));
    }
    farW(bank, entryAt) += 4;
    farW(bank, framesLeft) = farW(bank, static_cast<u16>(entry + si + animRec::frameTime));
    u16 frame = farW(bank, static_cast<u16>(entry + si));
    for (int plane = 0; plane < 2; ++plane) {
      const u16 count = farW(bank, frame);
      frame = static_cast<u16>(frame + 2);
      u16 place = display::beforeTop;
      for (u16 i = 0; i < count; ++i) {
        const u8 b = farB(bank, frame++);
        place = static_cast<u16>(place + (b >> 1));
        if (b & 1) dot(plane, place) = kb(at::animDotColoursInsn, 1);
        else if ((b >> 1) != display::animSkip) dot(plane, place) = kb(at::animDotColoursInsn, 2);
      }
    }
  });

  // --- text that slides in from below, or out upwards
  auto slide = [this] {  // cs:4988
    const i16 row = W(at::msgScrollRow).s();
    u16 at = 0;
    if (row < 0) {
      W(at::glyphRowsCut) = static_cast<u16>(-row);
      W(at::glyphFirstRow) = static_cast<u16>(-row);
      at = kRow;
    } else if (row > 3) {
      W(at::glyphRowsCut) = static_cast<u16>(row - 3);
      W(at::glyphFirstRow) = 0;
      at = static_cast<u16>((row + 1) * kRow);
    } else {
      W(at::glyphRowsCut) = 0;
      W(at::glyphFirstRow) = 0;
      at = static_cast<u16>(row * kRow + kRow);
    }
    if (W(at::glyphRowsCut) < 0x0d) drawText(W(at::msgScrollText), at);
    if (W(at::msgScrollRow) == W(at::msgScrollTarget)) {
      si = 0;
      W(at::glyphRowsCut) = 0;
      W(at::glyphFirstRow) = 0;
    } else {
      si = 1;
    }
  };
  // --- a count-down: seconds on the left, counted off, with a score beside them
  auto countDown = [=, this] {  // cs:4780: from so many seconds
    B(at::pendingMode) = 0;
    W(at::taskState) = 1;
    W(at::task) = F(at::modeTask);
    forgetNumber();
    W(at::modeScoreShown) = arg(6);
    W(at::modeFrames) = 1;
    B(at::countdownTens) = static_cast<u8>(0x37 + arg(2));
    B(at::countdownUnits) = static_cast<u8>(0x37 + 1 + arg(4));  // one more: it is counted down before it is shown
    nextStep(8);
  };
  bind(at::modeContinue, [=, this] {  // cs:4757: on from where it was, and no longer held
    B(at::pendingMode) = 0;
    W(at::taskState) = 1;
    W(at::task) = F(at::modeTask);
    forgetNumber();
    W(at::modeScoreShown) = arg(6);
    W(at::modeFrames) = 1;
    B(at::countdownHeld) = 0;
    nextStep(8);
  });
  bind(at::modeStart, countDown);
  bind(at::modeStartOrContinue, [=, this] {  // or on from where it was, unless it is to begin again
    if (B(at::pendingMode) == kSet) return countDown();
    W(at::taskState) = 1;
    W(at::task) = F(at::modeTask);
    forgetNumber();
    W(at::modeFrames) = 1;
    nextStep(8);
  });
  bind(at::modeTask, [this] {
    si = 1;
    if (B(at::countdownHeld) != kSet && --W(at::modeFrames) == 0) {
      W(at::modeFrames) = W(at::modeSecondFrames);
      if (B(at::countdownUnits) == 0x37 && B(at::countdownTens) == 0x2a) {  // nought: it is over
        W(at::taskState) = 0;
        si = 0;
        return;
      }
      if (--B(at::countdownUnits) == 0x36) {
        B(at::countdownUnits) += 10;
        --B(at::countdownTens);
      }
      if (B(at::countdownTens) == 0x37) B(at::countdownTens) = 0x2a;  // no nought in front
      const u16 keptSi = si;
      call(F(at::tableModeSecond));
      si = keptSi;
      setFont(font::h11);
      W(at::glyphRowsCut) = 0;
      W(at::glyphFirstRow) = 0;
      W(at::drawSource) = A(at::countdownTens);
      W(at::drawAt) = display::countdownSeconds;
      W(at::drawRoutine) = F(at::drawText);
      return;
    }
    W(at::glyphFirstRow) = 0;
    W(at::numberDigits) = W(at::modeScoreShown);
    W(at::numberAt) = display::countdownScore;
    W(at::drawRoutine) = F(at::drawNumber13);
  });
  bind(at::dmMsgScrollUp, [=, this] {
    W(at::task) = F(at::msgScrollUpTask);
    W(at::msgScrollText) = arg(2);
    W(at::msgScrollTarget) = arg(4);
    W(at::msgScrollRow) = 0x10;
    nextStep(6);
  });
  bind(at::dmMsgScrollDown, [=, this] {
    W(at::task) = F(at::msgScrollDownTask);
    W(at::msgScrollText) = arg(2);
    W(at::msgScrollTarget) = arg(4);
    W(at::msgScrollRow) = 0xfff3;
    nextStep(6);
  });
  bind(at::msgScrollUpTask, [=, this] {
    --W(at::msgScrollRow);
    if (W(at::msgScrollRow).s() <= 2) {
      W(at::fillAt) = static_cast<u16>((W(at::msgScrollRow) + 0x0d) * kRow + kRow);
      W(at::fillWidth) = 0x50;
      W(at::fillRows) = 1;
      fillDisplay(W(at::fillAt), 0x50, 1);
    }
    setFont(font::h13);
    slide();
  });
  bind(at::msgScrollDownTask, [=, this] {
    ++W(at::msgScrollRow);
    if (W(at::msgScrollRow).s() >= 1) {
      W(at::fillAt) = static_cast<u16>((W(at::msgScrollRow) - 1) * kRow + kRow);
      W(at::fillWidth) = 0x50;
      W(at::fillRows) = 1;
      fillDisplay(W(at::fillAt), 0x50, 1);
    }
    setFont(font::h13);
    slide();
  });

  // --- wipes: the display cleared a little each frame
  bind(at::dmClear, [this] {  // at once
    W(at::taskState) = 1;
    W(at::task) = F(at::dmClearTask);
    W(at::wipeAt) = 0;
    nextStep(2);
  });
  bind(at::dmClearTask, [this] {
    W(at::wipeAt) += kRow;
    W(at::fillAt) = W(at::wipeAt);
    W(at::fillWidth) = 0x50;
    W(at::fillRows) = 0x10;
    fillDisplay(W(at::fillAt), 0x50, 0x10);
    si = 0;
  });
  bind(at::dmWipeDown, [this] {  // from the top down
    W(at::taskState) = 1;
    W(at::task) = F(at::wipeDownTask);
    W(at::wipeAt) = 0;
    nextStep(2);
  });
  bind(at::wipeDownTask, [this] {
    W(at::wipeAt) += kRow;
    if (W(at::wipeAt) == display::end) {
      si = 0;
      return;
    }
    W(at::fillAt) = W(at::wipeAt);
    W(at::fillWidth) = 0x50;
    W(at::fillRows) = 1;
    fillDisplay(W(at::fillAt), 0x50, 1);
    si = 1;
  });
  bind(at::dmWipeRight, [this] {  // from the left
    W(at::taskState) = 1;
    W(at::task) = F(at::wipeRightTask);
    W(at::wipeAt) = display::beforeTop;
    nextStep(2);
  });
  bind(at::wipeRightTask, [this] {
    W(at::wipeAt) += 1;
    if (W(at::wipeAt) == display::rightEnd) {
      si = 0;
      return;
    }
    W(at::fillAt) = W(at::wipeAt);
    W(at::fillWidth) = 1;
    W(at::fillRows) = 0x10;
    fillDisplay(W(at::fillAt), 1, 0x10);
    si = 1;
  });
  bind(at::dmWipeDownStriped, [this] {  // as a blind
    W(at::taskState) = 1;
    W(at::task) = F(at::wipeDownStripedTask);
    W(at::wipeAt) = 0;
    nextStep(2);
  });
  bind(at::wipeDownStripedTask, [this] {
    W(at::wipeAt) += kRow;
    if (W(at::wipeAt) == display::stripedEnd) {
      si = 0;
      return;
    }
    for (u16 band : {u16{0}, u16{0x2a0}, u16{0x540}, u16{0x7e0}}) {
      W(at::fillAt) = static_cast<u16>(W(at::wipeAt) + band);
      W(at::fillWidth) = 0x50;
      W(at::fillRows) = 1;
      fillDisplay(W(at::fillAt), 0x50, 1);
    }
    si = 1;
  });
}

}  // namespace encore
