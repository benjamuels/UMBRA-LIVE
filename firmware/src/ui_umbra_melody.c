/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* (included by ui_umbra.c, before umb_backdrop) built for size, as the main loop is (tools/size_fns.py): the FM-1's slot is
 * nearly full, and these plates are light; umm_backdrop stays a call so umb_backdrop's -Os does not take it in */
/* ------------------------------------------------------------------------------- SAROS and FILAMENT: the melody --- */
/* Plates of the melody (HOME > ECO > PLATE), from the same ring of notes as the others (voice.c umb_q; a note let go:
 * velocity 0), their state in the plates' shared block (umb.m). Still between notes, so HOME's changed-cells
 * blit (gfx.c cv_blit_cells) sends little to the panel: fades go in 16 steps and SAROS turns a pixel at a time at its
 * rim. Each strip is filled black (JIANT's), then the light added to its pixels (RGB565), dimmed under the knob row and the
 * footer as the field plates are (TOTALITY: the whole screen, undimmed).
 * SAROS: the melody as string art on the circle of fifths, the whole plate turning once in 100 s round a dark umbra. A
 * note lands on its pitch class (its track's root at the marked tick), its octave the ring; each melodic track's notes
 * are joined in turn by a thread in its colour. Fifths and fourths stay near the rim, thirds cut across, tritones pass
 * through the umbra and vanish; a phrase played again retraces its threads and burns them brighter (a thread fades in
 * ~6 s).
 * FILAMENT (since 2026-11-09 a plate module, plates/filament.c, through the plate API; told here as it was): a taut
 * string a track (4 or 6, as TRACKS). A note plucks its track's string where its pitch falls (C2 at the
 * left, C7 at the right; a drum where its lane is), its velocity the height and the melody's contour the side (rising
 * up, falling down); a higher note tightens the string so its waves run faster; a held note keeps bowing it; let go,
 * the string is damped and still within ~0.6 s. A 1D wave, 81 points a string, 125 substeps a second */
#define UMM_CX 120                                      /* SAROS's centre (px) */
#define UMM_CY 120
static const uint8_t UMM_WT[3] = {236, 250, 246}, UMM_TEAL[3] = {143, 217, 201}, UMM_REST[3] = {34, 60, 96};
static uint8_t umm_on;                                  /* the melody plate drawn last (0: none: it starts afresh) */
static int32_t umm_x0, umm_y0;                          /* the strip being drawn: its place on the screen */
static int32_t umm_lq, umm_fk, umm_ru;        /* HOME > ECO > PLATE, worked out once a frame (umm_tick): LIGHT
                                                         * (Q8), SAROS's FADE (its rate), SAROS's umbra's radius (SIZE: 26 px
                                                         * at 50, px Q8) */
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
/* SAROS: note n of track c on the dial (px, Q8): its place on the circle of fifths from the track's root, its octave
 * the ring (40 .. 107.5 px); NOTE MOVEMENT turns the track's whole web round the dial (a place a step) and moves it out
 * or in (a ring a step) */
static void umm_pos(uint32_t c, uint32_t n, int32_t *X, int32_t *Y)
{
    uint32_t k = ((n + 120u - (uint32_t)trk[c].p[P_ROOT] % 12u) % 12u * 7u + (uint32_t)(24 + ((umn.o[c][0] * 12) >> 8))) % 12u;
    int32_t r = umm_ru + 3584 + umn_at(c, 1u, umb_clamp((int32_t)n / 12 - 3, 0, 5), 5) * (23936 - umm_ru) / 5;   /* (the rings
                                                         * from 14 px out of the umbra to 107.5) */
    *X = UMM_CX * 256 + ((r * umb.m.ux[k]) >> 15);
    *Y = UMM_CY * 256 + ((r * umb.m.uy[k]) >> 15);
}
static int32_t umm_glow(uint32_t i)                    /* a thread's energy now (Q8): it fades in ~6 s (FADE 45; 0 .. 99:
                                                         * 0.1 .. 13 s) */
{
    uint32_t age = umb_now - umb.m.tt[i];
    return (umb.m.te[i] * umb_exp((int32_t)((age < 1048576u ? age : 1048576u) * (uint32_t)umm_fk >> 11))) >> 8;
}
/* a note played (v: velocity, Q8): its point, and the thread from the track's note before (one already there burns) */
static void umm_saros_on(uint32_t c, uint32_t n, int32_t v)
{
    umm_t *m = &umb.m;
    uint32_t p = m->last[c], i, k, a, b;
    int32_t lo = 1 << 30;
    m->nn[m->nnd] = (uint8_t)n;
    m->nc[m->nnd] = (uint8_t)c;
    m->nv[m->nnd] = (uint8_t)v;
    m->nt[m->nnd] = umb_now;
    m->nnd = (uint8_t)((m->nnd + 1u) % UMM_ND);
    m->last[c] = (uint8_t)(n + 1u);
    if (!p-- || p == n)
        return;
    a = p < n ? p : n;
    b = p < n ? n : p;
    for (i = 0; i < m->nth && (m->ta[i] != a || m->tb[i] != b || m->tc[i] != c); i++)
        ;
    if (i == m->nth) {                                  /* a new thread (all in use: in place of the faintest) */
        if (m->nth < UMM_TH)
            m->nth++;
        else
            for (k = 0; k < UMM_TH; k++)
                if (umm_glow(k) < lo) {
                    lo = umm_glow(k);
                    i = k;
                }
        m->ta[i] = (uint8_t)a;
        m->tb[i] = (uint8_t)b;
        m->tc[i] = (uint8_t)c;
        m->te[i] = 0;
    }
    m->te[i] = (uint16_t)umb_clamp(umm_glow(i) + v, 0, 640);
    m->tt[i] = umb_now;
    m->tf[i] = (uint8_t)p;
}
/* once a frame: the turn, the circle's places, the threads as drawn (a faded one goes) */
static void umm_saros_frame(void)
{
    umm_t *m = &umb.m;
    uint32_t i, j, k, th;
    m->th += (umb_now - m->tms) * (42950u * umv(1u, 0u) / 30u);   /* (SPIN 30: a turn in 100 s; 0: still; 99: in 30 s) */
    m->tms = umb_now;
    th = m->th - m->th % 5892960u;                     /* (a step at a time: a pixel at the rim, 116 px) */
    for (i = 0; i < 12u; i++) {
        uint32_t ph = i * 357913941u - 0x40000000u + th;   /* (the root at the top, turned) */
        m->ux[i] = (int16_t)sine_i(ph + 0x40000000u);
        m->uy[i] = (int16_t)sine_i(ph);
    }
    for (i = j = 0; i < m->nth; i++) {
        int32_t e = (umb_clamp(umm_glow(i), 0, 512) + 8) & ~15, X, Y, n, dx, dy;   /* (16 steps a unit) */
        const uint8_t *tc = UMB_EVC[(m->tc[i] * 3u) % 8u];
        if (!e)
            continue;
        m->ta[j] = m->ta[i];
        m->tb[j] = m->tb[i];
        m->tc[j] = m->tc[i];
        m->tf[j] = m->tf[i];
        m->te[j] = m->te[i];
        m->tt[j] = m->tt[i];
        umm_pos(m->tc[j], m->ta[j], &m->lx[j], &m->ly[j]);
        umm_pos(m->tc[j], m->tb[j], &X, &Y);
        dx = X - m->lx[j];
        dy = Y - m->ly[j];
        n = ((dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy)) / 256 + 1;
        m->ln[j] = (int16_t)n;
        m->lsx[j] = dx / n;
        m->lsy[j] = dy / n;
        for (k = 0; k < 3u; k++)
            m->lc[j][k] = (uint8_t)umb_clamp((tc[k] * e) >> 8, 0, 255);   /* (its colour once played, brighter burned) */
        j++;
    }
    m->nth = (uint8_t)j;
}
static int umm_umbra(int32_t X, int32_t Y)             /* behind the umbra (26 px at SIZE 50) */
{
    int32_t dx = (X >> 4) - UMM_CX * 16, dy = (Y >> 4) - UMM_CY * 16;
    return dx * dx + dy * dy < (umm_ru >> 4) * (umm_ru >> 4);
}
static void umm_saros_draw(void)
{
    umm_t *m = &umb.m;
    int32_t i, k, ylo = (umm_y0 - 2) * 256, yhi = (umm_y0 + (int32_t)cv_h + 1) * 256;
    if (umm_y0 < UMM_CY + (umm_ru >> 8) + 6 && umm_y0 + (int32_t)cv_h > UMM_CY - (umm_ru >> 8) - 6)
        for (i = 0; i < 180; i++) {                     /* the umbra's corona (as it was: a turn leaves it so) */
            int32_t s = sine_i((uint32_t)i * 23860929u), c = sine_i((uint32_t)i * 23860929u + 0x40000000u), r = umm_ru * 55 / 52;
            umm_splat(UMM_CX * 256 + ((c * r) >> 15), UMM_CY * 256 + ((s * r) >> 15), 51, UMM_TEAL);
            r = umm_ru * 60 / 52;
            umm_splat(UMM_CX * 256 + ((c * r) >> 15), UMM_CY * 256 + ((s * r) >> 15), 13, UMM_TEAL);
        }
    for (i = 0; i < 12; i++)                            /* the dial: the root's tick teal */
        umm_splat(UMM_CX * 256 + ((29696 * m->ux[i]) >> 15), UMM_CY * 256 + ((29696 * m->uy[i]) >> 15), i ? 179 : 128,
                  i ? UMM_REST : UMM_TEAL);
    for (i = 0; i < m->nth; i++) {                      /* the threads, a sample a pixel (only the strip's) */
        int32_t X = m->lx[i], Y = m->ly[i], sx = m->lsx[i], sy = m->lsy[i], n = m->ln[i], k0 = 0, k1 = n;
        uint32_t age = umb_now - m->tt[i];
        if (sy) {
            int32_t ka = (ylo - Y) / sy, kb = (yhi - Y) / sy;
            k0 = ka < kb ? ka : kb;
            k1 = (ka < kb ? kb : ka) + 1;
            k0 = k0 < 0 ? 0 : k0;
            k1 = k1 > n ? n : k1;
        } else if (Y < ylo || Y > yhi)
            continue;
        for (k = k0; k <= k1; k++)
            if (!umm_umbra(X + sx * k, Y + sy * k))
                umm_splat(X + sx * k, Y + sy * k, 256, m->lc[i]);
        if (age < 220u) {                               /* a spark runs from the note before to this one */
            int32_t f = (int32_t)(age * 1192u >> 10), Xa = X, Ya = Y, Xb = X + sx * n, Yb = Y + sy * n, x, y;
            if (m->tf[i] != m->ta[i]) {
                Xa = Xb; Ya = Yb; Xb = X; Yb = Y;
            }
            x = Xa + (((Xb - Xa) >> 4) * f >> 4);
            y = Ya + (((Yb - Ya) >> 4) * f >> 4);
            if (!umm_umbra(x, y)) {
                umm_splat(x, y, 307, UMM_WT);
                for (k = 0; k < 4; k++)
                    umm_splat(x + (k == 0 ? 256 : k == 1 ? -256 : 0), y + (k == 2 ? 256 : k == 3 ? -256 : 0), 77, UMM_WT);
            }
        }
    }
    for (i = 0; i < UMM_ND; i++) {                      /* the notes' points: a short glow (~1 s) */
        uint32_t age = umb_now - m->nt[i];
        const uint8_t *tc = UMB_EVC[(m->nc[i] * 3u) % 8u];
        int32_t b, X, Y, dx, dy;
        if (!m->nv[i] || age > 3000u)
            continue;
        b = ((((m->nv[i] * umb_exp((int32_t)(age * 73u >> 11))) >> 8) * 230 >> 8) + 8) & ~15;
        umm_pos(m->nc[i], m->nn[i], &X, &Y);
        X = (X + 128) >> 8;
        Y = (Y + 128) >> 8;
        if (!b || Y + 3 < umm_y0 || Y - 3 >= umm_y0 + (int32_t)cv_h)
            continue;
        for (dy = -3; dy <= 3; dy++)
            for (dx = -3; dx <= 3; dx++) {
                int32_t w = 256 - (dx * dx + dy * dy) * 25;
                if (w > 0)
                    umm_px(X + dx, Y + dy, (b * ((w * w) >> 8)) >> 8, (tc[0] + UMM_WT[0]) >> 1, (tc[1] + UMM_WT[1]) >> 1,
                           (tc[2] + UMM_WT[2]) >> 1);
            }
    }
}
/* once a frame: a plate just chosen starts afresh; the notes since; the plate's work */
static void umm_tick(uint32_t pl)
{
    umm_t *m = &umb.m;
    umm_lq = umv_light(pl);
    umm_fk = 506 / ((int32_t)umv(1u, 1u) + 1);         /* (FADE 45: 11) */
    umm_ru = 26 * umv_sc(1u);                           /* (SIZE) */
    if (umm_on != pl) {
        memset(m, 0, sizeof *m);
        umm_on = (uint8_t)pl;
        umb.init = 0;
        umb.qr = umb_q.w;
        m->tms = umb_now;
    }
    if (umb.f0 == ui.frame)
        return;
    umb.f0 = ui.frame;
    if (umb_q.w - umb.qr > UMB_NQ)                      /* (a burst: the newest only) */
        umb.qr = umb_q.w - UMB_NQ;
    for (; umb.qr != umb_q.w; umb.qr++) {
        uint32_t q = umb.qr % UMB_NQ, c = umb_q.c[q], v = umb_vel(umb_q.vel[q]);   /* (REACT) */
        if (c >= NTRK || (umb_q.vel[q] && (eco_off(c) || !v)))
            continue;
        if (v)
            umn_note(c, q);                             /* (NOTE MOVEMENT) */
        if (v && trk[c].engine != ENGI_DRUM)
            umm_saros_on(c, umb_q.note[q], (int32_t)v * 2 + 1);
    }
    umm_saros_frame();
}
static __attribute__((noinline)) void umm_backdrop(uint32_t pl, int32_t x0, int32_t y0)
{
    uint32_t i, n = cv_w * cv_h;
    umm_tick(pl);
    umm_x0 = x0;
    umm_y0 = y0;
    for (i = 0; i < n; i++)                             /* (the ground: black, as JIANT's; 5 5 8 until 2026-11-08) */
        cv_px[i] = 0;
    umm_saros_draw();
}
