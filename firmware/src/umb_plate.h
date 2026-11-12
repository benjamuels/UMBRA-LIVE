/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* UMBRA LIVE: the plate API, version 1 (plans/umbra-plate-bank.md, plans/umbra-plate-compositing.md).
 *
 * A plate is a module: a header (umb_plate_t) and four entry points. It keeps no state of its own outside the block the
 * firmware hands it (umb_plate_t.state bytes, zeroed before init): no globals, no statics that change, nothing but its
 * own code and constants. Everything else reaches it through the layer's context (umb_lctx_t, once a frame) and the
 * services table (umb_api_t). Two layers can run at once (a field plate under an overlay), each with its own block, its
 * own notes and its own context, so nothing here is one of a kind.
 *
 * This file is the contract between the firmware and every plate file ever built: it only grows. A new field or service
 * goes at the end of its struct and raises UMB_API; nothing is moved, resized or given a new meaning. A plate built for
 * API n runs on any firmware with UMB_API >= n.
 *
 * Built two ways: included into the firmware (a resident plate, ui_umbra.c), or alone with only this header (a plate
 * file for the bank: no other include, no call into the firmware but through umb_api_t). */
#ifndef UMB_PLATE_H
#define UMB_PLATE_H
#include <stdint.h>

#define UMB_API 3u                                      /* (2: the context's tracks' offsets and roots, the
                                                         * services' tables: what the field plates read; 3: the
                                                         * controls are elements: the tracks chosen for each, the level
                                                         * and the beat in the context, UMB_EL_* in the header) */
#define UMB_PLATE_MAGIC 0x544C5055u                     /* "UPLT", first in a plate's header */
#define UMB_NTRK 6u                                     /* tracks (a note's track is 0 .. UMB_NTRK - 1) */
#define UMB_NCTL 8u                                     /* controls a plate may declare: its elements, as LIVE VIZ CTRLS
                                                         * lists them (a setting, or a part of the picture the notes drive) */
#define UMB_EL_NOTES 1u                                 /* (API 3) umb_plate_t.el: the notes drive this control (LIVE VIZ
                                                         * CTRLS offers its tracks: umb_lctx_t.et); its value how hard */

enum { UMB_KIND_FIELD = 1u, UMB_KIND_OVERLAY = 2u };    /* what it draws: every pixel (a ground), or sparse light */
enum { UMB_DRAW_REPLACE = 0u, UMB_DRAW_ADD = 1u };      /* every pixel of the strip set / light added to what is there */
#define UMB_MODE(m) (1u << (m))                         /* umb_plate_t.modes: the draw modes a plate takes */
#ifndef UMB_PLATE_HEAD                                  /* a plate file's build puts the header first in its slot */
#define UMB_PLATE_HEAD
#endif

typedef struct {                                        /* a note, as the firmware hands it to a layer */
    uint8_t track;                                      /* 0 .. UMB_NTRK - 1 */
    uint8_t note;                                       /* MIDI note */
    uint8_t vel;                                        /* velocity after REACT, 1..127; 0: let go */
    uint8_t drum;                                       /* 1: the track is a kit */
    uint8_t lane;                                       /* .. and the note's lane (0..7), else 0 */
    uint8_t rsv;
    int16_t mo[2];                                      /* where NOTE MOVEMENT put this note: its track's offset as it was
                                                         * played, across and down (Q8 of a span, -256 .. 256); umb_api_t.move */
} umb_note_t;

typedef struct {                                        /* a control a plate declares */
    char name[7];                                       /* as the PLATE page shows it, 0-ended */
    uint8_t def;                                        /* its default, 0..99 */
} umb_ctl_t;

typedef struct {                                        /* a layer, worked out once a frame */
    uint32_t now;                                       /* the plates' clock (ms; SPEED applied, frozen at 0) */
    uint32_t dt;                                        /* ms since the layer's last frame, at most 250 */
    uint8_t k[UMB_NCTL];                                /* the controls, 0..99, as routed (macros, ROUTING) */
    int32_t light;                                      /* LIGHT, Q8 (256: full) */
    uint8_t bare;                                       /* 1: TOTALITY, the whole screen the plate's */
    uint8_t tracks;                                     /* tracks playing (TRACKS: 4 or 6) */
    uint8_t layer;                                      /* 0: the ground, 1: the overlay */
    uint8_t rsv;
    /* (API 2) */
    int16_t mo[UMB_NTRK][2];                            /* each track's NOTE MOVEMENT offset now (its last note's), across
                                                         * and down (Q8 of a span; umb_note_t.mo as it is now) */
    uint8_t root[UMB_NTRK];                             /* each track's root, 0..11 (C..B) */
    uint8_t rsv2[2];
    /* (API 3) */
    uint8_t et[UMB_NCTL];                               /* each control's tracks as LIVE VIZ CTRLS chose them (bit t: track
                                                         * t's notes drive it, every one of them); 0: the plate's own choice */
    uint8_t level;                                      /* the mix's level now, 0..255 */
    uint8_t beat;                                       /* 255 on the beat, falling to the next (stopped: 0) */
    uint8_t rsv3[2];
} umb_lctx_t;

typedef struct {                                        /* the strip being drawn */
    uint16_t *px;                                       /* the canvas, w x h, as the panel takes it (RGB565, bytes swapped) */
    int32_t w, h;                                       /* its size (px) */
    int32_t x0, y0;                                     /* its place on the screen (px) */
    const int16_t *vg;                                  /* each row's light (Q8: LIGHT and the page's darkening), vg[y - y0] */
    uint32_t mode;                                      /* UMB_DRAW_* */
} umb_strip_t;

typedef struct {                                        /* the firmware's services */
    uint32_t api;                                       /* the firmware's UMB_API */
    int32_t (*move)(const umb_note_t *n, uint32_t axis, int32_t v, int32_t span);   /* NOTE MOVEMENT: v (0..span along
                                                         * axis 0 across, 1 down) where note n's offset moves it, folded */
    const int16_t *sine;                                /* a sine turn, 1024 points + the first again, +-32767 */
    const uint8_t (*colour)[3];                         /* the notes' eight colours, r g b (a track's: (track x 3) % 8) */
    /* (API 2) the firmware's tables, made at power-on */
    const uint8_t *noise;                               /* 32 x 32 bytes of value noise that tiles (row by row) */
    const uint8_t *fade;                                /* 256 points: 255 exp(-i / 32) */
    const uint16_t *log;                                /* 160 points: 512 ln(r / 16), r 16 .. 175 */
    const uint16_t (*pal)[256];                         /* the cold palette as the panel takes it (RGB565, bytes swapped),
                                                         * a 2 x 2 dither each: [(y & 1) x 2 + (x & 1)]; [1] undithered */
} umb_api_t;

typedef struct umb_plate_s {                            /* (not umb_plate: ui_umbra.c's stored plate) */
    uint32_t magic;                                     /* UMB_PLATE_MAGIC */
    uint16_t api;                                       /* the UMB_API it was built for */
    uint16_t state;                                     /* its block's bytes */
    char name[10];                                      /* as the screen shows it, 0-ended */
    uint8_t kind;                                       /* UMB_KIND_* */
    uint8_t modes;                                      /* UMB_MODE(UMB_DRAW_*) it takes */
    uint8_t nctl;                                       /* the controls it declares (ctl[0 .. nctl - 1]); LIGHT is the
                                                         * firmware's, never one of them */
    uint8_t rsv[3];
    umb_ctl_t ctl[UMB_NCTL];
    /* chosen (its block zeroed): start afresh */
    void (*init)(void *st, const umb_lctx_t *c, const umb_api_t *a);
    /* a note (before the frame's call, in the order played; a burst: only the newest) */
    void (*note)(void *st, const umb_lctx_t *c, const umb_api_t *a, const umb_note_t *n);
    /* once a frame, after the notes: fades, steps, whatever moves */
    void (*frame)(void *st, const umb_lctx_t *c, const umb_api_t *a);
    /* a strip: in REPLACE every pixel of it set; in ADD light added to what is there (only the modes it takes) */
    void (*draw)(void *st, const umb_lctx_t *c, const umb_api_t *a, const umb_strip_t *s);
    /* (API 3: read only where api >= 3) */
    uint8_t el[UMB_NCTL];                               /* each control: UMB_EL_* */
} umb_plate_t;

#endif
