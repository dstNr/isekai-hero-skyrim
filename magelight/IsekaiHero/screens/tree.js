/* ============================================================================
   Isekai Hero — the skill tree screen (loaded by system.html)
   ----------------------------------------------------------------------------
   Payloads::Tree(): the projected node net, the mastery rail, the readout that names
   cost, prerequisites and tier gate of whatever is pointed at, buy and respec.

   A factory, run once by system.html with the page's shared helpers (core); it
   returns the screen's registry entry. Nothing here runs at load time.
   ========================================================================== */
(function (window) {
  "use strict";
  const defs = window.isekaiScreens = window.isekaiScreens || {};

  defs.tree = function (core) {
    const { ui, $, esc, iconUrl, t, tf, closeScreen } = core;

    /*
       Payloads::Tree(): the header numbers and every node with its position (design
       units), next price, state flags, rank, mastery tier, zone, size, gate and
       prerequisites. The net is laid out from the data: the extents decide the scale,
       the zones are framed around their own nodes, so a node moved in SkillTree.cpp
       keeps a correct picture without touching this page. */
    const SVGNS = "http://www.w3.org/2000/svg";
    const NET_W = 720, NET_H = 560;          // the net's canvas in px, as in the markup
    const NODE_R = 34;                       // a scale-1 node's half-size, design units
    /* The always-on name under a node, in canvas px (not scaled: the text is not either).
       Must agree with .nlabel's width, margin and line-height. A node's footprint is the
       tile UNION this box, and the box is the wider of the two: the zone frames and the
       links reason about the footprint, or names hang out of frames and links run through
       their own parent's name. */
    const LABEL_W = 84, LABEL_GAP = 6, LABEL_LINE = 13, LABEL_LINES = 2;
    const LABEL_H = LABEL_GAP + LABEL_LINE * LABEL_LINES;
    const ZPAD_X = 10, ZPAD_TOP = 22, ZPAD_BOT = 10;   // zone frame padding, design units
    const ROMAN = ["", "I", "II", "III", "IV", "V"];
    const STATES = ["owned", "buyable", "poor", "sealed", "locked"];

    let TREE = { nodes: [] };
    let treeByKey = {};
    let treeSig = "";
    let nodeEls = {}, rowEls = {};
    let hoverKey = 0;
    let respecArmed = 0;

    /* Mastery nodes (capped repeatables: SkillTree's IsMasteryNode) live in the rail,
       not the net. Decided from the numbers, not from the zone name: the zone names
       arrive translated. */
    const isMastery = (n) => !!n.repeatable && +n.maxRank > 0;

    function stateOf(n) {
      if (n.owned) return "owned";
      const reachable = n.prereqMet && n.tierMet;
      if (reachable && (+TREE.points || 0) >= n.cost) return "buyable";
      if (reachable) return "poor";
      if (!n.tierMet) return "sealed";
      return "locked";
    }

    /* The one-line verdict for the readout: the same wording the old views used. */
    function statusOf(n) {
      const tag = n.masteryTier > 0
                ? tf("view.tree.tierTag", " · {} ({}/{})", n.masteryTierName, n.rank, n.maxRank)
                : n.repeatable
                ? (n.maxRank > 0 ? tf("view.tree.rankOf", " · Rank {}/{}", n.rank, n.maxRank)
                                 : tf("view.tree.rank", " · Rank {}", n.rank))
                : "";
      if (n.repeatable && n.maxRank > 0 && n.rank >= n.maxRank) {
        return { text: (n.masteryTier > 0 ? t("view.tree.grandmaster", "Grandmaster")
                                          : t("view.tree.maxed", "Maxed")) + tag, tone: "gold" };
      }
      if (n.owned) return { text: t("view.tree.unlocked", "Unlocked"), tone: "cyan" };
      // A dormant blessing seals by level (violet, its tone), a rebirth gate by the tier.
      if (!n.tierMet) {
        return n.reqLevel ? { text: tf("view.tree.sealedLevel", "Sealed — awakens at level {}", n.reqLevel), tone: "violet" }
                          : { text: tf("view.tree.sealedRebirth", "Sealed — needs {} rebirth", n.reqPower), tone: "red" };
      }
      if (!n.prereqMet) return { text: t("view.tree.locked", "Locked — needs a connected node"), tone: "", dim: true };
      const ok = (+TREE.points || 0) >= n.cost;
      return { text: tf("view.tree.cost", "{} system point(s)", n.cost) + tag, tone: ok ? "cyan" : "red" };
    }

    /* Uniform scale from the graph's own extents, centred. Asymmetric padding: a zone's
       caption sits above its topmost node and every node carries a name below it. */
    function treeLayout(nodes) {
      const graph = nodes.filter((n) => !isMastery(n));
      if (!graph.length) return null;
      let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
      graph.forEach((n) => {
        minX = Math.min(minX, n.x); maxX = Math.max(maxX, n.x);
        minY = Math.min(minY, n.y); maxY = Math.max(maxY, n.y);
      });
      const cw = Math.max(1, maxX - minX), ch = Math.max(1, maxY - minY);
      const padX = 50, padTop = 56, padBot = 74;
      const k = Math.min((NET_W - padX * 2) / cw, (NET_H - padTop - padBot) / ch);
      return { k: k,
               ox: (NET_W - cw * k) / 2 - minX * k,
               oy: padTop + (NET_H - padTop - padBot - ch * k) / 2 - minY * k };
    }
    const posOf = (L, n) => ({ x: L.ox + n.x * L.k, y: L.oy + n.y * L.k });
    const radOf = (L, n) => NODE_R * (+n.scale || 1) * L.k;

    function nodeBox(L, n) {
      const p = posOf(L, n), r = radOf(L, n), hw = Math.max(r, LABEL_W / 2);
      return { x0: p.x - hw, x1: p.x + hw, y0: p.y - r, y1: p.y + r + LABEL_H };
    }

    /* One padded frame per zone around its own nodes. The core is the zone of the root
       (the node without prerequisites); the branches below it are siblings and share a
       top and a bottom, or the shorter one reads as a glitch rather than a group. */
    function zoneBoxes(L, graph) {
      const px = ZPAD_X * L.k, pt = ZPAD_TOP * L.k, pb = ZPAD_BOT * L.k;
      const root = graph.filter((n) => !(n.prereq || []).some((k) => k))[0];
      const order = [], boxes = {};
      graph.forEach((n) => {
        const z = String(n.zone || "");
        const b = nodeBox(L, n);
        if (!boxes[z]) { boxes[z] = { zone: z, x0: b.x0, x1: b.x1, y0: b.y0, y1: b.y1 }; order.push(z); return; }
        const o = boxes[z];
        o.x0 = Math.min(o.x0, b.x0); o.x1 = Math.max(o.x1, b.x1);
        o.y0 = Math.min(o.y0, b.y0); o.y1 = Math.max(o.y1, b.y1);
      });
      const coreZone = root ? String(root.zone || "") : "";
      const branches = order.filter((z) => z !== coreZone).map((z) => boxes[z]);
      if (branches.length > 1) {
        const top = Math.min.apply(null, branches.map((b) => b.y0));
        const bot = Math.max.apply(null, branches.map((b) => b.y1));
        branches.forEach((b) => { b.y0 = top; b.y1 = bot; });
      }
      return order.map((z) => {
        const b = boxes[z];
        return { zone: z, x: b.x0 - px, y: b.y0 - pt, w: (b.x1 - b.x0) + px * 2, h: (b.y1 - b.y0) + pt + pb };
      });
    }

    /* Pull point a toward b so a link stops at the tile edge instead of running under it. */
    function trimTo(a, b, r) {
      const dx = b.x - a.x, dy = b.y - a.y, len = Math.sqrt(dx * dx + dy * dy) || 1;
      return { x: a.x + dx / len * r, y: a.y + dy / len * r };
    }

    /* Where a link leaves its PARENT's footprint (tile plus name): trimming to the tile
       alone ran the first stretch of every link through the parent's own name. */
    function linkStart(L, from, to, gap) {
      const a = posOf(L, from), b = posOf(L, to);
      const dx = b.x - a.x, dy = b.y - a.y;
      const box = nodeBox(L, from);
      const hits = [];
      if (dx > 0) hits.push((box.x1 - a.x) / dx);
      if (dx < 0) hits.push((box.x0 - a.x) / dx);
      if (dy > 0) hits.push((box.y1 - a.y) / dy);
      if (dy < 0) hits.push((box.y0 - a.y) / dy);
      const tt = hits.length ? Math.min.apply(null, hits) : 0;
      const len = Math.hypot(dx, dy) || 1;
      const d = Math.min(tt * len + gap, len - radOf(L, to) - gap);
      return { x: a.x + dx / len * d, y: a.y + dy / len * d };
    }

    /* A link is straight unless a third node stands in its way; then it bows around it.
       By geometry rather than by special case, so a node added later cannot make the net
       claim a gate that does not exist. Returns the curve's control point, or null. */
    function bowAround(L, graph, a, b, from, to) {
      const dx = b.x - a.x, dy = b.y - a.y, len = Math.hypot(dx, dy) || 1;
      let worst = 0, side = 1;
      graph.forEach((n) => {
        if (n === from || n === to) return;
        const p = posOf(L, n), r = radOf(L, n);
        const u = Math.max(0, Math.min(1, ((p.x - a.x) * dx + (p.y - a.y) * dy) / (len * len)));
        const d = Math.hypot(p.x - (a.x + dx * u), p.y - (a.y + dy * u));
        const need = r + 10 - d;
        if (need > worst) {
          worst = need;
          side = (dx * (p.y - a.y) - dy * (p.x - a.x)) > 0 ? -1 : 1;
        }
      });
      if (worst <= 0) return null;
      const off = (worst + 14) * 2;   // a quadratic peaks at half its control offset
      return { x: (a.x + b.x) / 2 + (-dy / len) * off * side, y: (a.y + b.y) / 2 + (dx / len) * off * side };
    }

    function svgEl(tag, attrs, cls) {
      const el = document.createElementNS(SVGNS, tag);
      Object.keys(attrs).forEach((k) => el.setAttribute(k, attrs[k]));
      if (cls) el.setAttribute("class", cls);
      return el;
    }

    /* A chamfered square: the outline of the icon art's own frame. */
    function chamfer(p, h) {
      const c = h * 0.28;
      return [[p.x - h + c, p.y - h], [p.x + h - c, p.y - h], [p.x + h, p.y - h + c], [p.x + h, p.y + h - c],
              [p.x + h - c, p.y + h], [p.x - h + c, p.y + h], [p.x - h, p.y + h - c], [p.x - h, p.y - h + c]]
        .map((q) => q[0].toFixed(1) + "," + q[1].toFixed(1)).join(" ");
    }

    /* The SVG layer: cheap, so redrawn on every push. The buttons above it persist. */
    function drawNet(L, graph) {
      const zones = $("tZones"), links = $("tLinks"), frames = $("tFrames");
      zones.textContent = ""; links.textContent = ""; frames.textContent = "";
      if (!L) return;

      zoneBoxes(L, graph).forEach((b) => {
        const g = svgEl("g", {}, "zone");
        g.appendChild(svgEl("rect", { x: b.x.toFixed(1), y: b.y.toFixed(1), width: b.w.toFixed(1), height: b.h.toFixed(1) }));
        const label = svgEl("text", { x: (b.x + 8).toFixed(1), y: (b.y + 13).toFixed(1) });
        label.textContent = b.zone;
        g.appendChild(label);
        zones.appendChild(g);
      });

      graph.forEach((n) => (n.prereq || []).forEach((pk) => {
        const from = pk && treeByKey[pk];
        if (!from || isMastery(from)) return;
        const a = linkStart(L, from, n, 5), b = trimTo(posOf(L, n), posOf(L, from), radOf(L, n) + 5);
        const bow = bowAround(L, graph, a, b, from, n);
        const add = (cls) => {
          links.appendChild(bow
            ? svgEl("path", { d: "M " + a.x.toFixed(1) + " " + a.y.toFixed(1) + " Q " + bow.x.toFixed(1) + " " +
                                  bow.y.toFixed(1) + " " + b.x.toFixed(1) + " " + b.y.toFixed(1) }, "lnk " + cls)
            : svgEl("line", { x1: a.x.toFixed(1), y1: a.y.toFixed(1), x2: b.x.toFixed(1), y2: b.y.toFixed(1) }, "lnk " + cls));
        };
        if (from.owned && n.owned) { add("halo"); add("on"); }
        else add(from.owned ? "half" : "off");
      }));

      graph.forEach((n) => {
        const st = stateOf(n), p = posOf(L, n), h = radOf(L, n);
        const g = svgEl("g", {});
        if (sealTone(n)) g.setAttribute("data-tone", sealTone(n));
        if (st === "buyable") g.appendChild(svgEl("polygon", { points: chamfer(p, h + 4) }, "glow"));
        g.appendChild(svgEl("polygon", { points: chamfer(p, h) }, "frame " + st));
        frames.appendChild(g);
      });
    }

    /* A sealed node wears its gate's tone: violet while a dormant blessing sleeps (a level
       away), red behind a rebirth it has not reached. */
    function sealTone(n) {
      if (stateOf(n) !== "sealed") return "";
      return n.reqLevel ? "violet" : "red";
    }

    function treeBuy(key) {
      const n = treeByKey[key];
      if (n && stateOf(n) === "buyable") ui.send("buy", key);
    }

    function hookNode(el, key) {
      el.dataset.key = String(key);
      el.addEventListener("mouseenter", () => { hoverKey = key; showTreeInfo(); });
      el.addEventListener("mouseleave", () => { hoverKey = 0; showTreeInfo(); });
      el.addEventListener("focus", showTreeInfo);
      el.addEventListener("blur", () => setTimeout(showTreeInfo, 0));
      el.addEventListener("click", () => treeBuy(key));
    }

    function nodeImg(n) {
      const src = iconUrl(n.icon);
      return src ? '<img class="icon-lift" alt="" src="' + esc(src) + '">' : "";
    }

    function renderNode(L, n) {
      let el = nodeEls[n.key];
      if (!el) {
        el = document.createElement("button");
        el.className = "node";
        el.innerHTML = nodeImg(n) + '<span class="nlabel"></span>';
        const img = el.querySelector("img");
        if (img) img.onerror = () => img.remove();
        hookNode(el, n.key);
        $("tNodes").appendChild(el);
        nodeEls[n.key] = el;
      }
      // Swap only the state class: the element, and with it the pad's focus, survives.
      el.className = "node " + stateOf(n);
      if (sealTone(n)) el.setAttribute("data-tone", sealTone(n));
      else el.removeAttribute("data-tone");
      el.setAttribute("aria-label", n.name || "");
      el.querySelector(".nlabel").textContent = n.name || "";
      const p = posOf(L, n), h = radOf(L, n), inset = h * 0.22;
      el.style.left = (p.x - h + inset).toFixed(1) + "px";
      el.style.top = (p.y - h + inset).toFixed(1) + "px";
      el.style.width = el.style.height = (2 * (h - inset)).toFixed(1) + "px";
      // The label hangs below the TILE, not the inner icon box.
      el.querySelector(".nlabel").style.marginTop = (LABEL_GAP + inset).toFixed(1) + "px";

      let pip = el.querySelector(".pip");
      if (n.repeatable && n.rank > 0) {
        if (!pip) { pip = document.createElement("span"); pip.className = "pip"; el.appendChild(pip); }
        // Mastery tier as a numeral (I-V); Perk Synthesis, which has none, counts purchases.
        pip.textContent = n.masteryTier > 0 ? (ROMAN[n.masteryTier] || "") : String(n.rank);
      } else if (pip) {
        pip.remove();
      }
    }

    /* The mastery rail: icon, name, ten rank pips in five pairs (the tiers), and the tier
       reached or the next price. */
    function renderRow(n) {
      let row = rowEls[n.key];
      const max = Math.max(1, Math.min(+n.maxRank || 10, 20));
      if (!row || row.querySelectorAll(".pips i").length !== max) {
        if (row) row.remove();
        row = document.createElement("button");
        row.className = "mrow";
        let pips = "";
        for (let r = 0; r < max; r++) pips += (r % 2 === 1 && r !== max - 1) ? '<i class="sep"></i>' : "<i></i>";
        row.innerHTML = '<span class="mi">' + nodeImg(n) + '</span><span class="mb"><span class="mName"></span>' +
                        '<span class="mMeta"><span class="pips">' + pips + '</span><span class="mTier"></span></span></span>';
        const img = row.querySelector("img");
        if (img) img.onerror = () => img.remove();
        hookNode(row, n.key);
        $("tRows").appendChild(row);
        rowEls[n.key] = row;
      }
      row.className = "mrow " + stateOf(n);
      row.querySelector(".mName").textContent = n.name || "";
      const pipEls = row.querySelectorAll(".pips i");
      for (let r = 0; r < pipEls.length; r++) pipEls[r].classList.toggle("on", r < n.rank);
      const tier = row.querySelector(".mTier");
      tier.textContent = n.rank > 0 ? String(n.masteryTierName || "").toUpperCase()
                                    : tf("view.spAmount", "{} SP", n.cost);
      // The last tier is a rank-up worth marking: gold.
      tier.classList.toggle("top", n.masteryTier >= 5);
      if (n.masteryTier >= 5) tier.setAttribute("data-tone", "gold");
      else tier.removeAttribute("data-tone");
    }

    /* The readout: what the pointer is on, else what the pad is on. */
    function treeInfoKey() {
      if (hoverKey && treeByKey[hoverKey]) return hoverKey;
      const a = document.activeElement;
      return a && a.dataset && a.dataset.key && $("screen-tree").contains(a) ? +a.dataset.key : 0;
    }

    function fact(label, value, tone) {
      return '<span class="fact"><span class="k">' + esc(label) + '</span><span class="v"' +
             (tone ? ' data-tone="' + tone + '"' : "") + ">" + value + "</span></span>";
    }

    function showTreeInfo() {
      const n = treeByKey[treeInfoKey()];
      $("tInfo").classList.toggle("empty", !n);
      if (!n) return;
      $("tiName").textContent = n.name || "";
      const st = statusOf(n);
      const state = $("tiState");
      state.textContent = st.text;
      state.setAttribute("data-tone", st.tone || "cyan");
      state.classList.toggle("dim", !!st.dim);
      $("tiDesc").textContent = n.desc || "";

      // Cost, prerequisites, tier gate: each in red while it is what stands in the way.
      const done = n.owned || (n.repeatable && n.maxRank > 0 && n.rank >= n.maxRank);
      const cost = done ? "—" : esc(tf("view.spAmount", "{} SP", n.cost));
      const short = !done && (+TREE.points || 0) < n.cost;
      const reqs = (n.prereq || []).filter((k) => k && treeByKey[k]).map((k) => {
        const p = treeByKey[k];
        return '<span data-tone="' + (p.owned ? "cyan" : "red") + '" style="color:var(--tone)">' + esc(p.name) + "</span>";
      });
      const gate = n.reqLevel ? tf("view.tree.gateLevel", "Level {}", n.reqLevel)
                              : tf("view.tree.gateRebirth", "{} rebirth", n.reqPower);
      $("tiFacts").innerHTML =
        fact(t("view.tree.costLabel", "Cost"), cost, short ? "red" : "") +
        fact(t("view.tree.requires", "Requires"), reqs.length ? reqs.join(" + ") : esc(t("view.tree.none", "None")), "") +
        fact(t("view.tree.gate", "Gate"), esc(gate), n.tierMet ? "" : (n.reqLevel ? "violet" : "red"));
    }

    /* Respec: two presses within three seconds instead of a nested dialog. */
    function disarmRespec() {
      if (respecArmed) clearTimeout(respecArmed);
      respecArmed = 0;
    }
    function renderRespec() {
      const btn = $("tRespec");
      const amount = +TREE.respec || 0;
      btn.classList.toggle("hidden", amount <= 0);
      if (amount <= 0) disarmRespec();
      btn.classList.toggle("armed", !!respecArmed);
      btn.querySelector(".lbl").textContent = respecArmed ? t("view.confirm", "Confirm?")
                                                          : tf("view.respec", "Respec  +{}", amount);
    }
    function respecClick() {
      if (respecArmed) {
        disarmRespec();
        ui.send("respec");
      } else {
        respecArmed = setTimeout(() => { respecArmed = 0; renderRespec(); }, 3000);
      }
      renderRespec();
    }

    function treeHome() {
      const graph = (TREE.nodes || []).filter((n) => !isMastery(n));
      const pick = graph.filter((n) => stateOf(n) === "buyable")[0] || graph[0];
      return pick ? nodeEls[pick.key] : null;
    }

    function renderTree(data, fresh) {
      TREE = data;
      const nodes = Array.isArray(data.nodes) ? data.nodes.filter((n) => n && +n.key > 0) : [];
      treeByKey = {};
      nodes.forEach((n) => { treeByKey[n.key] = n; });
      if (fresh) { hoverKey = 0; disarmRespec(); }
      $("tPts").textContent = +data.points || 0;
      $("tPerks").textContent = (+data.perks || 0) + " / " + (+data.perkMax || 0);
      renderRespec();

      // A different node set starts clean; the same set updates in place.
      const sig = nodes.map((n) => n.key).join(",");
      if (sig !== treeSig) {
        $("tNodes").innerHTML = ""; $("tRows").innerHTML = "";
        nodeEls = {}; rowEls = {}; treeSig = sig;
      }
      const graph = nodes.filter((n) => !isMastery(n));
      const L = treeLayout(nodes);
      drawNet(L, graph);
      if (L) graph.forEach((n) => renderNode(L, n));
      nodes.filter(isMastery).forEach(renderRow);
      showTreeInfo();
    }

    $("tBack").onclick = () => ui.send("nav", "status");
    $("tClose").onclick = closeScreen;
    $("tRespec").onclick = respecClick;

    return { render: renderTree, escape: () => false, onEscape: closeScreen, refresh: true, home: treeHome };
  };
})(window);
