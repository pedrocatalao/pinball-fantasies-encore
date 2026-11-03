// The frame: the driver's two callbacks, the program's own loop, the keyboard, the timers.
#include "engine/table/Engine.h"

namespace encore {

Engine::Engine(ByteView prg, int table) : Program(prg, table) {
  for (auto& plane : video_) plane.assign(0x10000, 0);
  // Which slope each stretch of eight dots lies on, worked out once from the masks as the
  // program comes with them: the first of the stretch to its left, itself and the one to
  // its right that has nothing solid in it says so (cs:59d9), and 0xff if none has.
  for (int layer = 0; layer < 2; ++layer) {
    const u16 marks = S(layer == 0 ? at::marksGround : at::marksRamps), walls = S(layer == 0 ? at::wallsGround : at::wallsRamps),
              slope = S(layer == 0 ? at::slopesGround : at::slopesRamps);
    auto& out = slopes_[static_cast<std::size_t>(layer)];
    out.assign(0x28 * 0x240, 0xff);
    auto clear = [&](u16 at) { return farB(marks, at) == 0 && farB(walls, at) == 0; };
    for (u16 at = 0; at < out.size(); ++at) {
      if (at != 0 && clear(static_cast<u16>(at - 1))) out[at] = farB(slope, static_cast<u16>(at - 1)) & 0x0f;
      else if (clear(at)) out[at] = farB(slope, at) & 0x0f;
      else if (clear(static_cast<u16>(at + 1))) out[at] = farB(slope, static_cast<u16>(at + 1)) & 0x0f;
    }
  }
  bind(at::nothing, [] {});  // the original's "nothing": a lone ret, which empty slots point at
  bindDisplay();
  bindGame();
  bindPlunger();
}

void Engine::effect(u16 record) {
  sound->effect(B(record), B(record, 1), 0, static_cast<u8>(B(record, 3) + 1));
}

void Engine::frame() {
  if (keysFirst)
    for (int i = 0; i < loopsPerFrame; ++i) mainLoop();
  frameCallback();
  midFrameCallback();
  if (!keysFirst)
    for (int i = 0; i < loopsPerFrame; ++i) mainLoop();
}

/// The ball's moves at the start of the frame: the two that are due there, and with
/// `stepsTogether` the ones from part way down the frame as well.
void Engine::frameSteps() {
  physicsSteps();
  if (!stepsTogether) return;
  B(at::inMidFrame) = 0xff;
  physicsSteps();
  B(at::inMidFrame) = 0;
}

// ---------------------------------------------------------------------------------------
// cs:3e44, the keyboard interrupt
// ---------------------------------------------------------------------------------------
void Engine::key(u8 al) {
  if (!(al & 0x80)) {
    B(at::lastKey) = al;
    CB(at::keyDown) = 0x0f;
  }
  if (CB(at::extendedKey) == 0xff) return keyExtended(al);
  if (al == 0xe0) {
    CB(at::extendedKey) = 0xff;
    return;
  }
  if (al == 0x57) { B(at::scrollKey) = 0x0b; return; }  // F11
  if (al == 0x58) { B(at::scrollKey) = 0x0c; return; }  // F12
  // left shift, alt, ctrl: the left flipper
  if ((al == 0x2a || al == 0x38 || al == 0x1d) && !(B(at::keys) & 2)) {
    B(at::keys) |= 2;
    if (!amended || B(at::flippersLive) != 0) B(at::flipperPressed) = 0xff;  // (amended: not while the flippers are dead)
    if (B(at::flippersLive) != 0) effect(at::soundFlipper);
    return;
  }
  if (al == 0x36 && !(B(at::keys) & 1)) {  // right shift: the right flipper
    B(at::keys) |= 1;
    if (!amended || B(at::flippersLive) != 0) B(at::flipperPressed) = 0xff;  // (amended: not while the flippers are dead)
    if (B(at::flippersLive) != 0) effect(at::soundFlipper);
    return;
  }
  if (al == 0xb6) { B(at::keys) &= 0xfe; return; }
  if (al == 0xaa || al == 0xb8 || al == 0x9d) { B(at::keys) &= 0xfd; return; }
  if (al == 0x39) { B(at::spaceHeld) = 0xff; return; }  // space: the nudge, held
  if (al == 0xb9) {
    B(at::spaceHeld) = 0;
    CB(at::nudgeHeld) = 0;
  }
}

/// cs:3f8a: the key after an 0xe0, which is how the arrows and the right-hand alt and ctrl
/// arrive.
void Engine::keyExtended(u8 al) {
  CB(at::extendedKey) = 0;
  if (B(at::attract) == 0xff) {  // no game: page up and page down choose the mid-frame steps
    if (al == 0x49) B(at::keys) &= 0xfb;
    else if (al == 0x51) B(at::keys) |= 4;
    return;
  }
  switch (al) {
    case 0x50:  // down arrow: the plunger is pulled
      W(at::plungerRoutine) = F(at::plungerPulling);
      B(at::plungerHeld) = 0xff;
      break;
    case 0xd0:  // and let go
      W(at::plungerRoutine) = F(at::plungerLetGo);
      B(at::plungerHeld) = 0;
      break;
    case 0x38: case 0x1d:  // right alt, right ctrl
      if (!(B(at::keys) & 1)) {
        B(at::keys) |= 1;
        if (!amended || B(at::flippersLive) != 0) B(at::flipperPressed) = 0xff;  // (amended: not while the flippers are dead)
        if (B(at::flippersLive) != 0) effect(at::soundFlipper);
      }
      break;
    case 0xb8: case 0x9d: B(at::keys) &= 0xfe; break;
    default: break;
  }
}

// ---------------------------------------------------------------------------------------
// cs:41c6, the callback at the start of the frame
// ---------------------------------------------------------------------------------------
void Engine::frameCallback() {
  ++W(at::frameCount);
  B(at::coloursInterrupted) = 0xff;
  B(at::lightsOutInterrupted) = 0xff;
  if (B(at::callbacksOn) != 0xff) return;
  B(at::frameOnTime) = 0xff;  // the driver says no frame was missed
  if (B(at::afterPause) != 0) --B(at::afterPause);
  W(at::loopCounter) += 0x406;
  if (B(at::attract) == 0xff) return attractFrame();
  B(at::inFrameCallback) = 0;
  if (CB(at::midFrameDue) == 0xff) return;
  const u8 busy = B(at::stepsBusy);
  B(at::stepsBusy) = 0xff;
  if (busy == 0xff) return;

  // Which of the ball and the flippers is drawn before the sub-steps and which after depends
  // on how far down the screen each is; the flags are kept as the original keeps them.
  B(at::thirdWithBall) = 0;
  B(at::thirdMidFrame) = 0;
  const u16 ballY = W(at::ballY);
  if (ballY >= kw(at::thirdBallTopInsn, 4) && ballY <= kw(at::thirdBallBottomInsn, 4)) {
    B(at::thirdWithBall) = 0xff;
  } else if (W(at::screenTop) >= (high() ? kw(at::thirdScreenTopHighInsn, 1) : kw(at::thirdScreenTopLowInsn, 1))) {
    B(at::thirdMidFrame) = 0xff;
  } else {
    drawFlipper(2);
  }
  if (ballY >= static_cast<u16>(W(at::screenTop) + (high() ? 0x9e : 0x67))) {
    if (B(at::drawOrder) != 0xff) {
      B(at::flippersWithBall) = 0xff;
      drawBall();
    } else {
      B(at::drawOrder) = 0;
      drawFlipper(0);
      drawFlipper(1);
    }
    frameSteps();
    B(at::ballMidFrame) = 0;
  } else if (B(at::drawOrder) != 0xff) {
    B(at::drawOrder) = 0xff;
    B(at::ballMidFrame) = 0xff;
    B(at::flippersWithBall) = 0xff;
    drawBall();
    frameSteps();
  } else {
    drawFlipper(0);
    drawFlipper(1);
    frameSteps();
    B(at::ballMidFrame) = 0xff;
    if (static_cast<u16>(W(at::screenTop) + (high() ? 0xa3 : 0x6c)) < kw(at::thirdFlipperRowInsn, 1)) drawFlipper(2);
  }
  afterSteps();
  B(at::stepsBusy) = 0;
  if (B(at::rulesBusy) != 0) {
    CB(at::midFrameDue) = 0xff;
    return;
  }
  CB(at::midFrameDue) = 0xff;
  B(at::rulesBusy) = 0xff;
  todo(at::tableFrame);
  runRules();
  call(W(at::plungerRoutine));
  if (B(at::frameOnTime) != 0) {
    const u8 al = CB(at::displayBusy);
    CB(at::displayBusy) = 0xff;
    if (al != 0xff) {
      displayStep();
      CB(at::displayBusy) = 0;
    }
  }
  flushColours();
  B(at::rulesBusy) = 0;
}

/// cs:4140: the ball is drawn where it now is, and with it whichever flippers are due. The
/// engine draws nothing: it keeps where the original would have drawn each, for whoever
/// makes the picture.
void Engine::drawBall() {
  if (B(at::flippersWithBall) == 0xff) {
    drawFlipper(0);
    drawFlipper(1);
  }
  if (B(at::thirdWithBall) == 0xff) drawFlipper(2);
  shown_.ballX = W(at::ballX).s();
  shown_.ballY = static_cast<i16>(W(at::ballY) + (B(at::ballHidden) == 0xff ? 0 : W(at::nudgeLift)));
  shown_.ramps = B(at::layer) != 0;
  ballDrawn_ = true;
}

/// cs:550e: a flipper is drawn as it now stands.
void Engine::drawFlipper(int which) {
  const u16 record = static_cast<u16>(A(at::flippers) + which * flipperRec::size);
  if (which == 2 && nativeB(record) == 0) return;
  const u16 now = nativeW(static_cast<u16>(record + flipperRec::picture)), was = nativeW(static_cast<u16>(record + flipperRec::pictureDrawn));
  nativeW(static_cast<u16>(record + flipperRec::pictureDrawn)) = now;
  shown_.flipper[static_cast<std::size_t>(which)] = now;
  if (now != was && onFlipperDrawn) onFlipperDrawn(record, was, now);
}

// ---------------------------------------------------------------------------------------
// cs:55db, the callback part way down the frame
// ---------------------------------------------------------------------------------------
void Engine::midFrameCallback() {
  B(at::coloursInterrupted) = 0xff;
  B(at::lightsOutInterrupted) = 0xff;
  if (B(at::inFrameCallback) != 0) return;
  if (B(at::callbacksOn) != 0xff) return;
  B(at::frameOnTime) = 0xff;
  if (B(at::attract) == 0xff) return attractMidFrame();
  if (B(at::inMidFrame) != 0) return;
  if (CB(at::midFrameDue) != 0xff) return;
  scroll();
  if (B(at::stepsBusy) != 0) return;
  B(at::inMidFrame) = 0xff;
  if (!stepsTogether) physicsSteps();
  if (B(at::ballMidFrame) == 0xff) {
    B(at::flippersWithBall) = 0;
    drawBall();
  }
  if (B(at::thirdMidFrame) == 0xff) drawFlipper(2);
  if (const u8 al = B(at::startKey); al != 0) {  // a game was asked for: with this many players
    B(at::startKey) = 0;
    B(at::players) = static_cast<u8>(al - 0x3a);
    B(at::playersDigit) = static_cast<u8>(al - 0x3a + 0x37);
    startScript(A(at::scriptGameStartPlayers));
    effect(at::soundGameStart);
    B(at::playersMayJoin) = 0;
    addTimer(F(at::setStartKeysActiveTimer));
  }
  CB(at::midFrameDue) = 0;
  B(at::inMidFrame) = 0;
}

/// cs:6011: the frame while no game is played.
void Engine::attractFrame() {
  attractLights();
  displayFlash();
  if (B(at::frameOnTime) != 0) {
    const u8 al = CB(at::displayBusy);
    CB(at::displayBusy) = 0xff;
    if (al != 0xff) {
      displayStep();
      CB(at::displayBusy) = 0;
    }
  }
  flushColours();
}

/// cs:60c5: the screen drifts up and down the table, and a game starts when one is asked for.
void Engine::attractMidFrame() {
  const i16 bottom = high() ? 0x103 : 0x171;
  if (W(at::lookAt).s() >= bottom) W(at::attractScroll) = 0xffff;
  if (W(at::lookAt).s() <= 0) W(at::attractScroll) = 1;
  W(at::lookAt) += W(at::attractScroll);
  scroll();
  if (const u8 al = B(at::startKey); al != 0) {
    B(at::startKey) = 0;
    B(at::players) = static_cast<u8>(al - 0x3a);
    B(at::playersDigit) = static_cast<u8>(al - 0x3a + 0x37);
    startScript(A(at::scriptGameStart));
    effect(at::soundGameStart);
    W(at::lookAt) = 0xffff;
    B(at::attract) = 0;  // cs:5ff9
    newGame();
    todo(at::tableNewGame);
    todo(at::tableGameMusic);
    todo(at::serveBall);
    B(at::playersMayJoin) = 0;
    addTimer(F(at::setStartKeysActiveTimer));
  }
}

/// cs:4018: where the screen looks. It follows the ball, more or less quickly as the options
/// say, unless something has said where to look.
void Engine::scroll() {
  i16 row = static_cast<i16>(W(at::ballY) - (high() ? 0x82 : 0x4b));
  const i16 last = high() ? 0x103 : 0x171;
  if (row < 0) row = 0;
  else if (static_cast<u16>(row) >= static_cast<u16>(last)) row = last;
  if (W(at::lookAt) != 0xffff) {
    row = static_cast<i16>(W(at::lookAt) + 0x21);
    W(at::scrollFixed) = static_cast<u16>(row << 4);
    W(at::screenTop) = static_cast<u16>(row - 0x21);
    screenRow_ = static_cast<u16>(row);
    return;
  }
  if (W(at::lookAtSoft) != 0xffff) row = W(at::lookAtSoft).s();
  row = static_cast<i16>(row + 0x21);
  i16 step = static_cast<i16>((row - (W(at::scrollFixed).s() >> 4)) * W(at::scrollSpeed).s());
  W(at::scrollFixed) += static_cast<u16>(step >> 2);
  i16 off = static_cast<i16>(row - (W(at::scrollFixed).s() >> 4));
  if (off >= 0) {
    off = static_cast<i16>(off - (high() ? 0xaa : 0x73));
    if (off >= 0) W(at::scrollFixed) += static_cast<u16>(off << 4);
  } else {
    off = static_cast<i16>(off + (high() ? 0x82 : 0x4b));
    if (off <= 0) W(at::scrollFixed) += static_cast<u16>(off << 4);
  }
  const i16 shown = static_cast<i16>((W(at::scrollFixed).s() >> 4) + W(at::nudgeLift).s());
  screenRow_ = static_cast<u16>(shown);
  W(at::screenTop) = static_cast<u16>(shown - 0x21);
}

// ---------------------------------------------------------------------------------------
// cs:35fd, the program's own loop
// ---------------------------------------------------------------------------------------
void Engine::mainLoop() {
  if (pause_ != Pause::No) return paused();  // the original waits inside the pause for a key
  if (CB(at::keysOn) != 0) {
    u8& key = B(at::lastKey);
    if (key == 0x43) { key = 0xff; W(at::scrollSpeed) = 0x09; }  // F9, F10: how fast the screen follows
    if (key == 0x44) { key = 0xff; W(at::scrollSpeed) = 0x0b; }
    if (B(at::scrollKey) == 0x0b) { B(at::scrollKey) = 0; W(at::scrollSpeed) = 0x14; }
    if (B(at::scrollKey) == 0x0c) { B(at::scrollKey) = 0; W(at::scrollSpeed) = 0x28; }
    if (B(at::attract) == 0xff) {  // no game
      playersKey();
      typeCheat();
      if (key == 0x01) {  // escape: asks whether to quit (cs:3196)
        key = 0xff;
        if (CB(at::quitAsked) != 0xff) {
          CB(at::keysOn) = 0;
          CB(at::quitAsked) = 0xff;
          startScript(A(at::scriptConfirmQuit));
        }
      }
    } else if (B(at::betweenBalls) != 0xff) {
      bool keys = true;
      if (B(at::ballOnPlunger) == 0xff) {
        if (key == 0x01) {  // escape while the ball waits on the plunger: the game is given up (cs:32b1)
          key = 0xff;
          placeBall(0x12c, 0x23a);
          CB(at::keysOn) = 0xff;
          addTimer(F(at::gameOverTimer));
          call(F(at::tableAttractMusic));
          B(at::lastKey) = 0xff;
          displaySteady();
          startScript(A(at::scriptAttract));
        }
        if (CB(at::keysOn) == 0) keys = false;
        else playersKey();
      }
      if (keys) {
        if (key == 0x39) {
          key = 0xff;
          // (amended, with the keys read first: the shake is counted after the frame's moves,
          // so a tilt takes the flippers from the next frame on, not from this one)
          if (amended && keysFirst) nudgeDue_ = true;
          else nudgeKey();
        }
        if (key == 0x32) { key = 0xff; musicKey(); }
        if (key == 0x19) { key = 0xff; B(at::pauseAsked) = 0xff; }
        pauseKey();
        if (pause_ != Pause::No) return;  // the rest of this turn waits with it
      }
    } else if (amended && (key == 0x19 || key == 0x32) && W(at::task) != F(at::highScoreName)) {
      // While a ball is being lost the original takes no keys, and keeps the last one pressed:
      // a P or an M pressed then does its work when the next ball comes. Amended: it is
      // dropped. (Not while initials are typed, which are read from the same place.)
      key = 0xff;
    }
  }
  if (B(at::lastKey) == 0x01) B(at::lastKey) = 0xff;
  ++W(at::loopCounter);
  if (B(at::ballHidden) != 0 && !drawsChance()) {  // the spin a ball will be served with is drawn afresh all the while
    u16 spin = W(at::loopCounter) & 0x3ff;
    if (spin & 1) spin = static_cast<u16>(-spin);
    W(at::spin) = spin;
  }
  if (CB(at::quit) == 0xff) {
    exited_ = true;
    return;
  }
  // The driver is polled; the silent one calls the music's callback every time it is.
  if (pollCallsMusic) musicCallback(8);
}

/// cs:33f9: F1 to F8 start a game for that many players; enter adds one.
void Engine::playersKey() {
  u8 al = B(at::lastKey);
  if (al >= 0x3b && al <= 0x42) {
    B(at::lastKey) = 0xff;
    if (B(at::playersMayJoin) == 0) return;
    if (refuseFewerPlayers && B(at::attract) != 0xff && al - 0x3a < B(at::player)) return;
    B(at::playersMayJoin) = 0;
    B(at::startKey) = al;
    B(at::lastStartKey) = al;
    B(at::countPlayersAfresh) = 0;
  }
  if (al != 0x1c) return;
  B(at::lastKey) = 0xff;
  if (B(at::players) >= 8) return;
  if (B(at::playersMayJoin) == 0) return;
  if (refuseFewerPlayers && B(at::attract) != 0xff && (B(at::countPlayersAfresh) != 0 ? 0 : B(at::players)) + 1 < B(at::player)) return;
  if (B(at::countPlayersAfresh) != 0) {
    B(at::players) = 0;
    B(at::countPlayersAfresh) = 0;
  }
  ++B(at::players);
  al = static_cast<u8>(B(at::players) + 0x3a);
  B(at::playersMayJoin) = 0;
  B(at::startKey) = al;
  B(at::lastStartKey) = al;
}

/// cs:3549: letters typed while no game is played are matched against the cheats' words.
void Engine::typeCheat() {
  u8 al = B(at::lastKey);
  if (al == 0xff) return;
  al = B(at::scancodeLetters, al);
  if (al == 0) return;
  B(at::cheatLetter) = al;
  B(at::lastKey) = 0xff;
  B(at::cheatBuffer, W(at::cheatTyped)) = al;
  ++W(at::cheatTyped);
  if (W(at::cheatTyped) > 0x0c) {
    W(at::cheatTyped) = 0;
    for (u16 i = 0; i < 0x0c; ++i) B(at::cheatBuffer, i) = 0x20;
    return;
  }
  u16 s = A(at::cheats);
  B(at::cheatPrefix) = 0;
  for (;;) {
    const u16 handler = nativeW(s);
    s = static_cast<u16>(s + 2);
    if (handler == 0xff) break;
    u16 typed = 0;
    bool skip = false;
    for (;;) {
      const u8 c = nativeB(s++);
      if (c == 0x24) {  // the whole word: its routine
        W(at::cheatTyped) = 0;
        call(handler);
        return;
      }
      if (typed >= W(at::cheatTyped)) {  // so far so good, with more to come
        B(at::cheatPrefix) = 0xff;
        skip = true;
        break;
      }
      ++typed;
      if (c != B(at::cheatBuffer, static_cast<u16>(typed - 1))) {
        skip = true;
        break;
      }
    }
    if (skip)
      while (nativeB(s++) != 0x24) {}
  }
  if (B(at::cheatPrefix) == 0xff) return;
  W(at::cheatTyped) = 1;  // no word begins like this: start again from this letter
  B(at::cheatBuffer) = B(at::cheatLetter);
  for (u16 i = 1; i < 0x0c; ++i) B(at::cheatBuffer, i) = 0x20;
}

/// cs:3478: a press of the space bar shakes the table.
void Engine::nudgeKey() {
  if (B(at::noTilt) == 0xff) return;
  if (B(at::ballUntouched) != 0xff && B(at::tilted) != 0xff && B(at::ballLost) != 0xff && CB(at::nudgeHeld) != 0xff) {
    W(at::tiltCounter) += 0x3c;
    CB(at::nudgeHeld) = 0xff;
    if (W(at::tiltCounter) > 0x78) {
      B(at::tilted) = 0xff;
      todo(at::tableTilt);
    } else if (W(at::tiltCounter) > 0x3c) {
      todo(at::tableTiltWarning);
    }
  }
  B(at::lastKey) = 0xff;
}

/// cs:34e3: M turns the music off and on.
void Engine::musicKey() {
  B(at::musicOff) ^= 0xff;
  if (B(at::musicOff) == 0) {
    B(at::tunePlunger) = 0;
    B(at::tuneMain) = kb(at::musicOnTuneInsn, 1);
    B(at::musicNext) = kb(at::musicOnPlaceInsn, 4);
    if (B(at::ballUntouched) == 0xff) B(at::musicNext) = 0;
    if (static_cast<i8>(B(at::musicRepeats)) < 1) B(at::musicRepeats) = 1;
  } else {
    const u8 silence = kb(at::musicOffPlaceInsn, 1);  // the place in the table's music where nothing plays
    B(at::tunePlunger) = silence;
    B(at::tuneMain) = silence;
    if (B(at::musicNext) != silence) addTimer(F(at::musicOffTimer));
  }
}

/// cs:31cd: P pauses. The callbacks stop doing anything, the music stops, and the display
/// says so over what it showed, which is kept to put back.
void Engine::pauseKey() {
  if (B(at::pauseAsked) == 0) return;
  B(at::callbacksOn) = 0;
  sound->stop();
  saveDisplay();
  B(at::lastKey) = 0xff;
  pause_ = Pause::AnyKey;
}

void Engine::paused() {
  const u8 key = B(at::lastKey);
  if (key == 0xff) return;
  if (pause_ == Pause::AnyKey && key == 0x01) {  // escape: leave the table?
    message(A(at::textReallyQuit), 0x150);
    B(at::lastKey) = 0xff;
    pause_ = Pause::YesOrNo;
    return;
  }
  if (pause_ == Pause::YesOrNo && B(at::scancodeLetters, key) == 0x59) {  // yes (cs:323a)
    B(at::ballHidden) = 0xff;
    restoreDisplay();
    B(at::pauseAsked) = 0;
    B(at::musicOff) ^= 0xff;
    CB(at::pauseQuit) = 0xff;
    sound->volume(0);
    B(at::callbacksOn) = 0xff;
    sound->start();
    exited_ = true;
    pause_ = Pause::No;
    return;
  }
  // any other key: on with the game (cs:3203)
  B(at::afterPause) = 0x1e;
  B(at::lastKey) = 0xff;
  sound->start();
  restoreDisplay();
  B(at::callbacksOn) = 0xff;
  B(at::pauseAsked) = 0;
  pause_ = Pause::No;
  // what the loop does after the pause, which this turn of it has yet to do
  if (B(at::lastKey) == 0x01) B(at::lastKey) = 0xff;
  ++W(at::loopCounter);
  if (B(at::ballHidden) != 0 && !drawsChance()) {
    u16 spin = W(at::loopCounter) & 0x3ff;
    if (spin & 1) spin = static_cast<u16>(-spin);
    W(at::spin) = spin;
  }
  if (pollCallsMusic) musicCallback(8);
}

void Engine::message(u16 text, u16 at) {
  W(at::fillAt) = 0xa8;
  W(at::fillWidth) = 0x50;
  W(at::fillRows) = 0x10;
  fillDisplay(0xa8, 0x50, 0x10);
  setFont(0);
  W(at::glyphRowsCut) = 0;
  W(at::glyphFirstRow) = 0;
  drawText(text, at);
}

void Engine::saveDisplay() {
  displayNormal();
  flushColours();
  u16 to = A(at::savedDisplay);
  B(at::savedDisplayBit) = 0x80;
  for (int plane = 0; plane < 2; ++plane)
    for (u16 row = 0; row < 0x10; ++row) {
      nativeB(to) = 0;
      for (u16 x = 0; x < 0x50; ++x) {
        if (dot(plane, static_cast<u16>(0xa8 + row * 0xa8 + x)) == lit()) nativeB(to) |= B(at::savedDisplayBit);
        const bool wrapped = B(at::savedDisplayBit) & 1;
        B(at::savedDisplayBit) = static_cast<u8>((B(at::savedDisplayBit) >> 1) | (wrapped ? 0x80 : 0));
        if (wrapped) nativeB(++to) = 0;
      }
    }
  W(at::savedDisplayAt) = to;
  W(at::drawSource) = A(at::textPaused);
  W(at::drawAt) = 0x162;
  message(A(at::textPaused), 0x162);
  displayNormal();
  flushColours();
}

void Engine::restoreDisplay() {
  u16 from = A(at::savedDisplay);
  B(at::savedDisplayBit) = 0x80;
  for (int plane = 0; plane < 2; ++plane)
    for (u16 row = 0; row < 0x10; ++row)
      for (u16 x = 0; x < 0x50; ++x) {
        dot(plane, static_cast<u16>(0xa8 + row * 0xa8 + x)) = (nativeB(from) & B(at::savedDisplayBit)) ? lit() : unlit();
        const bool wrapped = B(at::savedDisplayBit) & 1;
        B(at::savedDisplayBit) = static_cast<u8>((B(at::savedDisplayBit) >> 1) | (wrapped ? 0x80 : 0));
        if (wrapped) ++from;
      }
  W(at::savedDisplayAt) = from;
}

/// cs:3a6a: called by the driver when the music comes to a jump (and by the silent driver
/// whenever it is polled). What it is given and gives back in AL is the place to go on from.
u8 Engine::musicCallback(u8 al) {
  const u8 left = --B(at::musicRepeats);
  if (left == 0) {
    B(at::musicPriority) = B(at::musicAfter);
    B(at::musicRepeats) = 0;
    al = B(at::musicNext);
    B(at::jingleEnded) = 0xff;
    B(at::jingleEndedRules) = 0xff;
  } else if (left == 0xff || left == 0x7f) {  // below zero, as the original's signed test has it
    B(at::musicRepeats) = 0;
  }
  if (B(at::musicOff) != 0 && al <= kb(at::musicLastPlaceInsn, 1)) al = kb(at::silencePlaceInsn, 1);
  return al;
}

// ---------------------------------------------------------------------------------------
// timers
// ---------------------------------------------------------------------------------------
void Engine::addTimer(u16 native) {
  // The original looks for a free slot with BX and leaves it there. A step that goes on to
  // use BX as its place in the script, without having kept it, goes astray in the original
  // too (Party Land's cs:0b5b does), so BX is left the same way here.
  for (bx = A(at::timers); bx != A(at::timerCount); bx = static_cast<u16>(bx + 2)) {
    if (nativeW(bx) != F(at::nothing)) continue;
    ++W(at::timerCount);
    nativeW(bx) = native;
    if (timersRunning_) (startsNow_ ? startedNow_ : waiting_)[static_cast<std::size_t>(bx - A(at::timers)) / 2 % waiting_.size()] = true;
    return;
  }
  exited_ = true;  // the original gives up when all fifty are taken
}

void Engine::runTimers() {
  u16 slot = A(at::timers);
  // In the original a routine started by another of these is run in the same frame if the
  // free place it was given happens to come later in the list, and in the next if not: by
  // luck. Amended: it waits for the next frame wherever its place is; or, started with
  // `startsNow_`, is run in this one wherever its place is.
  const u16 first = slot;
  timersRunning_ = amended;
  for (int i = 0, slots = kw(at::runTimers, 1); i < slots; ++i, slot = static_cast<u16>(slot + 2)) {
    const std::size_t n = static_cast<std::size_t>(i) % waiting_.size();
    if (waiting_[n]) continue;
    startedNow_[n] = false;  // (its turn comes anyway)
    W(at::timerSlot) = slot;
    bx = slot;  // as the original has it, which matters to a routine that takes BX for its place in a script
    call(nativeW(slot));
  }
  for (bool again = amended; again;) {  // the ones started for this frame in places already passed
    again = false;
    slot = first;
    for (int i = 0, slots = kw(at::runTimers, 1); i < slots; ++i, slot = static_cast<u16>(slot + 2)) {
      const std::size_t n = static_cast<std::size_t>(i) % waiting_.size();
      if (!startedNow_[n]) continue;
      startedNow_[n] = false;
      again = true;
      W(at::timerSlot) = slot;
      bx = slot;
      call(nativeW(slot));
    }
  }
  timersRunning_ = false;
  waiting_.fill(false);
}

void Engine::endTimer() {
  nativeW(W(at::timerSlot)) = F(at::nothing);
  --W(at::timerCount);
}

bool Engine::countTo(u16 nativeCounter, u16 limit) {
  if (nativeW(nativeCounter) == limit) {
    nativeW(nativeCounter) = 0;
    return true;
  }
  ++nativeW(nativeCounter);
  return false;
}

}  // namespace encore
