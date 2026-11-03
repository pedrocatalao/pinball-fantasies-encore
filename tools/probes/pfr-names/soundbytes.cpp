// Question: where TableSound.cpp's addresses and pfr's (assets/table/sound.rs) differ, what bytes
// are at each in the table's data segment?
//   usage: g++ -std=c++20 -O1 -Isrc tools/probes/pfr-names/soundbytes.cpp build/libencore_engine.a build/libencore_core.a -o build/soundbytes && build/soundbytes "$DATA"
#include <cstdio>
#include <string>

#include "core/File.h"
#include "engine/data/TableData.h"

using namespace encore;

int main(int argc, char** argv) {
  struct Q { int table; unsigned at; const char* what; };
  const Q qs[] = {
      {2, 0x0a59, "ours matchStart"}, {2, 0x0a62, "pfr MatchStart"},
      {2, 0x0a5c, "ours matchWin"},   {2, 0x0a65, "pfr MatchWin"},
      {3, 0x08cf, "ours silence"},    {3, 0x08ad, "pfr Silence"},
      {4, 0x090a, "ours matchStart"}, {4, 0x0910, "pfr MatchStart"},
      {4, 0x090d, "ours matchWin = pfr MatchWin"},
      {4, 0x08b4, "ours raiseTargets (pfr: none)"},
      {1, 0x0c6c, "table 1 silence"},
  };
  for (const Q& q : qs) {
    auto prg = file::findCaseInsensitive(argv[1], "TABLE" + std::to_string(q.table) + ".PRG");
    TableData t = TableData::load(*prg, q.table - 1);
    std::printf("table %d %04x %-32s", q.table, q.at, q.what);
    for (unsigned i = 0; i < 4; ++i) std::printf(" %02x", t.dataSegment[q.at + i]);
    std::printf("\n");
  }
}
