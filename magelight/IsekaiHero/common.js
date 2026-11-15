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
