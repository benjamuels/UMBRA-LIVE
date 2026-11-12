// (UMBRA LIVE) web/plates.html's own script against the firmware's plate commands (editor_plate.c, umb_slot.c on a
// simulated flash: tests/plate_slot_test.c serve): connect, the bank listed, plates sent (moved for their slots), a slot
// emptied.
//   cc -Ifirmware/src -Ifirmware/hal -o /tmp/pst tests/plate_slot_test.c && node tests/plate_page_test.mjs /tmp/pst
import fs from 'node:fs';
import vm from 'node:vm';
import { spawn, execFileSync } from 'node:child_process';
import path from 'node:path';
import os from 'node:os';
import { fileURLToPath } from 'node:url';
const bin = process.argv[2], here = path.dirname(fileURLToPath(import.meta.url));
const html = fs.readFileSync(path.join(here, '..', 'web', 'plates.html'), 'utf8');
const script = html.slice(html.indexOf('<script>') + 8, html.lastIndexOf('</script>'));
const img = path.join(os.tmpdir(), `plate_page_test_${process.pid}.plate`);
execFileSync(bin, ['image', img]);
const bytes = new Uint8Array(fs.readFileSync(img)); fs.unlinkSync(img);

const els = {};
let rows = [];                                          // (the bank's rows as the page made them: slot, name, info)
const mk = (id) => ({ id, textContent: '', className: '', disabled: false, innerHTML: '', onclick: null, onchange: null, kids: {},
  appendChild(c) { if (this.id === 'bank') rows.push(c); }, querySelector(q) { return this.kids[q] || (this.kids[q] = mk(q)); } });
const el = (id) => els[id] || (els[id] = mk(id));
const bankEl = el('bank');
Object.defineProperty(bankEl, 'innerHTML', { set(v) { if (v === '') rows = []; }, get() { return ''; } });
const ctx = vm.createContext({ document: { getElementById: el, querySelectorAll: () => [], createElement: () => mk('new') },
  navigator: {}, fetch: () => Promise.reject(new Error('offline')), setTimeout, clearTimeout, console, Uint8Array, Array, String, Math,
  Error, Promise, DataView });
const shown = () => rows.map((r) => [r.kids['.slot'].textContent, r.kids['b'].textContent].join(' ').trim());
vm.runInContext(script, ctx);

const dev = spawn(bin, ['serve']);
let buf = '';
dev.stdout.on('data', (d) => {
  buf += d; let i;
  while ((i = buf.indexOf('\n')) >= 0) {
    const ln = buf.slice(0, i); buf = buf.slice(i + 1);
    const r = new Uint8Array(ln.match(/../g).map((h) => parseInt(h, 16)));
    ctx.__reply(r);
  }
});
ctx.__send = (a) => dev.stdin.write(Buffer.from(a).toString('hex') + '\n');
ctx.__reply = (r) => vm.runInContext('onMidi({ data: __r })', Object.assign(ctx, { __r: r }));
vm.runInContext('dev = { inp: {}, out: { send: (a) => __send(a) }, wait: null };', ctx);

let fails = 0;
const port = (name) => ({ name, state: 'connected', onmidimessage: null, send: (a) => ctx.__send(a) });
const access = (names) => async () => ({ inputs: new Map(names.map((n, i) => [`i${i}`, port(n)])),
                                         outputs: new Map(names.map((n, i) => [`o${i}`, port(n)])) });
const check = (ok, what) => { console.log(`plate page: ${what.padEnd(64)} ${ok ? 'ok' : 'FAIL'}`); fails += !ok; };
ctx.__bytes = bytes;
ctx.navigator.requestMIDIAccess = access(['IAC Driver Bus 1', 'Launchkey MK3']);
vm.runInContext('dev = null', ctx);
await vm.runInContext('connect()', ctx);
check(el('status').className === 'bad' && /IAC Driver Bus 1, Launchkey MK3/.test(el('status').textContent),
      `connect, no FM-1: says which ports it sees`);
ctx.navigator.requestMIDIAccess = access(['IAC Driver Bus 1', 'FM-1']);   // (the FM-1's USB name since JIANT V10)
await vm.runInContext('connect()', ctx);
check(el('dev').textContent === 'FM-1' && /Plate API 3, 16 slots, 0 in use/.test(el('devinfo').textContent),
      `connect: finds "FM-1", the bank: "${el('devinfo').textContent}"`);
check(shown().join('|') === 'CORONA|16 empty slots', `the bank, empty: ${shown().join(' | ')}`);
await vm.runInContext('send([[__bytes, "FIL EMBER"]])', ctx);
check(el('status').textContent === 'FIL EMBER in slot 1.' && !el('status').className, `send: "${el('status').textContent}"`);
check(shown().join('|') === 'CORONA|1 FIL EMBER|15 empty slots', `the bank after: ${shown().join(' | ')}`);
await vm.runInContext('send([[__bytes, "FIL EMBER"]])', ctx);
check(shown().join('|') === 'CORONA|1 FIL EMBER|15 empty slots', `the same plate again: replaced where it is`);
await vm.runInContext('bank[0].name = "X"', ctx);       // (as if slot 1 held another plate: the next empty slot, moved there)
await vm.runInContext('send([[__bytes, "FIL EMBER"]])', ctx);
check(el('status').textContent === 'FIL EMBER in slot 2.' && shown().join('|') === 'CORONA|1 FIL EMBER|2 FIL EMBER|14 empty slots',
      `another slot (moved for slot 2, taken by the FM-1): "${el('status').textContent}"`);
const nrel = new DataView(bytes.buffer).getUint32(bytes.length - 12, true);
ctx.__raw = bytes.subarray(0, bytes.length - 12 - 4 * nrel);   // (the image alone: no list of addresses)
await vm.runInContext('send([[__raw, "RAW"]])', ctx);
check(el('status').className === 'bad' && /not a plate file for the bank/.test(el('status').textContent), `a file without its addresses, refused before sending`);
ctx.__bad = bytes.slice(); new DataView(ctx.__bad.buffer).setUint32(ctx.__bad.length - 16, 4, true);   // (the last address listed wrong: the header's API word)
await vm.runInContext('bank[0].name = "X"; bank[1].name = "Y"', ctx);   // (so it goes to slot 3: moved by 0x4000)
await vm.runInContext('send([[__bad, "BAD"]])', ctx);
check(el('status').className === 'bad' && /refused/.test(el('status').textContent) && shown().join('|') === 'CORONA|1 FIL EMBER|2 FIL EMBER|14 empty slots',
      `addresses moved wrong: the FM-1 refuses it, slot 3 left empty: "${el('status').textContent}"`);
await vm.runInContext('erase(0)', ctx);
check(shown().join('|') === 'CORONA|2 FIL EMBER|15 empty slots' && /Slot 1 is empty/.test(el('status').textContent), `erase slot 1: "${el('status').textContent}", ${shown().join(' | ')}`);
{ // (an address at a halfword, as pi32v2's 48-bit loads hold one: moved like the others)
  const v = (b) => new DataView(b.buffer, b.byteOffset).getUint32(1002, true);
  const m3 = vm.runInContext('moved(parse(__bytes), 3)', ctx);
  check(nrel === 5 && v(m3) === ((v(bytes) + 3 * 0x2000) >>> 0), `an address at a halfword (1002), moved for slot 3`);
}
dev.stdin.end();
console.log(`plate page: ${fails ? 'FAILED' : 'all passed'} (${fails} failed)`);
process.exit(fails ? 1 : 0);
