/* ============================================================================
   Isekai Hero — mock plugin for the playground
   ----------------------------------------------------------------------------
   Stands in for the SKSE plugin so the real Magelight pages can be developed
   and reviewed in a browser without launching Skyrim. It plays both halves of
   what the game does:

     - The host. Every window.magelight.send() the page makes arrives here
       (serve.mjs injects the forwarding shim into the page). "isekai.ready"
       releases the queue the way src/UI/Magelight.cpp does — the translation
       table and UiScale first, then everything shown before the page was ready.
       "isekai.action" is routed like Screens::OnAction: choose, buy, respec,
       shopBuy, status, nav, close.
     - The game. The tree data is the real 17-node graph from src/SkillTree.cpp
       (keys, coordinates, icons, costs, prerequisites, rebirth-tier gates), the
       shop is Shop::Catalog() in order, and the panel scenarios use the real
       strings from src/System.cpp, so a layout seen here is a layout the player
       sees. buildTree() mirrors Payloads::Tree(), buildShop() Payloads::Shop().

   The page is never modified: this file only calls the page's own entry points
   (isekai.show / loc / scale) and answers what it sends.
   ============================================================================ */

(function (global) {
  "use strict";

  /* --- rebirth tiers (PowerLevel in the plugin: Normal < Hero < Ascended) --- */
  var TIER = { Normal: 0, Hero: 1, Ascended: 2 };
  // PowerName(): the plugin sends the tier names in upper case, and no gate name for
  // a Normal gate (Payloads::Tree), since every node opens at Normal.
  var TIER_NAME = ["NORMAL", "HERO", "ASCENDED"];

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
    // might (left band)
    { key: 3,  zone: "MIGHT",  name: "Vital Surge",         icon: "spells_25_frame.png", x: 300, y: 400, cost: 10, req: TIER.Normal,   prereq: [1, 0],   desc: "+100 Health." },
    { key: 4,  zone: "MIGHT",  scale: 1.2, name: "Thu'um Omniscience",  icon: "spells_39_frame.png", x: 210, y: 540, cost: 25, req: TIER.Hero,     prereq: [3, 0],   desc: "The System pours every dragon's voice into you.\nAll shouts and words of power unlocked." },
    { key: 5,  zone: "MIGHT",  name: "Emberguard",          icon: "spells_12_frame.png", x: 390, y: 540, cost: 15, req: TIER.Normal,   prereq: [3, 0],   desc: "+25% Fire Resist." },
    // arcana (right band)
    { key: 6,  zone: "ARCANA", name: "Mana Well",           icon: "spells_15_frame.png", x: 930, y: 400, cost: 10, req: TIER.Normal,   prereq: [1, 0],   desc: "+100 Magicka." },
    { key: 7,  zone: "ARCANA", scale: 1.2, name: "Arcane Omniscience",  icon: "spells_36_frame.png", x: 1015, y: 540, cost: 25, req: TIER.Hero,     prereq: [6, 0],   desc: "Every enchantment laid bare.\nAll enchantments known without disenchanting." },
    { key: 8,  zone: "ARCANA", name: "Frostguard",          icon: "spells_16_frame.png", x: 845, y: 540, cost: 15, req: TIER.Normal,   prereq: [6, 0],   desc: "+25% Frost Resist." },
    { key: 9,  zone: "ARCANA", scale: 1.2, name: "Spell Omniscience",   icon: "spells_37_frame.png", x: 1015, y: 680, cost: 40, req: TIER.Hero,     prereq: [7, 0],   desc: "The System reads every tome ever written.\nAll spells with a spell tome learned." },
    // shadow (centre band)
    { key: 10, zone: "SHADOW", name: "Swift Blood",         icon: "spells_32_frame.png", x: 615, y: 400, cost: 10, req: TIER.Normal,   prereq: [1, 0],   desc: "+100 Stamina." },
    { key: 11, zone: "SHADOW", scale: 1.2, name: "Alchemical Insight",  icon: "spells_31_frame.png", x: 530, y: 540, cost: 25, req: TIER.Hero,     prereq: [10, 0],  desc: "Every ingredient gives up its secrets.\nAll ingredient effects known." },
    { key: 12, zone: "SHADOW", name: "Plagueward",          icon: "spells_34_frame.png", x: 700, y: 540, cost: 15, req: TIER.Normal,   prereq: [10, 0],  desc: "+25% Disease Resist." },
    // capstone (bottom of the shadow band)
    { key: 13, zone: "SHADOW", scale: 1.45, name: "World Tree",         icon: "spells_09_frame.png", x: 615, y: 690, cost: 50, req: TIER.Ascended, prereq: [11, 12], desc: "The System blossoms through your soul.\n+100 Health, Magicka and Stamina." },
    // enchant (System enchantments, all hang off Arcane Omniscience)
    { key: 24, zone: "ENCHANT", name: "Divine Judgment",    icon: "spells_14_frame.png", x: 1190, y: 400, cost: 30, req: TIER.Hero,     prereq: [7, 0],   desc: "The System judges with light.\nSun damage, doubled against the undead." },
    { key: 25, zone: "ENCHANT", name: "Stormchain",         icon: "spells_02_frame.png", x: 1190, y: 540, cost: 35, req: TIER.Hero,     prereq: [7, 0],   desc: "The System's lightning seeks every foe.\nShock damage that leaps to up to three more enemies." },
    { key: 26, zone: "ENCHANT", name: "Soul Rend",          icon: "spells_08_frame.png", x: 1190, y: 680, cost: 30, req: TIER.Hero,     prereq: [7, 0],   desc: "The System tears life from the wound.\nAbsorbs health and magicka with every hit." },
    { key: 27, zone: "ENCHANT", name: "System Breach",      icon: "spells_38_frame.png", x: 1190, y: 820, cost: 40, req: TIER.Ascended, prereq: [7, 0],   desc: "The System rewrites what the blow is allowed to ignore.\nExtra damage that bypasses armor and resistances." },
    { key: 28, zone: "ENCHANT", name: "Bulwark",            icon: "spells_05_frame.png", x: 1340, y: 350, cost: 30, req: TIER.Hero,     prereq: [7, 0],   desc: "The System raises a wall around you.\nGreatly raised armor rating and reduced damage taken." },
    { key: 29, zone: "ENCHANT", name: "Thorncrown",         icon: "spells_29_frame.png", x: 1340, y: 490, cost: 30, req: TIER.Hero,     prereq: [7, 0],   desc: "The System returns every blow in kind.\nReflects part of melee damage taken." },
    { key: 30, zone: "ENCHANT", name: "Phoenix Heart",      icon: "spells_11_frame.png", x: 1340, y: 630, cost: 40, req: TIER.Ascended, prereq: [7, 0],   desc: "The System will not let you fall.\nStrong health regeneration; once a minute, a rescue heal when near death." },
    { key: 31, zone: "ENCHANT", name: "Voidstep",           icon: "spells_04_frame.png", x: 1340, y: 770, cost: 30, req: TIER.Hero,     prereq: [7, 0],   desc: "The System lightens every step.\nFaster movement, cheaper stamina, no fall damage." },
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
     (4, 7, 9, 11) and the eight System enchantments (24-31) — they write knowledge the
     player keeps — and Perk Synthesis (14), whose points already became perk points. */
  var NO_REFUND = { 4: 1, 7: 1, 9: 1, 11: 1, 14: 1, 24: 1, 25: 1, 26: 1, 27: 1, 28: 1, 29: 1, 30: 1, 31: 1 };

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

  /* --- buildTreeJson: same shape as the tree JSON the plugin builds (src/UI/Payloads.cpp) ------------- */
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
          reqPower: n.req ? TIER_NAME[n.req] : "",
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

  /* --- SYSTEM SHOP, mirroring src/Shop.cpp -------------------------------
     Same goods, same prices, same order as Shop::Catalog(), so the indices the view
     sends back mean the same thing here as in the plugin. Quantities are per material
     TYPE — one alchemy pack covers every official ingredient in the load order. */
  var SHOP_ITEMS = [
    { name: "Smithing Materials",  qty: "100 of each", cost:  5, icon: "shop_smithing_small.png", shelf: "MATERIALS" },
    { name: "Smithing Crate",      qty: "500 of each", cost: 15, icon: "shop_smithing_large.png", shelf: "MATERIALS" },
    { name: "Alchemy Ingredients", qty: "100 of each", cost:  5, icon: "shop_alchemy_small.png", shelf: "MATERIALS" },
    { name: "Alchemy Crate",       qty: "500 of each", cost: 15, icon: "shop_alchemy_large.png", shelf: "MATERIALS" },
    { name: "Soul Gems",           qty: "50 of each",  cost:  5, icon: "shop_souls_small.png", shelf: "MATERIALS" },
    { name: "Soul Gem Crate",      qty: "250 of each", cost: 15, icon: "shop_souls_large.png", shelf: "MATERIALS" },
    { name: "Gold",                qty: "x100,000",    cost:  5, icon: "shop_gold_small.png", shelf: "WEALTH" },
    { name: "Gold Hoard",          qty: "x1,000,000",  cost: 20, icon: "shop_gold_large.png", shelf: "WEALTH" },
    // System potions. These only appear in game once their ESP record resolves; the
    // mock always shows them, which is the point of a preview.
    //
    // `fx` is the hover tooltip's text. In game the plugin READS it off the potion's own
    // record (Shop::DescribeEffects), so it cannot disagree with what drinking one does —
    // there is no such record here, so these lines are transcribed from Part H of
    // docs/CREATION_KIT_ESP.md and are a preview of the layout, not a source of truth.
    // The game also supplies the effect names in the player's language; these are English.
    { name: "Restorative: Vigor",       qty: "x10", cost: 3, icon: "shop_potion_vigor.png", shelf: "RESTORATIVES",
      fx: "Restore Stamina 10000" },
    { name: "Restorative: Focus",       qty: "x10", cost: 3, icon: "shop_potion_focus.png", shelf: "RESTORATIVES",
      fx: "Restore Magicka 10000" },
    { name: "Restorative: Vitality",    qty: "x10", cost: 3, icon: "shop_potion_vitality.png", shelf: "RESTORATIVES",
      fx: "Restore Health 10000" },
    { name: "Panacea",                  qty: "x10", cost: 3, icon: "shop_potion_panacea.png", shelf: "RESTORATIVES",
      fx: "Cure Disease\nCure Poison" },
    { name: "Elixir of the System",     qty: "x10", cost: 6, icon: "shop_elixir_system.png", shelf: "ELIXIRS",
      fx: "Fortify Health 500  (60 min)\nFortify Magicka 500  (60 min)\nFortify Stamina 500  (60 min)" },
    { name: "Draught of the Ascended",  qty: "x10", cost: 6, icon: "shop_elixir_ascended.png", shelf: "ELIXIRS",
      fx: "Fortify One-Handed 500  (60 min)\nFortify Two-Handed 500  (60 min)\nFortify Marksman 500  (60 min)\nFortify Destruction 500  (60 min)" },
    { name: "Aegis Elixir",             qty: "x10", cost: 6, icon: "shop_elixir_aegis.png", shelf: "ELIXIRS",
      fx: "Resist Magic 85  (60 min)\nResist Fire 85  (60 min)\nResist Frost 85  (60 min)\nResist Shock 85  (60 min)" },
    { name: "Phantom Draught",          qty: "x10", cost: 6, icon: "shop_elixir_phantom.png", shelf: "ELIXIRS",
      fx: "Invisibility  (60 min)\nMuffle  (60 min)\nFortify Sneak 500  (60 min)" },
    { name: "Elixir of Endless Casting", qty: "x10", cost: 6, icon: "shop_elixir_casting.png", shelf: "ELIXIRS",
      fx: "Fortify Magicka Regen 1000  (60 min)\nFortify Magicka 1000  (60 min)" },
    { name: "Titan's Draught",          qty: "x10", cost: 6, icon: "shop_elixir_titan.png", shelf: "ELIXIRS",
      fx: "Fortify Carry Weight 2000  (60 min)\nFortify Stamina 500  (60 min)\nWaterbreathing  (60 min)" }
    // The Flameforged Oathblade and its ARMAMENTS shelf are parked; see src/Shop.cpp.
  ];

  var SHOP_SHELVES = ["MATERIALS", "WEALTH", "RESTORATIVES", "ELIXIRS"];

  function buildShop() { return { points: G.points, shelves: SHOP_SHELVES, items: SHOP_ITEMS }; }

  /* Mirrors Shop::Buy(): spend, deliver, then re-push so the balance and every card's
     affordability refresh. There is no chest in the mock, so "deliver" is the log line. */
  function shopBuy(idx) {
    var it = SHOP_ITEMS[idx];
    if (!it) return;
    if (G.points < it.cost) {
      log("shop: cannot afford " + it.name);
    } else {
      G.points -= it.cost;
      log("shop: bought " + it.name + " " + it.qty + " for " + it.cost + " SP");
    }
    show("shop", buildShop());
  }

  /* --- the status ledger, shaped like BuildStatusJson ---------------------
     One character per blessing kind. rankTone / blessingTone mirror the plugin's
     Logic::ToneForRank and Logic::ToneFor(Blessing) (src/UI/UiLogic.h), which the
     plugin sends with the ledger; level is the character level the header shows.
     The "s" character has every one of the 79 milestones: the stress case for
     scaling, where the window must stay on screen and the titles scroll inside it. */
  var STATUS_KINDS = {
    custom:    { tier: "ASCENDED", mode: "CUSTOM",    rank: "C", level: 23, earned: 7,
                 rewardTier: "ASCENDED", treeTier: "HERO", grantTier: "NORMAL" },
    s:         { tier: "ASCENDED", mode: "",          rank: "S", level: 81, earned: 79 },
    hero:      { tier: "HERO",     mode: "",          rank: "B", level: 37, earned: 24 },
    shattered: { tier: "ASCENDED", mode: "SHATTERED", rank: "B", level: 34, earned: 22 },
    dormant:   { tier: "NORMAL",   mode: "DORMANT",   rank: "D", level: 12, earned: 4,
                 nextAwakening: 25, quiet: 6 }
  };
  var statusKind = "custom";   // the last one shown, for nav:status and re-pushes

  function toneForRank(rank) { return rank === "S" ? "gold" : "cyan"; }
  function toneForBlessing(tier, mode) {
    if (mode === "SHATTERED") return "red";
    if (mode === "DORMANT" || mode === "CUSTOM") return "violet";
    return tier === "ASCENDED" ? "gold" : "cyan";
  }

  var TITLES = [
    { name: "Survivor",      effect: "+100 Health",        quest: "Unbound" },
    { name: "Wayfarer",      effect: "+100 Carry Weight",  quest: "Before the Storm" },
    { name: "Dragonborn",    effect: "+15% Magic Resist",  quest: "The Way of the Voice" },
    { name: "Tomb Raider",   effect: "+25 Stamina",        quest: "Bleak Falls Barrow" },
    { name: "Dragon Slayer", effect: "+10% Fire Resist",   quest: "Dragon Rising" },
    { name: "Voice Wielder", effect: "+50 Magicka",        quest: "The Horn of Jurgen Windcaller" },
    { name: "Horn Bearer",   effect: "+5% Shock Resist",   quest: "The Horn of Jurgen Windcaller" }
  ];

  function buildStatus(kind) {
    var k = STATUS_KINDS[kind] || STATUS_KINDS.custom;
    var titles = TITLES.slice(0, Math.min(k.earned, TITLES.length));
    for (var i = titles.length + 1; i <= k.earned; i++) {
      titles.push({ name: "Title " + i, effect: "+" + (i * 2) + " Health",
                    quest: "Quest milestone number " + i });
    }
    return {
      tier: k.tier, rank: k.rank, mode: k.mode,
      rewardTier: k.rewardTier || k.tier, treeTier: k.treeTier || k.tier, grantTier: k.grantTier || k.tier,
      nextAwakening: k.nextAwakening || 0,
      milestonesEarned: k.earned, milestonesTotal: 79,
      points: G.points, hasTree: true, hasStorage: true, hasShop: true, canReboot: true,
      // The standing System objective (src/Quests.cpp). Empty questText hides the row,
      // unless hours remain until the next one ("the System is quiet").
      // questCanReroll is the ini's testing switch (QuestRerollButton), on here so the
      // preview shows the widest possible action row.
      questText: k.quiet ? "" : "Slay 25 Draugr", questProgress: k.earned >= 79 ? 25 : 9, questTarget: 25,
      questReward: 16, questWaitHours: k.quiet || 0, questCanReroll: !k.quiet,
      attunements: k.earned < 5 ? [{ stat: "Health", amount: 100 }, { stat: "Carry Weight", amount: 100 }] : [
        { stat: "Health", amount: 250 }, { stat: "Magicka", amount: 150 },
        { stat: "Stamina", amount: 150 }, { stat: "Carry Weight", amount: 200 },
        { stat: "Magic Resist", amount: 15 }, { stat: "Fire Resist", amount: 25 },
        { stat: "Frost Resist", amount: 25 }, { stat: "Disease Resist", amount: 25 }
      ],
      titles: titles,
      level: k.level,
      rankTone: toneForRank(k.rank),
      blessingTone: toneForBlessing(k.tier, k.mode)
    };
  }

  function showStatus(kind) {
    if (kind) statusKind = kind;
    show("status", buildStatus(statusKind));
  }

  /* ======================================================================
     The hud view (hud.html) — toasts, the flourish and the threat labels
     ----------------------------------------------------------------------
     What Task 10's src/UI/Hud.cpp sends. Toasts use the real strings from
     Quests.cpp / Professions.cpp; the labels are a camp of actors around a player
     of level 30, with the verdict worked out as Logic::VerdictFor does it. Positions
     drift at 60 Hz (labels.pos every frame while something moves); a still camp gets
     the labels.keep() heartbeat at 4 Hz instead; "Stall" stops every call, which is
     what free camera and a pause look like to the page (it hides the labels after
     600 ms and shows them again on the next call).
     ====================================================================== */

  var PLAYER_LEVEL = 30;

  /* Logic::VerdictFor(enemy level - player level), the plugin's thresholds. */
  function verdictFor(diff) {
    if (diff >= 20) return "lethal";
    if (diff >= 5) return "dangerous";
    if (diff <= -15) return "trivial";
    return "manageable";
  }
  var VERDICT_TAG = { lethal: "LETHAL", dangerous: "DANGEROUS", manageable: "MANAGEABLE", trivial: "TRIVIAL" };

  /* name, level, hp, mp, sp ([now, max]; max 0 = the actor has none), x, y (0..1 of the
     view), t (distance / range), drift phase. The nameless one shows the page's "Unknown". */
  var CAMP = [
    ["Bandit Chief",          38, [612, 780],   [0, 0],     [310, 340], .50, .40, .05],
    ["Draugr Deathlord",      54, [1480, 1480], [120, 150], [420, 420], .27, .30, .30],
    ["Frost Troll",           33, [455, 980],   [0, 0],     [260, 300], .74, .36, .22],
    ["Forsworn Briarheart",   36, [500, 560],   [180, 260], [220, 240], .36, .53, .12],
    ["Bandit Thug",           24, [320, 320],   [0, 0],     [190, 200], .63, .57, .10],
    ["Wolf",                  10, [83, 140],    [0, 0],     [0, 0],     .84, .64, .40],
    ["Vampire Nightstalker",  31, [410, 410],   [350, 380], [180, 180], .16, .47, .55],
    ["Mudcrab",                2, [15, 15],     [0, 0],     [0, 0],     .52, .74, .70],
    ["Hagraven",              42, [300, 520],   [400, 400], [0, 0],     .87, .24, .66],
    ["Bandit Archer",         27, [260, 270],   [0, 0],     [140, 260], .10, .27, .80],
    ["",                      28, [200, 220],   [0, 0],     [0, 0],     .62, .18, .85],
    ["Skeever",                1, [8, 10],      [0, 0],     [0, 0],     .30, .76, .92]
  ];

  function campLabels(n) {
    return CAMP.slice(0, n).map(function (c, i) {
      var v = verdictFor(c[1] - PLAYER_LEVEL);
      return {
        id: 0xFF000800 + i, x: c[5], y: c[6], s: 1 - 0.32 * c[7],
        level: c[1], name: c[0], tag: VERDICT_TAG[v], verdict: v,
        tone: v === "lethal" || v === "dangerous" ? "red" : "cyan",
        hp: c[2], mp: c[3], sp: c[4]
      };
    });
  }

  var hudFrame = null;       // the <iframe> holding hud.html
  var hudWin = null;         // its window, known from the first message it sends
  var hudReady = false;
  var hudQueue = [];
  var drift = { raf: 0, keep: 0, t0: 0 };

  /* Magelight::Run(View::Hud, ...): straight through once the hud is ready, queued before. */
  function hudRun(fn) {
    if (hudReady && hudWin && hudWin.isekai) fn(hudWin.isekai);
    else hudQueue.push(fn);
  }

  function stopLabels() {
    if (drift.raf) cancelAnimationFrame(drift.raf);
    clearInterval(drift.keep);
    drift.raf = 0; drift.keep = 0;
  }

  function sendLabels(set) {
    hudRun(function (isekai) { isekai.labels.full(set); });
  }

  /* 60 Hz positions: each actor circles slowly around its spot and walks nearer and
     farther, so the distance scale and dimming change too. */
  function driftLabels(n) {
    stopLabels();
    var set = campLabels(n);
    sendLabels(set);
    drift.t0 = performance.now();
    function frame(now) {
      var k = (now - drift.t0) / 1000;
      var p = set.map(function (l, i) {
        var c = CAMP[i];
        var ph = c[7] * 6.283;
        var tt = Math.min(Math.max(c[7] + 0.18 * Math.sin(k * 0.6 + ph), 0), 1);
        return [l.id, c[5] + 0.03 * Math.cos(k * 0.9 + ph), c[6] + 0.02 * Math.sin(k * 1.3 + ph),
                1 - 0.32 * tt];
      });
      hudRun(function (isekai) { isekai.labels.pos(p); });
      drift.raf = requestAnimationFrame(frame);
    }
    drift.raf = requestAnimationFrame(frame);
  }

  /* A still camp: nothing moves, so only the heartbeat arrives (at most 4 Hz). */
  function holdLabels(n) {
    stopLabels();
    if (n) sendLabels(campLabels(n));
    drift.keep = setInterval(function () {
      hudRun(function (isekai) { isekai.labels.keep(); });
    }, 250);
  }

  function hudToast(text, key, banner, tone) {
    var d = { text: text, key: key || "", banner: !!banner };
    if (tone) d.tone = tone;
    hudRun(function (isekai) { isekai.toast(d); });
  }

  function later(ms, fn) { setTimeout(fn, ms); }

  /* A burst: a new objective (banner, keyed "quest"), three kills on one tally key (each
     replaces the line and restarts it), a craft payout, then the objective paid out —
     the same "quest" key, now gold, replacing the first banner in its slot. */
  function toastBurst() {
    hudToast("[ SYSTEM ]  New objective:  slay 3 Bandit", "quest", true);
    later(350, function () { hudToast("Bandit   1 / 3", "tally"); });
    later(800, function () { hudToast("Bandit   2 / 3", "tally"); });
    later(1250, function () { hudToast("Bandit   3 / 3", "tally"); });
    later(1500, function () { hudToast("[ SYSTEM ]  Craft recognised  —  +1 SP"); });
    later(1900, function () {
      hudToast("[ SYSTEM ]  Objective complete.  +5 System Points", "quest", true, "gold");
    });
  }

  /* ======================================================================
     Panel scenarios — the real strings from src/System.cpp
     ====================================================================== */

  var DORMANT_HERO = 25, DORMANT_ASCENDED = 80;   // ini defaults

  function panel(title, body, buttons, tone, extra) {
    var p = { title: title, body: body, reveal: 45, width: 720, tone: tone || "cyan",
              buttons: buttons || CONTINUE };
    for (var k in (extra || {})) p[k] = extra[k];
    return p;
  }
  var CONTINUE = [{ label: "CONTINUE", icon: "", iconOnly: false }];
  var TIERS3 = [
    { label: "NORMAL",   icon: "blessing_normal.png",   iconOnly: false },
    { label: "HERO",     icon: "blessing_hero.png",     iconOnly: false },
    { label: "ASCENDED", icon: "blessing_ascended.png", iconOnly: false }
  ];

  function storyPanel(mode) {
    return panel("[ SYSTEM ]",
      "REINCARNATION COMPLETE\n\n  Power level   ASCENDED" + (mode ? "  (" + mode + ")" : "") + "\n" +
      (mode === "DORMANT" ? "  Awakens at    level " + DORMANT_HERO + "\n" : "") +
      "\nThe System is now bound to your soul.\nYour new life begins.",
      CONTINUE, "gold");   // the blessing has landed: gold, as System.cpp sends it
  }

  /* The reboot confirmation (Screens.cpp ConfirmReboot): red, CANCEL goes back to the
     status screen, REBOOT re-opens the blessing choice. Escape and pad B answer CANCEL. */
  var REBOOT_TEXT =
    "REBOOT THE SYSTEM?\n\nRe-opens the blessing choice, so you can pick a different path\n" +
    "(tier, Full/Shattered, Dormant).\n\n" +
    "KEPT:    milestones, System Points, storage. The skill\n" +
    "         tree is refunded, not lost — every point spent\n" +
    "         on it comes back to spend again.\n" +
    "UNDONE:  the skills and attributes a previous blessing\n" +
    "         granted — you return to the mortal you were\n" +
    "         when the System first bound itself. Your\n" +
    "         character level is left alone.\n" +
    "NOTE:    gold and perk points already spent stay spent.\n" +
    "         A character from before this update has no such\n" +
    "         record, and keeps what it was given.";
  function rebootConfirm() {
    open("panel", panel("[ SYSTEM ]", REBOOT_TEXT, [
      { label: "CANCEL", icon: "", iconOnly: false },
      { label: "REBOOT", icon: "", iconOnly: false }
    ], "red", { cancel: 0 }), function (i) {
      if (i === 1) SCENARIOS.filter(function (s) { return s.id === "blessing"; })[0].run();
      else showStatus();
    });
  }

  var CUSTOM_STEPS = [
    "CUSTOM — 1 of 3: STARTING GIFT\n\nThe flat head start handed over at birth — skills, body,\n" +
    "gold and seed points.\n\n  NORMAL    Nothing. Begin as any mortal.\n  HERO      A hero's start.\n" +
    "  ASCENDED  A transcendent's fortune.",
    "CUSTOM — 2 of 3: REWARD PACE\n\nHow fast the System keeps paying out — milestone\n" +
    "passives, System Points, dragon souls, perk points.\n\n  NORMAL    x1. The honest grind.\n" +
    "  HERO      x2.\n  ASCENDED  x4.",
    "CUSTOM — 3 of 3: SKILL TREE\n\nHow deep the tree opens. You still buy each node\n" +
    "with System Points, so a deep tree on a NORMAL\npace is access, not a free ride.\n\n" +
    "  NORMAL    The self-made half.\n  HERO      + the Omniscience gifts.\n" +
    "  ASCENDED  + the World Tree capstone."
  ];

  function customStep(i) {
    // The CUSTOM builder: violet, Logic::ToneFor(Blessing::Custom).
    open("panel", panel("[ SYSTEM ]", CUSTOM_STEPS[i], TIERS3, "violet"), function () {
      if (i < 2) customStep(i + 1); else open("panel", storyPanel("CUSTOM"));
    });
  }

  function pathPanel(dormant) {
    var body = dormant
      ? "The dormant blessing stirs.\nHow will each awakening reach you?\n\n" +
        "  FULL       Every awakening arrives whole —\n             skills, body and fortune granted\n" +
        "             the moment it comes.\n  SHATTERED  Only the System's reach grows: faster\n" +
        "             rewards and deeper gifts unlock, but\n             nothing is ever handed to you.\n\n" +
        "Choose how you rise:"
      : "The ASCENDED blessing resonates.\nHow will you receive it?\n\n" +
        "  FULL       Awaken at once — skills, body and\n             fortune granted now.\n" +
        "  SHATTERED  The System is fractured. Begin as any\n             mortal, but its rewards still flow faster\n" +
        "             and its deepest gifts stay open to you.\n             Higher ceiling, same floor — earn it.\n\n" +
        "Choose how you rise:";
    return panel("[ SYSTEM ]", body, [
      { label: "FULL AWAKENING", icon: "blessing_full.png",      iconOnly: false },
      { label: "SHATTERED",      icon: "blessing_shattered.png", iconOnly: false, tone: "red" }
    ], dormant ? "violet" : "cyan");   // violet when dormant, else cyan; only SHATTERED red, as System.cpp
  }

  var SCENARIOS = [
    { id: "story", group: "Panel", label: "Story", hint: "Single CONTINUE, typewriter: REINCARNATION COMPLETE, gold",
      run: function () { open("panel", storyPanel("")); } },
    { id: "blessing", group: "Panel", label: "Blessing",
      hint: "The real blessing choice: NORMAL or TRANSMIGRATOR, then four gifts and BACK, gold",
      run: function blessing() {
        open("panel", panel("[ SYSTEM ]",
          "You have been reincarnated.\nThe System offers you a blessing.\n\n" +
          "  NORMAL         No blessing. Pure challenge —\n                 the start for a first life.\n" +
          "  TRANSMIGRATOR  Arrive with a gift: HERO, ASCENDED,\n" +
          "                 a DORMANT blessing or a CUSTOM build.\n\nChoose your path:",
          [
            { label: "NORMAL",        icon: "blessing_normal.png", iconOnly: false },
            { label: "TRANSMIGRATOR", icon: "blessing_hero.png",   iconOnly: false }
          ], "gold"),
          function (i) {
            if (i === 0) { open("panel", storyPanel("")); return; }
            open("panel", panel("[ SYSTEM ]",
              "A transmigrator arrives with a gift.\n\n  HERO      Awakened power.\n" +
              "  ASCENDED  Transcend mortal limits.\n  DORMANT   The blessing sleeps. Begin as a mortal;\n" +
              "            it wakes to HERO at level " + DORMANT_HERO + ", then to\n" +
              "            ASCENDED at level " + DORMANT_ASCENDED + ".\n" +
              "  CUSTOM    Set the pieces yourself — starting gift,\n" +
              "            reward pace and skill-tree depth, each on\n            its own.\n\nChoose your gift:",
              [
                { label: "HERO",     icon: "blessing_hero.png",     iconOnly: false },
                { label: "ASCENDED", icon: "blessing_ascended.png", iconOnly: false },
                { label: "DORMANT",  icon: "blessing_dormant.png",  iconOnly: false },
                { label: "CUSTOM",   icon: "blessing_custom.png",   iconOnly: false },
                { label: "BACK" }
              ], "gold", { cancel: 4 }),
              function (j) {
                if (j === 4) blessing();
                else if (j === 3) customStep(0);
                else open("panel", pathPanel(j === 2), function () { open("panel", storyPanel(j === 2 ? "DORMANT" : "")); });
              });
          });
      } },
    { id: "cards", group: "Panel", label: "Blessing ×3", hint: "Three cards (CUSTOM step 1), violet",
      run: function () { customStep(0); } },
    { id: "awaken", group: "Panel", label: "Awakening", hint: "Two cards: FULL or SHATTERED, cyan with a red SHATTERED",
      run: function () { open("panel", pathPanel(false), function () { open("panel", storyPanel("")); }); } },
    { id: "dormant", group: "Panel", label: "Dormant", hint: "The dormant blessing stirs, violet",
      run: function () { open("panel", pathPanel(true), function () { open("panel", storyPanel("DORMANT")); }); } },
    { id: "reboot", group: "Panel", label: "Reboot", hint: "The reboot confirmation, red: CANCEL back to status",
      run: function () { rebootConfirm(); } },
    { id: "warning", group: "Panel", label: "Warning", hint: "A failing self-test, red",
      run: function () {
        open("panel", panel("[ SYSTEM ]",
          "SELF-TEST\n\n  magelight host    PASS  1.9.0\n  system view       PASS\n  hud view          PASS\n" +
          "  page ready        FAIL  page dev, plugin 3f2a9c1\n  interface icons   PASS  72 icons\n\n" +
          "The page and the plugin come from different builds.\nReinstall the mod so both match.",
          [{ label: "CLOSE", icon: "", iconOnly: false }], "red", { reveal: 120 }));
      } },
    { id: "emblem", group: "Panel", label: "Emblem", hint: "Crest + caption, icon actions top right",
      run: function () {
        open("panel", panel("[ STATUS ]",
          "  Power level   ASCENDED  (CUSTOM)\n  Milestones    7 / 79\n  System Points  " + G.points + "\n\n" +
          "The System watches. Spend what it gives you.",
          [
            { label: "Skill tree", icon: "ui_skilltree.png", iconOnly: true },
            { label: "Storage",    icon: "ui_storage.png",   iconOnly: true },
            { label: "Shop",       icon: "ui_shop.png",      iconOnly: true },
            { label: "CLOSE",      icon: "",                 iconOnly: false }
          ], "cyan",
          { reveal: 200, emblem: { icon: "icons/rank_s.png", caption: "Rank S", tone: "gold" } }));
      } },
    { id: "status", group: "Status", label: "Custom", screen: "status",
      hint: "ASCENDED (CUSTOM), rank C: blessing violet, the three dials spelled out",
      run: function () { onSelect = null; showStatus("custom"); } },
    { id: "status-s", group: "Status", label: "S rank", screen: "status",
      hint: "Rank S and ASCENDED, both gold; all 79 titles (the scroll stress case)",
      run: function () { onSelect = null; showStatus("s"); } },
    { id: "status-hero", group: "Status", label: "Hero", screen: "status",
      hint: "HERO, rank B: everything cyan",
      run: function () { onSelect = null; showStatus("hero"); } },
    { id: "status-shattered", group: "Status", label: "Shattered", screen: "status",
      hint: "ASCENDED (SHATTERED): blessing red",
      run: function () { onSelect = null; showStatus("shattered"); } },
    { id: "status-dormant", group: "Status", label: "Dormant", screen: "status",
      hint: "NORMAL (DORMANT), awakens at 25, the System quiet between objectives: blessing violet",
      run: function () { onSelect = null; showStatus("dormant"); } },
    { id: "tree", group: "Screens", label: "Skill tree", screen: "tree",
      run: function () { open("tree", buildTree()); } },
    { id: "shop", group: "Screens", label: "Shop", screen: "shop",
      hint: "The dearest goods of a shelf are gold",
      run: function () { open("shop", buildShop()); } },
    { id: "shop-low", group: "Screens", label: "Shop 12 SP", screen: "shop",
      hint: "Sets the balance to 12 SP: the crates and the hoard cannot be afforded",
      run: function () { G.points = 12; changed(); open("shop", buildShop()); } },

    /* The hud view. Plays over whatever the system view shows (it is the Popup layer). */
    { id: "hud-toasts", group: "Hud", label: "Toasts", screen: "hud",
      hint: "A burst: objective banner, a kill tally replacing itself under one key, a payout, the objective paid out in gold",
      run: function () { toastBurst(); } },
    { id: "hud-banner", group: "Hud", label: "Banners", screen: "hud",
      hint: "A gold payout banner, a red banner and a cyan line at once",
      run: function () {
        hudToast("[ SYSTEM ]  Objective complete.  +5 System Points", "quest", true, "gold");
        later(150, function () { hudToast("[ SYSTEM ]  Self-test: page and plugin builds differ", "", true, "red"); });
        later(300, function () { hudToast("Draugr   4 / 6", "tally"); });
      } },
    { id: "hud-flourish", group: "Hud", label: "Flourish", screen: "hud",
      hint: "The level-up flourish, gold (AWAKENED / HERO)",
      run: function () { hudRun(function (isekai) { isekai.flourish({ title: "AWAKENED", subtitle: "HERO" }); }); } },
    { id: "hud-flourish-gm", group: "Hud", label: "Grandmaster", screen: "hud",
      hint: "A mastery reaching Grandmaster: the node's name as the subtitle",
      run: function () { hudRun(function (isekai) { isekai.flourish({ title: "GRANDMASTER", subtitle: "Vital Surge" }); }); } },
    { id: "hud-labels", group: "Hud", label: "Labels", screen: "hud",
      hint: "Twelve threat labels drifting, positions pushed at 60 Hz",
      run: function () { driftLabels(12); } },
    { id: "hud-labels-still", group: "Hud", label: "Still", screen: "hud",
      hint: "Eight labels, nothing moving: only the keep() heartbeat (4 Hz)",
      run: function () { holdLabels(8); } },
    { id: "hud-stall", group: "Hud", label: "Stall", screen: "hud",
      hint: "No label call at all (free camera, a pause): the page hides them after 600 ms",
      run: function () { stopLabels(); } },
    { id: "hud-clear", group: "Hud", label: "Clear", screen: "hud",
      hint: "labels.full([]): what a pausing menu or the console sends",
      run: function () { stopLabels(); sendLabels([]); } }
  ];

  /* A sample translation, partial on purpose (that is what real ones are). Switching
     it while a screen is up re-renders that screen in place, as a settings reload does. */
  var LOC_DE = {
    "view.panel.skip": "Klicken oder Enter zum Überspringen",
    "view.close": "Schließen",
    "threat.unknown": "Unbekannt",
    "view.hud.scan": "Erfassung"
  };

  /* ======================================================================
     The fake host
     ====================================================================== */

  var frame = null;          // the <iframe> holding system.html
  var view = null;           // its window, known from the first message it sends
  var ready = false;         // isekai.ready seen
  var queue = [];            // calls made before ready, in order
  var onSelect = null;       // what the open panel's choice continues with
  var last = null;           // the last screen shown, for the tone override
  var opts = { lang: "en", scale: 1, tone: "", build: "", instant: false, vr: false };
  var listeners = [];

  function log(m) { if (global.VS_LOG) global.VS_LOG(m); }
  function changed() { listeners.forEach(function (f) { f(); }); }

  /* Magelight::Run: straight through once ready, queued before. */
  function run(fn) {
    if (ready && view && view.isekai) fn(view.isekai);
    else queue.push(fn);
  }

  /* The hud's side of the host: only isekai.ready is ever expected from it. */
  function isHud(win) {
    try { return /\/hud\.html$/.test(win.location.pathname); } catch (e) { return false; }
  }
  function hudReceive(win, name, payload) {
    hudWin = win;
    if (name !== "isekai.ready") { log("hud sent " + name + " " + payload + "  (the plugin listens for ready only)"); return; }
    hudReady = true;
    log("hud ready, page build " + payload);
    win.isekai.loc(opts.lang === "de" ? LOC_DE : {});
    win.isekai.scale(opts.scale);
    if (opts.vr) win.isekai.vr(true);
    var q = hudQueue; hudQueue = [];
    q.forEach(function (fn) { fn(win.isekai); });
    changed();
  }

  /* The prelude C++ sends on ready, before anything queued (Magelight::SetPrelude). */
  function prelude(isekai) {
    isekai.loc(opts.lang === "de" ? LOC_DE : {});
    isekai.scale(opts.scale);
  }

  function show(screen, payload) {
    last = { screen: screen, payload: payload };
    var p = payload;
    if (screen === "panel" && (opts.tone || opts.instant)) {
      p = {};
      for (var k in payload) p[k] = payload[k];
      if (opts.tone) p.tone = opts.tone;
      if (opts.instant) p.reveal = 1000000;
    }
    if (frame) frame.classList.remove("is-hidden");
    run(function (isekai) { isekai.show(screen, p); });
  }

  /* What a screen-opening call site does in C++: remember the continuation, show. */
  function open(screen, payload, then) {
    onSelect = then || null;
    show(screen, payload);
  }

  /* ReleaseUIMode: Magelight hides the view. */
  function hide() { if (frame) frame.classList.add("is-hidden"); }

  /* Re-push the data screen on display, if it is one of `screens` and the view is up. */
  function repush(screens) {
    if (!last || screens.indexOf(last.screen) < 0) return;
    if (!frame || frame.classList.contains("is-hidden")) return;
    if (last.screen === "tree") show("tree", buildTree());
    else if (last.screen === "shop") show("shop", buildShop());
    else if (last.screen === "status") showStatus();
  }

  /* Screens::OnAction, by the same "type:arg" grammar UiLogic::ParseAction reads. */
  function onAction(text) {
    var colon = text.indexOf(":");
    var type = colon < 0 ? text : text.slice(0, colon);
    var arg = colon < 0 ? "" : text.slice(colon + 1);
    log("action " + text);
    switch (type) {
      case "choose": {
        var fn = onSelect;
        onSelect = null;
        hide();
        if (fn) fn(parseInt(arg, 10));
        break;
      }
      case "close": hide(); break;
      case "nav":
        if (arg === "tree") show("tree", buildTree());
        else if (arg === "shop") show("shop", buildShop());
        else if (arg === "status") showStatus();
        break;
      case "status":
        if (arg === "tree") show("tree", buildTree());
        else if (arg === "shop") show("shop", buildShop());
        else if (arg === "reroll") showStatus();   // a new objective: the ledger is re-pushed
        else if (arg === "reboot") rebootConfirm();   // swaps in, as the plugin does
        else if (arg === "close" || arg === "storage") hide();
        break;
      case "buy": tryUnlock(parseInt(arg, 10) || 0); show("tree", buildTree()); break;
      case "respec": doRespec(); show("tree", buildTree()); break;
      case "shopBuy": shopBuy(parseInt(arg, 10)); break;
      default: log("  (the plugin would reject this action)");
    }
    changed();
  }

  var Mock = {
    SCENARIOS: SCENARIOS,

    /* Called by the injected window.magelight.send in the page. */
    receive: function (win, name, payload) {
      // Which view is talking: the hud page by its path (its ready can arrive before the
      // harness has attached its frame), the system page otherwise.
      if (isHud(win)) { hudReceive(win, name, payload); return; }
      view = win;
      if (name === "isekai.ready") {
        opts.build = payload;
        ready = true;
        log("ready, page build " + payload);
        prelude(win.isekai);
        var q = queue; queue = [];
        q.forEach(function (fn) { fn(win.isekai); });
        changed();
      } else if (name === "isekai.action") {
        onAction(payload);
      } else if (name === "__escapecapture") {
        log("escape " + (payload === "1" ? "captured by the page" : "closes the view"));
      } else {
        log("unknown message " + name + " " + payload);
      }
    },

    /* The page's frame. Messages it sent before this script could take them wait in
       its __mlQueue (see serve.mjs). */
    attach: function (iframeEl) {
      frame = iframeEl;
      var w = frame.contentWindow;
      var early = (w && w.__mlQueue) || [];
      if (w) w.__mlQueue = [];
      early.forEach(function (m) { Mock.receive(w, m[0], m[1]); });
    },

    /* The hud view's frame, the same way. A reload of it (a new window) starts over:
       not ready until it says so, as Magelight.cpp resets the gate on ViewReloaded. */
    attachHud: function (iframeEl) {
      hudFrame = iframeEl;
      var w = hudFrame.contentWindow;
      var early = (w && w.__mlQueue) || [];
      if (w) w.__mlQueue = [];
      early.forEach(function (m) { Mock.receive(w, m[0], m[1]); });
    },

    open: function (id, e) {
      for (var i = 0; i < SCENARIOS.length; i++) {
        if (SCENARIOS[i].id === id) { SCENARIOS[i].run(e); return; }
      }
    },

    /* The screens the page has a renderer for (isekai.screens). */
    supports: function (screen) {
      if (screen === "hud") return hudReady;
      return !!(view && view.isekai && view.isekai.screens &&
                view.isekai.screens.indexOf(screen) >= 0);
    },

    /* Re-show the last panel in another tone (review aid; in game C++ picks it). */
    setTone: function (tone) {
      opts.tone = tone;
      if (last && last.screen === "panel") show(last.screen, last.payload);
    },
    setInstant: function (on) { opts.instant = !!on; },
    setLang: function (lang) {
      opts.lang = lang;
      run(function (isekai) { isekai.loc(lang === "de" ? LOC_DE : {}); });
      hudRun(function (isekai) { isekai.loc(lang === "de" ? LOC_DE : {}); });
    },
    setScale: function (s) {
      opts.scale = s;
      run(function (isekai) { isekai.scale(s); });
      hudRun(function (isekai) { isekai.scale(s); });
    },
    /* VR: the hud turns the labels into the head-locked scan list (in game Hud::Install
       pushes isekai.vr(true) on ready when REL::Module::IsVR()). */
    setVr: function (on) {
      opts.vr = !!on;
      hudRun(function (isekai) { isekai.vr(opts.vr); });
    },

    /* Game state. A change re-pushes the screen it shows on, as the plugin re-pushes after
       a purchase: the points reach every data screen, the rebirth tier and the dormant
       seal only the tree. */
    addPoints: function (n) { Mock.setPoints(G.points + n); },
    setPoints: function (n) { G.points = Math.max(0, n | 0); repush(["tree", "shop", "status"]); changed(); },
    setTier: function (t) { G.tier = t; repush(["tree"]); },
    getTier: function () { return G.tier; },
    setDormant: function (b) { G.dormant = !!b; repush(["tree"]); },
    getDormant: function () { return G.dormant; },
    reset: function () { G = freshState(); repush(["tree", "shop", "status"]); changed(); },

    onChange: function (f) { listeners.push(f); },
    state: function () { return { points: G.points, ready: ready, build: opts.build }; }
  };

  global.IsekaiMock = Mock;
})(window);
