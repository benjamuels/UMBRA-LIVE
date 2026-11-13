/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* (UMBRA LIVE) the fluid plates and the layers made for them (firmware/src/plates/fluid.inc) run on this host under the
 * sanitizers, each in a block of exactly the bytes its header asks for (a write past its state is caught): random songs,
 * every element at its ends and turned under it, frames of every length, strips of every shape (the whole screen, 8-row
 * strips, the TIMELINE preview's single rows of 108 px), TOTALITY and the page, REPLACE and ADD. Also each header's
 * names: a plate's (9 letters), an element's (6), a preset's (7), the description (39). Exit 0: all passed.
 * cc -w -g -O1 -fsanitize=address,undefined -fno-sanitize=shift -fno-sanitize-recover=undefined -Ifirmware/src \
 *    -o plate_fluid_test tests/plate_fluid_test.c -lm
 * (-fno-sanitize=shift: the shared helpers shift negative numbers left, as the firmware's own always have) */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "plates/plate_lib.h"
#include "plates/fluid.inc"
static int16_t sine[1025]; static uint8_t colour[8][3], noise[1024], fade[256]; static uint16_t pal[4][256], lg[160];
static uint32_t rs = 12345; static uint32_t rnd(void) { rs = rs * 1664525u + 1013904223u; return rs >> 8; }
static int32_t mv(const umb_note_t *n, uint32_t ax, int32_t v, int32_t span) { (void)n; (void)ax; (void)span; return v; }
int main(void) {
    const umb_plate_t *const P[] = {UMB_RES_FLUID};
    umb_api_t a; int i, f, pi, run, big = 0, tot = 0;
    for (i = 0; i <= 1024; i++) sine[i] = (int16_t)(32767 * sin(i * 6.283185307 / 1024));
    for (i = 0; i < 8; i++) colour[i][0] = 255 - 20 * i, colour[i][1] = 100 + 10 * i, colour[i][2] = 200;
    for (i = 0; i < 1024; i++) noise[i] = rnd() & 255;
    for (i = 0; i < 256; i++) fade[i] = (uint8_t)(255 * exp(-i / 32.0));
    for (i = 0; i < 1024; i++) pal[i >> 8][i & 255] = (uint16_t)((i & 255) * 257);
    memset(&a, 0, sizeof a); a.api = UMB_API; a.move = mv; a.sine = sine; a.colour = colour; a.noise = noise; a.fade = fade; a.log = lg; a.pal = pal;
    for (pi = 0; pi < (int)(sizeof P / sizeof P[0]); pi++) {
        const umb_plate_t *p = P[pi];
        big = p->state > big ? p->state : big; tot += p->state;
        if (strlen(p->name) > 9 || strlen(p->desc) > 39 || p->nctl > 8 || p->npre > 3) { printf("%s: header\n", p->name); return 1; }
        for (i = 0; i < p->npre; i++) if (strlen(p->pre[i].name) > 7) { printf("%s: preset name %s\n", p->name, p->pre[i].name); return 1; }
        for (i = 0; i < p->nctl; i++) if (strlen(p->ctl[i].name) > 6 || p->ctl[i].def > 99) { printf("%s: element %d\n", p->name, i); return 1; }
        for (run = 0; run < 12; run++) {
            uint8_t *blk = malloc(p->state);                /* exactly its block: a write past it is caught */
            umb_lctx_t c; umb_strip_t S; int16_t vg[240];
            memset(blk, 0, p->state); memset(&c, 0, sizeof c);
            for (i = 0; i < 8; i++) c.k[i] = run == 0 ? p->ctl[i].def : run == 1 ? 0 : run == 2 ? 99 : rnd() % 100;
            for (i = 0; i < 8; i++) c.et[i] = run & 4 ? rnd() & 63 : 0;
            c.bare = run & 1; c.light = 256; c.now = rnd(); c.tracks = 6;
            p->init(blk, &c, &a);
            for (f = 0; f < 700; f++) {
                int n = run == 3 ? 12 : rnd() % 4, y;
                c.dt = f % 97 == 0 ? 250 : f % 13 == 0 ? 1 : 5 + rnd() % 60; c.now += c.dt; c.level = rnd(); c.beat = rnd();
                if (run >= 5 && f % 50 == 0) for (i = 0; i < 8; i++) c.k[i] = rnd() % 100;   /* the knobs turned under it */
                while (n--) {
                    umb_note_t nt; memset(&nt, 0, sizeof nt);
                    nt.track = rnd() % 6; nt.note = rnd() % 128; nt.vel = rnd() % 128; nt.drum = rnd() % 3 == 0; nt.lane = nt.drum ? rnd() % 8 : 0;
                    nt.mo[0] = (int16_t)(rnd() % 513) - 256; nt.mo[1] = (int16_t)(rnd() % 513) - 256;
                    p->note(blk, &c, &a, &nt);
                }
                p->frame(blk, &c, &a);
                for (i = 0; i < 240; i++) vg[i] = 256;
                S.vg = vg; S.mode = p->kind == UMB_KIND_OVERLAY && (run & 2) ? UMB_DRAW_ADD : UMB_DRAW_REPLACE;
                if (f % 3 == 0) {                               /* the screen in 8-row strips */
                    for (y = 0; y < 240; y += 8) { uint16_t *px = malloc(240 * 8 * 2); memset(px, 0, 240 * 8 * 2); S.px = px; S.w = 240; S.h = 8; S.x0 = 0; S.y0 = y; p->draw(blk, &c, &a, &S); free(px); }
                } else if (f % 3 == 1) {                        /* whole */
                    uint16_t *px = malloc(240 * 240 * 2); memset(px, 0, 240 * 240 * 2); S.px = px; S.w = 240; S.h = 240; S.x0 = 0; S.y0 = 0; p->draw(blk, &c, &a, &S); free(px);
                } else                                          /* a preview's rows: 108 px from 66, one row at a time */
                    for (y = 66; y < 174; y += 2) { uint16_t *px = malloc(108 * 2); memset(px, 0, 108 * 2); S.px = px; S.w = 108; S.h = 1; S.x0 = 66; S.y0 = y; p->draw(blk, &c, &a, &S); free(px); }
            }
            free(blk);
        }
        printf("%-7s state %5u B  ok\n", p->name, p->state);
    }
    printf("largest state %d B; all of them %d B\n", big, tot);
    return 0;
}
