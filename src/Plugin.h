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

    // Build the runtime FormID of one of OUR forms from its local ID, the same way
    // DumpForms does: the file's partial index shifted over the local bits (12 for a
    // light/ESL plugin, 24 for a regular one).
    //
    // Why not just TESDataHandler::LookupForm(localID, kFileName)? That path fails in
    // Skyrim VR for this ESL-flagged plugin — it returned null for every one of our
    // forms (Passives 0/8, Sounds 0/4, storage container missing), even though the
    // forms are loaded and DumpForms finds them fine with the math below. So resolving
    // our own forms goes through here instead. On SE/AE it yields the identical FormID
    // LookupForm would, so nothing changes there.
    [[nodiscard]] RE::FormID ResolveLocalID(RE::FormID a_localID);

    // Typed lookup of one of our forms by local ID, via ResolveLocalID. Returns nullptr
    // if the plugin is absent or the form is the wrong type.
    template <class T>
    [[nodiscard]] T* LookupOurForm(RE::FormID a_localID) {
        const RE::FormID full = ResolveLocalID(a_localID);
        return full != 0 ? RE::TESForm::LookupByID<T>(full) : nullptr;
    }
}
