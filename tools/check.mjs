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
import { PRESETS, STARTS, HUDS, build as buildPresets } from "./make-presets.mjs";
import { collect as collectStrings, template as stringTemplate } from "./extract-strings.mjs";

const ROOT = dirname(dirname(fileURLToPath(import.meta.url)));
const read = (p) => readFileSync(join(ROOT, p), "utf8");

/* The Magelight pages. The system page's markup, its styles and the panel's script live
   in system.html; the status, skill tree and shop screens each in screens/<name>.js; the
   projection theme and the helpers every page shares (translation, scale, typewriter,
   host messages) next to it. A rule about "the page" means all of them together. */
const SYSTEM_PAGE = "magelight/IsekaiHero/system.html";
const PAGE_THEME = "magelight/IsekaiHero/theme.css";
const PAGE_COMMON = "magelight/IsekaiHero/common.js";
const PAGE_SCREENS = "magelight/IsekaiHero/screens";
const HUD_PAGE = "magelight/IsekaiHero/hud.html";
const screenScripts = () => existsSync(join(ROOT, PAGE_SCREENS))
  ? readdirSync(join(ROOT, PAGE_SCREENS)).filter((n) => n.endsWith(".js")).sort().map((n) => `${PAGE_SCREENS}/${n}`)
  : [];
const systemPage = () => [SYSTEM_PAGE, PAGE_THEME, PAGE_COMMON, ...screenScripts()].map(read).join("\n");

let failures = 0;
let checks = 0;
const results = [];

function check(name, fn) {
  checks++;
  try {
    const detail = fn();
    // An async check would return a Promise here, its assertions would run after this
    // file had already printed its summary, and a failing one would surface as an
    // unhandled rejection long after "40/40 checks passed". A check that cannot fail is
    // worse than no check, so refuse the shape outright.
    if (detail && typeof detail.then === "function") {
      fail("this check is async — check() runs synchronously, so nothing it asserts " +
           "would ever be reported. Import at the top of the file instead.");
    }
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
  // Field 1 is the translation key (see Entry in Shop.cpp); the display name follows it.
  const re = /\{\s*"([\w.]+)",\s*"([^"]+)",\s*"([^"]+)",\s*(\d+),\s*\n?\s*"([^"]+)",\s*\n?\s*Kind::k(\w+),\s*Cat::k\w+,\s*([\d']+),\s*Shelf::k(\w+)\s*(?:,\s*(0x[0-9A-Fa-f]+))?\s*\}/g;
  const out = [...body.matchAll(re)].map((m) => ({
    key: m[1], name: m[2], qty: m[3], cost: +m[4], icon: m[5],
    kind: m[6], amount: m[7], shelf: m[8], localID: m[9] ? parseInt(m[9], 16) : null,
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
  // `fx` is optional and comes last: the material packs have no magic effects, and in
  // game the field is read off the form rather than written down, so nothing here can be
  // compared against C++ — it only has to not break the parse of the fields that can.
  const re = /\{\s*name:\s*"([^"]+)",\s*qty:\s*"([^"]+)",\s*cost:\s*(\d+),\s*icon:\s*"([^"]+)",\s*shelf:\s*"([^"]+)"(?:,\s*fx:\s*"(?:[^"\\]|\\.)*")?\s*\}/g;
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
  const onlyMock = [...mock.keys()].filter((k) => !cpp.has(k));
  need(onlyMock.length === 0, `keys ${onlyMock.join(", ")} are in the playground mock but missing in SkillTree.cpp`);
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

check("storage: the chest's position is logged after open and at close", () => {
  // #38: players see the chest standing next to them. These two log lines decide whether
  // the engine's queued half of MoveTo lands after the SetPosition in Open().
  const src = read("src/Storage.cpp");
  const open = src.indexOf("void Open(");
  const act = src.indexOf("ActivateRef(", open);
  need(open >= 0 && act >= 0, "Open() / its ActivateRef call not found");
  need(src.indexOf('LogChestPosition(chest, "after open")', act) > act,
       'Open() must call LogChestPosition(chest, "after open") after ActivateRef');
  const w = src.indexOf("class ChestWatcher");
  need(w >= 0, "ChestWatcher sink not found");
  need(src.indexOf("LogChestPosition(", w) > w, "ChestWatcher must call LogChestPosition");
});

check("config: an unmapped key (-1 from the MCM) reads as 0, not 0xFFFFFFFF", () => {
  // #37: SkyUI stores -1 for "unmapped". stoul("-1") wraps to 0xFFFFFFFF, which every Bind*
  // would take for a real key. AsScanCode must parse signed and map negatives to 0.
  const src = read("src/Config.cpp");
  const at = src.indexOf("std::uint32_t AsScanCode(");
  need(at >= 0, "AsScanCode not found");
  const end = src.indexOf("[[nodiscard]]", at);  // the next helper; CRLF-safe
  const body = src.slice(at, end);
  need(!/stoul\(/.test(body), "AsScanCode must not parse the raw value with stoul");
  need(/stoll\(/.test(body), "AsScanCode must parse signed (stoll) first");
  need(/<\s*0\s*\)\s*\{\s*return 0;/.test(body), "AsScanCode needs a negative branch that returns 0");
});

check("storage: the chest watcher is not registered on VR", () => {
  // Its menu-target lookups resolve through 2-argument relocation IDs (docs/VR.md).
  const src = read("src/Storage.cpp");
  const reg = src.indexOf("AddEventSink<RE::MenuOpenCloseEvent>(ChestWatcher");
  need(reg >= 0, "ChestWatcher registration not found");
  need(/IsVR\(\)/.test(src.slice(Math.max(0, reg - 400), reg)),
       "ChestWatcher must be registered only when !REL::Module::IsVR()");
});

check("readiness: every blocker has a name, and System.cpp no longer owns the check", () => {
  // #36: the log only said "not ready yet". The check lives in Readiness now and names
  // the condition that failed; System.cpp must not keep a second copy that stays silent.
  // The enum and Name() live in the engine-free ReadinessLogic.h (unit-tested there);
  // Readiness.h includes it.
  const h = read("src/ReadinessLogic.h");
  need(read("src/Readiness.h").includes('#include "ReadinessLogic.h"'),
       "Readiness.h must include ReadinessLogic.h");
  const en = h.match(/enum class Blocker[^{]*\{([^}]*)\}/);
  need(en, "Blocker enum not found in ReadinessLogic.h");
  const names = [...en[1].matchAll(/\bk\w+/g)].map((m) => m[0]);
  need(names.length >= 2, "Blocker enum parsed to nothing");
  const nameFn = h.slice(h.indexOf("Name(Blocker"));
  for (const n of names) {
    need(new RegExp(`case\\s+Blocker::${n}\\s*:`).test(nameFn), `Name() has no case for Blocker::${n}`);
  }
  const sys = read("src/System.cpp");
  need(!/bool\s+IsPlayerReady\s*\(/.test(sys), "System.cpp still defines IsPlayerReady");
  need(!sys.includes("g_firstReadyCell"), "System.cpp still owns g_firstReadyCell");
  const hot = sys.indexOf("requested by hotkey, but the player is not ready yet");
  need(hot >= 0 && sys.slice(hot, hot + 300).includes("Readiness::Name("),
       "the not-ready log line must format Readiness::Name(");
  return `${names.length} blockers named`;
});

check("magelight: Install logs before RequestApi4 and each CreateViewEx, and page paths are under Data\\Magelight\\IsekaiHero", () => {
  // Same lesson as #35: a crash inside a host call must leave the line naming that call.
  // Install creates its views through one helper, Create(), so every Create(api, ...) in
  // Install needs its own "creating view" line after the previous one. The page paths are
  // pinned too: the packaged pages live in one folder, and logs name only that relative
  // folder (the absolute one can sit under the user's folder).
  const strip = (s) => s.replace(/\/\*[\s\S]*?\*\//g, "").replace(/\/\/[^\n]*/g, "");
  const src = strip(read("src/UI/Magelight.cpp"));
  const at = src.indexOf("bool Install()");
  need(at >= 0, "Install() not found in Magelight.cpp");
  const inst = src.slice(at);
  const req = inst.indexOf("requesting the API");
  const api = inst.indexOf("RequestApi4(");
  need(api >= 0, "Install no longer calls RequestApi4 — parser stale?");
  need(req >= 0 && req < api, "Install must log before RequestApi4");
  need([...src.matchAll(/CreateViewEx\(/g)].length === 1,
       "CreateViewEx must be called in exactly one place, the Create() helper");
  const helper = src.slice(src.indexOf("bool Create("), src.indexOf("bool Listen("));
  need(helper.includes("CreateViewEx("), "the Create() helper no longer calls CreateViewEx");
  const calls = [...inst.matchAll(/\bCreate\(api,/g)].map((m) => m.index);
  need(calls.length >= 2, `${calls.length} view creations parsed in Install, expected system and hud`);
  calls.forEach((pos, i) => {
    const from = i === 0 ? 0 : calls[i - 1];
    need(inst.slice(from, pos).includes("Magelight: creating view "),
         `view creation ${i + 1} in Install is not logged before it`);
  });
  need(src.includes('kPageDir = "Data\\\\Magelight\\\\IsekaiHero\\\\"'),
       "kPageDir must be Data\\Magelight\\IsekaiHero\\");
  need(src.includes('L"Data" / L"Magelight" / L"IsekaiHero" / a_file'),
       "PagePath must build <exe dir>\\Data\\Magelight\\IsekaiHero\\<file>");
  for (const page of ["system.html", "hud.html"]) {
    need(src.includes(`"${page}"`), `the ${page} view is gone from Magelight.cpp`);
  }
  need(!/logger::\w+\([^;]*(\bpaths?\[|Utf8\()/.test(src), "a log line formats an absolute page path");
  return `RequestApi4 and ${calls.length} view creations logged first, pages under Data\\Magelight\\IsekaiHero`;
});

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
  // Comments are stripped first: a comment can explain a path helper with a
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

check("ui: the web view's script parses", () => {
  // Nothing else here would notice. The page is not compiled, and a syntax error in it
  // does not fail the build or any other check — it shows up in the game as a panel that
  // never appears, which is an expensive way to find a typo. The shared common.js is
  // loaded by every page, so a typo there blanks all of them; a typo in a screen script
  // takes that screen away.
  const html = read(SYSTEM_PAGE);
  const blocks = [...html.matchAll(/<script\b[^>]*>([\s\S]*?)<\/script>/g)]
    .map((m, i) => [`<script> block ${i + 1}`, m[1]]).filter((b) => b[1].trim());
  need(blocks.length > 0, "no inline <script> blocks found — parser stale?");
  const inline = blocks.length;
  for (const f of [PAGE_COMMON, ...screenScripts()]) blocks.push([f.replace(/^.*IsekaiHero\//, ""), read(f)]);
  for (const [name, js] of blocks) {
    try {
      new Function(js);  // parses only; nothing runs
    } catch (e) {
      need(false, `${name} does not parse: ${e.message}`);
    }
  }
  return `${inline} block(s) + ${blocks.length - inline} script(s), ` +
         `${blocks.reduce((n, b) => n + b[1].split("\n").length, 0)} lines`;
});

check("ui: the web view actually translates itself", () => {
  // Everything else about the translation is checked by reading the source. This RUNS it:
  // the page's own t/tf/setLoc from common.js, against a table shaped like Loc::Json's
  // output and deliberately partial, which is what every real translation is.
  const els = [
    { dataset: { loc: "view.close" }, textContent: "Close" },
    { dataset: { loc: "view.milestones" }, textContent: "Milestones" },
  ];
  const doc = {
    querySelectorAll: () => els,
    addEventListener: () => {},
    documentElement: { style: { setProperty: () => {} } },
    readyState: "loading",
  };
  const win = {};
  new Function("window", "document", read(PAGE_COMMON))(win, doc);
  const api = win.isekaiUi;
  need(api && typeof api.t === "function" && typeof api.tf === "function" &&
       typeof api.setLoc === "function",
       "common.js no longer publishes isekaiUi.t / tf / setLoc — check stale?");

  api.setLoc({ "view.close": "Schließen", "view.tree.cost": "{} Systempunkt(e)" });
  need(els[0].textContent === "Schließen", "a covered label was not translated");
  need(els[1].textContent === "Milestones", "an uncovered label must stay English");
  need(api.tf("view.tree.cost", "{} system point(s)", 7) === "7 Systempunkt(e)",
       "tf did not fill {} from the translated pattern");
  need(api.tf("view.tree.rank", " · Rank {}", 3) === " · Rank 3",
       "tf did not fill {} in the English fallback");
  need(api.t("view.buy", "Buy") === "Buy", "a missing key did not fall back to English");

  // A second push must translate from the stashed English, not from the first result.
  api.setLoc({ "view.close": "Fermer" });
  need(els[0].textContent === "Fermer", "a re-push translated an already-translated label");
  need(els[1].textContent === "Milestones", "a re-push corrupted an uncovered label");
  // Anything but an object (a failed parse on the C++ side) must reset to English, not throw.
  api.setLoc(null);
  need(els[0].textContent === "Close", "an empty push did not fall back to English");
  return "partial table, re-push, and both fallbacks";
});

check("ui: every translated label in the web view carries its English", () => {
  // A data-loc element's own text IS the English fallback. Empty it, and that label
  // disappears entirely for anyone whose translation happens not to cover that key —
  // which is the exact case the fallback exists for.
  const html = read(SYSTEM_PAGE);
  const labels = [...html.matchAll(/data-loc="([\w.]+)"[^>]*>([^<]*)</g)];
  need(labels.length > 0, "no data-loc labels parsed — parser stale?");
  for (const [, key, text] of labels) {
    need(text.trim().length > 0, `data-loc="${key}" has no English text to fall back to`);
  }
  return `${labels.length} labels`;
});

check("ui: the system page speaks the plugin's message contract", () => {
  // src/UI/Magelight.cpp listens for exactly two names on the system view and parses
  // the action text with UiLogic::ParseAction. A page that sends "choose 2" or
  // "isekai:action" is silently ignored — the player clicks and nothing happens, and in
  // the blessing choice that means a character who can never finish being reborn.
  // RUN the page's helpers against a fake host and read what they sent.
  const sent = [];
  const win = { magelight: { send: (name, arg) => sent.push([name, arg]) } };
  new Function("window", "document", read(PAGE_COMMON))(win, {
    querySelectorAll: () => [], addEventListener: () => {}, readyState: "loading",
    documentElement: { style: { setProperty: () => {} } },
  });
  const ui = win.isekaiUi;
  ui.send("choose", 2);
  ui.send("close");
  ui.send("status", "tree");
  ui.ready();
  win.ISEKAI_UI_BUILD = "abc1234";
  ui.ready();
  ui.escapeCapture(true);
  ui.escapeCapture(false);
  const want = [
    ["isekai.action", "choose:2"], ["isekai.action", "close"], ["isekai.action", "status:tree"],
    ["isekai.ready", "dev"], ["isekai.ready", "abc1234"],
    ["__escapecapture", "1"], ["__escapecapture", "0"],
  ];
  need(JSON.stringify(sent) === JSON.stringify(want),
       `the page sent ${JSON.stringify(sent)}, the plugin expects ${JSON.stringify(want)}`);

  const cpp = read("src/UI/Magelight.cpp");
  for (const name of ["isekai.action", "isekai.ready"]) {
    need(cpp.includes(`"${name}"`), `Magelight.cpp no longer listens for ${name}`);
  }
  need(/isekai\.hello\(\)/.test(cpp), "Magelight.cpp no longer re-asks a reloaded page for ready");
  // The first-ready race: a page that reported ready before its listener existed is asked
  // again, once per view, right after the listeners are registered.
  const install = cpp.slice(cpp.indexOf("bool Install()"));
  need((install.match(/InvokeJS\(\w+, kHello\)/g) || []).length >= 2,
       "Install no longer asks both pages for ready after registering their listeners");
  // A UI-mode exit the page did not ask for (Escape or pad B on a data screen) is told to
  // the page, so the next open starts fresh.
  need(/"isekai\.closed\(\)"/.test(read("src/UI/Screens.cpp")), "Screens.cpp no longer tells the page its view closed");

  // The page side: hello() re-sends ready, and the build id is loaded from version.js
  // without the page depending on it (a missing file must not stop the page).
  const html = read(SYSTEM_PAGE);
  need(/<script\s+src="version\.js"[^>]*onerror=/.test(html),
       "system.html does not load version.js with a failure-tolerant <script>");
  for (const fn of ["loc", "scale", "show", "hello", "closed"]) {
    need(new RegExp(`\\b${fn}\\s*[:=]\\s*(function\\b|\\([^)]*\\)\\s*=>)`).test(html),
         `system.html no longer defines isekai.${fn}`);
  }
  need(/\bhello\s*[:=][^\n]*\bready\(\)/.test(html), "isekai.hello() no longer re-sends isekai.ready");
  return `${want.length} messages, hello/loc/scale/show/closed defined, version.js optional`;
});

check("ui: system.html loads every screen script", () => {
  // A screen script that is not loaded is a screen that is not there: the page then
  // simply does not offer it, and the plugin's show() of it logs a warning nobody reads.
  // Each one is loaded by a plain <script src> before the page's own script runs them,
  // and each registers the factory its file name promises.
  const html = read(SYSTEM_PAGE);
  const files = screenScripts();
  need(files.length >= 3, `${files.length} screen scripts in ${PAGE_SCREENS} — status, tree and shop expected`);
  const main = html.search(/<script>\s*"use strict";/);
  need(main >= 0, "the page's own inline script not found — parser stale?");
  for (const f of files) {
    const name = f.slice(PAGE_SCREENS.length + 1, -3);
    const tag = html.indexOf(`<script src="screens/${name}.js"></script>`);
    need(tag >= 0, `system.html never loads screens/${name}.js`);
    need(tag < main, `screens/${name}.js loads after the script that runs it`);
    need(new RegExp(`defs\\.${name}\\s*=\\s*function`).test(read(f)),
         `screens/${name}.js does not register defs.${name}`);
    need(html.includes(`"${name}"`), `system.html never asks for the ${name} screen`);
  }
  return files.map((f) => f.slice(PAGE_SCREENS.length + 1)).join(", ");
});

check("ui: every action the page sends is one the plugin parses", () => {
  // UiLogic::ParseAction rejects an unknown type or word outright, so a typo in a button
  // handler is a button that does nothing, in game only. Read every ui.send() and every
  // statusAction() the page makes and hold it to the parser's own word lists.
  const logic = read("src/UI/UiLogic.h");
  const parse = logic.slice(logic.indexOf("inline std::optional<Action> ParseAction"));
  need(parse.length > 100, "ParseAction not found in UiLogic.h — parser stale?");
  const words = (type) => {
    const m = parse.match(new RegExp(`type == "${type}"\\)\\s*\\{\\s*static constexpr std::string_view kWords\\[\\] = \\{([^}]*)\\}`));
    need(m, `no word list for "${type}" in ParseAction — parser stale?`);
    return [...m[1].matchAll(/"(\w+)"/g)].map((x) => x[1]);
  };
  const bare = ["close", "respec"];                // no argument
  const numbered = ["choose", "shopBuy", "buy"];  // a number
  for (const w of [...bare, ...numbered]) {
    need(parse.includes(`"${w}"`), `ParseAction no longer knows "${w}" — update this check`);
  }
  const allowed = { status: words("status"), nav: words("nav") };

  const page = [SYSTEM_PAGE, PAGE_COMMON, ...screenScripts()].map(read).join("\n")
    .replace(/\/\*[\s\S]*?\*\//g, "").replace(/^[ \t]*\/\/.*$/gm, "");
  const sends = [...page.matchAll(/\bui\.send\(\s*"(\w+)"\s*(?:,\s*([^)]*))?\)/g)];
  need(sends.length >= 8, `${sends.length} ui.send calls parsed — parser stale?`);
  for (const [, type, arg] of sends) {
    if (bare.includes(type)) { need(arg === undefined, `ui.send("${type}") takes no argument`); continue; }
    if (numbered.includes(type)) { need(arg !== undefined && !/^"/.test(arg.trim()), `ui.send("${type}") needs a number`); continue; }
    need(type in allowed, `ui.send("${type}", …): ParseAction knows no type "${type}"`);
    if (type === "status" && arg.trim() === "a") continue;   // statusAction's own pass-through, checked below
    const word = arg.trim().match(/^"(\w+)"$/);
    need(word, `ui.send("${type}", ${arg}) does not send a literal word`);
    need(allowed[type].includes(word[1]), `ui.send("${type}", "${word[1]}"): not one of ${allowed[type].join("/")}`);
  }
  const status = [...page.matchAll(/\bstatusAction\(\s*"(\w+)"\s*\)/g), ...page.matchAll(/\baddIcon\([^;]*?,\s*"[\w.]+\.png",\s*"(\w+)"\)/g),
                  ...page.matchAll(/\bstatusButton\(\s*\w+,\s*"(\w+)"/g)].map((m) => m[1]);
  need(status.length >= 6, `${status.length} status actions parsed — parser stale?`);
  for (const w of status) {
    need(allowed.status.includes(w), `status action "${w}" is not one of ${allowed.status.join("/")}`);
  }
  return `${sends.length} sends, ${new Set(status).size} status words, all parseable`;
});

check("ui: the web view's status buttons name real icons", () => {
  const html = systemPage();
  // The label argument goes through t("key", "English"), so it holds commas of its own —
  // anything up to the PNG rather than "one comma-free argument". Calls only: not the
  // definition, and never across a line, or the definition's match would swallow the
  // first call's icon and that call would go unchecked.
  const icons = [...html.matchAll(/(?<!function\s+)\baddIcon\(([^;\n]*?),\s*"([\w.]+\.png)"/g)].map((m) => m[2]);
  need(icons.length >= 3, `${icons.length} addIcon calls parsed (tree, storage, shop expected) — parser stale?`);
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

  // The scenarios live in mock.js (the screen picker), the harness in index.html.
  const pg = read("playground/index.html") + read("playground/mock.js");
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
  // again, which is exactly the bug it was written for. Cards (two to four choices) carry
  // the same .lbl span; the stacked row is what one, or five and more, choices use.
  const view = systemPage();
  need(/#pButtons \.btn \.lbl\s*\{/.test(view),
       "no #pButtons .btn .lbl rule — the label clipping is gone");
  need(/#pButtons \.btn\s*\{[^}]*min-width:\s*0/.test(view),
       "#pButtons .btn no longer sets min-width: 0, so a flex button cannot shrink " +
       "below its content and the row overflows the panel instead");
  need(/#pButtons \.btn\s*\{[^}]*flex-direction:\s*column/.test(view),
       "#pButtons .btn is no longer stacked; icon beside label does not fit five choices");
  // The page's functions sit indented inside an IIFE, so the next function at any
  // indent ends buildButtons — not the next one at column 0, which does not exist.
  const from = view.indexOf("function buildButtons()");
  need(from >= 0, "buildButtons not found in the view — parser stale?");
  const next = view.slice(from + 1).search(/\n[ \t]*function \w/);
  need(next >= 0, "nothing follows buildButtons — the delimiter is stale");
  const build = view.slice(from, from + 1 + next);
  // Each builder on its own: from its className assignment to the next one (or to the
  // end of the loop body). Dropping the span from either must fail this rule.
  const branch = (cls) => {
    const at = build.indexOf(`el.className = "${cls}`);
    need(at >= 0, `buildButtons no longer builds .${cls} elements — parser stale?`);
    const rest = build.slice(at + 1);
    const end = Math.min(...[rest.indexOf("el.className ="), rest.indexOf("el.querySelectorAll(")]
      .filter((i) => i >= 0));
    return build.slice(at, at + 1 + (Number.isFinite(end) ? end : rest.length));
  };
  const lbl = /class="lbl">'\s*\+\s*esc\(b\.label\)/;
  need(lbl.test(branch("card")), "the card builder emits a label that is not inside a .lbl span");
  need(lbl.test(branch("btn")), "the row builder emits a label that is not inside a .lbl span");
  return "stacked, clipped, and both builders wrap the label";
});

check("loc: the translation template matches the strings in the source", () => {
  // The template is what a translator works from, and it is generated. Ship a stale one
  // and they translate keys that no longer exist while the new ones stay English - and
  // nothing anywhere says so, because a missing key is DESIGNED to fall back silently.
  // That safety is exactly why this needs checking from outside.
  const { found, dupes } = collectStrings();
  need(dupes.length === 0,
       `these keys are used with different English text in different places: ` +
       `${dupes.join(", ")} — one key must mean one string`);
  for (const [key] of found) {
    need(/^[a-z][a-z0-9]*(\.[a-z0-9]+)+$/i.test(key),
         `translation key "${key}" is not in dotted lowercase form (area.thing)`);
  }
  need(read("lang/template.txt") === stringTemplate(found),
       "lang/template.txt is out of date — run node tools/extract-strings.mjs and commit it");
  return `${found.size} strings`;
});

check("fomod: the installer offers files that exist, and presets that are current", () => {
  // The installer can only choose files, so every preset is a whole copy of the ini.
  // Two ways that rots: a source path in ModuleConfig that no longer exists (the manager
  // aborts the install with an XML error, which is the worst possible first impression),
  // and a preset generated before the ini last changed (the player picks "Modlist
  // testing" and silently gets last version's settings). Both are checked by rebuilding
  // the presets and comparing.
  const xml = read("fomod/ModuleConfig.xml");

  // The installer asks two questions; every ANSWER has to be offered, and every
  // COMBINATION of answers has to resolve to an ini. A missing pattern is the worst kind
  // of miss: that path through the installer silently ships no settings file at all.
  for (const opt of [...STARTS, ...HUDS]) {
    need(xml.includes(`name="${opt.name}"`),
         `installer option "${opt.name}" is in make-presets.mjs but not in ` +
         "ModuleConfig.xml — regenerate with node tools/make-fomod.mjs");
  }
  for (const p of PRESETS) {
    need(xml.includes(`flag="start" value="${p.start}"`) &&
         xml.includes(`flag="hud" value="${p.hud || "default"}"`),
         `no conditionalFileInstalls pattern for "${p.name}"`);
  }
  need((xml.match(/<pattern>/g) || []).length === PRESETS.length,
       `${(xml.match(/<pattern>/g) || []).length} install patterns for ${PRESETS.length} ` +
       "preset combinations — every combination needs exactly one");

  // Every source path must resolve to something. The archive is staged by package.ps1,
  // so the repo path differs — this maps the few that are staged from elsewhere and
  // insists the rest exist as written.
  const staged = {
    "IsekaiHero.esp": "plugin/IsekaiHero.esp",
    "SKSE\\Plugins\\IsekaiHeroSKSE.dll": null,   // build output
    "Magelight\\IsekaiHero": "magelight/IsekaiHero",   // pages; icons and version.js are staged into it
    "SKSE\\Plugins\\IsekaiHero.ini": "IsekaiHero.ini",
    "SKSE\\Plugins\\IsekaiHero\\lang\\template.txt": "lang/template.txt",
    "LICENSE": "LICENSE",
    "Sound": "sounds",
    "MCM\\Config\\IsekaiHero": "mcm-patch/MCM/Config/IsekaiHero",
    "Scripts\\IsekaiHeroMCM.pex": "mcm-patch/Scripts/IsekaiHeroMCM.pex",
    "Scripts\\IsekaiHeroCraft.pex": "papyrus-api/Scripts/IsekaiHeroCraft.pex",
    "Source\\Scripts\\IsekaiHeroCraft.psc": "papyrus-api/Source/Scripts/IsekaiHeroCraft.psc",
  };
  const sources = [...new Set([...xml.matchAll(/source="([^"]+)"/g)].map((m) => m[1]))];
  need(sources.length >= 5, "ModuleConfig lists almost nothing — generator stale?");
  for (const src of sources) {
    const mapped = src in staged ? staged[src] : src.replace(/\\/g, "/");
    if (mapped === null) continue;
    need(existsSync(join(ROOT, mapped)),
         `ModuleConfig installs "${src}", which resolves to nothing in the repo ` +
         `(looked for ${mapped})`);
  }

  // Screenshots, same rule as the files: a path that resolves to nothing gives the player
  // an empty pane and no clue why. The generator omits the attribute when the picture is
  // absent, so anything that IS written here has to be on disk.
  for (const img of [...new Set([...xml.matchAll(/(?:image|moduleImage)[^>]*path="([^"]+)"/g)]
                                  .map((m) => m[1]))]) {
    need(existsSync(join(ROOT, img.replace(/\\/g, "/"))),
         `ModuleConfig shows the picture "${img}", which is not in the repo`);
  }

  // The pages load their siblings by relative path, so the archive has to carry the whole
  // set at the same relative layout. Every <script src>/<link href> of both pages must be
  // a file in the repo (version.js is generated by the build) AND be staged by
  // package.ps1, by name or by the screens\*.js and icons\*.png wildcards.
  const pkg = read("package.ps1");
  need(!/prisma/i.test(pkg), "package.ps1 still stages PrismaUI files");
  for (const page of [SYSTEM_PAGE, HUD_PAGE]) {
    const html = read(page);
    for (const [, ref] of html.matchAll(/<(?:script|link)\b[^>]*\b(?:src|href)="([^"]+)"/g)) {
      if (/^(?:[a-z]+:|\/\/|#)/i.test(ref)) continue;
      const generated = ref === "version.js";
      need(generated || existsSync(join(ROOT, "magelight/IsekaiHero", ref)),
           `${page} loads "${ref}", which is not in magelight/IsekaiHero`);
      const staged = ref.startsWith("screens/") ? pkg.includes("screens\\*.js")
                   : pkg.includes(ref);
      need(staged, `${page} loads "${ref}", which package.ps1 does not stage`);
    }
  }
  need(pkg.includes("icons\\*.png"), "package.ps1 no longer stages icons\\*.png");

  // Regenerating must be a no-op: if it is not, the shipped presets are stale.
  const before = PRESETS.map((p) => read(`fomod-presets/${p.dir}/IsekaiHero.ini`));
  buildPresets();
  const after = PRESETS.map((p) => read(`fomod-presets/${p.dir}/IsekaiHero.ini`));
  for (let i = 0; i < PRESETS.length; i++) {
    need(before[i] === after[i],
         `the "${PRESETS[i].name}" preset is out of date with IsekaiHero.ini — ` +
         "run node tools/make-fomod.mjs and commit the result");
  }
  return `${PRESETS.length} presets, ${sources.length} sources`;
});

check("ui: UiScale reaches both pages", () => {
  // The setting is one number that has to arrive in two pages: the system page (every
  // screen) and the hud (toasts, flourish, threat labels). Either half can rot on its own
  // without anything failing - half the interface simply stops resizing. This is an
  // accessibility setting, so silent half-coverage is the wrong failure.
  const view = systemPage();
  const hud = read(HUD_PAGE);
  for (const [name, html] of [[SYSTEM_PAGE, read(SYSTEM_PAGE)], [HUD_PAGE, hud]]) {
    need(/\bscale\s*[:=]\s*(function\b|\([^)]*\)\s*=>)/.test(html), `${name} no longer defines isekai.scale`);
  }
  need(/--ui-scale/.test(read(PAGE_COMMON)), "setScale no longer writes the --ui-scale variable");
  need(/transform:\s*scale\(var\(--ui-scale/.test(view),
       "nothing in the system page applies --ui-scale, so the plugin would push a number nowhere");
  need(/transform:\s*scale\(var\(--ui-scale/.test(hud),
       "nothing in the hud applies --ui-scale, so the plugin would push a number nowhere");
  // A value arriving from outside the page: a 0 would make the whole interface vanish
  // with no way left to reach the setting that did it. Run the clamp, don't just read it.
  let pushed = null;
  const win = {};
  new Function("window", "document", read(PAGE_COMMON))(win, {
    querySelectorAll: () => [], addEventListener: () => {}, readyState: "loading",
    documentElement: { style: { setProperty: (k, v) => { if (k === "--ui-scale") pushed = +v; } } },
  });
  win.isekaiUi.setScale(0);
  need(pushed === 0.5, `setScale(0) wrote ${pushed}, not the 0.5 floor`);
  win.isekaiUi.setScale("garbage");
  need(pushed === 1, `setScale("garbage") wrote ${pushed}, not 1`);
  // The plugin side: the scale goes into the prelude every fresh page gets first, for
  // both views, and again after a settings reload.
  const screens = read("src/UI/Screens.cpp");
  need(/"isekai\.scale\("\s*\+\s*nlohmann::json\(scale\)\.dump\(\)/.test(screens) &&
       /const float scale = Config::UiScale\(\)/.test(screens),
       "Screens.cpp no longer builds isekai.scale from Config::UiScale()");
  need(/SetPrelude\(Magelight::View::System,\s*&SettingsCalls\)/.test(screens),
       "the system view's prelude no longer carries the settings calls");
  // The hud's prelude belongs to Hud.cpp (SetPrelude replaces, one owner per view) and is
  // built on the same calls, plus the VR switch.
  const hudCpp = read("src/UI/Hud.cpp");
  const hudPrelude = hudCpp.match(/std::vector<std::string> HudPrelude\(\) \{([\s\S]*?)\n        \}/);
  need(/SetPrelude\(Magelight::View::Hud,\s*&HudPrelude\)/.test(hudCpp) && hudPrelude,
       "Hud.cpp no longer sets the hud view's prelude (HudPrelude) — parser stale?");
  need(/Screens::SettingsCalls\(\)/.test(hudPrelude[1]),
       "the hud view's prelude no longer carries the settings calls");
  need(/IsVR\(\)/.test(hudCpp) && /isekai\.vr\(/.test(hudPrelude[1]),
       "the hud view's prelude no longer switches the page to the VR scan list");
  need(!/SetPrelude\(Magelight::View::Hud/.test(screens),
       "Screens.cpp sets the hud prelude too — SetPrelude replaces, whichever runs last wins");
  need(/UI::Screens::OnSettingsReloaded\(\)/.test(read("src/System.cpp")),
       "a settings reload no longer re-sends UiScale to the pages");
  return "system and hud pages, prelude and reload";
});

check("ui: every skill-tree link in the web view is visibly drawn", () => {
  // The web view's first net routed every link straight from its parent's name to the
  // child's tile; between a branch's two rows that left 0-18 px of a faint dashed line,
  // so 9 of 14 links could not be seen. This RUNS the page's own routing over the real
  // node table and measures, per link, how much of it lies outside every tile and name.
  const MIN = 12;
  const win = {};
  new Function("window", read(`${PAGE_SCREENS}/tree.js`))(win);
  const geo = win.isekaiScreens && win.isekaiScreens.treeGeometry;
  need(geo && typeof geo.layout === "function" && typeof geo.links === "function" &&
       typeof geo.sample === "function" && typeof geo.footprints === "function",
       "tree.js no longer publishes isekaiScreens.treeGeometry { layout, links, sample, footprints }");
  const graph = parseSkillTreeNodes().filter((n) => n.zone !== "MASTERY");
  const L = geo.layout(graph);
  need(L, "the page laid out no net");
  const links = geo.links(L, graph);
  const expected = graph.reduce((a, n) => a + n.prereq.length, 0);
  need(links.length === expected, `${links.length} links routed, ${expected} prerequisites in SkillTree.cpp`);
  const boxes = graph.flatMap((n) => { const f = geo.footprints(L, n); return [f.tile, f.label]; });
  const inside = (q) => boxes.some((b) => q.x >= b[0] && q.x <= b[2] && q.y >= b[1] && q.y <= b[3]);
  let worst = Infinity, who = "";
  for (const lk of links) {
    const pts = geo.sample(lk.route);
    let vis = 0;
    for (let i = 1; i < pts.length; i++) {
      if (!inside(pts[i - 1]) && !inside(pts[i])) vis += Math.hypot(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y);
    }
    const name = `${lk.from.name} -> ${lk.to.name}`;
    need(vis >= MIN, `"${name}" shows only ${vis.toFixed(1)} px outside tiles and names (at least ${MIN})`);
    if (vis < worst) { worst = vis; who = name; }
  }
  return `${links.length} links, shortest ${who} at ${worst.toFixed(0)} px`;
});

/* The page's own net geometry over the real node table, in canvas px. */
function webNet() {
  const win = {};
  new Function("window", read(`${PAGE_SCREENS}/tree.js`))(win);
  const geo = win.isekaiScreens.treeGeometry;
  need(typeof geo.zones === "function", "tree.js no longer publishes treeGeometry.zones");
  const graph = parseSkillTreeNodes().filter((n) => n.zone !== "MASTERY");
  const L = geo.layout(graph);
  return { geo, graph, L, fp: graph.map((n) => ({ n, f: geo.footprints(L, n) })) };
}

check("ui: no skill-tree link runs across a node's name", () => {
  // A dashed line through a name reads as strike-through. The first Enchant band fanned
  // eight straight links from one node across its inner column's names.
  const { geo, graph, L, fp } = webNet();
  const crossed = [];
  for (const lk of geo.links(L, graph)) {
    for (const { n, f } of fp) {
      const b = f.label;
      if (geo.sample(lk.route).some((q) => q.x > b[0] && q.x < b[2] && q.y > b[1] && q.y < b[3])) {
        crossed.push(`${lk.from.name} -> ${lk.to.name} over "${n.name}"`);
      }
    }
  }
  need(crossed.length === 0, crossed.join("; "));
  return `scale ${L.k.toFixed(3)}, no name crossed`;
});

check("ui: no skill-tree tile covers a zone header", () => {
  // The header is 10px mono text with 3px letter-spacing at the frame's top-left corner
  // (drawNet): about 9px a letter. Divine Judgment once sat on "ENCHANT".
  const { geo, graph, L, fp } = webNet();
  for (const z of geo.zones(L, graph)) {
    const h = [z.x + 8, z.y + 3, z.x + 8 + z.zone.length * 9, z.y + 15];
    for (const { n, f } of fp) {
      const t = f.tile;
      need(!(t[0] < h[2] && t[2] > h[0] && t[1] < h[3] && t[3] > h[1]),
           `"${n.name}" covers the ${z.zone} header`);
    }
  }
  return "all headers clear";
});

check("ui: the web view's zone frames keep apart", () => {
  // Lauf 41: neighbouring branch frames shared or crossed their edges ("the frames of the
  // trees still overlap"). The design-unit check above holds; the page pads every frame
  // by its own nodes' names, so it is measured here in canvas px, as drawn.
  const GAP = 12;
  const { geo, graph, L } = webNet();
  const zs = geo.zones(L, graph);
  let tightest = Infinity;
  for (let i = 0; i < zs.length; i++) {
    for (let j = i + 1; j < zs.length; j++) {
      const a = zs[i], b = zs[j];
      const gap = Math.max(b.x - (a.x + a.w), a.x - (b.x + b.w), b.y - (a.y + a.h), a.y - (b.y + b.h));
      need(gap >= GAP, `${a.zone} and ${b.zone} frames are ${gap.toFixed(1)} px apart (at least ${GAP})`);
      tightest = Math.min(tightest, gap);
    }
  }
  return `${zs.length} frames, tightest gap ${tightest.toFixed(0)} px, scale ${L.k.toFixed(3)}`;
});

check("ui: the controller's focus ring can reach every kind of target", () => {
  // A pad has no cursor, so in the web view it navigates by moving focus between the
  // elements named in a selector list. Those are class selectors written by hand against
  // markup built somewhere else in the same file — rename `.card` in renderShop and the
  // shop silently stops being reachable by controller, with nothing failing anywhere.
  // Nobody here plays on a pad, so nobody would notice.
  //
  // The Magelight page: the pad arrives as keys (Magelight's key style), and the page's
  // own arrow keys walk the FOCUSABLE list. Every kind of
  // target is a <button> (focusable without a tabindex), every class in the list must
  // exist, and theme.css must draw a ring on :focus.
  const page = systemPage();
  const list = read(SYSTEM_PAGE).match(/const FOCUSABLE\s*=\s*"([^"]+)"/);
  need(list, "system.html has no FOCUSABLE selector list — controller navigation gone?");
  const kinds = list[1].split(",").map((x) => x.trim().replace(/^\./, ""));
  need(kinds.length >= 5,
       `FOCUSABLE is down to ${kinds.length} selectors; it has to cover buttons, cards ` +
       "(choices and shop goods), icon actions, tree nodes, mastery rows and shelves");
  for (const c of kinds) {
    need(new RegExp(`class(Name)?\\s*=?\\s*"${c}[ "]`).test(page),
         `FOCUSABLE names .${c}, but nothing in the page ever carries that class`);
  }
  const alt = kinds.join("|");
  need(!new RegExp(`createElement\\("(?!button")\\w+"\\)[^;]*;\\s*\\w+\\.className\\s*=\\s*"(${alt})\\b`).test(page),
       "a pad target is built as a non-button element, which focus cannot reach");
  need(/:focus\b[^{]*\{[^}]*outline/.test(read(PAGE_THEME)),
       "theme.css draws no outline on :focus — the controller would move an invisible focus");
  return `${kinds.length} kinds, all present, all buttons`;
});

check("ui: the status payload's rank tone comes from ToneForRank", () => {
  // The rank's colour is the part you are meant to trust without reading the letter, so
  // the page and the plugin must never disagree about it. The plugin decides the tone
  // (Logic::ToneForRank, sent as rankTone by BuildStatusJson); the page falls back to the
  // same rule for a plugin that does not send it. RUN the page's rankTone() against every
  // rank the plugin can return, and against the field.
  //
  // It also has to reach the screen. The old view shipped six correct colours that all
  // rendered white because an id rule outranked them — so the letter must take its
  // colour from the tone, and the rank block must be given the rank's tone.
  const logic = read("src/UI/UiLogic.h");
  const fn = logic.match(/constexpr Tone ToneForRank\(char a_rank\)\s*\{([\s\S]*?)\n    \}/);
  need(fn, "ToneForRank not found in UiLogic.h — parser stale?");
  need(/\?\s*Tone::Gold\s*:\s*Tone::Cyan/.test(fn[1]),
       "ToneForRank is no longer `letter ? Gold : Cyan` — update this check with it");
  const gold = [...fn[1].matchAll(/a_rank == '([A-Z])'/g)].map((m) => m[1]);
  need(gold.length > 0, "no gold rank parsed from ToneForRank — parser stale?");

  const cpp = read("src/Progression.cpp");
  const from = cpp.indexOf("std::string SystemRank()");
  need(from >= 0, "SystemRank not found in Progression.cpp");
  const letters = [...new Set([...cpp.slice(from, cpp.indexOf("\n    }", from))
    .matchAll(/return "(\w)";/g)].map((m) => m[1]))];
  need(letters.length > 0, "no rank letters parsed — parser stale?");

  // The payload: rankTone from ToneForRank on the rank it sends, blessingTone from
  // ToneFor(Blessing), and the level the page shows beside them.
  const at = cpp.indexOf("std::string BuildStatusJson()");
  need(at >= 0, "BuildStatusJson not found in Progression.cpp");
  const body = cpp.slice(at, cpp.indexOf("\n        }\n", at));
  need(/\\"rankTone\\"[\s\S]{0,160}ToneName\(UI::Logic::ToneForRank\(rank/.test(body),
       "BuildStatusJson no longer sends rankTone from Logic::ToneForRank");
  need(/\\"blessingTone\\"[\s\S]{0,160}ToneName\(UI::Logic::ToneFor\(BlessingOf/.test(body),
       "BuildStatusJson no longer sends blessingTone from Logic::ToneFor(Blessing)");
  need(/\\"level\\"/.test(body), "BuildStatusJson no longer sends the character level");

  const html = systemPage();
  // One tone table for the page and every screen: common.js's, which has no prototype
  // (a payload tone of "constructor" must not pass as a tone).
  need(/\bconst TONES = ui\.TONES;/.test(read(SYSTEM_PAGE)),
       "system.html no longer takes its TONES from common.js (ui.TONES) — update this check");
  need(!/\bTONES\s*=\s*\{/.test(html), "a page script defines its own plain TONES object");
  const src = html.match(/function rankTone\(s\) \{[\s\S]*?\n\s*\}/);
  need(src, "rankTone(s) not found in system.html — parser stale?");
  const tones = Object.assign(Object.create(null), { cyan: 1, gold: 1, red: 1, violet: 1 });
  const rankTone = new Function("TONES", `${src[0]}\nreturn rankTone;`)(tones);
  need(rankTone({ rank: "C", rankTone: "constructor" }) === "cyan", "rankTone takes \"constructor\" as a tone");
  for (const l of letters) {
    const want = gold.includes(l) ? "gold" : "cyan";
    need(rankTone({ rank: l }) === want, `rank ${l}: the plugin says ${want}, the page ${rankTone({ rank: l })}`);
  }
  need(rankTone({ rank: "C", rankTone: "gold" }) === "gold", "the page ignores the plugin's rankTone");
  need(rankTone({ rank: gold[0], rankTone: "bogus" }) === "gold",
       "an unknown rankTone must fall back to the rank's own rule");

  const idRule = html.match(/#sRankLetter\s*\{([^}]*)\}/);
  need(idRule, "#sRankLetter rule not found — parser stale?");
  need(/(^|[;\s])color:\s*var\(--tone\)/.test(idRule[1]),
       "#sRankLetter is not drawn in the tone, so the rank's tone never shows");
  need(/\$\("sRank"\)\.setAttribute\("data-tone",\s*rankTone\(s\)\)/.test(html),
       "the rank block is never given the rank's tone");
  return `${letters.join("")}: ${gold.join("")} gold, the rest cyan, rankTone honoured`;
});

check("ui: the web view's rank insignia is actually on screen", () => {
  // It was not, for as long as it existed. The status chip carried a bare "rank" class,
  // and the skill tree's node pip owns an unscoped `.rank { position: absolute;
  // bottom: -7px; right: -7px }` — so the chip was pinned to the bottom-right corner of
  // the status card. Nothing about that is a syntax error and both screens still
  // "worked", which is exactly why it went unnoticed until the chip gained an emblem.
  const view = systemPage();
  need(!/^\s*\.rank\s*\{/m.test(view),
       "an unscoped `.rank {` rule is back — it will also match the status screen's " +
       "rank element. Scope it to `.node .rank`.");
  for (const id of ["sRankIcon", "sRankLetter"]) {
    need(new RegExp(`id="${id}"`).test(view), `the status screen lost #${id}`);
    need(new RegExp(`\\$\\("${id}"\\)`).test(view), `nothing fills #${id} at render time`);
  }
  return "scoped, and both parts are rendered";
});

/* ---- the hud page (toasts, flourish, threat labels) --------------------------------
   It plays over live gameplay and never closes, so what it costs per frame is what the
   whole rewrite is about (spec 9). The CSS helpers strip comments first: a comment
   that mentions "transition: width" is not a transition. */
const cssOf = (text) => text.replace(/\/\*[\s\S]*?\*\//g, "");
// common.js run against a fake window and document; returns its isekaiUi.
const commonUi = (doc = {}) => {
  const win = {};
  new Function("window", "document", read(PAGE_COMMON))(win, {
    querySelectorAll: () => [], addEventListener: () => {}, readyState: "loading",
    documentElement: { style: { setProperty: () => {} } }, ...doc,
  });
  return win.isekaiUi;
};
const hudStyle = () => cssOf([...read(HUD_PAGE).matchAll(/<style\b[^>]*>([\s\S]*?)<\/style>/g)].map((m) => m[1]).join("\n"));
const hudScript = () => [...read(HUD_PAGE).matchAll(/<script\b(?![^>]*\bsrc=)[^>]*>([\s\S]*?)<\/script>/g)].map((m) => m[1]).join("\n");

/* Every property a stylesheet animates: the @keyframes bodies and the transition lists.
   Returns [[where, property], ...], how many @keyframes were read, and whether any
   animation loops forever. */
function animatedProps(css) {
  const out = [];
  let keyframes = 0;
  for (const m of css.matchAll(/@keyframes\s+([\w-]+)\s*\{((?:[^{}]*\{[^{}]*\})*)\s*\}/g)) {
    keyframes++;
    for (const step of m[2].matchAll(/\{([^{}]*)\}/g)) {
      for (const d of step[1].matchAll(/(^|;)\s*([\w-]+)\s*:/g)) out.push([`@keyframes ${m[1]}`, d[2]]);
    }
  }
  const noKeyframes = css.replace(/@keyframes\s+[\w-]+\s*\{(?:[^{}]*\{[^{}]*\})*\s*\}/g, "");
  for (const m of noKeyframes.matchAll(/(?:^|[;{\s])transition(?:-property)?\s*:\s*([^;}]+)/g)) {
    for (const part of m[1].split(/,(?![^(]*\))/)) {
      const prop = part.trim().split(/\s+/)[0];
      if (prop && !/^\d/.test(prop)) out.push(["transition", prop]);
    }
  }
  const loops = /(?:^|[;{\s])animation(?:-iteration-count)?\s*:[^;}]*\binfinite\b/.test(noKeyframes);
  return { out, keyframes, loops };
}

check("hud: the page animates only transform and opacity", () => {
  // Anything else — width, top, a colour, a filter — makes Ultralight lay out or re-raster
  // the view on every frame of the animation, over a running game, and an `infinite`
  // keeps it doing that forever. theme.css counts: the hud loads it.
  const ALLOWED = new Set(["transform", "opacity", "none"]);
  // The scanner itself must catch a bad sheet, or it verifies nothing.
  const probe = animatedProps("@keyframes x { 0% { opacity: 0; } to { width: 2px; } } .a { transition: opacity 1s, left 2s; animation: x 1s infinite; }");
  need(probe.out.some((p) => p[1] === "width") && probe.out.some((p) => p[1] === "left") && probe.loops,
       "the animation scanner no longer sees keyframe or transition properties — parser stale?");

  let total = 0, frames = 0;
  for (const [name, css] of [[HUD_PAGE, hudStyle()], [PAGE_THEME, cssOf(read(PAGE_THEME))]]) {
    const { out, keyframes, loops } = animatedProps(css);
    need(keyframes > 0, `no @keyframes parsed in ${name} — parser stale?`);
    for (const [where, prop] of out) {
      need(ALLOWED.has(prop), `${name}: ${where} animates "${prop}" — only transform and opacity may move`);
    }
    need(!loops, `${name}: an animation loops forever (infinite) — every hud animation must end`);
    total += out.length; frames += keyframes;
  }
  // The script may switch a transition off (a frame handed to a new actor must not slide)
  // and must not set any other.
  for (const m of hudScript().matchAll(/\.style\.(transition\w*)\s*=\s*([^;\n]+)/g)) {
    need(/^(fresh \? "none" : ""|"none"|"")$/.test(m[2].trim()),
         `hud.html sets style.${m[1]} = ${m[2].trim()} — the script may only switch a transition off`);
  }
  return `${frames} keyframes and ${total} animated properties, all transform/opacity, none looping`;
});

check("hud: a label position push writes one transform and nothing else", () => {
  // labels.pos runs every frame while labels move. A layout read there (offsetWidth,
  // getBoundingClientRect, innerWidth) forces a synchronous layout per call, DOM creation
  // or a class change restyles the page — at 60 Hz, over the game. The view's size is
  // cached on resize instead.
  const js = hudScript();
  const body = (name) => {
    const at = js.search(new RegExp(`function ${name}\\(`));
    need(at >= 0, `function ${name}( not found in hud.html — parser stale?`);
    const open = js.indexOf("{", at);
    let depth = 0, i = open;
    for (; i < js.length; i++) {
      if (js[i] === "{") depth++;
      else if (js[i] === "}" && --depth === 0) break;
    }
    return js.slice(open, i + 1);
  };
  const FORBIDDEN = /createElement|appendChild|insertBefore|replaceChild|removeChild|innerHTML|textContent|classList|setAttribute|getBoundingClientRect|getComputedStyle|\b(offset|client|scroll)(Width|Height|Top|Left)\b|innerWidth|innerHeight|querySelector|getElementById/;
  for (const name of ["pos", "place"]) {
    const src = body(name);
    const bad = src.match(FORBIDDEN);
    need(!bad, `${name}() uses ${bad && bad[0]} — the per-frame label path may only write transforms`);
    for (const m of src.matchAll(/\.style\.(\w+)\s*=/g)) {
      need(m[1] === "transform" || m[1] === "opacity", `${name}() writes style.${m[1]} per frame`);
    }
  }
  need(/style\.transform\s*=/.test(body("place")), "place() no longer moves the label by transform");
  need(/\bplace\(/.test(body("pos")), "pos() no longer goes through place()");
  // Liveness: any labels.* call keeps the labels, STALE_MS without one hides them. The
  // limit is UiLogic's LabelHeartbeat::kPageStale, 600 ms: the 250 ms keep() plus room for
  // a frame that lands late at a low frame rate, so the labels do not blink.
  const stale = js.match(/\bconst\b[^;]*\bSTALE_MS\s*=\s*(\d+)\b/);
  const pageStale = read("src/UI/UiLogic.h").match(/kPageStale\{\s*(\d+)\s*\}/);
  need(stale, "the staleness limit STALE_MS is gone from hud.html");
  need(pageStale, "LabelHeartbeat::kPageStale not found in UiLogic.h — parser stale?");
  need(+stale[1] === +pageStale[1], `hud.html hides labels after ${stale[1]} ms, UiLogic.h says ${pageStale[1]} ms`);
  need(+stale[1] === 600, `the staleness limit is ${stale[1]} ms, the ruling is 600 ms`);
  for (const name of ["full", "pos", "keep"]) {
    need(/\balive\(\)/.test(body(name)), `labels.${name}() is not a sign of life (alive())`);
  }
  return `pos/place: transforms and opacity only; full/pos/keep keep labels alive for ${stale[1]} ms`;
});

check("hud: threat labels take the plugin's verdict tones", () => {
  // The tone IS the verdict at a glance, so the page and Logic::ToneFor(Verdict) must
  // never disagree. The plugin sends the tone; the page falls back to the same rule for a
  // label without one. RUN the page's functions against every Logic::Verdict.
  const logic = read("src/UI/UiLogic.h");
  const en = logic.match(/enum class Verdict\s*:\s*[\w:]+\s*\{([^}]*)\}/);
  need(en, "enum class Verdict not found in UiLogic.h — parser stale?");
  const names = en[1].split(",").map((s) => s.trim()).filter(Boolean);
  const fn = logic.match(/constexpr Tone ToneFor\(Verdict a_verdict\)\s*\{([\s\S]*?)\n    \}/);
  need(fn, "ToneFor(Verdict) not found in UiLogic.h — parser stale?");
  need(/\?\s*Tone::Red\s*:\s*Tone::Cyan/.test(fn[1]),
       "ToneFor(Verdict) is no longer `verdicts ? Red : Cyan` — update this check with it");
  const red = [...fn[1].matchAll(/Verdict::(\w+)/g)].map((m) => m[1]);
  need(red.length > 0, "no red verdict parsed from ToneFor(Verdict) — parser stale?");

  const js = hudScript();
  const grab = (re, what) => { const m = js.match(re); need(m, `${what} not found in hud.html — parser stale?`); return m[0]; };
  const src = [
    grab(/const VERDICTS = \[[^\]]*\];/, "VERDICTS"),
    grab(/function verdictName\(v\) \{[\s\S]*?\n  \}/, "verdictName(v)"),
    grab(/function verdictTone\(v\) \{[\s\S]*?\n  \}/, "verdictTone(v)"),
    grab(/function labelTone\(l\) \{[^\n]*\}/, "labelTone(l)"),
  ].join("\n");
  // The page's own TONES: hud.html takes common.js's, so run common.js and hand that in.
  need(/\bconst TONES = ui\.TONES;/.test(js), "hud.html no longer takes its TONES from common.js (ui.TONES) — update this check");
  const tones = commonUi().TONES;
  need(tones && Object.getPrototypeOf(tones) === null,
       "common.js's TONES has a prototype — a payload tone of \"constructor\" would pass as a tone");
  const page = new Function("TONES", `${src}\nreturn { VERDICTS, verdictTone, labelTone };`)(tones);
  need(JSON.stringify(page.VERDICTS) === JSON.stringify(names.map((n) => n.toLowerCase())),
       `the page's VERDICTS ${JSON.stringify(page.VERDICTS)} are not Logic::Verdict in order (${names.join(", ")})`);
  names.forEach((n, i) => {
    const want = red.includes(n) ? "red" : "cyan";
    for (const v of [n.toLowerCase(), n.toUpperCase(), i]) {
      need(page.verdictTone(v) === want, `verdict ${JSON.stringify(v)}: the plugin says ${want}, the page ${page.verdictTone(v)}`);
    }
  });
  need(page.labelTone({ verdict: "trivial", tone: "gold" }) === "gold", "the page ignores the plugin's tone");
  need(page.labelTone({ verdict: "lethal", tone: "bogus" }) === "red", "an unknown tone must fall back to the verdict's own");
  need(page.labelTone({ verdict: "lethal", tone: "constructor" }) === "red", "a prototype name passes as a tone");
  // At most as many labels as the plugin selects.
  const cap = read("src/UI/ThreatLabels.cpp").match(/kMaxLabels\s*=\s*(\d+)/);
  need(cap, "kMaxLabels not found in ThreatLabels.cpp — parser stale?");
  need(new RegExp(`\\bMAX_LABELS\\s*=\\s*${cap[1]}\\b`).test(js), `hud.html's MAX_LABELS is not the plugin's ${cap[1]}`);
  return `${names.join("/")}: ${red.join(", ")} red, the rest cyan; plugin tone honoured; cap ${cap[1]}`;
});

check("hud: the page speaks the plugin's contract and keeps the ImGui timings", () => {
  // The hud is driven entirely by InvokeJS calls the plugin makes by name; a renamed entry
  // point is a toast that silently never appears. The page answers with isekai.ready and
  // nothing else — it has no actions, so a send of anything else is a bug.
  const html = read(HUD_PAGE);
  const js = hudScript();
  need(js.trim(), "no inline script in hud.html — parser stale?");
  try { new Function(js); } catch (e) { need(false, `hud.html's script does not parse: ${e.message}`); }
  need(/<script\s+src="version\.js"[^>]*onerror=/.test(html), "hud.html does not load version.js with a failure-tolerant <script>");
  need(/<script\s+src="common\.js"/.test(html) && /href="theme\.css"/.test(html), "hud.html no longer loads common.js and theme.css");
  for (const fn of ["loc", "scale", "vr", "hello", "toast", "flourish"]) {
    need(new RegExp(`\\b${fn}\\s*:\\s*(function\\b|\\([^)]*\\)\\s*=>)`).test(js), `hud.html no longer defines isekai.${fn}`);
  }
  need(/labels\s*:\s*\{\s*full\s*:\s*full\s*,\s*pos\s*:\s*pos\s*,\s*keep\s*:\s*keep\s*\}/.test(js), "isekai.labels is not { full, pos, keep }");
  need(/\bhello\s*:[^\n]*\bready\(\)/.test(js), "isekai.hello() no longer re-sends isekai.ready");
  need(/\bui\.ready\(\);\s*\}\)\(\);$/.test(js.trim()), "hud.html no longer sends isekai.ready when it loads");
  need(!/\bui\.send\(|magelight\.send|escapeCapture/.test(js), "hud.html sends something besides isekai.ready — the hud has no actions");
  // spec / constraints: toast line 3.2 s, banner 5.5 s, fade 0.8 s, rise 0.22 s; flourish 3.2 s, fade 0.7 s.
  // The toast strip is shared with the system page: its script is common.js's, its
  // styles theme.css's.
  need(/\bui\.toaster\(\$\("toasts"\)\)/.test(js) && /id="toasts"/.test(html), "hud.html no longer runs common.js's toaster on its #toasts");
  const common = read(PAGE_COMMON);
  const num = (src, file, name) => { const m = src.match(new RegExp(`\\b${name}\\s*=\\s*(\\d+)`)); need(m, `${name} not found in ${file}`); return +m[1]; };
  const want = [[common, PAGE_COMMON, { LINE_MS: 3200, BANNER_MS: 5500, FADE_MS: 800 }],
                [js, HUD_PAGE, { FLOURISH_MS: 3200, FLOURISH_FADE_MS: 700 }]];
  for (const [src, file, vals] of want) {
    for (const [k, v] of Object.entries(vals)) need(num(src, file, k) === v, `${file}: ${k} is ${num(src, file, k)}, the timings say ${v}`);
  }
  const theme = cssOf(read(PAGE_THEME));
  need(/\.toast\s*\{[^}]*animation:\s*toast-rise\s+220ms/.test(theme), "the toast no longer rises in 220 ms");
  need(/\.toast \.tin\s*\{[^}]*animation:\s*toast-fade\s+800ms/.test(theme), "the toast fade is no longer 800 ms");
  need(/#toasts\s*\{[^}]*pointer-events:\s*none/.test(theme), "#toasts takes clicks — on the system page it would sit over the buttons");
  // Run the toaster: a key replaces in place, a fifth toast pushes the oldest out.
  {
    const made = [];
    const mk = (tag) => {
      const e = { tagName: tag, children: [], parentNode: null, style: {}, attrs: {}, className: "", textContent: "",
                  setAttribute(k, v) { this.attrs[k] = v; },
                  appendChild(c) { c.parentNode = this; this.children.push(c); return c; },
                  removeChild(c) { this.children.splice(this.children.indexOf(c), 1); c.parentNode = null; return c; },
                  replaceChild(n, o) { const i = this.children.indexOf(o); this.children[i] = n; n.parentNode = this; o.parentNode = null; return o; },
                  get firstElementChild() { return this.children[0]; } };
      made.push(e);
      return e;
    };
    const ui = commonUi({ createElement: mk });
    const box = mk("div");
    const toast = ui.toaster(box);
    const texts = () => box.children.map((c) => c.children[0].children.at(-1).textContent);
    toast({ text: "Wolves 1/5", key: "q" });
    toast({ text: "[ SYSTEM ] New objective", banner: true, tone: "gold" });
    toast({ text: "Wolves 2/5", key: "q" });
    need(JSON.stringify(texts()) === JSON.stringify(["Wolves 2/5", "New objective"]),
         `a keyed toast did not replace in place: ${JSON.stringify(texts())}`);
    need(box.children[1].attrs["data-tone"] === "gold" && /banner/.test(box.children[1].className),
         "the toaster drops the banner flag or the plugin's tone");
    toast({ text: "x", tone: "constructor" });
    need(box.children[2].attrs["data-tone"] === "cyan", "a prototype name passes as a toast tone");
    toast({ text: "a" }); toast({ text: "b" }); toast({ text: "   " });
    need(box.children.length === 4 && texts()[0] === "New objective", `more than four toasts, or the wrong one went: ${JSON.stringify(texts())}`);
  }
  // The system page mirrors toasts (spec 6.2), and the plugin sends there exactly then.
  const sys = read(SYSTEM_PAGE);
  need(/\btoast\s*:\s*\([^)]*\)\s*=>/.test(sys) && /ui\.toaster\(\$\("toasts"\)\)/.test(sys) && /id="toasts"/.test(sys),
       "system.html no longer mirrors toasts through common.js's toaster");
  const hudCpp = read("src/UI/Hud.cpp");
  need(/HudBelowPanels\(\)\s*&&\s*Magelight::HoldsUiMode\(\)\)\s*\{\s*Magelight::Run\(Magelight::View::System,\s*js\);\s*\}\s*else\s*\{\s*Magelight::Run\(Magelight::View::Hud,\s*js\);\s*\}/.test(hudCpp),
       "Hud.cpp must send a toast to the system view INSTEAD of the hud while the hud sits below it, never to both");
  for (const call of ["isekai.toast(", "isekai.flourish(", "isekai.labels.full(", "isekai.labels.pos(", "isekai.labels.keep()"]) {
    need(hudCpp.includes(`"${call}`), `Hud.cpp no longer calls ${call}`);
  }
  need(/\.fl\s*\{[^}]*animation:\s*fl-out\s+700ms/.test(hudStyle()), "the flourish fade is no longer 700 ms");
  // No light cone or beam (user look ruling, spec 7.1).
  need(!/\b(beam|cone|pillar)\b/i.test(cssOf(html).replace(/<!--[\s\S]*?-->/g, "").replace(/\/\/.*$/gm, "")),
       "hud.html draws a beam/cone/pillar — the look ruling says no light cone anywhere");
  return "loc/scale/vr/hello/toast/flourish/labels{full,pos,keep}, ready only, timings as the ImGui strip; toaster shared with the system page";
});

check("hud: the plugin sends labels the way the page reads them", () => {
  // hud.html's label contract (its header): y runs DOWN, s is the distance factor alone
  // (the page multiplies UiScale in), nums:false hides the health figures, pools with max
  // 0 draw nothing. And the liveness: keep() while nothing moves, and a pausing menu or
  // the console clears the labels at once (the frame tick stops there).
  const hud = read("src/UI/Hud.cpp");
  const sel = read("src/UI/ThreatLabels.cpp");
  need(/Logic::PageY\(/.test(sel), "ThreatLabels.cpp no longer flips the port's y into the page's (Logic::PageY)");
  need(/Logic::LabelDistanceScale\(/.test(hud), "Hud.cpp no longer sends s = Logic::LabelDistanceScale");
  need(!/UiScale|g_userScale|g_scale/.test(hud), "Hud.cpp scales labels by UiScale — the page does that, it would apply twice");
  need(/j\["nums"\]\s*=/.test(hud) && /ThreatLabelNumbers\(\)/.test(hud), "Hud.cpp no longer sends nums (ThreatLabelNumbers)");
  need(/ThreatLabelResources\(\)/.test(hud) && /a_res \? Sent\(a_label\.mp, false\) : LabelPool\{\}/.test(hud) &&
       /a_res \? Sent\(a_label\.sp, false\) : LabelPool\{\}/.test(hud),
       "Hud.cpp no longer drops magicka/stamina when ThreatLabelResources is off");
  // Combat regen changes magicka and stamina every frame: hashed and sent exactly, every
  // frame would be a full push. Pools without figures go at bar resolution; health keeps
  // its exact figures only while they are shown. Hash and payload come from one place.
  need(/Logic::PoolStep\(/.test(hud) && /Sent\(a_label\.hp, a_nums\)/.test(hud),
       "Hud.cpp no longer quantises the pools whose figures are not shown (Logic::PoolStep)");
  const contentOf = hud.match(/std::uint64_t ContentOf\([^)]*\) \{([\s\S]*?)\n        \}/);
  const fullJs = hud.match(/std::string FullJs\([\s\S]*?\n        \}/);
  need(contentOf && /ShownOf\(/.test(contentOf[1]) && !/a_label\.(hp|mp|sp)/.test(contentOf[1]) &&
       fullJs && /ShownOf\(/.test(fullJs[0]) && !/PoolJson\(l\./.test(fullJs[0]),
       "the content hash and labels.full must both use ShownOf — a raw pool would let regen defeat the gate");
  // Before the hud page is ready every call queues (newest 64 kept): label pushes would evict toasts.
  const onFrameSrc = hud.slice(hud.indexOf("void OnFrame()"), hud.indexOf("SelectLabels("));
  need(/PageReady\(Magelight::View::Hud\)/.test(onFrameSrc), "OnFrame pushes labels before the hud page is ready");
  need(/LabelHeartbeat/.test(hud) && /KeepDue\(/.test(hud), "Hud.cpp no longer sends the keep() heartbeat");
  need(/BSTEventSink<RE::MenuOpenCloseEvent>/.test(hud) && /PausesGame\(\)/.test(hud) && /Console::MENU_NAME/.test(hud),
       "Hud.cpp no longer clears the labels when a pausing menu or the console opens");
  need(/FrameTick::Add\(&OnFrame\)/.test(hud), "the label pass is no longer on the frame tick");
  // Toasts and the flourish are NOT on the frame tick: it stops while paused and in tfc.
  // Code only: a comment may well explain why toasts are not here.
  const onFrame = hud.slice(hud.indexOf("void OnFrame()"), hud.indexOf("class MenuWatcher")).replace(/\/\/.*$/gm, "");
  need(onFrame.length > 0 && !/toast|flourish/i.test(onFrame), "OnFrame sends toasts or the flourish — they would stop while paused");
  // The page's staleness vs the heartbeat period: a keep() that goes out one late frame
  // after its period must still land well inside it.
  const period = read("src/UI/UiLogic.h").match(/kPeriod\{\s*(\d+)\s*\}/);
  const staleMs = read(HUD_PAGE).match(/\bSTALE_MS\s*=\s*(\d+)\b/);
  need(period && staleMs, "kPeriod or STALE_MS not found — parser stale?");
  need(+period[1] === 250, `the keep() period is ${period[1]} ms, the ruling is 250 ms`);
  need(+period[1] * 2 < +staleMs[1], `the keep() period (${period[1]} ms) is not well inside the page's ${staleMs[1]} ms`);
  return `y down, s unscaled, nums/resources, keep() every ${period[1]} ms, menu clear`;
});

check("ui: the game's panels pass their spec 7.2 tones", () => {
  // The page never guesses a panel's tone (spec 7.2): a call site that drops it shows the
  // blessing choice in plain cyan. Each function below holds the panel it names.
  const sys = read("src/System.cpp");
  const fn = (src, name) => {
    const at = src.indexOf(name);
    need(at >= 0, `${name} not found — parser stale?`);
    const next = src.slice(at + name.length).search(/\n        (?:void|\[\[nodiscard\]\]|bool|std::)\s/);
    return src.slice(at, next < 0 ? undefined : at + name.length + next);
  };
  const want = [
    [sys, "void ShowPowerSelection() {", /UI::Logic::Tone::Gold\)/, "the blessing choice is not gold"],
    [sys, "void ShowTransmigratorSelection() {", /UI::Logic::Tone::Gold\)/, "the transmigrator gifts are not gold"],
    [sys, "void ShowPathSelection()", /g_state\.dormant \? UI::Logic::ToneFor\(UI::Logic::Blessing::Dormant\)\s*:\s*UI::Logic::Tone::Cyan\)/,
     "the Full/Shattered choice is not violet when dormant, else cyan"],
    [sys, "void ShowPathSelection()", /"SHATTERED"\),\s*BlessingIcon\("blessing_shattered\.png"\), false, UI::Logic::Tone::Red \}/,
     "the SHATTERED button is not red"],
    [sys, "void AskTier(", /ToneFor\(UI::Logic::Blessing::Custom\)/, "the CUSTOM builder is not ToneFor(Custom)"],
    [sys, "void AwakenTo(", /UI::Logic::Tone::Gold\)/, "AWAKENING / TRANSCENDENCE is not gold"],
    [read("src/Progression.cpp"), "void ShowPanelWhenFree(", /UI::Logic::Tone::Gold\)/, "a milestone payout is not gold"],
    [read("src/UI/Screens.cpp"), "void ConfirmReboot()", /L\("reboot\.confirm"[\s\S]*Logic::Tone::Red\)/,
     "the reboot confirmation is gone or not red"],
  ];
  for (const [src, name, re, why] of want) need(re.test(fn(src, name)), why);
  // REINCARNATION COMPLETE, after the AWAKENED flourish.
  const complete = sys.slice(sys.indexOf('L("flourish.awakened", "AWAKENED"), PowerName(g_state.power))'));
  need(/button\.continue[\s\S]{0,600}?UI::Logic::Tone::Gold\)/.test(complete), "REINCARNATION COMPLETE is not gold");
  // The reboot is confirmed by that panel alone: no second press on the page.
  need(!/rebootArmed|confirmReboot/.test(read(`${PAGE_SCREENS}/status.js`)), "status.js arms the reboot button again (the plugin's panel confirms it)");
  return `${want.length + 1} panels toned, reboot confirmed by its red panel`;
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
  // Split on either line ending and strip comments with [\s\S] rather than `.`, because
  // `.` does not match \r: on a CRLF file "; note = x\r" left `.*$` unable to anchor, the
  // comment survived stripping, and every commented line containing an "=" was reported
  // as a dead setting. Editing the ini in a Windows editor was enough to trigger it.
  const ini = read("IsekaiHero.ini");
  const documented = new Set(
    ini.split(/\r?\n/)
       .map((l) => l.replace(/[;#][\s\S]*$/, "").trim())
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
  const view = systemPage();
  need(/SHOP\.shelves/.test(view), "the view no longer reads SHOP.shelves — rail parser stale?");
  return cpp.join(" / ");
});

/* -- 4. cross-layer JSON contracts ----------------------------------------- */

function contract(label, cppFile, prefix) {
  const cpp = read(cppFile);
  const view = systemPage();
  const pg = read("playground/index.html") + read("playground/mock.js");

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

check("system: the blessing grants no character level", () => {
  // Skyrim's levelled lists key off the player's character level, so granting one pulled
  // every scaling enemy to its ceiling in a single step. The grant is gone, and so is the
  // machinery that held the number up across loads — actorData.level does not persist for
  // the player, which is the only reason that machinery existed.
  const sys = read("src/System.cpp");
  const gone = ["SetPlayerLevelAtLeast", "ReapplyLevelOnLoad", "RestoreLevelBeforeGrant",
                "g_levelBeforeGrant", "AttributeBonusPerStat"];
  const left = gone.filter((n) => sys.includes(n));
  need(left.length === 0,
       `System.cpp still carries ${left.join(", ")} — the level grant is only half removed, ` +
       `and a half-removed grant still moves the world`);
  need(!/std::uint16_t\s+playerLevel;/.test(sys),
       "Blessing still has a playerLevel field");
  need(/std::uint16_t\s+attrTarget;/.test(sys),
       "Blessing has no attrTarget field — attributes must be a target, not a level derivation");
  return "no level grant";
});

check("skill tree: every node price goes through NodeCost", () => {
  // RespecRefund gives back what RankCost charged. If one of them reads Node::cost raw
  // while the other applies the character's price scale, a respec refunds at a different
  // rate than was paid — which is exactly the promise MasteryCostToRank exists to keep.
  const src = read("src/SkillTree.cpp");
  const helper = src.match(/std::int32_t NodeCost\(const Node& a_node\)[\s\S]*?\n        \}/);
  need(helper, "NodeCost() not found in SkillTree.cpp");
  const raw = [...src.replace(helper[0], "").matchAll(/\b[A-Za-z_]\w*\.cost\b/g)]
    .map((m) => m[0]);
  need(raw.length === 0,
       `these read a node's price directly instead of through NodeCost(): ${raw.join(", ")}`);
  return "all prices scaled";
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
    ...grab("src/Voice.cpp", /^\s*(0x000[0-9A-Fa-f]{3}),\s*\/\/\s*(.+)$/gm),
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
        // Compressed records are rare here and nothing reads into them; their body stays
        // empty rather than half-parsed.
        const body = (rflags & 0x00040000) ? Buffer.alloc(0) : buf.subarray(off + 24, off + 24 + dsize);
        recs.push({ sig, formID: fid, local: fid & 0x00ffffff, edid: edidOf(body), body });
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
  add("src/Voice.cpp", /^\s*(0x000[0-9A-Fa-f]{3}),\s*\/\/\s*(.+)$/gm);
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

check("ESP: the voice table names 20 SNDRs whose files all ship", () => {
  // Voice.cpp resolves these by local FormID; the self-test reports N/20. A row pointing at
  // something other than a sound descriptor, or a descriptor naming a WAV that is not in
  // sounds/voice/ (package.ps1 ships that folder to Sound\fx\isekai\voice\), is silence.
  const ids = [...read("src/Voice.cpp").matchAll(/^\s*(0x000[0-9A-Fa-f]{3}),\s*\/\/\s*(.+)$/gm)]
    .map((m) => ({ id: parseInt(m[1], 16), note: m[2].trim() }));
  need(ids.length === 20, `Voice lists ${ids.length} descriptors, expected 20`);
  const { recs } = parseEsp();
  const byLocal = new Map(recs.map((r) => [r.local, r]));
  const prefix = "fx\\isekai\\voice\\";
  let files = 0;
  for (const { id, note } of ids) {
    const hex = `0x${id.toString(16).toUpperCase().padStart(6, "0")}`;
    const r = byLocal.get(id);
    need(r && r.sig === "SNDR", `${hex} (${note}) is ${r ? `a ${r.sig}` : "missing"}, not an SNDR`);
    need(r.edid === note, `${hex} is ${r.edid} in the ESP but Voice.cpp calls it ${note}`);
    need(r.body.length > 0, `${hex} is compressed, so its files cannot be read`);
    const anams = [];
    for (let i = 0; i + 6 <= r.body.length;) {
      const size = r.body.readUInt16LE(i + 4);
      if (r.body.subarray(i, i + 4).toString("ascii") === "ANAM") {
        anams.push(r.body.subarray(i + 6, i + 6 + size).toString("ascii").replace(/\0.*$/, ""));
      }
      i += 6 + size;
    }
    need(anams.length > 0, `${hex} (${note}) lists no sound file`);
    for (const a of anams) {
      need(a.toLowerCase().startsWith(prefix), `${note} plays ${a}, outside ${prefix}`);
      const name = a.slice(prefix.length);
      need(existsSync(join(ROOT, "sounds", "voice", name)), `${note}: sounds/voice/${name} is missing`);
      ++files;
    }
  }
  return `20 SNDRs, ${files} files present`;
});

check("ESP: the voice has its own category, outside the menu pause", () => {
  // Every System window pauses the game. Under AudioCategoryVOC (-> PausedDuringMenu) a line
  // waited for the window to close, so the voice has its own SNCT 0xDCB directly under
  // _AudioCategoryMaster, with its own slider in Settings -> Audio.
  const { recs } = parseEsp();
  const sub = (body, want) => {
    for (let i = 0; i + 6 <= body.length;) {
      const size = body.readUInt16LE(i + 4);
      if (body.subarray(i, i + 4).toString("ascii") === want) return body.subarray(i + 6, i + 6 + size);
      i += 6 + size;
    }
    return null;
  };
  const OWN_SNCT = 0x02000dcb;  // 02 = this plugin's own index (two masters)
  const cat = recs.find((r) => r.local === 0xdcb);
  need(cat && cat.sig === "SNCT", `0xDCB is ${cat ? `a ${cat.sig}` : "missing"}, not an SNCT`);
  need(cat.edid === "IsekaiAudioCategorySystemVoice", `0xDCB is ${cat.edid}, not IsekaiAudioCategorySystemVoice`);
  need(sub(cat.body, "PNAM")?.readUInt32LE(0) === 0x0eb803,
       "IsekaiAudioCategorySystemVoice's parent is not _AudioCategoryMaster (0x0EB803)");
  need((sub(cat.body, "FNAM")?.readUInt32LE(0) & 2) === 2,
       "IsekaiAudioCategorySystemVoice lacks ShouldAppearOnMenu - no slider in the Audio settings");
  const vox = recs.filter((r) => r.sig === "SNDR" && r.edid.startsWith("IsekaiVOX_"));
  need(vox.length === 20, `${vox.length} IsekaiVOX SNDRs, expected 20`);
  const wrong = vox.filter((r) => sub(r.body, "GNAM")?.readUInt32LE(0) !== OWN_SNCT);
  need(wrong.length === 0, `not in category 0xDCB: ${wrong.map((r) => r.edid).join(", ")}`);
  return "20 SNDRs in SNCT 0xDCB under _AudioCategoryMaster, on the menu";
});

check("ESP: the storage chest's model is empty and ships", () => {
  // The Dimensional Storage chest is a real reference parked under the player. Players
  // still saw it beside them now and then (#38), so the base record carries a model with
  // nothing in it: wherever the reference ends up, there is nothing to see. Rebuilding the
  // record in the Creation Kit would quietly put a chest mesh back.
  const { recs } = parseEsp();
  const chest = recs.find((r) => r.sig === "CONT" && r.local === 0xd7a);
  need(chest, "CONT 0xD7A (IsekaiStorageContainer) is missing from the ESP");
  need(chest.body.length > 0, "CONT 0xD7A is compressed, so its MODL cannot be read");
  let modl = null;
  for (let i = 0; i + 6 <= chest.body.length;) {
    const sig = chest.body.subarray(i, i + 4).toString("ascii");
    const size = chest.body.readUInt16LE(i + 4);
    if (sig === "MODL") modl = chest.body.subarray(i + 6, i + 6 + size).toString("latin1").replace(/\0.*$/s, "");
    i += 6 + size;
  }
  need(modl !== null, "CONT 0xD7A has no MODL");
  const want = "isekai\\storage\\empty.nif";
  need(modl.toLowerCase() === want, `the chest's model is ${modl}, expected ${want}`);

  const nifPath = join(ROOT, "meshes/isekai/storage/empty.nif");
  need(existsSync(nifPath), "meshes/isekai/storage/empty.nif does not exist");
  const nif = readFileSync(nifPath);
  const head = "Gamebryo File Format, Version 20.2.0.7\n";
  need(nif.subarray(0, head.length).toString("latin1") === head,
       "empty.nif does not start with the 20.2.0.7 header string");
  // Header after the string: version, endian, user version, block count, BS version, then
  // three byte-length-prefixed export strings, then the block-type table.
  let o = head.length;
  need(nif.readUInt32LE(o) === 0x14020007, "empty.nif is not version 20.2.0.7");
  o += 4 + 1;
  need(nif.readUInt32LE(o) === 12, "empty.nif user version is not 12");
  o += 4;
  const blocks = nif.readUInt32LE(o); o += 4;
  need(nif.readUInt32LE(o) === 100, "empty.nif BS version is not 100 (Skyrim SE)");
  o += 4;
  for (let s = 0; s < 3; s++) o += 1 + nif[o];
  const nTypes = nif.readUInt16LE(o); o += 2;
  const types = [];
  for (let t = 0; t < nTypes; t++) {
    const len = nif.readUInt32LE(o);
    types.push(nif.subarray(o + 4, o + 4 + len).toString("latin1"));
    o += 4 + len;
  }
  need(blocks === 1, `empty.nif has ${blocks} blocks, expected 1`);
  need(types.length === 1 && types[0] === "BSFadeNode",
       `empty.nif block types are [${types.join(", ")}], expected [BSFadeNode]`);
  return `MODL ${modl}; empty.nif is 1 × BSFadeNode, ${nif.length} bytes`;
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

/* -- 9. the public mod API ------------------------------------------------- */

check("api: the public header and the implementation agree on the version", () => {
  const hdr = read("include/IsekaiHeroAPI.h");
  const impl = read("src/ModAPI.cpp");

  // Every version the header offers must be one the implementation answers, or a
  // consumer asking for it gets a silent nullptr and no idea why.
  const offered = [...hdr.matchAll(/V(\d+)\s*=\s*(\d+)/g)].map((m) => Number(m[2]));
  need(offered.length > 0, "no InterfaceVersion values found in the public header");

  const answered = [...impl.matchAll(/InterfaceVersion::V(\d+)/g)].map((m) => Number(m[1]));
  const missing = offered.filter((v) => !answered.includes(v));
  need(
    missing.length === 0,
    `header offers V${missing.join(", V")} but ModAPI.cpp never answers it`
  );

  return `V${offered.join(", V")}`;
});

check("api: every node key the public header publishes still exists", () => {
  const hdr = read("include/IsekaiHeroAPI.h");
  const tree = read("src/SkillTree.cpp");

  // The header restates node keys so a consumer never has to read our source. That
  // makes it a second copy of a fact, which in this repo always earns a check.
  const published = [...hdr.matchAll(/inline constexpr uint32_t\s+k(\w+)\s*=\s*(\d+)/g)]
    .map((m) => ({ name: m[1], key: Number(m[2]) }));
  need(published.length > 0, "no node keys found in the public header");

  const live = new Set(
    [...tree.matchAll(/\{\s*(\d+),\s*Zone::k\w+,\s*"/g)].map((m) => Number(m[1]))
  );
  need(live.size > 0, "no node table found in SkillTree.cpp");

  const gone = published.filter((p) => !live.has(p.key));
  need(
    gone.length === 0,
    `the header publishes ${gone.map((g) => `k${g.name}=${g.key}`).join(", ")}, ` +
      "which no longer exist in SkillTree.cpp. A published key is a promise - retire " +
      "the node in the header with a comment rather than deleting the constant."
  );

  const dupes = published.filter((p, i) => published.findIndex((q) => q.key === p.key) !== i);
  need(dupes.length === 0, `duplicate key values in the header: ${dupes.map((d) => d.key).join(", ")}`);

  return `${published.length} keys published, all live`;
});

check("hotkeys: every registration is rebindable, and a settings reload rebinds", () => {
  const sys = read("src/System.cpp");
  const from = sys.indexOf("void ReloadSettings()");
  need(from >= 0, "ReloadSettings not found — parser stale?");
  const reload = sys.slice(from, sys.indexOf("\n        }", from));
  need(reload.includes("BindAllHotkeys("),
       "ReloadSettings does not call BindAllHotkeys — a rebound key waits for a restart");
  need(/BindAllHotkeys\(\s*(?:\/\*[^*]*\*\/\s*)?false\s*\)/.test(reload),
       "ReloadSettings must call BindAllHotkeys(false) — a rebind stays out of the log");
  need(/kDataLoaded:[\s\S]*?BindAllHotkeys\(\s*(?:\/\*[^*]*\*\/\s*)?true\s*\)/.test(sys),
       "the kDataLoaded block must call BindAllHotkeys(true) so load logs what it bound");
  // The definition, not the forward declaration ReloadSettings needs above it.
  const bindAll = sys.slice(sys.search(/(?:void|bool) BindAllHotkeys\(bool a_log\) \{/));
  need(/ClearHotkeys\(\)/.test(bindAll.slice(0, 800)),
       "BindAllHotkeys does not clear the table first — a key set to 0 would stay armed");

  // Every Register*Hotkey call outside Input.cpp must sit in a function named Bind*.
  const offenders = [];
  for (const f of cppSources().filter((p) => !p.endsWith("UI/Input.cpp"))) {
    const src = read(f);
    for (const m of src.matchAll(/Register(?:Gamepad|VR)?Hotkey\(/g)) {
      const before = src.slice(0, m.index);
      const fn = [...before.matchAll(/\n\s*(?:void|bool)\s+(\w+)\s*\(/g)].pop();
      if (!fn || !fn[1].startsWith("Bind")) offenders.push(`${f} (in ${fn ? fn[1] : "?"})`);
    }
  }
  need(offenders.length === 0, `hotkeys registered outside a Bind* function: ${offenders.join(", ")}`);
  return "reload rebinds, all registrations rebindable";
});

/* -- 10. the main-thread tick ---------------------------------------------- */

check("tick: quests and the mod API run from FrameTick", () => {
  // The clock used to live in a render-thread Present hook, which is gone. The two
  // calls belong to the main-thread FrameTick now.
  const tick = read("src/FrameTick.cpp");
  const names = ["Quests::Tick", "ModAPI::PublishIfChanged"];
  const missing = names.filter((n) => !tick.includes(n));
  need(missing.length === 0, `FrameTick.cpp does not call ${missing.join(", ")}`);
  return "both in FrameTick";
});

check("selftest: the Magelight checks are wired", () => {
  const src = read("src/SelfTest.cpp");
  const names = ["magelight host", "system view", "hud view", "page ready"];
  const missing = names.filter((n) => !src.includes(`"${n}"`));
  need(missing.length === 0, `SelfTest.cpp lacks the check(s): ${missing.join(", ")}`);
  need(!src.includes("SKSE\\Plugins\\IsekaiHero\\icons"), "SelfTest.cpp still checks the old icon path");
  return "host, both views, page version";
});

/* -- report ---------------------------------------------------------------- */

const pad = Math.max(...results.map((r) => r.name.length));
for (const r of results) {
  console.log(`${r.ok ? "  ok  " : "FAIL  "}${r.name.padEnd(pad)}  ${r.detail}`);
}
console.log(`\n${checks - failures}/${checks} checks passed`);
process.exit(failures ? 1 : 0);
