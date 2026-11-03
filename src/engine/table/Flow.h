#pragma once
// What the four tables do alike between balls and at the end of a game: the ball served, the
// bonus counted, the next player or ball, the best scores, the match. In each program this is
// among the table's own code (Party Land: cs:0202 to cs:0d47), nearly the same in all four;
// where a table does it differently it overrides the piece. Written against Party Land's
// addresses like the rest of the engine.
#include "engine/table/Engine.h"

namespace encore {

class Flow : public Engine {
 public:
  Flow(ByteView prg, int table);

 protected:
  // --- what each table has of its own
  /// Keeps what the player whose record is at `player` (an offset into the players' table)
  /// carries from ball to ball, and brings the current player's back.
  virtual void savePlayer(u16 player) = 0;
  virtual void restorePlayer() = 0;
  /// Everything about the ball in play as a ball begins (Party Land: cs:00aa).
  virtual void clearBall() = 0;
  /// Whether the player has another ball to come without it counting (Party Land: the light).
  virtual bool extraBallOwed() { return B(at::extraBallLight) == kSet; }
  /// The match's pace: how many frames each digit stands (Party Land: cs:0883, cs:08ab).
  virtual void matchPace() { CW(at::matchFrames) = high() ? frames::matchDigitHigh : frames::matchDigitLow; }
  /// How many digits the match runs through before it stops.
  /// (Party Land's is one more at the slower screen mode than at the faster, which makes the
  /// two last as long; amended: the faster one's count.)
  virtual u16 matchLength() { return high() || amended ? kw(at::matchLengthHighInsn, 1) : kw(at::matchLengthLowInsn, 1); }
  /// Whether the score shown goes back to nought when a game is over (Party Land: cs:0b99).
  virtual bool clearsScoreAtEnd() { return true; }
  /// The lights left on when a game is over (Party Land: cs:0ba5).
  virtual void gameOverLights() {
    setLight(kb(at::gameOverLight1Insn, 1));
    setLight(kb(at::gameOverLight2Insn, 1));
    setLight(kb(at::gameOverLight3Insn, 1));
  }
  /// The match's digit came out the same as the last one: what is shown instead (Party Land,
  /// cs:0902: the next one up).
  virtual u16 matchRepeated(u16 digit) { return digit == kDecimal - 1 ? 0 : static_cast<u16>(digit + 1); }
  /// A player matched (Party Land: cs:0a58): one more ball, and its tune is not played again.
  virtual void matchWon() {
    B(at::matchWon) = kSet;
    B(at::partyOn) = kSet;
  }
  /// Party Land brings the next player's things back as soon as the turn passes (cs:0b13),
  /// as well as when the ball is served; not all do.
  virtual bool restoresAtTurn() { return true; }
  /// What is put right once the player's things are back, as a ball is served (Party Land,
  /// cs:0be7: the light of a ball owed).
  virtual void afterRestore() {
    if (B(at::extraBalls) != 0) setLight(kb(at::extraBallLightInsn, 1));
  }
  /// The tune a ball opens with (Party Land, cs:0c3d: not after a match was won).
  virtual void serveMusic() {
    if (B(at::partyOn) == kSet) return;
    music(A(at::tunePlunger));
    B(at::partyOn) = 0;
  }
  virtual void ballShown() {}    ///< the ball's number was written for the display
  virtual void playerShown() {}  ///< and the player's
  virtual bool redrawNameWhileWaiting() { return true; }
  /// The same player plays again, a ball owed being used up (Party Land: cs:0b36).
  virtual void sameBallAgain() {
    --B(at::extraBalls);
    goTo(A(at::scriptShootAgain));
  }

  void serve();           // cs:0bca
  void startBall();       // cs:0c79
  u16 playerSize() const { return kw(at::playerSizeInsn, 1); }
  u16 currentPlayer() { return static_cast<u16>((B(at::player) - 1) * playerSize()); }
  /// The step after this one is run at once (cs:0b1d).
  void chain();
  void goTo(u16 nativeScript);  ///< go on from another script, at once

 private:
  void bindFlow();
};

}  // namespace encore
