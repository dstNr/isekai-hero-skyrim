---
name: release
description: Cut a new Isekai Hero release — read the milestone, regenerate, build, package, tag, publish on GitHub, and hand the Nexus text to the user. Use when the user asks to release, package or ship a version.
---

# Cutting a release

The version number is **never** yours to choose (see `CLAUDE.md`). If the user
has not named it, ask for it before touching anything.

## 1. Read the milestone first

```
gh issue list --milestone vX.Y.Z --state open
```

`gh` lives at `C:\Program Files\GitHub CLI\gh.exe` and may not be on PATH.

Say out loud what is on the milestone and what is still open. An open issue is
not automatically a blocker — but the user decides that, not you. Issues that
are code-complete but unconfirmed in-game belong on the *next* milestone, not
silently in this release's notes as done.

## 2. Set the version

Only after the user named it. Both of these:

- `CMakeLists.txt` — `VERSION X.Y.Z`
- `package.ps1` — the default `-Version` in the `param(...)` line

## 3. Regenerate what is generated

```
node tools/extract-strings.mjs
node tools/make-fomod.mjs
node tools/check.mjs
```

`check.mjs` is the consistency check, not a unit-test suite: it catches the same
fact drifting apart in two places, which is this repo's characteristic bug. It
must pass before anything is packaged.

Watch for line-ending noise here — a `git checkout` can rewrite the generated
files with CRLF and make the loc check fail; re-running the extractor fixes it.

## 4. Build

```
build.bat
```

It refuses to deploy while Skyrim is running rather than silently leaving a
stale DLL in place. `BUILD_OK` on the last line means it really deployed.

## 5. Write the changelogs

- `CHANGELOG.md` — a new `[X.Y.Z] — YYYY-MM-DD` section, the developer record:
  the why and the technical detail. Walk the commits since the last tag so
  nothing is missed; entries have gone missing here before.
- `docs/NEXUS_CHANGELOG.md` — the player's view: one line per change, one
  sentence, what changes for them. Never rewrite an entry already published.

## 6. Package

```
.\package.ps1 -Version X.Y.Z
```

Produces `dist\IsekaiHero-vX.Y.Z.7z`. Two lines of its output matter:

- `Name scrub: replaced N occurrence(s) in the DLL.`
- `Privacy gate: N staged file(s) checked, all clean.`

If the gate throws, **fix the leak, never the gate.**

## 7. Commit, tag, push

Commit message `release: X.Y.Z`. Tag `vX.Y.Z` (that is the existing convention —
`v0.4.0` through `v0.8.0`). Push the branch and the tag.

## 8. GitHub release

Write `docs/release-notes-vX.Y.Z.md`, then publish it with `gh release create`
and attach the 7z. Cross-check the notes against `CHANGELOG.md` — the two have
drifted apart before, with the release notes missing entries the changelog had.

## 9. Close the loop on the board

- Closed issues belong in `Done` — closing does not always move the card, so
  check.
- Create the next milestone and move anything unfinished or unconfirmed onto it.

## 9b. The two pages players actually read

Both drift every release, and neither is covered by `check.mjs`.

- **`README.md`** is the GitHub landing page. The version badge, the runtime
  badge, the Requirements table, the Installation paragraph and the Status
  section all name facts that a release changes. So does any feature table
  carrying numbers — the blessing table has been wrong before.
- **`docs/NEXUS_DESCRIPTION.md`** must carry a link to the GitHub repository:
  source, releases, issues. It is where bug reports come from and where players
  find the changelog in full. Check that its requirements and installation text
  still match what the FOMOD actually asks.

Read both against the new `CHANGELOG.md` section, not from memory.

## 10. Hand over the Nexus upload

The user uploads to Nexus themselves. Point them at `docs/NEXUS_CHANGELOG.md`
and `docs/NEXUS_DESCRIPTION.md`, and say plainly whether the build was actually
played end to end or only compiled — that is the one thing they cannot see from
the archive.
