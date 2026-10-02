#pragma once
// A small JSON value with its own reader and writer, in the spirit of the PNG reader: enough
// for the open table files and no more. Numbers are integers, since everything a table holds
// is; objects keep the order they were built in, so a file written twice comes out the same
// and differences between two tables read as differences.
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "core/Types.h"

namespace pfr {

class Json {
 public:
  using Array = std::vector<Json>;
  using Object = std::vector<std::pair<std::string, Json>>;

  Json() = default;  ///< null
  Json(std::nullptr_t) {}
  Json(bool b) : v_(b) {}
  Json(int i) : v_(static_cast<std::int64_t>(i)) {}
  Json(unsigned i) : v_(static_cast<std::int64_t>(i)) {}
  Json(long i) : v_(static_cast<std::int64_t>(i)) {}
  Json(unsigned long i) : v_(static_cast<std::int64_t>(i)) {}
  Json(long long i) : v_(static_cast<std::int64_t>(i)) {}
  Json(unsigned long long i) : v_(static_cast<std::int64_t>(i)) {}
  Json(const char* s) : v_(std::string(s)) {}
  Json(std::string s) : v_(std::move(s)) {}
  Json(Array a) : v_(std::move(a)) {}
  Json(Object o) : v_(std::move(o)) {}

  static Json array() { return Json(Array{}); }
  static Json object() { return Json(Object{}); }

  bool isNull() const { return std::holds_alternative<std::nullptr_t>(v_); }
  bool isObject() const { return std::holds_alternative<Object>(v_); }
  bool isArray() const { return std::holds_alternative<Array>(v_); }

  /// Each of these throws DataError, naming what was expected, on a value of another kind.
  std::int64_t integer() const;
  bool boolean() const;
  const std::string& string() const;
  const Array& items() const;
  const Object& members() const;

  /// A member, or nullptr when there is none.
  const Json* find(std::string_view key) const;
  /// A member that has to be there.
  const Json& at(std::string_view key) const;
  const Json& at(std::size_t i) const;
  std::size_t size() const;

  /// Building: adds a member (objects) or an element (arrays), and returns *this.
  Json& set(std::string key, Json value);
  Json& push(Json value);

  /// Two-space indentation; an array of plain values is laid out on as few lines as fit.
  std::string dump() const;
  static Json parse(std::string_view text);

  /// Bytes as a string, printable ASCII as itself and everything else as \u00XX, so that a
  /// dot-matrix message reads as text and still comes back byte for byte.
  static Json bytes(const std::vector<u8>& b);
  std::vector<u8> toBytes() const;

 private:
  void write(std::string& out, int indent) const;
  std::variant<std::nullptr_t, bool, std::int64_t, std::string, Array, Object> v_;
};

}  // namespace pfr
