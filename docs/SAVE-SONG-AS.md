# Save a complete song

Based on the public V10.6c firmware (`d993eae82ff33ead15daf96f2de1d1779f275f8c`).

Open **SAVE > PROJECT > SAVE SONG AS**. KNOB 1 chooses destination project 1–8 without
opening it. KNOB 4 arms SAVE; OCT+ continues. A used destination asks **REPLACE WHOLE
SONG n? / ALL A-D + ARRANGEMENT**. Confirm with OCT+, type a name, then OCT+ saves.
OCT- cancels either dialog. Stop playback before saving.

The copy includes:

- Sections A–D, including the current section's unsaved edits.
- Current tempo and global settings, written into each section so loading A restores them.
- Arrangement rows, bar counts, END/repetitions, scenes, per-track source sections and variations.
- LOOP/SONG mode, restored when selecting the saved song. Previously saved projects keep their existing mode behavior.

The source stays open and unchanged. Open the destination in PROJECTS to use the copy.
The name belongs to section A, as with existing project names. If A is empty, the live
music supplies A. Sections absent from the source clear old destination sections using
CRC-protected zero-length storage records. Saved copies no longer carry GEN ownership
bits, protecting them from a subsequent generation.

No storage format sizes or flash regions change. The copy reuses existing stopped-song
and editor staging buffers. The audio ISR, DSP and USB input path are unchanged.

## Failure behavior

Preflight validates serialization and arrangement dependencies before any destination
write. Each section and the final index use the existing verified A/B storage writes.
An error is reported, the naming dialog remains open, and the source stays available
for a retry. An overwrite of all four sections plus index is **not atomic as a whole**:
power loss or a write failure midway can leave a partially replaced destination. The
original song is not erased or replaced by this command.

## Validation

`tests/persistence_test.c` includes `song_save_cases.h`: actual NOR storage, full reload,
unsaved C, global tempo, scenes/source/variation metadata, empty sections over old data,
invalid targets, pending PLAY, missing sections, erase failure and index-write failure.

`tests/ui_test.c` includes `song_save_ui_cases.h`. Build with `-DSONG_SAVE_ONLY` to run
the new controls, dialogs, cancellation, failed-save behavior and SAVE navigation.
The full pre-existing UI suite has unrelated failures on the V10.6c base; compare against
the unchanged source before attributing those failures to this feature.

The branch CI builds an FM-1 package and checks target flash/RAM/layout limits. It does
not deploy the public or beta installer. Hardware validation is still required.
