#include "UI/Input.h"

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

        // Input events arrive on the main thread, ImGui is fed on the render
        // thread, so everything crossing that line sits behind this lock.
        struct Pending {
            float                                    dx = 0.0f;
            float                                    dy = 0.0f;
            float                                    wheel = 0.0f;
            std::vector<std::pair<int, bool>>        buttons;  // (ImGui button, down)
        };

        // Flip to true to log every key press with its device and scan code — the fast
        // way to find out what the game actually sends when a hotkey does not land.
        constexpr bool kLogKeyPresses = false;

        std::mutex g_mutex;
        Pending    g_pending;

        struct Hotkey {
            std::function<void()> fn;
            std::uint32_t         modifier = 0;  // scan code that must be held; 0 = none
        };

        std::mutex                               g_hotkeyMutex;
        std::unordered_map<std::uint32_t, Hotkey> g_hotkeys;

        // Keyboard keys currently held down, maintained from the same event stream
        // the hotkeys use — so modifier checks cannot drift from what the game sees.
        std::mutex                        g_heldMutex;
        std::unordered_map<std::uint32_t, bool> g_held;

        [[nodiscard]] bool IsHeld(std::uint32_t a_scanCode) {
            std::scoped_lock lock(g_heldMutex);
            const auto       it = g_held.find(a_scanCode);
            return it != g_held.end() && it->second;
        }

        // Input events arrive on the game's input thread; hand the callback to the
        // main thread before it touches anything.
        void FireHotkey(std::uint32_t a_scanCode) {
            std::function<void()> fn;
            {
                std::scoped_lock lock(g_hotkeyMutex);
                const auto       it = g_hotkeys.find(a_scanCode);
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
                    if (event->GetEventType() != RE::INPUT_EVENT_TYPE::kButton ||
                        event->GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
                        continue;
                    }
                    auto* button = event->AsButtonEvent();
                    if (!button) {
                        continue;
                    }

                    // Held-key bookkeeping first, so a modifier registers before the
                    // key it modifies is evaluated in the same batch.
                    {
                        std::scoped_lock lock(g_heldMutex);
                        g_held[button->GetIDCode()] = button->IsPressed();
                    }

                    if (!button->IsDown()) {
                        continue;
                    }

                    // DIAGNOSTIC: F11 never reached the handler and the log could not
                    // say why — whether no key events arrive at all, or they arrive
                    // under a device or scan code we did not expect. So print what
                    // actually shows up.
                    if constexpr (kLogKeyPresses) {
                        logger::info("key down: device={} scanCode={:#x}",
                                     static_cast<int>(event->GetDevice()), button->GetIDCode());
                    }

                    FireHotkey(button->GetIDCode());
                }

                if (!IsCapturingInput()) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                std::scoped_lock lock(g_mutex);

                for (auto* event = *a_event; event; event = event->next) {
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
                    a_event->GetDevice() == RE::INPUT_DEVICE::kKeyboard &&
                    (a_event->GetIDCode() == kEsc || a_event->GetIDCode() == kTab);

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
        std::scoped_lock lock(g_hotkeyMutex);
        g_hotkeys[a_scanCode] = Hotkey{ std::move(a_fn), a_modifier };
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
