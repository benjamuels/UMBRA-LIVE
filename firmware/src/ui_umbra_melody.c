/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA OBSCURA contributors */
/* (included by ui_umbra.c, before KILN's umb_backdrop) built for size, as the main loop is (tools/size_fns.py): the FM-1's
 * slot is nearly full, and these plates are light; umm_backdrop stays a call so KILN's -Os does not take it in */
/* ------------------------------------------------------------------------------- SAROS and FILAMENT: the melody --- */
/* Plates of the melody (MENU > SCREEN 2 > PLATE), from the same ring of notes as KILN (voice.c umb_q; a note let go:
 * velocity 0), their state in KILN's field (umb.m). Unlike KILN they are still between notes, so HOME's changed-cells
 * blit (gfx.c cv_blit_cells) sends little to the panel: fades go in 16 steps and SAROS turns a pixel at a time at its
 * rim. Each strip is filled near-black, then the light added to its pixels (RGB565), dimmed under the knob row and the
 * footer as KILN is (TOTALITY: the whole screen, undimmed).
 * SAROS: the melody as string art on the circle of fifths, the whole plate turning once in 100 s round a dark umbra. A
 * note lands on its pitch class (its track's root at the marked tick), its octave the ring; each melodic track's notes
 * are joined in turn by a thread in its colour. Fifths and fourths stay near the rim, thirds cut across, tritones pass
 * through the umbra and vanish; a phrase played again retraces its threads and burns them brighter (a thread fades in
 * ~6 s).
 * FILAMENT: a taut string a track (4 or 6, as TRACKS). A note plucks its track's string where its pitch falls (C2 at the
 * left, C7 at the right; a drum where its lane is), its velocity the height and the melody's contour the side (rising
 * up, falling down); a higher note tightens the string so its waves run faster; a held note keeps bowing it; let go,
 * the string is damped and still within ~0.6 s. A 1D wave, 81 points a string, 125 substeps a second */
#define UMM_CX 120                                      /* SAROS's centre (px) */
#define UMM_CY 120
static const uint8_t UMM_WT[3] = {236, 250, 246}, UMM_TEAL[3] = {143, 217, 201}, UMM_REST[3] = {34, 60, 96};
static uint8_t umm_on;                                  /* the plate drawn last (0: KILN) */
static int32_t umm_x0, umm_y0;                          /* the strip being drawn: its place on the screen */
static int32_t umm_lq, umm_fk, umm_damp;                /* HOME > ECO > PLATE, worked out once a frame (umm_tick): LIGHT
                                                         * (Q8), SAROS's FADE (its rate), FILAMENT's RING (released) */
/* add the light r g b (0..255) x w / 256 to the screen's pixel x, y if the strip has it */
static void umm_px(int32_t x, int32_t y, int32_t w, int32_t r, int32_t g, int32_t b)
{
    uint16_t *p;
    uint32_t q;
    int32_t R, G, B;
    if (w <= 0 || (uint32_t)(x - umm_x0) >= cv_w || (uint32_t)(y - umm_y0) >= cv_h)
        return;
    w = (((w * umm_lq) >> 8) * (umb.bare ? 256 : y < 56 ? 108 : y < 76 ? 108 + (y - 56) * 7 : y >= 226 ? 128 : 256)) >> 8;
                                                        /* (LIGHT; KILN's dimming, TOTALITY: none) */
    p = cv_px + (uint32_t)(y - umm_y0) * cv_w + (uint32_t)(x - umm_x0);
    q = swap16(*p);
    R = umb_clamp((int32_t)((q >> 8) & 0xF8u) + ((r * w) >> 8) + 4, 0, 255);
    G = umb_clamp((int32_t)((q >> 3) & 0xFCu) + ((g * w) >> 8) + 2, 0, 255);
    B = umb_clamp((int32_t)((q << 3) & 0xF8u) + ((b * w) >> 8) + 4, 0, 255);
    *p = swap16((uint32_t)(((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3)));
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
 * the ring (40 .. 107.5 px) */
static void umm_pos(uint32_t c, uint32_t n, int32_t *X, int32_t *Y)
{
    uint32_t k = (n + 120u - (uint32_t)trk[c].p[P_ROOT] % 12u) % 12u * 7u % 12u;
    int32_t r = 10240 + umb_clamp((int32_t)n / 12 - 3, 0, 5) * 3456;
    *X = UMM_CX * 256 + ((r * umb.m.ux[k]) >> 15);
    *Y = UMM_CY * 256 + ((r * umb.m.uy[k]) >> 15);
}
static int32_t umm_glow(uint32_t i)                    /* a thread's energy now (Q8): it fades in ~6 s (FADE 45; 0 .. 99:
                                                         * 0.1 .. 13 s) */
{
    uint32_t age = fm1_ms - umb.m.tt[i];
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
    m->nt[m->nnd] = fm1_ms;
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
    m->tt[i] = fm1_ms;
    m->tf[i] = (uint8_t)p;
}
/* once a frame: the turn, the circle's places, the threads as drawn (a faded one goes) */
static void umm_saros_frame(void)
{
    umm_t *m = &umb.m;
    uint32_t i, j, k, th;
    m->th += (fm1_ms - m->tms) * (42950u * umv(1u, 0u) / 30u);   /* (SPIN 30: a turn in 100 s; 0: still; 99: in 30 s) */
    m->tms = fm1_ms;
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
            m->lc[j][k] = (uint8_t)((tc[k] * e) >> 9);  /* (half its colour at full energy) */
        j++;
    }
    m->nth = (uint8_t)j;
}
static int umm_umbra(int32_t X, int32_t Y)             /* behind the umbra (26 px) */
{
    int32_t dx = (X >> 4) - UMM_CX * 16, dy = (Y >> 4) - UMM_CY * 16;
    return dx * dx + dy * dy < 26 * 26 * 256;
}
static void umm_saros_draw(void)
{
    umm_t *m = &umb.m;
    int32_t i, k, ylo = (umm_y0 - 2) * 256, yhi = (umm_y0 + (int32_t)cv_h + 1) * 256;
    if (umm_y0 < UMM_CY + 32 && umm_y0 + (int32_t)cv_h > UMM_CY - 32)
        for (i = 0; i < 180; i++) {                     /* the umbra's corona (as it was: a turn leaves it so) */
            int32_t s = sine_i((uint32_t)i * 23860929u), c = sine_i((uint32_t)i * 23860929u + 0x40000000u);
            umm_splat(UMM_CX * 256 + ((c * 7040) >> 15), UMM_CY * 256 + ((s * 7040) >> 15), 51, UMM_TEAL);
            umm_splat(UMM_CX * 256 + ((c * 7680) >> 15), UMM_CY * 256 + ((s * 7680) >> 15), 13, UMM_TEAL);
        }
    for (i = 0; i < 12; i++)                            /* the dial: the root's tick teal */
        umm_splat(UMM_CX * 256 + ((29696 * m->ux[i]) >> 15), UMM_CY * 256 + ((29696 * m->uy[i]) >> 15), i ? 179 : 128,
                  i ? UMM_REST : UMM_TEAL);
    for (i = 0; i < m->nth; i++) {                      /* the threads, a sample a pixel (only the strip's) */
        int32_t X = m->lx[i], Y = m->ly[i], sx = m->lsx[i], sy = m->lsy[i], n = m->ln[i], k0 = 0, k1 = n;
        uint32_t age = fm1_ms - m->tt[i];
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
        uint32_t age = fm1_ms - m->nt[i];
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
/* FILAMENT: a note played (velocity vel, 0: let go) on track c: its string plucked, held; let go, no longer held */
static void umm_fil_note(uint32_t c, uint32_t n, uint32_t vel)
{
    umm_t *m = &umb.m;
    uint32_t i, h, drum = trk[c].engine == ENGI_DRUM;
    int32_t v = (int32_t)vel * 2 + 1, k, x, w, iw, amp;
    for (h = 0; h < UMM_NH && m->hn[c][h] != n + 1u; h++)
        ;
    if (!vel) {
        if (h < UMM_NH)
            m->hn[c][h] = 0;
        return;
    }
    k = drum ? (int32_t)(drum_lane(n) % NLANE) * 36 + 2 : umb_clamp(((int32_t)n - 36) * 4369 >> 10, 0, 256);   /* (0..1, Q8) */
    x = k * 80;                                         /* (the place: points, Q8) */
    if (m->last[c] && n + 1u != m->last[c])
        m->dir[c] = (int8_t)(n + 1u > m->last[c] ? -1 : 1);   /* (rising: up) */
    if (!m->dir[c])
        m->dir[c] = -1;
    m->last[c] = (uint8_t)(n + 1u);
    w = 640 + (256 - k) * 7;                            /* (the pluck's half width: 2.5 .. 9.5 points, Q8) */
    iw = 65536 / w;
    amp = (1024 + v * 12) * ((int32_t)umv(2u, 0u) + 1) / 51 * m->dir[c];   /* (4 .. 16 px, Q8, at SWING 50) */
    for (i = 1; i < UMM_NP - 1; i++) {
        int32_t d = (int32_t)i * 256 - x;
        d = d < 0 ? -d : d;
        if (d < w)
            m->u[c][i] = (int16_t)umb_clamp(m->u[c][i] / 2 + ((((amp * (w - d)) >> 8) * iw) >> 8), -3584, 3584);
    }
    m->c2[c] = (uint8_t)(26 + ((k * 205) >> 8));        /* (higher, tighter: 0.1 .. 0.9) */
    m->mv[c] = 1;
    if (drum)                                           /* (a drum is a pluck: nothing held) */
        return;
    if (h == UMM_NH)
        for (h = 0; h < UMM_NH - 1u && m->hn[c][h]; h++)
            ;
    m->hn[c][h] = (uint8_t)(n + 1u);
    m->hx[c][h] = (uint8_t)umb_clamp((x + 128) >> 8, 1, UMM_NP - 2);
    m->hf[c][h] = 3435974u + (uint32_t)k * 67109u;     /* (0.8 .. 4.8 bows a second: phase a ms) */
    m->ha[c][h] = (int16_t)(v * m->dir[c]);
    m->h0[c][h] = fm1_ms;
}
/* the substeps owed (125 a second, 8 a frame at most): the strings' waves; a held note bows; let go, damped */
static void umm_fil_step(void)
{
    umm_t *m = &umb.m;
    uint32_t c, i, h, n = 0;
    while ((uint32_t)(fm1_ms - m->sub) >= 8u && n++ < 8u) {
        m->sub += 8u;
        for (c = 0; c < NTRK; c++) {
            int16_t *u = m->u[c], *v = m->v[c];
            int32_t held = 0, damp, mx = 0, a;
            if (!m->mv[c])
                continue;
            for (h = 0; h < UMM_NH; h++)
                if (m->hn[c][h]) {
                    uint32_t age = m->sub - m->h0[c][h];
                    if (age > 30000u) {                 /* (never let go: after 30 s, as if it were) */
                        m->hn[c][h] = 0;
                        continue;
                    }
                    held = 1;
                    i = m->hx[c][h];
                    v[i] = (int16_t)(v[i] + ((m->ha[c][h] * (sine_i(age * m->hf[c][h]) >> 7)) >> 13));
                }
            damp = held ? 32478 : umm_damp;             /* (Q15 a substep: held, 0.9 s; let go, 0.185 s at RING 50) */
            for (i = 1; i < UMM_NP - 1; i++)
                v[i] = (int16_t)(((v[i] + ((m->c2[c] * (u[i - 1] + u[i + 1] - 2 * u[i])) >> 8)) * damp) >> 15);
            for (i = 1; i < UMM_NP - 1; i++) {
                u[i] = (int16_t)umb_clamp(u[i] + v[i], -4096, 4096);
                a = u[i] < 0 ? -u[i] : u[i];
                mx = a > mx ? a : mx;
            }
            if (mx < 128 && !held) {                    /* (settled: exactly still, nothing more to send) */
                memset(u, 0, sizeof m->u[c]);
                memset(v, 0, sizeof m->v[c]);
                m->mv[c] = 0;
            }
        }
    }
    if ((uint32_t)(fm1_ms - m->sub) >= 8u)
        m->sub = fm1_ms;                                /* (behind: never a burst) */
}
/* the strings in the strip: each a line a column wide, its colour from the rest's deep blue to its track's as it
 * moves (16 steps), held notes tinting it; a glow either side while it moves */
static void umm_fil_draw(void)
{
    umm_t *m = &umb.m;
    int32_t c, x, nl = trk_live, sp = (nl > 4 ? 24 : 34) + (umb.bare ? 10 : 0), cy = umb.bare ? 120 : 150;   /* (TOTALITY:
                                                         * the whole screen) */
    for (c = 0; c < nl && c < NTRK; c++) {
        const int16_t *u = m->u[c];
        const uint8_t *tc = UMB_EVC[(c * 3) % 8];
        int32_t y0 = cy + (2 * c - nl + 1) * sp / 2, py = -1, held = 0, h;
        if (y0 + 20 < umm_y0 || y0 - 20 >= umm_y0 + (int32_t)cv_h)
            continue;
        for (h = 0; h < UMM_NH; h++)
            held |= m->hn[c][h];
        for (x = umm_x0 > 0 ? umm_x0 - 1 : 0; x < umm_x0 + (int32_t)cv_w; x++) {
            int32_t fi = (x * 21937) >> 8, i0 = fi >> 8, f = fi & 255, s, y, e, a, r, g, b, k;
            if (i0 >= UMM_NP - 1) {
                i0 = UMM_NP - 2;
                f = 256;
            }
            s = u[i0] + (((u[i0 + 1] - u[i0]) * f) >> 8);
            y = y0 * 256 + s;
            if (py < 0)
                py = y;
            if (x < umm_x0) {                           /* (the column before the strip: where its line comes from) */
                py = y;
                continue;
            }
            e = (((s < 0 ? -s : s) >> 3) + (held ? 90 : 0));
            e = (umb_clamp(e, 0, 256) + 8) & ~15;
            a = 141 + ((115 * e) >> 8);
            r = UMM_REST[0] + (((tc[0] - UMM_REST[0]) * e) >> 8);
            g = UMM_REST[1] + (((tc[1] - UMM_REST[1]) * e) >> 8);
            b = UMM_REST[2] + (((tc[2] - UMM_REST[2]) * e) >> 8);
            if ((py > y ? py - y : y - py) < 256) {    /* (a point, shared between two rows) */
                umm_px(x, y >> 8, ((256 - (y & 255)) * a) >> 8, r, g, b);
                umm_px(x, (y >> 8) + 1, ((y & 255) * a) >> 8, r, g, b);
            } else                                      /* (steep: the column from the last one's row) */
                for (k = (py < y ? py : y) >> 8; k <= (py < y ? y : py) >> 8; k++)
                    umm_px(x, k, (a * 205) >> 8, r, g, b);
            if (e) {
                umm_px(x, (y >> 8) - 2, (31 * e) >> 8, r, g, b);
                umm_px(x, (y >> 8) + 2, (31 * e) >> 8, r, g, b);
            }
            py = y;
        }
    }
}
/* once a frame: a plate just chosen starts afresh (KILN too, when it comes back); the notes since; the plate's work */
static void umm_tick(uint32_t pl)
{
    umm_t *m = &umb.m;
    uint32_t r = umv(2u, 1u) + 1u, d = 3607587u / (r * r);   /* (RING: 1 - damp, 1387 at 50, as r^-2; 0: 8192) */
    umm_lq = umv_light(pl);
    umm_fk = 506 / ((int32_t)umv(1u, 1u) + 1);         /* (FADE 45: 11) */
    umm_damp = 32768 - (int32_t)(d < 8192u ? d : 8192u);
    if (umm_on != pl) {
        memset(m, 0, sizeof *m);
        umm_on = (uint8_t)pl;
        umb.init = 0;
        umb.qr = umb_q.w;
        m->sub = m->tms = fm1_ms;
    }
    if (umb.f0 == ui.frame)
        return;
    umb.f0 = ui.frame;
    if (umb_q.w - umb.qr > UMB_NQ)                      /* (a burst: the newest only) */
        umb.qr = umb_q.w - UMB_NQ;
    for (; umb.qr != umb_q.w; umb.qr++) {
        uint32_t q = umb.qr % UMB_NQ, c = umb_q.c[q];
        if (c >= NTRK || (eco_off(c) && umb_q.vel[q]))
            continue;
        if (pl == 2u)
            umm_fil_note(c, umb_q.note[q], umb_q.vel[q]);
        else if (umb_q.vel[q] && trk[c].engine != ENGI_DRUM)
            umm_saros_on(c, umb_q.note[q], umb_q.vel[q] * 2 + 1);
    }
    if (pl == 2u)
        umm_fil_step();
    else
        umm_saros_frame();
}
static __attribute__((noinline)) void umm_backdrop(uint32_t pl, int32_t x0, int32_t y0)
{
    uint32_t i, n = cv_w * cv_h;
    umm_tick(pl);
    umm_x0 = x0;
    umm_y0 = y0;
    for (i = 0; i < n; i++)                             /* (the ground: near-black, 5 5 8) */
        cv_px[i] = swap16(0x0021u);
    if (pl == 2u)
        umm_fil_draw();
    else
        umm_saros_draw();
}
