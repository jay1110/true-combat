"""Run one serial NavStudio action, preserving evidence per map and invocation.

This client neither starts its own server nor sends RCON. Only an explicit
action POST asks the existing Studio to act on its one existing test instance.
An uncertain POST is never retried. Use --job to resume observation only.
"""

import argparse
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import sys
import time
from urllib.error import HTTPError, URLError
from urllib.request import Request
import uuid


APP_ID = "tce2-navstudio"
ET_STANDARD_MAPS = ("oasis", "battery", "goldrush", "railgun", "radar", "fueldump")
TCE_STANDARD_MAPS = ("obj_delta", "obj_northport", "obj_railhouse", "obj_snow",
                     "obj_stadtrand", "obj_village")
STANDARD_MAPS = ET_STANDARD_MAPS + TCE_STANDARD_MAPS
ACTIONS = ("load_map", "start_test", "stop_test",
           "snapshot", "rebuild", "repair", "native_check", "inspect", "install_test")
ACTIVE = {"queued", "running"}


def save_new(path, value):
    """Evidence files are never overwritten, including after a resumed wait."""
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, ensure_ascii=False)
        stream.write("\n")


class Client:
    def __init__(self, port):
        self.origin = f"http://127.0.0.1:{port}"
        # Do not route the local token through a configured HTTP proxy.
        from urllib.request import ProxyHandler, build_opener
        self.opener = build_opener(ProxyHandler({}))

    def state(self, timeout):
        with self.opener.open(self.origin + "/api/state", timeout=timeout) as response:
            return json.load(response)

    def submit(self, action, name, token, timeout):
        request = Request(self.origin + "/api/action",
                          data=json.dumps({"action": action, "map": name}).encode("utf-8"),
                          headers={"Content-Type": "application/json", "Origin": self.origin,
                                   "X-Navstudio-Token": token}, method="POST")
        with self.opener.open(request, timeout=timeout) as response:
            return json.load(response)


def validate_state(state, workspace):
    expected = workspace / "build" / "navgen-overkill"
    if (state.get("app") != APP_ID or
            Path(state.get("workspace", "")).resolve() != workspace or
            Path(state.get("test_instance", "")).resolve() != expected):
        raise ValueError("Studio app/workspace/test instance mismatch; no action sent.")


def validate_job(job, action, name, job_id=None):
    if (job.get("action") != action or job.get("map") != name or
            not re.fullmatch(r"[0-9a-f]{16}", job.get("id", "")) or
            (job_id is not None and job.get("id") != job_id)):
        raise ValueError("Job does not match the requested map, action, or ID.")


def run(args, client=None, *, clock=time.monotonic, sleep=time.sleep):
    workspace = args.workspace.resolve()
    root = workspace / "build" / "navgen-overkill"
    if not (root / "tce2").is_dir():
        raise ValueError("Existing build/navgen-overkill/tce2 not found; no instance is created.")
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    evidence = root / "navmesh-preview" / "standard-map-batch" / args.map / (stamp + "-" + uuid.uuid4().hex[:8])
    evidence.mkdir(parents=True, exist_ok=False)
    save_new(evidence / "request.json", {"schema": 1, "action": args.action, "map": args.map,
             "resume_job": args.job, "workspace": str(workspace), "port": args.port,
             "wait_seconds": args.seconds, "requested_utc": stamp})
    print("evidence " + str(evidence), flush=True)
    client = client or Client(args.port)
    end = clock() + args.seconds
    job_id = args.job
    last_job = None
    sequence = 0

    def timeout():
        return max(.05, min(10., end - clock()))

    def finish(status, code, message=None, **extra):
        result = {"schema": 1, "map": args.map, "action": args.action, "job_id": job_id,
                  "client_status": status, "message": message, "evidence": str(evidence), **extra}
        save_new(evidence / "outcome.json", result)
        print(json.dumps(result, ensure_ascii=True), flush=True)
        return code

    try:
        state = client.state(timeout())
        validate_state(state, workspace)
        if not args.job:
            if state.get("busy"):
                return finish("not_submitted", 2, "Studio is busy; wait for the existing action first.")
            if args.map not in {item["name"] for item in state.get("maps", [])}:
                return finish("not_submitted", 2, "Map is not available in this Studio asset inventory.")
            known = {job["id"] for job in state.get("jobs", [])}
            save_new(evidence / "before.json", {"existing_jobs": sorted(known),
                     "game": state.get("game"), "test_instance": state["test_instance"]})
            if clock() >= end:
                return finish("not_submitted", 2, "Wait deadline elapsed before submission.")
            try:
                # Exactly one POST. Never retry even if the connection drops
                # before the server's generated job ID reaches this client.
                response = client.submit(args.action, args.map, state["token"], timeout())
                save_new(evidence / "accepted.json", response)
                last_job = response["job"]
                validate_job(last_job, args.action, args.map)
                job_id = last_job["id"]
            except (OSError, URLError, ValueError, KeyError) as exc:
                if isinstance(exc, HTTPError) and 400 <= exc.code < 500:
                    return finish("rejected", 1, exc.read(16384).decode("utf-8", "replace"), http_status=exc.code)
                # Record possible IDs, but leave selection explicit: another
                # UI user may have submitted a similar job at the same time.
                candidates = []
                if clock() < end:
                    try:
                        fresh = client.state(timeout())
                        validate_state(fresh, workspace)
                        candidates = [job for job in fresh.get("jobs", []) if job.get("id") not in known
                                      and job.get("map") == args.map and job.get("action") == args.action]
                    except (OSError, URLError, ValueError):
                        pass
                save_new(evidence / "uncertain-post.json", {"error": str(exc), "possible_jobs": candidates})
                return finish("submission_unknown", 3, "POST was not repeated. Inspect possible_jobs; resume with --job ID.",
                              possible_job_ids=[job["id"] for job in candidates])
        else:
            last_job = next((job for job in state.get("jobs", []) if job.get("id") == job_id), None)
            if last_job is None:
                return finish("job_unavailable", 3, "Job is no longer in Studio's 20-job history or Studio restarted; no POST sent.")
            validate_job(last_job, args.action, args.map, job_id)

        print("job " + job_id, flush=True)
        previous = None
        while True:
            validate_job(last_job, args.action, args.map, job_id)
            current = (last_job.get("status"), last_job.get("progress"), last_job.get("error"))
            if current != previous:
                save_new(evidence / f"job-{sequence:04d}.json", last_job)
                sequence += 1
                print(json.dumps({"job": job_id, "status": current[0], "progress": current[1]}, ensure_ascii=True), flush=True)
                previous = current
            if last_job.get("status") not in ACTIVE:
                save_new(evidence / "job-final.json", last_job)
                return finish("finished", 0 if last_job.get("status") == "complete" else 1,
                              last_job.get("error"), job_status=last_job.get("status"))
            if clock() >= end:
                return finish("wait_timeout", 2, "Job may still be running. Resume with --job " + job_id + "; do not repeat the action.")
            sleep(min(1., max(0., end - clock())))
            if clock() >= end:
                continue
            try:
                fresh = client.state(timeout())
                validate_state(fresh, workspace)
            except (OSError, URLError) as exc:
                save_new(evidence / f"poll-error-{sequence:04d}.json", {"error": str(exc)})
                sequence += 1
                continue
            last_job = next((job for job in fresh.get("jobs", []) if job.get("id") == job_id), None)
            if last_job is None:
                return finish("job_unavailable", 3, "Observed job disappeared; no POST repeated.")
    except KeyboardInterrupt:
        return finish("interrupted", 130, "Only observation stopped; the Studio job may still be running.")
    except (OSError, URLError, ValueError, KeyError) as exc:
        return finish("client_error", 3, str(exc))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=ACTIONS)
    parser.add_argument("--map", required=True, help="Map name; standard maps: " + ", ".join(STANDARD_MAPS))
    parser.add_argument("--map-set", choices=("all", "et", "tce"), default="all",
                        help="Optional guard restricting the action to the six ET or TCE standard maps")
    parser.add_argument("--workspace", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--port", type=int, default=8766)
    parser.add_argument("--seconds", type=float, default=600., help="Maximum total wait; an unfinished job is not cancelled")
    parser.add_argument("--job", help="Resume waiting for this job ID without submitting anything")
    args = parser.parse_args(argv)
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", args.map):
        parser.error("Invalid map name")
    if args.map_set != "all" and args.map not in {"et": ET_STANDARD_MAPS, "tce": TCE_STANDARD_MAPS}[args.map_set]:
        parser.error("Map does not belong to the selected standard-map set")
    if not 1024 <= args.port <= 65535 or not 1 <= args.seconds <= 3600:
        parser.error("Port must be 1024..65535; seconds must be 1..3600")
    if args.job and not re.fullmatch(r"[0-9a-f]{16}", args.job):
        parser.error("Job ID must contain 16 lowercase hexadecimal characters")
    try:
        return run(args)
    except (ValueError, OSError) as exc:
        print(str(exc), file=sys.stderr)
        return 3


if __name__ == "__main__":
    raise SystemExit(main())
