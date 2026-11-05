#include "engine/table/Program.h"

#include <algorithm>
#include <array>
#include <cstdio>

namespace encore {
namespace {

struct Maps {
  std::array<std::unordered_map<u16, u16>, 3> data, indexed, codeData, value, target;
  struct Run {
    u16 first, last, to;
  };
  std::array<std::vector<Run>, 3> code;
};

const Maps& maps() {
  static const Maps m = [] {
    Maps m;
    auto put = [](std::array<std::unordered_map<u16, u16>, 3>& into, u16 a, u16 b, u16 c, u16 d) {
      const u16 to[3] = {b, c, d};
      for (int t = 0; t < 3; ++t)
        if (to[t] != kNone) into[static_cast<std::size_t>(t)][a] = to[t];
    };
#define ENCORE_MAP_DATA(a, b, c, d) put(m.data, a, b, c, d);
#define ENCORE_MAP_CODE_DATA(a, b, c, d) put(m.codeData, a, b, c, d);
#define ENCORE_MAP_INDEXED(a, b, c, d) put(m.indexed, a, b, c, d);
#define ENCORE_MAP_VALUE(a, b, c, d) put(m.value, a, b, c, d);
#define ENCORE_MAP_TARGET(a, b, c, d) put(m.target, a, b, c, d);
#define ENCORE_MAP_CODE(t, a, b, to) m.code[t - 1].push_back({a, b, to});
#include "engine/table/Maps.inc"
#undef ENCORE_MAP_DATA
#undef ENCORE_MAP_CODE_DATA
#undef ENCORE_MAP_INDEXED
#undef ENCORE_MAP_VALUE
#undef ENCORE_MAP_TARGET
#undef ENCORE_MAP_CODE
    return m;
  }();
  return m;
}

/// The segments of each program's listing: code, data.
constexpr u16 kCodeSegment = 0x0010;
constexpr u16 kDataSegments[4] = {0x19b5, 0x18cf, 0x1896, 0x164d};

[[noreturn]] void lost(const char* what, u16 a, int table) {
  char b[96];
  std::snprintf(b, sizeof b, "table %d: no place known for Party Land's %s %04x", table + 1, what, a);
  throw DataError(b);
}

}  // namespace

Program::Program(ByteView prg, int table) : table_(table & 3), ds_(0x10000, 0), cs_(0x10000, 0) {
  if (prg.size() < 0x200u || prg[0] != 'M' || prg[1] != 'Z') throw DataError("not a table program");
  const std::size_t header = std::size_t{rd16le(prg, mz::headerParagraphs)} * 16u;
  dataSegment_ = kDataSegments[table_];
  image_.assign(prg.begin() + static_cast<std::ptrdiff_t>(header), prg.end());
  image_.resize(image_.size() + 0x10000u, 0);
  // As DOS does on loading: every segment value the program carries gets the load segment added.
  const std::size_t relocations = rd16le(prg, mz::relocations), table0 = rd16le(prg, mz::relocationTable);
  for (std::size_t i = 0; i < relocations; ++i) {
    const std::size_t at = std::size_t{rd16le(prg, table0 + i * 4u + 2)} * 16u + rd16le(prg, table0 + i * 4u);
    if (at + 1 >= image_.size()) continue;
    const u16 v = static_cast<u16>((image_[at] | (image_[at + 1] << 8)) + kLoadSegment);
    image_[at] = static_cast<u8>(v);
    image_[at + 1] = static_cast<u8>(v >> 8);
  }
  auto segment = [&](u16 seg, std::vector<u8>& into) {
    const std::size_t from = std::size_t{seg} * 16u;
    if (from >= image_.size()) throw DataError("table program too short");
    const std::size_t n = std::min<std::size_t>(0x10000, image_.size() - from);
    std::copy_n(image_.begin() + static_cast<std::ptrdiff_t>(from), n, into.begin());
  };
  segment(kCodeSegment, cs_);
  segment(dataSegment_, ds_);
}

u16 Program::data(u16 a) const {
  if (table_ == 0) return a;
  const auto& m = maps();
  const std::size_t t = static_cast<std::size_t>(table_ - 1);
  // as a place by itself, then as a table's address used with a register, then as a number
  if (auto it = m.data[t].find(a); it != m.data[t].end()) return it->second;
  if (auto it = m.indexed[t].find(a); it != m.indexed[t].end()) return it->second;
  if (auto it = m.value[t].find(a); it != m.value[t].end()) return it->second;
  lost("data", a, table_);
}

u16 Program::S(u16 segment) const {
  if (table_ == 0) return static_cast<u16>(segment + kLoadSegment);
  const auto& m = maps().value[static_cast<std::size_t>(table_ - 1)];
  if (auto it = m.find(segment); it != m.end()) return static_cast<u16>(it->second + kLoadSegment);
  lost("segment", segment, table_);
}

u16 Program::codeData(u16 a) const {
  if (table_ == 0) return a;
  const auto& m = maps().codeData[static_cast<std::size_t>(table_ - 1)];
  if (auto it = m.find(a); it != m.end()) return it->second;
  const auto& values = maps().value[static_cast<std::size_t>(table_ - 1)];
  if (auto it = values.find(a); it != values.end()) return it->second;
  lost("code variable", a, table_);
}

u16 Program::F(u16 a) const {
  if (table_ == 0) return a;
  for (const auto& run : maps().code[static_cast<std::size_t>(table_ - 1)])
    if (a >= run.first && a <= run.last) return static_cast<u16>(run.to + (a - run.first));
  // not among the instructions that line up: one of the table's own routines, which the
  // engine calls or jumps to from an instruction that does
  const auto& targets = maps().target[static_cast<std::size_t>(table_ - 1)];
  if (auto it = targets.find(a); it != targets.end()) return it->second;
  // or one kept as a pointer, which an instruction that lines up carries as a number
  const auto& values = maps().value[static_cast<std::size_t>(table_ - 1)];
  if (auto it = values.find(a); it != values.end()) return it->second;
  lost("routine", a, table_);
}

bool Program::has(u16 a) const {
  if (table_ == 0) return true;
  const std::size_t t = static_cast<std::size_t>(table_ - 1);
  for (const auto& run : maps().code[t])
    if (a >= run.first && a <= run.last) return true;
  return maps().target[t].contains(a) || maps().value[t].contains(a);
}

u16 Program::partyLandData(u16 native) const {
  if (table_ == 0) return native;
  const auto& m = maps();
  const std::size_t t = static_cast<std::size_t>(table_ - 1);
  for (const auto& [a, b] : m.data[t])
    if (b == native) return a;
  return kNone;
}

void Program::bind(u16 a, std::function<void()> fn) { routines_[F(a)] = std::move(fn); }

void Program::call(u16 native) {
  const auto it = routines_.find(native);
  if (it == routines_.end()) {
    char b[80];
    std::snprintf(b, sizeof b, "table %d: the routine at %04x is not written yet", table_ + 1, native);
    throw DataError(b);
  }
  called_.insert(native);
  if (debugWatch < 0) return it->second();
  const u8 before = ds_[static_cast<std::size_t>(debugWatch)];
  it->second();
  if (ds_[static_cast<std::size_t>(debugWatch)] != before)
    std::fprintf(stderr, "[ours] %04x changed from %02x to %02x in the routine at %04x\n", static_cast<unsigned>(debugWatch), before,
                 ds_[static_cast<std::size_t>(debugWatch)], native);
}

std::vector<u16> Program::neverCalled() const {
  std::vector<u16> never;
  for (const auto& [at, fn] : routines_)
    if (!called_.contains(at)) never.push_back(at);
  std::sort(never.begin(), never.end());
  return never;
}

}  // namespace encore
