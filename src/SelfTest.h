#pragma once

// In-game self-test: the half of the testing story that cannot run outside Skyrim.
//
// `tools/check.mjs` verifies everything that is decidable from the source alone — data
// tables agreeing across layers, geometry, JSON contracts. What it cannot know is whether
// the things we name actually EXIST in a running load order: whether every ESP form
// resolves, whether a keyword editor ID we typed is real, whether the shop's catalog can
// hand over what it advertises.
//
// Those failures are silent by nature. A quest whose keyword is misspelled does not error
// — its counter simply never moves, which looks like "the feature is broken" and is
// almost impossible to report usefully. This runs the checks explicitly and writes a
// PASS/FAIL block to the log, so a tester can press one key and send the result.
//
// Each run also writes the same results as [SELFTEST] lines for test runners; the
// format is in SelfTestFormat.h.

#include <string_view>

namespace Isekai::SelfTest {

    // Register the self-test hotkey (see Config::SelfTestKey). Call after
    // UI::ClearHotkeys(), at load and again whenever the settings are re-read. a_log is
    // true at load only; a rebind stays quiet so a journal close does not write to the log.
    void BindHotkeys(bool a_log);

    // Run every check and write the report to the log. Main thread only.
    // Safe at any time: it only reads state, never changes it.
    //
    // a_notify controls the on-screen half. The hotkey wants it (you pressed a key and
    // deserve an answer); the automatic run on load does not, or every load would pop a
    // notification — there it speaks up only when something actually failed.
    void Run(bool a_notify = true);

    // Run once, shortly after a game finishes loading, so every log carries a report of a
    // REAL character without anyone having to set a diagnostic key first. That step was
    // the whole friction: a bug report needs this report, and it needed an ini edit and a
    // restart to get one.
    void RunOnLoad();

    // One test outside the full run — the storage chest on open and close, the token
    // return, a hotkey rebind — as its own block: the [SELFTEST] line, then ENDE
    // (SelfTestFormat.h says why). Any thread; it only writes to the log.
    void Event(std::string_view a_id, bool a_pass, std::string_view a_detail);

    // After a settings reload rebound the hotkeys (refs #37): is every configured key in
    // the table the input handler consults? Writes an Event only when the table changed,
    // so a journal close that rebinds the same keys stays silent.
    void OnHotkeysRebound(bool a_tableChanged);

    // The Papyrus native IsekaiHeroCraft.RunSelfTest(): queue a full run (no on-screen
    // panel) on the main thread. Lets a test runner start the self-test by script call.
    bool RegisterPapyrus(RE::BSScript::IVirtualMachine* a_vm);
}
