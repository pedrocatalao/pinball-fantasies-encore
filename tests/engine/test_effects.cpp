// The table's effects as they reach the sound card, against Speed Devils' own module (skipped
// when the game files are not there).
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>

#include "GameDir.h"
#include "Test.h"
#include "core/File.h"
#include "data/GameVersion.h"
#include "engine/audio/MusicDriver.h"

using namespace encore;

namespace {

constexpr u8 kSlingshot = 29, kFlipper = 25, kBumper = 24;  // SIDOBUMPER, FLIPPERUPP and BUMPER in TABLE2.MOD

bool haveData() {
  static const bool ok = test::haveTables();
  if (!ok) std::printf("  (skipped: no supported table files at %s)\n", test::gameDir().string().c_str());
  return ok;
}

Bytes module() {
  const auto path = file::findCaseInsensitive(test::gameDir(), "TABLE2.MOD");
  return path ? file::readAll(*path).value_or(Bytes{}) : Bytes{};
}

double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

std::vector<float> render(MusicDriver& d, int frames) {
  std::vector<float> out(static_cast<std::size_t>(frames) * 2);
  d.render(out.data(), frames);
  return out;
}

/// How far apart two renderings' loudness is, 5 ms at a time, against how loud the second is:
/// 0 the same. (Sample by sample says little: a sound started over keeps the fraction of a byte
/// its channel was at, as the driver does, and with the nearest byte taken a hair's shift
/// changes most of the values of a noisy sound without changing how it sounds.)
double apart(const std::vector<float>& a, const std::vector<float>& b) {
  double d = 0, loud = 0;
  for (std::size_t from = 0; from < a.size(); from += 480) {
    double ea = 0, eb = 0;
    for (std::size_t i = from; i < std::min(a.size(), from + 480); ++i) ea += a[i] * a[i], eb += b[i] * b[i];
    d += std::fabs(std::sqrt(ea) - std::sqrt(eb));
    loud += std::sqrt(eb);
  }
  std::printf("  apart %.4f\n", loud > 0 ? d / loud : 0);
  return loud > 0 ? d / loud : 0;
}

/// The first frame where two renderings differ, or -1.
int firstDifference(const std::vector<float>& a, const std::vector<float>& b) {
  for (std::size_t i = 0; i < a.size(); ++i)
    if (a[i] != b[i]) return static_cast<int>(i / 2);
  return -1;
}

}  // namespace

// An effect is heard one helping of sound after the moment it was asked for, wherever in the
// helping that falls: asked for 4 ms before the card asks for 512 samples (10.7 ms), it starts
// 6.7 ms into them.
TEST(an_effect_is_heard_a_helping_after_its_moment) {
  if (!haveData()) return;
  const Bytes mod = module();
  MusicDriver with, without;
  CHECK(with.load(mod) && without.load(mod));
  with.start();
  without.start();
  render(with, 4800), render(without, 4800);
  with.stampTime(now() - 0.004);
  with.effect(kSlingshot, 18, 0, 4);
  const int at = firstDifference(render(with, 512), render(without, 512));
  const int expected = static_cast<int>((512.0 / 48000 - 0.004) * 48000);
  std::printf("  heard from %d of 512, expected about %d\n", at, expected);
  CHECK(at >= expected - 16 && at <= expected + 16);
}

// A flipper's sound no longer cuts off a slingshot's: the slingshot plays on to its end beside it.
TEST(a_slingshot_plays_on_under_a_flipper) {
  if (!haveData()) return;
  const Bytes mod = module();
  MusicDriver both, flipperOnly;
  CHECK(both.load(mod) && flipperOnly.load(mod));
  both.start();
  flipperOnly.start();
  render(both, 4800), render(flipperOnly, 4800);
  both.effect(kSlingshot, 18, 0, 4);  // never stamped: heard at once
  render(both, 960), render(flipperOnly, 960);
  both.effect(kFlipper, 18, 0, 4);
  flipperOnly.effect(kFlipper, 18, 0, 4);
  // with the slingshot cut off, the two would now sound the same
  const auto a = render(both, 2400), b = render(flipperOnly, 2400);
  CHECK(apart(a, b) > 0.3);
}

// The same effect again starts over, as in the original: a bonus counted tick by tick does not
// pile up. Twice the same sound sounds just as the second alone.
TEST(the_same_effect_again_starts_over) {
  if (!haveData()) return;
  const Bytes mod = module();
  MusicDriver twice, once;
  CHECK(twice.load(mod) && once.load(mod));
  twice.start();
  once.start();
  render(twice, 4800), render(once, 4800);
  twice.effect(kBumper, 18, 0, 4);
  render(twice, 960), render(once, 960);
  twice.effect(kBumper, 18, 0, 4);
  once.effect(kBumper, 18, 0, 4);
  CHECK(apart(render(twice, 2400), render(once, 2400)) < 0.05);
}

// Two effects at most at once: a third, different again, cuts off the oldest.
TEST(two_effects_at_most_sound_at_once) {
  if (!haveData()) return;
  const Bytes mod = module();
  MusicDriver three, lastTwo;
  CHECK(three.load(mod) && lastTwo.load(mod));
  three.start();
  lastTwo.start();
  render(three, 4800), render(lastTwo, 4800);
  three.effect(kSlingshot, 18, 0, 4);
  render(three, 480), render(lastTwo, 480);
  three.effect(kBumper, 18, 0, 4);
  lastTwo.effect(kBumper, 18, 0, 4);
  render(three, 480), render(lastTwo, 480);
  three.effect(kFlipper, 18, 0, 4);
  lastTwo.effect(kFlipper, 18, 0, 4);
  CHECK(apart(render(three, 2400), render(lastTwo, 2400)) < 0.05);
}
