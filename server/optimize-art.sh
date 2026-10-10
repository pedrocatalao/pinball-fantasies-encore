#!/bin/bash
# optimize-art.sh — copies a folder of HD pictures (assets/hd by default) into another, each
# made smaller without changing a pixel of it, for publish-art.sh to publish: what every
# game downloads is then smaller, and the pictures kept in the repository stay as drawn.
#
# oxipng packs each picture tighter, keeping it to what the game reads (8 bits a channel, not
# interlaced), and check-png.py then compares it with the original: a copy the game could not
# read, or that shows anything differently, is replaced by the original. font.png goes as it
# is: the letters of the questions asked at the very start, before any picture is fetched.
#
#   optimize-art.sh <to> [from]
# Needs oxipng (the same version gives the same files, so publishing the same pictures again
# publishes nothing; .github/workflows/publish-art.yml pins it) and python3 with numpy and
# Pillow, for the check.
set -euo pipefail

to="$1"
from="${2:-$(dirname "$0")/../assets/hd}"
here="$(cd "$(dirname "$0")" && pwd)"
python="${PYTHON:-python3}"
shopt -s nullglob
pictures=("$from"/*.png)
[ ${#pictures[@]} -gt 0 ] || { echo "no pictures in $from" >&2; exit 1; }

mkdir -p "$to"
packed=()
for f in "${pictures[@]}"; do
  cp "$f" "$to/"
  [ "$(basename "$f")" = font.png ] || packed+=("$to/$(basename "$f")")
done
oxipng --version
oxipng -o 4 --strip safe --nb -i 0 -q "${packed[@]}"

before=0 after=0 kept=0
for f in "${packed[@]}"; do
  name=$(basename "$f")
  if ! why=$("$python" "$here/check-png.py" "$from/$name" "$f"); then
    echo "$name: $why; published as it is" >&2
    cp "$from/$name" "$f"
    kept=$((kept + 1))
  fi
  before=$((before + $(wc -c < "$from/$name")))
  after=$((after + $(wc -c < "$f")))
done
echo "$((${#packed[@]} - kept)) of ${#packed[@]} pictures made smaller, the rest kept as they were: $((before / 1048576)) MB to $((after / 1048576)) MB"
