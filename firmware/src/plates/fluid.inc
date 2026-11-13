/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 UMBRA LIVE contributors */
/* (ui_umbra.c, UMB_RESIDENT: the emulator and the host tests) the fluid plates an install brings, built in: five grounds
 * and the two veils made for SEPIA and FATHOM (tools/build.py BANK_PLATES gives their slots). WAX, RIVULET, BLOT and the
 * veils BOATS, FIZZ, BOKEH, RIDGE, QUILL, MURMUR are plate files no install brings (the user, 2026-11-13: sixteen plates) */
#include "sepia.c"
#include "gyre.c"
#include "aurora.c"
#include "fathom.c"
#include "rapids.c"
#include "koi.c"
#include "medusa.c"
#define UMB_RES_FLUID &umb_plate_sepia, &umb_plate_gyre, &umb_plate_aurora, &umb_plate_fathom, &umb_plate_rapids, &umb_plate_koi, \
                      &umb_plate_medusa                 /* (tests/plate_fluid_test.c) */
