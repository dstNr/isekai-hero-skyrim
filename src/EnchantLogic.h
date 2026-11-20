#pragma once

// The rules behind the System enchantments, with every engine read taken out.
// tests/enchant_logic_tests.cpp feeds them by hand. Nothing here may include CommonLibSSE.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace Isekai::Enchant::Logic {

    struct Candidate {
        float distance;
        bool hostileToAttacker;
        bool alive;
        bool isAttacker;
        bool isFirstTarget;
    };

    // Indices of alive, hostile, in-range candidates that are neither the attacker nor the
    // first target, nearest first, at most a_maxJumps.
    inline std::vector<std::size_t> PickChainTargets(std::span<const Candidate> a_candidates,
                                                     std::size_t a_maxJumps, float a_range) {
        std::vector<std::size_t> out;
        for (std::size_t i = 0; i < a_candidates.size(); ++i) {
            const auto& c = a_candidates[i];
            if (c.alive && c.hostileToAttacker && !c.isAttacker && !c.isFirstTarget && c.distance <= a_range) {
                out.push_back(i);
            }
        }
        std::stable_sort(out.begin(), out.end(), [&](std::size_t a, std::size_t b) {
            return a_candidates[a].distance < a_candidates[b].distance;
        });
        if (out.size() > a_maxJumps) out.resize(a_maxJumps);
        return out;
    }

    inline float ReflectFraction(float a_summedPercent) {
        return std::clamp(a_summedPercent, 0.0f, 90.0f) / 100.0f;
    }

    inline constexpr float kPhoenixThreshold = 0.25f;
    inline constexpr double kPhoenixCooldownSeconds = 60.0;

    inline bool PhoenixShouldFire(float a_healthFraction, double a_now, double a_readyAt) {
        return a_healthFraction > 0.0f && a_healthFraction < kPhoenixThreshold && a_now >= a_readyAt;
    }

    inline std::optional<float> StrongestMagnitude(std::span<const float> a_magnitudes) {
        if (a_magnitudes.empty()) return std::nullopt;
        return *std::max_element(a_magnitudes.begin(), a_magnitudes.end());
    }

    // A cooldown still running at a_now; expired ones are not worth saving.
    inline constexpr bool CooldownLive(double a_readyAt, double a_now) { return a_readyAt > a_now; }

    inline constexpr std::uint32_t kMaxSavedCooldowns = 4096;

    // Bounds a co-save read: a corrupt count must not drive an allocation.
    inline constexpr bool CooldownCountAcceptable(std::uint32_t a_count) { return a_count <= kMaxSavedCooldowns; }

}
