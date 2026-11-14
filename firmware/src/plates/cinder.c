/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* CINDER as a plate module (umb_plate.h; moved out of ui_umbra_field.c unchanged, pixel for pixel): cold embers. A heat
 * field (60 x 60) rises, sways and cools (20 steps a second); seen through a crust of cracks (the noise table, ridged, two
 * scales): cooling heat lingers in the veins, hot heat glows through. A note drops a coal where its pitch falls (C2 left ..
 * C7 right), its track's band its height (low notes wider); held, it keeps feeding it. The kick a swell from below, the
 * snare and clap a scatter of sparks, the rest one spark. Track 3's heat glows yellow, track 4's orange (a tint byte a cell,
 * carried with the heat). Elements: RISE, COOL, SWAY; COAL (the notes that are no drum), SWELL (the kick), SPARK (the
 * rest of the kit): each as hard as its value, its tracks LIVE VIZ CTRLS' to choose.
 * (NEW, API 6) EMBERS (on/off; COUNT: how many; SWIRL: how wide they spiral): embers swirling up off the heat on
 * curling paths, yellow-orange when new, cooling through orange to deep red as they climb, flickering, flaring on the
 * beat, then out. SMOKE (on/off; CURL: how much the wisps curl; HEIGHT:
 * how high they climb): faint wisps of smoke going up over the embers, thicker as the music swells. */
#include "plate_lib.h"

typedef struct {
    uint8_t h[PL_S];                                    /* the heat, a byte a cell */
    uint8_t rp[PL_N], rc[PL_N];                         /* the step's rows as they were: the one above, this one */
    uint8_t pw[256], sm[256], c1[256], c2[256];         /* curves: the heat's glow (^0.75), the hot glow, the crust's layers */
    uint8_t hx[UMB_NTRK], hy[UMB_NTRK], hr[UMB_NTRK], hv[UMB_NTRK], hn[UMB_NTRK];   /* each track's held note: where
                                                         * (cells), how wide (Q4), how hard; the note + 1 */
    uint32_t T;                                         /* the steps */
    uint8_t t[PL_S], tr[PL_N];                          /* the heat's tint, a byte a cell; the step's row as it was */
    uint32_t rs, ms;                                    /* the dice, the steps' clock */
} plc_t;

/* two tints mixed, by the heat each brings (wa, wb): the share so, which the one bringing more */
static __attribute__((noinline)) uint8_t plc_tmix(uint32_t ta, int32_t wa, uint32_t tb, int32_t wb)
{
    return (uint8_t)(((wb > wa ? tb : ta) & 128u) | (uint32_t)(((int32_t)(ta & 127u) * wa + (int32_t)(tb & 127u) * wb) / (wa + wb + 1)));
}
/* add amt (0..255) to the heat round cx, cy (cells, Q8) within r (cells, Q8), falling off to the edge; track c's tint mixed in */
static void plc_stamp(plc_t *s, uint32_t c, int32_t cx, int32_t cy, int32_t r, int32_t amt)
{
    int32_t x, y, r2 = (r * r) >> 8;
    for (y = pl_clamp((cy - r) >> 8, 0, PL_N - 1); y <= pl_clamp((cy + r + 255) >> 8, 0, PL_N - 1); y++)
        for (x = pl_clamp((cx - r) >> 8, 0, PL_N - 1); x <= pl_clamp((cx + r + 255) >> 8, 0, PL_N - 1); x++) {
            int32_t dx = x * 256 - cx, dy = y * 256 - cy, d2 = (dx * dx + dy * dy) >> 8, i = y * PL_N + x, h = s->h[i], a;
            if (d2 > r2)
                continue;
            a = amt * (r2 - d2) / (r2 + 1);
            s->t[i] = plc_tmix(s->t[i], h, pl_tint(c), a);
            s->h[i] = (uint8_t)(h + a > 255 ? 255 : h + a);
        }
}
static void plc_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    plc_t *s = st;
    uint32_t i, k, r, p;
    (void)a;
    s->rs = 218u + cx->now;
    s->ms = cx->now;
    for (i = 0; i < 256u; i++) {
        int32_t t = pl_clamp(((int32_t)i - 89) * 256 / 166, 0, 256);
        r = 255u - (uint32_t)(2 * (int32_t)i - 255 < 0 ? 255 - 2 * (int32_t)i : 2 * (int32_t)i - 255);   /* (ridged) */
        for (p = r, k = 0; k < 6u; k++)
            p = p * r / 255u;
        s->c1[i] = (uint8_t)(p * 217u / 255u);          /* (0.85 r^7) */
        p = p * r / 255u * r / 255u;
        s->c2[i] = (uint8_t)(p * 115u / 255u);          /* (0.45 r^9) */
        s->pw[i] = (uint8_t)pl_isqrt(pl_isqrt(i * i * i * 255u));   /* (255 (i / 255)^0.75) */
        s->sm[i] = (uint8_t)((((t * t * (768 - 2 * t)) >> 16) * 128) >> 8);   /* (0.5 smoothstep 0.35 .. 1) */
    }
}
/* a stamp of heat where note n's track has moved it (NOTE MOVEMENT: the field 3 .. 57 cells either way) */
static void plc_at(plc_t *s, const umb_note_t *n, int32_t x, int32_t y, int32_t r, int32_t amt)
{
    plc_stamp(s, n->track, 768 + pl_fold(n->mo[0], x - 768, 13824), 768 + pl_fold(n->mo[1], y - 768, 13824), r, amt);
}
static void plc_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    plc_t *s = st;
    uint32_t c = nt->track, n = nt->note, lane = nt->lane;
    int32_t v, k, x, y, r, i;
    (void)a;
    if (!nt->vel) {
        if (s->hn[c] == n + 1u)
            s->hn[c] = 0;
        return;
    }
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !lane)) != 0)   /* SWELL (the kick): a swell from below */
        plc_at(s, nt, (int32_t)(14u * 256u + pl_rnd(&s->rs) % 8192u), 58 * 256, 1280 + 3 * v, (200 * v) >> 8);
    if ((v = pl_drive(cx, 5u, nt, nt->drum && lane)) != 0)
        for (i = lane < 3u ? 5 : 1; i > 0; i--) {       /* SPARK: the snare, the clap (and what is no drum): sparks; the rest: one */
            int32_t X = (int32_t)(4u * 256u + pl_rnd(&s->rs) % 13312u);
            plc_at(s, nt, X, (int32_t)(lane < 3u ? 26u * 256u + pl_rnd(&s->rs) % 7168u : 8u * 256u + pl_rnd(&s->rs) % 11776u),
                   lane < 3u ? 384 : lane == 4u ? 410 : 282, lane < 3u ? (230 * v) >> 8 : 110 + ((120 * v) >> 8));
        }
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* COAL */
        return;
    k = pl_pk(n);
    x = 768 + pl_fold(nt->mo[0], k * 54, 13824);
    y = 768 + pl_fold(nt->mo[1], (49 - 6 * (int32_t)c) * 256, 13824);
    r = 410 + (((256 - k) * 768) >> 8) + ((v * 384) >> 8);   /* (1.6 .. 6 cells: low notes wider) */
    plc_stamp(s, c, x, y, r, 140 + ((115 * v) >> 8));
    s->hx[c] = (uint8_t)(x >> 8);
    s->hy[c] = (uint8_t)(y >> 8);
    s->hr[c] = (uint8_t)(r >> 4);
    s->hv[c] = (uint8_t)v;
    s->hn[c] = (uint8_t)(n + 1u);
}
/* a step: the held notes feed; each cell takes the heat from a little below (RISE), swayed sideways row by row, blended
 * with its neighbours, and cools (COOL: 0.4 .. 6 s); written in place from the rows as they were. The tint goes with the
 * heat */
static void plc_step(plc_t *s, const umb_lctx_t *cx, const umb_api_t *a)
{
    int32_t k1 = (int32_t)cx->k[1], rise = 10 + (int32_t)cx->k[0] * 77 / 99, n = 8 + k1 * k1 * 112 / 9801,
            kd = 4096 - 4096 / n, x, y, i, sw = (38 + rise) * (int32_t)cx->k[2] / 50;   /* (SWAY: 0 none, 99 twice) */
    for (i = 0; i < (int32_t)UMB_NTRK; i++)
        if (s->hn[i])
            plc_stamp(s, (uint32_t)i, s->hx[i] * 256 + 128, s->hy[i] * 256 + 128, s->hr[i] * 11, 10 + ((14 * s->hv[i]) >> 8));
    s->T++;
    pl_cpy(s->rp, s->h, PL_N);
    for (y = 0; y < PL_N; y++) {
        uint8_t *hr = s->h + y * PL_N, *tr = s->t + y * PL_N;
        const uint8_t *nx = y < PL_N - 1 ? hr + PL_N : s->rc, *tn = y < PL_N - 1 ? tr + PL_N : s->tr;
        int32_t w = (pl_sine(a, (uint32_t)y * 157286400u + s->T * 27487791u) * sw) >> 15;   /* (the sway, cells Q8) */
        pl_cpy(s->rc, hr, PL_N);
        pl_cpy(s->tr, tr, PL_N);
        for (x = 0; x < PL_N; x++) {
            int32_t sx = pl_clamp(x * 256 - w, 0, (PL_N - 1) * 256 - 1), x0 = sx >> 8, fx = sx & 255;
            int32_t t = s->rc[x0] + (((s->rc[x0 + 1] - s->rc[x0]) * fx) >> 8), b = nx[x0] + (((nx[x0 + 1] - nx[x0]) * fx) >> 8);
            int32_t v = t + (((b - t) * rise) >> 8);
            int32_t nb = s->rc[x ? x - 1 : 1] + s->rc[x < PL_N - 1 ? x + 1 : PL_N - 2] + s->rp[x] + nx[x];
            uint32_t ta = s->tr[x0 + (fx >> 7)], tb = tn[x0 + (fx >> 7)];
            hr[x] = (uint8_t)(v = ((((v * 225 + nb * 8) >> 8)) * kd) >> 12);
            tr[x] = (uint8_t)(v < 8 ? 0u : ta != tb ? plc_tmix(ta, t * (256 - rise), tb, b * rise) : ta);
        }
        pl_cpy(s->rp, s->rc, PL_N);
    }
}
static void plc_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)   /* the steps owed: 20 a second, 4 at most */
{
    plc_t *s = st;
    uint32_t n = 0;
    while ((uint32_t)(cx->now - s->ms) >= 50u && n++ < 4u) {
        s->ms += 50u;
        plc_step(s, cx, a);
    }
    if ((uint32_t)(cx->now - s->ms) >= 50u)
        s->ms = cx->now;
}
static const uint8_t PLC_SMOKE[3] = {84, 104, 150};   /* (NEW, API 6) SMOKE's grey-blue */
static const uint8_t PLC_EMB[4][3] = {{255, 232, 130}, {255, 150, 30}, {230, 54, 10}, {120, 12, 2}};   /* (NEW, API 6) an
                                                         * ember cooling: new, orange, red, deep red */
/* (NEW, API 6) EMBERS: ember h (its dice) c px up, swirling sw (px, Q8 at the foot): where across (px, Q8) */
static int32_t plc_emb(const umb_api_t *a, uint32_t h, int32_t c, int32_t sw, uint32_t now)
{
    uint32_t ph = (uint32_t)c * (60000000u + (h >> 26) * 1600000u) * (h & 1u ? 1u : ~0u) + h + now * 6000u;
    return (int32_t)((h >> 12) % 220u + 10u) * 256 + pl_pol(a, sw * (128 + c) / 128, ph) + (pl_sine(a, now * 400000u + h) >> 9);
}
static void plc_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const plc_t *s = st;
    uint8_t xi[240], xf[240], bb[240];
    int32_t vv[PL_N + 1], x0 = S->x0, y0 = S->y0;
    int32_t cxx, cy;
    pl_cols(S, xi, xf);
    for (cy = 0; cy < S->h; cy++) {
        int32_t py = y0 + cy, vg = S->vg[cy], any = pl_vrow(s->h, py, vv, 255), dq = (py & 1) << 1;
        const uint16_t *L = a->pal[dq];
        uint16_t *row = S->px + cy * S->w;
        uint32_t u1 = (uint32_t)(x0 * 54 + 794), v1 = (uint32_t)(py * 54 + 1971);   /* (the crust: 0.21 and 0.45 texels a px) */
        uint32_t u2 = (uint32_t)(x0 * 115 - py * 31 + 4429), v2 = (uint32_t)(py * 115 + x0 * 31 + 486);
        for (cxx = 0; cxx < S->w; cxx++, u1 += 54u, u2 += 115u, v2 += 31u) {
            int32_t px = x0 + cxx, v = any ? PL_LERP(vv, xi[cxx], xf[cxx]) : 0, cr, b;
            bb[cxx] = 0;
            if (v <= 0) {
                row[cxx] = L[(px & 1) * 256];
                continue;
            }
            cr = s->c1[pl_tbil(a, (int32_t)u1, (int32_t)v1) >> 8] + s->c2[pl_tbil(a, (int32_t)u2, (int32_t)v2) >> 8];
            cr = cr > 255 ? 255 : cr;
            b = ((s->pw[v] * (61 + ((195 * cr) >> 8))) >> 8) + s->sm[v];
            bb[cxx] = (uint8_t)(b > 255 ? 255 : b);
            row[cxx] = L[(px & 1) * 256 + PL_IX(b, vg)];
        }
        if (any && pl_vrow(s->t, py, vv, 127)) {        /* the warm tints: the row again where it has one */
            const uint8_t *T = s->t + ((pl_clamp((2 * py - 3) << 5, 0, (PL_N - 1) * 256 - 1) + 128) >> 8) * PL_N;
            for (cxx = 0; cxx < S->w; cxx++) {
                uint32_t t;
                if (bb[cxx] >= PL_WLO && (t = (uint32_t)PL_LERP(vv, xi[cxx], xf[cxx])) != 0u)
                    row[cxx] = pl_wpx(a, bb[cxx], t | (T[xi[cxx] + (xf[cxx] >> 7)] & 128u), vg, PL_D2[dq | ((x0 + cxx) & 1)]);
            }
        }
    }
    /* (NEW, API 6) SMOKE: five wisps from over the embers up HEIGHT px (60 .. 200), each swinging wider (two strands) as it climbs (CURL),
     * going out toward its top; the level thickens it */
    if (cx->xk[1][0]) {
        int32_t H = 60 + cx->xk[1][2] * 140 / 99, cu = 3 + cx->xk[1][1] * 36 / 99, k, j;
        for (k = 0; k < 5; k++) {
            uint32_t h = pl_hash((uint32_t)k + 300u);
            int32_t bx = 24 + k * 48 + (int32_t)(h >> 28) - 8, ox = 0, oy = 0, by = 214 - (int32_t)(h >> 27 & 15u);
            for (j = 0; j <= 24; j++) {
                int32_t y = by - j * H / 24, x = bx * 256 + ((pl_sine(a, (uint32_t)j * 120000000u - cx->now * 1300000u + h) * cu * j) >> 11)
                                                 + j * (int32_t)(h >> 24 & 7u) * 16;
                if (j) {                                /* (two strands, a pixel apart: a wisp, not a hair) */
                    pl_line(S, ox, oy, x, y * 256, (24 - j) * (60 + cx->level / 3) / 24, PLC_SMOKE);
                    pl_line(S, ox + 384, oy, x + 384, y * 256, (24 - j) * (30 + cx->level / 5) / 24, PLC_SMOKE);
                }
                ox = x;
                oy = y * 256;
            }
        }
    }
    /* (NEW, API 6) EMBERS: COUNT (4 .. 40) rise off the heat (as fast as RISE has it, each its own pace), each up a spiral
     * of its own (SWIRL: 1 .. 8 px round at the foot, widening as it climbs; one way or the other) for its own height
     * (90 .. 200 px); as it climbs it cools (yellow-orange, orange, red, deep red) and dims, flickering, flaring on the
     * beat, brighter on the near side of its turn; a short trail behind it along its path */
    if (cx->xk[0][0]) {
        int32_t n = 4 + cx->xk[0][1] * 36 / 99, sw = 256 + cx->xk[0][2] * 1792 / 99, k, j;
        for (k = 0; k < n; k++) {
            uint32_t h = pl_hash((uint32_t)k + 900u), sp = 20u + (uint32_t)cx->k[0] / 2u + (h >> 28) * 3u;
            int32_t L = 90 + (int32_t)(h >> 21 & 127u) * 110 / 127, c = (int32_t)((cx->now * sp / 1000u + (h >> 8)) % (uint32_t)L),
                    y = 232 - (int32_t)(h >> 4 & 15u) - c, t = c * 768 / L, g = t >> 8, f = t & 255, x = plc_emb(a, h, c, sw, cx->now),
                    e = (256 - ((c * c / L) << 8) / L) * (200 + cx->beat / 3 + (pl_sine(a, cx->now * 9000000u + h * 7u) >> 10)
                         + (pl_sine(a, cx->now * 23000000u + h * 3u) >> 10)) >> 8;
            uint8_t cl[3];
            e = e * (210 + (pl_sine(a, (uint32_t)c * (60000000u + (h >> 26) * 1600000u) * (h & 1u ? 1u : ~0u) + h + cx->now * 6000u) >> 10)) >> 8;
            for (j = 0; j < 3; j++)
                cl[j] = (uint8_t)(PLC_EMB[g][j] + (((PLC_EMB[g + 1][j] - PLC_EMB[g][j]) * f) >> 8));
            e = pl_clamp(e, 0, 255);
            for (j = 3; j < 12 && j < c; j += 3)        /* (the trail: three pieces along its path, going out) */
                pl_line(S, plc_emb(a, h, c - j, sw, cx->now), (y + j) * 256, plc_emb(a, h, c - j + 3, sw, cx->now), (y + j - 3) * 256,
                        e * (14 - j) / 24, cl);
            pl_glint(S, x, y * 256, e, cl);
            pl_splat(S, x, y * 256, e, cl);                 /* (its core: twice the light) */
        }
    }
}

#include "splash/cinder.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_cinder = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(plc_t), "CINDER",
    UMB_KIND_FIELD, UMB_MODE(UMB_DRAW_REPLACE), 6u, {0, 0, 0},
    {{"RISE", 45}, {"COOL", 50}, {"SWAY", 50}, {"COAL", 50}, {"SWELL", 50}, {"SPARK", 50}},
    plc_init, plc_note, plc_frame, plc_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES},
    "Cold embers under a crust of cracks", 3u, {0, 0, 0},
    {{"PULSE", {0, 0, 0, 0, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 4u), UMB_LINK(UMB_SRC_LEVEL, 0u)}, {99, 99, 60, 50}},
     {"SMOLDER", {21, 76, 31, 0, 0, 21, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 2u), 0}, {99, 99, 40, 0}},
     {"FLARE", {71, 31, 0, 81, 0, 91, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 5u), UMB_LINK(UMB_SRC_BEAT, 0u)}, {99, 99, 60, 25}}},
    62u, {0, 0, 0}, SPL_CINDER,
    /* (API 6, the test) its new elements */
    2u, {0, 0, 0},
    {{"EMBERS", 3u, 1u, {0, 0}, {{"ON", 0}, {"COUNT", 50}, {"SWIRL", 50}}, "Red embers swirl up off the coals, cooling from yellow-orange to deep red, flickering out",
      {"Embers on or off", "How many rise at once", "How wide they spiral as they climb"}},
     {"SMOKE", 3u, 1u, {0, 0}, {{"ON", 0}, {"CURL", 50}, {"HEIGHT", 50}}, "Faint wisps of smoke climb over the embers, thicker as the music swells",
      {"Smoke on or off", "How much the wisps curl", "How high they climb"}}}};
