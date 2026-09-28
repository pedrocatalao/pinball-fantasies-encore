#include "core/Json.h"

#include <cstdio>

#include "core/Error.h"

namespace pfr {
namespace {

constexpr std::size_t kLineWidth = 100;

[[noreturn]] void fail(const std::string& what) { throw DataError("json: " + what); }

void writeString(std::string& out, const std::string& s) {
  out += '"';
  for (const char ch : s) {
    const auto c = static_cast<unsigned char>(ch);
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else if (c == '\n') out += "\\n";
    else if (c < 0x20 || c >= 0x7f) {
      char buf[8];
      std::snprintf(buf, sizeof buf, "\\u%04x", c);
      out += buf;
    } else {
      out += ch;
    }
  }
  out += '"';
}

/// Reads one document, keeping track of where it is so an error can say.
class Parser {
 public:
  explicit Parser(std::string_view text) : s_(text) {}

  Json document() {
    Json v = value();
    space();
    if (at_ != s_.size()) error("unexpected text after the value");
    return v;
  }

 private:
  [[noreturn]] void error(const std::string& what) const {
    std::size_t line = 1, col = 1;
    for (std::size_t i = 0; i < at_ && i < s_.size(); ++i) {
      if (s_[i] == '\n') { ++line; col = 1; } else { ++col; }
    }
    fail(what + " at line " + std::to_string(line) + ", column " + std::to_string(col));
  }

  void space() {
    while (at_ < s_.size() && (s_[at_] == ' ' || s_[at_] == '\n' || s_[at_] == '\r' || s_[at_] == '\t')) ++at_;
  }

  bool take(char c) {
    space();
    if (at_ < s_.size() && s_[at_] == c) { ++at_; return true; }
    return false;
  }

  void expect(char c) {
    if (!take(c)) error(std::string("expected '") + c + "'");
  }

  bool word(std::string_view w) {
    if (s_.substr(at_, w.size()) != w) return false;
    at_ += w.size();
    return true;
  }

  Json value() {
    space();
    if (at_ >= s_.size()) error("unexpected end");
    const char c = s_[at_];
    if (c == '{') return object();
    if (c == '[') return array();
    if (c == '"') return Json(string());
    if (c == '-' || (c >= '0' && c <= '9')) return number();
    if (word("true")) return Json(true);
    if (word("false")) return Json(false);
    if (word("null")) return Json();
    error("unexpected character");
  }

  Json object() {
    expect('{');
    Json o = Json::object();
    if (take('}')) return o;
    do {
      space();
      if (at_ >= s_.size() || s_[at_] != '"') error("expected a member name");
      std::string key = string();
      expect(':');
      o.set(std::move(key), value());
    } while (take(','));
    expect('}');
    return o;
  }

  Json array() {
    expect('[');
    Json a = Json::array();
    if (take(']')) return a;
    do a.push(value());
    while (take(','));
    expect(']');
    return a;
  }

  Json number() {
    const bool negative = s_[at_] == '-';
    if (negative) ++at_;
    if (at_ >= s_.size() || s_[at_] < '0' || s_[at_] > '9') error("expected a digit");
    std::int64_t v = 0;
    while (at_ < s_.size() && s_[at_] >= '0' && s_[at_] <= '9') {
      v = v * 10 + (s_[at_++] - '0');
      if (v > (std::int64_t{1} << 53)) error("number too large");
    }
    if (at_ < s_.size() && (s_[at_] == '.' || s_[at_] == 'e' || s_[at_] == 'E'))
      error("only whole numbers are allowed here");
    return Json(negative ? -v : v);
  }

  std::string string() {
    ++at_;  // the opening quote
    std::string out;
    for (;;) {
      if (at_ >= s_.size()) error("unterminated string");
      const char c = s_[at_++];
      if (c == '"') break;
      if (c != '\\') { out += c; continue; }
      if (at_ >= s_.size()) error("unterminated escape");
      const char e = s_[at_++];
      switch (e) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'u': {
          if (at_ + 4 > s_.size()) error("short \\u escape");
          unsigned cp = 0;
          for (int i = 0; i < 4; ++i) {
            const char h = s_[at_++];
            cp <<= 4;
            if (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
            else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
            else error("bad \\u escape");
          }
          // Below 256 it is a byte, which is how byte strings are written; above, UTF-8.
          if (cp < 0x100) {
            out += static_cast<char>(cp);
          } else if (cp < 0x800) {
            out += static_cast<char>(0xc0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3f));
          } else {
            out += static_cast<char>(0xe0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (cp & 0x3f));
          }
          break;
        }
        default: error("unknown escape");
      }
    }
    return out;
  }

  std::string_view s_;
  std::size_t at_ = 0;
};

bool plain(const Json& v) { return !v.isArray() && !v.isObject(); }

}  // namespace

std::int64_t Json::integer() const {
  if (const auto* v = std::get_if<std::int64_t>(&v_)) return *v;
  fail("expected a number");
}
bool Json::boolean() const {
  if (const auto* v = std::get_if<bool>(&v_)) return *v;
  fail("expected true or false");
}
const std::string& Json::string() const {
  if (const auto* v = std::get_if<std::string>(&v_)) return *v;
  fail("expected a string");
}
const Json::Array& Json::items() const {
  if (const auto* v = std::get_if<Array>(&v_)) return *v;
  fail("expected an array");
}
const Json::Object& Json::members() const {
  if (const auto* v = std::get_if<Object>(&v_)) return *v;
  fail("expected an object");
}

const Json* Json::find(std::string_view key) const {
  for (const auto& [k, v] : members())
    if (k == key) return &v;
  return nullptr;
}

const Json& Json::at(std::string_view key) const {
  if (const Json* v = find(key)) return *v;
  fail("missing member \"" + std::string(key) + "\"");
}

const Json& Json::at(std::size_t i) const {
  const Array& a = items();
  if (i >= a.size()) fail("index " + std::to_string(i) + " past the end of an array of " + std::to_string(a.size()));
  return a[i];
}

std::size_t Json::size() const {
  if (const auto* a = std::get_if<Array>(&v_)) return a->size();
  if (const auto* o = std::get_if<Object>(&v_)) return o->size();
  return 0;
}

Json& Json::set(std::string key, Json value) {
  if (isNull()) v_ = Object{};
  auto* o = std::get_if<Object>(&v_);
  if (!o) fail("set on something that is not an object");
  o->emplace_back(std::move(key), std::move(value));
  return *this;
}

Json& Json::push(Json value) {
  if (isNull()) v_ = Array{};
  auto* a = std::get_if<Array>(&v_);
  if (!a) fail("push on something that is not an array");
  a->push_back(std::move(value));
  return *this;
}

void Json::write(std::string& out, int indent) const {
  const std::string pad(static_cast<std::size_t>(indent), ' ');
  const std::string inner(static_cast<std::size_t>(indent + 2), ' ');
  if (isNull()) { out += "null"; return; }
  if (const auto* b = std::get_if<bool>(&v_)) { out += *b ? "true" : "false"; return; }
  if (const auto* i = std::get_if<std::int64_t>(&v_)) { out += std::to_string(*i); return; }
  if (const auto* s = std::get_if<std::string>(&v_)) { writeString(out, *s); return; }
  if (const auto* a = std::get_if<Array>(&v_)) {
    if (a->empty()) { out += "[]"; return; }
    bool allPlain = true;
    for (const Json& e : *a) allPlain = allPlain && plain(e);
    if (allPlain) {
      // As many to a line as fit, which keeps long tables of numbers readable.
      out += '[';
      std::size_t col = static_cast<std::size_t>(indent) + 1;
      for (std::size_t i = 0; i < a->size(); ++i) {
        std::string item;
        (*a)[i].write(item, 0);
        if (i > 0) {
          if (col + item.size() + 2 > kLineWidth) {
            out += ",\n" + inner;
            col = inner.size();
          } else {
            out += ", ";
            col += 2;
          }
        }
        out += item;
        col += item.size();
      }
      out += ']';
      return;
    }
    out += "[\n";
    for (std::size_t i = 0; i < a->size(); ++i) {
      out += inner;
      (*a)[i].write(out, indent + 2);
      out += i + 1 < a->size() ? ",\n" : "\n";
    }
    out += pad + ']';
    return;
  }
  const Object& o = std::get<Object>(v_);
  if (o.empty()) { out += "{}"; return; }
  out += "{\n";
  for (std::size_t i = 0; i < o.size(); ++i) {
    out += inner;
    writeString(out, o[i].first);
    out += ": ";
    o[i].second.write(out, indent + 2);
    out += i + 1 < o.size() ? ",\n" : "\n";
  }
  out += pad + '}';
}

std::string Json::dump() const {
  std::string out;
  write(out, 0);
  out += '\n';
  return out;
}

Json Json::parse(std::string_view text) { return Parser(text).document(); }

Json Json::bytes(const std::vector<u8>& b) { return Json(std::string(b.begin(), b.end())); }

std::vector<u8> Json::toBytes() const {
  const std::string& s = string();
  return std::vector<u8>(s.begin(), s.end());
}

}  // namespace pfr
