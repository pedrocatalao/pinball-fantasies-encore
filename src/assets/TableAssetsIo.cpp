#include "assets/TableAssetsIo.h"

#include <string>
#include <system_error>

#include "assets/OpenFormat.h"

namespace pfr {
namespace {

using open::num;
namespace fs = std::filesystem;

constexpr int kFormat = 1;
constexpr const char* kLayerNames[] = {"ground", "overhead"};
constexpr const char* kSideNames[] = {"left", "right"};

// ---- small values ------------------------------------------------------------------------

Json rect(const TRect& r) { return Json(Json::Array{Json(r.x0), Json(r.y0), Json(r.x1), Json(r.y1)}); }
TRect toRect(const Json& j) { return TRect{num<i16>(j.at(0)), num<i16>(j.at(1)), num<i16>(j.at(2)), num<i16>(j.at(3))}; }

Json pair16(const std::array<i16, 2>& p) { return Json(Json::Array{Json(p[0]), Json(p[1])}); }
std::array<i16, 2> toPair16(const Json& j) { return {num<i16>(j.at(0)), num<i16>(j.at(1))}; }

/// Twelve digits, most significant first, as the game keeps them.
Json bcd(const Bcd& b) {
  std::string s;
  for (const u8 d : b.digits) s += static_cast<char>('0' + d);
  return Json(s);
}
Bcd toBcd(const Json& j) {
  const std::string& s = j.string();
  if (s.size() != Bcd::kDigits) throw DataError("json: a score has twelve digits, not \"" + s + "\"");
  Bcd b;
  for (std::size_t i = 0; i < Bcd::kDigits; ++i) {
    if (s[i] < '0' || s[i] > '9') throw DataError("json: a score is digits only, not \"" + s + "\"");
    b.digits[i] = static_cast<u8>(s[i] - '0');
  }
  return b;
}

Json sfx(const Sfx& s) { return Json::object().set("sample", s.sample).set("period", s.period).set("channel", s.channel); }
Sfx toSfx(const Json& j) { return Sfx{num<u8>(j.at("sample")), num<u8>(j.at("period")), num<u8>(j.at("channel"))}; }

Json jingle(const Jingle& g) { return Json::object().set("position", g.position).set("repeat", g.repeat).set("priority", g.priority); }
Jingle toJingle(const Json& j) { return Jingle{num<u8>(j.at("position")), num<u8>(j.at("repeat")), num<u8>(j.at("priority"))}; }

Json font(DmFont f) { return Json(dmFontHeight(f)); }
DmFont toFont(const Json& j) {
  switch (j.integer()) {
    case 5: return DmFont::H5;
    case 8: return DmFont::H8;
    case 11: return DmFont::H11;
    case 13: return DmFont::H13;
    default: throw DataError("json: dot-matrix fonts are 5, 8, 11 or 13 dots high");
  }
}

Json score(const ScriptScoreRef& s) {
  return Json::object().set("kind", open::name(s.kind, kScriptScoreNames)).set("arg", s.arg).set("value", bcd(s.value));
}
ScriptScoreRef toScore(const Json& j) {
  return ScriptScoreRef{open::toEnum<ScriptScore>(j.at("kind"), kScriptScoreNames), num<u8>(j.at("arg")), toBcd(j.at("value"))};
}

// ---- enum-indexed tables, written as objects keyed by name --------------------------------

template <typename T, std::size_t N, std::size_t M, typename F>
Json bound(const std::array<std::optional<T>, N>& binds, const char* const (&names)[M], F write) {
  static_assert(N == M, "one name for every entry");
  Json o = Json::object();
  for (std::size_t i = 0; i < N; ++i)
    if (binds[i]) o.set(names[i], write(*binds[i]));
  return o;
}

template <typename T, std::size_t N, std::size_t M, typename F>
void toBound(const Json& j, std::array<std::optional<T>, N>& binds, const char* const (&names)[M], F read) {
  static_assert(N == M, "one name for every entry");
  for (const auto& [key, value] : j.members()) {
    std::size_t i = 0;
    while (i < N && key != names[i]) ++i;
    if (i == N) throw DataError("json: unknown name \"" + key + "\"");
    binds[i] = read(value);
  }
}

// ---- the script ----------------------------------------------------------------------------

/// Only what differs from an empty instruction, so each reads as what it does.
Json uop(const Uop& u) {
  static const Uop blank;
  Json o = Json::object().set("op", open::name(u.kind, kUopKindNames));
  if (u.value != blank.value) o.set("value", u.value);
  if (u.target != blank.target) o.set("target", u.target);
  if (!(u.score == blank.score)) o.set("score", score(u.score));
  if (u.font != blank.font) o.set("font", font(u.font));
  if (u.flag != blank.flag) o.set("flag", u.flag);
  if (!(u.coord == blank.coord)) o.set("at", Json(Json::Array{Json(u.coord.x), Json(u.coord.y)}));
  if (u.msg != blank.msg) o.set("msg", u.msg);
  if (u.end != blank.end) o.set("end", u.end);
  if (u.anim != blank.anim) o.set("anim", u.anim);
  if (!(u.sfx == blank.sfx)) o.set("sfx", sfx(u.sfx));
  if (u.volume != blank.volume) o.set("volume", u.volume);
  if (!(u.jingle == blank.jingle)) o.set("jingle", jingle(u.jingle));
  if (u.time != blank.time) o.set("time", u.time);
  return o;
}

Uop toUop(const Json& j) {
  Uop u;
  u.kind = open::toEnum<UopKind>(j.at("op"), kUopKindNames);
  if (const Json* v = j.find("value")) u.value = num<u16>(*v);
  if (const Json* v = j.find("target")) u.target = num<u16>(*v);
  if (const Json* v = j.find("score")) u.score = toScore(*v);
  if (const Json* v = j.find("font")) u.font = toFont(*v);
  if (const Json* v = j.find("flag")) u.flag = v->boolean();
  if (const Json* v = j.find("at")) u.coord = DmCoord{num<i16>(v->at(0)), num<i16>(v->at(1))};
  if (const Json* v = j.find("msg")) u.msg = num<u16>(*v);
  if (const Json* v = j.find("end")) u.end = num<i16>(*v);
  if (const Json* v = j.find("anim")) u.anim = num<u8>(*v);
  if (const Json* v = j.find("sfx")) u.sfx = toSfx(*v);
  if (const Json* v = j.find("volume")) u.volume = num<u8>(*v);
  if (const Json* v = j.find("jingle")) u.jingle = toJingle(*v);
  if (const Json* v = j.find("time")) u.time = num<u8>(*v);
  return u;
}

// ---- pictures made of several frames: one sheet, frames stacked downwards ------------------

Grid8 sheet(const std::vector<Grid8>& frames, const std::string& what) {
  const int w = frames.front().width(), h = frames.front().height();
  for (const Grid8& f : frames)
    if (f.width() != w || f.height() != h) throw DataError(what + ": frames of different sizes cannot share a sheet");
  Grid8 s(w, h * static_cast<int>(frames.size()));
  for (std::size_t i = 0; i < frames.size(); ++i) s.paste(0, static_cast<int>(i) * h, frames[i]);
  return s;
}

std::vector<Grid8> unsheet(const Grid8& s, int frames, const std::string& what) {
  if (frames <= 0 || s.height() % frames != 0) throw DataError(what + ": the sheet does not divide into " + std::to_string(frames) + " frames");
  const int h = s.height() / frames;
  std::vector<Grid8> out;
  for (int i = 0; i < frames; ++i) out.push_back(s.slice(0, i * h, s.width(), h));
  return out;
}

std::string patchFile(std::size_t i, const char* which) {
  return std::string("patch-") + kPhysmapBindNames[i] + "-" + which + ".png";
}

}  // namespace

void saveTableAssets(const TableAssets& a, const fs::path& dir) {
  std::error_code ec;
  fs::create_directories(dir, ec);
  if (ec) throw DataError("cannot create " + dir.string() + ": " + ec.message());

  // Pictures and maps.
  open::writeGrid(dir / "playfield.png", a.mainBoard);
  // The palette as stored holds, in the lamps' entries, colours the game never shows: it
  // writes them every frame, a lamp's own colour when lit and half of it when not. The two
  // previews follow that rule, so they show the table as it starts and with every lamp on.
  const auto withLamps = [&](bool lit) {
    std::vector<Rgb> pal = a.palette;
    for (const Light& l : a.lights)
      for (std::size_t i = 0; i < l.colors.size() && l.baseIndex + i < pal.size(); ++i) {
        const Rgb c = l.colors[i];
        pal[l.baseIndex + i] = lit ? c : Rgb{static_cast<u8>(c.r / 2), static_cast<u8>(c.g / 2), static_cast<u8>(c.b / 2)};
      }
    return pal;
  };
  open::writePreview(dir / "playfield-preview.png", a.mainBoard, withLamps(false));
  open::writePreview(dir / "playfield-lit-preview.png", a.mainBoard, withLamps(true));
  open::writeGrid(dir / "spring.png", a.spring);
  open::writeGrid(dir / "ball.png", a.ball);
  for (std::size_t l = 0; l < 2; ++l) {
    open::writeGrid(dir / (std::string("occmap-") + kLayerNames[l] + ".png"), a.occmaps[l]);
    open::writeGrid(dir / (std::string("physmap-") + kLayerNames[l] + ".png"), a.physmaps[l]);
  }
  if (a.dmTower) open::writeGrid(dir / "dm-tower.png", *a.dmTower);

  Json j = Json::object();
  j.set("format", kFormat);
  j.set("table", a.table);
  j.set("palette", open::palette(a.palette));
  j.set("dmTower", a.dmTower.has_value());

  Json patches = Json::object();
  for (std::size_t i = 0; i < a.physmapPatches.size(); ++i) {
    const auto& p = a.physmapPatches[i];
    if (!p) continue;
    Json o = Json::object().set("layer", kLayerNames[static_cast<std::size_t>(p->layer)]).set("x", p->x).set("y", p->y);
    o.set("raised", !p->raised.empty()).set("dropped", !p->dropped.empty());
    if (!p->raised.empty()) open::writeGrid(dir / patchFile(i, "raised"), p->raised);
    if (!p->dropped.empty()) open::writeGrid(dir / patchFile(i, "dropped"), p->dropped);
    patches.set(kPhysmapBindNames[i], o);
  }
  j.set("physmapPatches", patches);

  Json ramps = Json::array();
  for (const Ramp& r : a.ramps) ramps.push(Json::object().set("accel", pair16(r.accel)).set("accelHires", pair16(r.accelHires)));
  j.set("ramps", ramps);

  Json outline = Json::array();
  for (const BallOutlinePixel& p : a.ballOutline)
    outline.push(Json(Json::Array{Json(p.x), Json(p.y), Json(p.angle), Json(p.quad), Json(p.idx), Json(p.isBot), Json(p.isRight)}));
  j.set("ballOutline", outline);
  Json byAngle = Json::array();
  for (const auto& p : a.ballOutlineByAngle) byAngle.push(pair16(p));
  j.set("ballOutlineByAngle", byAngle);

  Json lights = Json::array();
  for (const Light& l : a.lights) lights.push(Json::object().set("base", l.baseIndex).set("colors", open::palette(l.colors)));
  j.set("lights", lights);
  Json attract = Json::array();
  for (const AttractLight& l : a.attractLights)
    attract.push(Json::object().set("light", l.light).set("reset", l.ctrReset).set("off", l.ctrOff).set("on", l.ctrOn));
  j.set("attractLights", attract);
  Json lightBinds = Json::object();
  for (std::size_t i = 0; i < a.lightBinds.size(); ++i) {
    if (a.lightBinds[i].empty()) continue;
    Json l = Json::array();
    for (const u8 v : a.lightBinds[i]) l.push(v);
    lightBinds.set(kLightBindNames[i], l);
  }
  j.set("lightBinds", lightBinds);

  j.set("dmPalette", Json::object()
                         .set("off", a.dmPalette.indexOff)
                         .set("on", a.dmPalette.indexOn)
                         .set("colorOff", open::rgb(a.dmPalette.colorOff))
                         .set("colorOn", open::rgb(a.dmPalette.colorOn)));
  Json fonts = Json::object();
  static constexpr DmFont kFonts[] = {DmFont::H5, DmFont::H8, DmFont::H11, DmFont::H13};
  for (std::size_t f = 0; f < a.dmFonts.size(); ++f) {
    Json glyphs = Json::object();
    for (const auto& [code, rows] : a.dmFonts[f]) {
      Json r = Json::array();
      for (const u8 v : rows) r.push(v);
      glyphs.set(std::to_string(code), r);
    }
    fonts.set(std::to_string(dmFontHeight(kFonts[f])), glyphs);
  }
  j.set("dmFonts", fonts);

  Json flippers = Json::array();
  for (std::size_t i = 0; i < a.flippers.size(); ++i) {
    const Flipper& f = a.flippers[i];
    const std::string base = "flipper-" + std::to_string(i);
    if (!f.physmap.empty()) open::writeGrid(dir / (base + "-physmap.png"), sheet(f.physmap, base));
    if (!f.gfx.empty()) open::writeGrid(dir / (base + "-gfx.png"), sheet(f.gfx, base));
    flippers.push(Json::object()
                      .set("side", kSideNames[static_cast<std::size_t>(f.side)])
                      .set("rect", Json(Json::Array{Json(f.rectX), Json(f.rectY)}))
                      .set("physmapFrames", f.physmap.size())
                      .set("gfxFrames", f.gfx.size())
                      .set("ballBox", rect(f.ballBbox))
                      .set("origin", Json(Json::Array{Json(f.originX), Json(f.originY)}))
                      .set("vertical", f.isVertical)
                      .set("quantumMax", f.quantumMax)
                      .set("posMax", f.posMax)
                      .set("accelPress", f.accelPress)
                      .set("accelRelease", f.accelRelease)
                      .set("speedPressStart", f.speedPressStart));
  }
  j.set("flippers", flippers);

  Json down = Json::array(), up = Json::array();
  for (const TRect& r : a.transitionsDown) down.push(rect(r));
  for (const TRect& r : a.transitionsUp) up.push(rect(r));
  j.set("transitionsDown", down);
  j.set("transitionsUp", up);

  Json bumpers = Json::array();
  for (const Bumper& b : a.bumpers)
    bumpers.push(Json::object().set("kicker", b.isKicker).set("rect", rect(b.rect)).set("sfx", sfx(b.sfx)).set("score", bcd(b.score)));
  j.set("bumpers", bumpers);

  const auto rolls = [](const std::array<std::vector<RollTriggerArea>, 2>& layers) {
    Json out = Json::object();
    for (std::size_t l = 0; l < 2; ++l) {
      Json list = Json::array();
      for (const RollTriggerArea& t : layers[l])
        list.push(Json::object().set("rect", rect(t.rect)).set("kind", open::name(t.kind, kRollTriggerNames)).set("arg", t.arg));
      out.set(kLayerNames[l], list);
    }
    return out;
  };
  j.set("rollTriggers", rolls(a.rollTriggers));
  j.set("rollTriggersTilt", rolls(a.rollTriggersTilt));
  Json hits = Json::array();
  for (const HitTriggerArea& t : a.hitTriggers)
    hits.push(Json::object().set("rect", rect(t.rect)).set("kind", open::name(t.kind, kHitTriggerNames)).set("arg", t.arg));
  j.set("hitTriggers", hits);

  j.set("jingles", bound(a.jingleBinds, kJingleBindNames, jingle));
  j.set("sfx", bound(a.sfxBinds, kSfxBindNames, sfx));
  j.set("positionJingleStart", a.positionJingleStart);

  Json scripts = Json::array();
  for (const Uop& u : a.scripts) scripts.push(uop(u));
  j.set("scripts", scripts);
  j.set("scriptBinds", bound(a.scriptBinds, kScriptBindNames, [](u16 v) { return Json(v); }));
  Json msgs = Json::array();
  for (const auto& m : a.msgs) msgs.push(Json::bytes(m));
  j.set("messages", msgs);
  Json anims = Json::array();
  for (const DmAnim& an : a.anims) {
    Json frames = Json::array();
    for (const auto& [frame, time] : an.frames) frames.push(Json(Json::Array{Json(frame), Json(time)}));
    anims.push(Json::object().set("repeats", an.repeats).set("restart", an.restart).set("numFrames", an.numFrames).set("frames", frames));
  }
  j.set("anims", anims);
  // A frame is a run of dots, each x, y and 1 for lit or 0 for dark, in the order they change:
  // flat, so a frame packs onto a few lines rather than one line a dot.
  Json animFrames = Json::array();
  for (const DmAnimFrame& f : a.animFrames) {
    Json dots = Json::array();
    for (const auto& [at, on] : f) dots.push(at.x).push(at.y).push(on ? 1 : 0);
    animFrames.push(dots);
  }
  j.set("animFrames", animFrames);
  Json cheats = Json::array();
  for (const Cheat& c : a.cheats)
    cheats.push(Json::object().set("keys", Json(c.keys)).set("script", c.script).set("effect", open::name(c.effect, kCheatEffectNames)));
  j.set("cheats", cheats);
  j.set("effects", bound(a.effects, kEffectBindNames, [](const Effect& e) {
          return Json::object()
              .set("jingle", e.jingle ? jingle(*e.jingle) : Json())
              .set("silentPriority", e.silentPriority)
              .set("scoreMain", bcd(e.scoreMain))
              .set("scoreBonus", bcd(e.scoreBonus))
              .set("script", e.script ? Json(*e.script) : Json());
        }));

  j.set("scores", Json::object()
                      .set("jackpotInit", bcd(a.scoreJackpotInit))
                      .set("jackpotIncr", bcd(a.scoreJackpotIncr))
                      .set("modeHitIncr", bcd(a.scoreModeHitIncr))
                      .set("modeRampIncr", bcd(a.scoreModeRampIncr)));
  j.set("issueBallPos", pair16(a.issueBallPos));
  j.set("issueBallReleasePos", pair16(a.issueBallReleasePos));
  Json sine = Json::array();
  for (const i16 v : a.sineTable) sine.push(v);
  j.set("sineTable", sine);

  open::writeJson(dir / "table.json", j);
}

TableAssets loadTableAssets(const fs::path& dir) {
  const Json j = open::readJson(dir / "table.json");
  if (j.at("format").integer() != kFormat)
    throw DataError((dir / "table.json").string() + ": written in a format this version does not read");
  try {
    TableAssets a;
    a.table = num<int>(j.at("table"));
    a.palette = open::toPalette(j.at("palette"));
    a.mainBoard = open::readGrid(dir / "playfield.png");
    a.spring = open::readGrid(dir / "spring.png");
    a.ball = open::readGrid(dir / "ball.png");
    for (std::size_t l = 0; l < 2; ++l) {
      a.occmaps[l] = open::readGrid(dir / (std::string("occmap-") + kLayerNames[l] + ".png"));
      a.physmaps[l] = open::readGrid(dir / (std::string("physmap-") + kLayerNames[l] + ".png"));
    }
    if (j.at("dmTower").boolean()) a.dmTower = open::readGrid(dir / "dm-tower.png");

    for (const auto& [key, o] : j.at("physmapPatches").members()) {
      std::size_t i = 0;
      while (i < a.physmapPatches.size() && key != kPhysmapBindNames[i]) ++i;
      if (i == a.physmapPatches.size()) throw DataError("json: unknown name \"" + key + "\"");
      PhysmapPatch p;
      p.layer = open::toEnum<Layer>(o.at("layer"), kLayerNames);
      p.x = num<int>(o.at("x"));
      p.y = num<int>(o.at("y"));
      if (o.at("raised").boolean()) p.raised = open::readGrid(dir / patchFile(i, "raised"));
      if (o.at("dropped").boolean()) p.dropped = open::readGrid(dir / patchFile(i, "dropped"));
      a.physmapPatches[i] = std::move(p);
    }

    for (const Json& r : j.at("ramps").items()) a.ramps.push_back(Ramp{toPair16(r.at("accel")), toPair16(r.at("accelHires"))});
    for (const Json& p : j.at("ballOutline").items())
      a.ballOutline.push_back(BallOutlinePixel{num<i16>(p.at(0)), num<i16>(p.at(1)), num<u16>(p.at(2)), num<u8>(p.at(3)),
                                               num<u8>(p.at(4)), p.at(5).boolean(), p.at(6).boolean()});
    for (const Json& p : j.at("ballOutlineByAngle").items()) a.ballOutlineByAngle.push_back(toPair16(p));

    for (const Json& l : j.at("lights").items()) a.lights.push_back(Light{num<u8>(l.at("base")), open::toPalette(l.at("colors"))});
    for (const Json& l : j.at("attractLights").items())
      a.attractLights.push_back(AttractLight{num<u16>(l.at("reset")), num<u16>(l.at("off")), num<u16>(l.at("on")), num<u8>(l.at("light"))});
    for (const auto& [key, list] : j.at("lightBinds").members()) {
      const auto b = open::toEnum<LightBind>(Json(key), kLightBindNames);
      for (const Json& v : list.items()) a.lightBinds[static_cast<std::size_t>(b)].push_back(num<u8>(v));
    }

    const Json& dp = j.at("dmPalette");
    a.dmPalette = DmPalette{num<u8>(dp.at("off")), num<u8>(dp.at("on")), open::toRgb(dp.at("colorOff")), open::toRgb(dp.at("colorOn"))};
    static constexpr DmFont kFonts[] = {DmFont::H5, DmFont::H8, DmFont::H11, DmFont::H13};
    const Json& fonts = j.at("dmFonts");
    for (std::size_t f = 0; f < a.dmFonts.size(); ++f)
      for (const auto& [code, rows] : fonts.at(std::to_string(dmFontHeight(kFonts[f]))).members()) {
        std::vector<u8> r;
        for (const Json& v : rows.items()) r.push_back(num<u8>(v));
        a.dmFonts[f][static_cast<u8>(std::stoi(code))] = std::move(r);
      }

    std::size_t n = 0;
    for (const Json& o : j.at("flippers").items()) {
      const std::string base = "flipper-" + std::to_string(n++);
      Flipper f;
      f.side = open::toEnum<FlipperSide>(o.at("side"), kSideNames);
      f.rectX = num<int>(o.at("rect").at(0));
      f.rectY = num<int>(o.at("rect").at(1));
      if (const int frames = num<int>(o.at("physmapFrames")); frames > 0)
        f.physmap = unsheet(open::readGrid(dir / (base + "-physmap.png")), frames, base);
      if (const int frames = num<int>(o.at("gfxFrames")); frames > 0)
        f.gfx = unsheet(open::readGrid(dir / (base + "-gfx.png")), frames, base);
      f.ballBbox = toRect(o.at("ballBox"));
      f.originX = num<i16>(o.at("origin").at(0));
      f.originY = num<i16>(o.at("origin").at(1));
      f.isVertical = o.at("vertical").boolean();
      f.quantumMax = num<u16>(o.at("quantumMax"));
      f.posMax = num<i16>(o.at("posMax"));
      f.accelPress = num<i16>(o.at("accelPress"));
      f.accelRelease = num<i16>(o.at("accelRelease"));
      f.speedPressStart = num<i16>(o.at("speedPressStart"));
      a.flippers.push_back(std::move(f));
    }

    for (const Json& r : j.at("transitionsDown").items()) a.transitionsDown.push_back(toRect(r));
    for (const Json& r : j.at("transitionsUp").items()) a.transitionsUp.push_back(toRect(r));
    for (const Json& b : j.at("bumpers").items())
      a.bumpers.push_back(Bumper{b.at("kicker").boolean(), toRect(b.at("rect")), toSfx(b.at("sfx")), toBcd(b.at("score"))});

    const auto rolls = [](const Json& o, std::array<std::vector<RollTriggerArea>, 2>& layers) {
      for (std::size_t l = 0; l < 2; ++l)
        for (const Json& t : o.at(kLayerNames[l]).items())
          layers[l].push_back(RollTriggerArea{toRect(t.at("rect")), open::toEnum<RollTrigger>(t.at("kind"), kRollTriggerNames), num<u8>(t.at("arg"))});
    };
    rolls(j.at("rollTriggers"), a.rollTriggers);
    rolls(j.at("rollTriggersTilt"), a.rollTriggersTilt);
    for (const Json& t : j.at("hitTriggers").items())
      a.hitTriggers.push_back(HitTriggerArea{toRect(t.at("rect")), open::toEnum<HitTrigger>(t.at("kind"), kHitTriggerNames), num<u8>(t.at("arg"))});

    toBound(j.at("jingles"), a.jingleBinds, kJingleBindNames, toJingle);
    toBound(j.at("sfx"), a.sfxBinds, kSfxBindNames, toSfx);
    a.positionJingleStart = num<u8>(j.at("positionJingleStart"));

    for (const Json& u : j.at("scripts").items()) a.scripts.push_back(toUop(u));
    toBound(j.at("scriptBinds"), a.scriptBinds, kScriptBindNames, [](const Json& v) { return num<u16>(v); });
    for (const Json& m : j.at("messages").items()) a.msgs.push_back(m.toBytes());
    for (const Json& an : j.at("anims").items()) {
      DmAnim d;
      d.repeats = num<u16>(an.at("repeats"));
      d.restart = num<std::size_t>(an.at("restart"));
      d.numFrames = num<std::size_t>(an.at("numFrames"));
      for (const Json& f : an.at("frames").items()) d.frames.emplace_back(num<u8>(f.at(0)), num<u16>(f.at(1)));
      a.anims.push_back(std::move(d));
    }
    for (const Json& f : j.at("animFrames").items()) {
      if (f.size() % 3 != 0) throw DataError("json: an animation frame is x, y, lit triples");
      DmAnimFrame frame;
      for (std::size_t i = 0; i < f.size(); i += 3)
        frame.emplace_back(DmCoord{num<i16>(f.at(i)), num<i16>(f.at(i + 1))}, num<u8>(f.at(i + 2)) != 0);
      a.animFrames.push_back(std::move(frame));
    }
    for (const Json& c : j.at("cheats").items())
      a.cheats.push_back(Cheat{c.at("keys").string(), num<u16>(c.at("script")), open::toEnum<CheatEffect>(c.at("effect"), kCheatEffectNames)});
    toBound(j.at("effects"), a.effects, kEffectBindNames, [](const Json& e) {
      Effect out;
      if (!e.at("jingle").isNull()) out.jingle = toJingle(e.at("jingle"));
      out.silentPriority = num<u8>(e.at("silentPriority"));
      out.scoreMain = toBcd(e.at("scoreMain"));
      out.scoreBonus = toBcd(e.at("scoreBonus"));
      if (!e.at("script").isNull()) out.script = num<u16>(e.at("script"));
      return out;
    });

    const Json& s = j.at("scores");
    a.scoreJackpotInit = toBcd(s.at("jackpotInit"));
    a.scoreJackpotIncr = toBcd(s.at("jackpotIncr"));
    a.scoreModeHitIncr = toBcd(s.at("modeHitIncr"));
    a.scoreModeRampIncr = toBcd(s.at("modeRampIncr"));
    a.issueBallPos = toPair16(j.at("issueBallPos"));
    a.issueBallReleasePos = toPair16(j.at("issueBallReleasePos"));
    const Json& sine = j.at("sineTable");
    if (sine.size() != a.sineTable.size()) throw DataError("json: the sine table has " + std::to_string(a.sineTable.size()) + " entries");
    for (std::size_t i = 0; i < a.sineTable.size(); ++i) a.sineTable[i] = num<i16>(sine.at(i));
    return a;
  } catch (const DataError& e) {
    throw DataError((dir / "table.json").string() + ": " + e.what());
  }
}

}  // namespace pfr
