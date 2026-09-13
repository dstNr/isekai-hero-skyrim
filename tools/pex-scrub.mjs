/* Strip the author's identity out of a compiled Papyrus script.
 *
 * PapyrusCompiler.exe writes three strings into every .pex header: the source file it
 * compiled, the Windows account that ran it, and the machine name. The account here is
 * the author's real name, so a freshly compiled .pex trips package.ps1's privacy gate:
 *
 *     Privacy gate: '<your Windows account name>' is still present in: ...\Scripts\IsekaiHeroMCM.pex
 *
 * That gate is correct and must not be loosened, so the leak is fixed instead. There is no
 * compiler switch for it and no build path that avoids it — the account name is stamped in
 * wherever you compile from.
 *
 * Run this on the .pex after every recompile:
 *
 *     node tools/pex-scrub.mjs mcm-patch/Scripts/IsekaiHeroMCM.pex
 *
 * .pex is serialised strictly in sequence with no internal byte offsets, so rewriting the
 * three header strings — to a different length, even — leaves everything after them valid.
 * The check below proves that for the file at hand rather than taking it on trust: it walks
 * the string table the header is followed by and fails if it no longer decodes.
 */
import { readFileSync, writeFileSync } from "node:fs";

const MAGIC = 0xfa57c0de;
const USERNAME = "isekai";
const MACHINE = "build";

/** Read the header's three length-prefixed strings, returning them and where they end. */
function readHeader(buf) {
  if (buf.readUInt32BE(0) !== MAGIC) {
    throw new Error("not a .pex file — magic is not 0xFA57C0DE");
  }
  let at = 16; // magic(4) + major(1) + minor(1) + gameID(2) + compilationTime(8)
  const str = () => {
    const len = buf.readUInt16BE(at);
    at += 2;
    const s = buf.subarray(at, at + len).toString("latin1");
    at += len;
    return s;
  };
  return { source: str(), username: str(), machine: str(), end: at };
}

/** Walk the string table that follows the header. It is the first thing after it, so if
 *  the header was rewritten wrongly this is what stops decoding. */
function checkStringTable(buf, from) {
  let at = from;
  const count = buf.readUInt16BE(at);
  at += 2;
  if (count === 0 || count > 4096) {
    throw new Error(`string table count ${count} is not plausible — header rewrite is wrong`);
  }
  for (let i = 0; i < count; i++) {
    const len = buf.readUInt16BE(at);
    at += 2;
    if (at + len > buf.length) {
      throw new Error(`string ${i} of ${count} runs past the end of the file`);
    }
    at += len;
  }
  return { count, end: at };
}

function scrub(buf) {
  const head = readHeader(buf);
  const wstr = (s) => {
    const body = Buffer.from(s, "latin1");
    const len = Buffer.alloc(2);
    len.writeUInt16BE(body.length);
    return Buffer.concat([len, body]);
  };
  return {
    head,
    out: Buffer.concat([
      buf.subarray(0, 16),
      wstr(head.source), // kept: it is already just a file name
      wstr(USERNAME),
      wstr(MACHINE),
      buf.subarray(head.end),
    ]),
  };
}

const path = process.argv[2];
if (!path) {
  console.error("usage: node tools/pex-scrub.mjs <file.pex>");
  process.exit(2);
}

const before = readFileSync(path);
const { head, out } = scrub(before);

// Prove the rewrite left the file readable before overwriting anything.
const after = readHeader(out);
const table = checkStringTable(out, after.end);
const tail = before.length - head.end;
if (out.length - after.end !== tail) {
  throw new Error("body length changed — only the header may be touched");
}
if (!out.subarray(after.end).equals(before.subarray(head.end))) {
  throw new Error("body bytes changed — only the header may be touched");
}

writeFileSync(path, out);
console.log(`${path}: username ${JSON.stringify(head.username)} -> ${JSON.stringify(after.username)}, ` +
            `machine ${JSON.stringify(head.machine)} -> ${JSON.stringify(after.machine)}`);
console.log(`  source ${JSON.stringify(after.source)} kept | string table ${table.count} entries, ` +
            `body ${tail} bytes untouched`);
