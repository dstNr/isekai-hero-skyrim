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
2. **Overlay/UI (der große Punkt).** Das ImGui-Overlay hookt `IDXGISwapChain::Present`
   der Desktop-**Mirror**-Swap-Chain (`renderWindows[0]`, `src/UI/Overlay.cpp`). Das
   heißt bestenfalls: UI erscheint **auf dem Monitor, nicht in der Brille**. Zwei
   Dinge zu klären:
   - Crasht der Zugriff `renderer->data.renderWindows[0].swapChain` in VR? Das
     `RendererData`-Layout ist nur für die Flat-Editionen per `static_assert` fixiert.
   - Ist die Mirror-UI überhaupt bedienbar (Maus/Tastatur), oder braucht es die
     In-HMD-Lösung sofort?
   → Das ist der Inhalt von **Phase 2** (In-HMD-Rendering via OpenVR-Overlay oder
     Kompositing in die Stereo-Targets). Ohne die ist die Segenswahl in VR ggf. nur
     über den Monitor bedienbar.
3. **PrismaUI-Patch in VR.** Ebenfalls ein 2D-Overlay; ob PrismaUI VR unterstützt, ist
   upstream ungeklärt. Für den ersten VR-Test besser den ImGui-Pfad (Basis-Mod ohne
   Patch) verwenden.
4. **Crafting-Hooks.** Die VR-Offsets in `CraftHooks.cpp` stammen aus der Vorarbeit und
   sind in VR nie geprüft — an einer Werkbank in VR gegentesten (Rezept-Sichtbarkeit,
   Materialabzug).

## Phasen

- **Phase 1 (erledigt, ungetestet):** VR-ladefähiger Build, Runtime-Logging, Doku,
  Requirements. UI vorerst nur auf dem Mirror.
- **Phase 2 (offen):** UI in die Brille bringen. Erst sinnvoll, wenn eine
  VR-Testumgebung existiert — sonst blind.
