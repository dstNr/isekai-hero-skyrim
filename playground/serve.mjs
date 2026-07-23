/* ============================================================================
   Isekai Hero — playground dev server
   ----------------------------------------------------------------------------
   Serves the repo over http://localhost so the playground can drive the REAL
   PrismaUI view in an iframe. A local http origin is used on purpose: opening
   the harness as a file:// page makes the browser treat the parent and the
   iframe as different origins, which blocks the cross-document scripting the
   playground needs (defining the callbacks on the view's window, and calling
   window.isekaiShowTree on it). Over http://localhost everything is one origin
   and it just works.

   One alias does the only piece of path fixing needed: in the shipped patch the
   node icons sit next to index.html (PrismaUI\views\IsekaiHero\icons\), but in
   the repo they live at the top-level icons\ folder. The view asks for
   "icons/<name>.png" relative to itself, so requests under the view's icons
   path are redirected to the repo's icons folder. No files are moved or copied.

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

/* The view references icons relative to itself; in the repo they are elsewhere.
   Rewrite that one path prefix and leave everything else untouched. */
const ICON_ALIAS = "/prisma-patch/PrismaUI/views/IsekaiHero/icons/";

const server = createServer(async (req, res) => {
  try {
    let pathname = decodeURIComponent(new URL(req.url, "http://x").pathname);

    // Directory index: "/" opens the playground, and any "…/" serves its index.html
    // (the README points people at "/playground/", which is a directory read -> EISDIR
    // -> 500 without this).
    if (pathname === "/") pathname = "/playground/index.html";
    else if (pathname.endsWith("/")) pathname += "index.html";
    if (pathname.startsWith(ICON_ALIAS)) {
      pathname = "/icons/" + pathname.slice(ICON_ALIAS.length);
    }

    // Contain the path to the repo root -- no "../" escapes.
    const rel = normalize(pathname).replace(/^(\.\.[/\\])+/, "");
    const file = join(ROOT, rel);
    if (!file.startsWith(ROOT)) {
      res.writeHead(403).end("forbidden");
      return;
    }

    const body = await readFile(file);
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
