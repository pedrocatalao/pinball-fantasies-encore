// The latest release's downloads, from GitHub, and the high scores, from this site's own API.

const REPO = "pedrocatalao/pinball-fantasies-encore";
const TABLES = [
  { name: "Party Land", code: "PARTYLND" },
  { name: "Speed Devils", code: "SPDDEVLS" },
  { name: "Billion Dollar Gameshow", code: "GAMESHOW" },
  { name: "Stones 'n' Bones", code: "STONBONE" },
];
const BALLS = [["", "All balls"], ["3", "3 balls"], ["5", "5 balls"]];
const ANGLES = [["", "Any angle"], ["low", "Low"], ["high", "High"], ["higher", "Higher"]];
const ADVANCE_MS = 8000;
const reducedMotion = matchMedia("(prefers-reduced-motion: reduce)").matches;

const $ = (id) => document.getElementById(id);
const el = (tag, props = {}, ...children) => {
  const e = Object.assign(document.createElement(tag), props);
  e.append(...children);
  return e;
};

// ---- the background ---------------------------------------------------------------------

if (reducedMotion) $("bg-video").pause();

// Left alone for a while, the page steps back and lets the game play through; anything done
// brings it forward again.
const IDLE_MS = 15000;
let idleTimer = 0;
function wake() {
  document.body.classList.remove("idle");
  clearTimeout(idleTimer);
  idleTimer = setTimeout(() => document.body.classList.add("idle"), IDLE_MS);
}
for (const event of ["pointermove", "pointerdown", "keydown", "wheel", "scroll", "touchstart"])
  addEventListener(event, wake, { passive: true });
wake();

// ---- the comparison ---------------------------------------------------------------------

// Wherever it is dragged, the line follows.
const compare = $("compare");
compare.querySelector("input").addEventListener("input", (e) => compare.style.setProperty("--pos", `${e.target.value}%`));

// ---- downloads --------------------------------------------------------------------------

// The newest release, pre-releases included (GitHub's "latest" leaves those out).
async function showDownloads() {
  const platforms = [
    [/macos-universal\.zip$/, "macOS", "Universal"],
    [/windows-x64\.zip$/, "Windows", "x64"],
    [/linux-x86_64\.tar\.gz$/, "Linux", "x86_64"],
    [/linux-arm64\.tar\.gz$/, "Linux", "arm64"],
  ];
  try {
    const r = await fetch(`https://api.github.com/repos/${REPO}/releases?per_page=10`);
    const release = (await r.json()).find((x) => !x.draft);
    if (!release) throw new Error("no release");
    const version = release.tag_name.replace(/^v/, "");
    const news = showNews(release.body ?? "");
    $("release").replaceChildren(
      `${news ? "New in version" : "Version"} ${version}${release.prerelease ? ", a first cut for testing" : ""} · `,
      el("a", { href: release.html_url }, "release notes"));
    const buttons = [];
    for (const [pattern, platform, detail] of platforms) {
      const asset = release.assets.find((a) => pattern.test(a.name));
      if (!asset) continue;
      buttons.push(
        el("a", { className: "button", href: asset.browser_download_url, title: asset.name },
          el("span", { className: "platform" }, platform),
          el("span", { className: "file" }, `${detail} · ${(asset.size / 1048576).toFixed(0)} MB`)),
      );
    }
    if (buttons.length) $("downloads").replaceChildren(...buttons);
  } catch {
    $("release").textContent = "The releases are on GitHub.";
  }
}

// The game's lines are all capitals; here only their first letter is, and the names of things.
const NAMES = { linux: "Linux", windows: "Windows", macos: "macOS", hd: "HD", crt: "CRT", dos: "DOS" };
function sentence(line) {
  const lower = line.toLowerCase().replace(/[a-z]+/g, (w) => NAMES[w] ?? w);
  return lower.charAt(0).toUpperCase() + lower.slice(1);
}

// The lines the game shows when it offers a release, from the notes' <!-- game ... --> block,
// listed under the version; whether there were any.
function showNews(body) {
  const block = /<!--\s*game\s*\n([\s\S]*?)-->/.exec(body);
  const lines = (block ? block[1].split("\n") : []).map((l) => l.trim()).filter(Boolean);
  if (!lines.length) return false;
  $("news").replaceChildren(el("ul", { className: "features" }, ...lines.map((l) => el("li", {}, sentence(l)))));
  $("news").hidden = false;
  return true;
}

// The repository's stars, in the footer; without an answer from GitHub, nothing is shown.
async function showStars() {
  try {
    const repo = await (await fetch(`https://api.github.com/repos/${REPO}`)).json();
    if (typeof repo.stargazers_count !== "number") return;
    $("star-count").textContent = repo.stargazers_count.toLocaleString("en-US");
    $("stars").hidden = false;
  } catch {}
}

// ---- high scores ------------------------------------------------------------------------

const state = { table: 0, balls: "", angle: "" };
let linked = false;  // a board was asked for in the address: start there

// #scores?table=2&balls=3&angle=high, so a board can be linked to.
function readHash() {
  if (!location.hash.startsWith("#scores?")) return;
  const q = new URLSearchParams(location.hash.split("?")[1]);
  const t = Number(q.get("table"));
  if (t >= 1 && t <= 4) {
    state.table = t - 1;
    linked = true;
  }
  if (BALLS.some(([v]) => v === q.get("balls"))) state.balls = q.get("balls");
  if (ANGLES.some(([v]) => v === q.get("angle"))) state.angle = q.get("angle");
}
function writeHash() {
  const q = new URLSearchParams({ table: state.table + 1 });
  if (state.balls) q.set("balls", state.balls);
  if (state.angle) q.set("angle", state.angle);
  history.replaceState(null, "", `#scores?${q}`);
}

function filters() {
  const group = (id, options, key) =>
    $(id).replaceChildren(...options.map(([value, label]) =>
      el("button", {
        type: "button",
        textContent: label,
        ariaPressed: String(state[key] === value),
        onclick: () => {
          state[key] = value;
          filters();
          writeHash();
          loadBoards();
        },
      })));
  group("balls", BALLS, "balls");
  group("angle", ANGLES, "angle");
}

const day = (seconds) => new Date(seconds * 1000).toISOString().slice(0, 10);

// Stars for the games a player's initials have had verified, on any table: 4, 16 and 64.
const STARS = [4, 16, 64];
function stars(games) {
  const n = STARS.filter((g) => games >= g).length;
  return n ? el("span", { className: "played", title: `${games} verified games` },
    ...Array.from({ length: n }, () => el("i"))) : "";
}

function row(table, s) {
  const name = ["FANTASY", table.code, s.initials.replace(/ /g, "_"), s.tag, s.score, day(s.at).replace(/-/g, "")]
    .join("-") + ".RPL";
  return el("li", {},
    el("span", { className: "rank" }, String(s.rank)),
    el("span", { className: "who" }, s.initials, el("span", { className: "tag" }, s.tag), stars(s.games)),
    el("span", { className: "score" }, Number(s.score).toLocaleString("en-US")),
    // Each part kept whole on a line; the line breaks between them, if it must.
    el("span", { className: "meta" },
      el("span", {}, `${s.balls} balls`), " · ", el("span", {}, `${s.angle} angle`), " · ",
      el("span", {}, day(s.at)),
      el("a", { className: "replay", href: `/v1/runs/${s.run}/replay`, download: name, title: "Download the recording" }, "RPL")),
  );
}

// One slide per table, made once; their boards are filled in whenever the filters change.
// A board shows its first ten, and the rest when asked.
const TOP = 10;
const slides = TABLES.map((t, i) => {
  const board = el("ol", { className: "board" });
  const empty = el("p", { className: "dmd-text board-empty", hidden: true });
  const more = el("button", { type: "button", className: "board-more", hidden: true });
  const s = { board, empty, more, all: false };
  more.onclick = async () => {
    // Back to ten: first up to the table's top, then the rest folded away, so the reader goes
    // up with the list rather than being left below it. (The page is scrolled, not the slide
    // into view: that would move the carousel itself.)
    if (s.all) {
      const top = s.slide.getBoundingClientRect().top + window.scrollY - 16;
      if (top < window.scrollY) {
        const arrived = new Promise((done) => {
          window.addEventListener("scrollend", done, { once: true });
          setTimeout(done, 1000);  // (where the browser does not say when it has stopped)
        });
        window.scrollTo({ top, behavior: "smooth" });
        await arrived;
      }
    }
    s.all = !s.all;
    shorten(s);
    fit();
  };
  s.slide = el("article", { className: "slide", ariaLabel: t.name, ariaRoleDescription: "slide" },
    el("img", { src: `img/table${i + 1}.jpg`, alt: t.name, width: 1194, height: 285 }),
    el("div", { className: "dmd dmd-board", ariaLive: "polite" }, board, empty, more));
  return s;
});

// The rows past the first ten hidden or shown, and the button saying which it will do.
function shorten(s) {
  const rows = [...s.board.children];
  rows.forEach((r, n) => { r.hidden = !s.all && n >= TOP; });
  s.more.hidden = rows.length <= TOP;
  s.more.textContent = s.all ? `Show top ${TOP}` : `Show all ${rows.length}`;
}
$("track").replaceChildren(...slides.map((s) => s.slide));
$("dots").replaceChildren(...TABLES.map((t, i) =>
  el("button", { type: "button", role: "tab", ariaLabel: t.name, onclick: () => show(i, true) })));

let asked = 0;
async function loadBoards() {
  const mine = ++asked;
  await Promise.all(TABLES.map(async (t, i) => {
    const q = new URLSearchParams({ table: i + 1, limit: 200 });
    if (state.balls) q.set("balls", state.balls);
    if (state.angle) q.set("angle", state.angle);
    const { board, empty } = slides[i];
    try {
      const { scores } = await (await fetch(`/v1/scores?${q}`)).json();
      if (mine !== asked) return;  // the filters changed meanwhile
      board.replaceChildren(...scores.map((s) => row(t, s)));
      shorten(slides[i]);
      empty.textContent = "NO SCORES YET";
      empty.hidden = scores.length > 0;
      fit();
    } catch {
      if (mine !== asked) return;
      board.replaceChildren();
      shorten(slides[i]);
      empty.textContent = "SCORES UNAVAILABLE";
      empty.hidden = false;
      fit();
    }
  }));
}

// ---- the carousel -------------------------------------------------------------------------

let timer = 0;
let held = false;  // the pointer or the keyboard is on it: it waits

// As tall as the table on show, rather than the one with the longest board.
function fit() {
  $("track").parentElement.style.height = `${slides[state.table].slide.offsetHeight}px`;
}

function show(i, byHand = false) {
  state.table = (i + TABLES.length) % TABLES.length;
  $("track").style.transform = `translateX(-${state.table * 100}%)`;
  fit();
  slides.forEach((s, k) => (s.slide.ariaHidden = String(k !== state.table)));
  [...$("dots").children].forEach((d, k) => (d.ariaSelected = String(k === state.table)));
  if (byHand) writeHash();
  schedule();
}

// On to the next table every few seconds, counted again from each change.
function schedule() {
  clearTimeout(timer);
  if (!held && !reducedMotion) timer = setTimeout(() => show(state.table + 1), ADVANCE_MS);
}

const carousel = $("carousel");
$("prev").onclick = () => show(state.table - 1, true);
$("next").onclick = () => show(state.table + 1, true);
carousel.addEventListener("pointerenter", () => { held = true; schedule(); });
carousel.addEventListener("pointerleave", () => { held = false; schedule(); });
carousel.addEventListener("focusin", () => { held = true; schedule(); });
carousel.addEventListener("focusout", () => { held = false; schedule(); });
// The arrow keys switch tables from anywhere on the page, but not with a modifier held,
// which is the browser's (back, forward, selecting).
addEventListener("keydown", (e) => {
  if (e.altKey || e.ctrlKey || e.metaKey || e.shiftKey) return;
  if (e.target instanceof HTMLInputElement) return;  // the comparison's line, say
  if (e.key === "ArrowLeft") show(state.table - 1, true);
  if (e.key === "ArrowRight") show(state.table + 1, true);
});
// A swipe across it, on a phone.
let swipeFrom = null;
carousel.addEventListener("pointerdown", (e) => { swipeFrom = e.clientX; });
carousel.addEventListener("pointerup", (e) => {
  if (swipeFrom === null) return;
  const dx = e.clientX - swipeFrom;
  swipeFrom = null;
  if (Math.abs(dx) > 50) show(state.table + (dx < 0 ? 1 : -1), true);
});
addEventListener("resize", fit);
slides.forEach((s) => s.slide.querySelector("img").addEventListener("load", fit));
document.addEventListener("visibilitychange", () => (document.hidden ? clearTimeout(timer) : schedule()));

readHash();
filters();
show(state.table);
if (linked) $("scores").scrollIntoView();
loadBoards();
showDownloads();
showStars();
