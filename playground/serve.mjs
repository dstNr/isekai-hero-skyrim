/* ============================================================================
   Isekai Hero — playground dev server
   ----------------------------------------------------------------------------
   Serves the repo over http://localhost so the playground can drive the REAL
   Magelight pages (magelight/IsekaiHero/*.html) in an iframe. A local http
   origin is used on purpose: opening the harness as a file:// page makes the
   browser treat the parent and the iframe as different origins, which blocks
   the cross-document scripting the playground needs (the page's fake host
   calling into mock.js, and mock.js calling isekai.show on the page). Over
   http://localhost everything is one origin and it just works.

   Three pieces of path fixing, none of which moves or copies a file:
     - Icons. In the shipped mod they sit next to the pages
       (Data\Magelight\IsekaiHero\icons\); in the repo they live in the
       top-level icons\ folder. Requests under a page's icons/ path are served
       from there.
     - version.js. The build generates it into build\generated\; it is served
       from there when a build exists, else a 404, which the page survives and
       then reports build "dev" — exactly what a missing file does in game.
     - The host. In game Magelight puts window.magelight into the page before
       it runs. Asked for with ?host=mock, a page is served with a small script
       injected at the top of <head> that does the same, forwarding every
       send() to mock.js in the parent window. The page file itself is untouched.

   Nothing but Node's standard library — no install, no lockfile, no network.

   Run:  node playground/serve.mjs      (then open the printed URL)
   ============================================================================ */

import { createServer } from "node:http";
import { readFile } from "node:fs/promises";
import { join, extname, normalize, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = dirname(dirname(fileURLToPath(import.meta.url))); // repo root
const PORT = Number(process.env.PORT) || 5173;

const MIME = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".mjs": "text/javascript; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".json": "application/json; charset=utf-8",
  ".png": "image/png",
  ".jpg": "image/jpeg",
  ".jpeg": "image/jpeg",
  ".gif": "image/gif",
  ".svg": "image/svg+xml",
  ".woff2": "font/woff2",
  ".woff": "font/woff",
  ".ttf": "font/ttf"
};

/* The pages reference icons relative to themselves; in the repo they are elsewhere.
   Rewrite those path prefixes and leave everything else untouched. */
const ICON_ALIASES = ["/magelight/IsekaiHero/icons/"];
const VERSION_JS = "/magelight/IsekaiHero/version.js";

/* The fake host (see the header). Messages sent before the parent's mock is reachable
   wait in __mlQueue; mock.js drains it when it attaches. */
const HOST_SHIM =
  "<script>/* injected by playground/serve.mjs: the fake Magelight host */\n" +
  "window.magelight = { send: function (name, payload) {\n" +
  "  var m = null; try { m = window.parent && window.parent.IsekaiMock; } catch (e) {}\n" +
  "  if (m && m.receive) m.receive(window, String(name), String(payload));\n" +
  "  else (window.__mlQueue = window.__mlQueue || []).push([String(name), String(payload)]);\n" +
  "} };\n</script>";

const server = createServer(async (req, res) => {
  try {
    const url = new URL(req.url, "http://x");
    let pathname = decodeURIComponent(url.pathname);

    // Directory index: "/" opens the playground, and any "…/" serves its index.html
    // (the README points people at "/playground/", which is a directory read -> EISDIR
    // -> 500 without this).
    if (pathname === "/") pathname = "/playground/index.html";
    else if (pathname.endsWith("/")) pathname += "index.html";
    for (const alias of ICON_ALIASES) {
      if (pathname.startsWith(alias)) pathname = "/icons/" + pathname.slice(alias.length);
    }
    if (pathname === VERSION_JS) pathname = "/build/generated/version.js";

    // Contain the path to the repo root -- no "../" escapes.
    const rel = normalize(pathname).replace(/^(\.\.[/\\])+/, "");
    const file = join(ROOT, rel);
    if (!file.startsWith(ROOT)) {
      res.writeHead(403).end("forbidden");
      return;
    }

    let body = await readFile(file);
    if (url.searchParams.get("host") === "mock" && extname(file).toLowerCase() === ".html") {
      body = Buffer.from(body.toString("utf8").replace(/<head>/i, (h) => h + "\n" + HOST_SHIM));
    }
    res.writeHead(200, {
      "Content-Type": MIME[extname(file).toLowerCase()] || "application/octet-stream",
      // The whole point of the playground is fast iteration; never cache.
      "Cache-Control": "no-store"
    });
    res.end(body);
  } catch (err) {
    if (err && err.code === "ENOENT") {
      res.writeHead(404, { "Content-Type": "text/plain" }).end("404 — " + req.url);
    } else {
      res.writeHead(500, { "Content-Type": "text/plain" }).end("500 — " + (err && err.message));
    }
  }
});

/* If the chosen port is busy, step to the next one instead of crashing with an
   EADDRINUSE stack trace — a leftover instance shouldn't block a restart. */
function listen(port, triesLeft) {
  server.once("error", (err) => {
    if (err.code === "EADDRINUSE" && triesLeft > 0) {
      console.log("  port " + port + " is busy — trying " + (port + 1) + " …");
      listen(port + 1, triesLeft - 1);
    } else {
      console.error("\n  Could not start the server: " + (err && err.message) + "\n");
      process.exit(1);
    }
  });
  server.listen(port, () => {
    console.log("\n  Isekai Hero playground");
    console.log("  → http://localhost:" + port + "/playground/\n");
    console.log("  Serving repo root:", ROOT);
    console.log("  Ctrl+C to stop.\n");
  });
}

listen(PORT, 12);
