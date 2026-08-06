#!/usr/bin/env node
/* ============================================================================
   Isekai Hero — consistency checks
   ----------------------------------------------------------------------------
   Runs without Skyrim, without a compiler, in about a second:

       node tools/check.mjs

   These are NOT unit tests. Almost every line of this mod orchestrates game API
   calls and cannot run outside Skyrim, so classic unit tests would have caught
   very little. What actually broke during development was something else: the
   same data described in several places drifting apart, and geometry that
   overlapped because one term did not scale with the others. That is what this
   file checks.

   Every check below exists because the corresponding bug happened at least once.
   Adding a check is cheaper than re-finding its bug.
   ========================================================================== */

import { readFileSync, existsSync, readdirSync, statSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";

const ROOT = dirname(dirname(fileURLToPath(import.meta.url)));
const read = (p) => readFileSync(join(ROOT, p), "utf8");

let failures = 0;
let checks = 0;
const results = [];

function check(name, fn) {
  checks++;
  try {
    const detail = fn();
    results.push({ ok: true, name, detail: detail || "" });
  } catch (err) {
    failures++;
    results.push({ ok: false, name, detail: err.message });
  }
}
const fail = (msg) => { throw new Error(msg); };
const need = (cond, msg) => { if (!cond) fail(msg); };

/* -- parsers ---------------------------------------------------------------
   Deliberately strict: if a table's shape changes so a parser stops matching,
   the check FAILS rather than silently verifying nothing. An early version of
   the geometry check reported "OK" while parsing zero nodes. */

function parseSkillTreeNodes() {
  const cpp = read("src/SkillTree.cpp");
  const from = cpp.indexOf("constexpr Node kNodes[] = {");
  need(from >= 0, "kNodes table not found in SkillTree.cpp");
  const body = cpp.slice(from, cpp.indexOf("\n        };", from));

  const opens = [...body.matchAll(/\{\s*(\d+|kAnalyzeNodeKey),\s*Zone::k(\w+),/g)];
  need(opens.length > 0, "no node entries parsed — has the table's shape changed?");

  return opens.map((m, i) => {
    const chunk = body.slice(m.index, i + 1 < opens.length ? opens[i + 1].index : body.length);
    const pos = chunk.match(/"[^"]*\.png",\s*(-?[\d.]+)f,\s*(-?[\d.]+)f,\s*(\d+),/);
    need(pos, `node ${m[1]}: could not read x/y/cost`);
    const scale = chunk.match(/0\.0f,\s*([\d.]+)f\s*\}/);
    // First quoted string after the zone is the display name; the prereq pair sits
    // immediately before the Effect. Both feed the label-geometry checks below.
    const name = chunk.match(/Zone::k\w+,\s*"([^"]+)"/);
    need(name, `node ${m[1]}: could not read the name`);
    const pre = chunk.match(/\{\s*(\d+),\s*(\d+)\s*\},\s*Effect::/);
    need(pre, `node ${m[1]}: could not read the prereq pair`);
    return {
      key: m[1] === "kAnalyzeNodeKey" ? 23 : +m[1],
      zone: m[2].toUpperCase(),
      name: name[1],
      x: +pos[1], y: +pos[2], cost: +pos[3],
      prereq: [+pre[1], +pre[2]].filter(Boolean),
      scale: scale ? +scale[1] : 1,
    };
  });
}

function parseMockNodes() {
  const mock = read("playground/mock.js");
  const re = /\{\s*key:\s*(\d+),\s*zone:\s*"(\w+)",\s*(?:scale:\s*([\d.]+),\s*)?name:[\s\S]*?x:\s*(-?[\d.]+),\s*y:\s*(-?[\d.]+),\s*cost:\s*(\d+),/g;
  const out = [...mock.matchAll(re)].map((m) => ({
    key: +m[1], zone: m[2], scale: m[3] ? +m[3] : 1, x: +m[4], y: +m[5], cost: +m[6],
  }));
  need(out.length > 0, "no nodes parsed from playground/mock.js");
  return out;
}

function parseShopCatalog() {
  const cpp = read("src/Shop.cpp");
  const from = cpp.indexOf("constexpr Entry kCatalog[] = {");
  need(from >= 0, "kCatalog not found in Shop.cpp");
  const body = cpp.slice(from, cpp.indexOf("\n        };", from));
  const re = /\{\s*"([^"]+)",\s*"([^"]+)",\s*(\d+),\s*"([^"]+)",\s*\n?\s*Kind::k(\w+),\s*Cat::k\w+,\s*([\d']+),\s*Shelf::k(\w+)\s*(?:,\s*(0x[0-9A-Fa-f]+))?\s*\}/g;
  const out = [...body.matchAll(re)].map((m) => ({
    name: m[1], qty: m[2], cost: +m[3], icon: m[4],
    kind: m[5], amount: m[6], shelf: m[7], localID: m[8] ? parseInt(m[8], 16) : null,
  }));
  need(out.length > 0, "no catalog entries parsed from Shop.cpp");
  return out;
}

/* The shelf NAMES, in the order both renderers offer them. */
function parseShelfNames() {
  const cpp = read("src/Shop.cpp");
  const m = cpp.match(/kShelfNames\[\]\s*=\s*\{([\s\S]*?)\};/);
  need(m, "kShelfNames not found in Shop.cpp");
  const out = [...m[1].matchAll(/"([^"]+)"/g)].map((x) => x[1]);
  need(out.length > 0, "no shelf names parsed — parser stale?");
  return out;
}

function parseMockShop() {
  const mock = read("playground/mock.js");
  const re = /\{\s*name:\s*"([^"]+)",\s*qty:\s*"([^"]+)",\s*cost:\s*(\d+),\s*icon:\s*"([^"]+)",\s*shelf:\s*"([^"]+)"\s*\}/g;
  const out = [...mock.matchAll(re)].map((m) => ({
    name: m[1], qty: m[2], cost: +m[3], icon: m[4], shelf: m[5],
  }));
  need(out.length > 0, "no shop items parsed from playground/mock.js");
  return out;
}

/* -- 1. skill tree data: C++ is the source of truth, the mock must match ---- */

check("skill tree: C++ and playground node tables agree", () => {
  const cpp = new Map(parseSkillTreeNodes().map((n) => [n.key, n]));
  const mock = new Map(parseMockNodes().map((n) => [n.key, n]));
  need(cpp.size === mock.size, `C++ has ${cpp.size} nodes, playground has ${mock.size}`);
  for (const [key, c] of cpp) {
    const m = mock.get(key);
    need(m, `node ${key} missing from the playground mock`);
    for (const f of ["zone", "x", "y", "cost", "scale"]) {
      need(c[f] === m[f], `node ${key}.${f}: C++ ${c[f]} vs playground ${m[f]}`);
    }
  }
  return `${cpp.size} nodes`;
});

check("skill tree: node keys are unique", () => {
  const keys = parseSkillTreeNodes().map((n) => n.key);
  const dupes = keys.filter((k, i) => keys.indexOf(k) !== i);
  need(dupes.length === 0, `duplicate node keys: ${[...new Set(dupes)].join(", ")}`);
  return `${keys.length} unique`;
});

check("skill tree: every node icon file exists", () => {
  const cpp = read("src/SkillTree.cpp");
  const from = cpp.indexOf("constexpr Node kNodes[] = {");
  const body = cpp.slice(from, cpp.indexOf("\n        };", from));
  const icons = [...new Set([...body.matchAll(/"([\w.]+\.png)"/g)].map((m) => m[1]))];
  need(icons.length > 0, "no node icons parsed — parser stale?");
  for (const i of icons) {
    need(existsSync(join(ROOT, "icons", i)), `icons/${i} is missing — that tile renders blank`);
  }
  return `${icons.length} icons`;
});

check("skill tree: every prerequisite names a real node", () => {
  const cpp = read("src/SkillTree.cpp");
  const from = cpp.indexOf("constexpr Node kNodes[] = {");
  const body = cpp.slice(from, cpp.indexOf("\n        };", from));
  const keys = new Set(parseSkillTreeNodes().map((n) => n.key));
  const prereqs = [...body.matchAll(/\{\s*(\d+),\s*(\d+)\s*\},\s*Effect::/g)];
  need(prereqs.length > 0, "no prereq pairs parsed — parser stale?");
  let refs = 0;
  for (const m of prereqs) {
    for (const p of [+m[1], +m[2]]) {
      if (p === 0) continue;   // 0 = no prerequisite
      refs++;
      need(keys.has(p), `a node lists prerequisite #${p}, which does not exist`);
    }
  }
  return `${refs} references`;
});

check("skill tree: a retired node key is never reused", () => {
  // The save stores bare numbers. Putting a retired key back in the table would hand
  // whoever still has it a node they never bought, with no way to tell the difference.
  const hdr = read("src/SkillTree.h");
  const from = hdr.indexOf("kRetiredNodes[] = {");
  need(from >= 0, "kRetiredNodes not found in SkillTree.h");
  const body = hdr.slice(from, hdr.indexOf("};", from));
  const retired = [...body.matchAll(/\{\s*(\d+),\s*(\d+),\s*"([^"]+)"\s*\}/g)]
    .map((m) => ({ key: +m[1], refund: +m[2], name: m[3] }));
  need(retired.length > 0, "no retired nodes parsed — parser stale?");

  const live = new Set(parseSkillTreeNodes().map((n) => n.key));
  for (const r of retired) {
    need(!live.has(r.key),
         `key ${r.key} is retired ("${r.name}") but the tree uses it again`);
  }
  return retired.map((r) => `${r.name} #${r.key} (+${r.refund} SP)`).join(", ");
});

check("source: no path literal has an unescaped backslash", () => {
  // "Data\SKSE\..." compiles. MSVC warns C4129 and then drops the backslash, so the
  // path becomes "DataSKSEPlugins..." and every file under it silently fails to load.
  // That shipped in BlessingIcon and cost the blessing screen all seven of its icons;
  // the warning was printed on every build and read as noise.
  const bad = [];
  for (const f of cppSources().concat(["src/Config.h", "src/SkillTree.h"])) {
    const src = read(f);
    src.split("\n").forEach((line, i) => {
      for (const m of line.matchAll(/"((?:[^"\\\n]|\\.)*)"/g)) {
        // Walk the literal rather than pattern-matching it: a backslash consumes the
        // character after it, so `\\S` is a legal escape followed by an S, while `\S`
        // is the bug. A regex that tests every position cannot tell those apart — the
        // first version of this check flagged every correctly-escaped path in the repo.
        const body = m[1];
        const legal = "\\ntrfvab0'\"?xu";
        for (let k = 0; k < body.length; k++) {
          if (body[k] !== "\\") continue;
          const next = body[k + 1];
          if (next === undefined || !legal.includes(next)) {
            bad.push(`${f}:${i + 1}  ${line.trim()}`);
            break;
          }
          k++;  // the escape consumed it
        }
      }
    });
  }
  need(bad.length === 0, `unescaped backslash:\n    ${bad.join("\n    ")}`);
  return "all path literals escaped";
});

/* -- 2. zone geometry ------------------------------------------------------
   Both renderers pad zone frames in design units scaled by the same fit factor
   as the node positions and radii, so the whole layout is proportional and can
   be verified in design units alone. It was NOT proportional once: the padding
   was in raw pixels, and below a fit of ~0.9 the frames grew into each other.

   What is measured here is the node BOX, not the tile: every node carries an
   always-on name below it, and the name is wider than its tile. Checking tiles
   alone passed happily while the outer names hung over their zone border, every
   link ran through its own parent's name, and the CORE column had 6px between
   "System Core" and the tile under it. */

const NODE_R = 31;
const LABEL_W = 108, LABEL_H = 38;   // design units; ~88x31 CSS px in the web view
const ZPAD_X = 10, ZPAD_TOP = 18, ZPAD_BOT = 12;

/* Tile union name — a node's real footprint. */
function nodeBox(n) {
  const r = NODE_R * n.scale, hw = Math.max(r, LABEL_W / 2);
  return { x0: n.x - hw, x1: n.x + hw, y0: n.y - r, y1: n.y + r + LABEL_H };
}

function zoneFrames() {
  const graph = parseSkillTreeNodes().filter((n) => n.zone !== "MASTERY");
  const z = {};
  for (const n of graph) {
    const nb = nodeBox(n);
    const b = (z[n.zone] ??= { x0: 1e9, x1: -1e9, y0: 1e9, y1: -1e9 });
    b.x0 = Math.min(b.x0, nb.x0); b.x1 = Math.max(b.x1, nb.x1);
    b.y0 = Math.min(b.y0, nb.y0); b.y1 = Math.max(b.y1, nb.y1);
  }
  // The three branches share a top and bottom (see zoneBoxes / the ImGui equivalent).
  const branches = Object.keys(z).filter((k) => k !== "CORE");
  const top = Math.min(...branches.map((k) => z[k].y0));
  const bot = Math.max(...branches.map((k) => z[k].y1));
  for (const k of branches) { z[k].y0 = top; z[k].y1 = bot; }

  const out = {};
  for (const [k, b] of Object.entries(z)) {
    out[k] = { x0: b.x0 - ZPAD_X, x1: b.x1 + ZPAD_X, y0: b.y0 - ZPAD_TOP, y1: b.y1 + ZPAD_BOT };
  }
  return out;
}

check("skill tree: zone frames do not overlap", () => {
  const f = zoneFrames();
  const names = Object.keys(f);
  need(names.length >= 2, "expected several zones");
  let tightest = Infinity;
  for (let i = 0; i < names.length; i++) {
    for (let j = i + 1; j < names.length; j++) {
      const a = f[names[i]], b = f[names[j]];
      const ox = Math.min(a.x1, b.x1) - Math.max(a.x0, b.x0);
      const oy = Math.min(a.y1, b.y1) - Math.max(a.y0, b.y0);
      need(!(ox > 0 && oy > 0),
           `${names[i]} and ${names[j]} overlap by ${ox.toFixed(0)}x${oy.toFixed(0)}`);
      tightest = Math.min(tightest, Math.max(-ox, -oy));
    }
  }
  return `closest pair ${tightest.toFixed(0)} units apart`;
});

check("skill tree: the three branch frames are equal height", () => {
  const f = zoneFrames();
  const hs = Object.entries(f).filter(([k]) => k !== "CORE")
                              .map(([, b]) => (b.y1 - b.y0).toFixed(2));
  need(new Set(hs).size === 1, `branch frame heights differ: ${hs.join(", ")}`);
  return `${hs[0]} units`;
});

check("skill tree: every node stays inside its own zone frame", () => {
  // The one the screenshot showed: "Perk Synthesis" and "Dragon's Voice" hung over
  // the CORE border because the frame was grown from the tiles, and a name is wider
  // than its tile.
  const frames = zoneFrames();
  let tightest = Infinity, who = "";
  for (const n of parseSkillTreeNodes().filter((x) => x.zone !== "MASTERY")) {
    const b = nodeBox(n), f = frames[n.zone];
    const c = Math.min(b.x0 - f.x0, f.x1 - b.x1, b.y0 - f.y0, f.y1 - b.y1);
    need(c >= 0, `"${n.name}" sticks out of the ${n.zone} frame by ${(-c).toFixed(0)} units`);
    if (c < tightest) { tightest = c; who = n.name; }
  }
  return `tightest "${who}" with ${tightest.toFixed(0)} units`;
});

check("skill tree: node boxes never collide", () => {
  // Boxes, not tiles: two tiles 40 units apart do not touch, but their names do.
  const nodes = parseSkillTreeNodes().filter((n) => n.zone !== "MASTERY");
  let closest = Infinity, pair = "";
  for (let i = 0; i < nodes.length; i++) {
    for (let j = i + 1; j < nodes.length; j++) {
      const a = nodeBox(nodes[i]), b = nodeBox(nodes[j]);
      const gap = Math.max(Math.max(a.x0, b.x0) - Math.min(a.x1, b.x1),
                           Math.max(a.y0, b.y0) - Math.min(a.y1, b.y1));
      need(gap >= 0,
           `"${nodes[i].name}" and "${nodes[j].name}" overlap once their names are counted`);
      if (gap < closest) { closest = gap; pair = `${nodes[i].name}/${nodes[j].name}`; }
    }
  }
  return `closest ${pair} at ${closest.toFixed(0)} units`;
});

check("skill tree: a link always has room to leave its parent's name", () => {
  // Both renderers start a link where it exits the PARENT's box, name included, and
  // clamp it so it cannot overshoot the child. When the two nodes sit closer than
  // parent-tile + name + child-tile, that clamp bites and the link is drawn starting
  // inside the parent's own name — which is what the CORE column did at 104 units.
  const nodes = parseSkillTreeNodes().filter((n) => n.zone !== "MASTERY");
  const byKey = new Map(nodes.map((n) => [n.key, n]));
  let tightest = Infinity, who = "none";
  for (const child of nodes) {
    for (const pk of child.prereq || []) {
      const parent = pk && byKey.get(pk);
      if (!parent) continue;
      const b = nodeBox(parent);
      const dx = child.x - parent.x, dy = child.y - parent.y, len = Math.hypot(dx, dy);
      const hits = [];
      if (dx > 0) hits.push((b.x1 - parent.x) / dx);
      if (dx < 0) hits.push((b.x0 - parent.x) / dx);
      if (dy > 0) hits.push((b.y1 - parent.y) / dy);
      if (dy < 0) hits.push((b.y0 - parent.y) / dy);
      const slack = (len - NODE_R * child.scale) - (hits.length ? Math.min(...hits) : 0) * len;
      need(slack >= 0, `"${parent.name}" -> "${child.name}" has no room: the link would ` +
                       `start ${(-slack).toFixed(0)} units inside "${parent.name}"'s own name`);
      if (slack < tightest) { tightest = slack; who = `${parent.name} -> ${child.name}`; }
    }
  }
  return `tightest ${who} with ${tightest.toFixed(0)} units`;
});

check("skill tree: no link is drawn through an unrelated node", () => {
  // A link that passes over a third node reads as a gate that does not exist — the
  // hub reaches Swift Blood straight through System Analysis. The renderers bow those
  // links around the obstacle; this counts them, and fails if one is so deeply buried
  // that a bow could not read as a detour.
  const nodes = parseSkillTreeNodes().filter((n) => n.zone !== "MASTERY");
  const byKey = new Map(nodes.map((n) => [n.key, n]));
  const bowed = [];
  for (const child of nodes) {
    for (const pk of child.prereq || []) {
      const parent = pk && byKey.get(pk);
      if (!parent) continue;
      const dx = child.x - parent.x, dy = child.y - parent.y, len = Math.hypot(dx, dy) || 1;
      for (const o of nodes) {
        if (o === parent || o === child) continue;
        const t = Math.max(0, Math.min(1,
          ((o.x - parent.x) * dx + (o.y - parent.y) * dy) / (len * len)));
        const d = Math.hypot(o.x - (parent.x + dx * t), o.y - (parent.y + dy * t));
        const r = NODE_R * o.scale;
        if (d < r + 10) {
          need(d > r * 0.3, `"${parent.name}" -> "${child.name}" runs almost dead-centre ` +
                            `through "${o.name}" — no bow can make that read as a detour`);
          bowed.push(`${parent.name}->${child.name} around ${o.name}`);
        }
      }
    }
  }
  return bowed.length ? `${bowed.length} bowed: ${bowed.join(", ")}` : "all links are straight";
});

/* -- 3. shop catalog ------------------------------------------------------- */

check("shop: visible catalog matches the playground", () => {
  const cpp = parseShopCatalog();
  const visible = cpp.filter((e) => e.kind !== "OurItem" || e.localID);
  const mock = parseMockShop();
  need(visible.length === mock.length,
       `${visible.length} visible C++ entries vs ${mock.length} in the playground`);
  visible.forEach((e, i) => {
    need(mock[i].shelf.toLowerCase() === e.shelf.toLowerCase(),
         `"${e.name}" is on ${e.shelf} in C++ but ${mock[i].shelf} in the playground`);
    for (const f of ["name", "qty", "cost", "icon"]) {
      need(e[f] === mock[i][f], `entry ${i}.${f}: C++ "${e[f]}" vs playground "${mock[i][f]}"`);
    }
  });
  const hidden = cpp.length - visible.length;
  return `${visible.length} visible, ${hidden} awaiting an ESP record`;
});

check("shop: every visible card has its icon file", () => {
  const visible = parseShopCatalog().filter((e) => e.kind !== "OurItem" || e.localID);
  for (const e of visible) {
    need(existsSync(join(ROOT, "icons", e.icon)), `icons/${e.icon} is missing`);
  }
  return `${visible.length} icons present`;
});

check("shop: potions have an icon before they have an ESP id", () => {
  // Two independent things gate a potion card: its artwork and its ESP record. Having
  // the icon without the record is the normal pending state. Having the RECORD without
  // the icon is the bug — the card goes live and renders blank.
  const pots = parseShopCatalog().filter((e) => e.kind === "OurItem");
  need(pots.length > 0, "no potion entries parsed — parser stale?");
  for (const e of pots) {
    if (e.localID) {
      need(existsSync(join(ROOT, "icons", e.icon)),
           `${e.name} has an ESP id but no icons/${e.icon} — its card would be blank`);
    }
  }
  const icons = pots.filter((e) => existsSync(join(ROOT, "icons", e.icon))).length;
  const wired = pots.filter((e) => e.localID).length;
  return `${pots.length} potions: ${icons} icons ready, ${wired} ESP records` +
         (wired < pots.length ? ` — ${pots.length - wired} card(s) still hidden` : "");
});

/* -- UI chrome icons --------------------------------------------------------
   The skill tree and the shop name their icons in a table this file already
   parses. Everything else — the status panel's buttons, the blessing choice, the
   rank insignia — names them inline in C++ or in the web view, and the rank ones
   are not named at all but derived from a letter. Those are the ones that go
   missing quietly: a missing PNG draws nothing at all, which looks exactly like a
   button that was never added. */

function cppSources() {
  const out = [];
  (function walk(dir) {
    for (const e of readdirSync(join(ROOT, dir), { withFileTypes: true })) {
      const rel = `${dir}/${e.name}`;
      if (e.isDirectory()) walk(rel);
      else if (e.name.endsWith(".cpp")) out.push(rel);
    }
  })("src");
  return out;
}

check("ui: every icon a C++ file names by hand exists", () => {
  // Two spellings, because only one of them is a literal path. The status buttons write
  // the whole thing out ("…icons\\ui_shop.png"); the blessing choices pass a bare file
  // name to BlessingIcon(), which prepends the folder. The first version of this check
  // only knew the literal form and so silently covered 3 icons instead of 10 — it would
  // have passed with every blessing PNG missing, which is precisely the case it exists
  // for. Add a helper here whenever a new one is introduced.
  //
  // Comments are stripped first: Prisma.cpp explains its path helper with a
  // "…\\icons\\x.png" example, and an example is not a missing file.
  const strip = (s) => s.replace(/\/\*[\s\S]*?\*\//g, "").replace(/\/\/[^\n]*/g, "");
  const patterns = [/icons\\\\([\w.]+\.png)/g, /BlessingIcon\("([\w.]+\.png)"\)/g];
  const found = new Map();  // icon -> file that names it
  for (const f of cppSources()) {
    const src = strip(read(f));
    for (const re of patterns) {
      for (const m of src.matchAll(re)) {
        if (!found.has(m[1])) found.set(m[1], f);
      }
    }
  }
  need(found.size > 0, "no inline icon literals parsed — parser stale?");
  for (const [icon, file] of found) {
    need(existsSync(join(ROOT, "icons", icon)),
         `icons/${icon} is missing — ${file} draws nothing where it expects art`);
  }
  // Guard the guard: the blessing set is the one that vanished from this check once.
  const blessings = [...found.keys()].filter((n) => n.startsWith("blessing_"));
  need(blessings.length >= 7,
       `only ${blessings.length} blessing icons seen — the BlessingIcon parser is stale`);
  return `${found.size} icons (${blessings.length} blessings)`;
});

check("ui: the web view's status buttons name real icons", () => {
  const html = read("prisma-patch/PrismaUI/views/IsekaiHero/index.html");
  const icons = [...html.matchAll(/addIcon\([^,]+,\s*"([\w.]+\.png)"/g)].map((m) => m[1]);
  need(icons.length > 0, "no addIcon calls parsed — parser stale?");
  for (const i of icons) {
    need(existsSync(join(ROOT, "icons", i)), `icons/${i} is missing — that button renders bare`);
  }
  return `${icons.length} buttons`;
});

check("ui: every rank SystemRank can return has an insignia", () => {
  // Neither renderer names these files: both build "rank_<letter>.png" from the
  // letter. Retune the thresholds and add a rank, and nothing but this check
  // notices that its emblem was never drawn.
  const cpp = read("src/Progression.cpp");
  const from = cpp.indexOf("std::string SystemRank()");
  need(from >= 0, "SystemRank not found in Progression.cpp");
  const body = cpp.slice(from, cpp.indexOf("\n    }", from));
  const letters = [...new Set([...body.matchAll(/return "(\w)";/g)].map((m) => m[1]))];
  need(letters.length > 0, "no rank letters parsed — parser stale?");
  for (const l of letters) {
    const icon = `rank_${l.toLowerCase()}.png`;
    need(existsSync(join(ROOT, "icons", icon)), `rank ${l} has no icons/${icon}`);
  }
  return `${letters.join("")} — ${letters.length} insignia`;
});

check("ui: the playground previews every blessing icon the game uses", () => {
  // The blessing screen is the one panel a preview genuinely earns: five icon+label
  // buttons in one row, and the longest body text in the mod. A preview that has
  // drifted from the real icon set is worse than none — it would show a layout nobody
  // ever sees.
  const cpp = read("src/System.cpp");
  const used = new Set([...cpp.matchAll(/BlessingIcon\("([\w.]+\.png)"\)/g)].map((m) => m[1]));
  need(used.size > 0, "no BlessingIcon calls in System.cpp — parser stale?");

  const pg = read("playground/index.html");
  const shown = new Set([...pg.matchAll(/icon:\s*"(blessing_[\w.]+\.png)"/g)].map((m) => m[1]));
  need(shown.size > 0, "the playground previews no blessing panel at all");

  const missing = [...used].filter((i) => !shown.has(i));
  need(missing.length === 0,
       `the playground never shows ${missing.join(", ")} — that icon cannot be checked ` +
       `without starting Skyrim`);
  return `${used.size} icons, all previewable`;
});

check("ui: the panel's button labels are wrapped in the element their CSS styles", () => {
  // The five blessing buttons could not fit icon and label side by side, so both
  // renderers stack them and the view clips an over-long label with an ellipsis. That
  // clipping is a rule on a .lbl span — if buildButtons ever goes back to emitting a
  // bare text node, the rule silently stops applying and the label runs off the button
  // again, which is exactly the bug it was written for.
  const view = read("prisma-patch/PrismaUI/views/IsekaiHero/index.html");
  need(/#pButtons \.btn \.lbl\s*\{/.test(view),
       "no #pButtons .btn .lbl rule — the label clipping is gone");
  need(/#pButtons \.btn\s*\{[^}]*min-width:\s*0/.test(view),
       "#pButtons .btn no longer sets min-width: 0, so a flex button cannot shrink " +
       "below its content and the row overflows the panel instead");
  need(/#pButtons \.btn\s*\{[^}]*flex-direction:\s*column/.test(view),
       "#pButtons .btn is no longer stacked; icon beside label does not fit five choices");
  const from = view.indexOf("function buildButtons()");
  need(from >= 0, "buildButtons not found in the view — parser stale?");
  const build = view.slice(from, view.indexOf("\nfunction ", from + 1));
  need(/class="lbl">'\s*\+\s*esc\(b\.label\)/.test(build),
       "buildButtons emits a row label that is not inside a .lbl span");
  return "stacked, clipped, and the span exists";
});

check("ui: both renderers colour a System Rank the same", () => {
  // The rank ramp is written twice — once as ImVec4 in Style::RankColor, once as CSS in
  // the view — because neither renderer can read the other's. Two screens showing the
  // same character's rank in different colours would be worse than neither having any:
  // the colour is the part you are meant to trust without reading the letter.
  const style = read("src/UI/Style.h");
  const cpp = {};
  for (const m of style.matchAll(/case '([SABCDE])':/g)) {
    const rest = style.slice(m.index);
    const ret = rest.match(/return \{ ([\d.]+)f, ([\d.]+)f, ([\d.]+)f/);
    need(ret, `no colour follows case '${m[1]}' in RankColor`);
    cpp[m[1]] = [+ret[1], +ret[2], +ret[3]].map((v) => Math.round(v * 255));
  }
  need(Object.keys(cpp).length === 6, `RankColor names ${Object.keys(cpp).length} ranks, not 6`);

  const view = read("prisma-patch/PrismaUI/views/IsekaiHero/index.html");

  // The ramp is applied as a CLASS to #sRankLetter, and an id selector outranks a class.
  // This shipped once: the six colours below were correct, agreed with the C++ to the
  // byte, and every rank still rendered white because #sRankLetter also said `color:
  // #fff`. Comparing the values alone cannot see that — so check that the id rule sets
  // no colour of its own.
  const idRule = view.match(/#sRankLetter\s*\{([^}]*)\}/);
  need(idRule, "#sRankLetter rule not found — parser stale?");
  need(!/(^|[;\s])color\s*:/.test(idRule[1]),
       "#sRankLetter sets its own colour, which outranks the .rank-* classes — " +
       "every rank will render in that colour whatever the ramp says");

  for (const [rank, rgb] of Object.entries(cpp)) {
    const css = view.match(new RegExp(`\\.rank-${rank.toLowerCase()}\\s*\\{\\s*color:\\s*#([0-9a-f]{6})`, "i"));
    need(css, `the view has no .rank-${rank.toLowerCase()} colour`);
    const hex = [0, 2, 4].map((i) => parseInt(css[1].slice(i, i + 2), 16));
    // Two levels of slack for the float-to-byte rounding, and nothing more: a real
    // divergence is never off by one.
    const off = hex.map((v, i) => Math.abs(v - rgb[i]));
    need(Math.max(...off) <= 2,
         `rank ${rank}: C++ says rgb(${rgb}), the view #${css[1]} — they have drifted`);
  }
  return `${Object.keys(cpp).length} ranks agree`;
});

check("ui: the web view's rank insignia is actually on screen", () => {
  // It was not, for as long as it existed. The status chip carried a bare "rank" class,
  // and the skill tree's node pip owns an unscoped `.rank { position: absolute;
  // bottom: -7px; right: -7px }` — so the chip was pinned to the bottom-right corner of
  // the status card. Nothing about that is a syntax error and both screens still
  // "worked", which is exactly why it went unnoticed until the chip gained an emblem.
  const view = read("prisma-patch/PrismaUI/views/IsekaiHero/index.html");
  need(!/^\s*\.rank\s*\{/m.test(view),
       "an unscoped `.rank {` rule is back — it will also match the status screen's " +
       "rank element. Scope it to `.node .rank`.");
  for (const id of ["sRankIcon", "sRankLetter"]) {
    need(new RegExp(`id="${id}"`).test(view), `the status screen lost #${id}`);
    need(new RegExp(`\\$\\("${id}"\\)`).test(view), `nothing fills #${id} at render time`);
  }
  return "scoped, and both parts are rendered";
});

check("icons: every shipped PNG is 512x512", () => {
  // The generator hands back whatever resolution it likes; the first batch of UI art
  // arrived at 1024 and 2048, which is 16 MB of archive for pixels nothing ever
  // samples (the largest of these draws at 76px, the rank badges at ~24px).
  const odd = [];
  for (const name of readdirSync(join(ROOT, "icons")).filter((f) => f.endsWith(".png"))) {
    const b = readFileSync(join(ROOT, "icons", name));
    need(b.length > 24 && b.readUInt32BE(0) === 0x89504e47, `icons/${name} is not a PNG`);
    const w = b.readUInt32BE(16), h = b.readUInt32BE(20);
    if (w !== 512 || h !== 512) odd.push(`${name} (${w}x${h})`);
  }
  need(odd.length === 0, `off-spec: ${odd.join(", ")}`);
  return `${readdirSync(join(ROOT, "icons")).filter((f) => f.endsWith(".png")).length} PNGs`;
});

check("quests: every quarry names a keyword the game actually has", () => {
  // "ActorTypeFalmer" shipped in the quarry table and does not exist. Nothing catches
  // that from the outside: the objective is handed out, reads perfectly, and simply
  // never counts a kill — the in-game self-test found it only because someone ran it.
  //
  // The allowed set below was read out of the masters themselves, not from memory:
  //   Skyrim.esm     14   (the full ActorType* set)
  //   Dawnguard.esm   3
  //   Dragonborn.esm  1
  //   HearthFires.esm 0
  // Falmer are ActorTypeNPC like any other humanoid, which is why no Falmer-specific
  // keyword exists — and why that quarry could not simply be re-pointed.
  const KNOWN = new Set([
    "ActorTypeAnimal", "ActorTypeCow", "ActorTypeCreature", "ActorTypeDaedra",
    "ActorTypeDragon", "ActorTypeDwarven", "ActorTypeFamiliar", "ActorTypeGhost",
    "ActorTypeGiant", "ActorTypeHorse", "ActorTypeNPC", "ActorTypePrisoner",
    "ActorTypeTroll", "ActorTypeUndead",
    "ActorTypeDLC1Boss", "ActorTypeMount", "ActorTypeVampireBrute",  // Dawnguard
    "DLC2ActorTypeMiraak",                                          // Dragonborn
  ]);

  const cpp = read("src/Quests.cpp");
  const from = cpp.indexOf("constexpr Quarry kQuarries[] = {");
  need(from >= 0, "kQuarries not found in Quests.cpp");
  const body = cpp.slice(from, cpp.indexOf("\n        };", from));
  const rows =
    [...body.matchAll(/\{\s*(\d+),\s*"([^"]+)",\s*"([^"]+)",\s*(\d+),\s*(\d+),\s*(\d+)\s*\}/g)]
      .map((m) => ({ key: +m[1], name: m[2], keyword: m[3], count: +m[4], reward: +m[5],
                     minLevel: +m[6] }));
  need(rows.length > 0, "no quarries parsed — parser stale?");

  // At least one quarry has to be reachable from level 1, or a fresh character is
  // handed nothing and the feature looks dead exactly when it is first met.
  need(rows.some((q) => q.minLevel <= 1),
       "every quarry is gated above level 1 — a new character would never get an objective");

  for (const q of rows) {
    need(KNOWN.has(q.keyword),
         `quarry ${q.key} ("${q.name}") hunts "${q.keyword}", which no master defines — ` +
         `that objective can be handed out but never completed`);
  }

  // Keys are promised never to be reused: a live objective in someone's save is just
  // this number, so re-pointing one silently changes what they were sent to kill.
  const keys = rows.map((q) => q.key);
  need(new Set(keys).size === keys.length, `duplicate quarry keys: ${keys.join(", ")}`);
  return `${rows.length} quarries, keys ${keys.join("/")}`;
});

check("quests: the first objective is one a new character can go and do", () => {
  // The roll is uniform over everything the level allows, so a fresh character had an
  // even chance of drawing "slay 25 Draugr" as their introduction to the feature — a
  // barrow crawl, before anything has explained what the System is. The first one is
  // therefore fixed, and this makes sure the key it is fixed to still means something.
  const cpp = read("src/Quests.cpp");
  const starter = cpp.match(/constexpr std::uint32_t kStarterKey = (\d+);/);
  need(starter, "kStarterKey not found in Quests.cpp");

  const from = cpp.indexOf("constexpr Quarry kQuarries[] = {");
  const body = cpp.slice(from, cpp.indexOf("\n        };", from));
  const rows =
    [...body.matchAll(/\{\s*(\d+),\s*"([^"]+)",\s*"([^"]+)",\s*(\d+),\s*(\d+),\s*(\d+)\s*\}/g)]
      .map((m) => ({ key: +m[1], name: m[2], minLevel: +m[6] }));
  const hit = rows.find((q) => q.key === +starter[1]);
  need(hit, `kStarterKey is ${starter[1]}, which no quarry claims — the first objective ` +
            `would fall back to a random one`);
  need(hit.minLevel <= 1,
       `the starter quarry "${hit.name}" needs level ${hit.minLevel}, so a level-1 ` +
       `character would be handed nothing at all`);
  return `${hit.name} (key ${hit.key})`;
});

check("config: the ini and Config::Load name the same settings", () => {
  // A setting documented in the ini that Config::Load does not parse is invisible: the
  // player writes it, the file is read, nothing happens, and no log line says why. The
  // reverse — a parsed key the ini never mentions — is a feature nobody can find.
  const ini = read("IsekaiHero.ini");
  const documented = new Set(
    ini.split("\n")
       .map((l) => l.replace(/[;#].*$/, "").trim())
       .filter((l) => l.includes("="))
       .map((l) => l.slice(0, l.indexOf("=")).trim().toLowerCase())
       .filter(Boolean));
  const parsed = new Set(
    [...read("src/Config.cpp").matchAll(/key == "([a-z]+)"/g)].map((m) => m[1]));

  need(documented.size > 0, "no settings parsed out of IsekaiHero.ini — parser stale?");
  need(parsed.size > 0, "no key comparisons found in Config.cpp — parser stale?");

  const dead = [...documented].filter((k) => !parsed.has(k));
  need(dead.length === 0,
       `the ini documents ${dead.join(", ")}, which Config::Load never reads — ` +
       `setting it does nothing at all`);
  const hidden = [...parsed].filter((k) => !documented.has(k));
  need(hidden.length === 0,
       `Config::Load reads ${hidden.join(", ")}, which the shipped ini never mentions`);
  return `${documented.size} settings`;
});

check("shop: every entry names a shelf that exists", () => {
  const names = new Set(parseShelfNames());
  const cpp = parseShopCatalog();
  for (const e of cpp) {
    // The table writes Shelf::kElixirs; kShelfNames holds the display string. Compare
    // case-insensitively rather than duplicating the mapping here.
    const hit = [...names].some((n) => n.toLowerCase() === e.shelf.toLowerCase());
    need(hit, `"${e.name}" is on shelf ${e.shelf}, which kShelfNames does not list`);
  }
  const used = new Set(cpp.map((e) => e.shelf.toUpperCase()));
  const empty = [...names].filter((n) => !used.has(n));
  need(empty.length === 0, `shelf ${empty.join(", ")} holds nothing — it would render blank`);
  return `${names.size} shelves, ${cpp.length} entries`;
});

check("shop: the shelf list agrees across C++, the view and the playground", () => {
  const cpp = parseShelfNames();
  const mock = read("playground/mock.js").match(/SHOP_SHELVES\s*=\s*\[([^\]]*)\]/);
  need(mock, "SHOP_SHELVES not found in playground/mock.js");
  const mockNames = [...mock[1].matchAll(/"([^"]+)"/g)].map((m) => m[1]);
  need(cpp.join("|") === mockNames.join("|"),
       `C++ offers [${cpp.join(", ")}], the playground [${mockNames.join(", ")}]`);
  // The view builds its rail from the pushed list, so it must not hardcode any of them.
  const view = read("prisma-patch/PrismaUI/views/IsekaiHero/index.html");
  need(/SHOP\.shelves/.test(view), "the view no longer reads SHOP.shelves — rail parser stale?");
  return cpp.join(" / ");
});

check("shop: the built-in window still fits a 1080p screen", () => {
  // The ImGui shop sizes itself from the catalog, so growing the catalog grows the
  // window. With the potions wired it reached 18 cards, and at the original four columns
  // that was a 1454px-tall window on a 1080p display - taller than the screen, with no
  // scrolling to fall back on. Now it sizes from the BIGGEST SHELF rather than the whole
  // catalog (so switching category never resizes it) plus the rail.
  const src = read("src/UI/ShopWindow.cpp");
  const num = (re, what) => {
    const m = src.match(re);
    need(m, `could not read ${what} from ShopWindow.cpp`);
    return parseFloat(m[1]);
  };
  const cardW = num(/kCardW = ([\d.]+)f/, "kCardW");
  const cardH = num(/kCardH = ([\d.]+)f/, "kCardH");
  const gap = num(/kCardGap = ([\d.]+)f/, "kCardGap");
  const pad = num(/kPad = ([\d.]+)f/, "kPad");
  const head = num(/kHeadH = ([\d.]+)f/, "kHeadH");
  const foot = num(/kFootH = ([\d.]+)f/, "kFootH");
  const railW = num(/kRailW = ([\d.]+)f/, "kRailW");
  const railGap = num(/kRailGap = ([\d.]+)f/, "kRailGap");
  const railRowH = num(/kRailRowH = ([\d.]+)f/, "kRailRowH");
  const cols = num(/kCols = (\d+)/, "kCols");

  const live = parseShopCatalog().filter((e) => e.kind !== "OurItem" || e.localID);
  const shelves = parseShelfNames();
  const widest = Math.max(1, ...shelves.map(
    (n) => live.filter((e) => e.shelf.toUpperCase() === n).length));
  const rows = Math.max(1, Math.ceil(widest / cols));
  const w = pad * 2 + railW + railGap + cols * cardW + (cols - 1) * gap;
  const h = head + rows * cardH + (rows - 1) * gap + foot;

  need(w <= 1920 && h <= 1080,
       `${widest} cards on the biggest shelf makes a ${w}x${h} window, which does not fit ` +
       `1920x1080`);
  // The rail lives in the same vertical band as the cards; more shelves than fit there
  // would run off the bottom, and it does not scroll.
  const railH = shelves.length * railRowH;
  need(railH <= h - head - foot + rows * cardH,
       `${shelves.length} shelves need ${railH} units of rail, more than the card area offers`);
  return `${live.length} cards, biggest shelf ${widest} -> ${cols}x${rows} -> ${w}x${h}`;
});

/* -- 4. cross-layer JSON contracts ----------------------------------------- */

function contract(label, cppFile, prefix) {
  const cpp = read(cppFile);
  const view = read("prisma-patch/PrismaUI/views/IsekaiHero/index.html");
  const pg = read("playground/index.html");

  const emitted = [...cpp.matchAll(new RegExp(`"\\\\"(${prefix}\\w+)\\\\":`, "g"))].map((m) => m[1]);
  const readBack = [...new Set([...view.matchAll(new RegExp(`\\bs\\.(${prefix}\\w+)`, "g"))].map((m) => m[1]))];
  const mocked = [...new Set([...pg.matchAll(new RegExp(`\\b(${prefix}\\w+):`, "g"))].map((m) => m[1]))];

  need(emitted.length > 0, `${label}: C++ emits no ${prefix}* fields — parser stale?`);
  for (const f of readBack) need(emitted.includes(f), `${label}: view reads ${f}, C++ never sends it`);
  for (const f of emitted) need(readBack.includes(f), `${label}: C++ sends ${f}, view ignores it`);
  for (const f of readBack) need(mocked.includes(f), `${label}: playground lacks ${f}, so its preview lies`);
  return `${emitted.length} fields`;
}

check("status JSON: quest fields agree across C++, view and playground",
      () => contract("quest", "src/Progression.cpp", "quest"));

/* -- 5. serialization ------------------------------------------------------ */

check("co-save: kVersion matches the highest version gate", () => {
  const sys = read("src/System.cpp");
  const kv = sys.match(/constexpr std::uint32_t kVersion = (\d+);/);
  need(kv, "kVersion not found");
  const gates = [...sys.matchAll(/version >= (\d+)/g)].map((m) => +m[1]);
  need(gates.length > 0, "no version gates found — parser stale?");
  const highest = Math.max(...gates);
  need(+kv[1] === highest,
       `kVersion is ${kv[1]} but the highest read gate is ${highest} — ` +
       `a field was added without bumping, or bumped without being read`);
  return `v${kv[1]}`;
});

check("co-save: every state field written is also read", () => {
  const sys = read("src/System.cpp");
  const written = [...sys.matchAll(/WriteRecordData\(g_state\.(\w+)\)/g)].map((m) => m[1]);
  const readF = new Set([...sys.matchAll(/ReadRecordData\(g_state\.(\w+)\)/g)].map((m) => m[1]));
  need(written.length > 0, "no WriteRecordData(g_state.*) found — parser stale?");
  for (const f of written) {
    need(readF.has(f), `g_state.${f} is saved but never loaded`);
  }
  return `${written.length} fields round-trip`;
});

/* -- 6. the ESP's form IDs -------------------------------------------------
   Four separate tables in three files name local FormIDs in IsekaiHero.esp. They
   must not collide: two features pointing at one record means one of them is
   wired to the wrong thing, and nothing about that is visible at runtime — both
   resolve, both look fine, one behaves oddly. */

function espFormIds() {
  const grab = (file, re) =>
    [...read(file).matchAll(re)].map((m) => ({ id: parseInt(m[1], 16), file, note: m[2] || "" }));

  const ids = [
    ...grab("src/Passives.cpp", /^\s*(0x000[0-9A-Fa-f]{3}),\s*\/\/\s*(.+)$/gm),
    ...grab("src/Sounds.cpp", /^\s*(0x000[0-9A-Fa-f]{3}),\s*\/\/\s*(.+)$/gm),
    ...grab("src/Storage.cpp", /constexpr RE::FormID \w+ = (0x000[0-9A-Fa-f]{3});()/g),
  ];
  // Potion IDs are 0 until their records exist; a real one joins the collision check.
  const shop = [...read("src/Shop.cpp").matchAll(/Cat::k\w+,\s*\d+,\s*Shelf::k\w+,\s*(0x[0-9A-Fa-f]+)\s*\}/g)]
    .map((m) => parseInt(m[1], 16))
    .filter((v) => v !== 0)
    .map((v) => ({ id: v, file: "src/Shop.cpp", note: "potion" }));
  return [...ids, ...shop];
}

check("ESP: no two features claim the same form ID", () => {
  const all = espFormIds();
  need(all.length > 0, "no ESP form IDs parsed — parser stale?");
  const byId = new Map();
  for (const e of all) {
    const prev = byId.get(e.id);
    need(!prev,
         `0x${e.id.toString(16).toUpperCase().padStart(6, "0")} is claimed by both ` +
         `${prev?.file} (${prev?.note}) and ${e.file} (${e.note})`);
    byId.set(e.id, e);
  }
  return `${all.length} ids, all distinct`;
});

/* Read plugin/IsekaiHero.esp itself. Everything above compares source against source;
   this compares source against the actual plugin, which is the only way to catch a
   FormID the code names but the ESP does not have (or vice versa). */
function parseEsp() {
  const buf = readFileSync(join(ROOT, "plugin/IsekaiHero.esp"));
  need(buf.subarray(0, 4).toString("ascii") === "TES4", "plugin/IsekaiHero.esp is not a TES4 file");
  const hdrSize = buf.readUInt32LE(4);
  const flags = buf.readUInt32LE(8);
  const light = (flags & 0x200) !== 0;

  const edidOf = (body) => {
    let i = 0;
    while (i + 6 <= body.length) {
      const sig = body.subarray(i, i + 4).toString("ascii");
      const size = body.readUInt16LE(i + 4);
      if (sig === "EDID") return body.subarray(i + 6, i + 6 + size).toString("ascii").replace(/\0.*$/, "");
      i += 6 + size;
    }
    return "";
  };

  const recs = [];
  const walk = (off, end) => {
    while (off + 24 <= end) {
      const sig = buf.subarray(off, off + 4).toString("ascii");
      if (sig === "GRUP") {
        const gsize = buf.readUInt32LE(off + 4);
        if (gsize < 24) return;
        walk(off + 24, off + gsize);
        off += gsize;
      } else {
        const dsize = buf.readUInt32LE(off + 4);
        const rflags = buf.readUInt32LE(off + 8);
        const fid = buf.readUInt32LE(off + 12);
        // Compressed records are rare here and we only need the EDID; skip if so.
        const body = (rflags & 0x00040000) ? Buffer.alloc(0) : buf.subarray(off + 24, off + 24 + dsize);
        recs.push({ sig, formID: fid, local: fid & 0x00ffffff, edid: edidOf(body) });
        off += 24 + dsize;
      }
    }
  };
  walk(24 + hdrSize, buf.length);
  need(recs.length > 0, "no records parsed from the ESP — parser stale?");
  return { light, recs };
}

check("ESP: every FormID is valid for a light plugin", () => {
  const { light, recs } = parseEsp();
  if (!light) {
    return `${recs.length} records (plugin is NOT ESL-flagged, so any local id is fine)`;
  }
  // A light plugin has 12 bits of local FormID. A record above 0xFFF is remapped by the
  // engine to its low 12 bits, so it lands somewhere other than where the file says -
  // it may happen to work, but xEdit flags it and any FormID compaction will move it.
  const bad = recs.filter((r) => r.local > 0xfff);
  need(bad.length === 0,
       `${bad.length} record(s) outside the ESL range (max 0xFFF): ` +
       bad.map((r) => `${r.edid || r.sig} 0x${r.local.toString(16).toUpperCase()}`).join(", "));
  return `${recs.length} records, all <= 0xFFF`;
});

check("ESP: every FormID the code names exists in the plugin", () => {
  const { recs } = parseEsp();
  const byLocal = new Map(recs.map((r) => [r.local, r]));

  // Everywhere the source hardcodes a local FormID from our own ESP.
  const named = [];
  const add = (file, re, label) => {
    for (const m of read(file).matchAll(re)) {
      const id = parseInt(m[1], 16);
      if (id !== 0) named.push({ id, file, label: label || m[2] || "" });
    }
  };
  add("src/Passives.cpp", /^\s*(0x000[0-9A-Fa-f]{3}),\s*\/\/\s*(.+)$/gm);
  add("src/Sounds.cpp", /^\s*(0x000[0-9A-Fa-f]{3}),\s*\/\/\s*(.+)$/gm);
  add("src/Storage.cpp", /constexpr RE::FormID (?:\w+) = (0x000[0-9A-Fa-f]{3});/g, "storage form");
  add("src/Shop.cpp", /Cat::k\w+,\s*\d+,\s*Shelf::k\w+,\s*(0x[0-9A-Fa-f]+)\s*\}/g, "shop item");
  need(named.length > 0, "no ESP FormIDs found in the source — parser stale?");

  const missing = named.filter((n) => !byLocal.has(n.id));
  need(missing.length === 0,
       `the code references ${missing.length} form(s) the ESP does not contain: ` +
       missing.map((n) => `0x${n.id.toString(16).toUpperCase().padStart(6, "0")} ` +
                          `(${n.label.trim()}, ${n.file})`).join("; "));
  return `${named.length} referenced forms, all present`;
});

check("ESP: the ability and sound tables have their expected sizes", () => {
  // The self-test compares these counts at runtime; if the tables themselves change,
  // that comparison silently starts measuring the wrong thing.
  const abilities = [...read("src/Passives.cpp")
    .matchAll(/^\s*0x000[0-9A-Fa-f]{3},\s*\/\//gm)].length;
  const sounds = [...read("src/Sounds.cpp")
    .matchAll(/^\s*0x000[0-9A-Fa-f]{3},\s*\/\//gm)].length;
  const selfTest = read("src/SelfTest.cpp");
  const expected = selfTest.match(/kExpectedAbilities = (\d+)/);
  need(expected, "SelfTest.cpp no longer states kExpectedAbilities");
  need(abilities === +expected[1],
       `Passives lists ${abilities} abilities but the self-test expects ${expected[1]}`);
  need(sounds === 4, `Sounds lists ${sounds} descriptors, expected 4`);
  return `${abilities} abilities, ${sounds} sounds`;
});

/* -- 7. milestones --------------------------------------------------------- */

check("milestones: keys and quest editor IDs are unique", () => {
  const cpp = read("src/Progression.cpp");
  const from = cpp.indexOf("constexpr Milestone kMilestones[] = {");
  need(from >= 0, "kMilestones table not found");
  const body = cpp.slice(from, cpp.indexOf("\n        };", from));

  const keys = [...body.matchAll(/\{\s*(\d+),\s*"/g)].map((m) => +m[1]);
  need(keys.length > 0, "no milestones parsed — parser stale?");
  const dupeKeys = keys.filter((k, i) => keys.indexOf(k) !== i);
  need(dupeKeys.length === 0, `duplicate milestone keys: ${[...new Set(dupeKeys)].join(", ")}`);

  // Milestone is { key, editorID, questName, ... } — the editor ID is the FIRST quoted
  // string, not the second. Getting that wrong made this check flag "Prophet" and "The
  // Jagged Crown", which are legitimately shared DISPLAY names: DLC1VQ03Hunter and
  // DLC1VQ03Vampire are the two Dawnguard branches, CW02A/CW02B the two civil war sides.
  // Duplicate display names are expected; duplicate editor IDs are the bug.
  const ids = [...body.matchAll(/\{\s*\d+,\s*"([^"]+)"/g)].map((m) => m[1]);
  need(ids.length === keys.length, `parsed ${keys.length} keys but ${ids.length} editor IDs`);
  const dupeIds = ids.filter((v, i) => ids.indexOf(v) !== i);
  need(dupeIds.length === 0,
       `two milestones watch the same quest, so one can never fire: ` +
       `${[...new Set(dupeIds)].join(", ")}`);
  return `${keys.length} milestones`;
});

check("milestones: no more distinct actor values than there are abilities", () => {
  // Each passive is delivered by an ESP ability spell, one per actor value. If the
  // milestone table names MORE distinct actor values than there are ability spells, at
  // least one passive cannot possibly be backed — and an unbacked passive grants nothing,
  // silently. This is pure arithmetic, so it holds without knowing which actor value each
  // ability actually carries (only a running game knows that; the self-test names it).
  const cpp = read("src/Progression.cpp");
  const from = cpp.indexOf("constexpr Milestone kMilestones[] = {");
  const body = cpp.slice(from, cpp.indexOf("\n        };", from));
  const avs = new Set([...body.matchAll(/AV::k(\w+)/g)].map((m) => m[1]));
  need(avs.size > 0, "no actor values parsed from the milestone table — parser stale?");

  const abilities = [...read("src/Passives.cpp")
    .matchAll(/^\s*0x000[0-9A-Fa-f]{3},\s*\/\//gm)].length;
  need(abilities > 0, "no ability form IDs parsed — parser stale?");

  need(avs.size <= abilities,
       `milestones grant ${avs.size} distinct actor values (${[...avs].sort().join(", ")}) ` +
       `but the ESP has only ${abilities} ability spells — at least one passive is dead. ` +
       `Add an ability for the missing actor value, or stop granting it.`);
  return `${avs.size} actor values, ${abilities} abilities`;
});

/* -- 8. build wiring ------------------------------------------------------- */

check("build: every src file is listed in CMakeLists", () => {
  const cmake = read("CMakeLists.txt");
  const walk = (dir, acc = []) => {
    for (const e of readdirSync(join(ROOT, dir))) {
      const rel = `${dir}/${e}`;
      if (statSync(join(ROOT, rel)).isDirectory()) walk(rel, acc);
      else if (/\.(cpp|h)$/.test(e)) acc.push(rel);
    }
    return acc;
  };
  const missing = walk("src").filter((f) => !cmake.includes(f) && !f.endsWith("PCH.h"));
  need(missing.length === 0, `not in CMakeLists: ${missing.join(", ")}`);
  return "all listed";
});

/* -- report ---------------------------------------------------------------- */

const pad = Math.max(...results.map((r) => r.name.length));
for (const r of results) {
  console.log(`${r.ok ? "  ok  " : "FAIL  "}${r.name.padEnd(pad)}  ${r.detail}`);
}
console.log(`\n${checks - failures}/${checks} checks passed`);
process.exit(failures ? 1 : 0);
