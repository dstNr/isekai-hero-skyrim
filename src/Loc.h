#pragma once

#include <string>
#include <string_view>

// Translatable text.
//
// Every translatable string is written as L("some.key", "The English text"), and the
// English text stays IN THE SOURCE as the fallback. That one decision is what makes this
// safe to adopt one string at a time:
//
//   - There is no English language file to keep in sync, so English can never be missing.
//   - A translation that covers 12 of 400 keys works — the other 388 come out English
//     rather than blank or as a raw key, which is what a half-finished translation looks
//     like in most mods.
//   - A key that is renamed in code degrades to English instead of breaking.
//
// Translations live in Data\SKSE\Plugins\IsekaiHero\lang\<code>.txt, selected by
// `Language` in the ini. Format is one `key = text` per line, `;` comments, `\n` for a
// line break — deliberately the same shape as the settings ini, so a translator who has
// seen one has seen both. UTF-8.
//
// `node tools/extract-strings.mjs` writes lang/template.txt from the source, which is
// what a translator starts from.

namespace Isekai::Loc {

    // Read the language file named by Config::Language(). Call at kDataLoaded, after
    // Config::Load. Absent file or `Language = en` leaves everything English.
    void Load();

    // The translation for a_key, or a_english if there is none. a_english is what ships.
    //
    // Returns a pointer with static lifetime either way — into the loaded table, or at
    // the string literal itself. Nothing is allocated per call, which is what makes this
    // usable from the render thread.
    [[nodiscard]] const char* Get(std::string_view a_key, const char* a_english);

    // How many strings the loaded translation actually provides, for the log and the
    // self-test. 0 means "running English".
    [[nodiscard]] std::size_t Count();

    // The language code in force ("en" when untranslated).
    [[nodiscard]] const std::string& Code();
}

// Shorthand, because this wraps hundreds of literals and the noise matters.
#define L(key, english) ::Isekai::Loc::Get(key, english)
