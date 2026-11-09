/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * JIANT (FELUCCA TONIC): the song as SLOOP 2.4.1 plays it (isod89/sloop-fm1, GPL-3.0: arranger.h, arranger_scene.c,
 * its live sections, quick chain and SONG REC), rebuilt on Felucca's sequencer. */
/* The song (docs/TONIC-SONG-PLAN.md). Four sections A..D, the four saved projects: a section is the whole project, the
 * four tracks' sounds (engine, parameters, FM6 patch), the DRUM-X kit, their steps and the motion; the tempo and the
 * globals are the song's (not the sections'). Sections go in on the bar (a 4/4 bar of 16 1/16 steps at the tempo), every track from
 * its step 0, the remainder of the block carried.
 *   the song    up to CHAIN_ROWS rows, each a section and the bars it plays (1..CHAIN_BARS): PLAY on SONG (or anywhere
 *               in SONG mode) plays it from the top and keeps the music as it was (song_keep), which comes back when
 *               the song ends (its last row) or stops
 *   live        (the song layer, SAVE held: ui_layer.c) a section asked for while the loop plays goes in on the next
 *               bar (stopped: at once); the music can be stored into a section (RAM at once, flash once stopped) and
 *               the section playing brought back as it was stored (RECALL); a quick chain (two or more sections
 *               tapped in one hold) plays them in turn, each its longest pattern's bars, round and round; SONG REC
 *               writes the sections played and their bars into the song's rows
 *   main loop   decodes the section that goes in next into sec_stage (song_main.c); the ISR only swaps it in
 *               (song_play.c). A row whose section is not staged in time plays its section another bar
 * This file: the state and the rows; song_play.c (included at the end of seq.c): the ISR's part; song_main.c
 * (included by project.c): the main loop's. */
static chain_config_t chain_config;           /* the current song's rows */
#define dice_secs (chain_config.rsv[1])       /* (JIANT 0.6.81) the sections DICE ALL / GEN wrote (bit s); (gen6c) kept with
                                              * the song's rows in flash: after a power cycle GEN's FORM writes them again
                                              * (in RAM they were lost, and every section looked the player's) */
#define NSONG 6u                              /* songs: four sections each (storage.c: where they are kept). (V10.6p) 8
                                              * before: 7 and 8's flash is the drum sample bank's now (storage.c FL_SAMPLES) */
#define NSONG_IDX 8u                          /* (V10.6p) the index's room, as before: its layout (flash, backups) kept;
                                              * rows 7, 8 unused, kept valid for an older firmware's restore */
#define NSONG_USE 4u                          /* (UMBRA OBSCURA) the projects played and kept: 1..4; 5..8's flash (V10.7.6:
                                              * songs 5, 6 and the drum sample bank's, written by nothing yet) is the
                                              * plate bank (umb_slot.c), their index rows kept unused */
#define SONG_MODE_TAG 0xA0u                    /* chain.rsv[2]: saved LOOP/SONG mode, old files have zero */
#define SONG_MAGIC 0x474E4F53u                /* "SONG" */
/* (JIANT) a row's scene: when the row goes in (a song, not a quick chain) the tracks' mutes, DRUM's group mutes, the
 * macros and the punch-in MIDI effects become what was stored; they stay through the rows after it that have none.
 * Stored by KNOB 4 on SONG (the state now) or by SONG REC (the state as each row began) */
typedef struct {
    uint8_t on;                               /* bit 0 = the row has one; (JIANT 0.6) bits 1, 2: T5, T6 muted */
    uint8_t mute;                             /* bits 0..3 T1..T4 muted, 4..7 DRUM's groups (DXG_*) */
    uint16_t pfx;                             /* the punch-in MIDI effects held: bit k = PF_OCTD + k */
    uint8_t mac[4];                           /* M1..M4 (mod.c macro_v) */
} scene_t;
typedef struct {                              /* the song index (flash: OBJ_SONGIDX) */
    uint32_t magic;
    uint8_t version, cur, rsv[2];             /* rsv[0]: no index yet (RAM only); (V10.6p) rsv[1]: songs 7, 8's
                                              * retirement checked (song_main.c song_retired) */
    chain_config_t rows[NSONG_IDX];           /* each song's rows (the current one's: chain_config, kept here) */
    scene_t scene[NSONG_IDX][CHAIN_ROWS];     /* (version 2, JIANT) their scenes; a version 1 index: none */
    uint32_t tsrc[NSONG_IDX][CHAIN_ROWS];     /* (version 3, add15) where each track plays from in the row: 3 bits a track,
                                              * 0 the row's section, 1..4 A..D (SAVE > ARRANGE 2); older: all 0 */
    uint32_t tvar[NSONG_IDX][CHAIN_ROWS];     /* (version 4, add15-beta4) each track's variation in the row: 4 bits a track,
                                              * RV_* (0 none); older: all 0 */
} song_index_t;
#define SONG_IDX_V 4u
/* (add15-beta4) a row's variation of a track (ARRANGE 2's VAR): what its steps become when the row goes in (song_main.c
 * row_vary), MUTE the track silent for the row */
enum { RV_NONE, RV_MUTE, RV_THIN, RV_FILL, RV_HALF, RV_REV, RV_SHIFT, RV_OCTU, RV_OCTD, RV_MUT1, RV_N = RV_MUT1 + 4 };
static const char *const RV_NAME[RV_N] = {"--", "MUTE", "THIN", "FILL", "HALF", "REV", "SHIFT", "OCT+", "OCT-",
                                          "VAR 1", "VAR 2", "VAR 3", "VAR 4"};
static void song_idx_touch(void);             /* song_main.c: the index changed, into flash soon */
/* the state now as a scene (main loop: KNOB 4 on SONG; ISR: SONG REC) */
static void scene_capture(scene_t *s)
{
    uint32_t i, m = dx_mute & DXG_ALL, on = 1u;
    m <<= 4;
    for (i = 0; i < NTRK; i++)
        if (trk[i].p[P_MUTE]) {
            if (i < 4u) m |= 1u << i;
            else on |= 1u << (i - 3u);
        }
    s->on = (uint8_t)on;
    s->mute = (uint8_t)m;
    s->pfx = (uint16_t)((((perf_held | perf_latched) & PF_MIDI) >> PF_OCTD) | (scene_pfx >> PF_OCTD));
    memcpy(s->mac, macro_v, sizeof s->mac);
}
static song_index_t song_idx;
static uint8_t song_draft;                    /* Generated workspace: never auto-written to project slots. */
static uint8_t song_cur;                      /* the song whose sections proj_slot holds (0: the four project slots) */
/* track k of the current song's row r: the section it plays (the row's own unless ARRANGE 2 gave it another) */
static uint32_t row_src(uint32_t r, uint32_t k, uint32_t s)
{
    uint32_t v = r < CHAIN_ROWS ? song_idx.tsrc[song_cur][r] >> (3u * k) & 7u : 0u;
    return v ? v - 1u : s & 3u;
}
static uint32_t row_var(uint32_t r, uint32_t k)
{
    return r < CHAIN_ROWS ? song_idx.tvar[song_cur][r] >> (4u * k) & 15u : 0u;
}
#define SEC_LIVE 0xFEu                /* sec_stage.row: a live jump (the song layer), not a song row */
typedef struct {                      /* a track of a section, as the ISR puts it in */
    int16_t p[P_COUNT];
    uint8_t engine, preset;
    step_t step[NSTEP];
    uint8_t fm6[FP_SIZE + 1u];        /* (unpacked: eng_fm6.c fm6_set_patch) */
} sec_trk_t;
static struct {
    sec_trk_t t[NTRK];
    motion_store_t motion;
    dx_lane_t dx[8];                  /* the section's DRUM-X kit */
    uint16_t dx_mute;                 /* .. its mutes (groups, sounds) */
    uint8_t pfx_lane[32], pfx_ltgt;   /* .. its punch-in lane (pfx.c) */
    int16_t slg[G_SL_N];              /* .. (0.6.3) its SLICER (the globals G_SLMODE ..) */
    volatile uint8_t ready;           /* main: staged; ISR: taken (0) */
    uint8_t row, section;             /* the song row it is for (SEC_LIVE: a live jump), the section (0..3) */
    uint32_t src;                     /* (V10.6s) CLIPS: each track's part as a row's tsrc has it (SONG REC's rows) */
} sec_stage __attribute__((section(".lowbuf")));
static uint8_t sec_stage_fit[NTRK] __attribute__((section(".pool")));   /* (0.6.78: app.ld LOWBUF) */
/* (V10.6z8, a GENE user: "I change the instruments and at the end of the phrase they go back") a sound the player
 * changed while a song or a quick chain plays is kept across its sections: bit k track k (its engine, preset, sound
 * parameters: not param_kept's, its FM6 patch), bit NTRK the DRUM-X kit. Seen at each section's change (song_play.c
 * sec_apply): the sound unlike what the section before put in (snd_h, snd_seen: valid) is the player's; the
 * sections still bring the steps, the pattern, the mix and the mutes. Let go by a throw (song_draft_begin), anything
 * loaded (project_restore_runtime), a live jump or CLIPS (sec_apply, no song), EDIT > RECALL (project_recall_sound) */
static uint8_t snd_held __attribute__((section(".bss.snd"))), snd_seen __attribute__((section(".bss.snd")));   /* (its own
                                              * section: out of the merged statics, whose reach every function pays) */
static uint32_t snd_h[NTRK + 1u] __attribute__((section(".bss.snd")));
static struct {
    chain_config_t config;
    volatile uint8_t armed, running, row, remaining;   /* remaining: the bars of the row still to play */
    volatile uint8_t ended;           /* ISR: the song ended or stopped (song_poll brings the music back) */
    volatile uint8_t applied;         /* ISR: a section went in (song_poll: the FM6 SLOTs, the screen) */
    volatile uint8_t armed_bar;       /* a quick chain asked for while playing: it starts on the next bar */
    uint8_t loop;                     /* the quick chain: round and round (a song stops at its end) */
    volatile uint8_t pass;            /* (0.6.22) the song's times through so far (its END: chain_wraps) */
    uint8_t scn;                      /* the row whose scene is in effect + 1 (0: none since the song began) */
    uint8_t rec, kept;                /* kept: bit 0 snapshot valid; bits 1..3 original section + 1 (0 none) */
    uint32_t carry;                   /* samples past the bar line at the end of the block: the first step's */
    uint32_t bar_pos;                 /* samples into the bar (while playing) */
    uint8_t hold;                     /* (V10.6o) TIMELINE's PLAY: the row round and round (PLAY again: the song goes on) */
} chain;
/* (V10.6o) PARTS / TIMELINE: each section's tracks that play (steps, not muted: bit k), read with the sections' names
 * (ui_graph.c graph_project_used, song_main.c part_scan) */
static union { uint8_t b[4]; uint32_t w; } part_play;
static struct {
    volatile int8_t req;              /* a section asked for (staged), in on the next bar; -1 none */
    volatile int8_t cur;              /* the section playing: jumped to, loaded or stored; -1 none */
    volatile uint8_t srec;            /* SONG REC: 0 off, 1 armed (from the next bar), 2 recording */
    volatile uint8_t nrec;            /* .. its rows so far (the last one still growing) */
    volatile uint8_t rec_done;        /* .. ISR: written, n rows (0xFF: nothing played); song_poll takes them */
    chain_row_t rec[CHAIN_ROWS];
    scene_t rec_scene[CHAIN_ROWS];    /* .. the scene as each row began */
    uint8_t dirty;                    /* sections stored in RAM, not in flash yet (bit s) */
    uint8_t mode;                     /* 1 SONG: PLAY plays the song; 0 LOOP */
} live = {.req = -1, .cur = -1};

/* (V10.6s) SAVE > CLIPS (song_main.c clip_launch): each track's part that sounds, 1..4 A..D, CLIP_STOP stopped, 0 not
 * known (the music is no part as stored), and the one asked for, in on the next bar (0 none). Main loop; derived, not
 * saved: what goes in sets them (song_poll clip_seen) */
#define CLIP_STOP 5u
static uint8_t clip_now[NTRK] __attribute__((section(".lowbuf"))), clip_go[NTRK] __attribute__((section(".lowbuf")));
static uint8_t clip_mix __attribute__((section(".lowbuf")));   /* song_stage stages CLIPS' launch */
static uint32_t rec_src[CHAIN_ROWS] __attribute__((section(".lowbuf")));   /* SONG REC's rows: where their tracks
                                              * played from (sec_stage.src, tsrc's form: CLIPS' launches) */

static void song_mode_restore(void)
{
    if ((chain_config.rsv[2] & 0xFEu) == SONG_MODE_TAG)
        live.mode = chain_config.rsv[2] & 1u;
}

static void chain_start(void);                   /* song_play.c (end of seq.c) */
static void chain_stop(void);
static void chain_tick(uint32_t n);
static uint32_t step_samples(const track_t *t, uint32_t period, uint32_t idx);   /* seq.c (motion.c uses it) */

static void chain_defaults(chain_config_t *c)
{
    uint32_t i;
    memset(c, 0, sizeof *c);
    for (i = 0; i < CHAIN_ROWS; i++)
        c->row[i].bars = 1;
}
/* (0.6.22) a song's END (rsv[0]): 0 it stops after once, 2..8 it plays that many times, SONG_FOREVER round and round.
 * The rows' rsv bytes were zero (older songs: once, as before) */
#define SONG_END(c) ((c)->rsv[0])
#define SONG_FOREVER 255u
static uint32_t song_end_step(uint32_t v, int32_t d)   /* ARRANGE's END: STOP x2 .. x8 LOOP */
{
    uint32_t i = v == SONG_FOREVER ? 8u : v >= 2u && v <= 8u ? v - 1u : 0u;   /* 0 STOP, 1..7 x2..x8, 8 LOOP */
    i = (uint32_t)clamp((int32_t)i + d, 0, 8);
    return i == 8u ? SONG_FOREVER : i ? i + 1u : 0u;
}
static int chain_valid(const chain_config_t *c)
{
    uint32_t i;
    if (c->count > CHAIN_ROWS)
        return 0;
    for (i = 0; i < c->count; i++)
        if (c->row[i].slot >= 4u || !c->row[i].bars || c->row[i].bars > CHAIN_BARS)
            return 0;
    return 1;
}
static const step_t *seq_steps(const track_t *t) { return t->step; }   /* (a section's steps are the track's) */

