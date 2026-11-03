// Lights. A light is a few colours of the table's picture: lit, they are as the table gives
// them; out, half as bright. Changes are queued and sent to the video card once a frame.
#include "engine/table/Engine.h"

namespace encore {

/// Adds a record (first colour, count of bytes, the bytes) to the queue, as it is or halved.
void Engine::queueColours(u16 record, bool half) {
  u16 to = W(at::colourQueueEnd);
  nativeB(to++) = nativeB(record++);
  const u8 count = nativeB(record++);
  nativeB(to++) = count;
  for (u8 i = 0; i < count; ++i) {
    const u8 v = nativeB(record++);
    nativeB(to++) = half ? v >> 1 : v;
  }
  W(at::colourQueueEnd) = to;
}

void Engine::lightOn(u8 light) { queueColours(W(at::lightRecords, static_cast<u16>((light - 1) * 2)), false); }
void Engine::lightOff(u8 light) { queueColours(W(at::lightRecords, static_cast<u16>((light - 1) * 2)), true); }

void Engine::setLight(u8 light) {
  B(at::lightStates, light) = kSet;
  lightOn(light);
}

void Engine::clearLight(u8 light) {
  B(at::lightStates, light) = 0;
  lightOff(light);
}

void Engine::blink(u8 light, u8 start, u8 halfPeriod) {
  for (u16 slot = A(at::blinks); slot != A(at::lightStates); slot = static_cast<u16>(slot + blinkRec::size)) {
    if (nativeB(slot) != 0) continue;
    nativeB(slot) = light;
    nativeB(static_cast<u16>(slot + blinkRec::count)) = start;
    nativeB(static_cast<u16>(slot + blinkRec::halfPeriod)) = halfPeriod;
    nativeB(static_cast<u16>(slot + blinkRec::period)) = static_cast<u8>(halfPeriod << 1);
    return;
  }
}

void Engine::stopBlink(u8 light) {
  for (u16 slot = A(at::blinks); slot != A(at::lightStates); slot = static_cast<u16>(slot + blinkRec::size)) {
    if (nativeB(slot) != light) continue;
    nativeB(slot) = 0;
    return;
  }
}

void Engine::stopBlinks() {
  for (u16 i = 0, bytes = kw(at::stopBlinks, 1); i < bytes; ++i) B(at::blinks, i) = 0;
}

void Engine::runBlinks() {
  for (u16 slot = A(at::blinks); slot != A(at::lightStates); slot = static_cast<u16>(slot + blinkRec::size)) {
    const u8 light = nativeB(slot);
    if (light == 0) continue;
    u8& count = nativeB(static_cast<u16>(slot + blinkRec::count));
    if (count == 0) {
      lightOn(light);
    } else if (count == nativeB(static_cast<u16>(slot + blinkRec::halfPeriod))) {
      lightOff(light);
    } else if (count == nativeB(static_cast<u16>(slot + blinkRec::period))) {
      count = 0;
      lightOn(light);
    }
    ++count;
  }
}

void Engine::flushColours() {
  if (B(at::coloursBusy) == kSet) return;
  B(at::coloursBusy) = kSet;
  if (CB(at::fading) != kSet) {
    for (u16 at = A(at::colourQueue); at != W(at::colourQueueEnd);) {
      B(at::coloursInterrupted) = 0;
      std::size_t colour = std::size_t{nativeB(at++)} * 3;
      const u8 count = nativeB(at++);
      for (u8 i = 0; i < count; ++i, ++colour) dac_[colour % 768] = nativeB(at++) & 0x3f;
    }
  }
  W(at::colourQueueEnd) = A(at::colourQueue);
  B(at::coloursBusy) = 0;
}

/// cs:622d: the lights' show while no game is played. Each entry counts up; at one count its
/// light goes out, at another it comes on and the count starts again.
void Engine::attractLights() {
  for (u16 e = A(at::attractLightList); nativeW(e) != kNone; e = static_cast<u16>(e + attractRec::size)) {
    const u16 count = static_cast<u16>(nativeW(e) + 1);
    nativeW(e) = count;
    if (count == nativeW(static_cast<u16>(e + attractRec::offAt))) {
      lightOff(nativeB(static_cast<u16>(e + attractRec::light)));
    } else if (count == nativeW(static_cast<u16>(e + attractRec::onAt))) {
      lightOn(nativeB(static_cast<u16>(e + attractRec::light)));
      nativeW(e) = nativeW(static_cast<u16>(e + attractRec::restart));
    }
  }
}

void Engine::displayFlash() {
  if (B(at::flashing) != kSet) return;
  if (--W(at::flashCount) != 0) return;
  W(at::flashCount) = W(at::flashPeriod);
  W(at::flashPhase) = W(at::flashPhase) ^ kSet;
  queueColours(W(at::flashPhase) == 0 ? A(at::displayColoursOff) : A(at::displayColoursOn), false);
}

void Engine::displayNormal() { queueColours(A(at::displayColoursOn), false); }
void Engine::displayInverse() { queueColours(A(at::displayColoursOff), false); }

void Engine::displaySteady() {
  B(at::flashing) = 0;
  queueColours(A(at::displayColoursOn), false);
}

}  // namespace encore
