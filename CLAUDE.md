# Isekai Hero — working agreements

An SKSE C++ plugin (CommonLibSSE-NG) with an ImGui overlay and an optional
PrismaUI view, shipped as one FOMOD. The layout, the dependencies and the build
commands are readable from the tree, `CMakeLists.txt` and `vcpkg.json` — this
file only holds what they don't say.

## Language

Talk to the user in **German**. Everything that persists outside the chat is
**English** — commits, docs, code comments, Nexus text, and the whole GitHub
side: issue titles and bodies, label names, project columns, PR text, release
notes. Don't rewrite existing history for this; convert German text when you
touch it anyway.

## Versioning

**Never bump the version on your own initiative.** `CMakeLists.txt` VERSION,
`package.ps1`'s default `-Version`, git tags and `CHANGELOG.md` stay where they
are until the user names the next number. Build, package and commit freely at
the current version.

## Changelogs

`CHANGELOG.md` is the developer record — the why and the technical detail.
`docs/NEXUS_CHANGELOG.md` is the player's view: one line per change, one
sentence, what changes for the player. Entries already published on Nexus are
the historical record — never rewrite them.

## Privacy gate (do not weaken)

`package.ps1` scrubs the author's real name out of the shipped DLL and
deliberately does not ship the `.pdb`. Its gate walks the whole staged tree
before packing. Nothing shipped may re-expose the name — if the gate trips, fix
the leak, never the gate.

## How the mod is actually tested

Via MO2 from the packaged `dist\*.7z`, and from modlists (e.g. `E:\Modlists\NYA\`).
`build.bat` deploys into the base-game `Data\` — that does **not** reach the
test setup; the archive has to be rebuilt and updated in MO2.

The modlist writes its log to
`Documents\My Games\Skyrim.INI\SKSE\IsekaiHeroSKSE.log`, not to the default
`Skyrim Special Edition\SKSE\` folder.

## Roadmap

GitHub Project 1 plus repo milestones plan every release; read the milestone
before packaging one. `gh issue create` does **not** put an issue on the board —
every create must be followed by `gh project item-add` and an `item-edit`.
`gh` lives at `C:\Program Files\GitHub CLI\gh.exe` and may not be on PATH.

## Gotchas

- **Crafting token-lending lends official-master items only.** Moving a scripted
  mod item in or out fires its `OnContainerChanged` and floods the VM.
- **VR is untested** — there is no VR install on this machine. The ImGui overlay
  is off in VR; the whole UI goes through PrismaUI there.
- **Generated files are generated.** `fomod/ModuleConfig.xml` and
  `fomod-presets/*/IsekaiHero.ini` come from `tools/make-fomod.mjs`;
  `lang/template.txt` from `tools/extract-strings.mjs`. Edit the generator.
