#include "engine/data/MzImage.h"

#include <algorithm>
#include <set>

#include "core/Error.h"
#include "core/File.h"

namespace encore {
namespace {

/// The MZ header, by where each field is in it (words).
namespace mzRec {
constexpr std::size_t size = 0x1c;  ///< the fields read here, to the relocations' place
constexpr std::size_t lastPageBytes = 2, pages = 4, relocations = 6, headerParagraphs = 8;
constexpr std::size_t ip = 0x14, cs = 0x16;  ///< where the program begins
constexpr std::size_t relocationsAt = 0x18;
}  // namespace mzRec

/// A relocation: the offset of the word to be relocated, then its segment.
constexpr std::size_t kRelocationSegment = 2;

}  // namespace

MzImage MzImage::load(const std::filesystem::path& path) {
  auto bytes = file::readAll(path);
  if (!bytes) throw DataError("cannot read " + path.string());
  return parse(std::move(*bytes), path.filename().string());
}

MzImage MzImage::parse(Bytes fileBytes, std::string name) {
  MzImage mz;
  mz.name_ = std::move(name);
  mz.file_ = std::move(fileBytes);
  const ByteView f = mz.file_;
  if (f.size() < mzRec::size || f[0] != 'M' || f[1] != 'Z') throw DataError(mz.name_ + ": not an MZ executable");
  const u16 lastPageBytes = rd16le(f, mzRec::lastPageBytes);
  const u16 pages = rd16le(f, mzRec::pages);
  const u16 relocCount = rd16le(f, mzRec::relocations);
  const u16 headerParagraphs = rd16le(f, mzRec::headerParagraphs);
  mz.entryIp_ = rd16le(f, mzRec::ip);
  mz.entryCs_ = rd16le(f, mzRec::cs);
  const u16 relocOffset = rd16le(f, mzRec::relocationsAt);
  const std::size_t headerBytes = headerParagraphs * 16u;
  std::size_t imageEnd = pages * 512u;
  if (lastPageBytes) imageEnd -= 512u - lastPageBytes;
  imageEnd = std::min(imageEnd, f.size());
  if (headerBytes > imageEnd) throw DataError(mz.name_ + ": corrupt MZ header");
  mz.image_ = f.subspan(headerBytes, imageEnd - headerBytes);

  std::set<u16> values;
  values.insert(mz.entryCs_);
  for (u16 i = 0; i < relocCount; ++i) {
    const std::size_t r = relocOffset + i * 4u;
    if (r + 4u > headerBytes) break;
    const u16 off = rd16le(f, r);
    const u16 seg = rd16le(f, r + kRelocationSegment);
    const std::size_t at = seg * 16u + off;
    if (at + 2 > mz.image_.size()) continue;
    values.insert(rd16le(mz.image_, at));
  }
  for (u16 v : values) {
    const std::size_t off = v * 16u;
    if (off >= mz.image_.size()) continue;
    mz.segments_.push_back({v, off, 0});
  }
  for (std::size_t i = 0; i < mz.segments_.size(); ++i) {
    const std::size_t next = i + 1 < mz.segments_.size() ? mz.segments_[i + 1].offset : mz.image_.size();
    mz.segments_[i].length = next - mz.segments_[i].offset;
  }
  return mz;
}

ByteView MzImage::segment(u16 value) const {
  for (const auto& s : segments_)
    if (s.value == value) return image_.subspan(s.offset, s.length);
  throw DataError(name_ + ": unknown segment");
}

ByteView MzImage::farBytes(u16 seg, u16 offset) const {
  const std::size_t at = seg * 16u + offset;
  if (at > image_.size()) throw DataError(name_ + ": far pointer outside image");
  return image_.subspan(at);
}

}  // namespace encore
