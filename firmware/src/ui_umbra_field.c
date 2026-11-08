/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* (included by ui_umbra.c, after the melody plates) built for size, as the main loop is (tools/size_fns.py); ump_backdrop
 * stays a call. Prototyped in prototypes/umbra-kiln-alternatives.html.
 * ---------------------------------------------------------- CINDER, GRANULE, CORONA, TARN: the field plates ---
 * Each draws every pixel of its strip from a little state: a byte field read bilinearly (or a map made once), and the cold
 * palette as the panel takes it, dithered 2 x 2 (ump_lut: a plain pixel one read). No lighting, no per-pixel loop over the
 * notes; what changes steps (16 levels), so between notes a plate is still and HOME's blit sends nothing.
 * CINDER: cold embers. A heat field (60 x 60) rises, sways and cools (20 steps a second); seen through a crust of cracks
 *   (the noise table, ridged, two scales): cooling heat lingers in the veins, hot heat glows through. A note drops a coal
 *   where its pitch falls (C2 left .. C7 right), its track's band its height (low notes wider); held, it keeps feeding it.
 *   The kick a swell from below, the snare and clap a scatter of sparks, the rest one spark. RISE, COOL.
 * GRANULE: the sun's surface. 12 x 12 granules (a granule and a profile byte each 4 x 4 px block: lanes dark, hearts
 *   bright), each glowing faintly; a note lights its own (its place on the circle of fifths and its octave across, its track's
 *   band down) in its track's colour; the kick a cluster, the snare three, the hats bright points in the lanes. BOIL: how
 *   often a granule dies and is born nearby (its neighbourhood remapped once). FADE.
 * CORONA: the eclipse. The moon's disc (44 px) black, the corona's 128 streamers out from it, each a falloff from the limb;
 *   a note feeds the streamer at its place on the circle of fifths (the root at the top, as SAROS), the bass broad, a high
 *   note fine; the kick the chromosphere (the limb's ring), the snare a rose prominence, the hats Baily's beads. REACH: how
 *   far a bright streamer runs. FADE.
 * TARN: the eclipse in dark water. A ripple tank (48 x 48, 24 steps a second) bends the reflection (a pixel offset, no
 *   resampling) and its stars; the crests catch a teal sheen; it stills exactly (a dead band). A note a drop (pitch across,
 *   track down), the kick a heavy one by the moon, the snare one, the hats a star's glint. DRAG, BEND. */
static uint8_t ump_on = 0xFFu;                          /* the field plate drawn last (0xFF: none: it starts afresh) */
static int32_t ump_lq;                                  /* LIGHT, Q8, this frame */
static const int8_t UMP_D2[4] = {-8, 0, 4, -4};        /* the dither, 2 x 2 (fixed to the screen): [(y & 1) x 2 + (x & 1)] */
static uint16_t ump_lut[4][256] __attribute__((section(".pool")));   /* the palette as the panel takes it, a dither each */
static const uint8_t UMP_WT[3] = {236, 250, 246}, UMP_SH[3] = {70, 150, 160};   /* white-teal; the water's sheen */
/* a row's darkening (under the knob row and the footer; TOTALITY none), LIGHT in it */
static int32_t ump_vg(int32_t py)
{
    return ((umb.bare ? 256 : py < 56 ? 108 : py < 76 ? 108 + (py - 56) * 7 : py >= 226 ? 128 : 256) * ump_lq) >> 8;
}
/* a pixel: the palette at v (0..255) and the light r g b added, darkened by vg (Q8), dithered by d (-8..7), as the panel
 * takes it */
static inline __attribute__((always_inline)) uint16_t ump_rgb(int32_t v, int32_t r, int32_t g, int32_t b, int32_t vg, int32_t d)
{
    int32_t i = (v * vg) >> 8, R, G, B;
    const uint8_t *P = umb_pal + (i < 0 ? 0 : i > 255 ? 255 : i) * 3;
    R = P[0] + ((r * vg) >> 8) + (d >> 1);
    G = P[1] + ((g * vg) >> 8) + (d >> 2);
    B = P[2] + ((b * vg) >> 8) + (d >> 1);
    R = R < 0 ? 0 : R > 255 ? 255 : R;
    G = G < 0 ? 0 : G > 255 ? 255 : G;
    B = B < 0 ? 0 : B > 255 ? 255 : B;
    return swap16((uint32_t)(((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3)));
}
static void ump_lut_make(void)
{
    uint32_t k, i;
    for (k = 0; k < 4u; k++)
        for (i = 0; i < 256u; i++)
            ump_lut[k][i] = ump_rgb((int32_t)i, 0, 0, 0, 256, UMP_D2[k]);
}
#define UMP_IX(v, vg) ump_ix(((v) * (vg)) >> 8)
static inline __attribute__((always_inline)) int32_t ump_ix(int32_t i) { return i < 0 ? 0 : i > 255 ? 255 : i; }
/* a 60 x 60 field (cells 4 px, centres 4 i + 2) across the strip: each column's cell and blend */
static void ump_cols(int32_t x0, uint8_t *xi, uint8_t *xf)
{
    uint32_t x;
    for (x = 0; x < cv_w; x++) {
        int32_t t = umb_clamp((2 * (x0 + (int32_t)x) - 3) << 5, 0, (UMB_N - 1) * 256 - 1);
        xi[x] = (uint8_t)(t >> 8);
        xf[x] = (uint8_t)(t & 255);
    }
}
/* pixel row py of field f blended up and down into vv (x 256, 61: the last again); 0: all of it 0 */
static int32_t ump_vrow(const uint8_t *f, int32_t py, int32_t *vv)
{
    int32_t t = umb_clamp((2 * py - 3) << 5, 0, (UMB_N - 1) * 256 - 1), fy = t & 255, k, any = 0;
    const uint8_t *r0 = f + (t >> 8) * UMB_N, *r1 = r0 + UMB_N;
    for (k = 0; k < UMB_N; k++)
        any |= vv[k] = (r0[k] << 8) + (r1[k] - r0[k]) * fy;
    vv[UMB_N] = vv[UMB_N - 1];
    return any;
}
#define UMP_LERP(vv, xi, xf) (((vv)[xi] + ((((vv)[(xi) + 1] - (vv)[xi]) * (xf)) >> 8)) >> 8)
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
/* a fade in steps: of n, at age (ms) of life (ms), Q8 (0 when gone) */
static int32_t ump_fade(uint32_t age, uint32_t life, uint32_t n) { return age >= life ? 0 : (int32_t)((n - age * n / life) * 256u / n); }

/* ------------------------------------------------------------------------------------------------- CINDER --- */
/* add amt (0..255) to f round cx, cy (cells, Q8) within r (cells, Q8), falling off to the edge */
static void ump_stamp(uint8_t *f, int32_t cx, int32_t cy, int32_t r, int32_t amt)
{
    int32_t x, y, r2 = (r * r) >> 8;
    for (y = umb_clamp((cy - r) >> 8, 0, UMB_N - 1); y <= umb_clamp((cy + r + 255) >> 8, 0, UMB_N - 1); y++)
        for (x = umb_clamp((cx - r) >> 8, 0, UMB_N - 1); x <= umb_clamp((cx + r + 255) >> 8, 0, UMB_N - 1); x++) {
            int32_t dx = x * 256 - cx, dy = y * 256 - cy, d2 = (dx * dx + dy * dy) >> 8, v;
            if (d2 > r2)
                continue;
            v = f[y * UMB_N + x] + amt * (r2 - d2) / (r2 + 1);
            f[y * UMB_N + x] = (uint8_t)(v > 255 ? 255 : v);
        }
}
static void ump_ci_init(void)
{
    ump_ci_t *s = &umb.ci;
    uint32_t i, k, r, p;
    for (i = 0; i < 256u; i++) {
        int32_t t = umb_clamp(((int32_t)i - 89) * 256 / 166, 0, 256);
        r = 255u - (uint32_t)(2 * (int32_t)i - 255 < 0 ? 255 - 2 * (int32_t)i : 2 * (int32_t)i - 255);   /* (ridged) */
        for (p = r, k = 0; k < 6u; k++)
            p = p * r / 255u;
        s->c1[i] = (uint8_t)(p * 217u / 255u);          /* (0.85 r^7) */
        p = p * r / 255u * r / 255u;
        s->c2[i] = (uint8_t)(p * 115u / 255u);          /* (0.45 r^9) */
        s->pw[i] = (uint8_t)umb_isqrt(umb_isqrt(i * i * i * 255u));   /* (255 (i / 255)^0.75) */
        s->sm[i] = (uint8_t)((((t * t * (768 - 2 * t)) >> 16) * 128) >> 8);   /* (0.5 smoothstep 0.35 .. 1) */
    }
}
/* a stamp of heat where track c's notes have moved it (NOTE MOVEMENT: the field 3 .. 57 cells either way) */
static void ump_ci_at(uint32_t c, int32_t x, int32_t y, int32_t r, int32_t amt)
{
    ump_stamp(umb.ci.h, 768 + umn_at(c, 0u, x - 768, 13824), 768 + umn_at(c, 1u, y - 768, 13824), r, amt);
}
static void ump_ci_note(uint32_t c, uint32_t n, int32_t v, uint32_t drum, uint32_t lane)
{
    ump_ci_t *s = &umb.ci;
    int32_t k, x, y, r, i;
    if (!v) {
        if (s->hn[c] == n + 1u)
            s->hn[c] = 0;
        return;
    }
    if (drum) {
        if (!lane)                                      /* the kick: a swell from below */
            ump_ci_at(c, (int32_t)(14u * 256u + umb_rnd() % 8192u), 58 * 256, 1280 + 3 * v, (200 * v) >> 8);
        else
            for (i = lane < 3u ? 5 : 1; i > 0; i--)     /* the snare, the clap: sparks; the rest: one */
                ump_ci_at(c, (int32_t)(4u * 256u + umb_rnd() % 13312u),
                          (int32_t)(lane < 3u ? 26u * 256u + umb_rnd() % 7168u : 8u * 256u + umb_rnd() % 11776u),
                          lane < 3u ? 384 : lane == 4u ? 410 : 282, lane < 3u ? (230 * v) >> 8 : 110 + ((120 * v) >> 8));
        return;
    }
    k = ump_pk(n);
    x = 768 + umn_at(c, 0u, k * 54, 13824);
    y = 768 + umn_at(c, 1u, (49 - 6 * (int32_t)c) * 256, 13824);
    r = 410 + (((256 - k) * 768) >> 8) + ((v * 384) >> 8);   /* (1.6 .. 6 cells: low notes wider) */
    ump_stamp(s->h, x, y, r, 140 + ((115 * v) >> 8));
    s->hx[c] = (uint8_t)(x >> 8);
    s->hy[c] = (uint8_t)(y >> 8);
    s->hr[c] = (uint8_t)(r >> 4);
    s->hv[c] = (uint8_t)v;
    s->hn[c] = (uint8_t)(n + 1u);
}
/* a step: the held notes feed; each cell takes the heat from a little below (RISE), swayed sideways row by row, blended with
 * its neighbours, and cools (COOL: 0.4 .. 6 s); written in place from the rows as they were */
static void ump_ci_step(void)
{
    ump_ci_t *s = &umb.ci;
    int32_t k1 = (int32_t)umv(0u, 1u), rise = 10 + (int32_t)umv(0u, 0u) * 77 / 99, n = 8 + k1 * k1 * 112 / 9801,
            kd = 4096 - 4096 / n, x, y, i, sw = (38 + rise) * (int32_t)umv(0u, 2u) / 50;   /* (SWAY: 0 none, 99 twice) */
    for (i = 0; i < NTRK; i++)
        if (s->hn[i])
            ump_stamp(s->h, s->hx[i] * 256 + 128, s->hy[i] * 256 + 128, s->hr[i] * 11, 10 + ((14 * s->hv[i]) >> 8));
    s->T++;
    memcpy(s->rp, s->h, UMB_N);
    for (y = 0; y < UMB_N; y++) {
        uint8_t *hr = s->h + y * UMB_N;
        const uint8_t *nx = y < UMB_N - 1 ? hr + UMB_N : s->rc;
        int32_t w = (sine_i((uint32_t)y * 157286400u + s->T * 27487791u) * sw) >> 15;   /* (the sway, cells Q8) */
        memcpy(s->rc, hr, UMB_N);
        for (x = 0; x < UMB_N; x++) {
            int32_t sx = umb_clamp(x * 256 - w, 0, (UMB_N - 1) * 256 - 1), x0 = sx >> 8, fx = sx & 255;
            int32_t t = s->rc[x0] + (((s->rc[x0 + 1] - s->rc[x0]) * fx) >> 8), b = nx[x0] + (((nx[x0 + 1] - nx[x0]) * fx) >> 8);
            int32_t v = t + (((b - t) * rise) >> 8);
            int32_t nb = s->rc[x ? x - 1 : 1] + s->rc[x < UMB_N - 1 ? x + 1 : UMB_N - 2] + s->rp[x] + nx[x];
            hr[x] = (uint8_t)(((((v * 225 + nb * 8) >> 8)) * kd) >> 12);
        }
        memcpy(s->rp, s->rc, UMB_N);
    }
}
static void ump_ci_draw(int32_t x0, int32_t y0)
{
    ump_ci_t *s = &umb.ci;
    uint8_t xi[240], xf[240];
    int32_t vv[UMB_N + 1];
    uint32_t cx, cy;
    ump_cols(x0, xi, xf);
    for (cy = 0; cy < cv_h; cy++) {
        int32_t py = y0 + (int32_t)cy, vg = ump_vg(py), any = ump_vrow(s->h, py, vv);
        const uint16_t *L = ump_lut[(py & 1) << 1];
        uint16_t *row = cv_px + cy * cv_w;
        uint32_t u1 = (uint32_t)(x0 * 54 + 794), v1 = (uint32_t)(py * 54 + 1971);   /* (the crust: 0.21 and 0.45 texels a px) */
        uint32_t u2 = (uint32_t)(x0 * 115 - py * 31 + 4429), v2 = (uint32_t)(py * 115 + x0 * 31 + 486);
        for (cx = 0; cx < cv_w; cx++, u1 += 54u, u2 += 115u, v2 += 31u) {
            int32_t px = x0 + (int32_t)cx, v = any ? UMP_LERP(vv, xi[cx], xf[cx]) : 0, cr, b;
            if (v <= 0) {
                row[cx] = L[(px & 1) * 256];
                continue;
            }
            cr = s->c1[umb_tbil((int32_t)u1, (int32_t)v1) >> 8] + s->c2[umb_tbil((int32_t)u2, (int32_t)v2) >> 8];
            cr = cr > 255 ? 255 : cr;
            b = ((s->pw[v] * (61 + ((195 * cr) >> 8))) >> 8) + s->sm[v];
            row[cx] = L[(px & 1) * 256 + UMP_IX(b, vg)];
        }
    }
}

/* ------------------------------------------------------------------------------------------------ GRANULE --- */
static void ump_gr_seed(uint32_t g)                     /* a granule (re)born: its heart in its cell, +- 7 px */
{
    ump_gr_t *s = &umb.gr;
    s->sx[g] = (uint8_t)((g % UMG_G) * UMG_C + 3u + umb_rnd() % 15u);
    s->sy[g] = (uint8_t)((g / UMG_G) * UMG_C + 3u + umb_rnd() % 15u);
    s->amb[g] = (uint8_t)(23u + umb_rnd() % 28u);
    s->born[g] = umb_now;
}
/* the blocks bx0 .. bx1, by0 .. by1: each one's granule (the nearest heart) and profile: dark in the lanes (1 .. 4 px from
 * the next granule: the distance to their bisector, exact), brightest at the heart */
static void ump_gr_map(int32_t bx0, int32_t by0, int32_t bx1, int32_t by1)
{
    ump_gr_t *s = &umb.gr;
    int32_t bx, by, dx, dy;
    for (by = umb_clamp(by0, 0, UMB_N - 1); by <= umb_clamp(by1, 0, UMB_N - 1); by++)
        for (bx = umb_clamp(bx0, 0, UMB_N - 1); bx <= umb_clamp(bx1, 0, UMB_N - 1); bx++) {
            int32_t px = bx * 4 + 2, py = by * 4 + 2, gx = px / UMG_C, gy = py / UMG_C, f1 = 1 << 30, f2 = 1 << 30, db, t, k;
            uint32_t g1 = 0, g2 = 0, sep;
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++) {
                    int32_t X = gx + dx, Y = gy + dy, d, ex, ey;
                    uint32_t g;
                    if (X < 0 || Y < 0 || X >= (int32_t)UMG_G || Y >= (int32_t)UMG_G)
                        continue;
                    g = (uint32_t)(Y * (int32_t)UMG_G + X);
                    ex = px - s->sx[g];
                    ey = py - s->sy[g];
                    d = ex * ex + ey * ey;
                    if (d < f1) {
                        f2 = f1;
                        g2 = g1;
                        f1 = d;
                        g1 = g;
                    } else if (d < f2) {
                        f2 = d;
                        g2 = g;
                    }
                }
            dx = s->sx[g1] - s->sx[g2];
            dy = s->sy[g1] - s->sy[g2];
            sep = umb_isqrt((uint32_t)(dx * dx + dy * dy));
            db = ((f2 - f1) << 3) / (int32_t)(sep ? sep : 1u);   /* (px to the lane, Q4) */
            t = umb_clamp((db - 16) * 16 / 3, 0, 256);
            k = umb_clamp(256 - ((f1 * 376) >> 8), 0, 256);   /* (the heart's dome: 1 - (r / 13 px)^2) */
            k = (k * (179 + ((77 * (umb_tbil(px * 77 + 1408, py * 77 + 576) >> 8)) >> 8))) >> 8;   /* (its grain: 0.3 texels a px) */
            s->lab[by * UMB_N + bx] = (uint8_t)g1;
            s->prof[by * UMB_N + bx] = (uint8_t)umb_clamp((((t * t * (768 - 2 * t)) >> 16) * k) >> 8, 0, 255);
        }
}
static void ump_gr_init(void)
{
    uint32_t g;
    for (g = 0; g < UMG_N; g++) {
        ump_gr_seed(g);
        umb.gr.born[g] = umb_now - 2000u;
        umb.gr.col[g][0] = 143;
        umb.gr.col[g][1] = 217;
        umb.gr.col[g][2] = 201;
    }
    ump_gr_map(0, 0, UMB_N - 1, UMB_N - 1);
}
static void ump_gr_light(uint32_t g, int32_t e, const uint8_t *c)   /* granule g lit e (Q8) in colour c */
{
    ump_gr_t *s = &umb.gr;
    int32_t E, k, j;
    if (g >= UMG_N)
        return;
    E = s->E[g] + e;
    E = E > 410 ? 410 : E;
    s->E[g] = (uint16_t)E;
    k = E ? e * 256 / E : 256;
    for (j = 0; j < 3; j++)
        s->col[g][j] = (uint8_t)(s->col[g][j] + (((c[j] - s->col[g][j]) * k) >> 8));
}
/* the granule at row, col (0 .. 11) where track c's notes have moved it (NOTE MOVEMENT) */
static uint32_t ump_gr_at(uint32_t c, uint32_t row, uint32_t col)
{
    return (uint32_t)umn_at(c, 1u, (int32_t)row, UMG_G - 1) * UMG_G + (uint32_t)umn_at(c, 0u, (int32_t)col, UMG_G - 1);
}
static void ump_gr_note(uint32_t c, uint32_t n, int32_t v, uint32_t drum, uint32_t lane)
{
    ump_gr_t *s = &umb.gr;
    uint32_t g, o = n / 12u, nt = trk_live < 1u ? 1u : trk_live > NTRK ? NTRK : trk_live;
    const uint8_t *col = UMB_EVC[drum ? UMB_LANE_POS[lane] : (c * 3u) % 8u];
    if (!v)
        return;
    if (drum) {
        if (!lane) {                                    /* the kick: a cluster */
            g = 3u + umb_rnd() % 7u;
            g = ump_gr_at(c, g, 2u + umb_rnd() % 8u);
            ump_gr_light(g, 102 + ((205 * v) >> 8), col);
            ump_gr_light(g - 1u, (90 * v) >> 8, col);
            ump_gr_light(g + 1u, (90 * v) >> 8, col);
            ump_gr_light(g - UMG_G, (90 * v) >> 8, col);
            ump_gr_light(g + UMG_G, (90 * v) >> 8, col);
        } else if (lane < 3u)                           /* the snare, the clap: three */
            for (g = 0; g < 3u; g++)
                ump_gr_light(umb_rnd() % UMG_N, 128 + ((102 * v) >> 8), col);
        else {                                          /* the rest: a bright point in a lane (between two hearts) */
            uint32_t a = umb_rnd() % UMG_N, b = a % UMG_G < UMG_G - 1u ? a + 1u : a - 1u, i = s->nb++ % UMG_NB;
            s->bx[i] = (uint8_t)((s->sx[a] + s->sx[b]) >> 1);
            s->by[i] = (uint8_t)((s->sy[a] + s->sy[b]) >> 1);
            s->bv[i] = (uint8_t)v;
            s->bc[i] = (uint8_t)lane;
            s->bt[i] = umb_now;
        }
        return;
    }
    g = ump_gr_at(c, (c * UMG_G / nt + (o & 1u)) % UMG_G, (ump_fifth(c, n) * 5u + o * 3u) % UMG_G);
    ump_gr_light(g, 115 + ((154 * v) >> 8), col);
}
/* once a frame: the light fades (FADE: 0.3 .. 4.2 s), granules reborn (BOIL: 0.3 .. 6 a second), this frame's levels */
static void ump_gr_frame(uint32_t dt)
{
    ump_gr_t *s = &umb.gr;
    int32_t k0 = (int32_t)umv(3u, 0u), k1 = (int32_t)umv(3u, 1u), life = 300 + k1 * k1 * 2 / 5, f = (int32_t)dt * 65536 / life;
    uint32_t g, gw = umv(3u, 2u);                       /* (GLOW: the glow at rest, 0 none .. 99 twice) */
    s->tb += dt * (uint32_t)(k0 ? 77 + k0 * 1536 / 99 : 0);
    while (s->tb >= 256000u) {
        int32_t gx, gy, t;
        s->tb -= 256000u;
        g = umb_rnd() % UMG_N;
        for (t = 0; t < 6 && s->E[g] > 31u; t++)        /* (a quiet one) */
            g = umb_rnd() % UMG_N;
        ump_gr_seed(g);
        gx = (int32_t)(g % UMG_G) * UMG_C;
        gy = (int32_t)(g / UMG_G) * UMG_C;
        ump_gr_map((gx - 30) >> 2, (gy - 30) >> 2, (gx + 50) >> 2, (gy + 50) >> 2);
    }
    for (g = 0; g < UMG_N; g++) {
        uint32_t age = umb_now - s->born[g];
        s->E[g] = (uint16_t)(s->E[g] - ((s->E[g] * f) >> 16));
        s->gb[g] = (uint8_t)(s->amb[g] * (age >= 1600u ? 8u : age / 200u) * gw / 400u);   /* (born: it fades in, 8 steps) */
        s->gl[g] = (uint16_t)(s->E[g] & ~15u);
    }
}
static void ump_gr_draw(int32_t x0, int32_t y0)
{
    ump_gr_t *s = &umb.gr;
    uint8_t xi[240], xf[240];
    int32_t vv[UMB_N + 1], i;
    uint32_t cx, cy;
    ump_cols(x0, xi, xf);
    for (cy = 0; cy < cv_h; cy++) {
        int32_t py = y0 + (int32_t)cy, vg = ump_vg(py), dq = (py & 1) << 1;
        const uint8_t *lab = s->lab + (py >> 2) * UMB_N;
        const uint16_t *L = ump_lut[dq];
        uint16_t *row = cv_px + cy * cv_w;
        ump_vrow(s->prof, py, vv);
        uint32_t qg = UMG_N;                            /* (the pair's even pixel: lit, granule qg) */
        for (cx = 0; cx < cv_w; cx++) {
            int32_t px = x0 + (int32_t)cx, p = UMP_LERP(vv, xi[cx], xf[cx]), e, R, G, B;
            uint32_t g = lab[px >> 2];
            if (p < 3 || !s->gl[g]) {                   /* (dark, or a granule at rest: one read) */
                row[cx] = L[(px & 1) * 256 + (p < 3 ? 0 : UMP_IX((p * s->gb[g]) >> 8, vg))];
                qg = UMG_N;
                continue;
            }
            if ((px & 1) && g == qg) {                  /* (lit: worked out a pixel pair at a time) */
                row[cx] = row[cx - 1u];
                continue;
            }
            qg = g;
            e = ((((p * p) >> 8) * s->gl[g]) * 410) >> 16;   /* (lit: a dome, brightest at the heart) */
            R = (s->col[g][0] * e) >> 8;
            G = (s->col[g][1] * e) >> 8;
            B = (s->col[g][2] * e) >> 8;
            if (e > 115) {
                R += (UMP_WT[0] * (e - 115) * 230) >> 16;
                G += (UMP_WT[1] * (e - 115) * 230) >> 16;
                B += (UMP_WT[2] * (e - 115) * 230) >> 16;
            }
            row[cx] = ump_rgb((p * s->gb[g]) >> 8, R, G, B, vg, UMP_D2[dq | (px & 1)]);
        }
    }
    for (i = 0; i < (int32_t)UMG_NB; i++) {             /* the bright points: 0.4 s, 8 steps */
        int32_t f = ump_fade(umb_now - s->bt[i], 400u, 8u);
        if (f && s->bv[i])
            ump_glint(s->bx[i] * 256, s->by[i] * 256, (f * (128 + (s->bv[i] >> 1)) * 384) >> 16,
                      s->bc[i] == 4u ? UMP_WT : UMB_EVC[UMB_LANE_POS[s->bc[i] % NLANE]]);
    }
}

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
static void ump_co_note(uint32_t c, uint32_t n, int32_t v, uint32_t drum, uint32_t lane)
{
    ump_co_t *s = &umb.co;
    int32_t a0, sg, e, d;
    if (!v)
        return;
    if (drum) {
        if (!lane)                                      /* the kick: the chromosphere flares */
            s->P = (uint16_t)(s->P > 128 + (v >> 1) ? s->P : 128 + (v >> 1));
        else if (lane < 3u) {                           /* the snare, the clap: a prominence */
            uint32_t i = s->np++ % 4u;
            s->pa_[i] = (uint16_t)(umb_rnd() & 1023u);
            s->ph[i] = (uint8_t)(6 + 7 * v / 256);
            s->pt[i] = umb_now;
        } else {                                        /* the rest: a bead */
            uint32_t i = s->nbd++ % 10u;
            s->ba[i] = (uint16_t)(umb_rnd() & 1023u);
            s->bv[i] = (uint8_t)v;
            s->bt[i] = umb_now;
        }
        return;
    }
    a0 = (int32_t)ump_fifth(c, n) * 128 / 12 + 96 + (umn.o[c][0] >> 1);   /* (the root at the top; NOTE MOVEMENT round) */
    sg = 16 + (((256 - ump_pk(n)) * 40) >> 8);          /* (its width: 1 .. 3.5 streamers, Q4) */
    e = 90 + ((141 * v) >> 8);
    for (d = -(3 * sg >> 4) - 1; d <= (3 * sg >> 4) + 1; d++) {
        uint32_t j = (uint32_t)(a0 + d) & (UMR_NA - 1u);
        int32_t a = (e * umb_exp(4096 * d * d / (sg * sg))) >> 8;
        s->E[j] = (uint16_t)(s->E[j] + ((a * (410 - s->E[j])) / 410));   /* (it saturates: a dense song, not a white sky) */
    }
}
static void ump_co_frame(uint32_t dt)
{
    ump_co_t *s = &umb.co;
    int32_t k0 = (int32_t)umv(4u, 0u), k1 = (int32_t)umv(4u, 1u), life = 400 + k1 * k1 * 3 / 5, f = (int32_t)dt * 65536 / life,
            reach = 640 * k0 / 99, a;
    umr_rm = umb_clamp(44 * umv_sc(4u) >> 8, 16, 88);   /* (SIZE) */
    umr_k = 8192 / umr_rm;
    s->P = (uint16_t)(s->P - ((s->P * (int32_t)dt * 218) >> 16));   /* (the pulse: 0.3 s) */
    for (a = 0; a < (int32_t)UMR_NA; a++) {
        int32_t S;
        s->E[a] = (uint16_t)(s->E[a] - ((s->E[a] * f) >> 16));
        S = ((s->base[a] * 154) >> 8) + (s->E[a] & ~15);
        s->S[a] = (uint16_t)S;
        s->K[a] = (int16_t)(947 - ((reach * (S > 256 ? 256 : S)) >> 8));   /* (3.7 .. 1.2: REACH, a bright one further) */
    }
    s->S[UMR_NA] = s->S[0];
    s->K[UMR_NA] = s->K[0];
}
/* a row of 2 x 2 px blocks (block row qy's, py even .. odd): each one's streamer light, from x0 (into bv) */
static void ump_co_blocks(int32_t x0, int32_t py, int16_t *bv)
{
    ump_co_t *s = &umb.co;
    int32_t dy = (py & ~1) - 120, qy = dy >= 0 ? dy >> 1 : (-dy - 1) >> 1, lnm = umb_ln[UMR_RM - 16], b, n;
    const uint8_t *pa = s->pa + qy * UMB_N, *pr = s->pr + qy * UMB_N;
    for (b = x0 >> 1, n = 0; b <= (x0 + (int32_t)cv_w - 1) >> 1; b++, n++) {
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
    }
}
static void ump_co_draw(int32_t x0, int32_t y0)
{
    ump_co_t *s = &umb.co;
    int32_t Pq = s->P & ~31, i, u, ch = 128 + ((154 * Pq) >> 8), ic = (64 * Pq) >> 8;
    int16_t bv[122];
    uint32_t cx, cy;
    for (cy = 0; cy < cv_h; cy++) {
        int32_t py = y0 + (int32_t)cy, vg = ump_vg(py), dy = py - 120, fdy2 = (2 * dy + 1) * (2 * dy + 1) - 4 * UMR_RM * UMR_RM;
        const uint16_t *L = ump_lut[(py & 1) << 1];
        uint16_t *row = cv_px + cy * cv_w;
        if (!cy || !(py & 1))
            ump_co_blocks(x0, py, bv);
        for (cx = 0; cx < cv_w; cx++) {
            int32_t px = x0 + (int32_t)cx, dx = 2 * (px - 120) + 1, v;
            u = ((dx * dx + fdy2) * umr_k) >> 8;        /* (px past the limb, Q8) */
            if (u < -128) {                             /* the disc: earthshine */
                row[cx] = L[(px & 1) * 256 + UMP_IX(8, vg)];
                continue;
            }
            v = bv[(px >> 1) - (x0 >> 1)];
            if (u < 14 * 256) {                         /* the chromosphere, and the inner corona the kick lights */
                int32_t uu = u < 0 ? 0 : u;
                v += (umb_exp((uu * 71) >> 10) * ch) >> 8;
                v += (ic * (256 - ((uu * 18) >> 8))) >> 8;
                if (u < 128)
                    v = (v * (u + 128)) >> 8;
            }
            row[cx] = L[(px & 1) * 256 + UMP_IX(v > 255 ? 255 : v, vg)];
        }
    }
    for (i = 0; i < 4; i++) {                           /* the prominences: an arch on the limb, 1.4 s, 8 steps */
        uint32_t age = umb_now - s->pt[i];
        int32_t fq = s->ph[i] ? ump_fade(age, 1400u, 8u) : 0, rise = age >= 250u ? 256 : (int32_t)(age * 256u / 250u),
                dl = s->ph[i] * 10 / 3;
        if (!fq)
            continue;
        for (u = 0; u <= 50; u++) {
            uint32_t an = (uint32_t)(s->pa_[i] + dl * (2 * u - 50) / 50) << 22;
            int32_t ro = UMR_RM * 256 + 128 + ((s->ph[i] * rise * (sine_i((uint32_t)u * 42949673u) >> 7)) >> 8),
                    co = sine_i(an + 0x40000000u), si = sine_i(an), X = 120 * 256 + ((ro * co) >> 15), Y = 120 * 256 + ((ro * si) >> 15);
            umm_splat(X, Y, (fq * 128) >> 8, UMB_EVC[6]);
            umm_splat(X - (co >> 7), Y - (si >> 7), (fq * 77) >> 8, UMB_EVC[6]);
        }
    }
    for (i = 0; i < 10; i++) {                          /* the beads: a glint on the limb, 0.3 s */
        int32_t fq = s->bv[i] ? ump_fade(umb_now - s->bt[i], 300u, 8u) : 0, ro = UMR_RM * 256 + 154;
        uint32_t an = (uint32_t)s->ba[i] << 22;
        if (fq)
            ump_glint(120 * 256 + ((ro * sine_i(an + 0x40000000u)) >> 15), 120 * 256 + ((ro * sine_i(an)) >> 15),
                      (fq * (128 + (s->bv[i] >> 1)) * 410) >> 16, UMP_WT);
    }
}

/* --------------------------------------------------------------------------------------------------- TARN --- */
#define UMT_RX 120                                      /* the reflection's centre (px) and its moon's radius */
#define UMT_RY 126
#define UMT_RR 30
static void ump_ta_init(void)
{
    ump_ta_t *s = &umb.ta;
    int32_t q;
    for (q = 0; q < 1024; q++) {                        /* the eclipse by r^2: the disc, the corona (r^-2.6), the limb's ring */
        int32_t r16 = (int32_t)umb_isqrt((uint32_t)(q * 32 + 16) << 8), r = r16 >> 4, v;
        if (r16 < UMT_RR * 16 - 8) {
            s->rl[q] = 6;
            continue;
        }
        v = (179 * umb_exp(((umb_ln[(r > 175 ? 175 : r < 16 ? 16 : r) - 16] - umb_ln[UMT_RR - 16]) * 83) >> 9) +
             230 * umb_exp(((r16 - UMT_RR * 16) * 20) >> 4)) >> 8;
        v = (v * umb_clamp((r16 - UMT_RR * 16 + 8) * 16, 0, 256)) >> 8;
        s->rl[q] = (uint8_t)umb_clamp(((v - 15) * 272) >> 8, 0, 255);   /* (its faint tail taken off: bent, unseen changes) */
    }
    s->grj[0] = s->grj[1] = -99;
}
/* a drop at cx, cy (cells, Q8), r (cells, Q8), amp deep */
static void ump_ta_drop(int32_t cx, int32_t cy, int32_t r, int32_t amp)
{
    int16_t *h = umb.ta.h[umb.ta.cur];
    int32_t x, y, r2 = (r * r) >> 8;
    for (y = umb_clamp((cy - r) >> 8, 1, UMT_N - 2); y <= umb_clamp((cy + r + 255) >> 8, 1, UMT_N - 2); y++)
        for (x = umb_clamp((cx - r) >> 8, 1, UMT_N - 2); x <= umb_clamp((cx + r + 255) >> 8, 1, UMT_N - 2); x++) {
            int32_t dx = x * 256 - cx, dy = y * 256 - cy, d2 = (dx * dx + dy * dy) >> 8;
            if (d2 <= r2)
                h[y * UMT_N + x] = (int16_t)umb_clamp(h[y * UMT_N + x] - amp * (r2 - d2) / (r2 + 1), -32000, 32000);
        }
}
/* a drop where track c's notes have moved it (NOTE MOVEMENT: the water 3 .. 45 cells either way) */
static void ump_ta_at(uint32_t c, int32_t x, int32_t y, int32_t r, int32_t amp)
{
    ump_ta_drop(768 + umn_at(c, 0u, x - 768, 10752), 768 + umn_at(c, 1u, y - 768, 10752), r, amp);
}
static void ump_ta_note(uint32_t c, uint32_t n, int32_t v, uint32_t drum, uint32_t lane)
{
    ump_ta_t *s = &umb.ta;
    int32_t k;
    if (!v)
        return;
    if (drum) {
        if (!lane)                                      /* the kick: a heavy drop by the moon */
            ump_ta_at(c, (int32_t)(20u * 256u + umb_rnd() % 2048u), (int32_t)(21u * 256u + umb_rnd() % 2048u), 666, (2600 * v) >> 8);
        else if (lane < 3u)                             /* the snare, the clap: one */
            ump_ta_at(c, (int32_t)(4u * 256u + umb_rnd() % 10240u), (int32_t)(4u * 256u + umb_rnd() % 10240u), 461, (1700 * v) >> 8);
        else {                                          /* the rest: a star's glint */
            uint32_t i = s->nt++ % 8u;
            s->tx[i] = (uint8_t)(umb_rnd() % 240u);
            s->ty[i] = (uint8_t)(umb_rnd() % 240u);
            s->tv[i] = (uint8_t)v;
            s->tt[i] = umb_now;
        }
        return;
    }
    k = ump_pk(n);
    ump_ta_at(c, 768 + k * 42, (42 - 5 * (int32_t)c) * 256, 307 + (((256 - k) * 410) >> 8), 700 + ((1500 * v) >> 8));
}
/* a step: the wave (h' = (the four round) / 2 - h before), dragged (DRAG), stilled below a dead band */
static void ump_ta_step(void)
{
    ump_ta_t *s = &umb.ta;
    const int16_t *c = s->h[s->cur];
    int16_t *p = s->h[s->cur ^ 1u];
    int32_t k0 = (int32_t)umv(5u, 0u), dk = 82 + k0 * k0 * 600 / 9801, x, y;
    for (y = 0; y < UMT_N; y++) {
        const int16_t *r0 = c + y * UMT_N, *ru = c + (y ? y - 1 : 1) * UMT_N, *rd = c + (y < UMT_N - 1 ? y + 1 : UMT_N - 2) * UMT_N;
        for (x = 0; x < UMT_N; x++) {
            int32_t nv = ((r0[x ? x - 1 : 1] + r0[x < UMT_N - 1 ? x + 1 : UMT_N - 2] + ru[x] + rd[x]) >> 1) - p[y * UMT_N + x];
            nv -= nv * dk / 4096;
            p[y * UMT_N + x] = (int16_t)(nv > -12 && nv < 12 ? 0 : umb_clamp(nv, -32000, 32000));
        }
    }
    s->cur ^= 1u;
    s->grj[0] = s->grj[1] = -99;
}
static const int16_t *ump_ta_grow(int32_t j)            /* row j's slopes (x, y), two rows kept (by parity) */
{
    ump_ta_t *s = &umb.ta;
    const int16_t *c = s->h[s->cur], *r0 = c + j * UMT_N, *ru = c + (j ? j - 1 : j) * UMT_N, *rd = c + (j < UMT_N - 1 ? j + 1 : j) * UMT_N;
    int16_t *g = s->gr[j & 1];
    int32_t x;
    if (s->grj[j & 1] != j) {
        s->grj[j & 1] = (int16_t)j;
        for (x = 0; x < UMT_N; x++) {
            g[2 * x] = (int16_t)umb_clamp(r0[x < UMT_N - 1 ? x + 1 : x] - r0[x ? x - 1 : x], -32000, 32000);
            g[2 * x + 1] = (int16_t)umb_clamp(rd[x] - ru[x], -32000, 32000);
        }
    }
    return g;
}
static void ump_ta_draw(int32_t x0, int32_t y0)
{
    ump_ta_t *s = &umb.ta;
    uint8_t xi[240], xf[240], still[UMT_N];
    int32_t m = ((102 + 819 * (int32_t)umv(5u, 1u) / 99) * 164) >> 8, i, sj = -1;   /* (BEND: px a slope unit, Q16) */
    int32_t q = (1 << 24) / (umv_sc(5u) * umv_sc(5u));  /* (SIZE: the reflection read nearer or further, Q8: 256 as it was) */
    uint32_t cx, cy;
    for (cx = 0; cx < cv_w; cx++) {
        int32_t t = umb_clamp(((x0 + (int32_t)cx - 2) * 256) / 5, 0, (UMT_N - 1) * 256 - 1);
        xi[cx] = (uint8_t)(t >> 8);
        xf[cx] = (uint8_t)(t & 255);
    }
    for (cy = 0; cy < cv_h; cy++) {
        int32_t py = y0 + (int32_t)cy, vg = ump_vg(py), t = umb_clamp(((py - 2) * 256) / 5, 0, (UMT_N - 1) * 256 - 1), fy = t & 255,
                b = py - UMT_RY, b2 = b * b, k;
        const int16_t *g0 = ump_ta_grow(t >> 8), *g1 = ump_ta_grow((t >> 8) + 1);
        const uint16_t *L = ump_lut[(py & 1) << 1];
        uint16_t *row = cv_px + cy * cv_w;
        uint32_t hx = (uint32_t)x0 * 73856093u, hy = (uint32_t)py * 19349663u;
        int32_t ox = 0, oy = 0, hl = -1;                /* (the pair's offset and sheen; -1: none yet) */
        if (sj != t >> 8) {                             /* (which cells' corners are still: nothing bent there) */
            sj = t >> 8;
            for (k = 0; k < UMT_N - 1; k++)
                still[k] = !(g0[2 * k] | g0[2 * k + 1] | g0[2 * k + 2] | g0[2 * k + 3] | g1[2 * k] | g1[2 * k + 1] | g1[2 * k + 2] |
                             g1[2 * k + 3]);
        }
        for (cx = 0; cx < cv_w; cx++, hx += 73856093u) {
            int32_t px = x0 + (int32_t)cx, a, c, X, Y, v, fx;
            uint32_t hh;
            k = 2 * xi[cx];
            if (still[xi[cx]]) {                        /* (still water: the reflection as it is) */
                hl = -1;
                a = px - UMT_RX;
                v = s->rl[umb_clamp((a * a + b2) * q >> 13, 0, 1023)];
                hh = hx ^ hy;
                if (!((hh >> 20) & 1023u))
                    v += ((int32_t)((hh >> 8) & 127u) + 40) * 154 >> 8;
                row[cx] = L[(px & 1) * 256 + UMP_IX(v > 255 ? 255 : v, vg)];
                continue;
            }
            if (!(px & 1) || hl < 0) {                  /* (the slope a pixel pair at a time) */
                int32_t gx, gy;
                fx = xf[cx];
                a = g0[k] + (((g0[k + 2] - g0[k]) * fx) >> 8);
                c = g1[k] + (((g1[k + 2] - g1[k]) * fx) >> 8);
                gx = a + (((c - a) * fy) >> 8);
                a = g0[k + 1] + (((g0[k + 3] - g0[k + 1]) * fx) >> 8);
                c = g1[k + 1] + (((g1[k + 3] - g1[k + 1]) * fx) >> 8);
                gy = a + (((c - a) * fy) >> 8);
                ox = (gx * m + 32768) >> 16;            /* (whole pixels: still water, the same pixels) */
                oy = (gy * m + 32768) >> 16;
                hl = umb_clamp((((-gx * 154 - gy * 205) * 36) >> 16) - 20, 0, 256) & ~31;   /* (the sheen: 8 steps) */
                hl = (hl * 154) >> 8;
            }
            X = px + ox;
            Y = py + oy;
            a = X - UMT_RX;
            c = Y - UMT_RY;
            v = s->rl[umb_clamp((a * a + c * c) * q >> 13, 0, 1023)];
            if ((uint32_t)X < 240u && (uint32_t)Y < 240u) {   /* (the stars: one pixel in 1024) */
                hh = (uint32_t)X * 73856093u ^ (uint32_t)Y * 19349663u;
                if (!((hh >> 20) & 1023u))
                    v += ((int32_t)((hh >> 8) & 127u) + 40) * 154 >> 8;
            }
            row[cx] = hl ? ump_rgb(v > 255 ? 255 : v, (UMP_SH[0] * hl) >> 8, (UMP_SH[1] * hl) >> 8, (UMP_SH[2] * hl) >> 8, vg,
                                   UMP_D2[((py & 1) << 1) | (px & 1)])
                         : L[(px & 1) * 256 + UMP_IX(v > 255 ? 255 : v, vg)];
        }
    }
    for (i = 0; i < 8; i++) {                           /* the stars glinting: 0.35 s, 6 steps */
        int32_t f = s->tv[i] ? ump_fade(umb_now - s->tt[i], 350u, 6u) : 0;
        if (f)
            ump_glint(s->tx[i] * 256, s->ty[i] * 256, (f * (128 + (s->tv[i] >> 1)) * 230) >> 16, UMP_WT);
    }
}

/* ---------------------------------------------------------------------------------------------------------- */
/* once a frame: a plate just chosen starts afresh; the notes since (a burst: the newest); the plate's frame and steps
 * (CINDER 20 a second, TARN 24; never more than four, never a catch-up burst) */
static void ump_tick(uint32_t pl)
{
    uint32_t dt, n = 0, step = pl == 5u ? 42u : 50u;
    ump_lq = umv_light(pl);
    if (!ump_lut[3][255])                               /* (the palette as the panel takes it: once) */
        ump_lut_make();
    if (ump_on != pl) {
        memset(&umb, 0, __builtin_offsetof(umb_t, f0));
        ump_on = (uint8_t)pl;
        umb.rs = 218u + umb_now;
        umb.qr = umb_q.w;
        umb.ms = umb.last = umb_now;
        if (!pl)
            ump_ci_init();
        else if (pl == 3u)
            ump_gr_init();
        else if (pl == 4u)
            ump_co_init();
        else
            ump_ta_init();
    }
    if (umb.f0 == ui.frame)
        return;
    umb.f0 = ui.frame;
    dt = umb_now - umb.last;
    dt = dt > 250u ? 250u : dt;
    umb.last = umb_now;
    if (umb_q.w - umb.qr > UMB_NQ)
        umb.qr = umb_q.w - UMB_NQ;
    for (; umb.qr != umb_q.w; umb.qr++) {
        uint32_t q = umb.qr % UMB_NQ, c = umb_q.c[q], nn = umb_q.note[q], drum, lane, u = umb_vel(umb_q.vel[q]);
        int32_t v = u ? (int32_t)u * 2 + 1 : 0;         /* (REACT: 0, a note that does not reach it) */
        if (c >= NTRK || (umb_q.vel[q] && (!v || eco_off(c))))
            continue;
        drum = trk[c].engine == ENGI_DRUM;
        lane = drum ? drum_lane(nn) % NLANE : 0u;
        if (v)
            umn_note(c);                                /* (NOTE MOVEMENT) */
        if (!pl)
            ump_ci_note(c, nn, v, drum, lane);
        else if (pl == 3u)
            ump_gr_note(c, nn, v, drum, lane);
        else if (pl == 4u)
            ump_co_note(c, nn, v, drum, lane);
        else
            ump_ta_note(c, nn, v, drum, lane);
    }
    if (pl == 3u)
        ump_gr_frame(dt);
    else if (pl == 4u)
        ump_co_frame(dt);
    else {
        while ((uint32_t)(umb_now - umb.ms) >= step && n++ < 4u) {
            umb.ms += step;
            if (pl)
                ump_ta_step();
            else
                ump_ci_step();
        }
        if ((uint32_t)(umb_now - umb.ms) >= step)
            umb.ms = umb_now;
    }
}
static __attribute__((noinline)) void ump_backdrop(uint32_t pl, int32_t x0, int32_t y0)
{
    ump_tick(pl);
    umm_x0 = x0;                                        /* (the glints: the melody plates' splat) */
    umm_y0 = y0;
    umm_lq = ump_lq;
    if (!pl)
        ump_ci_draw(x0, y0);
    else if (pl == 3u)
        ump_gr_draw(x0, y0);
    else if (pl == 4u)
        ump_co_draw(x0, y0);
    else
        ump_ta_draw(x0, y0);
}
