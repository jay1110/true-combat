"""Resource locations shared by source, workers and the frozen desktop app."""
from pathlib import Path
import sys


def bundle_root():
    if getattr(sys, "frozen", False):
        return Path(sys._MEIPASS).resolve()
    return Path(__file__).resolve().parent.parent


REPO_ROOT = bundle_root()
APP_ROOT = REPO_ROOT / "navstudio"
WORKER_ROOT = APP_ROOT / "workers"
NAVMESH_ROOT = APP_ROOT / "navmesh"


def install_import_paths():
    for directory in (REPO_ROOT, APP_ROOT, WORKER_ROOT, NAVMESH_ROOT):
        value = str(directory)
        if value not in sys.path:
            sys.path.insert(0, value)


def worker_path(name, app_root=None):
    app = Path(app_root) if app_root is not None else APP_ROOT
    paths = {
        "bsp": app / "workers" / "tce_bsp_navmesh.py",
        "waygraph": app / "navmesh" / "waygraph.py",
        "audit": app / "workers" / "tce_navgen.py",
        "review": app / "navmesh" / "report.py",
    }
    if name not in paths:
        raise ValueError("Unknown offline worker: " + str(name))
    return paths[name]
