/* ============================================================================
   Isekai Hero — the shop screen (loaded by system.html)
   ----------------------------------------------------------------------------
   Payloads::Shop(): shelves, goods cards with price and affordability (the dearest
   goods of a shelf in gold), and the readout of what a card does.

   A factory, run once by system.html with the page's shared helpers (core); it
   returns the screen's registry entry. Nothing here runs at load time.
   ========================================================================== */
(function (window) {
  "use strict";
  const defs = window.isekaiScreens = window.isekaiScreens || {};

  defs.shop = function (core) {
    const { ui, $, esc, iconUrl, t, tf, closeScreen, focusedId, refocus } = core;

    /*
       Payloads::Shop(): the balance, the shelves in order, and the catalog. The index sent
       back is the item's place in the WHOLE catalog, which is what Shop::Buy resolves; a
       per-shelf index would buy whatever sits at that offset in the full list. */
    let SHOP = { points: 0, shelves: [], items: [] };
    let shelf = 0;          // the shelf on display; survives the re-push after a purchase
    let shopHover = -1;

    /* Top tier, shown gold: the dearest goods of a shelf whose prices differ (the crates,
       the hoard). A shelf of one price has no top tier. Decided from the catalog's own
       prices, so a price change in Shop.cpp moves the gold with it. */
    function topTierOf(items) {
      const hi = Object.create(null), lo = Object.create(null);
      items.forEach((it) => {
        const c = +it.cost || 0, s = String(it.shelf);
        if (!(s in hi) || c > hi[s]) hi[s] = c;
        if (!(s in lo) || c < lo[s]) lo[s] = c;
      });
      return (it) => { const s = String(it.shelf); return hi[s] > lo[s] && (+it.cost || 0) === hi[s]; };
    }

    function canAfford(it) { return (+SHOP.points || 0) >= (+it.cost || 0); }

    function renderShop(data, fresh) {
      SHOP = data;
      if (fresh) { shelf = 0; shopHover = -1; }
      const items = Array.isArray(SHOP.items) ? SHOP.items : [];
      const shelves = Array.isArray(SHOP.shelves) ? SHOP.shelves : [];
      shelf = Math.max(0, Math.min(shelf, shelves.length - 1));
      $("shPts").textContent = +SHOP.points || 0;

      const rail = $("shRail"), grid = $("shGrid");
      const railWas = focusedId(rail), gridWas = focusedId(grid);
      // A re-push of the shelf on screen (a purchase) keeps the scroll; a new shelf starts at the top.
      const scrollWas = !fresh && grid.dataset.shelf === String(shelf) ? grid.scrollTop : 0;
      rail.innerHTML = "";
      shelves.forEach((name, z) => {
        const held = items.filter((it) => it.shelf === name).length;
        const b = document.createElement("button");
        b.className = "shelf" + (z === shelf ? " on" : "");
        b.dataset.id = "s" + z;
        b.innerHTML = "<span>" + esc(name) + "</span><b>" + held + "</b>";
        b.onclick = () => { shelf = z; shopHover = -1; renderShop(SHOP, false); };
        rail.appendChild(b);
      });
      rail.classList.toggle("hidden", shelves.length < 2);

      const active = shelves[shelf];
      const top = topTierOf(items);
      grid.innerHTML = "";
      grid.dataset.shelf = String(shelf);
      const visible = items.map((it, i) => ({ it: it, i: i })).filter((v) => !active || v.it.shelf === active);
      if (!visible.length) {
        grid.innerHTML = '<div class="shEmpty mono">' + esc(items.length ? t("view.shelfEmpty", "Nothing on this shelf yet.")
                                                                        : t("view.catalogEmpty", "The catalog is empty.")) + "</div>";
      }
      visible.forEach((v) => {
        const it = v.it, i = v.i, afford = canAfford(it);
        const b = document.createElement("button");
        b.className = "card ware" + (afford ? "" : " cant");
        b.dataset.id = String(i);
        if (top(it)) b.setAttribute("data-tone", "gold");
        const src = iconUrl(it.icon);
        b.innerHTML = (src ? '<span class="ico icon-plate"><img class="icon-lift" alt="" src="' + esc(src) + '"></span>' : "") +
                      '<span class="nm">' + esc(it.name) + "</span>" +
                      (it.qty ? '<span class="qty mono">' + esc(it.qty) + "</span>" : "") +
                      '<span class="price mono"' + (afford ? "" : ' data-tone="red"') + ">" +
                      esc(tf("view.spAmount", "{} SP", +it.cost || 0)) + "</span>";
        const img = b.querySelector("img");
        if (img) img.onerror = () => img.remove();
        b.addEventListener("mouseenter", () => { shopHover = i; showShopInfo(); });
        b.addEventListener("mouseleave", () => { shopHover = -1; showShopInfo(); });
        b.addEventListener("focus", showShopInfo);
        b.addEventListener("blur", () => setTimeout(showShopInfo, 0));
        b.onclick = () => { if (canAfford(it)) ui.send("shopBuy", i); };
        grid.appendChild(b);
      });
      grid.scrollTop = scrollWas;
      refocus(rail, railWas);
      refocus(grid, gridWas);
      showShopInfo();
    }

    function showShopInfo() {
      const items = SHOP.items || [];
      let i = shopHover;
      if (i < 0) {
        const a = document.activeElement;
        if (a && a.dataset && a.dataset.id !== undefined && $("shGrid").contains(a)) i = +a.dataset.id;
      }
      const it = items[i];
      $("shInfo").classList.toggle("empty", !it);
      if (!it) return;
      const afford = canAfford(it);
      $("shiName").textContent = it.name || "";
      const st = $("shiState");
      st.textContent = afford ? tf("view.shop.tipCost", "{} SP · {}", +it.cost || 0, it.qty || "")
                              : t("view.shop.tipCant", "Not enough System Points");
      st.setAttribute("data-tone", afford ? "cyan" : "red");
      // What it does: the plugin reads it off the potion's own record. Goods have none.
      $("shiDesc").textContent = it.fx || t("view.shop.noEffects", "No magical effect — goods, not a potion.");
    }

    function shopHome() {
      return $("shGrid").querySelector(".card") || $("shRail").querySelector(".shelf");
    }

    $("shBack").onclick = () => ui.send("nav", "status");
    $("shClose").onclick = closeScreen;

    return { render: renderShop, escape: () => false, onEscape: closeScreen, refresh: true, home: shopHome };
  };
})(window);
