# FM-1 screen mirror (MIT)

Lets an M-VAVE FM-1 firmware send its screen over USB MIDI, so an app on a computer can show it big:
[FM-1 Mirror](https://github.com/pfkellogg/fm1-mirror) (free, Mac/Windows/Linux) and
[FM-1 Mirror + Spyglass](https://github.com/pfkellogg/fm1-mirror-spyglass-android) (free, Android) show it live, in
colour or high-contrast looks, for players who can't read the 1.54-inch display.

**MIT licensed** (see LICENSE), so any FM-1 firmware can use it, open source or not, under any licence.
That covers this folder and the same code in the screen-out branch's `app/screen.c` ("Screen mirror"
section) and the hello check in `app/transfer.c`, which I wrote. The rest of this repository is
M-VAVE's FM1 Open and keeps its own licence.

## The messages

| Direction | Message | Meaning |
|---|---|---|
| app → FM-1 | `F0 7D 46 53 00 F7` (`'F' 'S'`) | "keep sending your screen" — about once a second |
| app → FM-1 | `F0 7D 46 53 01 F7` | the same, and send the whole screen again (a new viewer) |
| FM-1 → app | `F0 7D 46 53 01 rl rh cl ch {n c0 c1 c2}… F7` | one screen row, from column `c` |
| app → FM-1 | `F0 7D 46 53 02 F7` | "keep sending the sound's name" only, no screen (for apps that only play it) |
| FM-1 → app | `F0 7D 46 53 02 name… F7` | the sound playing, in ASCII (e.g. `T1 FM6 ICE BELL`): after each hello of any kind, and whenever it changes |

- Row and column are two 7-bit bytes each (`low = v & 127`, `high = v >> 7`).
- Each run is four bytes: a count 1–127, then the RGB565 colour as bits 0–6, 7–13 and 14–15.
- Messages stay at 70 bytes or less, so they share a small USB queue with note output.
- Send the rows on the port the hello came from (USB or Bluetooth MIDI): the Big Screen firmware
  does, and FM-1 Mirror and FM-1 Mirror + Spyglass use either. Bluetooth is slower, but keeps up.
- The firmware sends only what it paints (what changes), and stops 3 seconds after the last hello.
  With nobody asking it sends nothing, so playing is unaffected.
- The sound's name (`02`) is optional: firmware that doesn't know it never sends it, and a viewer shows
  nothing then. FM-1 Mirror shows it on top of its spyglass.
- `7D` is MIDI's non-commercial manufacturer ID.

**It won't change under you.** These messages stay as they are. Anything new will be a new command
byte after `'F' 'S'` (`02` and up), never a change to these, so a viewer that ignores commands it
doesn't know keeps working. The hello must stay exactly 6 bytes: the firmware ignores a longer one.

**USB names and IDs** (handy for a viewer that looks for the FM-1): M-VAVE's original V15 firmware is
"FM-1", VID `0x4C4A` PID `0xC755`; the Big Screen firmware (on M-VAVE's Open V14) is "FM1 Community",
VID `0x3654` PID `0x7756`. Other firmwares may use their own.

## Using it

`screen_mirror.c` / `.h` are plain C with no dependencies:

1. Pass every incoming SysEx message to `sm_sysex()` (it returns 1 for a hello).
2. Call `sm_poll(now_ms, send)` often, with a function that sends one SysEx message over USB.
3. While `sm_on()`, give each block of rows you send to the LCD to `sm_rows()` as well. If it
   returns 0, the queue is full: try again after `sm_poll`, or repaint those rows later.
4. When `sm_take_full_request()` returns 1, repaint the whole screen.
5. Optional: call `sm_name(name, now_ms, send)` often (every 100 ms is plenty) with the sound playing,
   in ASCII. It sends only after a hello or when the name changes, and nothing when nobody's asking.

The buffer is 16 KB by default (`-DSM_BUFFER=…` to change it: ChoralRoot uses 6 KB). What has been sent frees its
room at once, so a small buffer keeps up too. `cc screen_mirror.c test.c && ./a.out` runs the test.

## A real example: ChoralRoot

[ChoralRoot](https://github.com/Quixotic7/ChoralRootFM1) (Quixotic7's chord instrument for the FM-1, built on
Felucca) runs with the big screen in [pfkellogg/ChoralRootFM1, branch `big-screen`](https://github.com/pfkellogg/ChoralRootFM1/tree/big-screen),
tested on a real FM-1 with FM-1 Mirror (Mac) and FM-1 Mirror + Spyglass (Android). What it took:

- **32 lines in ChoralRoot's own files**, and only **7 of them do the work**; the rest are an on/off switch
  (`CR_BIG_SCREEN`), so Felucca and the update loader, which share these files, build exactly as before:
  - `lcd.c`: one call in `lcd_blit` and one in `lcd_fill`, where every rectangle goes to the LCD.
  - `usb.c`: two lines where its SysEx parser finishes a message: a 6-byte `F0 7D 'F' 'S' .. F7` goes to `sm_sysex`.
  - `main.c`: one call a main-loop pass, `mirror_service()`.
  - `choralroot.c`: the switch, including the adapter, and a 512-event SysEx send ring instead of 64 (`usb.c` takes
    the size from outside now): the screen went out at ~25 KB/s, a big chord name in 0.9 s; now ~67 KB/s, 0.3 s.
    **Check your firmware's USB send queue first**: it, not USB, sets how fast the screen can go.
- **One new adapter file, 60 lines** (`firmware/src/cr_mirror.c`, MIT): turns ChoralRoot's byte-swapped pixels back
  into RGB565 rows for `sm_rows`, sends each message into usb.c's SysEx queue without ever waiting on it (`-2` when
  it's full), and asks ChoralRoot to redraw everything (`ui.force = 1`) when a new viewer joins or rows had to be
  dropped. A draw never waits more than 0.4 s for USB, so a host that stops reading can't slow the FM-1 down.
- **This module, unchanged.** Cost: about 1 KB of flash and 8 KB of RAM.

The same three hooks fit any firmware: where it draws to the LCD, where a SysEx message ends, and its main loop.

Questions, or want it tested on a real FM-1? pfkellogg@gmail.com — Patrick F. Kellogg

## Where it runs

Tested on real FM-1s in ChoralRoot, JIANT FM, Skiff and SLOOP (SLOOP's build isn't released yet), and in this Official Open fork.

M-VAVE is welcome to include this code in its own firmware, including V16 and later official releases: [m-vave/fm1_open#18](https://github.com/m-vave/fm1_open/issues/18).
