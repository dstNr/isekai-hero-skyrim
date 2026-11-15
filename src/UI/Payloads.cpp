#include "UI/Payloads.h"

#include "Shop.h"  // Shelves, Catalog
#include "SkillTree.h"
#include "System.h"  // PowerName, kMaxPerkPoints, AwakeningLevelFor, GetState

#include <cstdint>
#include <string>

namespace Isekai::UI::Payloads {

    // --- JSON helpers ---

    [[nodiscard]] std::string Escape(const char* a_s) {
        std::string out;
        for (const char* p = a_s; p && *p; ++p) {
            switch (*p) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(*p) >= 0x20) {
                    out += *p;
                }
                break;
            }
        }
        return out;
    }

    namespace {
        [[nodiscard]] const char* Bool(bool a_b) { return a_b ? "true" : "false"; }

        // Just the file name of an icon path ("...\\icons\\x.png" -> "x.png"); the web
        // view loads icons relative to itself as "icons/<name>".
        [[nodiscard]] std::string IconFile(const std::string& a_path) {
            const auto pos = a_path.find_last_of("\\/");
            return pos == std::string::npos ? a_path : a_path.substr(pos + 1);
        }
    }

    // Whole tree + header. Reads game state (perk pool) — MAIN THREAD ONLY.
    [[nodiscard]] std::string Tree() {
        std::size_t count = 0;
        const auto* nodes = SkillTree::Nodes(count);

        std::string j = "{";
        j += "\"points\":" + std::to_string(SkillTree::Points()) + ",";
        j += "\"perks\":" + std::to_string(SkillTree::PerkPool()) + ",";
        j += "\"perkMax\":" + std::to_string(Isekai::kMaxPerkPoints) + ",";
        j += "\"respec\":" + std::to_string(SkillTree::RespecRefund()) + ",";
        j += "\"nodes\":[";
        for (std::size_t i = 0; i < count; ++i) {
            const auto&        n = nodes[i];
            const std::int32_t rank = SkillTree::Rank(n.key);
            const bool         maxed = n.repeatable && n.maxRank > 0 && rank >= n.maxRank;
            const bool         owned = n.repeatable ? maxed : SkillTree::IsUnlocked(n.key);
            const std::int32_t tier = SkillTree::Tier(n.key);

            if (i != 0) {
                j += ",";
            }
            j += "{";
            j += "\"key\":" + std::to_string(n.key) + ",";
            j += "\"name\":\"" + Escape(SkillTree::Name(n)) + "\",";
            j += "\"desc\":\"" + Escape(SkillTree::Desc(n)) + "\",";
            j += "\"icon\":\"" + Escape(n.icon) + "\",";
            j += "\"x\":" + std::to_string(static_cast<int>(n.x)) + ",";
            j += "\"y\":" + std::to_string(static_cast<int>(n.y)) + ",";
            // The NEXT purchase's price, not the flat table value — mastery nodes
            // (capped repeatables) rise per tier, so what buying costs right now
            // depends on the rank already owned.
            j += "\"cost\":" + std::to_string(SkillTree::NextCost(n.key)) + ",";
            j += "\"owned\":" + std::string(Bool(owned)) + ",";
            j += "\"tierMet\":" + std::string(Bool(SkillTree::TierMet(n.key))) + ",";
            j += "\"prereqMet\":" + std::string(Bool(SkillTree::PrereqsMet(n.key))) + ",";
            j += "\"repeatable\":" + std::string(Bool(n.repeatable)) + ",";
            j += "\"rank\":" + std::to_string(rank) + ",";
            j += "\"maxRank\":" + std::to_string(n.maxRank) + ",";
            j += "\"masteryTier\":" + std::to_string(tier) + ",";
            j += "\"masteryTierName\":\"" + Escape(SkillTree::TierName(tier)) + "\",";
            // Display grouping and size — the view frames the graph as labelled
            // zones and draws the landmarks (hub, capstone, gifts) larger.
            j += "\"zone\":\"" + Escape(SkillTree::ZoneName(n.zone)) + "\",";
            j += "\"scale\":" + std::to_string(n.scale) + ",";
            j += "\"reqPower\":\"" +
                 Escape(Isekai::PowerName(SkillTree::RequiredPower(n.key)).c_str()) + "\",";
            // >0 only for a DORMANT blessing: the seal is a level away, not a
            // rebirth away, and the tooltip should say so.
            j += "\"reqLevel\":" +
                 std::to_string(Isekai::AwakeningLevelFor(SkillTree::RequiredPower(n.key))) +
                 ",";
            j += "\"prereq\":[" + std::to_string(n.prereq[0]) + "," +
                 std::to_string(n.prereq[1]) + "]";
            j += "}";
        }
        j += "]}";
        return j;
    }

    [[nodiscard]] std::string Panel(const PanelSpec& a_spec) {
        std::string j = "{";
        j += "\"title\":\"" + Escape(a_spec.title.c_str()) + "\",";
        j += "\"body\":\"" + Escape(a_spec.body.c_str()) + "\",";
        j += "\"reveal\":" + std::to_string(static_cast<int>(a_spec.revealCharsPerSec)) + ",";
        j += "\"width\":" + std::to_string(static_cast<int>(a_spec.widthHint)) + ",";
        j += "\"tone\":\"" + std::string(Logic::ToneName(a_spec.tone)) + "\",";
        // The crest beside the title; the Prisma page ignores it. Absent when the panel
        // has neither an icon nor a caption.
        if (!a_spec.emblemIcon.empty() || !a_spec.emblemCaption.empty()) {
            j += "\"emblem\":{\"icon\":\"";
            if (!a_spec.emblemIcon.empty()) {
                j += "icons/" + Escape(IconFile(a_spec.emblemIcon).c_str());
            }
            j += "\",\"caption\":\"" + Escape(a_spec.emblemCaption.c_str()) + "\",";
            j += "\"tone\":\"" + std::string(Logic::ToneName(a_spec.emblemTone)) + "\"},";
        }
        j += "\"buttons\":[";
        for (std::size_t i = 0; i < a_spec.choices.size(); ++i) {
            const auto& c = a_spec.choices[i];
            if (i != 0) {
                j += ",";
            }
            j += "{\"label\":\"" + Escape(c.label.c_str()) + "\",";
            j += "\"icon\":\"" + Escape(IconFile(c.icon).c_str()) + "\",";
            j += "\"iconOnly\":" + std::string(Bool(c.iconOnly)) + "}";
        }
        j += "]}";
        return j;
    }

    // The shop catalog + the current balance. Reads game state — MAIN THREAD ONLY.
    [[nodiscard]] std::string Shop() {
        std::string j = "{\"points\":" + std::to_string(GetState().systemPoints) + ",";

        // The shelves come over as their own list, in order, so an empty category
        // (potions before their ESP records exist) still holds its place in the rail
        // instead of vanishing and reordering the ones around it.
        j += "\"shelves\":[";
        const auto shelves = Isekai::Shop::Shelves();
        for (std::size_t i = 0; i < shelves.size(); ++i) {
            j += (i ? "," : "") + ("\"" + Escape(shelves[i].c_str()) + "\"");
        }
        j += "],\"items\":[";

        const auto items = Isekai::Shop::Catalog();
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (i != 0) {
                j += ",";
            }
            j += "{\"name\":\"" + Escape(items[i].name.c_str()) + "\",";
            j += "\"qty\":\"" + Escape(items[i].qty.c_str()) + "\",";
            // One effect per line; Esc turns the newlines into \n and the card renders
            // them with white-space: pre-line.
            j += "\"fx\":\"" + Escape(items[i].effects.c_str()) + "\",";
            j += "\"cost\":" + std::to_string(items[i].cost) + ",";
            j += "\"shelf\":\"" + Escape(items[i].shelf.c_str()) + "\",";
            j += "\"icon\":\"" + Escape(IconFile(items[i].icon).c_str()) + "\"}";
        }
        j += "]}";
        return j;
    }
}
