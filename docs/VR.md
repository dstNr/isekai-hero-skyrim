# Skyrim VR — Status & Plan

**Status: EXPERIMENTAL / UNGETESTET.** Die Mod wird VR-kompatibel gebaut, konnte aber
mangels SkyrimVR-Installation von niemandem im echten Spiel verifiziert werden. Bis
das passiert ist, gilt VR ausdrücklich als „läuft vielleicht", nicht als „unterstützt".

## Warum das meiste schon steht

CommonLibSSE-NG baut **eine** DLL für SE, AE und VR; die Runtime wird beim Laden
erkannt. Der vcpkg-Port (v3.7.0) ist mit `ENABLE_SKYRIM_VR=1` gebaut, diese Defines
propagieren an unser Plugin (`HAS_SKYRIM_MULTI_TARGETING=1`). Die Plugin-Deklaration
nutzt `VersionIndependence::AddressLibrary` ohne Runtime-Einschränkung — das schließt
VR über die VR Address Library ein.

Die Logik-Ebene ist runtime-neutral:
- Reincarnation, Milestones, Passives, Storage, Skill-Tree-Daten, Serialisierung —
  reine Spielobjekt-Logik.
- Crafting-Hooks nutzen bereits `REL::VariantID(se, ae, vr)` mit echten VR-Offsets
  (`src/CraftHooks.cpp`).
- VTables, Event-Sources (`LevelIncrease`), `BSGraphics::Renderer` löst NG pro Runtime
  auf. Keine nackten `REL::ID`/2-arg-`RelocationID`, die in VR brechen würden.

`src/main.cpp` loggt beim Laden die erkannte Edition (SE/AE/VR) — die erste Zeile,
die man bei einem VR-Bringup prüft.

## Spieler-Requirements für VR (abweichend von SE/AE)

- **SKSEVR** statt SKSE64
- **VR Address Library for SKSEVR** statt „Address Library for SKSE Plugins"
- Dasselbe Mod-Archiv wie für SE/AE — die DLL ist multi-runtime, ein eigener VR-Build
  ist nicht nötig.

## Die offenen Risiken (was ein VR-Test zuerst prüfen muss)

1. **Lädt das Plugin überhaupt?** Log auf `Runtime edition: Skyrim VR` prüfen. Wenn
   die Zeile fehlt oder SKSEVR das Plugin ablehnt → VR Address Library / SKSEVR-Version.
2. **Overlay/UI (der große Punkt) — BESTÄTIGT: crasht in VR, jetzt deaktiviert.**
   Das ImGui-Overlay hookte `IDXGISwapChain::Present` der Desktop-**Mirror**-Swap-Chain
   (`renderWindows[0]`, `src/UI/Overlay.cpp`). In VR liefert derselbe Struct-Zugriff
   einen **ungültigen, aber nicht-null** `swapChain` (das `RendererData`-Layout ist nur
   für die Flat-Editionen per `static_assert` fixiert) — das Dereferenzieren seiner
   vtable war ein sofortiger `EXCEPTION_ACCESS_VIOLATION` beim Laden (von einem Tester
   mit 0.5.0 gemeldet, `Overlay.cpp:221`). **Seit dem Fix wird der Overlay-Hook in VR
   per `REL::Module::IsVR()` übersprungen** — kein Crash mehr, aber in VR gibt es
   dadurch **kein ImGui-UI** (die Segenswahl über den ImGui-Pfad erscheint nicht).
   → Echtes VR-UI ist **Phase 2** (In-HMD-Rendering via OpenVR-Overlay/Stereo-Targets
     oder — kleiner — ein Fallback auf spieleigene `MessageBox`-Menüs für die Panels,
     die die Brille selbst rendert).
3. **PrismaUI-Patch in VR.** Ebenfalls ein 2D-Overlay; ob PrismaUI VR unterstützt, ist
   upstream ungeklärt. Für den ersten VR-Test besser den ImGui-Pfad (Basis-Mod ohne
   Patch) verwenden.
4. **Crafting-Hooks.** Die VR-Offsets in `CraftHooks.cpp` stammen aus der Vorarbeit und
   sind in VR nie geprüft — an einer Werkbank in VR gegentesten (Rezept-Sichtbarkeit,
   Materialabzug).

## Phasen

- **Phase 1 (erledigt):** VR-ladefähiger Build, Runtime-Logging, Doku, Requirements.
  Der Overlay-Crash ist behoben (Hook wird in VR übersprungen) — die Mod lädt und die
  Logik läuft, aber in VR gibt es vorerst kein UI.
- **Phase 2 (offen):** UI in die Brille bringen. Zwei Wege: (a) In-HMD-Rendering via
  OpenVR-Overlay/Stereo-Targets (aufwändig), oder (b) für die Panels ein Fallback auf
  spieleigene `MessageBox`-Menüs (rendert die Brille nativ; der Skill-Tree bleibt der
  schwierige Fall). Sinnvoll erst mit VR-Testumgebung.
