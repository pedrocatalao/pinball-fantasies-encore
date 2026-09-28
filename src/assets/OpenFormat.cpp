#include "assets/OpenFormat.h"

#include "core/File.h"
#include "core/Png.h"

namespace pfr::open {
namespace {

const std::vector<Rgb>& greys() {
  static const std::vector<Rgb> g = [] {
    std::vector<Rgb> out(256);
    for (std::size_t i = 0; i < out.size(); ++i) out[i] = Rgb{static_cast<u8>(i), static_cast<u8>(i), static_cast<u8>(i)};
    return out;
  }();
  return g;
}

}  // namespace

void writeGrid(const std::filesystem::path& file, const Grid8& g) {
  if (g.empty()) throw DataError("cannot write an empty picture to " + file.string());
  if (!writeIndexedPng(file, g.raw().data(), g.width(), g.height(), greys()))
    throw DataError("cannot write " + file.string());
}

Grid8 readGrid(const std::filesystem::path& file) {
  const auto png = readPng(file);
  if (!png) throw DataError("cannot read " + file.string());
  Grid8 g(png->width, png->height);
  // Every channel carries the index; red is as good as any.
  for (int y = 0; y < png->height; ++y)
    for (int x = 0; x < png->width; ++x)
      g(x, y) = png->pixels[(static_cast<std::size_t>(y) * png->width + x) * 4];
  return g;
}

void writePreview(const std::filesystem::path& file, const Grid8& g, const std::vector<Rgb>& colours) {
  if (g.empty() || colours.empty()) return;
  std::vector<Rgb> full(256);
  for (std::size_t i = 0; i < full.size() && i < colours.size(); ++i) full[i] = colours[i];
  writeIndexedPng(file, g.raw().data(), g.width(), g.height(), full);
}

void writeJson(const std::filesystem::path& file, const Json& j) {
  const std::string text = j.dump();
  if (!file::writeAll(file, ByteView(reinterpret_cast<const u8*>(text.data()), text.size())))
    throw DataError("cannot write " + file.string());
}

Json readJson(const std::filesystem::path& file) {
  const auto bytes = file::readAll(file);
  if (!bytes) throw DataError("cannot read " + file.string());
  try {
    return Json::parse(std::string_view(reinterpret_cast<const char*>(bytes->data()), bytes->size()));
  } catch (const DataError& e) {
    throw DataError(file.string() + ": " + e.what());
  }
}

Json rgb(const Rgb& c) { return Json(Json::Array{Json(c.r), Json(c.g), Json(c.b)}); }

Rgb toRgb(const Json& j) { return Rgb{num<u8>(j.at(0)), num<u8>(j.at(1)), num<u8>(j.at(2))}; }

Json palette(const std::vector<Rgb>& colours) {
  Json a = Json::array();
  for (const Rgb& c : colours) a.push(rgb(c));
  return a;
}

std::vector<Rgb> toPalette(const Json& j) {
  std::vector<Rgb> out;
  for (const Json& c : j.items()) out.push_back(toRgb(c));
  return out;
}

}  // namespace pfr::open
