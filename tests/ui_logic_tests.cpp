// Unit tests for src/UI/UiLogic.h: the engine-free logic of the Magelight UI
// (tones, verdicts, action parsing, ready gate, decision re-offer, label push gate),
// without Skyrim. Run with tools/run-unit-tests.ps1.

#include "../src/UI/UiLogic.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

namespace {
    int g_failed = 0;
    int g_checks = 0;

    void Expect(bool a_ok, const char* a_what, int a_line) {
        ++g_checks;
        if (!a_ok) {
            ++g_failed;
            std::printf("  FAIL line %d: %s\n", a_line, a_what);
        }
    }
#define EXPECT(cond) Expect((cond), #cond, __LINE__)

    using namespace Isekai::UI::Logic;

    void Tones() {
        EXPECT(ToneFor(Blessing::Ascended) == Tone::Gold);
        EXPECT(ToneFor(Blessing::Shattered) == Tone::Red);
        EXPECT(ToneFor(Blessing::Dormant) == Tone::Violet);
        EXPECT(ToneFor(Blessing::Custom) == Tone::Violet);
        EXPECT(ToneFor(Blessing::Hero) == Tone::Cyan);
        EXPECT(ToneFor(Blessing::Normal) == Tone::Cyan);
        EXPECT(ToneFor(Blessing::Full) == Tone::Cyan);
        EXPECT(ToneForRank('S') == Tone::Gold);
        EXPECT(ToneForRank('A') == Tone::Cyan);
        EXPECT(ToneName(Tone::Cyan) == "cyan");
        EXPECT(ToneName(Tone::Gold) == "gold");
        EXPECT(ToneName(Tone::Red) == "red");
        EXPECT(ToneName(Tone::Violet) == "violet");
    }

    void Verdicts() {
        EXPECT(VerdictFor(20) == Verdict::Lethal);
        EXPECT(VerdictFor(19) == Verdict::Dangerous);
        EXPECT(VerdictFor(5) == Verdict::Dangerous);
        EXPECT(VerdictFor(4) == Verdict::Manageable);
        EXPECT(VerdictFor(0) == Verdict::Manageable);
        EXPECT(VerdictFor(-14) == Verdict::Manageable);
        EXPECT(VerdictFor(-15) == Verdict::Trivial);
        EXPECT(ToneFor(Verdict::Dangerous) == Tone::Red);
        EXPECT(ToneFor(Verdict::Lethal) == Tone::Red);
        EXPECT(ToneFor(Verdict::Manageable) == Tone::Cyan);
        EXPECT(ToneFor(Verdict::Trivial) == Tone::Cyan);
    }

    void ParseValid() {
        auto a = ParseAction("choose:2");
        EXPECT(a && a->type == Action::Type::Choose && a->number == 2);
        a = ParseAction("shopBuy:0");
        EXPECT(a && a->type == Action::Type::ShopBuy && a->number == 0);
        a = ParseAction("buy:123456");
        EXPECT(a && a->type == Action::Type::Buy && a->number == 123456);
        a = ParseAction("status:storage");
        EXPECT(a && a->type == Action::Type::Status && a->word == "storage");
        a = ParseAction("nav:tree");
        EXPECT(a && a->type == Action::Type::Nav && a->word == "tree");
        a = ParseAction("close");
        EXPECT(a && a->type == Action::Type::Close);
        a = ParseAction("respec");
        EXPECT(a && a->type == Action::Type::Respec);
        for (const char* w : { "tree", "storage", "shop", "reroll", "reboot", "close" }) {
            a = ParseAction(std::string("status:") + w);
            EXPECT(a && a->word == w);
        }
        for (const char* w : { "status", "tree", "shop" }) {
            a = ParseAction(std::string("nav:") + w);
            EXPECT(a && a->word == w);
        }
    }

    void ParseInvalid() {
        EXPECT(!ParseAction(""));
        EXPECT(!ParseAction("choose:-1"));
        EXPECT(!ParseAction("choose:x"));
        EXPECT(!ParseAction("choose:"));
        EXPECT(!ParseAction("choose:2x"));
        EXPECT(!ParseAction("choose"));
        EXPECT(!ParseAction("buy:0"));
        EXPECT(!ParseAction("buy:4294967296"));
        EXPECT(!ParseAction("status:format"));
        EXPECT(!ParseAction("nav:inventory"));
        EXPECT(!ParseAction("close:1"));
        EXPECT(!ParseAction("respec:1"));
        EXPECT(!ParseAction("launch"));
        EXPECT(!ParseAction(std::string(257, 'a')));
        EXPECT(!ParseAction("choose:" + std::string(250, '1')));
    }

    void ParseLengthCap() {
        // Would parse as choose:1 if the cap did not apply.
        EXPECT(!ParseAction("choose:" + std::string(300, '0') + "1"));
        EXPECT(!ParseAction("choose:" + std::string(249, '0') + "1"));  // 257 chars
        const std::string atCap = "choose:" + std::string(248, '0') + "1";  // exactly 256 chars
        EXPECT(atCap.size() == 256);
        const auto a = ParseAction(atCap);
        EXPECT(a && a->type == Action::Type::Choose && a->number == 1);
    }

    void ReadyGateQueues() {
        ReadyGate g;
        EXPECT(!g.Ready());
        EXPECT(!g.Submit("a"));
        EXPECT(!g.Submit("b"));
        EXPECT(!g.Submit("c"));
        const auto q = g.MarkReady();
        EXPECT(g.Ready());
        EXPECT(q.size() == 3 && q[0] == "a" && q[1] == "b" && q[2] == "c");
        const auto now = g.Submit("d");
        EXPECT(now && *now == "d");
        EXPECT(g.MarkReady().empty());
    }

    void ReadyGateDropsOldest() {
        ReadyGate g;
        for (int i = 1; i <= 65; ++i) g.Submit("call" + std::to_string(i));
        const auto q = g.MarkReady();
        EXPECT(q.size() == ReadyGate::kMaxPending);
        EXPECT(!q.empty() && q.front() == "call2");
        EXPECT(!q.empty() && q.back() == "call65");
    }

    void ReadyGateReset() {
        ReadyGate g;
        g.Submit("a");
        g.MarkReady();
        EXPECT(g.Ready());
        g.Reset();
        EXPECT(!g.Ready());
        EXPECT(!g.Submit("b"));
        g.Reset();
        EXPECT(g.MarkReady().empty());
    }

    void DecisionReoffer() {
        PendingDecision d;
        d.Offer(7);
        d.OnUiModeLost();
        EXPECT(d.NeedsReoffer());
        EXPECT(d.Current() == 7u);
        d.Resolve(8);
        EXPECT(d.Current() == 7u);
        EXPECT(d.NeedsReoffer());
        d.Resolve(7);
        EXPECT(!d.Current());
        EXPECT(!d.NeedsReoffer());
    }

    void DecisionNothingPending() {
        PendingDecision d;
        d.OnUiModeLost();
        EXPECT(!d.NeedsReoffer());
        EXPECT(!d.Current());
    }

    void DecisionOfferSemantics() {
        PendingDecision d;
        d.Offer(7);
        d.OnUiModeLost();
        d.Offer(7);
        EXPECT(!d.NeedsReoffer());
        d.OnUiModeLost();
        d.Offer(9);
        EXPECT(d.Current() == 9u);
        EXPECT(!d.NeedsReoffer());
        d.OnUiModeLost();
        d.Reset();
        EXPECT(!d.Current() && !d.NeedsReoffer());
    }

    // The sequence Screens.cpp runs for a decision panel whose UI mode was lost: the
    // re-shown panel offers the same serial again, which clears the re-offer flag, and the
    // choice that finally arrives resolves it.
    void DecisionScreensReoffer() {
        PendingDecision d;
        d.Offer(1);
        d.OnUiModeLost();
        EXPECT(d.NeedsReoffer());
        d.Offer(1);
        EXPECT(!d.NeedsReoffer());
        EXPECT(d.Current() == 1u);
        d.Resolve(1);
        EXPECT(!d.Current());
        EXPECT(!d.NeedsReoffer());
    }

    // A screen closed and the next one opened in the same tick (queued reward panels): the
    // request waits for the exit of our own release, and goes out when it arrives.
    void OwnExitsCloseThenOpenSameTick() {
        OwnExits e;
        e.Close(/*a_entered=*/true);
        EXPECT(!e.Open());
        EXPECT(e.Deferred());
        EXPECT(e.OnExited() == OwnExits::Exit::OursRequestNow);
        EXPECT(!e.Deferred() && e.Owed() == 0);
        EXPECT(e.OnExited() == OwnExits::Exit::Theirs);  // the next exit is a real loss
    }

    // The exit arrived before the next open: nothing to wait for, and nothing held back.
    void OwnExitsCloseExitThenOpen() {
        OwnExits e;
        e.Close(true);
        EXPECT(e.OnExited() == OwnExits::Exit::Ours);
        EXPECT(e.Open());
        EXPECT(!e.Deferred());
        // Closing a request that never entered owes nothing.
        e.Close(false);
        EXPECT(e.Owed() == 0 && e.Open());
    }

    // Two exits owed (a stale entry and our close): the held open waits for the second.
    void OwnExitsTwoOwedHoldOpen() {
        OwnExits e;
        e.StaleEntry();
        e.Close(true);
        EXPECT(e.Owed() == 2);
        EXPECT(!e.Open());
        EXPECT(e.OnExited() == OwnExits::Exit::Ours);
        EXPECT(e.Deferred() && e.Owed() == 1);  // still held
        EXPECT(e.OnExited() == OwnExits::Exit::OursRequestNow);
        EXPECT(!e.Deferred() && e.Owed() == 0);
        EXPECT(e.OnExited() == OwnExits::Exit::Theirs);
    }

    void OwnExitsCancelAndReset() {
        OwnExits e;
        e.Close(true);
        EXPECT(!e.Open());
        e.CancelDeferred();
        EXPECT(e.OnExited() == OwnExits::Exit::Ours);  // still owed, but nobody waits
        e.StaleEntry();
        e.StaleEntry();
        EXPECT(!e.Open());
        EXPECT(e.OnExited() == OwnExits::Exit::Ours);
        e.Reset();
        EXPECT(e.Owed() == 0 && !e.Deferred() && e.Open());
        e.Close(true);
        EXPECT(!e.Open());
        e.Close(false);  // a close drops the waiting open
        EXPECT(e.OnExited() == OwnExits::Exit::Ours);
    }

    void LabelPushDecisions() {
        constexpr float W = 1920.0f, H = 1080.0f;
        LabelPushGate g;
        const std::vector<LabelFrame> none;
        EXPECT(g.Update(none, W, H) == LabelPush::None);

        const std::vector<LabelFrame> f{ { 1, 0.5f, 0.5f, 1.0f, 11 }, { 2, 0.2f, 0.3f, 1.0f, 22 } };
        EXPECT(g.Update(f, W, H) == LabelPush::Full);
        EXPECT(g.Update(f, W, H) == LabelPush::None);

        auto moved = f;
        moved[0].x += 0.4f / W;
        EXPECT(g.Update(moved, W, H) == LabelPush::None);
        // Sub-threshold drift is measured against the last pushed position.
        moved = f;
        moved[0].x += 0.6f / W;
        EXPECT(g.Update(moved, W, H) == LabelPush::Positions);
        EXPECT(g.Update(moved, W, H) == LabelPush::None);

        auto content = moved;
        content[1].content = 23;
        EXPECT(g.Update(content, W, H) == LabelPush::Full);

        auto ids = content;
        ids[1].id = 3;
        EXPECT(g.Update(ids, W, H) == LabelPush::Full);

        auto fewer = ids;
        fewer.pop_back();
        EXPECT(g.Update(fewer, W, H) == LabelPush::Full);

        EXPECT(g.Update(none, W, H) == LabelPush::Full);
        EXPECT(g.Update(none, W, H) == LabelPush::None);

        g.Reset();
        EXPECT(g.Update(f, W, H) == LabelPush::Full);
    }

    void LabelPushEdges() {
        constexpr float W = 1920.0f, H = 1080.0f;
        const std::vector<LabelFrame> f{ { 1, 0.5f, 0.5f, 1.0f, 11 } };

        LabelPushGate yOnly;
        yOnly.Update(f, W, H);
        auto m = f;
        m[0].y += 0.6f / H;
        EXPECT(yOnly.Update(m, W, H) == LabelPush::Positions);

        LabelPushGate scaleOnly;
        scaleOnly.Update(f, W, H);
        m = f;
        m[0].scale += 0.01f;
        EXPECT(scaleOnly.Update(m, W, H) == LabelPush::Positions);

        // Two +0.4 px steps: each is under the threshold on its own, but movement is
        // measured from the last pushed frame, so the second one (0.8 px total) pushes.
        LabelPushGate drift;
        drift.Update(f, W, H);
        auto step1 = f;
        step1[0].x += 0.4f / W;
        EXPECT(drift.Update(step1, W, H) == LabelPush::None);
        auto step2 = f;
        step2[0].x += 0.8f / W;
        EXPECT(drift.Update(step2, W, H) == LabelPush::Positions);
    }

    void LabelGeometry() {
        // y: the port runs up, the page down.
        EXPECT(PageY(0.0f) == 1.0f);
        EXPECT(PageY(1.0f) == 0.0f);
        EXPECT(PageY(0.25f) == 0.75f);
        // s: the distance factor alone, 1 at the player, 0.68 at range and beyond.
        EXPECT(LabelDistanceScale(0.0f, 4000.0f) == 1.0f);
        EXPECT(std::abs(LabelDistanceScale(2000.0f, 4000.0f) - 0.84f) < 1e-6f);
        EXPECT(std::abs(LabelDistanceScale(4000.0f, 4000.0f) - 0.68f) < 1e-6f);
        EXPECT(std::abs(LabelDistanceScale(9000.0f, 4000.0f) - 0.68f) < 1e-6f);
        EXPECT(LabelDistanceScale(-5.0f, 4000.0f) == 1.0f);
        EXPECT(std::abs(LabelDistanceScale(10.0f, 0.0f) - 0.68f) < 1e-6f);  // no range: farthest
        // On screen: 200 px of slack left/right, 100 px top/bottom.
        constexpr float W = 1920.0f, H = 1080.0f;
        EXPECT(LabelOnScreen(0.5f, 0.5f, W, H));
        EXPECT(LabelOnScreen(-190.0f / W, 0.5f, W, H));
        EXPECT(!LabelOnScreen(-210.0f / W, 0.5f, W, H));
        EXPECT(LabelOnScreen((W + 190.0f) / W, 0.5f, W, H));
        EXPECT(!LabelOnScreen((W + 210.0f) / W, 0.5f, W, H));
        EXPECT(LabelOnScreen(0.5f, -90.0f / H, W, H));
        EXPECT(!LabelOnScreen(0.5f, -110.0f / H, W, H));
        EXPECT(!LabelOnScreen(0.5f, (H + 110.0f) / H, W, H));
        EXPECT(!LabelOnScreen(std::nanf(""), 0.5f, W, H));
    }

    void LabelContentHash() {
        const auto h = [](std::int64_t a_level, std::string_view a_name, std::int64_t a_hp) {
            return ContentHash{}.AddInt(a_level).AddText(a_name).AddInt(a_hp).Value();
        };
        EXPECT(h(10, "Bandit", 50) == h(10, "Bandit", 50));
        EXPECT(h(10, "Bandit", 50) != h(11, "Bandit", 50));
        EXPECT(h(10, "Bandit", 50) != h(10, "Bandit", 49));
        EXPECT(h(10, "Bandit", 50) != h(10, "Bandit Chief", 50));
        // Length-prefixed: moving a character between two texts changes the hash.
        EXPECT(ContentHash{}.AddText("ab").AddText("c").Value() !=
               ContentHash{}.AddText("a").AddText("bc").Value());
        EXPECT(ContentHash{}.Value() != ContentHash{}.AddInt(0).Value());
    }

    void LabelKeepCadence() {
        using Clock = LabelHeartbeat::Clock;
        using std::chrono::milliseconds;
        const Clock::time_point t0{};
        LabelHeartbeat hb;
        EXPECT(hb.KeepDue(t0 + milliseconds(1000)));  // nothing sent yet: due
        hb.Sent(t0);
        EXPECT(!hb.KeepDue(t0));
        EXPECT(!hb.KeepDue(t0 + milliseconds(249)));
        EXPECT(hb.KeepDue(t0 + milliseconds(250)));   // 4 Hz at most
        // Every labels.* call restarts the period, so a moving set never gets a keep().
        hb.Sent(t0 + milliseconds(240));
        EXPECT(!hb.KeepDue(t0 + milliseconds(260)));
        EXPECT(hb.KeepDue(t0 + milliseconds(490)));
        // Well inside the page's 300 ms staleness limit.
        EXPECT(LabelHeartbeat::kPeriod < milliseconds(300));
        // A clock that reads earlier is due, never wedged.
        EXPECT(hb.KeepDue(t0));
        hb.Reset();
        EXPECT(hb.KeepDue(t0));

        // Still frames at 60 Hz for one second: keep() goes out 4 times.
        LabelHeartbeat sim;
        int keeps = 0;
        for (int f = 0; f < 60; ++f) {
            const auto now = t0 + milliseconds(f * 1000 / 60);
            if (sim.KeepDue(now)) {
                ++keeps;
                sim.Sent(now);
            }
        }
        EXPECT(keeps == 4);
    }

    void PageBuild() {
        EXPECT(PageMatchesBuild("a1b2c3", "a1b2c3"));
        EXPECT(!PageMatchesBuild("", ""));
        EXPECT(!PageMatchesBuild("dev", "a1b2c3"));
        EXPECT(!PageMatchesBuild("a1b2c3", ""));
    }

    struct Test {
        const char* name;
        void (*fn)();
    };
}

int main() {
    const Test tests[] = {
        { "Tones", Tones },
        { "Verdicts", Verdicts },
        { "ParseValid", ParseValid },
        { "ParseInvalid", ParseInvalid },
        { "ParseLengthCap", ParseLengthCap },
        { "ReadyGateQueues", ReadyGateQueues },
        { "ReadyGateDropsOldest", ReadyGateDropsOldest },
        { "ReadyGateReset", ReadyGateReset },
        { "DecisionReoffer", DecisionReoffer },
        { "DecisionNothingPending", DecisionNothingPending },
        { "DecisionOfferSemantics", DecisionOfferSemantics },
        { "DecisionScreensReoffer", DecisionScreensReoffer },
        { "OwnExitsCloseThenOpenSameTick", OwnExitsCloseThenOpenSameTick },
        { "OwnExitsCloseExitThenOpen", OwnExitsCloseExitThenOpen },
        { "OwnExitsTwoOwedHoldOpen", OwnExitsTwoOwedHoldOpen },
        { "OwnExitsCancelAndReset", OwnExitsCancelAndReset },
        { "LabelPushDecisions", LabelPushDecisions },
        { "LabelPushEdges", LabelPushEdges },
        { "LabelGeometry", LabelGeometry },
        { "LabelContentHash", LabelContentHash },
        { "LabelKeepCadence", LabelKeepCadence },
        { "PageBuild", PageBuild },
    };

    int failedTests = 0;
    for (const auto& t : tests) {
        const int before = g_failed;
        t.fn();
        const bool ok = g_failed == before;
        failedTests += ok ? 0 : 1;
        std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", t.name);
    }
    std::printf("%d of %d tests passed (%d checks, %d failed)\n",
                static_cast<int>(std::size(tests)) - failedTests, static_cast<int>(std::size(tests)),
                g_checks, g_failed);
    return failedTests == 0 ? 0 : 1;
}
