#pragma once
// What the open table and intro files have in common: pictures as PNG, everything else as
// JSON. A picture is stored as its palette indices, each written as that shade of grey, since
// the engine draws by index -- lamps light up by changing what an index means -- and two
// indices may well share a colour. A coloured preview is written beside it for people.
#include <filesystem>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

#include "assets/Grid.h"
#include "core/Error.h"
#include "core/Json.h"
#include "core/Types.h"

namespace pfr::open {

void writeGrid(const std::filesystem::path& file, const Grid8& g);
Grid8 readGrid(const std::filesystem::path& file);
void writePreview(const std::filesystem::path& file, const Grid8& g, const std::vector<Rgb>& palette);

void writeJson(const std::filesystem::path& file, const Json& j);
Json readJson(const std::filesystem::path& file);

Json rgb(const Rgb& c);
Rgb toRgb(const Json& j);
Json palette(const std::vector<Rgb>& colours);
std::vector<Rgb> toPalette(const Json& j);

/// A number that has to fit the type it is going into, or the file is wrong.
template <typename T>
T num(const Json& j) {
  const std::int64_t v = j.integer();
  // Unsigned types are compared unsigned: the largest std::size_t does not fit an int64_t.
  bool fits;
  if constexpr (std::is_unsigned_v<T>)
    fits = v >= 0 && static_cast<std::uint64_t>(v) <= std::numeric_limits<T>::max();
  else
    fits = v >= std::numeric_limits<T>::min() && v <= std::numeric_limits<T>::max();
  if (!fits) throw DataError("json: " + std::to_string(v) + " is out of range here");
  return static_cast<T>(v);
}

/// An enumeration by its name, from the name tables the enumerations come with.
template <typename E, std::size_t N>
Json name(E e, const char* const (&names)[N]) {
  return Json(names[static_cast<std::size_t>(e)]);
}
template <typename E, std::size_t N>
E toEnum(const Json& j, const char* const (&names)[N]) {
  const std::string& s = j.string();
  for (std::size_t i = 0; i < N; ++i)
    if (s == names[i]) return static_cast<E>(i);
  throw DataError("json: unknown name \"" + s + "\"");
}

}  // namespace pfr::open
