#!/bin/bash
# Question: does a file that includes a renamed header compile to the same object code with the
# header as it was at <ref> as with the header now? Prints IDENTICAL if so. (For the files that
# include a table's header without being renamed themselves: TableGame.cpp, tools/oracle/table.cpp.)
#   usage: tools/probes/pfr-names/hdrcmp.sh <repo> <ref> <header, from src/> <file.cpp> [-I...]
#   e.g.   tools/probes/pfr-names/hdrcmp.sh $PWD main engine/table/SpeedDevils.h tools/oracle/table.cpp -Itools/oracle
set -e
R=${1:?repo}
REF=${2:?ref}
H=${3:?header}
SRC=${4:?file.cpp}
shift 4
O=$(mktemp -d)
cd "$R"
mkdir -p "$O/old/$(dirname "$H")"
git show "$REF:src/$H" > "$O/old/$H"
"${CXX:-c++}" -I"$O/old" -Isrc "$@" -O2 -DNDEBUG -std=c++20 -c "$SRC" -o "$O/h0.o"
"${CXX:-c++}" -Isrc "$@" -O2 -DNDEBUG -std=c++20 -c "$SRC" -o "$O/h1.o"
objdump -d --no-show-raw-insn -r "$O/h0.o" | tail -n +4 > "$O/h0.txt"
objdump -d --no-show-raw-insn -r "$O/h1.o" | tail -n +4 > "$O/h1.txt"
status=0
cmp -s "$O/h0.txt" "$O/h1.txt" && echo "$SRC IDENTICAL ($(wc -l < "$O/h0.txt") lines)" || { echo "$SRC DIFFERS"; status=1; }
rm -rf -- "${O:?}"
exit $status
