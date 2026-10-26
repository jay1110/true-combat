"""Named, map-bound workspaces and recoverable leases of map archives.

Projects never share candidates, drafts or reports. Only one source PK3 is
leased into the existing game's etmain directory; no asset tree is copied.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import threading
import time
import uuid
import zipfile


MAP = re.compile(r"[A-Za-z0-9_-]{1,64}\Z")
IDENTITY = re.compile(r"[0-9a-f]{32}\Z")


def sha256(path):
    result = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + "." + uuid.uuid4().hex + ".tmp")
    try:
        with temporary.open("x", encoding="utf-8") as stream:
            json.dump(value, stream, ensure_ascii=False, indent=2, allow_nan=False)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if temporary.exists():
            temporary.unlink()


def read_json(path):
    path = Path(path)
    if path.stat().st_size > 1024 * 1024:
        raise ValueError("Projektdatei ist zu groß.")
    return json.loads(path.read_text(encoding="utf-8"))


def project_name(value):
    if not isinstance(value, str) or not 1 <= len(value.strip()) <= 100 or any(ord(c) < 32 for c in value):
        raise ValueError("Projektname benötigt 1 bis 100 Zeichen.")
    return value.strip()


class Projects:
    def __init__(self, directory, etmain, libraries):
        self.directory = Path(directory).resolve()
        self.etmain = Path(etmain).resolve()
        self.libraries = list(dict.fromkeys(Path(p).resolve() for p in libraries))
        self.directory.mkdir(parents=True, exist_ok=True)
        self.active = None
        self.lock = threading.RLock()
        self.archive_cache = {}
        self.catalog_records = {}

    def path(self, identity):
        if not isinstance(identity, str) or not IDENTITY.fullmatch(identity):
            raise ValueError("Ungültige Projekt-ID.")
        target = self.directory / identity
        if target.resolve().parent != self.directory:
            raise ValueError("Projektordner verlässt den Projektbereich.")
        return target

    def document(self, identity):
        value = read_json(self.path(identity) / "project.json")
        if (value.get("schema") != 1 or value.get("id") != identity or
                not isinstance(value.get("map"), str) or not MAP.fullmatch(value["map"]) or
                value.get("mod") not in ("tce2", "et") or value.get("gametype") not in (2, 5, 7)):
            raise ValueError("Projektdatei ist beschädigt oder nicht unterstützt.")
        project_name(value.get("name"))
        return value

    def view(self, document):
        root = self.path(document["id"])
        paths = {"root": str(root), "preview": str(root / "preview"),
                 "work": str(root / "work"), "drafts": str(root / "work" / "drafts"),
                 "native": str(root / "native"), "bundles": str(root / "bundles"),
                 "spawns": str(root / "work" / "spawns.json")}
        return {**document, "path": str(root / "project.json"), "paths": paths,
                "active": bool(self.active and document["id"] == self.active["id"]),
                "lease_pending": (root / "lease.json").is_file()}

    def list(self):
        values, warnings = [], []
        for root in self.directory.iterdir():
            if not root.is_dir() or not IDENTITY.fullmatch(root.name):
                continue
            try:
                values.append(self.view(self.document(root.name)))
            except (OSError, ValueError, KeyError) as exc:
                warnings.append(f"Projekt {root.name}: {exc}")
        return sorted(values, key=lambda v: v.get("updated", 0), reverse=True), warnings

    def catalog(self, assets=None):
        """Opaque catalog IDs prevent callers from supplying arbitrary file paths."""
        records, warnings = {}, []
        paths = {}
        for root in self.libraries:
            if root.is_dir():
                for path in root.glob("*.pk3"):
                    paths[path.resolve()] = "all_maps"
        if assets:
            for name, asset in assets.items():
                path = Path(asset["path"]).resolve()
                if path.suffix.lower() == ".pk3":
                    paths.setdefault(path, "installed")
        for path, location in sorted(paths.items(), key=lambda item: str(item[0]).lower()):
            try:
                stamp = path.stat()
                fingerprint = (stamp.st_mtime_ns, stamp.st_size)
                cache = self.archive_cache.get(str(path))
                if not cache or cache[0] != fingerprint:
                    with zipfile.ZipFile(path) as archive:
                        maps = sorted({match[1].lower() for info in archive.infolist()
                            if not info.is_dir() and (match := re.fullmatch(
                                r"maps/([A-Za-z0-9_-]{1,64})\.bsp", info.filename, re.IGNORECASE))})
                    self.archive_cache[str(path)] = fingerprint, maps
                else:
                    maps = cache[1]
                for name in maps:
                    identity = hashlib.sha256((str(path) + "\0" + name).encode()).hexdigest()
                    records[identity] = dict(source_id=identity, map=name,
                        mod="tce2" if name.startswith("obj_") else "et", filename=path.name,
                        source_path=str(path), location=location, size=stamp.st_size,
                        stamp=list(fingerprint))
            except (OSError, zipfile.BadZipFile) as exc:
                warnings.append(f"{path.name}: {exc}")
        # A leased PK3 disappears from its library but remains selectable in its
        # existing project. New projects use the installed record after refresh.
        self.catalog_records = records
        return sorted(records.values(), key=lambda v: (v["mod"], v["map"], v["filename"])), warnings

    def create(self, name, map_name, mod, gametype, source_id):
        with self.lock:
            self.require_closed()
            name = project_name(name)
            if (not isinstance(map_name, str) or not MAP.fullmatch(map_name) or
                    mod not in ("tce2", "et") or type(gametype) is not int or gametype not in (2, 5, 7)):
                raise ValueError("Map, Mod oder Gametype ist ungültig.")
            if map_name.lower().startswith("obj_") != (mod == "tce2"):
                raise ValueError("Map gehört nicht zum ausgewählten Mod.")
            record = self.catalog_records.get(source_id)
            if not record or record["map"] != map_name.lower() or record["mod"] != mod:
                raise ValueError("Map aus dem aktuellen Projektkatalog auswählen.")
            source = Path(record["source_path"])
            stamp = source.stat()
            if [stamp.st_mtime_ns, stamp.st_size] != record["stamp"]:
                raise RuntimeError("Map-Paket wurde geändert. Projektkatalog neu laden.")
            identity = uuid.uuid4().hex
            root = self.path(identity)
            root.mkdir()
            now = time.time()
            document = dict(schema=1, id=identity, name=name, map=map_name.lower(), mod=mod,
                gametype=gametype, created=now, updated=now, revision=0,
                source={"path": str(source), "filename": source.name, "location": record["location"],
                        "sha256": sha256(source), "size": stamp.st_size})
            write_json(root / "project.json", document)
            try:
                return self.open(identity)
            except Exception:
                # Keep a recoverable named project if a lease cannot be obtained.
                raise

    def require_closed(self):
        if self.active:
            raise RuntimeError("Aktuelles Projekt zuerst speichern und schließen.")

    def open(self, identity):
        with self.lock:
            self.require_closed()
            document = self.document(identity)
            root = self.path(identity)
            for part in ("preview", "work/drafts", "native", "bundles"):
                (root / part).mkdir(parents=True, exist_ok=True)
            self.acquire(document)
            self.active = document
            return self.view(document)

    def lease_paths(self, document, lease=None):
        source = Path(document["source"]["path"])
        target = self.etmain / document["source"]["filename"]
        if source.name != document["source"]["filename"] or source.suffix.lower() != ".pk3":
            raise ValueError("Projekt-Paketpfad ist ungültig.")
        if document["source"]["location"] == "all_maps" and source.parent.resolve() not in self.libraries:
            raise ValueError("Projekt-Paket liegt außerhalb von all_maps.")
        if target.resolve().parent != self.etmain:
            raise ValueError("Map-Paket verlässt etmain.")
        if lease and (lease.get("source") != str(source) or lease.get("target") != str(target) or
                      lease.get("sha256") != document["source"]["sha256"] or lease.get("owner") != document["id"]):
            raise ValueError("Paket-Lease gehört nicht zu diesem Projekt.")
        return source, target

    def acquire(self, document):
        root = self.path(document["id"])
        journal = root / "lease.json"
        source, target = self.lease_paths(document)
        expected = document["source"]["sha256"]
        for other in self.directory.glob("*/lease.json"):
            if other.parent == root:
                continue
            lease = read_json(other)
            if lease.get("target") == str(target):
                raise RuntimeError("Dieses Map-Paket gehört noch zu einem anderen Projekt. Dieses Projekt zuerst öffnen und schließen.")
        if journal.is_file():
            lease = read_json(journal)
            self.lease_paths(document, lease)
            if not target.exists() and source.is_file() and sha256(source) == expected:
                # Process stopped after preparing the journal but before linking.
                journal.unlink()
            elif not target.is_file() or sha256(target) != expected:
                raise RuntimeError("Zurückgebliebenes Projekt-Paket fehlt oder wurde geändert; nicht überschreiben.")
            else:
                return
        if not source.is_file() or sha256(source) != expected:
            raise RuntimeError("Map-Paket fehlt oder passt nicht mehr zum gespeicherten Projekt.")
        if document["source"]["location"] != "all_maps":
            # Installed game data stays in place. It is not owned by this project.
            return
        if target.exists():
            if not target.is_file() or sha256(target) != expected:
                raise RuntimeError("In etmain existiert ein anderes Paket mit diesem Namen. Kein Paket wurde überschrieben.")
            return
        self.etmain.mkdir(parents=True, exist_ok=True)
        lease = dict(schema=1, owner=document["id"], source=str(source), target=str(target),
                     sha256=expected, phase="prepared", created=time.time())
        write_json(journal, lease)
        try:
            # Exclusive hard-link then unlink is a no-overwrite, same-volume move.
            # Persisted journal permits recovery of either intermediate state.
            os.link(source, target)
            lease["phase"] = "linked"
            write_json(journal, lease)
            if sha256(source) != expected or sha256(target) != expected:
                raise RuntimeError("Map-Paket hat sich während des Öffnens geändert.")
            source.unlink()
            lease["phase"] = "moved"
            write_json(journal, lease)
        except Exception:
            # Keep the ownership journal: cleanup without verification would risk
            # deleting a package changed by somebody else.
            raise

    def save(self, name=None, gametype=None):
        with self.lock:
            if not self.active:
                raise RuntimeError("Zuerst ein Projekt erstellen oder öffnen.")
            document = dict(self.active)
            if name is not None:
                document["name"] = project_name(name)
            if gametype is not None:
                if type(gametype) is not int or gametype not in (2, 5, 7):
                    raise ValueError("Gametype muss 2, 5 oder 7 sein.")
                document["gametype"] = gametype
            document["revision"] += 1
            document["updated"] = time.time()
            write_json(self.path(document["id"]) / "project.json", document)
            self.active = document
            return self.view(document)

    def release(self, document):
        journal = self.path(document["id"]) / "lease.json"
        if not journal.exists():
            return
        lease = read_json(journal)
        source, target = self.lease_paths(document, lease)
        expected = lease["sha256"]
        if not target.is_file():
            if source.is_file() and sha256(source) == expected:
                journal.unlink()
                return
            raise RuntimeError("Geliehenes Map-Paket fehlt. Projekt bleibt zur Wiederherstellung geöffnet.")
        if sha256(target) != expected:
            raise RuntimeError("Geliehenes Map-Paket wurde geändert. Es wird nicht automatisch verschoben.")
        source.parent.mkdir(parents=True, exist_ok=True)
        if source.exists():
            if not source.is_file() or sha256(source) != expected:
                raise RuntimeError("In all_maps liegt bereits ein anderes Paket. Kein Paket wurde überschrieben.")
        else:
            os.link(target, source)
        if sha256(source) != expected:
            raise RuntimeError("Rückgabe-Paket konnte nicht bestätigt werden.")
        target.unlink()
        journal.unlink()

    def close(self):
        with self.lock:
            if not self.active:
                return None
            self.save()
            document = self.active
            self.release(document)
            self.active = None
            return self.view(document)

    def recovery(self):
        """Expose unfinished leases at startup; never move a running game's PK3."""
        values, _ = self.list()
        return [dict(id=p["id"], name=p["name"], map=p["map"], path=p["path"])
                for p in values if p["lease_pending"]]
