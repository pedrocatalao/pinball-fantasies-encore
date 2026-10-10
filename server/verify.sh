#!/bin/bash
# verify.sh — checks the games waiting on the server: fetches each recording, plays it again
# with encore-play --verify against the game's own files, and reports what it found. The
# GitHub Actions job runs this every few minutes; it can as well be run by hand.
#
#   ENCORE_API       the Worker, e.g. https://thebestpinball.com
#   VERIFIER_TOKEN   the Worker's secret of the same name
#   ENCORE_PLAY      the encore-play program, built from the same version as the game
#   GAME_DIR         the folder with the game's TABLE1.PRG ... TABLE4.MOD; without it they
#                    are fetched from the server, where upload-game-files.sh put them
#
# Nothing it prints contains the game's files or the token.
set -euo pipefail

: "${ENCORE_API:?}" "${VERIFIER_TOKEN:?}" "${ENCORE_PLAY:?}"
auth=(-H "Authorization: Bearer $VERIFIER_TOKEN")
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

ids=$(curl -fsS "${auth[@]}" "$ENCORE_API/v1/verifier/pending?limit=200" | jq -r '.runs[].id')
[ -z "$ids" ] && { echo "nothing waiting"; exit 0; }

if [ -z "${GAME_DIR:-}" ]; then
  GAME_DIR="$work/game"
  mkdir -p "$GAME_DIR"
  for n in 1 2 3 4; do
    for ext in PRG MOD; do
      curl -fsS "${auth[@]}" -o "$GAME_DIR/TABLE$n.$ext" "$ENCORE_API/v1/verifier/files/TABLE$n.$ext"
    done
  done
fi

# A game the verifier cannot play at all is told to the server as such, and the job goes on to
# the next: the server tries it again later, behind the others, and after a few tries rejects
# it. Only table files that cannot be read stop the job, as then every game would fail.
failures=0
failed() {
  echo "game $1: could not be checked ($2)" >&2
  failures=$((failures + 1))
  jq -cn --arg why "$2" '{failed: $why}' |
    curl -fsS "${auth[@]}" -H "Content-Type: application/json" --data @- "$ENCORE_API/v1/verifier/runs/$1" > /dev/null ||
    echo "game $1: and the server could not be told" >&2
}

for id in $ids; do
  if ! curl -fsS "${auth[@]}" -o "$work/$id.RPL" "$ENCORE_API/v1/verifier/runs/$id/replay"; then
    failed "$id" "its recording could not be fetched"
    continue
  fi
  # A recording it cannot play comes back as {"ok":false,...} and an exit code of 1; a missing
  # table file is 2; anything else (a crash) is the verifier failing on this one game.
  set +e
  verdict=$(env -u VERIFIER_TOKEN "$ENCORE_PLAY" "$GAME_DIR" --verify "$work/$id.RPL")
  code=$?
  set -e
  if [ $code -eq 2 ]; then
    echo "game $id: the table files could not be read; stopping" >&2
    exit 1
  fi
  if [ $code -gt 1 ] || [ -z "$verdict" ]; then
    failed "$id" "encore-play stopped ($code)"
    continue
  fi
  if ! result=$(curl -fsS "${auth[@]}" -H "Content-Type: application/json" --data "$verdict" \
    "$ENCORE_API/v1/verifier/runs/$id"); then
    echo "game $id: the server did not take the verdict" >&2
    failures=$((failures + 1))
    continue
  fi
  echo "game $id: $(jq -r '.status + (if .reason then " (" + .reason + ")" else "" end)' <<<"$result")"
done
# (the job is marked as failed, for someone to look, though every other game was checked)
[ $failures -eq 0 ]
