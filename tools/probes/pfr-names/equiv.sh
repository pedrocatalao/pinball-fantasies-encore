#!/bin/bash
# Question: does a change that is meant to rename only change anything a table does?
# Runs the referee (encore-oracle-table) on the table over a fixed set of seeds and modes, the
# table's recordings in tests/recordings (--verify), random games (encore-play) and the engine
# tests, and writes every output into one folder. Run it once on the code before, once after
# (rebuilding the tools between), then `diff -r before after`: the same outputs mean the same
# behaviour on everything these runs reach. (One line of the engine tests' output, "heard from
# N of 512", comes from an unseeded draw and changes from run to run on its own.)
#   usage: tools/probes/pfr-names/equiv.sh <repo> <outdir> <table 1-4> [frames]   (DATA: the game's folder)
set -u
REPO=${1:?repo}
OUT=${2:?outdir}
T=${3:?table 1-4}
FRAMES=${4:-40000}
NAMES=(x PARTYLND SPDDEVLS GAMESHOW STONBONE)
DATA=${DATA:-$REPO/build/data}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)  # (the runs below are made from build/)
cd "$REPO/build" || exit 1
for seed in 1 2 3 4 5 6 7 8; do
  ENCORE_COVER=1 ./encore-oracle-table "$DATA" "$T" "$FRAMES" --all --start --flip "$seed" >"$OUT/oracle-flip-$seed.txt" 2>&1 &
  ENCORE_COVER=1 ./encore-oracle-table "$DATA" "$T" "$FRAMES" --all --start --wild --rough --flip "$seed" >"$OUT/oracle-wild-$seed.txt" 2>&1 &
done
wait
for seed in 1 2 3; do
  ./encore-oracle-table "$DATA" "$T" 15000 --all --start --wild --rough --picture --flip "$seed" >"$OUT/oracle-picture-$seed.txt" 2>&1 &
done
wait
for rpl in "$REPO"/tests/recordings/FANTASY-${NAMES[$T]}-*.RPL; do
  ./encore-play "$DATA" --verify "$rpl" >"$OUT/verify-$(basename "$rpl").txt" 2>&1
done
for seed in 11 12 13; do
  ./encore-play "$DATA" "$T" 60000 "$seed" "$OUT/play-$seed.png" >"$OUT/play-$seed.txt" 2>&1
done
ENCORE_DATA="$DATA" ./encore-engine-tests >"$OUT/engine-tests.txt" 2>&1
echo "engine tests exit $?" >>"$OUT/engine-tests.txt"
echo done
