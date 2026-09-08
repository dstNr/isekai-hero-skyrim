# A public API for other mods

Other SKSE plugins should be able to read what the System knows about the player
and to reward the player with System Points. Nothing more in v1.

## Goals

- A foreign SKSE plugin can ask: is the System active, what tier, how many
  points, what rank, does the player hold a given skill-tree node.
- A foreign SKSE plugin can grant System Points, and every grant is attributable
  in the log.
- The mod stays script-free. No Papyrus, no `.pex`, no VM load.
- A consumer built against v1 keeps working after the reward model is redesigned.

## Non-goals

- No Papyrus surface. Reach is smaller; script-freedom is worth more, and it can
  be added later as a thin layer without touching this design.
- Nothing that mutates structure: no unlocking nodes, no setting the tier, no
  editing milestones. Those bypass the prerequisites, costs and the load-time
  reconciliation that keep a save valid.
- No enumeration of the skill tree. See "What node reads do and do not expose".

## Approach

A versioned pure-virtual interface, obtained through an exported
`RequestPluginAPI(InterfaceVersion)` — the same pattern this mod already consumes
from PrismaUI, so the idiom is familiar to the authors we want to reach and a
worked example already sits in `src/PrismaUI_API.h`.

Rejected: a flat C function-pointer table. Marginally more ABI-proof in theory,
but every SKSE plugin is MSVC x64, so the advantage is theoretical while the
unfamiliarity is real.

Rejected: SKSE messaging alone. Right for notifications, wrong for queries —
messages are asynchronous and return nothing.

### ABI rules

Only POD types and `const char*` cross the DLL boundary. No `std::string`, no
containers, no exceptions. `const char*` returns point to storage owned by this
plugin and valid until the next call on the same interface; consumers copy what
they need. This follows `PrismaUI_API.h`, which does the same and is proof the
constraint is workable.

## The v1 interface

```cpp
class IVIsekaiHero1
{
public:
    // --- state ---
    virtual bool        IsActive()        = 0;  // has the player been reincarnated
    virtual uint8_t     GetTier()         = 0;  // NORMAL / HERO / ASCENDED / DORMANT
    virtual const char* GetTierName()     = 0;  // localised, for display
    virtual int32_t     GetSystemPoints() = 0;  // unspent points
    virtual uint8_t     GetRank()         = 0;  // E through S

    // --- skill tree, by stable key ---
    virtual bool        HasNode(uint32_t key)     = 0;
    virtual int32_t     GetNodeRank(uint32_t key) = 0;  // 0 when not owned

    // --- reward ---
    virtual void GrantSystemPoints(int32_t amount, const char* source) = 0;
};
```

The header defines the enums behind the two `uint8_t` returns — the tier values
and the rank letters — so a consumer never hardcodes a bare number. They are
`uint8_t` on the wire so that adding a value later cannot change the interface
layout.

`GetNodeRank` returns 0 when the player does not hold the node, 1 for a one-shot
node they do hold, and the purchase count for a repeatable one. So a caller that
only wants a yes/no can use either function, and `HasNode(k)` is exactly
`GetNodeRank(k) > 0`.

### What node reads do and do not expose

`SkillTree.h` already documents the node key as *"stable across releases; never
reuse a value"*, and the co-save stores those keys in `unlockedNodes` and
`nodeRanks`. They can therefore never be renumbered without breaking every
existing save: they are already a frozen contract that this project pays for
internally. Publishing them adds no new constraint.

What stays private is the node *structure* — the table itself, effect
magnitudes, costs, zones and layout. That is the part the reward redesign will
change. A consumer can ask "does the player hold key 14" and get a durable
answer; it cannot enumerate the tree or read what a node does.

New reward axes may add keys freely. Existing consumers keep working because no
key ever changes meaning.

The published key list lives with the API header so an author does not have to
read our source to use it.

### The source tag

`GrantSystemPoints` takes a `source` string and writes it to the log with every
grant. It is not decoration. When a player reports an implausible point total,
the log names the mod that awarded them, and the report stops being ours to
diagnose. One line of code, and it is the difference between a support burden
and a two-second answer.

An empty or null `source` is accepted but logged as `<unnamed>`; a grant is
never refused for lacking one.

## Threading

A foreign plugin calls from whatever thread it happens to be on.

**Grants** are queued onto the main thread through the existing
`System::DelayedMainThread` and never applied inline. Progression state is owned
by the main thread and mutating it from a worker is how save corruption starts.

**Reads** return copies of scalar values and are documented as main-thread calls.
They are cheap and non-blocking.

## Notifications

When the System activates or the tier changes, this plugin dispatches a small POD
message through `SKSE::GetMessagingInterface()->Dispatch`. Any plugin may listen;
we keep no registration list.

Callbacks with our own bookkeeping would mean tracking consumer lifetimes,
unregistering on unload, and surviving a consumer that crashes inside our call.
The messaging interface already solves all of that and consumers already know it.

## Failure behaviour

- `RequestPluginAPI` with an unknown version returns `nullptr`. A consumer that
  asks for v2 against a v1 build gets a clean refusal, never a crash.
- Grants arriving before a save is loaded are dropped with a log line rather than
  queued against no state.
- No call throws. A consumer's mistake must never take the game down through us.

## Versioning policy

`IVIsekaiHero1` is frozen once released: no method added, removed or reordered.
Additions ship as `IVIsekaiHero2`, and `RequestPluginAPI` keeps answering v1
requests with the v1 interface for as long as it is supported.

This is what makes the narrow v1 surface safe rather than limiting: expanding
later costs a new interface, not a broken consumer.

## Verification

- A `check.mjs` assertion that the version enum in the shipped header matches the
  implementation. The header and the code naming the same thing in two places is
  exactly the drift this harness exists to catch.
- A second assertion that every node key named in the header's published key list
  still exists in `SkillTree.cpp` — this is what turns "never reuse a value" from
  a comment into a guarantee.
- The in-game self-test logs the API's own answers, so a bug report carries them.

## Shipping

`include/IsekaiHeroAPI.h` — one file, no dependencies, MIT, safe to vendor. The
README gains a short section showing the three lines an author needs to obtain
the interface.

## Open point, to settle during implementation

`GetTierName()` returns localised text, which means it depends on the language
file loaded at the time. It is meant for display only. If it turns out that
consumers want a stable identifier instead, the answer is a separate
`GetTierId()` returning an untranslated string, not a change to this method.
