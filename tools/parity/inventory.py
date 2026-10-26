"""Inventory differential evidence without promoting helper tests to parity."""
import argparse
from collections import Counter
import hashlib
from functools import lru_cache
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET


DOMAINS = {
    "movement_prediction": r"movement|predict|pm_|duck|prone|step|lean|jump|fall|speed",
    "animation": r"anim|frame|tag|character|lerp",
    "weapons_reload_zoom": r"weapon|reload|aim|zoom|ammo|fire|attack|recoil|eject|grenade",
    "damage_projectiles": r"damage|bullet|missile|trajectory|fragment|impact|projectile|hit|killed|death",
    "gt2_objectives": r"objective|flag|capture|gametype|round|spawn",
    "gt5_objectives": r"objective|bomb|plant|defuse|gametype|round|spawn",
    "gt7_rounds_spawns": r"gametype|round|spawn|respawn|session",
    "ui_hud": r"ui_|menu|hud|score|draw|text|popup|limbo|loadpanel|vote|graphics",
    "assets": r"media|shader|model|skin|cache|character|texture|graphics|asset",
    "cvars": r"cvar|config|option|controls|binding",
    "sound": r"sound|volume|footstep|audio|voice|sfx",
    "rendering": r"render|light|scene|draw|particle|wall|mark|decal|camera|view|sprite|polygon|tag|frame",
}
LIMITS = {
    "movement_prediction": "Complete multi-frame player prediction, engine traces, input and server reconciliation remain separate.",
    "animation": "Real animation files, skeletal/tag transforms and rendered transitions require asset/live comparison.",
    "weapons_reload_zoom": "Complete input-to-server/client transitions, inventory, media and all weapon variants require integration.",
    "damage_projectiles": "Real world/entity collision, timing, health, kill attribution and rendered/audio results require integration.",
    "gt2_objectives": "HUD, predicates or spawn tests do not prove USE pickup, carrying and return/capture in a match.",
    "gt5_objectives": "HUD, predicates or spawn tests do not prove plant/defuse, mission triggers or round outcomes.",
    "gt7_rounds_spawns": "Spawn/session helpers do not prove whole-match deathmatch respawn, score or round progression.",
    "ui_hud": "Trap/callback equality does not prove visible glyphs, input focus or interaction with actual menus.",
    "assets": "Handle/path checks do not prove real PK3/model/skin/shader loading or visual compatibility.",
    "cvars": "Controlled cvar values do not prove all archived/latched/runtime engine propagation.",
    "sound": "Sound call/handle/volume equality does not prove actual samples, mixing or audible results.",
    "rendering": "Draw-call equality under mocks does not prove visibility, renderer limits or final pixels.",
}


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def configured_commands(build, configuration):
    """Read generated CTest commands even before their executables exist."""
    result = {}
    for file in Path(build).rglob("CTestTestfile.cmake"):
        lines = file.read_text(encoding="utf-8").splitlines()
        for line in lines:
            match = re.search(r"add_test\(\[=\[(.*?)\]=\]\s+(.*)\)$", line)
            if not match:
                continue
            args = re.findall(r'"((?:\\.|[^"\\])*)"', match[2])
            if not args:
                continue
            command = [a.replace('\\"', '"').replace("\\\\", "\\") for a in args]
            other_configs = ("Debug", "Release", "MinSizeRel", "RelWithDebInfo")
            if any("/" + c + "/" in arg for c in other_configs if c != configuration for arg in command):
                continue
            native = next((arg for arg in command if arg.lower().endswith(".exe") and Path(arg).stem.startswith("tce_")), None)
            result[match[1]] = {"command": command, "working_directory": str(file.parent),
                                "target": Path(native).stem if native else None,
                                "native_executable": native, "labels": []}
        for line in lines:
            match = re.search(r'set_tests_properties\(\[=\[(.*?)\]=\].*?\bLABELS\s+"([^"]*)"', line)
            if match and match[1] in result:
                result[match[1]]["labels"] = match[2].split(";")
    return result


@lru_cache(maxsize=4)
def project_files(build):
    return {p.stem: p for p in Path(build).rglob("*.vcxproj")}


def project_sources(build, target):
    if not target:
        return []
    project = project_files(str(build)).get(target)
    if project is None:
        return []
    tree = ET.parse(project)
    return [n.attrib["Include"] for n in tree.iter() if n.tag.endswith("}ClCompile") and "Include" in n.attrib]


def inventory(repo, build=None, configuration="Release"):
    repo = Path(repo).resolve()
    scenarios_file = repo / "src/reconstruction/verification_scenarios.json"
    scenarios = json.loads(scenarios_file.read_text(encoding="utf-8")) if scenarios_file.is_file() else {"scenarios": []}
    tests = []
    for path in sorted((repo / "src/tests").glob("*.c")):
        source = path.read_text(encoding="utf-8", errors="replace")
        linked = []
        for scenario in scenarios["scenarios"]:
            for function in scenario.get("functions", []):
                for test in function.get("tests", []):
                    if test.get("path") == path.relative_to(repo).as_posix():
                        linked.append({"scenario": scenario["id"], "function": function["id"],
                                       "test_boundary": test.get("boundary"), "live_boundary": scenario.get("live_boundary")})
        tests.append({"path": path.relative_to(repo).as_posix(), "sha256": sha256(path),
                      "domains": [d for d, pattern in DOMAINS.items() if re.search(pattern, path.name, re.I)],
                      "loads_original_dll": bool(re.search(r"\bLoadLibrary(?:A|W)?\s*\(", source)),
                      "runtime_hook_instrumentation": "VirtualProtect" in source or "FlushInstructionCache" in source,
                      "explicit_fp_environment": bool(re.search(r"_control(?:fp|87)|fesetround|_fpreset", source)),
                      "whole_in_name_only": "whole" in path.name,
                      "controlled_provider_name_locators": sorted(set(re.findall(r"\b(?:mock|stub|fake)[A-Za-z_0-9]*", source, re.I)))[:30],
                      "documented_scenarios": linked,
                      "coverage_status": "inventory_only; source names/comments are not execution proof"})
    commands = configured_commands(build, configuration) if build else {}
    for item in commands.values():
        item["direct_project_sources"] = project_sources(build, item["target"])
    references = []
    for directory in ("tcetest", "etmain"):
        for name in ("cgame_mp_x86.dll", "qagame_mp_x86.dll", "ui_mp_x86.dll",
                     "cgame.mp.i386.so", "qagame.mp.i386.so", "ui.mp.i386.so"):
            path = repo / directory / name
            references.append({"path": path.relative_to(repo).as_posix(), "exists": path.is_file(),
                               "sha256": sha256(path) if path.is_file() else None,
                               "role": "TC:E original" if directory == "tcetest" else "SDK reference, not TC:E runtime substitute",
                               "execution_scope": "Win32 address-specific harness" if name.endswith(".dll") else "fingerprinted only; separate Linux executor required"})
    coverage = [{"domain": domain, "candidate_test_files": sum(domain in t["domains"] for t in tests),
                 "classification": "lexical inventory, not verified feature coverage", "remaining_integration": LIMITS[domain],
                 "whole_feature_parity": False} for domain in DOMAINS]
    return {"schema": 1, "test_c_files": len(tests), "configured_ctest_entries": len(commands),
            "scenarios": scenarios.get("scenarios", []), "tests": tests, "configured_commands": commands,
            "references": references, "coverage_matrix": coverage,
            "whole_module_parity": False,
            "limits": ["A C source file can contain stubs/support only or several comparisons; file counts are not function counts.",
                       "Some targets reuse shared context objects; direct_project_sources does not include their transitive closure.",
                       "Whole-in-name and source comments are declarations, not proof of exhaustive input or integration coverage."]}


def write_inventory(repo, build, output, configuration="Release"):
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    value = inventory(repo, build, configuration)
    (output / "coverage-inventory.json").write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8", newline="\n")
    lines = ["# Original / port differential coverage inventory", "",
             f"{value['test_c_files']} C files; {value['configured_ctest_entries']} configured CTest entries; "
             f"{len(value['scenarios'])} documented live scenarios. These are evidence inventories, not parity percentages.", "",
             "| Domain | Candidate C files (name matching only) | Separate integration obligation |", "| --- | ---: | --- |"]
    lines += [f"| {r['domain']} | {r['candidate_test_files']} | {r['remaining_integration']} |" for r in value["coverage_matrix"]]
    lines += ["", "Linux originals are fingerprinted, not executed by the Win32 runner. "
              "Original-vs-port callback tests and root-controlled live A/B observations must remain distinct."]
    (output / "coverage-matrix.md").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--configuration", default="Release")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    value = write_inventory(args.workspace, args.build_dir, args.output, args.configuration)
    print(f"Inventoried {value['test_c_files']} C files and {value['configured_ctest_entries']} configured tests; parity remains unproven.")


if __name__ == "__main__":
    main()
