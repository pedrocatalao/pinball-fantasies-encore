// Pure-logic tests that need no game files.
#include "Test.h"

#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

#include "core/Bcd.h"
#include "core/File.h"
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
