/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* HELIX (the file and its symbol still FIZZ's), a plate module (umb_plate.h): a double helix rising up the middle; made
 * to lie over WAX. Two threads wind round an axis that sways, beads on them at each eighth of a turn with a faint rung
 * between the pair; the near side bright and the beads there larger, the far dim; it goes out towards the top. A note
 * lights a bead in its track's colour on the helix at its pitch's height (C2 low .. C7 high), a ring flashing out from
 * it; the bead rides up with the helix and goes out. The kick swells the helix wide and flashes it; the rest of the kit
 * throws sparks off the threads, outward. Line art only: its light is added to the strip, so it lies on a layer as well
 * as it stands alone (REPLACE: black under it).
 * Elements: RISE (how fast it climbs), TWIST (how tightly it winds), SPREAD (how wide; the mix widens it), BEAD (the notes
 * that are no drum), PULSE (the kick), SPARK (the rest of the kit), SHINE (the near side's light), SWAY (how far the axis
 * snakes from side to side).
 * (NEW, API 6) BUBBLES (ON, RISE, MANY): bubbles rising round the helix, each orbiting it, near large, far small; a note
 * lets one in its colour; they burst at the top in eight rays.
 * (NEW, API 6) WAVES (ON, SPEED, REACH): ripples running out from the helix to both sides on the kick and the beat, a
 * wavy front each side and a fainter one behind, going out as they go. */
#include "plate_lib.h"

#define OFZ_N 14                                        /* bubbles at once */
#define OFZ_NB 12                                       /* lit beads at once */
#define OFZ_NK 12                                       /* sparks at once */
#define OFZ_NW 6                                        /* waves at once */
typedef struct {
    int16_t y[OFZ_N];                                   /* (NEW, API 6) BUBBLES: each how high (px, Q6) .. */
    uint16_t pop[OFZ_N];                                /* .. bursting: the ms left of it (0: whole) */
    uint8_t r[OFZ_N], v[OFZ_N], c[OFZ_N], sd[OFZ_N];    /* .. its size (px; 0: none), how hard, its track (7: none's), its dice */
    int16_t by[OFZ_NB];                                 /* each lit bead: where (px, Q6) .. */
    uint16_t bl[OFZ_NB];                                /* .. the ms left of it (0: none) */
    uint8_t bc[OFZ_NB], bv[OFZ_NB], bj[OFZ_NB];         /* .. its track, how hard, its thread */
    int16_t kx[OFZ_NK], ky[OFZ_NK], kv[OFZ_NK];         /* each spark: off the axis, where (px, Q4); how fast out (px a s) */
    uint16_t kl[OFZ_NK];                                /* .. the ms left of it (0: none) */
    uint32_t rs, spin, wb[OFZ_NW];                      /* the dice; the helix's turn (2^32 a turn); WAVES: each one's birth */
    uint16_t jolt, own;                                 /* the kick's swell (0..255); BUBBLES: the ms to the next of its own */
    uint8_t ws[OFZ_NW];                                 /* WAVES: each one's strength (0: none) */
    uint8_t n, nb, nk, nw, pb;                          /* the next bubble, bead, spark, wave; the beat a frame ago */
} ofz_t;

/* the helix's shape now: a turn every pf-th of 2^32 a px (TWIST: 230 .. 50 px a turn); its lowest row */
static uint32_t ofz_pf(const umb_lctx_t *cx) { return 0xFFFFFFFFu / (uint32_t)(230 - cx->k[1] * 180 / 99); }
static int32_t ofz_w(const umb_lctx_t *cx, const ofz_t *s)   /* how wide (px, Q8): SPREAD, the mix, the kick */
{
    return (6 + cx->k[2] * 44 / 99 + (cx->level >> 4) + s->jolt / 12) * 256;
}
static int32_t ofz_ax(const umb_lctx_t *cx, const umb_api_t *a, int32_t y)   /* the axis at row y (px): x (px, Q8) */
{
    return 120 * 256 + pl_pol(a, cx->k[7] * 48 / 99 * 256, cx->now * 150000u + (uint32_t)y * 9000000u);
}
/* thread j's phase at row Y (px, Q8): sin of it the side, cos how near */
static uint32_t ofz_ph(const ofz_t *s, uint32_t pf, int32_t Y, uint32_t j) { return (uint32_t)Y * (pf >> 8) + s->spin + j * 0x80000000u; }
static void ofz_let(ofz_t *s, int32_t r, int32_t v, uint32_t c)   /* (NEW, API 6) BUBBLES: one let go at the bottom */
{
    uint32_t i = s->n++ % OFZ_N;
    s->y[i] = (int16_t)((244 + r) * 64);
    s->pop[i] = 0;
    s->r[i] = (uint8_t)r;
    s->v[i] = (uint8_t)v;
    s->c[i] = (uint8_t)c;
    s->sd[i] = (uint8_t)pl_rnd(&s->rs);
}
static void ofz_wave(ofz_t *s, uint32_t now, int32_t v)   /* (NEW, API 6) WAVES: one sent out */
{
    uint32_t i = s->nw++ % OFZ_NW;
    s->wb[i] = now;
    s->ws[i] = (uint8_t)pl_clamp(v, 40, 255);
}
static void ofz_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    ofz_t *s = st;
    (void)a;
    s->rs = 811u + cx->now;
}
static void ofz_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    ofz_t *s = st;
    int32_t v, i, k, top = PL_TOP(cx) + 16;
    if (!nt->vel)
        return;
    if (cx->xk[1][0] && nt->drum && !nt->lane)          /* (NEW, API 6) WAVES: the kick sends one */
        ofz_wave(s, cx->now, nt->vel * 2);
    if (cx->xk[0][0] && !nt->drum)                      /* (NEW, API 6) BUBBLES: a note lets one in its colour */
        ofz_let(s, pl_clamp(2 + nt->vel / 24 + cx->xk[0][2] / 50, 2, 9), nt->vel * 2, nt->track);
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) != 0 && v > s->jolt)   /* PULSE (the kick) */
        s->jolt = (uint16_t)v;
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0) {   /* SPARK: the snare, the clap three, the rest one */
        uint32_t pf = ofz_pf(cx);
        for (k = nt->lane < 3u ? 3 : 1; k > 0; k--) {
            int32_t y = top + (int32_t)(pl_rnd(&s->rs) % (uint32_t)(236 - top)), o;
            i = s->nk++ % OFZ_NK;
            o = pl_pol(a, ofz_w(cx, s), ofz_ph(s, pf, y * 256, pl_rnd(&s->rs) & 1u));
            s->kx[i] = (int16_t)(o >> 4);
            s->ky[i] = (int16_t)(y * 16);
            s->kv[i] = (int16_t)((o < 0 ? -1 : 1) * (50 + v / 2 + (int32_t)(pl_rnd(&s->rs) % 50u)));
            s->kl[i] = 420;
        }
    }
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* BEAD: lit where its pitch lies, on its track's thread */
        return;
    i = s->nb++ % OFZ_NB;
    s->by[i] = (int16_t)((232 - pl_fold(nt->mo[1], pl_pk(nt->note), 256) * (220 - top) / 256) * 64);
    s->bl[i] = (uint16_t)(700 + v * 5);
    s->bc[i] = nt->track;
    s->bv[i] = (uint8_t)v;
    s->bj[i] = nt->track & 1u;
}
static void ofz_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    ofz_t *s = st;
    int32_t dt = (int32_t)cx->dt, top = PL_TOP(cx) + 4, i, d = dt * (6 + cx->k[0] * 60 / 99) * 256 / 1000;
    (void)a;
    s->spin += (uint32_t)d * (ofz_pf(cx) >> 8);         /* the helix climbs (RISE: 6 .. 66 px a second) .. */
    for (i = 0; i < OFZ_NB; i++)                        /* .. and its lit beads with it */
        if (s->bl[i]) {
            s->by[i] = (int16_t)(s->by[i] - (d >> 2));
            s->bl[i] = s->bl[i] > dt && s->by[i] > (top + 8) * 64 ? (uint16_t)(s->bl[i] - dt) : 0;
        }
    for (i = 0; i < OFZ_NK; i++)                        /* the sparks fly out */
        if (s->kl[i]) {
            s->kx[i] = (int16_t)(s->kx[i] + s->kv[i] * dt * 16 / 1000);
            s->kl[i] = s->kl[i] > dt ? (uint16_t)(s->kl[i] - dt) : 0;
        }
    s->jolt = pl_dec(s->jolt, dt * 218);                /* (0.3 s) */
    /* (NEW, API 6) WAVES: the beat sends one too, unless the kick just did; each gone past its reach */
    if (cx->xk[1][0] && cx->beat > s->pb + 100 && cx->now - s->wb[(s->nw + OFZ_NW - 1u) % OFZ_NW] > 200u)
        ofz_wave(s, cx->now, 110);
    s->pb = cx->beat;
    for (i = 0; i < OFZ_NW; i++)
        if (!cx->xk[1][0] || (cx->now - s->wb[i]) * (30u + cx->xk[1][1] * 120u / 99u) / 1000u > 30u + cx->xk[1][2] * 130u / 99u)
            s->ws[i] = 0;
    /* (NEW, API 6) BUBBLES: some of their own (MANY), rising (RISE: the larger the faster), bursting at the top */
    if (cx->xk[0][0] && cx->xk[0][2]) {
        if (s->own > dt)
            s->own = (uint16_t)(s->own - dt);
        else {
            ofz_let(s, (int32_t)(2u + pl_rnd(&s->rs) % 5u), 110, 7u);
            s->own = (uint16_t)(150u + (99u - cx->xk[0][2]) * 25u + pl_rnd(&s->rs) % 300u);
        }
    }
    for (i = 0; i < OFZ_N; i++) {
        int32_t r = s->r[i];
        if (!r)
            continue;
        if (!cx->xk[0][0]) {
            s->r[i] = 0;
            continue;
        }
        if (s->pop[i]) {
            if (s->pop[i] > dt)
                s->pop[i] = (uint16_t)(s->pop[i] - dt);
            else
                s->pop[i] = s->r[i] = 0;
            continue;
        }
        s->y[i] = (int16_t)(s->y[i] - (10 + cx->xk[0][1] * (8 + r) / 24) * dt * 64 / 1000 - ((s->jolt * dt) >> 5));
        if (s->y[i] < (top + 12 + r) * 64)
            s->pop[i] = 300;
    }
}
static void ofz_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const ofz_t *s = st;
    int32_t lo = PL_TOP(cx) + 16, H = 240 - lo, W = ofz_w(cx, s), fl = (s->jolt >> 2) + (cx->beat >> 4), i, j, k, y;
    int32_t sh = 40 + cx->k[6] * 2, gap = (int32_t)(0x20000000u / (ofz_pf(cx) >> 8)), Y;
    uint32_t pf = ofz_pf(cx);
    pl_clear(S);
    for (j = 0; j < 2; j++) {                           /* the threads: a line each, the near side bright, out at the top */
        int32_t pX = 0, pa = 0;
        for (y = 240; y >= lo; y -= 2) {
            uint32_t ph = ofz_ph(s, pf, y * 256, (uint32_t)j);
            int32_t nX = ofz_ax(cx, a, y) + pl_pol(a, W, ph), al = (40 + (((32768 + pl_sine(a, ph + 0x40000000u)) * sh * 3) >> 17) + fl) * (y - lo) / H;
            if (y < 240)
                pl_line(S, pX, (y + 2) * 256, nX, y * 256, (al + pa) >> 1, j ? PL_INK : PL_WT);
            pX = nX;
            pa = al;
        }
    }
    /* its beads, an eighth of a turn apart, on both threads at once, a rung between them */
    for (Y = (int32_t)(((uint32_t)(-(int32_t)s->spin) & 0x1FFFFFFFu) / (pf >> 8)); Y <= 240 * 256; Y += gap) {
        int32_t ax = ofz_ax(cx, a, Y >> 8), e = (Y - lo * 256) / H, X[2];
        if (e <= 0)
            continue;
        for (j = 0; j < 2; j++) {
            uint32_t ph = ofz_ph(s, pf, Y, (uint32_t)j);
            int32_t c = 32768 + pl_sine(a, ph + 0x40000000u);
            X[j] = ax + pl_pol(a, W, ph);
            pl_arc(S, a, X[j], Y, 300 + ((c * 520) >> 16), 0u, 0xFFFFFFFFu, ((60 + ((c * sh * 3) >> 17) + fl + (cx->beat >> 3)) * e) >> 8, j ? PL_INK : PL_WT);
        }
        pl_line(S, X[0], Y, X[1], Y, (90 * e) >> 8, PL_DEEP);
    }
    for (i = 0; i < OFZ_NB; i++)                        /* BEAD: lit in the track's colour, a ring flashing out from it */
        if (s->bl[i]) {
            uint32_t ph = ofz_ph(s, pf, s->by[i] * 4, s->bj[i]), t = 700u + s->bv[i] * 5u - s->bl[i];
            int32_t c = 32768 + pl_sine(a, ph + 0x40000000u), X = ofz_ax(cx, a, s->by[i] >> 6) + pl_pol(a, W, ph), al = (90 + s->bv[i]) * s->bl[i] / 1000;
            const uint8_t *col = pl_ncol(a, s->bc[i]);
            al = al * (140 + (c >> 9)) >> 8;
            pl_arc(S, a, X, s->by[i] * 4, 700 + ((c * 500) >> 16), 0u, 0xFFFFFFFFu, al * 2, col);
            pl_arc(S, a, X, s->by[i] * 4, 300, 0u, 0xFFFFFFFFu, al * 2, col);
            if (t < 400u)
                pl_arc(S, a, X, s->by[i] * 4, 768 + (int32_t)t * 7, 0u, 0xFFFFFFFFu, (int32_t)(400u - t) * s->bv[i] / 300, col);
        }
    for (i = 0; i < OFZ_NK; i++)                        /* SPARK: a short streak flying out, going out */
        if (s->kl[i]) {
            int32_t X = ofz_ax(cx, a, s->ky[i] >> 4) + s->kx[i] * 16, Yk = s->ky[i] * 16;
            pl_line(S, X - s->kv[i] * 20, Yk, X, Yk, s->kl[i] * 200 / 420, PL_WT);
        }
    /* (NEW, API 6) BUBBLES: each a ring orbiting the axis as it rises, near large and bright, far small and dim; its shine
     * a short arc inside; bursting: eight rays flying out */
    for (i = 0; i < OFZ_N; i++) {
        int32_t r = s->r[i], Yb = s->y[i] * 4, X, c, e, R;
        uint32_t ph = s->sd[i] * 16843009u + cx->now * (500000u + s->sd[i] * 3000u);
        const uint8_t *col = s->c[i] < UMB_NTRK ? pl_ncol(a, s->c[i]) : PL_INK;
        if (!r || Yb + (r + 9) * 256 < S->y0 * 256 || Yb - (r + 9) * 256 > (S->y0 + S->h) * 256)
            continue;
        c = 32768 + pl_sine(a, ph + 0x40000000u);
        X = ofz_ax(cx, a, Yb >> 8) + pl_pol(a, W + (6 + s->sd[i] % 22) * 256, ph);
        e = (90 + s->v[i] * 3 / 4) * (100 + (c >> 9)) >> 8;
        R = r * (180 + (c >> 10));
        if (s->pop[i]) {
            int32_t r0 = R + (300 - s->pop[i]) * 13, r1 = r0 + (2 + r / 3) * 256;
            for (k = 0; k < 8; k++) {
                uint32_t q = (uint32_t)k * 536870912u + s->sd[i] * 16843009u;
                pl_line(S, X + pl_pol(a, r0, q + 0x40000000u), Yb + pl_pol(a, r0, q), X + pl_pol(a, r1, q + 0x40000000u), Yb + pl_pol(a, r1, q),
                        e * s->pop[i] / 300, col);
            }
            continue;
        }
        pl_arc(S, a, X, Yb, R, 0u, 0xFFFFFFFFu, e, col);
        if (r > 3)
            pl_arc(S, a, X, Yb, R * 5 / 8, 0x90000000u, 0x28000000u, e, PL_WT);
    }
    /* (NEW, API 6) WAVES: each a wavy front running out from the helix's side, both ways (SPEED: 30 .. 150 px a second;
     * REACH: 30 .. 160 px), a fainter one behind; coming up as it leaves, going out as it goes, and at its ends */
    for (i = 0; i < OFZ_NW; i++)
        if (s->ws[i]) {
            int32_t mx = 30 + cx->xk[1][2] * 130 / 99, dd = (int32_t)((cx->now - s->wb[i]) * (30u + cx->xk[1][1] * 120u / 99u) / 10u),
                    al = s->ws[i] * (mx * 100 - dd) / (mx * 100) * (dd < 1500 ? dd : 1500) / 1500, sg;
            for (sg = -1; sg <= 1; sg += 2)
                for (k = 0; k < 2; k++) {
                    int32_t pX = 0, py = 0, D = W + 1536 + (dd * 256 / 100) - k * 1536;
                    for (y = lo; y <= 240; y += 6) {
                        int32_t nX = ofz_ax(cx, a, y) + sg * (D + pl_pol(a, 768 + dd / 12, (uint32_t)y * 60000000u - cx->now * 1500000u + (uint32_t)i * 0x2A000000u)),
                                env = pl_sine(a, (uint32_t)(y - lo) * (0x7FFFFFFFu / (uint32_t)H)) >> 7;
                        if (y > lo && D > W)
                            pl_line(S, pX, py, nX, y * 256, ((al * env) >> (k ? 10 : 8)), PL_WT);
                        pX = nX;
                        py = y * 256;
                    }
                }
        }
}

#include "splash/fizz.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_fizz = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(ofz_t), "HELIX",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 8u, {0, 0, 0},
    {{"RISE", 40}, {"TWIST", 50}, {"SPREAD", 50}, {"BEAD", 50}, {"PULSE", 50}, {"SPARK", 50}, {"SHINE", 50}, {"SWAY", 30}},
    ofz_init, ofz_note, ofz_frame, ofz_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0},
    "A double helix rising, a note in a bead", 3u, {0, 0, 0},
    {{"STRAND", {31, 61, 41, 61, 31, 21, 71, 21}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LEVEL, 2u), UMB_LINK(UMB_SRC_BEAT, 6u)}, {99, 99, 40, 30}},
     {"VORTEX", {91, 86, 26, 51, 71, 61, 51, 11}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 2u), 0}, {99, 99, 30, 0}},
     {"STORM", {61, 31, 81, 100, 100, 100, 61, 71}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 2u), UMB_LINK(UMB_SRC_LEVEL, 7u)}, {99, 99, 30, 40}}},
    65u, {0, 0, 0}, SPL_FIZZ,
    /* (API 6, the test) its new elements */
    2u, {0, 0, 0},
    {{"BUBBLES", 3u, 1u, {0, 0}, {{"ON", 0}, {"RISE", 50}, {"MANY", 50}},
      "Bubbles rise round the helix, each orbiting it, and burst at the top; a note lets one in its colour",
      {"Bubbles on or off", "How fast they rise", "How many rise on their own (0 none: only the notes')"}},
     {"WAVES", 3u, 1u, {0, 0}, {{"ON", 0}, {"SPEED", 40}, {"REACH", 50}},
      "Ripples run out from the helix to both sides on the kick and the beat, fading as they go",
      {"Waves on or off", "How fast they run out", "How far they reach before they are gone"}}}};
