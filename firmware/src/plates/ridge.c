/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* RIDGE, a plate module (umb_plate.h): the land under a night sky; made to lie over AURORA. Two lines of hills along the
 * bottom, the far one high and broken, the near one low with pines on it and a fire; stars over them. A note is a
 * falling star: it comes down and to the right from the top, where across by its pitch (C2 left .. C7 right), in its
 * track's colour, a tail behind it, and is gone behind the far hills. The kick lights the far hills' line from the middle
 * outwards, as if something behind them had flashed. The rest of the kit throw sparks up from the fire. The sky turns:
 * each star comes out slowly, shines, fades and comes out again somewhere else (each its own 9 .. 18 s), a quarter of
 * them twinkling hard, one in eight a bright one with rays. Nothing here moves but what the song moves, the stars and
 * the fire's flicker: it costs the screen next to nothing. Line art
 * only: its light is added to the strip, so it lies on a layer as well as it stands alone (REPLACE: black under it).
 * Elements: STARS (their light), FADE (how long a falling star lasts), SIZE (the hills' height), METEOR (the notes that
 * are no drum), ECHO (the kick), SPARKS (the rest of the kit; 0: no fire), RIDGE (the hills' light), PINES (theirs).
 * (NEW, API 6) MOON (ON, PHASE, HALO): a moon over the far hills, new to full; its halo swells with the music.
 * (NEW, API 6) SMOKE (ON, RISE, DRIFT): two strands of smoke curling up from the fire, leaning with the wind. */
#include "plate_lib.h"

#define ORI_NM 6                                        /* falling stars at once */
#define ORI_NS 8                                        /* sparks at once */
#define ORI_NP 12                                       /* pines */
#define ORI_NT 26                                       /* stars */
#define ORI_FX 70                                       /* the fire's place across (px) */
typedef struct {
    uint32_t mt[ORI_NM], st[ORI_NS], et;                /* since when (0: none): each falling star, each spark, the echo */
    uint8_t mx[ORI_NM], my[ORI_NM], mv[ORI_NM], mc[ORI_NM];   /* a falling star: where it began (px), how hard, whose */
    uint8_t sd[ORI_NS], sv[ORI_NS];                     /* a spark: its own dice, how hard */
    uint32_t rs;
    uint8_t ev, nm, ns;                                 /* the echo: how hard */
} ori_t;

/* the hills' row at x (px): the far line's (nr 0) or the near one's, SIZE their height */
static int32_t ori_y(const umb_lctx_t *cx, const umb_api_t *a, int32_t x, int nr)
{
    int32_t h = pl_tbil(a, x * (nr ? 31 : 19) + (nr ? 9000 : 0), nr ? 2816 : 1280) >> 8, p = (int32_t)(pl_hash((uint32_t)(x >> 4) + 200u) >> 24),
            q = (int32_t)(pl_hash((uint32_t)(x >> 4) + 201u) >> 24);
    if (nr)
        return 228 - ((h * (10 + cx->k[2] / 5)) >> 8);
    h = (h + p + (((q - p) * (x & 15)) >> 4)) >> 1;     /* (the far ones: a rise and fall, and a corner of its own every 16 px) */
    return 198 - ((h * (24 + cx->k[2])) >> 8);
}
/* (NEW, API 6) MOON: its place and radius (px) */
#define ORI_MX 186
#define ORI_MR 11
static void ori_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    ori_t *s = st;
    (void)a;
    s->rs = 1913u + cx->now;
}
static void ori_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    ori_t *s = st;
    uint32_t i;
    int32_t v;
    (void)a;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) != 0) {   /* ECHO (the kick) */
        s->et = cx->now | 1u;
        s->ev = (uint8_t)v;
    }
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0) {   /* SPARKS */
        i = s->ns++ % ORI_NS;
        s->st[i] = cx->now | 1u;
        s->sd[i] = (uint8_t)pl_rnd(&s->rs);
        s->sv[i] = (uint8_t)v;
    }
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* METEOR */
        return;
    i = s->nm++ % ORI_NM;
    s->mt[i] = cx->now | 1u;
    s->mx[i] = (uint8_t)pl_nx(nt, 0, 196);
    s->my[i] = (uint8_t)(PL_TOP(cx) + 4 + nt->track * 9);
    s->mv[i] = (uint8_t)v;
    s->mc[i] = nt->track;
}
static void ori_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)   /* (nothing to step: all of it is told by the clock) */
{
    (void)st;
    (void)cx;
    (void)a;
}
static void ori_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const ori_t *s = st;
    int32_t top = PL_TOP(cx), rl = cx->k[6] * 2, pn = cx->k[7] * 2, life = 350 + cx->k[1] * 7, fy = ori_y(cx, a, ORI_FX, 1) - 2, i, k;
    uint32_t ea = cx->now - s->et;
    pl_clear(S);
    for (i = 0; i < ORI_NT; i++) {                      /* the stars, each with its own slow flicker and its own life: out
                                                         * over a quarter of it, shining, gone over a quarter, a while dark;
                                                         * each life a new place */
        uint32_t h0 = pl_hash((uint32_t)i + 9u), P = 9000u + (h0 >> 20) % 9000u, t = cx->now + h0 % P, u = t % P * 256u / P,
                 h = pl_hash((uint32_t)i * 977u + t / P * 131u + 9u);
        int32_t env = u < 64u ? (int32_t)u * 4 : u < 160u ? 256 : u < 224u ? (224 - (int32_t)u) * 4 : 0,
                X = (int32_t)((h >> 8) % 240u) * 256, Y = (top + 4 + (int32_t)((h >> 18) % (uint32_t)(112 - top))) * 256,
                e = ((cx->k[0] * (90 + (pl_sine(a, cx->now * (200000u + (h >> 24) * 3000u) + h) >> (h & 3u ? 9 : 8)))) >> 5) * env >> 8;
        if (h >> 29)
            pl_splat(S, X, Y, e, PL_WT);
        else
            pl_star(S, X, Y, (env >> 7) * 256 + 256, e, PL_WT);
    }
    for (k = 0; k < 2; k++) {                           /* the hills: the far line (the echo along it), the near one */
        int32_t py = ori_y(cx, a, 0, k);
        for (i = 1; i <= 30; i++) {
            int32_t y = ori_y(cx, a, i * 8, k), e = k ? rl / 2 : rl, d = pl_abs(pl_abs(i - 15) * 30 - (int32_t)ea);
            if (!k && s->et && ea < 600u && d < 90)     /* (from the middle to the edges in 0.45 s) */
                e += pl_out(s->ev, ea, 600u) * (90 - d) / 90;
            pl_line(S, (i - 1) * 2048, py * 256, i * 2048, y * 256, e > 255 ? 255 : e, PL_INK);
            py = y;
        }
    }
    for (i = 0; i < ORI_NP; i++) {                      /* the pines on the near hills: a stem and three pairs of boughs */
        uint32_t h = pl_hash((uint32_t)i + 60u);
        int32_t x = 6 + (int32_t)((h >> 8) % 228u), y = ori_y(cx, a, x, 1), ht = 9 + (int32_t)((h >> 20) % 10u);
        if (pl_abs(x - ORI_FX) < 9)                     /* (none in the fire) */
            continue;
        pl_line(S, x * 256, y * 256, x * 256, (y - ht) * 256, pn, PL_INK);
        for (k = 1; k <= 3; k++) {
            int32_t ay = (y - ht + (k - 1) * ht / 4) * 256, by = ay + (ht / 4 + 1) * 256, w = (1 + k) * 256;
            pl_line(S, x * 256, ay, x * 256 - w, by, pn, PL_INK);
            pl_line(S, x * 256, ay, x * 256 + w, by, pn, PL_INK);
        }
    }
    if (cx->k[5])                                       /* the fire: a warm point that flickers; its sparks, 0.9 s up */
        pl_glint(S, ORI_FX * 256, fy * 256, 110 + (pl_tbil(a, (int32_t)(cx->now * 40u & 0x1FFFu), 0) >> 9), PL_WARM[1]);
    for (i = 0; i < ORI_NS; i++) {
        uint32_t age = cx->now - s->st[i];
        if (s->st[i] && age < 900u)
            pl_splat(S, ORI_FX * 256 + pl_pol(a, 3 * 256 + (int32_t)age * 4, age * 3000000u + s->sd[i] * 16843009u) + ((int32_t)s->sd[i] - 128) * (int32_t)age / 50,
                     fy * 256 - 512 - (int32_t)age * 12, pl_out(s->sv[i], age, 900u), PL_WARM[0]);
    }
    if (cx->xk[0][0]) {                                 /* (NEW, API 6) MOON: the lit limb on the right, the dark one faint;
                                                         * the terminator an ellipse's half by PHASE, the lit part hatched
                                                         * row by row; the halo round it with the level, a breath on the beat */
        int32_t X = ORI_MX * 256, Y = (top + 34) * 256, R = ORI_MR * 256, t = 256 - cx->xk[0][1] * 512 / 99, y0, hl;
        pl_arc(S, a, X, Y, R, 0xC0000000u, 0x80000000u, 230, PL_WT);
        pl_arc(S, a, X, Y, R, 0x40000000u, 0x80000000u, 40, PL_WT);
        for (k = -ORI_MR + 1; k < ORI_MR; k++) {        /* (a row of the disc: from the terminator to the limb) */
            int32_t w = (int32_t)pl_isqrt((uint32_t)(ORI_MR * ORI_MR - k * k) << 16);   /* (its half width, px Q8) */
            y0 = Y + k * 256;
            pl_line(S, X + (w * t >> 8), y0, X + w, y0, 150, PL_WT);
        }
        hl = (cx->level * (cx->xk[0][2] + 10) >> 7) + (cx->beat * cx->xk[0][2] >> 9);
        pl_arc(S, a, X, Y, R * 2 + (cx->beat << 2), 0u, 0xFFFFFFFFu, hl / 2, PL_INK);
        pl_arc(S, a, X, Y, R * 3 / 2 + (cx->beat << 1), 0u, 0xFFFFFFFFu, hl / 3, PL_WT);
    }
    if (cx->xk[1][0])                                   /* (NEW, API 6) SMOKE: two strands of 14 points up from the fire,
                                                         * curling, leaning with DRIFT; thicker with the level */
        for (k = 0; k < 2; k++) {
            int32_t h = (40 + cx->xk[1][1]) * 256 / 14, dr = (cx->xk[1][2] - 30) * 4, px = ORI_FX * 256, py = fy * 256 - 768, j;
            for (j = 1; j <= 14; j++) {
                int32_t x = ORI_FX * 256 + dr * j * j / 14 + pl_pol(a, j * 160, cx->now * (2400000u + (uint32_t)k * 1100000u) - (uint32_t)j * 0x16000000u + (uint32_t)k * 0x50000000u),
                        y = fy * 256 - 768 - j * h, e = (70 + (cx->level >> 2)) * (15 - j) / 14;
                pl_line(S, px, py, x, y, e, j < 3 ? PL_WARM[0] : PL_WT);
                px = x;
                py = y;
            }
        }
    for (i = 0; i < ORI_NM; i++) {                      /* the falling stars: 200 px a second, a tail of 28 px */
        uint32_t age = cx->now - s->mt[i];
        int32_t X = s->mx[i] * 256 + (int32_t)age * 41, Y = s->my[i] * 256 + (int32_t)age * 31, e;
        if (!s->mt[i] || age >= (uint32_t)life || Y > ori_y(cx, a, X >> 8, 0) * 256)   /* (gone behind the hills) */
            continue;
        e = pl_out(s->mv[i], age, (uint32_t)life);
        pl_line(S, X, Y, X - 2867, Y - 2150, e, pl_ncol(a, s->mc[i]));
        pl_line(S, X - 2867, Y - 2150, X - 5734, Y - 4301, e / 3, pl_ncol(a, s->mc[i]));
        pl_glint(S, X, Y, e, PL_WT);
    }
}

#include "splash/ridge.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_ridge = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(ori_t), "RIDGE",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 8u, {0, 0, 0},
    {{"STARS", 50}, {"FADE", 50}, {"SIZE", 50}, {"METEOR", 50}, {"ECHO", 50}, {"SPARKS", 50}, {"RIDGE", 50}, {"PINES", 50}},
    ori_init, ori_note, ori_frame, ori_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0},
    "Hills at night under falling stars", 3u, {0, 0, 0},
    {{"CAMP", {0, 0, 31, 0, 26, 100, 0, 81}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LEVEL, 5u), UMB_LINK(UMB_SRC_BEAT, 6u)}, {99, 99, 40, 25}},
     {"SHOWER", {81, 91, 0, 100, 0, 1, 31, 21}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LEVEL, 0u), 0}, {99, 99, 40, 0}},
     {"ALPS", {21, 0, 100, 0, 100, 0, 91, 1}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 4u), UMB_LINK(UMB_SRC_LFO(0), 2u)}, {99, 99, 50, 20}}},
    42u, {0, 0, 0}, SPL_RIDGE,
    /* (API 6, the test) its new elements */
    2u, {0, 0, 0},
    {{"MOON", 3u, 1u, {0, 0}, {{"ON", 0}, {"PHASE", 30}, {"HALO", 50}},
      "A moon hangs over the far hills; its halo swells with the music and breathes on the beat",
      {"Moon on or off", "How much of it is lit, new to full", "How bright its halo grows"}},
     {"SMOKE", 3u, 1u, {0, 0}, {{"ON", 0}, {"RISE", 50}, {"DRIFT", 60}},
      "Two strands of smoke curl up from the fire, leaning with the wind",
      {"Smoke on or off", "How high the smoke rises", "Which way the wind leans it (30 straight up)"}}}};
