#!/bin/bash
# Question: do a renamed file's data sections (.rodata, .data: tables of constants) hold the same
# bytes as at <ref>? objcmp.sh compares only the code. Compiles each file as it is at <ref> (git
# archive) and as it is now, and compares objdump -s of every section but the code's.
#   usage: tools/probes/pfr-names/datacmp.sh <repo> <ref> <src/file.cpp>...
R=$(cd "${1:?repo}" && pwd); REF=${2:?ref}; shift 2
S=$(mktemp -d)
mkdir -p "$S/ref"
git -C "$R" archive "$REF" src | tar -x -C "$S/ref"
status=0
for f in "$@"; do
  for side in ref now; do
    root=$R; [ $side = ref ] && root=$S/ref
    "${CXX:-c++}" -I"$root/src" -O2 -DNDEBUG -std=c++20 -c "$root/$f" -o "$S/$side.o" || exit 1
    objdump -s "$S/$side.o" | grep -v "file format" \
      | awk '/^Contents of section/{keep = ($4 !~ /^\.text/ && $4 !~ /debug|eh_frame|comment|note/)} keep' > "$S/$side.txt"
  done
  if cmp -s "$S/ref.txt" "$S/now.txt"; then echo "SAME   $f ($(wc -l < "$S/now.txt") lines of data)"; else echo "DIFFER $f"; status=1; fi
done
rm -rf -- "${S:?}"
exit $status
