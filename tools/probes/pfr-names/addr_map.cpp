// Question: for each of a table's own data addresses, which Party Land address is it (the
// engine's map, ffff for none; Names.inc names the Party Land ones), and what bytes start there
// (shown also as the display's characters, 0x37 being the digit 0)?
//   usage: addr_map <TABLEn.PRG> <n 1-4> <hex addr>...
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>
#include "engine/table/Program.h"
using namespace encore;
int main(int argc, char** argv) {
  std::ifstream f(argv[1], std::ios::binary);
  std::vector<u8> prg((std::istreambuf_iterator<char>(f)), {});
  Program p(prg, std::atoi(argv[2]) - 1);
  for (int i = 3; i < argc; ++i) {
    const u16 a = static_cast<u16>(std::strtoul(argv[i], nullptr, 16));
    std::printf("%04x PL=%04x  ", a, p.partyLandData(a));
    for (int k = 0; k < 12; ++k) std::printf("%02x ", p.nativeB(static_cast<u16>(a + k)));
    std::printf(" |");
    for (int k = 0; k < 12; ++k) { u8 c = p.nativeB(static_cast<u16>(a + k)); std::putchar(c >= 0x37 && c <= 0x40 ? '0' + c - 0x37 : (c >= 0x20 && c < 0x7f ? c : '.')); }
    std::printf("|\n");
  }
}
