#!/usr/bin/env python3
"""Generate first-party periodic native-ET detail layers and explicit material shaders.

These are neutral multiplicative microtextures, not normal/height/PBR maps.
No stock image is copied or replaced. --check is deterministic and offline.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
from generate_nav_icons import tga, png

ROOT = Path(__file__).resolve().parents[2]
DEST = ROOT / "src/assets/textures/tce2/details"
MATERIAL_DEST = ROOT / "src/assets/materials/tce2/world"
SIZE = 128
KINDS = ("asphalt", "concrete", "brick", "wood")
# Original shader stages and material surface flags are preserved. Implicit
# materials get the equivalent lightmap*diffuse shader, without compile flags.
MATERIALS = (
    ("textures/coroner/asphalt_e1", "asphalt_e1", "textures/coroner/asphalt_e1.jpg", "asphalt", (8, 8), ""),
    ("textures/coroner/cement_s1", "cement_s1", "textures/coroner/cement_s1.jpg", "concrete", (8, 4), ""),
    ("textures/twy_brickstiles/brick_dirt_7", "brick_dirt_7", "textures/twy_brickstiles/brick_dirt_7.jpg", "brick", (8, 8), ""),
    ("textures/bruce_trim/concrete_blue", "concrete_blue", "textures/bruce_crete/concrete_blue.tga", "concrete", (8, 2), ""),
    ("textures/liq/hout4", "wood_hout4", "textures/liq/hout4.tga", "wood", (4, 4), "mat_wood"),
)


def hashed(x, y, seed):
    value = (x * 0x9e3779b1 + y * 0x85ebca77 + seed * 0xc2b2ae3d) & 0xffffffff
    value ^= value >> 16
    value = value * 0x7feb352d & 0xffffffff
    value ^= value >> 15
    value = value * 0x846ca68b & 0xffffffff
    value ^= value >> 16
    return value / 4294967295.0


def periodic(x, y, grid, seed):
    # Smooth periodic lattice, including all four tile borders.
    gx, gy = x * grid / SIZE, y * grid / SIZE
    ix, iy = math.floor(gx), math.floor(gy)
    fx, fy = gx - ix, gy - iy
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a, b = hashed(ix % grid, iy % grid, seed), hashed((ix + 1) % grid, iy % grid, seed)
    c, d = hashed(ix % grid, (iy + 1) % grid, seed), hashed((ix + 1) % grid, (iy + 1) % grid, seed)
    return ((a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy) * 2 - 1


def sample(kind, x, y):
    fine = periodic(x, y, 32, 17)
    medium = periodic(x, y, 16, 29)
    coarse = periodic(x, y, 8, 43)
    if kind == "asphalt":
        # Angular aggregate and finer dust; no wet/reflective pretence.
        return 22 * fine + 12 * medium + 3 * coarse
    if kind == "concrete":
        # Restrained pores with a soft granular base.
        pit = min(0, periodic(x, y, 32, 71) + .42)
        return 12 * fine + 7 * medium + 19 * pit
    if kind == "brick":
        # Fired-clay grain only; no new brick joints across the original mortar.
        return 15 * fine + 8 * medium + 4 * coarse
    # Vertical fibres follow the inspected original hout4. All phases are
    # integer periods so adjacent tiles meet, including the warp field.
    phase = 2 * math.pi * (x * 24 / SIZE + .13 * periodic(x, y, 4, 83))
    fibres = math.sin(phase) * (.6 + .4 * periodic(x, y, 8, 97))
    return 14 * fibres + 6 * fine + 3 * medium


def pixels(kind):
    raw = [sample(kind, x, y) for y in range(SIZE) for x in range(SIZE)]
    mean = sum(raw) / len(raw)
    values = [max(80, min(175, round(127.5 + value - mean))) for value in raw]
    # Exact neutral global mean for GL_DST_COLOR GL_SRC_COLOR (2*src*dst).
    error = int(127.5 * len(values)) - sum(values)
    for index in sorted(range(len(values)), key=lambda i: hashed(i % SIZE, i // SIZE, 109))[:abs(error)]:
        values[index] += 1 if error > 0 else -1
    assert sum(values) == int(127.5 * len(values))
    assert min(values) >= 80 and max(values) <= 175
    return bytes(channel for v in values for channel in (v, v, v, 255)), values


def shader_text(material):
    original, name, base, kind, scale, flag = material
    text = "// Native ET detail material. Install via DynamicShader before LoadWorldMap.\n"
    text += "// Outside scripts/: no automatic override, no runtime remap/lightmap loss.\n"
    text += f"\n{original}\n{{\n\tqer_editorimage {base}\n"
    if flag:
        text += f"\tsurfaceparm {flag}\n"
    text += f"\t{{\n\t\tmap $lightmap\n\t\trgbGen identity\n\t}}\n\t{{\n\t\tmap {base}\n\t\tblendFunc GL_DST_COLOR GL_ZERO\n\t\trgbGen identity\n\t}}\n"
    text += f"\t{{\n\t\tmap textures/tce2/details/{kind}.tga\n\t\tblendFunc GL_DST_COLOR GL_SRC_COLOR\n\t\trgbGen identity\n\t\ttcMod scale {scale[0]} {scale[1]}\n\t\tdetail\n\t}}\n}}\n"
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--preview", type=Path)
    args = parser.parse_args()
    output, records, images = {}, [], {}
    for kind in KINDS:
        rgba, values = pixels(kind)
        # Periodicity is checked on the continuous generator, not by making
        # duplicate edge rows (which would leave a visible repeated seam).
        for x, y in ((0, 0), (17.25, 41.5), (127.5, 99.75)):
            assert abs(sample(kind, x, y) - sample(kind, x + SIZE, y)) < 1e-8
            assert abs(sample(kind, x, y) - sample(kind, x, y + SIZE)) < 1e-8
        data = tga(SIZE, rgba)
        output[DEST / (kind + ".tga")] = data
        images[kind] = rgba
        records.append({"asset": kind + ".tga", "width": SIZE, "height": SIZE, "min": min(values),
                        "max": max(values), "mean": sum(values) / len(values),
                        "blend": "2*src*dst, exact neutral global mean", "sha256": hashlib.sha256(data).hexdigest()})
    materials = []
    for row in MATERIALS:
        original, name, base, kind, scale, flag = row
        shader = shader_text(row).encode()
        path = MATERIAL_DEST / (name + ".shader")
        output[path] = shader
        materials.append({"path": path.relative_to(ROOT / "src/assets").as_posix(), "original_name": original,
                          "diffuse": base, "detail": kind, "scale": list(scale), "surfaceparm": flag,
                          "bytes": len(shader), "sha256": hashlib.sha256(shader).hexdigest()})
    metadata = {"schema": 1, "origin": "Original deterministic periodic material microstructure; no stock texture pixels copied.",
                "license": "CC0-1.0", "generator": "tools/assets/generate_world_details.py",
                "generator_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                "load": "Explicit DynamicShader original-name install before LoadWorldMap; renderer restart for A/B; no runtime remap.",
                "materials": materials, "files": records}
    output[DEST / "SOURCE.json"] = (json.dumps(metadata, indent=2) + "\n").encode()
    for path, data in output.items():
        if args.check:
            assert path.is_file() and path.read_bytes() == data, path
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    if args.preview:
        # Runtime values (no extra contrast). 2x2 tiles verify visible seams.
        width, height = 4 * SIZE * 2, SIZE * 2
        board = bytearray(width * height * 4)
        for col, kind in enumerate(KINDS):
            rgba = images[kind]
            for y in range(height):
                for x in range(SIZE * 2):
                    at = ((y % SIZE) * SIZE + x % SIZE) * 4
                    dest = (y * width + col * SIZE * 2 + x) * 4
                    board[dest:dest + 4] = rgba[at:at + 4]
        png(args.preview, width, height, board)
    print(json.dumps({"checked": args.check, "textures": len(KINDS), "materials": len(MATERIALS), "entries": [str(p.relative_to(ROOT / 'src/assets')).replace('\\', '/') for p in output]}))


if __name__ == "__main__":
    main()
