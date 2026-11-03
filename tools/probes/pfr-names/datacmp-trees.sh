#!/bin/bash
# Question: do files outside src/ (the referee's, tools/oracle/*.cpp) hold the same data sections
# (.rodata: jump tables, constants; .data) in two trees? datacmp.sh does this for src/ against a
# git ref; this takes two checked-out trees instead, so that it reaches tools/.
#   usage: datacmp-trees.sh <tree before> <tree after> <file.cpp>...
A=$(cd "${1:?tree before}" && pwd); B=$(cd "${2:?tree after}" && pwd); shift 2
S=$(mktemp -d)
status=0
for f in "$@"; do
  for side in A B; do
    root=$A; [ $side = B ] && root=$B
    "${CXX:-c++}" -I"$root/src" -I"$root/tools/oracle" -O2 -DNDEBUG -std=c++20 -c "$root/$f" -o "$S/$side.o" || exit 1
    objdump -s "$S/$side.o" | grep -v "file format" \
      | awk '/^Contents of section/{keep = ($4 !~ /^\.text/ && $4 !~ /debug|eh_frame|comment|note/)} keep' > "$S/$side.txt"
  done
  if cmp -s "$S/A.txt" "$S/B.txt"; then echo "SAME   $f ($(wc -l < "$S/B.txt") lines of data)"; else echo "DIFFER $f"; status=1; fi
done
rm -rf -- "${S:?}"
exit $status
