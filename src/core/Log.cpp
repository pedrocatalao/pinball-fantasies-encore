#include "core/Log.h"

#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>

namespace encore::log {
namespace {
Level g_minimum = Level::Info;
std::mutex g_mutex;
/// The file, when there is one; and while there is none, the lines it is to start with. A
/// stream rather than stdio, which would need the path narrowed and lose names on Windows.
std::ofstream g_file;
std::string g_held;
std::size_t g_written = 0;
/// Enough for hours of play with --stats; a runaway is cut short, not left to fill the disk.
constexpr std::size_t kMostInFile = 4u << 20;
constexpr std::size_t kMostHeld = 64u << 10;
const char* prefix(Level level) {
  switch (level) {
    case Level::Debug: return "debug";
    case Level::Info: return "info ";
    case Level::Warn: return "warn ";
    case Level::Error: return "error";
  }
  return "?";
}
void toFileLocked(const std::string& line) {
  if (!g_file.is_open()) {
    if (g_held.size() + line.size() <= kMostHeld) g_held += line;
    return;
  }
  if (g_written > kMostInFile) return;
  g_written += line.size();
  if (g_written > kMostInFile) g_file << "[warn ] the log is long enough; the rest is not kept\n";
  else g_file << line;
  g_file.flush();  // a crash keeps every line up to it
}
}  // namespace

void setMinimumLevel(Level level) { g_minimum = level; }

void write(Level level, std::string_view message) {
  if (level < g_minimum) return;
  std::scoped_lock lock(g_mutex);
  std::fprintf(stderr, "[%s] %.*s\n", prefix(level), static_cast<int>(message.size()), message.data());
  toFileLocked("[" + std::string(prefix(level)) + "] " + std::string(message) + "\n");
}

void toFile(const std::filesystem::path& path) {
  std::scoped_lock lock(g_mutex);
  if (g_file.is_open()) g_file.close();
  g_written = 0;
  if (path.empty()) return;
  g_file.open(path, std::ios::binary | std::ios::app);
  if (!g_file) {
    std::fprintf(stderr, "[warn ] the log cannot be kept in %s\n", path.string().c_str());
    return;
  }
  const std::string held = std::move(g_held);
  g_held.clear();
  toFileLocked(held);
}

void debug(std::string_view m) { write(Level::Debug, m); }
void info(std::string_view m) { write(Level::Info, m); }
void warn(std::string_view m) { write(Level::Warn, m); }
void error(std::string_view m) { write(Level::Error, m); }

}  // namespace encore::log
