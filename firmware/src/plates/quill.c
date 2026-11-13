/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* QUILL, a plate module (umb_plate.h): a pen writing; made to lie over BLOT's ink. A point of light crosses the page
 * from the left in the loops of a running hand, line under line and from the top again, and what it has written fades
 * behind it, the oldest first. Left alone the loops are small and even. A note is a letter: the loops being written go
 * as tall as its pitch is high, and the stretch written while it sounds is in its track's colour. The kick is a swash:
 * the loop swells past the line above. The rest of the kit are dots over the line. 40 points a second, the last 200 of
 * them kept. Line art only: its light is added to the strip, so it lies on a layer as well as it stands alone (REPLACE:
 * black under it).
 * Elements: SPEED (the pen's), FADE (how much of the writing is left to see), SIZE (the loops' at rest), LOOPS (the notes
 * that are no drum), SWASH (the kick), DOTS (the rest of the kit), SLANT (the hand's lean: 20 upright), LINES (1 .. 5). */
#include "plate_lib.h"

#define OQU_N 200                                       /* the points kept */
#define OQU_ND 6                                        /* dots at once */
#define OQU_UP 255u                                     /* (a point the pen was lifted to: no line to it) */
typedef struct {
    uint8_t x[OQU_N], y[OQU_N], c[OQU_N];               /* each point: where (px), whose (a track + 1; 0: no one's; OQU_UP) */
    uint32_t dt[OQU_ND];                                /* the dots: since when (0: none) .. */
    uint8_t dx[OQU_ND], dy[OQU_ND], dv[OQU_ND];         /* .. where (px), how hard */
    uint32_t ph, ms, rs;                                /* where in the loop (2^32 a loop); the steps' clock; the dice */
    int32_t px;                                         /* the pen along its line (px, Q8) */
    uint16_t hd, sw;                                    /* the newest point; the swash (Q8) */
    uint8_t ln, amp, want, nc, nv, nd;                  /* the line it is on; the loops' height now and wanted (px); the
                                                         * note being written: its track + 1, the steps left of it */
} oqu_t;

/* the page: the lines' distance apart (px), the first one's top; the loops' height at rest */
static int32_t oqu_gap(const umb_lctx_t *cx, int32_t *first, int32_t *rest)
{
    int32_t gap;
    *first = PL_TOP(cx) + 8;
    gap = (236 - *first) / (1 + cx->k[7] * 4 / 99);
    *rest = pl_clamp(4 + cx->k[2] * 14 / 50, 4, gap - 10);
    return gap;
}
static void oqu_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    oqu_t *s = st;
    (void)a;
    s->rs = 3301u + cx->now;
    s->ms = cx->now;
    s->px = 14 * 256;
    s->c[0] = OQU_UP;
}
static void oqu_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    oqu_t *s = st;
    int32_t v, first, rest, gap = oqu_gap(cx, &first, &rest);
    uint32_t i;
    (void)a;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) > s->sw)   /* SWASH (the kick) */
        s->sw = (uint16_t)v;
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0) {   /* DOTS: over the line, by the pen */
        i = s->nd++ % OQU_ND;
        s->dt[i] = cx->now | 1u;
        s->dx[i] = (uint8_t)pl_clamp((s->px >> 8) + (int32_t)(pl_rnd(&s->rs) % 9u) - 4, 2, 237);
        s->dy[i] = (uint8_t)pl_clamp(first + s->ln * gap + gap - 11 - s->amp - (int32_t)(pl_rnd(&s->rs) % 5u), 2, 237);
        s->dv[i] = (uint8_t)v;
    }
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* LOOPS */
        return;
    s->want = (uint8_t)(rest + ((pl_pk(nt->note) * v * (gap - 10 - rest)) >> 16));
    s->nc = (uint8_t)(nt->track + 1u);
    s->nv = (uint8_t)(10 + v / 16);                     /* (0.25 .. 0.65 s of line) */
}
static void oqu_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    oqu_t *s = st;
    uint32_t n = pl_owed(cx->now, &s->ms, 25u);
    int32_t first, rest, gap = oqu_gap(cx, &first, &rest), nl = 1 + cx->k[7] * 4 / 99, v = 100 + cx->k[0] * 3;   /* (SPEED 50: 39 px a second) */
    while (n--) {
        int32_t amp, ht, up = 0;
        if (s->want < rest)
            s->want = (uint8_t)rest;
        else if (s->want > rest)
            s->want--;
        s->amp = (uint8_t)(s->amp + (s->want - s->amp + (s->want > s->amp ? 3 : 0)) / 4);
        s->px += v;
        s->ph += (uint32_t)v * 699051u;                 /* (a loop every 24 px) */
        if (s->px > 224 * 256) {                        /* the line's end: lifted, to the next one's start */
            s->px = 14 * 256;
            s->ln++;
            up = 1;
        }
        if (s->ln >= nl)
            s->ln = 0;
        amp = pl_clamp(s->amp + ((s->sw * gap) >> 9), 2, gap + 8);
        ht = (amp * (32768 - pl_sine(a, s->ph + 0x40000000u))) >> 8;   /* how high over the line now (px, Q8) */
        s->hd = (uint16_t)((s->hd + 1u) % OQU_N);
        /* (back at the loop's top: the hand runs on by less than the loop turns; leaning by its height) */
        s->x[s->hd] = (uint8_t)pl_clamp((s->px + (((4 + amp / 2) * pl_sine(a, s->ph)) >> 7) + ht * (cx->k[6] - 20) / 100) >> 8, 0, 239);
        s->y[s->hd] = (uint8_t)pl_clamp(first + s->ln * gap + gap - 6 - (ht >> 8), 1, 239);
        s->c[s->hd] = (uint8_t)(up ? OQU_UP : s->nv ? s->nc : 0u);
        if (s->nv)
            s->nv--;
        s->sw = pl_dec(s->sw, 5461);                    /* (0.3 s) */
    }
}
static void oqu_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const oqu_t *s = st;
    int32_t nv = 40 + cx->k[1] * 158 / 99, j;
    uint32_t i;
    pl_clear(S);
    for (j = 0; j < nv - 1; j++) {                      /* the writing: from the pen back, going out in sixteen steps */
        uint32_t p1 = (s->hd + OQU_N - (uint32_t)j) % OQU_N, p0 = (p1 + OQU_N - 1u) % OQU_N;
        int32_t e = (nv - j) * 16 / nv * 16;
        if (s->c[p1] == OQU_UP || !s->y[p0])
            continue;
        if (s->c[p1])
            pl_line(S, s->x[p0] * 256, s->y[p0] * 256, s->x[p1] * 256, s->y[p1] * 256, 60 + ((e * 195) >> 8), pl_ncol(a, s->c[p1] - 1u));
        else
            pl_line(S, s->x[p0] * 256, s->y[p0] * 256, s->x[p1] * 256, s->y[p1] * 256, (e * 150) >> 8, PL_WT);
    }
    pl_glint(S, s->x[s->hd] * 256, s->y[s->hd] * 256, 255, PL_WT);   /* the pen */
    for (i = 0; i < OQU_ND; i++) {
        uint32_t age = cx->now - s->dt[i];
        if (s->dt[i] && age < 1500u)
            pl_glint(S, s->dx[i] * 256, s->dy[i] * 256, pl_out(s->dv[i], age, 1500u), PL_WT);
    }
}

#include "splash/quill.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_quill = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(oqu_t), "QUILL",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 8u, {0, 0, 0},
    {{"SPEED", 50}, {"FADE", 50}, {"SIZE", 50}, {"LOOPS", 50}, {"SWASH", 50}, {"DOTS", 50}, {"SLANT", 50}, {"LINES", 50}},
    oqu_init, oqu_note, oqu_frame, oqu_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0},
    "A pen writing loops, taller with pitch", 3u, {0, 0, 0},
    {{"LETTER", {0, 100, 31, 0, 0, 0, 0, 100}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LEVEL, 0u), 0}, {99, 99, 30, 0}},
     {"AUTOGRF", {81, 41, 91, 100, 100, 0, 81, 1}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 4u), UMB_LINK(UMB_SRC_BEAT, 2u)}, {99, 99, 50, 25}},
     {"SCRAWL", {100, 21, 0, 0, 61, 100, 6, 51}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 6u), UMB_LINK(UMB_SRC_LEVEL, 2u)}, {99, 99, 60, 40}}},
    20u, {0, 0, 0}, SPL_QUILL};
