"""Run authenticated historical Win32 DLL comparisons without starting ET."""
import argparse
from collections import Counter
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time
import xml.etree.ElementTree as ET

from inventory import configured_commands, project_sources, write_inventory


HERE = Path(__file__).resolve().parent


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def comparison_contract(row):
    """Accepted product extensions must never inflate strict equality totals."""
    if row.get("target") in ("tce_geoip_test", "tce_score_packet_test"):
        return "portable-contract"
    return "accepted-extension" if (
        "accepted-extension" in row.get("labels", []) or any(
            arg.startswith("--accepted-") or arg == "--capacity-contract"
            for arg in row["command"][1:])) else "strict-original"


def source_hashes(repo):
    value = {p.relative_to(repo).as_posix(): digest(p)
            for directory in ("src/tests", "src/game", "src/cgame", "src/ui", "compat", "platform")
            for p in (repo / directory).rglob("*")
            if p.is_file() and p.suffix in (".c", ".cpp", ".h", ".inc", ".py", ".json")}
    for name in ("run.py", "inventory.py", "reference-policy.json"):
        path = HERE / name
        value[path.relative_to(repo).as_posix()] = digest(path)
    return value


def authenticate(repo, policy):
    rows = []
    for group in ("required_windows", "linux_inventory_only"):
        for relative, expected in policy[group].items():
            path = repo / relative
            actual = digest(path) if path.is_file() else None
            rows.append({"path": relative, "expected_sha256": expected, "actual_sha256": actual,
                         "authenticated": actual == expected, "executed_by_this_runner": group == "required_windows"})
            if group == "required_windows" and actual != expected:
                raise RuntimeError("Address-specific original DLL identity mismatch: " + relative)
    return rows


def command(argv, cwd, log, timeout=None):
    started = time.monotonic()
    try:
        with log.open("wb") as stream:
            result = subprocess.run(argv, cwd=cwd, stdin=subprocess.DEVNULL, stdout=stream,
                                    stderr=subprocess.STDOUT, timeout=timeout,
                                    creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        code = result.returncode
    except subprocess.TimeoutExpired:
        code = "timeout"
    return {"command": argv, "exit_code": code, "seconds": round(time.monotonic() - started, 3), "log": str(log)}


def junit_rows(path):
    if not path.is_file():
        return []
    rows = []
    for case in ET.parse(path).getroot().iter("testcase"):
        fail = case.find("failure")
        skip = case.find("skipped")
        message = fail.attrib.get("message", "") if fail is not None else skip.attrib.get("message", "") if skip is not None else ""
        output = case.findtext("system-out", "")
        not_run = skip is not None or case.attrib.get("status", "").lower() in ("notrun", "not-run", "disabled") or message.lower() == "not run"
        rows.append({"name": case.attrib.get("name"), "seconds": case.attrib.get("time"),
                     "status": "not-run" if not_run else "failed" if fail is not None else "passed",
                     "failure_message": message or None,
                     "output": output[:8000],
                     "diagnostic": ((message + "\n" + (fail.text or "") + "\n" + output)[:8000] if fail is not None else
                                    (message + "\n" + (skip.text or "") + "\n" + output)[:8000] if skip is not None else None)})
    return rows


def write_summary(report, output):
    results = report.get("test_results", [])
    counts = Counter(row["status"] for row in results)
    builds = [step for step in report["steps"] if "--build" in step["command"]]
    failed_builds = [step for step in builds if step["exit_code"] != 0]
    lines = ["# Original-DLL comparison results", "",
             f"Selected: {len(report.get('selected_tests', []))}; recorded: {len(results)}; "
             f"passed: {counts['passed']}; failed: {counts['failed']}; not run: {counts['not-run']}.", "",
             f"Build batches: {len(builds)}; failed batches: {len(failed_builds)}. "
             f"Bundle exit: {report['exit_code']}.", "",
             "These are isolated Win32 comparisons with controlled dependencies. "
             "They do not establish whole-module, live-engine, visual, audio or Linux parity.", ""]
    for contract in ("strict-original", "accepted-extension", "portable-contract"):
        separate = Counter(row["status"] for row in results
                           if row.get("comparison_contract", "strict-original") == contract)
        lines.append(f"{contract}: passed {separate['passed']}; failed {separate['failed']}; "
                     f"not run {separate['not-run']}.")
    lines += ["Accepted-extension and portable contracts are separate from strict original equality.", ""]
    if report.get("error"):
        lines += ["Runner error: " + report["error"], ""]
    if report.get("changed_source_inputs_during_run"):
        lines += ["Source inputs changed during execution; current-source bundle provenance is incomplete:", ""]
        lines += ["- " + path for path in report["changed_source_inputs_during_run"]]
        lines.append("")
    if failed_builds:
        lines += ["## Build failures", ""]
        lines += [f"- {Path(step['log']).name}: exit {step['exit_code']}" for step in failed_builds]
        lines.append("")
    failures = [row for row in results if row["status"] != "passed"]
    if failures:
        lines += ["## Comparisons requiring review", "",
                  "A crash/timeout is not an equality result. A nonzero comparison may reflect "
                  "a stale harness, deliberate product extension or real source mismatch; classify before changing expectations.", "",
                  "| Test | Status | CTest reason | First diagnostic line |", "| --- | --- | --- | --- |"]
        for row in failures:
            diagnostic = next((line.strip() for line in row.get("output", "").splitlines() if line.strip()), "")
            clean = lambda value: str(value or "").replace("|", "\\|").replace("\n", " ")
            lines.append(f"| {clean(row['name'])} | {row['status']} | {clean(row.get('failure_message'))} | {clean(diagnostic[:240])} |")
    lines += ["", "See results.json for commands, reference authentication, executable/source hashes "
              "and exclusions; see ctest.log/XML and build logs for full diagnostics."]
    (Path(output) / "summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, default=HERE.parents[1])
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--configuration", choices=("Release", "Debug"), default="Release")
    parser.add_argument("--tests", nargs="+")
    parser.add_argument("--all-reference", action="store_true", help="Select all configured TC:E tests, not just the bounded default bundle")
    parser.add_argument("--include-accepted-extensions", action="store_true",
                        help="Also select separate accepted-extension contracts with --all-reference; never count them as strict original equality")
    parser.add_argument("--include-portable-contracts", action="store_true",
                        help="Also run portable GeoIP/score-packet contracts, separately from original-DLL equality")
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--test-timeout", type=int, default=120)
    parser.add_argument("--skip-configure", action="store_true")
    parser.add_argument("--skip-build", action="store_true", help="Execute existing binaries; does not establish current-source build provenance")
    args = parser.parse_args()
    repo = args.workspace.resolve()
    build = (args.build_dir or repo / "build/parity-win32").resolve()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if os.name != "nt":
        parser.error("This executor is Win32/MSVC-specific; Linux inventory is not an execution substitute.")
    if args.jobs < 1 or args.test_timeout < 1:
        parser.error("jobs and timeout must be positive")
    # Child differential programs must not open crash/file-error dialogs.
    ctypes.windll.kernel32.SetErrorMode(0x0001 | 0x0002 | 0x8000)
    policy = json.loads((HERE / "reference-policy.json").read_text(encoding="utf-8"))
    report = {"schema": 1, "started": time.strftime("%Y-%m-%dT%H:%M:%S"),
              "scope": "isolated original-DLL versus current source comparisons with each harness's mocks",
              "build_directory": str(build), "configuration": args.configuration,
              "limits": policy["limits"], "whole_module_parity": False, "game_started": False,
              "steps": [], "test_results": []}
    status = 1
    try:
        report["references_before"] = authenticate(repo, policy)
        report["source_and_harness_hashes_before"] = source_hashes(repo)
        cache = build / "CMakeCache.txt"
        if cache.is_file():
            text = cache.read_text(encoding="utf-8", errors="replace")
            match = re.search(r"^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$", text, re.M)
            if match and Path(match[1].strip()).resolve() != repo:
                raise RuntimeError("Existing cache belongs to another source root; choose a fresh --build-dir.")
            if "CMAKE_GENERATOR_PLATFORM:INTERNAL=Win32" not in text:
                raise RuntimeError("Reference execution requires a Win32 cache; choose a fresh --build-dir.")
        if not args.skip_configure:
            step = command(["cmake", "-S", str(repo), "-B", str(build), "-A", "Win32",
                            "-DBUILD_TESTING=ON", "-DTCE_BUILD_REFERENCE_TESTS=ON", "-DFEATURE_OMNIBOT=OFF",
                            "-DFEATURE_NAVSTUDIO=OFF", "-DFEATURE_LUA=OFF"], repo, output / "configure.log")
            report["steps"].append(step)
            if step["exit_code"] != 0:
                raise RuntimeError("Reference configure failed; inspect configure.log")
        commands = configured_commands(build, args.configuration)
        report["separate_accepted_extension_contracts"] = sorted(
            name for name, row in commands.items() if comparison_contract(row) == "accepted-extension")
        selected = args.tests or (sorted(n for n, row in commands.items() if n.startswith("tce_") and (
                                      any("tcetest" in a for a in row["command"][1:]) or n in policy.get("approved_wrappers", {})) and (
                                      args.include_accepted_extensions or comparison_contract(row) == "strict-original"))
                                  if args.all_reference else policy["default_tests"])
        if args.include_portable_contracts:
            selected = sorted(set(selected) | {name for name, row in commands.items()
                                               if comparison_contract(row) == "portable-contract"})
        unknown = sorted(set(selected) - commands.keys())
        if unknown:
            raise RuntimeError("Requested tests not configured: " + ", ".join(unknown))
        audit = []
        safe = []
        # Entity-spawn mocks are not process launches. Only match the concrete
        # CRT/POSIX process entry points, including MSVC's optional wide forms.
        process_api = re.compile(r"\b(?:CreateProcess(?:A|W)?|ShellExecute(?:Ex)?(?:A|W)?|WinExec|_w?spawn(?:l|le|lp|lpe|v|ve|vp|vpe)|posix_spawnp?|system|_?popen|OpenProcess|WriteProcessMemory)\s*\(")
        for name in selected:
            row = commands[name]
            files = [Path(p) for p in project_sources(build, row["target"])
                     if Path(p).is_relative_to(repo / "src/tests")]
            findings = []
            wrapper = policy.get("approved_wrappers", {}).get(name)
            if wrapper:
                script = repo / wrapper["script"]
                if (not script.is_file() or digest(script) != wrapper["sha256"] or
                        str(script.resolve()).lower() not in [str(Path(a).resolve()).lower() for a in row["command"][1:]] or
                        row["target"] != wrapper["native_target"]):
                    findings.append({"reason": "Reviewed fixture wrapper identity or native target changed."})
            elif row["native_executable"] != row["command"][0]:
                findings.append({"reason": "Unreviewed non-native command wrapper."})
            for source in files:
                for number, line in enumerate(source.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                    if process_api.search(line):
                        findings.append({"file": str(source), "line": number, "source": line[:300]})
            if not row["target"] or not files:
                findings.append({"reason": "No native test target/source provenance; not authorized for automatic execution."})
            audit.append({"name": name, "target": row["target"], "test_sources": [str(p) for p in files],
                          "approved_wrapper": wrapper,
                          "excluded": bool(findings), "process_api_findings": findings})
            if not findings:
                safe.append(name)
        report["execution_safety_audit"] = audit
        report["excluded_tests"] = [r for r in audit if r["excluded"]]
        selected = safe
        if not selected:
            raise RuntimeError("No eligible isolated native original-DLL tests remain after the process audit.")
        report["selected_tests"] = selected
        report["selected_comparison_contracts"] = {name: comparison_contract(commands[name]) for name in selected}
        targets = sorted({commands[name]["target"] for name in selected if commands[name]["target"]})
        report["selected_targets"] = targets
        write_inventory(repo, build, output, args.configuration)
        if not args.skip_build:
            # Preserve build failures as failures. CTest still records which
            # selected programs were runnable, missing, crashed or different.
            batches = [[target] for target in targets] if args.all_reference else [targets]
            for index, batch in enumerate(batches, 1):
                log = output / ("build-" + batch[0] + ".log" if args.all_reference else "build.log")
                step = command(["cmake", "--build", str(build), "--config", args.configuration,
                                "--target", *batch, "--parallel", str(args.jobs)], repo, log)
                report["steps"].append(step)
                (output / "build-progress.json").write_text(json.dumps({
                    "built_attempts": index, "total_batches": len(batches), "last_targets": batch,
                    "failed_batches": [r for r in report["steps"] if r["exit_code"] != 0]
                }, indent=2) + "\n", encoding="utf-8", newline="\n")
                if args.all_reference and (index % 20 == 0 or step["exit_code"] != 0):
                    print("Reference build", index, "/", len(batches), batch[0], "exit", step["exit_code"], flush=True)
        report["build_provenance"] = "existing binaries; current-source build not established" if args.skip_build else "requested source targets; inspect build exit code"
        report["build_cache_sha256"] = digest(cache) if cache.is_file() else None
        report["executables_before_tests"] = [{
            "test": name, "path": commands[name]["native_executable"],
            "sha256": digest(commands[name]["native_executable"]) if Path(commands[name]["native_executable"]).is_file() else None
        } for name in selected]
        expression = "^(" + "|".join(re.escape(name) for name in selected) + ")$"
        junit = output / "ctest.xml"
        if junit.exists():
            raise RuntimeError("Output already has ctest.xml; choose a new output directory to preserve attempts.")
        report["steps"].append(command(["ctest", "--test-dir", str(build), "-C", args.configuration,
                                         "--output-on-failure", "--no-tests=error", "--timeout", str(args.test_timeout),
                                         "--output-junit", str(junit), "-R", expression],
                                        repo, output / "ctest.log"))
        report["test_results"] = junit_rows(junit)
        for row in report["test_results"]:
            row["comparison_contract"] = report["selected_comparison_contracts"].get(row["name"], "unknown")
        report["references_after"] = authenticate(repo, policy)
        report["source_and_harness_hashes"] = source_hashes(repo)
        before = report["source_and_harness_hashes_before"]
        after = report["source_and_harness_hashes"]
        report["changed_source_inputs_during_run"] = sorted(
            path for path in before.keys() | after.keys() if before.get(path) != after.get(path))
        complete = len(report["test_results"]) == len(selected)
        status = 0 if complete and not report["changed_source_inputs_during_run"] and all(s["exit_code"] == 0 for s in report["steps"]) and all(
            t["status"] == "passed" for t in report["test_results"]) else 1
        report["selected_bundle_passed"] = status == 0
    except (OSError, RuntimeError, ValueError) as exc:
        report["error"] = str(exc)
        print(str(exc), flush=True)
    finally:
        report["exit_code"] = status
        (output / "results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
        write_summary(report, output)
        print("Selected differential bundle exit:", status, "report:", output / "results.json", flush=True)
    return status


if __name__ == "__main__":
    raise SystemExit(main())
