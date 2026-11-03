#!/bin/bash
# Question: does a source file (by default StonesNBones.cpp) compile to the same machine code
# as before a rename? Compiles it as the build does (-O2, without -g) and writes its disassembly with every symbol
# name, address, raw byte and PC-relative offset taken out, so that renamed routines, and named
# constants that fold to the same numbers, compare equal. Compare two outputs with fncmp.py.
# Use the same compiler for both sides.
#   usage: tools/probes/pfr-names/objcmp.sh <repo> <out.txt> [src/...cpp]   (CXX picks the compiler)
REPO=${1:?repo}
OUT=${2:?out.txt}
SRC=${3:-src/engine/table/StonesNBones.cpp}
O=$(mktemp -d)
"${CXX:-c++}" -I"$REPO/src" -O2 -DNDEBUG -std=c++20 -c "$REPO/$SRC" -o "$O/s.o" || exit 1
objdump -d --no-show-raw-insn -r "$O/s.o" \
  | sed -E 's/^[0-9a-f]+ <.*>:$/<fn>:/; s/^ +[0-9a-f]+:\t//; s/<[^>]*>//g; s/R_X86_64_[A-Z0-9_]+\t.*/RELOC/; s/^\s+[0-9a-f]+: RELOC/RELOC/; s/(call|jmp|j[a-z]+) +[0-9a-f]+ *$/\1 X/' \
  | sed -E 's/-?0x[0-9a-f]+\(%rip\)/X(%rip)/; s/ +# .*$//' > "$OUT"
rm -rf -- "${O:?}"
wc -l "$OUT"
