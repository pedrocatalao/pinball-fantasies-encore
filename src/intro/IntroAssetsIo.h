#pragma once
// The intro in the open format: intro.json and a picture for every slide, panel, banner and
// font, reading back into exactly the IntroAssets it was written from.
#include <filesystem>

#include "intro/IntroAssets.h"

namespace pfr {

void saveIntroAssets(const IntroAssets& a, const std::filesystem::path& dir);
IntroAssets loadIntroAssets(const std::filesystem::path& dir);

}  // namespace pfr
