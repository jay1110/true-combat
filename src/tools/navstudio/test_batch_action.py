"""Offline regression checks for serial dispatch and durable map evidence."""
import argparse
import contextlib
import copy
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import batch_action


JOB_ID = "0123456789abcdef"


class FakeClient:
    def __init__(self, root, statuses=("running", "complete"), uncertain=False):
        self.root = root
        self.statuses = list(statuses)
        self.uncertain = uncertain
        self.posts = 0
        self.job = {"id": JOB_ID, "action": "native_check", "map": "goldrush", "status": "queued"}
        self.busy = False
        self.states = 0

    def state(self, timeout):
        self.states += 1
        result = {"app": batch_action.APP_ID, "workspace": str(self.root),
                  "test_instance": str(self.root / "build/navgen-overkill"),
                  "maps": [{"name": "goldrush"}], "token": "private-token", "busy": self.busy, "jobs": []}
        if self.posts or self.states > 1:
            if self.statuses:
                self.job["status"] = self.statuses.pop(0)
            result["jobs"] = [copy.deepcopy(self.job)]
        return result

    def submit(self, action, name, token, timeout):
        self.posts += 1
        assert token == "private-token"
        if self.uncertain:
            raise ConnectionResetError("Lost acknowledgement")
        return {"job": copy.deepcopy(self.job)}


class BatchActionTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name).resolve()
        (self.root / "build/navgen-overkill/tce2").mkdir(parents=True)
        self.args = argparse.Namespace(workspace=self.root, map="goldrush", action="native_check",
                                       job=None, seconds=8, port=8766)
        self.now = 0

    def sleep(self, seconds):
        self.now += seconds

    def run_client(self, client):
        with contextlib.redirect_stdout(io.StringIO()):
            return batch_action.run(self.args, client, clock=lambda: self.now, sleep=self.sleep)

    def outcomes(self):
        return [json.loads(path.read_text()) for path in self.root.rglob("outcome.json")]

    def test_one_submit_and_complete_evidence_with_no_token(self):
        client = FakeClient(self.root)
        self.assertEqual(self.run_client(client), 0)
        self.assertEqual(client.posts, 1)
        self.assertEqual(self.outcomes()[0]["job_status"], "complete")
        evidence = list(self.root.rglob("*.json"))
        self.assertTrue(any(path.name == "job-final.json" for path in evidence))
        self.assertFalse(any("private-token" in path.read_text() for path in evidence))

    def test_uncertain_post_not_replayed_and_possible_id_preserved(self):
        client = FakeClient(self.root, uncertain=True)
        self.assertEqual(self.run_client(client), 3)
        self.assertEqual(client.posts, 1)
        self.assertEqual(self.outcomes()[0]["possible_job_ids"], [JOB_ID])
        self.assertEqual(self.outcomes()[0]["client_status"], "submission_unknown")

    def test_resume_performs_no_post(self):
        client = FakeClient(self.root)
        client.states = 1
        self.args.job = JOB_ID
        self.assertEqual(self.run_client(client), 0)
        self.assertEqual(client.posts, 0)

    def test_timeout_records_resume_id_without_cancelling(self):
        client = FakeClient(self.root, statuses=("running",))
        self.args.seconds = 2
        self.assertEqual(self.run_client(client), 2)
        self.assertEqual(client.posts, 1)
        self.assertEqual(self.outcomes()[0]["job_id"], JOB_ID)
        self.assertEqual(self.outcomes()[0]["client_status"], "wait_timeout")

    def test_busy_refuses_without_post(self):
        client = FakeClient(self.root)
        client.busy = True
        self.assertEqual(self.run_client(client), 2)
        self.assertEqual(client.posts, 0)

    def test_wrong_map_resume_refuses_without_post(self):
        client = FakeClient(self.root)
        client.states = 1
        client.job["map"] = "oasis"
        self.args.job = JOB_ID
        self.assertEqual(self.run_client(client), 3)
        self.assertEqual(client.posts, 0)

    def test_multiple_invocations_keep_independent_evidence(self):
        self.assertEqual(self.run_client(FakeClient(self.root)), 0)
        before = {path: path.read_bytes() for path in self.root.rglob("*.json")}
        self.assertEqual(self.run_client(FakeClient(self.root)), 0)
        self.assertEqual(len(self.outcomes()), 2)
        for path, data in before.items():
            self.assertEqual(path.read_bytes(), data)

    def test_wrong_workspace_no_submit(self):
        client = FakeClient(self.root / "elsewhere")
        self.assertEqual(self.run_client(client), 3)
        self.assertEqual(client.posts, 0)

    def test_each_tce_map_dispatches_without_aliasing(self):
        for name in batch_action.TCE_STANDARD_MAPS:
            with self.subTest(map=name):
                self.args.map = name
                client = FakeClient(self.root)
                client.job["map"] = name
                original_state = client.state

                def state(timeout):
                    result = original_state(timeout)
                    result["maps"] = [{"name": name}]
                    return result

                client.state = state
                self.assertEqual(self.run_client(client), 0)
                self.assertEqual(client.posts, 1)
                self.assertTrue(any(result["map"] == name for result in self.outcomes()))

    def test_explicit_map_set_guards_and_original_et_set(self):
        for group, names in (("et", batch_action.ET_STANDARD_MAPS),
                             ("tce", batch_action.TCE_STANDARD_MAPS)):
            for name in names:
                with self.subTest(group=group, map=name), mock.patch.object(batch_action, "run", return_value=0) as run:
                    self.assertEqual(batch_action.main(["native_check", "--map", name, "--map-set", group]), 0)
                    self.assertEqual(run.call_args.args[0].map, name)
        for group, name in (("et", "obj_delta"), ("tce", "goldrush")):
            with self.subTest(group=group), contextlib.redirect_stderr(io.StringIO()), mock.patch.object(batch_action, "run") as run:
                with self.assertRaises(SystemExit):
                    batch_action.main(["native_check", "--map", name, "--map-set", group])
                run.assert_not_called()


if __name__ == "__main__":
    unittest.main()
