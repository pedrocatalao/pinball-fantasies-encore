// The program's start-up: the options taken in, tables put in the form the engine uses them
// in, the collision masks' copies laid out in video memory, the flippers made ready.
#include "engine/table/Engine.h"

namespace encore {
namespace {

/// The original's "times five, divided by six", for the slower of the two screen modes.
u16 fiveSixths(u16 v) { return static_cast<u16>((5 * i32{static_cast<i16>(v)}) / 6); }

}  // namespace

void Engine::start(const Options& o, ByteView bestScores) {
  B(at::optionFiveBalls) = o.fiveBalls;
  B(at::optionLowAngle) = o.lowAngle;
  B(at::optionScrolling) = o.scrolling;
  B(at::optionMusicOff) = o.musicOff;
  B(at::optionHighResolution) = o.highResolution;
  B(at::optionMono) = o.mono;
  B(at::highResolution) = o.highResolution ? 0xff : 0;
  angle_ = o.lowAngle ? 0 : 1;
  if (o.lowAngle) {  // cs:30e9: the table lies flatter: less pull down it on every slope
    const u16 slopes = high() ? kw(at::slopesHighInsn, 1) : kw(at::slopesLowInsn, 1);
    for (u16 i = 0, n = kw(at::slopeCountInsn, 1); i < n; ++i) nativeW(static_cast<u16>(slopes + i * 4 + 2)) -= 3;
  }
  B(at::balls) = o.fiveBalls ? 5 : 3;
  if (o.musicOff) musicKey();
  W(at::scrollSpeed) = o.scrolling < 1 ? 0x14 : o.scrolling == 1 ? 0x0b : 0x09;
  if (o.scrolling < 1) B(at::scrollKey) = 0;

  // cs:69bb: the sines are kept high byte first
  for (u16 i = 0; i < 0x0a00; ++i) {
    const u8 a = B(at::sines, static_cast<u16>(i * 2));
    B(at::sines, static_cast<u16>(i * 2)) = B(at::sines, static_cast<u16>(i * 2 + 1));
    B(at::sines, static_cast<u16>(i * 2 + 1)) = a;
  }
  W(at::longMsgText) = A(at::demoMessage);  // cs:6e7c
  W(at::longMsgPlace) = 0x2a0;
  CW(at::longMsgAtA) = 0xa8;
  CW(at::longMsgAtB) = 0xa8;
  if (!high()) {  // cs:3106: everything about speed is for 70 frames a second
    for (u16 i = 0; i < 8; ++i) W(at::materials, static_cast<u16>(i * 16 + 6)) = fiveSixths(W(at::materials, static_cast<u16>(i * 16 + 6)));
    for (u16 a = at::speedMin; a <= at::pushSpeedRelease; a = static_cast<u16>(a + 2)) W(a) = fiveSixths(W(a));
  }
  W(at::task) = F(at::defaultTask);  // cs:63c8
  W(at::taskState) = 0;
  startScript(A(at::scriptInit));
  for (u16 i = 0; i < 12; ++i) B(at::scoreDrawn, i) = 0x12;
  W(at::scoreDrawnGroups) = 0;

  // cs:69cb: the two maps of where the ball is hidden, moved to rows of 42 bytes
  struct Move { u16 segment, from, to; };
  for (const Move m : {Move{at::coverGround, 0x580, 0x100}, Move{at::coverRamps, 0x480, 0x000}}) {
    const u16 segment = S(m.segment);
    u16 from = m.from, to = m.to;
    for (u16 row = 0; row < 0x240; ++row, to = static_cast<u16>(to + 2))
      for (u16 b = 0; b < 0x28; ++b) farB(segment, to++) = farB(segment, from++);
  }

  // cs:6515, as far as the logic goes: the picture's colours, a quarter as large as the file
  // has them, which is what the video card takes
  for (u16 strip : {at::tablePicture1, at::tablePicture2, at::tablePicture3, at::tablePicture4}) {
    const u16 picture = S(strip);
    u16 at = 0;
    while (at < 0x8000 && !(farB(picture, at) == 'C' && farB(picture, static_cast<u16>(at + 1)) == 'M' &&
                            farB(picture, static_cast<u16>(at + 2)) == 'A' && farB(picture, static_cast<u16>(at + 3)) == 'P'))
      ++at;
    at = static_cast<u16>(at + 8);
    for (u16 i = 0; i < 0x300; ++i) {
      farB(picture, static_cast<u16>(at + i)) >>= 2;
      CB(at::palette, i) = farB(picture, static_cast<u16>(at + i));  // the last strip's are the ones kept
    }
  }
  scroll();
  W(at::displayLine) = high() ? 0x13d : 0x19e;  // cs:66cd

  if (bestScores.size() >= 0x40)  // cs:635d
    for (u16 i = 0; i < 0x40; ++i) nativeB(static_cast<u16>(kw(at::bestScoresInsn, 1) + i)) = bestScores[i];
  W(at::colourQueueEnd) = A(at::colourQueue);

  // cs:5841: the lights' colours: counts of colours become counts of bytes, and the colours
  // go from the file's range to the video card's
  for (u16 at = A(at::lightColours); nativeB(at) != 0;) {
    ++at;
    const u16 bytes = static_cast<u16>(nativeB(at) * 3);
    nativeB(at++) = static_cast<u8>(bytes);
    for (u16 i = 0; i < bytes; ++i, ++at) nativeB(at) = static_cast<u8>((nativeB(at) * 0xa2) >> 8);
  }
  // cs:627f: the attract show's times, kept as gaps, become times
  for (u16 e = A(at::attractLightList); nativeW(e) != 0xffff; e = static_cast<u16>(e + attractRec::size)) {
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
    for (u16 i = 0; i < 0x528; ++i) W(at::flipperQuanta, static_cast<u16>(i * 2)) = static_cast<u16>(i / 0x37);
    for (auto& plane : video_) std::fill(plane.begin(), plane.begin() + video::picture, u8{0});
    const u16 pieces = S(at::flipperPieces);  // cs:3cd4: kept there for the flippers' pictures to be drawn from
    const u16 each = static_cast<u16>(kw(at::flipperPiecesSizeInsn, 1) >> 2), kept = kw(at::flipperPiecesKeptInsn, 1);
    for (std::size_t plane = 0; plane < 4; ++plane)
      for (u16 i = 0; i < each; ++i) video_[plane][static_cast<u16>(kept + i)] = farB(pieces, static_cast<u16>(plane * each + i));
    const u16 first = A(at::flippers);
    nativeW(static_cast<u16>(first + flipperRec::masks)) = S(at::leftFlipperMasks);
    nativeW(static_cast<u16>(first + flipperRec::size + flipperRec::masks)) = S(at::rightFlipperMasks);
    nativeW(static_cast<u16>(first + 2 * flipperRec::size + flipperRec::masks)) = S(at::thirdFlipperMasks);
    if (!high())
      for (u16 f = 0; f < 3; ++f)
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
      nativeB(static_cast<u16>(f + flipperRec::pictureStamped)) = 0xff;
      w(flipperRec::maskStep) = static_cast<u16>(w(flipperRec::maskStep) / 3);
      // cs:3d81: each of its pictures takes in the wall round it, so that putting one into
      // the mask does not rub the wall out
      const u16 pictures = w(flipperRec::masks), walls = S(at::wallsGround);
      const u16 width = static_cast<u16>(w(flipperRec::width) * 2), rows = w(flipperRec::rows);
      const u16 place = static_cast<u16>(w(flipperRec::y) * 0x28 + (w(flipperRec::x) >> 3));
      u16 to = 0;
      for (u16 picture = 0; picture <= w(flipperRec::lastPicture); ++picture)
        for (u16 r = 0; r < rows; ++r)
          for (u16 b = 0; b < width; ++b) farB(pictures, to++) |= farB(walls, static_cast<u16>(place + r * 0x28 + b));
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
      for (u16 k = 0; k < 0x23f; ++k)
        for (u16 j = 0; j < 4; ++j)
          video_[plane][static_cast<u16>(video::firstRowSpare + k * 0x54 + j)] = farB(s, static_cast<u16>((k * 4 + j) * 4 + plane));
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
  if (high()) B(at::keys) |= 4;
  B(at::mousePresent) = 0;
  B(at::drawOrder) = 0;
  B(at::ballNeverDrawn) = 0xff;
  B(at::ballMidFrame) = 0;
  B(at::callbacksOn) = 0;

  // cs:3759
  B(at::musicPriority) = 0;
  B(at::musicNext) = 0;
  B(at::musicAfter) = 0;
  W(at::drawRoutine) = F(at::nothing);
  // (3706 and 370e: nothing else reads or writes them)
  for (u16 a : {u16{0x3706}, at::msgScrollText, at::msgScrollRow, u16{0x370e}, at::glyphRowsCut, at::glyphFirstRow, at::fontNext, at::idleShowing}) W(a) = 0;
  B(at::flippersLive) = 0;
  B(at::startKey) = 0;
  B(at::lastStartKey) = 0;
  W(at::idleFrames) = 0x2ce;
  B(at::numberCentred) = 0;
  stopBlinks();
  for (u16 i = 0, slots = kw(at::timerSlotsInsn, 1); i < slots; ++i) W(at::timers, static_cast<u16>(i * 2)) = F(at::nothing);
  for (u16 i = 0; i < 0x32; ++i) W(at::counters, static_cast<u16>(i * 2)) = 0;
  if (high()) W(at::modeSecondFrames) = 0x47;

  call(F(at::tableStart));
  toAttract();
  W(at::lookAt) = high() ? 0x103 : 0x171;
  W(at::attractScroll) = 0xffff;
  call(F(at::tableAttractMusic));
  scroll();
  // cs:58f5: in the picture's own colours the lights are out
  for (u16 light = 1, lights = kw(at::lightCountInsn, 1); light <= lights; ++light) {
    B(at::lightStates, light) = 0;
    u16 record = W(at::lightRecords, static_cast<u16>((light - 1) * 2));
    u16 colour = static_cast<u16>(nativeB(record++) * 3);
    const u8 count = nativeB(record++);
    for (u8 i = 0; i < count; ++i) CB(at::palette, colour++) = nativeB(record++) >> 1;
  }
  if (o.mono) {
    // cs:6a2f, cs:69fd: every colour becomes the grey of its three parts
    for (u16 at = A(at::lightColours), n = 0, records = kw(at::lightColourRecordsInsn, 1); n < records; ++n) {
      ++at;
      const u16 colours = static_cast<u16>(nativeB(at++) / 3);
      for (u16 i = 0; i < colours; ++i, at = static_cast<u16>(at + 3)) {
        const u8 grey = static_cast<u8>((nativeB(at) + nativeB(static_cast<u16>(at + 1)) + nativeB(static_cast<u16>(at + 2))) / 3);
        nativeB(at) = nativeB(static_cast<u16>(at + 1)) = nativeB(static_cast<u16>(at + 2)) = grey;
      }
    }
    for (u16 i = 0; i < 0x300; i = static_cast<u16>(i + 3)) {
      const u8 grey = static_cast<u8>((CB(at::palette, i) + CB(at::palette, static_cast<u16>(i + 1)) + CB(at::palette, static_cast<u16>(i + 2))) / 3);
      CB(at::palette, i) = CB(at::palette, static_cast<u16>(i + 1)) = CB(at::palette, static_cast<u16>(i + 2)) = grey;
    }
  }
  for (u16 i = 0; i < 0x300; ++i) dac_[i] = CB(at::palette, i) & 0x3f;  // cs:4f3c, at full brightness
  sound->start();
  B(at::lightsReady) = 0xff;
  B(at::callbacksOn) = 0xff;
  speedLimits_ = {W(at::speedMin).s(), W(at::speedMax).s()};
}

void Engine::setAngle(int angle) {
  angle = angle < 0 ? 0 : angle > 2 ? 2 : angle;
  // the pull down the table on each of its slopes: three less when it lies low (cs:30e9), and
  // three more when steeper
  const u16 slopes = high() ? kw(at::slopesHighInsn, 1) : kw(at::slopesLowInsn, 1);
  for (u16 i = 0, n = kw(at::slopeCountInsn, 1); i < n; ++i) nativeW(static_cast<u16>(slopes + i * 4 + 2)) += static_cast<u16>((angle - angle_) * 3);
  angle_ = angle;
  B(at::optionLowAngle) = angle == 0;
  W(at::speedMin) = static_cast<u16>(angle == 2 ? speedLimits_[0] * 5 / 4 : speedLimits_[0]);
  W(at::speedMax) = static_cast<u16>(angle == 2 ? speedLimits_[1] * 5 / 4 : speedLimits_[1]);
}

void Engine::setScrolling(u8 scrolling) {
  B(at::optionScrolling) = scrolling;
  W(at::scrollSpeed) = scrolling < 1 ? 0x14 : scrolling == 1 ? 0x0b : 0x09;
}

void Engine::write(std::string_view text) {
  W(at::fillAt) = 0xa8;
  W(at::fillWidth) = 0x50;
  W(at::fillRows) = 0x10;
  fillDisplay(0xa8, 0x50, 0x10);
  setFont(0);
  W(at::glyphRowsCut) = 0;
  W(at::glyphFirstRow) = 0;
  if (text.size() > 20) text = text.substr(0, 20);
  u16 at = static_cast<u16>(0x150 + (20 - text.size()) * 2);  // in the middle, as GAME PAUSED is
  for (const char c : text) {
    u16 place = at;
    drawChar(static_cast<u8>(c >= '0' && c <= '9' ? c - '0' + 0x37 : c), place);
    at = static_cast<u16>(at + 4);
  }
}

}  // namespace encore
