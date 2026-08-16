// Generate the FOMOD's ini presets from the shipped IsekaiHero.ini.
//
// The installer can only ever CHOOSE FILES, not edit them, so every preset is a whole
// ini. Writing four of those by hand would guarantee that a setting added to one is
// forgotten in the other three — so they are generated: same file, same comments, a
// handful of values replaced. `node tools/make-presets.mjs` after touching the ini.
//
// TWO independent questions, asked separately, because they are about different things:
// when the System wakes up, and whether it writes on the screen during a fight. Rolling
// them into one list of named presets meant "Quiet HUD" and "Alternate start" were
// mutually exclusive choices in the same radio group, which they are not.
//
// The installer still only chooses FILES, so each combination needs its own whole ini —
// the installer picks between them with condition flags (see tools/make-fomod.mjs). That
// is why the axes stay at two questions of two answers: four files, one per combination.
// A third axis of three choices would be twelve copies of the same document, which is the
// trade this file has always refused. If a setting is worth an installer question, it has
// to be worth doubling the preset count.

import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");

// Axis 1 — when the System introduces itself.
export const STARTS = [
  {
    key: "standard",
    name: "Standard",
    blurb: "The System boots itself the first time you load into the world.",
    values: {},   // the shipped file, unchanged
  },
  {
    key: "alternate-start",
    name: "Alternate start",
    blurb:
      "For a start that opens somewhere the System's boot sequence does not belong — a " +
      "modern-world prologue, a dream, a cell. Nothing happens until you press the " +
      "System hotkey yourself.",
    values: { AutoStart: "0" },
  },
];

// Axis 2 — the threat readings over enemies. Its own step because it is a HUD taste
// question, unrelated to how the game starts.
export const HUDS = [
  {
    key: "",   // no suffix: this is the plain form of whichever start was chosen
    name: "Show them",
    blurb:
      "Name, level, health and a verdict over anything that is fighting you or that you " +
      "aim at. F10 hides them for a screenshot.",
    image: "threat-labels.png",
    values: {},
  },
  {
    key: "quiet",
    name: "Quiet HUD",
    blurb:
      "Nothing floating over enemies. Everything else is unchanged, and F10 still " +
      "switches the readings back on mid-game if you change your mind.",
    values: { ThreatLabels: "0" },
  },
];

// One preset per combination. `dir` is also the flag pair that selects it.
export const PRESETS = STARTS.flatMap((start) =>
  HUDS.map((hud) => ({
    dir: hud.key ? `${start.key}-${hud.key}` : start.key,
    name: hud.key ? `${start.name} + ${hud.name}` : start.name,
    start: start.key,
    hud: hud.key,
    values: { ...start.values, ...hud.values },
  })));

// Replace the value on a setting's line, leaving every comment and blank line alone.
// Fails loudly on a key that is not in the file: a silently ignored override would ship
// a preset that quietly does nothing, which is the whole failure this file exists to
// prevent.
function apply(ini, values) {
  let out = ini;
  for (const [key, value] of Object.entries(values)) {
    const re = new RegExp(`^(${key}\\s*=\\s*).*$`, "mi");
    if (!re.test(out)) {
      throw new Error(`preset override "${key}" matches no setting in IsekaiHero.ini`);
    }
    out = out.replace(re, `$1${value}`);
  }
  return out;
}

function header(preset) {
  const changed = Object.entries(preset.values)
    .map(([k, v]) => `;   ${k} = ${v}`)
    .join("\n");
  return (
    `; ---------------------------------------------------------------------------\n` +
    `; Installed by the FOMOD as the "${preset.name}" preset.\n` +
    `;\n` +
    (changed
      ? `; It differs from the default file in these settings only:\n${changed}\n`
      : `; It is the default file, unchanged.\n`) +
    `;\n` +
    `; Everything below is editable at any time; nothing here is locked in by the\n` +
    `; installer. Delete the file and the mod uses its built-in defaults.\n` +
    `; ---------------------------------------------------------------------------\n\n`
  );
}

export function build() {
  const ini = readFileSync(join(root, "IsekaiHero.ini"), "utf8").replace(/\r\n/g, "\n");
  const written = [];
  for (const preset of PRESETS) {
    const dir = join(root, "fomod-presets", preset.dir);
    mkdirSync(dir, { recursive: true });
    writeFileSync(join(dir, "IsekaiHero.ini"), header(preset) + apply(ini, preset.values));
    written.push(preset.dir);
  }
  return written;
}

if (process.argv[1] && process.argv[1].endsWith("make-presets.mjs")) {
  const written = build();
  console.log(`wrote ${written.length} presets: ${written.join(", ")}`);
}
