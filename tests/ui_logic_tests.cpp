// Unit tests for src/UI/UiLogic.h: the engine-free logic of the Magelight UI
// (tones, verdicts, action parsing, ready gate, decision re-offer, label push gate),
// without Skyrim. Run with tools/run-unit-tests.ps1.

#include "../src/UI/UiLogic.h"

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
        { "ReadyGateQueues", ReadyGateQueues },
        { "ReadyGateDropsOldest", ReadyGateDropsOldest },
        { "ReadyGateReset", ReadyGateReset },
        { "DecisionReoffer", DecisionReoffer },
        { "DecisionNothingPending", DecisionNothingPending },
        { "DecisionOfferSemantics", DecisionOfferSemantics },
        { "LabelPushDecisions", LabelPushDecisions },
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
