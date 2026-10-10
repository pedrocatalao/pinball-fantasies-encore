#include "game/Online.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <random>
#include <string>
#include <system_error>
#include <vector>

#include "core/File.h"
#include "core/Log.h"
#include "game/Release.h"
#include "platform/Http.h"

namespace encore {

std::string onlineApi() {
  if (const char* a = std::getenv("ENCORE_API")) return a;
  return "https://thebestpinball.com";
}

namespace {

/// This installation's secret, made the first time it is wanted.
std::string token(const std::filesystem::path& saveDir) {
  const auto path = saveDir / "online.txt";
  if (const auto kept = file::readAll(path); kept && kept->size() >= 64) {
    std::string t(kept->begin(), kept->begin() + 64);
    if (std::all_of(t.begin(), t.end(), [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); })) return t;
  }
  std::random_device random;
  std::string t;
  static constexpr char kHex[] = "0123456789abcdef";
  for (int i = 0; i < 64; ++i) t += kHex[random() % 16];
  file::writeAll(path, ByteView(reinterpret_cast<const u8*>(t.data()), t.size()));
  return t;
}

/// A recording, by its name: .RPL, in any case.
bool isRecording(const std::filesystem::path& p) {
  std::string ext = p.extension().string();
  for (char& c : ext) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return ext == ".RPL";
}

/// A field of the server's small JSON answers, as text.
std::string field(const Bytes& body, const std::string& name) {
  const std::string s(body.begin(), body.end()), key = "\"" + name + "\":\"";
  const auto at = s.find(key);
  if (at == std::string::npos) return {};
  const auto end = s.find('"', at + key.size());
  return end == std::string::npos ? std::string() : s.substr(at + key.size(), end - at - key.size());
}

/// Sends the outbox, oldest first. Stops at the first game the server cannot take now, to try
/// again later; a game it refuses outright is let go (its recording stays in replays/).
void sendAll(const std::filesystem::path& saveDir) {
  const auto outbox = saveDir / "replays" / "outbox";
  std::error_code ec;
  std::vector<std::filesystem::path> waiting;
  for (const auto& e : std::filesystem::directory_iterator(outbox, ec))
    if (isRecording(e.path())) waiting.push_back(e.path());
  if (waiting.empty()) return;
  std::sort(waiting.begin(), waiting.end());
  const std::string url = onlineApi() + "/v1/runs", auth = "Authorization: Bearer " + token(saveDir);
  // (which version played it, kept beside the game on the server: "dev" for a build of no release)
  const std::string version = "X-Encore-Version: " + std::string(thisRelease().empty() ? "dev" : thisRelease());
  for (const auto& path : waiting) {
    const auto data = file::readAll(path);
    if (!data) continue;
    std::string error;
    const auto reply = httpPost(url, *data, {auth, version, "Content-Type: application/octet-stream"}, &error);
    const std::string name = path.filename().string();
    if (!reply) {
      log::info("online scores: " + name + " not sent (" + error + "); it will be tried again");
      return;
    }
    if (reply->status >= 200 && reply->status < 300) {
      log::info("online scores: " + name + " sent, as " + field(reply->body, "tag") + "; " +
                field(reply->body, "status"));
      std::filesystem::remove(path, ec);
    } else if (reply->status == 429 || reply->status >= 500) {
      log::info("online scores: " + name + " not taken now (" + std::to_string(reply->status) + "); later");
      return;
    } else {
      log::info("online scores: " + name + " refused: " + field(reply->body, "error"));
      std::filesystem::remove(path, ec);
    }
  }
}

}  // namespace

ScoreSender::ScoreSender(std::filesystem::path saveDir) : saveDir_(std::move(saveDir)) {}

ScoreSender::~ScoreSender() {
  // A game half sent when the window closes stays in the outbox for next time, so the thread
  // is only waited for if it has finished; otherwise it is left to end with the program.
  if (!thread_.joinable()) return;
  if (busy_->load())
    thread_.detach();
  else
    thread_.join();
}

void ScoreSender::queue(const std::filesystem::path& recording) {
  const auto outbox = saveDir_ / "replays" / "outbox";
  std::error_code ec;
  std::filesystem::create_directories(outbox, ec);
  std::filesystem::copy_file(recording, outbox / recording.filename(), std::filesystem::copy_options::overwrite_existing,
                             ec);
  if (ec) log::error("online scores: cannot keep " + recording.filename().string() + " to send: " + ec.message());
}

void ScoreSender::send() {
  if (busy_->exchange(true)) return;
  if (thread_.joinable()) thread_.join();
  // Only copies go to the thread, so it may outlive this object.
  thread_ = std::thread([dir = saveDir_, busy = busy_] {
    sendAll(dir);
    busy->store(false);
  });
}

}  // namespace encore
