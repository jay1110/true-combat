"""Owned game lifecycle is visible and always interactive; no real process/UDP calls."""
import os
import subprocess
from pathlib import Path
from types import SimpleNamespace
import tempfile
import threading
import unittest
from unittest.mock import Mock, patch
from server import Studio, GameConnectionError
from watch import Windows, ProcessIdentityChanged


class RuntimeTests(unittest.TestCase):
    @unittest.skipUnless(os.name == 'nt', 'Windows process discovery')
    def test_slow_windows_owner_query_fails_closed_with_a_bounded_retry_message(self):
        s=self.studio;s.args=SimpleNamespace(rcon_port=27986)
        with patch('server.subprocess.run',side_effect=subprocess.TimeoutExpired('discovery',20)) as runner:
            with self.assertRaisesRegex(RuntimeError,'kein weiteres Testspiel gestartet'):
                Studio.test_processes(s)
        self.assertEqual(runner.call_count,1)
        self.assertEqual(runner.call_args.kwargs['timeout'],20)
        s.rcon.assert_not_called();self.assertEqual(s.process_api.mock_calls,[])

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="tce-navstudio-runtime-")
        self.root = Path(self.temp.name).resolve()
        self.studio = Studio.__new__(Studio)
        self.studio.test = self.root
        self.studio.mod = self.root / "tce2"
        self.studio.mod.mkdir()
        self.studio.game_executable = self.root / "ET.exe"
        self.studio.live_gametype = Mock(return_value=2)
        self.studio.update = lambda job, **fields: job.update(fields)
        self.studio.process_api = Mock()
        self.studio.test_processes = Mock(return_value=[])
        self.studio.rcon = Mock(side_effect=AssertionError("No real RCON in offline tests"))
        self.job = {"map": "obj_overkill", "log": ""}


    def tearDown(self):
        self.assertTrue(self.root.name.startswith("tce-navstudio-runtime-"))
        self.temp.cleanup()


    def test_status_timeout_uses_fresh_authenticated_map_read(self):
        s = self.studio
        s.udp_exchange = Mock(return_value="")
        s.rcon = Mock(return_value='"mapname" is:"obj_village^7" default:"nomap^7"')
        self.assertEqual(s.server_map(), "obj_village")
        s.rcon.assert_called_once_with("mapname", read_only=True)
        s.rcon.return_value = '"mapname" is:"nomap^7"'
        with self.assertRaisesRegex(RuntimeError, "noch keine Map"):
            s.server_map()


    def test_apply_draft_saves_checks_installs_and_requires_reload(self):
        s = self.studio
        s.drafts = Mock(); s.drafts.get.return_value = {"revision": 4}
        s.draft_environment = Mock(return_value={}); s.preview = self.root
        events = []
        s.drafts.save.side_effect = lambda *a: events.append("save")
        s.drafts.export_candidate.side_effect = lambda *a: events.append("export")
        s.action_load_map = Mock(side_effect=lambda *a:events.append("load"))
        s.action_native_check = Mock(side_effect=lambda *a:events.append("check"))
        s.install = Mock(side_effect=lambda *a:events.append("install") or {"reload_confirmed":True})
        self.assertEqual(s.action_apply_draft(self.job)["draft_revision"],4)
        self.assertEqual(events,["save","export","load","check","install"])
        s.action_native_check.side_effect = RuntimeError("invalid graph")
        s.install.reset_mock()
        with self.assertRaisesRegex(RuntimeError,"invalid graph"):s.action_apply_draft(self.job)
        s.install.assert_not_called()
        s.action_native_check.side_effect=None
        s.install.side_effect=None;s.install.return_value={"reload_confirmed":False}
        with self.assertRaisesRegex(RuntimeError,"nicht bestätigt"):s.action_apply_draft(self.job)


    def route_setup(self, process):
        s = self.studio
        s.own_test_process = Mock(return_value=process)
        s.stage_map = Mock(return_value=None)
        s.launch_visible = Mock()
        s.stop_owned_test = Mock()
        s.server_map = Mock(return_value="obj_overkill")
        s.rcon_with_followup = Mock(return_value="sent")
        s.wait_visible_map = Mock(return_value={"visible": True})
        return s


    def test_offline_load_stages_before_one_visible_start(self):
        s = self.route_setup(None)
        calls = []
        s.stage_map.side_effect = lambda name: calls.append("stage")
        s.launch_visible.side_effect = lambda job: calls.append("launch")
        self.assertTrue(s.action_load_map(self.job)["visible"])
        self.assertEqual(calls, ["stage", "launch"])
        s.server_map.assert_not_called()
        s.rcon_with_followup.assert_not_called()


    def test_existing_visible_map_is_reused(self):
        s = self.route_setup({"pid": 7, "dedicated": 0, "owns_port": True})
        s.action_start_test(self.job)
        s.launch_visible.assert_not_called()
        s.stop_owned_test.assert_not_called()
        s.rcon_with_followup.assert_not_called()


    def test_gametype_change_restarts_same_instance(self):
        s=self.route_setup({'pid':7,'dedicated':0,'owns_port':True})
        self.job['gametype']=7
        s.action_load_map(self.job)
        s.stop_owned_test.assert_called_once()
        s.launch_visible.assert_called_once()
        s.rcon_with_followup.assert_not_called()


    def test_other_map_restarts_same_installation_once_no_blind_retry(self):
        s = self.route_setup({"pid": 7, "dedicated": 0, "owns_port": True})
        s.server_map.return_value = "obj_railhouse"
        s.wait_visible_map.side_effect = RuntimeError("timeout")
        with self.assertRaisesRegex(RuntimeError, "timeout"):
            s.action_load_map(self.job)
        s.rcon_with_followup.assert_not_called()
        s.stop_owned_test.assert_called_once()
        s.launch_visible.assert_called_once()


    def test_confirmed_dedicated_or_new_package_reuses_installation(self):
        for dedicated, package in [(1, None), (0, self.root / "map.pk3")]:
            s = self.route_setup({"pid": 7, "dedicated": dedicated, "owns_port": True})
            s.stage_map.return_value = package
            s.action_load_map(self.job)
            s.stop_owned_test.assert_called_once()
            s.launch_visible.assert_called_once()


    def test_ambiguous_or_foreign_process_cannot_receive_commands(self):
        for processes in [[{"pid": 7, "owns_port": False}], [{"pid": 7}, {"pid": 8}]]:
            self.studio.test_processes.return_value = processes
            with self.assertRaises(RuntimeError):
                self.studio.own_test_process()
        self.studio.rcon.assert_not_called()


    def test_stop_timeout_terminates_only_exact_verified_test_process(self):
        s = self.studio
        process = {"pid": 7, "owns_port": True}
        s.test_processes.side_effect = [[process], []]
        identity = (os.path.normcase(str(s.game_executable)), 100)
        s.process_api.identity.return_value = identity
        s.process_api.terminate.return_value = {"terminated": True}
        s.rcon_with_followup = Mock(return_value="sent")
        with patch("server.time.monotonic", side_effect=[0, 21]):
            s.stop_owned_test(self.job, process)
        s.rcon_with_followup.assert_called_once_with("quit")
        s.process_api.terminate.assert_called_once_with(7, identity, expected_exe=s.game_executable)


    def test_visible_wait_keeps_player_control(self):
        s = self.studio
        s.own_test_process = Mock(return_value={"pid": 7, "dedicated": 0, "window": 10})
        s.server_map = Mock(return_value="obj_overkill")
        s.rcon_with_followup = Mock(return_value="execing navstudio-waypoints.cfg")
        self.job["test_pid"] = 7
        self.assertTrue(s.wait_visible_map(self.job)["visible"])
        s.own_test_process.return_value = {"pid": 8}
        with self.assertRaises(ProcessIdentityChanged):
            s.wait_visible_map(self.job)


    def test_closed_port_hung_window_cannot_prevent_explicit_own_process_stop(self):
        s = self.studio
        process = {"pid": 7, "owns_port": False}
        s.test_processes.side_effect = [[process], []]
        identity = (os.path.normcase(str(s.game_executable)), 100)
        s.process_api.identity.return_value = identity
        s.process_api.terminate.return_value = {"terminated": True}
        s.rcon_with_followup = Mock()
        with patch("server.time.monotonic", side_effect=[0, 2]):
            s.stop_owned_test(self.job, process)
        s.rcon_with_followup.assert_not_called()
        s.process_api.terminate.assert_called_once_with(7, identity, expected_exe=s.game_executable)


    def test_status_is_cached_read_only_without_window_input_state(self):
        s = self.studio
        s.game_state_cache = None
        s.game_state_lock = threading.Lock()
        s.test_processes.return_value = [{"pid": 7, "dedicated": 0, "owns_port": True, "window": 10}]
        s.udp_exchange = Mock(return_value="\\mapname\\obj_overkill\\sv_hostname\\test")
        for unused in range(2):
            state = s.game_state()
            self.assertEqual(state["state"], "running")
            self.assertIn("normal bedienbar", state["input_note"])
            self.assertNotIn("watch", state)
            self.assertNotIn("input_isolated", state)
            self.assertNotIn("can_release_input", state)
        s.udp_exchange.assert_called_once()
        s.process_api.assert_not_called()
        s.rcon.assert_not_called()

    def test_single_visible_launch_never_changes_window_or_input(self):
        s = self.studio
        s.resources = self.root
        s.args = SimpleNamespace(rcon_port=27986)
        (self.root / "start-test.ps1").write_text("# fixture only")
        (self.root / "ET.exe").write_bytes(b"fixture, never executed")
        result = SimpleNamespace(returncode=0, stdout=b'{"pid":7,"mode":"listen"}\r\n')
        with patch("server.subprocess.run", return_value=result) as runner:
            s.launch_visible(self.job)
        runner.assert_called_once()
        self.assertEqual(self.job["test_pid"], 7)
        s.process_api.assert_not_called()
        self.assertEqual(s.process_api.mock_calls, [])
        self.assertFalse(list(s.mod.glob("navstudio-input*")))
        self.assertFalse((s.mod / "navstudio-watch.cfg").exists())

    def test_wrong_stop_identity_cannot_quit_or_terminate_foreign_process(self):
        s = self.studio
        process = {"pid": 7, "owns_port": True}
        s.test_processes.return_value = [process]
        s.process_api.identity.return_value = (str(self.root / "other.exe"), 100)
        s.rcon_with_followup = Mock()
        with self.assertRaises(ProcessIdentityChanged):
            s.stop_owned_test(self.job, process)
        s.process_api.terminate.assert_not_called()
        s.rcon_with_followup.assert_not_called()

    def test_visible_timeout_does_not_lock_or_release_window(self):
        s = self.studio
        s.own_test_process = Mock(return_value={"pid": 7, "dedicated": 0, "window": 10})
        s.server_map = Mock(return_value="obj_other")
        s.rcon_with_followup = Mock()
        with patch("server.time.monotonic", side_effect=[0, 2]):
            with self.assertRaisesRegex(RuntimeError, "Kein zweiter Spielstart"):
                s.wait_visible_map(self.job, timeout=1)
        self.assertEqual(s.process_api.mock_calls, [])
        s.rcon_with_followup.assert_not_called()


if __name__ == "__main__": unittest.main()
