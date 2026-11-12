#pragma once
#include <filesystem>
#include <string_view>

namespace encore::log {

enum class Level { Debug, Info, Warn, Error };

void setMinimumLevel(Level level);
void write(Level level, std::string_view message);

/// Also into a file from now on, added to its end. What was written while there was no file
/// goes in first, so the lines from before the file was known are there too. An empty path
/// closes the file, and what follows is held for the next one.
void toFile(const std::filesystem::path& path);

void debug(std::string_view message);
void info(std::string_view message);
void warn(std::string_view message);
void error(std::string_view message);

}  // namespace encore::log
