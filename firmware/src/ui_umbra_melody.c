/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* (included by ui_umbra.c, before the field plates) built for size, as the main loop is (tools/size_fns.py).
 * ---------------------------------------------------------------------------------- the plates' light, added ---
 * What CORONA (resident: the welcome uses it) adds to its strip, a pixel or a point at a time; the strip being drawn, the
 * rows' darkening (under the knob row and the footer as on every plate; TOTALITY: the whole screen, undimmed). SAROS and
 * FILAMENT, the melody plates once drawn here, are plate modules since 2026-11-09 (plates/saros.c, plates/filament.c:
 * their own copies of these, plates/plate_lib.h) */
static int32_t umm_x0, umm_y0;                          /* the strip being drawn: its place on the screen */
static int32_t umm_lq;                                  /* LIGHT (Q8), worked out once a frame */
/* a row's darkening (under the knob row and the footer; TOTALITY none), LIGHT in it (all the plates') */
static int32_t umb_rowlight(int32_t py, int32_t lq)   /* .. of LIGHT lq (Q8): the plate modules' rows too (umb_lay_draw) */
{
    return ((umb.bare ? 256 : py < 56 ? 108 : py < 76 ? 108 + (py - 56) * 7 : py >= 226 ? 128 : 256) * lq) >> 8;
}
static int32_t umm_vg(int32_t py) { return umb_rowlight(py, umm_lq); }
/* r g b (0..255, or past it: held there) dithered by d (-8..7), as the panel takes it */
static uint16_t ump_pack(int32_t R, int32_t G, int32_t B, int32_t d)
{
    R += d >> 1;
    G += d >> 2;
    B += d >> 1;
    R = R < 0 ? 0 : R > 255 ? 255 : R;
    G = G < 0 ? 0 : G > 255 ? 255 : G;
    B = B < 0 ? 0 : B > 255 ? 255 : B;
    return swap16((uint32_t)(((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3)));
}
/* the panel's pixel q (its colour read back, half a step up) and the light r g b (0..255) x w / 256 added, dithered by d */
static __attribute__((noinline)) uint16_t umm_add(uint32_t q, int32_t r, int32_t g, int32_t b, int32_t w, int32_t d)
{
    q = swap16(q);
    return ump_pack((int32_t)((q >> 8) & 0xF8u) + 4 + ((r * w) >> 8), (int32_t)((q >> 3) & 0xFCu) + 2 + ((g * w) >> 8),
                    (int32_t)((q << 3) & 0xF8u) + 4 + ((b * w) >> 8), d);
}
/* add the light r g b (0..255) x w / 256 to the screen's pixel x, y if the strip has it */
static void umm_px(int32_t x, int32_t y, int32_t w, int32_t r, int32_t g, int32_t b)
{
    uint16_t *p;
    if (w <= 0 || (uint32_t)(x - umm_x0) >= cv_w || (uint32_t)(y - umm_y0) >= cv_h)
        return;
    w = (w * umm_vg(y)) >> 8;                           /* (LIGHT; the dimming, TOTALITY: none) */
    p = cv_px + (uint32_t)(y - umm_y0) * cv_w + (uint32_t)(x - umm_x0);
    *p = umm_add(*p, r, g, b, w, 0);
}
/* a point at X, Y (px, Q8), a (Q8) of colour c, shared among the four pixels round it */
static void umm_splat(int32_t X, int32_t Y, int32_t a, const uint8_t *c)
{
    int32_t x = X >> 8, y = Y >> 8, fx = X & 255, fy = Y & 255, ax = (a * (256 - fx)) >> 8, bx = (a * fx) >> 8;
    if (y + 1 < umm_y0 || y >= umm_y0 + (int32_t)cv_h)
        return;
    umm_px(x, y, (ax * (256 - fy)) >> 8, c[0], c[1], c[2]);
    umm_px(x + 1, y, (bx * (256 - fy)) >> 8, c[0], c[1], c[2]);
    umm_px(x, y + 1, (ax * fy) >> 8, c[0], c[1], c[2]);
    umm_px(x + 1, y + 1, (bx * fy) >> 8, c[0], c[1], c[2]);
}
