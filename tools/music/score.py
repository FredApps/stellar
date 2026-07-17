#!/usr/bin/env python3
"""Build the deliberately monophonic PC-speaker audition score.

The score is written as literal melodies.  This script only validates the
notation and emits the compact DOS data table; it does not derive harmony,
arpeggiate chords, or synthesize additional voices.
"""

from __future__ import annotations

import json
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tools" / "music" / "pcspktracks.h"
META = ROOT / "tools" / "music" / "tracks.json"

NOTE_BASE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def score(*phrases: str) -> str:
    return " ".join(phrases)


# One line at a time, deliberately constrained to A3-E5.  The campaign motif
# is the descending B-A-G-E gesture; each chapter treats it differently while
# retaining its own rhythm, mode, and melodic contour.
TRACKS = [
    ("TITLE", "Launch fanfare", score(
        "E4/18 G4/12 A4/12 B4/30 R/6 A4/12 G4/12 E4/24 R/12",
        "G4/12 A4/12 B4/18 D5/24 B4/12 A4/12 G4/18 E4/30 R/12",
        "E4/12 FS4/12 G4/12 B4/24 A4/12 G4/12 FS4/18 E4/36 R/18",
        "B3/12 E4/12 G4/12 A4/12 B4/24 G4/12 A4/12 FS4/18 E4/42 R/18")),
    ("01 GORGON", "Armoured advance", score(
        "D4/24 D4/12 F4/18 E4/12 D4/30 R/6 C4/12 D4/24 R/12",
        "A3/18 C4/12 D4/24 F4/18 E4/12 D4/30 R/12",
        "D4/12 F4/12 G4/18 F4/12 E4/12 D4/24 C4/18 D4/36 R/18",
        "A3/12 D4/18 E4/12 F4/18 E4/12 C4/18 D4/42 R/18")),
    ("02 REAPER", "Dagger pursuit", score(
        "A4/9 C5/9 B4/9 A4/9 G4/12 E4/12 A4/18 R/6 E4/9 G4/9 A4/18 R/9",
        "A4/9 B4/9 C5/12 B4/9 A4/9 G4/12 E4/18 R/6 FS4/9 G4/9 A4/24 R/12",
        "C5/9 B4/9 A4/9 G4/9 E4/12 G4/12 A4/18 C5/12 B4/12 A4/24 R/12",
        "E4/9 FS4/9 G4/12 A4/9 B4/9 C5/12 B4/9 A4/9 GS4/12 A4/30 R/18")),
    ("03 LEVIATHAN", "Carrier horizon", score(
        "C4/30 G4/18 F4/12 EB4/24 C4/24 R/12 EB4/18 F4/12 G4/30 R/12",
        "G4/12 AB4/12 G4/18 F4/12 EB4/18 C4/30 R/12 D4/12 EB4/12 F4/24 R/12",
        "C4/18 EB4/18 G4/24 AB4/18 G4/12 F4/24 EB4/18 C4/36 R/18",
        "G3/18 C4/18 D4/12 EB4/18 F4/12 EB4/18 D4/12 C4/42 R/18")),
    ("04 SEEKER", "Orbital signal", score(
        "D4/12 F4/12 A4/12 B4/12 A4/12 F4/12 E4/12 D4/18 R/6",
        "E4/12 G4/12 B4/12 A4/12 G4/12 E4/12 D4/12 E4/18 R/6",
        "F4/12 A4/12 C5/12 A4/12 G4/12 F4/12 E4/12 D4/24 R/12",
        "A4/12 G4/12 F4/12 E4/12 D4/18 E4/12 F4/12 D4/30 R/18")),
    ("05 MANTIS", "Pincer rhythm", score(
        "E4/9 F4/9 E4/12 G4/9 F4/9 E4/18 R/6 B3/9 D4/9 E4/18 R/9",
        "F4/9 A4/9 G4/12 F4/9 E4/9 D4/18 R/6 E4/9 F4/9 E4/24 R/12",
        "B4/9 A4/9 G4/12 F4/9 E4/9 F4/12 G4/9 F4/9 E4/18 R/9",
        "D4/9 F4/9 E4/12 D4/9 C4/9 B3/18 D4/12 E4/30 R/18")),
    ("06 ANVIL", "Foundry march", score(
        "G3/30 G3/12 BB3/18 A3/12 G3/30 R/12 D4/18 C4/12 BB3/24 R/12",
        "G3/18 BB3/18 D4/24 EB4/18 D4/12 C4/24 BB3/18 G3/36 R/18",
        "D4/18 D4/12 F4/18 EB4/12 D4/24 C4/18 BB3/12 A3/18 G3/30 R/12",
        "BB3/12 C4/12 D4/18 C4/12 BB3/18 A3/12 G3/42 R/18")),
    ("07 SERAPH", "Wing sweep", score(
        "A3/18 C4/12 E4/18 A4/24 G4/12 E4/18 D4/12 C4/24 R/12",
        "E4/12 G4/12 A4/18 C5/24 B4/12 A4/18 G4/12 E4/30 R/12",
        "F4/18 A4/12 C5/18 B4/12 A4/18 G4/12 E4/18 D4/24 R/12",
        "C4/12 E4/18 G4/12 A4/24 G4/12 E4/18 C4/12 A3/42 R/18")),
    ("08 NEXUS", "Split-core dialogue", score(
        "CS4/12 E4/12 GS4/18 R/6 E4/12 FS4/12 GS4/18 R/12",
        "B4/12 GS4/12 FS4/18 R/6 E4/12 DS4/12 CS4/24 R/12",
        "E4/9 FS4/9 GS4/18 B4/12 A4/12 GS4/18 FS4/12 E4/24 R/12",
        "GS4/9 A4/9 B4/18 GS4/12 FS4/12 E4/18 DS4/12 CS4/36 R/18")),
    ("09 KRAKEN", "Tentacle procession", score(
        "F3/18 AB3/12 C4/24 R/6 DB4/12 C4/18 AB3/12 F3/30 R/12",
        "C4/12 EB4/18 F4/12 AB4/24 G4/12 F4/18 EB4/12 C4/30 R/12",
        "DB4/12 F4/12 AB4/18 G4/12 F4/12 EB4/18 C4/12 DB4/24 R/12",
        "AB3/12 C4/18 EB4/12 F4/18 EB4/12 C4/18 AB3/12 F3/42 R/18")),
    ("10 PHANTOM", "Blinking transmission", score(
        "B3/24 R/18 FS4/18 R/12 E4/12 D4/24 R/18 B3/18 R/12",
        "D4/12 FS4/24 R/12 A4/18 GS4/12 FS4/24 R/18",
        "E4/12 FS4/12 A4/24 R/12 B4/18 A4/12 FS4/24 E4/18 R/18",
        "D4/12 E4/12 FS4/18 R/9 E4/12 D4/18 CS4/12 B3/36 R/24")),
    ("11 CITADEL", "Fortress cadence", score(
        "C4/24 C4/12 EB4/18 G4/24 F4/12 EB4/18 D4/12 C4/30 R/12",
        "G3/18 C4/18 D4/12 EB4/24 G4/18 F4/12 EB4/24 R/12",
        "AB4/18 G4/12 F4/18 EB4/12 D4/18 C4/24 G3/18 C4/30 R/12",
        "EB4/12 F4/12 G4/18 F4/12 EB4/18 D4/12 C4/42 R/18")),
    ("12 VORTEX", "Circular current", score(
        "D4/9 E4/9 F4/9 A4/9 C5/9 A4/9 F4/9 E4/9 D4/18 R/6",
        "F4/9 G4/9 A4/9 C5/9 D5/9 C5/9 A4/9 G4/9 F4/18 R/6",
        "A4/9 G4/9 F4/9 E4/9 D4/9 E4/9 F4/9 A4/9 C5/18 R/6",
        "A4/9 F4/9 E4/9 D4/9 C4/12 D4/12 E4/12 D4/30 R/18")),
    ("13 BASILISK", "Unblinking corridor", score(
        "E4/18 F4/9 E4/9 D4/18 C4/12 B3/24 R/9 F4/12 E4/24 R/12",
        "G4/18 F4/9 E4/9 D4/18 F4/12 E4/24 R/9 B4/12 A4/24 R/12",
        "C5/12 B4/12 A4/18 G4/12 F4/18 E4/12 D4/24 R/12",
        "F4/12 E4/12 D4/18 C4/12 B3/18 D4/12 E4/42 R/18")),
    ("14 TITAN", "Dreadnought weight", score(
        "FS3/30 A3/18 CS4/24 B3/12 A3/24 FS3/30 R/12",
        "CS4/18 E4/12 FS4/24 A4/18 GS4/12 FS4/24 E4/18 CS4/30 R/12",
        "D4/18 FS4/12 A4/24 B4/18 A4/12 FS4/24 E4/18 D4/30 R/12",
        "CS4/12 E4/18 FS4/12 E4/18 CS4/12 B3/18 A3/12 FS3/42 R/18")),
    ("15 OVERLORD", "Final assault", score(
        "E4/12 G4/12 B4/18 A4/12 G4/12 E4/24 R/6 FS4/12 A4/12 C5/24 R/12",
        "B4/9 C5/9 D5/18 C5/12 B4/12 A4/18 G4/12 FS4/24 R/12",
        "E4/9 FS4/9 G4/12 A4/9 B4/9 C5/18 B4/12 A4/12 G4/24 R/12",
        "B4/12 A4/12 G4/12 FS4/12 E4/18 D4/12 B3/18 E4/42 R/18")),
    ("16 FREEPLAY", "Endurance sortie", score(
        "E4/9 G4/9 A4/12 B4/9 A4/9 G4/12 E4/18 R/6 FS4/9 A4/9 B4/18 R/9",
        "G4/9 B4/9 C5/12 D5/9 C5/9 B4/12 A4/18 G4/12 E4/24 R/12",
        "A4/9 C5/9 B4/12 A4/9 G4/9 FS4/12 E4/18 D4/12 B3/24 R/12",
        "E4/9 FS4/9 G4/9 A4/9 B4/12 G4/12 FS4/12 E4/30 R/18")),
    ("VICTORY", "Homeward signal", score(
        "E4/18 GS4/12 B4/24 R/6 A4/12 B4/12 CS5/24 B4/18 R/12",
        "A4/12 CS5/18 B4/12 GS4/24 FS4/12 E4/30 R/12",
        "B3/12 E4/12 FS4/12 GS4/18 B4/24 CS5/18 B4/12 GS4/30 R/12",
        "A4/12 B4/12 CS5/18 B4/12 GS4/18 FS4/12 E4/48 R/24")),
]


def midi(note: str) -> int:
    match = re.fullmatch(r"([A-G])(S|B)?([2-5])", note)
    if not match:
        raise ValueError(f"bad note {note!r}")
    name, accidental, octave_text = match.groups()
    value = 12 * (int(octave_text) + 1) + NOTE_BASE[name]
    if accidental == "S":
        value += 1
    elif accidental == "B":
        value -= 1
    return value


def parse(text: str) -> list[tuple[int, int, int]]:
    events = []
    for token in text.split():
        parts = token.split("/")
        if len(parts) not in (2, 3):
            raise ValueError(f"bad event {token!r}")
        duration = int(parts[1])
        if not 3 <= duration <= 96:
            raise ValueError(f"bad duration in {token!r}")
        if parts[0] == "R":
            events.append((0, duration, 0))
            continue
        note = midi(parts[0])
        gate = int(parts[2]) if len(parts) == 3 else max(2, duration - max(2, duration // 6))
        if gate > duration:
            raise ValueError(f"gate exceeds duration in {token!r}")
        events.append((note, duration, gate))
    return events


def emit() -> None:
    parsed = [(name, description, parse(text)) for name, description, text in TRACKS]
    lines = [
        "/* Generated by compose_pcspeaker.py.  One PIT voice only. */",
        "#ifndef PCSPKTRACKS_H",
        "#define PCSPKTRACKS_H",
        "",
        "typedef struct NoteEvent { u8 note, ticks, gate; } NoteEvent;",
        "typedef struct TrackDef { const NoteEvent *events; u16 count; const char *name; } TrackDef;",
        "",
    ]
    metadata = []
    for index, (name, description, events) in enumerate(parsed):
        ident = f"track_{index:02d}"
        lines.append(f"static const NoteEvent {ident}[] = {{")
        row = []
        for note, ticks, gate in events:
            row.append(f"{{{note},{ticks},{gate}}}")
            if len(row) == 6:
                lines.append("    " + ", ".join(row) + ",")
                row = []
        if row:
            lines.append("    " + ", ".join(row) + ",")
        lines.append("};")
        ticks = sum(event[1] for event in events)
        metadata.append({
            "index": index,
            "name": name,
            "description": description,
            "ticks": ticks,
            "seconds": round(ticks / 60.0, 3),
            "events": len(events),
        })
        lines.append("")
    lines.append("static const TrackDef tracks[] = {")
    for index, (name, _, events) in enumerate(parsed):
        lines.append(f'    {{track_{index:02d}, {len(events)}, "{name}"}},')
    lines += ["};", "", "#define TRACK_COUNT (sizeof(tracks) / sizeof(tracks[0]))", "#endif", ""]
    OUT.write_text("\n".join(lines), encoding="ascii")
    META.write_text(json.dumps(metadata, indent=2) + "\n", encoding="ascii")


if __name__ == "__main__":
    emit()
