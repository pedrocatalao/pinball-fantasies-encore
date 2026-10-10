// Online high scores for Pinball Fantasies: Encore!
//
// The game sends each game it played as a recording. Nothing it claims is believed: a
// recording waits here as 'pending' until the verifier (a GitHub Actions job running
// encore-play --verify against the game's own files) has played it again and reported what it
// found, and only then does its score count.
//
//   GET  /                                         the project's page, from ../site (wrangler.toml)
//   GET  /media/<file>                             its videos, a part of one when asked (Range)
//   GET  /v1/scores?table=1&balls=3&angle=high   best verified score per player and initials, and their games
//   GET  /v1/players/<tag>                         a player's verified games
//   POST /v1/runs               <recording>        send a game                  (player token)
//   GET  /v1/runs/<id>                             a game, and its rank once verified
//   GET  /v1/runs/<id>/replay                      a verified game's recording
//   GET  /v1/fantasy                               where the game fetches its first-start archive
//   GET  /v1/release                               the newest release, for the game to offer (below)
//   GET  /v1/art                                   the current set of HD pictures, as text (below)
//   GET  /v1/art/<sha256>.png                      one of its pictures, by its contents
//   HEAD /v1/art/<sha256>.png                      whether a picture is here already
//   PUT  /v1/publish/art/<sha256>  <picture>       keep a picture              (publisher token)
//   PUT  /v1/publish/art  <manifest JSON>          make a set the current one  (publisher token)
//   GET  /v1/verifier/pending                      games waiting to be checked  (verifier token)
//   GET  /v1/verifier/runs/<id>/replay             any game's recording         (verifier token)
//   POST /v1/verifier/runs/<id>  <verdict JSON>    what the verifier found, or {failed} (verifier token)
//   GET  /v1/verifier/files                        the game's table files kept  (verifier token)
//   GET  /v1/verifier/files/<name>                 one of them                  (verifier token)
//   PUT  /v1/verifier/files/<name>  <file>         keep one (TABLE1.PRG ...)    (verifier token)
//
// A player is an installation of the game. Its token is a random 64-hex-digit secret the game
// makes and keeps, and sends with every game; only its SHA-256 is stored here. Everyone else
// knows it by its tag, five hexadecimal digits given out the first time it sends a game. A
// score shows the initials typed for it, which anyone may type, and the tag: "RDX (4e87a)".
// The verifier's token is a Worker secret, VERIFIER_TOKEN. A game arriving starts the job at
// once, through GitHub's API, when the Worker has a token allowed to (GITHUB_DISPATCH_TOKEN):
// the job's schedule is only for what that misses, as GitHub runs schedules late or not at all.
//
// The HD pictures are not part of a release: they live in an R2 bucket, each under the SHA-256
// of its contents, so a picture is uploaded once and every set that has it shares it. A set is
// a manifest naming its pictures; publishing one gives it the next version number and makes it
// the current one, and the game asks for that. Going back is publishing the older pictures
// again, which only writes a manifest. The publisher's token is a Worker secret, PUBLISH_TOKEN.
// A set has a format, which a game must understand to use it: it goes up only when pictures
// need code a game does not have (a new kind of picture, say), and older games keep theirs.
// The game reads the set as text, a line for each picture after two of its own:
//
//   version 7
//   format 1
//   <sha256> <size> <name> <url>

export interface Env {
  DB: D1Database;
  VERIFIER_TOKEN: string;
  /** Where the game fetches the archive it offers on its first start (wrangler.toml [vars]), so
   *  that a new address needs a deploy here and not a release of the game. */
  FANTASY_URL: string;
  /** The HD pictures and their manifests (wrangler.toml [[r2_buckets]]). */
  ART: R2Bucket;
  /** The page's own files (wrangler.toml [assets]). */
  ASSETS: Fetcher;
  PUBLISH_TOKEN: string;
  /** A GitHub token that may start the checking job (Actions: read and write, on this
   *  repository alone); without it, games wait for the job's schedule. */
  GITHUB_DISPATCH_TOKEN?: string;
  /** The repository the job is in (wrangler.toml [vars]). */
  GITHUB_REPO: string;
}

/** Recording formats a verifier can play (Recording::kFormat in the game). */
const FORMATS = [3];
const MAX_RECORDING = 512 * 1024;
const MAX_PENDING_PER_PLAYER = 50;
const MAX_RUNS_PER_DAY = 300;
/** As the game lets them be typed: three capitals or spaces. */
const INITIALS = /^[A-Z ]{3}$/;
const TAG = /^[0-9a-f]{5}$/;
/** The tables as recordings are named after them (kTableCodes in the game). */
const TABLE_CODES = ["PARTYLND", "SPDDEVLS", "GAMESHOW", "STONBONE"];

/** As the game names a recording, with the tag: FANTASY-STONBONE-RDX-56070-67108120-20261002.RPL */
function fileName(table: number, initials: string | null, tag: string, score: number, at: number): string {
  const who = initials && INITIALS.test(initials) ? initials.replace(/ /g, "_") : "---";
  const day = new Date(at * 1000).toISOString().slice(0, 10).replace(/-/g, "");
  return `FANTASY-${TABLE_CODES[table - 1]}-${who}-${tag}-${score}-${day}.RPL`;
}
const ANGLES = ["low", "high", "higher"];
/** The original game's files the verifier plays recordings with. */
const GAME_FILE = /^TABLE[1-4]\.(PRG|MOD)$/;
const MAX_GAME_FILE = 1536 * 1024;

const CORS = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Methods": "GET, POST, OPTIONS",
  "Access-Control-Allow-Headers": "Authorization, Content-Type, X-Encore-Version",
};

function json(body: unknown, status = 200): Response {
  return new Response(JSON.stringify(body), { status, headers: { "Content-Type": "application/json", ...CORS } });
}
const fail = (status: number, error: string) => json({ error }, status);

async function sha256(data: ArrayBuffer | Uint8Array): Promise<string> {
  const hash = await crypto.subtle.digest("SHA-256", data);
  return [...new Uint8Array(hash)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

function bearer(req: Request): string | null {
  const m = /^Bearer ([0-9a-f]{64})$/.exec(req.headers.get("Authorization") ?? "");
  return m ? m[1] : null;
}

/** Comparing secrets without the time it takes saying how much of one was right. */
function sameSecret(a: string, b: string): boolean {
  if (a.length !== b.length) return false;
  let d = 0;
  for (let i = 0; i < a.length; ++i) d |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return d === 0;
}

const now = () => Math.floor(Date.now() / 1000);

interface Player {
  id: number;
  tag: string;
}

/** The installation a token belongs to, made the first time it is seen, with a tag of its own. */
async function playerOf(env: Env, req: Request): Promise<Player | null> {
  const token = bearer(req);
  if (!token) return null;
  const tokenHash = await sha256(new TextEncoder().encode(token));
  const known = await env.DB.prepare("SELECT id, tag FROM players WHERE token_hash = ?").bind(tokenHash).first<Player>();
  if (known) return known;
  for (let tries = 0; tries < 20; ++tries) {
    const tag = [...crypto.getRandomValues(new Uint8Array(3))].map((b) => b.toString(16).padStart(2, "0")).join("").slice(0, 5);
    const made = await env.DB.prepare(
      "INSERT INTO players (token_hash, tag, created_at) VALUES (?, ?, ?) ON CONFLICT DO NOTHING RETURNING id, tag",
    )
      .bind(tokenHash, tag, now())
      .first<Player>();
    if (made) return made;
    // The tag was taken, or the same token arrived twice at once.
    const raced = await env.DB.prepare("SELECT id, tag FROM players WHERE token_hash = ?").bind(tokenHash).first<Player>();
    if (raced) return raced;
  }
  return null;
}

function blob(value: unknown): Uint8Array {
  return value instanceof ArrayBuffer ? new Uint8Array(value) : Uint8Array.from(value as number[]);
}

// ---- the boards ----------------------------------------------------------------------------

/** The best verified score of each player and initials on a table (so everyone who plays on one
 *  installation has a place of their own), optionally for one ball count and angle; with how many
 *  verified games those initials have from that installation, on any table, which the site shows
 *  as stars. */
async function scores(env: Env, url: URL): Promise<Response> {
  const table = Number(url.searchParams.get("table"));
  if (!(table >= 1 && table <= 4)) return fail(400, "table must be 1 to 4");
  const ballsParam = url.searchParams.get("balls");
  const balls = ballsParam === null ? null : Number(ballsParam);
  if (balls !== null && !(balls >= 1 && balls <= 9)) return fail(400, "balls must be 1 to 9");
  const angle = url.searchParams.get("angle");
  if (angle !== null && !ANGLES.includes(angle)) return fail(400, "angle must be low, high or higher");
  const limit = Math.min(Math.max(Number(url.searchParams.get("limit") ?? 50) || 50, 1), 200);
  const { results } = await env.DB.prepare(
    `SELECT initials, tag, score, balls, angle, run, at,
            (SELECT COUNT(*) FROM runs c WHERE c.status = 'verified' AND c.player_id = best.player_id
               AND c.initials = best.initials) AS games
     FROM (
       SELECT r.player_id, r.initials, p.tag, r.score, r.balls, r.angle, r.id AS run, r.verified_at AS at,
              ROW_NUMBER() OVER (PARTITION BY r.player_id, r.initials ORDER BY r.score DESC, r.verified_at ASC) AS n
       FROM runs r JOIN players p ON p.id = r.player_id
       WHERE r.status = 'verified' AND r.table_no = ?1 AND (?2 IS NULL OR r.balls = ?2) AND (?3 IS NULL OR r.angle = ?3)
     ) best WHERE n = 1 ORDER BY score DESC, at ASC LIMIT ?4`,
  )
    .bind(table, balls, angle, limit)
    .all();
  return json({ table, balls, angle, scores: results.map((r, i) => ({ rank: i + 1, ...r })) });
}

async function playerPage(env: Env, tag: string): Promise<Response> {
  if (!TAG.test(tag)) return fail(404, "no such player");
  const player = await env.DB.prepare("SELECT id, tag, created_at FROM players WHERE tag = ?")
    .bind(tag)
    .first<Player & { created_at: number }>();
  if (!player) return fail(404, "no such player");
  const { results } = await env.DB.prepare(
    `SELECT id AS run, initials, table_no AS "table", balls, angle, score, frames, verified_at AS at FROM runs
     WHERE player_id = ? AND status = 'verified' ORDER BY verified_at DESC LIMIT 200`,
  )
    .bind(player.id)
    .all();
  return json({ tag: player.tag, since: player.created_at, runs: results });
}

// ---- games ---------------------------------------------------------------------------------

/** Takes a recording to be checked. Only what can be seen without playing it is looked at. */
/** Starts the checking job now. A second start while it runs waits behind it (the job's
 *  concurrency group), so the game sent meanwhile is checked too; any more are dropped by GitHub,
 *  which is harmless. What goes wrong is only logged: the schedule still comes round. */
async function startVerifier(env: Env): Promise<void> {
  if (!env.GITHUB_DISPATCH_TOKEN || !env.GITHUB_REPO) return;
  const r = await fetch(`https://api.github.com/repos/${env.GITHUB_REPO}/actions/workflows/verify-scores.yml/dispatches`, {
    method: "POST",
    headers: {
      Authorization: `Bearer ${env.GITHUB_DISPATCH_TOKEN}`,
      Accept: "application/vnd.github+json",
      "X-GitHub-Api-Version": "2022-11-28",
      "User-Agent": "pinball-fantasies-encore-server",
    },
    body: JSON.stringify({ ref: "main" }),
  });
  if (!r.ok) console.error(`starting the checking job: ${r.status} ${await r.text()}`);
}

async function sendRun(env: Env, req: Request, ctx: ExecutionContext): Promise<Response> {
  const player = await playerOf(env, req);
  if (!player) return fail(401, "a player token is needed");
  const data = new Uint8Array(await req.arrayBuffer());
  if (data.length < 15 || data.length > MAX_RECORDING) return fail(413, "not a recording, or too big");
  if (String.fromCharCode(...data.slice(0, 4)) !== "PFRP") return fail(400, "not a recording");
  const format = data[4] | (data[5] << 8);
  if (!FORMATS.includes(format)) return fail(400, "a recording from a version this server cannot check");
  const hash = await sha256(data);
  // The same file again, or another file with the same seed: the second is the first game
  // changed where the score does not depend on it (the initials, say), which only the player who
  // first sent the seed may send.
  const seed = [...data.slice(7, 15)].map((b) => b.toString(16).padStart(2, "0")).join("");
  const existing = await env.DB.prepare(
    "SELECT id, player_id, status FROM runs WHERE replay_sha256 = ?1 OR seed = ?2 ORDER BY id LIMIT 1",
  )
    .bind(hash, seed)
    .first<{ id: number; player_id: number; status: string }>();
  if (existing) {
    if (existing.player_id !== player.id) return fail(409, "already sent by someone else");
    return json({ id: existing.id, status: existing.status, tag: player.tag });
  }
  const counts = await env.DB.prepare(
    `SELECT SUM(status = 'pending') AS pending, SUM(submitted_at > ?2) AS today FROM runs WHERE player_id = ?1`,
  )
    .bind(player.id, now() - 86400)
    .first<{ pending: number | null; today: number | null }>();
  if ((counts?.pending ?? 0) >= MAX_PENDING_PER_PLAYER || (counts?.today ?? 0) >= MAX_RUNS_PER_DAY)
    return fail(429, "too many games waiting; try again later");
  const version = (req.headers.get("X-Encore-Version") ?? "").slice(0, 32) || null;
  // The game and its recording together or not at all (a batch is one transaction). The same
  // file or seed sent at the same moment by someone else meets the database's own uniqueness,
  // and then neither row is written: the answer is as for a copy.
  const [inserted] = await env.DB.batch([
    env.DB.prepare(
      `INSERT INTO runs (player_id, replay_sha256, seed, format, client_version, submitted_at) VALUES (?, ?, ?, ?, ?, ?)
         ON CONFLICT DO NOTHING RETURNING id`,
    ).bind(player.id, hash, seed, format, version, now()),
    env.DB.prepare(
      `INSERT INTO replays (run_id, data) SELECT id, ?2 FROM runs
         WHERE replay_sha256 = ?1 AND NOT EXISTS (SELECT 1 FROM replays WHERE replays.run_id = runs.id)`,
    ).bind(hash, data),
  ]);
  const run = (inserted.results as { id: number }[])[0];
  if (!run) {
    const other = await env.DB.prepare(
      "SELECT id, player_id, status FROM runs WHERE replay_sha256 = ?1 OR seed = ?2 ORDER BY id LIMIT 1",
    )
      .bind(hash, seed)
      .first<{ id: number; player_id: number; status: string }>();
    if (!other || other.player_id !== player.id) return fail(409, "already sent by someone else");
    return json({ id: other.id, status: other.status, tag: player.tag });
  }
  // After the answer, so the game is not kept waiting on GitHub.
  ctx.waitUntil(startVerifier(env).catch((e) => console.error(e)));
  return json({ id: run.id, status: "pending", tag: player.tag }, 202);
}

async function getRun(env: Env, id: number): Promise<Response> {
  const run = await env.DB.prepare(
    `SELECT r.id, r.initials, p.tag, r.status, r.reason, r.table_no AS "table", r.balls, r.angle, r.score, r.frames,
            r.submitted_at, r.verified_at
     FROM runs r JOIN players p ON p.id = r.player_id WHERE r.id = ?`,
  )
    .bind(id)
    .first<Record<string, unknown>>();
  if (!run) return fail(404, "no such game");
  if (run.status === "verified") {
    // Among each player's and initials' best on the same table, balls and angle.
    const above = await env.DB.prepare(
      `SELECT COUNT(*) AS n FROM (SELECT MAX(score) AS best FROM runs
         WHERE status = 'verified' AND table_no = ? AND balls = ? AND angle = ? GROUP BY player_id, initials)
       WHERE best > ?`,
    )
      .bind(run.table, run.balls, run.angle, run.score)
      .first<{ n: number }>();
    run.rank = (above?.n ?? 0) + 1;
  }
  return json(run);
}

async function getReplay(env: Env, id: number, anyStatus: boolean): Promise<Response> {
  const row = await env.DB.prepare(
    `SELECT d.data, r.status, r.table_no, r.initials, r.score, r.verified_at, p.tag
     FROM replays d JOIN runs r ON r.id = d.run_id JOIN players p ON p.id = r.player_id
     WHERE r.id = ? AND (? OR r.status = 'verified')`,
  )
    .bind(id, anyStatus ? 1 : 0)
    .first<{ data: unknown; status: string; table_no: number; initials: string | null; score: number; verified_at: number; tag: string }>();
  if (!row) return fail(404, "no such recording");
  const name =
    row.status === "verified" ? fileName(row.table_no, row.initials, row.tag, row.score, row.verified_at) : `FANTASY-${id}.RPL`;
  return new Response(blob(row.data), {
    headers: {
      "Content-Type": "application/octet-stream",
      "Content-Disposition": `attachment; filename="${name}"`,
      ...CORS,
    },
  });
}

// ---- the verifier --------------------------------------------------------------------------

function isVerifier(env: Env, req: Request): boolean {
  const token = (req.headers.get("Authorization") ?? "").replace(/^Bearer /, "");
  return !!env.VERIFIER_TOKEN && sameSecret(token, env.VERIFIER_TOKEN);
}

async function pending(env: Env, url: URL): Promise<Response> {
  const limit = Math.min(Math.max(Number(url.searchParams.get("limit") ?? 50) || 50, 1), 500);
  const { results } = await env.DB.prepare(
    // (a game the verifier could not play waits behind the others, so it holds none of them up)
    "SELECT id, format FROM runs WHERE status = 'pending' ORDER BY attempts, submitted_at LIMIT ?",
  )
    .bind(limit)
    .all();
  return json({ runs: results });
}

/** What encore-play --verify printed for a game: the game counts only for one player, whole. */
interface Verdict {
  ok: boolean;
  /** Instead of a verdict: the verifier could not play the game at all, and says why. */
  failed?: string;
  reason?: string;
  format?: number;
  table?: number;
  balls?: number;
  angle?: string;
  frames?: number;
  games?: { endFrame: number; abandoned: boolean; initials?: string; scores: number[] }[];
  /** Whether the games and keys the recording itself says it has came out of playing it. */
  claimsMatch?: boolean;
}

/** How many times the verifier may fail to play a game before it is rejected. */
const MAX_ATTEMPTS = 3;

async function report(env: Env, req: Request, id: number): Promise<Response> {
  const v = (await req.json().catch(() => null)) as Verdict | null;
  if (!v || (typeof v.ok !== "boolean" && typeof v.failed !== "string")) return fail(400, "not a verdict");
  const run = await env.DB.prepare("SELECT status, attempts FROM runs WHERE id = ?")
    .bind(id)
    .first<{ status: string; attempts: number }>();
  if (!run) return fail(404, "no such game");
  // A verdict counts once: a late or repeated one does not change a game already settled.
  if (run.status !== "pending") return fail(409, `already ${run.status}`);
  if (typeof v.failed === "string") {
    const attempts = run.attempts + 1;
    if (attempts < MAX_ATTEMPTS) {
      await env.DB.prepare("UPDATE runs SET attempts = ? WHERE id = ? AND status = 'pending'").bind(attempts, id).run();
      console.error(`game ${id} could not be checked (${attempts} of ${MAX_ATTEMPTS}): ${v.failed.slice(0, 200)}`);
      return json({ id, status: "pending", attempts });
    }
    await env.DB.prepare(
      "UPDATE runs SET status = 'rejected', reason = 'it could not be checked', attempts = ?, verified_at = ? WHERE id = ? AND status = 'pending'",
    )
      .bind(attempts, now(), id)
      .run();
    return json({ id, status: "rejected", reason: "it could not be checked" });
  }
  let reason: string | null = null;
  if (!v.ok) reason = v.reason ?? "not believed";
  else if (v.games?.length !== 1) reason = "not one whole game";
  else if (v.games[0].scores.length !== 1) reason = "more than one player";
  else if (!INITIALS.test(v.games[0].initials ?? "")) reason = "no initials typed for it";
  else if (v.claimsMatch !== true) reason = "it does not play to what it says";
  else if (!(v.table! >= 1 && v.table! <= 4) || !ANGLES.includes(v.angle!) || !(v.balls! >= 1 && v.balls! <= 9))
    reason = "a verdict that makes no sense";
  if (reason) {
    await env.DB.prepare("UPDATE runs SET status = 'rejected', reason = ?, verified_at = ? WHERE id = ? AND status = 'pending'")
      .bind(reason.slice(0, 200), now(), id)
      .run();
    return json({ id, status: "rejected", reason });
  }
  await env.DB.prepare(
    `UPDATE runs SET status = 'verified', reason = NULL, table_no = ?, balls = ?, angle = ?, frames = ?, score = ?,
       initials = ?, verified_at = ? WHERE id = ? AND status = 'pending'`,
  )
    .bind(v.table, v.balls, v.angle, v.frames, v.games![0].scores[0], v.games![0].initials, now(), id)
    .run();
  return json({ id, status: "verified" });
}

// ---- the game's own files, for the verifier only --------------------------------------------

async function listFiles(env: Env): Promise<Response> {
  const { results } = await env.DB.prepare(
    "SELECT name, sha256, length(data) AS size, uploaded_at FROM game_files ORDER BY name",
  ).all();
  return json({ files: results });
}

async function getFile(env: Env, name: string): Promise<Response> {
  if (!GAME_FILE.test(name)) return fail(404, "no such file");
  const row = await env.DB.prepare("SELECT data FROM game_files WHERE name = ?").bind(name).first<{ data: unknown }>();
  if (!row) return fail(404, "no such file");
  return new Response(blob(row.data), { headers: { "Content-Type": "application/octet-stream" } });
}

async function putFile(env: Env, req: Request, name: string): Promise<Response> {
  if (!GAME_FILE.test(name)) return fail(400, "only TABLE1.PRG to TABLE4.MOD");
  const data = new Uint8Array(await req.arrayBuffer());
  if (data.length === 0 || data.length > MAX_GAME_FILE) return fail(413, "empty, or too big");
  const hash = await sha256(data);
  await env.DB.prepare(
    `INSERT INTO game_files (name, data, sha256, uploaded_at) VALUES (?1, ?2, ?3, ?4)
     ON CONFLICT (name) DO UPDATE SET data = ?2, sha256 = ?3, uploaded_at = ?4`,
  )
    .bind(name, data, hash, now())
    .run();
  return json({ name, sha256: hash, size: data.length });
}

// ---- the page's videos ---------------------------------------------------------------------

/** A video from the page's files, whole or the part asked for. Safari on an iPhone plays a video
 *  only from a server that answers for parts of it (Range, 206), which the files alone do not. */
async function media(env: Env, req: Request): Promise<Response> {
  const whole = await env.ASSETS.fetch(new Request(req.url, { method: "GET" }));
  if (!whole.ok) return whole;
  const headers = new Headers(whole.headers);
  headers.set("Accept-Ranges", "bytes");
  const range = /^bytes=(\d*)-(\d*)$/.exec(req.headers.get("Range") ?? "");
  if (!range || (!range[1] && !range[2])) {
    return new Response(req.method === "HEAD" ? null : whole.body, { status: 200, headers });
  }
  const data = await whole.arrayBuffer();
  const size = data.byteLength;
  // bytes=10-19, bytes=10- (to the end), bytes=-20 (the last 20).
  let start = range[1] ? Number(range[1]) : Math.max(0, size - Number(range[2]));
  let end = range[1] && range[2] ? Math.min(Number(range[2]), size - 1) : size - 1;
  if (start >= size || start > end) {
    headers.set("Content-Range", `bytes */${size}`);
    return new Response(null, { status: 416, headers });
  }
  headers.set("Content-Range", `bytes ${start}-${end}/${size}`);
  headers.set("Content-Length", String(end - start + 1));
  return new Response(req.method === "HEAD" ? null : data.slice(start, end + 1), { status: 206, headers });
}

// ---- the newest release -------------------------------------------------------------------

// The game asks at start whether there is a release newer than itself, and shows a few lines
// about it in its own letters. Those come from the release's notes on GitHub, in a comment there
// (so the page does not show them):
//
//   <!-- game
//   ONLINE HIGH SCORES
//   HD ART THAT UPDATES ITSELF
//   -->
//
// The game reads, as text:
//
//   version 1.0.0
//   url https://github.com/.../releases/tag/v1.0.0
//   line ONLINE HIGH SCORES

/** What the game's letters can show (putChar), and how many fit on its screen. */
const LETTERS = /[^A-Z0-9 .:\-?>]/g;
const LINE_LENGTH = 34;
const MAX_LINES = 6;

function releaseLines(body: string): string[] {
  const block = /<!--\s*game\s*\n([\s\S]*?)-->/.exec(body);
  if (!block) return [];
  const lines = block[1]
    .split("\n")
    .map((l) => l.toUpperCase().replace(LETTERS, "").replace(/ +/g, " ").trim().slice(0, LINE_LENGTH));
  while (lines.length && !lines[0]) lines.shift();
  while (lines.length && !lines[lines.length - 1]) lines.pop();
  return lines.slice(0, MAX_LINES);
}

/** The newest release that is not a draft, pre-releases included as on the page; GitHub is asked
 *  at most every ten minutes, whatever the number of games starting. */
async function latestRelease(env: Env, ctx: ExecutionContext): Promise<Response> {
  const cache = caches.default;
  const key = new Request("https://cache.encore/v1/release");
  const cached = await cache.match(key);
  if (cached) return cached;
  const headers: Record<string, string> = {
    Accept: "application/vnd.github+json",
    "X-GitHub-Api-Version": "2022-11-28",
    "User-Agent": "pinball-fantasies-encore-server",
  };
  if (env.GITHUB_DISPATCH_TOKEN) headers.Authorization = `Bearer ${env.GITHUB_DISPATCH_TOKEN}`;
  const r = await fetch(`https://api.github.com/repos/${env.GITHUB_REPO}/releases?per_page=10`, { headers });
  if (!r.ok) return fail(502, `GitHub answered ${r.status}`);
  const releases = await r.json<{ draft: boolean; tag_name: string; html_url: string; body: string | null }[]>();
  const release = releases.find((x) => !x.draft);
  const version = release?.tag_name.replace(/^v/, "");
  if (!release || !version || !/^\d+\.\d+\.\d+$/.test(version)) return fail(404, "no release");
  const text = [`version ${version}`, `url ${release.html_url}`, ...releaseLines(release.body ?? "").map((l) => `line ${l}`)];
  const answer = new Response(text.join("\n") + "\n", {
    headers: { "Content-Type": "text/plain", "Cache-Control": "public, max-age=600", ...CORS },
  });
  ctx.waitUntil(cache.put(key, answer.clone()));
  return answer;
}

// ---- the HD pictures ------------------------------------------------------------------------

/** As the game names its pictures (HdPicture, the flippers, the ball): playfield1_on.png. */
const ART_NAME = /^[a-z0-9_]{1,64}\.png$/;
const SHA256 = /^[0-9a-f]{64}$/;
const MAX_PICTURE = 32 * 1024 * 1024;

interface ArtFile {
  name: string;
  size: number;
  sha256: string;
}
interface ArtSet {
  version: number;
  format: number;
  publishedAt: number;
  files: ArtFile[];
}

const pictureKey = (hash: string) => `pictures/${hash}`;

/** The current set, with where each picture is fetched from; nothing yet is a 404. */
async function currentArt(env: Env, url: URL): Promise<Response> {
  const current = await env.ART.get("current.json");
  if (!current) return fail(404, "no pictures published yet");
  const set = await current.json<ArtSet>();
  const lines = [`version ${set.version}`, `format ${set.format}`];
  for (const f of set.files) lines.push(`${f.sha256} ${f.size} ${f.name} ${url.origin}/v1/art/${f.sha256}.png`);
  return new Response(lines.join("\n") + "\n", {
    headers: { "Content-Type": "text/plain", "Cache-Control": "no-store", ...CORS },
  });
}

/** A picture never changes under its name, so it may be kept anywhere for as long as wanted. */
async function picture(env: Env, hash: string, head: boolean): Promise<Response> {
  const headers = { "Content-Type": "image/png", "Cache-Control": "public, max-age=31536000, immutable", ...CORS };
  if (head) {
    const found = await env.ART.head(pictureKey(hash));
    return found ? new Response(null, { headers: { ...headers, "Content-Length": String(found.size) } }) : fail(404, "no such picture");
  }
  const found = await env.ART.get(pictureKey(hash));
  return found ? new Response(found.body, { headers }) : fail(404, "no such picture");
}

function isPublisher(env: Env, req: Request): boolean {
  const token = (req.headers.get("Authorization") ?? "").replace(/^Bearer /, "");
  return !!env.PUBLISH_TOKEN && sameSecret(token, env.PUBLISH_TOKEN);
}

/** A picture is kept under what it is, so one sent under another hash is turned away. */
async function putPicture(env: Env, req: Request, hash: string): Promise<Response> {
  const data = new Uint8Array(await req.arrayBuffer());
  if (data.length === 0 || data.length > MAX_PICTURE) return fail(413, "empty, or too big");
  if ((await sha256(data)) !== hash) return fail(400, "the picture is not what its name says");
  if (!(data[0] === 0x89 && data[1] === 0x50 && data[2] === 0x4e && data[3] === 0x47)) return fail(400, "not a PNG");
  await env.ART.put(pictureKey(hash), data, { httpMetadata: { contentType: "image/png" } });
  return json({ sha256: hash, size: data.length });
}

/** A new set, from { format?, files: [{ name, size, sha256 }] }: every picture in it must be
 *  here already. It becomes the next version and the current one. */
async function publishArt(env: Env, req: Request): Promise<Response> {
  let body: { format?: unknown; files?: unknown };
  try {
    body = await req.json();
  } catch {
    return fail(400, "not JSON");
  }
  const format = body.format ?? 1;
  if (typeof format !== "number" || !Number.isInteger(format) || format < 1) return fail(400, "format must be 1 or more");
  if (!Array.isArray(body.files) || body.files.length === 0 || body.files.length > 256) return fail(400, "no files, or too many");
  const files: ArtFile[] = [];
  const names = new Set<string>();
  for (const f of body.files as Partial<ArtFile>[]) {
    if (typeof f.name !== "string" || !ART_NAME.test(f.name) || names.has(f.name)) return fail(400, `bad or repeated name: ${f.name}`);
    if (typeof f.sha256 !== "string" || !SHA256.test(f.sha256)) return fail(400, `bad hash for ${f.name}`);
    const found = await env.ART.head(pictureKey(f.sha256));
    if (!found) return fail(409, `${f.name} has not been uploaded`);
    names.add(f.name);
    files.push({ name: f.name, size: found.size, sha256: f.sha256 });
  }
  files.sort((a, b) => a.name.localeCompare(b.name));
  const bytes = files.reduce((n, f) => n + f.size, 0);
  // The same pictures again are not news: no new version, so no game is asked to fetch them.
  const previousObject = await env.ART.get("current.json");
  const previous = previousObject ? await previousObject.json<ArtSet>() : null;
  const same = (a: ArtFile[], b: ArtFile[]) => a.length === b.length && a.every((f, i) => f.name === b[i].name && f.sha256 === b[i].sha256);
  if (previous && previous.format === format && same(previous.files, files))
    return json({ version: previous.version, files: files.length, bytes, unchanged: true });
  const version = previous ? previous.version + 1 : 1;
  const set: ArtSet = { version, format, publishedAt: now(), files };
  const text = JSON.stringify(set);
  await env.ART.put(`sets/${version}.json`, text, { httpMetadata: { contentType: "application/json" } });
  await env.ART.put("current.json", text, { httpMetadata: { contentType: "application/json" } });
  return json({ version, files: files.length, bytes });
}

// ---- routing -------------------------------------------------------------------------------

export default {
  async fetch(req: Request, env: Env, ctx: ExecutionContext): Promise<Response> {
    if (req.method === "OPTIONS") return new Response(null, { status: 204, headers: CORS });
    const url = new URL(req.url);
    const path = url.pathname.replace(/\/+$/, "");
    const get = req.method === "GET", post = req.method === "POST";
    let m: RegExpExecArray | null;
    try {
      if (get && path === "/v1/scores") return await scores(env, url);
      if (get && path === "/v1/fantasy")
        return env.FANTASY_URL
          ? new Response(env.FANTASY_URL, { headers: { "Content-Type": "text/plain", "Cache-Control": "no-store", ...CORS } })
          : fail(404, "nothing to fetch");
      if ((get || req.method === "HEAD") && path.startsWith("/media/")) return await media(env, req);
      if (get && path === "/v1/release") return await latestRelease(env, ctx);
      if (get && path === "/v1/art") return await currentArt(env, url);
      if ((get || req.method === "HEAD") && (m = /^\/v1\/art\/([0-9a-f]{64})\.png$/.exec(path)))
        return await picture(env, m[1], req.method === "HEAD");
      if (path.startsWith("/v1/publish/")) {
        if (!isPublisher(env, req)) return fail(401, "publisher only");
        if (req.method === "PUT" && (m = /^\/v1\/publish\/art\/([0-9a-f]{64})$/.exec(path))) return await putPicture(env, req, m[1]);
        if (req.method === "PUT" && path === "/v1/publish/art") return await publishArt(env, req);
      }
      if (get && (m = /^\/v1\/players\/([^/]+)$/.exec(path))) return await playerPage(env, decodeURIComponent(m[1]));
      if (post && path === "/v1/runs") return await sendRun(env, req, ctx);
      if (get && (m = /^\/v1\/runs\/(\d+)$/.exec(path))) return await getRun(env, Number(m[1]));
      if (get && (m = /^\/v1\/runs\/(\d+)\/replay$/.exec(path))) return await getReplay(env, Number(m[1]), false);
      if (path.startsWith("/v1/verifier/")) {
        if (!isVerifier(env, req)) return fail(401, "verifier only");
        if (get && path === "/v1/verifier/pending") return await pending(env, url);
        if (get && (m = /^\/v1\/verifier\/runs\/(\d+)\/replay$/.exec(path))) return await getReplay(env, Number(m[1]), true);
        if (post && (m = /^\/v1\/verifier\/runs\/(\d+)$/.exec(path))) return await report(env, req, Number(m[1]));
        if (get && path === "/v1/verifier/files") return await listFiles(env);
        if (get && (m = /^\/v1\/verifier\/files\/([^/]+)$/.exec(path))) return await getFile(env, m[1]);
        if (req.method === "PUT" && (m = /^\/v1\/verifier\/files\/([^/]+)$/.exec(path)))
          return await putFile(env, req, m[1]);
      }
      return fail(404, "nothing here");
    } catch (e) {
      console.error(e);
      return fail(500, "something went wrong");
    }
  },
};
