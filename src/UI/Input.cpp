#include "UI/Input.h"

#include "Config.h"
#include "UI/Overlay.h"
#include "UI/ShopWindow.h"
#include "UI/SkillTreeWindow.h"
#include "UI/SystemWindow.h"

#include <imgui.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Isekai::UI {

    namespace {
        // Skyrim's mouse id codes: buttons count up from 0, the wheel sits above them.
        constexpr std::uint32_t kMouseLeft = 0;
        constexpr std::uint32_t kMouseRight = 1;
        constexpr std::uint32_t kMouseMiddle = 2;
        constexpr std::uint32_t kMouseWheelUp = 8;
        constexpr std::uint32_t kMouseWheelDown = 9;

        // The game only ever gives us relative motion, so we keep the cursor
        // position ourselves. Feels roughly like the vanilla menu cursor.
        constexpr float kCursorSpeed = 1.6f;

        // Left stick -> cursor, per event. The stick reports a position, not a motion,
        // so this is a speed: how far the cursor travels while the stick is held all the
        // way over. The deadzone is what stops a worn stick from drifting the cursor
        // across the screen on its own.
        constexpr float kStickSpeed = 13.0f;
        constexpr float kStickDeadzone = 0.18f;

        // Hotkeys are keyed by DEVICE and code, never by code alone. A gamepad button
        // arrives as its XInput mask — D-pad up is 0x0001, Start is 0x0010 — and those
        // collide head-on with keyboard scan codes, where 0x0001 is ESCAPE and 0x0010 is
        // Q. One map keyed by the bare code would quietly make "D-pad up" and "ESC" the
        // same hotkey, and whichever registered last would win.
        [[nodiscard]] constexpr std::uint32_t HotkeyId(RE::INPUT_DEVICE a_device,
                                                       std::uint32_t    a_code) {
            return (static_cast<std::uint32_t>(a_device) << 16) | (a_code & 0xFFFFu);
        }

        // A gamepad button in the same encoding, for callers that speak XInput masks.
        [[nodiscard]] constexpr std::uint32_t PadId(std::uint32_t a_button) {
            return HotkeyId(RE::INPUT_DEVICE::kGamepad, a_button);
        }

        // B closes our panels, the way ESC and Tab do on a keyboard.
        constexpr std::uint32_t kPadB = 0x2000;

        // A HotkeyId back in words, for the log.
        [[nodiscard]] std::string HotkeyName(std::uint32_t a_id) {
            const auto code = a_id & 0xFFFFu;
            return (a_id >> 16) == static_cast<std::uint32_t>(RE::INPUT_DEVICE::kGamepad)
                       ? "pad " + Config::GamepadButtonName(code)
                       : Config::KeyName(code);
        }

        // Input events arrive on the main thread, ImGui is fed on the render
        // thread, so everything crossing that line sits behind this lock.
        struct Pending {
            float                                    dx = 0.0f;
            float                                    dy = 0.0f;
            float                                    wheel = 0.0f;
            std::vector<std::pair<int, bool>>        buttons;  // (ImGui button, down)
        };

        // (The old compile-time kLogKeyPresses lived here. It is
        // Config::LogInputDiagnostics() now — a build switch is useless for diagnosing
        // someone else's machine.)

        // The input diagnostic stops itself after this many presses. Enough to identify
        // the device and scan code a hotkey arrives under, which is all it is for, and
        // short enough that leaving the setting on cannot turn into a running record of
        // everything typed in game. It also keeps the log readable.
        constexpr int         kInputDiagnosticLimit = 200;
        std::atomic<int>      g_inputDiagnosticCount{ 0 };

        std::mutex g_mutex;
        Pending    g_pending;

        struct Hotkey {
            std::function<void()> fn;
            std::uint32_t         modifier = 0;  // HotkeyId that must be held; 0 = none
        };

        std::mutex                               g_hotkeyMutex;
        std::unordered_map<std::uint32_t, Hotkey> g_hotkeys;

        // Buttons currently held down, keyboard and gamepad alike, maintained from the
        // same event stream the hotkeys use — so modifier checks cannot drift from what
        // the game sees. Keyed by HotkeyId for the reason given above.
        std::mutex                        g_heldMutex;
        std::unordered_map<std::uint32_t, bool> g_held;

        [[nodiscard]] bool IsHeld(std::uint32_t a_id) {
            std::scoped_lock lock(g_heldMutex);
            const auto       it = g_held.find(a_id);
            return it != g_held.end() && it->second;
        }

        // Input events arrive on the game's input thread; hand the callback to the
        // main thread before it touches anything.
        void FireHotkey(std::uint32_t a_id) {
            std::function<void()> fn;
            {
                std::scoped_lock lock(g_hotkeyMutex);
                const auto       it = g_hotkeys.find(a_id);
                if (it == g_hotkeys.end()) {
                    return;
                }
                if (it->second.modifier != 0 && !IsHeld(it->second.modifier)) {
                    return;
                }
                fn = it->second.fn;
            }
            if (auto* task = SKSE::GetTaskInterface()) {
                task->AddTask([fn = std::move(fn)]() { fn(); });
            }
        }

        class InputSink : public RE::BSTEventSink<RE::InputEvent*> {
        public:
            static InputSink* GetSingleton() {
                static InputSink singleton;
                return std::addressof(singleton);
            }

            RE::BSEventNotifyControl ProcessEvent(
                RE::InputEvent* const*             a_event,
                RE::BSTEventSource<RE::InputEvent*>*) override {
                if (!a_event) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                // Hotkeys work whether or not a panel is open, so they are handled
                // before the capture check below.
                for (auto* event = *a_event; event; event = event->next) {
                    if (event->GetEventType() != RE::INPUT_EVENT_TYPE::kButton) {
                        continue;
                    }
                    auto* button = event->AsButtonEvent();
                    if (!button) {
                        continue;
                    }

                    // DIAGNOSTIC, before the device filter on purpose. When a hotkey does
                    // not fire, the log otherwise cannot separate "no input event arrives
                    // at all" from "one arrives under a device we ignore" — and the second
                    // is real: Skyrim VR delivers controller buttons as kVRRight (5) /
                    // kVRLeft (6), which the keyboard-only path below drops silently.
                    // Switched from the ini so a tester needs no special build.
                    //
                    // Device and scan code only — nothing here or anywhere else turns a
                    // scan code into a character. It stops after kInputDiagnosticLimit so
                    // that forgetting the setting cannot leave it recording.
                    if (button->IsDown() && Config::LogInputDiagnostics()) {
                        const int n = g_inputDiagnosticCount.fetch_add(1) + 1;
                        if (n <= kInputDiagnosticLimit) {
                            logger::info("input: device={} scanCode={:#x} ({})",
                                         static_cast<int>(event->GetDevice()),
                                         button->GetIDCode(),
                                         event->GetDevice() == RE::INPUT_DEVICE::kKeyboard
                                             ? "keyboard — hotkeys see this"
                                             : "NOT keyboard — hotkeys ignore this");
                            if (n == kInputDiagnosticLimit) {
                                logger::info("input: diagnostic limit reached ({} presses) — "
                                             "no further input will be logged this session",
                                             kInputDiagnosticLimit);
                            }
                        }
                    }

                    const auto device = event->GetDevice();
                    if (device != RE::INPUT_DEVICE::kKeyboard &&
                        device != RE::INPUT_DEVICE::kGamepad) {
                        continue;
                    }
                    const std::uint32_t id = HotkeyId(device, button->GetIDCode());

                    // Held-button bookkeeping first, so a modifier registers before the
                    // button it modifies is evaluated in the same batch.
                    {
                        std::scoped_lock lock(g_heldMutex);
                        g_held[id] = button->IsPressed();
                    }

                    if (!button->IsDown()) {
                        continue;
                    }

                    FireHotkey(id);
                }

                if (!IsCapturingInput()) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                std::scoped_lock lock(g_mutex);

                for (auto* event = *a_event; event; event = event->next) {
                    // Gamepad: the left stick drives our cursor and A is a left click.
                    //
                    // Deliberately a cursor rather than ImGui's gamepad focus navigation.
                    // Focus nav only reaches widgets, and the skill tree is a pannable
                    // canvas with no widgets in it at all — a controller player would have
                    // got the panels and not the tree. A cursor covers every screen we
                    // have with one mechanism.
                    if (event->GetDevice() == RE::INPUT_DEVICE::kGamepad) {
                        if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kThumbstick) {
                            auto* stick = static_cast<RE::ThumbstickEvent*>(event);
                            if (stick->IsLeft()) {
                                const float x = stick->xValue;
                                const float y = stick->yValue;
                                if (std::abs(x) > kStickDeadzone) {
                                    g_pending.dx += x * kStickSpeed;
                                }
                                if (std::abs(y) > kStickDeadzone) {
                                    g_pending.dy -= y * kStickSpeed;  // stick up is +y
                                }
                            }
                        } else if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton) {
                            if (auto* button = event->AsButtonEvent();
                                button && button->GetIDCode() == 0x1000) {  // A
                                g_pending.buttons.emplace_back(ImGuiMouseButton_Left,
                                                               button->IsPressed());
                            }
                        }
                        continue;
                    }

                    if (event->GetDevice() != RE::INPUT_DEVICE::kMouse) {
                        continue;
                    }

                    switch (event->GetEventType()) {
                    case RE::INPUT_EVENT_TYPE::kMouseMove:
                        if (auto* move = static_cast<RE::MouseMoveEvent*>(event)) {
                            g_pending.dx += static_cast<float>(move->mouseInputX) * kCursorSpeed;
                            g_pending.dy += static_cast<float>(move->mouseInputY) * kCursorSpeed;
                        }
                        break;

                    case RE::INPUT_EVENT_TYPE::kButton:
                        if (auto* button = event->AsButtonEvent()) {
                            switch (button->GetIDCode()) {
                            case kMouseLeft:
                                g_pending.buttons.emplace_back(ImGuiMouseButton_Left,
                                                               button->IsPressed());
                                break;
                            case kMouseRight:
                                g_pending.buttons.emplace_back(ImGuiMouseButton_Right,
                                                               button->IsPressed());
                                break;
                            case kMouseMiddle:
                                g_pending.buttons.emplace_back(ImGuiMouseButton_Middle,
                                                               button->IsPressed());
                                break;
                            case kMouseWheelUp:
                                g_pending.wheel += 1.0f;
                                break;
                            case kMouseWheelDown:
                                g_pending.wheel -= 1.0f;
                                break;
                            default:
                                break;
                            }
                        }
                        break;

                    default:
                        break;
                    }
                }

                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            InputSink() = default;
        };
    }

    namespace {
        // Belt to the guard's braces. Consuming the key in the handler chain has not
        // reliably kept the journal shut (tried: front of the chain, back of the
        // chain, swallowing until key release) — so this attacks from the other end:
        // any journal/tween menu that opens while a System panel is up, or within a
        // short window after one was dismissed by ESC, is immediately closed again.
        // Menu open/close is observable regardless of input plumbing semantics.
        std::atomic<std::int64_t> g_suppressMenusUntilMs{ 0 };

        [[nodiscard]] std::int64_t NowMs() {
            using namespace std::chrono;
            return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        }

        void ArmMenuSuppressor() {
            g_suppressMenusUntilMs.store(NowMs() + 400, std::memory_order_release);
        }

        class MenuSuppressor : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
        public:
            static MenuSuppressor* GetSingleton() {
                static MenuSuppressor singleton;
                return std::addressof(singleton);
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent* a_event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
                if (!a_event || !a_event->opening) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                const bool guarded =
                    IsCapturingInput() ||
                    NowMs() < g_suppressMenusUntilMs.load(std::memory_order_acquire);
                if (!guarded) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (a_event->menuName == RE::JournalMenu::MENU_NAME ||
                    a_event->menuName == RE::TweenMenu::MENU_NAME) {
                    if (auto* queue = RE::UIMessageQueue::GetSingleton()) {
                        queue->AddMessage(a_event->menuName, RE::UI_MESSAGE_TYPE::kHide, nullptr);
                        logger::info("UI: suppressed {} under a System panel",
                                     a_event->menuName.c_str());
                    }
                }
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            MenuSuppressor() = default;
        };

        // Sits at the front of MenuControls' handler chain and eats button events
        // while one of our panels is open. blockPlayerInput only silences the
        // player-control handlers — the journal/tween menu opens through THIS chain,
        // which is why ESC used to punch straight through the System panel into
        // Skyrim's own menu. ESC and Tab dismiss our panel instead.
        class MenuGuard : public RE::MenuEventHandler {
        public:
            static MenuGuard* GetSingleton() {
                static MenuGuard singleton;
                return std::addressof(singleton);
            }

            bool CanProcess(RE::InputEvent* a_event) override {
                if (!a_event || a_event->GetEventType() != RE::INPUT_EVENT_TYPE::kButton) {
                    return false;
                }
                return IsCapturingInput() || s_swallowDismissKey;
            }

            bool ProcessButton(RE::ButtonEvent* a_event) override {
                constexpr std::uint32_t kEsc = 0x01;  // DIK_ESCAPE
                constexpr std::uint32_t kTab = 0x0F;  // DIK_TAB
                const bool dismissKey =
                    (a_event->GetDevice() == RE::INPUT_DEVICE::kKeyboard &&
                     (a_event->GetIDCode() == kEsc || a_event->GetIDCode() == kTab)) ||
                    // B is "back" everywhere else in the game; a controller player will
                    // press it to leave our panel too.
                    (a_event->GetDevice() == RE::INPUT_DEVICE::kGamepad &&
                     a_event->GetIDCode() == kPadB);

                if (IsCapturingInput()) {
                    if (dismissKey && a_event->IsDown()) {
                        if (IsSkillTreeOpen()) {
                            DismissSkillTree();
                        } else if (IsShopWindowOpen()) {
                            DismissShopWindow();
                        } else {
                            DismissSystemWindow();
                        }
                        // The panel closes on the key's DOWN — but Skyrim's journal
                        // listens for its release. By the time the UP arrives, we no
                        // longer capture, the up sailed through, and the journal opened
                        // "right as the System menu closed". So the key that dismissed
                        // a panel stays swallowed until it is actually let go.
                        s_swallowDismissKey = true;
                        ArmMenuSuppressor();
                    }
                    return true;  // consumed: nothing may fire underneath our panel
                }

                if (s_swallowDismissKey && dismissKey) {
                    if (a_event->IsUp()) {
                        s_swallowDismissKey = false;
                    }
                    return true;  // still draining the dismissing key
                }
                return false;  // not ours — let the chain have it
            }

        private:
            MenuGuard() = default;

            static inline bool s_swallowDismissKey = false;
        };
    }

    void RegisterHotkey(std::uint32_t a_scanCode, std::function<void()> a_fn,
                        std::uint32_t a_modifier) {
        // Scan code 0 is not a key. Callers use 0 to mean "this feature is switched off"
        // (SelfTestKey), and registering it left a dead entry in the map that looked, in
        // the log and in this table, exactly like an armed hotkey.
        if (a_scanCode == 0) {
            logger::warn("UI: ignoring a hotkey registered on scan code 0 — that is never "
                         "a key, and 0 is how the ini spells \"off\"");
            return;
        }

        // ESC and Tab belong to our own panels (MenuGuard dismisses on both), so a hotkey
        // bound to either fires every time a panel is closed. This is not hypothetical:
        // the ini is otherwise full of 0/1 switches, so "SelfTestKey = 1" reads as "on"
        // and is in fact scan code 1 — ESCAPE. The self-test then ran on every ESC, and
        // the key the player believed they had set did nothing.
        constexpr std::uint32_t kEsc = 0x01;
        constexpr std::uint32_t kTab = 0x0F;
        if (a_scanCode == kEsc || a_scanCode == kTab) {
            logger::warn("UI: refusing to bind a hotkey to {} — our panels already use it "
                         "to close. If you meant \"switch this on\", the setting wants a KEY "
                         "(e.g. F11), not 1.",
                         Config::KeyName(a_scanCode));
            return;
        }
        std::scoped_lock lock(g_hotkeyMutex);
        g_hotkeys[HotkeyId(RE::INPUT_DEVICE::kKeyboard, a_scanCode)] = Hotkey{
            std::move(a_fn),
            a_modifier == 0 ? 0 : HotkeyId(RE::INPUT_DEVICE::kKeyboard, a_modifier)
        };
    }

    void RegisterGamepadHotkey(std::uint32_t a_button, std::function<void()> a_fn,
                               std::uint32_t a_modifier) {
        if (a_button == 0) {
            logger::info("UI: no gamepad hotkey (button = 0)");
            return;
        }
        // Same reasoning as ESC and Tab on the keyboard: B closes our panels, so a
        // hotkey on B would re-open whatever the player just closed.
        if (a_button == kPadB) {
            logger::warn("UI: refusing to bind a gamepad hotkey to B — our panels already "
                         "use it to close");
            return;
        }
        std::scoped_lock lock(g_hotkeyMutex);
        g_hotkeys[PadId(a_button)] = Hotkey{ std::move(a_fn),
                                             a_modifier == 0 ? 0 : PadId(a_modifier) };
    }

    void LogHotkeys() {
        std::scoped_lock lock(g_hotkeyMutex);
        if (g_hotkeys.empty()) {
            logger::warn("UI: no hotkeys armed at all");
            return;
        }
        std::string list;
        for (const auto& [id, hk] : g_hotkeys) {
            list += (list.empty() ? "" : ", ") + HotkeyName(id);
            if (hk.modifier != 0) {
                list += " + " + HotkeyName(hk.modifier);
            }
        }
        logger::info("UI: {} hotkey(s) armed: {}", g_hotkeys.size(), list);
    }

    void InstallInput() {
        if (auto* manager = RE::BSInputDeviceManager::GetSingleton()) {
            manager->AddEventSink(InputSink::GetSingleton());
            logger::info("UI: hooked Skyrim's input event stream");
        } else {
            logger::error("UI: no input device manager — the overlay will not take clicks");
        }

        if (auto* ui = RE::UI::GetSingleton()) {
            ui->AddEventSink<RE::MenuOpenCloseEvent>(MenuSuppressor::GetSingleton());
        }

        if (auto* menuControls = RE::MenuControls::GetSingleton()) {
            menuControls->AddHandler(MenuGuard::GetSingleton());

            // AddHandler appends, and the chain stops at the FIRST handler that
            // consumes an event — appended last, we ran after the journal handler and
            // ESC closed our panel *and* opened Skyrim's menu on the same press.
            // Rotate ourselves to the front so the guard sees every button first.
            auto& handlers = menuControls->handlers;
            if (!handlers.empty() && handlers.back() == MenuGuard::GetSingleton()) {
                for (std::size_t i = handlers.size() - 1; i > 0; --i) {
                    handlers[i] = handlers[i - 1];
                }
                handlers[0] = MenuGuard::GetSingleton();
            }
            logger::info("UI: menu guard armed at the front of the chain ({} handlers)",
                         handlers.size());
        }
    }

    void FeedImGui(ImGuiIO& a_io) {
        // Render-thread owned: the cursor only exists while we are drawing.
        static float cursorX = 0.0f;
        static float cursorY = 0.0f;
        static bool  wasCapturing = false;

        const bool capturing = IsCapturingInput();

        // Drop the cursor in the middle of the screen each time a panel opens,
        // instead of wherever it happened to be left last time.
        if (capturing && !wasCapturing) {
            cursorX = a_io.DisplaySize.x * 0.5f;
            cursorY = a_io.DisplaySize.y * 0.5f;
            std::scoped_lock lock(g_mutex);
            g_pending = Pending{};
        }
        wasCapturing = capturing;

        if (!capturing) {
            a_io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);  // park it off-screen
            return;
        }

        Pending pending;
        {
            std::scoped_lock lock(g_mutex);
            pending = std::move(g_pending);
            g_pending = Pending{};
        }

        cursorX = std::clamp(cursorX + pending.dx, 0.0f, a_io.DisplaySize.x);
        cursorY = std::clamp(cursorY + pending.dy, 0.0f, a_io.DisplaySize.y);

        a_io.AddMousePosEvent(cursorX, cursorY);
        for (const auto& [button, down] : pending.buttons) {
            a_io.AddMouseButtonEvent(button, down);
        }
        if (pending.wheel != 0.0f) {
            a_io.AddMouseWheelEvent(0.0f, pending.wheel);
        }
    }
}
