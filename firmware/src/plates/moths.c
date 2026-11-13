/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* MOTHS, a plate module (umb_plate.h): the Keeper's lamp and its moths, from UMBRA OBSCURA, the game. A lamp in the
 * middle; a note lets a moth in from the dark at the edge (its pitch where round the lamp, its track its colour), and it
 * spirals in to circle the lamp, wings beating, until it tires and fades. The kick scatters them outward; they come back.
 * Line art only: its light is added to the strip, so it lies on a layer as well as it stands alone (REPLACE: black under
 * it).
 * Elements: FLIGHT (how fast they circle), FADE (how long a moth lasts), SIZE (the wings'), MOTHS (the notes that are no
 * drum: a moth each), SCATTR (the kick: they scatter), LAMP (the rest of the kit: the lamp flares), ORBIT (how far out
 * they circle). */
#include "plate_lib.h"

#define PLM_N 16                                        /* moths at once */
typedef struct {
    uint32_t t0[PLM_N];                                 /* each moth: since when (0: none) .. */
    uint16_t a[PLM_N];                                  /* .. where round the lamp (65536 a turn) */
    int16_t r[PLM_N];                                   /* .. how far out (px, Q6) */
    uint8_t v[PLM_N], c[PLM_N], sd[PLM_N];              /* .. how hard, its track, its own dice */
    uint16_t lamp, scat;                                /* the lamp's flare, the scatter (Q8) */
    uint8_t n;
} plm_t;

static void plm_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    (void)st;
    (void)cx;
    (void)a;
}
static void plm_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    plm_t *m = st;
    uint32_t i;
    int32_t v;
    (void)a;
    if (!nt->vel)
        return;
    if ((v = pl_drive(cx, 4u, nt, nt->drum && !nt->lane)) > m->scat)
        m->scat = (uint16_t)v;
    if ((v = pl_drive(cx, 5u, nt, nt->drum && nt->lane)) > m->lamp)
        m->lamp = (uint16_t)v;
    if (!(v = pl_drive(cx, 3u, nt, !nt->drum)))
        return;
    i = m->n++ % PLM_N;
    m->t0[i] = cx->now | 1u;
    m->a[i] = (uint16_t)(pl_fold(nt->mo[0], pl_pk(nt->note), 256) * 255 + nt->track * 10923u);
    m->r[i] = 118 * 64;
    m->v[i] = (uint8_t)v;
    m->c[i] = nt->track;
    m->sd[i] = (uint8_t)(nt->note * 37u + m->n * 101u);
}
static void plm_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    plm_t *m = st;
    int32_t dt = (int32_t)cx->dt, life = 1200 + cx->k[1] * 76, i;   /* (FADE 50: 5 s) */
    (void)a;
    for (i = 0; i < PLM_N; i++) {
        int32_t home = (18 + cx->k[6] * 30 / 50 + (m->sd[i] & 15)) * 64, r = m->r[i];   /* (ORBIT 50: 48 .. 63 px) */
        if (!m->t0[i])
            continue;
        if (cx->now - m->t0[i] >= (uint32_t)life) {
            m->t0[i] = 0;
            continue;
        }
        r += (home - r) * dt / 450 + (m->scat * dt * 3 >> 6);   /* (in to its orbit in ~0.5 s; the scatter: ~30 px out) */
        m->r[i] = (int16_t)pl_clamp(r, 8 * 64, 150 * 64);
        m->a[i] = (uint16_t)(m->a[i] + (uint32_t)dt * cx->k[0] * (20u + (m->sd[i] >> 4)) / 50u);   /* (FLIGHT 50: a turn in 2 .. 3 s) */
    }
    m->scat = pl_dec(m->scat, dt * 262);                /* (0.25 s) */
    m->lamp = pl_dec(m->lamp, dt * 164);                /* (0.4 s) */
}
static void plm_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    const plm_t *m = st;
    int32_t X = 120 * 256, Y = (cx->bare ? 120 : 142) * 256, i, life = 1200 + cx->k[1] * 76, w = (3 + cx->k[2] * 5 / 50) * 256,
            lg = 90 + cx->level / 3 + m->lamp / 2;
    pl_clear(S);
    pl_arc(S, a, X, Y, 6 * 256, 0u, 0xFFFFFFFFu, lg, PL_WT);   /* the lamp: its glass, its light, eight rays */
    pl_glint(S, X, Y, 150 + lg / 2, PL_WARM[0]);
    for (i = 0; i < 8; i++) {
        uint32_t ph = (uint32_t)i * 536870912u + cx->now * 20000u;
        pl_line(S, X + pl_pol(a, 10 * 256, ph + 0x40000000u), Y + pl_pol(a, 10 * 256, ph), X + pl_pol(a, (13 + lg / 16) * 256, ph + 0x40000000u),
                Y + pl_pol(a, (13 + lg / 16) * 256, ph), lg / 2, PL_WARM[0]);
    }
    for (i = 0; i < PLM_N; i++) {                       /* the moths: a body, two wings beating, its wobble its own */
        uint32_t age = cx->now - m->t0[i], ph = (uint32_t)m->a[i] << 16, hd, fl;
        const uint8_t *tc;
        int32_t r, x, y, e, wa;
        if (!m->t0[i] || age >= (uint32_t)life)
            continue;
        tc = a->colour[(m->c[i] * 3u) % 8u];
        r = m->r[i] * 4 + pl_pol(a, 3 * 256, age * 9000000u + m->sd[i] * 16843009u);
        x = X + pl_pol(a, r, ph + 0x40000000u);
        y = Y + pl_pol(a, r, ph);
        e = m->v[i] * (age * 3u > (uint32_t)life * 2u ? 3 * (int32_t)((uint32_t)life - age) : life) / life;   /* (its last third: fading) */
        e = 70 + e * 3 / 4;
        hd = ph + 0x80000000u;                          /* (its heading: along its circle) */
        fl = age * 60000000u + m->sd[i] * 7777777u;     /* (14 beats a second) */
        wa = 0x18000000 + (pl_sine(a, fl) >> 4) * 0x40000;   /* (the wings' angle from its back: 34 +- 45 degrees) */
        pl_splat(S, x, y, e + 60, PL_WT);
        pl_line(S, x, y, x + pl_pol(a, w, hd + (uint32_t)wa + 0x40000000u), y + pl_pol(a, w, hd + (uint32_t)wa), e, tc);
        pl_line(S, x, y, x + pl_pol(a, w, hd - (uint32_t)wa + 0x40000000u), y + pl_pol(a, w, hd - (uint32_t)wa), e, tc);
    }
}

UMB_PLATE_HEAD const umb_plate_t umb_plate_moths = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(plm_t), "MOTHS",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 7u, {0, 0, 0},
    {{"FLIGHT", 50}, {"FADE", 50}, {"SIZE", 50}, {"MOTHS", 50}, {"SCATTR", 50}, {"LAMP", 50}, {"ORBIT", 50}},
    plm_init, plm_note, plm_frame, plm_draw,
    {0, 0, 0, UMB_EL_NOTES, UMB_EL_NOTES, UMB_EL_NOTES, 0},
    "A lamp at night, a moth for each note", 3u, {0, 0, 0},
    {{"DUSK", {26, 81, 0, 0, 0, 36, 71, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_LFO(0), 6u), UMB_LINK(UMB_SRC_LEVEL, 5u)}, {99, 99, 30, 50}},
     {"SWARM", {86, 0, 0, 96, 61, 0, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 5u), UMB_LINK(UMB_SRC_HIT(0), 4u)}, {99, 99, 60, 40}},
     {"FLICKER", {0, 26, 71, 0, 0, 91, 0, 0}, {UMB_LINK(UMB_SRC_KNOB(0), 0u), UMB_LINK(UMB_SRC_KNOB(1), 1u), UMB_LINK(UMB_SRC_BEAT, 5u), UMB_LINK(UMB_SRC_LEVEL, 0u)}, {99, 99, -70, 40}}}};
