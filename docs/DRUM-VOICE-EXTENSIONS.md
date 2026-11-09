# ADD beta: DRUM-X performance octaves and VOICE lyrics

Candidate base: `V10.6c-dxv`. ADD only; no sampler. The public installer and public emulator are not part of this release. The previous working beta remains available.

## DRUM-X

Choose **EDIT → KIT 4 → KEY MAP → EXTENDED**. CLASSIC is the default and preserves the earlier octave behavior. The setting is stored per track in the existing parameter store; no project-format or flash-map expansion.

| OCT | EXTENDED behavior |
| --- | --- |
| 0 | Original GM kit |
| +1 | 27 different 16-step fills, one per physical key; repeat while held, stop at release |
| +2, +3 | Twelve combinations of kick, rim/tom and snare/hat, with per-lane 1/2/4 hits per step |
| −1 | First 13 keys: chromatic kick, −6…+6 semitones; remaining 14: chromatic tom, −6…+7 |
| −2 | Twelve per-hit FX variations: full reverb/delay sends, longer decay, oscillator feedback, fold or noise, depending on key |
| −3 | Existing FLAM touch |

The FX variations do not rewrite the kit or global FX controls. Reverb/delay still require their bus to be enabled. Feedback here means the existing drum oscillator feedback, not a new delay feedback loop. Repeated keys share the original eight drum lanes and the existing held-performance slots per track. Only one extended performance plays at once; a second slot queues its replacement. These modes do not increase hardware polyphony.

With ARP OFF, fills and sets start/switch on the next track step (DIV, FIT and swing apply). The last pressed key replaces the active performance at that boundary. Older held keys do not restart it when the latest key is released. A pending replacement released before its boundary is cancelled. Mild per-step velocity humanization (−4…+3 in MIDI velocity) varies successive repetitions without moving hit times off the tempo grid.

During an active fill/set, that track's original sequencer hits are suppressed. Without REC the saved pattern stays intact. With REC each occupied step is replaced by the performance, including its rests: previous notes, lane hits and ratchets do not get overdubbed underneath. Humanized step velocity is recorded. The recorded step remains when the key is released, even early in the step. Both step components, conditions, probability and parameter automation are preserved; the master remains available. Chromatic and per-hit FX sounds retain encoded note values. GM notes 35–81 are unchanged. In EXTENDED, external MIDI uses 0–12 for kick, 13–26 for tom; FX at 27–34 and 82–85; 27 fills at 86–112; sets at 113–127. In CLASSIC all previous MIDI bands remain unchanged.

## VOICE

**EDIT → WORDS → LANG** appends RU, DE, FR, ZH, PT and GN after OFF/ES/EN/JA. Earlier IDs and factory patches keep their meaning. These are compact phonetic singing approximations using the existing 46 phonemes, not native speech synthesis. New-language W modes have two seed words per theme; free TEXT is the mode for intentional lyrics. Scat uses neutral syllables. Russian soft/palatal distinctions, French/Portuguese/Guaraní nasality and several vowel contrasts are approximated.

FM-1 input remains printable Latin text: Russian transliteration; German `ae/oe/ue/ss`; French `ei` for é, `eh` for è/ê, nasal `a~/e~/o~`; Mandarin pinyin (`v` for ü); nasal `~` for Portuguese/Guaraní. Guaraní `ny` and apostrophe are recognized. Pinyin tone marks/digits do not drive lexical tone: sequencer notes provide the melody. Native Cyrillic can be converted in the beta editor. Hanzi is not converted; the editor requests pinyin instead of silently erasing it. No new screen fonts.

**SEQ → AUTOMATION → LYRIC STEP**:

1. K1 selects the target step.
2. K2 selects a word from the track's TEXT.
3. K3 selects its syllable. Zero places AUTO advance.
4. OCT+ places the choice as an ordinary POS parameter lock and selects **TEXT FIXED**.

TEXT FIXED retriggers the selected POS on every note, so adjacent steps can deliberately repeat one syllable. POS 0 advances normally. Existing TEXT retains its original behavior: a changed POS seeks, an unchanged POS continues. Existing AUTO LIST can remove the POS lock. This shares the existing 64 motion/lock entries; a full store reports an error. USER lyrics and whole-project lyrics use the existing persistence mechanisms.

The beta editor previews transliteration and checks the **63-character limit after conversion**. It rejects unsupported scripts and oversized transliterations rather than sending a silently truncated lyric. Older connected firmware disables unsupported language/mode options according to its descriptors.

## Memory and validation

No extra audio buffers, delay lines, voices, lyric cache, or sample banks. Two bytes of main-loop lyric selection state. Existing English words and ES/EN/JA generator banks are retained: bounded-depth phoneme pair compression, 5-bit alphabet packing, packed group ranges and nibble metadata are lossless. English dictionary search keeps eight words per indexed block; smaller acceleration tables retain the same canonical fallback; English text phonemes remain resolved off the audio interrupt.

Host checks cover every original English dictionary entry and every original generator word, original singer/seek behavior, classic DRUM octaves/recording/MIDI, all 27 unique fills, quantized choke/last-key replacement, humanized recording, fill release/wrap, chromatic/FX ownership and sends, recording, language bounds under ASan, exact syllable locks, project round trip, editor syntax and transliteration. The broader existing workflow checks storage, song generation/save, FIT, USB audio, MIDI, modulation/followers and the known full-UI baseline. Target compilation must pass normal flash/RAM/layout/paired-segment guards before publication. Physical FM-1 validation remains a user test after beta installation.
