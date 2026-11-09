# VOICE text in user presets

Projects already saved each track's text. USER presets saved parameters and pattern but
omitted the text, so loading a VOICE USER slot could use whichever lyric happened to be
on that track. The correction saves and restores all 63 printable ASCII characters,
including case, spaces and punctuation, alongside the preset's language and TEXT mode.

The existing per-slot auxiliary payload (previously FM6-only) has 112 bytes. A VOICE
slot uses `LYR1` plus the 64-byte null-terminated text. The stored record's engine and
hash select the payload type. An FM6 slot keeps its existing packed patch format.
There are no new flash sectors, banks, RAM buffers or record-size changes. Full backups
already include this auxiliary object. Renaming a slot preserves its attached text.

VOICE slots saved by older firmware have no recoverable text in them. They load an
empty lyric, rather than inheriting unrelated words. Recover text from a saved project
if available and save the USER slot again. Individual older editor preset exports that
contain only the sound record do not contain this auxiliary text.

A failed auxiliary flash write reports failure and restores the old RAM entry. As with
the existing FM6 save flow, the sound record and auxiliary data are two separate A/B
writes, not one atomic transaction. The record hash prevents a payload belonging to a
different sound from being loaded. Saving requires the transport stopped.

`tests/voice_user_cases.h` uses the actual NOR driver to check 63-character text, reboot,
language/mode, rename, independent slots, empty and legacy records, the final slot,
malformed text, failed-write rollback/retry and coexistence with FM6 patches.
