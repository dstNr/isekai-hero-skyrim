#pragma once

// Engine-free logic of the Magelight UI: tones, threat verdicts, the JS-to-plugin
// action wire format, the page ready gate, decision re-offer, the label push gate and
// the label geometry, content hash and heartbeat, and the page console rate limit.
//
// Standard library only, on purpose: the plugin and the bare-MSVC unit tests
// (tests/ui_logic_tests.cpp) both compile this header.

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <iterator>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Isekai::UI::Logic {

    // --- Tones ---------------------------------------------------------------------

    enum class Tone : std::uint8_t { Cyan, Gold, Red, Violet };

    constexpr std::string_view ToneName(Tone a_tone) {
        switch (a_tone) {
            case Tone::Gold: return "gold";
            case Tone::Red: return "red";
            case Tone::Violet: return "violet";
            case Tone::Cyan: break;
        }
        return "cyan";
    }

    enum class Blessing : std::uint8_t { Normal, Hero, Ascended, Full, Shattered, Dormant, Custom };

    constexpr Tone ToneFor(Blessing a_blessing) {
        switch (a_blessing) {
            case Blessing::Ascended: return Tone::Gold;
            case Blessing::Shattered: return Tone::Red;
            case Blessing::Dormant:
            case Blessing::Custom: return Tone::Violet;
            case Blessing::Normal:
            case Blessing::Hero:
            case Blessing::Full: break;
        }
        return Tone::Cyan;
    }

    constexpr Tone ToneForRank(char a_rank) {
        return a_rank == 'S' ? Tone::Gold : Tone::Cyan;
    }

    // --- Threat verdict ------------------------------------------------------------

    enum class Verdict : std::uint8_t { Trivial, Manageable, Dangerous, Lethal };

    // a_levelDiff = enemy level minus player level.
    constexpr Verdict VerdictFor(std::int32_t a_levelDiff) {
        if (a_levelDiff >= 20) return Verdict::Lethal;
        if (a_levelDiff >= 5) return Verdict::Dangerous;
        if (a_levelDiff <= -15) return Verdict::Trivial;
        return Verdict::Manageable;
    }

    constexpr Tone ToneFor(Verdict a_verdict) {
        return (a_verdict == Verdict::Dangerous || a_verdict == Verdict::Lethal) ? Tone::Red : Tone::Cyan;
    }

    // --- Action wire format --------------------------------------------------------

    struct Action {
        enum class Type : std::uint8_t { Choose, Buy, Respec, ShopBuy, Status, Close, Nav };
        Type type;
        std::uint32_t number = 0;
        std::string word;
    };

    namespace detail {
        constexpr std::size_t kMaxActionLength = 256;

        inline bool ParseUInt(std::string_view a_text, std::uint32_t& a_out) {
            if (a_text.empty()) return false;
            const char* first = a_text.data();
            const char* last = first + a_text.size();
            std::uint32_t value = 0;
            const auto [ptr, ec] = std::from_chars(first, last, value);
            if (ec != std::errc{} || ptr != last) return false;
            a_out = value;
            return true;
        }

        template <std::size_t N>
        constexpr bool OneOf(std::string_view a_word, const std::string_view (&a_set)[N]) {
            return std::ranges::find(a_set, a_word) != std::end(a_set);
        }
    }

    // "type" or "type:arg". Anything malformed gives nullopt; never throws.
    inline std::optional<Action> ParseAction(std::string_view a_text) {
        if (a_text.empty() || a_text.size() > detail::kMaxActionLength) return std::nullopt;

        const auto colon = a_text.find(':');
        const bool hasArg = colon != std::string_view::npos;
        const std::string_view type = a_text.substr(0, colon);
        const std::string_view arg = hasArg ? a_text.substr(colon + 1) : std::string_view{};

        Action action{};
        if (type == "close" || type == "respec") {
            if (hasArg) return std::nullopt;
            action.type = type == "close" ? Action::Type::Close : Action::Type::Respec;
            return action;
        }
        if (!hasArg) return std::nullopt;

        if (type == "choose" || type == "shopBuy" || type == "buy") {
            if (!detail::ParseUInt(arg, action.number)) return std::nullopt;
            if (type == "buy") {
                if (action.number == 0) return std::nullopt;
                action.type = Action::Type::Buy;
            } else {
                action.type = type == "choose" ? Action::Type::Choose : Action::Type::ShopBuy;
            }
            return action;
        }
        if (type == "status") {
            static constexpr std::string_view kWords[] = { "tree", "storage", "shop", "reroll", "reboot", "close" };
            if (!detail::OneOf(arg, kWords)) return std::nullopt;
            action.type = Action::Type::Status;
            action.word = std::string(arg);
            return action;
        }
        if (type == "nav") {
            static constexpr std::string_view kWords[] = { "status", "tree", "shop" };
            if (!detail::OneOf(arg, kWords)) return std::nullopt;
            action.type = Action::Type::Nav;
            action.word = std::string(arg);
            return action;
        }
        return std::nullopt;
    }

    // --- Ready gate ----------------------------------------------------------------

    // JS calls made before the page reports ready are queued (bounded) and flushed in
    // order once it does. Reset is for a page reload.
    class ReadyGate {
    public:
        static constexpr std::size_t kMaxPending = 64;

        // Before ready: queues and returns nullopt. After ready: returns the call to run now.
        std::optional<std::string> Submit(std::string a_call) {
            if (ready_) return a_call;
            if (pending_.size() >= kMaxPending) pending_.pop_front();
            pending_.push_back(std::move(a_call));
            return std::nullopt;
        }

        std::vector<std::string> MarkReady() {
            ready_ = true;
            std::vector<std::string> out(std::make_move_iterator(pending_.begin()),
                                         std::make_move_iterator(pending_.end()));
            pending_.clear();
            return out;
        }

        void Reset() {
            ready_ = false;
            pending_.clear();
        }

        bool Ready() const { return ready_; }

    private:
        bool ready_ = false;
        std::deque<std::string> pending_;
    };

    // --- Pending decision ----------------------------------------------------------

    // The one open decision (for example a blessing choice). If the UI mode is lost
    // while it is open, it has to be offered again once the UI is back.
    class PendingDecision {
    public:
        // Replaces any current decision. Offering the same id again is the re-offer: it
        // clears NeedsReoffer.
        void Offer(std::uint32_t a_id) {
            id_ = a_id;
            reoffer_ = false;
        }

        // A wrong id is a no-op.
        void Resolve(std::uint32_t a_id) {
            if (id_ && *id_ == a_id) Reset();
        }

        void OnUiModeLost() {
            if (id_) reoffer_ = true;
        }

        bool NeedsReoffer() const { return reoffer_; }
        std::optional<std::uint32_t> Current() const { return id_; }

        void Reset() {
            id_.reset();
            reoffer_ = false;
        }

    private:
        std::optional<std::uint32_t> id_;
        bool reoffer_ = false;
    };

    // --- Label push gate -----------------------------------------------------------

    struct LabelFrame {
        std::uint32_t id;
        float x, y, scale;  // x/y normalised 0..1
        std::uint64_t content;  // hash of everything that is not a position
    };

    enum class LabelPush : std::uint8_t { None, Positions, Full };

    // Decides per frame what, if anything, has to be sent to the page: nothing, only
    // positions, or the full label set. Movement is measured against the last frame that
    // was pushed, so slow drift cannot stay under the threshold forever.
    class LabelPushGate {
    public:
        LabelPush Update(std::span<const LabelFrame> a_frames, float a_screenW, float a_screenH,
                         float a_minMovePx = 0.5f) {
            if (a_frames.empty()) {
                if (last_.empty()) return LabelPush::None;
                last_.clear();
                return LabelPush::Full;
            }

            bool full = a_frames.size() != last_.size();
            bool moved = false;
            if (!full) {
                for (std::size_t i = 0; i < a_frames.size(); ++i) {
                    const LabelFrame& now = a_frames[i];
                    const LabelFrame& was = last_[i];
                    if (now.id != was.id || now.content != was.content) {
                        full = true;
                        break;
                    }
                    const float dx = (now.x - was.x) * a_screenW;
                    const float dy = (now.y - was.y) * a_screenH;
                    if (std::abs(dx) > a_minMovePx || std::abs(dy) > a_minMovePx ||
                        std::abs(now.scale - was.scale) > kScaleEpsilon) {
                        moved = true;
                    }
                }
            }

            if (full || moved) last_.assign(a_frames.begin(), a_frames.end());
            if (full) return LabelPush::Full;
            return moved ? LabelPush::Positions : LabelPush::None;
        }

        void Reset() { last_.clear(); }

    private:
        static constexpr float kScaleEpsilon = 0.005f;
        std::vector<LabelFrame> last_;
    };

    // --- Label geometry and content ------------------------------------------------

    // NiCamera::WorldPtToScreenPt3 gives the port's y running UP from the bottom; the hud
    // page's y runs DOWN from the top (both 0..1).
    constexpr float PageY(float a_portY) { return 1.0f - a_portY; }

    // The distance factor alone, 1 - 0.32*t with t = distance / range clamped to 0..1. The
    // page multiplies UiScale in and dims with the same t; this must not do either.
    constexpr float LabelDistanceScale(float a_distance, float a_range) {
        const float t = a_range > 0.0f ? std::clamp(a_distance / a_range, 0.0f, 1.0f) : 1.0f;
        return 1.0f - 0.32f * t;
    }

    // A projected head point (page space, 0..1, y down) still worth a label on an a_w x
    // a_h screen: up to 200 px past either side and 100 px past the top or bottom, so a
    // frame half out of view does not blink off at the edge. NaN fails every comparison.
    constexpr bool LabelOnScreen(float a_x, float a_y, float a_w, float a_h) {
        const float px = a_x * a_w;
        const float py = a_y * a_h;
        return px > -200.0f && px < a_w + 200.0f && py > -100.0f && py < a_h + 100.0f;
    }

    // A pool at bar resolution: now/max as 0..kPoolSteps, rounded, 0 when there is no pool
    // (max <= 0). Magicka and stamina regenerate every frame in combat; hashed and sent
    // exactly, they would turn every frame into a full push for a change no bar can show.
    // Sent as [step, kPoolSteps], the page's fill (now / max) is the same bar.
    inline constexpr std::int32_t kPoolSteps = 200;

    constexpr std::int32_t PoolStep(std::int32_t a_now, std::int32_t a_max) {
        if (a_max <= 0) return 0;
        const std::int64_t now = std::clamp<std::int64_t>(a_now, 0, a_max);
        return static_cast<std::int32_t>((now * kPoolSteps + a_max / 2) / a_max);
    }

    // FNV-1a over a label's non-position content (level, name, verdict, pools): what
    // LabelFrame::content holds. Integers go in as 8 little-endian bytes, text with its
    // length first, so "ab"+"c" and "a"+"bc" differ.
    class ContentHash {
    public:
        ContentHash& AddInt(std::int64_t a_value) {
            auto v = static_cast<std::uint64_t>(a_value);
            for (int i = 0; i < 8; ++i) {
                Byte(static_cast<std::uint8_t>(v & 0xFFu));
                v >>= 8;
            }
            return *this;
        }

        ContentHash& AddText(std::string_view a_text) {
            AddInt(static_cast<std::int64_t>(a_text.size()));
            for (const char c : a_text) Byte(static_cast<std::uint8_t>(c));
            return *this;
        }

        std::uint64_t Value() const { return h_; }

    private:
        void Byte(std::uint8_t a_byte) {
            h_ ^= a_byte;
            h_ *= 1099511628211ull;
        }

        std::uint64_t h_ = 14695981039346656037ull;
    };

    // The labels' sign of life. The page hides every label kPageStale after the last
    // labels.* call (the frame tick stops in free camera and while paused). While labels
    // are up and a frame sends nothing (the push gate said None), keep() goes out once the
    // last call is kPeriod old: at most 4 Hz. kPageStale is more than twice kPeriod, so a
    // frame that lands late at a low frame rate (a keep() due at 250 ms that goes out with
    // the next frame, 100 ms later) does not blink the labels off.
    class LabelHeartbeat {
    public:
        using Clock = std::chrono::steady_clock;
        static constexpr std::chrono::milliseconds kPeriod{ 250 };
        // hud.html's STALE_MS (tools/check.mjs holds the two equal).
        static constexpr std::chrono::milliseconds kPageStale{ 600 };

        // A labels.* call went out (full, pos or keep).
        void Sent(Clock::time_point a_now) { last_ = a_now; }

        // Nothing else went out this frame and labels are on the page: send keep() now?
        // A clock reading earlier than the last call counts as due rather than wedging.
        bool KeepDue(Clock::time_point a_now) const {
            return !last_ || a_now < *last_ || a_now - *last_ >= kPeriod;
        }

        void Reset() { last_.reset(); }

    private:
        std::optional<Clock::time_point> last_;
    };

    // --- Page console rate limit --------------------------------------------------

    // Page console messages go to the plugin log; a page stuck in a loop that warns every
    // frame would fill it. At most kMax lines per kWindow, counted from the first message
    // of a window; what is held back is counted and reported once, with the first message
    // of the next window.
    class ConsoleLogLimit {
    public:
        using Clock = std::chrono::steady_clock;
        static constexpr std::uint32_t kMax = 20;
        static constexpr std::chrono::seconds kWindow{ 10 };

        struct Verdict {
            bool log = false;              // write this message
            std::uint32_t suppressed = 0;  // first write "N messages suppressed" (0 = none)
        };

        Verdict Admit(Clock::time_point a_now) {
            Verdict v;
            // A new window, also when the clock reads earlier than the window start.
            if (!start_ || a_now < *start_ || a_now - *start_ >= kWindow) {
                v.suppressed = std::exchange(held_, 0u);
                start_ = a_now;
                count_ = 0;
            }
            if (count_ < kMax) {
                ++count_;
                v.log = true;
            } else {
                ++held_;
            }
            return v;
        }

        std::uint32_t Held() const { return held_; }

    private:
        std::optional<Clock::time_point> start_;
        std::uint32_t count_ = 0;
        std::uint32_t held_ = 0;
    };

    // --- UI-mode exits we owe ourselves ---------------------------------------------

    // Every UI-mode period ends in exactly one exit event. The exits our own releases cause
    // are counted here and swallowed, and a request made while one is still owed waits for
    // it: the host runs a release later, so a request made before that runs would be ended
    // by it (an invisible screen, the game unpaused).
    class OwnExits {
    public:
        // We release UI mode. a_entered: the period had been entered, so its exit follows.
        void Close(bool a_entered) {
            deferred_ = false;
            if (a_entered) ++owed_;
        }

        // An entry that ran just before we closed: our release ends it, its exit follows.
        void StaleEntry() { ++owed_; }

        // We want UI mode. True = request it now; false = wait for the owed exit.
        bool Open() {
            if (owed_ == 0) return true;
            deferred_ = true;
            return false;
        }

        enum class Exit : std::uint8_t {
            Theirs,          // not ours: the screen lost UI mode
            Ours,            // ours: swallow it
            OursRequestNow,  // ours, the last one owed, and an Open waited for it: request now
        };

        Exit OnExited() {
            if (owed_ == 0) return Exit::Theirs;
            if (--owed_ > 0 || !deferred_) return Exit::Ours;
            deferred_ = false;
            return Exit::OursRequestNow;
        }

        // The wanted screen went away (lost, refused): nothing waits any more.
        void CancelDeferred() { deferred_ = false; }

        // Back to nothing owed (a load, a recovery from a count that leaked).
        void Reset() {
            owed_ = 0;
            deferred_ = false;
        }

        int Owed() const { return owed_; }
        bool Deferred() const { return deferred_; }

    private:
        int  owed_ = 0;
        bool deferred_ = false;
    };

    // --- Page / build match --------------------------------------------------------

    // The page reports the build it was made for; both must be present and equal.
    constexpr bool PageMatchesBuild(std::string_view a_page, std::string_view a_build) {
        return !a_page.empty() && !a_build.empty() && a_page == a_build;
    }

}  // namespace Isekai::UI::Logic
