# Online high scores

When a one-player game makes the local high scores, the game asks, after the initials are
typed, whether to send it online; if so, it sends the game as a recording. A Cloudflare Worker keeps it
as *pending*; a GitHub Actions job (`.github/workflows/verify-scores.yml`) plays it again with
`encore-play --verify` against the game's own files and reports what it found; only then does
the score count. The score on the board is the one the replay arrives at.

- `src/index.ts`: the Worker, with the API at the top of the file.
- `migrations/`: the D1 database.
- `verify.sh`: the checking, as the job runs it.
- `upload-game-files.sh`: puts the game's table files on the server, for the job.
- `publish-art.sh`: publishes the HD pictures the game fetches (see below), made smaller by
  `optimize-art.sh` and checked by `check-png.py`.

The Worker, its database, the recordings (a few KB each) and the game's table files (about
3 MB, readable only with the verifier's token) fit in Cloudflare's free plan; the HD pictures'
R2 bucket wants a payment method on the account, though they stay inside its free allowance.

## Setting it up

You need a Cloudflare account and Node.js. In this folder:

```bash
npm install
npx wrangler login
npx wrangler d1 create encore-scores
```

Put the `database_id` it prints into `wrangler.toml`, then:

```bash
npx wrangler d1 migrations apply encore-scores --remote
npx wrangler secret put VERIFIER_TOKEN
npx wrangler deploy
```

For `VERIFIER_TOKEN`, paste a long random secret, for example the output of
`openssl rand -hex 32`. Keep it: the checking job needs the same one. `deploy` prints the
Worker's address, `https://pinball-fantasies-encore.<your subdomain>.workers.dev`, and the
project's own, `https://thebestpinball.com` (the `routes` in `wrangler.toml`: take them out,
or put your own domain there, if you set up a server of your own). Either reaches it; the
examples below use the project's.

Then put the game's table files on the server, from a machine that has the game:

```bash
ENCORE_API=https://thebestpinball.com VERIFIER_TOKEN=<the secret> \
  ./upload-game-files.sh <the game's folder>
```

And give the checking job the two things it needs, in the GitHub repository's **Settings →
Secrets and variables → Actions**: a **secret** `VERIFIER_TOKEN` (the same secret) and a
**variable** `ENCORE_API` (the Worker's address). The job runs every ten minutes from the
default branch, or by hand from the Actions tab. GitHub stops scheduled jobs in a repository
with no activity for 60 days; the Actions tab turns it back on.

GitHub runs schedules late, and sometimes not at all, so the Worker also starts the job itself
as soon as a game arrives, when it has a token for it. Make one on GitHub under **Settings →
Developer settings → Personal access tokens → Fine-grained tokens**: only this repository, with
the permission **Actions: Read and write**, and as long an expiry as it allows. Then:

```bash
npx wrangler secret put GITHUB_DISPATCH_TOKEN
```

Without it, or once it has expired, games simply wait for the schedule. A burst of games
starts the job once a minute at most.

## Limits

Anyone can send a game, and a player is only a token the game makes up, so the limits that
matter hold for everyone together. A request is checked to be a recording (its magic, format,
and at most 256 KB) before anything is written: a copy, or anything refused, makes no player. One
address may send 10 games a minute (Cloudflare's rate limiter, `[[ratelimits]]` in
`wrangler.toml`: nothing of the address is kept); at most 200 games wait to be checked, and at
most 60 players are made in an hour, from everyone; a player may have 50 games waiting and send
300 a day. Past any of these the answer is 429, and the game keeps the recording and sends it
later. Once a day (`[triggers]` in `wrangler.toml`) the recordings of games rejected more than
30 days ago are deleted; the games stay, with why they were rejected.

After a change to the Worker or a new migration:

```bash
npx wrangler d1 migrations apply encore-scores --remote
npx wrangler deploy
```

## The HD pictures

The HD pictures are not in the releases: the game fetches them from here. They are kept in an
R2 bucket, each under the SHA-256 of its contents, and a *set* is a manifest naming them;
`GET /v1/art` gives the current set, as text, which the game compares with what it has. Publishing gives
a set the next version number, unless it is the same pictures as the current one.

Once, to make the bucket (R2 wants a payment method on the account, though the pictures stay
far inside its free allowance) and the publisher's secret (another `openssl rand -hex 32`):

```bash
npx wrangler r2 bucket create encore-art
npx wrangler secret put PUBLISH_TOKEN
npx wrangler deploy
```

A GitHub Actions job (`.github/workflows/publish-art.yml`) publishes them whenever `assets/hd`
changes on the default branch, or by hand from the Actions tab; it needs the same token as a
repository **secret** `PUBLISH_TOKEN`, beside the `ENCORE_API` variable. The pictures are
published made smaller without a pixel changed (`optimize-art.sh`, with oxipng): each copy is
checked to be one the game reads and the same picture (`check-png.py`), or the original goes
instead, and `font.png` always goes as it is. The pictures in `assets/hd` stay as drawn. To
publish from your own machine instead, from the repository root, with oxipng 10.2.1 (the
job's, so that the same pictures make the same files) and python3 with numpy and Pillow:

```bash
server/optimize-art.sh /tmp/hd
ENCORE_API=https://thebestpinball.com PUBLISH_TOKEN=<the secret> \
  server/publish-art.sh /tmp/hd
```

Only the pictures the server does not have are uploaded. When the pictures need code that
games out there do not have (a new kind of picture, say), raise `kArtFormat` in
`src/game/Art.h` and publish with `ART_FORMAT=` the same number: games that do not understand
it keep the set they have. Going back to older
pictures is running it from an older checkout: they are all still there, so it only writes the
set.

## A newer release

A release build (one made by the release workflows, which give it the release's version) asks
`GET /v1/release` at start whether there is a newer release, and offers to open its page. The
Worker answers from GitHub's list of releases, asking it at most every ten minutes. The few
lines the game shows about what is new come from the release's notes, in a comment so that the
page does not show them; capitals, digits and `. : - ? >` only, six lines of 34 letters at most:

```
<!-- game
ONLINE HIGH SCORES
HD ART THAT UPDATES ITSELF
F11 FOR FULLSCREEN
-->
```

## Trying it locally

```bash
printf 'VERIFIER_TOKEN=local-secret\n' > .dev.vars
npx wrangler d1 migrations apply encore-scores --local
npx wrangler dev
```

Then, from the repository root, with a player token of your own (`openssl rand -hex 32`), and a
recording with initials: one of `tests/recordings`, or one the game made pointed at the local
server (`ENCORE_API=http://localhost:8787`):

```bash
curl -X POST -H "Authorization: Bearer $TOKEN" \
  --data-binary @tests/recordings/FANTASY-SPDDEVLS-RDX-28013570-20261003-1939.RPL http://localhost:8787/v1/runs
ENCORE_API=http://localhost:8787 VERIFIER_TOKEN=local-secret ENCORE_PLAY=build/encore-play \
  GAME_DIR=<the game's folder> server/verify.sh
curl "http://localhost:8787/v1/scores?table=1"
```

## What counts

One whole game, by one player, played to its end without cheats (no tilt, slow motion, or
more balls than the options give), with initials typed for it, in a recording format the
verifier can play. What the replay does is what counts, not what the recording's header says:
a cheat's word typed into the recording before the start is seen as the game starts, and a game
whose angle was changed while paused counts as played at the gentlest angle it had.

A recording that plays again to anything other than its own games and keys is rejected, as one
that does not play to what it says. Each game's seed (what every chance in it is drawn from)
belongs to the first player who sends it: the same seed from anyone else is a copy of their
game however the rest of the file was changed, and is refused (409); the same player sending it
again gets the first one back. The database keeps one game per seed, so copies sent at the same
moment cannot both get in. A game the verifier cannot play at all (its recording cannot be
fetched, or encore-play stops) waits behind the others and is tried again; after three tries it
is rejected as one that could not be checked. A verdict counts once: one sent for a game
already settled is refused (409).

The verifier is built from the newest code on main, and checks games sent by released versions
(each says which, in `X-Encore-Version`, kept beside it). That holds because main may not change
how a recorded game plays: see the end of "The game" in [own-engine.md](../docs/own-engine.md#the-game).

A player is an installation of the game: it makes a secret token the first time it sends a
game (kept in `online.txt` beside the high scores) and the server gives it a public tag of
five hexadecimal digits. A score shows the initials typed for it and the tag, `RDX (4e87a)`,
so anyone may type any initials and still be told apart. The boards show the best
verified score of each installation and initials per table, so everyone who plays on one
computer has a place of their own, and can be narrowed to a ball count and an angle; `/v1/players/<tag>` lists all of one installation's games, and every verified game's
recording can be downloaded from `/v1/runs/<id>/replay`.
