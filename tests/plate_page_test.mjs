// (UMBRA LIVE) web/plates.html's core script (the plate files, the loading bank, the protocol) against the firmware's
// plate commands (editor_plate.c, umb_slot.c on a simulated flash: tests/plate_slot_test.c serve): connect, the bank read,
// the loading bank made the FM-1's (plates written where they were put, moved for their slots; slots emptied), and the
// page's table of controls against the plate files built (build/plates, when there). (Plate API 7) The palettes: colours
// read out of text (hex, a coolors.co URL, JSON, a GIMP palette, the page's own export), the generator, a picture's
// colours, a plate's parts in its header, and PAL_GET / PAL_SET against the same simulated FM-1.
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
check(run('LIB.size') === 29 && run('LIB.get("CORONA").builtin') && run('LIB.get("KOI").kind') === 'v' && run('LIB.get("WAX").ctl[0].what.length') > 5,
      `the library with no files: 29 plates known, their kinds and controls from the page`);
ctx.navigator.requestMIDIAccess = access(['IAC Driver Bus 1', 'Launchkey MK3']);
await run('connect()');
check(st().bad && /IAC Driver Bus 1, Launchkey MK3/.test(st().text), `connect, no FM-1: says which ports it sees`);
ctx.navigator.requestMIDIAccess = access(['IAC Driver Bus 1', 'FM-1']);   // (the FM-1's USB name since JIANT V10)
await run('connect()');
check(run('dev.name') === 'FM-1' && /Plate API 7, 16 slots, 0 in use/.test(run('dev.info')), `connect: finds "FM-1", the bank: "${run('dev && dev.info')}"`);
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
{ // (plate API 7) the palettes: colours out of text
  const P = (t) => { ctx.__t = t; return JSON.parse(run('JSON.stringify(palParse(__t))')); };
  const hx = (r) => r.cols.map((c) => c.map((v) => v.toString(16).padStart(2, '0')).join('')).join(' ');
  check(JSON.stringify(run('[unhex("#AaBbCc"), unhex("abc"), unhex("12345"), unhex("#gggggg")]')) === '[[170,187,204],[170,187,204],null,null]' &&
        run('hex([0, 15, 255])') === '#000fff', `hex colours read and written (#AaBbCc, abc; 12345 and #gggggg are none)`);
  check(hx(P('#aabbcc')) === 'aabbcc' && hx(P('aabbcc, DDEEFF\n#123 0a0b0c;#01020304')) === 'aabbcc ddeeff 112233 0a0b0c 010203',
        `text: #aabbcc, bare, #abc, commas, spaces, new lines, #rrggbbaa`);
  check(hx(P('https://coolors.co/264653-2a9d8f-e9c46a-f4a261-e76f51')) === '264653 2a9d8f e9c46a f4a261 e76f51' &&
        hx(P('https://coolors.co/palette/264653-2a9d8f')) === '264653 2a9d8f', `a coolors.co URL: its five colours`);
  check(hx(P('["#264653", "2a9d8f", "nope"]')) === '264653 2a9d8f' && hx(P('[[38, 70, 83], [42, 157, 143, 255]]')) === '264653 2a9d8f' &&
        hx(P('{"name": "x", "colors": ["#e9c46a"]}')) === 'e9c46a' && hx(P('rgb(38, 70, 83) rgba(42 157 143 / 1)')) === '264653 2a9d8f',
        `JSON arrays of hex strings and of [r, g, b], an object's colours, rgb(..)`);
  check(hx(P('GIMP Palette\nName: test\nColumns: 3\n#\n 38  70  83\tdeep\n233 196 106 sand\n300 0 0 not one\n')) === '264653 e9c46a', `a GIMP palette (.gpl)`);
  check(hx(P('264653\n2A9D8F\n')) === '264653 2a9d8f' && P('').cols.length === 0 && P('the quick brown fox').cols.length === 0, `a .hex file's lines; text with no colours: none`);
  ctx.__x = 0;
  check(P('[(globalThis.__x = 1), "#aabbcc"]').cols.length === 1 && P('{"__proto__": {"x": 1}, "a": "#010203"}').cols.length === 1 &&
        P('#aabbcc"); __x = 2; ("').cols.length === 1 && ctx.__x === 0, `text that is code is never run: its hex colours at most`);
  run('pals[1][2] = [1, 2, 3]; palUse = 2');
  const ex = P(run('palExport()')), exj = JSON.parse(run('palExport()'));
  check(exj.umbra_palettes === 1 && exj.palettes.length === 4 && exj.palettes.every((p) => p.length === 6 && p.every((h) => /^#[0-9a-f]{6}$/.test(h))) && exj.in_use === 3 &&
        ex.use === 2 && JSON.stringify(ex.all) === run('JSON.stringify(pals)') && ex.all[1][2].join() === '1,2,3', `the page's export read back: four palettes, the one in use`);
  run('pals = palCopy(PAL0); palUse = 0');
  const f = JSON.parse(run('JSON.stringify([palFill([[38, 70, 83], [233, 196, 106]]), palFill([]), palFill([[1,1,1],[2,2,2],[3,3,3],[4,4,4],[5,5,5],[6,6,6],[7,7,7]])])'));
  check(f[0].cols.length === 6 && f[0].added === 4 && f[0].cols[0].join() === '38,70,83' && new Set(f[0].cols.map(String)).size === 6 &&
        f[1].cols.length === 0 && f[2].cols.length === 6 && f[2].had === 7 && f[2].added === 0, `fewer than six made six (lighter repeats, each its own), more than six: the first six`);
}
{ // .. the generator: six colours a screen can show, none under the floor unless asked
  const L = (c) => { ctx.__c = c; return run('lab(__c)[0]'); };
  const bad = [];
  let darkSeen = false;
  for (const sch of run('Object.keys(SCHEMES)'))
    for (const base of [[64, 110, 230], [16, 16, 32], [255, 255, 255], [128, 128, 128], [255, 0, 0], [20, 60, 10]])
      for (const [spread, sat, light] of [[0, 0, 0], [50, 30, 40], [100, 100, 100]])
        for (const dark of [false, true]) {
          ctx.__o = { base, scheme: sch, spread, sat, light, dark };
          const g = JSON.parse(run('JSON.stringify(palGen(__o))'));
          const ok = g.length === 6 && g.every((c) => c.length === 3 && c.every((v) => Number.isInteger(v) && v >= 0 && v <= 255));
          if (!ok) bad.push(`${sch} ${base}: not six colours`);
          else if (!dark && g.some((c) => L(c) < 0.445)) bad.push(`${sch} ${base} ${spread}/${sat}/${light}: L ${Math.min(...g.map(L)).toFixed(3)}`);
          else if (dark && g.some((c) => L(c) < 0.44)) darkSeen = true;
        }
  check(!bad.length && darkSeen, `the generator: six colours for each of ${run('Object.keys(SCHEMES).length')} schemes, none under L 0.45 unless dark ones are allowed${bad.length ? ': ' + bad.slice(0, 4).join('; ') : ''}`);
  ctx.__o = { base: [64, 110, 230], scheme: 'analogous', spread: 50, sat: 30, light: 40 };
  const an = JSON.parse(run('JSON.stringify(palGen(__o))')), tr = JSON.parse(run('JSON.stringify(palGen({ ...__o, scheme: "triad" }).map((c) => Math.round(lch(c)[2])))'));
  const hd = (a, b) => Math.abs(((a - b + 540) % 360) - 180);
  check(an[2].every((v, k) => Math.abs(v - [64, 110, 230][k]) <= 1) && new Set(an.map(String)).size === 6 && hd(tr[2], tr[0]) > 100 && hd(tr[4], tr[0]) > 100 && hd(tr[4], tr[2]) > 100 &&
        JSON.stringify(run('palGen({ base: [64, 110, 230], scheme: "nope" })')) === JSON.stringify(run('palGen({ base: [64, 110, 230], scheme: "analogous" })')),
        `.. the base is one of an analogous six; a triad's hues a third of the wheel apart (${tr[0]}, ${tr[2]}, ${tr[4]})`);
  let n = 0; ctx.__rnd = () => [0.1, 0.9, 0.5, 0.99][n++ % 4];
  const sh = run('palShuffle(__rnd)');
  check(sh.base.length === 3 && L(sh.base) > 0.5 && run('Object.keys(SCHEMES)').includes(sh.scheme), `shuffle: a base and a scheme (${sh.scheme})`);
  const rt = run('[[64, 110, 230], [255, 255, 255], [0, 0, 0], [255, 122, 28], [1, 2, 3]].every((c) => { const [l, ch, h] = lch(c); return unlch(l, ch, h).every((v, k) => Math.abs(v - c[k]) <= 1); })');
  check(rt && Math.abs(L([255, 255, 255]) - 1) < 0.001 && L([0, 0, 0]) < 0.001, `sRGB to OKLCH and back`);
}
{ // .. a picture's colours: six flat fields with a little noise, 48 x 48
  const want = [[220, 40, 40], [240, 200, 40], [40, 200, 90], [40, 120, 230], [170, 60, 220], [235, 235, 235]];
  const px = new Uint8Array(48 * 48 * 4);
  let seed = 7; const rnd = () => (seed = (seed * 1103515245 + 12345) >>> 0, (seed >>> 16) % 13 - 6);
  for (let i = 0; i < 48 * 48; i++) {
    const c = want[(i % 48 >> 3)];
    for (let k = 0; k < 3; k++) px[i * 4 + k] = Math.max(0, Math.min(255, c[k] + rnd()));
    px[i * 4 + 3] = 255;
  }
  ctx.__px = px;
  const got = JSON.parse(run('JSON.stringify(palCluster(__px, 6))'));
  const nearest = (c) => Math.min(...got.map((g) => Math.hypot(g[0] - c[0], g[1] - c[1], g[2] - c[2])));
  const hues = got.slice(0, 5).map((c) => { ctx.__c = c; return run('lch(__c)[2]'); });
  check(got.length === 6 && want.every((c) => nearest(c) < 12) && got[5].every((v) => v > 220) && hues.filter((h, i) => i && h < hues[i - 1]).length <= 1,
        `a picture of six colours: each found (within ${Math.max(...want.map(nearest)).toFixed(1)}), by hue round the wheel, the grey last`);
  const two = new Uint8Array(400 * 4), none = new Uint8Array(400 * 4);
  for (let i = 0; i < 400; i++) two.set(i < 300 ? [200, 30, 30, 255] : i < 390 ? [30, 30, 200, 255] : [0, 255, 0, 0], i * 4);
  ctx.__px = two; const g2 = JSON.parse(run('JSON.stringify(palCluster(__px, 6))'));
  ctx.__px = none;
  check(g2.length === 2 && g2.map(String).sort().join(' ') === '200,30,30 30,30,200' && run('palCluster(__px, 6).length') === 0 &&
        JSON.stringify(run('palLift([[20, 20, 60], [200, 200, 200]])[1]')) === '[200,200,200]' && run('lab(palLift([[20, 20, 60]])[0])[0]') >= 0.445,
        `a picture of two colours: two (transparent pixels left out); none: none; a dark one lifted to the floor`);
}
{ // .. a plate's parts: read from its header (umb_plate_t.ncol at 396, umb_colr_t 12 bytes), what each resolves to
  const b = bytes.slice(), put = (at, name, rgb, def) => { b.fill(0, at, at + 12); b.set(Buffer.from(name), at); b.set([...rgb, def], at + 8); };
  b[396] = 3; put(400, 'FISH', [255, 122, 28], 0); put(412, 'NOTES', [0, 0, 0], 0x87); put(424, 'PATCH 1', [10, 20, 30], 0x22);
  ctx.__f = b;
  const h = JSON.parse(run('JSON.stringify(head(parse(__f).img))'));
  b[4] = 6; const h6 = run('head(parse(__f).img)');
  check(h.api === 7 && JSON.stringify(h.col) === '[{"name":"FISH","rgb":[255,122,28],"def":0},{"name":"NOTES","rgb":[0,0,0],"def":135},{"name":"PATCH 1","rgb":[10,20,30],"def":34}]' &&
        h6.api === 6 && h6.col.length === 0 && run('COL_AT') === 396 && run('COL_SZ') === 12, `a plate's parts from its header (API 7 only): ${h.col.map((c) => c.name).join(', ')}`);
  ctx.__h = h;
  check(JSON.stringify(run('[partCol(__h.col[0], 0, PAL0[0]), partCol(__h.col[0], 4, PAL0[0]), partCol(__h.col[1], 0, PAL0[0]), partCol(__h.col[1], 7, PAL0[0]), partCol(__h.col[2], 1, PAL0[0])]')) ===
        '[{"kind":"own","rgb":[255,122,28]},{"kind":"pal","rgb":[255,206,64]},{"kind":"notes","rgb":null},{"kind":"tracks","rgb":null},{"kind":"pal","rgb":[32,55,115]}]',
        `a part's colour as chosen: its own, the palette's, the notes' own, the tracks', a palette's at half light`);
}
{ // .. the FM-1's palettes: PAL_GET, PAL_SET (a channel halved; the FM-1 makes v << 1 | v >> 6 of it)
  const J = (s) => JSON.parse(run(`JSON.stringify(${s})`));
  check(JSON.stringify(J('palEnc([[255, 0, 1], [128, 127, 64], [2, 3, 254], [9, 9, 9], [0, 0, 0], [255, 255, 255]])').slice(0, 9)) === '[127,0,0,64,63,32,1,1,127]' &&
        JSON.stringify(J('palDec([127, 0, 1, 64, 63, 32, 115, 55, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0])').slice(0, 3)) === '[[255,0,2],[129,126,64],[231,110,64]]' &&
        run('PAL0.every((p) => palEnc(palDec(palEnc(p))).join() === palEnc(p).join())') && run('palSame(PAL0, PAL0.map((p) => palDec(palEnc(p))))') && run('!palSame(PAL0, [PAL0[1], PAL0[0], PAL0[2], PAL0[3]])'),
        `a palette as 18 halved bytes and back (255 -> 127 -> 255, 230 -> 115 -> 231)`);
  await run('connect()');
  const r0 = await run('palRead()');
  check(run('palCan()') && r0.use === 0 && JSON.stringify(r0.pals) === JSON.stringify(J('PAL0.map((p) => palDec(palEnc(p)))')),
        `PAL_GET: the FM-1's four palettes (the factory's), palette 1 in use`);
  const sent = [], send0 = ctx.__send;
  ctx.__send = (a) => { sent.push(Array.from(a)); send0(a); };
  const mine = [[[255, 0, 0], [0, 255, 0], [0, 0, 255], [1, 2, 3], [129, 131, 133], [254, 253, 252]], ...J('PAL0').slice(1, 3), [[9, 8, 7], [6, 5, 4], [3, 2, 1], [0, 0, 0], [77, 88, 99], [200, 100, 50]]];
  ctx.__mine = mine;
  await run('palSend(__mine, 3)');
  const r1 = await run('palRead()');
  ctx.__r = r1.pals;
  check(sent.length === 6 && sent.slice(0, 4).every((f, i) => f.length === 25 && f[4] === 102 && f[5] === i && f.slice(5, 24).every((v) => v < 128)) &&
        sent[0].slice(6, 12).join() === '127,0,0,0,127,0' && sent[4].join() === [0xF0, 0x7D, 0x46, 0x4C, 102, 67, 0xF7].join() && sent[5][4] === 101 && sent[5].length === 6 &&
        r1.use === 3 && JSON.stringify(r1.pals) === JSON.stringify(J('__mine.map((p) => palDec(palEnc(p)))')) && r1.pals[0][0].join() === '255,0,0' && r1.pals[0][4].join() === '129,131,133' &&
        run('palSame(__mine, __r)'),
        `PAL_SET x 4, then the one in use; PAL_GET reads them back as sent (to 7 bits), palette 4 in use`);
  const a5 = await run('request(CMD.PAL_SET, [4, ...palEnc(PAL0[0])])'), a17 = await run('request(CMD.PAL_SET, [1, ...palEnc(PAL0[0]).slice(1)])');
  const r2 = await run('palRead()');
  check(a5.join() === '4,1' && a17.join() === '1,1' && JSON.stringify(r2) === JSON.stringify(r1), `a palette the FM-1 has not (5), one a byte short: rc 1, nothing changed`);
  await run('palSend(PAL0, 0)');                        // (as it was)
  ctx.__send = (a) => { if (a[4] !== 101 && a[4] !== 102) send0(a); };   // (firmware of before API 7 does not answer these)
  let e1 = '', e2 = '', e3 = '';
  try { await run('palRead(150)'); } catch (e) { e1 = e.message; }
  try { await run('palSend(PAL0, 0, 150)'); } catch (e) { e2 = e.message; }
  ctx.__send = send0;
  run('devApi = 6');
  try { await run('palRead()'); } catch (e) { e3 = e.message; }
  check(/no palettes yet \(it needs plate API 7\)/.test(e1) && e2 === e1 && e3 === e1 && !run('palCan()') && run('dev.wait') === null,
        `an FM-1 that does not answer them, or says plate API 6: "${e1}"`);
  run('devApi = 7; dev = null');
  let e4 = ''; try { await run('palRead()'); } catch (e) { e4 = e.message; }
  check(e4 === 'Connect the FM-1 first.' && !run('palCan()'), `no FM-1: "${e4}"`);
}
dev.stdin.end();
console.log(`plate page: ${fails ? 'FAILED' : 'all passed'} (${fails} failed)`);
process.exit(fails ? 1 : 0);
