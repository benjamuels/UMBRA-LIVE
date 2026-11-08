#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Build the main-loop code for size (build.py): the functions defined in SIZE_FILES get LLVM's
minsize (about -Oz) while everything else (the sound, render and ISR code, the flash and OTA code,
the main loop) keeps -Os. The JieLi clang 4 has no '#pragma clang attribute' and its minsize
attribute is an error on variables, so build.py compiles felucca.c to LLVM IR without the
optimizer, this script adds 'minsize' to those definitions, and the IR is then compiled at -Os
(without the edit, that round trip is byte-identical to compiling felucca.c directly).
  size_fns.py IN.ll OUT.ll"""
import re
import sys
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[1]
SRC = _ROOT / "firmware" / "src" if (_ROOT / "firmware" / "src").is_dir() else _ROOT / "src"   # either layout
# the UI, the stores, the editor and the console: main loop only, never called by sound code; and (1.1) every
# other file without DSP: USB, MIDI, the sequencer, the drawing, the LCD, OTA, ... Of those, what an interrupt
# reaches (ISR_ROOTS and everything they call or name, from the IR) keeps -Os (isr_reach)
SIZE_FILES = ["ui.c", "favorites.c", "menu_items.c", "icons.c", "ui_graph.c", "ui_draw.c", "ui_menu.c", "ui_input.c",
              "storage.c", "upreset.c", "project.c", "settings_persist.c",
              "editor.c", "editor_preferences.c", "editor_backup.c", "editor_menu.c", "console.c",
              "usb.c", "midi_uart.c", "midi_control.c", "midi_clock.c", "seq.c", "song_chain.c", "motion.c",
              "panel.c", "lcd.c", "gfx.c", "ota.c", "ota_hw.c", "up_fm6.c", "params.c", "editor_fm6.c",
              "ui_layer.c", "ui_name.c", "ui_events.c", "song_main.c", "main.c", "libc.c", "storage_hw.c", "engines.c", "fm4_convert.c",
              # (0.9.0) the files the ones above include, missed until now (-16.7 KB): GEN, DICE, the lenses, the line
              # art, the beings, the tech graphs, the tools, the FM6 bank and VOICE's words (its sound side, reached
              # from the audio ISR through ENG[]'s hooks, keeps -Os: isr_reach follows globals)
              "gen.c", "gen_tables.c", "song_dice.c", "ui_lens.c", "ui_organic.c", "ui_fm6op.c", "ui_being.c",
              "ui_engvis.c", "ui_tech.c", "ui_lzsave.c", "ui_lzfx.c", "ui_lzhome.c", "ui_umbra.c", "ui_umbra_melody.c", "ui_umbra_field.c", "ui_umbra_plate.c", "ui_umbra_tot.c", "umb_mirror.c", "screen_mirror/screen_mirror.c", "ui_lzedit.c", "ui_lzpage.c", "ui_dx.c",
              "ui_tools.c", "fm6_vbank.c", "sing.c",
              # (0.9.7) the main-loop functions of the sound files (what the interrupts reach stays -Os, as above): the
              # MASTER pot's boot and poll, FM6's patch store, the kit's names and grids, MOD's names, the PUNCH lane
              "pfx.c", "master.c", "audio.c", "mod.c", "fx.c", "drumx_voice.c", "eng_drum.c", "eng_fm6.c",
              # (V10.29) the files added since, and the other engines' main-loop side (as 0.9.7's: what the
              # interrupts reach keeps -Os): EUCLID, SCENE, LIVE, MIDI LEARN, the song player, PERFORM, PLAYHEADS
              "ui_euclid.c", "ui_scene.c", "ui_live.c", "seq_live.c", "midi_learn.c", "song_play.c", "perform.c",
              "fugue.c", "slicer.c", "voice.c", "fm6_core.c", "eng_add.c", "eng_analog.c", "eng_animal.c",
              "eng_digital.c", "eng_formant.c", "eng_lofi.c", "eng_noise.c", "eng_phase.c", "eng_string.c",
              "eng_wheel.c", "bt.c", "click.c"]
ISR_ROOTS = ("fm1_alnk0_irq", "fm1_timer5_irq")      # hal/fm1_isr.S: the audio ISR, the key / LED scan (+ USB poll)
DEF = re.compile(r"^(?:static|void|int|uint\w*|int\w*|const)\b[^;=(]*?\b([A-Za-z_]\w*)\s*\(", re.M)
IR_DEF = re.compile(r"^(define [^\n]*?@\"?([\w.]+)\"?\([^\n]*\)(?: unnamed_addr| local_unnamed_addr)?)( #\d+[^\n]*\{)$",
                    re.M)


def definitions(text):
    """names of the functions text defines (a signature at column 0 whose ')' is followed by '{')"""
    names = []
    text = re.sub(r"/\*.*?\*/|//[^\n]*", " ", text, flags=re.S)        # comments (none hold code)
    text = re.sub(r"\b__attribute__\s*\(\((?:[^()]|\([^()]*\))*\)\)", " ", text)   # (0.9.0) before the name too
    for m in DEF.finditer(text):
        i, depth = m.end() - 1, 0
        while i < len(text):
            depth += {"(": 1, ")": -1}.get(text[i], 0)
            i += 1
            if depth == 0:
                break
        rest = re.sub(r"^(__attribute__\s*\(\(.*?\)\)\s*)+", "", text[i:i + 200].lstrip())
        if rest.startswith("{"):
            names.append(m.group(1))
    return names


def size_names():
    names = set()
    for f in SIZE_FILES:
        names.update(definitions((SRC / f).read_text()))
    return names


IR_BODY = re.compile(r"^define [^\n]*?@\"?([\w.]+)\"?\((.*?)^\}", re.M | re.S)
IR_REF = re.compile(r"@\"?([\w.]+)\"?")
IR_GLOBAL = re.compile(r"^@\"?([\w.]+)\"? = [^\n]*", re.M)


def isr_reach(ir, with_globals=False):
    """the functions an interrupt can run: ISR_ROOTS and, transitively, every function their bodies call or
    name (a pointer taken there counts as called), from the unoptimised IR (before inlining: a superset).
    (0.9.0) A global they name counts as its initialiser: a function in a table they read (an engine's hooks:
    ENG[].block -> formant_block -> sing_block) is reached too. with_globals: the globals reached as well (the
    constant tables the sound reads: tools/build.py's flash layout)"""
    body = {m.group(1): m.group(2) for m in IR_BODY.finditer(ir)}
    fns = set(body)
    for m in IR_GLOBAL.finditer(ir):
        body.setdefault(m.group(1), m.group(0))
    seen, todo = set(), [r for r in ISR_ROOTS if r in body]
    while todo:
        f = todo.pop()
        if f in seen:
            continue
        seen.add(f)
        todo += [g for g in IR_REF.findall(body[f]) if g in body and g not in seen]
    return seen if with_globals else {f for f in seen if f in fns}


def main(src, dst):
    ir = Path(src).read_text()
    isr = isr_reach(ir)
    names, hit = size_names() - isr, set()

    def mark(m):
        if m.group(2) not in names:
            return m.group(0)
        hit.add(m.group(2))
        return m.group(1) + " minsize" + m.group(3)
    Path(dst).write_text(IR_DEF.sub(mark, ir))
    print(f"size: {len(hit)} main-loop functions built for size ({len(names) - len(hit)} not in this build; "
          f"{len(size_names() & isr)} reached from an interrupt kept -Os)")
    return 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:3]))
