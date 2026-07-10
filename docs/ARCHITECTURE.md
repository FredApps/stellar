# Architecture

Stellar is a single-threaded, real-mode DOS program. The main loop reads input,
updates the world, draws to a back buffer, and presents one finished 320x200
frame.

## Target And Memory Model

- CPU target: Intel 80486 or better.
- OS target: MS-DOS, 16-bit real mode.
- Compiler: Open Watcom, compact memory model (`-mc`).
- Code pointers are near; data pointers are far.
- The back buffer (`g_back`) and static nebula background (`g_bg`) are each one
  64,000-byte far allocation.
- The game uses conventional memory only. It does not require XMS, EMS, a DPMI
  host, or an FPU.

## Timing

Gameplay is frame-driven: movement, animation, sound cadence, spawn timers, and
score timing all advance once per game frame.

Mode 13h refreshes at roughly 70 Hz. A loop that updates once per refresh runs
too fast on 66 MHz and Pentium-class machines. To preserve the original 486-era
feel, the interactive loop and `/bench` use `vga_present_paced()`:

1. Wait for a retrace.
2. Copy the back buffer to `0xA000:0000`.
3. Wait through the next retrace before the next game update.

That caps fast machines near 35 FPS while keeping the code simple and avoiding
a PIT timer ISR. Slow machines may still run below the cap if they cannot finish
a full update and copy inside the available budget.

The browser port uses the same 35 Hz logic rate so movement speed matches the
native DOS build.

## Modules

| Module | Responsibility |
|--------|----------------|
| `main.c` | entry point, command-line modes, init/shutdown |
| `vga.c` | mode 13h, palette, frame pacing, drawing primitives |
| `input.c` | INT 9 keyboard ISR and key state helpers |
| `sound.c` | PC speaker tones, music sequencer, priority SFX mixer |
| `sprites.c` | sprite bitmaps and ROM font text drawing |
| `game.c` | state machine, waves, enemies, boss logic, scoring |
| `hiscore.c` | `HISCORE.DAT` load/save and ranking |
| `bmpdump.c` | BMP dumps for visual verification |

## Video

- Mode set/restore uses BIOS `int 10h` through Watcom `int86`.
- The game draws everything into `g_back`, then copies all 64,000 bytes to VGA
  memory during presentation.
- `vga_wait_vsync()` polls port `0x3DA`, bit 3.
- `vga_present()` waits for one retrace and blits.
- `vga_present_paced()` presents and then waits for one more retrace to enforce
  the 35 FPS cap.
- Custom DAC ramps provide fire, nebula, and glow colors above the standard
  EGA-compatible first 16 colors.
- `vga_bg_blit()` wraps the static nebula vertically into the back buffer for a
  cheap scrolling background.

## Input

`input.c` installs a compact INT 9 handler. It reads scancodes from port `0x60`,
updates held and edge tables, sends EOI to the PIC, and restores the previous
handler on exit.

Consumers use:

- `key_pressed(sc)` for held movement and fire.
- `key_hit(sc)` for fresh menu/action edges.
- `kbd_getchar()` for high-score name entry.

## Sound

The PC speaker is also frame-driven. `snd_update()` runs once per game frame.
Sound effects have priorities so explosions and power-ups are not swallowed by
low-priority fire pips. Music resumes when the active SFX finishes.

## Game Logic

The game avoids dynamic allocation during play. It uses fixed pools for player
bullets, enemy bullets, enemies, power-ups, particles, missiles, stars, and
foreground dust. Positions are integers; sine movement uses `sintab[]`.

Boss sprites are procedural pixel bitmaps built at startup into
`spr_boss[15][72*52]` (56,160 bytes — one far object, kept under the 64 KB
compact-model limit). Several bosses add cheap per-frame overlays at draw time
(tentacles, orbiting pods/orbs, tracking pupils) on top of the static bitmap.
Boss movement is a per-kind state machine in `boss_move()` bounded by a
per-boss vertical envelope (`boss_max_y()`); an every-other-frame O(n^2)
separation pass keeps the enemy pool unstacked.

The state machine is:

```text
TITLE -> PLAY -> OVER -> ENTRY/SCORES -> TITLE
```

Hidden command-line modes:

- `/shot` renders `TITLE.BMP`, `FRAME.BMP`, `HELP1/2.BMP`, `STAGES.BMP`, and the
  boss roster atlases `BOSSES1.BMP`/`BOSSES2.BMP`, then runs headless logic
  checks (enemy separation + all boss movement envelopes) into `SELFTEST.TXT`.
- `/audiodump` writes speaker frequencies to `AUDIO.TXT`.
- `/bench` writes a rough rendered-frame timing sample to `BENCH.TXT`.

## Constraints

- The engine prefers determinism and tiny real-mode code over timer complexity.
- If a slow machine misses the 35 FPS budget, the game slows rather than
  dropping simulation frames.
- Audio is single-channel by design.
