// Collect every L("key", "English") in the sources into lang/template.txt, which is what
// a translator starts from.
//
// Deliberately a regex over the source rather than anything cleverer. The call shape is
// fixed by the macro in src/Loc.h and a real parse would buy nothing — but a regex CAN
// silently match less than it should, so this fails loudly on a suspiciously small
// harvest and check.mjs asserts the template is current.
//
//     node tools/extract-strings.mjs

import { readFileSync, writeFileSync, mkdirSync, readdirSync, statSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..");

function sources(dir, out = []) {
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) sources(p, out);
    else if (/\.(cpp|h)$/.test(name)) out.push(p);
  }
  return out;
}

// L("key", "text") and LF("key", "text with {}", value) — the text may contain escapes
// and adjacent "a" "b" concatenation, which is how a long panel body is written in C++.
// The trailing [,)] is what admits LF: its English text is followed by the arguments
// rather than by the closing bracket.
//
// The first version of this matched `\bL\(` only and silently skipped every LF call. The
// template came out short, and the check that compares template to source compared a
// stale template against the same stale collector and passed. Hence COUNT below.
const CALL = /\bLF?\(\s*"([^"]+)"\s*,\s*((?:"(?:[^"\\]|\\.)*"\s*)+)[,)]/g;

// Every call site, however malformed, just to count them. If this and CALL disagree the
// regex above has stopped matching something it should — which is a silent failure, so
// it is made loud.
const COUNT = /\bLF?\(\s*"/g;

// Every call, literal key or not. A computed key — LF(cond ? "a.x" : "a.y", ...) — is the
// one way to write a translatable string that BOTH regexes above miss: no key in the
// template, no count mismatch to notice, nothing in the log. So it is rejected outright.
const ANY_CALL = /\bLF?\(\s*(.)/g;

// The 79 passive titles ("Survivor", "Dragon Slayer") are not written as L() calls: they
// live in the kMilestones table in Progression.cpp and are looked up at runtime as
// `passive.<milestone key>`. So they are harvested from the table instead, by the same
// strict-vs-loose pair as above — LOOSE_ROW counts anything row-shaped, ROW parses it,
// and a disagreement means the table's formatting has moved out from under this regex.
//
// Quest names are deliberately NOT harvested: they come from the player's own game at
// runtime, already localised. See QuestName() in Progression.cpp.
const ROW = /\{\s*(\d+),\s*"[^"]+",\s*"[^"]+",\s*\{\s*"([^"]+)"/g;
const LOOSE_ROW = /^\s*\{\s*\d+,\s*"/gm;

function milestones() {
  const file = join(ROOT, "src", "Progression.cpp");
  const code = readFileSync(file, "utf8");
  const rows = [...code.matchAll(ROW)];
  const loose = (code.match(LOOSE_ROW) || []).length;
  if (rows.length !== loose) {
    throw new Error(
      `milestone table: ${loose} rows look like rows but ${rows.length} parsed — ` +
        "the passive-title regex in tools/extract-strings.mjs has gone stale",
    );
  }
  return rows.map(([, key, name]) => ["passive." + key, name]);
}

// The skill-tree nodes, same idea: title and description live in the kNodes table in
// SkillTree.cpp and are looked up as `node.<key>` / `node.<key>.desc`.
const NODE = /\{\s*(\d+),\s*Zone::\w+,\s*"((?:[^"\\]|\\.)*)",\s*((?:"(?:[^"\\]|\\.)*"\s*)+),/g;
const LOOSE_NODE = /\{\s*\d+,\s*Zone::/g;

function nodes() {
  const file = join(ROOT, "src", "SkillTree.cpp");
  const code = readFileSync(file, "utf8");
  const rows = [...code.matchAll(NODE)];
  const loose = (code.match(LOOSE_NODE) || []).length;
  if (rows.length !== loose) {
    throw new Error(
      `skill tree: ${loose} rows look like rows but ${rows.length} parsed — ` +
        "the node regex in tools/extract-strings.mjs has gone stale",
    );
  }
  const out = [];
  for (const [, key, name, descLiterals] of rows) {
    const desc = [...descLiterals.matchAll(/"((?:[^"\\]|\\.)*)"/g)].map((x) => x[1]).join("");
    out.push(["node." + key, name], ["node." + key + ".desc", desc]);
  }
  return out;
}

// The shop catalog and its shelves. The entries carry their translation key as a field —
// see the comment on Entry in Shop.cpp — so nothing is derived here: the key is read, not
// built. A shelf needs no field, its English name is already a valid key segment.
const SHOP = /\{\s*"([\w.]+)",\s*"((?:[^"\\]|\\.)*)",\s*"((?:[^"\\]|\\.)*)",\s*\d+,/g;
const LOOSE_SHOP = /\{\s*"shop\.\w+",/g;

function shop() {
  const code = readFileSync(join(ROOT, "src", "Shop.cpp"), "utf8");
  const from = code.indexOf("constexpr Entry kCatalog[] = {");
  if (from < 0) throw new Error("shop: kCatalog not found in Shop.cpp");
  const body = code.slice(from, code.indexOf("\n        };", from));
  const rows = [...body.matchAll(SHOP)];
  const loose = (body.match(LOOSE_SHOP) || []).length;
  if (rows.length !== loose) {
    throw new Error(
      `shop: ${loose} rows look like rows but ${rows.length} parsed — ` +
        "the catalog regex in tools/extract-strings.mjs has gone stale",
    );
  }
  const out = [];
  for (const [, key, name, qty] of rows) {
    out.push([key, name], [key + ".qty", qty]);
  }
  const shelves = code.match(/kShelfNames\[\]\s*=\s*\{([\s\S]*?)\};/);
  if (!shelves) throw new Error("shop: kShelfNames not found in Shop.cpp");
  for (const [, s] of shelves[1].matchAll(/"([^"]+)"/g)) {
    out.push(["shelf." + s, s]);
  }
  return out;
}

// The hunt quarries, keyed by the quarry key. Same shape as the two tables above.
function quarries() {
  const code = readFileSync(join(ROOT, "src", "Quests.cpp"), "utf8");
  const from = code.indexOf("constexpr Quarry kQuarries[] = {");
  if (from < 0) throw new Error("quests: kQuarries not found in Quests.cpp");
  const body = code.slice(from, code.indexOf("\n        };", from));
  const rows = [...body.matchAll(/\{\s*(\d+),\s*"([^"]+)",\s*"ActorType/g)];
  const loose = (body.match(/^\s*\{\s*\d+,/gm) || []).length;
  if (rows.length !== loose) {
    throw new Error(
      `quests: ${loose} rows look like rows but ${rows.length} parsed — ` +
        "the quarry regex in tools/extract-strings.mjs has gone stale",
    );
  }
  return rows.map(([, key, name]) => ["quarry." + key, name]);
}

export function collect() {
  const found = new Map();
  const dupes = [];
  let callSites = 0;
  for (const file of sources(join(ROOT, "src"))) {
    // Comments first: Loc.h documents the macro by showing a call, and without this the
    // example ends up in the template as a string every translator has to wonder about.
    // Comments, then the #define lines that declare L and LF themselves — otherwise the
    // macro's own parameter list reads as a call site with a computed key.
    const code = readFileSync(file, "utf8")
      .replace(/\/\*[\s\S]*?\*\//g, "")
      .replace(/^[ \t]*\/\/.*$/gm, "")
      .replace(/^[ \t]*#define(?:.*\\\r?\n)*.*$/gm, "");
    for (const m of code.matchAll(CALL)) {
      const key = m[1];
      // Join the adjacent literals, keeping their escapes as written: the language file
      // uses the same \n and \\ escapes, so this round-trips.
      const text = [...m[2].matchAll(/"((?:[^"\\]|\\.)*)"/g)].map((x) => x[1]).join("");
      if (found.has(key) && found.get(key).text !== text) {
        dupes.push(key);
      }
      found.set(key, { text, file: relative(ROOT, file).replace(/\\/g, "/") });
    }
    for (const m of code.matchAll(ANY_CALL)) {
      if (m[1] !== '"') {
        const line = code.slice(0, m.index).split("\n").length;
        throw new Error(
          `${relative(ROOT, file).replace(/\\/g, "/")}:${line}: L()/LF() needs a literal ` +
            `key, got ${JSON.stringify(m[1])} — a computed key never reaches the template`,
        );
      }
    }
    callSites += (code.match(COUNT) || []).length;
  }
  for (const [key, text] of milestones()) {
    found.set(key, { text, file: "src/Progression.cpp (passive titles)" });
  }
  for (const [key, text] of nodes()) {
    found.set(key, { text, file: "src/SkillTree.cpp (skill tree)" });
  }
  for (const [key, text] of shop()) {
    found.set(key, { text, file: "src/Shop.cpp (catalog)" });
  }
  for (const [key, text] of quarries()) {
    found.set(key, { text, file: "src/Quests.cpp (quarries)" });
  }
  return { found, dupes, callSites };
}

export function template(found) {
  const byFile = new Map();
  for (const [key, v] of found) {
    if (!byFile.has(v.file)) byFile.set(v.file, []);
    byFile.get(v.file).push([key, v.text]);
  }
  let out =
    "; Isekai Hero - translation template\n" +
    ";\n" +
    "; Copy this to <code>.txt (de.txt, zh.txt, pl.txt ...), put it in\n" +
    "; Data\\SKSE\\Plugins\\IsekaiHero\\lang\\, and set Language in IsekaiHero.ini.\n" +
    ";\n" +
    "; Translate the RIGHT side of each = only. Rules:\n" +
    ";   - \\n is a line break. Keep them: panel layouts are measured, not wrapped.\n" +
    ";   - {} is a value the mod fills in. Keep every one, in any order you need.\n" +
    ";   - Lines starting with ; are comments. UTF-8, no BOM needed.\n" +
    ";   - A key you delete or leave empty simply stays English. A partial\n" +
    ";     translation is a working translation.\n" +
    ";\n" +
    "; GENERATED by tools/extract-strings.mjs - re-run it after the mod updates to see\n" +
    "; which keys are new.\n";
  for (const [file, entries] of [...byFile].sort()) {
    out += `\n; --- ${file} ---\n`;
    for (const [key, text] of entries.sort()) {
      out += `${key} = ${text}\n`;
    }
  }
  return out;
}

if (process.argv[1] && process.argv[1].endsWith("extract-strings.mjs")) {
  const { found, dupes } = collect();
  if (dupes.length) {
    console.error(`same key, different English text: ${dupes.join(", ")}`);
    process.exit(1);
  }
  mkdirSync(join(ROOT, "lang"), { recursive: true });
  writeFileSync(join(ROOT, "lang", "template.txt"), template(found));
  console.log(`wrote lang/template.txt with ${found.size} strings`);
}
