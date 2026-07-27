Was als **0.5.1** auf Nexus hochgeladen wurde.

Kleiner Nachzügler zu 0.5.0 — beide Fixes betreffen den optionalen PrismaUI-Patch.
Kein Save-Format-Wechsel; ein 0.5.0-Spielstand laeuft unveraendert weiter.

Archive: `IsekaiHero-v0.5.1.7z` (Hauptmod) und `IsekaiHero-PrismaUI-Patch-v0.5.1.7z` (optionaler UI-Patch).

### Fixed

- **Der Level-Up-Sound fehlt unter PrismaUI nicht mehr.** Beim Schliessen eines System-Panels verlaesst das Spiel kurz die Menue-Pause, und die Engine verwarf den im selben Frame gestarteten Sting — er erreichte nie die Lautsprecher. Sounds nach einem Panel-Schluss spielen jetzt knapp nach dem Unpause. (Das eingebaute ImGui-UI pausiert nie und war nie betroffen.)
- **Hoehere, stabilere FPS im System-Menue (PrismaUI).** Zwei Akzente animierten dauerhaft — ein Glanz-Sweep unter den Ueberschriften und ein pulsierender Ring auf jedem leistbaren Node — was den Web-Renderer jeden Frame die ganze Ansicht neu zeichnen liess. Beide sind jetzt statisch; das Menue sieht in Ruhe gleich aus, laesst den Renderer aber idlen, sodass die Framerate haelt.

### Intern (kein Spieler-Effekt)

- Der Klarname des Entwicklers ist nicht mehr in der ausgelieferten DLL eingebettet, und die Debug-`.pdb` wird nicht mehr mitgepackt.
