#pragma once

namespace Isekai::Plugin {

    // Our own ESP. Everything it holds is looked up through TESDataHandler, which
    // resolves the load order for us, so the local FormIDs below stay valid no matter
    // where the plugin sits in the list.
    inline constexpr const char* kFileName = "IsekaiHero.esp";

    // Log every form the ESP contributes, with its local FormID.
    //
    // Unlike quests, spells and containers do NOT keep their editor IDs at runtime, so
    // there is no way to find them by name — only by FormID. And the Creation Kit picks
    // those FormIDs itself; they cannot be predicted. So we read them back out of the
    // real file rather than guessing, exactly as we did for the quest table.
    void DumpForms();

    // True once the ESP is present and its forms resolved.
    [[nodiscard]] bool IsLoaded();
}
