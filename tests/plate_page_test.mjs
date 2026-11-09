// (UMBRA OBSCURA) web/plates.html's own script against the firmware's plate commands (editor_plate.c, umb_slot.c on a
// simulated flash: tests/plate_slot_test.c serve): connect, send a plate, LIST shows it, empty the slot.
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
const el = (id) => els[id] || (els[id] = { id, textContent: '', className: '', disabled: false, innerHTML: '', onclick: null, onchange: null,
  appendChild() {}, querySelector: () => el(id + '/q') });
const ctx = vm.createContext({ document: { getElementById: el, querySelectorAll: () => [], createElement: () => el('new') },
  navigator: {}, fetch: () => Promise.reject(new Error('offline')), setTimeout, clearTimeout, console, Uint8Array, Array, String, Math, Error, Promise });
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
check(el('dev').textContent === 'FM-1' && /Plate API 1/.test(el('devinfo').textContent), `connect: finds the port named "FM-1"`);
await vm.runInContext('refresh()', ctx);
check(el('slotname').textContent === 'Empty', `LIST, empty: "${el('slotname').textContent}"`);
await vm.runInContext('send(__bytes, "FIL EMBER")', ctx);
check(el('status').textContent === 'FIL EMBER is in the slot.' && !el('status').className, `send: "${el('status').textContent}"`);
check(el('slotname').textContent === 'FIL EMBER' && /2186 bytes, running/.test(el('slotinfo').textContent),
      `LIST after: "${el('slotname').textContent}", "${el('slotinfo').textContent}"`);
ctx.__bad = bytes.slice(); ctx.__bad[1000] ^= 1;
await vm.runInContext('send(__bad.subarray(0, 64), "SHORT")', ctx);
check(el('status').className === 'bad' && /not a plate file/.test(el('status').textContent), `too short, refused before sending: "${el('status').textContent}"`);
await vm.runInContext('erase()', ctx);
check(el('slotname').textContent === 'Empty' && /empty/.test(el('status').textContent), `erase: "${el('status').textContent}"`);
dev.stdin.end();
console.log(`plate page: ${fails ? 'FAILED' : 'all passed'} (${fails} failed)`);
process.exit(fails ? 1 : 0);
