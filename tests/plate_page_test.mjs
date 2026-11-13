// (UMBRA LIVE) web/plates.html's core script (the plate files, the loading bank, the protocol) against the firmware's
// plate commands (editor_plate.c, umb_slot.c on a simulated flash: tests/plate_slot_test.c serve): connect, the bank read,
// the loading bank made the FM-1's (plates written where they were put, moved for their slots; slots emptied), and the
// page's table of controls against the plate files built (build/plates, when there).
//   cc -Ifirmware/src -Ifirmware/hal -o /tmp/pst tests/plate_slot_test.c && node tests/plate_page_test.mjs /tmp/pst
import fs from 'node:fs';
import vm from 'node:vm';
import { spawn, execFileSync } from 'node:child_process';
import path from 'node:path';
import os from 'node:os';
import { fileURLToPath } from 'node:url';
const bin = process.argv[2], here = path.dirname(fileURLToPath(import.meta.url));
const html = fs.readFileSync(path.join(here, '..', 'web', 'plates.html'), 'utf8');
const at = html.indexOf('<script id="core">') + 18;
const script = html.slice(at, html.indexOf('</script>', at));
const img = path.join(os.tmpdir(), `plate_page_test_${process.pid}.plate`);
execFileSync(bin, ['image', img]);
const bytes = new Uint8Array(fs.readFileSync(img)); fs.unlinkSync(img);

const ctx = vm.createContext({ navigator: {}, fetch: () => Promise.reject(new Error('offline')), setTimeout, clearTimeout, console,
  Uint8Array, Uint32Array, Array, String, Math, Error, Promise, DataView, Map, Set, Object });
vm.runInContext(script, ctx);
const run = (s) => vm.runInContext(s, ctx);
const st = () => run('status'), names = () => run('bank.map((b) => (b && b.st ? b.name : "-")).join(" ")');

const dev = spawn(bin, ['serve']);
let buf = '';
dev.stdout.on('data', (d) => {
  buf += d; let i;
  while ((i = buf.indexOf('\n')) >= 0) {
    const ln = buf.slice(0, i); buf = buf.slice(i + 1);
    ctx.__r = new Uint8Array(ln.match(/../g).map((h) => parseInt(h, 16)));
    run('onMidi({ data: __r })');
  }
});
ctx.__send = (a) => dev.stdin.write(Buffer.from(a).toString('hex') + '\n');

let fails = 0;
const port = (name) => ({ name, state: 'connected', onmidimessage: null, send: (a) => ctx.__send(a) });
const access = (names) => async () => ({ inputs: new Map(names.map((n, i) => [`i${i}`, port(n)])),
                                         outputs: new Map(names.map((n, i) => [`o${i}`, port(n)])) });
const check = (ok, what) => { console.log(`plate page: ${what.padEnd(72)} ${ok ? 'ok' : 'FAIL'}`); fails += !ok; };
const E = '- - - - - - - - - - - - - - - -';
ctx.__bytes = bytes;
await run('library([])');
check(run('LIB.size') === 28 && run('LIB.get("CORONA").builtin') && run('LIB.get("KOI").kind') === 'v' && run('LIB.get("WAX").ctl[0].what.length') > 5,
      `the library with no files: 28 plates known, their kinds and controls from the page`);
ctx.navigator.requestMIDIAccess = access(['IAC Driver Bus 1', 'Launchkey MK3']);
await run('connect()');
check(st().bad && /IAC Driver Bus 1, Launchkey MK3/.test(st().text), `connect, no FM-1: says which ports it sees`);
ctx.navigator.requestMIDIAccess = access(['IAC Driver Bus 1', 'FM-1']);   // (the FM-1's USB name since JIANT V10)
await run('connect()');
check(run('dev.name') === 'FM-1' && /Plate API 5, 16 slots, 0 in use/.test(run('dev.info')), `connect: finds "FM-1", the bank: "${run('dev && dev.info')}"`);
check(names() === E && run('plan.length') === 16 && run('plan.every((x) => x === null)'), `the bank, empty; the loading bank follows it`);
const e = run('addFile(__bytes, "ember.plate", true)');
check(e && e.name === 'FIL EMBER' && e.own && e.len > 0 && run('movable("FIL EMBER")') && !run('movable("CORONA")'),
      `a plate file read into the library: ${e && e.name}, ${e && e.len} bytes`);
run('planSet(0, "FIL EMBER")');
check(JSON.stringify(run('diff().write')) === '[0]' && run('diff().same') === 15, `put in slot 1 of the loading bank: one slot to write`);
await run('load()');
check(st().text === 'Loaded: FIL EMBER in slot 1.' && !st().bad && names() === 'FIL EMBER' + E.slice(1), `load: "${st().text}"`);
await run('load()');
check(/already holds/.test(st().text), `load again: nothing to do`);
run('planSet(2, "FIL EMBER")');                         // (a plate lies in the bank once: it moves)
check(run('plan[0]') === null && JSON.stringify(run('[diff().erase, diff().write]')) === '[[0],[2]]', `moved to slot 3: slot 1 to empty, slot 3 to write`);
await run('load()');
check(st().text === 'Loaded: slot 1 emptied, FIL EMBER in slot 3.' && names() === '- - FIL EMBER' + E.slice(5),
      `load (moved for slot 3, taken by the FM-1): "${st().text}"`);
run('planOwn = false; plan.fill(null); dev = null');   // (a page opened afresh: the loading bank follows the FM-1)
await run('connect()');
check(run('plan[2]') === 'FIL EMBER' && run('diff().same') === 16 && /shows what the FM-1 holds/.test(st().text), `connect again: the loading bank is the FM-1's`);
const nrel = new DataView(bytes.buffer).getUint32(bytes.length - 12, true);
ctx.__raw = bytes.subarray(0, bytes.length - 12 - 4 * nrel);   // (the image alone: no list of addresses)
check(run('addFile(__raw, "raw.plate", true)') === null, `a file without its addresses: not a plate file`);
ctx.__bad = bytes.slice(); new DataView(ctx.__bad.buffer).setUint32(ctx.__bad.length - 16, 4, true);   // (the last address listed wrong: the header's API word)
run('addFile(__bad, "bad.plate", true); planSet(4, "FIL EMBER")');   // (slot 5: moved by 0x8000)
await run('load()');
check(st().bad && /refused/.test(st().text) && names() === E, `addresses moved wrong: the FM-1 refuses it, slot 5 left empty: "${st().text}"`);
run('addFile(__bytes, "ember.plate", true); plan.fill(null); planSet(1, "FIL EMBER"); plan[7] = "ELSE"');                // (a plate with no file here, where the FM-1 does not hold it)
await run('load()');
check(st().bad && /No plate file here for ELSE/.test(st().text) && names() === E, `a plate with no file: said, nothing written`);
run('planSet(7, null)');
await run('load()');
run('planSet(1, null)');
check(JSON.stringify(run('diff().erase')) === '[1]', `taken out of the loading bank: slot 2 to empty`);
await run('load()');
check(st().text === 'Loaded: slot 2 emptied.' && names() === E, `load: "${st().text}"`);
{ // (an address at a halfword, as pi32v2's 48-bit loads hold one: moved like the others)
  const v = (b) => new DataView(b.buffer, b.byteOffset).getUint32(1002, true);
  const m3 = run('moved(parse(__bytes), 3)');
  check(nrel === 5 && v(m3) === ((v(bytes) + 3 * 0x2000) >>> 0), `an address at a halfword (1002), moved for slot 3`);
}
{ // the page's table (DOC, META, ALL) against the plate files built
  const dir = path.join(here, '..', 'build', 'plates'), files = fs.existsSync(dir) ? fs.readdirSync(dir).filter((f) => f.endsWith('.plate')) : [];
  if (!files.length) console.log('plate page: (no build/plates here: the table is not checked against the files)');
  else {
    const bad = [], seen = new Set();
    for (const f of files) {
      ctx.__f = new Uint8Array(fs.readFileSync(path.join(dir, f)));
      const h = run('head(parse(__f).img)'), doc = run(`DOC[${JSON.stringify(h.name)}]`), meta = run(`META[${JSON.stringify(h.name)}]`);
      if (run('HIDE').includes(h.name)) continue;
      seen.add(h.name);
      if (!doc || !meta) bad.push(`${h.name}: not in the page`);
      else if (doc.map((d) => `${d[0]} ${d[1]}`).join(',') !== h.ctl.map((c) => `${c.name} ${c.def}`).join(','))
        bad.push(`${h.name}: its controls are ${h.ctl.map((c) => `${c.name} ${c.def}`).join(', ')}`);
      else if (meta.k !== h.kind) bad.push(`${h.name}: its kind is ${h.kind}`);
    }
    for (const n of run('Object.keys(DOC)')) if (n !== 'CORONA' && !seen.has(n)) bad.push(`${n}: no plate file built`);
    for (const n of run('ALL')) if (!seen.has(n)) bad.push(`${n}: in the standard sixteen, no plate file`);
    check(!bad.length, `the page's controls, kinds and names against ${files.length} plate files${bad.length ? ': ' + bad.join('; ') : ''}`);
  }
}
dev.stdin.end();
console.log(`plate page: ${fails ? 'FAILED' : 'all passed'} (${fails} failed)`);
process.exit(fails ? 1 : 0);
