#!/usr/bin/env python3
"""Deterministic original TC role badges; no ET class art, fonts or downloads."""
import argparse
import hashlib
import json
from pathlib import Path
from generate_nav_icons import Canvas, INK, WHITE, tga, png

ROOT = Path(__file__).resolve().parents[2]
DEST = ROOT / "src/assets/gfx/tce2/fireteam"
OLIVE = (137, 164, 109, 255)


def assault(c):
    shield = [(10, 9), (54, 9), (54, 37), (43, 49), (32, 57), (21, 49), (10, 37)]
    c.line(shield, INK, 6, True)
    c.polygon(shield, OLIVE)
    c.line([(18, 20), (32, 28), (46, 20)], WHITE, 5)
    c.line([(18, 31), (32, 39), (46, 31)], WHITE, 5)


def recon(c):
    eye = [(5, 32), (17, 18), (32, 12), (47, 18), (59, 32), (47, 46), (32, 52), (17, 46)]
    c.line(eye, INK, 6, True)
    c.polygon(eye, OLIVE)
    c.circle(32, 32, 15, INK)
    c.circle(32, 32, 11, WHITE)
    c.circle(32, 32, 5, INK)


def sniper(c):
    c.circle(32, 32, 22, INK)
    c.circle(32, 32, 18, OLIVE)
    c.circle(32, 32, 12, INK)
    c.line([(3, 32), (22, 32)], WHITE, 5)
    c.line([(42, 32), (61, 32)], WHITE, 5)
    c.line([(32, 3), (32, 22)], WHITE, 5)
    c.line([(32, 42), (32, 61)], WHITE, 5)
    c.circle(32, 32, 3, WHITE)


ART = {"assault": assault, "recon": recon, "sniper": sniper}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()
    generated, records = {}, []
    for name, draw in ART.items():
        c = Canvas(64)
        draw(c)
        data = tga(64, c.finish())
        generated[name + ".tga"] = data
        records.append({"asset": name + ".tga", "width": 64, "height": 64,
                        "channels": "BGRA8, straight alpha, top-left origin",
                        "sha256": hashlib.sha256(data).hexdigest()})
    source = {"schema": 1, "origin": "Original geometric TC Assault, Recon and Sniper role badges.",
              "license": "CC0-1.0", "generator": "tools/assets/generate_fireteam_icons.py",
              "generator_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "rasterizer": "tools/assets/generate_nav_icons.py", "files": records}
    generated["SOURCE.json"] = (json.dumps(source, indent=2) + "\n").encode()
    for name, data in generated.items():
        if args.check:
            assert (DEST / name).read_bytes() == data, name
        else:
            DEST.mkdir(parents=True, exist_ok=True)
            (DEST / name).write_bytes(data)
    if args.preview:
        # Opaque dark/light contact sheet: original64, HUD-sized16 and11px.
        width, height = 288, 180
        board = bytearray(bytes((19, 25, 23, 255)) * width * height)
        for index, draw in enumerate(ART.values()):
            for size, y in ((64, 8), (16, 88), (11, 117), (11, 153)):
                c = Canvas(size); draw(c); data = c.finish()
                x = 96 * index + (96-size)//2
                for py in range(size):
                    for px in range(size):
                        at = (py*size+px)*4; dest = ((y+py)*width+x+px)*4
                        base = (220, 223, 218) if y > 140 else (19, 25, 23)
                        a = data[at+3]
                        for k in range(3): board[dest+k] = (data[at+k]*a+base[k]*(255-a)+127)//255
        png(args.preview, width, height, board)
    print(json.dumps({"checked": args.check, "assets": len(ART), "directory": str(DEST)}))


if __name__ == "__main__":
    main()
