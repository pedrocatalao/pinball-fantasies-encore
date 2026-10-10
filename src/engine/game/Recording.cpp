#include "engine/game/Recording.h"

#include <cstring>
#include <utility>

#include "engine/game/TableGame.h"

namespace encore {

namespace {

constexpr char kMagic[4] = {'P', 'F', 'R', 'P'};

struct Writer {
  Bytes out;
  void u8_(u8 v) { out.push_back(v); }
  void u16_(u16 v) { for (int i = 0; i < 2; ++i) out.push_back(static_cast<u8>(v >> (8 * i))); }
  void u32_(u32 v) { for (int i = 0; i < 4; ++i) out.push_back(static_cast<u8>(v >> (8 * i))); }
  void u64_(u64 v) { for (int i = 0; i < 8; ++i) out.push_back(static_cast<u8>(v >> (8 * i))); }
  /// Seven bits at a time, low first: frames between events are mostly small.
  void var(u32 v) {
    while (v >= 0x80) {
      out.push_back(static_cast<u8>(v | 0x80));
      v >>= 7;
    }
    out.push_back(static_cast<u8>(v));
  }
  void bcd(const Bcd& b) { out.insert(out.end(), b.digits.begin(), b.digits.end()); }
};

struct Reader {
  ByteView in;
  std::size_t at = 0;
  bool ok = true;
  bool need(std::size_t n) {
    if (at + n > in.size()) ok = false;
    return ok;
  }
  u8 u8_() { return need(1) ? in[at++] : 0; }
  u64 le(int n) {
    if (!need(static_cast<std::size_t>(n))) return 0;
    u64 v = 0;
    for (int i = 0; i < n; ++i) v |= u64{in[at++]} << (8 * i);
    return v;
  }
  u32 var() {
    u32 v = 0;
    for (int shift = 0; shift < 35; shift += 7) {
      const u8 b = u8_();
      v |= u32{static_cast<u8>(b & 0x7f)} << shift;
      if (!(b & 0x80)) return v;
    }
    ok = false;
    return 0;
  }
  Bcd bcd() {
    Bcd b;
    for (u8& d : b.digits) {
      d = u8_();
      if (d > 9) ok = false;
    }
    return b;
  }
};

}  // namespace

Bytes Recording::save() const {
  Writer w;
  w.out.insert(w.out.end(), kMagic, kMagic + 4);
  w.u16_(kFormat);
  w.u8_(static_cast<u8>(table));
  w.u64_(seed);
  w.u8_(options.balls);
  w.u8_(static_cast<u8>(options.angle));
  w.u8_(static_cast<u8>(options.scrollSpeed));
  // The size of screen is the viewer's, never the recording's (it changes only what is drawn):
  // its place is kept, as normal, for the versions that read one there.
  w.u8_(static_cast<u8>(Resolution::Normal));
  w.u8_(options.noMusic);
  w.u8_(options.mono);
  w.u8_(carry.noTilt);
  w.u8_(carry.otherSteps);
  w.u8_(carry.balls);
  w.u16_(carry.scrollPos);
  w.u16_(carry.scrollAt);
  for (const HighScore& h : highScores) {
    w.bcd(h.score);
    w.out.insert(w.out.end(), h.name.begin(), h.name.end());
  }
  w.u32_(frames);
  w.u32_(static_cast<u32>(events.size()));
  u32 last = 0;
  for (const Event& e : events) {
    w.var(e.frame - last);
    last = e.frame;
    w.u8_(static_cast<u8>(e.kind));
    if (e.kind == Event::Kind::Music) w.u32_(e.value);
    else w.u8_(static_cast<u8>(e.value));
  }
  w.u32_(static_cast<u32>(games.size()));
  for (const Game& g : games) {
    w.u32_(g.endFrame);
    w.u8_(g.abandoned);
    w.u8_(static_cast<u8>(g.scores.size()));
    for (const Bcd& s : g.scores) w.bcd(s);
    w.out.insert(w.out.end(), g.initials.begin(), g.initials.end());
  }
  return std::move(w.out);
}

std::optional<Recording> Recording::load(ByteView data) {
  Reader r{data};
  if (!r.need(4) || std::memcmp(data.data(), kMagic, 4) != 0) return std::nullopt;
  r.at = 4;
  if (r.le(2) != kFormat) return std::nullopt;
  Recording p;
  p.table = r.u8_();
  p.seed = r.le(8);
  p.options.balls = r.u8_();
  p.options.angle = static_cast<Angle>(r.u8_());
  p.options.scrollSpeed = static_cast<ScrollSpeed>(r.u8_());
  r.u8_();  // the size of screen, which a recording does not decide (see save)
  p.options.noMusic = r.u8_() != 0;
  p.options.mono = r.u8_() != 0;
  p.carry.noTilt = r.u8_() != 0;
  p.carry.otherSteps = r.u8_() != 0;
  p.carry.balls = r.u8_();
  p.carry.scrollPos = static_cast<u16>(r.le(2));
  p.carry.scrollAt = static_cast<u16>(r.le(2));
  if (p.table > 3 || static_cast<u8>(p.options.angle) > 2 || static_cast<u8>(p.options.scrollSpeed) > 2)
    return std::nullopt;
  for (HighScore& h : p.highScores) {
    h.score = r.bcd();
    for (u8& c : h.name) c = r.u8_();
  }
  p.frames = static_cast<u32>(r.le(4));
  const u32 events = static_cast<u32>(r.le(4));
  u32 frame = 0;
  for (u32 i = 0; i < events && r.ok; ++i) {
    Event e;
    frame += r.var();
    e.frame = frame;
    const u8 kind = r.u8_();
    if (kind > static_cast<u8>(Event::Kind::Music)) return std::nullopt;
    e.kind = static_cast<Event::Kind>(kind);
    e.value = e.kind == Event::Kind::Music ? static_cast<u32>(r.le(4)) : r.u8_();
    if (e.isKey() && e.value > static_cast<u32>(Key::Z)) return std::nullopt;
    p.events.push_back(e);
  }
  const u32 games = static_cast<u32>(r.le(4));
  for (u32 i = 0; i < games && r.ok; ++i) {
    Game g;
    g.endFrame = static_cast<u32>(r.le(4));
    g.abandoned = r.u8_() != 0;
    const u8 n = r.u8_();
    for (u8 j = 0; j < n && r.ok; ++j) g.scores.push_back(r.bcd());
    for (u8& c : g.initials) c = r.u8_();
    p.games.push_back(std::move(g));
  }
  if (!r.ok || r.at != data.size()) return std::nullopt;
  return p;
}

std::string Recording::fileName(const std::string& when, const std::string& tag) const {
  std::string initials, score = "0";
  if (!games.empty()) {
    for (u8 c : games[0].initials) initials += c >= 'A' && c <= 'Z' ? static_cast<char>(c) : c == ' ' ? '_' : '\0';
    if (initials.find('\0') != std::string::npos) initials.clear();
    if (!games[0].scores.empty()) {
      const auto digits = games[0].scores[0].toAscii();
      score.assign(digits.begin(), digits.end());
      score.erase(0, score.find_first_not_of(' '));
    }
  }
  if (initials.empty()) initials = "---";
  std::string name = std::string("FANTASY-") + kTableCodes[table & 3] + "-" + initials + "-";
  if (!tag.empty()) name += tag + "-";
  return name + score + "-" + when + ".RPL";
}

Recording replay(ByteView prg, ByteView module, const Recording& recording, HowPlayed* how) {
  TableGame::Setup setup;
  setup.options = recording.options;
  setup.highScores = recording.highScores;
  setup.seed = recording.seed;
  setup.carry = recording.carry;
  setup.picture = false;
  TableGame t(prg, module, recording.table, setup);
  t.playBack(&recording);
  std::size_t next = 0;
  for (u32 f = 0; f < recording.frames && !t.left(); ++f) {
    for (; next < recording.events.size() && recording.events[next].frame == f; ++next)
      if (recording.events[next].isKey()) t.key(recording.events[next].key(), recording.events[next].down());
    t.frame();
  }
  if (how) *how = {t.playedWithCheats(), t.gentlestAngle()};
  return t.recording();
}

Verdict verify(ByteView prg, ByteView module, const Recording& rec, const VerifyLimits& limits) {
  Verdict v;
  auto refuse = [&](std::string why) {
    v.ok = false;
    v.reason = std::move(why);
    return v;
  };
  if (rec.frames > limits.maxFrames) return refuse("longer than allowed");
  if (rec.events.size() > limits.maxEvents) return refuse("too many events");
  if (rec.options.balls < 1 || rec.options.balls > 9 || rec.carry.balls > 9)
    return refuse("odd number of balls");
  if (rec.cheated()) return refuse("played with cheats");
  for (std::size_t i = 0; i < rec.events.size(); ++i) {
    const Recording::Event& e = rec.events[i];
    if (e.frame > rec.frames || (i && e.frame < rec.events[i - 1].frame)) return refuse("events out of order");
  }

  HowPlayed how;
  v.replayed = replay(prg, module, rec, &how);
  if (v.replayed.games.size() != 1) return refuse("not one whole game");
  if (v.replayed.games[0].abandoned) return refuse("quit before the end");
  // (the header says what the player's game had; the replay what the table really did: a
  // recording made by hand can type a cheat's word before the start)
  if (how.cheats) return refuse("played with cheats");
  v.angle = how.angle;
  v.claimsMatch = v.replayed.games == rec.games && v.replayed.events == rec.events;
  v.ok = true;
  return v;
}

}  // namespace encore
