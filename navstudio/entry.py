"""Packaged NavStudio entry point and explicitly selected offline workers."""
import runpy
import sys
from layout import install_import_paths, worker_path


def main():
    install_import_paths()
    if len(sys.argv) > 1 and sys.argv[1] == "--worker":
        if len(sys.argv) < 3:
            raise SystemExit("Unknown offline worker")
        script = worker_path(sys.argv[2])
        sys.argv = [str(script)] + sys.argv[3:]
        runpy.run_path(str(script), run_name="__main__")
        return
    from desktop import main as desktop_main
    desktop_main()


if __name__ == "__main__":
    main()
