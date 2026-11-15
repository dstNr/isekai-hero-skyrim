#pragma once

// A per-frame tick on Skyrim's MAIN thread: a vtable hook on PlayerCharacter's
// Update(float). It replaces the old clock inside the render-thread Present hook, which
// had to hand every piece of work to an SKSE task.
//
// Contract for listeners:
// - They run on the main thread, once per frame, but ONLY while the player's update
//   runs. It does not run while the game is paused (menus that pause, the console)
//   or in the free camera (tfc, and a scene's automatic free camera).
// - So they must tolerate gaps of any length, and must not be used for anything that
//   has to happen while paused or in the free camera.
// - They run after the game's own Update, in the order they were added.
// - An exception thrown by a listener is caught and logged; it never reaches the game.
// - Keep them cheap. The hook path itself allocates nothing and looks up no forms; a
//   listener that needs a slower cadence throttles itself (see TickThrottle.h).

namespace Isekai::FrameTick {

    // Install the hook and the built-in listener (the once-a-second quest tick and the
    // mod API publish). Call once, at kDataLoaded.
    void Install();

    using Listener = void (*)();

    // Append a listener. Call only at kDataLoaded, from the main thread: the list is a
    // fixed array the hook reads without a lock, and it must be complete before the
    // first frame of play.
    void Add(Listener a_listener);
}
