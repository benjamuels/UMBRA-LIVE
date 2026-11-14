/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* MURMUR, a plate module (umb_plate.h): a flock of birds at dusk, wheeling; made to lie over RAPIDS. Each bird is two
 * short strokes, its wings, beating, set along the way it flies. Each goes round a point that wanders the sky, at a
 * distance and a pace of its own, the inner ones lapping the outer: so the flock holds together, stretches, folds over
 * itself and never rests. A note calls the flock to its place (its pitch
 * across, its track down) for a moment, and a third of the birds take its track's colour. The kick scatters them from
 * their middle; the snare and the clap turn the wheeling the other way, the rest of the kit drive it faster. 40 steps
 * a second. Line art only: its light is added to the strip, so it lies on a layer as well as it stands alone (REPLACE:
 * black under it).
 * Elements: SPEED (how fast they fly), FADE (how long a bird keeps a note's colour), SIZE (the wings'), SWOOP (the notes
 * that are no drum), SCATTR (the kick), WHEEL (the rest of the kit), FLOCK (how many: 8 .. 40), SPREAD (how loose).
 * (NEW, API 6) HAWK (ON, STOOP, SHY): a hawk soaring round the whole frame near its edges, slow strong beats and long
 * glides, tipped into its turns; on the kick it stoops in toward the middle, into the flock; the birds near it shy away.
 * (NEW, API 6) CALLS (ON, REACH, ECHOES): rings of sound spreading from where a note calls the flock, in its colour. */
#include "plate_lib.h"

#define OMU_N 40                                        /* birds at most */
typedef struct {
    int16_t x[OMU_N], y[OMU_N];                         /* each bird: where (px, Q6) .. */
    int16_t vx[OMU_N], vy[OMU_N];                       /* .. how fast (px a step, Q6) */
    uint8_t e[OMU_N], c[OMU_N];                         /* .. the note's light on it, whose (its track) */
    int16_t ax, ay;                                     /* where a note called them (px, Q6) .. */
    uint16_t at;                                        /* .. and the ms left of the call (0: none) */
    uint16_t wh;                                        /* the rest of the kit's drive (Q8) */
    uint32_t wp, ms, rs;                                /* where their point is on its round; the steps' clock; the dice */
    int8_t dir;                                         /* which way they wheel */
    uint8_t nn;
    /* (NEW, API 6) HAWK: its stoop, since when (0: none), how hard; CALLS: each ring since when, where (px, Q6), how
     * hard, whose */
    uint8_t hv, nc;
    uint32_t ht, ct[4];
    int16_t rx[4], ry[4];
    uint8_t cv[4], cc[4];
} omu_t;

static int32_t omu_len(int32_t x, int32_t y)            /* how long x, y is, near enough (never 0) */
{
    x = pl_abs(x);
    y = pl_abs(y);
    return (x > y ? x + y / 2 : y + x / 2) + 1;
}
/* (NEW, API 6) HAWK: where it is at t (px, Q6): soaring round the whole frame near its edges, a lap in 22 s, on a loop
 * squared off toward the corners; as it stoops (1.2 s) drawn in toward the middle, into the flock, and out again */
static void omu_hawk(const omu_t *s, const umb_lctx_t *cx, const umb_api_t *a, uint32_t t, int32_t *x, int32_t *y)
{
    uint32_t ph = t * 195000u, age = t - s->ht;
    int32_t mid = (PL_TOP(cx) + 240) / 2, ry = (240 - PL_TOP(cx)) / 2 - 12, dx, dy, am;
    *x = 120 * 64 + pl_pol(a, 112 * 64, ph + 0x40000000u) - pl_pol(a, 12 * 64, ph * 3u + 0x40000000u);
    *y = mid * 64 + pl_pol(a, ry * 64, ph) + pl_pol(a, ry * 64 / 9, ph * 3u);
    if (s->ht && age < 1200u) {
        am = (pl_sine(a, age * 1789569u) >> 7) * (30 + cx->xk[0][1]) * s->hv >> 10;
        dx = 120 * 64 - *x;
        dy = mid * 64 - *y;
        *x += dx * am / omu_len(dx, dy);
        *y += dy * am / omu_len(dx, dy);
    }
}
static void omu_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    omu_t *s = st;
    uint32_t i;
    (void)a;
    s->rs = 6007u + cx->now;
    s->ms = cx->now;
    s->dir = 1;
    for (i = 0; i < OMU_N; i++) {
        s->x[i] = (int16_t)((60u + pl_rnd(&s->rs) % 120u) * 64u);
        s->y[i] = (int16_t)((60u + pl_rnd(&s->rs) % 120u) * 64u);
        s->vx[i] = (int16_t)((int32_t)(pl_rnd(&s->rs) % 201u) - 100);
        s->vy[i] = (int16_t)((int32_t)(pl_rnd(&s->rs) % 201u) - 100);
    }
}
static void omu_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    omu_t *s = st;
    int32_t v, i, cnt = 8 + cx->k[6] * 32 / 99, mx = 0, my = 0;
    (void)a;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) != 0) {   /* SCATTR (the kick): each thrown from the flock's middle */
        for (i = 0; i < cnt; i++) {
            mx += s->x[i];
            my += s->y[i];
        }
        for (i = 0; i < cnt; i++) {
            int32_t dx = s->x[i] - mx / cnt, dy = s->y[i] - my / cnt, d = omu_len(dx, dy), to = ((96 + cx->k[0] * 26 / 10) * v) >> 8;
            s->vx[i] = (int16_t)(s->vx[i] + dx * to / d);
            s->vy[i] = (int16_t)(s->vy[i] + dy * to / d);
        }
    }
    /* (NEW, API 6) HAWK: the kick sends it down (1.2 s), then it soars at least 2.4 s before the next */
    if (cx->xk[0][0] && nt->drum && !nt->lane && (!s->ht || cx->now - s->ht >= 3600u)) {
        s->ht = cx->now | 1u;
        s->hv = (uint8_t)(128 + nt->vel);
    }
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) != 0) {   /* WHEEL */
        if (nt->lane < 3u)
            s->dir = (int8_t)-s->dir;
        if (v > s->wh)
            s->wh = (uint16_t)v;
    }
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))         /* SWOOP */
        return;
    s->ax = (int16_t)(pl_nx(nt, 50, 190) * 64);
    s->ay = (int16_t)(pl_ny(nt, PL_TOP(cx) + 50, 190) * 64);
    s->at = (uint16_t)(400 + v * 2);
    s->nn++;
    for (i = s->nn % 3u; i < OMU_N; i += 3) {
        s->e[i] = (uint8_t)v;
        s->c[i] = nt->track;
    }
    if (cx->xk[1][0]) {                                 /* (NEW, API 6) CALLS: a ring from where the note called them */
        i = s->nc++ % 4u;
        s->ct[i] = cx->now | 1u;
        s->rx[i] = s->ax;
        s->ry[i] = s->ay;
        s->cv[i] = (uint8_t)v;
        s->cc[i] = nt->track;
    }
}
static void omu_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    omu_t *s = st;
    uint32_t n = pl_owed(cx->now, &s->ms, 25u);
    int32_t cnt = 8 + cx->k[6] * 32 / 99, k1 = cx->k[1], vmax = 96 + cx->k[0] * 26 / 10, i;   /* (SPEED 50: 140 px a second at most) */
    while (n--) {
        int32_t tx, ty;
        s->wp += 2200000u + cx->k[0] * 44000u;          /* (their point: round in 24 s) */
        if (s->at) {
            tx = s->ax;
            ty = s->ay;
            s->at = (uint16_t)(s->at > 25u ? s->at - 25u : 0u);
        } else {
            tx = 120 * 64 + pl_pol(a, 50 * 64, s->wp * 2u);
            ty = (PL_TOP(cx) + 240) * 32 + pl_pol(a, 40 * 64, s->wp * 3u + 0x30000000u);
        }
        for (i = 0; i < cnt; i++) {
            uint32_t h = pl_hash((uint32_t)i);
            /* (each wants to go round a place of its own by the point, at a distance of its own, 4 .. 60 px: along the
             * round, and in or out toward its distance; it turns to that way a quarter a step) */
            int32_t dx = tx + ((int32_t)(h >> 22 & 31u) - 16) * 64 - s->x[i], dy = ty + ((int32_t)(h >> 27) - 16) * 64 - s->y[i], d = omu_len(dx, dy),
                    ux = dx * 256 / d, uy = dy * 256 / d,
                    in = pl_clamp((d - (4 + (int32_t)(h >> 8 & 63u) * cx->k[7] * 9 / 500) * 64) / 8, -256, 256),
                    sp = (vmax * (160 + (int32_t)(h >> 16 & 63u)) * (256 + s->wh)) >> 16,
                    vx = s->vx[i] + ((((ux * in - uy * s->dir * 256) >> 8) * sp >> 8) - s->vx[i]) / 4,
                    vy = s->vy[i] + ((((uy * in + ux * s->dir * 256) >> 8) * sp >> 8) - s->vy[i]) / 4, m;
            m = omu_len(vx, vy);
            if (m > vmax * 2) {
                vx = vx * vmax * 2 / m;
                vy = vy * vmax * 2 / m;
            }
            s->vx[i] = (int16_t)vx;
            s->vy[i] = (int16_t)vy;
            s->x[i] = (int16_t)pl_clamp(s->x[i] + vx, -30 * 64, 270 * 64);
            s->y[i] = (int16_t)pl_clamp(s->y[i] + vy, -30 * 64, 270 * 64);
            s->e[i] = (uint8_t)pl_dec(s->e[i], 25 * 65536 / (300 + k1 * k1 * 4 / 5));   /* (FADE 50: 2.3 s) */
        }
        s->wh = pl_dec(s->wh, 3277);                    /* (0.5 s) */
        if (cx->xk[0][0]) {                             /* (NEW, API 6) HAWK: the birds within SHY of it pushed away */
            int32_t hx, hy, r = (16 + cx->xk[0][2] * 3 / 4) * 64;
            omu_hawk(s, cx, a, cx->now, &hx, &hy);
            for (i = 0; i < cnt; i++) {
                int32_t dx = s->x[i] - hx, dy = s->y[i] - hy, d = omu_len(dx, dy);
                if (d < r) {
                    s->vx[i] = (int16_t)(s->vx[i] + dx * (r - d) / d / 6);
                    s->vy[i] = (int16_t)(s->vy[i] + dy * (r - d) / d / 6);
                }
            }
        }
    }
}
static void omu_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const omu_t *s = st;
    int32_t cnt = 8 + cx->k[6] * 32 / 99, w = 640 + cx->k[2] * 20, i;   /* (SIZE 50: wings of 6 px) */
    pl_clear(S);
    for (i = 0; i < cnt; i++) {
        int32_t X = s->x[i] * 4, Y = s->y[i] * 4, m, bx, by, ox, oy, f, e;
        uint8_t col[3];
        if (Y + w + 512 < S->y0 * 256 || Y - w - 512 > (S->y0 + S->h) * 256)
            continue;
        m = omu_len(s->vx[i], s->vy[i]);
        f = (w * (140 + (pl_sine(a, cx->now * 30000000u + (uint32_t)i * 0x13579BDFu) >> 9))) >> 8;   /* (seven beats a second) */
        bx = s->vx[i] * (w / 2) / m;                    /* (back along its way; out to either side by the beat) */
        by = s->vy[i] * (w / 2) / m;
        ox = -s->vy[i] * f / m;
        oy = s->vx[i] * f / m;
        e = pl_mix(col, PL_WT, pl_ncol(a, s->c[i]), 120, s->e[i]);
        pl_line(S, X, Y, X - bx + ox, Y - by + oy, e, col);
        pl_line(S, X, Y, X - bx - ox, Y - by - oy, e, col);
    }
    if (cx->xk[0][0]) {   /* (NEW, API 6) HAWK, seen from below along its way: broad wings, fingered tips, a fanned tail;
                           * three slow strong beats then a long glide (3.6 s), wings held out, tipped into the turn
                           * (the inner wing foreshortened), folded half in as it stoops; warm */
        int32_t p[6], ux, uy, l, tl, f = 0, sf = 0, k, j, sp;
        uint32_t cy = cx->now % 3600u, age = cx->now - s->ht;
        static const int8_t HW[9][4] = {{2, 8, 4, 45}, {4, 45, -4, 100}, {-4, 100, -10, 50}, {-10, 50, -5, 8},   /* (a wing: along
                                                         * px, out % of its span; the fingers, the tail: out px) */
                                        {-5, 84, -11, 104}, {-2, 92, -6, 110}, {-5, 3, -15, 6}, {-15, 6, -15, 0}, {2, 2, 8, 0}};
        for (k = 0; k < 3; k++)
            omu_hawk(s, cx, a, cx->now + (uint32_t)k * 160u - 160u, &p[k * 2], &p[k * 2 + 1]);
        ux = p[4] - p[0];
        uy = p[5] - p[1];
        l = omu_len(ux, uy);
        tl = pl_clamp(((p[2] - p[0]) * (p[5] - p[3]) - (p[3] - p[1]) * (p[4] - p[2])) / (l * l / 4 / 256 + 1) * 3, -64, 64);
        ux = ux * 256 / l;
        uy = uy * 256 / l;
        if (cy < 1500u)
            f = pl_sine(a, cy * 8589934u) >> 7;         /* (the beats: 0.5 s each) */
        if (s->ht && age < 1200u)
            sf = pl_sine(a, age * 1789569u) >> 8;
        for (k = -1; k <= 1; k += 2) {
            sp = (26 * 256 * (256 - pl_abs(f) / 3 - sf) >> 8) * (256 - k * tl) >> 8;   /* (k 1: its right wing) */
            for (j = 0; j < 9; j++) {                   /* (a wing's points out by its span, swung forward and back by
                                                         * the beat the further out; the tail, the head out in px) */
                int32_t a0 = HW[j][0] * 256 + (j < 6 ? f * HW[j][1] / 25 : 0), a1 = HW[j][2] * 256 + (j < 6 ? f * HW[j][3] / 25 : 0),
                        c0 = k * HW[j][1] * (j < 6 ? sp / 100 : 256), c1 = k * HW[j][3] * (j < 6 ? sp / 100 : 256);
                pl_line(S, p[2] * 4 + ((ux * a0 - uy * c0) >> 8), p[3] * 4 + ((uy * a0 + ux * c0) >> 8),
                        p[2] * 4 + ((ux * a1 - uy * c1) >> 8), p[3] * 4 + ((uy * a1 + ux * c1) >> 8), j < 4 ? 230 : 180, PL_WARM[0]);
            }
        }
        pl_glint(S, p[2] * 4 + ux * 8, p[3] * 4 + uy * 8, 255, PL_WT);
    }
    if (cx->xk[1][0])                                   /* (NEW, API 6) CALLS: each ring and its echoes behind it */
        for (i = 0; i < 4; i++) {
            uint32_t age = cx->now - s->ct[i];
            int32_t life = 1400, R = (20 + cx->xk[1][1]) * 256, ne = 1 + cx->xk[1][2] * 3 / 99, k, r, e;
            if (!s->ct[i] || age >= (uint32_t)life)
                continue;
            for (k = 0; k < ne; k++) {
                r = (int32_t)age * (R / life) - k * 9 * 256;
                e = pl_out(90 + s->cv[i], age, (uint32_t)life) * (ne - k) / ne;
                if (r > 256)
                    pl_arc(S, a, s->rx[i] * 4, s->ry[i] * 4, r, 0u, 0xFFFFFFFFu, e, pl_ncol(a, s->cc[i]));
            }
        }
}

#include "splash/murmur.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_murmur = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(omu_t), "MURMUR",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 8u, {0, 0, 0},
    {{"SPEED", 50}, {"FADE", 50}, {"SIZE", 50}, {"SWOOP", 50}, {"SCATTR", 50}, {"WHEEL", 50}, {"FLOCK", 50}, {"SPREAD", 50}},
    omu_init, omu_note, omu_frame, omu_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0, 0},
    "A flock of birds that swoops to notes", 3u, {0, 0, 0},
    {{"DUSK", {31, 81, 0, 0, 0, 31, 100, 71}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 7u), 0}, {99, 99, 30, 0}},
     {"STARTLE", {81, 0, 0, 0, 100, 100, 0, 31}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 4u), UMB_LINK(UMB_SRC_LEVEL, 0u)}, {99, 99, 50, 40}},
     {"RIBBON", {61, 100, 71, 100, 21, 0, 100, 1}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 2u), UMB_LINK(UMB_SRC_LEVEL, 7u)}, {99, 99, 30, 40}}},
    60u, {0, 0, 0}, SPL_MURMUR,
    /* (API 6, the test) its new elements */
    2u, {0, 0, 0},
    {{"HAWK", 3u, 1u, {0, 0}, {{"ON", 0}, {"STOOP", 50}, {"SHY", 50}},
      "A hawk soars round the edges of the frame with slow wingbeats and long glides, stooping into the flock on every kick; the birds near it shy away",
      {"Hawk on or off", "How far in it dives on the kick", "How far the birds keep from it"}},
     {"CALLS", 3u, 1u, {0, 0}, {{"ON", 0}, {"REACH", 50}, {"ECHOES", 50}},
      "Rings of sound spread from where each note calls the flock, in the note's colour",
      {"Calls on or off", "How far the rings spread", "How many rings follow each call (1 to 4)"}}}};
