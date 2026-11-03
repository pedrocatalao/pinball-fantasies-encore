#include "engine/table/Flow.h"

namespace encore {

Flow::Flow(ByteView prg, int table) : Engine(prg, table) { bindFlow(); }

void Flow::chain() {
  bx = static_cast<u16>(bx + 2);
  if (nativeW(bx) == 0) {
    W(at::nextStep) = 0;
    return;
  }
  W(at::nextStep) = bx;
  call(nativeW(bx));
}

void Flow::goTo(u16 script) {
  bx = script;
  W(at::nextStep) = bx;
  call(nativeW(bx));
}

void Flow::serve() {
  B(at::betweenBalls) = 0;
  B(at::ballLost) = 0;
  B(at::ballUntouched) = 0xff;
  beginBall();
  lightsOut();  // cs:009d
  stopBlinks();
  clearBall();
  restorePlayer();
  afterRestore();
  B(at::ballHidden) = 0xff;
  placeBall(kw(at::serveXInsn, 4), kw(at::serveYInsn, 4));
  lostParked_ = false;
  W(at::lookAtSoft) = 0xffff;
  if (B(at::playersMayJoin) != 0xff) serveMusic();
  B(at::musicNext) = 0;
  B(at::scoredThisBall) = 0;
  if (B(at::playersMayJoin) != 0xff || servesAlike) startBall();
  else addTimer(F(at::issueBallFinishTimer));
}

void Flow::startBall() {
  addTimer(F(at::issueBallSfxTimer));
  addTimer(F(at::issueBallReleaseTimer));
  if (has(at::issueBallRaiseSfxTimer)) addTimer(F(at::issueBallRaiseSfxTimer));  // (Stones 'n Bones has no sound here)
  B(at::flippersLive) = 0xff;
  B(at::tilted) = 0;
  W(at::tiltCounter) = 0;
  B(at::scoredThisBall) = 0;
}

void Flow::bindFlow() {
  auto arg = [this](u16 n) { return nativeW(static_cast<u16>(bx + n)); };
  auto wait1 = [this] {
    W(at::taskState) = 1;
    W(at::task) = F(at::waitTask);
  };
  auto plain = [this](int font) {
    setFont(font);
    W(at::glyphRowsCut) = 0;
    W(at::glyphFirstRow) = 0;
  };
  auto holdBonus = [this] {  // the bonus as it was before it was counted, if it is to be kept
    if (B(at::holdBonus) != 0xff) return;
    for (u16 i = 0; i < 12; ++i) B(at::bonus, i) = B(at::bonusKept, i);
  };

  bind(at::serveBall, [this] { serve(); });
  bind(at::issueBallFinishTimer, [this] {
    if (!countTo(A(at::issueBallFinishCounter), 0x1e)) return;
    startBall();
    endTimer();
  });
  if (has(at::issueBallRaiseSfxTimer)) bind(at::issueBallRaiseSfxTimer, [this] {
    if (!countTo(A(at::issueBallRaiseSfxCounter), 5)) return;
    effect(at::soundRaiseTargets);
    endTimer();
  });
  bind(at::issueBallSfxTimer, [this] {
    // (the sound of the ball coming up: three of the tables wait 50 frames for it and one 45;
    // amended: all 45)
    if (!countTo(A(at::issueBallSfxCounter), amended ? 0x2d : kw(at::issueBallSfxTimer, 1))) return;
    effect(at::soundIssueBall);
    endTimer();
  });
  bind(at::issueBallReleaseTimer, [this] {  // the ball appears at the top of the lane, rolling right
    if (!countTo(A(at::issueBallReleaseCounter), 0x50)) return;
    placeBall(kw(at::laneTopXInsn, 4), kw(at::laneTopYInsn, 4));
    W(at::ballVx) = kw(at::laneTopVxInsn, 4);
    B(at::ballHidden) = 0;
    W(at::lookAtSoft) = 0xffff;
    endTimer();
  });
  // --- the bonus is counted into the score, a digit's worth at a time
  bind(at::accBonus, [this] {
    CW(at::accBonusFrame) = 0;
    W(at::bonusAdding) = A(at::bonusUnit);
    W(at::bonusDigit) = A(at::bonus + 11);
    for (u16 i = 0; i < 12; ++i) B(at::bonusKept, i) = B(at::bonus, i);
    W(at::task) = F(at::accBonusTask);
    nextStep(2);
  });
  bind(at::accBonusTask, [this] {
    si = 0x4b;
    if (++CW(at::accBonusFrame) != 4) return;
    CW(at::accBonusFrame) = 0;
    for (;;) {
      const u16 digit = W(at::bonusDigit);
      if (nativeB(digit) == 0) {  // this digit is done: the next one up
        if (++W(at::bonusAdding) == A(at::bonusUnit + 12)) {
          W(at::fillAt) = 0x498;
          W(at::fillWidth) = 0x1c;
          W(at::fillRows) = 0x0a;
          fillDisplay(0x498, 0x1c, 0x0a);
          si = 0;
          return;
        }
        --W(at::bonusDigit);
        continue;
      }
      if (--nativeB(digit) == 0) {
        bool left = false;
        for (u16 i = 0; i < 12; ++i)
          if (B(at::bonus, i) != 0) left = true;
        if (left) CW(at::accBonusFrame) = 0xfff6;  // a pause before the next digit
      }
      addScore(A(at::score), W(at::bonusAdding));
      effect(at::soundTickBonus);
      W(at::numberDigits) = A(at::bonus);
      W(at::numberAt) = 0x488;
      W(at::glyphFirstRow) = 0;
      W(at::drawRoutine) = F(at::drawNumber8);
      drawScore(A(at::score), 0xc8);
      si = 0x4b;
      return;
    }
  });
  bind(at::anotherBall, [=, this] {
    holdBonus();
    if (++B(at::ballDigit) >= 0x41) {
      B(at::ballDigit) = 0x37;
      if (B(at::ballTens) == 0x38) ++B(at::ballTens);
      else B(at::ballTens) = 0x38;
    }
    savePlayer(currentPlayer());
    const u16 step = bx;
    addTimer(F(at::issueBallTimer));
    bx = step;
    chain();
  });

  // --- the best scores: has any player beaten one?
  bind(at::recordHighScores, [this] {
    W(at::taskState) = 1;
    CB(at::highScorePlayers) = 0;
    CW(at::highScorePlayer) = 0;
    W(at::task) = F(at::recordHighScoresTask);
    nextStep(4);
  });
  bind(at::recordHighScoresTask, [=, this] {
    const u16 score = static_cast<u16>(A(at::playerRecords) + CW(at::highScorePlayer));
    CW(at::highScorePlayer) += playerSize();
    if (++CB(at::highScorePlayers) > B(at::players)) {  // every player looked at
      B(at::highScoreJingle) = 0;
      if (B(at::gotHighScore) != 0xff) {
        B(at::musicPriority) = 0;
        B(at::musicRepeats) = 1;
        music(A(at::tuneGameOverSad));
        B(at::musicPriority) = 0;
        B(at::sadJingle) = 0xff;
      }
      B(at::gotHighScore) = 0;
      si = 0;
      return;
    }
    si = 1;
    for (u16 left = 4, entry = kw(at::bestScoresCompareInsn, 1); left > 0; --left, entry = static_cast<u16>(entry + 0x10)) {
      int order = 0;
      for (u16 i = 0; i < 12 && order == 0; ++i) {
        const u8 mine = nativeB(static_cast<u16>(score + i)), best = nativeB(static_cast<u16>(entry + i));
        order = mine < best ? -1 : mine > best ? 1 : 0;
      }
      if (order <= 0) continue;
      CW(at::highScorePlace) = left;
      if (B(at::highScoreJingle) != 0xff) {
        B(at::highScoreJingle) = 0xff;
        B(at::musicPriority) = 0;
        B(at::musicRepeats) = 1;
        B(at::gotHighScore) = 0xff;
        music(A(at::tuneGameOverHighScore));
      }
      W(at::taskState) = 1;
      B(at::highScorePlayerDigit) = static_cast<u8>(CB(at::highScorePlayers) + 0x37);
      W(at::task) = F(at::highScoreInsert);
      plain(0);
      drawText(A(at::textHighScore), 0x150);
      return;
    }
  });
  bind(at::highScoreInsert, [this] {  // the scores below make room, and this one goes in
    const u16 below = static_cast<u16>((CW(at::highScorePlace) - 1) * 0x10);
    const u16 from = kw(at::bestScoreThirdEndInsn, 1), to = kw(at::bestScoreFourthEndInsn, 1);  // the last bytes of the third and fourth
    for (u16 i = 0; i < below; ++i) nativeB(static_cast<u16>(to - i)) = nativeB(static_cast<u16>(from - i));
    const u16 entry = static_cast<u16>(to - below - 0x0f);
    const u16 score = static_cast<u16>(A(at::playerRecords) + (CB(at::highScorePlayers) - 1) * playerSize());
    for (u16 i = 0; i < 12; ++i) nativeB(static_cast<u16>(entry + i)) = nativeB(static_cast<u16>(score + i));
    CW(at::highScoreLetters) = 3;
    CW(at::highScoreLetter) = static_cast<u16>(entry + 12);
    B(at::lastKey) = 0xff;
    W(at::task) = F(at::highScoreName);
    si = 1;
  });
  bind(at::highScoreName, [=, this] {  // three letters are typed for it
    u8 key = B(at::lastKey);
    bool typed = false;
    if (key != 0xff) {
      B(at::lastKey) = 0xff;
      key = B(at::scancodeLetters, key);
      if (key != 0) {
        typed = true;
        nativeB(CW(at::highScoreLetter)) = key;
        CW(at::highScoreLetter) += 1;
        B(at::highScoreInitials, static_cast<u16>(3 - CW(at::highScoreLetters))) = key;
        if (--CW(at::highScoreLetters) == 0) {
          W(at::taskState) = 0x3c;
          W(at::task) = F(at::highScoreNameDone);
          plain(0);
          drawText(A(at::textHighScore), 0x150);
          for (u16 i = 0; i < 3; ++i) B(at::highScoreInitials, i) = 0x20;
          si = W(at::taskState);
          return;
        }
      }
    }
    if (typed || redrawNameWhileWaiting()) {
      plain(0);
      drawText(A(at::textHighScore), 0x150);
    }
    si = W(at::taskState);
  });
  bind(at::highScoreNameDone, [=, this] {
    --si;
    if (si == 0x1e) {
      plain(0);
      drawText(A(at::textBlank), 0x150);
    }
    if (si <= 2) {
      W(at::task) = F(at::recordHighScoresTask);
      si = 1;
    }
  });

  bind(at::repeatSetup, [=, this] {  // (the next step entered past its count: sets it)
    CW(at::repeatCount) = arg(2);
    wait1();
    nextStep(6);
  });
  bind(at::repeatLoop, [=, this] {  // back to an earlier step, so many times
    if (--CW(at::repeatCount) != 0) return goTo(arg(4));
    CW(at::repeatCount) = arg(2);
    wait1();
    nextStep(6);
  });

  // --- each player's score in turn
  bind(at::eachScoreBegin, [=, this] {
    CW(at::eachScorePlayer) = static_cast<u16>(-playerSize());
    B(at::playerDigit) = 0x37;
    playerShown();
    CB(at::eachScoreLeft) = static_cast<u8>(B(at::players) + 1);
    wait1();
    nextStep(4);
  });
  bind(at::eachScoreNext, [=, this] {
    ++B(at::playerDigit);
    playerShown();
    CW(at::eachScorePlayer) += playerSize();
    if (--CB(at::eachScoreLeft) == 0) {
      wait1();
      nextStep(4);
      return;
    }
    W(at::taskState) = 1;
    W(at::task) = F(at::eachScoreDraw);
    if (CB(at::eachScoreLeft) == 1) return nextStep(4);
    bx = arg(2);
    W(at::nextStep) = bx;
  });
  bind(at::eachScoreDraw, [=, this] {
    plain(3);
    drawText(A(at::textPlayer), 0x150);
    drawScore(static_cast<u16>(CW(at::eachScorePlayer) + A(at::playerRecords)), 0xc8);
    si = 0;
  });

  // --- the match: a digit drawn by chance against the last-but-one of each score
  bind(at::match, [this] {
    W(at::task) = F(at::matchBegin);
    W(at::taskState) = 1;
    B(at::player) = 1;
    nextStep(4);
    if (amended) {  // the match begins in this frame, not the next
      si = 1;
      call(F(at::matchBegin));
      W(at::taskState) = si;
    }
  });
  bind(at::matchBegin, [=, this] {
    B(at::musicRepeats) = 1;
    music(A(at::tuneMatchStart));
    // (after the match, silence; the Gameshow's names a place past the end of its music, so
    // that its music starts again from the top. Amended: the table's own silence)
    B(at::musicNext) = amended ? kb(at::silencePlaceInsn, 1) : kb(at::musicAfterMatchInsn, 4);
    W(at::task) = F(at::matchDigit);
    for (u16 player = 0; player < B(at::players); ++player) {
      const u16 digit = static_cast<u16>(0x2000 | static_cast<u8>(nativeB(static_cast<u16>(A(at::playerRecords) + 0x0a + player * playerSize())) + 0x37));
      W(at::matchPlayers, static_cast<u16>(player * 2)) = digit;
      W(at::matchWinners, static_cast<u16>(player * 2)) = digit;
    }
    plain(3);
    drawText(A(at::matchPlayers), 0xa8);
    // (amended: the digit the match sets out from is drawn by chance too, and the next is
    // never the same again: the one after it is shown instead, on every table)
    if (amended) W(at::matchNumber) = chance(10, W(at::matchNumber));
    si = matchLength();
    W(at::taskState) = si;
    matchPace();
  });
  bind(at::matchDigit, [=, this] {
    if (--CW(at::matchFrames) != 0) return;
    matchPace();
    plain(3);
    drawText(A(at::matchBlank), static_cast<u16>(0x540 + (W(at::matchNumber) << 3)));  // the last digit rubbed out
    u16 digit = chance(10, W(at::loopCounter) % 10);
    if (W(at::matchNumber) == digit) digit = amended ? (digit == 9 ? 0 : static_cast<u16>(digit + 1)) : matchRepeated(digit);
    W(at::matchNumber) = digit;
    B(at::matchDigitText) = static_cast<u8>(digit + 0x37);
    plain(3);
    W(at::drawSource) = A(at::matchDigitText);
    W(at::drawAt) = static_cast<u16>(0x540 + (digit << 3));
    W(at::drawRoutine) = F(at::drawText);
    if (--si != 0) return;
    // the last one stands: who has it?
    CB(at::matchedAny) = 0;
    for (u16 player = 0; player < B(at::players); ++player) {
      if (B(at::matchPlayers, static_cast<u16>(player * 2)) == static_cast<u8>(digit + 0x37)) CB(at::matchedAny) = 0xff;
      else W(at::matchWinners, static_cast<u16>(player * 2)) = 0x2a2a;
    }
    si = 0;
    if (CB(at::matchedAny) != 0xff) return;
    W(at::flashPeriod) = 3;
    W(at::flashCount) = 3;
    W(at::flashPhase) = 0xff;
    B(at::flashing) = 0xff;
    plain(3);
    drawText(A(at::matchWinners), 0xa8);
    B(at::musicPriority) = 0;
    music(A(at::tuneMatchWin));
    B(at::musicRepeats) = 1;
    B(at::musicPriority) = 0;
    B(at::musicNext) = kb(at::musicAfterMatchWonInsn, 4);
  });
  bind(at::checkMatch, [this] {  // the next player who matched plays a ball more
    for (;;) {
      const u16 mark = static_cast<u16>(B(at::player) * 2 + A(at::matchPlayers - 2));
      const u8 digit = static_cast<u8>(nativeB(mark) - 0x37);
      nativeW(mark) = 0x5858;
      if (W(at::matchNumber) == digit) {
        B(at::playerDigit) = static_cast<u8>(B(at::player) + 0x37);
        matchWon();
        return goTo(A(at::scriptShootAgain));
      }
      if (++B(at::player) > B(at::players)) return goTo(A(at::scriptPostMatch));
    }
  });
  bind(at::nextBallIfMatched, [=, this] {
    if (B(at::matchWon) == 0) return chain();
    savePlayer(currentPlayer());
    if (extraBallOwed()) {
      return sameBallAgain();
    }
    if (B(at::player) == B(at::players)) return goTo(A(at::scriptPostMatch));
    ++B(at::player);
    B(at::playerDigit) = static_cast<u8>(B(at::player) + 0x37);
    goTo(A(at::scriptCheckMatch));
  });

  // --- after a ball: the same player again, the next player, the next ball, or the end
  bind(at::nextBall, [=, this] {
    holdBonus();
    savePlayer(currentPlayer());
    if (extraBallOwed()) {
      return sameBallAgain();
    }
    if (B(at::player) != B(at::players)) {
      ++B(at::player);
    } else {
      if (++B(at::ball) > B(at::balls)) {  // that was the last ball
        savePlayer(currentPlayer());
        return goTo(A(at::scriptMatch));
      }
      B(at::player) = 1;
      B(at::ballDigit) = static_cast<u8>(B(at::ball) + 0x37);
      ballShown();
    }
    B(at::playerDigit) = static_cast<u8>(B(at::player) + 0x37);
    playerShown();
    if (restoresAtTurn()) restorePlayer();
    const u16 step = bx;
    addTimer(F(at::issueBallTimer));
    bx = step;
    chain();
  });
  bind(at::issueBall, [this] {
    const u16 step = bx;
    addTimer(F(at::issueBallTimer));
    bx = step;
    chain();
  });
  bind(at::gameOver, [this] {  // (BX is not kept here in the original: see addTimer)
    addTimer(F(at::gameOverTimer));
    chain();
  });
  bind(at::gameOverTimer, [this] {  // the game is over: the table goes back to waiting
    B(at::countPlayersAfresh) = 0xff;
    toAttract();
    W(at::lookAt) = high() ? 0x103 : 0x171;
    W(at::attractScroll) = 0xffff;
    if (clearsScoreAtEnd())
      for (u16 i = 0; i < 12; ++i) B(at::score, i) = 0;
    lightsOut();
    B(at::playersMayJoin) = 0xff;
    gameOverLights();
    endTimer();
  });
  bind(at::issueBallTimer, [this] {
    // (two of the tables wait a second here and two half a second; amended: all half a second)
    if (countTo(A(at::issueBallCounter), amended ? 0x1e : kw(at::issueBallTimer, 1))) serve();
  });
}

}  // namespace encore
