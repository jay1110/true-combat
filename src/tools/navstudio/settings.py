"""Persistent paths for one existing game installation; never copy a game tree."""
import json
from pathlib import Path


def validate_settings(value):
    root = Path(str(value.get("instance_path", ""))).expanduser()
    if not root.is_absolute():
        raise ValueError("Spielordner muss ein absoluter Pfad sein.")
    root = root.resolve()
    if not all((root / part).is_dir() for part in ("etmain", "tce2")):
        raise ValueError("Spielordner benötigt die vorhandenen Unterordner etmain und tce2.")
    exe = Path(str(value.get("game_executable", ""))).expanduser()
    if not exe.is_absolute():
        raise ValueError("Spielprogramm muss ein absoluter Pfad sein.")
    exe = exe.resolve()
    if exe.parent != root or exe.name.lower() not in ("et.exe", "etl.exe") or not exe.is_file():
        raise ValueError("Vorhandene ET.exe oder etl.exe direkt im Spielordner auswählen.")
    dirs = value.get("maps_dirs", [])
    if not isinstance(dirs, list) or len(dirs) > 32:
        raise ValueError("Höchstens 32 zusätzliche Mapordner angeben.")
    normalized = []
    for item in dirs:
        path = Path(str(item)).expanduser()
        if not path.is_absolute() or not path.is_dir():
            raise ValueError("Mapordner existiert nicht: " + str(path))
        text = str(path.resolve())
        if text not in normalized:
            normalized.append(text)
    return {"schema": 1, "instance_path": str(root), "game_executable": str(exe), "maps_dirs": normalized}


def save_settings(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)
