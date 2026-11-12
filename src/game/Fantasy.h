#pragma once
// A trial run at files that do not travel with the application: the first time the game
// starts, an archive is fetched and unpacked, whatever it holds, into the folder this
// version keeps its own things in (settings and high scores), wherever the platform puts it.
#include <filesystem>
#include <functional>

namespace encore {

/// Offers the download on the first start and fetches it if it is wanted, unpacking
/// everything the archive holds into `into`. False when the offer was turned down, or the
/// window was closed while it was being fetched, which ends the game; true to carry on -- the files being there already, or fetched now,
/// or the fetching having failed, in which case the game runs with what it has.
/// Puts the offer to whoever is playing and waits for the answer. The game draws this in
/// its own window.
using FantasyAsk = std::function<bool()>;

/// One turn of whatever the caller shows while the fetching runs, given the seconds since
/// it started. Called about sixty times a second.
using FantasyWaitTick = std::function<void(double seconds)>;

bool downloadFantasyOnce(const std::filesystem::path& into, const FantasyAsk& ask, const FantasyWaitTick& tick);

}  // namespace encore
