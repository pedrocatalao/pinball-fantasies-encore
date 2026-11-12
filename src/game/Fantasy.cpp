#include "game/Fantasy.h"

#include <SDL3/SDL.h>
#include <atomic>
#include <cctype>
#include <chrono>
#include <string>
#include <thread>
#include <system_error>
#include <vector>

#include "core/File.h"
#include "core/Log.h"
#include "core/Zip.h"
#include "game/Online.h"
#include "platform/Http.h"

namespace encore {
namespace {

/// An archive says where each file goes, so it decides where the writing happens: anything
/// reaching outside the folder it is being unpacked into is dropped, as are the entries a
/// Mac adds to a zip of its own accord.
bool safeName(const std::string& name) {
  if (name.empty() || name.front() == '/' || name.find('\\') != std::string::npos) return false;
  if (name.size() > 1 && name[1] == ':') return false;
  std::size_t at = 0;
  while (at <= name.size()) {
    const std::size_t slash = name.find('/', at);
    const std::string part = name.substr(at, slash == std::string::npos ? std::string::npos : slash - at);
    if (part == ".." || part == "." || part.empty()) return false;
    if (part == "__MACOSX" || part.front() == '.') return false;
    if (slash == std::string::npos) break;
    at = slash + 1;
  }
  return true;
}

/// What a finished download leaves behind, holding the address the archive came from. Once it
/// is there the server is not asked again, so moving the archive to another address does not
/// fetch it a second time.
constexpr const char* kDoneFile = "downloaded.txt";

bool alreadyHere(const std::filesystem::path& into) {
  std::error_code ec;
  return std::filesystem::exists(into / kDoneFile, ec);
}

/// The archive's address is the server's to say, so that it can move without a new release.
std::optional<std::string> archiveUrl(std::string& error) {
  const auto body = httpDownload(onlineApi() + "/v1/fantasy", &error);
  if (!body) return std::nullopt;
  std::string url(body->begin(), body->end());
  while (!url.empty() && std::isspace(static_cast<unsigned char>(url.back()))) url.pop_back();
  if (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0) {
    error = "the server gave no address";
    return std::nullopt;
  }
  return url;
}

/// How many times the archive is asked for before the game gives up on it, and how long it
/// waits before each try after the first. The archive is on a host of someone else's, which
/// some connections reach only now and then.
constexpr int kAttempts = 5;
constexpr int kPauseSeconds[kAttempts - 1] = {2, 4, 8, 16};

/// One try: the address from the server, the archive from there, and what it holds. Nothing
/// when any of the three fails, with the reason in `error`.
std::vector<ZipEntry> fetchOnce(std::string& url, std::string& error) {
  const auto where = archiveUrl(error);
  if (!where) return {};
  url = *where;
  log::info("fetching " + url);
  const auto body = httpDownload(url, &error);
  if (!body) return {};
  log::info("fetched " + std::to_string(body->size() / 1024 / 1024) + " MB; unpacking");
  auto entries = readZip(*body);
  if (entries.empty()) error = "the archive holds nothing this reader can unpack";
  return entries;
}

/// The fetching happens on a thread of its own so that the caller can keep drawing, and is
/// tried again, after a pause, while it fails. Closing the window stops it: no pause is waited
/// out and no try is begun after that, though one under way runs to its end (`closed`).
std::vector<ZipEntry> fetchWhileWaiting(std::string& url, std::string& error, const FantasyWaitTick& tick, bool& closed) {
  std::vector<ZipEntry> entries;
  std::atomic<bool> done{false}, stop{false};
  std::thread worker([&] {
    for (int attempt = 1; attempt <= kAttempts && !stop; ++attempt) {
      entries = fetchOnce(url, error);
      if (!entries.empty() || attempt == kAttempts) break;
      const int pause = kPauseSeconds[attempt - 1];
      log::warn("the archive could not be fetched (" + error + "); trying again in " + std::to_string(pause) + " s");
      for (int waited = 0; waited < pause * 10 && !stop; ++waited) std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    done.store(true, std::memory_order_release);
  });
  const u64 start = SDL_GetTicks();
  while (!done.load(std::memory_order_acquire)) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) stop = true;
    }
    if (tick) tick(static_cast<double>(SDL_GetTicks() - start) / 1000.0);
    SDL_Delay(16);
  }
  worker.join();
  closed = stop;
  return entries;
}

}  // namespace

bool downloadFantasyOnce(const std::filesystem::path& into, const FantasyAsk& ask, const FantasyWaitTick& tick) {
  std::error_code ec;
  if (alreadyHere(into)) return true;
  if (!ask || !ask()) {
    log::info("the download was not wanted; leaving");
    return false;
  }

  std::string url, error;
  bool closed = false;
  const auto entries = fetchWhileWaiting(url, error, tick, closed);
  if (closed) {
    log::info("the window was closed during the download; leaving");
    return false;
  }
  if (entries.empty()) {
    log::error("the archive could not be fetched after " + std::to_string(kAttempts) + " tries: " + error);
    return true;
  }
  // Whatever the archive holds, where it says to put it, under the folder given.
  std::filesystem::create_directories(into, ec);
  int written = 0;
  for (const auto& entry : entries) {
    if (!safeName(entry.name)) continue;
    const std::filesystem::path path = into / entry.name;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (!file::writeAll(path, entry.data)) {
      log::error("cannot write " + path.string());
      return true;
    }
    ++written;
  }
  log::info(std::to_string(written) + " files unpacked into " + into.string());
  // Only now, with everything written: a run that stops halfway asks again next time.
  const std::string done = url + "\n";
  file::writeAll(into / kDoneFile, ByteView(reinterpret_cast<const u8*>(done.data()), done.size()));
  return true;
}

}  // namespace encore
