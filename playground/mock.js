/* ============================================================================
   Isekai Hero — mock plugin for the playground
   ----------------------------------------------------------------------------
   Stands in for the SKSE plugin (src/UI/Prisma.cpp) so the real PrismaUI view
   can be developed and demoed in a browser without launching Skyrim.

   It reproduces the plugin's actual behaviour, not an approximation:

     - The tree data is the real 17-node graph from src/SkillTree.cpp, with the
       same keys, coordinates, icons, costs, prerequisites and rebirth-tier
       gates.
     - buildTreeJson() mirrors Prisma.cpp::BuildTreeJson exactly (same field
       names and shapes), so the view receives byte-compatible data.
     - onBuy() mirrors OnBuy -> SkillTree::TryUnlock -> PushTree: it validates
       the purchase, spends points, unlocks or ranks up, then re-pushes the
       whole tree -- which is what makes buying feel live.

   The view is never modified. Exactly like the plugin, this defines the
   callbacks on the view's window (isekaiBuy / isekaiChoose / isekaiCloseTree)
   and calls the view's entry points (window.isekaiShowTree / isekaiShowPanel /
   isekaiFlourish) on it.
   ============================================================================ */

(function (global) {
  "use strict";

  /* --- rebirth tiers (PowerLevel in the plugin: Normal < Hero < Ascended) --- */
  var TIER = { Normal: 0, Hero: 1, Ascended: 2 };
  var TIER_NAME = ["Normal", "Hero", "Ascended"];

  /* --- the real tree ------------------------------------------------------
     Transcribed from kNodes in src/SkillTree.cpp. `req` is the minimum rebirth
     tier; `prereq` are node keys (0 = none). `rep`/`maxRank` mark repeatables. */
  var NODES = [
    // hub
    { key: 1,  name: "System Core",         icon: "spells_01_frame.png", x: 550, y: 110, cost: 5,  req: TIER.Normal,   prereq: [0, 0],   desc: "The System takes root.\n+25 Health, Magicka and Stamina." },
    { key: 2,  name: "Dragon's Voice",      icon: "spells_10_frame.png", x: 780, y: 110, cost: 15, req: TIER.Normal,   prereq: [1, 0],   desc: "Your Thu'um recovers faster.\n-20% shout cooldown." },
    { key: 14, name: "Perk Synthesis",      icon: "spells_21_frame.png", x: 320, y: 110, cost: 1,  req: TIER.Normal,   prereq: [1, 0],   rep: true, maxRank: 0, desc: "Condense a System Point into raw potential.\n+5 perk points per purchase. REPEATABLE." },
    // craft (left)
    { key: 3,  name: "Vital Surge",         icon: "spells_25_frame.png", x: 250, y: 260, cost: 10, req: TIER.Normal,   prereq: [1, 0],   desc: "+100 Health." },
    { key: 4,  name: "Thu'um Omniscience",  icon: "spells_39_frame.png", x: 160, y: 410, cost: 25, req: TIER.Hero,     prereq: [3, 0],   desc: "The System pours every dragon's voice into you.\nAll shouts and words of power unlocked." },
    { key: 5,  name: "Emberguard",          icon: "spells_12_frame.png", x: 340, y: 410, cost: 15, req: TIER.Normal,   prereq: [3, 0],   desc: "+25% Fire Resist." },
    // arcana (right)
    { key: 6,  name: "Mana Well",           icon: "spells_15_frame.png", x: 850, y: 260, cost: 10, req: TIER.Normal,   prereq: [1, 0],   desc: "+100 Magicka." },
    { key: 7,  name: "Arcane Omniscience",  icon: "spells_36_frame.png", x: 760, y: 410, cost: 25, req: TIER.Hero,     prereq: [6, 0],   desc: "Every enchantment laid bare.\nAll enchantments known without disenchanting." },
    { key: 8,  name: "Frostguard",          icon: "spells_16_frame.png", x: 940, y: 410, cost: 15, req: TIER.Normal,   prereq: [6, 0],   desc: "+25% Frost Resist." },
    { key: 9,  name: "Spell Omniscience",   icon: "spells_37_frame.png", x: 850, y: 555, cost: 40, req: TIER.Hero,     prereq: [7, 0],   desc: "The System reads every tome ever written.\nAll spells with a spell tome learned." },
    // shadow (centre-down)
    { key: 10, name: "Swift Blood",         icon: "spells_32_frame.png", x: 550, y: 300, cost: 10, req: TIER.Normal,   prereq: [1, 0],   desc: "+100 Stamina." },
    { key: 11, name: "Alchemical Insight",  icon: "spells_31_frame.png", x: 460, y: 450, cost: 25, req: TIER.Hero,     prereq: [10, 0],  desc: "Every ingredient gives up its secrets.\nAll ingredient effects known." },
    { key: 12, name: "Plagueward",          icon: "spells_34_frame.png", x: 640, y: 450, cost: 15, req: TIER.Normal,   prereq: [10, 0],  desc: "+25% Disease Resist." },
    // capstone
    { key: 13, name: "World Tree",          icon: "spells_09_frame.png", x: 550, y: 600, cost: 50, req: TIER.Ascended, prereq: [11, 12], desc: "The System blossoms through your soul.\n+100 Health, Magicka and Stamina." },
    // utility (left margin, repeatable)
    { key: 16, name: "Beast of Burden",     icon: "spells_22_frame.png", x: 95,  y: 190, cost: 2,  req: TIER.Normal,   prereq: [0, 0],   rep: true, maxRank: 0,  desc: "The System shoulders your load.\n+25 Carry Weight per rank." },
    { key: 15, name: "Fleet of Foot",       icon: "spells_28_frame.png", x: 95,  y: 330, cost: 3,  req: TIER.Normal,   prereq: [0, 0],   rep: true, maxRank: 10, desc: "The System quickens your stride.\n+3% movement speed per rank." },
    { key: 17, name: "Enduring Vigor",      icon: "spells_06_frame.png", x: 95,  y: 470, cost: 4,  req: TIER.Normal,   prereq: [0, 0],   rep: true, maxRank: 0,  desc: "The System deepens your reserves.\n+25 Health, Magicka and Stamina per rank." },
  ];

  var PERK_MAX = 255; // Isekai::kMaxPerkPoints (see System.h)

  /* --- mutable game state (co-save equivalents) --------------------------- */
  function freshState() {
    return {
      points: 30,             // System Points
      perks: 0,               // perk pool
      tier: TIER.Ascended,    // rebirth tier reached (start high so nothing is sealed)
      unlocked: {},           // key -> true, one-shot nodes
      ranks: {}               // key -> count, repeatable nodes
    };
  }
  var G = freshState();

  var byKey = {};
  NODES.forEach(function (n) { byKey[n.key] = n; });

  /* --- SkillTree queries, mirroring the plugin's predicates --------------- */
  function rankOf(key) { return G.ranks[key] || 0; }

  function isUnlocked(key) {
    var n = byKey[key];
    if (!n) return false;
    return n.rep ? false : !!G.unlocked[key]; // repeatables never enter `unlocked`
  }

  function maxed(n) {
    return n.rep && n.maxRank > 0 && rankOf(n.key) >= n.maxRank;
  }

  function owned(n) {
    return n.rep ? maxed(n) : isUnlocked(n.key);
  }

  function prereqsMet(n) {
    for (var i = 0; i < n.prereq.length; i++) {
      var pk = n.prereq[i];
      if (pk && !isUnlocked(pk)) return false;
    }
    return true;
  }

  function tierMet(n) { return G.tier >= n.req; }

  /* --- buildTreeJson: same shape as Prisma.cpp::BuildTreeJson ------------- */
  function buildTree() {
    return {
      points: G.points,
      perks: G.perks,
      perkMax: PERK_MAX,
      nodes: NODES.map(function (n) {
        return {
          key: n.key,
          name: n.name,
          desc: n.desc,
          icon: n.icon,
          x: n.x,
          y: n.y,
          cost: n.cost,
          owned: owned(n),
          tierMet: tierMet(n),
          prereqMet: prereqsMet(n),
          repeatable: !!n.rep,
          rank: rankOf(n.key),
          maxRank: n.maxRank || 0,
          reqPower: TIER_NAME[n.req],
          prereq: [n.prereq[0] || 0, n.prereq[1] || 0]
        };
      })
    };
  }

  /* --- TryUnlock, mirroring SkillTree::TryUnlock + OnBuy ------------------- */
  function tryUnlock(key) {
    var n = byKey[key];
    if (!n) return;
    if (owned(n) || !prereqsMet(n) || !tierMet(n) || G.points < n.cost) return;

    // Perk Synthesis is blocked when the pool is already full, like the plugin.
    if (n.key === 14 && G.perks >= PERK_MAX) return;

    G.points -= n.cost;
    if (n.rep) {
      G.ranks[key] = rankOf(key) + 1;
      if (n.key === 14) G.perks = Math.min(PERK_MAX, G.perks + 5);
    } else {
      G.unlocked[key] = true;
    }
  }

  /* ======================================================================
     View plumbing -- the parent reaches into the iframe exactly as the
     plugin reaches into the view.
     ====================================================================== */

  var frame = null;           // the <iframe> element
  var onSelect = null;        // pending panel callback
  var panelMulti = false;

  function win() { return frame && frame.contentWindow; }

  function call(fn, arg) {
    var w = win();
    if (w && typeof w[fn] === "function") w[fn](arg);
  }

  function pushTree() { call("isekaiShowTree", buildTree()); }

  /* Install the callbacks on the view's window (RegisterJSListener equivalent). */
  function installCallbacks() {
    var w = win();
    if (!w) return;

    w.isekaiBuy = function (arg) {
      var key = parseInt(arg, 10) || 0;
      if (!key) return;
      tryUnlock(key);
      pushTree();                 // OnBuy: TryUnlock then PushTree
      global.VS_LOG && global.VS_LOG("buy " + key + " → points " + G.points);
    };

    w.isekaiCloseTree = function () {
      hide();
      global.VS_LOG && global.VS_LOG("close tree");
    };

    w.isekaiChoose = function (arg) {
      var idx = parseInt(arg, 10);
      var fn = onSelect;
      onSelect = null;
      hide();
      global.VS_LOG && global.VS_LOG("choose " + idx);
      if (fn && idx >= 0) fn(idx);
    };
  }

  /* The view has no hide of its own beyond switching screens; in-game the
     plugin hides the whole PrismaUI view. Here the harness fades the iframe. */
  function show() { if (frame) frame.classList.remove("is-hidden"); }
  function hide() { if (frame) frame.classList.add("is-hidden"); }

  /* ======================================================================
     Public API used by the harness controls
     ====================================================================== */
  var Mock = {
    attach: function (iframeEl) {
      frame = iframeEl;
      installCallbacks();
    },

    openTree: function () {
      show();
      pushTree();
      // Focus the frame so the view's ESC handler can fire.
      if (win()) win().focus();
    },

    /* ShowPanel(title, body, choices, onSelect, reveal, width) */
    showPanel: function (opts) {
      onSelect = opts.onSelect || null;
      panelMulti = (opts.buttons || []).length > 1;
      show();
      call("isekaiShowPanel", {
        title: opts.title || "",
        body: opts.body || "",
        reveal: opts.reveal || 45,
        width: opts.width || 720,
        buttons: opts.buttons || [{ label: "Continue", icon: "", iconOnly: false }]
      });
      if (win()) win().focus();
    },

    flourish: function (title, subtitle) {
      show();
      call("isekaiFlourish", { title: title, subtitle: subtitle });
      // Purely visual in-game; auto-hide after the animation like the plugin.
      setTimeout(function () { hide(); }, 2400);
    },

    addPoints: function (n) { G.points = Math.max(0, G.points + n); pushTree(); },
    setTier: function (t) { G.tier = t; pushTree(); },
    getTier: function () { return G.tier; },
    reset: function () { G = freshState(); pushTree(); },

    state: function () { return G; }
  };

  global.IsekaiMock = Mock;
})(window);
