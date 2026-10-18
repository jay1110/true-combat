"""Packaged NavStudio entry point and explicitly selected offline workers."""
from pathlib import Path
import runpy
import sys


def main():
    root = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parents[3]))
    tool_root = root / "src" / "tools"
    if len(sys.argv) > 1 and sys.argv[1] == "--worker":
        workers = {
            "bsp": tool_root / "tce_bsp_navmesh.py",
            "waygraph": tool_root / "navmesh" / "waygraph.py",
            "audit": tool_root / "tce_navgen.py",
            "review": tool_root / "navmesh" / "report.py",
        }
        if len(sys.argv) < 3 or sys.argv[2] not in workers:
            raise SystemExit("Unknown offline worker")
        script = workers[sys.argv[2]]
        sys.path[:0] = [str(tool_root), str(script.parent)]
        sys.argv = [str(script)] + sys.argv[3:]
        runpy.run_path(str(script), run_name="__main__")
        return
    sys.path[:0] = [str(tool_root / "navstudio"), str(tool_root), str(tool_root / "navmesh")]
    from desktop import main as desktop_main
    desktop_main()


if __name__ == "__main__":
    main()
