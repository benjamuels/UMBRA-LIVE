// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// Felucca in an AudioWorklet: felucca.wasm is the whole FM-1 firmware against a simulated FM-1
// (web/emu/felucca_web.c). Each render quantum asks it for 128 frames, which runs the device's clock forward;
// between quanta the page's input goes in and, about 30 times a second (4 when the page is hidden), the screen,
// the lights and any changed flash go out. After X0X's web/emu/worklet.js (charlesvestal/fm1-x0x, GPL-3.0).

// The flash the page keeps: storage.c's areas (projects, user presets and their FM6 patches, the songs
// 0x97000..0xDFFFF; the autosave 0xE5000..0xE6FFF (1.2); the settings and the second FM6 copy
// 0xFC000..0xFFFFF), only the sectors that hold something (an erased sector is all 0xFF). {offset: bytes}
export const STORE_AREAS = [[0x97000, 0xE0000], [0xE5000, 0xE7000], [0xFC000, 0x100000]];
export function storeSectors(nor) {
  const out = {};
  for (const [a, b] of STORE_AREAS)
    for (let off = a; off < b; off += 4096) {
      const s = nor.subarray(off, off + 4096);
      if (s.some((v) => v !== 0xFF)) out[off] = s.slice();
    }
  return out;
}
export function restoreSectors(nor, sectors) {
  for (const [k, v] of Object.entries(sectors || {})) {
    const off = +k, b = v instanceof Uint8Array ? v : new Uint8Array(v);
    if (STORE_AREAS.some(([a, e]) => off >= a && off + 4096 <= e && off % 4096 === 0) && b.length === 4096)
      nor.set(b, off);
  }
}

export const HOLD_MS = 24;

/* (API 6) a plate module's new elements (umb_plate.h umb_xel_t, API 6), read from the wasm's memory:
 * [{ name, what, p: [{ name, def, tog, what }] }] */
export function xels(ex, h) {
  if (!ex.web_umx_xoff) return [];
  const mem = new Uint8Array(ex.memory.buffer), dv = new DataView(ex.memory.buffer);
  const str = (p) => { let s = ""; for (; p && mem[p]; p++) s += String.fromCharCode(mem[p]); return s; };
  const api = mem[h + 4] | mem[h + 5] << 8, off = ex.web_umx_xoff(0), sz = ex.web_umx_xoff(1), ow = ex.web_umx_xoff(2), op = ex.web_umx_xoff(3);
  if (api < 6) return [];
  const out = [], n = Math.min(3, mem[h + off]);           // (UMB_NXE)
  for (let e = 0; e < n; e++) {
    const b = h + off + 4 + e * sz, np = Math.min(3, mem[b + 8]), tog = mem[b + 9], el = { name: str(b), what: str(dv.getUint32(b + ow, true)), p: [] };
    for (let i = 0; i < np; i++)
      el.p.push({ name: str(b + 12 + 8 * i), def: mem[b + 19 + 8 * i], tog: !!(tog >> i & 1), what: str(dv.getUint32(b + op + 4 * i, true)) });
    out.push(el);
  }
  return out;
}

if (typeof registerProcessor === "function") {
  const clock = globalThis.performance ? () => globalThis.performance.now() : () => Date.now();
  class Felucca extends AudioWorkletProcessor {
    constructor() {
      super();
      this.ex = null;
      this.sentDraws = -1;
      this.sentWrites = -1;
      this.sentLeds = "";
      this.lastFrame = 0;
      this.period = 1 / 30;
      this.busy = 0;
      this.frames = 0;
      this.queue = [];                                // button / key changes, each held >= HOLD_MS of device time
      this.nextOk = 0;
      this.port.onmessage = (e) => this.onMessage(e.data);
    }

    async onMessage(m) {
      if (m.type === "load") {
        try {
          const { instance } = await WebAssembly.instantiate(m.wasm, {});
          const ex = instance.exports;
          ex._initialize();
          ex.web_nor_erase();
          restoreSectors(new Uint8Array(ex.memory.buffer, ex.web_nor(), ex.web_nor_size()), m.sectors);
          ex.web_master(m.master ?? 700);
          if (ex.web_seed) ex.web_seed((Math.random() * 4294967296) >>> 0);   // (JIANT 0.4) this session's macros
          ex.web_boot();
          for (let n = Math.round((m.settle || 0) * 44.1); n > 0; n -= 128) ex.web_render(128);   // (UMBRA LIVE) the plates page: past the welcome
          this.ex = ex;
          this.sentWrites = ex.web_flash_writes_count();
          this.port.postMessage({ type: "ready", rate: ex.web_sample_rate() });
        } catch (err) {
          this.port.postMessage({ type: "error", message: String(err && err.message || err) });
        }
        return;
      }
      const ex = this.ex;
      if (!ex) return;
      if (m.type === "buttons" || m.type === "keys") this.queue.push(m);
      else if (m.type === "enc") ex.web_enc(m.role, m.n | 0);
      else if (m.type === "master") ex.web_master(m.value | 0);
      else if (m.type === "umb" && ex.web_umb) ex.web_umb(m.i | 0, m.v | 0);   // (UMBRA LIVE) the PLATE and its settings
      else if (m.type === "umbget" && ex.web_umb_get)                  // (UMBRA LIVE) the projector's controls
        this.port.postMessage({ type: "umb", v: [99, 0, 1, 2].map((i) => ex.web_umb_get(i)) });
      else if (m.type === "umx" && ex.web_umx) ex.web_umx(m.pl | 0, m.j | 0, m.v | 0);   // (UMBRA LIVE) the plates page's player: a page's element
      else if (m.type === "umxget" && ex.web_umx_get)                 // .. a page's elements as set, its preset
        this.port.postMessage({ type: "umx", pl: m.pl, v: [0, 1, 2, 3, 4, 5, 6, 7].map((j) => ex.web_umx_get(m.pl, j)), pre: ex.web_umx_get(m.pl, 100) });
      else if (m.type === "umxx" && ex.web_umx_x) ex.web_umx_x(m.pl | 0, m.j | 0, m.v | 0);   // (API 6) one of its other elements's parameter
      else if (m.type === "umxinfo" && ex.web_umx_mod) {              // .. the pages there (number, name, a plate's header), the genres
        const mem = new Uint8Array(ex.memory.buffer), str = (p) => { let s = ""; for (; p && mem[p]; p++) s += String.fromCharCode(mem[p]); return s; };
        const pages = [];
        for (let pl = 0; pl < ex.web_umb_get(90); pl++)
          if (ex.web_umb_get(100 + pl)) { const h = ex.web_umx_mod(pl); pages.push({ pl, name: str(ex.web_umb_name(pl)), head: h ? mem.slice(h, h + 240) : null, xel: h ? xels(ex, h) : [] }); }
        const genres = []; for (let i = 0; i < ex.web_gen_count(); i++) genres.push(str(ex.web_gen_name(i)));
        this.port.postMessage({ type: "umxinfo", pages, genres });
      } else if (m.type === "gen" && ex.web_gen) {                     // .. a song made and played (the generator's)
        const lie = () => [ex.web_umb_get(99), ex.web_umb_get(80), ex.web_umb_get(83)];   // (the ground, LAYER 1, LAYER 3: 63 none)
        const was = lie(), W = [1, 2, 4];
        ex.web_gen(m.genre | 0, m.seed | 0, m.energy | 0, m.form | 0);
        for (let r = 0; r < ex.web_umb_get(70); r++)                  // (the plates GENE gave its song: its rows' taken out, what
          for (const tk of [0, 1]) ex.web_umb(94, (r << 24 | tk << 16 | 254) >>> 0);   //  it laid taken off, the page's own laid again)
        lie().forEach((pl) => { if (pl !== 63) ex.web_umb(97, pl); });
        was.forEach((pl, i) => { if (pl !== 63) ex.web_umb(97, pl | W[i] << 8); });
        ex.web_play(1);
      } else if (m.type === "play" && ex.web_play) ex.web_play(m.on ? 1 : 0);
      else if (m.type === "midi") ex.web_midi(m.data[0] | 0, m.data[1] | 0, m.data[2] | 0);
      else if (m.type === "visible") this.period = m.on ? 1 / (m.fps || 30) : 1 / 4;   // (the projector: 60)
    }

    publish() {
      const ex = this.ex, mem = ex.memory.buffer;
      const msg = { type: "frame" };
      const leds = [ex.web_lit_buttons(), ex.web_lit_keys(), ex.web_dim_buttons(), ex.web_dim_keys(), ex.web_dim_level(),
                    ex.web_breath_buttons(), ex.web_breath_keys(), ex.web_mid_keys ? ex.web_mid_keys() : 0];
      const an = ex.web_anim_levels ? ex.web_anim_levels() : 0;   // the power-on sweep: each LED's level
      const anim = an ? Array.from(new Uint8Array(mem, an, 41)) : null;
      const key = leds.join(",") + (anim ? ";" + anim.join(",") : "");
      if (key !== this.sentLeds) {
        this.sentLeds = key;
        msg.leds = leds;
        msg.anim = anim;
      }
      if (this.frames >= 44100) {                     // the share of real time spent running the device
        msg.load = this.busy / (this.frames / 44.1);
        this.busy = this.frames = 0;
      }
      const transfer = [];
      const draws = ex.web_screen_draws();
      if (draws !== this.sentDraws) {
        this.sentDraws = draws;
        msg.fb = new Uint16Array(mem, ex.web_screen(), 240 * 240).slice();
        transfer.push(msg.fb.buffer);
      }
      const writes = ex.web_flash_writes_count();
      if (writes !== this.sentWrites) {               // a save is done within one main-loop pass: whole here
        this.sentWrites = writes;
        msg.sectors = storeSectors(new Uint8Array(mem, ex.web_nor(), ex.web_nor_size()));
      }
      if (msg.leds || msg.fb || msg.sectors || msg.load !== undefined) this.port.postMessage(msg, transfer);
    }

    // A click's down and up can arrive within one render quantum, but the FM-1's contacts never close for less than
    // its debounce, and the UI reads them once a frame: each change of the buttons or the keys stays HOLD_MS of the
    // device's time before the next one goes in (a tap: down for a frame and more, then up).
    input() {
      const ex = this.ex, now = ex.web_now_ms();
      while (this.queue.length && now - this.nextOk >= 0) {
        const m = this.queue.shift();
        if (m.type === "buttons") ex.web_buttons(m.mask >>> 0);
        else ex.web_keys(m.mask >>> 0);
        this.nextOk = now + HOLD_MS;
      }
    }

    process(inputs, outputs) {
      const ex = this.ex;
      if (!ex) return true;
      if (this.queue.length) this.input();
      const out = outputs[0], n = out[0].length;
      const t0 = clock();
      ex.web_render(n);
      this.busy += clock() - t0;
      this.frames += n;
      out[0].set(new Float32Array(ex.memory.buffer, ex.web_out_l(), n));
      if (out[1]) out[1].set(new Float32Array(ex.memory.buffer, ex.web_out_r(), n));
      if (currentTime - this.lastFrame >= this.period) {
        this.lastFrame = currentTime;
        this.publish();
      }
      return true;
    }
  }
  registerProcessor("felucca", Felucca);
}
