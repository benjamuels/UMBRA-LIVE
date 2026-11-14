/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* (included by ui_umbra.c, after the plates' light) built for size, as the main loop is (tools/size_fns.py); ump_backdrop
 * stays a call. Prototyped in prototypes/umbra-kiln-alternatives.html.
 * ------------------------------------------------------------------------------------- CORONA, the resident plate ---
 * Drawn from a little state, every pixel of its strip from a map made once and the cold palette as the panel takes it,
 * dithered 2 x 2 (ump_lut: a plain pixel one read). No lighting, no per-pixel loop over the notes; what changes steps (16
 * levels), so between notes it is still and HOME's blit sends nothing. Built into the firmware (the welcome uses its state
 * and its map); CINDER, GRANULE and TARN, the other field plates drawn here until 2026-11-09, are plate modules
 * (plates/cinder.c, granule.c, tarn.c: the same arithmetic, through umb_plate.h; the palette, the noise and the fade are
 * handed to them, umb_api).
 * CORONA: the eclipse. The moon's disc (44 px) black, the corona's 128 streamers out from it, each a falloff from the limb.
 *   Its elements (LIVE VIZ CTRLS; the tracks of the last four can be chosen, at first as said):
 *     REACH   how far a bright streamer runs          FADE    how long the notes' light lasts
 *     SIZE    the moon's (a quarter .. twice)         HALO    the corona at rest
 *     STREAM  a note feeds the streamer at its place on the circle of fifths (the root at the top, as SAROS), the bass
 *             broad, a high note fine (at first: every note that is no drum)
 *     PULSE   the chromosphere (the limb's ring) flares (the kick)
 *     PROM    a rose prominence (the snare, the clap)
 *     BEADS   Baily's beads (the hats and the rest of the kit)
 *   each of the four as hard as its value (50: as played, 0: never, 99: twice). Track 3's light yellow, track 4's orange. */
static const int8_t UMP_D2[4] = {-8, 0, 4, -4};        /* the dither, 2 x 2 (fixed to the screen): [(y & 1) x 2 + (x & 1)] */
static uint16_t ump_lut[4][256] __attribute__((section(".pool")));   /* the palette as the panel takes it, a dither each
                                                         * ([1]: none, the palette itself) */
/* the cold palette's stops (its index, r g b), worked out between them once (ump_lut_make): black (JIANT's, 0 0 0), barely
 * lifted to 40, then a deep blue, azure, pale teal and white, the top reached sooner. (On 5 5 8 and slate-blue low down until
 * 2026-11-08: the plates looked grey beside JIANT's screens; a stored table, umb_pal, until the warm tints wanted its 768
 * bytes of the slot) */
static const uint8_t UMP_ST[6][4] = {{0, 0, 0, 0}, {40, 2, 5, 18}, {84, 14, 38, 112}, {136, 44, 130, 200}, {190, 132, 218, 228},
                                     {255, 248, 255, 255}};
static const uint8_t UMP_WT[3] = {236, 250, 246};       /* white-teal */
static const uint8_t UMP_WARM[2][3] = {{255, 206, 64}, {255, 122, 28}};   /* the warm tints: yellow, orange (ump_tint) */
#define UMP_IX(v, vg) ump_ix(((v) * (vg)) >> 8)
static inline __attribute__((always_inline)) int32_t ump_ix(int32_t i) { return i < 0 ? 0 : i > 255 ? 255 : i; }
/* a pixel: the palette at v (0..255; its plain entry) and the light r g b added, darkened by vg (Q8), dithered by d (only
 * lit pixels, GRANULE's, TARN's sheen and the warm tints, come here) */
#define ump_rgb(v, r, g, b, vg, d) umm_add(ump_lut[1][UMP_IX(v, vg)], r, g, b, vg, d)
static void ump_lut_make(void)
{
    uint32_t k, i, j = 0, c;
    int32_t p[3];
    for (i = 0; i < 256u; i++) {
        const uint8_t *a, *b;
        while (i > UMP_ST[j + 1][0])
            j++;
        a = UMP_ST[j];
        b = UMP_ST[j + 1];
        for (c = 1; c < 4u; c++)
            p[c - 1] = a[c] + (b[c] - a[c]) * (int32_t)(i - a[0]) / (b[0] - a[0]);
        for (k = 0; k < 4u; k++)
            ump_lut[k][i] = ump_pack(p[0], p[1], p[2], UMP_D2[k]);
    }
}
/* the warm tints (CINDER, CORONA): a track's own (3 yellow, 4 orange; 0: none, the cold palette), as a tint byte: its share
 * of the light (0 .. 127) and which (128: orange) */
static uint32_t ump_tint(uint32_t c) { return c == 2u ? 127u : c == 3u ? 255u : 0u; }
#define UMP_WLO 20                                      /* (light under it black either way: no tint worked out) */
/* a pixel of light b (0..255) under tint t: its share the tint's colour (b squared: dark, as the palette is, low down; the
 * colour itself at 176, brighter past it), the rest the cold palette's; darkened by vg, dithered by d */
static __attribute__((noinline)) uint16_t ump_wpx(int32_t b, uint32_t t, int32_t vg, int32_t d)
{
    const uint8_t *C = UMP_WARM[t >> 7];
    int32_t w = (b * (int32_t)(t & 127u)) >> 7, k = (w * w * 541) >> 8;
    return ump_rgb(b - w, (C[0] * k) >> 16, (C[1] * k) >> 16, (C[2] * k) >> 16, vg, d);
}
/* a pitch across the keyboard, 0..256 (C2 .. C7); its place on the circle of fifths from track c's root (0..11) */
static int32_t ump_pk(uint32_t n) { return umb_clamp(((int32_t)n - 36) * 4369 >> 10, 0, 256); }
static uint32_t ump_fifth(uint32_t c, uint32_t n) { return (n + 120u - (uint32_t)trk[c].p[P_ROOT] % 12u) % 12u * 7u % 12u; }
/* a glint at X, Y (px, Q8): a point and the four round it a third as bright */
static void ump_glint(int32_t X, int32_t Y, int32_t a, const uint8_t *c)
{
    umm_splat(X, Y, a, c);
    umm_splat(X + 256, Y, a * 77 >> 8, c);
    umm_splat(X - 256, Y, a * 77 >> 8, c);
    umm_splat(X, Y + 256, a * 77 >> 8, c);
    umm_splat(X, Y - 256, a * 77 >> 8, c);
}
/* v faded by f (Q16 of it), rounded up: it reaches 0. (Rounded down until 2026-11-08, a note's light stuck once f x v fell
 * under a unit, ~100 of 410 at 60 frames a second: GRANULE's lit granules and CORONA's fed streamers never went dark) */
static uint16_t ump_dec(uint32_t v, int32_t f) { return (uint16_t)(v - ((v * (uint32_t)f + 65535u) >> 16)); }
/* a fade in steps: of n, at age (ms) of life (ms), Q8 (0 when gone) */
static int32_t ump_fade(uint32_t age, uint32_t life, uint32_t n) { return age >= life ? 0 : (int32_t)((n - age * n / life) * 256u / n); }

/* ------------------------------------------------------------------------------------------------- CORONA --- */
#define UMR_RM umr_rm                                   /* the moon's radius (px): 44 at SIZE 50, 16 .. 88 */
static int32_t umr_rm = 44, umr_k = 186;                /* (.. and 8192 / it: a pixel's distance past the limb, Q8) */
static void ump_co_init(void)
{
    ump_co_t *s = &umb.co;
    int32_t bx, by, a;
    for (by = 0; by < UMB_N; by++)                      /* the map: a quarter's 2 x 2 px blocks, from the centre */
        for (bx = 0; bx < UMB_N; bx++) {
            int32_t dx = 2 * bx + 1, dy = 2 * by + 1, q, i;
            q = dx >= dy ? dy * 1024 / dx : dx * 1024 / dy;
            i = q >> 5;
            a = i >= 32 ? 128 : umb_at[i] + (((umb_at[i + 1] - umb_at[i]) * (q & 31)) >> 5);
            s->pa[by * UMB_N + bx] = (uint8_t)umb_clamp(dx >= dy ? a : 256 - a, 0, 255);
            s->pr[by * UMB_N + bx] = (uint8_t)umb_clamp((int32_t)umb_isqrt((uint32_t)(dx * dx + dy * dy)), 0, 255);
        }
    for (a = 0; a < (int32_t)UMR_NA; a++) {             /* the corona at rest: ragged (the noise), streamers at the equator */
        int32_t n = umb_tbil(a * 64, 9 * 256 + 77) >> 8, d0 = a < 64 ? a : 128 - a, d1 = a < 64 ? 64 - a : a - 64;
        s->base[a] = (uint8_t)umb_clamp(36 + ((n * n) >> 9) + ((102 * (umb_exp(d0 * d0 * 32 / 81) + umb_exp(d1 * d1 * 32 / 81))) >> 8),
                                        0, 255);
    }
}
/* note nt drives CORONA's element j: its tracks as chosen (LIVE VIZ CTRLS), else own; how hard (0: not at all) */
static int32_t ump_co_on(const umb_lctx_t *cx, uint32_t j, const umb_note_t *nt, int own)
{
    return (cx->et[j] ? cx->et[j] >> nt->track & 1u : (uint32_t)own) ? ((int32_t)nt->vel * 2 + 1) * cx->k[j] / 50 : 0;
}
/* a drum's lane's shape (KICK SNARE CLAP HATCL HATOP TOM RIM BELL): an arch's kind 0..3 (wide, spike, twin, loop), or
 * 4 + a glint's 0..3 (one, a ray, a pair, a star); an arch's half width and height (quarters of its own), a glint's count,
 * its steps out (Q8 px) and round (1024 a turn), its start above the limb (Q8 px) */
static const uint8_t UMR_LK[8] = {0, 1, 2, 4, 5, 3, 6, 7}, UMR_PW[4] = {13, 2, 13, 5}, UMR_PH[4] = {4, 9, 3, 2};
static const uint16_t UMR_BG[4][4] = {{1, 0, 0, 154}, {3, 1024, 0, 154}, {2, 0, 14, 154}, {1, 0, 0, 3584}};
static __attribute__((noinline)) void ump_co_bead(ump_co_t *s, int32_t v, uint32_t tn, uint32_t k)   /* a glint more, kind k */
{
    uint32_t i = s->nbd++ % 10u;
    s->ba[i] = (uint16_t)(umb_rnd() & 1023u);
    s->bv[i] = (uint8_t)(v > 255 ? 255 : v);
    s->bt[i] = umb_now;
    s->bc[i] = (uint8_t)tn;
    s->bk[i] = (uint8_t)k;
}
static void ump_co_note(void *st, const umb_lctx_t *cx, const umb_api_t *ap, const umb_note_t *nt)
{
    ump_co_t *s = st;
    uint32_t c = nt->track, n = nt->note, drum = nt->drum, lane = nt->lane, tn = ump_tint(c), i;   /* (tracks 3 and 4: warm) */
    int32_t a0, sg, e, d, v;
    (void)ap;
    if (!nt->vel)
        return;
    /* (the user, 2026-11-13) a drum only ever the arches, a shape its lane's: KICK a wide arch, SNARE a spike, CLAP twin
     * arches, TOM a loop lifting off; the hats and the small ones glints: HATCL one on the limb, HATOP a ray of three, RIM
     * a pair, BELL a star above it. Other notes: PULSE the low ones (the chromosphere flares), BEADS the high ones, PROM
     * a track chosen for it, STREAM all */
    if ((v = ump_co_on(cx, 5u, nt, (int)drum)) != 0) {
        uint32_t k = drum ? UMR_LK[lane & 7u] : 0u;
        if (k & 4u)                                     /* (a glint) */
            ump_co_bead(s, v, tn, k & 3u);
        else {
            i = s->np++ % 4u;
            s->pa_[i] = (uint16_t)(umb_rnd() & 1023u);
            s->ph[i] = (uint8_t)(10 + 12 * v / 256);
            s->pt[i] = umb_now;
            s->pc[i] = (uint8_t)tn;
            s->pk[i] = (uint8_t)k;
        }
    }
    if (drum)
        return;
    if ((v = ump_co_on(cx, 4u, nt, n < 48u)) != 0 && s->P <= 128 + (v >> 1)) {   /* PULSE: the chromosphere flares */
        s->P = (uint16_t)(128 + (v >> 1));
        s->Pt = (uint8_t)tn;
    }
    if ((v = ump_co_on(cx, 6u, nt, n >= 76u)) != 0)     /* BEADS: a bead */
        ump_co_bead(s, v, tn, 0u);
    if (!(v = ump_co_on(cx, 3u, nt, 1)))                /* STREAM */
        return;
    a0 = (int32_t)ump_fifth(c, n) * 128 / 12 + 96 + (nt->mo[0] >> 1);   /* (the root at the top; NOTE MOVEMENT round) */
    sg = 16 + (((256 - ump_pk(n)) * 40) >> 8);          /* (its width: 1 .. 3.5 streamers, Q4) */
    e = 90 + ((141 * v) >> 8);
    for (d = -(3 * sg >> 4) - 1; d <= (3 * sg >> 4) + 1; d++) {
        uint32_t j = (uint32_t)(a0 + d) & (UMR_NA - 1u);
        int32_t a = (e * umb_exp(4096 * d * d / (sg * sg))) >> 8;
        a = (a * (410 - s->E[j])) / 410;                /* (it saturates: a dense song, not a white sky) */
        s->E[j] = (uint16_t)(s->E[j] + a);
        if (tn) {
            if (a >= s->W[j])
                s->Wc[j] = (uint8_t)tn;
            s->W[j] = (uint16_t)(s->W[j] + a);
        }
    }
}
static void ump_co_frame(void *st, const umb_lctx_t *cx, const umb_api_t *ap)
{
    ump_co_t *s = st;
    int32_t k0 = cx->k[0], k1 = cx->k[1], k2 = cx->k[2], life = 400 + k1 * k1 * 3 / 5, f = (int32_t)cx->dt * 65536 / life,
            reach = 640 * k0 / 99, hb = 154 * cx->k[7] / 50, a;   /* (HALO: 50 as it was) */
    (void)ap;
    umr_rm = umb_clamp(44 * (k2 <= 50 ? 64 + k2 * 192 / 50 : 256 + (k2 - 50) * 256 / 49) >> 8, 16, 88);   /* (SIZE: a quarter .. twice) */
    umr_k = 8192 / umr_rm;
    s->P = (uint16_t)(s->P - ((s->P * (int32_t)cx->dt * 218) >> 16));   /* (the pulse: 0.3 s) */
    for (a = 0; a < (int32_t)UMR_NA; a++) {
        int32_t S;
        s->E[a] = ump_dec(s->E[a], f);
        s->W[a] = ump_dec(s->W[a], f);
        S = ((s->base[a] * hb) >> 8) + (s->E[a] & ~15);
        s->S[a] = (uint16_t)S;
        s->K[a] = (int16_t)(947 - ((reach * (S > 256 ? 256 : S)) >> 8));   /* (3.7 .. 1.2: REACH, a bright one further) */
        s->T[a] = (uint8_t)((s->Wc[a] & 128u) | (umb_clamp(s->W[a] * 254 / (S + 1), 0, 127) & 0x78));   /* (the warm
                                                         * share, twice its light's: a hit all its colour; 16 steps) */
    }
    s->S[UMR_NA] = s->S[0];
    s->K[UMR_NA] = s->K[0];
    s->T[UMR_NA] = s->T[0];
}
/* a row of 2 x 2 px blocks (block row qy's, py even .. odd): each one's streamer light, from x0 (into bv), and tint (the
 * nearer streamer's, into bt) */
static void ump_co_blocks(int32_t x0, int32_t py, int16_t *bv, uint8_t *bt)
{
    ump_co_t *s = &umb.co;
    int32_t dy = (py & ~1) - 120, qy = dy >= 0 ? dy >> 1 : (-dy - 1) >> 1, lnm = umb_ln[UMR_RM - 16], b, n;
    const uint8_t *pa = s->pa + qy * UMB_N, *pr = s->pr + qy * UMB_N;
    for (b = x0 >> 1, n = 0; b <= (x0 + (int32_t)umm_w - 1) >> 1; b++, n++) {
        int32_t dx = 2 * b - 120, qx = dx >= 0 ? dx >> 1 : (-dx - 1) >> 1, A = pa[qx], i, f, sv, kx, r, l, k, t;
        A = (dx >= 0 ? (dy >= 0 ? A : 1024 - A) : (dy >= 0 ? 512 - A : 512 + A)) & 1023;
        i = A >> 3;
        f = (A & 7) << 5;
        sv = s->S[i] + (((s->S[i + 1] - s->S[i]) * f) >> 8);
        kx = s->K[i] + (((s->K[i + 1] - s->K[i]) * f) >> 8);
        r = pr[qx];
        l = r > UMR_RM ? umb_ln[(r > 175 ? 175 : r) - 16] - lnm : 0;
        k = (A >> 1) & 511;                             /* (its striations: the noise table's row 21) */
        t = umb_tn[21 * UMB_T + (k >> 4)];
        t += ((umb_tn[21 * UMB_T + (((k >> 4) + 1) & (UMB_T - 1))] - t) * ((k & 15) << 4)) >> 8;
        bv[n] = (int16_t)((((sv * umb_exp((kx * l) >> 12)) >> 8) * (115 + ((141 * t) >> 8))) >> 8);
        bt[n] = s->T[i + (f >> 7)];
    }
}
static void ump_co_draw(int32_t x0, int32_t y0)
{
    ump_co_t *s = &umb.co;
    int32_t Pq = s->P & ~31, i, u, ch = 128 + ((154 * Pq) >> 8), ic = (64 * Pq) >> 8;
    int16_t bv[122];
    uint8_t bt[122];
    uint32_t cx, cy, t, k, pk;
    for (cy = 0; cy < umm_h; cy++) {
        int32_t py = y0 + (int32_t)cy, vg = umm_vg(py), dy = py - 120, fdy2 = (2 * dy + 1) * (2 * dy + 1) - 4 * UMR_RM * UMR_RM;
        const uint16_t *L = ump_lut[(py & 1) << 1];
        uint16_t *row = umm_p + cy * umm_w;
        if (!cy || !(py & 1))
            ump_co_blocks(x0, py, bv, bt);
        for (cx = 0, pk = 0; cx < umm_w; cx++) {
            int32_t px = x0 + (int32_t)cx, dx = 2 * (px - 120) + 1, v;
            u = ((dx * dx + fdy2) * umr_k) >> 8;        /* (px past the limb, Q8) */
            if (u < -128) {                             /* the disc: black */
                row[cx] = L[0];
                pk = 0;
                continue;
            }
            v = bv[(px >> 1) - (x0 >> 1)];
            t = bt[(px >> 1) - (x0 >> 1)];
            if (u < 14 * 256) {                         /* the chromosphere, and the inner corona the kick lights (its
                                                         * track's tint while it lasts) */
                int32_t uu = u < 0 ? 0 : u;
                v += (umb_exp((uu * 71) >> 10) * ch) >> 8;
                v += (ic * (256 - ((uu * 18) >> 8))) >> 8;
                if (u < 128)
                    v = (v * (u + 128)) >> 8;
                if (s->Pt && Pq)
                    t = (s->Pt & 128u) | (uint32_t)(Pq >> 1);
            }
            v = v > 255 ? 255 : v;
            k = t & 127u && v >= UMP_WLO ? (uint32_t)v | t << 8 : 0u;   /* (warm: a pixel pair at a time, as they match) */
            row[cx] = !k ? L[(px & 1) * 256 + UMP_IX(v, vg)] : (px & 1) && k == pk ? row[cx - 1]
                                                             : ump_wpx(v, t, vg, UMP_D2[((py & 1) << 1) | (px & 1)]);
            pk = k;
        }
    }
    for (i = 0; i < 4; i++) {                           /* the arches: on the limb, 1.4 s, 8 steps; each its kind's shape */
        uint32_t age = umb_now - s->pt[i], kd = s->pk[i] & 3u;
        int32_t fq = s->ph[i] ? ump_fade(age, 1400u, 8u) : 0, rise = age >= 250u ? 256 : (int32_t)(age * 256u / 250u),
                dl = s->ph[i] * UMR_PW[kd] >> 2, hh = s->ph[i] * UMR_PH[kd] * rise >> 2,
                lift = kd == 3u ? s->ph[i] * rise * 3 >> 1 : 0;
        const uint8_t *pc = s->pc[i] ? UMP_WARM[s->pc[i] >> 7] : UMB_EVC[6];   /* (rose; tracks 3 and 4 theirs) */
        if (!fq)
            continue;
        for (u = 0; u <= 50; u++) {
            uint32_t an = (uint32_t)(s->pa_[i] + dl * (2 * u - 50) / 50) << 22;
            int32_t f = kd == 1u ? (50 - (u > 25 ? 2 * u - 50 : 50 - 2 * u)) * 5 : sine_i((uint32_t)u * (kd == 2u ? 85899346u : 42949673u)) >> 7,
                    d = (hh * (f < 0 ? -f : f)) >> 8, ro = UMR_RM * 256 + 128 + lift + d,
                    co = sine_i(an + 0x40000000u), si = sine_i(an), X = 120 * 256 + ((ro * co) >> 15), Y = 120 * 256 + ((ro * si) >> 15);
            umm_splat(X, Y, (fq * 128) >> 8, pc);
            if (kd == 3u)                               /* (the loop's lower side; the others: a line two thick) */
                d *= 2;
            else
                d = 256;
            umm_splat(X - ((d * co) >> 15), Y - ((d * si) >> 15), (fq * 77) >> 8, pc);
        }
    }
    for (i = 0; i < 10; i++) {                          /* the glints: on the limb or above it, 0.3 s */
        const uint16_t *g = UMR_BG[s->bk[i] & 3u];
        int32_t fq = s->bv[i] ? ump_fade(umb_now - s->bt[i], 300u, 8u) : 0, ro = UMR_RM * 256 + g[3];
        if (fq)
            for (u = 0; u < (int32_t)g[0]; u++) {
                uint32_t an = (uint32_t)(s->ba[i] + g[2] * u) << 22;
                ump_glint(120 * 256 + (((ro + g[1] * u) * sine_i(an + 0x40000000u)) >> 15), 120 * 256 + (((ro + g[1] * u) * sine_i(an)) >> 15),
                          (fq * (128 + (s->bv[i] >> 1)) * 410) >> 16, s->bc[i] ? UMP_WARM[s->bc[i] >> 7] : UMP_WT);
            }
    }
}

/* (API 6) CORONA's new elements, light over the eclipse:
 *   COMET    a comet crosses the sky above the eclipse, its tail streaming behind it; its head flares on the beat
 *   PLANETS  tiny planets all round the frame, each coming and going on its own (never all lit), twinkling with the mix */
#if UMB_RES_ALL                                         /* (the web player only, as the FM-1 cannot turn them on yet) */
static const uint8_t UMR_PLC[5][3] = {{255, 248, 230}, {255, 206, 120}, {230, 150, 110}, {200, 230, 255}, {255, 236, 180}};
static void ump_co_xdraw(const umb_lctx_t *cx)
{
    int32_t i;
    if (cx->xk[0][0]) {                                 /* COMET: across in 24 s .. 4 s (SPEED), TAIL 6 .. 40 points */
        uint32_t per = 24000u - cx->xk[0][1] * 200u, t = umb_now % per;
        int32_t X = -30 * 256 + (int32_t)((uint64_t)t * 300u * 256u / per), Y = 18 * 256 + (int32_t)((uint64_t)t * 54u * 256u / per),
                n = 6 + cx->xk[0][2] * 34 / 99, hd = 200 + (cx->beat >> 2);
        for (i = n; i >= 0; i--) {                      /* (the tail back along its path, spreading and fading) */
            int32_t a = hd * (n - i) / n, sp = i * 40, w = (int32_t)(sine_i((umb_now + (uint32_t)i * 97u) * 900000u) >> 9) * i / n;
            umm_splat(X - i * 3 * 256, Y - i * 154 + w, a, UMP_WT);
            if (i > 2)
                umm_splat(X - i * 3 * 256, Y - i * 154 + w + sp / 4 + 256, a >> 2, UMB_EVC[2]);
        }
        ump_glint(X, Y, hd, UMP_WT);
    }
    if (cx->xk[1][0]) {                                 /* PLANETS: COUNT 4 .. 28 tiny ones, SHINE how bright */
        int32_t n = 4 + cx->xk[1][1] * 24 / 99, sh = 120 + cx->xk[1][2] * 2;
        for (i = 0; i < n; i++) {                       /* each on its own cycle (5 .. 11 s): in, shining, out, dark a
                                                         * while, then somewhere else round the frame: never all lit */
            uint32_t h = (uint32_t)i * 0x9E3779B1u, per = 5000u + (h >> 7) % 6000u, t = umb_now + (h >> 3) % per,
                     g = (h ^ (t / per) * 0x85EBCA77u) * 0xC2B2AE3Du, ph = t % per * 256u / per;
            int32_t X, Y, dx, dy, ex, ey, tw, sz = (g >> 29) == 0;
            if (ph >= 160u)                             /* (dark the last part of its cycle) */
                continue;
            tw = ph < 40u ? (int32_t)ph * 6 : ph > 120u ? (int32_t)(160u - ph) * 6 : 240;
            X = 8 + (int32_t)((g >> 8) % 224u), Y = 8 + (int32_t)((g >> 16) % 224u);
            ex = X - 120, ey = Y - 120;                 /* (none on the eclipse: out past its rim) */
            if (ex * ex + ey * ey < (UMR_RM + 14) * (UMR_RM + 14)) {
                X = ex < 0 ? X - UMR_RM - 14 : X + UMR_RM + 14;
                X = X < 6 ? 6 : X > 234 ? 234 : X;
            }
            tw = tw * sh * (200 + cx->level / 4 + (sine_i(umb_now * 300000u + h) >> 10)) >> 16;
            tw = tw > 255 ? 255 : tw;
            X *= 256, Y *= 256;
            for (dy = -sz; dy <= sz; dy++)              /* (a dot, now and then a tiny disc; a cross of light when bright) */
                for (dx = -sz; dx <= sz; dx++)
                    umm_splat(X + dx * 256, Y + dy * 256, tw * (dx * dx + dy * dy == 0 ? 256 : 140) >> 8, UMR_PLC[g % 5u]);
            for (dx = sz + 1; tw > 170 && dx < sz + 4; dx++) {
                int32_t f = (tw - 170) * (sz + 4 - dx) / 4;
                umm_splat(X + dx * 256, Y, f, UMR_PLC[g % 5u]), umm_splat(X - dx * 256, Y, f, UMR_PLC[g % 5u]);
                umm_splat(X, Y + dx * 256, f, UMR_PLC[g % 5u]), umm_splat(X, Y - dx * 256, f, UMR_PLC[g % 5u]);
            }
        }
    }
}
#endif

/* ---------------------------------------------------------------------------------------------------------- */
/* CORONA as a plate (umb_plate.h), built in: its state the block's start (umb.co: the welcome borrows it) */
static void ump_co_start(void *st, const umb_lctx_t *cx, const umb_api_t *ap)
{
    (void)st;
    (void)ap;
    umb.rs = 218u + cx->now;
    ump_co_init();
}
static void ump_co_strip(void *st, const umb_lctx_t *cx, const umb_api_t *ap, const umb_strip_t *sp)
{
    (void)st;
    (void)ap;
    umm_lq = cx->light;                                 /* (LIGHT: umm_vg) */
    umm_x0 = sp->x0;                                    /* (the glints: umm_splat) */
    umm_y0 = sp->y0;
    umm_p = sp->px;
    umm_w = (uint32_t)sp->w;
    umm_h = (uint32_t)sp->h;
    ump_co_draw(sp->x0, sp->y0);
#if UMB_RES_ALL
    ump_co_xdraw(cx);                                   /* (NEW, API 6) */
#else
    (void)cx;
#endif
}
static const umb_plate_t umb_plate_corona = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(ump_co_t), "CORONA",
    UMB_KIND_FIELD, UMB_MODE(UMB_DRAW_REPLACE), 8u, {0, 0, 0},
    {{"REACH", 50}, {"FADE", 45}, {"SIZE", 50}, {"STREAM", 50}, {"PULSE", 50}, {"PROM", 50}, {"BEADS", 50}, {"HALO", 50}},
    ump_co_start, ump_co_note, ump_co_frame, ump_co_strip,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0},
    "The eclipse: streamers, drums as arches", 3u, {0, 0, 0},
    {{"BREATH", {0, 71, 0, 0, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 2u), UMB_LINK(UMB_SRC_LEVEL, 0u)}, {99, 99, 20, 50}},
     {"DIAMOND", {0, 0, 36, 31, 0, 91, 96, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 7u), UMB_LINK(UMB_SRC_HIT(0), 0u)}, {99, 99, 60, 40}},
     {"ANNULAR", {26, 0, 81, 71, 0, 0, 0, 91}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 4u), UMB_LINK(UMB_SRC_LEVEL, 7u)}, {99, 99, 50, 30}}},
    80u, {0, 0, 0}, 0,
#if UMB_RES_ALL
    /* (API 6) its other elements (the web player only) */
    2u, {0, 0, 0},
    {{"COMET", 3u, 1u, {0, 0}, {{"ON", 0}, {"SPEED", 40}, {"TAIL", 50}},
      "A comet crosses the sky above the eclipse, its head flaring on every beat",
      {"Comet on or off", "How fast it crosses (24 s to 4 s)", "How long its tail streams"}},
     {"PLANETS", 3u, 1u, {0, 0}, {{"ON", 0}, {"COUNT", 60}, {"SHINE", 50}},
      "Tiny planets all round the frame, each fading in and out on its own, so never all lit at once",
      {"Planets on or off", "How many planets (4 to 28)", "How bright they shine"}}}
#else
    0u, {0, 0, 0}, {{""}}
#endif
};

/* ------------------------------------------------------------------------------------------------ WELCOME --- */
/* (UMBRA LIVE) the power-on screen, UMW_T ms (ui_draw.c draw_splash, splash_write: main.c boot_splash, web/emu): an
 * eclipse reaching totality, on CORONA's corona. Out of the black a pale sun, the moon already on it from the lower left,
 * slowing as it covers it; its last light a bead on the upper right that flares (the diamond ring) with the chromosphere;
 * the corona unfurling round the black disc, turning slowly to a stop; the name rising out of the dark under it, then the
 * line under that; by 3.55 s all still, held so 1 s; and in the last 0.45 s all sinks to black. ms: from its start (past
 * UMW_T: nothing drawn). It borrows CORONA's state (the ground after it starts afresh) */
#define UMW_T 5000u                                     /* (3.55 s of the eclipse, 1 s still, 0.45 s to black: the user,
                                                         * 2026-11-13, a second shorter) */
#define UMW_DY 24                                       /* (the eclipse 24 px above the screen's centre, CORONA's) */
static __attribute__((noinline)) int32_t umw_rp(int32_t t, int32_t t0, int32_t d)   /* 0..256 from t0 over d ms */
{
    return umb_clamp((t - t0) * 256 / d, 0, 256);
}
static __attribute__((noinline)) int32_t umw_in(int32_t t, int32_t t0, int32_t d)   /* .. slowing as it arrives */
{
    int32_t q = 256 - umw_rp(t, t0, d);
    return 256 - (q * q >> 8);
}
static __attribute__((noinline)) int32_t umw_out(int32_t dx, int32_t dy, int32_t r2)   /* out of a disc r2 (1/16 px,
                                                                                         * squared) by dx dy: Q8 */
{
    return umb_clamp(128 + ((dx * dx + dy * dy - r2) >> 6), 0, 256);
}
static void umb_welcome(uint32_t ms)
{
    ump_co_t *s = &umb.co;
    int32_t t = (int32_t)(ms < 3550u ? ms : 3550u), a, x, y, h, g, c, gl, sg, off, mx, my, r;   /* (t: still from 3.55 s) */
    static uint8_t made;
    if (!made) {                                        /* (the palette, CORONA at rest: its state zeroed at boot) */
        umb_tabs();
        ump_lut_make();
        ump_co_init();
        made = 1;
    }
    umb_lay[0].pp = 0;                                  /* (the ground's plate after it: afresh) */
    if (ms >= UMW_T)
        return;
    g = 256 - umw_rp((int32_t)ms, (int32_t)UMW_T - 450, 450);
    g = g * g >> 8;                                     /* the end: to black over 0.45 s, faster as it goes */
    c = umw_in(t, 1450, 1000);                          /* the corona */
    sg = (umw_rp(t, 150, 550) * g) >> 8;                /* the sun, out of the black */
    a = 256 - umw_rp(t, 200, 1450);
    off = ((a * (256 + a)) >> 9) * 46;                  /* the moon's way still to go (px, Q8): slowing, its last
                                                         * pixel at ~1.6 s, as the bead flares */
    mx = 1912 - (off * 205 >> 12);                      /* (its centre, 1/16 px, less half a pixel: from the lower left) */
    my = 1912 + (off * 154 >> 12);
    a = umw_rp(t, 1300, 350);
    gl = t < 1650 ? a * a >> 8 : umb_exp((t - 1650) * 45 >> 8);   /* the bead: the diamond ring, 0.18 s to fade */
    r = umw_in(t, 1450, 2100) * 3;                      /* (the corona turning 3 streamers as it shows, slowing to still) */
    for (a = 0; a <= (int32_t)UMR_NA; a++) {            /* the corona at rest, turned, unfurling (REACH) as it comes;
                                                         * brighter than the plate's: no notes feed it */
        int32_t i = (a + (r >> 8)) & (UMR_NA - 1), b = s->base[i], S;
        S = ((b + (((s->base[(i + 1) & (UMR_NA - 1)] - b) * (r & 255)) >> 8)) * 3) >> 1;
        s->S[a] = (uint16_t)S;
        s->K[a] = (int16_t)(947 - (((180 + ((c * 300) >> 8)) * (S > 256 ? 256 : S)) >> 8));
    }
    s->P = (uint16_t)gl;                                /* (the chromosphere flaring with the bead) */
    umr_rm = 30;
    umr_k = 8192 / 30;
    umb.bare = 1;
    umm_x0 = 0;
    for (y = 0; y < 240; y += 120) {
        cv_begin(240, 120, 0);
        umm_y0 = y + UMW_DY;
        umm_lq = (g * c) >> 8;
        umm_p = cv_px, umm_w = 240u, umm_h = y ? 96u : 120u;                         /* (CORONA's maps end at its screen's foot, 240 - UMW_DY) */
        if (c)
            ump_co_draw(0, y + UMW_DY);
        umm_h = 120u;
        umm_lq = g;
        h = 33 + (off >> 8);                            /* the sun (r 29, its limb darker) and the moon (r 30): a box */
        for (a = 120 - h; a < 120 + h; a++) {
            int32_t ry = a - umm_y0;
            if ((uint32_t)ry >= 120u)
                continue;
            for (x = 120 - h; x < 120 + h; x++) {
                int32_t dx = x * 16 - 1912, dy = a * 16 - 1912, am = umw_out(x * 16 - mx, a * 16 - my, 480 * 480),
                        as = 256 - umw_out(dx, dy, 464 * 464);
                if (as || am < 128)
                    cv_px[ry * 240 + x] = ump_lut[((a & 1) << 1) | (x & 1)][UMP_IX(((((255 - ((dx * dx + dy * dy) >> 12)) * as) >> 8) * am) >> 8, sg)];
            }
        }
        for (a = -14; gl && a <= 14; a++)               /* the bead: a glow, and a star's four rays (none at the edge) */
            for (x = -14; x <= 14; x++) {
                int32_t p = a * x < 0 ? -a * x : a * x;
                umm_px(144 + x, 102 + a, gl * 17 / (6 + a * a + x * x + 6 * p) - gl / 12, 236, 250, 246);
            }
        if (y) {                                        /* the name rising, pale teal; the line under it, grey */
            a = umw_in(t, 2150, 650);
            cv_text_in(0, 60 - (a * 6 >> 8), 240, &AF_M, "U M B R A   L I V E", swap16(ump_lut[1][(((a * 215) >> 8) * g) >> 8]), 0);
            a = umw_in(t, 2550, 650);
            cv_text_in(0, 100, 240, &AF_X, "live visuals system built on Jiant", ux_gray((uint32_t)((((a * 150) >> 8) * g) >> 11)), 0);
        }
        cv_blit(0, (uint32_t)y);
    }
    umb.bare = 0;
}
