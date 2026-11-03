// The program's start-up: the options taken in, tables put in the form the engine uses them
// in, the collision masks' copies laid out in video memory, the flippers made ready.
#include "engine/table/Engine.h"

namespace encore {
namespace {

/// The original's "times five, divided by six", for the slower of the two screen modes.
u16 fiveSixths(u16 v) { return static_cast<u16>((5 * i32{static_cast<i16>(v)}) / 6); }

/// The sines' table (at::sines): words, from a quarter turn before nought to a full turn.
constexpr u16 kSineWords = 0x0a00;
/// The four strips of the table's picture are IFF files: the colours follow "CMAP" and the
/// chunk's length. A strip is not looked into further than this.
constexpr u16 kChunkHead = 8;
constexpr int kStripSearched = 0x8000;
/// The lights' colours, from the file's range (0-255) to the card's: so many 256ths.
constexpr u16 kLightScale = 0xa2;
/// The two words that start-up clears and nothing else reads or writes.
constexpr u16 kUnusedA = 0x3706, kUnusedB = 0x370e;
/// A flipper's note of the picture last put into the mask, as none has been.
constexpr u8 kNoPicture = 0xff;

}  // namespace

void Engine::start(const Options& o, ByteView bestScores) {
  B(at::optionFiveBalls) = o.fiveBalls;
  B(at::optionLowAngle) = o.lowAngle;
  B(at::optionScrolling) = o.scrolling;
  B(at::optionMusicOff) = o.musicOff;
  B(at::optionHighResolution) = o.highResolution;
  B(at::optionMono) = o.mono;
  B(at::highResolution) = o.highResolution ? kSet : 0;
  angle_ = o.lowAngle ? tableAngle::low : tableAngle::high;
  if (o.lowAngle) {  // cs:30e9: the table lies flatter: less pull down it on every slope
    const u16 slopes = high() ? kw(at::slopesHighInsn, 1) : kw(at::slopesLowInsn, 1);
    for (u16 i = 0, n = kw(at::slopeCountInsn, 1); i < n; ++i) nativeW(static_cast<u16>(slopes + i * slopeRec::size + slopeRec::pullY)) -= slopeRec::lowAngle;
  }
  B(at::balls) = o.fiveBalls ? ballsPerGame::more : ballsPerGame::usual;
  if (o.musicOff) musicKey();
  W(at::scrollSpeed) = o.scrolling < scrollOption::medium ? screenScroll::hard : o.scrolling == scrollOption::medium ? screenScroll::medium : screenScroll::soft;
  if (o.scrolling < scrollOption::medium) B(at::scrollKey) = 0;

  // cs:69bb: the sines are kept high byte first
  for (u16 i = 0; i < kSineWords; ++i) {
    const u8 a = B(at::sines, static_cast<u16>(i * 2));
    B(at::sines, static_cast<u16>(i * 2)) = B(at::sines, static_cast<u16>(i * 2 + 1));
    B(at::sines, static_cast<u16>(i * 2 + 1)) = a;
  }
  W(at::longMsgText) = A(at::demoMessage);  // cs:6e7c
  W(at::longMsgPlace) = display::longMsgPlace;
  CW(at::longMsgAtA) = display::top;
  CW(at::longMsgAtB) = display::top;
  if (!high()) {  // cs:3106: everything about speed is for 70 frames a second
    for (u16 i = 0; i < material::count; ++i) W(at::materials, static_cast<u16>(i * material::size + material::minBounceSpeed)) = fiveSixths(W(at::materials, static_cast<u16>(i * material::size + material::minBounceSpeed)));
    for (u16 a = at::speedMin; a <= at::pushSpeedRelease; a = static_cast<u16>(a + 2)) W(a) = fiveSixths(W(a));
  }
  W(at::task) = F(at::defaultTask);  // cs:63c8
  W(at::taskState) = 0;
  startScript(A(at::scriptInit));
  for (u16 i = 0; i < kScoreDigits; ++i) B(at::scoreDrawn, i) = display::scoreNotDrawn;
  W(at::scoreDrawnGroups) = 0;

  // cs:69cb: the two maps of where the ball is hidden, moved to rows of 42 bytes
  struct Move { u16 segment, from, to; };
  for (const Move m : {Move{at::coverGround, 0x580, 0x100}, Move{at::coverRamps, 0x480, 0x000}}) {
    const u16 segment = S(m.segment);
    u16 from = m.from, to = m.to;
    for (u16 row = 0; row < playfield::rows; ++row, to = static_cast<u16>(to + 2))
      for (u16 b = 0; b < playfield::maskRow; ++b) farB(segment, to++) = farB(segment, from++);
  }

  // cs:6515, as far as the logic goes: the picture's colours, a quarter as large as the file
  // has them, which is what the video card takes
  for (u16 strip : {at::tablePicture1, at::tablePicture2, at::tablePicture3, at::tablePicture4}) {
    const u16 picture = S(strip);
    u16 at = 0;
    while (at < kStripSearched && !(farB(picture, at) == 'C' && farB(picture, static_cast<u16>(at + 1)) == 'M' &&
                            farB(picture, static_cast<u16>(at + 2)) == 'A' && farB(picture, static_cast<u16>(at + 3)) == 'P'))
      ++at;
    at = static_cast<u16>(at + kChunkHead);
    for (u16 i = 0; i < video::paletteBytes; ++i) {
      farB(picture, static_cast<u16>(at + i)) >>= 2;
      CB(at::palette, i) = farB(picture, static_cast<u16>(at + i));  // the last strip's are the ones kept
    }
  }
  scroll();
  W(at::displayLine) = high() ? video::splitLineHigh : video::splitLineLow;  // cs:66cd

  if (bestScores.size() >= bestScoreRec::allBytes)  // cs:635d
    for (u16 i = 0; i < bestScoreRec::allBytes; ++i) nativeB(static_cast<u16>(kw(at::bestScoresInsn, 1) + i)) = bestScores[i];
  W(at::colourQueueEnd) = A(at::colourQueue);

  // cs:5841: the lights' colours: counts of colours become counts of bytes, and the colours
  // go from the file's range to the video card's
  for (u16 at = A(at::lightColours); nativeB(at) != 0;) {
    ++at;
    const u16 bytes = static_cast<u16>(nativeB(at) * video::colourBytes);
    nativeB(at++) = static_cast<u8>(bytes);
    for (u16 i = 0; i < bytes; ++i, ++at) nativeB(at) = static_cast<u8>((nativeB(at) * kLightScale) >> 8);
  }
  // cs:627f: the attract show's times, kept as gaps, become times
  for (u16 e = A(at::attractLightList); nativeW(e) != kNone; e = static_cast<u16>(e + attractRec::size)) {
    nativeW(static_cast<u16>(e + attractRec::offAt)) += nativeW(static_cast<u16>(e + attractRec::restart));
    nativeW(static_cast<u16>(e + attractRec::onAt)) += nativeW(static_cast<u16>(e + attractRec::offAt));
  }

  // cs:3ab6: three of the masks are copied into video memory past the picture, a byte to
  // each plane in turn, where the original reads the ramps' slopes and materials from
  auto copyMask = [this](u16 segment, u16 from, u16 to, u16 count) {
    const u16 s = S(segment);
    for (std::size_t plane = 0; plane < 4; ++plane)
      for (u16 i = 0; i < count; ++i) video_[plane][static_cast<u16>(to + i)] = farB(s, static_cast<u16>(from + i * 4 + plane));
  };
  copyMask(at::marksRamps, 0, W(at::marksRampsCopy), video::rampsMaskBytes);
  W(at::slopesRampsCopy) = static_cast<u16>(W(at::marksRampsCopy) + video::rampsMaskBytes);
  {  // cs:3b8a
    for (u16 i = 0; i < flipperRec::quanta; ++i) W(at::flipperQuanta, static_cast<u16>(i * 2)) = static_cast<u16>(i / flipperRec::quantum);
    for (auto& plane : video_) std::fill(plane.begin(), plane.begin() + video::picture, u8{0});
    const u16 pieces = S(at::flipperPieces);  // cs:3cd4: kept there for the flippers' pictures to be drawn from
    const u16 each = static_cast<u16>(kw(at::flipperPiecesSizeInsn, 1) >> 2), kept = kw(at::flipperPiecesKeptInsn, 1);
    for (std::size_t plane = 0; plane < 4; ++plane)
      for (u16 i = 0; i < each; ++i) video_[plane][static_cast<u16>(kept + i)] = farB(pieces, static_cast<u16>(plane * each + i));
    const u16 first = A(at::flippers);
    nativeW(static_cast<u16>(first + flipperRec::masks)) = S(at::leftFlipperMasks);
    nativeW(static_cast<u16>(first + flipperRec::size + flipperRec::masks)) = S(at::rightFlipperMasks);
    nativeW(static_cast<u16>(first + flipperRec::third * flipperRec::size + flipperRec::masks)) = S(at::thirdFlipperMasks);
    if (!high())
      for (u16 f = 0; f < flipperRec::count; ++f)
        for (u16 field = flipperRec::accelHeld; field <= flipperRec::speedHeldMax + 2; field = static_cast<u16>(field + 2)) {
          const u16 at = static_cast<u16>(first + f * flipperRec::size + field);
          nativeW(at) = fiveSixths(nativeW(at));
        }
    for (u16 field = flipperRec::size + flipperRec::backLists; field <= flipperRec::size + flipperRec::lists; field = static_cast<u16>(field + 2)) {
      nativeW(static_cast<u16>(first + field)) += 8;
      nativeW(static_cast<u16>(first + flipperRec::size + field)) += 0x10;
    }
    for (u16 f = first; nativeB(f) != 0; f = static_cast<u16>(f + flipperRec::size)) {
      auto w = [&](u16 field) { return nativeW(static_cast<u16>(f + field)); };
      nativeB(static_cast<u16>(f + flipperRec::pictureStamped)) = kNoPicture;
      w(flipperRec::maskStep) = static_cast<u16>(w(flipperRec::maskStep) / 3);
      // cs:3d81: each of its pictures takes in the wall round it, so that putting one into
      // the mask does not rub the wall out
      const u16 pictures = w(flipperRec::masks), walls = S(at::wallsGround);
      const u16 width = static_cast<u16>(w(flipperRec::width) * 2), rows = w(flipperRec::rows);
      const u16 place = static_cast<u16>(w(flipperRec::y) * playfield::maskRow + (w(flipperRec::x) >> 3));
      u16 to = 0;
      for (u16 picture = 0; picture <= w(flipperRec::lastPicture); ++picture)
        for (u16 r = 0; r < rows; ++r)
          for (u16 b = 0; b < width; ++b) farB(pictures, to++) |= farB(walls, static_cast<u16>(place + r * playfield::maskRow + b));
    }
    const u16 x = W(at::ballX), y = W(at::ballY);  // cs:3d36 for each, wherever the ball is
    for (u16 f = first; nativeB(f) != 0; f = static_cast<u16>(f + flipperRec::size)) {
      W(at::ballX) = nativeW(static_cast<u16>(f + flipperRec::ballXMin));
      W(at::ballY) = nativeW(static_cast<u16>(f + flipperRec::ballYMin));
      stampFlippers();
    }
    W(at::ballX) = x;
    W(at::ballY) = y;
  }
  copyMask(at::slopesRamps, 0, W(at::slopesRampsCopy), video::rampsMaskBytes);
  W(at::slopesGroundCopy) = static_cast<u16>(W(at::slopesRampsCopy) + video::rampsMaskBytes);
  copyMask(at::slopesGround, video::groundTopBytes, W(at::slopesGroundCopy), video::groundRestBytes);
  {  // and the playfield's top rows in the four spare bytes at the end of each row of picture
    const u16 s = S(at::slopesGround);
    for (std::size_t plane = 0; plane < 4; ++plane)
      for (u16 k = 0; k < playfield::rows - 1; ++k)
        for (u16 j = 0; j < video::rowSpare; ++j)
          video_[plane][static_cast<u16>(video::firstRowSpare + k * video::rowBytes + j)] = farB(s, static_cast<u16>((k * video::rowSpare + j) * 4 + plane));
  }
  {  // cs:65b1: the plunger's picture at rest, and a clean first row past the picture
    const u16 s = S(at::plungerPicture);
    u16 from = 0, to = kw(at::plungerAtInsn, 1);
    for (u16 row = 0, rows = kw(at::plungerRowsInsn, 1), width = kw(at::plungerWidthInsn, 1); row < rows; ++row, ++to)
      for (u16 px = 0; px < width; ++px) {
        video_[px & 3][to] = farB(s, from++);
        if ((px & 3) == 3) ++to;
      }
    for (auto& plane : video_) std::fill(plane.begin() + video::pictureEnd, plane.begin() + video::pictureEnd + video::plungerRowBytes, u8{0});
    B(at::plungerShown) = 0;
  }
  if (high()) B(at::keys) |= keyBits::oneStep;
  B(at::mousePresent) = 0;
  B(at::drawOrder) = 0;
  B(at::ballNeverDrawn) = kSet;
  B(at::ballMidFrame) = 0;
  B(at::callbacksOn) = 0;

  // cs:3759
  B(at::musicPriority) = 0;
  B(at::musicNext) = 0;
  B(at::musicAfter) = 0;
  W(at::drawRoutine) = F(at::nothing);
  // (3706 and 370e: nothing else reads or writes them)
  for (u16 a : {u16{kUnusedA}, at::msgScrollText, at::msgScrollRow, u16{kUnusedB}, at::glyphRowsCut, at::glyphFirstRow, at::fontNext, at::idleShowing}) W(a) = 0;
  B(at::flippersLive) = 0;
  B(at::startKey) = 0;
  B(at::lastStartKey) = 0;
  W(at::idleFrames) = frames::idleAtStart;
  B(at::numberCentred) = 0;
  stopBlinks();
  for (u16 i = 0, slots = kw(at::timerSlotsInsn, 1); i < slots; ++i) W(at::timers, static_cast<u16>(i * 2)) = F(at::nothing);
  for (u16 i = 0; i < kTimerCounters; ++i) W(at::counters, static_cast<u16>(i * 2)) = 0;
  if (high()) W(at::modeSecondFrames) = frames::modeSecondHigh;

  call(F(at::tableStart));
  toAttract();
  W(at::lookAt) = high() ? screenScroll::lastHigh : screenScroll::lastLow;
  W(at::attractScroll) = screenScroll::up;
  call(F(at::tableAttractMusic));
  scroll();
  // cs:58f5: in the picture's own colours the lights are out
  for (u16 light = 1, lights = kw(at::lightCountInsn, 1); light <= lights; ++light) {
    B(at::lightStates, light) = 0;
    u16 record = W(at::lightRecords, static_cast<u16>((light - 1) * 2));
    u16 colour = static_cast<u16>(nativeB(record++) * video::colourBytes);
    const u8 count = nativeB(record++);
    for (u8 i = 0; i < count; ++i) CB(at::palette, colour++) = nativeB(record++) >> 1;
  }
  if (o.mono) {
    // cs:6a2f, cs:69fd: every colour becomes the grey of its three parts
    for (u16 at = A(at::lightColours), n = 0, records = kw(at::lightColourRecordsInsn, 1); n < records; ++n) {
      ++at;
      const u16 colours = static_cast<u16>(nativeB(at++) / video::colourBytes);
      for (u16 i = 0; i < colours; ++i, at = static_cast<u16>(at + video::colourBytes)) {
        const u8 grey = static_cast<u8>((nativeB(at) + nativeB(static_cast<u16>(at + 1)) + nativeB(static_cast<u16>(at + 2))) / 3);
        nativeB(at) = nativeB(static_cast<u16>(at + 1)) = nativeB(static_cast<u16>(at + 2)) = grey;
      }
    }
    for (u16 i = 0; i < video::paletteBytes; i = static_cast<u16>(i + video::colourBytes)) {
      const u8 grey = static_cast<u8>((CB(at::palette, i) + CB(at::palette, static_cast<u16>(i + 1)) + CB(at::palette, static_cast<u16>(i + 2))) / 3);
      CB(at::palette, i) = CB(at::palette, static_cast<u16>(i + 1)) = CB(at::palette, static_cast<u16>(i + 2)) = grey;
    }
  }
  for (u16 i = 0; i < video::paletteBytes; ++i) dac_[i] = CB(at::palette, i) & video::colourMax;  // cs:4f3c, at full brightness
  sound->start();
  B(at::lightsReady) = kSet;
  B(at::callbacksOn) = kSet;
  speedLimits_ = {W(at::speedMin).s(), W(at::speedMax).s()};
}

void Engine::setAngle(int angle) {
  angle = angle < tableAngle::low ? int{tableAngle::low} : angle > tableAngle::steeper ? int{tableAngle::steeper} : angle;
  // the pull down the table on each of its slopes: three less when it lies low (cs:30e9), and
  // three more when steeper
  const u16 slopes = high() ? kw(at::slopesHighInsn, 1) : kw(at::slopesLowInsn, 1);
  for (u16 i = 0, n = kw(at::slopeCountInsn, 1); i < n; ++i) nativeW(static_cast<u16>(slopes + i * slopeRec::size + slopeRec::pullY)) += static_cast<u16>((angle - angle_) * slopeRec::lowAngle);
  angle_ = angle;
  B(at::optionLowAngle) = angle == tableAngle::low;
  W(at::speedMin) = static_cast<u16>(angle == tableAngle::steeper ? speedLimits_[0] * 5 / 4 : speedLimits_[0]);
  W(at::speedMax) = static_cast<u16>(angle == tableAngle::steeper ? speedLimits_[1] * 5 / 4 : speedLimits_[1]);
}

void Engine::setScrolling(u8 scrolling) {
  B(at::optionScrolling) = scrolling;
  W(at::scrollSpeed) = scrolling < scrollOption::medium ? screenScroll::hard : scrolling == scrollOption::medium ? screenScroll::medium : screenScroll::soft;
}

void Engine::write(std::string_view text) {
  W(at::fillAt) = display::top;
  W(at::fillWidth) = display::width;
  W(at::fillRows) = display::rows;
  fillDisplay(display::top, display::width, display::rows);
  setFont(font::h13);
  W(at::glyphRowsCut) = 0;
  W(at::glyphFirstRow) = 0;
  if (text.size() > display::lineChars) text = text.substr(0, display::lineChars);
  u16 at = static_cast<u16>(display::line + (display::lineChars - text.size()) * 2);  // in the middle, as GAME PAUSED is
  for (const char c : text) {
    u16 place = at;
    drawChar(static_cast<u8>(c >= '0' && c <= '9' ? c - '0' + glyph::digit0 : c), place);
    at = static_cast<u16>(at + display::letter);
  }
}

}  // namespace encore
