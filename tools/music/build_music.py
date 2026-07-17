#!/usr/bin/env python3
"""Generate native and browser music assets from the approved score."""

from __future__ import annotations

import json
import math
import random
import struct
from pathlib import Path

from score import TRACKS, parse


ROOT = Path(__file__).resolve().parents[2]
NATIVE_H = ROOT / "src" / "music_data.h"
BANK = ROOT / "dist" / "AYRIEN.SND"
WEB_JS = ROOT / "web" / "music-data.js"
TITLE_JS = ROOT / "web" / "title-music-data.js"
MANIFEST = Path(__file__).with_name("manifest.json")
SRC_HZ = 60
DST_HZ = 35
SAMPLE_RATE = 11025

# Explicit accompaniment lines.  They are melodies in their own right, not
# chord extraction from the approved lead.  Each line loops to the lead end.
ARRANGEMENTS = [
    ("E3/48 B2/24 C3/24 D3/48 E3/48", "B3/48 D4/48 E4/48 B3/48", "K/12 H/12 S/12 H/12"),
    ("D3/48 A2/48 C3/48 D3/48", "A3/48 C4/48 D4/48 A3/48", "K/24 S/24 K/12 H/12 S/24"),
    ("A2/24 E3/24 G2/24 A2/24", "E4/24 G4/24 A4/48", "K/12 H/6 H/6 S/12 H/12"),
    ("C3/60 G2/36 AB2/36 C3/60", "G3/60 EB4/60 F4/48", "K/24 H/12 S/24 H/12"),
    ("D3/36 A2/36 C3/36 D3/36", "F3/36 A3/36 B3/36 A3/36", "K/12 H/12 S/12 H/12"),
    ("E3/24 B2/24 F3/24 E3/24", "B3/24 E4/24 F4/24 E4/24", "K/12 H/6 H/6 S/12 H/12"),
    ("G2/60 D3/36 F3/36 G2/60", "D4/60 BB3/48 C4/48", "K/24 K/12 S/24 H/12"),
    ("A2/48 E3/48 F3/48 C3/48", "E4/48 A3/48 C4/48 E4/48", "K/24 H/12 S/24 H/12"),
    ("CS3/36 GS2/36 B2/36 CS3/36", "GS3/36 E4/36 FS4/36 E4/36", "K/12 R/12 S/12 H/12"),
    ("F2/48 C3/48 DB3/48 F2/48", "C4/48 AB3/48 DB4/48 C4/48", "K/24 H/12 S/24 K/12"),
    ("B2/72 FS3/48 E3/48 B2/72", "FS4/72 R/24 E4/48 D4/48", "K/36 R/12 H/24 S/36"),
    ("C3/60 G2/36 AB2/36 C3/60", "G3/60 C4/48 EB4/48", "K/24 K/12 S/24 H/12"),
    ("D3/24 A2/24 C3/24 D3/24", "A3/24 C4/24 D4/24 F4/24", "K/12 H/6 H/6 S/12 H/12"),
    ("E3/48 B2/24 C3/24 E3/48", "B3/48 F4/48 E4/48 D4/48", "K/24 R/12 S/12 H/12"),
    ("FS2/60 CS3/36 D3/36 FS2/60", "CS4/60 A3/48 B3/48", "K/24 K/12 S/24 H/12"),
    ("E3/24 B2/24 D3/24 E3/24", "B3/24 D4/24 G4/24 FS4/24", "K/12 H/6 H/6 S/12 H/12"),
    ("E3/24 D3/24 B2/24 E3/24", "G3/24 B3/24 D4/24 E4/24", "K/12 H/6 S/12 H/6 H/6"),
    ("E3/48 B2/48 A2/48 E3/48", "B3/48 CS4/48 GS3/48 B3/48", "K/24 H/12 S/24 H/12"),
]

# Hand-set lead, pad, bass, and percussion balances for each cue.
DYNAMICS = [
    (104, 70, 82, 76), (94, 60, 104, 96), (108, 58, 88, 102),
    (94, 78, 92, 76), (86, 68, 76, 58), (106, 62, 90, 104),
    (90, 58, 108, 94), (102, 82, 78, 70), (88, 74, 72, 62),
    (96, 70, 98, 82), (80, 76, 70, 48), (92, 64, 106, 94),
    (110, 62, 86, 108), (98, 66, 92, 76), (94, 60, 112, 100),
    (112, 76, 108, 110), (108, 64, 96, 106), (100, 86, 82, 72),
]

NOTE_BASE = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}


def quantize(events):
    out, src_pos, dst_pos = [], 0, 0
    for note, duration, gate in events:
        src_end = src_pos + duration
        dst_end = round(src_end * DST_HZ / SRC_HZ)
        qdur = max(1, dst_end - dst_pos)
        if note:
            gate_end = round((src_pos + gate) * DST_HZ / SRC_HZ)
            qgate = max(1, min(qdur, gate_end - dst_pos))
        else:
            qgate = 0
        out.append((note, qdur, qgate))
        src_pos, dst_pos = src_end, dst_end
    return out


def parse_drums(text):
    result = []
    for token in text.split():
        name, duration = token.split("/")
        result.append((name, int(duration)))
    return result


def lane_events(notes, channel, instrument, loop_ticks, velocity):
    commands, tick = [], 0
    while tick < loop_ticks:
        for note, duration, gate in quantize(notes):
            if tick >= loop_ticks:
                break
            end = min(loop_ticks, tick + duration)
            if note:
                commands.append((tick, channel, velocity, note, instrument))
                commands.append((min(loop_ticks - 1, end, tick + gate), channel, 0, note, instrument))
            tick = end
    return commands


def drum_events(pattern, loop_ticks, velocity):
    ids = {"K": (36, 3), "S": (38, 4), "H": (42, 5), "R": (0, 0)}
    commands, src_tick, dst_tick = [], 0, 0
    while dst_tick < loop_ticks:
        for name, duration in pattern:
            new_src = src_tick + duration
            new_dst = round(new_src * DST_HZ / SRC_HZ)
            qdur = max(1, new_dst - dst_tick)
            note, inst = ids[name]
            if note and dst_tick < loop_ticks:
                accent = max(24, velocity - (28 if name == "H" else 6 if name == "S" else 0))
                commands.append((dst_tick, 3, accent, note, inst))
                commands.append((min(loop_ticks - 1, dst_tick + max(1, qdur - 1)), 3, 0, note, inst))
            src_tick, dst_tick = new_src, min(loop_ticks, dst_tick + qdur)
            if dst_tick >= loop_ticks:
                break
    return commands


def make_samples():
    rng = random.Random(0x5354454C)
    samples = []

    def wave(name, length, fn, loop=True, base=29):
        data = bytes(max(0, min(255, round(128 + 104 * fn(i / length)))) for i in range(length))
        samples.append((name, data, 0, length if loop else 0, base))

    wave("LEAD", 256, lambda p: 0.62 * (1 if p < 0.35 else -1) + 0.28 * math.sin(2 * math.pi * p))
    wave("PAD", 256, lambda p: 1 - 4 * abs(p - 0.5) if 0 <= p < 1 else 0)
    wave("BASS", 256, lambda p: 0.72 * math.sin(2 * math.pi * p) + 0.22 * math.sin(4 * math.pi * p))
    kick = bytearray()
    phase = 0.0
    for i in range(1800):
        t = i / SAMPLE_RATE
        phase += 2 * math.pi * (45 + 120 * math.exp(-t * 22)) / SAMPLE_RATE
        kick.append(round(128 + 118 * math.sin(phase) * math.exp(-t * 18)))
    samples.append(("KICK", bytes(kick), 0, 0, 36))
    for name, length, decay, bright, base in (("SNARE", 1500, 20, False, 38), ("HAT", 650, 38, True, 42)):
        data, last = bytearray(), 0.0
        for i in range(length):
            n = rng.uniform(-1, 1)
            value = n if bright else n * 0.7 + last * 0.3
            last = value
            env = math.exp(-(i / SAMPLE_RATE) * decay)
            data.append(round(128 + 110 * value * env))
        samples.append((name, bytes(data), 0, 0, base))
    return samples


def build_tracks():
    built = []
    for index, ((name, description, lead_text), parts, dynamics) in enumerate(zip(TRACKS, ARRANGEMENTS, DYNAMICS)):
        lead = quantize(parse(lead_text))
        loop_ticks = sum(item[1] for item in lead)
        commands = lane_events(parse(lead_text), 0, 0, loop_ticks, dynamics[0])
        commands += lane_events(parse(parts[1]), 1, 1, loop_ticks, dynamics[1])
        commands += lane_events(parse(parts[0]), 2, 2, loop_ticks, dynamics[2])
        commands += drum_events(parse_drums(parts[2]), loop_ticks, dynamics[3])
        commands.sort(key=lambda e: (e[0], e[2], e[1]))
        built.append({"index": index, "name": name, "description": description,
                      "lead": lead, "loop_ticks": loop_ticks, "events": commands})
    return built


def validate(tracks, samples):
    for source, track in zip(TRACKS, tracks):
        source_ticks = sum(item[1] for item in parse(source[2]))
        expected = round(source_ticks * DST_HZ / SRC_HZ)
        if track["loop_ticks"] != expected or not 1 <= expected <= 0xFFFF:
            raise RuntimeError(f"{track['name']}: invalid cumulative quantization")
        if track["events"] != sorted(track["events"], key=lambda e: (e[0], e[2], e[1])):
            raise RuntimeError(f"{track['name']}: unordered events")
        active = {}
        for tick, channel, command, note, instrument in track["events"]:
            if not 0 <= tick < expected or not 0 <= channel < 4 or not 0 <= command <= 127:
                raise RuntimeError(f"{track['name']}: invalid event")
            if not 24 <= note <= 95 or not 0 <= instrument < 6:
                raise RuntimeError(f"{track['name']}: pitch or instrument out of range")
            if command:
                if channel in active:
                    raise RuntimeError(f"{track['name']}: overlapping channel {channel}")
                active[channel] = (note, tick)
            elif channel in active and active[channel][0] == note:
                del active[channel]
        # A gate ending on the final tick is silenced by the sequencer's loop reset.
        if any(start != expected - 1 for note, start in active.values()):
            raise RuntimeError(f"{track['name']}: invalid loop closure")
        if len(track["events"]) > 0xFFFF:
            raise RuntimeError(f"{track['name']}: event table too large")
    for name, data, loop_start, loop_end, base in samples:
        if len(data) > 0xFFFF or not 0 <= loop_start <= loop_end <= len(data) or not 12 <= base <= 95:
            raise RuntimeError(f"{name}: invalid sample metadata")


def write_header(tracks):
    lines = [
        "/* Generated by tools/music/build_music.py. */",
        "#ifndef MUSIC_DATA_H", "#define MUSIC_DATA_H", "",
        "typedef struct MusicNote { u8 note, duration, gate; } MusicNote;",
        "typedef struct MusicLead { const MusicNote __far *notes; u16 count; u16 loop_ticks; } MusicLead;", "",
    ]
    for track in tracks:
        lines.append(f"static const MusicNote __far lead_{track['index']:02d}[] = {{")
        row = []
        for note, duration, gate in track["lead"]:
            row.append(f"{{{note},{duration},{gate}}}")
            if len(row) == 8:
                lines.append("    " + ",".join(row) + ","); row = []
        if row: lines.append("    " + ",".join(row) + ",")
        lines.append("};\n")
    lines.append("static const MusicLead music_leads[18] = {")
    for track in tracks:
        lines.append(f"    {{lead_{track['index']:02d}, {len(track['lead'])}, {track['loop_ticks']}}},")
    lines += ["};", "", "#endif", ""]
    NATIVE_H.write_text("\n".join(lines), encoding="ascii")


def write_bank(tracks, samples):
    header_size = 32
    track_dir_size = len(tracks) * 12
    sample_dir_size = len(samples) * 16
    event_offset = header_size + track_dir_size + sample_dir_size
    event_blobs, track_dir, cursor = [], [], event_offset
    for track in tracks:
        blob = b"".join(struct.pack("<HBBBB", *event) for event in track["events"])
        event_blobs.append(blob)
        track_dir.append(struct.pack("<IHHI", cursor, len(track["events"]), track["loop_ticks"], 0))
        cursor += len(blob)
    sample_blobs, sample_dir = [], []
    for name, data, loop_start, loop_end, base in samples:
        sample_dir.append(struct.pack("<IHHHBBI", cursor, len(data), loop_start, loop_end, base, 1 if loop_end else 0, 0))
        sample_blobs.append(data); cursor += len(data)
    header = struct.pack("<4sBBBBIIIIII", b"AYSN", 1, len(tracks), len(samples), DST_HZ,
                         header_size, header_size + track_dir_size, event_offset, cursor, 0, 0)
    payload = header + b"".join(track_dir) + b"".join(sample_dir) + b"".join(event_blobs) + b"".join(sample_blobs)
    checksum = sum(payload) & 0xFFFFFFFF
    payload = payload[:24] + struct.pack("<I", checksum) + payload[28:]
    if len(payload) > 512 * 1024:
        raise RuntimeError("AYRIEN.SND exceeds 512 KiB")
    BANK.write_bytes(payload)
    return checksum


def write_web(tracks):
    compact = [{"name": t["name"], "loop": t["loop_ticks"], "events": t["events"]} for t in tracks]
    text = "/* Generated by tools/music/build_music.py. */\nconst MUSIC_SCORE_HZ=35;\nconst MUSIC_SONGS="
    text += json.dumps(compact, separators=(",", ":")) + ";\n"
    WEB_JS.write_text(text, encoding="ascii")


def pair_title_notes(track):
    active, notes = {}, []
    for tick, channel, velocity, note, instrument in track["events"]:
        if velocity:
            active[channel] = (tick, note, instrument, velocity)
        elif channel in active and active[channel][1] == note:
            start, active_note, active_instrument, active_velocity = active.pop(channel)
            notes.append((start, max(1, tick - start), channel, active_note,
                          active_instrument, active_velocity))
    for channel, (start, note, instrument, velocity) in active.items():
        notes.append((start, max(1, track["loop_ticks"] - start), channel,
                      note, instrument, velocity))
    notes.sort(key=lambda item: (item[0], item[2]))
    if not notes or any(start < 0 or duration < 1 or start + duration > track["loop_ticks"]
                        for start, duration, channel, note, instrument, velocity in notes):
        raise RuntimeError("TITLE: invalid paired note spans")
    return notes


def write_title_web(track):
    notes = pair_title_notes(track)
    payload = {"hz": DST_HZ, "loop": track["loop_ticks"], "notes": notes}
    text = "/* Generated by tools/music/build_music.py. */\nconst TITLE_MUSIC_SCORE="
    text += json.dumps(payload, separators=(",", ":")) + ";\n"
    TITLE_JS.write_text(text, encoding="ascii")
    return len(notes)


def main():
    if len(TRACKS) != 18 or len(ARRANGEMENTS) != 18 or len(DYNAMICS) != 18:
        raise RuntimeError("soundtrack must contain exactly 18 tracks")
    tracks, samples = build_tracks(), make_samples()
    validate(tracks, samples)
    write_header(tracks)
    checksum = write_bank(tracks, samples)
    write_web(tracks)
    title_notes = write_title_web(tracks[0])
    manifest = {"version": 1, "score_hz": DST_HZ, "tracks": [
        {"index": t["index"], "name": t["name"], "ticks": t["loop_ticks"],
         "events": len(t["events"])} for t in tracks],
        "samples": [s[0] for s in samples], "bank_bytes": BANK.stat().st_size,
        "checksum": checksum, "title_notes": title_notes}
    MANIFEST.write_text(json.dumps(manifest, indent=2) + "\n", encoding="ascii")
    print(json.dumps(manifest, indent=2))


if __name__ == "__main__":
    main()
