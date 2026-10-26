"""Local, single-instance TCE2 BSP/Recast workspace. Standard library only.

No game is started, no RCON command is sent, and no navigation is installed by
opening the UI. Explicit serial jobs operate on one configured test instance.
"""

import argparse
from collections import deque
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import math
import mimetypes
import os
from pathlib import Path, PurePosixPath
import re
import secrets
import shutil
import socket
import struct
import subprocess
import sys
import threading
import time
from types import SimpleNamespace
from urllib.parse import parse_qs, quote, urlsplit
from urllib.request import urlopen
import webbrowser
import zipfile
from layout import APP_ROOT, install_import_paths, worker_path
install_import_paths()
from edit import Drafts, graph_hash, action_protection, waypoint_metadata
from watch import ProcessIdentityChanged, Windows
from settings import validate_settings, save_settings
from navbundle import NavBundles
from projects import Projects
import botmenu
import mapscript
import spawns
import quiet
from completion import navigation_readiness,spawn_bindings


APP_ID = "tce2-navstudio"
MAP_NAME = re.compile(r"[A-Za-z0-9_-]{1,64}\Z")
MAX_JSON = 64 * 1024 * 1024
STATIC_FILES = {"project-ui.js", "project-ui.css", "flags.js","menu.js", "botmenu.js", "navbundle.js", "language.js", "index.html", "app.js", "style.css", "bsp-viewer.js", "guide.html"}
ASSET_EXTENSIONS = {".tga", ".jpg", ".jpeg", ".png", ".webp", ".shader", ".bsp",
                    ".md3", ".mdc", ".mds", ".mdm", ".mdx", ".skin"}
MOVEMENT_PROFILES = {
    0: {"radius": 14, "standing_height": 70, "crouching_height": 49, "origin_above_feet": 24, "step": 18},
    1: {"radius": 16, "standing_height": 87, "crouching_height": 61, "origin_above_feet": 30, "step": 18},
    "et": {"radius": 18, "standing_height": 72, "crouching_height": 48, "origin_above_feet": 24, "step": 18},
}

# This exact Classic ET 2.60b binary has a verified 500ms RCON guard.
# Unknown/custom engines retain the conservative interval. Primary bytes:
# reconstruction/evidence/navstudio_rcon_guard_20261016.json (local evidence).
RCON_500MS_ENGINES=frozenset({'94e8aa5b52a05dbb240811616d1b8766dfbc95f02991137e3e1e54f6b27880ed'})


def rcon_min_interval(executable_sha256):
    return .55 if type(executable_sha256)is str and executable_sha256 in RCON_500MS_ENGINES else 2.1


def checked_probe_context(reply,name,gametype,profile,gravity):
    """Check the state of the same native call that performed the body probes."""
    def unique(pairs):
        result={}
        for key,value in pairs:
            if key in result:raise ValueError('duplicate native context field')
            result[key]=value
        return result
    try:
        rows=re.findall(r'(?m)^navgen probe context: ([^\r\n]+)',reply)
        if len(rows)!=1:raise ValueError('missing or duplicate native context')
        value=json.loads(rows[0],object_pairs_hook=unique)
        keys={'schema','map','gametype','g_newbbox','bbox','gravity'}
        if set(value)!=keys or type(value['schema'])is not int or value['schema']!=1:raise ValueError('native context schema')
        if value['map']!=name or type(value['gametype'])is not int or value['gametype']!=gametype:raise ValueError('native map/gametype changed')
        selected=next((key for key in(0,1)if MOVEMENT_PROFILES[key]==profile),None)
        if selected is None or type(value['g_newbbox'])is not int or value['g_newbbox']!=selected:raise ValueError('native body selection changed')
        expected=dict(half_width=profile['radius'],stand_height=profile['standing_height'],
            crouch_height=profile['crouching_height'],origin_above_feet=profile['origin_above_feet'])
        box=value['bbox']
        if not isinstance(box,dict)or set(box)!=set(expected)or any(type(box[k])not in(int,float)or box[k]!=v for k,v in expected.items()):raise ValueError('native body changed')
        if type(gravity)not in(int,float)or not math.isfinite(gravity)or type(value['gravity'])not in(int,float)or value['gravity']!=gravity:raise ValueError('native traversal gravity changed')
        return value
    except(ValueError,TypeError,KeyError,StopIteration)as exc:
        raise ProcessIdentityChanged('Native Probe gehört nicht zum erwarteten Map-/GT-/Körper-/Gravitationszustand: '+str(exc))from exc


def repair_archive_bundle_digest(binding,parts):
    """Keep every literal attempt even when geometry/probe output repeats."""
    sources={key:hashlib.sha256(binding[key]).hexdigest()for key,_ in parts}
    return hashlib.sha256(json.dumps(sources,sort_keys=True,separators=(',',':')).encode('utf8')).hexdigest()


def imported_way_ids(binary):
    """Live-added WAY nodes lack tce_node; their compact index is not an ID."""
    explicit=[]
    for node in binary:
        value=node.get("properties",{}).get("tce_node")
        if value is None:explicit.append(None);continue
        try:value=int(value)
        except (ValueError,TypeError):raise ValueError("Importierte WAY hat eine ungültige Punkt-ID.")
        if not 0<=value<2147483647:raise ValueError("Importierte WAY-Punkt-ID außerhalb des nativen Bereichs.")
        explicit.append(value)
    next_id=max([i for i in explicit if i is not None]+[int(n.get("uid",0)) for n in binary if 0<=int(n.get("uid",0))<2147483647]+[-1])+1
    used=set();result=[]
    for identity in explicit:
        if identity is None or identity in used:
            identity=next_id;next_id+=1
        if identity>=2147483647:raise ValueError("Importierte WAY-Punkt-IDs erschöpft.")
        result.append(identity);used.add(identity)
    return result


def native_body_profile(report):
    bbox = report.get("bbox", {})
    body = {"radius": bbox.get("half_width"), "standing_height": bbox.get("stand_height"),
            "crouching_height": bbox.get("crouch_height"), "origin_above_feet": bbox.get("origin_above_feet"), "step": 18}
    if body not in MOVEMENT_PROFILES.values():
        raise RuntimeError("Nativer Bericht enthaelt kein unterstuetztes vollstaendiges TCE-Koerperprofil.")
    return body


class GameConnectionError(RuntimeError):
    def __init__(self, message, sent=False):
        super().__init__(message)
        self.sent = sent


class CommandFailure(RuntimeError):
    """Nur ein nachweislich fehlgeschlagener Prozess darf erneut laufen."""
    def __init__(self, label, returncode, output):
        super().__init__(f"{label}: Rückgabecode {returncode}.\n{output[-5000:]}")
        self.returncode = returncode
        self.output = output


class IncompleteNativeFeedback(RuntimeError):
    """Vollständig zugeordnete Rückmeldung, ausdrücklich keine WAY-Freigabe."""
    def __init__(self, message, feedback):
        super().__init__(message)
        self.feedback = feedback


def incomplete_export_summary(report):
    values = report.get("native_input_coverage", {}).get("ordinary_without_exit", [])
    ids = [value for value in values if type(value) is int] if isinstance(values, list) else []
    shown = ", ".join(map(str, ids[:32])) + (", …" if len(ids) > 32 else "")
    return (f"Native Prüfung unvollständig: {len(ids)} gewöhnliche Punkte ohne Ausgang"
            + (f" (Source-IDs: {shown})" if ids else "") + ". Ältere WAY-Dateien werden nicht übernommen.")


def isolated_export_feedback(report, metadata, candidate):
    """Nur vollständige Format-4-Eingaben mit belegten Ausgangslücken reparieren."""
    def require(condition, message):
        if not condition:
            raise RuntimeError(incomplete_export_summary(report) + " Keine automatische Reparatur: " + message)
    coverage = report.get("native_input_coverage", {})
    capacity = coverage.get("capacity", {})
    count = metadata.get("counts", {}).get("nodes")
    links = metadata.get("counts", {}).get("directed_edges")
    nodes, edges = report.get("nodes"), report.get("edges")
    base = metadata.get("nodes")
    require(report.get("schema") == 1 and report.get("generation_complete") is False and
            report.get("candidate_mode") == "automatic_spatial" and
            report.get("candidate_kind") == "external_bsp_recast" and
            report.get("preserve_spatial_samples") is True and
            metadata.get("graph_policy") == "preserve_spatial_samples",
            "kein unterstützter räumlicher Zwischenbericht.")
    require(capacity.get("input_validation_complete") is True and capacity.get("input_truncated") is False,
            "Eingabeprüfung unvollständig oder gekürzt.")
    require(type(count) is int and 0 < count <= 15872 and type(links) is int and 0 <= links <= 253952 and
            isinstance(base, list) and len(base) == count and
            isinstance(metadata.get("edges"), list) and len(metadata["edges"])*2 == links and
            coverage.get("input_nodes") == count and coverage.get("initial_directed_links") == links,
            "Eingabezahlen stimmen nicht überein.")
    require(type(report.get("traces")) is int and type(report.get("trace_budget")) is int and
            0 <= report["traces"] < report["trace_budget"], "globales Prüfbudget erschöpft oder unbekannt.")
    with candidate.open(encoding="ascii") as stream:
        require(stream.readline().split() == ["TCE_NAVMESH", "4", report["map"], str(count), str(links)],
                "Kandidatkopf stimmt nicht mit dem Bericht überein.")
    require(isinstance(nodes, list) and len(nodes) <= 16384 and isinstance(edges, list) and len(edges) <= 262144 and
            all(isinstance(n, dict) and type(n.get("id")) is int for n in nodes), "ungültige Knotentabelle.")
    by_id = {n["id"]: n for n in nodes}
    require(len(by_id) == len(nodes) and coverage.get("input_ids") == list(range(count)) and
            {n["id"] for n in nodes if "native_input" in n} == set(range(count)), "Eingabe-ID-Liste unvollständig.")
    origin = native_body_profile(report)["origin_above_feet"]
    for i, node in enumerate(base):
        actual = by_id[i]
        point, raw = node.get("point"), actual.get("raw_position")
        require(node.get("id") == i and isinstance(point, list) and isinstance(raw, list) and
                len(point) == len(raw) == 3 and
                all(type(x) in (int, float) and math.isfinite(x) for x in point+raw) and
                math.dist([point[0], point[1], point[2]+origin], raw) <= .02,
                f"Quellkoordinaten von Punkt {i} stimmen nicht überein.")
        require(isinstance(actual.get("native_input"), dict) and
                type(actual["native_input"].get("floor_accepted")) is bool,
                f"Bodenprüfung von Punkt {i} fehlt.")
        require(all(type(actual["native_input"].get(key)) is int and actual["native_input"][key] >= 0
                    for key in ("initial_incoming", "initial_outgoing")), f"Verbindungsprüfung von Punkt {i} fehlt.")
    floor_count = sum(by_id[i]["native_input"]["floor_accepted"] for i in range(count))
    accepted = coverage.get("initial_accepted_links")
    rejected = coverage.get("initial_rejected_links")
    require(type(accepted) is int and accepted >= 0 and type(rejected) is int and rejected >= 0 and
            accepted+rejected == links and coverage.get("native_floor_accepted") == floor_count and
            coverage.get("native_floor_rejected") == count-floor_count and
            all(sum(by_id[i]["native_input"][key] for i in range(count)) == accepted
                for key in ("initial_incoming", "initial_outgoing")), "Boden-/Verbindungsbilanz unvollständig.")
    require(all(isinstance(e, dict) and type(e.get("accepted")) is bool and
                type(e.get("from")) is int and type(e.get("to")) is int and
                e["from"] in by_id and e["to"] in by_id for e in edges), "ungültige Verbindungstabelle.")
    outgoing = {e["from"] for e in edges if e["accepted"]}
    isolated = coverage.get("ordinary_without_exit")
    # The JSON flags include posture (CROUCH=64); native ordinary-node
    # accounting uses action flags only. Crouching is still a floor route.
    actual_isolated = {n["id"] for n in nodes if n.get("accepted") is True and n.get("nav_flags") in (0, 64)
                       and n["id"] not in outgoing}
    require(isinstance(isolated, list) and isolated and all(type(i) is int for i in isolated) and
            len(isolated) == len(set(isolated)) and set(isolated) == actual_isolated,
            "isolierte Ausgangspunkte sind nicht exakt belegt.")
    return {"status": "incomplete_export_feedback_only", "ordinary_without_exit": isolated,
            "isolated_node_count": len(isolated), "input_nodes": count, "input_directed_links": links,
            "generation_complete": False, "acceptance_certificate": False, "installed": False}


def recast_memory_failure(error):
    if not isinstance(error, CommandFailure) or error.returncode != 1:
        return False
    # Keine Wiederholung bei beliebigen Geometriefehlern, Timeouts oder Crashes.
    return any(re.fullmatch(r"rc[A-Za-z0-9_]+: Out of memory\b.*", line.strip()) or
               line.strip() in ("Recast allocation limit exceeded",
                                "Recast allocation limit exceeded during contour retry")
               for line in error.output.splitlines())


def recast_height_domain(geometry, name, collision):
    """Nur die explizite, zum OBJ passende Arbeitsdomäne des Exporters verwenden."""
    domain = geometry.get("vertical_raster_domain", {})
    output = geometry.get("output", {})
    numeric = lambda value: type(value) in (int, float) and math.isfinite(value)
    basis = domain.get("basis", [])
    if (geometry.get("map") != name or not output.get("obj") or
            Path(output["obj"]).resolve() != collision.resolve() or output.get("obj_sha256") != digest(collision) or
            domain.get("coordinate_system") != "BSP_Z_equals_OBJ_Y" or
            domain.get("rule") != "playable_model_and_spawn_bounds_plus_hull_step_padding" or
            domain.get("collision_geometry_modified") is not False or domain.get("add_cut_caps") is not False or
            domain.get("padding") != 128 or not isinstance(basis, list) or not basis or
            not all(isinstance(item, dict) and item.get("kind") in ("world_model", "active_inline_model", "spawn") and
                    numeric(item.get("minimum")) and numeric(item.get("maximum")) and
                    item["minimum"] <= item["maximum"] for item in basis) or
            not any(item.get("kind") == "world_model" and item.get("model") == 0 for item in basis) or
            not numeric(domain.get("minimum")) or not numeric(domain.get("maximum")) or
            domain["minimum"] != min(item["minimum"] for item in basis) - 128 or
            domain["maximum"] != max(item["maximum"] for item in basis) + 128):
        raise RuntimeError("Geometriemanifest enthält keine verifizierbare explizite Recast-Höhendomäne.")
    return {"minimum": domain["minimum"], "maximum": domain["maximum"]}


def valid_map(value):
    if not isinstance(value, str) or not MAP_NAME.fullmatch(value):
        raise ValueError("Ungültiger Mapname.")
    return value


def asset_name(value):
    value = str(value).replace("\\", "/")
    if not value or len(value) > 256 or value.startswith("/") or ":" in value or "\x00" in value:
        raise ValueError("Ungültiger Assetpfad.")
    parts = PurePosixPath(value).parts
    if any(p in (".", "..") for p in parts) or "//" in value:
        raise ValueError("Ungültiger Assetpfad.")
    normalized = "/".join(parts).lower()
    if Path(normalized).suffix not in ASSET_EXTENSIONS:
        raise ValueError("Nicht unterstützter Assettyp.")
    return normalized


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def read_json(path, missing=None):
    path = Path(path)
    if not path.is_file():
        return missing
    if path.stat().st_size > MAX_JSON:
        raise ValueError("JSON-Datei überschreitet das Größenlimit.")
    return json.loads(path.read_text(encoding="utf-8-sig"))


def discover_workspace(value=None):
    if value:
        path = Path(value).expanduser().resolve()
        if not (path / "tce2").is_dir() or not (path / "etmain").is_dir():
            raise ValueError("Workspace benötigt die vorhandenen Ordner tce2 und etmain.")
        return path
    if getattr(sys, "frozen", False):
        executable_folder = Path(sys.executable).resolve().parent
        if executable_folder.name.lower() == "navgen-overkill" and executable_folder.parent.name.lower() == "build":
            project = executable_folder.parent.parent
            if all((project / part).is_dir() for part in ("navstudio", "tce2", "etmain")):
                # A double-click sets cwd to the combined game/app folder.
                # Keep the project identity used by Start.cmd and the running
                # Studio, including its separate production-data target.
                return project
        if all((executable_folder / part).is_dir() for part in ("tce2", "etmain")):
            # A relocated standalone installation owns its own workspace;
            # an unrelated shell working directory must not override it.
            return executable_folder
    starts = [Path.cwd(), Path(sys.executable).parent, Path(__file__).parent]
    for start in starts:
        for path in (start, *start.parents):
            if (path / "tce2").is_dir() and (path / "etmain").is_dir():
                return path.resolve()
    raise ValueError("TCE2-Workspace nicht gefunden. Mit --workspace PFAD starten.")


class Assets:
    """Read the actual local VFS assets in place; never unpack whole PK3s."""
    def __init__(self, roots):
        self.roots = [Path(p).resolve() for p in roots if Path(p).is_dir()]
        self.entries, self.maps, self.errors = {}, {}, []
        self.lock = threading.RLock()
        self.scan_time = 0
        self.refresh()

    def refresh(self, force=False):
        with self.lock:
            if not force and time.monotonic() - self.scan_time < 30:
                return
            entries, maps, errors = {}, {}, []
            for root in self.roots:
                for archive in sorted(root.glob("*.pk3"), key=lambda p: p.name.lower()):
                    try:
                        with zipfile.ZipFile(archive) as zipped:
                            for info in zipped.infolist():
                                if info.is_dir():
                                    continue
                                try:
                                    name = asset_name(info.filename)
                                except ValueError:
                                    continue
                                if info.file_size > 256 * 1024 * 1024:
                                    continue
                                entries[name] = {"path": archive, "member": info.filename,
                                                 "size": info.file_size, "name": name}
                    except (OSError, zipfile.BadZipFile) as exc:
                        errors.append(f"{archive.name}: {exc}")
                # Loose assets override PK3 members within this search root.
                for directory in ("maps", "textures", "scripts", "models", "gfx", "env"):
                    subdir = root / directory
                    if not subdir.is_dir():
                        continue
                    for path in subdir.rglob("*"):
                        if not path.is_file():
                            continue
                        try:
                            resolved = path.resolve()
                            resolved.relative_to(root)
                            name = asset_name(path.relative_to(root).as_posix())
                            entries[name] = {"path": resolved, "member": None,
                                             "size": resolved.stat().st_size, "name": name}
                        except (OSError, ValueError):
                            continue
            for name, record in entries.items():
                match = re.fullmatch(r"maps/([A-Za-z0-9_-]{1,64})\.bsp", name)
                if match:
                    maps[match[1]] = record
            self.entries, self.maps, self.errors = entries, maps, errors
            self.scan_time = time.monotonic()

    def lookup(self, name):
        name = asset_name(name)
        with self.lock:
            if name not in self.entries:
                raise FileNotFoundError(name)
            return dict(self.entries[name])

    def read(self, name, maximum=64 * 1024 * 1024):
        record = self.lookup(name)
        if record["size"] > maximum:
            raise ValueError("Asset überschreitet das Größenlimit.")
        if record["member"] is None:
            with record["path"].open("rb") as stream:
                data = stream.read(maximum + 1)
        else:
            with zipfile.ZipFile(record["path"]) as zipped:
                with zipped.open(record["member"]) as stream:
                    data = stream.read(maximum + 1)
        if len(data) > maximum:
            raise ValueError("Asset überschreitet das Größenlimit.")
        return data


class Studio:
    ACTIONS = {"capture_navigation", "apply_spawns", "generate_goals", "apply_goals", "apply_draft", "start_test", "load_map", "stop_test", "snapshot", "generate", "rebuild", "repair", "repair_check", "native_check", "inspect",
               "install_test", "install_production", "mode_tce2", "mode_et"}

    def __init__(self, args):
        self.workspace = discover_workspace(args.workspace)
        # A source workspace may itself contain a production ET.exe. Prefer its
        # explicit test installation; only a standalone combined folder uses itself.
        known_test = self.workspace / "build" / "navgen-overkill"
        default_test = known_test if known_test.is_dir() else self.workspace
        self.settings_file = Path(getattr(args, "settings_file", None) or
                                  ((Path(sys.executable).parent if getattr(sys, "frozen", False) else default_test) /
                                   "navstudio-settings.json")).resolve()
        saved = read_json(self.settings_file, {})
        self.test = Path(saved.get("instance_path", str(default_test))).resolve()
        self.game_executable = Path(saved.get("game_executable", str(self.test / "ET.exe"))).resolve()
        self.maps_dirs = saved.get("maps_dirs", [str(p.resolve()) for p in (args.maps_dir or [])])
        self.game_mode = saved.get("game_mode", "tce2")
        if self.game_mode not in ("tce2", "et"): self.game_mode = "tce2"
        self.mod = self.test / ("tce2" if self.game_mode == "tce2" else "omnibot")
        self.preview = self.test / ("navmesh-preview" if self.game_mode == "tce2" else "navmesh-preview-et")
        self.runtime = self.mod / "navgen"
        self.work = self.preview / "navstudio"
        self.work.mkdir(parents=True, exist_ok=True)
        self.drafts = Drafts(self.work / "drafts")
        self.resources = APP_ROOT
        self.tools = self.resources
        self.args = args
        self.token = secrets.token_urlsafe(32)
        self.selected_map = ""
        self.jobs = deque(maxlen=20)
        self.lock = threading.RLock()
        self.executor = ThreadPoolExecutor(max_workers=1, thread_name_prefix="navstudio-job")
        self.busy = False
        self.rcon_last = 0.
        self.transport_lock = threading.RLock()
        self.transport_last = 0.
        self.native_cached = {}
        self.hash_cached = {}
        self.game_state_cache = None
        self.game_state_lock = threading.Lock()
        self.process_api = None
        self.assets = Assets([self.workspace / "etmain",
                              self.test / "etmain", self.workspace / self.mod.name, self.mod, *self.maps_dirs])
        choices = sorted(n for n in self.assets.maps if self.map_in_mode(n))
        if self.selected_map not in choices:
            self.selected_map = ""
        self.recast = self.find_recast()
        self.projects = self.make_projects()
        self.active_project = None
        self.project_native = None

    def make_projects(self):
        return Projects(self.test / "projects", self.test / "etmain",
                        [self.workspace / "etmain" / "all_maps", self.test / "etmain" / "all_maps"])

    def spawn_layer(self, name):
        name = self.require_map(name)
        active = getattr(self, "active_project", None)
        original = spawns.source_spawns(self.assets, name)
        if not active:
            return dict(map=name, revision=0, enabled=False, nodes=original, generated=[], project_required=True)
        doc = spawns.SpawnStore(active["paths"]["spawns"], name).read()
        native = self.native(name) or {}
        origin = native.get("bbox",{}).get("origin_above_feet",30)
        return dict(map=name, revision=doc["revision"], enabled=doc["enabled"], nodes=original + doc["nodes"], generated=doc["nodes"],origin_above_feet=origin)

    def edit_spawns(self, payload):
        name=self.require_map(payload.get("map"))
        with self.lock:
            if self.busy: raise RuntimeError("Laufenden Auftrag abwarten.")
            if not self.active_project: raise RuntimeError("Zuerst ein Kartenprojekt öffnen.")
            operation=payload.get("op")
            if self.game_mode=='tce2':mapscript.require_gametype(self.assets,name,7,self.script_directory())
            if operation == "apply": return {"job": self.submit(dict(action="apply_spawns",map=name,gametype=7))}
            report=None
            if operation == "generate":
                proof=self.proof_status(name)
                if not proof.get("valid"): raise RuntimeError("Aktuelle Navigation prüfen, bevor Spawns erzeugt werden.")
                report=self.native(name)
            spawns.SpawnStore(self.active_project["paths"]["spawns"],name).change(payload,report)
            return self.spawn_layer(name)

    def action_apply_spawns(self, job):
        name=job["map"]
        if self.game_mode!="tce2" or not self.active_project: raise RuntimeError("TCE2-Projekt erforderlich.")
        mapscript.require_gametype(self.assets,name,7,self.script_directory())
        self.action_load_map(job)
        store=spawns.SpawnStore(self.active_project["paths"]["spawns"],name);doc=store.read()
        if not doc["nodes"]: raise ValueError("Keine zusätzlichen Spawns vorhanden.")
        for index,node in enumerate(doc["nodes"]):
            self.update(job,progress=f"Spawn {index+1}/{len(doc['nodes'])} im Spiel prüfen")
            reply=self.rcon("tce_navgen_spawncheck "+" ".join(format(v,".4f") for v in node["position"]))
            match=re.search(r'navgen spawncheck: (\{[^\n]+\})',reply)
            evidence=json.loads(match[1]) if match else {}
            if evidence.get("accepted") is not True or evidence.get("map")!=name:
                raise RuntimeError("Spawn "+node["id"]+" nicht übernommen: "+str(evidence.get("reason") or reply))
            node["status"]="native_checked";node["native_check"]=evidence
        source=mapscript.read(self.assets,name,gametype=7,directory=self.script_directory(live=True))
        if not source["exists"]: raise RuntimeError("Originales Mapscript fehlt; Spawn-Override kann nicht abgeleitet werden.")
        directory=Path(self.active_project["paths"]["root"])/"mapscripts";directory.mkdir(exist_ok=True)
        path=directory/(name+"_gt7.script")
        previous=path.read_bytes() if path.exists() else mapscript.read_raw(
            self.assets,name,7,self.script_directory(live=True),expected=source['resolution'])
        content=mapscript.render_gt7(previous,doc)
        if path.exists():
            backup=directory/"backups";backup.mkdir(exist_ok=True);shutil.copy2(path,backup/(path.name+"."+str(time.time_ns())))
        pending=path.with_suffix(".tmp");pending.write_bytes(content);pending.replace(path)
        runtime=self.mod/"mapscripts";runtime.mkdir(exist_ok=True)
        target=runtime/path.name
        if target.exists() and target.read_bytes()!=content:
            backup=runtime/".navstudio-backups";backup.mkdir(exist_ok=True);shutil.copy2(target,backup/(target.name+"."+str(time.time_ns())))
        shutil.copy2(path,target);store.write(doc)
        self.rcon("set g_mapScriptDirectory mapscripts")
        self.rcon("devmap "+name)
        self.require_loaded(name)
        return dict(path=str(path),installed=str(target),count=len(doc["nodes"]),revision=doc["revision"],gametype=7,
                    note="Spawns geprüft und GT7-Mapscript übernommen.")

    @staticmethod
    def navigation_overview_draw_script(token, restore=False):
        """Own only transient per-goal draw flags, including mixed selections."""
        if not re.fullmatch(r"[a-f0-9]{16}",token): raise ValueError("Invalid overview token")
        prefix="bot script_run global NavStudioOverviewDrawState;"
        if restore:
            return (prefix+"if(NavStudioOverviewDrawState!=null && NavStudioOverviewDrawState.Token=='"+token+"'){"
                    "local goals={};GetGoals(goals);foreach(id and g in goals){"
                    "local saved=NavStudioOverviewDrawState.Goals[g.SerialNum];if(saved!=null){"
                    "g.RenderGoal=saved.RenderGoal;g.RenderRoutes=saved.RenderRoutes;}}"
                    "NavStudioOverviewDrawState=null;print('NAVSTUDIO_OVERVIEW_DRAW_RESTORED:"+token+"');}"
                    "else{print('NAVSTUDIO_OVERVIEW_DRAW_NOT_OWNED');}")
        return (prefix+"if(NavStudioOverviewDrawState!=null){print('NAVSTUDIO_OVERVIEW_DRAW_BUSY');}else{"
                "NavStudioOverviewDrawState={Token='"+token+"',Goals={}};local goals={};GetGoals(goals);"
                "foreach(id and g in goals){NavStudioOverviewDrawState.Goals[g.SerialNum]="
                "{RenderGoal=g.RenderGoal,RenderRoutes=g.RenderRoutes};g.RenderGoal=false;g.RenderRoutes=false;}"
                "print('NAVSTUDIO_OVERVIEW_DRAW_OFF:"+token+"');}")

    @contextmanager
    def navigation_overview(self,job):
        """Hold TCE round resets only while photographing this owned game.

        Combat, bots, user input and physical entities keep running. A hold
        already owned by another authoring operation is never renewed/released.
        """
        process=self.own_test_process(required=True)
        api=getattr(self,"process_api",None) or Windows()
        identity=list(api.identity(process["pid"]) or ())
        if not identity or identity[0]!=os.path.normcase(str(self.game_executable.resolve())):
            raise ProcessIdentityChanged("Übersicht verweigert: Testprozessidentität ist ungültig.")
        state=dict(model="navigation_overview_round_hold_v1",pid=process["pid"],identity=identity,
                   map=job["map"],gametype=int(job["gametype"]),token=secrets.token_hex(8),
                   authoring_prior_active=None,authoring_acquired=False,draw_restore_pending=False,
                   bot_population_changed=False,user_input_locked=False)
        job["overview_capture_state"]=state
        def guard():
            current=self.own_test_process(required=True)
            if current["pid"]!=state["pid"] or list(api.identity(current["pid"]) or ())!=identity:
                raise ProcessIdentityChanged("Testprozess wurde ersetzt; Übersicht verändert keinen anderen Prozess.")
            self.require_loaded(state["map"])
            if self.live_gametype()!=state["gametype"]:
                raise RuntimeError("Gametype während der Übersicht geändert; Aufnahme nicht bestätigt.")
        failure=None
        try:
            guard()
            if getattr(self,"game_mode","tce2")=="tce2":
                reply=self.rcon("tce_navgen_authoring",read_only=True)
                state["authoring_query_reply"]=reply
                active=re.findall(r'^NAVSTUDIO_AUTHORING active=([01])\s*$',reply,re.M)
                if len(active)!=1: raise RuntimeError("Vorheriger TCE2-Authoringstatus nicht eindeutig bestätigt; keine Übersicht.")
                state["authoring_prior_active"]=active[0]=="1"
                if not state["authoring_prior_active"]:
                    # Record before dispatch: missing UDP acknowledgement can
                    # still mean that this exact VM acquired the hold.
                    state["authoring_acquired"]=True
                    reply=self.rcon_with_followup("tce_navgen_authoring on")
                    state["authoring_begin_reply"]=reply
                    if "NAVSTUDIO_AUTHORING_ON" not in reply:
                        raise RuntimeError("TCE2-Rundenhalt für Übersicht nicht bestätigt.")
            state["draw_restore_pending"]=True
            reply=self.run_quiet_script(self.navigation_overview_draw_script(state["token"]),state,"overview-begin")
            state["draw_begin_reply"]=reply
            if "NAVSTUDIO_OVERVIEW_DRAW_BUSY" in reply:
                state["draw_restore_pending"]=False
                raise RuntimeError("Eine frühere Übersicht besitzt noch die Goal-Anzeige; keine neue Aufnahme.")
            if "NAVSTUDIO_OVERVIEW_DRAW_OFF:"+state["token"] not in reply:
                raise RuntimeError("Temporäre Goal-Anzeige für Übersicht nicht bestätigt.")
            yield state,guard
        except Exception as exc:
            failure=exc
            raise
        finally:
            errors=[]
            if state["draw_restore_pending"]:
                try:
                    guard()
                    reply=self.run_quiet_script(self.navigation_overview_draw_script(state["token"],True),state,"overview-restore")
                    state["draw_restore_reply"]=reply
                    if "NAVSTUDIO_OVERVIEW_DRAW_RESTORED:"+state["token"] not in reply:
                        raise RuntimeError("Goal-Anzeige nach Übersicht nicht bestätigt wiederhergestellt.")
                    state["draw_restore_pending"]=False
                    for stage in ("overview-begin","overview-restore"):
                        ((getattr(self,"test",None) or self.work)/"navgen"/("navstudio-quiet-"+state["token"]+"-"+stage+".gm")).unlink(missing_ok=True)
                except Exception as exc: errors.append(str(exc))
            if state["authoring_acquired"]:
                try:
                    guard()
                    reply=self.rcon_with_followup("tce_navgen_authoring off")
                    state["authoring_release_reply"]=reply
                    if "NAVSTUDIO_AUTHORING_OFF" not in reply:
                        raise RuntimeError("TCE2-Rundenhalt nach Übersicht nicht bestätigt freigegeben (automatisches Ende nach 600 Sekunden).")
                    state["authoring_acquired"]=False
                except Exception as exc: errors.append(str(exc))
            if errors:
                raise RuntimeError((str(failure)+"; " if failure else "")+"; ".join(errors)) from failure

    def action_capture_navigation(self, job):
        name=job["map"];self.require_loaded(name)
        if not self.proof_status(name).get("valid"):
            raise RuntimeError("Aktuelles Wegnetz vor den Aufnahmen prüfen.")
        report=self.native(name)
        if not report or not report.get("generation_complete"): raise RuntimeError("Aktuelle geprüfte Navigation erforderlich.")
        points=[n for n in report["nodes"] if n.get("accepted") and n.get("spawn_reachable") and
                not int(n.get("nav_flags",0))&((1<<9)|(1<<27))]
        if not points: raise RuntimeError("Keine erreichbaren Kamerapunkte.")
        chosen=[min(points,key=lambda n:n["id"])]
        for _ in range(3):
            others=[n for n in points if n not in chosen]
            if not others: break
            chosen.append(max(others,key=lambda n:min(sum((a-b)**2 for a,b in zip(n["position"],p["position"])) for p in chosen)))
        def observer(expected=None,slot=None):
            # The movement acknowledgement only confirms a ps.origin write.
            # A followed player's state can replace it before rendering, so
            # inspect the named spectator independently across real RCON calls.
            reply=self.rcon("navprobe")
            state=re.search(r'^NAVSTATE \d+ GT (\d+)\s*$',reply,re.M)
            rows=[]
            for row in re.finditer(r'^NAVPOS (\d+) (\S+) (\S+) (\S+) -?\d+ (\d+) ([^\r\n]+)',reply,re.M):
                if row[6].strip()!="NavStudioObserver": continue
                try: position=[float(row[j]) for j in (2,3,4)]
                except ValueError: position=[]
                rows.append(dict(slot=int(row[1]),position=position,team=int(row[5]),name=row[6].strip()))
            if not state or int(state[1])!=int(job["gametype"]) or len(rows)!=1 or rows[0]["team"]!=3 or \
                    len(rows[0]["position"])!=3 or not all(math.isfinite(v) for v in rows[0]["position"]) or \
                    (slot is not None and rows[0]["slot"]!=slot):
                raise RuntimeError("Eigener Zuschauer NavStudioObserver nicht eindeutig bestätigt; keine Kameraaufnahme übernommen.")
            current=rows[0]
            if expected is not None:
                distance=math.dist(current["position"],expected)
                if distance>2:
                    raise RuntimeError(f"Zuschauerkamera folgt noch einem Spieler oder wurde versetzt ({distance:.3f} Einheiten); Aufnahme nicht bestätigt.")
                current["distance_from_checked_camera"]=distance
            return dict(observer=current,reply=reply)
        with self.navigation_overview(job) as (state,guard):
            observer_slot=observer()["observer"]["slot"]
            self.rcon("bot waypoint_view on")
            results=[]
            stamp=str(time.time_ns());prefix=(self.active_project["id"][:8] if self.active_project else "map")
            for i,node in enumerate(chosen):
                guard()
                self.update(job,progress=f"Sichtbare Aufnahme {i+1}/{len(chosen)}")
                # Normal no-argument follow exits spectator following. Never alter
                # a bot or lock the user's input to obtain an overview.
                self.rcon("follow")
                x,y,z=node["position"]
                checked=self.rcon("tce_navgen_camera "+" ".join(format(v,".4f") for v in (x,y,z,220)))
                match=re.search(r'navgen camera: (\{[^\n]+\})',checked)
                view=json.loads(match[1]) if match else {}
                if not view.get("accepted") or view.get("map")!=name:
                    raise RuntimeError("Kamerapunkt nicht kollisionsfrei bestätigt: "+checked)
                # Look into a proven floor route instead of into an arbitrary wall.
                # The near-vertical angle keeps the inspected waypoint in frame.
                targets={e["to"] for e in report["edges"] if e.get("accepted") and e["from"]==node["id"]}
                neighbours=[p for p in points if p["id"] in targets and
                            (p["position"][0]-x)**2+(p["position"][1]-y)**2>1]
                target=min(neighbours,key=lambda p:sum((a-b)**2 for a,b in zip(p["position"],node["position"]))) if neighbours else node
                yaw=math.degrees(math.atan2(target["position"][1]-y,target["position"][0]-x))
                camera=[*view["position"],80,yaw]
                reply=self.rcon("navcamera "+" ".join(format(v,".3f") for v in camera))
                if "local spectator moved" not in reply: raise RuntimeError("Zuschauerkamera nicht bestätigt: "+reply)
                before=observer(camera[:3],observer_slot)
                stem=f"navstudio_{prefix}_gt{job['gametype']}_{stamp}_{i+1}"
                screenshot=self.rcon("screenshotJPEG "+stem)
                path=self.mod/"screenshots"/(stem+".jpg")
                if not path.is_file() or "Wrote screenshots/" not in screenshot:
                    raise RuntimeError("Spielaufnahme nicht bestätigt: "+screenshot)
                after=observer(camera[:3],observer_slot)
                results.append(dict(path=str(path),camera=camera,node=node["id"],map=name,gametype=job["gametype"],camera_native_check=view,
                                    observer_before_screenshot=before,observer_after_screenshot=after))
            return dict(screenshots=results,overview_capture_state=state,
                        note="Echte Spielbilder; Goal-/Routentexte für diese Übersicht vorübergehend ausgeblendet und wiederhergestellt; vor Bewertung ansehen.")

    def objective_anchor_visit(self,job,gametype,callback):
        """Each restart gets its own quiet token; never restore across a PID."""
        if job.get('quiet_generation'):self.restore_generation_bots(job)
        self.update(job,gametype=gametype,progress=f'Gemeinsame Objectiveanker GT{gametype} nativ prüfen')
        self.action_load_map(job)
        self.quiet_generation(job)
        try:
            snapshot,evidence=(self.objective_snapshot(job)if gametype in(2,5)else self.objective_snapshot(job,allowed_gametypes=(7,)))
            return callback(snapshot,evidence)
        finally:
            if job.get('quiet_generation'):self.restore_generation_bots(job)

    def objective_anchor_scripts(self,job,require_loaded=False):
        """Actual test-VFS scripts, including g_mapScriptDirectory overrides."""
        import objectiveanchors
        reply=self.rcon('g_mapScriptDirectory',read_only=True)
        directory=mapscript.cvar_value(reply,'g_mapScriptDirectory')
        value=objectiveanchors.map_script_binding((self.test/'etmain',self.mod),job['map'],int(job['gametype']),directory)
        if require_loaded and value['selected']:
            console=self.mod/'etconsole.log';text=console.read_text(encoding='latin1')if console.is_file()else''
            loaded=re.findall(r'^Loading map script:\s*([^\r\n]+)',text,re.M)
            if not loaded or loaded[-1].strip().lower()!=value['selected']['relative'].lower():
                raise RuntimeError('Geladenes Mapscript entspricht nicht der gebundenen GT-/Override-Auflösung.')
            value['loaded_relative']=loaded[-1].strip()
        return value

    def objective_anchor_worker_binding(self):
        resources=getattr(self,'resources',APP_ROOT);tools=resources
        paths={name:resources/name for name in('server.py','objectiveanchors.py','goalgen.py','mapscript.py','edit.py','repair.py','pathrepair.py')}
        paths['navmesh/objective_contact.py']=tools/'navmesh/objective_contact.py'
        value=dict(files={name:dict(path=str(path),sha256=self.file_hash(path))for name,path in paths.items()},
            application=dict(kind='frozen'if getattr(sys,'frozen',False)else'source',path=str(Path(sys.executable).resolve()),
                sha256=self.file_hash(Path(sys.executable))if getattr(sys,'frozen',False)else None))
        if any(not re.fullmatch('[a-f0-9]{64}',item.get('sha256')or'')for item in value['files'].values()):
            raise RuntimeError('Objectiveanker-Programmquellen für die Provenienzbindung fehlen.')
        if value['application']['kind']=='frozen'and not re.fullmatch('[a-f0-9]{64}',value['application']['sha256']or''):
            raise RuntimeError('Objectiveanker-App für die Provenienzbindung fehlt.')
        return value

    def reconcile_objective_anchors(self,job,gametypes=None,capture_goals=False):
        """One shared, bounded Generate/Goals/Repair phase; Check never calls it."""
        if getattr(self,'game_mode','tce2')!='tce2':return None
        import objectiveanchors
        import goalgen
        from repair import parse_native_probe
        name=self.require_map(job['map']);selected=int(job['gametype'])
        available=self.available_game_types(name,live=True)['available']
        gametypes=tuple(gametypes or (tuple(g for g in(2,5)if g in available)if selected in(2,5)else(7,)))
        if any(g not in available for g in gametypes):raise ValueError('Objectiveprüfung für nicht vorhandenen Gametype abgelehnt.')
        if selected not in gametypes:raise RuntimeError('Ausgewählter GT gehört nicht zum Objectiveanker-Prüfumfang.')
        pointer=self.work/(name+'.objective-anchors.json')
        # A failed new attempt must not leave a previous success as "current".
        if pointer.exists():pointer.unlink()
        proof=self.proof_status(name);baseline=self.native(name)
        if not proof.get('valid')or baseline.get('generation_complete')is not True:
            raise RuntimeError('Gemeinsame Objectiveanker benötigen die aktuelle vollständige native Prüfung.')
        source_report=self.file_hash(self.native_path(name));project_id=self.active_project['id'];doc=self.drafts.get(name)
        worker_binding=self.objective_anchor_worker_binding()
        anchors=[];archive_binding={}
        if doc and doc['base'].get('generation_diagnostics'):
            manifest_path=self.work/(name+'.generation-diagnostics.json');raw=manifest_path.read_bytes();manifest=json.loads(raw)
            scanner=Path(manifest['native_report']);candidate=Path(manifest['candidate_path']);root=(self.work/'generation-scans').resolve()
            if any(not path.resolve().is_relative_to(root)for path in(scanner,candidate,candidate.with_suffix('.json'))):
                raise RuntimeError('Objectiveanker-Scannerarchiv liegt außerhalb dieses Projekts.')
            bundle=dict(manifest=raw,scanner=scanner.read_bytes(),candidate=candidate.read_bytes(),metadata=candidate.with_suffix('.json').read_bytes())
            anchors=objectiveanchors.archive_anchors(doc,baseline,bundle)
            archive_binding={k:dict(path=str(path),sha256=objectiveanchors.sha(bundle[k]))for k,path in
                (('manifest',manifest_path),('scanner',scanner),('candidate',candidate),('metadata',candidate.with_suffix('.json')))}
        elif doc and doc['base'].get('native_report')and baseline.get('candidate_mode')=='manual_draft' and not doc['base'].get('imported_origin_offset'):
            # Older imported projects have no generated-anchor provenance. They
            # must not be guessed into an automatic coordinate edit.
            if (self.work/(name+'.generation-diagnostics.json')).exists():
                raise RuntimeError('Generierter Entwurf hat keinen gebundenen Objectiveanker-Scannerbeleg.')
        if not anchors and not capture_goals:return None
        if not self.own_test_process(require_port=False):self.action_load_map(job)
        reconciler=(objectiveanchors.Reconciler(doc,baseline,anchors,gametypes,
            {g:goalgen.objectives(self.assets,name,g)if g in(2,5)else[]for g in gametypes})if anchors else None)
        initial_graph=graph_hash(doc)if doc else None;initial_inputs=proof['inputs'];snapshots={};evidences={};failure=None;certificate=None;failure_evidence=None
        source_documents={}
        if reconciler:
            directory=self.work/'objective-anchors';inputs_directory=directory/'inputs';inputs_directory.mkdir(parents=True,exist_ok=True)
            candidate=self.candidate(name)
            sources=dict(native=self.native_path(name).read_bytes(),draft=json.dumps(doc,sort_keys=True,ensure_ascii=False).encode('utf8'),
                candidate=candidate.read_bytes(),metadata=candidate.with_suffix('.json').read_bytes(),
                proof=(self.work/(name+'.native-proof.json')).read_bytes())
            for key,raw in sources.items():
                digest_value=objectiveanchors.sha(raw);path=inputs_directory/(digest_value+('.txt'if key=='candidate'else'.json'))
                if path.exists()and path.read_bytes()!=raw:raise RuntimeError('Objectiveanker-Quellarchiv enthält andere Bytes.')
                if not path.exists():
                    with path.open('xb')as stream:stream.write(raw)
                source_documents[key]=dict(path=str(path),sha256=digest_value)
        def unchanged():
            current=self.proof_status(name)
            if (not self.active_project or self.active_project['id']!=project_id or not current.get('valid')or
                current.get('inputs')!=initial_inputs or self.file_hash(self.native_path(name))!=source_report or
                (graph_hash(self.drafts.get(name))if doc else None)!=initial_graph):
                raise RuntimeError('Projekt/Wegnetz während des Objectiveanker-Abgleichs verändert; keine Übernahme.')
            if self.objective_anchor_worker_binding()!=worker_binding:raise RuntimeError('Objectiveanker-Programmquellen während der Prüfung verändert.')
        def visit(gt,final=False,proposal=None,moves=None):
            # Snapshot current script bytes before the restart, using the
            # existing instance's configured override directory. Capture
            # actual loaded GT-path and identical bytes again before/after.
            previous_gt=job['gametype'];self.update(job,gametype=gt)
            try:scripts_before=self.objective_anchor_scripts(job)
            finally:self.update(job,gametype=previous_gt)
            def capture(snapshot,evidence):
                unchanged();process=self.own_test_process(required=True);api=getattr(self,'process_api',None)or Windows()
                identity=list(api.identity(process['pid'])or());profile=self.require_bbox(job)
                if reconciler and profile!=native_body_profile(baseline):raise RuntimeError('Körperprofil während Objectiveankerprüfung verändert.')
                before=len(reconciler.queries)if reconciler else 0
                scripts_loaded=self.objective_anchor_scripts(job,require_loaded=True)
                if {k:v for k,v in scripts_loaded.items()if k!='loaded_relative'}!=scripts_before:
                    raise RuntimeError('Mapscript/Override wurde beim GT-Start geändert; keine Objectiveanker-Übernahme.')
                def probe(start,end):
                    unchanged()
                    api.probe_owner(process['pid'],identity,expected_exe=self.game_executable,port=self.args.rcon_port)
                    reply=self.rcon('tce_navgen_probe feet '+' '.join('%.5f'%v for v in list(start)+list(end)),read_only=True)
                    context=checked_probe_context(reply,name,gt,profile,baseline.get('traversal_physics',{}).get('gravity'))
                    parsed=parse_native_probe(reply,start,end,profile['origin_above_feet'])
                    parsed.update(raw_reply=reply,context=context)
                    self.update(job,progress=f'Gemeinsame Objectiveanker GT{gt}: {len(reconciler.queries)+1} native Proben (Limit {reconciler.limit})')
                    return parsed
                if reconciler:
                    if final:reconciler.verify_joint(gt,snapshot,evidence,probe,proposal,moves)
                    else:reconciler.observe(gt,snapshot,evidence,probe)
                # A fresh snapshot after probing binds stable model state. It
                # never overwrites the project's original Recast geometry.
                after,after_evidence=(self.objective_snapshot(job)if gt in(2,5)else self.objective_snapshot(job,allowed_gametypes=(7,)))
                if objectiveanchors.collision_sha(after)!=objectiveanchors.collision_sha(snapshot):
                    raise RuntimeError('Mapmodelle während der Objectiveankerproben verändert; keine Übernahme.')
                scripts_after=self.objective_anchor_scripts(job,require_loaded=True)
                if scripts_after!=scripts_loaded:raise RuntimeError('Mapscript/Override während Objectiveankerproben verändert; keine Übernahme.')
                if reconciler:reconciler.visits[-1].update(verification_snapshot=after_evidence,fresh_queries=len(reconciler.queries)-before,
                    process_id=process['pid'],process_identity=identity,mapscript_before=scripts_before,mapscript_loaded=scripts_loaded,mapscript_after=scripts_after)
                snapshots[gt]=after;evidences[gt]=after_evidence
                unchanged()
            return self.objective_anchor_visit(job,gt,capture)
        try:
            order=(selected,)+tuple(g for g in gametypes if g!=selected)
            for gt in order:visit(gt)
            # Proposals discovered only in the second state are checked on the
            # already required return visit, without restarting per candidate.
            if len(order)>1 and reconciler:visit(selected)
            if reconciler:
                proposal,moves=reconciler.proposal()
                if moves:
                    # All moved-neighbour pairs use FINAL coordinates in every
                    # GT. Independent old-neighbour probes cannot stand in.
                    for gt in order:visit(gt,True,proposal,moves)
                    unchanged()
                    # Native import runs in the source collision GT, whose
                    # geometry/provenance remains the Recast reference.
                    source_gt=read_json(self.snapshot_path(name),{}).get('gametype')
                    if source_gt not in gametypes:raise RuntimeError('Native Geometrie stammt aus einem anderen GT-Prüfumfang.')
                    if job['gametype']!=source_gt:
                        if job.get('quiet_generation'):self.restore_generation_bots(job)
                        self.update(job,gametype=source_gt);self.action_load_map(job)
                    self.quiet_generation(job)
                    current=self.drafts.apply_repair(name,doc['revision'],self.draft_environment(name),proposal)
                    self.prepare_draft(name);self.action_native_check(job)
                    final=self.native(name);rows={n['id']:n for n in final['nodes']};original={n['id']:n for n in baseline['nodes']}
                    for node in current['nodes']:
                        row=rows.get(node['id'],{});feet=row.get('position',[])[:]
                        if len(feet)==3:feet[2]-=reconciler.origin
                        if (row.get('accepted')is not True or len(feet)!=3 or math.dist(feet,node['position'])>.011 or
                            row.get('nav_flags')!=node['nav_flags']or row.get('radius')!=original[node['id']].get('radius')or
                            row.get('way_guid')!=original[node['id']].get('way_guid')or
                            row.get('name','')!=original[node['id']].get('name','')or
                            row.get('facing',[0,0,0])!=original[node['id']].get('facing',[0,0,0])or
                            row.get('custom_properties',{})!=original[node['id']].get('custom_properties',{})):
                            raise RuntimeError('Vollständiger Import veränderte den gemeinsam geprüften Punkt/UID/Flags/Radius: '+str(node['id']))
                certificate=reconciler.certificate(proposal,moves)
                certificate.update(status='complete',source_native_report_sha256=source_report,source_proof_inputs=initial_inputs,
                    final_native_report_sha256=self.file_hash(self.native_path(name)),final_way_sha256=self.file_hash(self.native_path(name).with_suffix('.way')),
                    final_proof_inputs=self.proof_status(name)['inputs'],scanner_archive=archive_binding,source_documents=source_documents,
                    worker_binding=worker_binding,project=project_id)
                directory=self.work/'objective-anchors';directory.mkdir(parents=True,exist_ok=True)
                raw=json.dumps(certificate,indent=2,ensure_ascii=False).encode('utf8');digest_value=objectiveanchors.sha(raw)
                path=directory/(name+'_'+str(time.time_ns())+'_'+digest_value[:16]+'.json')
                with path.open('xb')as stream:stream.write(raw)
                receipt=dict(path=str(path),sha256=digest_value,contract=objectiveanchors.CONTRACT,gametypes=list(gametypes),
                    native_report_sha256=certificate['final_native_report_sha256'],way_sha256=certificate['final_way_sha256'],
                    source_ids=certificate['source_ids'],moves=certificate['moves'],query_count=certificate['query_count'],status='complete')
            else:receipt=None
        except Exception as exc:
            failure=exc
            if reconciler:
                try:
                    diagnostic=dict(schema=1,status='failed',map=name,project=project_id,gametypes=list(gametypes),
                        error=str(exc),created=time.time(),source_native_report_sha256=source_report,
                        source_proof_inputs=initial_inputs,source_graph_sha256=initial_graph,
                        scanner_archive=archive_binding,source_documents=source_documents,worker_binding=worker_binding,
                        search=reconciler.diagnostic())
                    raw=json.dumps(diagnostic,indent=2,ensure_ascii=False).encode('utf8');digest_value=objectiveanchors.sha(raw)
                    directory=self.work/'objective-anchors';directory.mkdir(parents=True,exist_ok=True)
                    path=directory/(name+'_failed_'+str(time.time_ns())+'_'+digest_value[:16]+'.json')
                    with path.open('xb')as stream:stream.write(raw)
                    failure_evidence=dict(path=str(path),sha256=digest_value)
                    job['objective_anchor_failure_evidence']=failure_evidence
                except Exception as archive_error:
                    failure=RuntimeError(str(exc)+'; Einzelprüfungen konnten nicht gesichert werden: '+str(archive_error))
        finally:
            if job.get('quiet_generation'):self.restore_generation_bots(job)
            if failure or job['gametype']!=selected:
                self.update(job,gametype=selected,progress=f'Ausgewählten GT{selected} wiederherstellen')
                try:self.action_load_map(job)
                except Exception as restore:raise RuntimeError((str(failure)+'; 'if failure else'')+'GT-Wiederherstellung fehlgeschlagen: '+str(restore))from restore
        if failure:
            failed=dict(status='failed',map=name,gametypes=list(gametypes),error=str(failure),created=time.time(),evidence=failure_evidence)
            (self.work/(name+'.objective-anchors-failed.json')).write_text(json.dumps(failed,indent=2),encoding='utf8')
            raise failure
        if receipt:
            target=self.work/(name+'.objective-anchors.json');temporary=target.with_suffix('.tmp')
            temporary.write_text(json.dumps(receipt,indent=2),encoding='utf8');temporary.replace(target)
        return dict(certificate=receipt,runtime_snapshots=snapshots,runtime_objective_snapshots=evidences,restored_gametype=selected)

    def action_generate_goals(self, job):
        import goalgen
        self.update(job,progress="Goals für vorhandene Spieltypen erstellen")
        if self.game_mode!="tce2" or not self.active_project:
            raise ValueError("Automatische TC-Goals benötigen ein geöffnetes TCE2-Projekt.")
        name=self.require_map(job["map"])
        proof=self.proof_status(name)
        if not proof.get("valid"):
            raise RuntimeError("Goals benötigen die Prüfung des aktuellen Wegnetzes: "+"; ".join(proof.get("reasons",[])))
        selected=int(job["gametype"]);directory=self.script_directory(live=True)
        requested=tuple(g for g in(2,5)if g in self.available_game_types(name,directory=directory)['available'])
        if selected not in requested:raise ValueError('Für den gewählten Gametype existiert kein unterstütztes GT2-/GT5-Mapscript.')
        reconciled=self.reconcile_objective_anchors(job,requested,capture_goals=True)
        snapshots=reconciled['runtime_snapshots'];evidences=reconciled['runtime_objective_snapshots']
        support=self.stationary_goal_support(job)
        result=goalgen.generate(self,name,requested,runtime_snapshots=snapshots,runtime_snapshot_evidences=evidences,
                                stationary_support=support,map_script_directory=directory)
        if reconciled['certificate']:
            from projects import write_json
            manifest_path=Path(result['manifest']);manifest=read_json(manifest_path,{})
            manifest['objective_anchor_certificate']=reconciled['certificate'];write_json(manifest_path,manifest)
        result.update(restored_gametype=selected,runtime_objective_snapshots=evidences,objective_anchor_certificate=reconciled['certificate'])
        return result

    def stationary_goal_support(self,job):
        """Prove resting positions separately from transit graph import.

        One exact self-WALK per distinct candidate is shared by GT2/GT5. This
        never moves a bot or edits a waypoint. A stale cache cannot supply an
        acceptance for the new graph, module, BSP, body or collision inputs.
        """
        import goalgen
        name=self.require_map(job["map"])
        proof=self.proof_status(name)
        if not proof.get("valid"):
            raise RuntimeError("Stationäre Goals benötigen die aktuelle vollständige Wegnetzprüfung.")
        report=self.native(name);profile=native_body_profile(report)
        initial_report=self.file_hash(self.native_path(name))
        binding=goalgen.stationary_binding(name,report,initial_report,proof["inputs"])
        project_id=self.active_project["id"]
        process=self.own_test_process(required=True)
        api=getattr(self,"process_api",None) or Windows()
        identity=list(api.identity(process["pid"]) or ())
        if not identity or identity[0]!=os.path.normcase(str(self.game_executable.resolve())):
            raise ProcessIdentityChanged("Stationäre Goal-Prüfung verweigert: Testprozessidentität ist ungültig.")
        gametype=int(job["gametype"])
        def guard():
            current_process=self.own_test_process(required=True)
            if current_process["pid"]!=process["pid"] or list(api.identity(current_process["pid"]) or ())!=identity:
                raise ProcessIdentityChanged("Testprozess während stationärer Goal-Prüfung geändert.")
            self.require_loaded(name)
            if gametype not in (2,5) or self.live_gametype()!=gametype:
                raise RuntimeError("Map/Gametype während stationärer Goal-Prüfung geändert.")
            current=self.proof_status(name)
            if (not self.active_project or self.active_project["id"]!=project_id or not current.get("valid") or
                    current.get("inputs")!=proof["inputs"] or self.file_hash(self.native_path(name))!=initial_report or
                    self.require_bbox(job)!=profile):
                raise RuntimeError("Projekt, Körper oder Wegnetz während stationärer Goal-Prüfung geändert; keine Goals geschrieben.")
            self.require_profile_inputs(name,profile,self.candidate(name))
        guard()
        work=Path(self.active_project["paths"]["work"])
        cache_path=work/(name+".stationary-goals-cache.json")
        cache=read_json(cache_path,{})
        cached=cache.get("certificate") if isinstance(cache,dict) else None
        # An old cache is retained in its immutable archive, never relabelled.
        if not isinstance(cached,dict) or cached.get("binding")!=binding:cached=None
        replies=[]
        def probe(start,end):
            if len(replies)>=goalgen.STATIONARY_PROBE_LIMIT:
                raise RuntimeError("Gemeinsames natives Goal-Probenbudget überschritten.")
            self.update(job,progress=f"Sichere CAMP-/Attack-/Defend-Standplätze nativ prüfen: {len(replies)+1}/{goalgen.STATIONARY_PROBE_LIMIT}")
            reply=self.rcon("tce_navgen_probe feet "+" ".join("%.5f"%v for v in list(start)+list(end)),read_only=True)
            self.update(job,log=(job.get("log","")+"\n"+reply)[-250000:])
            replies.append(reply)
            return reply
        def finalize(certificate):
            guard()
            probed_gametypes=sorted({r["observation"]["probed_gametype"] for r in certificate["queries"]
                if type(r["observation"].get("probed_gametype"))is int})
            document=dict(schema=1,map=name,requested_gametype=gametype,probed_gametypes=probed_gametypes,project=project_id,created=time.time(),
                process_id=process["pid"],process_identity=identity,certificate=certificate,
                fresh_queries=len(replies),reused_queries=len(certificate["queries"])-len(replies),
                graph_unchanged=True,bots_moved=False)
            raw=json.dumps(document,indent=2,ensure_ascii=False).encode("utf8")
            digest_value=hashlib.sha256(raw).hexdigest()
            archive=work/"stationary-goals";archive.mkdir(parents=True,exist_ok=True)
            path=archive/(name+"_"+str(time.time_ns())+"_"+digest_value[:16]+".json")
            with path.open("xb")as stream:stream.write(raw)
            self.atomic_copy(path,cache_path)
            return dict(path=str(path),sha256=digest_value,map=name,requested_gametype=gametype,probed_gametypes=probed_gametypes,
                        process_id=process["pid"],process_identity=identity,
                        query_count=len(certificate["queries"]),fresh_query_count=len(replies))
        observation=dict(source="owned_game_readonly_probe",probed_gametype=gametype,process_id=process["pid"],
                         process_identity=identity,created=time.time())
        return goalgen.StationarySupport(report,binding,probe=probe,certificate=cached,finalize=finalize,observation=observation)

    def objective_snapshot(self, job,allowed_gametypes=(2,5)):
        """Read current mission entities separately from certified map geometry."""
        from projects import write_json
        name=self.require_map(job["map"])
        process=self.own_test_process(required=True)
        self.require_loaded(name)
        gametype=self.live_gametype()
        if gametype not in allowed_gametypes or gametype!=int(job["gametype"]):
            raise RuntimeError("Goal-Entities benötigen die ausgewählte laufende Map in GT2 oder GT5.")
        path=self.engine_nav_root()/(name+"_entities.json")
        before=path.stat().st_mtime_ns if path.exists() else 0
        self.update(job,progress="Aktuelle Missionsziele aus dem Spiel lesen; geprüftes Wegnetz erhalten")
        reply=self.rcon_with_followup("tce_navgen_entities")
        self.update(job,log=(job.get("log","")+"\n"+reply)[-250000:])
        value=self.wait_fresh(path,before,15)
        if (value.get("map")!=name or value.get("gametype")!=gametype or
                not isinstance(value.get("objective_entities"),list)):
            raise RuntimeError("Aktuelle Goal-Entities gehören nicht zur ausgewählten Map/Gametype.")
        self.require_loaded(name)
        if self.live_gametype()!=gametype or self.own_test_process(required=True).get("pid")!=process.get("pid"):
            raise ProcessIdentityChanged("Testinstanz oder Gametype beim Lesen der Goal-Entities geändert.")
        raw=path.read_bytes()
        if json.loads(raw)!=value:
            raise RuntimeError("Goal-Entities wurden während des Lesens geändert; erneut versuchen.")
        work=Path(self.active_project["paths"]["work"])
        archive=work/"objective-snapshots";archive.mkdir(parents=True,exist_ok=True)
        digest_value=hashlib.sha256(raw).hexdigest()
        archived=archive/(name+"_gt"+str(gametype)+"_"+str(time.time_ns())+"_"+digest_value[:16]+".json")
        with archived.open("xb")as stream:stream.write(raw)
        current=work/(name+"_gt"+str(gametype)+"_objectives.json")
        self.atomic_copy(archived,current)
        evidence=dict(map=name,gametype=gametype,path=str(archived),current_path=str(current),sha256=digest_value,
                      process_id=process["pid"],captured=time.time(),geometry_snapshot_unchanged=True)
        write_json(current.with_suffix(".proof.json"),evidence)
        return value,evidence

    def action_apply_goals(self, job):
        import goalgen
        return goalgen.apply(self,job,map_script_directory=self.script_directory())

    def script_directory(self,live=False):
        if not live:return 'mapscripts' # The existing visible test starter sets this explicitly.
        reply=self.rcon('g_mapScriptDirectory',read_only=True)
        return mapscript.cvar_value(reply,'g_mapScriptDirectory')

    def available_game_types(self,name,live=False,directory=None):
        if self.game_mode!='tce2':return dict(map=name,available=[2],gametypes={},mod='et')
        return mapscript.availability(self.assets,name,self.script_directory(live)if directory is None else directory)

    def complete_generated_project(self,job):
        """Only the generator writes completion: native graph, exact GT goals and checked GT7 spawns."""
        if self.game_mode!='tce2':return dict(goals=None,spawns=None,package_status=dict(ready=False,reasons=['ET-Projekt']))
        connectivity=self.generated_navigation_readiness(job['map'])
        if not connectivity['ready']:raise RuntimeError('; '.join(connectivity['reasons']))
        from projects import write_json
        name=job['map'];selected=int(job['gametype']);available=self.available_game_types(name,live=True)
        playable=[g for g in(2,5)if g in available['available']];goals=None;spawn_result=None
        try:
            if playable:
                if job['gametype']not in playable:
                    if job.get('quiet_generation'):self.restore_generation_bots(job)
                    self.update(job,gametype=playable[0]);self.action_load_map(job)
                goals=self.action_generate_goals(job)
                unresolved=[r for p in goals['plans']for r in p.get('unresolved',[])+p.get('route_unresolved',[])]
                if unresolved:raise RuntimeError('Goal-Generierung hat noch ungelöste Aufgaben: '+str(unresolved[:8]))
            else:self.reconcile_objective_anchors(job)
            if 7 in available['available']:
                if job.get('quiet_generation'):self.restore_generation_bots(job)
                self.update(job,gametype=7,progress='Deathmatch-Spawns automatisch auswählen und nativ prüfen')
                self.action_load_map(job);self.quiet_generation(job)
                report=self.native(name);expected=native_body_profile(report)
                if self.require_bbox(job)!=expected:raise RuntimeError('GT7 benötigt ein anderes Körperprofil als das erzeugte Wegnetz.')
                store=spawns.SpawnStore(self.active_project['paths']['spawns'],name);previous=store.read()
                accepted=[];attempts=[];pool=spawns.spawn_candidates(report)
                while pool and len(accepted)<24 and len(attempts)<96:
                    node=max(pool,key=lambda n:min(math.dist(n['position'],p['position'])for p in accepted))if accepted else min(pool,key=lambda n:n['id'])
                    pool.remove(node)
                    if accepted and min(math.dist(node['position'],p['position'])for p in accepted)<192:break
                    reply=self.rcon('tce_navgen_spawncheck '+' '.join('%.4f'%v for v in node['position']),read_only=True)
                    match=re.search(r'navgen spawncheck: (\{[^\n]+\})',reply);check=json.loads(match[1])if match else{}
                    attempts.append(dict(source_id=node['id'],raw_reply=reply,check=check))
                    if spawns.checked_spawn(name,node,report,check):accepted.append(node)
                if len(accepted)<4:raise RuntimeError('Keine vier nativ freien, verteilten Deathmatch-Spawns gefunden; Export bleibt gesperrt.')
                doc=spawns.checked_document(name,previous,accepted,report,attempts)
                source=mapscript.read(self.assets,name,gametype=7,directory=available['directory'])
                original=mapscript.read_raw(self.assets,name,7,available['directory'],expected=source['resolution'])
                content=mapscript.render_gt7(original,doc)
                target=Path(self.active_project['paths']['root'])/'mapscripts'/(name+'_gt7.script');target.parent.mkdir(exist_ok=True)
                if previous['nodes']:
                    backups=target.parent/'backups';backups.mkdir(exist_ok=True)
                    raw=json.dumps(previous,ensure_ascii=False,indent=2).encode('utf8');backup=backups/('spawns-'+hashlib.sha256(raw).hexdigest()+'.json')
                    if not backup.exists():backup.write_bytes(raw)
                store.write(doc);temporary=target.with_suffix('.tmp');temporary.write_bytes(content);temporary.replace(target)
                spawn_result=dict(count=len(doc['nodes']),path=str(target),sha256=self.file_hash(target),source=source['resolution'],
                                  store_sha256=self.file_hash(store.path),attempts=attempts,live_spawn_check=True)
        finally:
            if job.get('quiet_generation'):self.restore_generation_bots(job)
            if job['gametype']!=selected:
                self.update(job,gametype=selected,progress='Gewählten Spieltyp wiederherstellen');self.action_load_map(job)
        proof=self.proof_status(name)
        if not proof.get('valid'):raise RuntimeError('Abschlussstand nicht mehr aktuell: '+'; '.join(proof['reasons']))
        connectivity=self.generated_navigation_readiness(name)
        if not connectivity['ready']:raise RuntimeError('; '.join(connectivity['reasons']))
        if self.available_game_types(name,directory=available['directory'])!=available:raise RuntimeError('Mapscripts während Generierung verändert.')
        receipt=dict(schema=1,map=name,project=self.active_project['id'],created=time.time(),
            native_report_sha256=self.file_hash(self.native_path(name)),way_sha256=self.file_hash(self.native_path(name).with_suffix('.way')),
            proof_inputs=proof['inputs'],map_script_availability=available,
            goals_manifest_sha256=self.file_hash(Path(self.work)/'generated-goals.json')if goals else None,
            generated_gametypes=playable,spawns=spawn_result,connectivity=connectivity,bot_live_tested=False)
        write_json(self.work/(name+'.generation-ready.json'),receipt)
        return dict(goals=goals,spawns=spawn_result,generated_gametypes=playable,package_status=self.package_status(name),
                    objective_anchor_certificate=(goals or{}).get('objective_anchor_certificate'))

    def package_status(self,name):
        reasons=[];active=getattr(self,'active_project',None)
        if not active or active['map']!=name:return dict(ready=False,reasons=['Zuerst ein Kartenprojekt öffnen.'])
        receipt=read_json(self.work/(name+'.generation-ready.json'),{})
        if not receipt or receipt.get('map')!=name or receipt.get('project')!=active['id']:return dict(ready=False,reasons=['Netz, Goals & Spawns automatisch erstellen.'])
        proof=self.proof_status(name)
        reasons.extend(self.generated_navigation_readiness(name)['reasons'])
        if not proof.get('valid')or proof.get('inputs')!=receipt.get('proof_inputs'):reasons.append('Navigation geändert oder nicht vollständig nativ geprüft.')
        if receipt.get('native_report_sha256')!=self.file_hash(self.native_path(name))or receipt.get('way_sha256')!=self.file_hash(self.native_path(name).with_suffix('.way')):reasons.append('Aktueller WAY-/Prüfstand fehlt.')
        available=self.available_game_types(name,directory=receipt['map_script_availability']['directory'])
        if available!=receipt.get('map_script_availability'):reasons.append('Mapscripts/Spieltypen haben sich geändert. Neu generieren.')
        if sorted(receipt.get('generated_gametypes',[]))!=[g for g in(2,5)if g in available['available']]:reasons.append('Goals für vorhandene Spieltypen sind unvollständig.')
        if receipt['generated_gametypes']:
            manifest_path=self.work/'generated-goals.json';manifest=read_json(manifest_path,{})
            if self.file_hash(manifest_path)!=receipt.get('goals_manifest_sha256'):reasons.append('Goals wurden geändert oder fehlen.')
            if sorted(manifest.get('generated_gametypes',[]))!=sorted(receipt['generated_gametypes']):reasons.append('Goal-Spieltypen stimmen nicht mit der Generierung überein.')
            for filename,digest_value in manifest.get('files',{}).items():
                if Path(filename).name!=filename or self.file_hash(Path(active['paths']['bundles'])/filename)!=digest_value:reasons.append('Goal-Datei fehlt oder wurde geändert: '+filename)
        if 7 in available['available']:
            spawn=receipt.get('spawns')or{};path=Path(active['paths']['root'])/'mapscripts'/(name+'_gt7.script')
            if self.file_hash(path)!=spawn.get('sha256')or self.file_hash(active['paths']['spawns'])!=spawn.get('store_sha256'):reasons.append('Deathmatch-Spawns/Mapscript wurden geändert; erneut generieren.')
        return dict(ready=not reasons,reasons=reasons,gametypes=available['available'],generated_gametypes=receipt['generated_gametypes'],receipt=receipt)

    def export_project_package(self,name):
        with self.lock:
            if self.busy:raise RuntimeError('Laufenden Auftrag vor dem ZIP-Export abwarten.')
            status=self.package_status(name)
            if not status['ready']:raise RuntimeError('ZIP noch nicht bereit: '+'; '.join(status['reasons']))
            active=self.active_project;receipt=status['receipt'];files={};scripts={}
            for gt in receipt['generated_gametypes']:
                bundle=self.nav_bundles(gt)
                for kind in('script','goals'):
                    path=bundle.path(name,kind);files[path.name]=path.read_bytes()
            for gt in status['gametypes']:
                source=mapscript.read(self.assets,name,gametype=gt,directory=receipt['map_script_availability']['directory'])
                scripts[name+'_gt'+str(gt)+'.script']=(Path(active['paths']['root'])/'mapscripts'/(name+'_gt7.script')).read_bytes()if gt==7 else mapscript.read_raw(
                    self.assets,name,gt,receipt['map_script_availability']['directory'],expected=source['resolution'])
            way=self.native_path(name).with_suffix('.way').read_bytes()
            archive=NavBundles.archive_project(name,way,files,scripts,dict(map=name,project=active['name'],gametypes=status['gametypes'],native_checked=True,bot_live_tested=False))
            after=self.package_status(name)
            if not after['ready']or after['receipt']!=receipt:raise RuntimeError('Projekt während ZIP-Erstellung verändert; nichts exportiert.')
            return archive

    def generated_navigation_readiness(self,name):
        report=self.native(name);status=navigation_readiness(report)
        if status['ready']or report.get('generation_complete')is not True:return status
        return navigation_readiness(report,spawn_bindings(report,self.native_path(name)))

    def project_state(self):
        self.assets.refresh()
        catalog, catalog_warnings = self.projects.catalog(self.assets.maps)
        catalog=mapscript.catalog_availability(SimpleNamespace(
            roots=[self.test/'etmain',self.test/'tce2'],catalog_base_root=self.test/'etmain'),
            catalog,self.script_directory())
        values, project_warnings = self.projects.list()
        return {"active": self.active_project, "projects": values, "catalog": catalog,
                "recovery": self.projects.recovery(), "warnings": project_warnings + catalog_warnings,
                "directory": str(self.projects.directory)}

    def activate_project(self, active):
        mode = active["mod"]
        self.game_mode = mode
        self.mod = self.test / ("tce2" if mode == "tce2" else "omnibot")
        self.runtime = self.mod / "navgen"
        self.active_project = active
        self.project_native = Path(active["paths"]["native"])
        self.preview = Path(active["paths"]["preview"])
        self.work = Path(active["paths"]["work"])
        self.drafts = Drafts(active["paths"]["drafts"])
        self.selected_map = active["map"]
        # The leased source keeps its original PK3 name. Use the game's actual
        # base -> mod VFS order, including its real alphabetical overrides;
        # preview/library directories must not shadow the running engine.
        self.assets = Assets([self.test / "etmain", self.mod])
        self.native_cached.clear(); self.hash_cached.clear(); self.game_state_cache = None
        self.jobs.clear()

    def project(self, payload):
        with self.lock:
            if self.busy:
                raise RuntimeError("Laufenden Auftrag abwarten, bevor das Projekt geändert wird.")
            operation = payload.get("op")
            if operation in ("create", "open"):
                self.projects.require_closed()
                if operation == "open":
                    document = self.projects.document(payload.get("id"))
                    mode = document["mod"]
                else:
                    mode = payload.get("mod")
                if mode not in ("tce2", "et"):
                    raise ValueError("TCE2 oder ET als Projekt-Mod auswählen.")
                module = self.test / ("tce2" if mode == "tce2" else "omnibot") / "qagame_mp_x86.dll"
                if not module.is_file():
                    raise RuntimeError("Spielmodul für diesen Mod fehlt: " + str(module))
                process = self.own_test_process(require_port=False)
                if process:
                    self.stop_owned_test({"log": ""}, process)
                if operation == "create":
                    self.projects.catalog(self.assets.maps)
                    active = self.projects.create(payload.get("name"), payload.get("map"), mode,
                        payload.get("gametype", 2), payload.get("source_id"))
                else:
                    active = self.projects.open(payload.get("id"))
                self.activate_project(active)
                if mode=='tce2':
                    availability=self.available_game_types(active['map'])
                    if availability['available']and active['gametype']not in availability['available']:
                        self.active_project=self.projects.save(gametype=availability['available'][0])
            elif operation == "import_bundle":
                if not self.active_project:
                    raise RuntimeError("Zuerst ein Projekt erstellen oder öffnen.")
                name = self.active_project["map"]
                gametype = self.active_project["gametype"] if self.game_mode == "tce2" else None
                if gametype is not None:mapscript.require_gametype(self.assets,name,gametype,self.script_directory())
                source_root = self.test / ("omni-bot" if self.game_mode == "tce2" else "omnibot") / "et" / "nav"
                source = NavBundles(source_root, gametype)
                legacy = NavBundles(source_root)
                destination = self.nav_bundles(gametype)
                choices = {}
                for kind in ("way", "script", "goals"):
                    candidate = source.path(name, kind)
                    if not candidate.is_file() and kind != "way":
                        candidate = legacy.path(name, kind)
                    if candidate.is_file():
                        if candidate.stat().st_size > 16 * 1024 * 1024:
                            raise ValueError("Navigationsdatei ist zu groß: " + candidate.name)
                        target = destination.path(name, kind)
                        if target.exists():
                            raise RuntimeError("Projektdatei existiert bereits; Import überschreibt nichts: " + target.name)
                        choices[kind] = (candidate, target)
                if not choices:
                    raise FileNotFoundError("Für diese Map sind keine vorhandenen WAY-/GM-Dateien installiert.")
                if "way" in choices:
                    import tce_navgen
                    tce_navgen.read_way(choices["way"][0])
                created = []
                try:
                    for source_file, target in choices.values():
                        data = source_file.read_bytes()
                        with target.open("xb") as stream:
                            stream.write(data)
                        created.append(target)
                except Exception:
                    for target in created:
                        target.unlink()
                    raise
                self.active_project = self.projects.save()
                result = self.project_state()
                result["imported"] = [dict(kind=kind, source=str(pair[0]), path=str(pair[1]), sha256=self.file_hash(pair[1]))
                                      for kind, pair in choices.items()]
                return result
            elif operation in ("save", "close", "save_close"):
                if not self.active_project:
                    raise RuntimeError("Zuerst ein Projekt erstellen oder öffnen.")
                name = self.active_project["map"]
                if self.game_mode=='tce2'and payload.get('gametype')is not None:
                    mapscript.require_gametype(self.assets,name,payload['gametype'],self.script_directory())
                draft = self.drafts.get(name)
                if draft:
                    # Saving preserves the document's own source snapshot even
                    # after external geometry changes. Export/check/apply retain
                    # their current-environment gate; closing must not lose edits.
                    self.drafts.save(name, draft["revision"], draft["base"]["environment"])
                self.active_project = self.projects.save(payload.get("name"), payload.get("gametype"))
                if operation != "save":
                    process = self.own_test_process(require_port=False)
                    if process:
                        self.stop_owned_test({"log": ""}, process)
                    closed = self.projects.close()
                    self.active_project = None
                    self.project_native = None
                    self.preview = self.test / ("navmesh-preview" if self.game_mode == "tce2" else "navmesh-preview-et")
                    self.work = self.preview / "navstudio"
                    self.work.mkdir(parents=True, exist_ok=True)
                    self.drafts = Drafts(self.work / "drafts")
                    self.selected_map = ""
                    self.native_cached.clear(); self.hash_cached.clear(); self.game_state_cache = None
                    self.assets.refresh(force=True)
                    self.jobs.clear()
                    result = self.project_state()
                    result["closed"] = closed
                    return result
            else:
                raise ValueError("Unbekannte Projektaktion.")
            return self.project_state()

    def settings(self):
        return {"instance_path": str(self.test), "game_executable": str(self.game_executable),
                "maps_dirs": self.maps_dirs, "rcon_port": self.args.rcon_port,
                "settings_file": str(self.settings_file), "busy": self.busy,
                "running": bool(self.test_processes()),
                "note": "Eine vorhandene Installation. Pfadwechsel verschiebt und kopiert keine Spieldaten."}

    def configure(self, payload):
        value = validate_settings(payload)
        value["game_mode"] = getattr(self, "game_mode", "tce2")
        with self.lock:
            if getattr(self, "active_project", None):
                raise RuntimeError("Projekt zuerst speichern und schließen, bevor Pfade geändert werden.")
            if self.busy or self.test_processes():
                raise RuntimeError("Zuerst Testspiel schließen und laufenden Auftrag abwarten; danach Pfade ändern.")
            unsaved = sorted(name for name, doc in self.drafts.documents.items()
                             if doc.get("saved_revision") != doc.get("revision"))
            if unsaved:
                raise RuntimeError("Ungespeicherte Entwürfe (" + ", ".join(unsaved) +
                                   "). Zuerst speichern oder archivieren; danach Einstellungen ändern.")
            # Prepare and validate all new resources before replacing the live configuration.
            test = Path(value["instance_path"])
            exe = Path(value["game_executable"])
            if self.test_processes(exe):
                raise RuntimeError("Im gewählten Spielordner läuft bereits ein Spiel; erst schließen.")
            mod = test / ("tce2" if self.game_mode == "tce2" else "omnibot")
            assets = Assets([self.workspace / "etmain", test / "etmain", self.workspace / mod.name, mod, *value["maps_dirs"]])
            preview = test / ("navmesh-preview" if self.game_mode == "tce2" else "navmesh-preview-et")
            work = preview / "navstudio"
            work.mkdir(parents=True, exist_ok=True)
            save_settings(self.settings_file, value)
            self.test, self.game_executable, self.maps_dirs = test, exe, value["maps_dirs"]
            self.mod, self.preview = mod, preview
            self.runtime, self.work = self.mod / "navgen", work
            self.drafts, self.assets = Drafts(work / "drafts"), assets
            self.process_api = None
            self.native_cached.clear()
            self.hash_cached.clear()
            self.game_state_cache = None
            self.recast = self.find_recast()
            self.projects = self.make_projects()
            choices = sorted(n for n in self.assets.maps if self.map_in_mode(n))
            if self.selected_map not in choices:
                self.selected_map = ""
        return self.settings()

    def find_recast(self):
        name = "tce_navmesh_preview.exe" if os.name == "nt" else "tce_navmesh_preview"
        candidates = [self.test / "navmesh-build" / "Release" / name,
                      self.test / "navmesh-build" / name,
                      Path(sys.executable).parent / "bin" / name,
                      Path(sys.executable).parent / name]
        if getattr(sys, "frozen", False):
            bundle = Path(getattr(sys, "_MEIPASS", Path(sys.executable).parent))
            # The verified package owns its Recast binary. An old local build
            # beside the game must not override a newer frozen application.
            candidates = [bundle / "bin" / name, bundle / name] + candidates
        return next((p.resolve() for p in candidates if p.is_file()), None)

    def map_in_mode(self, name):
        return name.lower().startswith("obj_") == (getattr(self, "game_mode", "tce2") == "tce2")

    def nav_bundles(self, gametype=None):
        active = getattr(self, "active_project", None)
        if active:
            if active["mod"] == "et": gametype = None
            elif gametype is None: gametype = active["gametype"]
            return NavBundles(active["paths"]["bundles"], gametype)
        return self.runtime_nav_bundles(gametype)

    def runtime_nav_bundles(self, gametype=None):
        """Actual Omni serializer destination, independent of project storage."""
        root = self.mod if self.game_mode == 'et' else self.test / 'omni-bot'
        if self.game_mode == 'et': gametype = None
        return NavBundles(root / 'et' / 'nav', gametype)

    def live_gametype(self):
        reply = self.rcon('g_gametype', read_only=True)
        match = re.search(r'is:\s*"([0-9]+)', reply)
        if not match: raise RuntimeError('Aktiver Gametype konnte nicht gelesen werden.')
        return int(match[1])

    def change_mode(self, job, mode):
        if getattr(self, "active_project", None):
            raise RuntimeError("Der Mod gehört zum Projekt. Projekt schließen und ein anderes öffnen.")
        unsaved = [name for name, doc in self.drafts.documents.items()
                   if doc.get("saved_revision") != doc.get("revision")]
        if unsaved:
            raise RuntimeError("Save or archive unsaved drafts before switching game mode.")
        mod = self.test / ("tce2" if mode == "tce2" else "omnibot")
        if not (mod / "qagame_mp_x86.dll").is_file():
            raise RuntimeError("Game module missing: " + str(mod / "qagame_mp_x86.dll"))
        process = self.own_test_process(require_port=False)
        if process: self.stop_owned_test(job, process)
        self.game_mode, self.mod = mode, mod
        self.preview = self.test / ("navmesh-preview" if mode == "tce2" else "navmesh-preview-et")
        self.runtime, self.work = mod / "navgen", self.preview / "navstudio"
        self.work.mkdir(parents=True, exist_ok=True)
        self.drafts = Drafts(self.work / "drafts")
        self.assets = Assets([self.workspace / "etmain", self.test / "etmain", self.workspace / mod.name, mod, *self.maps_dirs])
        self.native_cached.clear(); self.hash_cached.clear(); self.game_state_cache = None
        choices = sorted(n for n in self.assets.maps if self.map_in_mode(n))
        self.selected_map = ""
        saved = read_json(self.settings_file, {})
        saved["game_mode"] = mode
        save_settings(self.settings_file, saved)
        return {"game_mode": mode, "fs_game": mod.name, "note": "Game mode selected. Open a map to start the visible test game."}

    def action_mode_tce2(self, job):
        return self.change_mode(job, "tce2")

    def action_mode_et(self, job):
        return self.change_mode(job, "et")

    def require_map(self, name):
        name = valid_map(name)
        active = getattr(self, "active_project", None)
        if active and name != active["map"]:
            raise ValueError("Diese Map gehört zu einem anderen Projekt. Zuerst das passende Projekt öffnen.")
        self.assets.refresh()
        if not self.map_in_mode(name):
            raise ValueError("Map does not belong to the selected game mode.")
        if name not in self.assets.maps:
            raise ValueError("Map wurde in den konfigurierten lokalen Paketen nicht gefunden.")
        return name

    def path(self, name, suffix):
        return self.preview / (valid_map(name) + suffix)

    def candidate(self, name):
        draft = self.drafts.get(name)
        # A changed draft is the current graph, even before it is exported.
        # Falling back to an older automatic candidate resurrects deleted IDs.
        if draft:
            edited = self.preview / (name + "_candidate_edit.txt")
            if draft.get("export") and edited.is_file() and edited.with_suffix(".json").is_file():
                return edited
            return None
        selection = read_json(self.work / (name + ".candidate-selection.json"), {})
        if selection.get("kind")=="imported_way":
            return None
        selected = self.preview / selection.get("name", "")
        if selection and selected.parent == self.preview and selected.is_file() and selected.with_suffix(".json").is_file():
            return selected
        options = [p for p in self.preview.glob(name + "_candidate*.txt")
                   if re.fullmatch(re.escape(name) + r"_candidate(?:_[A-Za-z0-9]+)?\.txt", p.name)
                   and p.name != name + "_candidate_edit.txt"
                   and p.with_suffix(".json").is_file()]
        return max(options, key=lambda p: p.stat().st_mtime_ns) if options else None

    def native_path(self, name):
        return (getattr(self, "project_native", None) or self.engine_nav_root()) / (valid_map(name) + "_navmesh.json")

    def engine_nav_root(self):
        # Original ET's bridge exposes fs_homepath through GetLogPath;
        # its Omni-bot adapter uses that root, not the mod's trap_FS path.
        return self.test / "navgen" if getattr(self,"game_mode","tce2")=="et" else self.runtime

    def runtime_native_path(self, name):
        return self.engine_nav_root() / (valid_map(name) + "_navmesh.json")

    def snapshot_path(self, name):
        return (getattr(self, "project_native", None) or self.engine_nav_root()) / (valid_map(name) + "_entities.json")

    def staged_candidate_path(self, candidate):
        return (getattr(self, "project_native", None) or self.engine_nav_root()) / candidate.name

    @staticmethod
    def atomic_copy(source, target):
        source, target = Path(source), Path(target)
        if source.resolve() == target.resolve():
            return
        target.parent.mkdir(parents=True, exist_ok=True)
        temporary = target.with_suffix(target.suffix + ".tmp")
        shutil.copy2(source, temporary)
        temporary.replace(target)

    def select_candidate(self, name, path):
        """Generation order is explicit; filesystem modification times are not."""
        self.work.mkdir(parents=True, exist_ok=True)
        selection = self.work / (name + ".candidate-selection.json")
        pending = selection.with_suffix(".tmp")
        pending.write_text(json.dumps(self.candidate_identity(path)), encoding="utf-8")
        pending.replace(selection)

    def prepare_draft(self, name):
        drafts = getattr(self, "drafts", None)
        doc = drafts.get(name) if drafts else None
        if doc:
            environment = self.draft_environment(name)
            drafts.save(name, doc["revision"], environment)
            return drafts.export_candidate(name, doc["revision"], environment, self.preview)
        return self.candidate(name)

    def native(self, name):
        path = self.native_path(name)
        stamp = path.stat().st_mtime_ns if path.is_file() else 0
        cached = self.native_cached.get(name)
        if cached is None or cached[0] != stamp:
            self.native_cached[name] = stamp, read_json(path)
        return self.native_cached[name][1]

    def file_hash(self, path):
        path = Path(path)
        if not path.is_file():
            return None
        stamp = (path.stat().st_mtime_ns, path.stat().st_size)
        key = str(path.resolve())
        cached = self.hash_cached.get(key)
        if cached is None or cached[0] != stamp:
            self.hash_cached[key] = stamp, digest(path)
        return self.hash_cached[key][1]

    def bsp_hash(self, name):
        record = self.assets.maps[name]
        path = record["path"]
        stamp = (path.stat().st_mtime_ns, path.stat().st_size, record["member"])
        key = "bsp:" + str(path) + ":" + str(record["member"])
        cached = self.hash_cached.get(key)
        if cached is None or cached[0] != stamp:
            value = hashlib.sha256(self.assets.read("maps/" + name + ".bsp", 256 * 1024 * 1024)).hexdigest()
            self.hash_cached[key] = stamp, value
        return self.hash_cached[key][1]

    def proof_status(self, name, candidate=None, receipt=True):
        """Bind every gate to current bytes, not filenames or successful old jobs."""
        candidate = candidate or self.candidate(name)
        reasons, hashes = [], {}
        metadata = read_json(candidate.with_suffix(".json"), {}) if candidate else {}
        draft = self.drafts.get(name)
        if draft:
            export = draft.get("export")
            evidence = metadata.get("edit_draft", {})
            if (not export or draft["saved_revision"] != draft["revision"] or
                    evidence.get("revision") != draft["revision"] or
                    evidence.get("sha256") != graph_hash(draft)):
                reasons.append("Bearbeitungsentwurf geändert: speichern, als Prüfkandidat exportieren und erneut nativ prüfen.")
            if draft["base"]["environment"] != self.draft_environment(name):
                reasons.append("Die Geometriegrundlage des Bearbeitungsentwurfs wurde geändert.")
        elif metadata.get("candidate_mode") == "manual_draft":
            reasons.append("Der zugehörige versionierte Bearbeitungsentwurf fehlt.")
        geometry = read_json(self.path(name, ".geometry.json"), {})
        if not candidate or metadata.get("map") != name:
            reasons.append("Kandidat oder passende Metadaten fehlen.")
        else:
            hashes["candidate"] = self.file_hash(candidate)
            hashes["candidate_metadata"] = self.file_hash(candidate.with_suffix(".json"))
            # The converter hashes canonical LF text; Windows text I/O may
            # store CRLF. A separate raw digest binds the staged engine file.
            normalized = hashlib.sha256(candidate.read_text(encoding="ascii").encode("ascii")).hexdigest()
            if metadata.get("candidate_sha256") != normalized:
                reasons.append("Kandidattext stimmt nicht mit seinen Metadaten überein.")
            for kind, suffix in (("stand", ".stand.json"), ("duck", ".duck.json"), ("geometry", ".geometry.json")):
                hashes[kind] = self.file_hash(self.path(name, suffix))
                expected = metadata.get("sources", {}).get(kind, {}).get("sha256")
                if not expected or hashes[kind] != expected:
                    reasons.append("Kandidatquelle geändert oder unvollständig: " + kind)
        hashes["bsp"] = self.bsp_hash(name)
        if geometry.get("input", {}).get("bsp_sha256") != hashes["bsp"]:
            reasons.append("BSP und erzeugte Kollisionsgeometrie gehören nicht zum selben Stand.")
        hashes["collision"] = self.file_hash(self.path(name, ".collision.obj"))
        if not hashes["collision"] or geometry.get("output", {}).get("obj_sha256") != hashes["collision"]:
            reasons.append("Kollisionsgeometrie fehlt oder wurde nachträglich geändert.")
        hashes["runtime_snapshot"] = self.file_hash(self.snapshot_path(name))
        if not hashes["runtime_snapshot"] or geometry.get("runtime_snapshot", {}).get("sha256") != hashes["runtime_snapshot"]:
            reasons.append("Mapmodell-Snapshot passt nicht mehr zur Kollisionsgeometrie.")
        module_dir=getattr(self,"mod",None)
        hashes["game_module"] = self.file_hash(module_dir/"qagame_mp_x86.dll") if module_dir else None
        # Bind policy semantics as well as binary/source bytes; changing the
        # posture-specific water or runtime flat-slick contract requires a
        # fresh current report even when an old project is opened offline.
        hashes["navigation_contract"] = "tce_pm_middle_water_v2_flat_slick_v1" if getattr(self,"game_mode","tce2")=="tce2" else "et_engine_native_trace_v1"
        candidate_valid = not reasons
        if receipt:
            proof = read_json(self.work / (name + ".native-proof.json"), {})
            report = self.native(name)
            if not proof:
                reasons.append("Hash-Beleg der nativen Prüfung fehlt; einmal erneut nativ prüfen.")
            else:
                if proof.get("inputs") != hashes or proof.get("candidate") != (candidate.name if candidate else None):
                    reasons.append("Native Prüfung verwendet andere Kandidaten oder Geometriedaten.")
                for key, path in (("native_report", self.native_path(name)), ("way", self.native_path(name).with_suffix(".way"))):
                    if proof.get(key) != self.file_hash(path):
                        reasons.append("Nativer Prüfbeleg ist überholt: " + key)
                if not candidate or proof.get("staged_candidate") != self.file_hash(self.staged_candidate_path(candidate)):
                    reasons.append("Der in TCE2 geprüfte Kandidat wurde ersetzt.")
            if not report or not report.get("generation_complete"):
                reasons.append("Vollständiger nativer Prüfbericht fehlt.")
            elif candidate and Path(report.get("source", "")).name != candidate.name:
                reasons.append("Native Prüfung gehört zu einem anderen Kandidaten.")
        return {"candidate_valid": candidate_valid, "valid": not reasons, "reasons": reasons, "inputs": hashes}

    def report_status(self, name, candidate=None):
        """A failed check can be current, but must never inherit an older WAY/audit."""
        candidate = candidate or self.candidate(name)
        inputs = self.proof_status(name, candidate, receipt=False)
        attempt = read_json(self.work / (name + ".native-attempt.json"), {})
        # Older completed receipts remain usable until the next real check.
        if not attempt:
            attempt = read_json(self.work / (name + ".native-proof.json"), {})
        reasons = list(inputs["reasons"])
        if not attempt or not candidate:
            reasons.append("Für den aktuellen Stand liegt noch keine Prüfung vor.")
        elif (attempt.get("candidate") != candidate.name or attempt.get("inputs") != inputs["inputs"] or
              attempt.get("native_report") != self.file_hash(self.native_path(name))):
            reasons.append("Prüfbericht gehört zu einem früheren Bearbeitungs- oder Generierungsstand.")
        report = self.native(name)
        if not report or report.get("map") != name or (candidate and Path(report.get("source", "")).name != candidate.name):
            reasons.append("Prüfbericht und ausgewählte Karte/Quelle stimmen nicht überein.")
        return {"current": not reasons, "reasons": reasons, "candidate": candidate.name if candidate else None,
                "candidate_sha256": inputs["inputs"].get("candidate"), "native_report": self.file_hash(self.native_path(name)),
                "generation_complete": bool(report and report.get("generation_complete"))}

    def current_audit(self, name, report_status):
        if not report_status["current"]:
            return None
        audit = read_json(self.path(name, ".native-audit.json"), {})
        if audit.get("report_binding", {}).get("native_report") != report_status["native_report"]:
            return None
        return audit

    def native_edit_identities(self, name):
        """Read current certified UIDs before a real edit, never on scene/read.

        Scanner failures have no exported identities and must not read an old
        WAY. Once a complete native graph exists, its exact draft/candidate,
        receipt and audited WAY must still agree before any zero-UID source
        can acquire that graph's identity.
        """
        doc=self.drafts.get(name)
        if not doc or not (getattr(self,'project_native',None) or getattr(self,'runtime',None)):return {}
        report=self.native(name)
        if not report:return {}
        if report.get('generation_complete') is not True:return {}
        # Older engines/fixtures do not publish this identity contract. They
        # cannot supply an adoption map by guessing from compact indices.
        if report.get('waypoint_metadata_contract')!='TCE_NODE_METADATA_1':return {}
        accepted={n['id']:n for n in report.get('nodes',[]) if n.get('accepted') is True}
        wanted={n['id'] for n in doc['nodes'] if not waypoint_metadata(n)[2] and n['id'] in accepted}
        if not wanted:return {}
        candidate=self.candidate(name)
        state=self.proof_status(name,candidate)
        status=self.report_status(name,candidate)
        if not state['valid'] or not status['current']:
            raise RuntimeError('WAY-Punktidentitäten sind noch nicht aktuell belegt. Navigation zuerst erneut nativ prüfen; keine alten WAY-UIDs übernommen.')
        audit=self.current_audit(name,status)
        path=self.native_path(name);way=path.with_suffix('.way')
        candidate_identity=self.candidate_identity(candidate)
        if (report.get('map')!=name or not audit or audit.get('binary_report_match') is not True or
                audit.get('sha256')!=self.file_hash(way) or
                audit.get('report_binding',{}).get('candidate')!=candidate_identity):
            raise RuntimeError('Aktueller WAY-Audit fehlt oder gehört zu einem anderen Kandidaten; Punktidentitäten nicht übernommen.')
        source=read_json(candidate.with_suffix('.json'),{})
        evidence=source.get('edit_draft',{})
        if (source.get('candidate_mode')!='manual_draft' or evidence.get('revision')!=doc['revision'] or
                evidence.get('sha256')!=graph_hash(doc)):
            raise RuntimeError('Native WAY-UIDs gehören nicht zum aktuellen Bearbeitungsentwurf.')
        from tce_navgen import read_way, audit_waypoint_metadata, audit_custom_properties
        before=(self.file_hash(path),self.file_hash(way),candidate_identity,graph_hash(doc))
        binary,_pairs=read_way(way)
        audit_waypoint_metadata(report,path,binary)
        audit_custom_properties(report,path,binary)
        identities={};uids=set()
        for node in doc['nodes']:
            native=accepted.get(node['id'])
            if native is None:raise RuntimeError('Nativer WAY-Beleg enthält nicht alle aktuellen Entwurfspunkte.')
            index=native.get('index');uid=native.get('way_guid')
            if (type(index) is not int or not 0<=index<len(binary) or type(uid) is not int or not 1<=uid<=2147483647 or
                    uid in uids or binary[index]['uid']!=uid or binary[index]['properties'].get('tce_node')!=str(node['id'])):
                raise RuntimeError('Native WAY-Punktidentität ist nicht eindeutig an Quelle und Datei gebunden.')
            prior=waypoint_metadata(node)[2]
            if prior and prior!=uid:raise RuntimeError('Native WAY-UID hat eine vorhandene Punktidentität verändert.')
            identities[node['id']]=uid;uids.add(uid)
        if (before!=(self.file_hash(path),self.file_hash(way),self.candidate_identity(candidate),graph_hash(doc)) or
                not self.proof_status(name,candidate)['valid'] or not self.report_status(name,candidate)['current']):
            raise RuntimeError('WAY-Identitätsgrundlage während der Bearbeitung verändert; keine Änderung übernommen.')
        return identities

    def imported_navigation(self, name):
        if not getattr(self, "active_project", None):
            return None
        path = self.nav_bundles().path(name, "way")
        if not path.is_file():
            return None
        from tce_navgen import read_way
        binary, pairs = read_way(path)
        default_profile=MOVEMENT_PROFILES["et"] if getattr(self,"game_mode","tce2")=="et" else MOVEMENT_PROFILES[1]
        profile = read_json(self.path(name, ".geometry.json"), {}).get("movement_profile") or default_profile
        try:
            profile = self.recast_body_profile(name)
        except RuntimeError:
            pass
        offset = profile["origin_above_feet"]
        identities = imported_way_ids(binary)
        by_uid = {node["uid"]: identities[node["index"]] for node in binary}
        nodes = []
        for index, source in enumerate(binary):
            props = source["properties"]
            roles = []
            from edit import door_role
            door = door_role(props, by_uid)
            if door is not None:
                roles.append(door)
            if props.get("tce_stand_jump"):
                target=by_uid.get(int(props.get("tce_stand_jump_target",-1)))
                if target is None: raise ValueError("Imported standing jump has no valid WAY landing target.")
                roles.append(dict(kind="standing_jump",role="launch",landing_node=target))
            if props.get("tce_free_climb"):
                target = by_uid.get(int(props.get("tce_free_climb_target", -1)))
                if target is None:
                    raise ValueError("Importiertes Freiklettern hat kein gültiges WAY-Landeziel.")
                roles.append(dict(kind="free_climb", role="launch", landing_node=target))
            if props.get("tce_walk_drop"):
                target = by_uid.get(int(props.get("tce_walk_drop_target", -1)))
                if target is None:
                    raise ValueError("Importierter Abstieg hat kein gültiges WAY-Landeziel.")
                roles.append(dict(kind="walk_drop", role="launch", landing_node=target))
            p = list(source["position"]); p[2] -= offset
            nodes.append(dict(id=identities[index], position=p, pos=p, nav_flags=source["flags"],
                              crouch=bool(source["flags"] & 64), radius=source["radius"],
                              accepted=None, native_checked=False, status="candidate", state="candidate",
                              reason="imported_way_requires_native_validation", locked=False,
                              action_properties=dict(props), action_roles=roles, facing=list(source["facing"]), name=source["name"],way_guid=source["uid"]))
        edges = [dict(**{"from":identities[a], "to":identities[b]}, accepted=None, native_checked=False,
                      status="candidate", reason="imported_way_requires_native_validation") for a,b in sorted(pairs)]
        return dict(nodes=nodes, edges=edges, source="imported_way", source_path=str(path), source_sha256=self.file_hash(path))

    def live_way_saved(self, name):
        """Promote exactly the game's saved WAY into the current project draft."""
        name=self.require_map(name)
        with self.lock:
            doc=self.drafts.get(name)
            if doc:
                if doc.get("saved_revision")!=doc["revision"]:
                    raise RuntimeError("Ungesicherten Studio-Entwurf vor Ingame-Speichern zuerst sichern; keine Änderungen überschreiben.")
                self.drafts.archive(name,doc["revision"])
            path=self.nav_bundles().path(name,"way")
            if not path.is_file():raise RuntimeError("Frische Ingame-WAY fehlt im aktuellen Projekt.")
            selection=self.work/(name+".candidate-selection.json")
            selection.write_text(json.dumps(dict(kind="imported_way",source=str(path),sha256=self.file_hash(path))),encoding="utf8")
            for suffix in (".native-proof.json",".native-attempt.json"):
                (self.work/(name+suffix)).unlink(missing_ok=True)
            self.native_cached.pop(name,None)
            graph=self.imported_navigation(name)
            if not graph:raise RuntimeError("Ingame-WAY konnte nicht als Projektgraph gelesen werden.")
            self.begin_imported_draft(name,graph)
            return dict(nodes=len(graph["nodes"]),directed_links=len(graph["edges"]),
                        source_sha256=graph["source_sha256"],requires_native_validation=True)

    def begin_imported_draft(self, name, graph):
        default_profile=MOVEMENT_PROFILES["et"] if getattr(self,"game_mode","tce2")=="et" else MOVEMENT_PROFILES[1]
        try:profile=self.recast_body_profile(name)
        except RuntimeError:profile=None
        base=dict(environment=self.draft_environment(name),native_report=None,way=graph.get("source_sha256"),
            candidate=None,candidate_name=None,movement_profile=None,
            imported_origin_offset=(profile or default_profile)["origin_above_feet"],
            protected_anchor_ids=[],generated_guide_ids=[],protected_action_edges=[])
        self.drafts.start(name,graph,base)
        self.drafts.save(name,0,base["environment"])
        return self.drafts.get(name)

    def begin_navigation_draft(self, name, navigation, native=None):
        """The same explicit native-to-draft contract for generation and editing."""
        candidate = self.candidate(name)
        try:
            profile = self.recast_body_profile(name)
        except RuntimeError:
            profile = None
        base = dict(environment=self.draft_environment(name), native_report=self.file_hash(self.native_path(name)),
                    way=self.file_hash(self.native_path(name).with_suffix(".way")),
                    candidate=self.file_hash(candidate) if candidate else None,
                    candidate_name=candidate.name if candidate else None, movement_profile=profile)
        if navigation.get("source") == "imported_way":
            base["imported_origin_offset"] = (profile or MOVEMENT_PROFILES[1])["origin_above_feet"]
        protection = action_protection(native or {})
        base.update({key: protection[key] for key in
                     ("protected_anchor_ids", "generated_guide_ids", "protected_action_edges")})
        return self.drafts.start(name, navigation, base)

    def normalize_generated_graph(self, job):
        """Publish the accepted network; retain every scanner rejection separately.

        A quarantined, native-valid floor remains an explicit missing area, not
        an accepted authored waypoint or a discarded geometry error. Accepted
        nodes with no exit are retained and still fail the following full check.
        This transition grants no WAY/proof and never overwrites an authored graph.
        """
        name = job["map"]
        store = getattr(self, "drafts", None)
        if not store or store.get(name):
            return None
        report = self.native(name)
        if not report or report.get("candidate_mode") != "automatic_spatial":
            return None
        candidate = self.candidate(name)
        if not candidate or not self.report_status(name, candidate)["current"]:
            raise RuntimeError("Scannerbericht ist veraltet; aktuellen Kandidaten zuerst nativ prüfen.")
        coverage = report.get("native_input_coverage", {})
        capacity = coverage.get("capacity", {})
        if (capacity.get("input_validation_complete") is not True or capacity.get("input_truncated") is not False or
                type(report.get("traces")) is not int or type(report.get("trace_budget")) is not int or
                not 0 <= report["traces"] < report["trace_budget"]):
            raise RuntimeError("Unvollständige Scannerprüfung wird nicht als bearbeitbares Netz übernommen.")
        profile = native_body_profile(report)
        source_hash = self.file_hash(self.native_path(name))
        identity = self.candidate_identity(candidate)
        environment = self.draft_environment(name)
        archive = self.work / "generation-scans" / (name + "-" + source_hash)
        archive.mkdir(parents=True, exist_ok=True)
        report_path, candidate_path = archive / "scanner.json", archive / candidate.name
        self.atomic_copy(self.native_path(name), report_path)
        self.atomic_copy(candidate, candidate_path)
        self.atomic_copy(candidate.with_suffix(".json"), candidate_path.with_suffix(".json"))
        if (self.file_hash(report_path) != source_hash or self.candidate_identity(candidate_path) != identity or
                not self.report_status(name, candidate)["current"] or self.draft_environment(name) != environment):
            raise RuntimeError("Scannergrundlage wurde während der Übernahme verändert; kein Entwurf angelegt.")
        origin = profile["origin_above_feet"]
        protection = action_protection(report)
        nodes = []
        for source in report.get("nodes", []):
            if source.get("accepted") is not True:
                continue
            node = dict(source)
            p = node["position"]
            node["position"] = [p[0], p[1], p[2]-origin]
            node["action_roles"] = protection["action_roles"].get(node["id"], [])
            node["action_properties"] = protection["action_properties"].get(node["id"], {})
            nodes.append(node)
        if not nodes:
            raise RuntimeError("Scanner hat kein akzeptiertes Netz erzeugt; Diagnose bleibt erhalten.")
        manifest = dict(schema=1, map=name, source="automatic_spatial_scanner", created=time.time(),
                        environment=environment, native_report=str(report_path), native_report_sha256=source_hash,
                        candidate=identity, candidate_path=str(candidate_path), movement_profile=profile,
                        acceptance_certificate=False, full_map_verified=False)
        manifest_path = self.work / (name + ".generation-diagnostics.json")
        temporary = manifest_path.with_suffix(".tmp")
        temporary.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
        temporary.replace(manifest_path)
        doc = self.begin_navigation_draft(name, dict(nodes=nodes, edges=report.get("edges", [])), report)
        doc["base"]["generation_diagnostics"] = self.file_hash(manifest_path)
        self.prepare_draft(name)
        value = self.generation_diagnostics(name)
        return dict(counts=value["counts"], draft_revision=doc["revision"],
                    accepted_node_ids=[n["id"] for n in nodes],
                    diagnostic_manifest=str(manifest_path), requires_native_validation=True,
                    note="Akzeptiertes Netz als Entwurf übernommen; fehlende Bodenbereiche und verworfene Scannerproben bleiben separat erhalten.")

    def generation_diagnostics(self, name):
        path = self.work / (name + ".generation-diagnostics.json")
        manifest = read_json(path, {})
        if not manifest or manifest.get("map") != name:
            return None
        archive = Path(manifest.get("native_report", ""))
        # A project's manifest cannot make another project's arbitrary file a
        # current diagnosis. Read only our hash-bound scanner archive.
        root = (self.work / "generation-scans").resolve()
        if root not in archive.resolve().parents or self.file_hash(archive) != manifest.get("native_report_sha256"):
            return dict(source="automatic_spatial_scanner", current=False, invalid=True,
                        note="Gesicherte Scannerdiagnose fehlt oder wurde verändert.")
        report = read_json(archive, {})
        origin = manifest["movement_profile"]["origin_above_feet"]
        reference = []
        for source in report.get("nodes", []):
            p = source.get("position", source.get("raw_position"))
            reference.append(dict(source, position=[p[0], p[1], p[2]-origin],
                diagnostic_kind=("accepted_network" if source.get("accepted") else "missing_floor_area"
                                 if source.get("native_input", {}).get("floor_accepted") is True else "geometry_rejection")))
        by_id = {n["id"]: n for n in reference}
        rejected = [n for n in reference if n.get("accepted") is not True]
        edges = [dict(e, from_position=by_id[e["from"]]["position"], to_position=by_id[e["to"]]["position"])
                 for e in report.get("edges", []) if e.get("accepted") is not True]
        doc = self.drafts.get(name)
        current = bool(doc and doc["base"].get("generation_diagnostics") == self.file_hash(path) and
                       manifest["environment"] == self.draft_environment(name))
        # Reconcile exact floor samples with the latest actually checked network,
        # never with a visual edit or mere nearest-node distance. The original
        # scanner record stays intact, including geometry failures and IDs.
        checked = self.native(name) if current and self.report_status(name)["current"] else None
        exits, entries, locations = set(), set(), {}
        if checked and checked.get("generation_complete") is True:
            for edge in checked.get("edges", []):
                if edge.get("accepted") is True:
                    exits.add(edge["from"]); entries.add(edge["to"])
            checked_origin = checked.get("bbox", {}).get("origin_above_feet", origin)
            for node in checked.get("nodes", []):
                if node.get("accepted") is not True or node["id"] not in exits & entries:
                    continue
                p = node["position"]
                feet = [p[0], p[1], p[2]-checked_origin]
                bucket = tuple(math.floor(v/.02) for v in feet)
                locations.setdefault(bucket, []).append((node["id"], feet))
        for node in rejected:
            if node["diagnostic_kind"] != "missing_floor_area":
                continue
            p = node["position"]; bucket = tuple(math.floor(v/.02) for v in p)
            matches = [identity for dx in (-1,0,1) for dy in (-1,0,1) for dz in (-1,0,1)
                       for identity, feet in locations.get((bucket[0]+dx, bucket[1]+dy, bucket[2]+dz), [])
                       if math.dist(p,feet) <= .02]
            node["diagnostic_resolved"] = bool(matches)
            if matches:
                node["resolved_by_source_ids"] = sorted(matches)
        counts = dict(scanner_nodes=len(reference), scanner_directed_edges=len(report.get("edges", [])),
                      accepted_nodes=len(reference)-len(rejected),
                      accepted_directed_edges=sum(e.get("accepted") is True for e in report.get("edges", [])),
                      geometry_rejected_nodes=sum(n["diagnostic_kind"] == "geometry_rejection" for n in rejected),
                      connectivity_pending_nodes=sum(n["diagnostic_kind"] == "missing_floor_area" for n in rejected),
                      rejected_directed_edges=len(edges))
        scanner_counts = dict(counts)
        counts["connectivity_resolved_nodes"] = sum(n.get("diagnostic_resolved") is True for n in rejected)
        counts["connectivity_pending_nodes"] -= counts["connectivity_resolved_nodes"]
        return dict(source="automatic_spatial_scanner", current=current, counts=counts, scanner_counts=scanner_counts, nodes=rejected,
                    edges=edges, reference_nodes=reference, candidate=manifest["candidate"],
                    native_report_sha256=manifest["native_report_sha256"],
                    native_input_coverage=report.get("native_input_coverage", {}),
                    diagnostic_manifest=str(path), full_map_verified=False, acceptance_certificate=False,
                    note="Scannerdiagnose, getrennt vom bearbeiteten Netz. Fehlende Bodenbereiche sind weiterhin offen; keine vollständige Kartenfreigabe.")

    def state(self):
        self.assets.refresh()
        game = self.game_state()
        with self.lock:
            maps = []
            for name, asset in sorted(self.assets.maps.items()):
                if not self.map_in_mode(name): continue
                maps.append({"name": name, "has_bsp": True, "source": str(asset["path"]),
                             "generated": self.path(name, ".stand.json").is_file(),
                             "native_checked": self.native_path(name).is_file()})
            return {"app": APP_ID, "version": "1.0", "workspace": str(self.workspace),
                    "active_project": getattr(self, "active_project", None), "project_required": True,
                    "selected_map": self.selected_map, "token": self.token, "game_mode": getattr(self, "game_mode", "tce2"),
                    "busy": self.busy, "maps": maps, "jobs": [dict(job) for job in reversed(self.jobs)
                        if job.get("project_id") == ((getattr(self,"active_project",None) or {}).get("id"))],
                    "test_instance": str(self.test), "rcon_port": self.args.rcon_port,
                    "game": game,
                    "capabilities": {"actions": sorted(self.ACTIONS), "recast": bool(self.recast),
                                     "test_instance": self.mod.is_dir(), "bsp_viewer": True},
                    "asset_warnings": self.assets.errors[:8],
                    "note": "Eine verbundene Navigation ist keine vollständige Live-Abnahme der Karte."}

    def scene(self, name, meshes=True):
        name = self.require_map(name)
        self.selected_map = name
        geometry = read_json(self.path(name, ".geometry.json"), {})
        candidate_path = self.candidate(name)
        candidate = read_json(candidate_path.with_suffix(".json"), {}) if candidate_path else {}
        native = self.native(name)
        draft = self.drafts.view(name, self.draft_environment(name))
        freshness = self.report_status(name, candidate_path)
        current_native = native if freshness["current"] else None
        source_kind = "native" if current_native else "draft" if draft else "candidate" if candidate else "bsp"
        nodes, edges = [], []
        if current_native:
            protection = action_protection(native)
            for source in native.get("nodes", []):
                node = dict(source)
                node["generated_guide"] = bool(source.get("editor_role", 0) & 1) or bool(source.get("nav_flags", 0) & ((1<<9)|(1<<27)|(1<<8)|(1<<18))) or source["id"] in protection["generated_guide_ids"]
                node["locked"] = False
                node["action_roles"] = protection["action_roles"].get(source["id"], [])
                node["action_properties"] = protection["action_properties"].get(source["id"], {})
                node["original_position"] = source["position"]
                node["position"] = [source["position"][0], source["position"][1],
                                    source["position"][2] - native.get("bbox", {}).get("origin_above_feet", 30)]
                node["pos"] = node["position"]
                node["state"] = "accepted" if node.get("accepted") else "rejected"
                nodes.append(node)
            edges = native.get("edges", [])
        elif draft:
            nodes = [{**node, "position": node["position"], "pos": node["position"], "accepted": None,
                      "native_checked": False, "status": "candidate", "state": "candidate",
                      "reason": "draft_changed_requires_validation"} for node in draft["nodes"]]
            edges = [{**edge, "accepted": None, "native_checked": False, "status": "candidate",
                      "reason": "draft_changed_requires_validation"} for edge in draft["edges"]]
        elif candidate:
            nodes = [{**node, "position": node["point"], "pos": node["point"],
                      "accepted": None, "native_checked": False, "status": "candidate", "state": "candidate", "reason": "awaiting_native_validation"}
                     for node in candidate.get("nodes", [])]
            edges = [{"from": a, "to": b, "accepted": None, "native_checked": False, "status": "candidate", "reason": "candidate_not_native_validated"}
                     for a, b in candidate.get("edges", [])]
        else:
            imported = self.imported_navigation(name)
            if imported:
                nodes, edges = imported["nodes"], imported["edges"]
                source_kind = "imported_way"
        native_source = str(native.get("source", "")) if native else ""
        expected = candidate_path.name if candidate_path else ""
        proof = self.proof_status(name, candidate_path) if candidate_path else None
        stale = bool(native and not freshness["current"])
        stats = {"nodes": sum(bool(n.get("accepted")) for n in nodes),
                 "edges": sum(bool(e.get("accepted")) for e in edges),
                 "candidate_nodes": len(nodes), "rejected_nodes": sum(n.get("state") == "rejected" for n in nodes),
                 "native_stale": stale, "native_complete": bool(current_native and native.get("generation_complete")),
                 "quarantined": native.get("quarantined_unreachable_nodes", 0) if current_native else 0}
        hashes = {"candidate": candidate.get("candidate_sha256"),
                  "geometry_bsp": geometry.get("input", {}).get("bsp_sha256")}
        if current_native:
            hashes["native_report"] = digest(self.native_path(name))
        result = {"map": name, "bsp_url": "/api/bsp?map=" + quote(name),
                  "collision_url": "/api/collision?map=" + quote(name),
                  "meshes": {}, "geometry": geometry, "source_hashes": hashes,
                  "proof_status": proof, "report_status": freshness, "draft": draft,
                  "navigation": {"coordinate_system": "TCE_Z_up", "nodes": nodes, "edges": edges,
                                 "source": source_kind,
                                 "stats": stats, "candidate_source": expected,
                                 "native_source": native_source},
                  "status": "manual_draft" if draft and not current_native else "native_stale" if stale else (
                            "native_checked_not_live_certified" if current_native and native.get("generation_complete") else
                            "native_failed" if current_native else "candidate_not_native_validated" if candidate or source_kind=="imported_way" else "bsp_only"),
                  "audit_state": "current" if freshness["current"] else "stale" if native else "missing",
                  "audit": self.current_audit(name, freshness),
                  "native_summary": {key: value for key, value in (current_native or {}).items() if key not in ("nodes", "edges")}}
        result["generation_diagnostics"] = self.generation_diagnostics(name)
        result["spawns"] = self.spawn_layer(name)
        result['gametype_availability']=self.available_game_types(name)
        result['package_status']={k:v for k,v in self.package_status(name).items()if k!='receipt'}
        if meshes:
            result["meshes"] = {kind: read_json(self.path(name, "." + kind + ".json")) for kind in ("stand", "duck")}
        return result

    def draft_environment(self, name):
        return {"bsp": self.bsp_hash(name), "stand": self.file_hash(self.path(name, ".stand.json")),
                "duck": self.file_hash(self.path(name, ".duck.json")),
                "geometry": self.file_hash(self.path(name, ".geometry.json")),
                "collision": self.file_hash(self.path(name, ".collision.obj")),
                "runtime_snapshot": self.file_hash(self.snapshot_path(name))}

    def edit(self, payload):
        name = self.require_map(payload.get("map"))
        with self.lock:
            if self.busy:
                raise RuntimeError("Bearbeiten erst nach Abschluss des laufenden Auftrags.")
            environment = self.draft_environment(name)
            operation = payload.get("op")
            if operation == "begin" and not self.drafts.get(name):
                scene = self.scene(name, meshes=False)
                normalized = self.normalize_generated_graph(dict(map=name))
                if not normalized:
                    self.begin_navigation_draft(name, scene["navigation"], self.native(name))
            elif operation == "begin":
                pass  # Opening an existing editor cannot change its graph/check.
            elif operation == "save":
                self.drafts.save(name, payload.get("revision"), environment)
            elif operation == "export":
                self.drafts.export_candidate(name, payload.get("revision"), environment, self.preview)
            elif operation == "restore":
                self.drafts.restore_latest(name)
            elif operation == "archive":
                self.drafts.archive(name, payload.get("revision"))
            else:
                self.drafts.change(name, payload, environment, identity_provider=lambda:self.native_edit_identities(name))
            return {"draft": self.drafts.view(name, environment),
                    "note": "Lokaler Entwurf; WAY-Dateien und laufendes Spiel bleiben unverändert. Neue native Prüfung vor Installation erforderlich."}

    def collision(self, name):
        name = self.require_map(name)
        path = self.path(name, ".collision.obj")
        if not path.is_file():
            raise FileNotFoundError("Kollisionsgeometrie wurde noch nicht erzeugt.")
        if path.stat().st_size > 100 * 1024 * 1024:
            raise ValueError("Kollisionsvorschau überschreitet das 100-MB-Limit.")
        vertices, triangles = [], []
        with path.open(encoding="ascii") as stream:
            for line in stream:
                fields = line.split()
                if fields and fields[0] == "v":
                    x, y, z = map(float, fields[1:4])
                    vertices.append([x, -z, y])
                elif fields and fields[0] == "f":
                    triangles.append([int(x) - 1 for x in fields[1:4]])
        return {"coordinate_system": "TCE_Z_up", "vertices": vertices, "triangles": triangles,
                "status": "collision_geometry_candidate_native_gate_required"}

    def manifest(self, name):
        name = self.require_map(name)
        result = {}
        # Prefer JPG/PNG to TGA when an equivalent texture is available.
        priority = {".tga": 0, ".jpg": 1, ".jpeg": 1, ".png": 2, ".webp": 2}
        with self.assets.lock:
            names = sorted(self.assets.entries, key=lambda p: priority.get(Path(p).suffix, -1))
            for asset in names:
                if Path(asset).suffix in priority:
                    url = "/api/texture?map=" + quote(name) + "&name=" + quote(asset, safe="/")
                    result[str(PurePosixPath(asset).with_suffix(""))] = url
        return result

    def shaders(self, name):
        name = self.require_map(name)
        map_archive = self.assets.maps[name]["path"]
        base, specific = [], []
        with self.assets.lock:
            for asset, record in sorted(self.assets.entries.items()):
                if asset.endswith(".shader"):
                    url = "/api/shader?map=" + quote(name) + "&name=" + quote(asset, safe="/")
                    (specific if record["path"] == map_archive else base).append(url)
        return {"base": base, "maps": {name: specific}}

    def submit(self, payload):
        action = payload.get("action")
        if action not in self.ACTIONS:
            raise ValueError("Unbekannte Aktion.")
        gametype = payload.get('gametype', 2)
        if type(gametype) is not int or gametype not in (2,5,7): raise ValueError('Gametype must be 2, 5 or 7.')
        if getattr(self, 'game_mode', 'tce2') == 'et': gametype = 2
        name = (self.selected_map if action in {"mode_tce2", "mode_et"} else self.require_map(payload.get("map")))
        if self.game_mode=='tce2'and action not in{'stop_test','mode_tce2','mode_et','inspect'}:
            mapscript.require_gametype(self.assets,name,gametype,self.script_directory())
        if getattr(self,"game_mode","tce2")=="et" and action=="install_production":
            raise RuntimeError("ET-Projekte in die vorhandene ET-Testinstanz übernehmen; TCE2-Produktion ist ein anderer Mod.")
        if action == "install_production" and payload.get("confirm") != "INSTALL":
            raise ValueError("Produktionsinstallation benötigt die ausdrückliche Bestätigung INSTALL.")
        with self.lock:
            if self.busy:
                raise RuntimeError("Eine Aktion läuft bereits. Diese Instanz verarbeitet immer nur einen Auftrag.")
            if hasattr(self, "projects") and not self.active_project and action not in {"stop_test", "mode_tce2", "mode_et"}:
                raise RuntimeError("Zuerst ein neues Projekt erstellen oder ein vorhandenes Projekt öffnen.")
            if action in {"rebuild", "snapshot"} and self.drafts.get(name):
                raise RuntimeError("Bearbeitungsentwurf zuerst sichern und archivieren, bevor seine Geometriegrundlage neu erzeugt wird.")
            job = {"id": secrets.token_hex(8), "action": action, "map": name,
                   "project_id": (getattr(self,"active_project",None) or {}).get("id"),
                   "allow_test_subset": payload.get("allow_test_subset") is True,
                   "gametype": gametype, "status": "queued", "progress": "Wartet", "started": None,
                   "finished": None, "result": None, "error": None, "log": ""}
            if action in {"native_check", "repair", "repair_check", "install_test", "install_production"}:
                candidate = self.candidate(name)
                job["candidate_at_submit"] = self.candidate_identity(candidate)
            self.jobs.appendleft(job)
            self.busy = True
            self.executor.submit(self.run_job, job)
            return dict(job)

    def update(self, job, **fields):
        with self.lock:
            job.update(fields)

    def run_job(self, job):
        self.update(job, status="running", started=time.time())
        outcome={}
        try:
            if "candidate_at_submit" in job and job["candidate_at_submit"] != self.candidate_identity(self.candidate(job["map"])):
                raise RuntimeError("Der ausgewählte Kandidat wurde seit dem Auftrag verändert. Den aktuellen Stand prüfen und die Aktion neu wählen.")
            result = getattr(self, "action_" + job["action"])(job)
            outcome=dict(status="complete",progress="Abgeschlossen",result=result)
        except Exception as exc:
            outcome=dict(status="failed",progress="Fehlgeschlagen",error=str(exc))
        finally:
            if job.get("quiet_generation"):
                self.update(job,progress="Ursprüngliche Bots und Autobalance wiederherstellen")
                try:
                    self.restore_generation_bots(job)
                except Exception as exc:
                    outcome.update(status="failed",progress="Botwiederherstellung nicht bestätigt",
                                   error=((outcome.get("error") or "") + " Botwiederherstellung: " + str(exc)).strip())
            with self.lock:
                # A terminal status promises that restoration and serialized
                # cleanup are finished. Publish it together with busy=false,
                # so the following install action cannot race this finally.
                job.update(outcome,finished=time.time())
                try:
                    (self.work / "latest-job.json").write_text(json.dumps(job,indent=2,ensure_ascii=False),encoding="utf8")
                finally:
                    self.busy=False
                    self.game_state_cache=None

    def candidate_identity(self, path):
        return None if path is None else {"name": path.name, "sha256": self.file_hash(path),
                                          "metadata": self.file_hash(path.with_suffix(".json"))}

    def test_processes(self, executable=None):
        """Read only the exact existing test executable and its UDP ownership."""
        if os.name != "nt":
            return []
        ps = Path(os.environ.get("SystemRoot", "C:/Windows")) / "System32/WindowsPowerShell/v1.0/powershell.exe"
        query = ("$ErrorActionPreference='Stop'; $navExe=$env:TCE_NAVSTUDIO_EXE; "
                 "$navPorts=@(Get-NetUDPEndpoint -LocalPort ([int]$env:TCE_NAVSTUDIO_PORT) -ErrorAction SilentlyContinue); "
                 "$navItems=@(Get-Process -Name ([IO.Path]::GetFileNameWithoutExtension($navExe)) -ErrorAction SilentlyContinue | Where-Object {$_.Path -eq $navExe} | ForEach-Object { "
                 "$navInfo=Get-CimInstance Win32_Process -Filter ('ProcessId = '+$_.Id); "
                 "$navDedicated=$null; if($navInfo.CommandLine -match '(?i)\\+set\\s+dedicated\\s+[\"'']?([012])') {$navDedicated=[int]$Matches[1]}; "
                 "[pscustomobject]@{pid=$_.Id; dedicated=$navDedicated; window=[long]$_.MainWindowHandle; "
                 "owns_port=([bool]($navPorts | Where-Object {$_.OwningProcess -eq $navInfo.ProcessId}))} }); "
                 "ConvertTo-Json -InputObject $navItems -Compress")
        try:
            result = subprocess.run([str(ps), "-NoProfile", "-NonInteractive", "-Command", query],
                                    env=dict(os.environ, TCE_NAVSTUDIO_EXE=str(executable or self.game_executable),
                                             TCE_NAVSTUDIO_PORT=str(self.args.rcon_port)),
                                    stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    timeout=20, creationflags=subprocess.CREATE_NO_WINDOW)
        except subprocess.TimeoutExpired as exc:
            # CIM/UDP discovery may be slow while native tools are compiling.
            # An unknown owner must never authorize a second game process.
            raise RuntimeError("Windows antwortet bei der Prozess-/Portprüfung nicht rechtzeitig. "
                               "Es wurde kein weiteres Testspiel gestartet; kurz warten und erneut versuchen.") from exc
        if result.returncode:
            raise RuntimeError("Prozess-/Portzuordnung der bestehenden Testinstanz konnte nicht gelesen werden.")
        return json.loads(result.stdout.decode("utf-8-sig") or "[]")

    def game_state(self):
        """A cached, read-only status packet; never launch or issue RCON from polling."""
        with self.game_state_lock:
            if self.game_state_cache and time.monotonic() - self.game_state_cache[0] < 5:
                return dict(self.game_state_cache[1])
            value = {"state": "offline", "map": None, "pid": None, "visible": False,
                     "input_note": "Das Testspiel ist jederzeit normal bedienbar."}
            try:
                processes = self.test_processes()
                owners = [p for p in processes if p.get("owns_port")]
                value["processes"] = len(processes)
                if processes:
                    value["state"] = "starting"
                if len(processes) > 1:
                    value.update(state="conflict", note="Mehrere Prozesse dieser Testinstanz; kein automatischer weiterer Start.")
                elif len(processes) == 1:
                    process = processes[0]
                    value.update(pid=process["pid"], visible=bool(process.get("window")),
                                 mode="dedicated" if process.get("dedicated") in (1, 2) else "listen")
                    if owners and getattr(self, "busy", False):
                        value.update(state="running", note="Spielprozess und Port aktiv; Prüfung läuft.")
                    if owners and not getattr(self, "busy", False):
                        reply = self.udp_exchange(b"\xff\xff\xff\xffgetstatus\n", .3)
                        match = re.search(r"\\mapname\\([^\\\r\n]+)", reply)
                        if match:
                            value.update(state="running", map=match[1])
                if value["state"] == "offline":
                    value["note"] = "Spiel geschlossen. Map laden startet die vorhandene Instanz sichtbar. Die BSP-Ansicht funktioniert offline."
            except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as exc:
                value.update(state="unavailable", note=str(exc))
            self.game_state_cache = (time.monotonic(), value)
            return dict(value)

    def worker(self, name, *args):
        path = worker_path(name, self.resources)
        if getattr(sys, "frozen", False):
            return [sys.executable, "--worker", name, *(str(a) for a in args)]
        return [sys.executable, str(path), *(str(a) for a in args)]

    def command(self, job, command, label, allowed=(0,), timeout=300):
        self.renew_generation_hold(job)
        self.update(job, progress=label)
        completed = subprocess.run(command, cwd=self.workspace, stdin=subprocess.DEVNULL,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=timeout,
                                   creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        output = completed.stdout.decode("utf-8", errors="replace")
        self.update(job, log=(job.get("log", "") + "\n" + label + "\n" + output)[-250000:])
        if completed.returncode not in allowed:
            raise CommandFailure(label, completed.returncode, output)
        return output

    def connection_error(self, detail, sent=False):
        note = (" Der Befehl wurde einmal gesendet; seine Ausführung ist nicht bestätigt. "
                "Er wurde nicht automatisch wiederholt." if sent else "")
        return GameConnectionError(
            f"Die vorhandene TCE2-Testinstanz auf 127.0.0.1:{self.args.rcon_port} antwortet nicht "
            f"({detail}). Die Instanz unter {self.test} starten bzw. ihren Neustart abwarten; "
            "danach die gewünschte Aktion erneut wählen. BSP, Recast und vorhandene Wegpunkte "
            "lassen sich auch ohne laufendes Spiel ansehen." + note, sent)

    def rcon_interval(self):
        executable=getattr(self,'game_executable',None)
        if executable is None:return 2.1
        try:return rcon_min_interval(self.file_hash(Path(executable)))
        except(OSError,ValueError,TypeError):return 2.1

    def udp_exchange(self, packet, timeout, drain=False):
        # Serialize public polling and authenticated commands on the same
        # engine endpoint. Concurrent browser requests must not burst UDP.
        if not hasattr(self, "transport_lock"):
            self.transport_lock = threading.RLock()
            self.transport_last = 0.
        with self.transport_lock:
            delay = self.rcon_interval() - (time.monotonic() - self.transport_last)
            if delay > 0:
                time.sleep(delay)
            try:
                return self._udp_exchange_once(packet, timeout, drain)
            finally:
                self.transport_last = time.monotonic()

    def _udp_exchange_once(self, packet, timeout, drain=False):
        """One send, bounded receive. UDP ICMP resets are normal offline state on Windows."""
        chunks = []
        sent = False
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
                sock.settimeout(timeout)
                sock.sendto(packet, ("127.0.0.1", self.args.rcon_port))
                sent = True
                deadline = time.monotonic() + (4.0 if drain else timeout)
                while time.monotonic() < deadline:
                    sock.settimeout(min(timeout, max(.01, deadline - time.monotonic())))
                    try:
                        data, source = sock.recvfrom(65535)
                    except socket.timeout:
                        break
                    if source != ("127.0.0.1", self.args.rcon_port):
                        continue
                    chunks.append(data.decode("latin1"))
                    if not drain:
                        break
        except OSError as exc:
            if chunks:
                return "".join(chunks)
            raise self.connection_error("UDP-Verbindung geschlossen oder nicht erreichbar", sent) from exc
        return "".join(chunks)

    def rcon(self, command, read_only=False):
        # Only fixed action implementations call this function. No command
        # string or remote host can be supplied through the HTTP API.
        password = os.environ.get("TCE_NAVSTUDIO_RCON_PASSWORD", "navgen_local")
        packet = b"\xff\xff\xff\xffrcon " + password.encode("ascii") + b" " + command.encode("ascii") + b"\n"
        response = ""
        for attempt in range(4 if read_only else 1):
            delay = self.rcon_interval() - (time.monotonic() - self.rcon_last)
            if delay > 0:
                time.sleep(delay)
            try:
                response = self.udp_exchange(packet, .8, drain=True)
                if response or not read_only:
                    break
            except RuntimeError:
                if not read_only or attempt == 3:
                    raise
            finally:
                self.rcon_last = time.monotonic()
        if read_only and not response:
            raise self.connection_error("keine Antwort auf die Leseabfrage")
        response = response.replace("\xff\xff\xff\xffprint\n", "")
        if "Bad rconpassword" in response or "No rconpassword" in response:
            raise RuntimeError("RCON-Passwort der vorhandenen Testinstanz stimmt nicht.")
        return response

    def server_map(self):
        # Public getstatus packets may be suppressed by the engine's flood
        # protection even while authenticated RCON and the game work normally.
        # This is a fresh read, not cached state or a retry of a mutation.
        text = re.sub(r"\^[0-9]", "", self.rcon("mapname", read_only=True))
        match = re.search(r'\bmapname"?\s+is:\s*"([^"\r\n]+)"', text)
        if not match or match[1] == "nomap":
            raise RuntimeError("Das Testspiel hat noch keine Map geladen. Map auswählen und Testspiel starten.")
        return valid_map(match[1])

    def rcon_with_followup(self, command):
        # Map/import/snapshot have a separate authoritative result check.
        # A reset after sending is ambiguous: check that result, do not resend.
        try:
            return self.rcon(command)
        except GameConnectionError as exc:
            if not exc.sent:
                raise
            return "Transporthinweis: " + str(exc)

    def require_loaded(self, name):
        current = self.server_map()
        if current != name:
            raise RuntimeError(f"Testinstanz läuft auf {current}; zuerst {name} ausdrücklich in der Testinstanz laden.")

    def require_bbox(self, job):
        if getattr(self,"game_mode","tce2")=="et":
            profile=dict(MOVEMENT_PROFILES["et"])
            self.update(job,movement_profile=profile)
            return profile
        # Delta und Stadtrand schalten beim Weltstart absichtlich auf Profil0.
        # Den bestaetigten Laufzeitwert lesen, niemals fuer die Generierung umsetzen.
        response = self.rcon("g_newbbox", read_only=True)
        plain = re.sub(r"\^[0-9]", "", response)
        match = re.search(r'\bg_newbbox\b"?\s+is:\s*"?(-?\d+)', plain, re.IGNORECASE)
        self.update(job, log=(job["log"] + "\n" + response)[-250000:])
        if not match or match[1] not in ("0", "1"):
            raise RuntimeError("g_newbbox konnte nicht eindeutig als 0 oder 1 bestaetigt werden.")
        profile = dict(MOVEMENT_PROFILES[int(match[1])])
        self.update(job, movement_profile=profile, g_newbbox=int(match[1]))
        return profile

    def recast_body_profile(self, name):
        meshes = {kind: read_json(self.path(name, "." + kind + ".json"), {}).get("profile", {}) for kind in ("stand", "duck")}
        for profile in MOVEMENT_PROFILES.values():
            if all(all(meshes[kind].get(key) == value for key, value in
                       (("height", height), ("radius", profile["radius"]), ("step", profile["step"])))
                   for kind, height in (("stand", profile["standing_height"]), ("duck", profile["crouching_height"]))):
                return dict(profile)
        raise RuntimeError("Recast-Stand/Duck passen zu keinem gemeinsamen TCE-Koerperprofil; neu erzeugen.")

    def require_profile_inputs(self, name, profile, candidate=None):
        if self.recast_body_profile(name) != profile:
            raise RuntimeError("Recast-Stand/Duck passen nicht zum aktiven TCE-Koerperprofil; Kandidaten neu erzeugen.")
        if candidate is not None:
            metadata = read_json(candidate.with_suffix(".json"), {})
            if metadata.get("movement_profile") != profile:
                raise RuntimeError("Kandidat hat kein passendes Bewegungsprofil samt Spawn-Hoehenversatz; neu erzeugen.")

    def wait_fresh(self, path, before, timeout):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if path.is_file() and path.stat().st_mtime_ns > before:
                try:
                    value = read_json(path)
                    if isinstance(value, dict):
                        return value
                except (ValueError, OSError):
                    pass
            time.sleep(.4)
        raise RuntimeError("Kein neuer vollständiger Bericht von der Testinstanz: " + str(path.name))

    def snapshot(self, job):
        name = job["map"]
        self.require_loaded(name)
        profile = self.require_bbox(job)
        self.update(job, progress="Aktuelle Map-Modelle und Scriptzustände aus TCE2 lesen")
        path = self.engine_nav_root() / (name + "_entities.json")
        before = path.stat().st_mtime_ns if path.exists() else 0
        reply = self.rcon_with_followup("bot navstudio_snapshot" if getattr(self,"game_mode","tce2")=="et" else "tce_navgen_entities")
        self.update(job, log=(job["log"] + "\n" + reply)[-250000:])
        value = self.wait_fresh(path, before, 15)
        if value.get("map") != name:
            raise RuntimeError("Runtime-Snapshot gehört nicht zur ausgewählten Map.")
        self.require_loaded(name)
        if self.require_bbox(job) != profile:
            raise RuntimeError("Koerperprofil hat sich waehrend des Snapshots geaendert; Snapshot erneut aufnehmen.")
        destination = self.snapshot_path(name)
        if getattr(self,"game_mode","tce2")=="et":
            from etsnapshot import join_bsp
            value=join_bsp(self.assets.read("maps/"+name+".bsp",128*1024*1024),value)
            value["engine_snapshot_sha256"]=digest(path)
            destination.parent.mkdir(parents=True,exist_ok=True)
            temporary=destination.with_suffix(".json.tmp")
            temporary.write_text(json.dumps(value,indent=2),encoding="utf8");temporary.replace(destination)
        else:
            self.atomic_copy(path, destination)
        # Nur die Rueckgabe ergaenzen; den von der Engine exportierten Snapshot nicht umschreiben.
        return destination, {**value, "movement_profile": profile}

    @staticmethod
    def collision_snapshot_hash(value, included=None):
        keys=("entity","modelindex","classname","linked","contents","stationary","pos_trtype","apos_trtype",
              "takedamage","navigation_ground_kind","origin","angles","mins","maxs",
              "collision_state_observed","state_source")
        models=[]
        for model in value.get("models",[]):
            if included is not None and (model.get("entity"),model.get("modelindex")) not in included:continue
            state={key:model[key] for key in keys if key in model}
            if "health" in model:state["health_collision_alive"]=not model.get("takedamage") or model["health"]>0
            models.append(state)
        models.sort(key=lambda model:(model.get("modelindex",0),model.get("entity",0)))
        payload=dict(map=value.get("map"),models=models)
        return hashlib.sha256(json.dumps(payload,sort_keys=True,separators=(",",":"),allow_nan=False).encode()).hexdigest()

    def verify_collision_snapshot(self, job, initial):
        """One fresh observation detects scripts/movers/damage during offline work.

        The original project snapshot remains the source of the geometry. A
        changed observation never silently becomes the evidence for that mesh.
        """
        name=job["map"]; path=self.engine_nav_root()/(name+"_entities.json")
        before=path.stat().st_mtime_ns if path.is_file() else 0
        self.update(job,progress="Prüfen, ob Mapmodelle während Recast unverändert geblieben sind")
        self.rcon_with_followup("bot navstudio_snapshot" if getattr(self,"game_mode","tce2")=="et" else "tce_navgen_entities")
        current=self.wait_fresh(path,before,15)
        if getattr(self,"game_mode","tce2")=="et":
            from etsnapshot import join_bsp
            current=join_bsp(self.assets.read("maps/"+name+".bsp",128*1024*1024),current)
        geometry=read_json(self.path(name,".geometry.json"),{})
        included={(model.get("entity"),model.get("modelindex",model.get("model"))) for model in geometry.get("inline_models",[])
                  if model.get("enabled") and model.get("triangles",0)>0 and not model.get("dynamic_obstacle")}
        initial_hash,current_hash=self.collision_snapshot_hash(initial,included),self.collision_snapshot_hash(current,included)
        evidence=dict(map=name,initial=initial_hash,current=current_hash,unchanged=initial_hash==current_hash,
                      includes_pose_linked_contents_health_liveness=True,static_geometry_models=len(included),
                      dynamic_obstacles_checked_by_current_native_hulls=True,native_body_validation_required=True)
        self.path(name,".collision-stability.json").write_text(json.dumps(evidence,indent=2),encoding="utf8")
        if not evidence["unchanged"]:
            raise RuntimeError("Mapkollision wurde während der Generierung geändert (Modellpose, Zustand oder Gesundheit). "
                               "Kein alter Snapshot wird freigegeben; mit ruhender Map erneut generieren.")
        self.require_loaded(name)
        return evidence

    def own_test_process(self, required=False, require_port=True):
        processes = self.test_processes()
        if len(processes) > 1:
            raise RuntimeError("Mehrere Prozesse der bestehenden Testinstanz laufen. Es wird keine weitere gestartet.")
        if not processes:
            if required:
                raise RuntimeError("Die sichtbare Testinstanz läuft nicht; zuerst Map in Testinstanz laden wählen.")
            return None
        process = processes[0]
        if require_port and not process.get("owns_port"):
            raise RuntimeError("Eigener Testprozess läuft, hat den Testport aber noch nicht geöffnet. Start abwarten; kein zweiter Prozess wird gestartet.")
        return process

    def quiet_generation(self, job):
        """Freeze the exact owned map's bot population while geometry is read."""
        if job.get("quiet_generation"):
            return
        self.require_loaded(job["map"])
        process = self.own_test_process(required=True)
        api = getattr(self, "process_api", None) or Windows()
        identity = api.identity(process["pid"])
        if not identity or identity[0] != os.path.normcase(str(self.game_executable.resolve())):
            raise ProcessIdentityChanged("Generierung verweigert: Testprozessidentität ist ungültig.")
        journal=(getattr(self,"test",None) or self.work)/"navstudio-quiet-session.json"
        previous=read_json(journal,{})
        if previous:
            if previous.get("pid")==process["pid"] and previous.get("identity")==list(identity):
                recovery=dict(map=previous["map"],log=job.get("log",""),quiet_generation=previous)
                self.restore_generation_bots(recovery)
                job["quiet_generation_recovered_previous_session"]=True
            else:
                # A dead process's GM table no longer exists. Do not apply its
                # settings/names to another PID or reused process identity.
                journal.unlink(missing_ok=True)
        token = secrets.token_hex(8)
        state = dict(token=token, pid=process["pid"], identity=list(identity), map=job["map"])
        # Record before dispatch: UDP silence after a sent command is ambiguous.
        job["quiet_generation"] = state
        temporary=journal.with_suffix(".json.tmp")
        temporary.write_text(json.dumps(state,indent=2),encoding="utf8");temporary.replace(journal)
        self.update(job, progress="Bots und Autobalance für unveränderte Kollisionsaufnahme pausieren")
        if getattr(self,"game_mode","tce2")=="tce2":
            # Keep the VM owning saved names/config intact across warmup.
            # This holds round transitions only; all physical entities move.
            state["authoring_hold"]=True
            temporary.write_text(json.dumps(state,indent=2),encoding="utf8");temporary.replace(journal)
            hold=self.rcon_with_followup("tce_navgen_authoring on")
            self.update(job,log=(job.get("log","")+"\n"+hold)[-250000:])
            if "NAVSTUDIO_AUTHORING_ON" not in hold:
                raise RuntimeError("TCE2 authoring hold was not confirmed; no geometry snapshot.")
            state["hold_confirmed_at"]=time.monotonic()
        else:
            deadline=time.monotonic()+35
            while time.monotonic()<deadline:
                response=self.rcon("gamestate",read_only=True)
                if re.search(r'"gamestate"\s+is:\s*"0"',re.sub(r"\^[0-9]","",response)):
                    break
                time.sleep(.25)
            else:
                raise RuntimeError("ET has not reached playing state; wait for warmup before navigation checks.")
        reply = self.run_quiet_script(quiet.begin_script(token),state,"begin")
        self.update(job, log=(job.get("log", "") + "\n" + reply)[-250000:])
        if "NAVSTUDIO_QUIET_BUSY" in reply:
            if state.get("authoring_hold"):
                release=self.rcon_with_followup("tce_navgen_authoring off")
                if "NAVSTUDIO_AUTHORING_OFF" not in release:
                    raise RuntimeError("TCE2 authoring hold release was not confirmed after a busy pause.")
                state["authoring_hold"]=False
            job.pop("quiet_generation", None)
            journal.unlink(missing_ok=True)
            raise RuntimeError("Eine vorherige Generierung hat die Bots noch pausiert; ihre Wiederherstellung zuerst abschließen.")
        if "NAVSTUDIO_QUIET_OK" not in reply:
            raise RuntimeError("Omni-bot hat die temporäre Botpause nicht bestätigt; keine Geometrieaufnahme.")
        self.rcon_with_followup("bot kickall")
        # Bot removal finishes on the game thread. Read the VM population,
        # instead of assuming that a UDP send already made the world quiet.
        deadline = time.monotonic() + 12
        while time.monotonic() < deadline:
            reply = self.rcon(quiet.count_script(), read_only=True)
            match = re.search(r"NAVSTUDIO_QUIET_COUNT:(\d+)", reply)
            if match and int(match[1]) == 0:
                state["population_confirmed_zero"] = True
                self.require_loaded(job["map"])
                return
        raise RuntimeError("Botpause nicht vollständig: aktive Bots würden die Kollisionsgeometrie während der Generierung verändern.")

    def renew_generation_hold(self,job):
        state=job.get("quiet_generation")
        if not state or not state.get("authoring_hold") or time.monotonic()-state.get("hold_confirmed_at",0)<240:return
        process=self.own_test_process(required=True)
        api=getattr(self, "process_api", None) or Windows()
        if process["pid"]!=state["pid"] or list(api.identity(process["pid"]) or ())!=state["identity"]:
            raise ProcessIdentityChanged("Test process changed; authoring hold is not renewed on a foreign process.")
        self.require_loaded(state["map"])
        reply=self.rcon_with_followup("tce_navgen_authoring on")
        if "NAVSTUDIO_AUTHORING_ON" not in reply:raise RuntimeError("TCE2 authoring hold renewal was not confirmed.")
        state["hold_confirmed_at"]=time.monotonic()
        self.update(job,log=(job.get("log","")+"\n"+reply)[-250000:])

    def restore_generation_bots(self, job):
        state = job.get("quiet_generation")
        if not state:
            return
        process = self.own_test_process(required=True)
        api = getattr(self, "process_api", None) or Windows()
        if process["pid"] != state["pid"] or list(api.identity(process["pid"]) or ()) != state["identity"]:
            raise ProcessIdentityChanged("Eigener Testprozess wurde ersetzt; gesicherte Bots nicht auf einen anderen Prozess anwenden.")
        try:
            self.require_loaded(state["map"])
            reply = self.run_quiet_script(quiet.restore_script(state["token"]),state,"restore")
            self.update(job, log=(job.get("log", "") + "\n" + reply)[-250000:])
            restored="NAVSTUDIO_QUIET_RESTORED" in reply
            if not restored and "NAVSTUDIO_QUIET_NOT_ACTIVE" not in reply:
                raise RuntimeError("Omni-bot hat die Wiederherstellung der Namen, Klassen, Teams und Autobalance nicht bestätigt.")
        finally:
            if state.get("authoring_hold"):
                release=self.rcon_with_followup("tce_navgen_authoring off")
                self.update(job,log=(job.get("log","")+"\n"+release)[-250000:])
                if "NAVSTUDIO_AUTHORING_OFF" not in release:
                    raise RuntimeError("TCE2 authoring hold release was not confirmed (automatic timeout: 600 seconds).")
                state["authoring_hold"]=False
        job["quiet_generation_result"] = dict(restored=restored,no_active_pause=not restored,
                                             population_was_zero=state.get("population_confirmed_zero", False))
        job.pop("quiet_generation", None)
        journal=(getattr(self,"test",None) or self.work)/"navstudio-quiet-session.json"
        journal.unlink(missing_ok=True)
        root=(getattr(self,"test",None) or self.work)/"navgen"
        for stage in ("begin","restore"):
            (root/("navstudio-quiet-"+state["token"]+"-"+stage+".gm")).unlink(missing_ok=True)

    def run_quiet_script(self, command, state, stage):
        # Original Omni-bot truncates every console argument at 127 chars.
        # Execute the exact source through its existing homepath mount, never
        # depend on quote/token reconstruction of a long console snippet.
        root=(getattr(self,"test",None) or self.work)/"navgen"
        root.mkdir(parents=True,exist_ok=True)
        path=root/("navstudio-quiet-"+state["token"]+"-"+stage+".gm")
        path.write_text(quiet.file_source(command),encoding="ascii")
        # Retain during an ambiguous transport result; the engine may still
        # consume the command. Confirmed restoration removes both tiny files.
        return self.rcon_with_followup("bot script_runfile homepath/navgen/"+path.name)

    def stage_map(self, name):
        asset = self.assets.maps[name]
        try:
            asset["path"].relative_to(self.test)
            return None
        except ValueError:
            pass
        destination = self.test / "etmain"
        destination /= ("maps/" + name + ".bsp") if asset["member"] is None else ("navstudio_" + name + ".pk3")
        destination.parent.mkdir(parents=True, exist_ok=True)
        if destination.exists():
            if not os.path.samefile(asset["path"], destination) and digest(asset["path"]) != digest(destination):
                raise RuntimeError("Testinstanz enthält bereits ein anderes Paket unter " + destination.name)
            return None
        try:
            os.link(asset["path"], destination)
        except OSError:
            required = asset["path"].stat().st_size + 256 * 1024 * 1024
            if shutil.disk_usage(destination.parent).free < required:
                raise RuntimeError("Zu wenig Speicher für dieses Map-Paket; Hardlink war nicht möglich.")
            shutil.copy2(asset["path"], destination)
        return destination if asset["member"] is not None else None

    def stop_owned_test(self, job, process):
        current = self.own_test_process(required=True, require_port=False)
        if current["pid"] != process["pid"]:
            raise RuntimeError("Testprozess wurde zwischenzeitlich ersetzt; Beenden abgebrochen.")
        api = getattr(self, "process_api", None) or Windows()
        identity = api.identity(process["pid"])
        if not identity or identity[0] != os.path.normcase(str(self.game_executable.resolve())):
            raise ProcessIdentityChanged("Beenden verweigert: Prozess gehört nicht zur eingestellten Testinstanz.")
        self.update(job, progress="Eigene Testinstanz kontrolliert beenden")
        if current.get("owns_port"):
            try:
                reply = self.rcon_with_followup("quit")
                self.update(job, log=(job.get("log", "") + "\n" + reply)[-250000:])
            except GameConnectionError as exc:
                self.update(job, log=(job.get("log", "") + "\n" + str(exc))[-250000:])
        deadline = time.monotonic() + (8 if current.get("owns_port") else 1)
        while time.monotonic() < deadline:
            if not self.test_processes():
                self.game_state_cache = None
                return
            time.sleep(.5)
        self.update(job, progress="Nicht reagierende eigene Testinstanz schließen")
        result = api.terminate(process["pid"], identity, expected_exe=self.game_executable)
        if not result.get("terminated") or self.test_processes():
            raise RuntimeError("Beenden der eigenen Testinstanz nicht bestätigt; kein weiterer Start.")
        self.game_state_cache = None

    def launch_visible(self, job):
        if self.mod.name == "omnibot":
            # Separate config uses only the original ET mod and its own bot library.
            (self.mod / "navstudio-base.cfg").write_text(
                'set sv_pure 0\nset g_gametype 2\nset g_doWarmup 0\nset g_warmup 0\n'
                'set omnibot_enable 1\nset bot_enable 1\nset logfile 2\n'
                'set rconPassword ' + os.environ.get("TCE_NAVSTUDIO_RCON_PASSWORD", "navgen_local") + '\n'
                'set omnibot_path "' + str(self.mod).replace('\\', '/') + '"\n'
                'set omnibot_render_distance 800\nset sv_master1 ""\nset sv_master2 ""\n', encoding="ascii")
        script = self.resources / "start-test.ps1"
        if os.name != "nt" or not script.is_file() or not self.game_executable.is_file():
            raise RuntimeError("Starter der bestehenden Windows-Testinstanz fehlt: " + str(script))
        if self.args.rcon_port != 27986:
            raise RuntimeError("Dieser Starter verwendet ausschließlich den vorhandenen Testport 27986.")
        if self.test_processes():
            raise RuntimeError("Eigener Testprozess ist bereits vorhanden. Es wird kein weiterer gestartet.")
        ps = Path(os.environ.get("SystemRoot", "C:/Windows")) / "System32/WindowsPowerShell/v1.0/powershell.exe"
        self.update(job, progress="Sichtbares Spiel in der vorhandenen Testinstanz starten (" + self.mod.name + ")")
        started = subprocess.run([str(ps), "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
                                  "-File", str(script), "-Instance", str(self.test), "-MapName", job["map"],
                                  "-GameExecutable", str(self.game_executable), "-GameMod", self.mod.name,
                                  "-GameType", str(job.get("gametype",2))],
                                 cwd=self.test, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, timeout=20, creationflags=subprocess.CREATE_NO_WINDOW)
        output = started.stdout.decode("utf-8", errors="replace")
        self.update(job, log=(job.get("log", "") + "\n" + output)[-250000:])
        if started.returncode:
            raise RuntimeError("Sichtbare Testinstanz konnte nicht gestartet werden: " + output[-2000:])
        # Record the one launched process before renderer/network readiness.
        launched = json.loads(output.strip().splitlines()[-1])
        job["test_pid"] = int(launched["pid"])
        # The game always retains normal player control.

    def map_load_failure(self,job):
        """A fresh owned launch's engine error is different from UDP startup."""
        after=job.get('map_load_log_after')
        if type(after)not in(int,float):return None
        path=self.mod/'etconsole.log'
        try:
            if path.stat().st_mtime<after:return None
            with path.open('rb')as stream:
                stream.seek(max(0,path.stat().st_size-131072));text=stream.read(131072).decode('utf8',errors='replace')
        except OSError:return None
        errors=list(re.finditer(r'(?m)^ERROR:[ \t]*([^\r\n]+)',text))
        if not errors:return None
        last=errors[-1]
        if '----- Server Shutdown -----'not in text[last.end():]:return None
        return dict(message=last[1].strip(),path=str(path),excerpt=text[max(0,last.start()-100):last.end()+500])

    def wait_visible_map(self, job, timeout=60):
        deadline = time.monotonic() + timeout
        last = "Spielprozess/Renderer noch nicht bereit."
        while time.monotonic() < deadline:
            ready = False
            try:
                process = self.own_test_process(required=True)
                if job.get("test_pid", process["pid"]) != process["pid"]:
                    raise ProcessIdentityChanged("Testprozess während Mapladen ersetzt; keine neue Instanz wird übernommen.")
                if process.get("dedicated") in (1, 2):
                    raise RuntimeError("Instanz läuft weiterhin ohne Renderer.")
                observed_map = self.server_map()
                ready = observed_map == job["map"] and process.get("window") and self.live_gametype() == job.get("gametype",2)
                last = ("Erwartete Map: " + job["map"] + "; geladene Map: " + str(observed_map) +
                        "; PID: " + str(process["pid"]) + "; sichtbares Fenster: " + str(bool(process.get("window"))))
            except ProcessIdentityChanged:
                raise
            except (OSError, ValueError, RuntimeError) as exc:
                # Read-only readiness probes may precede UDP/window startup.
                last = str(exc)
            if not ready:
                failure=self.map_load_failure(job)
                if failure:
                    self.update(job,log=(job.get('log','')+'\n'+failure['excerpt'])[-250000:])
                    raise RuntimeError('Spiel hat das Mapladen abgebrochen: '+failure['message']+'. Engineprotokoll: '+failure['path'])
            if ready:
                # Fixed test configuration runs once after map/Omni startup.
                # No window mutation, binding snapshot or input guard exists.
                (self.mod / "navstudio-waypoints.cfg").write_text(
                    "set cg_omnibotdrawing 1\nbot waypoint_view on\n"
                    "bind n noclip\nbind k kill\nbind ESCAPE togglemenu\nset in_mouse 1\n"
                    "set g_useralliedrespawntime 1\nset g_useraxisrespawntime 1\n"
                    "set timelimit 999\nset g_timelimit 999\nset g_redlimbotime 1000\nset g_bluelimbotime 1000\n", encoding="ascii")
                reply = self.rcon_with_followup("exec navstudio-waypoints.cfg")
                self.update(job, log=(job.get("log", "") + "\n" + reply)[-250000:])
                return {"loaded_map": job["map"], "pid": process["pid"], "visible": True,
                        "instance": str(self.test),
                        "note": "Sichtbares Testspiel läuft mit voller Bedienung. N: noclip, K: kill."}
            time.sleep(.5)
        raise RuntimeError("Mapladen nicht bestätigt. " + last + ". Kein zweiter Spielstart.")

    def action_load_map(self, job):
        process = self.own_test_process(require_port=False)
        staged = self.stage_map(job["map"])
        if process and (not process.get("owns_port") or process.get("dedicated") in (1, 2) or staged or self.server_map() != job["map"] or self.live_gametype() != job.get("gametype",2)):
            # Loading a map is explicit authorization to reuse this instance;
            # Restart this installation for a map change: classic ET caches PK3s
            # and its repeated VM reloads are unreliable. Never duplicate a game.
            self.stop_owned_test(job, process)
            process = None
        if process is None:
            job['map_load_log_after']=time.time()
            self.launch_visible(job)
        else:
            job["test_pid"] = process["pid"]
            current = self.server_map()
            if current != job["map"]:
                self.update(job, progress="Ausgewählte Map in derselben sichtbaren Testinstanz laden")
                reply = self.rcon_with_followup("devmap " + job["map"])
                self.update(job, log=(job.get("log", "") + "\n" + reply)[-250000:])
                if "can't find map" in reply.lower() or "vm_create" in reply.lower():
                    raise RuntimeError("Spiel konnte die Map nicht laden: " + reply.strip())
        return self.wait_visible_map(job)

    def action_start_test(self, job):
        return self.action_load_map(job)

    def action_stop_test(self, job):
        process = self.own_test_process(require_port=False)
        if process:
            self.stop_owned_test(job, process)
        return {"status": "stopped", "note": "Nur die bestehende Testinstanz wurde beendet."}

    def action_snapshot(self, job):
        path, value = self.snapshot(job)
        return {"snapshot": str(path), "sha256": digest(path), "entities": len(value.get("models", value.get("entities", []))),
                "movement_profile": value["movement_profile"]}

    def rebuild_recast(self, job, profile):
        """Frische Stand-/Duck-Dateien; genau ein begrenzter Speicher-Retry je Profil."""
        name = job["map"]
        collision = self.path(name, ".collision.obj")
        geometry = self.path(name, ".geometry.json")
        domain = recast_height_domain(read_json(geometry, {}), name, collision)
        run = self.work / "recast-builds" / (name + "-" + secrets.token_hex(8))
        run.mkdir(parents=True, exist_ok=False)
        evidence = run / "manifest.json"
        inputs = {"collision": {"path": str(collision), "sha256": digest(collision)},
                  "geometry": {"path": str(geometry), "sha256": digest(geometry)},
                  "compiler": {"path": str(self.recast), "sha256": digest(self.recast)}}
        report = {"schema": 1, "map": name, "movement_profile": profile, "inputs": inputs,
                  "height_domain": domain,
                  "started": time.time(), "status": "running", "attempts": [], "published": {}}
        self.update(job, recast_evidence=str(evidence))

        def save():
            evidence.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")

        def unchanged():
            if any(digest(Path(value["path"])) != value["sha256"] for value in inputs.values()):
                raise RuntimeError("Recast-Eingabe oder Compiler wurde während des Auftrags verändert.")

        save()
        selected = {}
        try:
            for kind, height in (("stand", profile["standing_height"]), ("duck", profile["crouching_height"])):
                for number in (1, 2):
                    unchanged()
                    target = run / (kind + ".attempt-" + str(number) + ".json")
                    log = target.with_suffix(".log")
                    command = [str(self.recast), str(collision), str(target), str(height), "4",
                               "--radius", str(profile["radius"]), "--height-domain",
                               str(domain["minimum"]), str(domain["maximum"])]
                    if number == 2:
                        command.extend(("--tile-size", "512"))
                    attempt = {"kind": kind, "attempt": number, "command": command, "output": str(target),
                               "log": str(log), "tile_cells": 512 if number == 2 else "auto", "status": "running"}
                    report["attempts"].append(attempt)
                    save()
                    label = "Recast: " + ("Stehflächen" if kind == "stand" else "Duckflächen")
                    if number == 2:
                        label += " (Speicher-Retry mit 512-Zellen-Kacheln)"
                    try:
                        output = self.command(job, command, label, timeout=300)
                    except Exception as exc:
                        output = getattr(exc, "output", None)
                        if isinstance(output, bytes):
                            output = output.decode("utf-8", errors="replace")
                        log.write_text(output if isinstance(output, str) else str(exc), encoding="utf-8")
                        attempt.update(status="failed", error=str(exc), returncode=getattr(exc, "returncode", None),
                                       log_sha256=digest(log), output_sha256=digest(target) if target.is_file() else None)
                        retry = number == 1 and recast_memory_failure(exc)
                        attempt["memory_retry"] = retry
                        save()
                        if retry:
                            continue
                        raise
                    log.write_text(output, encoding="utf-8")
                    attempt.update(status="built", returncode=0, log_sha256=digest(log))
                    unchanged()
                    mesh = read_json(target)
                    expected = {"height": height, "radius": profile["radius"], "step": profile["step"],
                                "cell": 4, "cell_height": 2}
                    if (not isinstance(mesh, dict) or mesh.get("schema") != 1 or
                            mesh.get("coordinate_system") != "TCE_Z_up" or
                            mesh.get("status") != "geometry_candidate_not_engine_validated" or
                            mesh.get("profile") != expected or
                            not isinstance(mesh.get("vertices"), list) or not isinstance(mesh.get("polygons"), list)):
                        raise RuntimeError("Frische Recast-Ausgabe fehlt oder hat ein falsches Format/Körperprofil.")
                    if number == 2 and (mesh.get("tiling", {}).get("enabled") is not True or
                                        mesh.get("tiling", {}).get("tile_cells") != 512):
                        raise RuntimeError("Recast hat die angeforderte 512-Zellen-Kachelung nicht bestätigt.")
                    used_domain = mesh.get("height_domain", {})
                    # Recast speichert Grenzen als float32, sein JSON mit zehn Ziffern.
                    def same_bound(key):
                        actual = used_domain.get("requested_" + key)
                        expected = struct.unpack("f", struct.pack("f", domain[key]))[0]
                        return type(actual) in (int, float) and math.isclose(actual, expected, rel_tol=1e-9, abs_tol=1e-6)
                    if (not same_bound("minimum") or not same_bound("maximum") or
                            used_domain.get("rule") != "original_triangles_intersected_by_raster_bounds_no_caps" or
                            used_domain.get("native_validation_required") is not True):
                        raise RuntimeError("Recast hat die angeforderte Höhendomäne ohne Deckflächen nicht bestätigt.")
                    attempt.update(status="validated", output_sha256=digest(target))
                    selected[kind] = target
                    save()
                    break
            # Erst wenn beide Profile gültig sind, bestehende Meshes ersetzen.
            unchanged()
            for kind, source in selected.items():
                destination = self.path(name, "." + kind + ".json")
                temporary = destination.with_name(destination.name + "." + secrets.token_hex(8) + ".tmp")
                try:
                    shutil.copyfile(source, temporary)
                    temporary.replace(destination)
                finally:
                    temporary.unlink(missing_ok=True)
                report["published"][kind] = {"path": str(destination), "sha256": digest(destination)}
            report["status"] = "complete"
        except Exception as exc:
            report.update(status="failed", error=str(exc))
            raise
        finally:
            report["finished"] = time.time()
            save()
        return str(evidence)

    def action_generate(self, job):
        """One bounded, serial workflow; feedback suggestions always get a new native proof."""
        stages = []
        # A previous successful project must never supply a new generation's
        # completion, even if this attempt stops before exporting a candidate.
        ready=self.work/(job['map']+'.generation-ready.json')
        if ready.exists():ready.unlink()
        store=getattr(self,"drafts",None)
        draft=store.get(job["map"]) if store else None
        if draft:
            # A user-requested regeneration starts a new graph. Keep the
            # authored graph as a recoverable archive without extra UI steps.
            self.update(job,progress="Bearbeiteten Stand vor Neuerzeugung sichern")
            store.save(job["map"],draft["revision"],draft["base"]["environment"])
            store.archive(job["map"],draft["revision"])
        self.action_load_map(job)
        self.quiet_generation(job)
        generated = self.action_rebuild(job)
        pending, checked = None, None

        def check(stage, authored=False):
            nonlocal pending, checked
            try:
                checked = self.action_native_check(job)
                pending = None
                stage.update(status="complete", proof=checked["proof"])
            except IncompleteNativeFeedback as error:
                pending, checked = error, None
                stage.update(status="incomplete_feedback_only", feedback=error.feedback, error=str(error))
            except RuntimeError as error:
                if not authored or not self.report_status(job["map"])["current"]:
                    raise
                pending, checked = error, None
                stage.update(status="authored_graph_requires_repair", error=str(error))
            stages.append(stage)
            self.update(job, workflow=stages)

        check({"stage": "generated_and_checked", "counts": generated["counts"]})
        for iteration in range(2):
            self.update(job, progress=f"Automatische Pfadnachbesserung {iteration + 1}/2", workflow=stages)
            repair = (self.action_repair(job, feedback=pending.feedback, scanner_only=True) if pending else
                      self.action_repair(job, scanner_only=True))
            # Even identical geometry has new metadata. Check that exact result,
            # then stop when the generator has no additional proposal.
            feedback = repair.get("repairs") or {}
            check({"stage": "repair_and_check", "iteration": iteration + 1,
                           "added_nodes": feedback.get("added_nodes", 0),
                           "added_directed_edges": feedback.get("added_directed_edges", 0)})
            if not feedback.get("added_nodes") and not feedback.get("added_directed_edges"):
                break
        normalized = self.normalize_generated_graph(job)
        if normalized:
            check({"stage": "accepted_network_draft", "normalization": normalized}, authored=True)
            # Generation and the Repair button must use the SAME posture,
            # anchor, directed-path and descent repair state machine. Its
            # helpers require an explicit repair context, including on rerun.
            original_action=job.get('action')
            try:
                self.update(job,action='repair_check',progress='Erzeugtes Netz automatisch vollständig reparieren')
                checked=self.action_repair_check(job,reconcile=False);pending=None
                stages.append(dict(stage='network_action_repair_and_check',status='complete',repair=checked['repair'],proof=checked['proof']))
            finally:
                if original_action is None:job.pop('action',None)
                else:self.update(job,action=original_action)
        self.update(job, workflow=stages)
        if pending:
            phase = "network_action_repair_and_check" if normalized else "repair_and_check"
            rounds = sum(stage["stage"] == phase for stage in stages)
            label = "Netz-/Aktionsreparaturen" if normalized else "Scanner-Reparaturrunden"
            raise RuntimeError(str(pending) + f" Nach {rounds}/2 begrenzten {label} keine Freigabe; "
                               "Restpunkte im aktuellen Netzbericht prüfen. Es wurde nichts installiert.")
        completed=self.complete_generated_project(job)
        if completed.get('objective_anchor_certificate'):
            checked=dict(audit=self.inspect(job),proof=read_json(self.work/(job['map']+'.native-proof.json')))
        return {"stages": stages, "audit": checked["audit"], "proof": checked["proof"], "installed": False,
                "objective_anchor_certificate":completed.get('objective_anchor_certificate'),
                "normalization": normalized,"completion":completed,
                "note": "Netz automatisch repariert und nativ geprüft; Goals für vorhandene Spieltypen und geprüfte Deathmatch-Spawns erstellt. ZIP-Export bereit. Botverhalten separat live testen."}

    def repair_rejection_context(self, name):
        inputs=self.proof_status(name,self.candidate(name),receipt=False)['inputs']
        return {key:value for key,value in inputs.items()if key not in('candidate','candidate_metadata')}

    def recover_rejected_shortcuts(self, job):
        """Withdraw only source-bound new automatic shortcuts, then require a full check."""
        from repair import rejected_shortcut_feedback
        name=job['map'];doc=self.drafts.get(name)
        receipt_path=self.path(name,'.repair-proposal.json');receipt=read_json(receipt_path,{})
        if not doc:return None
        if receipt.get('draft_revision')!=doc['revision'] or not doc.get('undo')or doc['undo'][-1].get('op')!='automatic_repair':
            return self.recover_historical_shortcuts(job)
        self.prepare_draft(name)
        if not self.report_status(name)['current']:
            try:self.action_native_check(job)
            except RuntimeError:
                if not self.report_status(name)['current']:raise
        if self.native(name).get('generation_complete')is not False:return None
        doc=self.drafts.get(name);candidate=self.candidate(name);context=self.repair_rejection_context(name)
        before=(graph_hash(doc),self.file_hash(self.native_path(name)),self.candidate_identity(candidate),self.file_hash(receipt_path))
        if receipt.get('proposal_graph_sha256'):
            if receipt['proposal_graph_sha256']!=before[0]or receipt.get('candidate')!=before[2]:return None
        else:
            # Older receipts have no proposal hash. Accept only their exact last
            # automatic patch, current full native attempt and unchanged prior WAY.
            proof=read_json(self.work/(name+'.native-proof.json'),{})
            if (receipt.get('source_native_report')!=proof.get('native_report')or
                {k:v for k,v in proof.get('inputs',{}).items()if k not in('candidate','candidate_metadata')}!=context or
                proof.get('way')!=self.file_hash(self.native_path(name).with_suffix('.way'))):return None
        source_native_bytes=None
        source_native_hash=receipt.get('source_native_report','')
        if re.fullmatch('[0-9a-f]{64}',source_native_hash):
            source_native_path=self.path(name,'.repair-source-'+source_native_hash[:12]+'.native.json')
            if source_native_path.is_file():source_native_bytes=source_native_path.read_bytes()
        rollback=rejected_shortcut_feedback(doc,self.native(name),receipt,source_native_bytes)
        if not rollback:return self.recover_historical_shortcuts(job)
        proposal,evidence=rollback
        if (not self.report_status(name)['current']or before!=(graph_hash(self.drafts.get(name)),
            self.file_hash(self.native_path(name)),self.candidate_identity(self.candidate(name)),self.file_hash(receipt_path))):
            raise RuntimeError('Reparaturgrundlage verändert; keine automatische Verbindung zurückgenommen.')
        current=self.drafts.apply_repair(name,doc['revision'],self.draft_environment(name),proposal)
        self.prepare_draft(name)
        memo_path=self.path(name,'.repair-rejections.json');memo=read_json(memo_path,{})
        previous=memo.get('rejected_edges',[])if memo.get('context')==context else[]
        entries={tuple(row['edge']):row for row in previous+evidence['rejected_edges']}
        memo=dict(schema=1,map=name,context=context,rejected_edges=list(entries.values())[-256:])
        memo_path.write_text(json.dumps(memo,indent=2),encoding='utf8')
        evidence.update(source_native_report=before[1],draft_revision=current['revision'])
        self.update(job,progress='Abgelehnte neue Automatikverbindungen zurücknehmen; erhaltenes Netz vollständig erneut prüfen')
        return evidence

    def archive_repair_proposal(self, name):
        """Keep literal receipts before a later posture/repair replaces one."""
        source=self.path(name,'.repair-proposal.json')
        if not source.is_file():return None
        raw=source.read_bytes();receipt=json.loads(raw)
        revision=receipt.get('draft_revision')
        if type(revision) is not int or revision<0:return None
        digest=hashlib.sha256(raw).hexdigest()
        target=self.path(name,f'.repair-proposal-r{revision}-{digest[:12]}.json')
        if target.exists() and target.read_bytes()!=raw:
            raise RuntimeError('Archivierter Reparaturbeleg enthält bereits andere Daten.')
        if not target.exists():target.write_bytes(raw)
        return dict(path=str(target),sha256=digest,revision=revision)

    def recover_historical_shortcuts(self, job):
        """Resume a rejected automatic addition without undoing later edits."""
        from repair import rejected_historical_shortcut_feedback
        name=job['map'];doc=self.drafts.get(name)
        if not doc or not doc.get('undo') or doc.get('redo'):return None
        directory=self.path(name,'.repair-proposal.json').parent
        archives=sorted(directory.glob(name+'.repair-source-*.native.json'),key=lambda p:p.stat().st_mtime_ns,reverse=True)[:32]
        if not archives:return None
        self.prepare_draft(name)
        if not self.report_status(name)['current']:
            try:self.action_native_check(job)
            except RuntimeError:
                if not self.report_status(name)['current']:raise
        if self.native(name).get('generation_complete') is not False:return None
        doc=self.drafts.get(name)
        before=(graph_hash(doc),self.file_hash(self.native_path(name)),self.candidate_identity(self.candidate(name)))
        sources={};paths={}
        # Legacy failed runs kept their full pre-proposal native report even
        # when the single receipt was subsequently overwritten. Its exact
        # graph is checked against the reconstructed automatic predecessor.
        for path in archives:
            raw=path.read_bytes();digest=hashlib.sha256(raw).hexdigest()
            if path.name!=name+'.repair-source-'+digest[:12]+'.native.json':continue
            sources[digest]=raw;paths[digest]=path
        result=rejected_historical_shortcut_feedback(doc,self.native(name),sources)
        if not result:return None
        proposal,evidence=result
        used={row['source_native_report'] for row in evidence['history_origins']}
        if (not self.report_status(name)['current'] or before!=(graph_hash(self.drafts.get(name)),
                self.file_hash(self.native_path(name)),self.candidate_identity(self.candidate(name))) or
                any(self.file_hash(paths[digest])!=digest for digest in used)):
            raise RuntimeError('Reparaturhistorie verändert; keine Automatikverbindung zurückgenommen.')
        self.archive_repair_proposal(name)
        current=self.drafts.apply_repair(name,doc['revision'],self.draft_environment(name),proposal)
        self.prepare_draft(name)
        context=self.repair_rejection_context(name)
        memo_path=self.path(name,'.repair-rejections.json');memo=read_json(memo_path,{})
        previous=memo.get('rejected_edges',[]) if memo.get('context')==context else []
        entries={tuple(row['edge']):row for row in previous+evidence['rejected_edges']}
        memo_path.write_text(json.dumps(dict(schema=1,map=name,context=context,
            rejected_edges=list(entries.values())[-256:]),indent=2),encoding='utf8')
        evidence.update(source_native_report=before[1],draft_revision=current['revision'],
            source_evidence=[dict(path=str(paths[digest]),sha256=digest) for digest in sorted(used)])
        self.path(name,'.historical-shortcut-withdrawal.json').write_text(json.dumps(evidence,indent=2),encoding='utf8')
        self.update(job,progress='Abgelehnte Automatiklinks aus belegter Historie zurücknehmen; aktuelle Punkte und Haltungen erhalten')
        return evidence

    def recover_rejected_ordinary(self, job):
        """Replace a rejected ordinary edge only by an already proved alternate."""
        from repair import rejected_ordinary_edge_feedback, ORDINARY_COLLISION_REJECTIONS
        name=job['map'];doc=self.drafts.get(name);native=self.native(name)
        reasons=ORDINARY_COLLISION_REJECTIONS
        if (not doc or native.get('generation_complete') is not False or
                not any(e.get('accepted') is False and e.get('reason') in reasons
                        for e in native.get('edges',[])) or not self.report_status(name)['current']):
            return None
        candidate=self.candidate(name);metadata=candidate.with_suffix('.json')
        if not candidate.is_file() or not metadata.is_file():return None
        before=(graph_hash(doc),self.file_hash(self.native_path(name)),self.candidate_identity(candidate))
        binding=dict(report_status=self.report_status(name),
                     attempt=read_json(self.work/(name+'.native-attempt.json'),{}),
                     inputs=self.proof_status(name,candidate,receipt=False)['inputs'],
                     metadata=read_json(metadata,{}),native_report_bytes=self.native_path(name).read_bytes(),
                     candidate_bytes=candidate.read_bytes(),candidate_metadata_bytes=metadata.read_bytes())
        automatic_files=[]
        receipt_path=self.path(name,'.repair-proposal.json')
        if receipt_path.is_file():
            raw=receipt_path.read_bytes()
            try:receipt=json.loads(raw)
            except(ValueError,TypeError):receipt={}
            digest=receipt.get('source_native_report','')
            if re.fullmatch('[0-9a-f]{64}',digest):
                source_path=self.path(name,'.repair-source-'+digest[:12]+'.native.json')
                if source_path.is_file():
                    source_raw=source_path.read_bytes();receipt_hash=hashlib.sha256(raw).hexdigest()
                    if hashlib.sha256(source_raw).hexdigest()==digest:
                        binding.update(automatic_shortcut_receipt_bytes=raw,automatic_shortcut_receipt_sha256=receipt_hash,
                            automatic_shortcut_source_native_bytes=source_raw,automatic_shortcut_source_native_sha256=digest)
                        automatic_files=[(receipt_path,receipt_hash),(source_path,digest)]
        result=rejected_ordinary_edge_feedback(doc,native,binding)
        if not result:return None
        proposal,evidence=result
        if (not self.report_status(name)['current'] or before!=(graph_hash(self.drafts.get(name)),
                self.file_hash(self.native_path(name)),self.candidate_identity(self.candidate(name))) or
                any(self.file_hash(path)!=digest for path,digest in automatic_files)):
            raise RuntimeError('Grundlage der Kantenreparatur verändert; keine Verbindung zurückgenommen.')
        # The following complete import replaces the failed report and exported
        # candidate. Keep their exact bytes so the edge-only repair remains
        # independently auditable after that successful import.
        evidence['source_evidence']={}
        archives=[('native_report_bytes','.native.json'),('candidate_bytes','.candidate.txt'),
                  ('candidate_metadata_bytes','.candidate.json')]
        if automatic_files:
            archives.extend((key,'.'+key+'-'+hashlib.sha256(binding[key]).hexdigest()[:12]+'.json')for key in
                ('automatic_shortcut_receipt_bytes','automatic_shortcut_source_native_bytes'))
        bundle_digest=repair_archive_bundle_digest(binding,archives)
        evidence['archive_bundle_sha256']=bundle_digest
        for key,suffix in archives:
            raw=binding[key]
            target=self.path(name,'.ordinary-withdrawal-'+bundle_digest[:12]+suffix)
            if target.exists() and target.read_bytes()!=raw:
                raise RuntimeError('Prüfbeleg der Kantenreparatur bereits mit anderem Inhalt vorhanden.')
            if not target.exists():target.write_bytes(raw)
            evidence['source_evidence'][key]=dict(path=str(target),sha256=self.file_hash(target))
        current=self.drafts.apply_repair(name,doc['revision'],self.draft_environment(name),proposal)
        self.prepare_draft(name)
        evidence.update(result_draft_revision=current['revision'],proposal_graph_sha256=graph_hash(current))
        suppressed=evidence.get('suppressed_automatic_shortcut_candidates',[])
        if suppressed:
            context=self.repair_rejection_context(name);memo_path=self.path(name,'.repair-rejections.json')
            memo=read_json(memo_path,{});previous=memo.get('rejected_edges',[])if memo.get('context')==context else[]
            entries={tuple(row['edge']):row for row in previous+suppressed}
            memo_path.write_text(json.dumps(dict(schema=1,map=name,context=context,
                rejected_edges=list(entries.values())[-256:]),indent=2),encoding='utf8')
        self.path(name,'.ordinary-edge-withdrawals.json').write_text(json.dumps(evidence,indent=2),encoding='utf8')
        self.update(job,progress='Nicht belegte Verbindung durch geprüften vorhandenen Umweg ersetzen; ganzes Netz erneut prüfen')
        return evidence

    def action_repair_check(self, job,reconcile=True):
        self.action_load_map(job)
        self.quiet_generation(job)
        from repair import native_crouch_feedback
        name=job['map'];doc=self.drafts.get(name)
        resumed=self.recover_rejected_shortcuts(job) or self.recover_rejected_ordinary(job)
        initial=native_crouch_feedback(doc,self.native(name)) if not resumed and doc and self.report_status(name)['current'] else None
        if resumed:
            key='withdrawn_rejected_ordinary'if resumed.get('model')=='native_rejected_edge_alternate_v2'else'resumed_rejected_shortcuts'
            repair={'repairs':{key:resumed},'status':'candidate_requires_native_validation'}
        elif initial:
            # Resume the already proposed topology; do not discover a second
            # batch of shortcuts merely to fix this pass's posture flags.
            proposal=json.loads(json.dumps(doc));wanted=set(initial['required_crouch_flags'])
            for node in proposal['nodes']:
                if node['id'] in wanted:node.update(nav_flags=int(node['nav_flags'])|64,crouch=True)
            initial.update(source_native_report=self.file_hash(self.native_path(name)))
            self.drafts.apply_repair(name,doc['revision'],self.draft_environment(name),proposal)
            self.prepare_draft(name)
            repair={'repairs':{'resumed_native_crouch':initial},'status':'candidate_requires_native_validation'}
        else:
            repair = self.action_repair(job)
        posture=[];withdrawn=[];ordinary_withdrawn=[];native_paths=[];native_anchors=[];native_descents=[];pending_jumps=[]
        path_search_done=bool(job.get('_native_path_search_attempted') or repair.get('repairs',{}).get('native_paths'))
        # One bounded native path search can be followed by a full check and
        # the existing proved-alternate withdrawal. Posture/history retries
        # keep separate receipts; no failed import becomes an accepted WAY.
        for attempt in range(5):
            try:
                checked = self.action_native_check(job)
                # A physically valid import may still contain a separate
                # spawn platform. It does not throw the ordinary-sink error,
                # so explicitly run the same bounded, source-bound traversal
                # repair before declaring this pass finished.
                if (attempt<4 and not job.get('_native_descent_search_attempted') and
                        not self.generated_navigation_readiness(name)['ready']):
                    descents=self.repair_descent_draft(job)
                    if descents and (descents.get('added_nodes') or descents.get('replaced_rejected_edges')):
                        native_descents.append(descents)
                        continue
                break
            except RuntimeError:
                rollback=(self.recover_rejected_shortcuts(job) or self.recover_rejected_ordinary(job)) if attempt<4 else None
                if rollback:
                    (ordinary_withdrawn if rollback.get('model')=='native_rejected_edge_alternate_v2' else withdrawn).append(rollback)
                    continue
                name=job['map'];doc=self.drafts.get(name)
                feedback=native_crouch_feedback(doc,self.native(name)) if doc and self.report_status(name)['current'] else None
                if not feedback:
                    if attempt<4 and not job.get('_pending_jump_cleanup_attempted'):
                        cleanup=self.repair_pending_jump_draft(job)
                        if cleanup:
                            pending_jumps.append(cleanup)
                            continue
                    if attempt<4 and not path_search_done:
                        path_search_done=True
                        paths=self.repair_path_draft(job)
                        if paths and (paths.get('added_node_ids') or paths.get('added_directed_edges')):
                            native_paths.append(paths)
                            continue
                    if attempt<4 and not job.get('_native_anchor_search_attempted'):
                        anchors=self.repair_anchor_draft(job)
                        if anchors and anchors.get('moves'):
                            native_anchors.append(anchors)
                            continue
                    if attempt<4 and not job.get('_native_descent_search_attempted'):
                        descents=self.repair_descent_draft(job)
                        if descents and (descents.get('added_nodes') or descents.get('replaced_rejected_edges')):
                            native_descents.append(descents)
                            continue
                    raise
                if attempt==4:raise
                # This is a source-bound repair proposal, never UID adoption
                # or acceptance of the failed WAY. Preserve all user fields
                # and topology and rerun the entire native import afterwards.
                proposal=json.loads(json.dumps(doc));wanted=set(feedback['required_crouch_flags'])
                for node in proposal['nodes']:
                    if node['id'] in wanted:node.update(nav_flags=int(node['nav_flags'])|64,crouch=True)
                feedback.update(source_native_report=self.file_hash(self.native_path(name)))
                self.drafts.apply_repair(name,doc['revision'],self.draft_environment(name),proposal)
                self.prepare_draft(name);posture.append(feedback)
                self.update(job,progress='Nativ erforderliches Ducken übernehmen und das ganze Netz erneut prüfen')
        if posture:
            repair['automatic_native_crouch']=posture
            self.path(job['map'],'.crouch-repair.json').write_text(json.dumps(posture,indent=2),encoding='utf8')
        if withdrawn:repair['withdrawn_rejected_shortcuts']=withdrawn
        if ordinary_withdrawn:repair['withdrawn_rejected_ordinary']=ordinary_withdrawn
        if native_paths:repair['native_path_repairs']=native_paths
        if native_anchors:repair['native_anchor_relocations']=native_anchors
        if native_descents:repair['native_action_repairs']=native_descents
        if pending_jumps:repair['redundant_pending_jumps']=pending_jumps
        common_anchors=self.reconcile_objective_anchors(job)if reconcile else None
        if common_anchors and(common_anchors.get('certificate')or{}).get('moves'):
            checked=dict(audit=self.inspect(job),proof=read_json(self.work/(job['map']+'.native-proof.json')))
        remaining = len(checked["audit"].get("unresolved_nodes", []))
        return {"repair": repair, "audit": checked["audit"],"proof":checked['proof'], "installed": False,
                "objective_anchor_certificate":(common_anchors or{}).get('certificate'),
                "note": (f"Reparatur geprüft. {remaining} Punkte mit fehlendem Hin-/Rückweg. "
                         "Unter Probleme beheben auf Kamera hin klicken. Es wurde nichts installiert.")}

    def action_rebuild(self, job):
        if not self.recast:
            raise RuntimeError("Recast-Compiler fehlt. tce_navmesh_preview.exe neben der Anwendung unter bin bereitstellen.")
        name = job["map"]
        self.quiet_generation(job)
        runtime, snapshot = self.snapshot(job)
        profile = snapshot["movement_profile"]
        record = self.assets.maps[name]
        self.command(job, self.worker("bsp", record["path"], "--map", name, "--entities", runtime,
                                    "--out-dir", self.preview, "--vertical-raster-domain"),
                     "BSP-Kollision, Patches und aktuelle Map-Modelle")
        recast_evidence = self.rebuild_recast(job, profile)
        self.require_profile_inputs(name, profile)
        candidate = self.path(name, "_candidate.txt")
        self.command(job, self.worker("waygraph", "--stand", self.path(name, ".stand.json"),
                                     "--duck", self.path(name, ".duck.json"), "--geometry", self.path(name, ".geometry.json"),
                                     "--out", candidate), "Regionen und sparsame Portalwege vorschlagen")
        generated = read_json(candidate.with_suffix(".json"))
        self.require_profile_inputs(name, profile, candidate)
        collision_stability=self.verify_collision_snapshot(job,snapshot)
        self.select_candidate(name, candidate)
        return {"candidate": str(candidate), "counts": generated["counts"],
                "movement_profile": profile, "recast_evidence": recast_evidence,"collision_stability":collision_stability,
                "status": "candidate_requires_native_validation", "installed": False}

    def action_repair(self, job, feedback=None, scanner_only=False):
        name = job["map"]
        if not scanner_only and not (getattr(self,"drafts",None) and self.drafts.get(name)):
            self.normalize_generated_graph(job)
        if getattr(self,"drafts",None) and self.drafts.get(name):
            return self.repair_manual_draft(job)
        report = self.native(name)
        if not report:
            raise RuntimeError("Zuerst einen Kandidaten nativ prüfen.")
        filename = Path(report.get("source", "")).name
        if not re.fullmatch(re.escape(name) + r"_candidate(?:_[A-Za-z0-9]+)?\.txt", filename):
            raise RuntimeError("Native Quelle ist kein unterstützter Recast-Kandidat.")
        base = self.preview / Path(filename).with_suffix(".json")
        if not base.is_file():
            raise RuntimeError("Zum nativen Bericht passende Kandidat-Metadaten fehlen.")
        if self.candidate_identity(base.with_suffix(".txt")) != self.candidate_identity(self.candidate(name)):
            raise RuntimeError("Reparaturbericht gehört zu einem früheren Kandidaten. Aktuellen Stand zuerst prüfen.")
        proof = self.proof_status(name, base.with_suffix(".txt"), receipt=False)
        if not proof["valid"]:
            raise RuntimeError("Reparaturquelle ist veraltet; zuerst Geometrie und Kandidat neu erzeugen: " + "; ".join(proof["reasons"]))
        self.require_profile_inputs(name, native_body_profile(report), base.with_suffix(".txt"))
        if report.get("generation_complete") is True and not self.report_status(name, base.with_suffix(".txt"))["current"]:
            raise RuntimeError("Reparaturbericht ist veraltet. Aktuellen Stand zuerst erneut prüfen.")
        if report.get("generation_complete") is not True:
            # receipt=False validates candidate bytes only, never a partial
            # native report. Only GENERATE supplies this freshly bound token.
            if (not feedback or feedback.get("status") != "incomplete_export_feedback_only" or
                    feedback.get("native_report") != self.file_hash(self.native_path(name)) or
                    feedback.get("inputs") != proof["inputs"] or feedback.get("candidate") != base.with_suffix(".txt").name or
                    feedback.get("staged_candidate") != self.file_hash(self.engine_nav_root()/base.with_suffix(".txt").name) or
                    feedback.get("movement_profile") != native_body_profile(report)):
                raise RuntimeError(incomplete_export_summary(report) + " Frische Generierungs-Rückmeldung fehlt oder wurde verändert.")
            isolated_export_feedback(report, read_json(base), base.with_suffix(".txt"))
        candidate = self.path(name, "_candidate_repair.txt")
        self.command(job, self.worker("waygraph", "--stand", self.path(name, ".stand.json"),
                                     "--duck", self.path(name, ".duck.json"), "--geometry", self.path(name, ".geometry.json"),
                                     "--base-candidate", base, "--native-feedback", self.native_path(name),
                                     "--out", candidate), "Zusätzliche Polygonwege für abgelehnte Direktstrecken")
        value = read_json(candidate.with_suffix(".json"))
        if feedback and (self.file_hash(self.native_path(name)) != feedback["native_report"] or
                         value.get("native_feedback_repairs", {}).get("sha256") != feedback["native_report"]):
            raise RuntimeError("Native Reparaturrückmeldung wurde während der Kandidaterzeugung verändert; keine Freigabe.")
        self.select_candidate(name, candidate)
        return {"candidate": str(candidate), "counts": value["counts"],
                "repairs": value.get("native_feedback_repairs"), "status": "candidate_requires_native_validation"}

    def repair_manual_draft(self, job):
        name=job["map"]
        self.prepare_draft(name)
        if not self.report_status(name)["current"]:
            try:
                self.action_native_check(job)
            except RuntimeError:
                # A rejected authored edge/floor still produces a fresh,
                # hash-bound report. Connection errors never authorize repair.
                if not self.report_status(name)["current"]:
                    raise
        ordinary=self.recover_rejected_ordinary(job)
        if ordinary:
            current=self.drafts.get(name)
            return dict(candidate=str(self.candidate(name)),counts=dict(nodes=len(current['nodes']),directed_edges=len(current['edges'])),
                        repairs=dict(withdrawn_rejected_ordinary=ordinary),status='candidate_requires_native_validation')
        pending_jump=self.repair_pending_jump_draft(job)
        if pending_jump:
            current=self.drafts.get(name)
            return dict(candidate=str(self.candidate(name)),counts=dict(nodes=len(current['nodes']),directed_edges=len(current['edges'])),
                        repairs=dict(redundant_pending_jump=pending_jump),status='candidate_requires_native_validation')
        paths=self.repair_path_draft(job)
        if paths and (paths.get('added_node_ids') or paths.get('added_directed_edges')):
            current=self.drafts.get(name)
            return dict(candidate=str(self.candidate(name)),counts=dict(nodes=len(current['nodes']),directed_edges=len(current['edges'])),
                        repairs=dict(native_paths=paths),status='candidate_requires_native_validation')
        anchors=self.repair_anchor_draft(job)
        if anchors and anchors.get('moves'):
            current=self.drafts.get(name)
            return dict(candidate=str(self.candidate(name)),counts=dict(nodes=len(current['nodes']),directed_edges=len(current['edges'])),
                        repairs=dict(native_anchor_relocations=anchors),status='candidate_requires_native_validation')
        from repair import propose, propose_descents, parse_native_probe
        from waygraph import Mesh
        doors=None
        if getattr(self,"game_mode","tce2")=="tce2" and self.proof_status(name).get("valid"):
            doors=self.repair_door_draft(job)
            if doors["added_node_ids"]:
                # The explicit repair report is a proposal, not the next
                # candidate's certificate. Recheck its complete authored form.
                self.action_native_check(job)
        native=self.native(name)
        doc=self.drafts.get(name)
        source_report=self.file_hash(self.native_path(name))
        proposal,evidence=propose(doc,native,Mesh(self.path(name,".stand.json")),Mesh(self.path(name,".duck.json")),
                                  read_json(self.path(name,".geometry.json")))
        if getattr(self,"game_mode","tce2")=="tce2" and not job.get('_native_descent_search_attempted'):
            profile=native_body_profile(native)
            self.require_loaded(name)
            if self.require_bbox(job)!=profile:
                raise RuntimeError("Körperprofil passt nicht zur Reparaturquelle; zuerst aktuell nativ prüfen.")
            probes=[0]
            def native_probe(start,end):
                values=list(start)+list(end)
                if any(type(v) not in (int,float) or not math.isfinite(v) or abs(v)>131072 for v in values):
                    raise RuntimeError("Ungültige Koordinaten für die native Reparaturprobe.")
                probes[0]+=1
                self.update(job,progress=f"Einbahn-Abstiege: native Kapsel-/Rückwegprobe {probes[0]}/96")
                reply=self.rcon("tce_navgen_probe feet "+" ".join("%.5f" % v for v in values),read_only=True)
                self.update(job,log=(job["log"]+"\n"+reply)[-250000:])
                return parse_native_probe(reply,start,end,profile["origin_above_feet"])
            job['_native_descent_search_attempted']=True
            proposal,actions=propose_descents(proposal,native,native_probe)
            self.require_loaded(name)
            if self.require_bbox(job)!=profile or self.file_hash(self.native_path(name))!=source_report:
                raise RuntimeError("Native Reparaturquelle wurde während der Proben verändert; kein Entwurf überschrieben.")
            resolved={tuple(edge) for edge in actions["replaced_rejected_edges"]}
            for route in evidence["routes"]:
                if tuple(route["edge"]) in resolved:
                    route["status"]="resolved_by_native_action_proposal"
            evidence["native_action_repairs"]=actions
            evidence["added_nodes"]+=actions["added_nodes"]
            evidence["replaced_rejected_edges"]+=actions["replaced_rejected_edges"]
            evidence["added_directed_edges"]=len({(e["from"],e["to"]) for e in proposal["edges"]}-
                                                  {(e["from"],e["to"]) for e in doc["edges"]})
        from repair import filter_rejected_shortcuts
        proposal,suppressed=filter_rejected_shortcuts(doc,proposal,read_json(self.path(name,'.repair-rejections.json'),{}),self.repair_rejection_context(name))
        evidence['suppressed_native_rejected_shortcuts']=suppressed
        evidence['added_directed_edges']=len({(e['from'],e['to'])for e in proposal['edges']}-{(e['from'],e['to'])for e in doc['edges']})
        if evidence.get('required_crouch_flags') or evidence.get('added_directed_edges'):
            source_native_path=self.path(name,'.repair-source-'+source_report[:12]+'.native.json')
            raw=self.native_path(name).read_bytes()
            if self.file_hash(self.native_path(name))!=source_report:
                raise RuntimeError('Native Duck-Rückmeldung verändert; kein Reparaturentwurf übernommen.')
            if source_native_path.exists() and source_native_path.read_bytes()!=raw:
                raise RuntimeError('Nativer Duck-Prüfbeleg mit anderem Inhalt vorhanden.')
            if not source_native_path.exists():source_native_path.write_bytes(raw)
            evidence['source_native_evidence']=dict(path=str(source_native_path),sha256=source_report)
        current=self.drafts.apply_repair(name,doc["revision"],self.draft_environment(name),proposal,
                                         identity_provider=lambda:self.native_edit_identities(name))
        self.prepare_draft(name)
        evidence.update(source_native_report=source_report,draft_revision=current["revision"],
                        proposal_graph_sha256=graph_hash(current),candidate=self.candidate_identity(self.candidate(name)))
        if doors is not None:evidence["native_door_repairs"]=doors
        self.archive_repair_proposal(name)
        self.path(name,".repair-proposal.json").write_text(json.dumps(evidence,indent=2),encoding="utf-8")
        self.archive_repair_proposal(name)
        return dict(candidate=str(self.candidate(name)),counts=dict(nodes=len(current["nodes"]),directed_edges=len(current["edges"])),
                    repairs=evidence,status="candidate_requires_native_validation")

    def repair_pending_jump_draft(self,job):
        """Explicitly retire one unchanged, redundant and unentered auto jump."""
        if (job.get('action') not in ('repair','repair_check') or job.get('_pending_jump_cleanup_attempted') or
                getattr(self,'game_mode','tce2')!='tce2'):return None
        from repair import redundant_pending_jump_feedback
        name=job['map'];doc=self.drafts.get(name);baseline=self.native(name)
        original_hash=doc.get('base',{}).get('native_report','') if doc else ''
        if (not doc or baseline.get('generation_complete') is not False or
                not isinstance(original_hash,str) or not re.fullmatch('[a-f0-9]{64}',original_hash)):return None
        original_path=self.work/'generation-scans'/(name+'-'+original_hash)/'scanner.json'
        if not original_path.is_file() or self.file_hash(original_path)!=original_hash:return None
        candidate=self.prepare_draft(name);doc=self.drafts.get(name)
        status=self.report_status(name,candidate);inputs=self.proof_status(name,candidate,receipt=False)
        if status.get('current') is not True or inputs.get('valid') is not True:return None
        attempt_path=self.work/(name+'.native-attempt.json');attempt_raw=attempt_path.read_bytes()
        binding=dict(report_status=status,attempt=json.loads(attempt_raw),inputs=inputs['inputs'],
            metadata=read_json(candidate.with_suffix('.json'),{}),native_attempt_bytes=attempt_raw,
            native_report_bytes=self.native_path(name).read_bytes(),candidate_bytes=candidate.read_bytes(),
            candidate_metadata_bytes=candidate.with_suffix('.json').read_bytes(),original_native_bytes=original_path.read_bytes())
        before=(graph_hash(doc),self.file_hash(self.native_path(name)),self.candidate_identity(candidate),
                self.file_hash(attempt_path),self.file_hash(original_path))
        result=redundant_pending_jump_feedback(doc,baseline,binding,binding['original_native_bytes'])
        if not result:return None
        proposal,evidence=result
        if (not self.report_status(name,candidate)['current'] or
                before!=(graph_hash(self.drafts.get(name)),self.file_hash(self.native_path(name)),
                         self.candidate_identity(self.candidate(name)),self.file_hash(attempt_path),self.file_hash(original_path)) or
                self.proof_status(name,candidate,receipt=False).get('inputs')!=inputs['inputs']):
            raise RuntimeError('Grundlage der Sprunghelfer-Bereinigung geändert; Entwurf bleibt erhalten.')
        evidence['source_evidence']={}
        archive_parts=(('native_report_bytes','.source.native.json'),('candidate_bytes','.candidate.txt'),
                       ('candidate_metadata_bytes','.candidate.json'),('native_attempt_bytes','.attempt.json'),
                       ('original_native_bytes','.original.native.json'))
        digest=repair_archive_bundle_digest(binding,archive_parts);evidence['archive_bundle_sha256']=digest
        for key,suffix in archive_parts:
            data=binding[key];archive=self.path(name,'.pending-jump-cleanup-'+digest[:12]+suffix)
            if archive.exists() and archive.read_bytes()!=data:
                raise RuntimeError('Archivierter Sprunghelfer-Beleg enthält bereits andere Daten.')
            if not archive.exists():archive.write_bytes(data)
            evidence['source_evidence'][key]=dict(path=str(archive),sha256=hashlib.sha256(data).hexdigest())
        # Keep automatic repair's blanket deletion prohibition. The existing
        # editor deletion owns Undo and retains the monotonic source-ID counter;
        # the source-bound selection above is the only authorization here.
        job['_pending_jump_cleanup_attempted']=True
        current=self.drafts.change(name,dict(op='delete_node',id=evidence['tombstoned_source_id'],revision=doc['revision']),
                                   self.draft_environment(name))
        if graph_hash(current)!=graph_hash(proposal) or current['next_id']!=doc['next_id']:
            self.drafts.documents[name]=doc
            raise RuntimeError('Sprunghelfer-Bereinigung hätte weitere Entwurfsdaten verändert.')
        evidence.update(result_draft_revision=current['revision'],proposal_graph_sha256=graph_hash(current))
        receipt=self.path(name,'.pending-jump-cleanup-'+digest[:12]+'.receipt.json')
        data=json.dumps(evidence,indent=2).encode('utf8')
        if receipt.exists() and receipt.read_bytes()!=data:
            self.drafts.documents[name]=doc
            raise RuntimeError('Archivierter Sprunghelfer-Reparaturbeleg enthält bereits andere Daten.')
        if not receipt.exists():receipt.write_bytes(data)
        self.prepare_draft(name)
        self.path(name,'.pending-jump-cleanup.json').write_bytes(data)
        self.update(job,progress='Unveränderten unbetretbaren Auto-Sprunghelfer durch beidseitig geprüfte Laufwege ersetzen; ganzes Netz erneut prüfen')
        return evidence

    def repair_descent_draft(self,job):
        """Resume only the existing action phase after ordinary repair retries.

        This never discovers Mesh shortcuts. A partial, source-bound import
        supplies candidates; every transition still needs a fresh native
        probe, followed by the caller's bounded complete graph check.
        """
        if (job.get('action') not in ('repair','repair_check') or getattr(self,'game_mode','tce2')!='tce2' or
                job.get('_native_descent_search_attempted')):return None
        from repair import propose_descents,parse_native_probe,terminal_spawn_components
        from pathrepair import validate_source
        name=job['map'];baseline=self.native(name);doc=self.drafts.get(name)
        if (not doc or (baseline.get('generation_complete') is not False and
                        not (baseline.get('generation_complete') is True and terminal_spawn_components(doc,baseline))) or
                not baseline.get('traversal_physics',{}).get('gravity')):return None
        candidate=self.prepare_draft(name);doc=self.drafts.get(name)
        status=self.report_status(name,candidate);inputs=self.proof_status(name,candidate,receipt=False)
        if status.get('current') is not True or inputs.get('valid') is not True:return None
        metadata=candidate.with_suffix('.json');profile=native_body_profile(baseline)
        attempt_raw=(self.work/(name+'.native-attempt.json')).read_bytes()
        binding=dict(report_status=status,attempt=json.loads(attempt_raw),native_attempt_bytes=attempt_raw,
            inputs=inputs['inputs'],metadata=read_json(metadata,{}),native_report_bytes=self.native_path(name).read_bytes(),
            candidate_bytes=candidate.read_bytes(),candidate_metadata_bytes=metadata.read_bytes())
        validate_source(doc,baseline,profile['origin_above_feet'],binding)
        self.require_loaded(name)
        if self.require_bbox(job)!=profile:raise RuntimeError('Körperprofil passt nicht zur Abstiegsreparatur.')
        self.require_profile_inputs(name,profile,candidate)
        before=(graph_hash(doc),self.file_hash(self.native_path(name)),self.candidate_identity(candidate),
                self.file_hash(self.work/(name+'.native-attempt.json')))
        replies=[];job['_native_descent_search_attempted']=True
        def probe(start,end):
            values=list(start)+list(end)
            if len(replies)>=96 or any(type(v) not in (int,float) or not math.isfinite(v) or abs(v)>131072 for v in values):
                raise RuntimeError('Ungültige oder zu viele native Proben für die Abstiegsreparatur.')
            self.update(job,progress=f'Verbliebenen Abstieg und Rückweg nativ prüfen: {len(replies)+1}/96')
            reply=self.rcon('tce_navgen_probe feet '+' '.join('%.5f'%v for v in values),read_only=True)
            self.update(job,log=(job.get('log','')+'\n'+reply)[-250000:])
            parsed=parse_native_probe(reply,start,end,profile['origin_above_feet'])
            replies.append(dict(start=start,end=end,reply=reply,sha256=parsed['reply_sha256']))
            return parsed
        proposal,evidence=propose_descents(doc,baseline,probe)
        self.require_loaded(name)
        if (self.require_bbox(job)!=profile or not self.report_status(name,candidate)['current'] or
                before!=(graph_hash(self.drafts.get(name)),self.file_hash(self.native_path(name)),
                         self.candidate_identity(self.candidate(name)),self.file_hash(self.work/(name+'.native-attempt.json'))) or
                self.proof_status(name,candidate,receipt=False).get('inputs')!=inputs['inputs']):
            raise RuntimeError('Grundlage während der Abstiegsreparatur geändert; Entwurf bleibt erhalten.')
        self.require_profile_inputs(name,profile,candidate)
        binding['probe_reply_bytes']=json.dumps(replies,indent=2).encode('utf8')
        evidence.update(source_inputs=json.loads(json.dumps(inputs['inputs'])),source_graph_sha256=before[0],
                        source_native_report=before[1],draft_revision=doc['revision'],candidate=candidate.name,
                        source_evidence={},installed=False)
        archive_parts=(('native_report_bytes','.source.native.json'),('candidate_bytes','.candidate.txt'),
                       ('candidate_metadata_bytes','.candidate.json'),('native_attempt_bytes','.attempt.json'),
                       ('probe_reply_bytes','.probes.json'))
        digest=repair_archive_bundle_digest(binding,archive_parts);evidence['archive_bundle_sha256']=digest
        for key,suffix in archive_parts:
            data=binding[key];archive=self.path(name,'.descent-repair-'+digest[:12]+suffix)
            if archive.exists() and archive.read_bytes()!=data:
                raise RuntimeError('Archivierter Abstiegsprüfbeleg enthält bereits andere Daten.')
            if not archive.exists():archive.write_bytes(data)
            evidence['source_evidence'][key]=dict(path=str(archive),sha256=hashlib.sha256(data).hexdigest())
        if evidence['added_nodes'] or evidence['replaced_rejected_edges']:
            current=self.drafts.apply_repair(name,doc['revision'],self.draft_environment(name),proposal)
            self.prepare_draft(name)
            evidence.update(result_draft_revision=current['revision'],proposal_graph_sha256=graph_hash(current))
        receipt=self.path(name,'.descent-repair-'+digest[:12]+'.receipt.json')
        data=json.dumps(evidence,indent=2).encode('utf8')
        if receipt.exists() and receipt.read_bytes()!=data:
            raise RuntimeError('Archivierter Abstiegsreparaturbeleg enthält bereits andere Daten.')
        if not receipt.exists():receipt.write_bytes(data)
        self.path(name,'.descent-repair.json').write_bytes(data)
        return evidence

    def repair_anchor_draft(self,job):
        """Explicit bounded ordinary-point repair; no Check/Generate mutation."""
        if (job.get('action') not in ('repair','repair_check') or getattr(self,'game_mode','tce2')!='tce2' or
                job.get('_native_anchor_search_attempted')):return None
        from repair import propose_anchor_relocations,parse_native_probe
        from pathrepair import validate_source
        name=job['map'];baseline=self.native(name);doc=self.drafts.get(name)
        if (not doc or baseline.get('generation_complete') is not False or not any(
                n.get('reason')=='quarantined_no_validated_route_from_spawn' and
                n.get('native_input',{}).get('floor_accepted') is True for n in baseline.get('nodes',[]))):return None
        candidate=self.prepare_draft(name);doc=self.drafts.get(name)
        status=self.report_status(name,candidate);inputs=self.proof_status(name,candidate,receipt=False)
        if status.get('current') is not True or inputs.get('valid') is not True:return None
        metadata=candidate.with_suffix('.json');profile=native_body_profile(baseline)
        attempt_raw=(self.work/(name+'.native-attempt.json')).read_bytes()
        binding=dict(report_status=status,attempt=json.loads(attempt_raw),native_attempt_bytes=attempt_raw,
            inputs=inputs['inputs'],metadata=read_json(metadata,{}),native_report_bytes=self.native_path(name).read_bytes(),
            candidate_bytes=candidate.read_bytes(),candidate_metadata_bytes=metadata.read_bytes())
        validate_source(doc,baseline,profile['origin_above_feet'],binding)
        self.require_loaded(name)
        if self.require_bbox(job)!=profile:raise RuntimeError('Körperprofil passt nicht zur Ankerkorrektur.')
        self.require_profile_inputs(name,profile,candidate)
        before=(graph_hash(doc),self.file_hash(self.native_path(name)),self.candidate_identity(candidate),
                self.file_hash(self.work/(name+'.native-attempt.json')))
        replies=[];job['_native_anchor_search_attempted']=True
        def probe(start,end):
            values=list(start)+list(end)
            if len(replies)>=96 or any(type(v) not in (int,float) or not math.isfinite(v) or abs(v)>131072 for v in values):
                raise RuntimeError('Ungültige oder zu viele native Proben für die Ankerkorrektur.')
            self.update(job,progress=f'Gewöhnlichen Bodenanker mit vollständiger Import-/Kapselprüfung korrigieren: {len(replies)+1}/96')
            reply=self.rcon('tce_navgen_probe feet '+' '.join('%.5f'%v for v in values),read_only=True)
            self.update(job,log=(job.get('log','')+'\n'+reply)[-250000:])
            parsed=parse_native_probe(reply,start,end,profile['origin_above_feet'])
            replies.append(dict(start=start,end=end,reply=reply,sha256=parsed['reply_sha256']))
            return parsed
        proposal,evidence=propose_anchor_relocations(doc,baseline,probe,binding)
        self.require_loaded(name)
        if (self.require_bbox(job)!=profile or not self.report_status(name,candidate)['current'] or
                before!=(graph_hash(self.drafts.get(name)),self.file_hash(self.native_path(name)),
                         self.candidate_identity(self.candidate(name)),self.file_hash(self.work/(name+'.native-attempt.json'))) or
                self.proof_status(name,candidate,receipt=False).get('inputs')!=inputs['inputs']):
            raise RuntimeError('Grundlage während der Ankerkorrektur geändert; Entwurf bleibt erhalten.')
        self.require_profile_inputs(name,profile,candidate)
        binding['probe_reply_bytes']=json.dumps(replies,indent=2).encode('utf8')
        evidence.update(source_inputs=json.loads(json.dumps(inputs['inputs'])),source_graph_sha256=before[0],
                        draft_revision=doc['revision'],candidate=candidate.name,source_evidence={})
        archive_parts=(('native_report_bytes','.source.native.json'),('candidate_bytes','.candidate.txt'),
                       ('candidate_metadata_bytes','.candidate.json'),('native_attempt_bytes','.attempt.json'),
                       ('probe_reply_bytes','.probes.json'))
        digest=repair_archive_bundle_digest(binding,archive_parts);evidence['archive_bundle_sha256']=digest
        for key,suffix in archive_parts:
            data=binding[key];archive=self.path(name,'.anchor-repair-'+digest[:12]+suffix)
            if archive.exists() and archive.read_bytes()!=data:
                raise RuntimeError('Archivierter Ankerprüfbeleg enthält bereits andere Daten.')
            if not archive.exists():archive.write_bytes(data)
            evidence['source_evidence'][key]=dict(path=str(archive),sha256=hashlib.sha256(data).hexdigest())
        if evidence['moves']:
            current=self.drafts.apply_repair(name,doc['revision'],self.draft_environment(name),proposal)
            self.prepare_draft(name)
            evidence.update(result_draft_revision=current['revision'],proposal_graph_sha256=graph_hash(current))
        receipt=self.path(name,'.anchor-repair-'+digest[:12]+'.receipt.json')
        data=json.dumps(evidence,indent=2).encode('utf8')
        if receipt.exists() and receipt.read_bytes()!=data:
            raise RuntimeError('Archivierter Ankerkorrekturbeleg enthält bereits andere Daten.')
        if not receipt.exists():receipt.write_bytes(data)
        self.path(name,'.anchor-repair.json').write_bytes(data)
        return evidence

    def repair_path_draft(self, job):
        """Append explicitly requested native paths to a current failed draft.

        A partial report is only feedback. Preserve its literal inputs and
        every current node/edge; the ordinary full import must check the new
        graph before any rejected old edge can use an alternate-path proof.
        """
        if (job.get('action') not in ('repair','repair_check') or getattr(self,'game_mode','tce2')!='tce2' or
                job.get('_native_path_search_attempted')):return None
        name=job['map'];baseline=self.native(name);doc=self.drafts.get(name)
        reasons={'blocked_clearance','blocked_sweep','blocked_step_up','blocked_step_down','different_floor',
                 'gap_or_drop','step_or_drop','capsule_support_gap_or_drop','capsule_support_step_or_drop'}
        if (not doc or baseline.get('generation_complete') is not False or
                not any(e.get('accepted') is False and e.get('reason') in reasons for e in baseline.get('edges',[]))):return None
        from pathrepair import merge, validate_source
        candidate=self.prepare_draft(name);doc=self.drafts.get(name)
        status=self.report_status(name,candidate);inputs=self.proof_status(name,candidate,receipt=False)
        if status.get('current') is not True or inputs.get('valid') is not True:return None
        metadata=candidate.with_suffix('.json');profile=native_body_profile(baseline)
        attempt_raw=(self.work/(name+'.native-attempt.json')).read_bytes()
        binding=dict(report_status=status,attempt=json.loads(attempt_raw),native_attempt_bytes=attempt_raw,
            inputs=inputs['inputs'],metadata=read_json(metadata,{}),native_report_bytes=self.native_path(name).read_bytes(),
            candidate_bytes=candidate.read_bytes(),candidate_metadata_bytes=metadata.read_bytes())
        if not validate_source(doc,baseline,profile['origin_above_feet'],binding):return None
        self.require_loaded(name)
        if self.require_bbox(job)!=profile:raise RuntimeError('Körperprofil passt nicht zur nativen Pfadreparatur.')
        self.require_profile_inputs(name,profile,candidate)
        before=(graph_hash(doc),self.file_hash(self.native_path(name)),self.candidate_identity(candidate),
                self.file_hash(self.work/(name+'.native-attempt.json')))
        runtime=self.runtime_native_path(name);stamp=runtime.stat().st_mtime_ns if runtime.exists() else 0
        target=self.engine_nav_root()/candidate.name
        if self.file_hash(target)!=inputs['inputs']['candidate']:self.atomic_copy(candidate,target)
        self.update(job,progress='Abgelehnte Laufwege mit frischen Kapsel- und Bodenproben als zusätzliche Pfade prüfen')
        job['_native_path_search_attempted']=True
        reply=self.rcon_with_followup('tce_navgen_import navgen/'+candidate.name+' repair_paths '+str(doc['next_id']))
        self.update(job,log=(job.get('log','')+'\n'+reply)[-250000:])
        rejected=re.search(r'navgen import: rejected [^\r\n]+; no WAY written',reply)
        if rejected:raise RuntimeError('Pfadreparatur abgelehnt: '+rejected[0])
        report=self.wait_fresh(runtime,stamp,180)
        raw=runtime.read_bytes();binding['repair_report_bytes']=raw
        self.require_loaded(name)
        if (json.loads(raw)!=report or report.get('map')!=name or Path(report.get('source','')).name!=candidate.name or
                native_body_profile(report)!=profile or self.require_bbox(job)!=profile or
                not self.report_status(name,candidate)['current'] or
                before!=(graph_hash(self.drafts.get(name)),self.file_hash(self.native_path(name)),
                         self.candidate_identity(self.candidate(name)),self.file_hash(self.work/(name+'.native-attempt.json'))) or
                self.proof_status(name,candidate,receipt=False).get('inputs')!=inputs['inputs'] or
                self.file_hash(target)!=inputs['inputs']['candidate']):
            raise RuntimeError('Grundlage während der Pfadreparatur geändert; Entwurf bleibt erhalten.')
        self.require_profile_inputs(name,profile,candidate)
        proposal,evidence=merge(doc,report,profile['origin_above_feet'],baseline,binding)
        evidence.update(source_inputs=json.loads(json.dumps(inputs['inputs'])),source_graph_sha256=before[0],
                        draft_revision=doc['revision'],candidate=candidate.name,source_evidence={})
        repair_digest=hashlib.sha256(raw).hexdigest()
        archive_parts=(('native_report_bytes','.source.native.json'),('candidate_bytes','.candidate.txt'),
                       ('candidate_metadata_bytes','.candidate.json'),('native_attempt_bytes','.attempt.json'),
                       ('repair_report_bytes','.native.json'))
        # A deterministic native report may recur after a fresh check. Its
        # attempt receipt still has a new timestamp and must retain its own
        # literal provenance. Address the whole named source bundle, not only
        # the native result; byte-identical retries can reuse the same bundle.
        digest=repair_archive_bundle_digest(binding,archive_parts)
        evidence['archive_bundle_sha256']=digest
        for key,suffix in archive_parts:
            data=binding[key];archive=self.path(name,'.path-repair-'+digest[:12]+suffix)
            if archive.exists() and archive.read_bytes()!=data:
                raise RuntimeError('Archivierter Pfadprüfbeleg enthält bereits andere Daten.')
            if not archive.exists():archive.write_bytes(data)
            evidence['source_evidence'][key]=dict(path=str(archive),sha256=hashlib.sha256(data).hexdigest())
        if evidence['added_node_ids'] or evidence.get('added_directed_edges'):
            current=self.drafts.apply_repair(name,doc['revision'],self.draft_environment(name),proposal,
                                            identity_provider=lambda:self.native_edit_identities(name))
            self.prepare_draft(name)
            evidence.update(result_draft_revision=current['revision'],proposal_graph_sha256=graph_hash(current))
        evidence.update(source_native_report=before[1],repair_report_sha256=repair_digest)
        receipt=self.path(name,'.path-repair-'+digest[:12]+'.receipt.json')
        data=json.dumps(evidence,indent=2).encode('utf8')
        if receipt.exists() and receipt.read_bytes()!=data:
            raise RuntimeError('Archivierter Pfadreparaturbeleg enthält bereits andere Daten.')
        if not receipt.exists():receipt.write_bytes(data)
        self.path(name,'.path-repair.json').write_bytes(data)
        return evidence

    def repair_door_draft(self, job):
        """Rediscover doors only on explicit Repair; checking never restores edits."""
        from doorrepair import merge
        name=job["map"];doc=self.drafts.get(name);candidate=self.prepare_draft(name)
        profile=self.require_bbox(job)
        inputs=self.proof_status(name,candidate)
        if not inputs["valid"] or not self.report_status(name)["current"]:
            raise RuntimeError("Türreparatur benötigt den aktuellen vollständigen Entwurf; zuerst nativ prüfen.")
        source_hash=self.file_hash(self.native_path(name));before_graph=graph_hash(doc)
        baseline=self.native(name)
        runtime=self.runtime_native_path(name)
        before=runtime.stat().st_mtime_ns if runtime.exists() else 0
        target=self.engine_nav_root()/candidate.name
        if self.file_hash(target)!=inputs["inputs"]["candidate"]:
            self.atomic_copy(candidate,target)
        self.update(job,progress="Benutzbare Türen mit sicheren Aktivierungs-, Warte- und Durchgangspunkten prüfen")
        reply=self.rcon_with_followup("tce_navgen_import navgen/"+candidate.name+" repair_doors "+str(doc["next_id"]))
        self.update(job,log=(job.get("log","")+"\n"+reply)[-250000:])
        rejected=re.search(r"navgen import: rejected [^\r\n]+; no WAY written",reply)
        if rejected:raise RuntimeError("Türkandidat abgelehnt: "+rejected[0])
        report=self.wait_fresh(runtime,before,180)
        if (Path(report.get("source","")).name!=candidate.name or native_body_profile(report)!=profile or
                self.require_bbox(job)!=profile or self.file_hash(self.native_path(name))!=source_hash or
                graph_hash(self.drafts.get(name))!=before_graph or
                self.proof_status(name,candidate).get("inputs")!=inputs["inputs"] or
                self.file_hash(target)!=inputs["inputs"]["candidate"]):
            raise RuntimeError("Grundlage während der Türreparatur geändert; Entwurf bleibt erhalten.")
        self.require_loaded(name)
        proposal,evidence=merge(doc,report,profile["origin_above_feet"],baseline)
        evidence.update(source_native_report=source_hash,repair_report_sha256=self.file_hash(runtime))
        if evidence["added_node_ids"]:
            self.drafts.apply_repair(name,doc["revision"],self.draft_environment(name),proposal,
                                      identity_provider=lambda:self.native_edit_identities(name))
            self.prepare_draft(name)
        self.path(name,".door-repair.json").write_text(json.dumps(evidence,indent=2),encoding="utf-8")
        return evidence

    def inspect(self, job):
        path = self.native_path(job["map"])
        if not path.is_file():
            raise RuntimeError("Noch kein nativer Bericht vorhanden.")
        output = self.command(job, self.worker("audit", "inspect", path, "--write-audit"),
                              "WAY-Datei und gerichtete Verbindungen prüfen", allowed=(0, 2))
        try:
            value = json.loads(output)
        except ValueError:
            value = {"output": output[-20000:]}
        value["report_binding"] = {"native_report": self.file_hash(path),
                                   "candidate": self.candidate_identity(self.candidate(job["map"]))}
        self.path(job["map"], ".native-audit.json").write_text(json.dumps(value, indent=2), encoding="utf-8")
        return value

    def record_native_attempt(self, name, candidate, inputs, report, profile, gametype=None):
        path = self.native_path(name)
        attempt = dict(schema=1, map=name, created=time.time(), candidate=candidate.name,
                       movement_profile=profile, inputs=inputs,
                       staged_candidate=self.file_hash(self.staged_candidate_path(candidate)),
                       native_report=self.file_hash(path), generation_complete=report.get("generation_complete") is True)
        if gametype is not None:attempt['gametype']=gametype
        destination = self.work / (name + ".native-attempt.json")
        destination.parent.mkdir(parents=True, exist_ok=True)
        pending = destination.with_suffix(".tmp")
        pending.write_text(json.dumps(attempt, indent=2), encoding="utf-8")
        pending.replace(destination)
        # Never keep the previous graph's 21000-line audit as current feedback.
        incoming, outgoing = {}, {}
        for edge in report.get("edges", []):
            if edge.get("accepted"):
                outgoing[edge["from"]] = outgoing.get(edge["from"], 0) + 1
                incoming[edge["to"]] = incoming.get(edge["to"], 0) + 1
        origin = report.get("bbox", {}).get("origin_above_feet", 30)
        unresolved = []
        for node in report.get("nodes", []):
            identity = node["id"]
            if node.get("accepted") and incoming.get(identity) and outgoing.get(identity):
                continue
            p = node.get("position", node.get("raw_position", [0,0,origin]))
            unresolved.append(dict(source_id=identity, position=[p[0],p[1],p[2]-origin],
                                   incoming=incoming.get(identity,0), outgoing=outgoing.get(identity,0),
                                   accepted=node.get("accepted"), reason=node.get("reason")))
        audit = dict(map=name, incomplete=report.get("generation_complete") is not True,
                     unresolved_nodes=unresolved,
                     rejected_edges=[dict(e) for e in report.get("edges", []) if not e.get("accepted")],
                     report_binding=dict(native_report=attempt["native_report"], candidate=self.candidate_identity(candidate)))
        self.path(name, ".native-audit.json").write_text(json.dumps(audit, indent=2), encoding="utf-8")
        return attempt

    def action_native_check(self, job):
        name = job["map"]
        if job.get("action")=="native_check":
            # The user requested a check of this project/map. Start/reuse the
            # one exact owned test instance directly; no second manual start.
            self.action_load_map(job)
            self.quiet_generation(job)
        self.require_loaded(name)
        profile = self.require_bbox(job)
        draft = getattr(self, "drafts", None)
        doc = draft.get(name) if draft else None
        if draft and not doc and self.candidate(name) is None:
            graph=self.imported_navigation(name)
            if graph:doc=self.begin_imported_draft(name,graph)
        if doc and not doc["base"].get("movement_profile"):
            # Imported WAYs can be edited immediately. Capture the exact game
            # geometry/profile once before their first check, then retain edits.
            self.update(job, progress="Importierte Wege: aktuelle Kollisionsgrundlage erfassen")
            self.action_rebuild(job)
            old_offset = doc["base"].pop("imported_origin_offset", profile["origin_above_feet"])
            shift = old_offset - profile["origin_above_feet"]
            if shift:
                for node in doc["nodes"]:
                    node["position"][2] += shift
            doc["base"]["environment"] = self.draft_environment(name)
            doc["base"]["movement_profile"] = profile
            doc["export"] = None
            doc["undo"] = []; doc["redo"] = []
        candidate = self.prepare_draft(name)
        if candidate is None:
            raise RuntimeError("Zuerst einen Recast-Kandidaten erzeugen.")
        inputs = self.proof_status(name, candidate, receipt=False)
        if not inputs["valid"]:
            raise RuntimeError("Kandidat ist veraltet. Neu erzeugen: " + "; ".join(inputs["reasons"]))
        self.require_profile_inputs(name, profile, candidate)
        self.engine_nav_root().mkdir(parents=True, exist_ok=True)
        target = self.engine_nav_root() / candidate.name
        temporary = target.with_suffix(".txt.tmp")
        shutil.copy2(candidate, temporary)
        temporary.replace(target)
        path = self.native_path(name)
        runtime_path = self.runtime_native_path(name)
        before = runtime_path.stat().st_mtime_ns if runtime_path.exists() else 0
        self.update(job, progress="TCE2 prüft jeden Punkt und Laufweg; Spielsitzung kann kurz pausieren")
        reply = self.rcon_with_followup(("bot navstudio_import navgen/" if getattr(self,"game_mode","tce2")=="et" else "tce_navgen_import navgen/") + candidate.name)
        self.update(job, log=(job["log"] + "\n" + reply)[-250000:])
        rejected=re.search(r"navgen import: rejected [^\r\n]+; no WAY written",reply)
        if rejected:raise RuntimeError("Prüfkandidat abgelehnt: "+rejected[0])
        report = self.wait_fresh(runtime_path, before, 180)
        if report.get("map") != name or Path(report.get("source", "")).name != candidate.name:
            raise RuntimeError("Native Prüfung lieferte einen Bericht für eine andere Quelle.")
        if native_body_profile(report) != profile:
            raise RuntimeError("Nativer Bericht verwendet ein anderes Koerperprofil; Pruefung nicht freigegeben.")
        self.require_loaded(name)
        if self.require_bbox(job) != profile:
            raise RuntimeError("Koerperprofil waehrend der nativen Pruefung geaendert; keine Freigabe.")
        self.require_profile_inputs(name, profile, candidate)
        after = self.proof_status(name, candidate, receipt=False)
        if not after["valid"] or after["inputs"] != inputs["inputs"] or self.file_hash(target) != inputs["inputs"]["candidate"]:
            raise RuntimeError("Quelldaten wurden während der nativen Prüfung verändert; Prüfung nicht freigegeben.")
        self.atomic_copy(runtime_path, path)
        self.atomic_copy(target, self.staged_candidate_path(candidate))
        if report.get("generation_complete") is True and path != runtime_path:
            runtime_way = runtime_path.with_suffix(".way")
            if not runtime_way.is_file():
                raise RuntimeError("Native Prüfung lieferte keine aktuelle WAY-Datei.")
            self.atomic_copy(runtime_way, path.with_suffix(".way"))
        self.record_native_attempt(name, candidate, inputs["inputs"], report, profile,gametype=job.get('gametype'))
        metadata = read_json(candidate.with_suffix(".json"), {})
        if job.get("action") == "native_check" and metadata.get("candidate_mode") != "manual_draft":
            normalized = self.normalize_generated_graph(job)
            if normalized:
                checked = self.action_native_check(job)
                checked["normalization"] = normalized
                return checked
        if metadata.get("candidate_mode") == "manual_draft":
            expected = {n["id"]: n for n in metadata.get("nodes", [])}
            actual = {n["id"]: n for n in report.get("nodes", []) if n.get("accepted")}
            missing = sorted(set(expected)-set(actual))
            changed = [i for i in expected.keys() & actual.keys()
                       if int(expected[i].get("nav_flags",0)) != int(actual[i].get("nav_flags",0)) or
                       bool(actual[i].get("crouch")) != bool(int(expected[i].get("nav_flags",0)) & 64)]
            links = {(e["from"],e["to"]) for e in report.get("edges",[]) if e.get("accepted")}
            lost = [pair for pair in metadata.get("edges",[]) if tuple(pair) not in links]
            if missing or changed or lost:
                raise RuntimeError("Bearbeitete Navigation nicht übernommen. Punkte abgelehnt: " + str(missing[:20]) +
                                   "; Flags verändert: " + str(changed[:20]) + "; Verbindungen abgelehnt: " + str(lost[:12]) +
                                   ". Entwurf bleibt erhalten; Fehlerpunkte im Prüfbericht auswählen.")
        if report.get("generation_complete") is not True:
            if metadata.get("candidate_mode") == "manual_draft":
                raise RuntimeError(incomplete_export_summary(report) +
                                   " Aktueller bearbeiteter Netzstand: Ausgangslücken bleiben offen; "
                                   "Entwurf und native Prüfergebnisse bleiben für die nächste Reparatur erhalten.")
            after = self.proof_status(name, candidate, receipt=False)
            if not after["valid"] or after["inputs"] != inputs["inputs"] or self.file_hash(target) != inputs["inputs"]["candidate"]:
                raise RuntimeError("Quelldaten wurden während der nativen Prüfung verändert; keine Reparaturrückmeldung.")
            feedback = isolated_export_feedback(report, read_json(candidate.with_suffix(".json")), candidate)
            report_hash = digest(path)
            if read_json(path) != report or digest(path) != report_hash:
                raise RuntimeError("Nativer Bericht während der Rückmeldungsprüfung verändert; keine Reparatur.")
            feedback.update(map=name, candidate=candidate.name, movement_profile=profile, inputs=inputs["inputs"],
                            staged_candidate=self.file_hash(target), native_report=report_hash)
            # Do not inspect the old WAY, write a proof receipt or return
            # success. The convenience workflow alone may use this feedback.
            raise IncompleteNativeFeedback(incomplete_export_summary(report), feedback)
        audit = self.inspect(job)
        after = self.proof_status(name, candidate, receipt=False)
        if not after["valid"] or after["inputs"] != inputs["inputs"] or self.file_hash(target) != inputs["inputs"]["candidate"]:
            raise RuntimeError("Quelldaten wurden während der nativen Prüfung verändert; Prüfung nicht freigegeben.")
        proof = {"schema": 1, "map": name, "created": time.time(), "candidate": candidate.name,
                 "movement_profile": profile,
                 "inputs": inputs["inputs"], "staged_candidate": self.file_hash(target),
                 "native_report": self.file_hash(path), "way": self.file_hash(path.with_suffix(".way"))}
        receipt = self.work / (name + ".native-proof.json")
        pending = receipt.with_suffix(".tmp")
        pending.write_text(json.dumps(proof, indent=2), encoding="utf-8")
        pending.replace(receipt)
        return {"report": str(path), "audit": audit, "proof": str(receipt), "installed": False}

    def action_inspect(self, job):
        if not self.report_status(job["map"])["current"]:
            return self.action_native_check(job)
        return self.inspect(job)

    def install(self, job, production):
        name = job["map"]
        report = self.native(name)
        if not report or not report.get("generation_complete"):
            raise RuntimeError("Vollständige native Prüfung und Audit sind vor einer Installation erforderlich.")
        proof = self.proof_status(name)
        if not proof["valid"]:
            raise RuntimeError("Aktuelle Hash-Belege der nativen Prüfung fehlen: " + "; ".join(proof["reasons"]))
        directory = (self.workspace / "tce2" if production else self.test) / "omni-bot" / "et" / "nav"
        if not production and getattr(self, "game_mode", "tce2") == "et":
            directory = self.mod / "et" / "nav"
        output = self.command(job, self.worker("audit", "install", self.native_path(name), directory, "--install",
                              *(["--test-main-component"] if not production and job.get("allow_test_subset") is True else [])),
                              "Geprüfte Navigation übernehmen; vorhandene Datei wird gesichert")
        value = json.loads(output)
        if not production:
            try:
                current = self.server_map()
                if current == name:
                    value["reload_reply"] = self.rcon("bot waypoint_load")
                    reply = value["reload_reply"] or ""
                    value["reload_confirmed"] = "Loaded Waypoints." in reply and "ERROR" not in reply.upper()
                    if value["reload_confirmed"]:
                        value["reload_note"] = "WAY-Datei installiert und im laufenden Spiel nachgeladen. Kein Map-Neustart erforderlich."
                    else:
                        value["reload_note"] = ("WAY-Datei wurde installiert. Der Ladebefehl wurde einmal gesendet; "
                                                "die Übernahme im laufenden Spiel ist noch nicht bestätigt.")
                else:
                    value["reload_note"] = (f"WAY-Datei wurde installiert. Die Testinstanz läuft auf {current}; "
                                            f"die Navigation wird beim nächsten Laden von {name} verwendet.")
            except RuntimeError as exc:
                value["reload_note"] = "WAY-Datei wurde installiert; Nachladen im Spiel nicht bestätigt. " + str(exc)
            value["note"] = "Testnavigation übernommen. Missionsziele und Leiter-/Sprungwege benötigen weiterhin gesonderte Tests."
            if value.get("test_subset"):
                value["note"] = (f"{value['installed_nodes']} von {value['nodes']} Punkten für das Testspiel übernommen. "
                                 f"{len(value['excluded_test_source_ids'])} Punkte ohne gegenseitigen Anschluss bleiben im Entwurf erhalten, "
                                 "werden aber im Testspiel nicht benutzt. Keine vollständige Map-Freigabe.")
        return value

    def action_apply_draft(self, job):
        name = job["map"]
        draft = self.drafts.get(name)
        if not draft:
            raise RuntimeError("Kein Bearbeitungsentwurf geöffnet. Zuerst bearbeiten oder archivierten Entwurf wieder öffnen.")
        revision = draft["revision"]
        environment = self.draft_environment(name)
        self.update(job, progress="1/4 Änderungen lokal sichern")
        self.drafts.save(name, revision, environment)
        self.update(job, progress="2/4 Änderungen als Prüfkandidat übernehmen")
        self.drafts.export_candidate(name, revision, environment, self.preview)
        self.action_load_map(job)
        self.update(job, progress="3/4 Bearbeitete Wege im Spiel prüfen")
        checked = self.action_native_check(job)
        self.update(job, progress="4/4 Geprüfte Änderungen ins Testspiel laden")
        installed = self.install(job, False)
        if not installed.get("reload_confirmed"):
            raise RuntimeError("Änderungen wurden gesichert und installiert, aber das Neuladen im Spiel wurde nicht bestätigt. "
                               + str(installed.get("reload_error") or installed.get("reload_reply") or "Keine Ladebestätigung."))
        installed["draft_revision"] = revision
        installed["note"] = (f"Änderungen aus Entwurf Revision {revision} sind im Testspiel geladen. " + installed.get("note", ""))
        return installed

    def action_install_test(self, job):
        return self.install(job, False)

    def action_install_production(self, job):
        return self.install(job, True)


class StudioHTTPServer(ThreadingHTTPServer):
    # Windows SO_REUSEADDR can bind beside another HTTP server and route some
    # requests to that process. Require exclusive ownership of this listener.
    allow_reuse_address = False

    def server_bind(self):
        if os.name == "nt" and hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        super().server_bind()

    def handle_error(self, request, client_address):
        # A tab closing while assets stream is not a generator failure.
        if isinstance(sys.exc_info()[1], (ConnectionError, BrokenPipeError)):
            return
        super().handle_error(request, client_address)


class Handler(BaseHTTPRequestHandler):
    server_version = "TCE2NavStudio/1.0"

    def log_message(self, pattern, *args):
        # Texture loads are numerous; avoid filling the user's disk/console.
        if args and str(args[1] if len(args) > 1 else "").startswith("5"):
            sys.stderr.write((pattern % args) + "\n")

    @property
    def studio(self):
        return self.server.studio

    def allowed_origin(self):
        host = self.headers.get("Host", "")
        allowed = {f"127.0.0.1:{self.server.server_port}", f"localhost:{self.server.server_port}"}
        if host not in allowed:
            return False
        origin = self.headers.get("Origin")
        if origin and origin not in {"http://" + h for h in allowed}:
            return False
        return self.headers.get("Sec-Fetch-Site") not in ("cross-site",)

    def send(self, status, content, mime="application/json; charset=utf-8"):
        if not isinstance(content, bytes):
            from edit import json_flags
            content = json.dumps(json_flags(content), ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        try:
            self.send_response(status)
            self.send_header("Content-Type", mime)
            self.send_header("Content-Length", str(len(content)))
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("X-Frame-Options", "DENY")
            self.send_header("Cache-Control", "no-store" if mime.startswith("application/json")or mime=='application/zip' else
                             "no-cache" if mime.split(";")[0] in ("text/html", "text/css", "text/javascript", "application/javascript") else
                             "private, max-age=60")
            self.send_header("Content-Security-Policy", "default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; connect-src 'self'; object-src 'none'; frame-ancestors 'none'; base-uri 'self'")
            self.end_headers()
            self.wfile.write(content)
        except (ConnectionError, BrokenPipeError):
            pass

    def do_GET(self):
        if not self.allowed_origin():
            self.send(403, {"error": "Nur lokale Zugriffe derselben Oberfläche sind erlaubt."})
            return
        try:
            parsed = urlsplit(self.path)
            query = parse_qs(parsed.query)
            name = query.get("map", [self.studio.selected_map])[0]
            if parsed.path == "/api/state":
                return self.send(200, self.studio.state())
            if parsed.path == "/api/settings":
                return self.send(200, self.studio.settings())
            if parsed.path == "/api/spawns":
                return self.send(200,self.studio.spawn_layer(name))
            if parsed.path == "/api/projects":
                return self.send(200, self.studio.project_state())
            if parsed.path == '/api/botmenu':
                return self.send(200, botmenu.catalog(self.studio.game_mode))
            if parsed.path == '/api/gamecontext':
                return self.send(200, {'map':self.studio.server_map(), 'gametype':self.studio.live_gametype()})
            if parsed.path == "/api/navbundle":
                bundles = self.studio.nav_bundles(query.get('gametype',[None])[0])
                result = bundles.read(name)
                result['mapscript'] = mapscript.read(self.studio.assets,name,result['files']['script'].get('text',''), bundles.gametype,directory=self.studio.script_directory())
                result['gametype_availability']=self.studio.available_game_types(name)
                result['custommapscript'] = mapscript.custom(self.studio,name,bundles.gametype)
                result['names'] = [n for n in bundles.names() if self.studio.map_in_mode(n)]
                result['game_mode'] = self.studio.game_mode
                return self.send(200, result)
            if parsed.path == "/api/navbundle.zip":
                if self.studio.game_mode=='tce2':mapscript.require_gametype(self.studio.assets,name,int(query.get('gametype',[self.studio.active_project['gametype']])[0]),self.studio.script_directory())
                return self.send(200, self.studio.nav_bundles(query.get('gametype',[None])[0]).archive(name), "application/zip")
            if parsed.path == '/api/project-package.zip':
                return self.send(200,self.studio.export_project_package(self.studio.require_map(name)), 'application/zip')
            if parsed.path == "/api/scene":
                return self.send(200, self.studio.scene(name, query.get("meshes", ["1"])[0] != "0"))
            if parsed.path == "/api/draft":
                name = self.studio.require_map(name)
                return self.send(200, {"draft": self.studio.drafts.view(name, self.studio.draft_environment(name))})
            if parsed.path == "/api/collision":
                return self.send(200, self.studio.collision(name))
            if parsed.path in ("/api/asset_manifest", "/assets/bsp/manifest.php"):
                return self.send(200, self.studio.manifest(name))
            if parsed.path in ("/api/shaders", "/assets/bsp/shaders.php"):
                return self.send(200, self.studio.shaders(name))
            if parsed.path in ("/api/bsp", "/api/texture", "/api/shader", "/api/model"):
                self.studio.require_map(name)
                asset = "maps/" + name + ".bsp" if parsed.path == "/api/bsp" else query.get("name", [""])[0]
                normalized = asset_name(asset)
                extension = Path(normalized).suffix
                allowed = {"/api/bsp": {".bsp"}, "/api/texture": {".tga", ".jpg", ".jpeg", ".png", ".webp"},
                           "/api/shader": {".shader", ".skin"}, "/api/model": {".md3", ".mdc", ".mds", ".mdm", ".mdx"}}
                if extension not in allowed[parsed.path]:
                    raise ValueError("Assettyp passt nicht zum Endpunkt.")
                content = self.studio.assets.read(normalized, maximum=256 * 1024 * 1024 if extension == ".bsp" else 64 * 1024 * 1024)
                mime = "text/plain; charset=utf-8" if extension in (".shader", ".skin") else mimetypes.guess_type(normalized)[0] or "application/octet-stream"
                return self.send(200, content, mime)
            filename = "index.html" if parsed.path == "/" else parsed.path.lstrip("/")
            if filename not in STATIC_FILES:
                raise FileNotFoundError("Nicht gefunden.")
            path = self.studio.resources / filename
            return self.send(200, path.read_bytes(), mimetypes.guess_type(filename)[0] or "application/octet-stream")
        except FileNotFoundError as exc:
            self.send(404, {"error": str(exc)})
        except (ValueError, OSError, zipfile.BadZipFile) as exc:
            self.send(400, {"error": str(exc)})
        except Exception as exc:
            self.send(500, {"error": str(exc)})

    def do_POST(self):
        if not self.allowed_origin():
            return self.send(403, {"error": "Fremde Origin oder Hostadresse."})
        if self.path not in ("/api/action", "/api/edit", "/api/project", "/api/spawns", "/api/goals", "/api/settings", "/api/navbundle", "/api/botmenu") or self.headers.get("Content-Type", "").split(";")[0] != "application/json":
            return self.send(400, {"error": "Nur JSON-Aktionen sind erlaubt."})
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length < 2 or length > (6 * 1024 * 1024 if self.path == '/api/navbundle' else 16384):
                raise ValueError("Ungültige Anfragelänge.")
            payload = json.loads(self.rfile.read(length))
            if not isinstance(payload, dict):
                raise ValueError("JSON-Objekt erwartet.")
            supplied = self.headers.get("X-Navstudio-Token") or payload.get("token", "")
            if not isinstance(supplied, str) or not secrets.compare_digest(supplied, self.studio.token):
                return self.send(403, {"error": "Aktionstoken fehlt oder ist abgelaufen. Oberfläche neu laden."})
            if self.path == "/api/edit":
                return self.send(200, self.studio.edit(payload))
            if self.path == "/api/settings":
                return self.send(200, self.studio.configure(payload))
            if self.path == "/api/spawns":
                return self.send(200,self.studio.edit_spawns(payload))
            if self.path == "/api/goals":
                return self.send(202,{"job": self.studio.submit(dict(action="generate_goals" if payload.get("op")=="generate" else "apply_goals",map=payload.get("map"),gametype=payload.get("gametype",2)))})
            if self.path == "/api/project":
                return self.send(200, self.studio.project(payload))
            if self.path == '/api/botmenu':
                return self.send(200, botmenu.execute(self.studio, payload))
            if self.path == '/api/navbundle':
                with self.studio.lock:
                    if self.studio.busy: raise RuntimeError('Wait for the running Studio job.')
                    if payload.get('game_mode') != self.studio.game_mode: raise RuntimeError('Game mode changed. Reload the script.')
                    bundles=self.studio.nav_bundles(payload.get('gametype'))
                    if self.studio.game_mode=='tce2':mapscript.require_gametype(self.studio.assets,payload.get('map'),bundles.gametype,self.studio.script_directory())
                    if payload.get('kind')=='custommapscript':
                        return self.send(200,mapscript.custom(self.studio,payload.get('map'),bundles.gametype,payload.get('text'),payload.get('sha256')))
                    if payload.get('operation')=='generate_spawns':
                        if bundles.gametype != 7: raise ValueError('GT7 auswaehlen.')
                        return self.send(200,mapscript.generate_spawns(self.studio,payload.get('map'),directory=self.studio.script_directory()))
                    if payload.get('operation')=='create_pair':
                        return self.send(200,bundles.create_pair(payload.get('map')))
                    return self.send(200, bundles.save(payload.get('map'), payload.get('kind'), payload.get('text'), payload.get('sha256')))
            return self.send(202, {"job": self.studio.submit(payload)})
        except RuntimeError as exc:
            self.send(409, {"error": str(exc)})
        except (ValueError, OSError) as exc:
            self.send(400, {"error": str(exc)})


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace")
    parser.add_argument("--settings-file", help="persistent settings JSON, default beside packaged program")
    parser.add_argument("--port", type=int, default=8766)
    parser.add_argument("--rcon-port", type=int, default=27986)
    parser.add_argument("--maps-dir", type=Path, action="append", help="additional local asset/search directory")
    parser.add_argument("--no-browser", action="store_true")
    args = parser.parse_args(argv)
    if not 1024 <= args.port <= 65535 or not 1 <= args.rcon_port <= 65535:
        parser.error("Ungültiger Port.")
    workspace = discover_workspace(args.workspace)
    url = f"http://127.0.0.1:{args.port}/"
    # A second double-click reuses the first workspace server, not another game
    # or another HTTP process. Do not commandeer an unrelated local service.
    try:
        with urlopen(url + "api/state", timeout=10) as response:
            state = json.load(response)
        if state.get("app") == APP_ID and Path(state.get("workspace", "")).resolve() == workspace:
            if not args.no_browser:
                webbrowser.open(url)
            print("Vorhandenes NavStudio geöffnet: " + url)
            return 0
    except (OSError, ValueError):
        pass
    studio = Studio(args)
    try:
        server = StudioHTTPServer(("127.0.0.1", args.port), Handler)
    except OSError as exc:
        studio.executor.shutdown(wait=False)
        raise RuntimeError(f"Port {args.port} wird bereits von einem anderen Dienst verwendet.") from exc
    server.studio = studio
    server.daemon_threads = True
    print("TCE2 NavStudio: " + url)
    print("Workspace: " + str(studio.workspace))
    print("Vorhandene Testinstanz: " + str(studio.test))
    if not args.no_browser:
        threading.Timer(.3, lambda: webbrowser.open(url)).start()
    try:
        server.serve_forever(poll_interval=.25)
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
        studio.executor.shutdown(wait=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
