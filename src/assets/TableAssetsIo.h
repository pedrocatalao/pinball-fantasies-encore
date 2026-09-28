#pragma once
// A table in the open format: a folder holding table.json and the pictures and maps beside
// it, which reads back into exactly the TableAssets it was written from. This is what lets
// the engine run a table without the executable it came out of, and what a table made from
// scratch will be written in.
#include <filesystem>

#include "assets/TableAssets.h"

namespace pfr {

/// Writes `a` into `dir`, which is created if it is not there.
void saveTableAssets(const TableAssets& a, const std::filesystem::path& dir);

/// Reads a table written by saveTableAssets. Throws DataError, naming the file, on anything
/// missing or malformed.
TableAssets loadTableAssets(const std::filesystem::path& dir);

}  // namespace pfr
