#!/usr/bin/env python3
"""Generate local native pose fixtures from actual PK3s and recorded renderer tags.

This optional diagnostic consumes local PK3 assets and recorded captures. Its
independent Python oracle/parser are self-contained by default. It
does not modify assets, production code, build files, or a running game. Five
expected tags come directly from captures; two thigh expectations are explicitly
analytic-only values from the independently observed Python arithmetic profile.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import importlib.util
import json
import math
from pathlib import Path, PurePosixPath
from types import SimpleNamespace
import re
import zipfile
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
EVIDENCE = ROOT / "src/reconstruction/modernization"
CAPTURES = ("pose-32152-1", "pose-control-crouch", "pose-control-prone",
            "pose-control-lean-left", "pose-control-lean-right",
            "pose-control-prone-aim")
ANCHORS = ("tag_head", "tag_chest", "tag_torso", "tag_legleft",
           "tag_legright", "tag_footleft", "tag_footright")
CAPTURED_MASK = 0x67
ANALYTIC_MASK = 0x18


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


# Independent Python test oracle and asset resolver. Derived from this project's
# measured local analysis, not copied from an engine implementation. Its fixed
# profile is verified against five captured renderer tags; thighs are oracle-only.
def finite(values):
    return all(math.isfinite(x) for x in values)

class Reader:
    def __init__(self, data):
        self.data = data

    def span(self, offset, count):
        require(isinstance(offset, int) and isinstance(count, int) and
                offset >= 0 and count >= 0 and offset <= len(self.data) and
                count <= len(self.data) - offset, "asset range outside file")
        return self.data[offset:offset+count]

    def unpack(self, fmt, offset):
        self.span(offset, struct.calcsize(fmt))
        return struct.unpack_from(fmt, self.data, offset)

    def name(self, offset):
        return self.span(offset, 64).split(b"\0", 1)[0].decode("ascii", "strict")

def add(a, b):
    return tuple(x+y for x, y in zip(a, b))

def sub(a, b):
    return tuple(x-y for x, y in zip(a, b))

def scale(v, s):
    return tuple(x*s for x in v)

def lerp(a, b, t):
    return add(scale(a, 1-t), scale(b, t))

def length(v):
    return math.sqrt(sum(x*x for x in v))

def normalize(v):
    size = length(v)
    require(size > 1e-10 and math.isfinite(size), "degenerate direction blend")
    return scale(v, 1/size)

def matvec(matrix, vector):
    """Rows are a linear operator (bone matrix contract)."""
    return tuple(sum(x*y for x, y in zip(row, vector)) for row in matrix)

def transpose(matrix):
    return tuple(zip(*matrix))

def matmul(a, b):
    return tuple(tuple(sum(a[i][k]*b[k][j] for k in range(3))
                       for j in range(3)) for i in range(3))

def basisvec(axis, vector):
    """Rows are world-space basis vectors (refEntity axis contract)."""
    return matvec(transpose(axis), vector)

def angles_axis(radians):
    pitch, yaw, roll = radians
    sp, cp, sy, cy, sr, cr = (math.sin(pitch), math.cos(pitch),
                             math.sin(yaw), math.cos(yaw),
                             math.sin(roll), math.cos(roll))
    return ((cp*cy, cp*sy, -sp),
            (sr*sp*cy-cr*sy, sr*sp*sy+cr*cy, sr*cp),
            (cr*sp*cy+sr*sy, cr*sp*sy-sr*cy, cr*cp))

def angle_blend(old, new, front):
    return tuple(a + (((b-a+math.pi) % math.tau)-math.pi)*front
                 for a, b in zip(old, new))

def direction(angles):
    pitch, yaw = angles
    return (math.cos(pitch)*math.cos(yaw), math.cos(pitch)*math.sin(yaw),
            -math.sin(pitch))

@dataclass(frozen=True)
class Bone:
    name: str
    parent: int
    weight: float
    distance: float
    flags: int

class Skeleton:
    def __init__(self, data, name="unnamed"):
        self.reader = r = Reader(data)
        require(r.span(0, 4) == b"MDXW" and r.unpack("<i", 4)[0] == 2,
                "unsupported MDX signature/version")
        self.name = name
        (self.frames, self.count, self.frame_offset, bone_offset,
         self.torso_parent, end) = r.unpack("<6i", 72)
        require(1 <= self.frames <= 100000 and 1 <= self.count <= 128,
                "invalid MDX dimensions")
        require(end == len(data) and 0 <= self.torso_parent < self.count,
                "invalid MDX file/torso parent")
        self.stride = 52+12*self.count
        require(self.frame_offset >= 96 and bone_offset >= 96,
                "MDX data overlaps header")
        r.span(self.frame_offset, self.frames*self.stride)
        r.span(bone_offset, self.count*80)
        self.bones = []
        for i in range(self.count):
            offset = bone_offset+i*80
            parent, weight, distance, flags = r.unpack("<iffi", offset+64)
            require(-1 <= parent < i and (parent >= 0 or i == 0),
                    "MDX must contain an ordered acyclic single-root hierarchy")
            require(finite((weight, distance)) and 0 <= weight <= 1 and distance >= 0,
                    "invalid MDX bone weight/distance")
            self.bones.append(Bone(r.name(offset), parent, weight, distance, flags))
        require(len({bone.name for bone in self.bones}) == self.count,
                "duplicate MDX bone names")

    def frame(self, frame):
        require(isinstance(frame, int) and 0 <= frame < self.frames,
                "MDX frame outside clip")
        offset = self.frame_offset+frame*self.stride
        root = self.reader.unpack("<3f", offset+40)
        require(finite(root), "nonfinite MDX root")
        decoded = []
        for i in range(self.count):
            values = self.reader.unpack("<6h", offset+52+i*12)
            decoded.append((tuple(x*math.tau/65536 for x in values[:3]),
                            direction(tuple(x*math.tau/65536 for x in values[4:]))))
        return root, decoded

    def compatible(self, other):
        # Shipped clips have different parentDist values, including >1-unit
        # thigh differences. Pose distances/weights come from current LEGS
        # metadata, while the other three clips supply compressed channels.
        require([(b.name, b.parent, b.weight, b.flags) for b in self.bones] ==
                [(b.name, b.parent, b.weight, b.flags) for b in other.bones] and
                self.torso_parent == other.torso_parent,
                "incompatible MDX skeletons in split pose")

class Mesh:
    def __init__(self, data, skeleton):
        r = Reader(data)
        require(r.span(0, 4) == b"MDMW" and r.unpack("<i", 4)[0] == 3,
                "unsupported MDM signature/version")
        surfaces, surface_offset, count, offset, end = r.unpack("<5i", 80)
        require(end == len(data) and 0 <= count <= 128 and 0 <= surfaces <= 32,
                "invalid MDM dimensions")
        self.tags = {}
        for _ in range(count):
            require(offset >= 100, "MDM tag overlaps header")
            name = r.name(offset)
            axis = r.unpack("<9f", offset+64)
            bone = r.unpack("<i", offset+100)[0]
            location = r.unpack("<3f", offset+104)
            refs, refs_offset, next_offset = r.unpack("<3i", offset+116)
            require(0 <= bone < skeleton.count and finite((*axis, *location)) and
                    0 <= refs <= skeleton.count and next_offset >= 128 and
                    refs_offset >= 128 and refs_offset+refs*4 <= next_offset,
                    "invalid MDM tag")
            r.span(offset, next_offset)
            references = r.unpack(f"<{refs}i", offset+refs_offset)
            require(all(0 <= ref < skeleton.count for ref in references),
                    "invalid MDM tag bone reference")
            require(name not in self.tags, "duplicate MDM tag")
            self.tags[name] = (bone, location, tuple(axis[i:i+3] for i in (0, 3, 6)))
            offset += next_offset
        # Retain weighted body vertices for bounded geometry experiments too.
        self.surfaces = []
        offset = surface_offset
        for _ in range(surfaces):
            require(offset >= 100, "MDM surface overlaps header")
            # Eleven ints follow name+shader; avoid relying on a native C layout.
            (_, _, header_delta, verts, verts_offset, triangles, triangle_offset,
             collapse_offset, refs, refs_offset, next_offset) = r.unpack("<11i", offset+132)
            require(header_delta == -offset and 0 <= verts <= 6000 and
                    0 <= triangles <= 8192 and 0 <= refs <= skeleton.count and
                    next_offset >= 176, "invalid MDM surface")
            r.span(offset, next_offset)
            for relative, size in ((verts_offset, 0), (triangle_offset, triangles*12),
                                   (collapse_offset, verts*4), (refs_offset, refs*4)):
                require(relative >= 176 and relative+size <= next_offset,
                        "MDM surface array outside surface")
            references = r.unpack(f"<{refs}i", offset+refs_offset)
            require(all(0 <= x < skeleton.count for x in references),
                    "invalid MDM surface bone reference")
            indices = r.unpack(f"<{triangles*3}i", offset+triangle_offset)
            require(all(0 <= x < verts for x in indices), "invalid MDM triangle")
            vertices = []
            position = offset+verts_offset
            for _ in range(verts):
                require(position+24 <= offset+next_offset, "MDM vertex outside surface")
                weights = r.unpack("<i", position+20)[0]
                require(1 <= weights <= skeleton.count and
                        position+24+20*weights <= offset+next_offset,
                        "invalid MDM vertex weights")
                vertex = []
                for j in range(weights):
                    bone, weight, *location = r.unpack("<if3f", position+24+20*j)
                    require(bone in references and finite((weight, *location)) and
                            0 <= weight <= 1, "invalid MDM weight data")
                    vertex.append((bone, weight, location))
                require(abs(sum(w[1] for w in vertex)-1) < .002,
                        "MDM vertex weights are not normalized")
                vertices.append(vertex)
                position += 24+20*weights
            self.surfaces.append({"name": r.name(offset+4), "vertices": vertices,
                                  "triangles": list(zip(*[iter(indices)]*3))})
            offset += next_offset

@dataclass(frozen=True)
class FramePair:
    current_model: Skeleton
    current: int
    old_model: Skeleton
    old: int
    backlerp: float = 0

    def sample(self):
        self.current_model.compatible(self.old_model)
        require(math.isfinite(self.backlerp) and 0 <= self.backlerp <= 1,
                "invalid MDX backlerp")
        root, current = self.current_model.frame(self.current)
        old_root, old = self.old_model.frame(self.old)
        return (lerp(root, old_root, self.backlerp),
                [(angle_blend(a[0], b[0], 1-self.backlerp),
                  normalize(lerp(a[1], b[1], 1-self.backlerp)))
                 for a, b in zip(old, current)])

def tag_pose(mesh, bones, name):
    index, offset, axes = mesh.tags[name]
    positions, matrices = bones
    return (add(positions[index], matvec(matrices[index], offset)),
            tuple(matvec(matrices[index], axis) for axis in axes))

pose = SimpleNamespace(ROOT=ROOT, require=require, finite=finite, add=add, sub=sub, scale=scale, lerp=lerp, length=length, normalize=normalize, matvec=matvec, transpose=transpose, matmul=matmul, basisvec=basisvec, angles_axis=angles_axis, angle_blend=angle_blend, direction=direction, Bone=Bone, Skeleton=Skeleton, Mesh=Mesh, FramePair=FramePair, tag_pose=tag_pose)
p = pose

class Assets:
    def __init__(self, mod_pk3, base_pk3):
        self.archives = [zipfile.ZipFile(path) for path in (mod_pk3, base_pk3)]
        self.hashes = {}
        self.skeletons = {}

    def close(self):
        for archive in self.archives:
            archive.close()

    def read(self, path):
        pose.require(not PurePosixPath(path).is_absolute() and ".." not in PurePosixPath(path).parts,
                     "invalid asset path")
        for archive in self.archives:
            try:
                info = archive.getinfo(path)
            except KeyError:
                continue
            pose.require(info.file_size <= 64*1024*1024, "asset exceeds probe size bound")
            data = archive.read(info)
            self.hashes[path] = hashlib.sha256(data).hexdigest()
            return data
        raise ValueError("missing installed asset: "+path)

    def skeleton(self, path):
        if path not in self.skeletons:
            self.skeletons[path] = pose.Skeleton(self.read(path), path)
        return self.skeletons[path]

    def character(self, path):
        text = self.read(path).decode("ascii")
        fields = {}
        for field in ("mesh", "hudhead", "animationgroup"):
            match = re.search(r"\b"+field+r'\s+"([^"\r\n]+)"', text, re.I)
            pose.require(match is not None, "character lacks "+field)
            fields[field] = match[1]
        text = self.read(fields["animationgroup"]).decode("ascii")
        animations = {}
        for model, content in re.findall(r'\banimfile\s+"([^"]+)"\s*\{(.*?)\}', text, re.S | re.I):
            includes = re.findall(r'#include\s+"([^"]+)"', content)
            pose.require(includes, "unsupported animation group block without include")
            for include in includes:
                for line in self.read(include).decode("ascii").splitlines():
                    match = re.match(r"\s*([\w]+)\s+(-?\d+)\s+(-?\d+)\s+", line)
                    if match:
                        name = match[1].lower()
                        pose.require(name not in animations or animations[name] == model,
                                     "ambiguous animation model association: "+name)
                        animations[name] = model
        fields["animations"] = animations
        return fields

def read_records(text):
    samples = {}
    for line in text.splitlines():
        if "H3POSE " in line:
            payload = line.split("H3POSE ", 1)[1]
        elif line.lstrip().startswith("{"):
            payload = line.strip()
        else:
            continue
        record = json.loads(payload)
        if record.get("kind") == "h3-renderer-capture-test-only":
            continue
        if "id" not in record or "part" not in record:
            continue
        pose.require(isinstance(record.get("id"), int), "capture lacks integer sample id")
        sample = samples.setdefault(record["id"], {"models": {}, "tag_axes": {}})
        part = record["part"]
        if part == "model":
            sample["models"][record["handle"]] = record["animation"]
        elif part == "tag_axis":
            sample["tag_axes"][record["name"]] = record["data"]
        else:
            pose.require(part not in sample, "duplicate capture part: "+part)
            sample[part] = record.get("data", record.get("file", True))
    return samples

def f32(value):
    return struct.unpack("<f", struct.pack("<f", value))[0]

def short_diff(a, b):
    return ((a-b+32768) & 65535)-32768

class Arithmetic:
    def __init__(self, table_size, fast, partial_current_only=False, torso_sameframe_reference=True, stored_angles=True):
        self.size, self.fast = table_size, fast
        self.partial_current_only = partial_current_only
        self.torso_sameframe_reference = torso_sameframe_reference
        self.stored_angles = stored_angles
        self.table = [f32(math.sin(f32(f32(i*360./(table_size-1))*f32(math.pi))/180.))
                      for i in range(table_size)] if table_size else None

    def sincos(self, value):
        if self.table:
            index = (int(value) & 65535)//(65536//self.size)
            return self.table[index], self.table[(index+self.size//4) % self.size]
        angle = value*math.tau/65536
        return math.sin(angle), math.cos(angle)

    def direction(self, values):
        sp, cp = self.sincos(values[0])
        sy, cy = self.sincos(values[1])
        return (f32(cp*cy), f32(cp*sy), -sp)

    def axis(self, values):
        sp, cp = self.sincos(values[0])
        sy, cy = self.sincos(values[1])
        sr, cr = self.sincos(values[2])
        return tuple(tuple(f32(v) for v in row) for row in
                     ((cp*cy, cp*sy, -sp),
                      (sr*sp*cy-cr*sy, sr*sp*sy+cr*cy, sr*cp),
                      (cr*sp*cy+sr*sy, cr*sp*sy-sr*cy, cr*cp)))

    def normalize(self, vector):
        if not self.fast:
            return p.normalize(vector)
        length2 = f32(sum(f32(v*v) for v in vector))
        bits = struct.unpack("<I", struct.pack("<f", length2))[0]
        initial = struct.unpack("<f", struct.pack("<I", 0x5f3759df-(bits >> 1)))[0]
        half = f32(.5*length2)
        inverse = f32(initial*f32(1.5-f32(f32(half*initial)*initial)))
        return tuple(f32(value*inverse) for value in vector)

    def blend_direction(self, old, new, front):
        return self.normalize(tuple(f32(f32(a*f32(1-front))+f32(b*front))
                                    for a, b in zip(old, new)))

    def channels(self, model, frame):
        model.frame(frame)  # Keep all original frame/range/finite checks.
        offset = model.frame_offset+model.stride*frame
        return [model.reader.unpack("<6h", offset+52+i*12) for i in range(model.count)]

    def evaluate(self, legs, torso, torso_axis):
        skeleton = legs.current_model
        skeleton.compatible(torso.current_model)
        # Renderer can collapse equal model/frame slots to no interpolation.
        # The local reference has a different-model comparison in the torso
        # condition; retain it as an explicit hypothesis until split capture.
        leg_back = 0 if (legs.current_model is legs.old_model and legs.current == legs.old) else legs.backlerp
        torso_equal_model = (torso.old_model is legs.old_model if self.torso_sameframe_reference
                             else torso.old_model is torso.current_model)
        torso_back = 0 if torso.current == torso.old and torso_equal_model else torso.backlerp
        lc, lo, tc, to = [self.channels(pair.current_model if current else pair.old_model,
                                        pair.current if current else pair.old)
                          for pair, current in ((legs, True), (legs, False),
                                                (torso, True), (torso, False))]
        roots = [pair.current_model.frame(pair.current)[0] for pair in (legs,)]
        old_root = legs.old_model.frame(legs.old)[0]
        root = tuple(f32(x) for x in p.lerp(roots[0], old_root, leg_back))
        positions, matrices = [], []
        interpolate = leg_back != 0 or torso_back != 0
        for index, bone in enumerate(skeleton.bones):
            angles = []
            for axis in range(3):
                if self.stored_angles:
                    leg = int(f32(lc[index][axis]-f32(leg_back*short_diff(lc[index][axis], lo[index][axis]))))
                    upper = int(f32(tc[index][axis]-f32(torso_back*short_diff(tc[index][axis], to[index][axis]))))
                    angles.append(int(f32(leg+f32(bone.weight*short_diff(upper, leg)))))
                else:
                    leg = int(lc[index][axis]-leg_back*short_diff(lc[index][axis], lo[index][axis]))
                    upper = int(tc[index][axis]-torso_back*short_diff(tc[index][axis], to[index][axis]))
                    angles.append(int(leg+bone.weight*short_diff(upper, leg)))
            if interpolate:
                matrices.append(self.axis(angles))
            else:
                radians = p.angle_blend(tuple(x*math.tau/65536 for x in lc[index][:3]),
                                        tuple(x*math.tau/65536 for x in tc[index][:3]), bone.weight)
                matrices.append(tuple(tuple(f32(x) for x in row) for row in p.angles_axis(radians)))
            ld, old_ld = self.direction(lc[index][4:]), self.direction(lo[index][4:])
            td, old_td = self.direction(tc[index][4:]), self.direction(to[index][4:])
            if bone.weight == 0:
                vector = self.blend_direction(old_ld, ld, 1-leg_back) if interpolate else ld
            elif bone.weight == 1:
                vector = self.blend_direction(old_td, td, 1-torso_back) if interpolate else td
            else:
                ld = self.blend_direction(old_ld, ld, 1-leg_back) if interpolate else ld
                if self.partial_current_only:
                    old_td = td
                td = self.blend_direction(old_td, td, 1-torso_back) if interpolate else td
                vector = self.blend_direction(ld, td, bone.weight)
            positions.append(root if bone.parent < 0 else
                             tuple(f32(a+f32(bone.distance*b))
                                   for a, b in zip(positions[bone.parent], vector)))
        pivot = positions[skeleton.torso_parent]
        rotation = p.transpose(torso_axis)
        for index, bone in enumerate(skeleton.bones):
            if not bone.weight:
                continue
            weighted = tuple(tuple(f32((1-bone.weight)*(i == j)+bone.weight*rotation[i][j])
                                   for j in range(3)) for i in range(3))
            positions[index] = tuple(f32(x) for x in p.add(pivot, p.matvec(weighted, p.sub(positions[index], pivot))))
            matrices[index] = tuple(tuple(f32(x) for x in row) for row in p.matmul(weighted, matrices[index]))
        return positions, matrices

def local_oracle():
    return SimpleNamespace(p=pose, replay=SimpleNamespace(Assets=Assets, read_records=read_records), Arithmetic=Arithmetic)

def load_oracle(path: Path):
    # The local oracle imports its separately hashed parser/matrix module.
    sys.path.insert(0, str(path.parent))
    spec = importlib.util.spec_from_file_location("pose_fixture_oracle", path)
    require(spec is not None and spec.loader is not None, "cannot load oracle")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def compact_mdx(raw: bytes, frames: set[int], pose):
    original = pose.Skeleton(raw)
    ordered = sorted(frames)
    require(ordered, "empty MDX frame selection")
    require(all(0 <= frame < original.frames for frame in ordered), "invalid frame selection")
    bone_offset = struct.unpack_from("<i", raw, 84)[0]
    metadata = raw[bone_offset:bone_offset + original.count * 80]
    header = bytearray(raw[:96])
    compact_bone_offset = 96 + len(ordered) * original.stride
    end = compact_bone_offset + len(metadata)
    struct.pack_into("<6i", header, 72, len(ordered), original.count, 96,
                     compact_bone_offset, original.torso_parent, end)
    chunks = [raw[original.frame_offset + frame * original.stride:
                  original.frame_offset + (frame + 1) * original.stride]
              for frame in ordered]
    data = bytes(header) + b"".join(chunks) + metadata
    parsed = pose.Skeleton(data)
    require(parsed.bones == original.bones and parsed.torso_parent == original.torso_parent,
            "compaction changed skeleton metadata")
    for index, frame in enumerate(ordered):
        require(data[parsed.frame_offset + index * parsed.stride:
                     parsed.frame_offset + (index + 1) * parsed.stride] == chunks[index],
                "compaction changed frame bytes")
    require(len(data) == end, "compact MDX length mismatch")
    return data, ordered, {frame: index for index, frame in enumerate(ordered)}


def closure(skeleton, mesh):
    selected = {skeleton.torso_parent}
    for name in ANCHORS:
        require(name in mesh.tags, "missing anchor tag " + name)
        selected.add(mesh.tags[name][0])
    for index in tuple(selected):
        parent = skeleton.bones[index].parent
        while parent >= 0:
            selected.add(parent)
            parent = skeleton.bones[parent].parent
    return sorted(selected)


def number(value):
    require(isinstance(value, (int, float)) and math.isfinite(value), "nonfinite fixture number")
    text = repr(float(value))
    return text + "f"


def c_array(value):
    if isinstance(value, (list, tuple)):
        return "{ " + ", ".join(c_array(element) for element in value) + " }"
    return number(value)


def byte_array(name, data):
    lines = [f"static const unsigned char {name}[] = {{"]
    lines.extend("    " + ", ".join(f"0x{x:02x}" for x in data[offset:offset+16]) + ","
                 for offset in range(0, len(data), 16))
    lines.append("};")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--captures", type=Path, default=EVIDENCE / "runtime-bugfix-20261026")
    parser.add_argument("--mod-pk3", type=Path, default=ROOT / "build/navgen-overkill/tce2/pak2.pk3")
    parser.add_argument("--base-pk3", type=Path, default=ROOT / "build/navgen-overkill/etmain/pak0.pk3")
    parser.add_argument("--oracle", type=Path, help="Optional external Python oracle for cross-checking; default is self-contained")
    parser.add_argument("--out", type=Path, default=EVIDENCE / "pose-evaluator-20261026/generated")
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    oracle = load_oracle(args.oracle.resolve()) if args.oracle else local_oracle()
    pose, replay = oracle.p, oracle.replay
    arithmetic = oracle.Arithmetic(4096, True, True, True, False)
    assets = Assets(args.mod_pk3, args.base_pk3)
    cases, frame_sets, characters, meshes, capture_hashes = [], {}, {}, {}, {}
    try:
        for stem in CAPTURES:
            path = args.captures / (stem + ".jsonl")
            raw = path.read_bytes()
            capture_hashes[stem] = {"path": str(path.resolve()), "sha256": digest(raw)}
            records = replay.read_records(raw.decode("utf-8"))
            require(records, "capture has no renderer evidence: " + stem)
            for original_id, sample in sorted(records.items()):
                required = ("end", "character", "frames", "torso_axis", "body_axis", "body_origin")
                require(all(name in sample for name in required), "incomplete capture")
                character_path = sample["character"]
                if character_path not in characters:
                    characters[character_path] = assets.character(character_path)
                character = characters[character_path]
                mapped = {handle: character["animations"][animation.lower()]
                          for handle, animation in sample["models"].items()}
                values = sample["frames"]
                require(len(values) == 10, "invalid frame tuple")
                paths = [mapped[values[index]] for index in (0, 2, 5, 7)]
                frames = [values[index] for index in (1, 3, 6, 8)]
                for name, frame in zip(paths, frames):
                    frame_sets.setdefault(name, set()).add(frame)
                pairs = [pose.FramePair(assets.skeleton(paths[2*i]), frames[2*i],
                                        assets.skeleton(paths[2*i+1]), frames[2*i+1], values[5*i+4])
                         for i in range(2)]
                mesh_path = character["mesh"]
                if mesh_path not in meshes:
                    meshes[mesh_path] = pose.Mesh(assets.read(mesh_path), pairs[0].current_model)
                mesh = meshes[mesh_path]
                bones = arithmetic.evaluate(*pairs, sample["torso_axis"])
                origins, axes, profile_errors = [], [], {}
                for index, name in enumerate(ANCHORS):
                    analytic_origin, analytic_axis = pose.tag_pose(mesh, bones, name)
                    if CAPTURED_MASK & (1 << index):
                        require(name in sample and name in sample["tag_axes"], "missing captured anchor")
                        # Do not replace captured values with a re-evaluated oracle.
                        origins.append(sample[name]); axes.append(sample["tag_axes"][name])
                        profile_errors[name] = {
                            "origin": pose.length(pose.sub(analytic_origin, sample[name])),
                            "axis": max(abs(analytic_axis[i][j] - sample["tag_axes"][name][i][j])
                                        for i in range(3) for j in range(3))}
                    else:
                        origins.append(analytic_origin); axes.append(analytic_axis)
                cases.append({"capture": stem, "original_id": original_id, "mesh": mesh_path,
                              "models": paths, "original_frames": frames,
                              "backlerp": [values[4], values[9]], "origin": sample["body_origin"],
                              "axis": sample["body_axis"], "torso_axis": sample["torso_axis"],
                              "expected_origin": origins, "expected_axis": axes,
                              "captured_mask": CAPTURED_MASK, "analytic_mask": ANALYTIC_MASK,
                              "closure": closure(pairs[0].current_model, mesh),
                              "profile_vs_capture": profile_errors})
        require(len(cases) == 176, "expected the six successful captures / 176 frames")
        model_paths, mesh_paths = sorted(frame_sets), sorted(meshes)
        compact, original_frames, remaps = {}, {}, {}
        for index, path in enumerate(model_paths):
            compact[path], original_frames[path], remaps[path] = compact_mdx(assets.read(path), frame_sets[path], pose)
            (args.out / f"model-{index}.mdx").write_bytes(compact[path])
        mesh_bytes = {name: assets.read(name) for name in mesh_paths}
        for index, path in enumerate(mesh_paths):
            (args.out / f"mesh-{index}.mdm").write_bytes(mesh_bytes[path])
        for case in cases:
            case["model"] = [model_paths.index(path) for path in case["models"]]
            case["frame"] = [remaps[path][frame] for path, frame in zip(case["models"], case["original_frames"])]
            case["mesh_index"] = mesh_paths.index(case["mesh"])
        lines = ["/* LOCAL GENERATED TEST INPUTS: captured tags plus labeled analytic thighs. */",
                 "#ifndef TCE_POSE_GENERATED_FIXTURES", "#define TCE_POSE_GENERATED_FIXTURES", "#include <stddef.h>",
                 "typedef struct { const char *name; const unsigned char *bytes; size_t size; const int *originalFrame; size_t frameCount; } tcePoseFixtureAsset_t;",
                 "typedef struct { const char *capture; int originalId; unsigned int mesh; unsigned int model[4]; int frame[4]; float backlerp[2]; float origin[3]; float axis[3][3]; float torsoAxis[3][3]; float expectedOrigin[7][3]; float expectedAxis[7][3][3]; unsigned int capturedMask; unsigned int analyticMask; } tcePoseFixtureCase_t;"]
        for index, path in enumerate(model_paths):
            lines += [byte_array(f"tcePoseFixtureModel{index}", compact[path]),
                      f"static const int tcePoseFixtureFrames{index}[] = {{ " + ", ".join(map(str, original_frames[path])) + " };" ]
        for index, path in enumerate(mesh_paths):
            lines.append(byte_array(f"tcePoseFixtureMesh{index}", mesh_bytes[path]))
        lines.append("static const tcePoseFixtureAsset_t tcePoseFixtureModels[] = {")
        for index, path in enumerate(model_paths):
            lines.append(f"    {{ {json.dumps(path)}, tcePoseFixtureModel{index}, sizeof(tcePoseFixtureModel{index}), tcePoseFixtureFrames{index}, {len(original_frames[path])} }},")
        lines.append("};\nstatic const tcePoseFixtureAsset_t tcePoseFixtureMeshes[] = {")
        for index, path in enumerate(mesh_paths):
            lines.append(f"    {{ {json.dumps(path)}, tcePoseFixtureMesh{index}, sizeof(tcePoseFixtureMesh{index}), NULL, 0 }},")
        lines += ["};", "static const char *const tcePoseFixtureAnchorNames[7] = { " + ", ".join(map(json.dumps, ANCHORS)) + " };",
                  "static const tcePoseFixtureCase_t tcePoseFixtureCases[] = {"]
        for case in cases:
            ints = lambda values: "{ " + ", ".join(map(str, values)) + " }"
            fields = [json.dumps(case["capture"]), str(case["original_id"]), str(case["mesh_index"]),
                      ints(case["model"]), ints(case["frame"]), c_array(case["backlerp"]),
                      c_array(case["origin"]), c_array(case["axis"]), c_array(case["torso_axis"]),
                      c_array(case["expected_origin"]), c_array(case["expected_axis"]), "0x67u", "0x18u"]
            lines.append("    { " + ", ".join(fields) + " },")
        lines += ["};", f"#define TCE_POSE_FIXTURE_MODEL_COUNT {len(model_paths)}",
                  f"#define TCE_POSE_FIXTURE_MESH_COUNT {len(mesh_paths)}",
                  f"#define TCE_POSE_FIXTURE_CASE_COUNT {len(cases)}", "#endif", ""]
        include = args.out / "pose_eval_fixtures.inc"
        include.write_text("\n".join(lines), encoding="utf-8", newline="\n")
        group_models = []
        for path in sorted({model for character in characters.values()
                            for model in character["animations"].values()}):
            skeleton = assets.skeleton(path)
            data = assets.read(path)
            group_models.append({"path": path, "bytes": len(data),
                                 "sha256": digest(data), "frames": skeleton.frames,
                                 "bones": skeleton.count, "frame_stride": skeleton.stride})
        dependencies = ([Path(oracle.__file__), Path(pose.__file__), Path(replay.__file__)]
                        if args.oracle else [Path(__file__)])
        manifest = {"schema": 1, "kind": "actual-assets-captured-five-tags-analytic-thighs-only",
                    "generator_sha256": digest(Path(__file__).read_bytes()), "case_count": len(cases),
                    "capture_sha256": capture_hashes, "oracle_inputs": {str(p): digest(p.read_bytes()) for p in dependencies},
                    "oracle_profile": [4096, True, True, True, False], "anchor_names": ANCHORS,
                    "expected_space": "model", "captured_mask": CAPTURED_MASK, "analytic_only_mask": ANALYTIC_MASK,
                    "asset_sha256": assets.hashes,
                    "asset_sources": [str(Path(archive.filename).resolve()) for archive in assets.archives],
                    "captured_character_animation_group_budget": {
                        "models": group_models,
                        "total_mdx_bytes": sum(model["bytes"] for model in group_models),
                        "total_mdx_frames": sum(model["frames"] for model in group_models),
                        "referenced_mesh_bytes": sum(map(len, mesh_bytes.values())),
                        "limit": "Captured characters' resolved animation groups; not every installed character or full-frame finiteness proof"},
                    "models": [{"path": path, "original_bytes": len(assets.read(path)),
                                "compact_bytes": len(compact[path]), "compact_sha256": digest(compact[path]),
                                "bone_count": assets.skeleton(path).count,
                                "original_frame_to_compact": remaps[path],
                                "bone_metadata": [vars(bone) for bone in assets.skeleton(path).bones]}
                               for path in model_paths],
                    "meshes": [{"path": path, "bytes": len(mesh_bytes[path]), "sha256": digest(mesh_bytes[path])}
                               for path in mesh_paths],
                    "closures": [{"indices": indices, "names": [assets.skeleton(model_paths[0]).bones[i].name for i in indices]}
                                 for indices in sorted({tuple(case["closure"]) for case in cases})],
                    "captured_profile_origin_max": max(v["origin"] for case in cases for v in case["profile_vs_capture"].values()),
                    "captured_profile_axis_max": max(v["axis"] for case in cases for v in case["profile_vs_capture"].values()),
                    "include_sha256": digest(include.read_bytes()),
                    "limits": ["Five raw captured model-space tags are real renderer evidence; two thighs are analytic-only oracle values",
                               "Compact fixtures preserve every original bone and selected frame bytes, not complete animation clips",
                               "Fixture generation proves no native C evaluator, game integration, clock, ladder/slope or damage parity"]}
        (args.out / "cases.json").write_text(json.dumps(cases, indent=2, allow_nan=False)+"\n", encoding="utf-8")
        (args.out / "manifest.json").write_text(json.dumps(manifest, indent=2, allow_nan=False)+"\n", encoding="utf-8")
        print(json.dumps({"cases": len(cases), "models": len(model_paths), "meshes": len(mesh_paths),
                          "closure_bones": [len(x["indices"]) for x in manifest["closures"]],
                          "captured_profile_origin_max": manifest["captured_profile_origin_max"],
                          "captured_profile_axis_max": manifest["captured_profile_axis_max"],
                          "include": str(include)}, indent=2))
    finally:
        assets.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
