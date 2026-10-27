#!/usr/bin/env python3
"""Generate original TCE2 navigation artwork using only Python's standard library.

Run from any directory; --check verifies every committed TGA and SOURCE.json.
--preview build/nav-icons-preview.png writes a small-scale contrast contact sheet.
No fonts, downloaded images, engine artwork, random values or timestamps are used.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[2]
DEST = ROOT / "src/assets/gfx/tce2/nav"
SAMPLES = 6
INK = (8, 12, 17, 255)
WHITE = (242, 248, 246, 255)
AMBER = (255, 192, 70, 255)


class Canvas:
    """64-unit geometric canvas, supersampled and alpha-correctly reduced."""
    def __init__(self, size):
        self.size = size
        self.width = size * SAMPLES
        self.scale = self.width / 64
        self.data = bytearray(self.width * self.width * 4)

    def polygon(self, points, color):
        points = [(x * self.scale, y * self.scale) for x, y in points]
        for y in range(max(0, int(min(p[1] for p in points))), min(self.width, math.ceil(max(p[1] for p in points)))):
            edges = []
            for (x1, y1), (x2, y2) in zip(points, points[1:] + points[:1]):
                if (y1 <= y + .5 < y2) or (y2 <= y + .5 < y1):
                    edges.append(x1 + (y + .5 - y1) * (x2 - x1) / (y2 - y1))
            edges.sort()
            for left, right in zip(edges[::2], edges[1::2]):
                start, end = max(0, math.ceil(left - .5)), min(self.width, math.ceil(right - .5))
                if end > start:
                    at = (y * self.width + start) * 4
                    self.data[at:at + (end - start) * 4] = bytes(color) * (end - start)

    def circle(self, x, y, radius, color):
        self.polygon([(x + radius * math.cos(i * math.tau / 96), y + radius * math.sin(i * math.tau / 96)) for i in range(96)], color)

    def line(self, points, color, width, closed=False):
        pairs = list(zip(points, points[1:] + (points[:1] if closed else [])))
        for (x1, y1), (x2, y2) in pairs:
            length = math.hypot(x2 - x1, y2 - y1)
            if not length:
                continue
            dx, dy = -(y2 - y1) / length * width / 2, (x2 - x1) / length * width / 2
            self.polygon([(x1 + dx, y1 + dy), (x2 + dx, y2 + dy), (x2 - dx, y2 - dy), (x1 - dx, y1 - dy)], color)
        for x, y in points:
            self.circle(x, y, width / 2, color)

    def finish(self):
        result = bytearray(self.size * self.size * 4)
        for y in range(self.size):
            for x in range(self.size):
                total = [0, 0, 0, 0]
                for sy in range(SAMPLES):
                    at = ((y * SAMPLES + sy) * self.width + x * SAMPLES) * 4
                    for sx in range(SAMPLES):
                        r, g, b, a = self.data[at + sx * 4:at + sx * 4 + 4]
                        total[0] += r * a; total[1] += g * a; total[2] += b * a; total[3] += a
                at = (y * self.size + x) * 4
                if total[3]:
                    result[at:at + 4] = bytes([round(v / total[3]) for v in total[:3]] + [round(total[3] / SAMPLES ** 2)])
        return bytes(result)


def player(c):
    shape = [(32, 7), (55, 44), (42, 41), (32, 56), (22, 41), (9, 44)]
    c.line(shape, INK, 6, True); c.polygon(shape, WHITE)
    c.polygon([(32, 20), (43, 38), (32, 33), (21, 38)], INK)


def self_player(c):
    shape = [(32, 5), (55, 55), (32, 44), (9, 55)]
    c.line(shape, INK, 7, True); c.polygon(shape, WHITE)
    c.polygon([(32, 14), (47, 46), (32, 39), (17, 46)], (63, 219, 238, 255))
    c.line([(32, 20), (32, 32)], WHITE, 3)


def dead(c):
    c.circle(32, 32, 26, INK); c.circle(32, 32, 22, WHITE); c.circle(32, 32, 17, INK)
    c.line([(22, 22), (42, 42)], INK, 10); c.line([(22, 42), (42, 22)], INK, 10)
    c.line([(23, 23), (41, 41)], WHITE, 5); c.line([(23, 41), (41, 23)], WHITE, 5)


def objective(c, letter):
    shield = [(17, 7), (47, 7), (57, 17), (57, 43), (47, 53), (32, 58), (17, 53), (7, 43), (7, 17)]
    c.line(shield, INK, 8, True); c.polygon(shield, INK)
    c.line(shield, AMBER, 3, True)
    c.polygon([(18, 11), (46, 11), (52, 17), (12, 17)], (57, 46, 29, 255))
    c.line([(24, 55), (32, 58), (40, 55)], WHITE, 1.4)
    if letter == "A":
        c.line([(20, 44), (32, 21), (44, 44)], WHITE, 5)
        c.line([(24, 36), (40, 36)], WHITE, 4)
    else:
        c.polygon([(22, 21), (35, 21), (43, 25), (43, 31), (39, 34), (45, 38), (45, 44), (36, 48), (22, 48)], WHITE)
        c.polygon([(27, 26), (34, 26), (38, 28), (38, 30), (34, 32), (27, 32)], INK)
        c.polygon([(27, 37), (35, 37), (40, 39), (40, 41), (35, 43), (27, 43)], INK)
    c.circle(12, 29, 1.3, AMBER); c.circle(52, 29, 1.3, AMBER)


def flag(c, color):
    cloth = [(16, 9), (51, 11), (47, 23), (52, 38), (16, 35)]
    c.line([(12, 9), (12, 56)], INK, 8); c.circle(12, 56, 5, INK)
    c.line(cloth, INK, 6, True); c.polygon(cloth, color)
    c.polygon([(43, 12), (51, 11), (47, 23), (52, 38), (43, 36)], tuple(round(v * .63) for v in color[:3]) + (255,))
    c.line([(12, 9), (12, 56)], WHITE, 3); c.circle(12, 56, 2, WHITE)
    # Original abstract tactical T + chevron; no national/team artwork is reused.
    c.polygon([(23, 17), (41, 17), (38, 21), (34, 21), (34, 28), (30, 28), (30, 21), (26, 21)], WHITE)
    c.polygon([(25, 26), (32, 30), (39, 26), (39, 30), (32, 34), (25, 30)], WHITE)


ART = {"player": (64, player), "player_self": (64, self_player), "player_dead": (64, dead),
       "objective_a": (128, lambda c: objective(c, "A")), "objective_b": (128, lambda c: objective(c, "B")),
       "flag_terror": (64, lambda c: flag(c, (225, 66, 57, 255))),
       "flag_specops": (64, lambda c: flag(c, (49, 135, 220, 255)))}


def tga(size, rgba):
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, size, size, 32, 0x28)
    bgra = bytes(channel for i in range(0, len(rgba), 4) for channel in (rgba[i + 2], rgba[i + 1], rgba[i], rgba[i + 3]))
    return header + bgra + bytes(8) + b"TRUEVISION-XFILE.\0"


def png(path, width, height, rgba):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)
    scan = b"".join(b"\0" + rgba[y * width * 4:(y + 1) * width * 4] for y in range(height))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(scan, 9)) + chunk(b"IEND", b""))


def preview(path, images):
    width, height = 1120, 430
    board = bytearray(bytes((21, 28, 36, 255)) * width * height)
    def paste(data, size, x, y):
        for py in range(size):
            for px in range(size):
                at = (py * size + px) * 4; dest = ((y + py) * width + x + px) * 4
                alpha = data[at + 3]
                for channel in range(3):
                    board[dest + channel] = (data[at + channel] * alpha + board[dest + channel] * (255 - alpha) + 127) // 255
    for index, (name, (size, draw)) in enumerate(ART.items()):
        x = index * 160
        paste(images[name], size, x + (160 - size) // 2, 16 + (128 - size) // 2)
        for target, y in [(32, 181), (24, 241), (16, 295)]:
            c = Canvas(target); draw(c); paste(c.finish(), target, x + (160 - target) // 2, y)
        for y in range(348, 414):
            for px in range(x + 16, x + 144):
                at = (y * width + px) * 4
                board[at:at + 4] = bytes((211, 215, 210, 255))
        c = Canvas(32); draw(c); paste(c.finish(), 32, x + 64, 365)
    png(path, width, height, board)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()
    generated, images, records = {}, {}, []
    for name, (size, draw) in ART.items():
        c = Canvas(size); draw(c); pixels = c.finish(); data = tga(size, pixels)
        assert len(pixels) == size * size * 4 and pixels[3] == 0 and pixels[(size // 2 * size + size // 2) * 4 + 3] > 0
        generated[name + ".tga"] = data; images[name] = pixels
        records.append({"asset": name + ".tga", "width": size, "height": size, "channels": "BGRA8, top-left TGA origin, straight alpha", "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    source = {"schema": 1, "origin": "Original repository-created geometric TCE2 navigation artwork; no external artwork or fonts.",
              "license": "CC0-1.0", "generator": "tools/assets/generate_nav_icons.py", "generator_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "renderer": "Python standard library polygon rasterizer, 6x supersampling, premultiplied coverage reduction", "files": records}
    generated["SOURCE.json"] = (json.dumps(source, indent=2) + "\n").encode()
    if args.check:
        for name, data in generated.items():
            if not (DEST / name).is_file() or (DEST / name).read_bytes() != data:
                raise SystemExit("Generated asset differs: " + name)
    else:
        DEST.mkdir(parents=True, exist_ok=True)
        for name, data in generated.items():
            (DEST / name).write_bytes(data)
    if args.preview:
        preview(args.preview, images)
    print(json.dumps({"checked": args.check, "assets": len(ART), "directory": str(DEST), "preview": str(args.preview) if args.preview else None}))


if __name__ == "__main__":
    main()
