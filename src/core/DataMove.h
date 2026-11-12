#pragma once
// Everything the game keeps, moved between its two homes: the system's folder for it, and the
// program's own folder (portable: a portable.txt beside the program). Nothing here knows which
// is which: a move is of the game's things from one folder to another, and either all of them
// arrive or nothing of them is left in the target.
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace encore {

/// What the game keeps, by name in its folder: the options and best scores, this version's own
/// flags, the player's online token, the recordings, the HD pictures, the game's files and its
/// log. Anything else in the folder is not the game's, and stays where it is.
inline constexpr std::string_view kMovedItems[] = {
    "PINBALL.CFG", "TABLE1.HI", "TABLE2.HI", "TABLE3.HI", "TABLE4.HI", "fullscreen.txt", "crt.txt",
    "hd.txt", "sfx.txt", "dotmatrix.txt", "trail.txt", "release-offered.txt", "downloaded.txt",
    "online.txt", "art", "replays", "FANTASY", "log.txt", "log-previous.txt"};

/// The items' size in bytes, of those that are there.
std::uintmax_t dataSize(const std::filesystem::path& from, std::span<const std::string_view> items = kMovedItems);
/// Whether a file can be made in the folder (which is made if need be).
bool writable(const std::filesystem::path& dir);
/// The space free on the folder's volume; nothing when that cannot be known.
std::optional<std::uintmax_t> freeSpace(const std::filesystem::path& dir);

struct MoveReport {
  bool ok = false;
  std::string step;  ///< what was being done when it failed
  std::string why;   ///< and what went wrong
};

/// Moves the items from one folder to the other. They are copied into a staging folder in the
/// target first and checked against the source; then put in their places, with whatever the
/// target had of the same names put aside; then `commit` is called, to turn whatever says
/// where the game's things are; and only then are they removed from the source. Anything
/// failing before the commit is undone: the target is left as it was, the source was never
/// touched. `commit` returns what went wrong, or an empty string. `progress` is told the bytes
/// copied so far.
MoveReport moveData(const std::filesystem::path& from, const std::filesystem::path& to,
                    const std::function<std::string()>& commit,
                    const std::function<void(std::uintmax_t)>& progress = {},
                    std::span<const std::string_view> items = kMovedItems);

/// Removes what a move left in the folder if the program ended in the middle of one. (The
/// source is whole until the commit, and after it the target is: nothing is lost either way.)
void discardStaleMove(const std::filesystem::path& dir);

}  // namespace encore
