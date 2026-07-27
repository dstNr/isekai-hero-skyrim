Was als **0.6.0** auf Nexus hochgeladen wurde.

Großes Feature-Release auf 0.5.x. Save-safe: bestehende Spielstände behalten alles und
leiten die neuen Felder aus ihrem Tier ab (Co-Save v10).

Archive: `IsekaiHero-v0.6.0.7z` (Hauptmod) und `IsekaiHero-PrismaUI-Patch-v0.6.0.7z` (optionaler UI-Patch).

### Added

- **Custom blessing** — ein fünfter Weg: Starting Gift, Reward Pace und Skill-Tree-Tiefe einzeln wählbar (NORMAL/HERO/ASCENDED). „Ganzer Baum offen, aber Normal-Start und -Rewards" ist damit ein gültiger Build; balanciert sich selbst über die Punkt-Ökonomie.
- **Reboot-Button** im System-Menü — Segen auf bestehendem Charakter neu wählen, ohne Neuinstallation; Milestones, Skill-Tree und Punkte bleiben.
- **Überarbeitetes Status-Menü** (PrismaUI-Patch) — echtes Dashboard: Tier-Header mit Skill-Tree-/Storage-Icons, Milestone-/Punkte-Tiles, Attunements und eine scrollende Titelliste, die bei allen Milestones auf dem Screen bleibt.
- **Optionale SkyrimNet-Integration (experimentell, ungetestet)** — mit SkyrimNet reagieren AI-NPCs auf die Wiedergeburt und die vom System anerkannten Taten. Noch nicht im laufenden Spiel mit SkyrimNet verifiziert; ohne SkyrimNet komplett wirkungslos, für alle anderen also unbedenklich. Per INI abschaltbar.
- **Skyrim VR lädt jetzt** (experimentell) — kein Crash mehr auf SkyrimVR, Forms lösen dort auf. UI in VR braucht den PrismaUI-VR-Build. Noch im Community-Test.

### Fixed

- **Charakter-Level fällt nach dem Laden nicht mehr auf 1.** (Skills, Attribute, Perks und Gold waren nie betroffen — nur die Level-Zahl.)

### Intern

- Kein Entwickler-Klarname in der ausgelieferten DLL; Debug-`.pdb` wird nicht mitgeliefert.
