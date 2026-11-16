#include "UI/Screens.h"

#include "Config.h"
#include "IsekaiBuildId.h"
#include "Loc.h"
#include "Progression.h"  // StatusJson, for nav back to the status screen and the reroll re-push
#include "Quests.h"       // Reroll, the status screen's "new task"
#include "Shop.h"         // Open, Buy
#include "SkillTree.h"    // TryUnlock, Respec
#include "Sounds.h"
#include "Storage.h"  // Open, the status screen's storage button
#include "System.h"   // RebootSystem
#include "UI/Payloads.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <utility>

// Threading: every page message and host event reaches this file on the game thread (the
// host wrapper posts them, Magelight.h), and every Show* caller is on the main thread too,
// so the state below is game-thread only. The atomics are the part other threads read
// (the input guards, DismissSystemWindow).
//
// "Requested" and "entered" are kept apart: g_open says a screen is wanted (UI mode asked
// for, maybe still queued behind another mod), g_entered that the host has entered it and
// the screen is on display. The input guards and Escape key on entered: a screen queued
// behind another mod's view owns no keys, and a reward panel nobody has seen yet must not
// be answered by an Escape meant for that other view.
//
// UI mode bookkeeping (Logic::OwnExits). Every UI-mode period we enter ends in exactly one
// UIModeExited. An exit we caused (Close, after the period was entered) is counted and
// swallowed, and a screen opened while one is still owed asks for UI mode only once it
// arrived: the host runs our release later, and would end a request made before that. Any
// other exit while a screen is wanted means UI mode was taken from us (Escape on a screen
// that leaves it to Magelight, another mod, a load boundary), and the screen's loss rule
// runs (Lost). A refused request is the same loss without the exit.
namespace Isekai::UI {

    namespace {
        using Logic::Action;

        enum class Screen : std::uint8_t { None, Panel, Status, Tree, Shop };

        constexpr const char* ScreenName(Screen a_screen) {
            switch (a_screen) {
                case Screen::Panel: return "panel";
                case Screen::Status: return "status";
                case Screen::Tree: return "tree";
                case Screen::Shop: return "shop";
                case Screen::None: break;
            }
            return "none";
        }

        constexpr bool IsDataScreen(Screen a_screen) {
            return a_screen == Screen::Status || a_screen == Screen::Tree || a_screen == Screen::Shop;
        }

        // Fast enough that no body finishes anything but instantly, without being an
        // infinity that arithmetic downstream has to survive.
        constexpr float kInstantReveal = 1.0e6f;

        // A panel shown, or a decision waiting for its answer.
        struct PanelState {
            std::string              json;  // the page payload (Payloads::Panel)
            std::function<void(int)> onSelect;
            std::size_t              choices = 0;
            int                      dismissWith = -1; // what Escape, B or a loss answer: the cancel button, else the only text button
            bool                     decision = false; // more than one text button and no cancel one
            std::uint32_t            id = 0;           // PendingDecision id
        };

        std::atomic<Screen>        g_screen{ Screen::None };  // the screen on the page (or about to be)
        std::atomic<bool>          g_open{ false };          // UI mode asked for and not closed or lost
        std::atomic<bool>          g_decisionWaiting{ false };  // g_decision.Current(), for other threads
        std::atomic<std::uint32_t> g_shown{ 0 };             // bumps with every isekai.show
        std::uint32_t              g_requests = 0;           // bumps with every RequestOpen (game thread)
        std::atomic<float>         g_pushedScale{ 0.0f };    // the UiScale the pages last got
        // A panel callback is running with the view still held: until it shows something,
        // the guards read "nothing open", as they did when the old panels closed first.
        std::atomic<bool>          g_handoff{ false };

        std::optional<PanelState> g_panel;
        Logic::PendingDecision    g_decision;
        std::uint32_t             g_panelIds = 0;
        std::atomic<bool>         g_entered{ false };      // UIModeEntered seen since the last exit (HoldsInput)
        Logic::OwnExits           g_ownExits;              // exits our releases still owe, a request waiting on them
        bool                      g_autoReoffered = false; // the one automatic re-offer is spent
        std::uint32_t             g_notedDecision = 0;     // the panel id the "choice is waiting" note was shown for
        bool                      g_holdNote = false;      // the hotkey offers the decision itself right after
        std::string               g_lastShow;              // re-sent to a system page that reloaded
        bool                      g_reloaded = false;      // the system page reloaded; its next ready re-shows, once
        bool                      g_showQueued = false;    // a show waits in the gate for the next ready
        std::chrono::steady_clock::time_point g_openedAt;   // the last RequestOpen

        void SyncDecisionFlag() { g_decisionWaiting.store(g_decision.Current().has_value()); }

        // Invalid UTF-8 (a plugin's Windows-1252 quest name, say) would make the whole
        // payload unparseable; each bad byte becomes U+FFFD instead.
        std::string CleanUtf8(std::string_view a_in) {
            std::string out;
            out.reserve(a_in.size());
            const auto* s = reinterpret_cast<const unsigned char*>(a_in.data());
            const std::size_t n = a_in.size();
            for (std::size_t i = 0; i < n;) {
                const unsigned char c = s[i];
                std::size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
                bool ok = len > 0 && i + len <= n && !(len == 2 && c < 0xC2) && !(len == 4 && c > 0xF4);
                for (std::size_t k = 1; ok && k < len; ++k) ok = (s[i + k] & 0xC0) == 0x80;
                if (ok && len == 3) {
                    const unsigned char d = s[i + 1];
                    ok = !(c == 0xE0 && d < 0xA0) && !(c == 0xED && d >= 0xA0);
                }
                if (ok && len == 4) {
                    const unsigned char d = s[i + 1];
                    ok = !(c == 0xF0 && d < 0x90) && !(c == 0xF4 && d >= 0x90);
                }
                if (!ok) {
                    out += "\xEF\xBF\xBD";
                    ++i;
                    continue;
                }
                out.append(a_in.data() + i, len);
                i += len;
            }
            return out;
        }

        // A payload re-serialised through nlohmann::json: the host wrapper takes JS built
        // from dump() output only. nullopt for text that is not JSON.
        std::optional<std::string> Normalise(std::string_view a_json) {
            auto j = nlohmann::json::parse(CleanUtf8(a_json), nullptr, /*allow_exceptions=*/false);
            if (j.is_discarded()) return std::nullopt;
            return j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        }

        // isekai.show(<screen>, <payload>) to the system page. False = not JSON (logged).
        bool Push(Screen a_screen, std::string_view a_json) {
            const auto payload = Normalise(a_json);
            if (!payload) {
                logger::error("Screens: the {} payload is not valid JSON, not shown", ScreenName(a_screen));
                return false;
            }
            std::string js = "isekai.show(" + nlohmann::json(ScreenName(a_screen)).dump() + "," + *payload + ")";
            g_lastShow = js;
            // Not ready (first load, or a reload): the gate delivers this one on ready.
            if (!Magelight::PageReady(Magelight::View::System)) g_showQueued = true;
            g_screen.store(a_screen);
            g_shown.fetch_add(1);
            Magelight::Run(Magelight::View::System, std::move(js));
            return true;
        }

        void Lost(bool a_exited, std::string_view a_why);

        // Tells the page its view is no longer open, so the next show plays the open
        // animation and starts the screen fresh (Escape or pad B can end UI mode without the
        // page having asked). Idempotent on the page.
        void SendClosed() { Magelight::Run(Magelight::View::System, "isekai.closed()"); }

        // "0x1f (S)" -> "S": the name a key carries in Config's tables, else its code.
        std::string KeyLabel(const std::string& a_name) {
            const auto open = a_name.find('(');
            const auto close = a_name.rfind(')');
            if (open == std::string::npos || close == std::string::npos || close <= open + 1) return a_name;
            return a_name.substr(open + 1, close - open - 1);
        }

        // The system-menu key as the player presses it: the VR controller button in VR, then
        // keyboard, then pad ("RSHIFT + S / LB + BACK"), each with its modifier.
        std::string MenuKeyText() {
            std::string out;
            if (REL::Module::IsVR()) {
                if (const auto vr = Config::VRMenuButton()) {
                    out = LF("ui.vrMenuButton", "VR controller button {}", std::format("0x{:X}", vr));
                }
            }
            if (const auto key = Config::SystemMenuKey()) {
                const auto mod = Config::SystemMenuModifier();
                if (!out.empty()) out += " / ";
                out += (mod ? KeyLabel(Config::KeyName(mod)) + " + " : std::string{}) + KeyLabel(Config::KeyName(key));
            }
            if (const auto pad = Config::GamepadMenuButton()) {
                const auto mod = Config::GamepadMenuModifier();
                if (!out.empty()) out += " / ";
                out += (mod ? KeyLabel(Config::GamepadButtonName(mod)) + " + " : std::string{}) +
                       KeyLabel(Config::GamepadButtonName(pad));
            }
            return out.empty() ? std::string(L("ui.menuKeyUnbound", "the System menu key")) : out;
        }

        // A decision lost UI mode and is not offered again by itself: say so once per
        // decision, with a native HUD note (our own hud may be what went away), so it does
        // not wait unseen.
        void NoteWaitingDecision() {
            if (!g_panel || g_holdNote || g_notedDecision == g_panel->id) return;
            g_notedDecision = g_panel->id;
            if (!Available()) {
                ShowNeedsMagelight();  // the overlay is gone: no key would bring it back
                return;
            }
            const std::string note =
                LF("ui.decisionWaiting", "[ SYSTEM ] A choice is waiting — press {}", MenuKeyText());
            RE::SendHUDMessage::ShowHUDMessage(note.c_str());
        }

        // RequestOpen (pause and queue). A request the host turns down at once is handled
        // as a loss on the next task, never inside the caller's Show*: a single-button panel
        // answers itself there, and its callback must not run under its own ShowSystemWindow.
        // Keyed on the request, not the show: a Push in between asks for nothing new, and
        // its screen still has no UI mode.
        void Request() {
            const auto request = ++g_requests;
            if (Magelight::RequestOpen()) return;
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([request]() {
                    if (g_open.load() && !g_entered && g_requests == request) {
                        Lost(false, "the host turned the request down");
                        if (!g_open.load()) SendClosed();  // no exit follows to tell the page
                    }
                });
            }
        }

        // UI mode for the screen just pushed, unless it is already held or asked for.
        void Open() {
            if (g_open.exchange(true)) return;
            g_openedAt = std::chrono::steady_clock::now();
            if (g_ownExits.Open()) {
                Request();
            } else {
                logger::info("Screens: the {} screen waits for the exit of our own release", ScreenName(g_screen.load()));
            }
        }

        // Leave UI mode ourselves. The page fades the screen out on its own.
        void Close() {
            g_screen.store(Screen::None);
            if (!g_open.exchange(false)) return;
            ++g_requests;  // a refusal still on its way belongs to the screen closed here
            const bool entered = g_entered.exchange(false);
            g_ownExits.Close(entered);
            Magelight::Close();
            // Not entered: the request is withdrawn (or its entry is released as a stale
            // one), and no exit of a period we entered tells the page.
            if (!entered) SendClosed();
        }

        // The panel's answer, with the sound rule the old panels had (choosing
        // clicks, dismissing a single button whooshes, after the unpause), then the
        // callback. A callback that shows the next screen (a chain of panels) gets the view
        // as it is, still in UI mode; only when it shows nothing does the view let go.
        // Releasing first and asking again would race the release: the host runs a
        // ReleaseUIMode later, and a request made before it would be ended by it.
        void Answer(int a_index) {
            if (!g_panel) return;
            PanelState p = std::move(*g_panel);
            g_panel.reset();
            g_decision.Resolve(p.id);
            g_autoReoffered = false;
            SyncDecisionFlag();
            g_screen.store(Screen::None);
            Sounds::PlayDelayed(p.choices > 1 ? Sounds::Sfx::ButtonClick : Sounds::Sfx::WindowClose,
                                Sounds::kResumeGraceMs);
            const auto shown = g_shown.load();
            if (p.onSelect) {
                struct Handoff {
                    Handoff() { g_handoff.store(true); }
                    ~Handoff() { g_handoff.store(false); }
                } handoff;
                p.onSelect(a_index);
            }
            if (g_shown.load() == shown) Close();
        }

        // Show the waiting decision panel again.
        void Reoffer() {
            if (!g_panel) return;
            g_decision.Offer(g_panel->id);
            SyncDecisionFlag();
            if (!Push(Screen::Panel, g_panel->json)) return;
            logger::info("Screens: offering the waiting decision again");
            Sounds::Play(Sounds::Sfx::WindowOpen);
            Open();
        }

        // UI mode ended without an answer (a_exited), or the request was refused.
        void Lost(bool a_exited, std::string_view a_why) {
            g_open.store(false);
            g_entered = false;
            g_ownExits.CancelDeferred();
            const Screen s = g_screen.exchange(Screen::None);
            if (s != Screen::Panel || !g_panel) {
                if (s != Screen::None) {
                    logger::info("Screens: {} closed ({})", ScreenName(s), a_why);
                    Sounds::PlayDelayed(Sounds::Sfx::WindowClose, Sounds::kResumeGraceMs);
                }
                return;
            }
            if (g_panel->decision) {
                g_decision.OnUiModeLost();
                SyncDecisionFlag();
                logger::info("Screens: a decision panel lost UI mode ({}); it waits for the system-menu key", a_why);
                // Once by itself (queued behind whoever holds UI mode); after that only
                // the hotkey, so a host that keeps ending UI mode cannot loop us.
                if (a_exited && g_decision.NeedsReoffer() && !g_autoReoffered && Available()) {
                    g_autoReoffered = true;
                    SendClosed();  // the re-offer opens fresh, with its flicker-on
                    Reoffer();
                    return;
                }
                NoteWaitingDecision();
                return;
            }
            if (g_panel->dismissWith >= 0) {
                logger::info("Screens: a panel lost UI mode ({}); answered with its dismiss button", a_why);
                Answer(g_panel->dismissWith);
                return;
            }
            g_panel.reset();  // icon buttons only: nothing to answer with
        }

        // A data screen (status, tree, shop) to the page, and UI mode if not yet held.
        void ShowData(Screen a_screen, std::string_view a_json) {
            if (!Available()) {
                ShowNeedsMagelight();
                return;
            }
            if (g_panel && g_screen.load() == Screen::Panel) {
                // Callers guard on IsSystemScreenOpen; this is the fault case.
                logger::warn("Screens: the {} screen replaces an open panel", ScreenName(a_screen));
                if (g_panel->decision) {
                    g_decision.OnUiModeLost();  // kept for the hotkey
                    NoteWaitingDecision();
                } else {
                    g_panel.reset();
                }
            }
            if (!Push(a_screen, a_json)) return;
            Sounds::Play(Sounds::Sfx::WindowOpen);
            Open();
        }

        // The same screen with new numbers (a purchase, a respec): no sound, no new open.
        void Refresh(Screen a_screen, std::string_view a_json) {
            if (g_screen.load() == a_screen && g_open.load()) Push(a_screen, a_json);
        }

        // Closing a data screen from the page or a key.
        void CloseData() {
            Close();
            Sounds::PlayDelayed(Sounds::Sfx::WindowClose, Sounds::kResumeGraceMs);
        }

        float RevealRate(float a_charsPerSec) {
            const float speed = Config::TextSpeed();
            const float rate = speed <= 0.0f ? kInstantReveal : a_charsPerSec * speed;
            return std::clamp(rate, 1.0f, kInstantReveal);
        }

        // One log line per unknown action type, so a page that spams one cannot fill the log.
        void LogBadAction(std::string_view a_raw) {
            static std::set<std::string> seen;
            const auto type = std::string(a_raw.substr(0, std::min<std::size_t>(a_raw.find(':'), 32)));
            if (seen.size() >= 32 || !seen.insert(type).second) return;
            logger::warn("Screens: the page sent an action the plugin does not take: '{}'", CleanUtf8(a_raw.substr(0, 64)));
        }

        // Reboot undoes a blessing's grants, so it asks first and says what it keeps and
        // what it takes back. In red (spec 7.2: a warning); it swaps in over the status
        // screen. REBOOT re-opens the blessing choice, whose panel takes the view over;
        // CANCEL goes back to the status screen. Not a binding decision: Escape, pad B and
        // a lost view all cancel, so only an explicit REBOOT reboots.
        void ConfirmReboot() {
            ShowSystemWindow(
                L("window.system", "[ SYSTEM ]"),
                L("reboot.confirm",
                  "REBOOT THE SYSTEM?\n"
                  "\n"
                  "Re-opens the blessing choice, so you can pick a different path\n"
                  "(tier, Full/Shattered, Dormant).\n"
                  "\n"
                  "KEPT:    milestones, System Points, storage. The skill\n"
                  "         tree is refunded, not lost — every point spent\n"
                  "         on it comes back to spend again.\n"
                  "UNDONE:  the skills and attributes a previous blessing\n"
                  "         granted — you return to the mortal you were\n"
                  "         when the System first bound itself. Your\n"
                  "         character level is left alone.\n"
                  "NOTE:    gold and perk points already spent stay spent.\n"
                  "         A character from before this update has no such\n"
                  "         record, and keeps what it was given."),
                std::vector<Choice>{ { .label = L("button.cancel", "CANCEL"), .cancel = true },
                                     { .label = L("button.reboot", "REBOOT") } },
                [](int a_choice) {
                    if (a_choice == 1) {
                        Isekai::RebootSystem();
                    } else if (g_open.load()) {  // a lost view cancels without coming back
                        ShowStatus(Progression::StatusJson());
                    }
                },
                45.0f, 720.0f, {}, Logic::Tone::Red);
        }

        void OnStatusAction(const std::string& a_word) {
            if (a_word == "tree") {
                ShowSkillTree();  // switches screens inside the view
            } else if (a_word == "storage") {
                // The container is a game menu, so the view lets go first.
                CloseData();
                Storage::Open();
            } else if (a_word == "shop") {
                Isekai::Shop::Open();  // its own guards, then ShowShop inside the view
            } else if (a_word == "reroll") {
                Quests::Reroll();
                Refresh(Screen::Status, Progression::StatusJson());
            } else if (a_word == "reboot") {
                ConfirmReboot();
            } else {  // "close"
                CloseData();
            }
        }
    }  // namespace

    void ShowSystemWindow(std::string a_title, std::string a_body, std::vector<Choice> a_choices,
                          std::function<void(int)> a_onSelect, float a_revealCharsPerSec, float a_widthHint,
                          Emblem a_emblem, Logic::Tone a_tone) {
        if (!Available()) {
            // The callback is dropped on purpose: a panel nobody can see must not leave a
            // decision behind. The reincarnation, which owns a one-shot flag, checks
            // Available() itself before it gets here.
            ShowNeedsMagelight();
            return;
        }
        PanelState p;
        p.choices = a_choices.size();
        std::size_t texts = 0;
        int         cancel = -1;
        for (std::size_t i = 0; i < a_choices.size(); ++i) {
            if (a_choices[i].cancel && cancel < 0) cancel = static_cast<int>(i);
            if (a_choices[i].iconOnly) continue;
            if (texts++ == 0) p.dismissWith = static_cast<int>(i);
        }
        if (texts != 1) p.dismissWith = -1;
        if (cancel >= 0) p.dismissWith = cancel;
        p.decision = texts > 1 && cancel < 0;
        p.id = ++g_panelIds;
        p.onSelect = std::move(a_onSelect);
        p.json = Payloads::Panel({ .title = std::move(a_title),
                                   .body = std::move(a_body),
                                   .choices = std::move(a_choices),
                                   .revealCharsPerSec = RevealRate(a_revealCharsPerSec),
                                   .widthHint = std::max(a_widthHint, 300.0f),
                                   .tone = a_tone,
                                   .emblemIcon = std::move(a_emblem.icon),
                                   .emblemCaption = std::move(a_emblem.caption),
                                   .emblemTone = a_emblem.tone });

        if (g_decision.Current()) {
            // Callers guard on IsSystemScreenOpen, which covers a waiting decision.
            logger::warn("Screens: a new panel replaces a decision that was still waiting for its answer");
        }
        g_decision.Reset();
        if (p.decision) g_decision.Offer(p.id);
        g_autoReoffered = false;
        g_panel = std::move(p);
        SyncDecisionFlag();
        if (!Push(Screen::Panel, g_panel->json)) {
            g_panel.reset();
            g_decision.Reset();
            SyncDecisionFlag();
            return;
        }
        Sounds::Play(Sounds::Sfx::WindowOpen);
        Open();
    }

    void ShowSystemWindow(std::string a_title, std::string a_body, std::vector<std::string> a_choices,
                          std::function<void(int)> a_onSelect, float a_revealCharsPerSec, float a_widthHint,
                          Logic::Tone a_tone) {
        std::vector<Choice> choices;
        choices.reserve(a_choices.size());
        for (auto& label : a_choices) choices.push_back({ std::move(label), {} });
        ShowSystemWindow(std::move(a_title), std::move(a_body), std::move(choices), std::move(a_onSelect),
                         a_revealCharsPerSec, a_widthHint, {}, a_tone);
    }

    void ShowSkillTree() { ShowData(Screen::Tree, Payloads::Tree()); }

    void ShowShop() { ShowData(Screen::Shop, Payloads::Shop()); }

    void ShowStatus(std::string a_json) { ShowData(Screen::Status, a_json); }

    bool IsSystemWindowOpen() {
        return (g_open.load() && g_screen.load() == Screen::Panel) || g_decisionWaiting.load();
    }

    bool IsSystemScreenOpen() {
        const bool up = g_open.load() && !(g_handoff.load() && g_screen.load() == Screen::None);
        return up || g_decisionWaiting.load();
    }

    bool HoldsInput() { return g_open.load() && g_entered.load() && Available(); }

    void DismissSystemWindow() {
        if (!HoldsInput()) return;
        const auto shown = g_shown.load();
        auto* task = SKSE::GetTaskInterface();
        if (!task) return;
        // Only the screen the key was pressed on: the page may have answered the same key
        // first, and the next panel of a chain must not be dismissed by it.
        task->AddTask([shown]() {
            if (!g_open.load() || !g_entered.load() || g_shown.load() != shown) return;
            const Screen s = g_screen.load();
            if (IsDataScreen(s)) {
                CloseData();
            } else if (s == Screen::Panel && g_panel) {
                if (g_panel->dismissWith >= 0) {
                    Answer(g_panel->dismissWith);
                } else if (!g_panel->decision) {
                    g_panel.reset();  // icon buttons only: a view, not a question
                    CloseData();
                }
                // A decision ignores it: Escape must not choose for the player.
            }
        });
    }

    bool Available() { return Magelight::Available(); }

    void ShowNeedsMagelight() {
        static bool logged = false;
        if (!logged) {
            logged = true;
            logger::warn("UI: Magelight UI is not available (not installed, older than 1.7.0, a page file "
                         "missing, or the overlay is off; the Magelight lines above say which), so Isekai "
                         "Hero shows no screens this session");
        }
        RE::SendHUDMessage::ShowHUDMessage(
            L("ui.needsMagelight", "[ SYSTEM ] Isekai Hero needs Magelight UI (1.7.0 or newer). The System will wait."));
    }

    bool ReofferPendingDecision() {
        if (!Available()) return false;
        // The hotkey only gets here with the game running, so a screen we think is up
        // while the host says we hold no UI mode, seconds after asking, lost it without
        // the exit event reaching us (a release that overtook a quick reopen). Recover
        // rather than leave every guard reading "a screen is open" until the next load.
        if (g_open.load() && !Magelight::HoldsUiMode() &&
            std::chrono::steady_clock::now() - g_openedAt > std::chrono::seconds(2)) {
            logger::warn("Screens: the {} screen lost UI mode without an exit event; recovering",
                         ScreenName(g_screen.load()));
            g_entered = false;
            g_ownExits.Reset();  // backstop: a count that leaked must not swallow the next real exit
            Magelight::Close();  // withdraws a request still queued behind another mod's UI mode
            g_holdNote = true;   // this key offers a waiting decision itself, just below
            Lost(/*a_exited=*/true, "UI mode gone without an exit event");
            g_holdNote = false;
            if (g_open.load()) return true;  // the decision was offered again by itself
            SendClosed();
        }
        if (!g_decision.Current() || !g_panel || g_open.load()) return false;
        Reoffer();
        return true;
    }

    namespace Screens {

        std::vector<std::string> SettingsCalls() {
            std::vector<std::string> out;
            if (auto loc = Normalise(Loc::Json())) {
                out.push_back("isekai.loc(" + *loc + ")");
            } else {
                logger::error("Screens: the translation table is not valid JSON, the pages stay English");
            }
            const float scale = Config::UiScale();
            g_pushedScale.store(scale);
            out.push_back("isekai.scale(" + nlohmann::json(scale).dump() + ")");
            return out;
        }

        void Install() {
            // The hud's prelude is Hud::Install's: these calls plus its own (SetPrelude
            // replaces, so only one owner per view).
            Magelight::SetPrelude(Magelight::View::System, &SettingsCalls);
        }

        void OnSettingsReloaded() {
            if (!Available()) return;
            const float scale = Config::UiScale();
            if (scale == g_pushedScale.load()) return;
            g_pushedScale.store(scale);
            const std::string js = "isekai.scale(" + nlohmann::json(scale).dump() + ")";
            Magelight::Run(Magelight::View::System, js);
            Magelight::Run(Magelight::View::Hud, js);
        }

        void OnGameLoaded() {
            const bool had = g_open.load() || g_panel.has_value();
            g_decision.Reset();
            g_autoReoffered = false;
            g_notedDecision = 0;
            Close();
            g_ownExits.CancelDeferred();
            g_panel.reset();
            SyncDecisionFlag();
            if (had) logger::info("Screens: a load dropped the screen or decision left from before it");
        }

    }  // namespace Screens

}  // namespace Isekai::UI

namespace Isekai::UI::Screens::detail {

    void OnAction(std::string_view a_raw) {
        const auto action = Logic::ParseAction(a_raw);
        if (!action) {
            LogBadAction(a_raw);
            return;
        }
        const Screen s = g_screen.load();
        switch (action->type) {
            case Action::Type::Choose:
                if (s != Screen::Panel || !g_panel || action->number >= g_panel->choices) {
                    logger::info("Screens: choose:{} with no such button on screen, ignored", action->number);
                    return;
                }
                Answer(static_cast<int>(action->number));
                return;
            case Action::Type::Buy:
                if (s != Screen::Tree) return;
                SkillTree::TryUnlock(action->number);
                Refresh(Screen::Tree, Payloads::Tree());
                return;
            case Action::Type::Respec:
                if (s != Screen::Tree) return;
                SkillTree::Respec();
                Refresh(Screen::Tree, Payloads::Tree());  // the refunded points, the cleared nodes
                return;
            case Action::Type::ShopBuy:
                if (s != Screen::Shop) return;
                // An index into the WHOLE catalog; Buy checks the range.
                Isekai::Shop::Buy(static_cast<int>(std::min<std::uint32_t>(action->number, 1u << 30)));
                Refresh(Screen::Shop, Payloads::Shop());
                return;
            case Action::Type::Status:
                if (s != Screen::Status) return;
                OnStatusAction(action->word);
                return;
            case Action::Type::Close:
                if (IsDataScreen(s)) CloseData();
                return;
            case Action::Type::Nav:
                if (!IsDataScreen(s)) return;
                if (action->word == "status") {
                    ShowStatus(Progression::StatusJson());
                } else if (action->word == "tree") {
                    ShowSkillTree();
                } else {
                    Isekai::Shop::Open();
                }
                return;
        }
    }

    void OnEntered() {
        if (!g_open.load()) {
            // An entry that ran just before we closed: Close released it, and the exit of
            // that release follows this event.
            g_ownExits.StaleEntry();
            logger::info("Screens: UI mode entered after the screen was closed; its exit is ours");
            return;
        }
        g_entered = true;
    }

    void OnExited(bool a_reopened) {
        g_entered = false;
        switch (g_ownExits.OnExited()) {
            case Logic::OwnExits::Exit::Ours:
                break;
            case Logic::OwnExits::Exit::OursRequestNow:
                // A screen opened while our release was still owed: its turn now.
                if (g_open.load()) Request();
                break;
            case Logic::OwnExits::Exit::Theirs:
                if (g_open.load() && !a_reopened) Lost(/*a_exited=*/true, "UI mode ended");
                break;
        }
        // The view is closed and nothing new was shown into it (a held screen, a re-offer
        // and a next panel all leave g_open set): the page lets go of the old screen.
        if (!a_reopened && !g_open.load()) SendClosed();
    }

    void OnRefused(std::string a_detail) {
        logger::info("Screens: opening the system view was refused: {}", CleanUtf8(a_detail));
        // Only the request of the screen we want now; an entered period is not refused.
        if (!g_open.load() || g_entered) return;
        Lost(/*a_exited=*/false, "refused");
        if (!g_open.load()) SendClosed();  // no exit follows a refusal to tell the page
    }

    void OnPageReady(Magelight::View a_view, std::string a_version) {
        const bool system = a_view == Magelight::View::System;
        if (!Logic::PageMatchesBuild(a_version, kUiBuildId)) {
            // Spec 6.4: logged here, reported by the self-test.
            logger::warn("Screens: the {} page is build '{}', this DLL is '{}': the page files come from "
                         "another build (a stale or hand-copied install)",
                         system ? "system" : "hud", CleanUtf8(std::string_view(a_version).substr(0, 64)),
                         kUiBuildId);
        }
        // A reload while a screen is up threw the page's state away: show it again, unless
        // a show made since the reload was queued and has just been delivered by the gate.
        // Once per reload: the page's own ready and its answer to isekai.hello() both land
        // here, and a second show would restart the panel's typewriter.
        if (!system) return;
        const bool delivered = std::exchange(g_showQueued, false);
        if (!std::exchange(g_reloaded, false)) return;
        if (!delivered && g_open.load() && !g_lastShow.empty()) {
            Magelight::Run(Magelight::View::System, g_lastShow);
        }
    }

    void OnPageReloaded() { g_reloaded = true; }

}  // namespace Isekai::UI::Screens::detail
