/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* TROUPE, a plate module (umb_plate.h): dancers on a black stage. Each is a solid silhouette lit at its edges (a rim of
 * light, the body dim inside), its arms drawn over it, a faint reflection on the floor below. They dance a choreography:
 * twelve poses (prep, second, fifth on the toes, arabesque, lunges, passe, attitude, a curl, sways, a wide plie) in four
 * phrases of eight, a pose a step, each reached on its beat and held. The beat drives it, phase-locked by small
 * corrections. A passe turns a pirouette (the body narrowing and widening, the skirt flares, the ribbons wind
 * round); a kit's kick sends the dancer up in a hop (knees tucked, landing on the floor); a note can choose the next pose
 * (low notes grounded ones, high notes lifted ones). Ribbons trail from both hands in the dancer's two tracks' colours,
 * flaring with their notes; skirts of light threads trail the hips, ride up over a raised leg and settle. Dancer d
 * takes the tracks t with t % CAST = d.
 * Light only: added to the strip, so it lies on a layer as well as it stands alone (REPLACE: black under).
 * Elements: CAST (one dancer, a duet, a trio), STEP (a pose every 4, 2 or 1 beats), FLOW (0 a snap into each pose and a
 * long hold, 99 one move flowing into the next), LEAP (the notes: hops, as hard as its value; a kit's kick by default),
 * SPIN (how often they turn: 99 double pirouettes), RIBBON (the ribbons' length), IMPROV (the notes: how often one
 * chooses the next pose), CANON (0 all in step, 99 each a step behind the last). */
#include "plate_lib.h"

#define PLQ_ND 3                                        /* the dancers */
#define PLQ_NP 12                                       /* the poses */
#define PLQ_NA 11                                       /* a pose's numbers */
#define PLQ_NR 24                                       /* a ribbon's points */
#define PLQ_NC 12                                       /* a dancer's shapes (the body's first, then the arms') */
#define PLQ_NB 7                                        /* .. the body's */
#define PLQ_NK 13                                       /* a skirt's threads */
enum { PLQ_HIP, PLQ_LHP, PLQ_RHP, PLQ_LKN, PLQ_RKN, PLQ_LFT, PLQ_RFT, PLQ_WST, PLQ_SHB, PLQ_HEAD, PLQ_LSH, PLQ_RSH, PLQ_LEL,
       PLQ_REL, PLQ_LHA, PLQ_RHA, PLQ_NJ };
/* a pose (1/256 turn; the right side's, the left mirrored): the lean (+ right), the head's tilt, the left arm (from down,
 * outward) and its forearm's bend, the right's, the left leg (from down, outward) and its shin's bend, the right's, the
 * rise (px, - onto the toes) */
static const int8_t PLQ_P[PLQ_NP][PLQ_NA] = {
    {0, 0, 14, -22, 14, -22, 5, 0, 5, 0, 0},            /* 0 prep: arms low and rounded */
    {0, 0, 58, 6, 58, 6, 16, 0, 16, 0, 0},              /* 1 second: arms open, feet apart */
    {0, -4, 120, 28, 120, 28, 3, 0, 3, 0, -5},          /* 2 fifth: arms an oval overhead, on the toes */
    {-18, 6, 92, 0, 60, 4, 2, 0, 84, 4, 0},             /* 3 arabesque: the right leg out level */
    {-10, -4, 100, 10, 44, 4, 30, -34, 40, 0, 0},       /* 4 lunge left */
    {10, 4, 44, 4, 100, 10, 40, 0, 30, -34, 0},         /* 5 lunge right */
    {0, 0, 120, 28, 120, 28, 2, 0, 50, -92, -5},        /* 6 passe: the foot at the knee (it turns) */
    {-10, 6, 60, 6, 110, 24, 2, 0, 70, -44, 0},         /* 7 attitude: the right leg up, bent */
    {0, -14, 6, -50, 6, -50, 14, -26, 14, -26, 0},      /* 8 curl: arms crossed low, knees bent */
    {16, 8, 34, -6, 122, 14, 22, 0, 2, 0, 0},           /* 9 sway right: the right arm up, the left foot pointed */
    {-16, -8, 122, 14, 34, -6, 2, 0, 22, 0, 0},         /* 10 sway left */
    {0, -6, 96, 0, 96, 0, 26, -30, 26, -30, 0}};        /* 11 wide plie: arms a V */
static const uint8_t PLQ_PH[4][8] = {{0, 1, 2, 9, 10, 6, 1, 8}, {1, 4, 5, 3, 7, 2, 6, 0},   /* the four phrases */
                                     {9, 10, 11, 2, 3, 1, 6, 8}, {4, 11, 5, 7, 1, 2, 6, 0}};
static const uint8_t PLQ_PITCH[11] = {8, 11, 4, 5, 1, 9, 10, 3, 7, 6, 2};   /* the poses low to high, as a note picks */
/* the shapes: two joints and the radius at each (px x 4): torso, neck, head, legs; shoulders, arms */
static const uint8_t PLQ_C[PLQ_NC][4] = {
    {PLQ_HIP, PLQ_SHB, 32, 44}, {PLQ_SHB, PLQ_HEAD, 14, 12}, {PLQ_HEAD, PLQ_HEAD, 36, 36},
    {PLQ_LHP, PLQ_LKN, 28, 19}, {PLQ_LKN, PLQ_LFT, 18, 8}, {PLQ_RHP, PLQ_RKN, 28, 19}, {PLQ_RKN, PLQ_RFT, 18, 8},
    {PLQ_LSH, PLQ_RSH, 18, 18}, {PLQ_LSH, PLQ_LEL, 17, 13}, {PLQ_LEL, PLQ_LHA, 13, 8},
    {PLQ_RSH, PLQ_REL, 17, 13}, {PLQ_REL, PLQ_RHA, 13, 8}};
/* where the dancers stand (px) and how large (Q8), for a cast of one, two, three */
static const uint8_t PLQ_X[3][PLQ_ND] = {{120, 120, 120}, {80, 160, 120}, {120, 52, 188}}, PLQ_S[3] = {232, 186, 150};
typedef struct {                                        /* a shape on the screen (px x 16) */
    int16_t ax, ay, dx, dy, r0, r1;                     /* its start, its run, its radii */
    int16_t x0, x1, y0, y1;                             /* its box (px; y0 > y1: none) */
    int32_t inv, ir;                                    /* 2^30 / its run squared (0: a disc); 2^20 / its radii's sum */
} plq_cap_t;
typedef struct {
    uint32_t bp, on, per;                               /* the beat (2^16 a beat), the last beat's time (ms), between beats */
    uint32_t rms;                                       /* the ribbons' last point (ms) */
    uint8_t lb, ok, rw, rn, n, sh, rsv[2];              /* the beat last frame; placed; the ribbons' newest, count; cast; step */
    uint8_t pose[PLQ_ND], amt[PLQ_ND], pend[PLQ_ND];   /* the pose danced toward, its turns, a note's pick */
    uint8_t lit[PLQ_ND][3];                             /* the light: the body's, each ribbon's */
    uint32_t si[PLQ_ND], tr0[PLQ_ND], turn[PLQ_ND];    /* the step; the turn at its start, now (2^32 a turn) */
    int16_t from[PLQ_ND][PLQ_NA], cur[PLQ_ND][PLQ_NA];  /* the pose at the step's start, now (65536 a turn; px x 256) */
    int32_t fx[PLQ_ND], fs[PLQ_ND], fa[PLQ_ND];         /* each dancer's place (px Q8), scale (Q16), presence (Q16), gliding */
    int32_t hy[PLQ_ND], hv[PLQ_ND];                     /* the hop: height, speed (px Q16, per ms; up -) */
    int32_t fl[PLQ_ND];                                 /* a turn's flare of the skirt (px Q8) */
    /* the skirt: threads from the waist, each a point halfway and one at the hem on springs toward where the legs and
     * the waist put them (px Q8; speeds px Q16 per ms; where they're pulled px Q8), and where each hangs from (px x 16) */
    int32_t sk[PLQ_ND][PLQ_NK][2][2], sv[PLQ_ND][PLQ_NK][2][2], stg[PLQ_ND][PLQ_NK][2][2];
    int16_t sa[PLQ_ND][PLQ_NK][2];
    int16_t p[PLQ_ND][PLQ_NJ][2];                       /* the joints on the screen (px x 16) */
    plq_cap_t c[PLQ_ND][PLQ_NC];
    int16_t rib[PLQ_ND][2][PLQ_NR][2];                  /* the ribbons: the hands as they were (px x 16) */
    uint16_t dep[2][240];                               /* a row's depth inside the body, the arms (px x 256) */
} plq_t;

static void plq_ease(int32_t *v, int32_t t, int32_t f) { *v += ((t - *v) >> 4) * f >> 12; }
/* a point L (px Q8) from f along heading h (65536 a turn, 0 straight down, 16384 right) */
static void plq_seg(const umb_api_t *a, int32_t *o, const int32_t *f, int32_t L, int32_t h)
{
    o[0] = f[0] + ((L * (PL_SIN(a, h) >> 3)) >> 12);
    o[1] = f[1] + ((L * (PL_COS(a, h) >> 3)) >> 12);
}
/* dancer d's joints and shapes from its pose now, its feet on the floor F (px Q8) */
static PL_SMALL void plq_pose(plq_t *m, const umb_api_t *a, uint32_t d, int32_t F)
{
    const int16_t *c = m->cur[d];
    int32_t S = m->fs[d] >> 8, j[PLQ_NJ][2], k, base, T = 32768 - c[0], tk = (-m->hy[d] >> 10) * 7,
            cs = pl_abs(pl_sine(a, m->turn[d] + 0x40000000u)), X = m->fx[d];
    cs = (9830 + (cs * 22938 >> 15)) * (d == 1u ? -1 : 1);   /* (a turn narrows it to a third and back; the partner mirrored) */
    j[PLQ_HIP][0] = j[PLQ_HIP][1] = j[PLQ_LHP][1] = j[PLQ_RHP][1] = 0;
    j[PLQ_LHP][0] = -6 * S, j[PLQ_RHP][0] = 6 * S;
    plq_seg(a, j[PLQ_LKN], j[PLQ_LHP], 40 * S, -c[6]);
    plq_seg(a, j[PLQ_LFT], j[PLQ_LKN], 40 * S, -(c[6] + c[7]) + tk);   /* (tk: the knees tucked in a hop) */
    plq_seg(a, j[PLQ_RKN], j[PLQ_RHP], 40 * S, c[8]);
    plq_seg(a, j[PLQ_RFT], j[PLQ_RKN], 40 * S, c[8] + c[9] - tk);
    plq_seg(a, j[PLQ_WST], j[PLQ_HIP], 8 * S, T);
    plq_seg(a, j[PLQ_SHB], j[PLQ_HIP], 40 * S, T);
    plq_seg(a, j[PLQ_HEAD], j[PLQ_SHB], 17 * S, T - c[1]);
    plq_seg(a, j[PLQ_LSH], j[PLQ_SHB], 13 * S, T + 16384);
    plq_seg(a, j[PLQ_RSH], j[PLQ_SHB], 13 * S, T - 16384);
    plq_seg(a, j[PLQ_LEL], j[PLQ_LSH], 28 * S, -c[2] - c[0]);
    plq_seg(a, j[PLQ_LHA], j[PLQ_LEL], 30 * S, -(c[2] + c[3]) - c[0]);
    plq_seg(a, j[PLQ_REL], j[PLQ_RSH], 28 * S, c[4] - c[0]);
    plq_seg(a, j[PLQ_RHA], j[PLQ_REL], 30 * S, c[4] + c[5] - c[0]);
    /* (the lower foot on the floor; a rise onto the toes, a hop above it) */
    base = F - (j[PLQ_LFT][1] > j[PLQ_RFT][1] ? j[PLQ_LFT][1] : j[PLQ_RFT][1]) + c[10] * S / 256 + (m->hy[d] >> 8) * S / 256;
    for (k = 0; k < PLQ_NJ; k++) {
        m->p[d][k][0] = (int16_t)((X + ((j[k][0] >> 3) * (cs >> 3) >> 9)) >> 4);
        m->p[d][k][1] = (int16_t)((base + j[k][1]) >> 4);
    }
    {   /* (the skirt: each thread hangs from the waist toward a hem stretched between the thighs, all the way down
         * them to the knees and outside them: a raised leg lifts its side, a turn flares it all out) */
        int32_t ax = j[PLQ_LKN][0] - 13 * S - m->fl[d], ay = j[PLQ_LKN][1] - 4 * S - (m->fl[d] >> 1),
                bx = j[PLQ_RKN][0] + 13 * S + m->fl[d], by = j[PLQ_RKN][1] - 4 * S - (m->fl[d] >> 1), i, u, hx, hy, wx, wy;
        for (i = 0; i < PLQ_NK; i++) {
            u = i * 683 >> 5;                       /* (i / 12 of the way, Q8) */
            hx = ax + ((bx - ax) * u >> 8), hy = ay + ((by - ay) * u >> 8) + 3 * S - ((u - 128) * (u - 128) * 3 * S >> 13);   /* (the hem round: the front lower) */
            wx = j[PLQ_WST][0] + ((u - 128) * 5 * S >> 7), wy = j[PLQ_WST][1];
            m->sa[d][i][0] = (int16_t)((X + ((wx >> 3) * (cs >> 3) >> 9)) >> 4);
            m->sa[d][i][1] = (int16_t)((base + wy) >> 4);
            wx = ((wx + hx) >> 1) + ((u - 128) * S >> 7);   /* (halfway, bowed out a little: a bell) */
            m->stg[d][i][0][0] = X + ((wx >> 3) * (cs >> 3) >> 9), m->stg[d][i][0][1] = base + ((wy + hy) >> 1);
            m->stg[d][i][1][0] = X + ((hx >> 3) * (cs >> 3) >> 9), m->stg[d][i][1][1] = base + hy;
        }
    }
    for (k = 0; k < PLQ_NC; k++) {
        plq_cap_t *q = &m->c[d][k];
        const int16_t *u = m->p[d][PLQ_C[k][0]], *v = m->p[d][PLQ_C[k][1]];
        int32_t r0 = PLQ_C[k][2] * S >> 6, r1 = PLQ_C[k][3] * S >> 6, rm, L2;
        rm = r0 > r1 ? r0 : r1;
        q->ax = u[0], q->ay = u[1], q->dx = (int16_t)(v[0] - u[0]), q->dy = (int16_t)(v[1] - u[1]);
        q->r0 = (int16_t)r0, q->r1 = (int16_t)r1;
        L2 = q->dx * q->dx + q->dy * q->dy;
        q->inv = L2 >= 64 ? (int32_t)((1u << 30) / (uint32_t)L2) : 0;
        q->ir = (1 << 20) / (r0 + r1 + 1);
        q->x0 = (int16_t)(((u[0] < v[0] ? u[0] : v[0]) - rm) >> 4), q->x1 = (int16_t)(((u[0] > v[0] ? u[0] : v[0]) + rm) >> 4);
        q->y0 = (int16_t)(((u[1] < v[1] ? u[1] : v[1]) - rm) >> 4), q->y1 = (int16_t)(((u[1] > v[1] ? u[1] : v[1]) + rm) >> 4);
        if (!rm)
            q->y0 = 1, q->y1 = 0;
    }
}
static void plq_init(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    plq_t *m = st;
    (void)a;
    m->per = 500u;
    m->on = m->rms = cx->now;
}
static PL_SMALL void plq_note(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_note_t *nt)
{
    plq_t *m = st;
    uint32_t d, s;
    int32_t v, h;
    (void)a;
    if (!nt->vel || !m->n)
        return;
    d = nt->track % m->n, s = (nt->track / m->n) & 1u;
    v = nt->vel * 2 + 1;
    m->lit[d][0] = (uint8_t)(v > m->lit[d][0] ? v : m->lit[d][0]);
    m->lit[d][1 + s] = (uint8_t)(v > m->lit[d][1 + s] ? v : m->lit[d][1 + s]);
    h = pl_drive(cx, 3u, nt, nt->drum && nt->lane == 0u);   /* (LEAP: a kit's kick, unless chosen otherwise) */
    if (h && m->hy[d] > -2 * 65536)
        m->hv[d] = -16450 * h / 255;                    /* (full: 22 px up and down in 0.35 s) */
    if (pl_drive(cx, 6u, nt, !nt->drum))                /* (IMPROV: the note's pitch picks the next pose) */
        m->pend[d] = (uint8_t)(1 + (pl_pk(nt->note) * 11 >> 8));
}
/* dancer d's next step (si): the pose to dance toward, its turns; the pose now kept as where it starts */
static PL_SMALL void plq_step(plq_t *m, const umb_lctx_t *cx, uint32_t d, uint32_t si)
{
    uint32_t h = pl_hash(si * 3u + d), q = PLQ_PH[(si >> 3) & 3u][si & 7u], k;
    m->si[d] = si;
    if (m->pend[d] && h % 100u < cx->k[6] + 1u)
        q = PLQ_PITCH[pl_clamp(m->pend[d] - 1, 0, 10)];
    m->pend[d] = 0;
    if (q == m->pose[d])
        q = PLQ_PH[(si >> 3) & 3u][(si + 3u) & 7u];
    for (k = 0; k < PLQ_NA; k++)
        m->from[d][k] = m->cur[d][k];
    m->tr0[d] = m->turn[d];
    m->amt[d] = (uint8_t)(q == 6u ? (cx->k[4] >= 10u) + (cx->k[4] >= 70u) : (h >> 8) % 300u < cx->k[4]);
    m->pose[d] = (uint8_t)q;
}
static PL_SMALL void plq_frame(void *st, const umb_lctx_t *cx, const umb_api_t *a)
{
    plq_t *m = st;
    uint32_t d, k, n = 1u + cx->k[0] * 3u / 100u, sh = cx->k[1] < 33u ? 2u : cx->k[1] < 66u ? 1u : 0u, sb = 1u << sh;
    int32_t w = 8192 + cx->k[2] * 24576 / 99, F = (cx->bare ? 198 : 206) * 256,
            fp = pl_clamp((int32_t)cx->dt * 65536 / 900, 0, 65536), fs = (int32_t)cx->dt * 65536 / 700, dt = (int32_t)cx->dt;
    if (cx->beat > m->lb + 40u) {                       /* (a beat: the tempo learnt slowly, the dance nudged toward it) */
        uint32_t gap = cx->now - m->on, nb = (m->bp + 0x8000u) & 0xFFFF0000u;
        if (gap >= 200u && gap <= 2000u)
            m->per = (uint32_t)((int32_t)m->per + ((int32_t)gap - (int32_t)m->per) / 4);
        m->on = cx->now;
        m->bp += (uint32_t)((int32_t)(nb - m->bp) / 4);
    }
    m->lb = cx->beat;
    m->bp += cx->dt * 65536u / m->per;
    m->n = (uint8_t)n;
    for (d = 0; d < PLQ_ND; d++) {
        uint32_t pos = m->bp - d * (cx->k[7] * (sb << 16) / 99u), si = pos >> (16u + sh);
        int32_t f, tf, e, X = (PLQ_X[n - 1u][d] + (cx->mo[d][0] * 6 >> 8)) * 256;
        /* (where it stands, how large, there or not: gliding) */
        if (!m->ok)
            m->fx[d] = X, m->fs[d] = PLQ_S[n - 1u] * 256;
        if (d < n)
            plq_ease(&m->fx[d], X, fp);
        plq_ease(&m->fs[d], PLQ_S[n - 1u] * 256, fp);
        plq_ease(&m->fa[d], d < n ? 65280 : 0, fp * 2);
        /* (the step: a pose every STEP's beats, reached in FLOW's share of it, eased in and out) */
        if (sh != m->sh || (int32_t)(si - m->si[d]) > 0 || m->si[d] - si > 4096u || !m->ok)
            plq_step(m, cx, d, si);
        f = pl_clamp((int32_t)(pos - (m->si[d] << (16u + sh))) >> sh, 0, 65535) >> 1;
        tf = pl_clamp(f * 32768 / w, 0, 32768);
        e = ((tf * tf) >> 15) * ((98304 - 2 * tf) >> 1) >> 14;
        for (k = 0; k < PLQ_NA; k++)
            m->cur[d][k] = (int16_t)(m->from[d][k] + (((PLQ_P[m->pose[d]][k] * 256 - m->from[d][k]) * (e >> 1)) >> 14));
        tf = f;                                         /* (a turn takes the whole step, whatever FLOW) */
        e = ((tf * tf) >> 15) * ((98304 - 2 * tf) >> 1) >> 14;
        m->turn[d] = m->tr0[d] + (uint32_t)e * 131072u * m->amt[d];
        k = m->amt[d] ? (uint32_t)pl_sine(a, (uint32_t)tf << 16) : 0u;   /* (a turn flares the skirt) */
        m->fl[d] = (int32_t)k * (6 + 4 * m->amt[d]) >> 7;
        /* (the hop: up, and down to the floor) */
        m->hy[d] += m->hv[d] * dt + 47 * dt * dt;
        m->hv[d] += 94 * dt;
        if (m->hy[d] >= 0)
            m->hy[d] = m->hv[d] = 0;
        for (k = 0; k < 3u; k++)
            m->lit[d][k] = (uint8_t)pl_dec(m->lit[d][k], fs);
        plq_pose(m, a, d, F);
        if (d != 1u) {                                  /* (the skirt's threads: springs, the hem softer, so it trails) */
            int32_t ns = (dt + 15) >> 4, h = dt / (ns ? ns : 1), i, q, x, it;
            for (it = 0; it < ns; it++)
                for (i = 0; i < PLQ_NK; i++)
                    for (q = 0; q < 2; q++)
                        for (x = 0; x < 2; x++) {
                            int32_t *P = &m->sk[d][i][q][x], *V = &m->sv[d][i][q][x],
                                    T = m->stg[d][i][q][x] + (q && !x ? pl_sine(a, cx->now * 0x500000u + (uint32_t)i * 0x30000000u) >> 6 : 0);
                            if (!m->ok || m->fa[d] < 512) {
                                *P = T, *V = 0;
                                continue;
                            }
                            *V += ((T - *P) * h * (q ? 24 : 90) >> 10) - (*V * h * (q ? 7 : 15) >> 10);
                            *P += *V * h >> 8;
                        }
        }
    }
    if (cx->now - m->rms >= 8u + cx->k[5] * 32u / 99u) {   /* (the ribbons: the hands' path, a point every 8 .. 40 ms) */
        m->rms = cx->now;
        m->rw = (uint8_t)((m->rw + 1u) % PLQ_NR);
        for (d = 0; d < PLQ_ND; d++)
            for (k = 0; k < 2u; k++) {
                m->rib[d][k][m->rw][0] = m->p[d][PLQ_LHA + k][0];
                m->rib[d][k][m->rw][1] = m->p[d][PLQ_LHA + k][1];
            }
        m->rn = (uint8_t)(m->rn < PLQ_NR ? m->rn + 1u : PLQ_NR);
    }
    m->sh = (uint8_t)sh;
    m->ok = 1;
}
/* dancer d's shapes on the strip as one silhouette, lit at its edge and dim inside, its arms over it (the brighter of
 * the two: an arm across the body shows as its outline); mirrored, faint, below the floor */
static __attribute__((noinline)) void plq_fill(plq_t *m, const umb_strip_t *S, uint32_t d, int32_t Fy, int32_t al, const uint8_t *col)
{
    int32_t y, x, i, xa = 240, xb = -1, ya = 999, yb = -999, rim = 210 + m->lit[d][0] / 6, fill = 72 + m->lit[d][0] / 4;
    for (i = 0; i < PLQ_NC; i++) {
        const plq_cap_t *q = &m->c[d][i];
        if (q->y0 > q->y1)
            continue;
        xa = q->x0 < xa ? q->x0 : xa, xb = q->x1 > xb ? q->x1 : xb, ya = q->y0 < ya ? q->y0 : ya, yb = q->y1 > yb ? q->y1 : yb;
    }
    xa = pl_clamp(xa, S->x0, S->x0 + S->w - 1), xb = pl_clamp(xb, S->x0 - 1, S->x0 + S->w - 1);
    xa = xa < 0 ? 0 : xa, xb = xb > 239 ? 239 : xb;
    for (y = S->y0; y < S->y0 + S->h; y++) {
        int32_t yy = y, g = al, p4;
        if (y >= Fy) {                                  /* (the floor's reflection: faint, gone 36 px down) */
            yy = 2 * Fy - 1 - y;
            g = al * (36 - (y - Fy)) / 160;
            if (g <= 0)
                continue;
        }
        if (yy < ya || yy > yb || xa > xb)
            continue;
        p4 = yy * 16 + 8;
        for (x = xa; x <= xb; x++)
            m->dep[0][x] = m->dep[1][x] = 0;
        for (i = 0; i < PLQ_NC; i++) {
            const plq_cap_t *q = &m->c[d][i];
            uint16_t *dp = m->dep[i >= PLQ_NB];
            int32_t uy = p4 - q->ay, x0, x1;
            if (yy < q->y0 || yy > q->y1)
                continue;
            x0 = q->x0 > xa ? q->x0 : xa, x1 = q->x1 < xb ? q->x1 : xb;
            for (x = x0; x <= x1; x++) {
                int32_t ux = x * 16 + 8 - q->ax, t = ux * q->dx + uy * q->dy, fq, ex, ey, d2, r, v;
                fq = !q->inv || t <= 0 ? 0 : ((t >> 6) * q->inv) >> 12;
                if (fq > 4096) {
                    if (i == PLQ_NB - 1)                /* (the skirt's hem: cut straight) */
                        continue;
                    fq = 4096;
                }
                ex = ux - (q->dx * fq >> 12), ey = uy - (q->dy * fq >> 12);
                d2 = ex * ex + ey * ey;
                r = q->r0 + ((q->r1 - q->r0) * fq >> 12);
                if (d2 >= r * r)
                    continue;
                v = (r * r - d2) * q->ir >> 16;          /* (how far inside: px x 256, true near the edge) */
                if (v > dp[x])
                    dp[x] = (uint16_t)(v > 4000 ? 4000 : v);
            }
        }
        for (x = xa; x <= xb; x++) {
            int32_t L = 0, k;
            for (k = 0; k < 2; k++) {
                int32_t v = m->dep[k][x], l = v < 256 ? v * rim >> 8 : v < 600 ? rim : v < 1100 ? rim - (v - 600) * (rim - fill) / 500 : fill;
                L = v && l > L ? l : L;
            }
            if (L)
                pl_px(S, x, y, L * g >> 8, col[0], col[1], col[2]);
        }
    }
}
static PL_SMALL void plq_draw(void *st, const umb_lctx_t *cx, const umb_api_t *a, const umb_strip_t *S)
{
    plq_t *m = st;
    uint32_t d, s, i;
    int32_t Fy = cx->bare ? 198 : 206, A = cx->k[5];
    pl_clear(S);
    for (d = 0; d < PLQ_ND; d++) {
        int32_t al = m->fa[d] >> 8;
        uint8_t col[3];
        if (al < 4)
            continue;
        for (s = 0; s < 2u && A; s++) {                 /* (the ribbons: newest first, fading, falling, fluttering) */
            const uint8_t *rc = pl_ncol(a, d + s * 3u);
            int32_t px = m->p[d][PLQ_LHA + s][0], py = m->p[d][PLQ_LHA + s][1],
                    b = pl_clamp(90 + A + m->lit[d][1 + s] / 2, 0, 255) * al >> 8;
            for (i = 0; i + 1u < m->rn; i++) {
                const int16_t *q = m->rib[d][s][(m->rw + PLQ_NR - i) % PLQ_NR];
                int32_t I = (int32_t)i + 1, wv = (pl_sine(a, cx->now * 0x400000u + (uint32_t)I * 0x14000000u) >> 9) * I / PLQ_NR,
                        qx = q[0] + wv, qy = q[1] + (I * I * 7 >> 4), bb = b * (PLQ_NR - I) / PLQ_NR;
                pl_line(S, px * 16, py * 16, qx * 16, qy * 16, bb, rc);
                pl_line(S, px * 16 + 128, py * 16 + 256, qx * 16 + 128, qy * 16 + 256, bb >> 1, rc);
                px = qx, py = qy;
            }
        }
        for (i = 0; i < 3u; i++)                        /* (the body pale: white touched by its colour) */
            col[i] = (uint8_t)((PL_WT[i] * 5 + pl_ncol(a, d)[i] * 3) >> 3);
        plq_fill(m, S, d, Fy, al, col);
        if (d != 1u) {                                  /* (the skirt: chiffon, light threads curving waist to hem, the legs
                                                         * seen through, a brighter hem; tinted by the dancer's colour) */
            uint8_t sc[3];
            for (i = 0; i < 3u; i++)
                sc[i] = (uint8_t)((col[i] + pl_ncol(a, d + 3u)[i]) >> 1);
            for (i = 0; i < PLQ_NK; i++) {              /* (a thread: a curve through its halfway point, and the hem on) */
                const int32_t(*k)[2] = m->sk[d][i];
                int32_t P[3][2], b = 110 * al >> 8, px, py, t;
                P[0][0] = m->sa[d][i][0] * 16, P[0][1] = m->sa[d][i][1] * 16;
                P[2][0] = k[1][0], P[2][1] = k[1][1];
                P[1][0] = 2 * k[0][0] - ((P[0][0] + P[2][0]) >> 1), P[1][1] = 2 * k[0][1] - ((P[0][1] + P[2][1]) >> 1);
                px = P[0][0], py = P[0][1];
                for (t = 1; t <= 4; t++) {              /* (four pieces, sheer at the waist, fuller below) */
                    int32_t u = 4 - t, qx = (P[0][0] * u * u + P[1][0] * 2 * u * t + P[2][0] * t * t) >> 4,
                            qy = (P[0][1] * u * u + P[1][1] * 2 * u * t + P[2][1] * t * t) >> 4;
                    pl_line(S, px, py, qx, qy, b * t >> 2, sc);
                    px = qx, py = qy;
                }
                if (i + 1u < PLQ_NK)                    /* (the hem) */
                    pl_line(S, k[1][0], k[1][1], m->sk[d][i + 1u][1][0], m->sk[d][i + 1u][1][1], 200 * al >> 8, sc);
            }
        }
        for (s = 0; s < 2u; s++)
            pl_splat(S, m->p[d][PLQ_LHA + s][0] * 16, m->p[d][PLQ_LHA + s][1] * 16, (160 + m->lit[d][1 + s] / 2) * al >> 8,
                     pl_ncol(a, d + s * 3u));
    }
}

#include "splash/troupe.h"                             /* (API 5) its still picture (.tools/harness/splash.mjs made it) */
UMB_PLATE_HEAD const umb_plate_t umb_plate_troupe = {
    UMB_PLATE_MAGIC, UMB_API, (uint16_t)sizeof(plq_t), "TROUPE",
    UMB_KIND_OVERLAY, UMB_MODE(UMB_DRAW_REPLACE) | UMB_MODE(UMB_DRAW_ADD), 8u, {0, 0, 0},
    {{"CAST", 50}, {"STEP", 50}, {"FLOW", 60}, {"LEAP", 50}, {"SPIN", 40}, {"RIBBON", 60}, {"IMPROV", 30}, {"CANON", 0}},
    plq_init, plq_note, plq_frame, plq_draw,
    {0, 0, 0, UMB_EL_NOTES, 0, 0, UMB_EL_NOTES, 0},
    "Dancers in silhouette: ribbons, turns", 3u, {0, 0, 0},
    /* SOLO: one dancer, long ribbons; KNOB 1 the ribbons, KNOB 2 the flow, the level the turns */
    {{"SOLO", {1, 51, 76, 41, 61, 86, 41, 1}, {UMB_LINK(UMB_SRC_KNOB(0), 5u), UMB_LINK(UMB_SRC_KNOB(1), 2u),
                                                UMB_LINK(UMB_SRC_LEVEL, 4u), UMB_LINK(UMB_SRC_LFO(0), 6u)}, {99, 99, 30, 40}},
     /* DUET: two, mirrored; KNOB 1 the canon, KNOB 2 the step, the level the ribbons, track 1's hits the hops */
     {"DUET", {51, 51, 61, 51, 31, 61, 21, 1}, {UMB_LINK(UMB_SRC_KNOB(0), 7u), UMB_LINK(UMB_SRC_KNOB(1), 1u),
                                                 UMB_LINK(UMB_SRC_LEVEL, 5u), UMB_LINK(UMB_SRC_HIT(0), 3u)}, {99, 99, 30, 40}},
     /* CANON: a trio, a step apart, a pose a beat; KNOB 1 the flow, KNOB 2 the turns */
     {"CANON", {100, 71, 41, 61, 51, 51, 11, 100}, {UMB_LINK(UMB_SRC_KNOB(0), 2u), UMB_LINK(UMB_SRC_KNOB(1), 4u),
                                                    UMB_LINK(UMB_SRC_LFO(0), 5u), UMB_LINK(UMB_SRC_LEVEL, 3u)}, {99, 99, 40, 30}}},
    55u, {0, 0, 0}, SPL_TROUPE};
