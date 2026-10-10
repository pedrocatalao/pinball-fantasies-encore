#include "Test.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "engine/GameDir.h"

namespace test {
std::vector<Case>& registry() { static std::vector<Case> r; return r; }
int& failures() { static int f = 0; return f; }
}  // namespace test

int main() {
  // (with the table files asked for, as CI does, their absence is a failure, not a skip)
  if (std::getenv("ENCORE_REQUIRE_DATA") && !test::haveTables()) {
    std::printf("the game's table files are required (ENCORE_REQUIRE_DATA) but missing or not the supported\n"
                "version at %s:", test::gameDir().string().c_str());
    for (const auto& name : test::missingGameFiles(false)) std::printf(" %s", name.c_str());
    std::printf("\n");
    return 1;
  }
  int failedCases = 0;
  for (const auto& c : test::registry()) {
    const int before = test::failures();
    std::printf("[ RUN  ] %s\n", c.name);
    c.fn();
    if (test::failures() != before) ++failedCases;
    std::printf("[ %s ] %s\n", test::failures() != before ? "FAIL" : " OK ", c.name);
  }
  std::printf("%zu cases, %d failed\n", test::registry().size(), failedCases);
  return failedCases ? 1 : 0;
}
