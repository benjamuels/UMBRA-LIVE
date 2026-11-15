/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* (branch plate-test-features) one plate's NEW elements (umb_plate.h API 6: umb_xel_t, umb_lctx_t.xk), on this host:
 *   check  the header (at most 3 elements of at most 3 parameters; names 7 / 6 letters; a toggle in each, its default 0 or
 *          1; every line of description there) and the plate run under the sanitizers, in a block of exactly its state,
 *          with its eight elements and the new parameters at their ends, random and turned under it, toggles flipped,
 *          strips of every shape, REPLACE and ADD. Exit 0: passed.
 *   pair OUT.png [secs]  a picture: the top row the plate as it was (every new toggle off), the bottom row with every new
 *          toggle on (the other new parameters at their defaults), each row three moments of a steady song (kick every
 *          0.5 s, hats, a snare, three melodic tracks), TOTALITY (the whole screen)
 *   shot OUT.png v0 .. v8  one row of three moments with the new parameters as given (element 0's three, then
 *          element 1's, then element 2's)
 * Build one plate (PF: its file, PV: its header's name):
 * cc -w -g -O1 -fsanitize=address,undefined -fno-sanitize=shift -fno-sanitize-recover=undefined -Ifirmware/src \
 *    -DPF='"plates/beacon.c"' -DPV=umb_plate_beacon -o /tmp/xt tests/plate_x_test.c -lm
 * The palette, the noise and the colours are close to the firmware's, not its own tables: for a look, not for pixels. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "plates/plate_lib.h"
#include PF
static int16_t sine[1025]; static uint8_t colour[8][3] = {{140, 236, 176}, {120, 222, 196}, {96, 214, 232}, {104, 160, 255},
    {118, 108, 232}, {168, 130, 255}, {232, 124, 196}, {242, 172, 96}}, noise[1024], fade[256]; static uint16_t pal[4][256], lg[160];
static const uint8_t ST[6][4] = {{0, 0, 0, 0}, {40, 2, 5, 18}, {84, 14, 38, 112}, {136, 44, 130, 200}, {190, 132, 218, 228}, {255, 248, 255, 255}};
static uint32_t rs = 12345; static uint32_t rnd(void) { rs = rs * 1664525u + 1013904223u; return rs >> 8; }
static int32_t mv(const umb_note_t *n, uint32_t ax, int32_t v, int32_t span) { (void)ax; return pl_fold(n->mo[ax & 1u], v, span); }
static umb_api_t A;
static void tables(void)
{
    int i, j = 0, k, c;
    for (i = 0; i <= 1024; i++) sine[i] = (int16_t)(32767 * sin(i * 6.283185307 / 1024));
    for (i = 0; i < 256; i++) fade[i] = (uint8_t)(255 * exp(-i / 32.0));
    for (i = 0; i < 160; i++) lg[i] = (uint16_t)(512 * log((i + 16) / 16.0));
    { float r[1024]; for (i = 0; i < 1024; i++) r[i] = rnd() & 255;      /* value noise, smoothed, that tiles */
      for (i = 0; i < 1024; i++) { int x = i & 31, y = i >> 5; float s = 0; int dx, dy;
        for (dy = -1; dy <= 1; dy++) for (dx = -1; dx <= 1; dx++) s += r[((y + dy) & 31) * 32 + ((x + dx) & 31)];
        noise[i] = (uint8_t)(s / 9); } }
    for (i = 0; i < 256; i++) {
        int p[3];
        while (i > ST[j + 1][0]) j++;
        for (c = 1; c < 4; c++) p[c - 1] = ST[j][c] + (ST[j + 1][c] - ST[j][c]) * (i - ST[j][0]) / (ST[j + 1][0] - ST[j][0]);
        for (k = 0; k < 4; k++) pal[k][i] = pl_pack(p[0], p[1], p[2], PL_D2[k]);
    }
    memset(&A, 0, sizeof A); A.api = UMB_API; A.move = mv; A.sine = sine; A.colour = colour; A.noise = noise; A.fade = fade; A.log = lg;
    A.pal = (const uint16_t (*)[256])pal;
}
static const umb_plate_t *P = &PV;
static int fail(const char *w) { printf("%s: %s\n", P->name, w); return 1; }
static int header(void)
{
    int e, i, tg;
    if (P->api < 6) return fail("api < 6");
    if (P->nxe < 1 || P->nxe > UMB_NXE) return fail("nxe: 1 .. 3 new elements");
    for (e = 0; e < P->nxe; e++) {
        const umb_xel_t *x = &P->xel[e];
        if (!x->name[0] || strlen(x->name) > 7) { printf("%s: element %d name '%s' (1..7 letters)\n", P->name, e, x->name); return 1; }
        if (x->np < 2 || x->np > UMB_NXP) { printf("%s: element %s: 2 or 3 parameters\n", P->name, x->name); return 1; }
        if (!x->what || strlen(x->what) < 10) { printf("%s: element %s: no description\n", P->name, x->name); return 1; }
        for (i = tg = 0; i < x->np; i++) {
            if (!x->p[i].name[0] || strlen(x->p[i].name) > 6) { printf("%s: %s parameter %d name '%s' (1..6 letters)\n", P->name, x->name, i, x->p[i].name); return 1; }
            if (!x->pw[i] || strlen(x->pw[i]) < 6) { printf("%s: %s.%s: no description\n", P->name, x->name, x->p[i].name); return 1; }
            if (x->tog >> i & 1) { tg++; if (x->p[i].def > 1) { printf("%s: %s.%s: a toggle's default is 0 or 1\n", P->name, x->name, x->p[i].name); return 1; } }
            else if (x->p[i].def > 99) { printf("%s: %s.%s: default past 99\n", P->name, x->name, x->p[i].name); return 1; }
        }
        if (tg != 1) { printf("%s: element %s: exactly one on/off toggle (tog bits)\n", P->name, x->name); return 1; }
        if (x->tog >> x->np) { printf("%s: element %s: a toggle bit past its parameters\n", P->name, x->name); return 1; }
    }
    if (strnlen(P->name, 10) > 9 || P->nctl > 8 || P->npre > 3) return fail("header");
    for (i = 0; i < P->nctl; i++) if (strlen(P->ctl[i].name) > 6 || P->ctl[i].def > 99) return fail("element");
    return 0;
}
static void xset(umb_lctx_t *c, int how)               /* how: 0 defaults, 1 all low, 2 all high, 3 random */
{
    int e, i;
    memset(c->xk, 0, sizeof c->xk);
    for (e = 0; e < P->nxe; e++)
        for (i = 0; i < P->xel[e].np; i++) {
            int t = P->xel[e].tog >> i & 1;
            c->xk[e][i] = (uint8_t)(how == 0 ? P->xel[e].p[i].def : how == 1 ? 0 : how == 2 ? (t ? 1 : 99) : t ? rnd() & 1 : rnd() % 100);
        }
}
static int check(void)
{
    int i, f, run;
    if (header()) return 1;
    for (run = 0; run < 16; run++) {
        uint8_t *blk = malloc(P->state);                  /* exactly its block: a write past it is caught */
        umb_lctx_t c; umb_strip_t S; int16_t vg[240];
        memset(blk, 0, P->state); memset(&c, 0, sizeof c);
        for (i = 0; i < 8; i++) c.k[i] = run == 0 ? P->ctl[i].def : run == 1 ? 0 : run == 2 ? 99 : rnd() % 100;
        for (i = 0; i < 8; i++) c.et[i] = run & 4 ? rnd() & 63 : 0;
        xset(&c, run < 3 ? run : run == 3 ? 2 : 3);
        c.bare = run & 1; c.light = 256; c.now = rnd(); c.tracks = 6;
        P->init(blk, &c, &A);
        for (f = 0; f < 700; f++) {
            int n = run == 3 ? 12 : rnd() % 4, y;
            c.dt = f % 97 == 0 ? 250 : f % 13 == 0 ? 1 : 5 + rnd() % 60; c.now += c.dt; c.level = rnd(); c.beat = rnd();
            if (run >= 5 && f % 50 == 0) { for (i = 0; i < 8; i++) c.k[i] = rnd() % 100; xset(&c, f % 100 ? 3 : (f / 100) % 3); }
            while (n--) {
                umb_note_t nt; memset(&nt, 0, sizeof nt);
                nt.track = rnd() % 6; nt.note = rnd() % 128; nt.vel = rnd() % 128; nt.drum = rnd() % 3 == 0; nt.lane = nt.drum ? rnd() % 8 : 0;
                nt.mo[0] = (int16_t)(rnd() % 513) - 256; nt.mo[1] = (int16_t)(rnd() % 513) - 256;
                P->note(blk, &c, &A, &nt);
            }
            P->frame(blk, &c, &A);
            for (i = 0; i < 240; i++) vg[i] = 256;
            S.vg = vg; S.mode = (P->modes & UMB_MODE(UMB_DRAW_ADD)) && (run & 2) ? UMB_DRAW_ADD : UMB_DRAW_REPLACE;
            if (!(P->modes & UMB_MODE(S.mode))) S.mode = UMB_DRAW_REPLACE;
            if (f % 3 == 0) {
                for (y = 0; y < 240; y += 8) { uint16_t *px = calloc(240 * 8, 2); S.px = px; S.w = 240; S.h = 8; S.x0 = 0; S.y0 = y; P->draw(blk, &c, &A, &S); free(px); }
            } else if (f % 3 == 1) {
                uint16_t *px = calloc(240 * 240, 2); S.px = px; S.w = 240; S.h = 240; S.x0 = 0; S.y0 = 0; P->draw(blk, &c, &A, &S); free(px);
            } else
                for (y = 66; y < 174; y += 2) { uint16_t *px = calloc(108, 2); S.px = px; S.w = 108; S.h = 1; S.x0 = 66; S.y0 = y; P->draw(blk, &c, &A, &S); free(px); }
        }
        free(blk);
    }
    printf("%-9s state %5u B, %d new element%s: ok\n", P->name, P->state, P->nxe, P->nxe == 1 ? "" : "s");
    return 0;
}
/* ---- pictures: a steady song, frames at three moments */
static uint8_t img[480][720][3];
static void song(uint8_t *blk, umb_lctx_t *c, uint32_t t0, uint32_t t1)   /* ms t0 .. t1, 30 frames a second */
{
    static uint32_t rs2 = 99; uint32_t t;
    for (t = t0; t < t1; t += 33) {
        uint32_t ms;
        c->dt = 33; c->now += 33;
        c->beat = (uint8_t)(255 - (t % 500) * 255 / 500); c->level = (uint8_t)(140 + 60 * sin(t / 900.0));
        for (ms = t; ms < t + 33; ms++) {
            umb_note_t n; memset(&n, 0, sizeof n);
            if (ms % 500 == 0) { memset(&n, 0, sizeof n); n.track = 0; n.drum = 1; n.lane = 0; n.note = 36; n.vel = 110; P->note(blk, c, &A, &n); }
            if (ms % 250 == 125) { memset(&n, 0, sizeof n); n.track = 0; n.drum = 1; n.lane = 3; n.note = 42; n.vel = 70; P->note(blk, c, &A, &n); }
            if (ms % 1000 == 500) { memset(&n, 0, sizeof n); n.track = 0; n.drum = 1; n.lane = 1; n.note = 38; n.vel = 100; P->note(blk, c, &A, &n); }
            if (ms % 375 == 0) { rs2 = rs2 * 1664525u + 1013904223u; n.track = 1 + (rs2 >> 24) % 3; n.note = 48 + (rs2 >> 8) % 30; n.vel = 60 + (rs2 >> 16) % 60;
                n.mo[0] = (int16_t)((rs2 >> 4) % 200) - 100; P->note(blk, c, &A, &n); n.vel = 0; P->note(blk, c, &A, &n); }
            if (ms % 2000 == 1000) { memset(&n, 0, sizeof n); n.track = 4; n.note = 60 + (ms / 2000) % 12; n.vel = 90; P->note(blk, c, &A, &n); }
            if (ms % 2000 == 1900) { memset(&n, 0, sizeof n); n.track = 4; n.note = 60 + (ms / 2000) % 12; n.vel = 0; P->note(blk, c, &A, &n); }
        }
        P->frame(blk, c, &A);
    }
}
static void row(int how, const int *xv, int ry, uint32_t secs)
{
    uint8_t *blk = calloc(1, P->state); umb_lctx_t c; umb_strip_t S; int16_t vg[240]; int i, k, x, y;
    uint16_t *px = calloc(240 * 240, 2);
    memset(&c, 0, sizeof c);
    for (i = 0; i < 8; i++) c.k[i] = i < P->nctl ? P->ctl[i].def : 0;
    xset(&c, 0);
    for (k = 0; how >= 0 && k < P->nxe; k++)
        for (i = 0; i < P->xel[k].np; i++)
            if (P->xel[k].tog >> i & 1) c.xk[k][i] = (uint8_t)how;
    if (xv) for (k = 0; k < 9; k++) if (k / 3 < P->nxe && k % 3 < P->xel[k / 3].np) c.xk[k / 3][k % 3] = (uint8_t)xv[k];
    c.bare = 1; c.light = 256; c.tracks = 6; c.now = 1000;
    P->init(blk, &c, &A);
    for (i = 0; i < 240; i++) vg[i] = 256;
    for (k = 0; k < 3; k++) {
        song(blk, &c, k * secs * 1000 / 3, (k + 1) * secs * 1000 / 3);
        memset(px, 0, 240 * 240 * 2);
        S.px = px; S.w = 240; S.h = 240; S.x0 = 0; S.y0 = 0; S.vg = vg; S.mode = UMB_DRAW_REPLACE;
        P->draw(blk, &c, &A, &S);
        for (y = 0; y < 240; y++) for (x = 0; x < 240; x++) {
            uint16_t v = px[y * 240 + x], p = (uint16_t)((v >> 8) | (v << 8));
            img[ry + y][k * 240 + x][0] = (uint8_t)(((p >> 11) & 31) * 255 / 31);
            img[ry + y][k * 240 + x][1] = (uint8_t)(((p >> 5) & 63) * 255 / 63);
            img[ry + y][k * 240 + x][2] = (uint8_t)((p & 31) * 255 / 31);
        }
    }
    free(px); free(blk);
}
static uint32_t crc_t[256];
static uint32_t crc(uint32_t c, const uint8_t *b, size_t n) { while (n--) c = crc_t[(c ^ *b++) & 255] ^ (c >> 8); return c; }
static void chunk(FILE *f, const char *ty, const uint8_t *d, uint32_t n)
{
    uint8_t h[4] = {n >> 24, n >> 16, n >> 8, n}; uint32_t c;
    fwrite(h, 1, 4, f); fwrite(ty, 1, 4, f); fwrite(d, 1, n, f);
    c = crc(crc(0xFFFFFFFFu, (const uint8_t *)ty, 4), d, n) ^ 0xFFFFFFFFu;
    h[0] = c >> 24; h[1] = c >> 16; h[2] = c >> 8; h[3] = c; fwrite(h, 1, 4, f);
}
static int png(const char *out, int H)                 /* (stored deflate: no zlib needed) */
{
    FILE *f = fopen(out, "wb"); uint32_t W = 720, raw = (W * 3 + 1) * H, n, i, o = 0, a = 1, b = 0, y;
    uint8_t *r = malloc(raw), *z = malloc(raw + raw / 65535 * 5 + 64), ih[13] = {0, 0, 720 >> 8, 720 & 255, 0, 0, 0, 0, 8, 2, 0, 0, 0};
    if (!f) { perror(out); return 1; }
    for (i = 0; i < 256; i++) { uint32_t c = i; int k; for (k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; crc_t[i] = c; }
    for (y = 0; y < (uint32_t)H; y++) { r[y * (W * 3 + 1)] = 0; memcpy(r + y * (W * 3 + 1) + 1, img[y], W * 3); }
    z[o++] = 0x78; z[o++] = 0x01;
    for (i = 0; i < raw; i += n) { n = raw - i > 65535 ? 65535 : raw - i; z[o++] = i + n >= raw; z[o++] = n & 255; z[o++] = n >> 8; z[o++] = ~n & 255; z[o++] = (~n >> 8) & 255; memcpy(z + o, r + i, n); o += n; }
    for (i = 0; i < raw; i++) { a = (a + r[i]) % 65521; b = (b + a) % 65521; }
    z[o++] = b >> 8; z[o++] = b; z[o++] = a >> 8; z[o++] = a;
    ih[6] = H >> 8; ih[7] = H & 255;
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f); chunk(f, "IHDR", ih, 13); chunk(f, "IDAT", z, o); chunk(f, "IEND", z, 0);
    fclose(f); free(r); free(z); printf("%s\n", out); return 0;
}
int main(int argc, char **argv)
{
    tables();
    if (argc >= 3 && !strcmp(argv[1], "pair")) {
        uint32_t s = argc > 3 ? (uint32_t)atoi(argv[3]) : 6;
        if (header()) return 1;
        row(0, 0, 0, s); row(1, 0, 240, s); return png(argv[2], 480);
    }
    if (argc >= 3 && !strcmp(argv[1], "shot")) {
        int xv[9] = {0}, i;
        if (header()) return 1;
        for (i = 0; i < 9; i++) xv[i] = argc > 3 + i ? atoi(argv[3 + i]) : 0;
        row(-1, xv, 0, 6); return png(argv[2], 240);
    }
    return check();
}
