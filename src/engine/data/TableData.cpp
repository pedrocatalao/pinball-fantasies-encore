#include "engine/data/TableData.h"

#include <algorithm>
#include <cstring>

#include "core/Error.h"
#include "core/File.h"
#include "core/Log.h"

namespace encore {
namespace {

constexpr const char* kNames[4] = {"Party Land", "Speed Devils", "Billion Dollar Gameshow", "Stones 'n Bones"};
constexpr std::size_t kMaskBytes = static_cast<std::size_t>(TableData::kWidth / 8) * TableData::kHeight;  // 23040
constexpr std::size_t kPlungerWidth = 10;
constexpr std::size_t kPlungerHeight = 23;
constexpr std::size_t kOcclusionGround = 0x580;
constexpr std::size_t kOcclusionOverhead = 0x6400;
}  // namespace

TableData TableData::load(const std::filesystem::path& prg, int index) {
  const auto bytes = file::readAll(prg);
  if (!bytes) throw DataError("cannot read " + prg.string());
  return parse(*bytes, prg.filename().string(), index);
}

TableData TableData::parse(Bytes bytes, const std::string& file, int index) {
  const std::filesystem::path prg = file;
  TableData t;
  t.index = index;
  t.name = kNames[std::clamp(index, 0, 3)];
  t.image = MzImage::parse(std::move(bytes), file);
  const MzImage& mz = t.image;
  const auto& segs = mz.segments();
  if (segs.size() < 20u) throw DataError(prg.string() + ": unexpected segment layout");

  // Block roles are recognised by position for the first four blocks and by size for the
  // rest, because the order of the sprite bank differs between tables (TABLE3 stores it
  // after the flipper stacks).
  auto view = [&](std::size_t k) { return mz.image().subspan(segs[k].offset, segs[k].length); };
  auto isForm = [&](std::size_t k) {
    return segs[k].length > 12u && std::memcmp(mz.image().data() + segs[k].offset, "FORM", 4u) == 0;
  };
  std::size_t i = 0;
  t.code = view(i++);
  t.sharedCode = view(i++);
  t.frameTable = view(i++);
  t.dataSegment = view(i++);
  auto takeMask = [&](std::size_t k) {
    if (segs[k].length < kMaskBytes) throw DataError(prg.string() + ": collision mask block too short");
    return Mask::fromPacked(view(k), kWidth, kHeight, kWidth / 8);
  };
  int collisionMasks = 0;
  for (; i < segs.size() && !isForm(i); ++i) {
    const std::size_t len = segs[i].length;
    if (len == kMaskBytes) {
      Mask m = takeMask(i);
      if (collisionMasks == 0) t.walls = std::move(m);
      else if (collisionMasks == 1) t.dynamic = std::move(m);
      else if (collisionMasks == 2) t.ramps = std::move(m);
      ++collisionMasks;
    } else if (len == 24448u) {
      t.ballGraphics = view(i);
      // The two occlusion maps live at fixed offsets from the start of this block and run
      // past its end into the next one, so they are read straight from the load image.
      const ByteView spill = mz.image().subspan(segs[i].offset);
      if (spill.size() >= kOcclusionOverhead + kMaskBytes) {
        t.occlusionGround = Mask::fromPacked(spill.subspan(kOcclusionGround), kWidth, kHeight, kWidth / 8);
        t.occlusionOverhead = Mask::fromPacked(spill.subspan(kOcclusionOverhead), kWidth, kHeight, kWidth / 8);
      }
    } else if (len == 24192u) {
      t.rampMask = view(i);
    } else if (len <= 12000u) {
      t.flipperBlocks.push_back(view(i));
    } else {
      t.spriteBank = view(i);
    }
  }
  if (collisionMasks != 3) throw DataError(prg.string() + ": expected three collision masks");
  // Four strips.
  t.playfield.assign(static_cast<std::size_t>(kWidth) * kHeight, 0);
  for (int s = 0; s < 4; ++s) {
    if (i >= segs.size()) throw DataError(prg.string() + ": missing image strip");
    auto img = decodeIff(view(i++));
    if (!img || img->width != kWidth) throw DataError(prg.string() + ": bad image strip");
    const int rows = std::min(img->height, 144);
    std::memcpy(t.playfield.data() + static_cast<std::size_t>(s) * 144 * kWidth, img->pixels.data(),
                static_cast<std::size_t>(rows) * kWidth);
    t.strips[s] = std::move(*img);
  }
  // The palette: strip 0's CMAP (the game loads the first strip's palette; strips differ only
  // in entries the picture does not use).
  t.palette = t.strips[0].palette;
  t.colorRanges = t.strips[0].ranges;
  // Occlusion masks.
  t.occlusionA = takeMask(i++);
  t.occlusionB = takeMask(i++);
  t.occlusionC = takeMask(i++);
  // The trailing blocks: the plunger is exactly 10 x 23 chunky pixels, the other is the
  // four-pixel dictionary the table's animations index into.
  for (; i < segs.size(); ++i) {
    if (segs[i].length == kPlungerWidth * kPlungerHeight) t.plungerImage = view(i);
    else if (t.smallImageA.empty()) t.smallImageA = view(i);
  }
  return t;
}

}  // namespace encore
