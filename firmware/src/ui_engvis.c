/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (JIANT 0.9.4 .. 0.9.9-snd5) Each engine draws a figure of its own where every other engine draws its being
 * (ui_being.c): EDIT's SOUND (ui_lzedit.c, on the left), HOME's BEING and ROUTES (ui_lzhome.c), TOTALITY's overlay
 * (ui_umbra_tot.c), LARGE's strip (ui_graph.c); DRUM-X's on EDIT > KIT and SOUND 1..4 (the whole panel), FM6's on
 * OPERATOR / OPERATOR 2 and OP ENV. Line art only, antialiased (ui_organic.c og_line), from small tables and the
 * synthesis state the UI already reads; nothing in the audio path. Each moves only with its sound: in silence it rests
 * (its time runs with the level).
 * (UMBRA LIVE) JIANT's creatures (the kit of beings, the cell, the tube worms, the vocal tract, the colony, the snail,
 * the caterpillar, the swarm, the spider, the jellyfish / totem, the bird / insect / mammal) are replaced by bodies on
 * an observatory plate: a dark disc, a lit limb, thin lines; no faces. They read the same engine values the creatures
 * did. What a figure is built of is the plates' deep blue; what lights takes the accent of the track the engine is on
 * (vx_col: six accents, one a track).
 *   DRUM-X  ORBEATRY      the bass the body at the centre, the drums its moons; a bass hit ripples outward
 *   ANALOG  DIAMOND RING  the oscillators two coronas round an eclipsed disc, the note a diamond on the limb
 *   WHEEL   GEMINIDS      nine lanes of comet pairs twisting round one another, a drawbar a lane
 *   VOICE   OCCULTATION   the jaw how far the moon slides off the sun, the vowel which way
 *   FM6     ASTERISM      the operators stars joined as the algorithm, the carriers over the horizon
 *   PHASE   WHORL         a planetary shell, its chambers at the bent phase
 *   LOFI    QUANTA        a disc with a stepped limb, the samples crossing it held
 *   NOISE   ACCRETION     the swarm as dust in a tilted ring round a dark body
 *   STRING  RETE          an astrolabe's net, a ray a voice, each the string itself
 *   ADD     COMET         a comet on a drifting loop, nine partials its nine streamers
 *   ANIMAL  CORVUS, MUSCA, CANIS   the three animals as constellations of the same twelve stars
 * Each takes a rectangle (vx_draw: EDIT's left panel, HOME's middle, or a whole panel) and scales to it.
 * State: vxs (in the pool: no global of the RAM region moves). MENU > ANIM OFF: they rest. */
#define VX_POOL __attribute__((section(".pool")))
static uint8_t vx_bare VX_POOL;                          /* (UMBRA LIVE) drawn over a plate (TOTALITY, HOME > BEING): DRUM-X's,
                                                         * PHASE's and LOFI's floors left out */
#define VX_CODE __attribute__((noinline))                 /* (each creature its own function: its size on its own) */
#define VX_SEG BTAPP_FN                                  /* (V10.48) LOFI's, ANIMAL's and STRING's figures (and more: vx_draw) in the Bluetooth
                                                         * segment (the app slot full): without it vx_draw says none (the
                                                         * being instead) */
#define VX_NE 9                                          /* the creatures: VOICE ANIMAL DRUM-X ANALOG FM6 WHEEL NOISE PHASE LOFI
                                                         * ((0.9.9-snd5) STRING ADD: PHASE's and WHEEL's slots, below) */
#define VX_NSW 56                                        /* NOISE's swarm (the mock-up's 84: its cost) */
typedef struct { int32_t cx, cy, s; } vxf_t;            /* a unit (Q4: 16) -> Q4 px: c + u * s / 256 */
static struct {
    uint32_t tm[VX_NE], fr[VX_NE];                       /* each one's time (it runs with its sound), the frame it ran */
    int32_t en[VX_NE];                                   /* its level now (0..256) */
    uint32_t mt[VX_NE], ms[VX_NE];                       /* (0.9.6) the mock-up's time (Q8 frames, 30 a second: with the sound), ms */
    uint32_t nage[VX_NE], nms[VX_NE];                    /* .. the newest note seen (voice_t.age), when it came (fm1_ms) */
    int16_t wv[256];                                     /* ANALOG's / PHASE's two cycles; LOFI's last 256 samples out (Q12) */
    int16_t sw[VX_NSW][5];                               /* NOISE: the swarm (x y vx vy Q8 units, its flare Q8) */
    uint32_t rs, swt;                                    /* .. its random state, the mock-up frames it has run */
    uint32_t rph[2], fhold;                              /* WHEEL: the rotors as last seen sounding; OP ENV's hold */
    int16_t bd[8];                                       /* LOFI: the byte's bits, slewed (0..256) */
    int16_t dal, frel;                                   /* lines drawn this much fainter (256 - alpha); OP ENV: the release from */
    int16_t art[11];                                     /* VOICE: the articulation, slewed (vx_voice) */
    int16_t w[3];                                        /* ANIMAL: bird insect mammal weights (0..256) */
    uint8_t age[8], last;                                /* DRUM-X: each lane's age seen; the lane struck last */
    uint32_t hitm[8], tl[8], kms;                        /* .. its last hit (fm1_ms), its being's time; the ms drawn */
    uint8_t kc;                                          /* .. drawing it (its colours: vx_col) */
    uint16_t rta;                                        /* (UMBRA LIVE) RETE: where its inner ring has swung to */
    uint8_t init;
} vxs VX_POOL;

/* (UMBRA LIVE) the bodies' colours: a heat 0..255 runs black, the plates' deep blue (what a figure is built of), the
 * accent of the track the engine is on (what lights), its white. Six accents, one a track: CORONA teal, AZURE, SODIUM
 * yellow, EMBER orange (3 and 4 the plates' warm tints), H-ALPHA rose, VIOLET. GREY and MONO: their greys */
static const uint8_t VK_HP[8] = {0, 26, 51, 82, 118, 154, 200, 255};
static const uint16_t VK_HC[8] = {0x13D4u, 0x2DFCu, 0x4B5Fu, 0x8B7Fu, 0xFA49u, 0xFC43u, 0xFE87u, 0xFFFFu};
static const uint16_t VX_ACC[6] = {0x7F1Au, 0x547Fu, 0xFE68u, 0xFBC3u, 0xFA4Du, 0xB3FFu};
static uint16_t vx_acc VX_POOL;
#define VX_BLUE 0x092Eu
static uint16_t vx_col(int32_t h)
{
    if (h < -1)                                          /* (-2 the theme's: the operator or the sound picked) */
        return T_THEME;
    if (h < 0)                                           /* (a muted sound: dim) */
        return ux.mono || settings.palette == UI_BW_INDEX ? ux_gray(ux_luma(T_DIM) >> 3) : T_DIM;
    if (!h)
        return 0;
    h = 66 + (clamp(h, 0, 255) * 189 >> 8);              /* (a line's dimmest still the deep blue) */
    if (ux.mono || settings.palette == UI_BW_INDEX)
        return ux_gray((uint32_t)h >> 3);
    return h <= 84 ? ux_mix(0, VX_BLUE, h * 100 / 84) : h <= 176 ? ux_mix(VX_BLUE, vx_acc, (h - 84) * 100 / 92) :
           ux_mix(vx_acc, 0xFFFFu, (h - 176) * 80 / 79);
}
/* (0.9.7) HOME's beings (ui_lzhome.c, variant A): the same ramp on JIANT, every other palette as the beings (bg_col) */
static uint16_t vk_heat(int32_t h)
{
    uint32_t k = 0;
    h = clamp(h, 0, 255);
    if (settings.palette != UI_JIANT_INDEX)
        return bg_col(h);
    while (k < 6u && h > VK_HP[k + 1u]) k++;
    return ux_mix(VK_HC[k], VK_HC[k + 1u], (h - VK_HP[k]) * 100 / (VK_HP[k + 1u] - VK_HP[k]));
}
static void vx_line(const vxf_t *f, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t h)
{
    int32_t X0 = f->cx + ((x0 * f->s) >> 8), Y0 = f->cy + ((y0 * f->s) >> 8);
    int32_t X1 = f->cx + ((x1 * f->s) >> 8), Y1 = f->cy + ((y1 * f->s) >> 8);
    og_line_a(X0, Y0, X1, Y1, vx_col(h), 256 - vxs.dal);
}
/* an ellipse (Q4 units), n sides, its top at angle a0 .. a1 (16-bit turns) */
static void vx_arc(const vxf_t *f, int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint32_t a0, uint32_t a1, uint32_t n,
                   int32_t h)
{
    uint32_t i;
    int32_t px = 0, py = 0;
    for (i = 0; i <= n; i++) {
        uint32_t a = a0 + (uint32_t)(((uint64_t)(a1 - a0) * i) / n);
        int32_t x = cx + ((rx * og_cos(a)) >> 15), y = cy + ((ry * og_sin(a)) >> 15);
        if (i) vx_line(f, px, py, x, y, h);
        px = x;
        py = y;
    }
}
static uint32_t vx_rnd(uint32_t *r) { *r ^= *r << 13; *r ^= *r >> 17; *r ^= *r << 5; return *r; }
/* the level of track t now (0..256: its part's peak, slewed: a hit at once, a quick fall), k: which drawing; its time
 * runs with it (once a frame) */
static int32_t vx_level(const track_t *t, uint32_t k, int32_t en_home)
{
    uint32_t dt = ui.frame - vxs.fr[k], dm = fm1_ms - vxs.ms[k];
    int32_t v;
    vx_acc = VX_ACC[(uint32_t)(t - trk) % 6u];           /* ((UMBRA LIVE) every figure asks its level first: its track's accent) */
    if (!dt)
        return vxs.en[k];
    if (dt > 8u) dt = 8u;
    vxs.fr[k] = ui.frame;
    vxs.ms[k] = fm1_ms;
    if (dm > 100u) dm = 100u;                            /* (a pause: no jump) */
    if (vxs.en[k])                                       /* (0.9.6) the mock-up's time: 30 frames a second while it sounds */
        vxs.mt[k] += dm * 7680u / 1000u;
    if (ANIM_OFF())
        return vxs.en[k] = 0;                            /* (ANIM OFF: at rest) */
    if (en_home >= 0)
        v = en_home;                                     /* (HOME: its own, eco_step's) */
    else {
        v = ((track_t *)t)->peak * 256 / 7000;
        ((track_t *)t)->peak = 0;
        v = v > 256 ? 256 : v;
        v = v > vxs.en[k] ? v : vxs.en[k] + (v - vxs.en[k] - 2) / 3;
    }
    v = v > 4 ? v : 0;                                   /* (HOME's level rests a little above 0: silence) */
    vxs.en[k] = v;
    if (v)
        vxs.tm[k] += dt * (uint32_t)(120 + v * 4);
    return v;
}
/* 256 * 2^(-ms / half), Q8 (a hit's fall) */
static int32_t vx_fall(uint32_t ms, uint32_t half)
{
    uint32_t n = ms / half;
    int32_t v;
    if (n > 8u) return 0;
    v = 256 >> n;
    return v - (int32_t)((uint32_t)v * (ms % half) / (2u * half));
}
/* a small dot at (x, y) (Q4 units), radius r (Q4 px: under 1.5 px a 2 x 2 dot) */
static void vx_dot(const vxf_t *f, int32_t x, int32_t y, int32_t r, int32_t h)
{
    uint16_t c = vx_col(h);
    int32_t X = f->cx + ((x * f->s) >> 8), Y = f->cy + ((y * f->s) >> 8);
    if (r >= 32) {
        og_node(X, Y, r >> 4, c);
        return;
    }
    X = (X - 8) >> 4;
    Y = (Y - 8) >> 4;
    for (r = 0; r < 4; r++)
        og_plot(X + (r & 1), Y + (r >> 1), c, 256 - og_dim);   /* (UMBRA LIVE: og_dim, TOTALITY's fade) */
}
/* (UMBRA LIVE) an ellipse turned by rot (16-bit turns) about its centre, from a0 to a1, in n lines */
static void vx_ell(const vxf_t *f, int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint32_t rot, uint32_t a0, uint32_t a1,
                   uint32_t n, int32_t h)
{
    uint32_t i;
    int32_t px = 0, py = 0, c = og_cos(rot), s = og_sin(rot);
    for (i = 0; i <= n; i++) {
        uint32_t a = a0 + (uint32_t)(((uint64_t)(a1 - a0) * i) / n);
        int32_t u = (rx * og_cos(a)) >> 15, v = (ry * og_sin(a)) >> 15, x = cx + ((u * c - v * s) >> 15), y = cy + ((u * s + v * c) >> 15);
        if (i) vx_line(f, px, py, x, y, h);
        px = x;
        py = y;
    }
}
/* a circle (Q4 units), its sides as many as its size asks (LITE, ECO: fewer) */
static void vx_circ(const vxf_t *f, int32_t x, int32_t y, int32_t r, int32_t h)
{
    uint32_t n = (uint32_t)clamp((r * f->s) >> 11, 8, 40);
    vx_arc(f, x, y, r, r, 0, 65536u, sys_load ? n * 2u / 3u : n, h);
}
/* a body at (x, y), radius r (Q4 units): black over what is behind it, its limb dim, lit (heat hl; under 0: as vx_col's)
 * on the side towards the angle a */
static void vx_orb(const vxf_t *f, int32_t x, int32_t y, int32_t r, uint32_t a, int32_t hl)
{
    uint32_t n = (uint32_t)clamp((r * f->s) >> 11, 6, 32);
    og_node(f->cx + ((x * f->s) >> 8), f->cy + ((y * f->s) >> 8), (r * f->s) >> 12, 0);
    vx_arc(f, x, y, r, r, 0, 65536u, n, hl < 0 ? hl : 44);
    vx_arc(f, x, y, r, r, a - 13000u, a + 13000u, n * 2u / 5u + 2u, hl);
}
/* a ray from r0 to r1 (Q4 units) out of (x, y) at the angle a */
static void vx_ray(const vxf_t *f, int32_t x, int32_t y, uint32_t a, int32_t r0, int32_t r1, int32_t h)
{
    int32_t c = og_cos(a), s = og_sin(a);
    vx_line(f, x + ((c * r0) >> 15), y + ((s * r0) >> 15), x + ((c * r1) >> 15), y + ((s * r1) >> 15), h);
}
static vxf_t vx_uf(const int32_t *A, int32_t k, int32_t dy);
static uint32_t vx_ang(uint32_t mt, int32_t r1000);
static int32_t vx_hash(uint32_t n);

/* ------------------------------------------------------------------------------------------------- VOICE --- */
/* (0.9.9, the mock-up's variant C, "Tracto vivo") the head in units (x right = back of the head, y down; the face left),
 * framed close (~80 x 62 about the mouth and the neck: vx_draw): its outline a ghost, the tract a tube */
/* the tract's fixed points (Q4 units): the roof from the teeth over the palate (the velum's two its own, then) and down
 * the pharynx's back wall to the glottis; the floor's from the epiglottis to the glottis; the air's way to the nose,
 * from the glottis up the pharynx, through the velum's port and the nasal cavity to the nostril (the branch: its 5th
 * point on) */
#define VH_N 22                                          /* (the walls' points, evenly along them) */

/* the articulation (vxs.art): 0 tongue x (front -), 1 its height (palate 19 .. low 32), 2 the tip's lift, 3 the jaw
 * (Q8 of its full opening), 4 lips round, 5 lips closed, 6 velum down, 7 lower lip to the teeth, 8 the constriction's
 * x, 9 its y (Q4 units), 10 how closed it is (0 open .. 256 shut); returns the phoneme's class (PC_VOW without WORDS) */
/* each place of articulation (sing.c PL_LAB .. PL_GLO): the tongue's body -x, y (units; 0: as the vowel), the tip's lift
 * (/ 2), the jaw (0: as the vowel), the lips (1 closed, 2 the lower on the teeth), the constriction's -x, y (units) */
static const uint8_t VH_PL[9][7] = {{0, 0, 0, 40, 1, 40, 30}, {0, 0, 0, 50, 2, 38, 28}, {26, 0, 150, 70, 0, 36, 27},
                                    {0, 0, 128, 60, 0, 31, 22}, {24, 21, 85, 60, 0, 27, 21}, {24, 19, 0, 50, 0, 23, 20},
                                    {12, 19, 0, 60, 0, 12, 21}, {9, 21, 0, 0, 0, 8, 24}, {0, 0, 0, 0, 0, 5, 51}};
static void vx_vowel_xy(int32_t f1, int32_t f2, int32_t *x, int32_t *y)
{
    *x = -11 * 16 - clamp(f2 - 800, 0, 1500) * 15 * 16 / 1500;      /* F2 800 .. 2300 Hz: back .. front */
    *y = 20 * 16 + clamp(f1 - 270, 0, 500) * 11 * 16 / 500;         /* F1 270 .. 770: high .. low */
}
static VX_CODE uint32_t vx_voice_target(const track_t *t, int16_t *a, int32_t *voi, int32_t *asp, int32_t *fri)
{
    int32_t f1, f2, x, y, i;
    uint32_t pl = PL_NONE, cls = PC_VOW, rnd = 0;
    if (t->p[P_VLANG]) {
        const sing_t *s = sing_view(t);
        uint32_t c = sing_phon(s);
        f1 = s->f[0];
        f2 = s->f[1];
        cls = PHON[c].cls;
        pl = PHON[c].place & 0x7Fu;
        rnd = PHON[c].place & PL_RND;
        if (cls == PC_ASP || cls == PC_SIL) pl = PL_NONE;
        *voi = s->gv > 0 ? s->gv >> 7 : 0;
        *asp = s->ga > 0 ? s->ga >> 7 : 0;
        *fri = s->gf > 0 ? s->gf >> 6 : 0;
        if (cls == PC_PLO && s->st == 0u) *fri = 0;
        if (cls == PC_SIL) *voi = 0;
    } else {                                             /* VOWEL -> VOWEL2 over TALK (as formant_render) */
        int32_t v0 = t->p[P_E0], v1 = t->p[P_E1], k = 0, j;
        uint32_t g = 0;
        for (j = 0; j < (int32_t)NVOICE; j++)
            if (t->v[j].active && (uint32_t)(t->v[j].s[7] & FMT_TALK_MASK) > g) g = (uint32_t)t->v[j].s[7] & FMT_TALK_MASK;
        if (t->p[P_E2]) { k = (int32_t)(g >> 9); k = (k * (65536 - k)) >> 15; }
        v0 += ((v1 - v0) * k) >> 15;
        {
            int32_t xx = clamp(v0, 0, 127) * 4 * 256 / 127, ii = xx >> 8, fr = xx & 255;
            if (ii >= 4) { ii = 3; fr = 256; }
            f1 = VOWEL_F[ii][0] + (((VOWEL_F[ii + 1][0] - VOWEL_F[ii][0]) * fr) >> 8);
            f2 = VOWEL_F[ii][1] + (((VOWEL_F[ii + 1][1] - VOWEL_F[ii][1]) * fr) >> 8);
            rnd = v0 > 80;
        }
        *voi = 256 - t->p[P_E5] * t->p[P_E5] / 64;
        *asp = t->p[P_E5] * 2;
        *fri = 0;
    }
    vx_vowel_xy(f1, f2, &x, &y);
    a[0] = (int16_t)x;
    a[1] = (int16_t)y;
    a[2] = 0;
    a[3] = (int16_t)clamp((f1 - 250) * 256 / 520, 0, 256);
    a[4] = (int16_t)(rnd ? 256 : 0);
    a[5] = 0;
    a[6] = (int16_t)(cls == PC_NAS ? 256 : 0);
    a[7] = 0;
    a[8] = (int16_t)(-30 * 16);                          /* (the constriction: the teeth by default) */
    a[9] = (int16_t)(27 * 16);
    a[10] = (int16_t)(pl == PL_NONE ? 0 : cls == PC_PLO || cls == PC_NAS || cls == PC_AFF ? 256 : cls == PC_FRI ? 190 : 110);
    if (pl != PL_NONE && pl <= PL_GLO) {                 /* the place: what touches what (VH_PL) */
        const uint8_t *g = VH_PL[pl - 1u];
        if (g[0]) a[0] = (int16_t)(-g[0] * 16);
        if (g[1]) a[1] = (int16_t)(g[1] * 16);
        a[2] = (int16_t)(g[2] * 2);
        if (g[3]) a[3] = g[3];
        a[5] = (int16_t)((g[4] & 1u) << 8);
        a[7] = (int16_t)((g[4] & 2u) << 7);
        a[8] = (int16_t)(-g[5] * 16);
        a[9] = (int16_t)(g[6] * 16);
    }
    for (i = 0; i < 11; i++)
        a[i] = (int16_t)clamp(a[i], -0x7000, 0x7000);
    return cls;
}
static uint32_t vx_age(const track_t *t, uint32_t k, int32_t *note);
static int32_t vx_isqrt(int32_t v) { int32_t r = 0, b; for (b = 1024; b; b >>= 1) if ((r + b) * (r + b) <= v) r += b; return r; }
/* (UMBRA LIVE) OCCULTATION: the moon over the sun. The jaw is how far the moon slides off (the lips closed: shut
 * again), the tongue's place front to back which way (the vowel), so the crescent left open is the mouth: hatched from
 * the limb inward. The voicing throws the corona's rays, frication and the bursts set beads at the crescent's horns */
static VX_CODE void vx_voice(const track_t *t, int32_t cx, int32_t cy, int32_t s, int32_t en_home)
{
    vxf_t f = {cx, cy, s};
    int16_t tg[11], *a = vxs.art;
    int32_t en = vx_level(t, 0, en_home), voi, asp, fri, i, d, mx, my, R = 400;
    uint32_t va, th, n = sys_load ? 40u : 64u;
    (void)vx_voice_target(t, tg, &voi, &asp, &fri);
    vx_age(t, 0, 0);
    if (!vxs.init) {
        vxs.init = 1;
        for (i = 0; i < 11; i++) a[i] = tg[i];
    }
    if (en || t->p[P_VLANG])                             /* (it moves as it sings; still in silence) */
        for (i = 0; i < 11; i++) a[i] = (int16_t)(a[i] + (tg[i] - a[i]) / 2);
    voi = voi * en >> 8;
    fri = (fri + asp / 2) * en >> 8;
    d = 32 + ((clamp(a[3], 0, 256) * (256 - clamp(a[5], 0, 256)) >> 8) * 256 >> 8);
    va = 16384u + (uint32_t)(clamp(a[0], -640, 640) * 12);
    mx = (og_cos(va) * d) >> 15;
    my = (og_sin(va) * d) >> 15;
    for (i = 0; i < (int32_t)n; i++) {                   /* the crescent */
        int32_t ex, ey, c, v;
        th = (uint32_t)i * 65536u / n;
        ex = og_cos(th);
        ey = og_sin(th);
        c = (ex * mx + ey * my) >> 15;                   /* (d cos: the moon's offset along this radius) */
        if (2 * R * c >= d * d)                          /* (this point of the limb is behind the moon) */
            continue;
        v = c + vx_isqrt(R * R - d * d + c * c);
        vx_ray(&f, 0, 0, th, v, R, 70 + (en * 80 >> 8) + (R - v) * 5 / 8);
        if (i % 4 == 0)
            vx_ray(&f, 0, 0, th, R + 24, R + 48 + (voi * 96 >> 8) * (128 + vx_hash((uint32_t)i + (vxs.mt[0] >> 9))) / 256, 50 + (voi * 150 >> 8));
    }
    vx_circ(&f, mx, my, R, 46);
    for (i = 0; fri > 24 && i < 6; i++) {                /* the beads */
        th = va + 32768u + (uint32_t)((i < 3 ? 1 : -1) * (11995 + vx_hash((uint32_t)i + (vxs.mt[0] >> 8)) * 20));
        vx_dot(&f, (og_cos(th) * R) >> 15, (og_sin(th) * R) >> 15, 32, 160 + (fri * 95 >> 8));
    }
}

/* ------------------------------------------------------------------------------------------------ ANIMAL --- */
/* (UMBRA LIVE) CORVUS, MUSCA, CANIS: the three animals as constellations of the same twelve stars (units, facing
 * left), each with its thirteen joins (a byte a join: two stars). The stars stand where the three are mixed (the weights
 * glide, ~0.6 s, to the MODE's animal, leaned by SIZE and stirred by CHAOS, as they always did), so they travel from one
 * figure to the next; a figure's joins as strong as its weight. The star that makes the call (the beak, the wing, the
 * nose) flares and rings with it */
static const int8_t VX_AN[3][24] = {
    {-38, -13, -28, -18, -17, -10, -12, 5, 5, 12, 30, 13, 42, 20, 7, -7, 19, -28, 31, -16, -5, 22, -10, 28},
    {-30, 0, -43, -18, -48, -8, -14, -4, 12, -1, 36, 4, 18, -12, 19, -25, 31, 19, -22, 17, -6, 18, 1, -14},
    {-41, -11, -29, -18, -22, -30, -16, -8, -13, 8, -17, 29, 13, -11, 31, -8, 46, -26, 35, 11, 31, 29, 11, 10}};
static const uint8_t VX_AJ[3][13] = {
    {0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x27, 0x75, 0x78, 0x89, 0x95, 0x4A, 0xAB},
    {0x01, 0x02, 0x03, 0x34, 0x45, 0x3B, 0xB6, 0x65, 0x47, 0x78, 0x39, 0x3A, 0x3A},
    {0x01, 0x12, 0x23, 0x13, 0x34, 0x45, 0x36, 0x67, 0x78, 0x79, 0x9A, 0x4B, 0xB7}};
static const uint8_t VA_CALL[3] = {0, 6, 0};
/* MODE -> the animal on the line insect 0 .. bird 128 .. mammal 256; the middle of its presets' SIZE */
static const uint8_t VA_POS[AN_NMODE] = {128, 0, 128, 0, 255}, VA_MID[AN_NMODE] = {64, 30, 50, 70, 64};   /* (.. its presets' SIZE) */
static VX_SEG void vx_animal(const track_t *t, int32_t cx, int32_t cy, int32_t s, int32_t en_home)
{
    vxf_t f = {cx, cy, s};
    const int16_t *p = t->p;
    uint32_t mode = an_mode(p), i, j, top;
    int32_t en = vx_level(t, 1, en_home), act = 0, w[3], m, tgt, pt[24];
    uint32_t tm = vxs.tm[1];
    for (i = 0; i < NVOICE; i++) {                       /* the call now: each voice's level (its syllable's, CMOS's gate) */
        const voice_t *v = &t->v[i];
        int32_t a;
        if (!v->active) continue;
        a = v->env_out > 0 ? v->env_out >> 7 : 0;
        if (mode == AN_WHSTL) a = a * clamp((int32_t)v->ph[1], 0, 24000) / 16000;
        if (a > act) act = a;
    }
    act = clamp(act, 0, 256) * clamp(en * 2, 0, 256) >> 8;
    tgt = VA_POS[mode];
    if (mode != AN_CMOS) {                               /* SIZE (CMOS: CAP there) past the mode's own animals' sizes */
        int32_t d = p[P_E3] - VA_MID[mode];
        d = d > 12 ? d - 12 : d < -12 ? d + 12 : 0;
        tgt += d * 6 / 5;
        if (mode != AN_GLOTT)
            tgt += (og_sin(tm >> 2) * p[P_E2]) >> 17;   /* (CHAOS stirs it) */
    }
    tgt = clamp(tgt, 0, 256);
    m = vxs.w[0];
    if (!vxs.w[1]) { vxs.w[1] = 1; m = tgt; }           /* (the first look: at once) */
    if (vxs.w[2] != (int16_t)ui.frame) {                 /* (the stars travel: once a frame) */
        m += (tgt - m) / 5 + (tgt > m) - (tgt < m);
        vxs.w[2] = (int16_t)ui.frame;
    }
    vxs.w[0] = (int16_t)m;
    w[0] = m >= 128 ? (256 - m) * 2 : m * 2;             /* bird */
    w[1] = m < 128 ? (128 - m) * 2 : 0;                  /* insect */
    w[2] = m > 128 ? (m - 128) * 2 : 0;                  /* mammal */
    if (w[0] > 256) w[0] = 256;
    top = w[1] > w[0] ? 1u : w[2] > w[0] ? 2u : 0u;
    for (i = 0; i < 24u; i++)                            /* the stars now: the three mixed (Q4 units) */
        pt[i] = (VX_AN[0][i] * w[0] + VX_AN[1][i] * w[1] + VX_AN[2][i] * w[2]) >> 4;
    for (j = 0; j < 3u; j++) {                           /* the joins */
        if (w[j] < 24) continue;
        vxs.dal = (int16_t)(256 - w[j]);
        for (i = 0; i < 13u; i++) {
            uint32_t a = VX_AJ[j][i] >> 4, b = VX_AJ[j][i] & 15u;
            vx_line(&f, pt[2 * a], pt[2 * a + 1], pt[2 * b], pt[2 * b + 1], 34);
        }
    }
    vxs.dal = 0;
    for (i = 0; i < 12u; i++) {                          /* the stars: a cross, a dot; the caller's flare */
        int32_t v = i == VA_CALL[top], h = v ? 150 + (act * 105 >> 8) : 90 + (vx_hash(i + 40u) * 80 >> 8) + ((og_sin((tm >> 1) + i * 12000u) * 30) >> 15);
        int32_t l = v ? 32 + (act * 72 >> 8) : 16 + (vx_hash(i * 5u) * 24 >> 8);
        vx_ray(&f, pt[2 * i], pt[2 * i + 1], 0, -l, l, h);
        vx_ray(&f, pt[2 * i], pt[2 * i + 1], 16384u, -l, l, h);
        vx_dot(&f, pt[2 * i], pt[2 * i + 1], v ? 32 : 16, h);
    }
    for (i = 0; act > 20 && i < 2u; i++) {               /* the call, ringing out of it */
        int32_t r = 64 + (int32_t)(((tm >> 3) + i * 72u) % 240u), x = pt[2 * VA_CALL[top]], y = pt[2 * VA_CALL[top] + 1];
        uint32_t a0 = top == 1u ? 54000u : 22000u;
        vx_arc(&f, x, y, r, r, a0, a0 + 22000u, 8, (act * 170 >> 8) * (320 - r) / 320);
    }
}

/* ------------------------------------------------------------------------------------------------ DRUM-X --- */
/* the kit as a colony of small beings (units ~124 x 90: x right, y down, the floor at 40), each drum one with eyes and
 * a mouth: the kick large in the middle, the snare flat on its stand, the toms on little legs, the hi-hat two shells,
 * the crash on a long stem, the cowbell a small head over the kick, the clap two little ones clapping */
enum { VK_KICK, VK_SNARE, VK_CLAP, VK_HATC, VK_HATO, VK_TOM, VK_RIM, VK_BELL };
/* a being's outline (ui_being.c bg_outline's, its lobes 3, the spikes' sharpness 4), squashed: at (x, y), radius r
 * (Q4 units), x stretched by ax / 256, then scaled sx sy / 256 about x and its foot (by: y's place after the squash) */
/* the kit at (cx, cy) Q4 px, s: a unit's px (Q8); sel: the sound edited (lit), -1 none */
static void vx_kit(const track_t *t, int32_t cx, int32_t cy, int32_t s, int32_t en_home, int32_t sel)
{
    vxf_t f = {cx, cy, s * 5 / 4};
    drum_lane_t *K = drum_kit_of(t);
    int32_t e[8], lt[8], en = vx_level(t, 2, en_home), i, k;
    uint32_t ag[8], now = fm1_ms, dt = now - vxs.kms;
    if (dt > 100u) dt = 100u;                            /* (a pause: no jump) */
    vxs.kms = now;
    vxs.kc = 1;
    for (i = 0; i < 8; i++) {                            /* each lane's hit now: its envelope (velocity, accent, decay) */
        int32_t a = K ? K[i].age : 255;
        lt[i] = dx_lane_muted((uint32_t)i) ? -1 : i == sel;
        e[i] = K && K[i].x.live && lt[i] >= 0 && !ANIM_OFF() ? clamp(K[i].x.ea >> 22, 0, 256) : 0;
        if (K && K[i].dm == 2u) e[i] = e[i] * 2 / 3;   /* (GHOST: a small squash) */
        if (K && a < vxs.age[i] && e[i]) {               /* a new hit (a FLAM's two, a ratchet's each) */
            vxs.hitm[i] = now;
            vxs.last = (uint8_t)i;
        }
        vxs.age[i] = (uint8_t)a;
        ag[i] = now - vxs.hitm[i] < 4000u ? now - vxs.hitm[i] : 4000u;
        if (en || e[i])                                  /* (its time runs with the sound: in silence it rests) */
            vxs.tl[i] += dt * (uint32_t)(60 * 256 + e[i] * 300) * 30u / 256000u;
    }
    {   /* (UMBRA LIVE) ORBEATRY: the bass the body at the centre, the drums its moons on four rings that rock slowly
         * from side to side. A bass hit sends a ripple out along the rings' plane, pushing each moon as it passes */
        static const uint16_t MA[5] = {9387, 35463, 19818, 53196, 43808};
        static const int8_t MS[5] = {17, -12, 9, 9, -6}, MO[5] = {0, 1, 2, 2, 3};
        uint32_t mt = vxs.mt[2], rot = (uint32_t)(-2300 + ((og_sin(vx_ang(mt, 14)) * 2100) >> 15));
        int32_t c = og_cos(rot), sn = og_sin(rot), rip = 160 + (int32_t)ag[VK_KICK] * 136 / 100, ra = vx_fall(ag[VK_KICK], 350), pass;
        int32_t lit[4];
#define VK_H(l, v) (lt[l] < 0 ? -1 : lt[l] > 0 ? -2 : 110 + (v) * 145 / 256)
        lit[0] = e[VK_SNARE] > e[VK_RIM] ? e[VK_SNARE] : e[VK_RIM];
        lit[1] = e[VK_HATC] > e[VK_HATO] ? e[VK_HATC] : e[VK_HATO];
        lit[2] = e[VK_TOM] > e[VK_CLAP] ? e[VK_TOM] : e[VK_CLAP];
        lit[3] = e[VK_BELL];
        for (i = 0; i < 4; i++)
            vx_ell(&f, 0, 0, 352 + i * 144, (352 + i * 144) * 2 / 5, rot, 0, 65536u, sys_load ? 24 : 40, 14 + lit[i] * 100 / 256);
        if (rip < 900 && ra > 8)
            vx_ell(&f, 0, 0, rip, rip * 2 / 5, rot, 0, 65536u, 32, ra * 190 / 256);
        for (pass = 0; pass < 2; pass++) {                   /* the moons behind the bass, the bass, the ones in front */
            for (i = 0; i < 5; i++) {
                uint32_t a = MA[i] + vx_ang(mt, MS[i]), la = a + rot + 32768u, b;
                int32_t rho = 352 + MO[i] * 144, d = rho - rip, x, y, u, v, ev, g;
                if ((og_sin(a) < 0) != !pass)
                    continue;
                d = d < 0 ? -d : d;
                rho += d < 128 ? ra * (128 - d) * 72 / 32768 : 0;
                u = (og_cos(a) * rho) >> 15;
                v = (og_sin(a) * rho * 2 / 5) >> 15;
                x = (u * c - v * sn) >> 15;
                y = (u * sn + v * c) >> 15;
                switch (i) {
                case 0:                                      /* the snare: a ringed moon, its ring thrown wide; RS a spark on it */
                    ev = e[VK_SNARE];
                    vx_orb(&f, x, y, 40, la, VK_H(VK_SNARE, ev));
                    vx_ell(&f, x, y, 72 + ev * 56 / 256, 21 + ev * 16 / 256, rot + 5215u, 0, 65536u, 16, lt[VK_SNARE] < 0 ? -1 : 90 + ev * 165 / 256);
                    if (e[VK_RIM] > 5 || lt[VK_RIM] > 0)
                        for (k = 0; k < 4; k++)
                            vx_ray(&f, x + 72, y, (uint32_t)k * 16384u + ag[VK_RIM] * 63u, 8, 24 + e[VK_RIM] * 48 / 256, VK_H(VK_RIM, e[VK_RIM]));
                    break;
                case 1:                                      /* the hats: two halves that clap shut (CH); OH throws them apart */
                    ev = lit[1];
                    g = 32 - (vx_fall(ag[VK_HATC], 50) * 32 >> 8) + e[VK_HATO] * 56 / 256;
                    b = la + 16384u;
                    for (k = 0; k < 2; k++, b += 32768u) {
                        int32_t hx = x - ((og_cos(b) * g) >> 15), hy = y - ((og_sin(b) * g) >> 15), hh = VK_H(k ? VK_HATO : VK_HATC, ev);
                        vx_arc(&f, hx, hy, 44, 44, b + 16384u, b + 49152u, 8, hh);
                        vx_ray(&f, hx, hy, b + 16384u, -44, 44, hh);
                    }
                    break;
                case 2:                                      /* the toms: a pair circling each other */
                    ev = e[VK_TOM];
                    for (k = 0; k < 2; k++) {
                        uint32_t q = vx_ang(mt, 73) + (uint32_t)k * 32768u;
                        int32_t px = x + ((og_cos(q) * 64) >> 15), py = y + ((og_sin(q) * 40) >> 15), r = 28 + k * 12, ek = k ? ev * 3 / 4 : ev;
                        vx_orb(&f, px, py, r, la, VK_H(VK_TOM, ek));
                        if (ek > 12) vx_circ(&f, px, py, r + 16 + (256 - ek) * 80 / 256, ek * 230 / 256);
                    }
                    break;
                case 3:                                      /* the clap: three moonlets scattered, then gathered */
                    ev = e[VK_CLAP];
                    for (k = 0; k < 3; k++) {
                        uint32_t q = vx_ang(mt, 47) + (uint32_t)k * 21845u;
                        g = 32 + ev * 80 / 256;
                        vx_orb(&f, x + ((og_cos(q) * g) >> 15), y + ((og_sin(q) * g) >> 15), 18, q + 32768u, VK_H(VK_CLAP, ev));
                    }
                    break;
                default:                                     /* BELL (the crash, the cowbell): a moon with rays, long to fade */
                    ev = e[VK_BELL];
                    for (k = 0; k < 10; k++)
                        vx_ray(&f, x, y, (uint32_t)k * 6554u + vx_ang(mt, 13), 48, 64 + ev * (k & 1 ? 56 : 104) / 256, lt[VK_BELL] < 0 ? -1 : 70 + ev * 185 / 256);
                    vx_orb(&f, x, y, 32, la, VK_H(VK_BELL, ev));
                }
            }
            if (!pass) {                                     /* the bass: the body they circle, swelling as it hits */
                vx_orb(&f, 0, 0, 136 + e[VK_KICK] * 32 / 256, 24576u, VK_H(VK_KICK, e[VK_KICK]));
                vx_circ(&f, 0, 0, 88, lt[VK_KICK] < 0 ? -1 : 50 + e[VK_KICK] * 120 / 256);
            }
        }
#undef VK_H
    }
    vxs.kc = 0;
}

/* ============================================================================ (0.9.6) the mock-up's family === */
/* the mock-up's frame (uFrame): its unit space 124 x 94 fitted in the rectangle A (x y w h px), k / 100 larger, lifted
 * dy (Q4 units) */
static vxf_t vx_uf(const int32_t *A, int32_t k, int32_t dy)
{
    vxf_t f;
    int32_t a = A[2] * 256 / 124, b = A[3] * 256 / 94;
    f.s = (a < b ? a : b) * k / 100;
    f.cx = (A[0] * 2 + A[2]) * 8;
    f.cy = (A[1] * 2 + A[3]) * 8 - ((dy * f.s) >> 8);
    return f;
}
/* the angle (16-bit turns) of t * r radians, t the mock-up's frames (mt, Q8), r = r1000 / 1000 */
static uint32_t vx_ang(uint32_t mt, int32_t r1000)
{
    return mt * (uint32_t)r1000 * 2670u >> 16;          /* (mod 2^32: an angle's 16 bits exact) */
}
static int32_t vx_hash(uint32_t n) { n = (n ^ 61u) ^ (n >> 16); n *= 9u; n ^= n >> 4; n *= 668265261u; return (int32_t)((n ^ (n >> 15)) & 255u); }
/* ms since track t's newest note (k: which creature saw it; 4000 at most); *note: that note */
static uint32_t vx_age(const track_t *t, uint32_t k, int32_t *note)
{
    uint32_t i, a = 0, n = 60;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].age > a) { a = t->v[i].age; n = t->v[i].note; }
    if (a > vxs.nage[k])
        vxs.nms[k] = fm1_ms;
    vxs.nage[k] = a;
    if (note) *note = (int32_t)n;
    return fm1_ms - vxs.nms[k] < 4000u ? fm1_ms - vxs.nms[k] : 4000u;
}
/* a polyline of n points (Q4 units: x y pairs), closed or not */
static void vx_poly(const vxf_t *f, const int32_t *p, uint32_t n, int closed, int32_t h)
{
    uint32_t i;
    for (i = 1; i < n; i++)
        vx_line(f, p[2u * i - 2u], p[2u * i - 1u], p[2u * i], p[2u * i + 1u], h);
    if (closed)
        vx_line(f, p[2u * n - 2u], p[2u * n - 1u], p[0], p[1], h);
}
/* a line between two points in polar form about c (cx cy, x y scales / 256): angles (16-bit turns), radii (Q4 units) */
static void vx_pl(const vxf_t *f, const int32_t *c, uint32_t a0, int32_t r0, uint32_t a1, int32_t r1, int32_t h)
{
    vx_line(f, c[0] + (((og_cos(a0) * r0) >> 15) * c[2] >> 8), c[1] + (((og_sin(a0) * r0) >> 15) * c[3] >> 8),
            c[0] + (((og_cos(a1) * r1) >> 15) * c[2] >> 8), c[1] + (((og_sin(a1) * r1) >> 15) * c[3] >> 8), h);
}
/* a ring (Q4 units), its sides as many as its size asks */
static void vx_ring(const vxf_t *f, int32_t x, int32_t y, int32_t rx, int32_t ry, int32_t h)
{
    uint32_t n = (uint32_t)clamp(((rx + ry) * f->s) >> 12, 8, 28);
    vx_arc(f, x, y, rx, ry, 0, 65536u, n, h);
}
/* text s centred on (x, its capitals' top y) Q4 px */
static uint8_t vx_small;                                 /* (V10.8) LARGE's strip: the creatures small, no labels */
static void vx_text(int32_t x, int32_t y, const char *s, uint16_t c)
{
    if (vx_small) return;
    cv_text((x >> 4) - text_w(&AF_X, s) / 2, (y >> 4) - AF_X_CAP_Y, &AF_X, s, c);
}

/* ------------------------------------------------------------------------------------------- ANALOG --- */
/* 2^(l / 256), Q12 */
static int32_t vx_x2(int32_t l)
{
    int32_t i = l >> 8, f = l & 255, m = 4096 + ((f * (2690 + ((f * 1407) >> 8))) >> 8);
    return i >= 0 ? m << i : i > -31 ? m >> -i : 0;
}
/* a wave of ANALOG's at phase q (16 bit): SAW SQR TRI SIN */
static int32_t vx_anb(uint32_t w, uint32_t q)
{
    q &= 65535u;
    w &= 3u;
    return w == 0u ? 32767 - (int32_t)q : w == 1u ? (q < 32768u ? 32767 : -32767) :
           w == 2u ? (q < 32768u ? (int32_t)q * 2 - 32768 : 98303 - (int32_t)q * 2) : og_sin(q);
}
/* two cycles (64 samples each) of ANALOG's oscillators, noise, the filter (CUT on the note, the envelope opening it a
 * little, RES) and DRIVE (tanh), as the mock-up's anTick: into vxs.wv, normalised (Q12) */
static VX_CODE void vx_an_wave(const track_t *t, int32_t en, uint32_t mt, int32_t note)
{
    const int16_t *p = t->p;
    uint32_t w = (uint32_t)p[P_E0] & 7u, i, rep, r = 4096, rs = (mt >> 8) * 2654435761u + 1u;
    int32_t det = p[P_E1], mix = (p[P_E2] * 516 >> 8), nz = (p[P_E3] * 361 >> 8), mo = clamp(t->mo_cur / 127, 0, 256), k;
    int32_t pos = (int32_t)w * 256 + mo * 3, fr = pos & 255, i0 = pos >> 8, g, a1, a2, a3, ic1 = 0, ic2 = 0, mx = 1, dg;
    int32_t r5 = 256 + det * 2022 / 127 * (256 - mo / 2) / 256 + mo * 3, d3 = det * 218 * (256 + 4 * mo) >> 8;
    uint32_t dr = mt * (uint32_t)det * 2642u >> 16, inc2, wd;
    for (k = p[P_E7]; k > 0; k--) r = (r * 4340u) >> 12;   /* INT: osc 2 semitones away, DETUNE cents */
    for (k = p[P_E7]; k < 0; k++) r = (r * 3866u) >> 12;
    r += r * (uint32_t)det / 1200u;
    inc2 = 1024u * r >> 12;
    wd = (uint32_t)(32768 - (mo * 27525 >> 8) - ((og_sin(vx_ang(mt, 50)) * 3932) >> 15));   /* PWM's width */
    g = vx_x2(-992 - 1536 + ((p[P_E4] * 4851) >> 8) - (note - 69) * 256 / 12 + (en * 82 >> 8));   /* CUT / the note / 64 */
    g = clamp(g, 4, 1843);
    g = (g * 12868) >> 12;                               /* (tan (pi fc)) */
    g = clamp(g + (((((g * g) >> 12) * g) >> 12) / 3), 1, 12288);
    k = 8192 - (p[P_E5] * 16017 >> 8);
    a1 = (4096 * 4096) / (4096 + ((g * (g + k)) >> 12));
    a2 = (g * a1) >> 12;
    a3 = (g * a2) >> 12;
    dg = 256 + (p[P_E6] * 3612 >> 8);
    for (rep = 0; rep < 2u; rep++)                       /* (a cycle for the filter to settle, then the two) */
        for (i = rep ? 0u : 64u; i < 128u; i++) {
            uint32_t P = i * 1024u, q = P & 65535u, q2 = (i * inc2 + dr) & 65535u;
            int32_t s, v1, v2, v3, u;
            if (w < 4u) {
                int32_t x1 = (vx_anb((uint32_t)i0, q) * (256 - fr) + vx_anb((uint32_t)i0 + 1u, q) * fr) >> 8;
                int32_t x2 = (vx_anb((uint32_t)i0, q2) * (256 - fr) + vx_anb((uint32_t)i0 + 1u, q2) * fr) >> 8;
                s = (x1 * (256 - mix / 2) + x2 * (mix / 2)) >> 8;
            } else if (w == 4u)
                s = ((q < wd ? 32767 : -32767) * (256 - mix / 2) + (q2 < wd ? 32767 : -32767) * (mix / 2)) >> 8;
            else if (w == 5u)
                s = (vx_anb(0, q) * (256 - mix) + vx_anb(0, (q * (uint32_t)r5) >> 8) * mix) >> 8;
            else if (w == 6u)
                s = (vx_anb(0, q) * (256 - mix) + ((vx_anb(0, q) * og_sin((P * (uint32_t)r5) >> 8)) >> 15) * mix) >> 8;
            else
                s = (vx_anb(0, P) + vx_anb(0, P + (((P >> 4) * (uint32_t)d3) >> 12) + dr) +
                     vx_anb(0, P - (((P >> 4) * (uint32_t)d3) >> 12) - dr * 13u / 10u)) / 3;
            rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5;
            s = (s + ((((int32_t)(rs >> 16) - 32768) * nz) >> 7)) >> 3;   /* (NOISE) Q12 */
            v3 = s - ic2;
            v1 = (a1 * ic1 + a2 * v3) >> 12;
            v2 = ic2 + ((a2 * ic1 + a3 * v3) >> 12);
            ic1 = clamp(2 * v1 - ic1, -150000, 150000);
            ic2 = clamp(2 * v2 - ic2, -150000, 150000);
            if (!rep) continue;
            u = clamp(((v2 * dg) >> 8) >> 4, -768, 768);   /* DRIVE: tanh, Q8 */
            u = u * (1769472 + u * u) / (1769472 + 9 * u * u);
            vxs.wv[i] = (int16_t)(u << 4);
            mx = u << 4 > mx ? u << 4 : -(u << 4) > mx ? -(u << 4) : mx;
        }
    if (mx > 205)
        for (i = 0; i < 128u; i++) vxs.wv[i] = (int16_t)(vxs.wv[i] * 4096 / mx);
}
/* (UMBRA LIVE) DIAMOND RING: an eclipsed disc, the oscillators its two coronas (three cycles of the wave round the
 * limb, the second drifting against the first as DETUNE beats), the filter roughening them (the wave is the filtered
 * one) and raising the prominences (DRIVE taller), RES a dashed halo the envelope draws in, NOISE rays past the corona;
 * each note sets the diamond on the limb at its pitch */
static VX_CODE void vx_analog(const track_t *t, const int32_t *A, int32_t en_home)
{
    const int16_t *p = t->p;
    int32_t en = vx_level(t, 3, en_home), note, j, i;
    uint32_t mt = vxs.mt[3], age = vx_age(t, 3, &note), a0 = vx_ang(mt, 17), n = sys_load ? 64u : 96u;
    vxf_t f = vx_uf(A, 100, 0);
    int32_t dr = (p[P_E6] * 516 >> 8), res = (p[P_E5] * 516 >> 8), nz = (p[P_E3] * 516 >> 8), mix = (p[P_E2] * 516 >> 8);
    int32_t am = 80 + en * 220 / 256, ev = vx_fall(age, 200), c[4] = {0, 0, 256, 256}, l, rr;
    vx_an_wave(t, en, mt, note);
    for (j = mix > 12 ? 1 : 0; j >= 0; j--) {           /* the coronas: the second oscillator's the fainter, behind */
        uint32_t pa = 0, off = j ? vx_ang(mt, 10 + (p[P_E1] * 202 >> 8)) >> 9 : 0u;
        int32_t pr = 0;
        for (i = 0; i <= (int32_t)n; i++) {
            uint32_t th = (uint32_t)i % n * 65536u / n + (j ? 0u - a0 / 2u : a0);
            int32_t w = vxs.wv[((uint32_t)i % n * 192u / n + off) & 127u], r = (j ? 424 : 392) + (((w + 4096) * am >> 13) * (j ? 140 : 256) >> 8);
            if (i) vx_pl(&f, c, pa, pr, th, r, j ? 30 + (en * 50 >> 8) : 80 + (en * 110 >> 8) + (w * 60 >> 12));
            pa = th; pr = r;
        }
    }
    rr = 704 - (en * 80 >> 8);
    for (i = 0; res > 10 && i < 24; i++)                 /* RES: the halo */
        vx_arc(&f, 0, 0, rr, rr, (uint32_t)i * 2731u + a0 / 4u, (uint32_t)i * 2731u + 1365u + a0 / 4u, 2, 20 + (res * 120 >> 8) + (en * 40 >> 8));
    for (i = 0; nz > 8 && i < 24; i++)                   /* NOISE: rays past the corona */
        vx_ray(&f, 0, 0, (uint32_t)i * 2731u, 420 + am, 436 + am + nz * vx_hash((uint32_t)i + (mt >> 8)) / 512, 50 + (nz * 77 >> 8));
    for (i = 0; i < 3; i++) {                            /* the prominences: loops off the limb */
        uint32_t a = (uint32_t)i * 22938u + 6258u + (uint32_t)((og_sin(vx_ang(mt, 10) + (uint32_t)i * 10430u) * 2607) >> 15);
        vx_ell(&f, (og_cos(a) * 356) >> 15, (og_sin(a) * 356) >> 15, 24 + (en * 72 >> 8) + (dr * 60 >> 8), 30, a, 49152u, 81920u, 8, 60 + (en * 120 >> 8));
    }
    og_node(f.cx, f.cy, (352 * f.s) >> 12, 0);           /* the disc, its limb */
    vx_circ(&f, 0, 0, 344, 50);
    l = 24 + (ev * 128 >> 8);                            /* the diamond */
    {
        uint32_t a = (uint32_t)(note % 12) * 5461u - 22938u;
        int32_t bx = (og_cos(a) * 360) >> 15, by = (og_sin(a) * 360) >> 15;
        for (i = 0; i < 4; i++)
            vx_ray(&f, bx, by, (uint32_t)i * 8192u, i & 1 ? -l * 2 / 5 : -l, i & 1 ? l * 2 / 5 : l, (i & 1 ? 90 : 120) + (ev * 130 >> 8));
        vx_dot(&f, bx, by, 32 + (ev >> 4), 200 + (ev * 55 >> 8));
    }
}

/* ---------------------------------------------------------------------------------------------- FM6 --- */
#ifndef VX_FM6_HOOK
#define VX_FM6_HOOK(k, x, y, r) ((void)0)               /* ui_render.c (host tests): each bicho as drawn (its lint) */
#endif
static const uint8_t FM6_CELL[32][6];                    /* (ui_graph.c) */
static void fm6_routes(uint32_t a, uint8_t *m);
static uint32_t fm6_fb_op(uint32_t a);
static uint32_t fm6_car_ops(uint32_t a);
static uint32_t fm6_alg_of(const track_t *t);
static void fop_freq(const uint8_t *op, char *s);
typedef struct {
    const uint8_t *p;                                    /* the patch as it plays (the macros on it) */
    int32_t env[6], amp[6], rat[6], rad[6], x[6], y[6];  /* per operator 1..6: its envelope now, level x envelope (0..256),
                                                          * ratio (Q8), radius, place (Q4 units) */
    uint32_t alg, car, fbo, st[6];                       /* the algorithm, its carriers, the feedback operator; each stage */
    uint8_t m[6];
} vxfm_t;
/* the operators of track t now: the sounding notes' envelopes (the loudest), each one's level, ratio, size, place */
static void vx_fm_ops(const track_t *t, vxfm_t *o)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK, k, v, hi = 0, top = 0;
    o->p = fm6_eff[tr].ok ? fm6_eff[tr].p : fm6_patch[tr];
    o->alg = fm6_alg_of(t);
    o->car = fm6_car_ops(o->alg);
    o->fbo = fm6_fb_op(o->alg);
    fm6_routes(o->alg, o->m);
    for (k = 0; k < 6u; k++) {
        const uint8_t *op = o->p + (5u - k) * FP_OP;
        int32_t e = 0, ol = op[FP_OL] % 100u, a = ol * 256 / 99, c = op[FP_FC] & 31;
        o->st[k] = 4;
        for (v = 0; v < FM6_POLY; v++) {                 /* (msfa's level: scaleout units 0..127, the stage's L 0..99) */
            const fm6_env_t *n = &fm6_note[tr][v].env[5u - k];
            int32_t s = (((n->level >> 16) - n->outlevel + 4256) / 32 - 28) * 256 / 99;
            if (!fm6_note[tr][v].live || !t->v[v].active) continue;
            if (s > e || o->st[k] == 4u) { o->st[k] = n->ix | (n->down ? 0u : 8u); }
            e = s > e ? s : e;
        }
        o->env[k] = clamp(e, 0, 256);
        o->amp[k] = ol ? (a * ((a * 154 >> 8) + 102) / 256) * o->env[k] / 256 : 0;   /* ((ol / 99) ^ 1.6) x env */
        o->rat[k] = op[FP_MODE] ? 256 : (c ? c * 256 : 128) * (100 + op[FP_FF] % 100u) / 100;
        o->rad[k] = 120 * (141 + 141 * ol / 99) / 256 * (200 + (o->env[k] * 82 >> 8)) / 256;
        hi = (FM6_CELL[o->alg][k] & 7u) > hi ? FM6_CELL[o->alg][k] & 7u : hi;
        top = (uint32_t)(FM6_CELL[o->alg][k] >> 4) > top ? (uint32_t)FM6_CELL[o->alg][k] >> 4 : top;
    }
    for (k = 0; k < 6u; k++) {                           /* (fmPos: columns 26 apart, the carriers on the floor 38) */
        o->x[k] = ((int32_t)(FM6_CELL[o->alg][k] & 7u) * 2 - (int32_t)hi) * 13 * 16;
        o->y[k] = 28 * 16 - (int32_t)(FM6_CELL[o->alg][k] >> 4) * (top >= 3u ? 324 : 352);
    }
}
/* (UMBRA LIVE) a star at (x, y): sp 1..3 its spikes (4, 6, 8: an operator's ratio), len their length (Q4 units) */
static void vx_star(const vxf_t *f, int32_t x, int32_t y, int32_t sp, int32_t len, int32_t h)
{
    uint32_t n = 2u + 2u * (uint32_t)sp, i;
    for (i = 0; i < n; i++)
        vx_ray(f, x, y, i * 65536u / n + (n > 4u ? 2731u : 0u), 16, 16 + (i & 1u ? len * 5 / 9 : len), h);
    vx_dot(f, x, y, 32, h < 0 ? h : h + 40);
}
/* the operator's number by its star */
static void vx_fm_num(const vxf_t *f, const vxfm_t *o, uint32_t k, int32_t x, int32_t y, int sel)
{
    char n[2] = {(char)('1' + k), 0};
    vx_text(f->cx + ((x * f->s) >> 8), f->cy + ((y * f->s) >> 8) - 64, n,
            sel ? T_THEME : o->p[(5u - k) * FP_OP + FP_OL] % 100u ? T_MID : T_DIM);
}
/* (UMBRA LIVE) ASTERISM: the six operators as stars joined as the algorithm wires them (FM6_CELL, fm6_routes), the
 * carriers over the horizon (the output), each standing on its own line down to it, every modulator wheeling slowly
 * about its place. A star's spikes are its RATIO, their length and its heat its LEVEL times its envelope now; pulses
 * run down the joins as fast as the modulator's ratio; FB a ring round the operator that feeds itself. sel the operator
 * picked (or 6): in the theme's colour; op2: OPERATOR 2 (its DETUNE a faint ring) */
static VX_CODE void vx_fm6(const track_t *t, const int32_t *A, int32_t en_home, uint32_t sel, int op2)
{
    vxfm_t o;
    vxf_t f = vx_uf(A, 100, 0);
    int32_t en = vx_level(t, 4, en_home), x0 = 9999, x1 = -9999, y0 = 9999, s, l = 9999, r = -9999, k, j, fb;
    uint32_t mt = vxs.mt[4];
    (void)vx_age(t, 4, 0);
    vx_fm_ops(t, &o);
    for (k = 0; k < 6; k++) {
        x0 = o.x[k] < x0 ? o.x[k] : x0; x1 = o.x[k] > x1 ? o.x[k] : x1; y0 = o.y[k] < y0 ? o.y[k] : y0;
        if (o.car >> k & 1u) { l = o.x[k] < l ? o.x[k] : l; r = o.x[k] > r ? o.x[k] : r; }
    }
    x0 -= 16 * 16; x1 += 18 * 16; y0 -= 13 * 16;         /* (fitTo: the asterism fills the frame, at most 1.75 x) */
    s = f.s * 124 * 16 / (x1 - x0);
    j = f.s * 94 * 16 / (47 * 16 - y0);
    s = s < j ? s : j;
    s = s < f.s * 7 / 4 ? s : f.s * 7 / 4;
    f.cx -= ((x0 + x1) / 2 * s) >> 8;
    f.cy -= ((y0 + 47 * 16) / 2 * s) >> 8;
    f.s = s;
    for (k = 0; k < 6; k++)                              /* each modulator wheels slowly about its place */
        if (!(o.car >> k & 1u)) {
            uint32_t a = vx_ang(mt, 7 + k) + (uint32_t)k * 10430u;
            o.x[k] += (og_cos(a) * 104) >> 15;
            o.y[k] += (og_sin(a) * 56) >> 15;
        }
    l = (l < -160 ? l : -160) - 128;                    /* the output: the horizon */
    r = (r > 160 ? r : 160) + 192;
    vx_line(&f, l, 38 * 16, r, 38 * 16, 36 + en / 2);
    for (j = l + 64; j < r; j += 128)
        vx_line(&f, j, 38 * 16, j, 38 * 16 + 28, 36);
    for (k = 0; k < 6; k++)                              /* the joins; pulses as fast as the ratio */
        for (j = 0; j < 6; j++)
            if (o.m[k] >> j & 1u) {
                int32_t a = o.amp[j], n, mr = o.rat[j] < 14 * 256 ? o.rat[j] : 14 * 256;
                vx_line(&f, o.x[j], o.y[j], o.x[k], o.y[k], 31 + (a * 154 >> 8));
                for (n = 0; a > 8 && n < 2; n++) {
                    int32_t d = (int32_t)(((mt * (uint32_t)(256 + mr / 4) * 786u >> 16) + (uint32_t)n * 128u) & 255u);
                    vx_dot(&f, o.x[j] + (o.x[k] - o.x[j]) * d / 256, o.y[j] + (o.y[k] - o.y[j]) * d / 256, (f.s * 7 >> 8), 154 + (a * 102 >> 8));
                }
            }
    for (k = 0; k < 6; k++)                              /* the carriers: down to the horizon */
        if (o.car >> k & 1u)
            vx_line(&f, o.x[k], o.y[k] + 48, o.x[k], 38 * 16, 34 + (o.amp[k] * 77 >> 8));
    fb = clamp(o.p[FP_FB] + t->p[P_E1], 0, 7);           /* FB: a ring, open at one side */
    vx_arc(&f, o.x[o.fbo], o.y[o.fbo], 96 + fb * 8, 96 + fb * 8, 6000u, 56000u, 12, fb ? 64 + fb * 154 / 7 : 20);
    for (k = 0; k < 6; k++) {
        const uint8_t *op = o.p + (5u - (uint32_t)k) * FP_OP;
        int32_t a = o.amp[k], tw = 240 + ((og_sin(vx_ang(mt, 300) + (uint32_t)k * 20000u) * 16) >> 15);
        if (op2 && (uint32_t)k == sel && op[FP_DET] != 7u)
            vx_circ(&f, o.x[k], o.y[k], 64 + (op[FP_DET] > 7u ? op[FP_DET] - 7 : 7 - op[FP_DET]) * 8, 60);
        if (!(op[FP_OL] % 100u))                         /* (silent: a cold point) */
            vx_dot(&f, o.x[k], o.y[k], 16, (uint32_t)k == sel ? -2 : 30);
        else
            vx_star(&f, o.x[k], o.y[k], clamp((o.rat[k] + 128) >> 8, 1, 3), (40 + (a * 160 >> 8)) * tw >> 8, (uint32_t)k == sel ? -2 : 90 + (a * 165 >> 8));
        if (A[2] >= 80)                                  /* (V10.8: small, LARGE's strip, the numbers left out) */
            vx_fm_num(&f, &o, (uint32_t)k, o.x[k] - 144, o.y[k] - 96, (uint32_t)k == sel);
        VX_FM6_HOOK(k, f.cx + ((o.x[k] * f.s) >> 8), f.cy + ((o.y[k] * f.s) >> 8), (145 * f.s) >> 8);   /* (the largest it gets) */
    }
}
/* OPERATOR / OPERATOR 2: the colony on the whole panel, the operator picked lit; "ALG 05" over it on the left, its
 * frequency on the right ("OP3 x1.50") */
static VX_CODE void vx_fm6_page(const track_t *t, uint32_t sel, int op2)
{
    static const int32_t A[4] = {4, 14, 232, 124};
    const uint8_t *pt = fm6_patch[(t - trk) % NTRK];
    uint32_t alg = fm6_alg_of(t);
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
    char b[16] = {'A', 'L', 'G', ' ', (char)('0' + (alg + 1u) / 10u), (char)('0' + (alg + 1u) % 10u), 0};
    char fq[16] = {'O', 'P', (char)('1' + sel), ' ', 0};
    cv_oy = 0;
    vx_fm6(t, A, -1, sel, op2);
    if (!t->p[P_E0])
        str_cpy(b + 6, " PAT", 5);
    cv_text(10, 4, &AF_S, b, hot == P_E0 || hot == P_E7 ? T_ACCENT : T_MID);
    fop_freq(pt + (5u - sel) * FP_OP, fq + 4);
    cv_text_r(228, 4, &AF_S, fq, T_TEXT, T_SURF);
}
/* OP ENV: the picked operator's envelope as the patch has it (from L4 to L1 at R1, L2 at R2, L3 at R3, held while the key
 * is down: a dashed line where it is let go, back to L4 at R4; a stage as wide as its rate is slow), the stage KNOB 3 / 4
 * edit in the accent, the operator's star riding the curve where it is now; "OP3" top left, the stage's R and L right */
static VX_CODE void vx_fm6env(const track_t *t)
{
    static const char *const STG[4] = {"ATK", "DEC", "SUS", "REL"};
    static const int32_t A[4] = {4, 12, 232, 126};
    const uint8_t *op = fop_bytes((uint32_t)(t - trk));
    vxfm_t o;
    vxf_t f = vx_uf(A, 100, 0);
    int32_t x[6], y[6], k, i, en = vx_level(t, 4, -1), px, py, e, a, r, u, sg = fop_stg & 3u;
    uint32_t age = vx_age(t, 4, 0), ko = fop_op % 6u, st;
    char b[24] = {'O', 'P', (char)('1' + ko), 0};
    (void)en;
    cv_oy = 0;
    vxs.kc = 1;
    vx_fm_ops(t, &o);
#define VFY(l) (22 * 16 - (int32_t)(l) * 42 * 16 / 99)
#define VFW(r) ((4 + (99 - (int32_t)(r)) * (99 - (int32_t)(r)) * 44 / 9801) * 112 * 16 / 216)
    x[0] = -56 * 16;
    y[0] = VFY(op[FP_L1 + 3]);
    for (k = 0; k < 3; k++) {
        x[k + 1] = x[k] + VFW(op[FP_R1 + k]);
        y[k + 1] = VFY(op[FP_L1 + k]);
    }
    x[5] = 56 * 16;
    x[4] = x[5] - VFW(op[FP_R1 + 3]);
    y[4] = y[3];
    y[5] = y[0];
    vx_line(&f, -60 * 16, 26 * 16, 60 * 16, 26 * 16, 26);
    for (i = -20 * 16; i < 22 * 16; i += 48)             /* the key let go */
        vx_line(&f, x[4], i, x[4], i + 22, 31);
    for (k = 0; k < 5; k++)                              /* (3: the hold, no stage of its own) */
        for (i = 0; i < 10; i++) {
            int32_t on = k != 3 && (k < 3 ? k : 3) == sg, dx = x[k + 1] - x[k], dy = y[k + 1] - y[k];
            int32_t ya = y[k] + dy * i / 10, d;
            for (d = 0; d <= on * 12; d += 12)
                vx_line(&f, x[k] + dx * i / 10, ya + d, x[k] + dx * (i + 1) / 10, y[k] + dy * (i + 1) / 10 + d,
                        on ? -2 : 38 + ((22 * 16 - ya) * 154 / (42 * 16)));
        }
    k = sg < 3 ? sg + 1 : 5;
    vx_ring(&f, x[k], y[k], 32, 32, -2);
    vx_ring(&f, x[k], y[k], 40, 40, -2);
    e = o.env[ko];                                       /* where the operator is now: on its stage, as far as its level */
    st = o.st[ko];                                       /* came (held: along the hold, 1.5 s across) */
    py = VFY(e * 99 / 256);
    k = (st & 7u) >= 4u ? 5 : st == 3u ? 3 : st < 3u ? (int32_t)st : 4;
    if (k == 3) {
        if (!vxs.fhold) vxs.fhold = fm1_ms | 1u;
        u = clamp((int32_t)(fm1_ms - vxs.fhold) * 256 / 1500, 0, 256);
    } else {
        int32_t l0 = k == 4 ? vxs.frel * 99 / 256 : op[FP_L1 + ((k + 3) & 3)], l1 = op[FP_L1 + (k < 4 ? k : 3)];
        vxs.fhold = 0;
        u = l1 != l0 ? clamp((e * 99 / 256 - l0) * 256 / (l1 - l0), 0, 256) : 256;
    }
    if (k == 5) { k = 4; u = 256; }
    if (k != 4) vxs.frel = (int16_t)e;                   /* (the level its release sets out from) */
    px = x[k] + (x[k + 1] - x[k]) * u / 256;
    a = o.amp[ko];
    r = 64 + (96 * e >> 8);
    (void)a; (void)age;
    vx_star(&f, px, py - r, clamp((o.rat[ko] + 128) >> 8, 1, 3), r, -2);   /* ((UMBRA LIVE) the operator's star) */
    k = sg < 3 ? (x[sg] + x[sg + 1]) / 2 : (x[4] + x[5]) / 2;
    k = clamp(k, -49 * 16, 49 * 16);                     /* (the stage's name under it, inside the curve's ends) */
    vx_text(f.cx + ((k * f.s) >> 8),
            f.cy + ((31 * 16 * f.s) >> 8) - 112, STG[sg], T_THEME);
#undef VFY
#undef VFW
    vxs.kc = 0;
    cv_text(10, 4, &AF_S, b, T_MID);
    str_cpy(b, STG[sg], sizeof b);
    str_cpy(b + str_len(b), "  R ", 5);
    fmt_int(b + str_len(b), op[FP_R1 + sg]);
    str_cpy(b + str_len(b), "  L ", 5);
    fmt_int(b + str_len(b), op[FP_L1 + sg]);
    cv_text_r(228, 4, &AF_S, b, T_TEXT, T_SURF);
}

/* -------------------------------------------------------------------------------------------- WHEEL --- */
/* (UMBRA LIVE) GEMINIDS: nine lanes, one a drawbar (the digits under them), each a pair of comets twisting round one
 * another, their tail as long as the bar is pulled and widening as it trails. The twist turns only while the organ
 * sounds (the figure's own time). The rotor sways the heads (its real phase), PERC flares the lane it strikes, DUST
 * motes drift */
static VX_SEG void vx_wheel(const track_t *t, const int32_t *A, int32_t en_home)
{
    const int16_t *p = t->p;
    const drw_trk_t *T = &drw_t[drw_part(t)];
    vxf_t f = vx_uf(A, 100, 0);
    uint32_t mt = vxs.mt[5], i, j, gph;
    int32_t en = vx_level(t, 5, en_home), perc = 0, pk, dep;
    uint32_t pc = (uint32_t)p[P_E4] % 7u;
    (void)vx_age(t, 5, 0);
    gph = vxs.tm[5] * 3u;
    if (en) { vxs.rph[0] = T->rph[0]; vxs.rph[1] = T->rph[1]; }   /* (the rotors as they turn while it sounds) */
    dep = (clamp(p[P_WRDEP], 0, 127) + 1) * (T->rwet >> 7) >> 7;  /* (0..256: DEPTH, the rotor on) */
    pk = pc ? 3 + (int32_t)((pc - 1u) & 1u) : -1;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && drw_v[drw_part(t)][i].perc > perc) perc = drw_v[drw_part(t)][i].perc;
    perc >>= 7;                                          /* (PERC's envelope, 0..256) */
    for (i = 0; i < 9u; i++) {
        int32_t l = drw_level(p, i), X = (-48 + (int32_t)i * 12) * 16, pp = (int32_t)i == pk ? perc : 0;
        int32_t len = l * 160 + (pp * 96 >> 8), top = 36 * 16 - len, hl = 70 + (en * 150 >> 8) + (pp * 35 >> 8);
        int32_t bend = (((og_sin((vxs.rph[1] >> 16) - i * 5734u) * dep) >> 15) * 40) >> 8, sg;
        uint32_t n = (uint32_t)clamp(len / 56, 2, sys_load ? 12 : 22);
        char d[2] = {(char)('0' + l), 0};
        vx_text(f.cx + ((X * f.s) >> 8), f.cy + ((36 * 16 * f.s) >> 8) + 64, d, l ? T_MID : T_DIM);
        if (!l) {
            vx_dot(&f, X, 36 * 16, 16, 40);
            continue;
        }
        for (sg = -1; sg <= 1; sg += 2) {
            int32_t qx = 0, qy = 0;
            for (j = 0; j <= n; j++) {
                int32_t u = (int32_t)(j * 256u / n), uu = (u * (512 - u)) >> 8;
                uint32_t ph = gph + (gph >> 4) * i + i * 17749u - (uint32_t)((len * 113 * uu) >> 8);
                int32_t x = X + ((bend * (256 - u)) >> 8) + ((og_sin(ph) * (12 + (u * 48 >> 8))) >> 15) * sg, y = top + ((len * u) >> 8);
                if (j)
                    vx_line(&f, qx, qy, x, y, (((((256 - u) * (256 - u)) >> 8) * hl >> 8) + 6) >> (og_cos(ph) * sg > 0 ? 0 : 1));
                else
                    vx_dot(&f, x, y, 28, 170 + (en * 85 >> 8));
                qx = x; qy = y;
            }
        }
    }
    if (T->dust)                                         /* DUST: motes drifting */
        for (i = 0; i < (uint32_t)((T->dust * 40 >> 8)); i++)
            vx_dot(&f, (vx_hash(i * 9u + 1u) * 124 / 256 - 62) * 16,
                   (int32_t)(((uint32_t)vx_hash(i * 3u + 2u) * 6u + (mt >> 4) * (uint32_t)(77 + vx_hash(i) / 2) / 256u) % 1504u) - 752, 16, 90);
}

/* -------------------------------------------------------------------------------------------- NOISE --- */
/* the swarm's step (the mock-up's, a frame of 30 a second): COLOR their walk, FREQ their swirl and its ring, PEAK
 * how tight, DUST's impulses flare them (DENSE) */
static VX_CODE void vx_nz_step(const track_t *t, int32_t en)
{
    const int16_t *p = t->p;
    int32_t col = (p[P_E1] * 516 >> 8), peak = (p[P_E3] * 516 >> 8), rp = (12 * 256 + p[P_E2] * 26 * 256 / 127), dens = (p[P_E5] * 516 >> 8);
    int32_t sp = (4 + (p[P_E2] * 60 >> 8)) * (256 + en) / 256, jit = (256 - col) * 358 / 256 * (77 + en) / 256 + (col * 90 >> 8);
    int32_t pull = 3 + (peak * 15 >> 8), damp = 230 + (col * 20 >> 8), i;
    uint32_t r = vxs.rs ? vxs.rs : 2463534242u;
    for (i = 0; i < VX_NSW; i++) {
        int16_t *q = vxs.sw[i];
        int32_t x = q[0], y = q[1], ax = x < 0 ? -x : x, ay = y < 0 ? -y : y, rr = (ax > ay ? ax + ay * 3 / 8 : ay + ax * 3 / 8) + 1;
        int32_t vx = q[2], vy = q[3], k = (rr - rp) * pull / 256;
        vx = (vx * damp >> 8) + ((int32_t)(vx_rnd(&r) & 255u) - 128) * jit / 256 - y * sp / 1000 - x * k / rr;
        vy = (vy * damp >> 8) + (((int32_t)(vx_rnd(&r) & 255u) - 128) * jit / 256) * 4 / 5 + x * sp / 1000 - y * k / rr;
        if ((p[P_E0] & 3) == NZ_DUST && (int32_t)(vx_rnd(&r) & 65535u) < dens * dens / 50 * (51 + en) / 256) {
            q[4] = 256;                                  /* (an impulse: it flares, it jumps) */
            vx += ((int32_t)(vx_rnd(&r) & 255u) - 128) * 6;
            vy += ((int32_t)(vx_rnd(&r) & 255u) - 128) * 6;
        }
        vx -= x / 250;
        vy -= y / 166;
        q[4] = (int16_t)((q[4] * 217 >> 8));
        x += vx / 2;
        y += vy / 2;
        if (x > 62 * 256 || x < -62 * 256 || y > 46 * 256 || y < -46 * 256) { x = (x * 77 >> 8); y = (y * 77 >> 8); vx = vy = 0; }
        q[0] = (int16_t)clamp(x, -32000, 32000); q[1] = (int16_t)clamp(y, -32000, 32000);
        q[2] = (int16_t)clamp(vx, -32000, 32000); q[3] = (int16_t)clamp(vy, -32000, 32000);
    }
    vxs.rs = r;
}
/* (UMBRA LIVE) ACCRETION: the swarm as dust in a tilted ring round a dark body (the same walk: COLOR, FREQ's swirl,
 * PEAK the ring they keep, DUST's impulses flaring them), the half behind the body dimmer; LFSR / META: the register
 * sixteen marks round the ring, a mark a bit */
static void vx_tilt(int32_t x, int32_t y, int32_t *X, int32_t *Y)   /* a place on the ring's plane (Q4 units) as seen */
{
    y = y * 108 >> 8;
    *X = (x * 30777 + y * 11247) >> 15;                  /* (turned -0.35 rad) */
    *Y = (y * 30777 - x * 11247) >> 15;
}
static VX_SEG void vx_noise(const track_t *t, const int32_t *A, int32_t en_home)
{
    const int16_t *p = t->p;
    vxf_t f = vx_uf(A, 100, 0);
    int32_t en = vx_level(t, 6, en_home), md = p[P_E0] & 3, peak = (p[P_E3] * 516 >> 8), i, n, pass, x, y;
    uint32_t mt = vxs.mt[6], reg = 0;
    (void)vx_age(t, 6, 0);
    if (!vxs.sw[0][0] && !vxs.sw[0][1])                  /* (scattered, the first time) */
        for (i = 0; i < VX_NSW; i++) {
            vxs.sw[i][0] = (int16_t)((vx_hash((uint32_t)i * 13u + 1u) - 128) * 80);
            vxs.sw[i][1] = (int16_t)((vx_hash((uint32_t)i * 29u + 7u) - 128) * 60);
        }
    for (n = 0; vxs.swt < mt >> 8 && n < 6; n++, vxs.swt++)   /* (its frames as its time ran: still in silence) */
        vx_nz_step(t, en);
    vxs.swt = mt >> 8;
    if (peak > 51) {                                     /* PEAK: the ring they keep */
        int32_t rp = (12 * 16 + p[P_E2] * 26 * 16 / 127) * 3 / 2;
        vx_ell(&f, 0, 0, rp, rp * 108 >> 8, 61886u, 0, 65536u, 28, 20 + (peak * 90 >> 8));
    }
    if (md >= NZ_LFSR) {
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].active) { reg = t->v[i].ph[1]; break; }
        for (i = 0; i < 16; i++) {
            int32_t on = md == NZ_LFSR ? (int32_t)(reg >> i & 1u) : vx_hash(((uint32_t)i + (mt >> 9)) % 16u * 7u + (uint32_t)(p[P_E5] >> 4)) > 128;
            int32_t c = og_cos((uint32_t)i * 4096u), sn = og_sin((uint32_t)i * 4096u), x1, y1;
            vx_tilt((c * 800) >> 15, (sn * 800) >> 15, &x, &y);
            vx_tilt((c * 900) >> 15, (sn * 900) >> 15, &x1, &y1);
            if (on) vx_line(&f, x, y, x1, y1, 128 + (en * 102 >> 8));
            else vx_dot(&f, x, y, 16, 46);
        }
    }
    for (pass = 0; pass < 2; pass++) {                   /* the dust behind the body, the body, the dust in front */
        for (i = md >= NZ_LFSR ? VX_NSW / 2 : 0; i < VX_NSW; i++) {
            const int16_t *q = vxs.sw[i];
            int32_t vx = q[2], vy = q[3], av = vx < 0 ? -vx : vx, bv = vy < 0 ? -vy : vy, v = av > bv ? av : bv;
            int32_t c = q[4] > 51 ? 218 + (q[4] * 38 >> 8) : clamp(50 + (v * 31 >> 8) + (en * 70 >> 8), 0, 190);
            if ((q[1] < 0) != !pass)
                continue;
            vx_tilt(q[0] * 3 / 32, q[1] * 3 / 32, &x, &y);
            vx_dot(&f, x, y, q[4] > 51 || i % 5 == 0 ? 36 : 16, pass ? c : c * 3 / 5);
        }
        if (!pass)
            vx_orb(&f, 0, 0, 200, 26214u, 100 + (en * 150 >> 8));
    }
}

/* -------------------------------------------------------------------------------------------- PHASE --- */
/* the bent phase of PHASE's wave w at dcw (0..65535), as eng_phase.c's pd_wave bends it (the mock-up's pdPhase) */
static uint32_t vx_pd(uint32_t w, uint32_t dcw, uint32_t ph)
{
    uint32_t x, pk;
    switch (w) {
    case 0: x = 32768u + ((dcw * 32767u) >> 16); return ph < x ? ph * 32768u / x : 32768u + (ph - x) * 32767u / (65535u - x);
    case 1:
        x = 32768u - ((dcw * 31120u) >> 16);
        return ph < x ? ph * 32768u / x : ph < 32768u ? 32768u : ph < 32768u + x ? 32768u + (ph - 32768u) * 32767u / x : 65535u;
    case 2: x = 65535u - ((dcw * 63487u) >> 16); return ph < x ? ph * 65535u / x : 65535u;
    case 3: x = 65535u - ((dcw * 49151u) >> 16); return ph < x ? ph * 65535u / x : (ph - x) * 65535u / (65535u - x);
    case 4:
        x = 65535u - ((dcw * 32767u) >> 16);
        pk = (x * (32768u + ((dcw * 29491u) >> 16))) >> 16;
        pk = pk ? pk : 1u;
        return ph >= x ? 65535u : ph < pk ? ph * 32768u / pk : 32768u + (ph - pk) * 32767u / (x - pk);
    default: return (ph + ((ph * ((dcw * 44000u) >> 16)) >> 16) * 7u) & 65535u;
    }
}
/* .. its sample (Q15): -cos of the bent phase; RSAW RTRI RTRP windowed */
static int32_t vx_pdw(uint32_t w, uint32_t dcw, uint32_t ph)
{
    int32_t c = -og_cos(vx_pd(w, dcw, ph)), win;
    if (w < 5u) return c;
    win = w == 5u ? 65535 - (int32_t)ph : w == 6u ? (ph < 32768u ? (int32_t)ph * 2 : (65535 - (int32_t)ph) * 2) :
          (ph < 16384u ? (int32_t)ph * 4 : ph > 49152u ? (65535 - (int32_t)ph) * 4 : 65535);
    return (c * (win > 65535 ? 65535 : win)) >> 16;
}
/* (UMBRA LIVE) WHORL: a planetary shell. A spiral of chambers (its radius 1.72 x a turn, four and a half turns), the
 * chamber walls at the bent phase (WAVE / WAVE2 at DCW: crowding where it rushes) and bowed by DCW, the outer whorl
 * ribbed by the wave itself (FB frays it, SUB a faint whorl outside), a chamber lit by each note, small bodies winding out
 * along it, the second line (DETUNE, RING) a companion circling the mouth. It turns slowly while it sounds */
static int32_t vx_shr(int32_t u) { return 64 * vx_x2(u * 200 >> 16) >> 12; }   /* its radius (Q4 units) at u (Q16 turns) */
static VX_SEG void vx_phase(const track_t *t, const int32_t *A, int32_t en_home)
{
    const int16_t *p = t->p;
    vxf_t f = vx_uf(A, 100, 0);
    int32_t en = vx_level(t, 7, en_home), i, mx = 1, prev = 0, note, pr = 0, r, ev, wq;
    uint32_t mt = vxs.mt[7], age = vx_age(t, 7, &note), w1 = (uint32_t)p[P_E0] & 7u, w2 = p[P_E1] ? (uint32_t)(p[P_E1] - 1) & 7u : w1;
    int32_t fb = (p[P_E7] * 516 >> 8), sub = (p[P_E6] * 516 >> 8), det = p[P_E4], ring = p[P_E5], line2 = det || ring;
    int32_t dcw = clamp((p[P_E2] * 516 >> 8) + en * p[P_E3] * 179 / 127 / 256 - (p[P_E3] * 77 >> 8), 0, 255) * 256;
    int32_t alt = p[P_E1] && (mt >> 8) / 20u % 2u, c[4] = {-64, 48, 256, 256};
    uint32_t dr = mt * (uint32_t)det * 6606u >> 16, rot = 0u - vx_ang(mt, 5), pa = 0, a, bow = 900u + (uint32_t)(dcw >> 8) * 6u;
    const int32_t T = 294912, T0 = 26214;
    for (i = 0; i < 128; i++) {                          /* two cycles out: WAVE then WAVE2, FB, the second line, SUB */
        uint32_t w = i >= 64 ? w2 : w1, ph = ((uint32_t)(i & 63) * 1024u + (uint32_t)((prev * fb >> 9)) + 65536u) & 65535u;
        int32_t s = vx_pdw(w, (uint32_t)dcw, ph);
        prev = s;
        if (line2) {
            int32_t s2 = vx_pdw(w, (uint32_t)dcw, ((uint32_t)i * 1024u * (3000u + (uint32_t)det) / 3000u + dr) & 65535u);
            s = ring ? (s * s2) >> 15 : (s + s2) / 2;
        }
        s += (((og_sin((uint32_t)i * 512u) * sub) >> 8) * 154) >> 8;
        vxs.wv[i] = (int16_t)clamp(s >> 3, -32000, 32000);
        mx = s >> 3 > mx ? s >> 3 : -(s >> 3) > mx ? -(s >> 3) : mx;
    }
    if (mx > 205)
        for (i = 0; i < 128; i++) vxs.wv[i] = (int16_t)(vxs.wv[i] * 4096 / mx);
    ev = vx_fall(age, 280);
    wq = 3 + ((dcw >> 8) * 13 >> 8);
    for (i = 0; i <= 98; i++) {                          /* the shell */
        int32_t u = T0 + (T - T0) / 98 * i, o = u > T - 65536 ? (u - (T - 65536)) >> 8 : 0;
        r = vx_shr(u);
        r += (((r * wq >> 8) * vxs.wv[(((uint32_t)o * 13u) >> 2 & 63u) + (alt ? 64u : 0u)]) >> 12) * o / 256 +
             ((vx_hash((uint32_t)i + (mt >> 8)) - 128) * fb >> 8) * 20 / 256 * o / 256;
        a = (uint32_t)u + rot;
        if (i) vx_pl(&f, c, pa, pr, a, r, 36 + i / 2 + (o * en >> 8) * 150 / 256);
        if (sub > 13 && o) {
            vxs.dal = (int16_t)(256 - sub * 3 / 4);
            vx_pl(&f, c, pa, pr * 277 >> 8, a, r * 277 >> 8, 90);
            vxs.dal = 0;
        }
        pa = a; pr = r;
    }
    for (i = 0; i < 34; i++) {                           /* its chambers */
        int32_t u = T - (i / 12) * 65536 - (int32_t)vx_pd(alt ? w2 : w1, (uint32_t)dcw, (uint32_t)(i % 12) * 5461u);
        int32_t r0, r1;
        if (u - 65536 < T0)
            continue;
        r0 = vx_shr(u);
        r1 = vx_shr(u - 65536);
        a = (uint32_t)u + rot;
        r = i ? 22 + (34 - i) * 30 / 34 + (i == 1 + note % 16 ? ev * 215 >> 8 : 0) : 150 + (en * 105 >> 8);
        vx_pl(&f, c, a, r0, a - bow, (r0 + r1) / 2, r);
        vx_pl(&f, c, a - bow, (r0 + r1) / 2, a, r1, r);
    }
    for (i = 0; i < (line2 ? 5 : 4); i++) {              /* the bodies winding out; the companion at the mouth */
        int32_t u = i < 4 ? 78643 + (int32_t)(((((mt * 15u) >> 4) + (uint32_t)i * 16384u) & 65535u) * (uint32_t)(T - 78643) >> 16) : T;
        r = vx_shr(u);
        a = (uint32_t)u + rot;
        if (i == 4) { r += 120; a += vx_ang(mt, (det * 202 >> 8) + 20); }
        vx_orb(&f, c[0] + ((og_cos(a) * r) >> 15), c[1] + ((og_sin(a) * r) >> 15), i < 4 ? 12 + r / 24 : 28, a + 32768u, 150 + (en * 105 >> 8));
    }
    vx_orb(&f, c[0], c[1], 24, rot, 140);
}

/* --------------------------------------------------------------------------------------------- LOFI --- */
/* (UMBRA LIVE) QUANTA: a disc with a stepped limb, CRUSH the step; the last samples out cross it held a step at a
 * time (the scope's: nothing more from the audio); inside, eight sides of an octagon the byte's bits (BYTE: lit with
 * their bit; FLOAT and the other waves: with the wave's level there) */
static VX_SEG void vx_lofi(const track_t *t, const int32_t *A, int32_t en_home)
{
    const int16_t *p = t->p;
    int32_t fl = p[P_E1] != RW_BYTE, new_fr = vxs.fr[8] != ui.frame;
    int32_t en = vx_level(t, 8, en_home), i, j, pk = 6000, bc[8], q = 32 + clamp(p[P_E3], 0, 127) * 88 / 127, R = 640, pw = 0, y, w, px, py;
    uint32_t w0 = scope_w - 256u, byte = 0;
    vxf_t f = vx_uf(A, 100, 0);
    (void)vx_age(t, 8, 0);
    for (i = 0; i < 256; i++) {                          /* the last 256 samples out (the scope's): their peak */
        int32_t v = scope_buf[(w0 + (uint32_t)i) & (SCOPE_N - 1u)];
        v = v < 0 ? -v : v;
        pk = v > pk ? v : pk;
    }
    for (i = 0, j = (4096 << 15) / pk; i < 256; i++)
        vxs.wv[i] = (int16_t)((scope_buf[(w0 + (uint32_t)i) & (SCOPE_N - 1u)] * j) >> 15);
    byte = (uint32_t)(vxs.wv[255] / 32 + 128) & 255u;   /* the byte now: BYTE's held sample, else the output's level */
    for (i = 0, j = 0; i < NVOICE; i++)
        if (!fl && t->v[i].active && t->v[i].age >= (uint32_t)j) { j = (int32_t)t->v[i].age; byte = (uint32_t)((t->v[i].s[0] + 32640) >> 8) & 255u; }
    for (i = 0; i < 8; i++) {                            /* (its bits slewed, once a frame) */
        if (new_fr)
            vxs.bd[i] = (int16_t)(vxs.bd[i] + (((byte >> i & 1u) ? en : 0) - vxs.bd[i]) * 141 / 256);
        bc[i] = clamp((vxs.bd[i] - 46) * 410 / 256, 0, 256);
    }
    for (y = -((R + q - 1) / q) * q; y < R; y += q) {    /* the stepped limb: a row of steps at a time, both sides */
        int32_t m = y < 0 ? -(y + q) : y;
        m = m < 0 ? 0 : m;
        w = m < R ? (vx_isqrt(R * R - m * m) + q / 2) / q * q : 0;
        for (j = -1; j <= 1; j += 2) {
            vx_line(&f, j * pw, y, j * w, y, 70 + (en * 150 >> 8));
            vx_line(&f, j * w, y, j * w, y + q, 70 + (en * 150 >> 8));
        }
        pw = w;
    }
    vx_line(&f, -pw, y, pw, y, 70 + (en * 150 >> 8));
    for (i = 0; i < 8; i++) {                            /* the byte: the octagon's sides */
        int32_t v = fl ? clamp(vxs.wv[i * 30 + 6] < 0 ? -vxs.wv[i * 30 + 6] : vxs.wv[i * 30 + 6], 0, 4096) * 218 / 4096 + 13 : bc[i];
        vx_arc(&f, 0, 0, 352, 352, (uint32_t)i * 8192u + 600u, (uint32_t)i * 8192u + 7592u, 1, 34 + (v * 200 >> 8));
    }
    px = -928;
    py = 0;
    for (i = 0; px < 928; i++) {                         /* the samples, held */
        int32_t v = (vxs.wv[(i * q >> 3) & 255] * 176) >> 12;
        vx_line(&f, px, py, px, v, 110 + (en * 145 >> 8));
        vx_line(&f, px, v, px + q, v, 110 + (en * 145 >> 8));
        px += q;
        py = v;
    }
}

/* (0.9.9-snd5) STRING and ADD keep their time, level and newest note in vxs's slots of PHASE (7) and WHEEL (5): one creature
 * is drawn at a time (EDIT's, HOME's), so a slot shared costs a glance at most when the track changes, and no byte of the
 * pool (at its floor) */
#define VX_K_STRING 7u
#define VX_K_ADD 5u
/* ---------------------------------------------------------------------------------------- (snd5) STRING --- */
/* (UMBRA LIVE) RETE: an astrolabe's net. The rim with its degrees; a ray a voice, at its note's place round the
 * octave, each ray the string itself (its delay line, one period of what the loop holds now: the real standing wave,
 * ringing down as it decays); a small body rides the newest ray at POS; the inner ring, off-centre as an ecliptic,
 * swings round to the newest note and swells with it; the hub as large as SIZE, lit by BODY; BRIGHT warms the lines */
static VX_SEG void vx_string(const track_t *t, const int32_t *A, int32_t en_home)
{
    const int16_t *p = t->p;
    vxf_t f = vx_uf(A, 100, 0);
    int32_t en = vx_level(t, VX_K_STRING, en_home), silk = 30 + p[P_E2] / 2, j, q[2 * 21], ux = 0, uy = -32768, R = 672, nw = -1, ev, er, ex, ey;
    uint32_t age = vx_age(t, VX_K_STRING, 0), i, a, th = 49152u, na = 0;
    ev = vx_fall(age, 400);
    vx_circ(&f, 0, 0, R, 52);
    vx_circ(&f, 0, 0, R + 40, 30);
    for (i = 0; i < 36u; i++)
        vx_ray(&f, 0, 0, i * 1820u, i % 3u ? R - 16 : R - 40, R, i % 3u ? 40 : 70);
    for (i = 0; i < NVOICE; i++)                         /* (the newest voice: the body's ray) */
        if (t->v[i].active && t->v[i].age >= na) { na = t->v[i].age; nw = (int32_t)i; }
    for (i = 0; i < NVOICE; i++) {                       /* the rays */
        const voice_t *v = &t->v[i];
        uint32_t n, w, d;
        const int16_t *b;
        int32_t pk = 0, rx, ry;
        if (!v->active) continue;
        b = str_line(t, v);
        a = 49152u + (uint32_t)(v->note % 12u) * 5461u;
        rx = og_cos(a);
        ry = og_sin(a);
        n = str_period(pitch_inc((uint32_t)clamp(v->pitch_cur, 0, 2047)), &d) >> 8;
        n = n < 2u ? 2u : n;
        w = v->ph[1] % ST_SL;
        if ((int32_t)i == nw) { th = a; ux = rx; uy = ry; }
        for (j = 0; j <= 20; j++) {
            int32_t sm = j && j < 20 ? b[(w + ST_SL - n + (uint32_t)j * n / 21u) % ST_SL] : 0, dd = sm * 7 >> 11, rr = 56 + (R - 56) * j / 20;
            pk = sm > pk ? sm : -sm > pk ? -sm : pk;
            q[2 * j] = ((rx * rr) >> 15) - ((ry * dd) >> 15);
            q[2 * j + 1] = ((ry * rr) >> 15) + ((rx * dd) >> 15);
        }
        vx_poly(&f, q, 21, 0, silk + 16 + ((pk >> 8) * 110 >> 7));
    }
    vxs.rta = (uint16_t)(vxs.rta + (int16_t)((uint16_t)th - vxs.rta) * 15 / 256);   /* the ecliptic, swinging to the newest note */
    er = 352 + (ev * 56 >> 8);
    ex = (og_cos(vxs.rta) * 128) >> 15;
    ey = (og_sin(vxs.rta) * 128) >> 15;
    vx_circ(&f, ex, ey, er, 38 + (ev * 60 >> 8));
    for (i = 0; i < 12u; i++)
        vx_ray(&f, ex, ey, vxs.rta + i * 5461u, er - 24, er, 44 + (ev * 60 >> 8));
    vx_dot(&f, ex + ((og_cos(vxs.rta) * er) >> 15), ey + ((og_sin(vxs.rta) * er) >> 15), 32, 120 + (ev * 135 >> 8));
    {   /* the body at POS along the newest ray; the hub */
        int32_t pu = 26 + p[P_E3] * 205 / 127, rr = 56 + ((R - 56) * pu >> 8);
        if (nw >= 0)
            vx_orb(&f, (ux * rr) >> 15, (uy * rr) >> 15, 36, th + 32768u, 130 + (ev * 125 >> 8));
        vx_orb(&f, 0, 0, 40 + p[P_E6] * 40 / 127, th, 60 + (p[P_E5] * (77 + en) / 127 * 170 >> 8));
    }
}

/* (UMBRA LIVE) COMET: it flies a loop that drifts and breathes (the figure's own time: still in silence). The nine
 * partials are the nine streamers of its tail, laid along the path it has flown, each as long as its gain now (the
 * voices' own: the highest fall first), bold near the head, twisting on its own. Swells run down them: one sits where
 * the track's LFO is, one runs at its own pace; SHIMMER how far they swell. PARTIAL's cut ones stay faint, COMB a gap */
static void vx_cpos(uint32_t s, int32_t *x, int32_t *y)   /* where it is at s (Q8 frames), Q4 units */
{
    int32_t r = 400 + ((og_sin(vx_ang(s, 6)) * 120 + og_sin(vx_ang(s, 18) + 20861u) * 56) >> 15);
    uint32_t a = vx_ang(s, 38);
    *x = ((og_sin(vx_ang(s, 8)) * 160) >> 15) + ((og_cos(a) * r) >> 15);
    *y = ((og_sin(vx_ang(s, 10) + 10430u) * 120) >> 15) + ((og_sin(a) * (r * 220 >> 8)) >> 15);
}
#define VC_N 20                                          /* the path behind it: points 0.12 s apart */
static VX_SEG void vx_add(const track_t *t, const int32_t *A, int32_t en_home)
{
    const int16_t *p = t->p;
    vxf_t f = vx_uf(A, 100, 0);
    uint32_t mt = vxs.mt[VX_K_ADD], i, k;
    int32_t en = vx_level(t, VX_K_ADD, en_home), g[DRW_NP], gmax = 1, px[VC_N + 2], py[VC_N + 2];
    int32_t np = 256 + clamp(p[P_E3], 0, 127) * 2048 / 127, mot = 16 + clamp(p[P_E7], 0, 63), comb = clamp(p[P_E5], 0, 127);
    int32_t lf = clamp((t->lfo_val + 32768) >> 8, 0, 256), l2 = (int32_t)((mt * 7u >> 6) & 255u);
    (void)vx_age(t, VX_K_ADD, 0);
    for (k = 0; k < DRW_NP; k++) {                       /* each partial's gain now, the voices' summed */
        g[k] = 0;
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].active) g[k] += drw_v[drw_part(t)][i].gp[k];
        if (g[k] > gmax) gmax = g[k];
    }
    for (i = 0; i < 30u; i++) {                          /* where it has been: a faint trace */
        int32_t x, y;
        vx_cpos(mt - (i + 4u) * 1536u, &x, &y);
        vxs.dal = (int16_t)(96 + i * 5);
        vx_dot(&f, x, y, 0, 30);
    }
    vxs.dal = 0;
    for (i = 0; i < VC_N + 2u; i++)
        vx_cpos(mt - i * 922u, &px[i], &py[i]);
    for (k = 0; k < DRW_NP; k++) {                       /* the streamers */
        int32_t gk = gmax > 1 ? (int32_t)(((int64_t)g[k] << 8) / gmax) : 0, off = ((int32_t)k - 4) * 64, qx = px[0], qy = py[0], cutp = (int32_t)k * 256 >= np;
        uint32_t n = 7u + (uint32_t)(vx_isqrt(gk << 8) * (VC_N - 7) >> 8);
        if (cutp) n = 3;
        for (i = 1; i <= n; i++) {
            int32_t u = (int32_t)(i * 256u / n), nx = -(py[i + 1] - py[i - 1]), ny = px[i + 1] - px[i - 1], nl = vx_isqrt(nx * nx + ny * ny) + 1;
            int32_t d1 = u - lf, d2 = u - l2, sw, w, x, y, h;
            d1 = d1 < 0 ? -d1 : d1;
            d2 = d2 < 0 ? -d2 : d2;
            sw = ((d1 < 46 ? 256 - d1 * 256 / 46 : 0) + (d2 < 46 ? 256 - d2 * 256 / 46 : 0)) * mot / 79;   /* the swells here, 0..512 */
            w = ((off * u >> 8) * (256 + sw / 2) >> 8) * 160 / 256 +
                ((og_sin((uint32_t)u * (uint32_t)(52 + k * 8) - vx_ang(mt, 53 + (int32_t)k * 7) + k * 19820u) * ((u * (36 + (sw * 56 >> 8))) >> 8)) >> 15);
            x = px[i] + nx * w / nl;
            y = py[i] + ny * w / nl;
            h = cutp ? 20 : ((256 - u) * (84 + (gk * 171 >> 8)) >> 8) + 8 + ((sw * 60 >> 8) * (256 - u) >> 8);
            if (!(comb && i == 2u + (uint32_t)comb / 32u)) {
                vx_line(&f, qx, qy, x, y, h);
                if (u < 140 + sw / 4 && !cutp)           /* (drawn twice near the head: bold) */
                    vx_line(&f, qx + nx * 9 / nl, qy + ny * 9 / nl, x + nx * 9 / nl, y + ny * 9 / nl, h * 7 / 8);
            }
            qx = x; qy = y;
        }
    }
    vx_arc(&f, px[0], py[0], 104, 104, vx_ang(mt, 38) + 16384u - 13000u, vx_ang(mt, 38) + 16384u + 13000u, 8, 60 + (en * 150 >> 8));
    vx_orb(&f, px[0], py[0], 60, vx_ang(mt, 38) + 16384u, 150 + (en * 105 >> 8));
}

#if FELUCCA_USB_IN
/* (V10.54) INPUT: the audio interface's two inputs as meters (lit: the ones SOURCE takes), GATE's level across them,
 * between them FOLLOW (the matrix's source) and the gate's gain; the source over them */
/* (SWARM) the swarm as it plays: each device heard a body at its side (its channel: L, both, R) and its note's height,
 * its size its level, lit on each advertisement (its rhythm), turning on a small orbit at its own period (the
 * clocks' polyrhythm seen), a line from the loudest to each other (the relation that detunes them); SWARM n above */
static void vx_swarm(const int32_t *A)
{
    uint32_t v, now = fm1_ticks(), n = 0, ref = SW_N, rlv = 0;
    int32_t x0 = A[0] + 8, w = A[2] - 16, y0 = A[1] + 16, h = A[3] - 26, px[SW_N], py[SW_N], hl[SW_N];
    char b[10];
    if (w < 16 || h < 16)
        return;
    for (v = 0; v < 3u; v++)                             /* (the floor: L, both, R) */
        cv_rect(x0 + (int32_t)(1u + 2u * v) * w / 6 - 3, y0 + h + 4, 7, 1, T_MID);
    for (v = 0; v < SW_N; v++) {
        const struct sw_dev *d = &sw[v];
        uint32_t per = d->per | 1u, ph;
        px[v] = -1;
        if (!d->id || d->amp < 8)
            continue;
        n++;
        ph = (uint32_t)(((uint64_t)((now - d->last) % per) << 32) / per);   /* (its own clock's phase) */
        px[v] = x0 + w / 8 + d->pan * (w * 3 / 4) / 32767 + (osc_sine(ph) * 6 >> 15);   /* (its place: IMAGE's) */
        py[v] = y0 + h - 4 - clamp((int32_t)d->nt - 24, 0, 108) * (h - 8) / 108 + (osc_sine(ph + 0x40000000u) * 4 >> 15);
        hl[v] = clamp(((int32_t)d->lv - sw_lmin) * 255 / (int32_t)(((uint32_t)(sw_lmax - sw_lmin) & 127u) | 1u), 0, 255);
        if (d->lv >= rlv)
            rlv = d->lv, ref = v;
    }
    for (v = 0; v < SW_N && ref < SW_N; v++)              /* the relations: the loudest to each other */
        if (px[v] >= 0 && v != ref)
            cv_line(px[ref], py[ref], px[v], py[v], ux_mix(T_MID, vk_heat(hl[v]), 35));
    for (v = 0; v < SW_N; v++) {
        const struct sw_dev *d = &sw[v];
        int32_t z;
        int hit = d->acc > d->amp / 2;
        if (px[v] < 0)
            continue;
        z = clamp(3 + d->amp * 6 / 14000, 3, 9);
        cv_rect(px[v] - z / 2, py[v] - z / 2, z, z, hit ? T_TEXT : vk_heat(hl[v]));
        if (hit)                                         /* (an advertisement now: its ring) */
            cv_frame(px[v] - z / 2 - 3, py[v] - z / 2 - 3, z + 6, z + 6, vk_heat(hl[v]));
    }
    cv_rect(x0, y0 + h + 7, sm.mq * w >> 7, 2, T_THEME);   /* (V10.6z6) FAR .. NEAR: the morph, under the floor */
    if (!vx_small) {
        str_cpy(b, "SWARM ", sizeof b);
        fmt_int(b + 6, (int32_t)n);
        cv_text(A[0] + A[2] / 2 - 3 * (int32_t)str_len(b), A[1] + 2 - AF_X_CAP_Y, &AF_X, b, T_THEME);
    }
}
static VX_SEG void vx_input(const track_t *t, const int32_t *A, int32_t en_home)
{
    if (t->p[P_E0] == AIN_SW) {                         /* (SWARM: the devices; the other SOURCEs: the meters below) */
        vx_swarm(A);
        return;
    }
    uint32_t src = (uint32_t)clamp(t->p[P_E0], 0, AIN_N - 1), k;
    int32_t bw = A[2] / 7, bh = A[3] - 30, by = A[1] + 16, cx = A[0] + A[2] / 2, thr = clamp(t->p[P_E2], 0, 127);
    int32_t lv = thr * thr * 2, gg = ain_gg[(uint32_t)(t - trk) % NTRK];
    (void)en_home;
    if (bw < 4 || bh < 16)
        return;
    if (!vx_small)
        cv_text(cx - 3 * (int32_t)str_len(N_AIN_SRC[src]), A[1] + 2 - AF_X_CAP_Y, &AF_X, N_AIN_SRC[src], T_THEME);
    for (k = 0; k < 2u; k++) {
        int32_t x = k ? cx + bw : cx - 2 * bw, v = clamp(ain_pk[k] * 2, 0, 32767), hh = v * bh / 32767;
        int on = src >= AIN_SUM || src == k;
        cv_frame(x, by, bw, bh, T_MID);
        cv_rect(x + 1, by + bh - hh, bw - 2, hh, on ? T_THEME : T_MID);
        if (thr) {
            int32_t gy = by + bh - clamp(lv * 2, 0, 32767) * bh / 32767;
            cv_rect(x - 2, gy, bw + 4, 1, T_TEXT);
        }
        if (!vx_small)
            cv_text(x + bw / 2 - 3, by + bh + 4 - AF_X_CAP_Y, &AF_X, k ? "2" : "1", on ? T_TEXT : T_MID);
    }
    {   /* FOLLOW: a thin column, the gate's gain a dot over it */
        int32_t fh = mod_follow * bh / 32767;
        cv_frame(cx - 2, by, 5, bh, T_MID);              /* (its frame, the input silent too) */
        cv_rect(cx - 1, by + bh - fh, 3, fh, T_TEXT);
        if (thr)
            cv_rect(cx - 2, by - 6, 5, 3, gg > 16384 ? T_THEME : T_MID);
    }
}
#endif

/* ------------------------------------------------------------------------------------------------ the glue --- */
/* the engine's own creature in the rectangle x y w h (px: EDIT's left panel 8 14 108 112, HOME's middle 146 x 112, or a
 * whole panel: each fits itself to it), en_home: HOME's level for the track (-1: its own); 0 when the engine has none
 * (its being then) */
static int32_t vx_fit(int32_t w, int32_t h, int32_t uw, int32_t uh)   /* a unit's px (Q8): uw x uh units in w x h px */
{
    return w * 256 / uw < h * 256 / uh ? w * 256 / uw : h * 256 / uh;
}
typedef struct { const engine_t *e; void (*f)(const track_t *, const int32_t *, int32_t); } vxe_t;
static const vxe_t VX_F[] = {{&ENG_ANALOG, vx_analog}, {&ENG_WHEEL, vx_wheel}, {&ENG_NOISE, vx_noise}, {&ENG_PHASE, vx_phase},
                             {&ENG_LOFI, vx_lofi}, {&ENG_STRING, vx_string}, {&ENG_ADD, vx_add}};   /* (0.9.9-snd5) */
static int vx_draw(const track_t *t, int32_t x, int32_t y, int32_t w, int32_t h, int32_t en_home)
{
    const engine_t *e = trk_engp(t);
    int32_t A[4] = {x, y, w, h}, cx = x * 16 + w * 8, cy = y * 16 + h * 8, s;
    if (!bt_hw_seg() && (e == &ENG_LOFI || e == &ENG_ANIMAL || e == &ENG_STRING || e == &ENG_PHASE || e == &ENG_ADD || e == &ENG_WHEEL ||
                         e == &ENG_NOISE))
        return 0;                                        /* (V10.48: those figures live in the segment; V10.6g PHASE's too;
                                                         * (UMBRA LIVE) ADD's, WHEEL's and NOISE's: the room the smaller
                                                         * figures left there, the app slot's kept for the pages) */
    if (e == &ENG_FORMANT) {                             /* (the tract ~80 x 62 units about (-20, 37): the crown off) */
        s = vx_fit(w, h, 124, 94);
        vx_voice(t, cx, cy, s, en_home);
    } else if (trk_eng(t) == ENGI_DRUM) {    /* (the kit ~128 x 92) */
        s = vx_fit(w, h, 128, 92);
        vx_kit(t, cx, cy, s, en_home, -1);
#if FELUCCA_USB_IN
    } else if (e == &ENG_INPUT) {                       /* (V10.54) the inputs' meters */
        if (!bt_hw_seg())
            return 0;
        vx_input(t, A, en_home);
#endif
    } else if (e == &ENG_ANIMAL)                        /* (the animal ~103 x 78) */
        vx_animal(t, cx, cy, vx_fit(w, h, 106, 70), en_home);
    else if (trk_eng(t) == ENGI_FM6)
        vx_fm6(t, A, en_home, 6u, 0);
    else {                                               /* (0.9.9-snd5: the creatures drawn the same way, a table) */
        uint32_t i;
        for (i = 0; i < NELEM(VX_F) && VX_F[i].e != e; i++) {}
        if (i == NELEM(VX_F))
            return 0;
        VX_F[i].f(t, A, en_home);
    }
    return 1;
}
/* EDIT > KIT (lane -1) and SOUND 1..4 (a DRUM track): the kit large on the panel, KIT over it on the left; on the right
 * the sound last struck (KIT) or the sound edited, lit (SOUND: lane) */
static void vx_kit_page(const track_t *t, int32_t lane)
{
    char b[16];
    uint32_t l;
    vx_kit(t, 120 * 16, 76 * 16, 256, -1, lane);
    l = lane < 0 ? vxs.last & 7u : (uint32_t)lane & 7u;
    str_cpy(b, drum_lane_abbr(t, l), sizeof b);
    str_cpy(b + str_len(b), " \xB7 ", sizeof b - str_len(b));
    str_cpy(b + str_len(b), drum_lane_name(t, l), sizeof b - str_len(b));
    cv_text(8, 6 - AF_X_CAP_Y, &AF_X, "KIT", T_THEME);
    cv_text_r(232, 6 - AF_X_CAP_Y, &AF_X, b, lane < 0 ? T_TEXT : T_THEME, T_SURF);
}
