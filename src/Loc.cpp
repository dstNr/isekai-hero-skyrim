#include "Loc.h"

#include "Config.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <unordered_map>

namespace Isekai::Loc {

    namespace {
        std::unordered_map<std::string, std::string> g_strings;
        std::string                                  g_code = "en";

        // No mutex, and none needed: the table is written once at kDataLoaded and only
        // read afterwards, from the main thread and the render thread both. If reloading
        // on the fly is ever wanted, that guarantee is what has to be replaced first.

        [[nodiscard]] std::string Trim(std::string a_s) {
            const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
            a_s.erase(a_s.begin(), std::find_if(a_s.begin(), a_s.end(), notSpace));
            a_s.erase(std::find_if(a_s.rbegin(), a_s.rend(), notSpace).base(), a_s.end());
            return a_s;
        }

        // Turn the two escapes a translator needs into real characters. A panel body is
        // multi-line and a settings-style file is line-based, so "\n" has to survive the
        // trip somehow; "\\" is here so a literal backslash remains writable.
        [[nodiscard]] std::string Unescape(const std::string& a_s) {
            std::string out;
            out.reserve(a_s.size());
            for (std::size_t i = 0; i < a_s.size(); ++i) {
                if (a_s[i] == '\\' && i + 1 < a_s.size()) {
                    switch (a_s[i + 1]) {
                    case 'n': out.push_back('\n'); ++i; continue;
                    case '\\': out.push_back('\\'); ++i; continue;
                    default: break;
                    }
                }
                out.push_back(a_s[i]);
            }
            return out;
        }
    }

    void Load() {
        g_strings.clear();
        g_code = Config::Language();

        if (g_code.empty() || g_code == "en") {
            logger::info("Loc: running English (Language = {})", g_code.empty() ? "en" : g_code);
            g_code = "en";
            return;
        }

        // Rejected rather than sanitised: a language code is two or three letters, and
        // anything else in a path we open is someone trying something.
        const bool sane = g_code.size() <= 8 &&
                          std::all_of(g_code.begin(), g_code.end(), [](unsigned char c) {
                              return std::isalnum(c) || c == '-' || c == '_';
                          });
        if (!sane) {
            logger::error("Loc: refusing language code \"{}\" — letters, digits, - and _ only",
                          g_code);
            g_code = "en";
            return;
        }

        const std::string path =
            "Data\\SKSE\\Plugins\\IsekaiHero\\lang\\" + g_code + ".txt";
        std::ifstream in(path);
        if (!in) {
            logger::warn("Loc: no language file at {} — running English", path);
            g_code = "en";
            return;
        }

        std::string line;
        std::size_t lineNo = 0;
        while (std::getline(in, line)) {
            ++lineNo;
            // A UTF-8 BOM on line 1 would otherwise become part of the first key, and the
            // first string would be the one that silently stayed English.
            if (lineNo == 1 && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF) {
                line.erase(0, 3);
            }
            const auto hash = line.find_first_not_of(" \t");
            if (hash == std::string::npos || line[hash] == ';' || line[hash] == '#') {
                continue;
            }
            const auto eq = line.find('=');
            if (eq == std::string::npos) {
                continue;
            }
            std::string key = Trim(line.substr(0, eq));
            std::string value = Unescape(Trim(line.substr(eq + 1)));
            if (key.empty() || value.empty()) {
                continue;  // an empty translation is not a translation
            }
            g_strings.emplace(std::move(key), std::move(value));
        }

        logger::info("Loc: loaded {} strings from {}", g_strings.size(), path);
    }

    const char* Get(std::string_view a_key, const char* a_english) {
        // Returns a pointer, not a string, and that is the whole thread-safety story: a
        // hit points into the table (written once, never rewritten), a miss points at the
        // string literal in the binary. Nothing is allocated and nothing is mutated, so
        // this is safe to call from the render thread hundreds of times a frame. An
        // earlier draft returned a reference and cached constructed fallbacks in a static
        // map — which the render thread would have been writing to.
        if (!g_strings.empty()) {
            const auto it = g_strings.find(std::string(a_key));
            if (it != g_strings.end()) {
                return it->second.c_str();
            }
        }
        return a_english ? a_english : "";
    }

    std::size_t Count() {
        return g_strings.size();
    }

    const std::string& Code() {
        return g_code;
    }
}
