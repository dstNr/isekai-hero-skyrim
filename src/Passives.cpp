#include "Passives.h"

#include "Plugin.h"
#include "Progression.h"

#include <map>
#include <vector>

namespace Isekai::Passives {

    namespace {
        // Local FormIDs of the ability spells, read out of the real IsekaiHero.esp
        // (see the form dump in Plugin.cpp). Spells do not keep their editor IDs at
        // runtime, so a FormID is the only handle we have.
        //
        // Note we do NOT record which actor value each spell is for: that is read back
        // off the spell's own magic effect below. Writing it down here would be a second
        // source of truth, free to drift out of step with what the ESP actually says.
        constexpr RE::FormID kAbilityFormIDs[] = {
            0x000D69,  // System: Vitality
            0x000D6A,  // System: Arcane
            0x000D6C,  // System: Endurance
            0x000D6E,  // System: Burden
            0x000D70,  // System: Warding
            0x000D72,  // System: Emberskin
            0x000D74,  // System: Frostskin
            0x000D76,  // System: Purity
        };

        struct Ability {
            RE::SpellItem* spell = nullptr;
            RE::Effect*    effect = nullptr;  // the one whose magnitude we drive
        };

        // Actor value -> the ability that fortifies it.
        std::map<RE::ActorValue, Ability> g_abilities;

        // The magnitude baked into an ESP spell is fixed; ours has to change as the
        // player earns more. Rewriting it is easy — but an ability already running on
        // the player snapshotted the old value, so it has to be re-applied to take.
        void Reapply(RE::Actor* a_player, RE::SpellItem* a_spell) {
            if (a_player->HasSpell(a_spell)) {
                a_player->RemoveSpell(a_spell);
            }
            a_player->AddSpell(a_spell);
        }
    }

    void Install() {
        g_abilities.clear();

        if (!Plugin::IsLoaded()) {
            logger::warn("Passives: {} not loaded — no ability spells", Plugin::kFileName);
            return;
        }

        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data) {
            return;
        }

        for (const auto id : kAbilityFormIDs) {
            auto* spell = data->LookupForm<RE::SpellItem>(id, Plugin::kFileName);
            if (!spell) {
                logger::error("Passives: no spell {:#08x} in {}", id, Plugin::kFileName);
                continue;
            }
            if (spell->effects.empty() || !spell->effects[0] || !spell->effects[0]->baseEffect) {
                logger::error("Passives: spell {:#08x} (\"{}\") has no magic effect attached", id,
                              spell->GetName());
                continue;
            }

            auto*      effect = spell->effects[0];
            const auto av = effect->baseEffect->data.primaryAV;

            if (const auto [it, fresh] = g_abilities.try_emplace(av, Ability{ spell, effect });
                !fresh) {
                logger::error(
                    "Passives: spell {:#08x} (\"{}\") drives the same actor value as an earlier "
                    "one — one of them is wired to the wrong magic effect",
                    id, spell->GetName());
                continue;
            }

            logger::info("Passives: \"{}\" fortifies {}", spell->GetName(),
                         static_cast<int>(av));
        }

        logger::info("Passives: {} of {} abilities resolved", g_abilities.size(),
                     std::size(kAbilityFormIDs));
    }

    void Refresh() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player || g_abilities.empty()) {
            return;
        }

        // Total up what the earned milestones are worth, per actor value.
        std::map<RE::ActorValue, float> totals;
        for (const auto* passive : Progression::EarnedPassives()) {
            totals[passive->actorValue] += Progression::PassiveAmount(*passive);
        }

        for (auto& [av, ability] : g_abilities) {
            const auto it = totals.find(av);
            const float total = it != totals.end() ? it->second : 0.0f;

            // Nothing earned for this stat yet: make sure the ability is not sitting on
            // the player advertising a bonus of zero.
            if (total <= 0.0f) {
                if (player->HasSpell(ability.spell)) {
                    player->RemoveSpell(ability.spell);
                }
                continue;
            }

            ability.effect->effectItem.magnitude = total;
            Reapply(player, ability.spell);

            logger::info("Passive \"{}\": {:.0f}", ability.spell->GetName(), total);
        }
    }
}
