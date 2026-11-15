/* ============================================================================
   Isekai Hero — helpers shared by every Magelight page (window.isekaiUi)
   ----------------------------------------------------------------------------
   The plugin side of every call here is src/UI/Magelight.cpp:
     - Page to plugin: window.magelight.send(name, stringPayload). The system view
       listens for "isekai.ready" (payload: the page's build id) and
       "isekai.action" (payload: "type" or "type:arg", parsed by
       UiLogic::ParseAction). "__escapecapture" is Magelight's own: "1" makes
       Escape a key the page receives instead of the key that closes the view.
     - Plugin to page: the page's own globals (isekai.*), never anything here.

   Plain ES5-compatible script with no DOM work at load time, so tools/check.mjs
   can run it against a fake window and document and check what it sends.
   ========================================================================== */
(function (window, document) {
  "use strict";

  var ui = {};

  /* ---- host messages ---------------------------------------------------- */

  /* The host injects window.magelight before the page runs. Looked up on every
     send rather than captured once, so a host that arrives late (the playground's
     shim, a reload) is still found. Without one the message goes to the console:
     a page opened in a plain browser must not throw on its first click. */
  function post(name, payload) {
    var host = window.magelight;
    if (host && typeof host.send === "function") {
      host.send(name, String(payload));
    } else if (window.console) {
      window.console.log("[isekai] no host, not sent: " + name + " " + payload);
    }
  }

  /* An action for the plugin: "choose:2", "status:tree", "close". The plugin
     rejects unknown types and malformed arguments, so the shape matters exactly. */
  ui.send = function (type, arg) {
    post("isekai.action", arg === undefined ? type : type + ":" + arg);
  };

  /* The ready handshake. The plugin queues every call until this arrives and
     compares the build id with its own (spec 6.4). version.js may be missing
     (a hand-copied page) or end in CRLF; neither matters, the id is read as the
     global it defines, never parsed from the file. */
  ui.ready = function () {
    post("isekai.ready", window.ISEKAI_UI_BUILD || "dev");
  };

  /* Take Escape (decision panels: a stray Escape must not dismiss a choice the
     game is waiting for) or hand it back to Magelight, which then closes the view. */
  ui.escapeCapture = function (on) {
    post("__escapecapture", on ? "1" : "0");
  };

  /* ---- translation -------------------------------------------------------
     The pages' own chrome ("Close", "Skip") never passes through C++, so it
     cannot go through Loc::Get. The plugin pushes the whole loaded string table
     instead (isekai.loc → setLoc); t() does the same lookup the C++ side does,
     with the same rule: a key that is missing stays English, because the English
     sits at the call site here too.

     Static labels carry data-loc="key" and keep their English as their own text.
     setLoc() stashes that original text before the first rewrite, so a second
     push translates from the English, not from the previous language.

     Call sites must pass a literal key: tools/extract-strings.mjs harvests them
     for lang/template.txt and refuses a computed one. */
  var LOC = {};

  function lookup(key, english) {
    var s = LOC[key];
    return (typeof s === "string" && s) ? s : english;
  }

  function t(key, english) { return lookup(key, english); }

  /* The same for a string with values in it, mirroring LF() in C++: each {} is
     filled from the arguments in order, so a translation can move the value. */
  function tf(key, english) {
    var args = Array.prototype.slice.call(arguments, 2);
    var i = 0;
    return lookup(key, english).replace(/\{\}/g, function () {
      return i < args.length ? String(args[i++]) : "{}";
    });
  }

  function applyLoc() {
    var els = document.querySelectorAll("[data-loc]");
    for (var i = 0; i < els.length; i++) {
      var el = els[i];
      if (el.dataset.locEn === undefined) el.dataset.locEn = el.textContent;
      el.textContent = lookup(el.dataset.loc, el.dataset.locEn);
    }
  }

  ui.t = t;
  ui.tf = tf;
  ui.applyLoc = applyLoc;
  ui.setLoc = function (table) {
    LOC = (table && typeof table === "object") ? table : {};
    applyLoc();
  };

  /* ---- UiScale -----------------------------------------------------------
     Clamped here as well as in C++: a value arriving from outside the page, and
     a 0 would make the whole interface vanish with no way left to reach the
     setting that did it. */
  ui.setScale = function (s) {
    var v = parseFloat(s);
    if (!(v > 0)) v = v === 0 ? 0.5 : 1;   // 0 floors, garbage is "unscaled"
    v = Math.min(Math.max(v, 0.5), 3);
    document.documentElement.style.setProperty("--ui-scale", String(v));
    return v;
  };

  /* ---- escaping and icons ------------------------------------------------ */

  ui.esc = function (s) {
    return String(s == null ? "" : s).replace(/[&<>"']/g, function (c) {
      return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c];
    });
  };

  /* ---- tones -------------------------------------------------------------
     The four tones theme.css knows (Logic::Tone). No prototype, so a name like
     "constructor" from a payload is not mistaken for one. */
  var TONES = Object.create(null);
  TONES.cyan = 1; TONES.gold = 1; TONES.red = 1; TONES.violet = 1;
  ui.TONES = TONES;

  /* ---- toasts ------------------------------------------------------------
     toaster(box) returns toast(payload) for isekai.toast({text, key, banner, tone}).
     The hud uses it, and the system page mirrors toasts through it while the hud
     sits below our panels (spec 6.2); the styles are theme.css's. The page owns the
     timings: a line lives 3.2 s, a banner 5.5 s, both fade over the last 0.8 s
     (the 220 ms rise is the CSS's). Nothing here runs until a toast arrives. */
  var LINE_MS = 3200, BANNER_MS = 5500, FADE_MS = 800, MAX_TOASTS = 4;

  ui.toaster = function (box) {
    var byKey = Object.create(null);

    function drop(el) {
      clearTimeout(el.isekaiTimer);
      if (el.parentNode) el.parentNode.removeChild(el);
      if (el.isekaiKey && byKey[el.isekaiKey] === el) delete byKey[el.isekaiKey];
    }

    function build(text, banner, tone, life) {
      var el = document.createElement("div");
      el.className = "toast " + (banner ? "banner" : "line");
      el.setAttribute("data-tone", tone);
      var tin = document.createElement("div");
      tin.className = "tin";
      tin.style.animationDelay = (life - FADE_MS) + "ms";
      // A leading "[ SYSTEM ]" (any short bracketed tag) is the System's voice: set apart.
      var m = /^\s*(\[[^\]\n]{1,24}\])\s*([\s\S]*)$/.exec(text);
      if (m && m[2]) {
        var tg = document.createElement("span");
        tg.className = "tg mono";
        tg.textContent = m[1];
        tin.appendChild(tg);
        text = m[2];
      }
      var tx = document.createElement("span");
      tx.className = "tx";
      tx.textContent = text;
      tin.appendChild(tx);
      el.appendChild(tin);
      return el;
    }

    /* A toast with a key already on screen replaces it in its slot and starts over
       (rise included, as the ImGui strip did): twenty kills leave one counter
       counting up. A fresh element rather than a rewound one, so restarting needs
       no style read. A string payload is parsed (the playground, a console call). */
    return function toast(payload) {
      var d = payload;
      if (typeof d === "string") {
        try { d = JSON.parse(d); } catch (e) { if (window.console) window.console.warn("[isekai] toast: bad payload"); return; }
      }
      if (!d || typeof d !== "object") return;
      var text = d.text == null ? "" : String(d.text);
      if (!text.replace(/\s+/g, "")) return;
      var banner = !!d.banner;
      var life = banner ? BANNER_MS : LINE_MS;
      var key = d.key ? String(d.key) : "";
      var el = build(text, banner, TONES[d.tone] ? d.tone : "cyan", life);

      var old = key ? byKey[key] : null;
      if (old && old.parentNode === box) {
        clearTimeout(old.isekaiTimer);
        box.replaceChild(el, old);
      } else {
        box.appendChild(el);
        // More than four at once stops being a status line and becomes a wall: the
        // oldest goes, the newest is what the player cares about.
        while (box.children.length > MAX_TOASTS) drop(box.firstElementChild);
      }
      if (key) { el.isekaiKey = key; byKey[key] = el; }
      el.isekaiTimer = setTimeout(function () { drop(el); }, life);
    };
  };

  /* Icons ship in icons/ next to the pages. Button and tree icons arrive as a bare
     file name ("blessing_hero.png"), the emblem as "icons/rank_c.png"; both end up
     relative to the page. Anything with a scheme or an absolute path is refused:
     the page only reads its own folder. */
  ui.iconUrl = function (s) {
    s = String(s || "");
    if (!s || /^[a-z]+:|^[\\/]|\.\./i.test(s)) return "";
    return s.indexOf("/") >= 0 ? s : "icons/" + s;
  };

  /* ---- typewriter --------------------------------------------------------
     typewriter(el, text, charsPerSec, opts) types text into el with a block
     caret and returns { finish(), cancel(), done }.

     opts.build(el, text) may lay the text out as markup first (the panel turns
     "LABEL  value" rows into a grid); the typewriter then reveals the text nodes
     of whatever was built, in document order. The full layout is measured before
     it empties, so the window keeps its final size and nothing jumps while the
     text arrives. opts.onDone runs once, when typing ends either way.

     Per frame it touches one text node; it stops requesting frames as soon as it
     is done, so a finished panel costs nothing. */
  ui.typewriter = function (el, text, charsPerSec, opts) {
    opts = opts || {};
    var cps = +charsPerSec > 0 ? +charsPerSec : 45;
    if (opts.build) opts.build(el, String(text || ""));
    else el.textContent = String(text || "");

    var nodes = [], full = [], total = 0;
    var walker = document.createTreeWalker(el, 4 /* NodeFilter.SHOW_TEXT */, null, false);
    for (var n = walker.nextNode(); n; n = walker.nextNode()) {
      nodes.push(n); full.push(n.data); total += n.data.length;
    }

    el.style.minHeight = "";
    el.style.minHeight = el.offsetHeight + "px";
    for (var i = 0; i < nodes.length; i++) nodes[i].data = "";

    var caret = document.createElement("span");
    caret.className = "caret";
    caret.setAttribute("aria-hidden", "true");
    el.appendChild(caret);

    var state = { done: false };
    var raf = 0, start = 0, shown = 0, active = -1;

    function place(idx) {
      if (idx === active || !nodes[idx]) return;
      active = idx;
      var node = nodes[idx];
      node.parentNode.insertBefore(caret, node.nextSibling);
    }

    function reveal(count) {
      var left = count;
      for (var k = 0; k < nodes.length; k++) {
        var len = full[k].length;
        if (left >= len) {
          if (nodes[k].data.length !== len) nodes[k].data = full[k];
          left -= len;
          if (left === 0 && count < total) { place(k); return; }
        } else {
          nodes[k].data = full[k].slice(0, left);
          place(k);
          for (var r = k + 1; r < nodes.length; r++) if (nodes[r].data) nodes[r].data = "";
          return;
        }
      }
      if (nodes.length) place(nodes.length - 1);
    }

    function end() {
      if (state.done) return;
      state.done = true;
      if (raf) window.cancelAnimationFrame(raf);
      raf = 0;
      reveal(total);
      caret.className = "caret done";
      if (opts.onDone) opts.onDone();
    }

    function tick(now) {
      raf = 0;
      if (state.done) return;
      if (!start) start = now;
      var count = Math.floor((now - start) / 1000 * cps);
      if (count >= total) { end(); return; }
      if (count !== shown) { shown = count; reveal(count); }
      raf = window.requestAnimationFrame(tick);
    }

    state.finish = end;
    state.cancel = function () {
      if (raf) window.cancelAnimationFrame(raf);
      raf = 0; state.done = true;
    };

    // Nothing to type, or a reveal faster than one frame (the plugin's "show it all at
    // once"): done now, without an empty first frame.
    if (total === 0 || total / cps < 0.016) end();
    else raf = window.requestAnimationFrame(tick);
    return state;
  };

  window.isekaiUi = ui;
})(window, document);
