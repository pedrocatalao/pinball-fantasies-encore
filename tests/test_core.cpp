// Pure-logic tests that need no game files.
#include "Test.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "core/Bcd.h"
#include "core/DataMove.h"
#include "core/File.h"
#include "core/Log.h"
#include "core/Sha256.h"

using namespace encore;

TEST(bcd_arithmetic) {
  CHECK(Bcd::of("999") + Bcd::of("1") == Bcd::of("1000"));
  CHECK(Bcd::of("1250") * 4 == Bcd::of("5000"));
  CHECK(Bcd::of("123").leadingZeros() == 9);
  CHECK(Bcd::kZero.isZero());
  const auto a = Bcd::of("1030060").toAscii();
  CHECK(std::string(a.begin(), a.end()) == "     1030060");
  const auto z = Bcd::kZero.toAscii();
  CHECK(std::string(z.begin(), z.end()) == "           0");
  CHECK(Bcd::of("50000000") > Bcd::of("25000000"));
}

TEST(sha256_known_vectors) {
  const std::string abc = "abc";
  CHECK_EQ(sha256Hex(ByteView(reinterpret_cast<const u8*>(abc.data()), abc.size())),
           std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
  CHECK_EQ(sha256Hex(ByteView()), std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

// SDL gives folders, dropped files and the command line as UTF-8 text, and the game makes paths
// of it as text: a file named so is the file named so in UTF-8. On Windows that holds only with
// the program's manifest (packaging/windows/utf8.manifest); without it the text is read in the
// language's code page, and C:\Users\José is not found.
TEST(utf8_text_names_the_file_it_says) {
  const std::string name = "Jos\xc3\xa9-\xc3\xb1-\xe6\xb5\x8b\xe8\xaf\x95";  // José-ñ-测试
  const std::u8string name8(reinterpret_cast<const char8_t*>(name.data()), name.size());
  std::error_code ec;
  const auto dir = std::filesystem::temp_directory_path() / "encore-utf8-test";
  std::filesystem::create_directories(dir, ec);
  const std::filesystem::path fromText = dir / std::filesystem::path(name.c_str());
  CHECK(fromText == dir / std::filesystem::path(name8));
  const std::string body = "hello";
  CHECK(file::writeAll(fromText, ByteView(reinterpret_cast<const u8*>(body.data()), body.size())));
  CHECK(std::filesystem::exists(dir / std::filesystem::path(name8)));
  std::filesystem::remove_all(dir, ec);
}

// --- the game's things moved between their two homes (core/DataMove.h)

namespace {

namespace fs = std::filesystem;

/// A folder with the game's things in it, in little: files, folders and one that is not the
/// game's (and stays).
void makeTree(const fs::path& dir) {
  std::error_code ec;
  fs::remove_all(dir, ec);
  for (const char* sub : {"art/1", "replays/outbox", "FANTASY", "backup"}) fs::create_directories(dir / sub, ec);
  const auto put = [&](const char* name, std::size_t size) {
    const std::vector<u8> bytes(size, static_cast<u8>(size));
    CHECK(file::writeAll(dir / name, ByteView(bytes.data(), bytes.size())));
  };
  put("PINBALL.CFG", 6);
  put("table1.hi", 64);  // (as the DOS game would leave it, in its own case)
  put("online.txt", 65);
  put("art/1/x.png", 3000);
  put("art/set.txt", 10);
  put("replays/outbox/a.rpl", 700);
  put("FANTASY/TABLE1.PRG", 5000);
  put("backup/keep.txt", 7);
}

/// Every file under a folder, by its path within it, with its size.
std::map<std::string, std::uintmax_t> snapshot(const fs::path& dir) {
  std::map<std::string, std::uintmax_t> files;
  std::error_code ec;
  if (!fs::exists(dir, ec)) return files;
  for (const auto& entry : fs::recursive_directory_iterator(dir, ec))
    if (entry.is_regular_file(ec)) files[entry.path().lexically_relative(dir).generic_string()] = entry.file_size(ec);
  return files;
}

const fs::path kFrom = fs::temp_directory_path() / "encore-move-from", kTo = fs::temp_directory_path() / "encore-move-to";
const std::map<std::string, std::uintmax_t> kGameFiles = {
    {"PINBALL.CFG", 6}, {"table1.hi", 64}, {"online.txt", 65}, {"art/1/x.png", 3000}, {"art/set.txt", 10},
    {"replays/outbox/a.rpl", 700}, {"FANTASY/TABLE1.PRG", 5000}};

void cleanUp() {
  std::error_code ec;
  fs::remove_all(kFrom, ec);
  fs::remove_all(kTo, ec);
}

}  // namespace

TEST(the_games_things_move_there_and_back) {
  makeTree(kFrom);
  CHECK_EQ(dataSize(kFrom), std::uintmax_t{6 + 64 + 65 + 3000 + 10 + 700 + 5000});
  int commits = 0;
  std::uintmax_t told = 0;
  const MoveReport there = moveData(kFrom, kTo, [&] { ++commits; return std::string(); }, [&](std::uintmax_t done) { told = done; });
  CHECK(there.ok);
  CHECK_EQ(commits, 1);
  CHECK_EQ(told, dataSize(kTo));
  CHECK(snapshot(kTo) == kGameFiles);
  CHECK((snapshot(kFrom) == std::map<std::string, std::uintmax_t>{{"backup/keep.txt", 7}}));  // (not the game's: stays)
  std::error_code ec;
  CHECK(!fs::exists(kTo / ".encore-move", ec));
  const MoveReport back = moveData(kTo, kFrom, [&] { ++commits; return std::string(); });
  CHECK(back.ok);
  CHECK_EQ(commits, 2);
  auto expected = kGameFiles;
  expected["backup/keep.txt"] = 7;
  CHECK(snapshot(kFrom) == expected);
  CHECK(snapshot(kTo).empty());
  cleanUp();
}

TEST(what_the_target_had_of_the_same_names_is_replaced) {
  makeTree(kFrom);
  std::error_code ec;
  fs::create_directories(kTo / "art" / "9", ec);
  const std::vector<u8> old(2, 1);
  CHECK(file::writeAll(kTo / "PINBALL.CFG", ByteView(old.data(), old.size())));
  CHECK(file::writeAll(kTo / "art" / "9" / "old.png", ByteView(old.data(), old.size())));
  CHECK(moveData(kFrom, kTo, [] { return std::string(); }).ok);
  CHECK(snapshot(kTo) == kGameFiles);
  cleanUp();
}

TEST(a_move_that_cannot_begin_leaves_the_source_as_it_was) {
  makeTree(kFrom);
  const auto before = snapshot(kFrom);
  // (a file where the target folder has to be made)
  const std::vector<u8> old(2, 1);
  CHECK(file::writeAll(kTo, ByteView(old.data(), old.size())));
  int commits = 0;
  const MoveReport r = moveData(kFrom, kTo, [&] { ++commits; return std::string(); });
  CHECK(!r.ok);
  CHECK(!r.step.empty());
  CHECK_EQ(commits, 0);
  CHECK(snapshot(kFrom) == before);
  std::error_code ec;
  CHECK(fs::is_regular_file(kTo, ec));
  cleanUp();
}

TEST(a_commit_that_fails_puts_the_target_back) {
  makeTree(kFrom);
  const auto before = snapshot(kFrom);
  std::error_code ec;
  fs::create_directories(kTo / "art" / "9", ec);
  const std::vector<u8> old(2, 1);
  CHECK(file::writeAll(kTo / "PINBALL.CFG", ByteView(old.data(), old.size())));
  CHECK(file::writeAll(kTo / "art" / "9" / "old.png", ByteView(old.data(), old.size())));
  const auto target = snapshot(kTo);
  const MoveReport r = moveData(kFrom, kTo, [] { return std::string("no marker today"); });
  CHECK(!r.ok);
  CHECK_EQ(r.why, std::string("no marker today"));
  CHECK(snapshot(kFrom) == before);
  CHECK(snapshot(kTo) == target);
  CHECK(!fs::exists(kTo / ".encore-move", ec));
  cleanUp();
}

TEST(a_move_left_half_done_is_cleared_away) {
  std::error_code ec;
  fs::create_directories(kTo / ".encore-move" / "new" / "art", ec);
  const std::vector<u8> some(3, 1);
  CHECK(file::writeAll(kTo / ".encore-move" / "new" / "art" / "x.png", ByteView(some.data(), some.size())));
  CHECK(file::writeAll(kTo / "PINBALL.CFG", ByteView(some.data(), some.size())));
  discardStaleMove(kTo);
  CHECK(!fs::exists(kTo / ".encore-move", ec));
  CHECK((snapshot(kTo) == std::map<std::string, std::uintmax_t>{{"PINBALL.CFG", 3}}));
  discardStaleMove(kTo);  // (nothing there: nothing happens)
  cleanUp();
}

TEST(sizes_and_writability_are_told_as_they_are) {
  std::error_code ec;
  fs::create_directories(kFrom, ec);
  const std::vector<u8> some(5, 1);
  CHECK(file::writeAll(kFrom / "sfx.txt", ByteView(some.data(), some.size())));
  CHECK_EQ(dataSize(kFrom), std::uintmax_t{5});  // (the rest of the items are not there)
  CHECK(writable(kFrom));
  CHECK(writable(kTo));  // (made)
  CHECK(!writable(kFrom / "sfx.txt" / "under-a-file"));
  CHECK(freeSpace(kFrom).has_value());
  cleanUp();
}

// --- the log kept in a file as well (core/Log.h)

TEST(the_log_file_has_every_line_including_those_before_it) {
  std::error_code ec;
  const auto dir = std::filesystem::temp_directory_path() / "encore-log-test";
  std::filesystem::remove_all(dir, ec);
  std::filesystem::create_directories(dir, ec);
  auto text = [](const std::filesystem::path& path) {
    const auto bytes = file::readAll(path);
    return bytes ? std::string(bytes->begin(), bytes->end()) : std::string();
  };
  log::info("before the file");
  log::toFile(dir / "log.txt");
  log::warn("in the file");
  log::toFile({});
  log::error("while it moves");
  log::toFile(dir / "log.txt");  // (added to, not written over)
  log::toFile({});
  const std::string kept = text(dir / "log.txt");
  // (with whatever the tests before this one logged, held from before there was a file)
  const std::string expected = "[info ] before the file\n[warn ] in the file\n[error] while it moves\n";
  CHECK(kept.size() >= expected.size() && kept.compare(kept.size() - expected.size(), expected.size(), expected) == 0);
  std::filesystem::remove_all(dir, ec);
}
