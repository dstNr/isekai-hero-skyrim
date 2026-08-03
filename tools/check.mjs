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
    return {
      key: m[1] === "kAnalyzeNodeKey" ? 23 : +m[1],
      zone: m[2].toUpperCase(),
      x: +pos[1], y: +pos[2], cost: +pos[3],
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
  const re = /\{\s*"([^"]+)",\s*"([^"]+)",\s*(\d+),\s*"([^"]+)",\s*\n?\s*Kind::k(\w+),\s*Cat::k\w+,\s*([\d']+)\s*(?:,\s*(0x[0-9A-Fa-f]+))?\s*\}/g;
  const out = [...body.matchAll(re)].map((m) => ({
    name: m[1], qty: m[2], cost: +m[3], icon: m[4],
    kind: m[5], amount: m[6], localID: m[7] ? parseInt(m[7], 16) : null,
  }));
  need(out.length > 0, "no catalog entries parsed from Shop.cpp");
  return out;
}

function parseMockShop() {
  const mock = read("playground/mock.js");
  const re = /\{\s*name:\s*"([^"]+)",\s*qty:\s*"([^"]+)",\s*cost:\s*(\d+),\s*icon:\s*"([^"]+)"\s*\}/g;
  const out = [...mock.matchAll(re)].map((m) => ({ name: m[1], qty: m[2], cost: +m[3], icon: m[4] }));
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

check("skill tree: kAnalyzeNodeKey names a real node", () => {
  const hdr = read("src/SkillTree.h");
  const m = hdr.match(/kAnalyzeNodeKey = (\d+)/);
  need(m, "kAnalyzeNodeKey not found in SkillTree.h");
  const keys = new Set(parseSkillTreeNodes().map((n) => n.key));
  need(keys.has(+m[1]),
       `kAnalyzeNodeKey is ${m[1]} but no node has that key — the Analyze hotkey ` +
       `could never be unlocked`);
  return `#${m[1]}`;
});

/* -- 2. zone geometry ------------------------------------------------------
   Both renderers pad zone frames in design units scaled by the same fit factor
   as the node positions and radii, so the whole layout is proportional and can
   be verified in design units alone. It was NOT proportional once: the padding
   was in raw pixels, and below a fit of ~0.9 the frames grew into each other. */

const NODE_R = 31, ZPAD_X = 16, ZPAD_TOP = 18, ZPAD_BOT = 44;

function zoneFrames() {
  const graph = parseSkillTreeNodes().filter((n) => n.zone !== "MASTERY");
  const z = {};
  for (const n of graph) {
    const r = NODE_R * n.scale;
    const b = (z[n.zone] ??= { x0: 1e9, x1: -1e9, y0: 1e9, y1: -1e9 });
    b.x0 = Math.min(b.x0, n.x - r); b.x1 = Math.max(b.x1, n.x + r);
    b.y0 = Math.min(b.y0, n.y - r); b.y1 = Math.max(b.y1, n.y + r);
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

check("skill tree: node tiles never collide", () => {
  const nodes = parseSkillTreeNodes().filter((n) => n.zone !== "MASTERY");
  let closest = Infinity, pair = "";
  for (let i = 0; i < nodes.length; i++) {
    for (let j = i + 1; j < nodes.length; j++) {
      const a = nodes[i], b = nodes[j];
      const need_ = NODE_R * a.scale + NODE_R * b.scale;
      const gap = Math.max(Math.abs(a.x - b.x) - need_, Math.abs(a.y - b.y) - need_);
      if (gap < closest) { closest = gap; pair = `${a.key}/${b.key}`; }
      need(gap >= 0, `tiles ${a.key} and ${b.key} overlap`);
    }
  }
  return `closest ${pair} at ${closest.toFixed(0)} units`;
});

check("skill tree: a node name can never reach its neighbour", () => {
  // Label wrap is 140 design units (both renderers). Two same-row neighbours must
  // therefore sit more than 140 apart, or their centred labels can touch.
  const WRAP = 140;
  const nodes = parseSkillTreeNodes().filter((n) => n.zone !== "MASTERY");
  let tightest = Infinity;
  for (let i = 0; i < nodes.length; i++) {
    for (let j = i + 1; j < nodes.length; j++) {
      const a = nodes[i], b = nodes[j];
      if (Math.abs(a.y - b.y) > 1) continue;         // different rows: labels cannot meet
      const dx = Math.abs(a.x - b.x);
      tightest = Math.min(tightest, dx);
      need(dx >= WRAP, `nodes ${a.key} and ${b.key} share a row but are only ${dx} apart ` +
                       `(labels wrap at ${WRAP})`);
    }
  }
  return tightest === Infinity ? "no same-row pairs" : `tightest row pair ${tightest} units`;
});

/* -- 3. shop catalog ------------------------------------------------------- */

check("shop: visible catalog matches the playground", () => {
  const cpp = parseShopCatalog();
  const visible = cpp.filter((e) => e.kind !== "OurItem" || e.localID);
  const mock = parseMockShop();
  need(visible.length === mock.length,
       `${visible.length} visible C++ entries vs ${mock.length} in the playground`);
  visible.forEach((e, i) => {
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
  const shop = [...read("src/Shop.cpp").matchAll(/Cat::k\w+,\s*\d+,\s*(0x[0-9A-Fa-f]+)\s*\}/g)]
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
