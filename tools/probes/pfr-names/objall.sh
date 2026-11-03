#!/bin/bash
# Question: do two trees compile every .cpp of src/engine, and the other files a change touches,
# to the same machine code? Runs objcmp.sh on each file in both trees (two at a time), then
# fncmp.py on each pair, and prints one line a file. The tests' include folder and their one
# definition (the same on both sides) are passed too, so that tests compile; src/game/App.cpp
# needs SDL3's headers (SDL3_INCLUDE, a folder with SDL3/SDL.h).
#   usage: tools/probes/pfr-names/objall.sh <tree before> <tree after> <outdir> [more .cpp ...]
set -u
A=${1:?tree before}; B=${2:?tree after}; OUT=${3:?outdir}
shift 3
P=$(cd "$(dirname "$0")" && pwd)
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
cat > "$OUT/cxx" <<EOF
#!/bin/bash
exec c++ -I"\${OBJ_REPO:?}/tests" ${SDL3_INCLUDE:+-isystem "$SDL3_INCLUDE"} -DENCORE_SOURCE_DIR='"/src"' "\$@"
EOF
chmod +x "$OUT/cxx"
FILES=$( (cd "$A" && ls src/engine/*/*.cpp; cd "$B" && ls src/engine/*/*.cpp; printf '%s\n' "$@") | sort -u)
one() {  # file, side, tree
  OBJ_REPO=$3 CXX=$OUT/cxx "$P/objcmp.sh" "$3" "$OUT/$(echo "$1" | tr / _).$2.txt" "$1" > /dev/null 2> "$OUT/$(echo "$1" | tr / _).$2.err"
}
for f in $FILES; do
  [ -f "$A/$f" ] && one "$f" before "$A" &
  [ -f "$B/$f" ] && one "$f" after "$B" &
  wait
done
for f in $FILES; do
  n=$OUT/$(echo "$f" | tr / _)
  if [ -s "$n.before.txt" ] && [ -s "$n.after.txt" ]; then
    python3 "$P/fncmp.py" "$n.before.txt" "$n.after.txt" > "$n.fncmp.txt"
    printf '%-45s %s\n' "$f" "$(head -1 "$n.fncmp.txt")"
  else
    printf '%-45s %s\n' "$f" "not compiled on both sides (see $n.*.err)"
  fi
done
