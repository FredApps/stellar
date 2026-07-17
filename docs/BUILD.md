# Building And Packaging

Ayrien Assault cross-compiles to a 16-bit real-mode MS-DOS executable from a Windows
host. The public repository does not include local toolchains, DOSBox configs,
or generated binaries.

## Prerequisites

| Tool | Purpose |
|------|---------|
| Open Watcom V2 | 16-bit DOS C compiler (`wcc`, `wlink`, `wmake`) |
| Python 3 | deterministic soundtrack generator, floppy builder, and BMP helper |
| DOSBox-X | optional emulator for smoke tests and `/bench` timing checks |

`build\build.ps1` looks for Open Watcom in `tools\watcom` first and then
`C:\tools\watcom`. Local toolchains under `tools/` are ignored; the versioned
canonical score and generator live under `tools/music/`.

## Open Watcom

Install an Open Watcom V2 snapshot and make sure these directories exist under
the chosen Watcom root:

```text
binnt64/ or binnt/
binw/
h/
lib286/
eddat/
```

The release workflow uses `open-watcom/setup-watcom@v1` with `version: "2.0"`
and `target: "dos"`, so local builds and CI use the same compiler family.

## Build

```powershell
build\build.ps1
```

The script sets the Open Watcom environment for the current build process only:

```text
WATCOM  = tools\watcom or C:\tools\watcom
PATH   += %WATCOM%\binnt64;%WATCOM%\binnt;%WATCOM%\binw
INCLUDE = %WATCOM%\h
EDPATH  = %WATCOM%\eddat
LIB     = %WATCOM%\lib286\dos;%WATCOM%\lib286
```

The makefile compiles with:

```text
wcc -bt=dos -ml -4 -os -s -zq -oi -zp1 -Isrc
```

| Flag | Meaning |
|------|---------|
| `-bt=dos` | target MS-DOS |
| `-ml` | large model: far code and far data for the audio backends |
| `-4` | 486 instruction scheduling |
| `-os` | optimize for size |
| `-s` | omit stack-overflow checks |
| `-oi` | inline intrinsic functions |
| `-zp1` | pack structures on 1-byte boundaries |
| `-zq` | quiet output |

The build first regenerates `src/music_data.h`, `web/music-data.js`, and the
versioned `dist/AYRIEN.SND` bank, then produces `AYRIEN.EXE`. Release
packaging copies the executable as `AYRIEN.EXE`.

## Packaging

Create a standard 1.44 MB FAT12 floppy image:

```powershell
python build\mkfloppy.py dist\AYRIEN.IMG AYRIEN.EXE dist\AYRIEN.SND dist\README.TXT
```

`HISCORE.DAT` is not shipped. The game creates it in the current directory on
first run.

## Release Pipeline

The repository includes `.github/workflows/release.yml`.

- Every run builds the DOS executable on `windows-latest`.
- Every run uploads an artifact named `Ayrien-Assault-MS-DOS`.
- When the ref is a `v*` tag, the workflow publishes a GitHub Release with
  `AYRIEN.EXE`, `AYRIEN.SND`, `README.TXT`, and a zip package.

Create a release by pushing a tag:

```powershell
git tag v0.1.0
git push origin v0.1.0
```

## Verification

### Visual Self-Test

`AYRIEN.EXE /shot` renders representative title and gameplay frames to
`TITLE.BMP` and `FRAME.BMP`.

```powershell
python build\bmp2png.py FRAME.BMP FRAME.png
```

### Audio Self-Test

`AYRIEN.EXE /audiodump /PCSPK` runs the music and SFX mixer without video and
writes `AUDIO.TXT`. Substitute `/ADLIB`, `/MT32:330`, or `/SB:220,7,1` to test
backend initialization; the dump names the device actually selected after any
fallback.

### Speed Check

`AYRIEN.EXE /bench` renders for about 10 seconds and writes `BENCH.TXT`.
Gameplay is intentionally capped by presenting on one VGA retrace and waiting
through the next, which holds fast machines near 35 FPS. Run `/bench` under
multiple DOSBox-X CPU settings to check that faster machines do not speed the
game up:

```ini
[cpu]
cputype=486_slow
cycles=fixed 12500
```

```ini
[cpu]
cputype=486_slow
cycles=fixed 25000
```

```ini
[cpu]
cputype=pentium_slow
cycles=fixed 80000
```

The exact benchmark count varies by emulator and host, but 66 MHz and Pentium
settings should remain close to the same 35 FPS cap rather than rising toward
the raw mode-13h refresh rate.

Generated DOSBox `.conf` files are intentionally ignored because they contain
local mount paths.

## Clean

```text
wmake -f build\wmakefile clean
```
