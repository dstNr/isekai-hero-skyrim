/* ============================================================================
   Isekai Hero — the status screen (loaded by system.html)
   ----------------------------------------------------------------------------
   BuildStatusJson (src/Progression.cpp): the System Rank with its insignia, the
   blessing (power tier and mode), milestones, points, the standing objective,
   attunement totals and the titles earned, and the ways on from there.

   A factory, run once by system.html with the page's shared helpers (core); it
   returns the screen's registry entry. Nothing here runs at load time.
   ========================================================================== */
(function (window) {
  "use strict";
  const defs = window.isekaiScreens = window.isekaiScreens || {};

  defs.status = function (core) {
    const { ui, $, esc, iconUrl, t, tf, TONES, toneOf, closing, focusedId, refocus } = core;

    /*
       BuildStatusJson (src/Progression.cpp): rank, power tier and mode, milestones,
       points, the standing objective, attunement totals and the titles earned. */

    /* The rank's tone. The plugin sends it as rankTone (Logic::ToneForRank); a plugin
       that does not yet gets the same rule, which has one case: S is gold. */
    function rankTone(s) {
      if (TONES[s.rankTone]) return s.rankTone;
      return String(s.rank || "").toUpperCase() === "S" ? "gold" : "cyan";
    }

    /* The blessing's tone (Logic::ToneFor(Blessing)) is the plugin's to decide; without
       it the blessing stays in the window's cyan rather than being guessed here. */
    function blessingTone(s) { return toneOf(s.blessingTone); }

    function setBar(el, part, whole) {
      const f = whole > 0 ? Math.max(0, Math.min(1, part / whole)) : 0;
      el.style.transform = "scaleX(" + f.toFixed(4) + ")";
    }

    function statusAction(a) {
      ui.send("status", a);
      // Tree and shop swap in place; reroll re-pushes this screen; reboot swaps in the
      // plugin's red confirmation panel (what is kept, what is undone), which is the one
      // confirmation it gets. Storage opens a game menu and close ends the view: those
      // leave this screen.
      if (a === "close" || a === "storage") closing();
    }

    function statusButton(box, id, label, icon, onClick) {
      const b = document.createElement("button");
      b.className = "btn";
      b.dataset.id = id;
      const src = icon ? iconUrl(icon) : "";
      b.innerHTML = (src ? '<img class="icon-lift" alt="" src="' + esc(src) + '">' : "") +
                    '<span class="lbl">' + esc(label) + "</span>";
      const img = b.querySelector("img");
      if (img) img.onerror = () => img.remove();
      b.onclick = () => onClick(b);
      box.appendChild(b);
      return b;
    }

    /* The ways on from the ledger, each with its own art. */
    function addIcon(label, icon, action) {
      statusButton($("sNav"), action, label, icon, () => statusAction(action));
    }

    function buildStatusActions(s) {
      const nav = $("sNav"), acts = $("sActions");
      const was = focusedId(nav) || focusedId(acts);
      nav.innerHTML = ""; acts.innerHTML = "";

      if (s.hasTree)    addIcon(t("view.skillTree", "Skill Tree"), "ui_skilltree.png", "tree");
      if (s.hasStorage) addIcon(t("view.storage", "Storage"), "ui_storage.png", "storage");
      if (s.hasShop)    addIcon(t("view.shop", "Shop"), "ui_shop.png", "shop");
      // New Task throws the objective away and rolls another, free and instantly: a testing
      // tool (QuestRerollButton in the ini, off by default). The plugin decides whether it
      // exists at all; the page only obeys.
      if (s.questCanReroll) statusButton(acts, "reroll", t("view.newTask", "New Task"), "", () => statusAction("reroll"));
      if (s.canReboot) {
        const rb = statusButton(acts, "reboot", t("view.reboot", "Reboot"), "", () => statusAction("reboot"));
        rb.setAttribute("data-tone", "red");
      }
      statusButton(acts, "close", t("view.close", "Close"), "", () => statusAction("close"));
      refocus(nav, was); refocus(acts, was);
    }

    function statusHome() {
      return $("sNav").querySelector(".btn") || $("sActions").querySelector(".btn");
    }

    function renderStatus(s) {
      // Rank: insignia and letter in the rank's tone. The plugin sends the letter only; the
      // file name is derived from it the way Progression::RankIcon does on the C++ side.
      const letter = String(s.rank || "E").toUpperCase();
      const rank = /^[A-Z]$/.test(letter) ? letter : "E";
      $("sRank").setAttribute("data-tone", rankTone(s));
      const icon = $("sRankIcon");
      const src = "icons/rank_" + rank.toLowerCase() + ".png";
      if (icon.getAttribute("src") !== src) {
        icon.style.display = "";          // a previous rank's missing PNG may have hidden it
        icon.onerror = () => { icon.style.display = "none"; };
        icon.src = src;
      }
      $("sRankLetter").textContent = tf("view.rankEmblem", "RANK {}", rank);

      // Blessing: the power tier, the mode as a tag, and what a dormant or custom blessing
      // needs said about it.
      $("sBless").setAttribute("data-tone", blessingTone(s));
      $("sTier").textContent = s.tier || "NORMAL";
      const mode = s.mode ? String(s.mode) : "";
      $("sMode").textContent = mode;
      $("sMode").classList.toggle("hidden", !mode);
      const awaken = +s.nextAwakening || 0;
      $("sAwaken").textContent = awaken > 0 ? tf("view.awakensAt", "Awakens at level {}", awaken) : "";
      $("sAwaken").classList.toggle("hidden", !(awaken > 0));
      // A CUSTOM build's three dials rarely agree, so they are spelled out. The labels are
      // translated text: built as nodes, never as markup.
      const cu = $("sCustom");
      cu.innerHTML = "";
      cu.classList.toggle("hidden", mode !== "CUSTOM");
      if (mode === "CUSTOM") {
        [[t("view.custom.rewards", "Rewards"), s.rewardTier || s.tier],
         [t("view.custom.tree", "Tree"), s.treeTier || s.tier],
         [t("view.custom.gift", "Gift"), s.grantTier || "NORMAL"]].forEach((pair, i) => {
          if (i) cu.appendChild(document.createTextNode("  ·  "));
          cu.appendChild(document.createTextNode(pair[0] + " "));
          const b = document.createElement("b");
          b.textContent = pair[1] || "";
          cu.appendChild(b);
        });
      }
      const level = +s.level || 0;
      $("sLevel").classList.toggle("hidden", !(level > 0));
      $("sLevelNum").textContent = level > 0 ? String(level) : "";

      const earned = +s.milestonesEarned || 0, total = +s.milestonesTotal || 0;
      $("sMile").textContent = earned;
      $("sMileTot").textContent = " / " + total;
      setBar($("sBar"), earned, total);
      $("sPts").textContent = +s.points || 0;

      // The standing objective. Between objectives the System stands down for a while, so
      // an empty questText with hours on the clock means "waiting" and has to say so: an
      // idle System must not look like a broken one. Both empty: no work at all.
      const qText = s.questText ? String(s.questText) : "";
      const qWait = +s.questWaitHours || 0;
      $("sQuestSect").classList.toggle("hidden", !qText && !qWait);
      if (qText) {
        const qp = +s.questProgress || 0, qt = +s.questTarget || 0;
        $("sQuestText").textContent = qText;
        $("sQuestCount").innerHTML = esc(qp + " / " + qt) + '<span class="rew">' +
          esc(tf("view.questReward", "+{} SP", +s.questReward || 0)) + "</span>";
        setBar($("sQuestBar"), Math.min(qp, qt), qt);
      } else if (qWait) {
        $("sQuestText").textContent = t("view.systemQuiet", "The System is quiet.");
        $("sQuestCount").innerHTML = '<span class="rew">' + esc(tf("view.questNextIn", "next in ~{}h", qWait)) + "</span>";
        setBar($("sQuestBar"), 0, 1);
      }

      const att = Array.isArray(s.attunements) ? s.attunements : [];
      $("sAtt").innerHTML = att.map((a) =>
        '<div class="att"><span>' + esc(a.stat) + "</span><b>+" + esc(+a.amount || 0) + "</b></div>").join("");
      $("sAttSect").classList.toggle("hidden", !att.length);

      const titles = Array.isArray(s.titles) ? s.titles : [];
      $("sTitleCount").textContent = titles.length ? String(titles.length) : "";
      $("sTitles").innerHTML = titles.length
        ? titles.map((x) => '<div class="trow"><span class="tName">' + esc(x.name) + '</span><span class="tEff">' +
                            esc(x.effect) + '</span><span class="tQuest">' + esc(x.quest) + "</span></div>").join("")
        : '<div class="trow empty">' + esc(t("view.noDeeds", "No deeds recognised yet.")) + "</div>";

      buildStatusActions(s);
    }

    return { render: renderStatus, escape: () => false, onEscape: () => statusAction("close"),
             refresh: true, home: statusHome };
  };
})(window);
