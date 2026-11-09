# ADD modulation and song beta

This candidate builds on the published VOICE/SPLIT ADD beta. It has no sampler.
Only the preview installer is intended to receive this candidate after target checks.

## Song generator

All three GEN pages display the actual arrangement when it exists: sections, bar lengths,
current row, remaining bars and play position. The display refreshes at bar
boundaries. A generated SONG can have 16- or 32-bar sections, so hearing the same
phrase for several bars does not itself mean the arranger is stuck.

SAVE from GEN opens SAVE SONG AS. Choose LOOP or SONG and a destination project, confirm an
existing destination if needed, then name it. SONG copies sections A–D, arrangement,
scenes and per-track source/variation metadata. LOOP writes the current music to A,
clearing B–D and the arrangement at that destination. The current project can be
overwritten explicitly. Stop playback before saving.

GEN now uses a RAM draft; saved A–D no longer block generation. Subsequent
generations do not overwrite saved projects. MAKE during playback stops,
regenerates and resumes the new draft. Projects and individual sections can be
erased explicitly from SAVE. See [draft and save behavior](GENERATOR-DRAFT-SAVE.md)
for the complete flow and persistence limits.
Explicit SONG/LOOP mode is restored from the existing tagged project index field;
legacy projects without the tag retain their default behavior.

## Track envelopes in the matrix

New sources T1 ENV through T6 ENV measure each track's stereo audio amplitude.
For example, on T3 set SRC=T1 ENV, DST=F.CUT and a positive AMT: track 1 opens
track 3's filter. A negative AMT moves it in the opposite direction. LEVEL is
also available as a destination for cross-track level movement.

The follower reads the track after its local processing, including stereo IMAGE,
and approximates its output level using the smoothed end-of-block LEVEL/PAN
and duck gain. Shared effect returns and legacy direct INPUT fallback are not
included. An INPUT engine track is measured normally. Attack moves halfway to
its peak per block; release uses the input follower's smoothing with rounding
that reaches zero. Publication is one block late, making cross-track routes
independent of track rendering order and avoiding an instantaneous feedback loop.

Only requested source tracks receive a peak scan; engines are not rendered a
second time, and no audio buffers or voices are allocated. The added envelope
state uses approximately 28 bytes of static RAM. There are no hardware CPU
measurements yet. Existing source/destination IDs remain unchanged.

F.CUT controls the audible track filter (the ANALOG engine uses its dedicated
cutoff parameter). Existing CUT now reaches the track filter on WHEEL, DRUM-X,
STRING and INPUT, whose renderers did not consume the voice cutoff value.
Other engines keep their existing CUT behavior. INPUT's former FOLLOW source
is displayed as IN ENV to distinguish it from track envelopes.

## Additional fixes

- Inactive tracks no longer disable the direct STEMS+IN fallback merely because
  their saved engine is INPUT; their master matrix routes are also ignored.
- ENV/ENV2 return zero for track/master routes when the owning voice has ended.
- INPUT meters and envelopes decay fully to zero.
- The matrix graph shows the source beside each of its four routes, keeping all
  sources selectable without trying to stack 24 source labels on the screen.
- Loading a project clears previous track envelope state.

Decorative pseudo-note previews are replaced by real stored sections or the
actual arrangement on every GEN page, without removing sound controls.

No existing matrix source was removed. No new step-component opcode is added:
the existing component encoding is full. Resonance was not appended as a matrix
destination because the existing 32-bit destination masks are now full.

## Validation

Host checks cover generated parts across 45 genre choices, traversal through all
six arrangement rows and wrap, real playhead invalidation, SAVE entry, regeneration
during playback and saved playback mode. Separate NOR/persistence tests cover both
save scopes, draft preservation, failure/retry, erase and round-tripping matrix IDs.

The matrix tests cover inactive INPUT, ended FM6 ENV2, cutoff mappings, restored
parameters, silent tracks, opposite-polarity stereo signals, one-block snapshots,
release to zero and demand-driven scanning. An AddressSanitizer/bounds sweep
completed 11,408 valid source/destination configurations across 16 engines.

The full UI suite is compared against the same pre-split baseline rather than
claiming its existing 18 failures are fixed. The generator's existing audio
balance test failure was also reproduced on the baseline. Target memory, layout,
RAM-call reach and package checks must pass before the beta is published.
