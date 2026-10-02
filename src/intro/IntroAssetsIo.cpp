#include "intro/IntroAssetsIo.h"

#include <string>
#include <system_error>

#include "assets/OpenFormat.h"

namespace pfr {
namespace {

using open::num;
namespace fs = std::filesystem;

constexpr int kFormat = 1;

/// A picture carries its own palette: the pixels beside the file, the colours in the JSON.
Json image(const IntroImage& img, const fs::path& dir, const std::string& name) {
  open::writeGrid(dir / (name + ".png"), img.data);
  open::writePreview(dir / (name + "-preview.png"), img.data, img.cmap);
  return Json::object().set("file", name + ".png").set("palette", open::palette(img.cmap));
}

IntroImage toImage(const Json& j, const fs::path& dir) {
  return IntroImage{open::readGrid(dir / j.at("file").string()), open::toPalette(j.at("palette"))};
}

Json lines(const std::vector<std::vector<u8>>& ls) {
  Json a = Json::array();
  for (const auto& l : ls) a.push(Json::bytes(l));
  return a;
}

std::vector<std::vector<u8>> toLines(const Json& j) {
  std::vector<std::vector<u8>> out;
  for (const Json& l : j.items()) out.push_back(l.toBytes());
  return out;
}

}  // namespace

void saveIntroAssets(const IntroAssets& a, const fs::path& dir) {
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec) throw DataError("cannot create " + dir.string() + ": " + ec.message());

  Json j = Json::object();
  j.set("format", kFormat);
  Json slides = Json::array();
  for (std::size_t i = 0; i < a.slides.size(); ++i) {
    const Slide& s = a.slides[i];
    slides.push(Json::object()
                    .set("image", image(s.image, dir, "slide-" + std::to_string(i + 1)))
                    .set("gapFrames", s.gapFrames)
                    .set("fadeInFrames", s.fadeInFrames)
                    .set("fadeOutFrames", s.fadeOutFrames)
                    .set("fadeOutTick", s.fadeOutTick)
                    .set("fadeFromWhite", s.fadeFromWhite));
  }
  j.set("slides", slides);
  j.set("left", image(a.left, dir, "left"));
  Json tables = Json::array();
  for (std::size_t i = 0; i < a.tables.size(); ++i) tables.push(image(a.tables[i], dir, "table-" + std::to_string(i + 1)));
  j.set("tables", tables);
  j.set("hiscoresLq", image(a.hiscoresLq, dir, "hiscores-lq"));
  j.set("hiscoresHq", image(a.hiscoresHq, dir, "hiscores-hq"));
  j.set("fontLq", image(a.fontLq, dir, "font-lq"));
  j.set("fontHq", image(a.fontHq, dir, "font-hq"));

  Json pages = Json::array();
  for (const TextPage& p : a.textPages)
    pages.push(Json::object().set("hiScores", p.hiScores).set("tables34", p.tables34).set("lines", lines(p.lines)));
  j.set("textPages", pages);
  j.set("leftTextMenu", lines(a.leftTextMenu));
  j.set("leftTextOptions", lines(a.leftTextOptions));
  Json warp = Json::array();
  for (const u8 v : a.warpTable) warp.push(v);
  j.set("warpTable", warp);
  j.set("warpFrames", a.warpFrames);

  open::writeJson(dir / "intro.json", j);
}

IntroAssets loadIntroAssets(const fs::path& dir) {
  const Json j = open::readJson(dir / "intro.json");
  if (j.at("format").integer() != kFormat)
    throw DataError((dir / "intro.json").string() + ": written in a format this version does not read");
  try {
    IntroAssets a;
    for (const Json& s : j.at("slides").items())
      a.slides.push_back(Slide{toImage(s.at("image"), dir), num<u8>(s.at("gapFrames")), num<u8>(s.at("fadeInFrames")),
                               num<u8>(s.at("fadeOutFrames")), num<u32>(s.at("fadeOutTick")), s.at("fadeFromWhite").boolean()});
    a.left = toImage(j.at("left"), dir);
    for (std::size_t i = 0; i < a.tables.size(); ++i) a.tables[i] = toImage(j.at("tables").at(i), dir);
    a.hiscoresLq = toImage(j.at("hiscoresLq"), dir);
    a.hiscoresHq = toImage(j.at("hiscoresHq"), dir);
    a.fontLq = toImage(j.at("fontLq"), dir);
    a.fontHq = toImage(j.at("fontHq"), dir);
    for (const Json& p : j.at("textPages").items())
      a.textPages.push_back(TextPage{p.at("hiScores").boolean(), p.at("tables34").boolean(), toLines(p.at("lines"))});
    a.leftTextMenu = toLines(j.at("leftTextMenu"));
    a.leftTextOptions = toLines(j.at("leftTextOptions"));
    for (const Json& v : j.at("warpTable").items()) a.warpTable.push_back(num<u8>(v));
    a.warpFrames = num<u8>(j.at("warpFrames"));
    return a;
  } catch (const DataError& e) {
    throw DataError((dir / "intro.json").string() + ": " + e.what());
  }
}

}  // namespace pfr
