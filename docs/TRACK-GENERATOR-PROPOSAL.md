# Selected-track generator (ADD beta)

Implemented in `V10.6c-gen`, after the DRUM/VOICE beta. ADD only, without a sampler. Public releases and the preceding beta packages stay available unchanged.

Open the last page of a track's **SEQ → GENERATE** chain. K1 selects GENRE (including RANDOM and EXPLORE), K2 SEED, K3 ROLE on tonal tracks (BASS, CHORDS, LEAD, ARP) or COMPLEX on DRUM-X, and K4 ENERGY. **OCT+ / MAKE** replaces the selected track's phrase. The existing **GLO + G4 UNDO** restores the previous patterns. The page previews the existing drum grid or piano roll. INPUT has no GENERATE page.

For example: select DRUM-X, DNB and MAKE; select a tonal track, DNB, BASS and MAKE; play the other tracks yourself. A fixed seed and settings reproduce the same phrase; change SEED to obtain another variation. The role changes the notes, not the selected engine or sound. CHORDS generates up to four notes; choose a polyphonic patch when you want to hear them together.

The phrase uses the current LEN (1–64), root and scale, keeping DIV, FIT, swing, mixer, FX, automation and locks. It preserves the other tracks, HOME generator settings/context, parts and arrangement. Notes outside the current LEN are retained. Loop playback can continue: generation runs into a temporary track on the main stack, followed by a short interrupt-guarded copy. The replacement becomes available to the next sequencer step; it is not queued until the next bar. Already sounding voices release normally. Generation is disabled while an arrangement is armed/running, as with the other sequence editing operations.

The four controls have independent per-track settings for the current session. Those controls themselves are not added to the project format; generated notes are saved through the existing project/part save functions. HOME's complete-song generator remains available.

## Flash and execution

Existing `gen_role` algorithms are reused, with isolated/restored HOME context. No new DSP, sample processing, voices, audio buffers or pool allocation. Configurations take 36 bytes; temporary generation uses the existing 15.75 KiB main stack (compile-time size assertion). The loader, partitions, audio anchors, audio RAM code reserve and core-1 pool reserve remain enforced.

Lossless glyph records use five bytes instead of eight; font alpha coding selects the smaller supported run alphabet. The renderer specializes away phase handling only when every generated face has one phase, retaining the generic path for other builds. All four faces retain every pixel, width, kerning pair and advance. The existing four Huffman cache slots are shared; no extra cache allocation. Original ABOUT/license/credit notices stay readable in source, with an acyclic, depth-checked byte-pair representation in the paired segment; every original byte is restored. The two track-control helpers use the recovered segment space through the existing CRC-checked gate.

Regression checks cover 164 engine/role/LEN combinations, every drum genre, EXPLORE, every selected scale, deterministic generation, all nonselected tracks, HOME state, sounds/mix, locks, FIT, live transport, project round trip, UNDO and arrangement edit protection. Font checks compare 4,488 string/font/palette combinations, all kerning pairs and keycap pixels, plus 10,002 metadata round trips and wide/multiphase fallbacks. Broader firmware checks and real target memory/layout validation are required before beta publication; physical FM-1 validation follows installation.
