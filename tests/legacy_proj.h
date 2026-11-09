/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (V10.6s) The project formats FUN1 .. FUN6 as Felucca's betas to 1.0.2 stored them, frozen: firmware/src/project.c
 * no longer reads them (they are refused); the tests build such images to check that. Include after project.c. */
#ifndef LEGACY_PROJ_H
#define LEGACY_PROJ_H
#define PROJ_MAGIC_V6 0x46554E36u              /* "FUN6": 69 parameters, drum grid, chain: retired */
#define PROJ_MAGIC_V5 0x46554E35u              /* "FUN5": the grid, without the chain: retired */
#define PROJ_MAGIC_V4 0x46554E34u              /* "FUN4": four tracks, PROJ_NP_V4 parameters, 8-byte steps: retired */
#define PROJ_MAGIC_V3 0x46554E33u              /* "FUN3": four tracks, PROJ_NP_V3 parameters: retired */
#define PROJ_MAGIC_V2 0x46554E32u              /* "FUN2": four tracks, PROJ_NP_V2 parameters: retired */
#define PROJ_MAGIC_V1 0x46554E31u              /* "FUN1": one instrument: retired */
#define PROJ_NP_V2 53u                         /* P_COUNT of formats 1 and 2 (P_E0 was 45) */
#define PROJ_NP_V3 57u                         /* P_COUNT of format 3 (P_E0 was 49) */
#define PROJ_NP_V4 69u                         /* P_COUNT of format 4 (P_E0 61) */
/* Historical FUN5/6 types are frozen, independent of today's P_COUNT/step_t. */
typedef struct { uint8_t note[4], n, time, flags, vel, hit, acc; } step10_t;
typedef struct { int16_t p[69]; uint8_t engine, preset; step10_t step[NSTEP]; uint8_t lane[NLANE][5]; } proj_trk_v5_t;
typedef struct {                               /* format 5, before the song chain */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V2];
    uint8_t sel, parts, phys, rsv;
    proj_trk_v5_t t[PROJ_LT];
    uint32_t sum;
} project_v5_t;
typedef struct { uint32_t magic, size; int16_t g[PROJ_NG_V2]; uint8_t sel, parts, phys, rsv;
    proj_trk_v5_t t[PROJ_LT]; chain_config_t chain; uint32_t sum; } project_v6_t;
_Static_assert(sizeof(project_v5_t) == 3352u && sizeof(project_v6_t) == 3388u, "frozen formats 5 / 6 sizes");
typedef struct {                               /* a track of format 4 */
    int16_t p[PROJ_NP_V4];
    uint8_t engine, preset;
    step8_t step[NSTEP];
} proj_trk_v4_t;
typedef struct {                               /* format 4 (1.0 development builds) */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V2];
    uint8_t sel, parts, phys, rsv;
    proj_trk_v4_t t[PROJ_LT];
    uint32_t sum;
} project_v4_t;
typedef struct {                               /* a track of format 3 */
    int16_t p[PROJ_NP_V3];
    uint8_t engine, preset;
    step8_t step[NSTEP];
} proj_trk_v3_t;
typedef struct {                               /* format 3 (0.9 .. 1.0) */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V2];
    uint8_t sel, parts, rsv[2];
    proj_trk_v3_t t[PROJ_LT];
    uint32_t sum;
} project_v3_t;
typedef struct {                               /* a track of formats 1 and 2 */
    int16_t p[PROJ_NP_V2];
    uint8_t engine, preset;
    step8_t step[NSTEP];
} proj_trk_v2_t;
typedef struct {                               /* format 2 (until 0.9) */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V2];
    uint8_t sel, rsv[3];
    proj_trk_v2_t t[PROJ_LT];
    uint32_t sum;
} project_v2_t;
typedef struct {                               /* format 1 (until 0.5 beta) */
    uint32_t magic, size;
    int16_t g[PROJ_NG_V2];
    proj_trk_v2_t t;
    uint32_t sum;
} project_v1_t;
_Static_assert(sizeof(project_v2_t) == 2552u && sizeof(project_v1_t) == 688u && sizeof(project_v3_t) == 2584u &&
               sizeof(project_v4_t) == 2680u, "formats 1 / 2 / 3 / 4 as they were stored");
#endif
