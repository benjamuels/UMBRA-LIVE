# Fit a track's steps inside a loop duration

`SEQ > LENGTH > RHYTHM`: **LEN / SPAN / FIT / DIV**. FIT defaults OFF, SPAN to 16.

For 18 steps in the same duration as another track's 16:

| Track | LEN | SPAN | FIT | DIV |
| --- | --- | --- | --- | --- |
| 1 | 16 | 16 | OFF | 1/16 |
| 2 | 18 | 16 | ON | 1/16 |

Start both with PLAY. Track 2 divides the same time into 18 events; both wrap together.
SPAN counts DIV units, not bars: `LEN 5 / SPAN 16 / DIV 1/16` is five steps in one 4/4 bar.
With FIT OFF, LEN and DIV retain their existing polymetric behavior.

The indexed rational clock distributes sample remainders across the steps. At a stable
tempo, every complete fitted loop lasts exactly `SPAN * div_samples(DIV)` samples. Swing
redistributes pairs; the final unpaired step of an odd LEN stays straight so the loop
does not grow. Gates, step LENGTH, ties, ratchets and microtiming use the fitted durations.
Audio processing still runs at its normal sample rate; this does not resample the engines.

SPAN remains selected when FIT is disabled. Editing FIT/SPAN or a fitted track's LEN/DIV
keeps fractional progress within its current step, preventing a burst of catch-up notes.
After changing loop relationships during playback, STOP/PLAY establishes a common origin.
JUMP, REV, HOLD, LIVE rates and extra PLAYHEADS can deliberately change the traversal or
timing; FIT does not override them. Extra PLAYHEADS retain their own DIV-based clock.
Very dense ratios request more note events and can raise CPU/voice demand. On exceptionally
long steps, negative microtiming is capped at 65535 samples to fit the existing early-offset
counter while keeping the loop duration intact.

Settings are per track and per saved section, including arrangement track-source selection,
project reload, undo, autosave, backups and SAVE SONG AS. The optional `RF` extension adds
8 bytes to a stored project only when FIT/SPAN settings are present. Old projects default
to FIT OFF. Parameter IDs, parameter counts, track size and the flash map stay unchanged.

Validation:

- `tests/fit_test.c`: 614400 LEN/SPAN/DIV/tempo/swing combinations; 18:16 through 200 real
  sequencer loops; gates, ties, ratchets, controls, project codec and arrangement staging.
- `tests/fit_storage_cases.h`: NOR flash reload, autosave detection and complete-song copy.
- `tests/ctl64_timing_test.c` with `-DTIMING_FIT`: fitted ANALOG POLY/MONO and DRUM onsets
  against a sample-by-sample sequencer reference, including microtiming.
- Legacy project, USB input and normal-clock audio timing regressions.

Unlike a screen-only change, this changes the sequencer's audio code. The candidate build
records a new layout in the existing code order (`FELUCCA_REPIN=keep`); hardware CPU and
polyphony validation is still needed. The public installer is not changed by this branch.

## Step probability correction

SEQ and STUTTER previously replaced a step's probability with legacy 100%, so a 0% step
could always play. The rendered replacement now keeps the probability configured for its
current pattern position. `probability_component_test.c` exercises the actual sequencer at
0%, 25%, 50% and 100%, for ordinary, SEQ and STUTTER steps on ANALOG and DRUM.
No punch-in FX change is included: the user confirmed recording works in the emulator.
