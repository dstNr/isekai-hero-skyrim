Scriptname IsekaiHeroMCM extends MCM_ConfigBase

; The whole of the mod's Papyrus, and it is deliberately empty.
;
; MCM Helper requires a script of the mod's own on the config quest - its C++ side accepts
; any script whose type is MCM_ConfigBase, and a subclass is how the documented setup does
; it. Everything the menu shows comes from MCM/Config/IsekaiHero/config.json, and every
; value the player picks is read back by the plugin out of MCM/Settings/IsekaiHero.ini.
;
; So there is nothing for this script to do, and nothing should be added to it. A callback
; here would be a second place where settings are handled, and the ini is already the one
; place. If a setting ever needs to act the moment it changes rather than when the menu
; closes, that belongs in Config::Load's reload path in C++, not here.
;
; After recompiling this, run `node tools/pex-scrub.mjs mcm-patch/Scripts/IsekaiHeroMCM.pex`.
; The compiler stamps the Windows account name into the .pex header, and package.ps1's
; privacy gate will refuse to build until it is gone.
