#!/bin/bash
# Question: with the engine's numbers turned into names, does re/gensymbols.py still write the
# same Names.inc and Maps.inc? Regenerates everything (re/disasm.py, re/align.py, gensymbols.py)
# in a worktree of <ref>, keeps the two files, copies this repository's re/, Names.inc and the
# engine's sources over the worktree, regenerates again, and compares. (Against <ref>'s own
# regeneration, not the committed Maps.inc: nasm 2.16.03 does not give the committed one.)
# Needs ndisasm on PATH (or NASM_BIN: its folder) and the game's files in DATA.
#   usage: tools/probes/pfr-names/genproof.sh <repo> <ref>
set -u
R=$(cd "${1:?repo}" && pwd)
REF=${2:?ref}
DATA=${DATA:-$R/build/data}
[ -n "${NASM_BIN:-}" ] && export PATH="$NASM_BIN:$PATH"
command -v ndisasm >/dev/null || { echo "no ndisasm (nasm)"; exit 2; }
T=$(mktemp -d)
git -C "$R" worktree add -q --detach "$T/w" "$REF" || exit 1
regen() {
  (cd "$T/w" && python3 re/disasm.py "$DATA" >/dev/null && for t in 2 3 4; do python3 re/align.py 1 "$t" >/dev/null || exit 1; done &&
     python3 re/gensymbols.py) || exit 1
}
regen
cp "$T/w/src/engine/table/Maps.inc" "$T/Maps-1.inc"
cp "$R"/re/*.py "$T/w/re/"
cp "$R/re/symbols/table1.txt" "$T/w/re/symbols/"
cp "$R"/src/engine/table/Engine*.cpp "$R"/src/engine/table/Engine.h "$R"/src/engine/table/Flow.* "$R/src/engine/table/Names.inc" "$T/w/src/engine/table/"
regen
cmp -s "$R/src/engine/table/Names.inc" "$T/w/src/engine/table/Names.inc" && echo "Names.inc: as gensymbols.py writes it" || echo "Names.inc: DIFFERS from what gensymbols.py writes"
if cmp -s "$T/Maps-1.inc" "$T/w/src/engine/table/Maps.inc"; then echo "Maps.inc: the same"; else echo "Maps.inc differs:"; diff "$T/Maps-1.inc" "$T/w/src/engine/table/Maps.inc"; fi
git -C "$R" worktree remove --force "$T/w"
rm -rf -- "${T:?}"
