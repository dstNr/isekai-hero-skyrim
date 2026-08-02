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
     tier; `prereq` are node keys (0 = none); `rep`/`maxRank` mark repeatables;
     `zone` is the display grouping (SkillTree::Zone) and `scale` the tile size
     multiplier that makes the hub, the capstone and the gifts read as landmarks. */
  var NODES = [
    // hub
    { key: 1,  zone: "CORE",   scale: 1.4, name: "System Core",         icon: "spells_01_frame.png", x: 615, y: 96,  cost: 5,  req: TIER.Normal,   prereq: [0, 0],   desc: "The System takes root.\n+25 Health, Magicka and Stamina." },
    { key: 2,  zone: "CORE",   name: "Dragon's Voice",      icon: "spells_10_frame.png", x: 880, y: 96,  cost: 15, req: TIER.Normal,   prereq: [1, 0],   desc: "Your Thu'um recovers faster.\n-20% shout cooldown." },
    { key: 14, zone: "CORE",   name: "Perk Synthesis",      icon: "spells_21_frame.png", x: 350, y: 96,  cost: 1,  req: TIER.Normal,   prereq: [1, 0],   rep: true, maxRank: 0, desc: "Condense a System Point into raw potential.\n+5 perk points per purchase. REPEATABLE." },
    { key: 23, zone: "CORE",   name: "System Analysis",     icon: "spells_02_frame.png", x: 615, y: 200, cost: 10, req: TIER.Normal,   prereq: [1, 0],   desc: "Unlocks the System's analytical eye.\nPress the Analyze hotkey to appraise whatever you are looking at." },
    // might (left band)
    { key: 3,  zone: "MIGHT",  name: "Vital Surge",         icon: "spells_25_frame.png", x: 300, y: 360, cost: 10, req: TIER.Normal,   prereq: [1, 0],   desc: "+100 Health." },
    { key: 4,  zone: "MIGHT",  scale: 1.2, name: "Thu'um Omniscience",  icon: "spells_39_frame.png", x: 210, y: 500, cost: 25, req: TIER.Hero,     prereq: [3, 0],   desc: "The System pours every dragon's voice into you.\nAll shouts and words of power unlocked." },
    { key: 5,  zone: "MIGHT",  name: "Emberguard",          icon: "spells_12_frame.png", x: 390, y: 500, cost: 15, req: TIER.Normal,   prereq: [3, 0],   desc: "+25% Fire Resist." },
    // arcana (right band)
    { key: 6,  zone: "ARCANA", name: "Mana Well",           icon: "spells_15_frame.png", x: 930, y: 360, cost: 10, req: TIER.Normal,   prereq: [1, 0],   desc: "+100 Magicka." },
    { key: 7,  zone: "ARCANA", scale: 1.2, name: "Arcane Omniscience",  icon: "spells_36_frame.png", x: 845, y: 500, cost: 25, req: TIER.Hero,     prereq: [6, 0],   desc: "Every enchantment laid bare.\nAll enchantments known without disenchanting." },
    { key: 8,  zone: "ARCANA", name: "Frostguard",          icon: "spells_16_frame.png", x: 1015, y: 500, cost: 15, req: TIER.Normal,   prereq: [6, 0],   desc: "+25% Frost Resist." },
    { key: 9,  zone: "ARCANA", scale: 1.2, name: "Spell Omniscience",   icon: "spells_37_frame.png", x: 845, y: 640, cost: 40, req: TIER.Hero,     prereq: [7, 0],   desc: "The System reads every tome ever written.\nAll spells with a spell tome learned." },
    // shadow (centre band)
    { key: 10, zone: "SHADOW", name: "Swift Blood",         icon: "spells_32_frame.png", x: 615, y: 360, cost: 10, req: TIER.Normal,   prereq: [1, 0],   desc: "+100 Stamina." },
    { key: 11, zone: "SHADOW", scale: 1.2, name: "Alchemical Insight",  icon: "spells_31_frame.png", x: 530, y: 500, cost: 25, req: TIER.Hero,     prereq: [10, 0],  desc: "Every ingredient gives up its secrets.\nAll ingredient effects known." },
    { key: 12, zone: "SHADOW", name: "Plagueward",          icon: "spells_34_frame.png", x: 700, y: 500, cost: 15, req: TIER.Normal,   prereq: [10, 0],  desc: "+25% Disease Resist." },
    // capstone (bottom of the shadow band)
    { key: 13, zone: "SHADOW", scale: 1.45, name: "World Tree",         icon: "spells_09_frame.png", x: 615, y: 650, cost: 50, req: TIER.Ascended, prereq: [11, 12], desc: "The System blossoms through your soul.\n+100 Health, Magicka and Stamina." },
    // mastery rail (repeatable, capped at 10 ranks / 5 tiers)
    { key: 16, zone: "MASTERY", name: "Beast of Burden",     icon: "spells_22_frame.png", x: 95,  y: 150, cost: 2,  req: TIER.Normal,   prereq: [0, 0],   rep: true, maxRank: 10, desc: "The System shoulders your load.\n+25 Carry Weight per rank." },
    { key: 15, zone: "MASTERY", name: "Fleet of Foot",       icon: "spells_28_frame.png", x: 95,  y: 221, cost: 3,  req: TIER.Normal,   prereq: [0, 0],   rep: true, maxRank: 10, desc: "The System quickens your stride.\n+3% movement speed per rank." },
    { key: 17, zone: "MASTERY", name: "Enduring Vigor",      icon: "spells_06_frame.png", x: 95,  y: 292, cost: 4,  req: TIER.Normal,   prereq: [0, 0],   rep: true, maxRank: 10, desc: "The System deepens your reserves.\n+25 Health, Magicka and Stamina per rank." },
    { key: 18, zone: "MASTERY", name: "Storm Ward",          icon: "spells_18_frame.png", x: 95,  y: 363, cost: 3,  req: TIER.Normal,   prereq: [0, 0],   rep: true, maxRank: 10, desc: "The System turns aside the lightning.\n+5% Shock Resist per rank." },
    { key: 19, zone: "MASTERY", name: "Warded Mind",         icon: "spells_19_frame.png", x: 95,  y: 434, cost: 4,  req: TIER.Normal,   prereq: [0, 0],   rep: true, maxRank: 10, desc: "The System shields your soul from magic.\n+5% Magic Resist per rank." },
    { key: 20, zone: "MASTERY", name: "Arcane Absorption",   icon: "spells_20_frame.png", x: 95,  y: 505, cost: 5,  req: TIER.Normal,   prereq: [0, 0],   rep: true, maxRank: 10, desc: "The System drinks the spells cast against you.\n+4% Spell Absorption per rank." },
    { key: 21, zone: "MASTERY", name: "Iron Skin",           icon: "spells_23_frame.png", x: 95,  y: 576, cost: 4,  req: TIER.Normal,   prereq: [0, 0],   rep: true, maxRank: 10, desc: "The System hardens your hide.\n+10 Armor Rating per rank." },
    { key: 22, zone: "MASTERY", name: "Rapid Recovery",      icon: "spells_24_frame.png", x: 95,  y: 647, cost: 5,  req: TIER.Normal,   prereq: [0, 0],   rep: true, maxRank: 10, desc: "The System accelerates your body's grace.\n+10% Health, Magicka and Stamina regeneration per rank." },
  ];

  var PERK_MAX = 255; // Isekai::kMaxPerkPoints (see System.h)

  /* Mirrors SkillTree.cpp::IsRefundable. Not refundable: the four Omniscience unlocks
     (4, 7, 9, 11) — they write knowledge the player keeps — and Perk Synthesis (14),
     whose points already became perk points. */
  var NO_REFUND = { 4: 1, 7: 1, 9: 1, 11: 1, 14: 1 };

  /* --- mutable game state (co-save equivalents) --------------------------- */
  function freshState() {
    return {
      points: 30,             // System Points
      perks: 0,               // perk pool
      tier: TIER.Ascended,    // rebirth tier reached (start high so nothing is sealed)
      dormant: false,         // DORMANT blessing: sealed nodes name a LEVEL, not a rebirth
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

  /* Isekai::AwakeningLevelFor — 0 unless the blessing is dormant. The levels match
     the ini defaults (DormantHeroLevel / DormantAscendedLevel). */
  var DORMANT_LEVEL = [0, 25, 80];
  function awakeningLevelFor(req) { return G.dormant ? (DORMANT_LEVEL[req] || 0) : 0; }

  function refundable(n) { return !NO_REFUND[n.key]; }

  /* Mastery tiers, mirroring SkillTree.cpp's IsMasteryNode/TierOfRank/RankCost. A
     MASTERY node is a capped repeatable (maxRank > 0) — Perk Synthesis (maxRank 0) is
     the only other repeatable and stays a flat, uncapped exchange. */
  var TIER_COUNT = 5, RANKS_PER_TIER = 2;
  var MASTERY_TIER_NAMES = ["Novice", "Adept", "Expert", "Master", "Grandmaster"];

  function isMastery(n) { return n.rep && n.maxRank > 0; }

  function tierOfRank(rank) {
    if (rank <= 0) return 0;
    return Math.min(TIER_COUNT, Math.floor((rank - 1) / RANKS_PER_TIER) + 1);
  }

  function tierOf(n) { return isMastery(n) ? tierOfRank(rankOf(n.key)) : 0; }

  /* What buying rank `rank` of a mastery node costs: base cost x that rank's tier. */
  function rankCost(n, rank) { return isMastery(n) ? n.cost * tierOfRank(rank) : n.cost; }

  function nextCost(n) { return rankCost(n, (n.rep ? rankOf(n.key) : 0) + 1); }

  /* Total SP sunk into a repeatable at `rank` — the sum of every tiered purchase for
     mastery nodes, a flat cost*rank for everything else (Perk Synthesis). */
  function repeatableCostToRank(n, rank) {
    if (!isMastery(n)) return n.cost * rank;
    var total = 0;
    for (var i = 1; i <= rank; i++) total += rankCost(n, i);
    return total;
  }

  /* RespecRefund / Respec, mirroring the plugin. */
  function respecAmount() {
    var total = 0;
    NODES.forEach(function (n) {
      if (!refundable(n)) return;
      total += n.rep ? repeatableCostToRank(n, rankOf(n.key)) : (isUnlocked(n.key) ? n.cost : 0);
    });
    return total;
  }

  function doRespec() {
    var refund = respecAmount();
    if (refund <= 0) return;
    NODES.forEach(function (n) {
      if (!refundable(n)) return;
      if (n.rep) { delete G.ranks[n.key]; } else { delete G.unlocked[n.key]; }
    });
    G.points += refund;
  }

  /* --- buildTreeJson: same shape as Prisma.cpp::BuildTreeJson ------------- */
  function buildTree() {
    return {
      points: G.points,
      perks: G.perks,
      perkMax: PERK_MAX,
      respec: respecAmount(),
      nodes: NODES.map(function (n) {
        return {
          key: n.key,
          name: n.name,
          desc: n.desc,
          icon: n.icon,
          x: n.x,
          y: n.y,
          zone: n.zone,
          scale: n.scale || 1,
          cost: nextCost(n),
          owned: owned(n),
          tierMet: tierMet(n),
          prereqMet: prereqsMet(n),
          repeatable: !!n.rep,
          rank: rankOf(n.key),
          maxRank: n.maxRank || 0,
          masteryTier: tierOf(n),
          masteryTierName: MASTERY_TIER_NAMES[tierOf(n) - 1] || "",
          reqPower: TIER_NAME[n.req],
          reqLevel: awakeningLevelFor(n.req),
          prereq: [n.prereq[0] || 0, n.prereq[1] || 0]
        };
      })
    };
  }

  /* --- TryUnlock, mirroring SkillTree::TryUnlock + OnBuy ------------------- */
  function tryUnlock(key) {
    var n = byKey[key];
    if (!n) return;
    var cost = nextCost(n);
    if (owned(n) || !prereqsMet(n) || !tierMet(n) || G.points < cost) return;

    // Perk Synthesis is blocked when the pool is already full, like the plugin.
    if (n.key === 14 && G.perks >= PERK_MAX) return;

    G.points -= cost;
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

  /* --- SYSTEM SHOP, mirroring src/Shop.cpp -------------------------------
     Same goods, same prices, same order as Shop::Catalog(), so the indices the view
     sends back mean the same thing here as in the plugin. Quantities are per material
     TYPE — one alchemy pack covers every official ingredient in the load order. */
  var SHOP_ITEMS = [
    { name: "Smithing Materials",  qty: "20 of each",  cost: 15, icon: "shop_smithing_small.png" },
    { name: "Smithing Crate",      qty: "100 of each", cost: 50, icon: "shop_smithing_large.png" },
    { name: "Alchemy Ingredients", qty: "20 of each",  cost: 15, icon: "shop_alchemy_small.png" },
    { name: "Alchemy Crate",       qty: "100 of each", cost: 50, icon: "shop_alchemy_large.png" },
    { name: "Soul Gems",           qty: "10 of each",  cost: 20, icon: "shop_souls_small.png" },
    { name: "Soul Gem Crate",      qty: "50 of each",  cost: 65, icon: "shop_souls_large.png" },
    { name: "Gold",                qty: "x1000",       cost: 10, icon: "shop_gold_small.png" },
    { name: "Gold Hoard",          qty: "x10000",      cost: 75, icon: "shop_gold_large.png" }
  ];

  function buildShop() { return { points: G.points, items: SHOP_ITEMS }; }
  function pushShop() { call("isekaiShowShop", buildShop()); }

  /* Mirrors Shop::Buy(): spend, deliver, then re-push so the balance and every card's
     affordability refresh. There is no chest in the mock, so "deliver" is the log line. */
  function shopBuy(idx) {
    var it = SHOP_ITEMS[idx];
    if (!it) return;
    if (G.points < it.cost) {
      global.VS_LOG && global.VS_LOG("shop: cannot afford " + it.name);
    } else {
      G.points -= it.cost;
      global.VS_LOG && global.VS_LOG("shop: bought " + it.name + " " + it.qty +
                                     " for " + it.cost + " SP");
    }
    pushShop();
  }

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

    w.isekaiRespec = function () {
      doRespec();
      pushTree();
      global.VS_LOG && global.VS_LOG("respec → points " + G.points);
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

    w.isekaiShopBuy = function (arg) {
      var idx = parseInt(arg, 10);
      if (!isNaN(idx)) shopBuy(idx);
    };

    w.isekaiShopClose = function () {
      hide();
      global.VS_LOG && global.VS_LOG("close shop");
    };

    // Mirrors Prisma::OnStatusAction — tree / storage / shop / reboot / close.
    w.isekaiStatusAction = function (action) {
      global.VS_LOG && global.VS_LOG("status action: " + action);
      if (action === "tree") { pushTree(); }        // OpenTree switches screens in-view
      else if (action === "shop") { pushShop(); }    // OpenShop, likewise in-view
      else if (action === "close" || action === "storage") { hide(); }
      // Storage opens a native container menu in-game — nothing to preview here.
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

    openShop: function () {
      show();
      pushShop();
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

    /* Mirrors Prisma::ShowStatus — hands the structured status payload to the real
       status screen so its layout can be seen and clicked here. */
    showStatus: function (data) {
      show();
      call("isekaiShowStatus", data);
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
    setDormant: function (b) { G.dormant = !!b; pushTree(); },
    getDormant: function () { return G.dormant; },
    reset: function () { G = freshState(); pushTree(); },

    state: function () { return G; }
  };

  global.IsekaiMock = Mock;
})(window);
