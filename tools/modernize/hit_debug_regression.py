"""Compile actual server/client hit-debug modules and pass a real event between them."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/modernize"))
from compat_validate import msvc_environment
from weapon_regression import function

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--optimized", action="store_true")
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    env, cc = msvc_environment(out, "x64" if args.sanitize else "x86", "14.44" if args.sanitize else "14.39")
    fixture = Path(__file__).with_name("hit_debug_fixture.c")
    inputs = [fixture, Path(__file__), *(ROOT / p for p in (
        "src/game/g_tce_hit_debug.c", "src/game/g_tce_hit_debug.h", "src/game/tce_hit_debug.h",
        "src/cgame/cg_tce_hit_debug.c", "src/cgame/cg_tce_hit_debug.h", "src/game/tce_damage.h", "src/game/tce_damage_trace.h", "src/game/g_vote.c"))]
    hashes = {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    generated = out / "actual-hit-debug.c"
    text = fixture.read_text()
    text = text.replace("/* ACTUAL_SERVER */", (ROOT / "src/game/g_tce_hit_debug.c").read_text().replace('#include "g_local.h"', ''))
    text = text.replace("/* ACTUAL_CLIENT */", (ROOT / "src/cgame/cg_tce_hit_debug.c").read_text().replace('#include "cg_local.h"', ''))
    text = text.replace("/* ACTUAL_ANTILAG_VOTE */", function((ROOT / "src/game/g_vote.c").read_text(), "G_AntiLag_v"))
    generated.write_text(text, encoding="utf-8", newline="\n")
    rows = []
    for mode in ("server", "client"):
        exe = out / (mode + ".exe")
        command = [cc, "/nologo", "/TC", "/W3", "/O2" if args.optimized else "/Od", "/DWIN32", "/D_CRT_SECURE_NO_WARNINGS",
                   "/I" + str(ROOT), "/I" + str(ROOT / "src/game"), "/I" + str(ROOT / "src/cgame"),
                   "/DCGAMEDLL" if mode == "client" else "/DGAMEDLL"]
        if mode == "client": command.append("/DHIT_DEBUG_CLIENT")
        if args.sanitize: command += ["/fsanitize=address", "/Zi", "/Fd:" + str(out / (mode + ".pdb"))]
        command += [str(generated), "/Fe:" + str(exe), "/Fo:" + str(out / (mode + ".obj"))]
        built = subprocess.run(command, env=env, cwd=out, text=True, capture_output=True)
        (out / (mode + ".compile.log")).write_text(built.stdout + built.stderr, encoding="utf-8")
        row = {"mode": mode, "command": command, "build_exit": built.returncode}
        if not built.returncode:
            run = subprocess.run([str(exe), str(out / "actual-server-event.bin")], env=env, cwd=out, text=True, capture_output=True)
            row.update(exit_code=run.returncode, stdout=run.stdout, stderr=run.stderr)
            print(run.stdout + run.stderr, end="")
        else: print(built.stdout + built.stderr)
        rows.append(row)
        if row.get("exit_code", built.returncode): break
    stable = hashes == {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    report = {"sources": hashes, "source_stable": stable, "compiler": cc, "rows": rows,
              "limits": ["Deterministic mocked engine/asset services; no live rasterizer, engine delta serialization or affected mesh proof."]}
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return int(not stable or len(rows) != 2 or any(row.get("exit_code", row["build_exit"]) for row in rows))

if __name__ == "__main__": raise SystemExit(main())
