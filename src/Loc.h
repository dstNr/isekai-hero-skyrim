#pragma once

#include <format>
#include <set>
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

    // The same, for a string with values in it: "it wakes at level {}". The translator
    // may move the placeholders around, which is the entire reason this goes through
    // std::format rather than string concatenation — in some languages the number belongs
    // in a different place in the sentence, and a concatenated string cannot express that.
    //
    // A translation file is data written by someone else, so a broken format string is a
    // question of when, not if: "{" alone, "{}" too many times, "{1}" out of range. Every
    // one of those throws, and an exception from a UI string would take the game down over
    // a typo in a community translation. So it falls back to the English text on any
    // failure and says so in the log, once.
    template <class... Args>
    [[nodiscard]] std::string Fmt(std::string_view a_key, const char* a_english,
                                  Args&&... a_args) {
        const char* pattern = Get(a_key, a_english);
        try {
            return std::vformat(pattern, std::make_format_args(a_args...));
        } catch (const std::format_error& e) {
            static std::set<std::string> complained;
            if (complained.emplace(a_key).second) {
                logger::warn("Loc: \"{}\" has a broken format string in this translation "
                             "({}) — falling back to English for it",
                             a_key, e.what());
            }
            try {
                return std::vformat(a_english, std::make_format_args(a_args...));
            } catch (...) {
                return a_english ? a_english : "";
            }
        }
    }

    // How many strings the loaded translation actually provides, for the log and the
    // self-test. 0 means "running English".
    [[nodiscard]] std::size_t Count();

    // The language code in force ("en" when untranslated).
    [[nodiscard]] const std::string& Code();
}

// Shorthand, because this wraps hundreds of literals and the noise matters.
#define L(key, english) ::Isekai::Loc::Get(key, english)

// The same for a string with {} placeholders in it.
#define LF(key, english, ...) ::Isekai::Loc::Fmt(key, english, __VA_ARGS__)
