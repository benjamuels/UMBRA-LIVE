/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FELUCCA: one compilation unit (the HAL is header-only). Each file uses what the files above it
 * define, so the order matters. */

/* ---------------------------------------------------- build options --- */
/* build.py: FELUCCA_<NAME>=0 / 1 in the environment overrides these. Elsewhere: FELUCCA_FM4
 * (core.h: the DIGITAL engine), FELUCCA_ICONS (icons.c, the parameter icons; on by default),
 * FELUCCA_OTA_RAMONLY (ota.c). */
#ifndef FELUCCA_FLASH
#define FELUCCA_FLASH 1          /* flash driver + storage.c: settings, projects, user presets */
#endif
#ifndef FELUCCA_OTA
#define FELUCCA_OTA 1            /* M-UPGRADE update entry and the editor SysEx; needs FELUCCA_FLASH */
#endif
#ifndef FELUCCA_OTA_DRYRUN
#define FELUCCA_OTA_DRYRUN 0     /* 1 = stage, ask "success", then undo: no record, no reset */
#endif
#ifndef FELUCCA_CDC
#define FELUCCA_CDC 0            /* USB CDC-ACM serial console (UMBRA OBSCURA: off, its ~7 KB of flash and ~3 KB of
                                  * RAM for the plates; a build with it: FELUCCA_CDC=1) */
#endif
#ifndef FELUCCA_UAC
#define FELUCCA_UAC 1            /* USB audio input: the master output, 16-bit stereo 44.1 kHz (usb.c) */
#endif
#ifndef FELUCCA_UAC_TONE
#define FELUCCA_UAC_TONE 0       /* bench: the USB input sends test triangles instead of the music */
#endif
#ifndef FELUCCA_UART
#define FELUCCA_UART 1           /* TRS MIDI IN on UART1 / PH8 */
#endif
#ifndef FELUCCA_DUAL_CORE
#define FELUCCA_DUAL_CORE 1      /* (JIANT 0.6.71) the second core renders some synth tracks (hal/fm1_multicore.h) */
#endif
#ifndef FELUCCA_HOTRAM
#define FELUCCA_HOTRAM 1         /* (JIANT 0.6.78, experimental) core 0's hottest audio code in RAM (core.h) */
#endif
#ifndef FELUCCA_BT
#define FELUCCA_BT 1             /* (V10.12) MENU > SYSTEM > BLUETOOTH: bt.c, bt_hw.c, the segment firmware/bt/bt_seg.c */
#endif
#ifndef FELUCCA_USB_HOST
#define FELUCCA_USB_HOST 0       /* (V10.41) MENU > SYSTEM > USB > HOST: a USB-MIDI keyboard on the port (usb_host.c;
                                  * UMBRA OBSCURA: off, no USB input) */
#endif
#ifndef FELUCCA_UAC_48K
#define FELUCCA_UAC_48K 0        /* (usb.c) USB audio at 48 kHz too (UMBRA OBSCURA: off, 44.1 kHz only: ~5.5 KB of flash and
                                  * ~16 KB of pool, where KILN's field goes) */
#endif
#ifndef FELUCCA_ID
#define FELUCCA_ID "FM-1_900"    /* package identity (build.py: the .fwsc marker string) */
#endif
#ifndef FELUCCA_VERSION
#define FELUCCA_VERSION "V10.6c" /* (2026-11-03) V10.61: PROX tuned (the user: it works with the phone, but the phone had to go far away for PROX to let go; a narrower field, a quicker response): the PROX page's KNOB 1 MIN (0 .. 90 %: under it PROX is 0), KNOB 2 MAX (100 .. 10 %: over it PROX is full), both over the calibrated FAR .. NEAR, 10 % apart at least; KNOB 3 SPEED 1..5 (the level's smoothing 1/32 as before .. 1/2: quicker, noisier); saved as FAR / NEAR; 0, 100 and 1 as V10.60. (2026-11-03) V10.60: room (the user's ask): the conversion of DIGITAL's sounds to FM6 (fm4_convert.c) and the read of JIANT 0.1-0.5's projects (FUNB..FUN7, project.c proj_unpack) moved into the Bluetooth segment (without it: a DIGITAL sound loads as the FM6 factory preset that covers it, those old projects are not read); the app slot 4.3 KB free (was 0.8), the segment 2.7 KB; BTAPP_FN / BTAPP_RO in core.h. (2026-11-03) V10.59: FX > RPT 1/8 at slow tempos (the user: dimmed at 61 BPM): a REPEAT longer than the loop at 22 kHz (371 ms) records at 11 kHz, lo-fi (the effect's), up to 742 ms: 1/8 from 41 BPM (was 81), 1/16 from 21 (was 41); the faster ones as before, bit for bit. (2026-11-03) V10.58: users' reports: the master EQ (and TAPE) went back to flat on its own (going to GLO's pages, since V10.44): every read of a stored project or section (a slot checked, a section listed) set the EQ and TAPE playing; they now live in the project read and are applied only when it is loaded; MENU in BOTTOM and HUD showed its four cards under the panel (a blank band over it): the panel right under the tabs there, the cards at the foot as on every page of theirs; AIR's HOME: the beings swim under its cards (they crossed the tabs and the values); FX > EQ with SCOPE ON: the master's scope behind the curve. (2026-11-03) V10.57: USB > HOST takes an audio interface at 48 kHz too (the user's OP-Z, and the many interfaces with no 44,100): set to 48,000 when it has no 44,100, read at the I2S clock from a step of 48 / 44.1 (the reader's interpolation, its drift steering as before); packets of up to 50 frames taken. (2026-11-03) V10.56: USB IN+OUT no longer mixes the computer's or the phone's audio into the last track by itself (the user: recording from KOALA with IN+OUT on looped into a digital feedback, the app sending the FM-1's own output back): the input sounds where an INPUT track takes it; STEMS+IN keeps its own stem as before. (2026-11-03) V10.55: GEN's sounds right again: V10.42 replaced FM6 presets 8..12 and 14 and WHEEL 5, 6, and GEN still picked them by number for its roles (a SUB BASS played the bells, leads the electric pianos and plucks, a pedal bass the organs): its BELL EP PLUCK STRINGS SINE KEYS ORGAN DUB CHORD and KEYS take today's nearest (BELL MARIMBA TINE EP PLUCK PAD WAVE ORGAN GOSPEL JAZZ PERC); INPUT's meters keep FOLLOW's frame with the input silent; the host tests brought up to date with the UI (the DJ FILTER on LIVE > MANO, the roll's 13 px columns, INPUT in the engine lists). (2026-11-03) V10.54: the INPUT engine (the user's ask: the audio input routed, the track in an audio mode, an envelope follower): engine 15 plays the USB audio input (an interface on HOST, or the computer's USB IN+OUT) on its track: SOURCE IN 1 / IN 2 / 1+2 / STEREO (two tracks can take IN 1 and IN 2; STEREO keeps the image through the part's chain as IMAGE does), GAIN, GATE, PLAY THRU / KEYS (only while a note plays, at its envelope: a trance gate from the steps), WIDTH, RING (KEYS: a sine at the note), BITS, RATE; the track's FILTER DIST LEVEL PAN sends as any part's; EDIT INPUT shows both inputs' meters, GATE's level and FOLLOW; while a track is INPUT the last track's IN+OUT mix steps aside; MOD SRC FOLLOW, the input's envelope, on any track and any destination. (2026-11-03) V10.53: USB > HOST takes an audio interface too, an experiment (the user's ask: a USB audio input into the synth): a class compliant USB audio 1.0 capture (16-bit, mono or stereo, 44,1 kHz among its rates: Behringer UM2 / UCA and the like), alone or behind a powered hub, set to 44,1 kHz (SET_CUR) and read each frame (isochronous IN) into IN+OUT's ring: it plays through the last track (its LEVEL PAN and sends, as USB IN+OUT's host audio); usb_host_desc.h uh_parse_uac, tested; not yet on hardware. (2026-11-03) V10.52: a synth's STEREO page shows IMAGE at work (the user: no visual feedback): the field L .. R, each voice sounding a blob at its place, its size its level now, its note over it (IMAGE 0: the notes in a row), the silent ones a tick where they would sit, the span IMAGE opens as a bar. (2026-11-03) V10.51: the roll follows the playhead for real (the user: V10.47's did not): the bar playing was chosen in the roll's drawing, and a playhead out of the bar shown changed nothing the panel's redraw looks at (graph_signature): never drawn, never followed; now chosen there. (2026-11-03) V10.50: the user's fixes to V10.49: a synth's STEREO IMAGE its own parameter (P_SIMG, the lane-1 level's slot, stored inverted: every part's 127 = 0 %; on VOICE V10.49's showed and moved LYRIC, the slot VOICE uses), its own STEREO page; EDIT's order: the sound's pages first, VOICE and VOICE 2 last in a group of their own (SOUND TONE FOCUS VOICE; FM6: SOUND TONE OPS VOICE). (2026-11-03) V10.49: users' asks: FX > DLY 3 KNOB 4 R TIME, the right echo its own time as a ratio of TIME (=L as before, x1/2 x2/3 x3/4 x4/3 x3/2 x2; not with PING-PONG or the grains; saved, project block DR); EDIT > STEREO IMAGE on synth parts in POLY: each voice its own place across the stereo field (DRUM-X's way: what each side gives up, through the part's filter, narrowed by DIST), the page shows the eight voices' places. (2026-11-03) V10.48: room: LOFI's, ANIMAL's and STRING's creatures (ui_engvis.c, ~6 KB) moved into the Bluetooth segment (without it: the being, as other engines); the app slot ~6 KB free again, the segment ~7 KB. (2026-11-03) V10.47: the roll and the drum grid follow the playhead (a user's ask): while it plays and nothing was touched for 3 s, the bar shown is the one playing; a key or a knob holds it 3 s. (2026-11-02) V10.46: a safe start (reports of FM-1s that stay dark after installing; the cause not known): a start that hangs between the supply setup (V10.11, hal/fm1_power.h) and the UI is reset by the watchdog, and the next start skips that setup (the boot loader's supplies, as up to V10.10) until the power goes off, and says SAFE START. (2026-11-02) V10.45: LIVE > MANO KNOB 4 the DJ FILTER again (a user's report: V10.37 turned it into the tempo, MIXER's new KNOB 4); EDIT > MORPH on ANALOG PWM SYNC RING SAW3 PLS3 draws the wave as MORPH bends it (the user: a ramp), in the Bluetooth segment. (2026-11-02) V10.44: the rest of the mock-up's pictures, in the Bluetooth segment (the app slot full; without the segment the page keeps its old picture): FX > MASTER > EQ the four bands' curve (LOW / HIGH shelves, the mids bells, a dot each, 60 250 2K 8K); GLO > SYSTEM 2 the three ports as boxes (USB TRS BT, their state), a light each way flashing as MIDI passes, PROX's bar when Bluetooth is linked or in AIR; LEVELS' picture moved there too. (2026-11-02) V10.43: pictures that show what the page does (the user's picks from the mock-up): EDIT > LEVELS / LEVELS 2 a column a DRUM-X sound, its LEVEL the height, lit as it plays, the page's four framed (was the scope); FX > MASTER the CLIP curve (in -> out, the level now a dot), PUNCH's push as a bar, the duck's gain now a line over its curve; FX > PUNCH the lane 16 steps a row, every step a box, a recorded one its punch's colour, the playhead framed (an empty lane drew nothing). (2026-11-02) V10.42: the factory presets less alike, the basics there (the user's ask): FM6's seven near copies (ICE BELL DX KEYS FM PLUCK SOFT BELL FM STRINGS WOOD, the first eight bent a little) are other instruments through the macros: FM LEAD SQR LEAD SAW LEAD, SLAP BASS SUB BASS, BELL PAD (program numbers kept, HARD BASS stays); WHEEL PEDAL BASS and DRIVE LEAD where DUB ORGAN and SOFT KEYS were; LOFI PULSE BASS appended. (2026-11-02) V10.41: MENU > SYSTEM > USB > HOST (a user's ask, an experiment): the port as the host of a class compliant USB-MIDI keyboard or controller, alone or behind a hub, what it plays in as USB-MIDI (CLK USB+BT, LEARN, MAP alike); Melodee's host (usb_host.c, recursivefieldinstruments' enumeration), input only, its main-loop side in the Bluetooth segment (the app slot full); never saved (every power-on a device); the port is not powered: a keyboard on its own power or a powered hub. (2026-11-02) V10.40: the editor sees a mode's own knobs (ANALOG SAW3 / PLS3 INT 3 -24..24, STRING FDBK's, NOISE LFSR's: editor.c ed_tdesc as the panel, a WAVE / EXCITE set from the editor resets the knobs that change meaning, the page re-reads them); HOME's lazy redraw mends a missed cell within ~10 s (a strip's n / 128 + 1 cells sent anyway a frame, was one: up to ~95 s on HOME's panel; ~4 % more sent). (2026-11-02) V10.39: ANALOG SAW3 and PLS3, each oscillator its own interval (the user's ask): NOISE's knob (EDIT 1 KNOB 4) reads INT 3 there, osc 3's interval in semitones (-24..24; INT stays osc 2's), no noise in those two WAVEs (every factory SAW3 has NOISE 0: the same sound); turning WAVE into or out of them sets that knob to 0. (2026-11-02) V10.38: intervals in tune at random (the user: GEN's ANALOG sounds took a random INT, the song out of tune): whatever chooses an engine's interval by chance (GEN and EXPLORE, the sound dice EDIT + SELECT) picks 0, a fourth, a fifth or an octave, up or down (ANALOG INT, VOICE SHIFT; GEN keeps a preset's, snapped to the nearest: GLASS KEYS' 19 kept); the random macros never aim at an interval. (2026-11-02) V10.37: GLO > MIXER KNOB 4 BPM (the DJ FILTER on LIVE > MANO), GLO's pages LIVE MIX COMPS GLOBAL SYSTEM; Bluetooth named where MIDI is (the user's ask): GLOBAL > CLOCK reads USB +BT (USB's source follows Bluetooth's clock too), GLO > SYSTEM 2 KNOB 3 shows BT's state (OFF, ADV, CONN n, AIR) beside USB and TRS IN. (2026-11-02) V10.36: LFO WAVE DRIFT (a user's ask, ambient drones and pads that breathe): a smooth random wander, each cycle a new value within half the range of the last, reached on a smoothstep (no step, no corner; with RATE 4BAR..16BAR a slow drift); the factory pads breathe by themselves (BREATHE in the presets' spare byte: DRIFT on FILTER, some a little on PITCH, over 4 or 16 bars): ANALOG SOFT PAD STRINGS JUNO PAD DARK PAD, FM6 PAD FM STRINGS, PHASE GLASS PAD VOX PAD, NOISE HISS PAD, LOFI CHIP PAD. (2026-11-02) V10.35: MENU > SYSTEM > BLUETOOTH AIR, an experiment: the radio listens instead of advertising (passive scan, nothing sent, no MIDI) and every Bluetooth packet heard around (phones, watches, headphones, TVs) feeds PROX: a body near the antenna shadows them, PROX's page and the MS_PROX source as with a phone (Optimist's scanner, fm1-base 4039a8b, minimal central level 3, in the segment: 56 KB of 72). (2026-11-02) V10.34: DRUM-X kit HIP HOP (a user's ask: boom bap, a round kick with a knock, a fat cracking snare, a tight clap, dusty band-passed hats, a woody rim, a shaker in the BELL's place) and the pattern BOOM BAP it suggests (SEQ > PATTERNS 14). (2026-11-02) V10.33: the MIDI clock steady. Out (MIDI OUT CLOCK / ALL): each of the 24 pulses at its own sample, sent at its own time (USB: usb.c ep1_tx holds it to its TIMER4 tick; Bluetooth: stamped with it), no longer a burst each 2.9 ms render, and no rounding in the pulse length (a follower read 105.1 for 104). In (CLK USB / TRS, Bluetooth too): the tempo over two beats, smoothed, a whole BPM held within 0.6 (it was six pulses raw: 103 105 104..), the interpolation at the steady tempo. (2026-11-02) V10.32: Bluetooth MIDI pairs: Just Works (no PIN) with bonding and the link encrypted when a central asks (Windows, some Macs and Androids refused "Pairing Not Supported" before); the bond's key made again from EDIV / Rand (bt/ble ble_smp_ltk_of), nothing stored, every bonded computer back after a power-off; Optimist's AES / CCM (ble_aes.c, fm1-base 4039a8b) in the Bluetooth segment (51 KB of its 72). (2026-11-02) V10.31: EUCLID reversible: the sequence it took is kept whole (the last track taken, 1.5 KB in RAMTEXT's room: notes, chords, velocities, components, micro), put back when its settings are at rest again (VARY and MODES 0, its own rhythm) or HITS / DENSITY go to 0; another track's taken since: its rhythm and notes come back from what EUCLID took. SCL > SCENE kept with the project (FUNC 'S' 'C': ON and the six roles; none: OFF, all KEEP). (2026-11-02) V10.30: flash by hand: a parameter card in one place (pf_column: eleven cards of ui_draw.c), "a/n" in one place (fmt_frac: eight); every screen pixel for pixel as before (9250 renders hashed): ~0.6 KB more free. (2026-11-02) V10.29: flash: the files added since 0.9.7 (EUCLID, SCENE, LIVE, MIDI LEARN, the song player, PERFORM, PLAYHEADS, SLICER) and the engines' main-loop side built for size (tools/size_fns.py; what the interrupts reach keeps -Os): ~1.8 KB more free, the sound as it was. (2026-11-02) V10.28: EUCLID starts at 0 (no HITS, no DENSITY, MODES and VARY 0: built from nothing); a sequence the track has (recorded, edited, a song's; not a factory pattern) is transformed: its rhythm and holds until HITS STEPS or ROTATE are turned, its notes always (a kit: its lanes); VARY in two halves: up to 50 the one cycle moved, each value its own (one hit at least), above 50 the four turns A A' A'' B. (2026-11-02) V10.27: testers' fixes: the CHORD page no longer writes VOICING LEAD's last chord (a key played another inversion after the page was shown: chord_peek); MENU: KNOB k edits its card's row (all four edited the selected row); TAPE WOW wobbles like a warped record (a smoothed random drift, ~+-45 cents at 100; its noise source was stuck at 0: the hiss and drift were dead); slower: LFO RATE 8BAR 16BAR, PLAYHEADS 1/8 1/16, STRUM past 64 up to ~1 s. (2026-11-02) V10.26: SCENE starts with every track KEEP (OFF, as before); OCT+ on SCENE dices the roles (each role one track at most: never two doing the same, never two playing chords), OCT- all KEEP; turned ON with none set, it dices; a role picked by hand that another track has sends that one to KEEP. (2026-11-02) V10.25: SEQ > STRETCH's NUDGE in two modes: OCT- the whole sequence a step (blue, as before), OCT+ the cursor step's MICRO timing (red, 64ths of a step; PRESETS moves the cursor), MICRO back on a knob (a tester's ask). (2026-11-02) V10.24: the user's picks from the redesign mock: SEQ > EUCLID the lanes as concentric rings (heat colours, VARY's turns under the ring); SEQ > MUTATE the anchor above, the pattern playing below (changed steps lit), an arc to the next mutation; SAVE > PRESETS two columns (the engines, their presets), OCT+ keeps the sound browsed to, OCT- brings back the one before; SAVE > TOOLS by scope (KNOB 1 TRACK SOUND SONG SYSTEM, KNOB 2 the action), OCT+ held a second does it (no dialog); SCL > SCENE, the multitimbral mode: one key plays every track given a role (KEEP CHORD ARP BASS LEAD; the kit keeps its sequence), REC records each into its own steps. (2026-11-01) V10.23: STRING FDBK heard (its BLOOM level was applied twice, -15 dB); EXCITE into / out of FDBK keeps each side's EDIT 2 knobs (GAIN no longer starts at STRETCH's 0), FDBK's defaults audible. (2026-11-01) V10.22: testers' asks: ENV > ENV 2 (a second ADSR, MOD SRC ENV2, any DEST), FILTER TYPE LP24 (4 poles), ANALOG WAVE PLS3 (three pulses, SHAPE the width). (2026-11-01) V10.21: EUCLID's lane rows named right (they were one off: ALL's name on BD's row); the ring shows one cycle (the lane's, ALL's, the melody's) and the turn playing, its playhead going round; PROX's page says the phone, not the hand. (2026-11-01) V10.20: PROX smooth: the RSSI reading fixed on word 0 as the level (V10.18's widest range picked a noisy byte: a random sample and hold), the median of nine and ~0.3 s; FAR and NEAR too close (under 4 levels) refused. (2026-10-31) V10.19: the PROX page redraws when a reading arrives (all-zero words left it on NOT CONNECTED) and shows the link as seen: L (link), the RSSI hook's packets / the good packets. (2026-10-31) V10.18: PROX finds the RSSI reading that moves (the four RSSI words of each packet, twelve readings, the widest range on the link; kept once FAR / NEAR is stored), the page shows the four words. (2026-10-31) V10.17: SAVE > SECTIONS KNOB 2 ERASE (the section empty again, RAM and flash; the music stays). (2026-10-31) V10.16: SEQ > EUCLID's VARY in four turns (A A' A'' B, each moved anew, the motif answered); SEQ > MUTATE (EVERY 1..16 bars, AMOUNT, MODE ORBIT / DRIFT / PULSE, WHAT GROOVE / RHYTHM / FEEL / ALL; OCT+ a mutation now, OCT- back to the anchor). (2026-10-31) V10.15: VOICE's English dictionary in the Bluetooth segment (without it: the letter rules). (2026-10-31) V10.14: PROX, a modulation source from the Bluetooth signal (a hand near the antenna), calibrated on MENU > SYSTEM > BLUETOOTH ON + OCT+ (OCT- FAR, OCT+ NEAR). (2026-10-31) V10.13: BLE-MIDI in and out (what a central sends plays as USB-MIDI, MIDI OUT goes to it too); ABOUT's credits in the Bluetooth segment (the app slot had 128 B free). (2026-10-31) V10.12: MENU > SYSTEM > BLUETOOTH OFF / ON: a BLE-MIDI peripheral "JIANT FM-1" (advertising, connectable; MIDI not routed yet), Optimist's stack in the Bluetooth segment at 0xEA000 (written by the update loader from the package's bt.bin), live RF calibration, a boot guard. (2026-10-31) V10.11: the supplies set up as stock V15 does (the DC-DC on, hal/fm1_power.h from Melodee), the radio's prerequisite; 0xEA000..0xFBFFF reserved for the Bluetooth segment. (2026-10-31) V10.10: the radio's calibration kept: the update loader keeps the stock VM sector (0xE8000: RF trims, BT address) but its record's 256 B; VM_READ (editor 80) reads that area for web/btcheck. (2026-10-31) V10.9: MENU > LARGE shows the animations (HOME: the six tracks' creatures in the strip; EDIT: the engine's). (2026-10-31) V10.8: testers' fixes: GEN parts set OFF stay off; GEN keeps its GENRE between visits; SEQ > STEP KNOB 4 VEL (1..127, then ACC SLD A+S); delay 1/4D 1/8D 1/16D; less pitch wobble from the generators (no macro on PITCH, pitched delay seldom, octaves); STOP TO EDIT says SONG PLAYS. (2026-10-31) V10.7: SEQ > EUCLID on a kit: LANE ALL first (DENSITY: a whole usable kit), a lane picked writes that lane alone; EDIT + PRESETS jumps between engines; the kit's SEQ dice plays in every genre; EUCLID's HITS exact (VARY moved them only); REC held alone and let go: UNDO (again: REDO). (2026-10-31) V10.6: SEQ > EUCLID and MODES (a euclidean pattern generator, melodies in the key) in MUTATE's place; MUTATE and SEQ TOOLS' RANDOM COOK BEAT gone. (2026-10-31) V10.5: automation thinned (a recording at the stop, the 64 when full) into holds and RAMPs. (2026-10-31) V10.4: ARRANGE / TIMELINE: OCT- deletes the row picked (DELETE ROW?). (2026-10-31) V10.3: TIMELINE: the lines between lanes soft (the block's colour, darker), the rows' edges stronger. (2026-10-31) V10.2: TIMELINE keeps the black line between lanes (only the rows touch); SAVE composes first: TIMELINE SECTIONS PROJECTS ARRANGE TOOLS USER PRESETS. (2026-10-30) V10.1: TIMELINE blocks fill the lanes (no black gaps between rows or tracks). (2026-10-30) JIANT FM V10: add15-beta6 as the new base (TIMELINE, VAR, ADD, official V16 both ways); the version history before it: the git log of the add15 line (codex/add15-private) */
#endif
#if FELUCCA_OTA && !FELUCCA_FLASH
#error "FELUCCA_OTA needs FELUCCA_FLASH"
#endif
#if FELUCCA_BT && !FELUCCA_FLASH
#error "FELUCCA_BT needs FELUCCA_FLASH"
#endif

/* ------------------------------------------------------------- HAL --- */
#include <stdint.h>
#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_power.h"                  /* (V10.11) the stock supply setup (main.c, before the rails) */
#include "fm1_irq.h"
#include "fm1_guard.h"
#if FELUCCA_DUAL_CORE
#include "fm1_multicore.h"
#endif
#include "fm1_input.h"
#include "fm1_timer.h"
#include "fm1_audio.h"
#include "fm1_adc.h"
#include "fm1_lcd_hw.h"
#include "felucca_tables.h"      /* generated by build.py (tools/gen_tables.py) */

/* ----------------------------------------------------- base, display --- */
#include "libc.c"
#define UMB_MIRROR 0             /* (UMBRA OBSCURA) the screen on a computer, over USB MIDI (umb_mirror.c). Out since 2026-11-09
                                  * (the user: too slow on the FM-1 for the plates, set aside; its ~4.3 KB of flash and ~4.7 KB
                                  * of RAM and pool to HOME > UMBRA's MACROS pages). 1 builds it again, as it was */
#if UMB_MIRROR
#define SM_BUFFER 3072u          /* (its queue, in the pool: RAM is short) */
#define SM_BUFFER_ATTR __attribute__((section(".pool")))
#include "screen_mirror/screen_mirror.c"   /* Patrick F. Kellogg's FM-1 screen mirror (MIT: screen_mirror/LICENSE) */
#endif
#include "lcd.c"
#include "gfx.c"

/* ----------------------------------------------------------- sound --- */
#include "core.h"
#include "engines.c"             /* dsp.c, the eng_*.c files, the factory patterns */
#include "params.c"
#include "mod.c"               /* the modulation matrix, used by voice.c and fx.c */
#include "voice.c"
#include "slicer.c"              /* the SLICER, one on a bus (JIANT 0.6.3), used by fx.c */
#include "fx.c"

/* ------------------------------------------------- MIDI, sequencer --- */
#include "usb.c"
#if FELUCCA_UART
#include "midi_uart.c"
#endif
#include "song_chain.c"
#include "seq.c"
#include "audio.c"

/* -------------------------------------------------------------- UI --- */
#include "panel.c"
#include "ui.c"
#include "menu_items.c"          /* MENU's rows and settings (ui_menu.c, editor_menu.c) */
#include "icons.c"               /* parameter icons (FELUCCA_ICONS), used by ui_draw.c */
#include "ui_graph.c"
#include "ui_draw.c"
#include "ui_menu.c"
#include "ui_input.c"
#include "ui_layer.c"              /* the quick layers (FX GLO SCL EDIT REC held; SEQ TOOLS: ui_tools.c) */

/* --------------------------------------------- storage, update, editor --- */
#if FELUCCA_FLASH
#if FELUCCA_DUAL_CORE
#define FL_PARK() fm1_mc_park()  /* (0.9.9-stab) core 1 out of any job before the flash goes off (hal/fm1_multicore.h) */
#endif
#include "fm1_flash.h"
#include "fm1_clk320.h"          /* (0.9.9-stab, as 0.9.12) MENU > SYSTEM > CPU CLOCK 320 MHZ's switch (curist's, MIT; main.c) */
#include "storage_hw.c"
#include "storage.c"
#include "umb_slot.c"              /* (UMBRA OBSCURA) the plate slot: a plate file in flash (editor_plate.c) */
#endif
#include "upreset.c"             /* user presets (RAM mirror; flash with FELUCCA_FLASH) */
#include "project.c"
#if FELUCCA_OTA
#include "ota.c"
#include "ota_hw.c"
#include "editor.c"              /* web editor SysEx (needs the OTA SysEx plumbing) */
#endif
#if FELUCCA_CDC
#include "console.c"
#endif
#include "master.c"              /* the MASTER pot (#137: read before the audio starts) */
#if FELUCCA_BT
#include "bt_hw.c"               /* (V10.12) bt.c's calls into the Bluetooth segment */
#endif
#if UMB_MIRROR
#include "umb_mirror.c"         /* (UMBRA OBSCURA) the mirror's side here: the panel's rows, the queue, the name */
#endif
#include "main.c"


