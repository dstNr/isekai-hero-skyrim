// Generate the FOMOD's ini presets from the shipped IsekaiHero.ini.
//
// The installer can only ever CHOOSE FILES, not edit them, so every preset is a whole
// ini. Writing four of those by hand would guarantee that a setting added to one is
// forgotten in the other three — so they are generated: same file, same comments, a
// handful of values replaced. `node tools/make-presets.mjs` after touching the ini.
//
// Deliberately one flat list of presets rather than a preset per setting. Independent
// groups multiply: three groups of three choices is 27 files, and every one of them a
// copy of the same document. Four named presets each tell a story instead.

import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..");

export const PRESETS = [
  {
    dir: "standard",
    name: "Standard",
    blurb: "The mod as intended. Pick this unless one of the others describes you.",
    values: {},   // the shipped file, unchanged
  },
  {
    dir: "alternate-start",
    name: "Alternate start",
    blurb:
      "For a start that opens somewhere the System's boot sequence does not belong — a " +
      "modern-world prologue, a dream, a cell. Nothing happens until you press the " +
      "System hotkey yourself.",
    values: { AutoStart: "0" },
  },
  {
    dir: "quiet-hud",
    name: "Quiet HUD",
    blurb:
      "No threat readings floating over enemies. Everything else is unchanged, and F10 " +
      "still switches them back on mid-game if you change your mind.",
    values: { ThreatLabels: "0" },
  },
  {
    dir: "modlist-testing",
    name: "Modlist testing",
    blurb:
      "For building and testing a load order, not for playing. The System waits for your " +
      "hotkey, panels appear instantly with no typing, objectives arrive immediately and " +
      "can be re-rolled, and F11 runs the self-test.",
    values: {
      AutoStart: "0",
      TextSpeed: "0",
      FirstTaskHours: "0",
      TaskIntervalHours: "0",
      QuestRerollButton: "1",
      SelfTestKey: "F11",
    },
  },
];

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
