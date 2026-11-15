/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* FILAMENT, the first plate built as a module (umb_plate.h): a taut string a track (4 or 6, as TRACKS). A note plucks its
 * track's string where its pitch falls (C2 at the left, C7 at the right; a drum where its lane is), its velocity the
 * height and the melody's contour the side (rising up, falling down); a higher note tightens the string so its waves run
 * faster; a held note keeps bowing it; let go, the string is damped and still within ~0.6 s. A 1D wave, 81 points a
 * string, 125 substeps a second. Its light is added to the strip (an overlay by nature): in REPLACE the strip is
 * cleared to black first (JIANT's), in ADD the strings light whatever is under them.
 * Elements: SWING (the pluck's height), RING (how long a string rings once let go), SPREAD (the strings' spacing: a
 * quarter .. twice); PLUCK (every note) and BOW (the notes that are no drum, held: they keep bowing the string): each
 * as hard as its value, its tracks LIVE VIZ CTRLS' to choose; GLOW (the strings' own light).
 * (NEW, API 6) BEADS (on/off; COUNT: one .. four a string; SLIDE: how fast): glass beads threaded on the strings, sliding
 * to and fro and riding their waves, brighter on the beat. FRETS (on/off; FRETS: how many; FLASH: how bright a pluck
 * lights them): frets across the strings, closer toward the right as on a neck; a pluck lights the one nearest it. */
#include "umb_plate.h"
#include "plate_lib.h"                                 /* (NEW, API 6) the line art the new elements draw with */

#define PLF_NP 81                                       /* a string's points (3 px apart) */
#define PLF_NH 4                                        /* the notes a string holds */
typedef struct {
    int16_t u[UMB_NTRK][PLF_NP], v[UMB_NTRK][PLF_NP];  /* each string's height and speed (px, Q8) */
    uint8_t c2[UMB_NTRK];                               /* its tension (Q8) */
    uint8_t hn[UMB_NTRK][PLF_NH], hx[UMB_NTRK][PLF_NH]; /* the notes it holds (+ 1), where */
    int16_t ha[UMB_NTRK][PLF_NH];                       /* how hard they bow it (the side its sign) */
    uint32_t hf[UMB_NTRK][PLF_NH], h0[UMB_NTRK][PLF_NH];/* how fast (phase a ms), since when */
    int8_t dir[UMB_NTRK];                               /* the side a pluck goes */
    uint8_t mv[UMB_NTRK];                               /* moving (still: nothing to work out) */
    uint8_t last[UMB_NTRK];                             /* each track's last note + 1 */
    uint32_t sub;                                       /* the substeps' clock (ms) */
    int32_t damp;                                       /* RING, once a frame: a substep's damping let go (Q15) */
    uint8_t fv[20];                                     /* (NEW, API 6) FRETS: each one's flash */
} plf_t;
#ifndef PLF_REST_RGB                                    /* (a plate file's build may set them: tools/build.py plate_files, */
#define PLF_REST_RGB 34, 60, 96                         /* FIL EMBER, the slot's test: warm strings at rest) */
#endif
#ifndef PLF_NAME
#define PLF_NAME "FILAMENT"
#endif
static const uint8_t PLF_REST[3] = {PLF_REST_RGB};      /* a string at rest: deep blue */

static int32_t plf_clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static uint16_t plf_swap(uint32_t c) { return (uint16_t)(((c >> 8) & 0xFFu) | ((c & 0xFFu) << 8)); }
/* the panel's pixel q (its colour read back, half a step up) and the light r g b (0..255) x w / 256 added */
static uint16_t plf_add(uint32_t q, int32_t r, int32_t g, int32_t b, int32_t w)
{
    int32_t R, G, B;
    q = plf_swap(q);
    R = plf_clamp((int32_t)((q >> 8) & 0xF8u) + 4 + ((r * w) >> 8), 0, 255);
    G = plf_clamp((int32_t)((q >> 3) & 0xFCu) + 2 + ((g * w) >> 8), 0, 255);
    B = plf_clamp((int32_t)((q << 3) & 0xF8u) + 4 + ((b * w) >> 8), 0, 255);
    return plf_swap((uint32_t)(((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3)));
}
/* add the light r g b x w / 256 to the screen's pixel x, y if the strip has it (its row's light applied) */
static void plf_px(const umb_strip_t *s, int32_t x, int32_t y, int32_t w, int32_t r, int32_t g, int32_t b)
{
    uint16_t *p;
    if (w <= 0 || (uint32_t)(x - s->x0) >= (uint32_t)s->w || (uint32_t)(y - s->y0) >= (uint32_t)s->h)
        return;
    w = (w * s->vg[y - s->y0]) >> 8;
    p = s->px + (uint32_t)(y - s->y0) * (uint32_t)s->w + (uint32_t)(x - s->x0);
    *p = plf_add(*p, r, g, b, w);
}
static int32_t plf_sine(const umb_api_t *a, uint32_t ph)   /* the sine, between the table's points */
{
    uint32_t i = ph >> 22;
    int32_t p = a->sine[i], q = a->sine[(i + 1u) & 1023u];
    return p + (((q - p) * (int32_t)((ph >> 7) & 0x7FFFu)) >> 15);
}

/* (NEW, API 6) FRETS: fret f of n (px across): closer toward the right, as on a neck (each 0.94 of the one before) */
static int32_t plf_fret(int32_t f, int32_t n)
{
    int32_t q = 4096, qn = 4096, i;                     /* (Q12) */
    for (i = 0; i <= n; i++) {
        if (i == f + 1)
            q = qn;
        qn = (qn * 3866) >> 12;
    }
    return 6 + (4096 - q) * 228 / (4096 - qn);
}
/* how hard note n drives control j (plate_lib.h pl_drive: its tracks as chosen, else own; its value its strength) */
static int32_t plf_drive(const umb_lctx_t *c, uint32_t j, const umb_note_t *n, int own)
{
    int32_t v = (c->et[j] ? c->et[j] >> n->track & 1u : (uint32_t)own) ? ((int32_t)n->vel * 2 + 1) * c->k[j] / 50 : 0;
    return v > 255 ? 255 : v;
}
static void plf_init(void *st, const umb_lctx_t *c, const umb_api_t *a)
{
    plf_t *m = st;
    (void)a;
    m->sub = c->now;
}
/* a note played (let go: vel 0): its string plucked, held; let go, no longer held */
static void plf_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    plf_t *m = st;
    uint32_t c = nt->track, n = nt->note, i, h;
    int32_t v, vb, k, x, w, iw, amp;
    for (h = 0; h < PLF_NH && m->hn[c][h] != n + 1u; h++)
        ;
    if (!nt->vel) {
        if (h < PLF_NH)
            m->hn[c][h] = 0;
        return;
    }
    v = plf_drive(cx, 3u, nt, 1);                       /* PLUCK */
    vb = plf_drive(cx, 4u, nt, !nt->drum);              /* BOW */
    if (!v && !vb)
        return;
    k = nt->drum ? (int32_t)nt->lane * 36 + 2 : plf_clamp(((int32_t)n - 36) * 4369 >> 10, 0, 256);   /* (0..1, Q8) */
    x = a->move(nt, 0u, k * 80, 80 * 256);              /* (the place: points, Q8; NOTE MOVEMENT's along the string) */
    if (cx->xk[1][0]) {                                 /* (NEW, API 6) FRETS: the one nearest the pluck lights */
        int32_t f, nf = 5 + cx->xk[1][1] * 15 / 99, bd = 1 << 30, best = 0, fx;
        for (f = 0; f < nf; f++) {
            fx = plf_fret(f, nf);
            if (pl_abs(fx - x * 3 / 256) < bd) {
                bd = pl_abs(fx - x * 3 / 256);
                best = f;
            }
        }
        f = 60 + v + vb / 2;
        m->fv[best] = (uint8_t)(f > 255 ? 255 : f > m->fv[best] ? f : m->fv[best]);
    }
    if (m->last[c] && n + 1u != m->last[c])
        m->dir[c] = (int8_t)(n + 1u > m->last[c] ? -1 : 1);   /* (rising: up) */
    if (!m->dir[c])
        m->dir[c] = -1;
    m->last[c] = (uint8_t)(n + 1u);
    w = 640 + (256 - k) * 7;                            /* (the pluck's half width: 2.5 .. 9.5 points, Q8) */
    iw = 65536 / w;
    amp = (1024 + v * 12) * ((int32_t)cx->k[0] + 1) / 51 * m->dir[c];   /* (4 .. 16 px, Q8, at SWING 50) */
    for (i = 1; v && i < PLF_NP - 1; i++) {
        int32_t d = (int32_t)i * 256 - x;
        d = d < 0 ? -d : d;
        if (d < w)
            m->u[c][i] = (int16_t)plf_clamp(m->u[c][i] / 2 + ((((amp * (w - d)) >> 8) * iw) >> 8), -3584, 3584);
    }
    m->c2[c] = (uint8_t)(26 + ((k * 205) >> 8));        /* (higher, tighter: 0.1 .. 0.9) */
    m->mv[c] = 1;
    if (!vb)                                            /* (a drum is a pluck: nothing held) */
        return;
    if (h == PLF_NH)
        for (h = 0; h < PLF_NH - 1u && m->hn[c][h]; h++)
            ;
    m->hn[c][h] = (uint8_t)(n + 1u);
    m->hx[c][h] = (uint8_t)plf_clamp((x + 128) >> 8, 1, PLF_NP - 2);
    m->hf[c][h] = 3435974u + (uint32_t)k * 67109u;     /* (0.8 .. 4.8 bows a second: phase a ms) */
    m->ha[c][h] = (int16_t)(vb * m->dir[c]);
    m->h0[c][h] = cx->now;
}
/* once a frame: RING; the substeps owed (125 a second, 8 a frame at most): the waves; a held note bows; let go, damped */
static void plf_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    plf_t *m = st;
    uint32_t c, i, h, n = 0, r = cx->k[1] + 1u, d = 3607587u / (r * r);   /* (RING: 1 - damp, 1387 at 50, as r^-2; 0: 8192) */
    m->damp = 32768 - (int32_t)(d < 8192u ? d : 8192u);
    while ((uint32_t)(cx->now - m->sub) >= 8u && n++ < 8u) {
        m->sub += 8u;
        for (c = 0; c < UMB_NTRK; c++) {
            int16_t *u = m->u[c], *v = m->v[c];
            int32_t held = 0, damp, mx = 0, q;
            if (!m->mv[c])
                continue;
            for (h = 0; h < PLF_NH; h++)
                if (m->hn[c][h]) {
                    uint32_t age = m->sub - m->h0[c][h];
                    if (age > 30000u) {                 /* (never let go: after 30 s, as if it were) */
                        m->hn[c][h] = 0;
                        continue;
                    }
                    held = 1;
                    i = m->hx[c][h];
                    v[i] = (int16_t)(v[i] + ((m->ha[c][h] * (plf_sine(a, age * m->hf[c][h]) >> 7)) >> 13));
                }
            damp = held ? 32478 : m->damp;              /* (Q15 a substep: held, 0.9 s; let go, 0.185 s at RING 50) */
            for (i = 1; i < PLF_NP - 1; i++)
                v[i] = (int16_t)(((v[i] + ((m->c2[c] * (u[i - 1] + u[i + 1] - 2 * u[i])) >> 8)) * damp) >> 15);
            for (i = 1; i < PLF_NP - 1; i++) {
                u[i] = (int16_t)plf_clamp(u[i] + v[i], -4096, 4096);
                q = u[i] < 0 ? -u[i] : u[i];
                mx = q > mx ? q : mx;
            }
            if (mx < 128 && !held) {                    /* (settled: exactly still, nothing more to send) */
                for (i = 0; i < PLF_NP; i++)
                    u[i] = v[i] = 0;
                m->mv[c] = 0;
            }
        }
    }
    if ((uint32_t)(cx->now - m->sub) >= 8u)
        m->sub = cx->now;                               /* (behind: never a burst) */
    for (i = 0; i < 20u; i++)                           /* (NEW, API 6) FRETS: a flash goes out in 0.5 s */
        m->fv[i] = (uint8_t)pl_dec(m->fv[i], (int32_t)cx->dt * 131);
}
/* the strings in the strip: each a line a column wide, its colour from the rest's deep blue to its track's as it moves
 * (16 steps), held notes tinting it; a glow either side while it moves */
static void plf_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *s)
{
    const plf_t *m = st;
    int32_t c, x, nl = cx->tracks, k2 = cx->k[2], sc = k2 <= 50 ? 64 + k2 * 192 / 50 : 256 + (k2 - 50) * 256 / 49,
            sp = (((nl > 4 ? 24 : 34) + (cx->bare ? 10 : 0)) * sc) >> 8, cy = cx->bare ? 120 : 150, gl = cx->k[5];   /* (GLOW: 50 as it was) */
                                                        /* (TOTALITY: the whole screen; SPREAD: a quarter .. twice) */
    if (s->mode == UMB_DRAW_REPLACE)                    /* (the ground: black, as JIANT's) */
        for (x = 0; x < s->w * s->h; x++)
            s->px[x] = 0;
    if (nl > 1 && sp * (nl - 1) > 224)                  /* (all on the screen) */
        sp = 224 / (nl - 1);
    for (c = 0; c < nl && c < (int32_t)UMB_NTRK; c++) {
        const int16_t *u = m->u[c];
        const uint8_t *tc = a->colour[(c * 3) % 8];
        int32_t y0 = cy + (2 * c - nl + 1) * sp / 2, py = -1, held = 0, h;
        if (y0 + 20 < s->y0 || y0 - 20 >= s->y0 + s->h)
            continue;
        for (h = 0; h < PLF_NH; h++)
            held |= m->hn[c][h];
        for (x = s->x0 > 0 ? s->x0 - 1 : 0; x < s->x0 + s->w; x++) {
            int32_t fi = (x * 21937) >> 8, i0 = fi >> 8, f = fi & 255, sv, y, e, al, r, g, b, k;
            if (i0 >= PLF_NP - 1) {
                i0 = PLF_NP - 2;
                f = 256;
            }
            sv = u[i0] + (((u[i0 + 1] - u[i0]) * f) >> 8);
            y = y0 * 256 + sv;
            if (py < 0)
                py = y;
            if (x < s->x0) {                            /* (the column before the strip: where its line comes from) */
                py = y;
                continue;
            }
            e = (((sv < 0 ? -sv : sv) >> 3) + (held ? 90 : 0));
            e = (plf_clamp(e, 0, 256) + 8) & ~15;
            al = (141 + ((115 * e) >> 8)) * gl / 50;
            r = PLF_REST[0] + (((tc[0] - PLF_REST[0]) * e) >> 8);
            g = PLF_REST[1] + (((tc[1] - PLF_REST[1]) * e) >> 8);
            b = PLF_REST[2] + (((tc[2] - PLF_REST[2]) * e) >> 8);
            if ((py > y ? py - y : y - py) < 256) {    /* (a point, shared between two rows) */
                plf_px(s, x, y >> 8, ((256 - (y & 255)) * al) >> 8, r, g, b);
                plf_px(s, x, (y >> 8) + 1, ((y & 255) * al) >> 8, r, g, b);
            } else                                      /* (steep: the column from the last one's row) */
                for (k = (py < y ? py : y) >> 8; k <= (py < y ? y : py) >> 8; k++)
                    plf_px(s, x, k, (al * 205) >> 8, r, g, b);
            if (e) {
                plf_px(s, x, (y >> 8) - 2, (31 * e) >> 8, r, g, b);
                plf_px(s, x, (y >> 8) + 2, (31 * e) >> 8, r, g, b);
            }
            py = y;
        }
    }
    /* (NEW, API 6) FRETS: lines across every string, faint, lit by a pluck (FLASH) */
    if (cx->xk[1][0] && nl > 0) {
        int32_t nf = 5 + cx->xk[1][1] * 15 / 99, ya = (cy - (nl - 1) * sp / 2 - 16) * 256, yb = (cy + (nl - 1) * sp / 2 + 16) * 256, f, fx;
        for (f = 0; f < nf; f++) {
            fx = plf_fret(f, nf) * 256;
            pl_line(s, fx, ya, fx, yb, 40 + ((m->fv[f] * (20 + cx->xk[1][2] * 3 / 2)) >> 8), m->fv[f] > 60 ? PL_INK : PL_DEEP);
        }
    }
    /* (NEW, API 6) BEADS: COUNT on each string, sliding to and fro (SLIDE) at their own pace, riding its waves */
    if (cx->xk[0][0])
        for (c = 0; c < nl && c < (int32_t)UMB_NTRK; c++) {
            const int16_t *u = m->u[c];
            int32_t y0 = cy + (2 * c - nl + 1) * sp / 2, k, nb = 1 + cx->xk[0][1] * 4 / 100;
            for (k = 0; k < nb; k++) {
                uint32_t h = pl_hash((uint32_t)(c * 4 + k) + 50u);
                int32_t bx = 120 * 256 + pl_pol(a, 112 * 256, cx->now * (uint32_t)(20000 + cx->xk[0][2] * 3000) * (4u + (h >> 30)) / 4u + h),
                        fi = ((bx >> 8) * 21937) >> 8, i0 = plf_clamp(fi >> 8, 0, PLF_NP - 2), f = fi & 255,
                        by = y0 * 256 + u[i0] + (((u[i0 + 1] - u[i0]) * f) >> 8) - 512, e = 150 + cx->beat / 3;
                pl_arc(s, a, bx, by, 640, 0u, 0xFFFFFFFFu, e, a->colour[(c * 3) % 8]);
                pl_splat(s, bx - 256, by - 256, e, PL_WT);
            }
        }
}

#include "splash/filament.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_filament = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(plf_t), PLF_NAME,
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 6u, {0, 0, 0},
    {{"SWING", 50}, {"RING", 50}, {"SPREAD", 50}, {"PLUCK", 50}, {"BOW", 50}, {"GLOW", 50}},
    plf_init, plf_note, plf_frame, plf_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, 0},
    "A taut string a track, plucked, bowed", 3u, {0, 0, 0},
    {{"HARP", {0, 86, 0, 86, 21, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 5u), 0}, {99, 99, 50, 0}},
     {"BOWED", {26, 61, 0, 16, 96, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LEVEL, 5u), UMB_LINK(UMB_SRC_LFO(0), 2u)}, {99, 99, 60, 30}},
     {"SNAP", {91, 21, 0, 100, 0, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_HIT(0), 0u), UMB_LINK(UMB_SRC_BEAT, 2u)}, {99, 99, 50, 30}}},
    48u, {0, 0, 0}, SPL_FILAMENT,
    /* (API 6, the test) its new elements */
    2u, {0, 0, 0},
    {{"BEADS", 3u, 1u, {0, 0}, {{"ON", 0}, {"COUNT", 30}, {"SLIDE", 40}}, "Glass beads threaded on the strings slide to and fro, riding the waves",
      {"Beads on or off", "How many on each string (one .. four)", "How fast they slide"}},
     {"FRETS", 3u, 1u, {0, 0}, {{"ON", 0}, {"FRETS", 50}, {"FLASH", 60}}, "Frets across the strings, as on a neck; a pluck lights the one nearest it",
      {"Frets on or off", "How many frets (five .. twenty)", "How bright a pluck lights its fret"}}}};
