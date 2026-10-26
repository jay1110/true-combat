"""Generation pauses only the exact owned game's bots and always restores them."""
import os
import json
from pathlib import Path
from types import SimpleNamespace
import tempfile
import threading
from unittest import TestCase, main
from unittest.mock import Mock

import quiet
from server import Studio
from watch import ProcessIdentityChanged


class QuietGenerationTests(TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.s = Studio.__new__(Studio)
        self.s.work = Path(self.temp.name)
        self.s.game_executable = self.s.work / "ET.exe"
        self.identity = [os.path.normcase(str(self.s.game_executable.resolve())), 1234]
        self.s.process_api = SimpleNamespace(identity=Mock(return_value=self.identity))
        self.s.own_test_process = Mock(return_value=dict(pid=12, owns_port=True))
        self.s.require_loaded = Mock()
        self.s.update = lambda job, **values: job.update(values)
        self.s.rcon_with_followup = Mock(side_effect=["NAVSTUDIO_AUTHORING_ON", "NAVSTUDIO_QUIET_OK", "kicked", "NAVSTUDIO_QUIET_RESTORED", "NAVSTUDIO_AUTHORING_OFF"])
        self.s.rcon = Mock(return_value="NAVSTUDIO_QUIET_COUNT:0")
        self.job = dict(map="obj_delta", log="")

    def test_owned_bot_pause_is_nested_once_and_names_are_vm_values(self):
        self.s.quiet_generation(self.job)
        self.s.quiet_generation(self.job)
        self.assertTrue(self.job["quiet_generation"]["population_confirmed_zero"])
        self.assertEqual(self.s.rcon_with_followup.call_count, 3)
        script=quiet.begin_script("1"*16)
        self.assertIn("Name=b.Name",script)
        self.assertIn("Server.SaveConfigChanges=false",script)
        self.s.restore_generation_bots(self.job)
        self.assertNotIn("quiet_generation",self.job)
        self.assertTrue(self.job["quiet_generation_result"]["restored"])
        self.assertEqual("tce_navgen_authoring off",self.s.rcon_with_followup.call_args.args[0])

    def test_pid_replacement_never_receives_restoration(self):
        self.s.quiet_generation(self.job)
        self.s.own_test_process.return_value=dict(pid=13,owns_port=True)
        with self.assertRaises(ProcessIdentityChanged):
            self.s.restore_generation_bots(self.job)
        self.assertEqual(self.s.rcon_with_followup.call_count,3)

    def test_unconfirmed_pause_does_not_remove_bots(self):
        self.s.rcon_with_followup.side_effect=None
        self.s.rcon_with_followup.side_effect=["NAVSTUDIO_AUTHORING_ON", "Error Running Script."]
        with self.assertRaisesRegex(RuntimeError,"nicht bestätigt"):
            self.s.quiet_generation(self.job)
        self.assertEqual(self.s.rcon_with_followup.call_count,2)
        self.assertIn("quiet_generation",self.job) # ambiguous sent command can be restored

    def test_failed_worker_job_restores_bot_settings(self):
        self.s.lock=threading.RLock(); self.s.busy=True
        def fail(job):
            self.s.quiet_generation(job)
            raise RuntimeError("real geometry failure")
        self.s.action_generate=fail
        self.job.update(action="generate",id="test")
        self.s.run_job(self.job)
        self.assertEqual(self.job["status"],"failed")
        self.assertEqual(self.job["error"],"real geometry failure")
        self.assertTrue(self.job["quiet_generation_result"]["restored"])
        self.assertFalse(self.s.busy)

    def test_authoring_hold_released_even_when_gm_restoration_fails(self):
        self.s.quiet_generation(self.job)
        self.s.rcon_with_followup.side_effect=["GM parse error", "NAVSTUDIO_AUTHORING_OFF"]
        with self.assertRaisesRegex(RuntimeError,"Wiederherstellung"):
            self.s.restore_generation_bots(self.job)
        self.assertEqual(self.s.rcon_with_followup.call_args.args[0],"tce_navgen_authoring off")
        self.assertFalse(self.job["quiet_generation"]["authoring_hold"])

    def test_busy_pause_releases_only_the_new_hold(self):
        self.s.rcon_with_followup.side_effect=["NAVSTUDIO_AUTHORING_ON","NAVSTUDIO_QUIET_BUSY","NAVSTUDIO_AUTHORING_OFF"]
        with self.assertRaises(RuntimeError):self.s.quiet_generation(self.job)
        self.assertNotIn("quiet_generation",self.job)
        self.assertEqual(self.s.rcon_with_followup.call_args.args[0],"tce_navgen_authoring off")

    def test_original_et_waits_playing_before_gm_snapshot_without_tce_hold(self):
        self.s.game_mode="et"
        self.s.rcon_with_followup.side_effect=["NAVSTUDIO_QUIET_OK","kicked","NAVSTUDIO_QUIET_RESTORED"]
        self.s.rcon.side_effect=['"gamestate" is: "1"','"gamestate" is: "0"','NAVSTUDIO_QUIET_COUNT:0']
        self.s.quiet_generation(self.job)
        self.assertNotIn("authoring_hold",self.job["quiet_generation"])
        self.s.restore_generation_bots(self.job)
        self.assertFalse(any("tce_navgen_authoring" in c.args[0] for c in self.s.rcon_with_followup.call_args_list))

    def test_tokens_cannot_inject_commands(self):
        for builder in (quiet.begin_script,quiet.restore_script):
            with self.assertRaises(ValueError): builder("';quit")

    def test_collision_identity_ignores_elapsed_time_but_detects_damage_and_pose(self):
        initial=dict(map="obj_delta",time=3,models=[dict(entity=9,modelindex=2,linked=True,takedamage=True,health=200,origin=[0,0,0])])
        later={**initial,"time":900}
        self.assertEqual(Studio.collision_snapshot_hash(initial),Studio.collision_snapshot_hash(later))
        for change in (dict(health=0),dict(linked=False),dict(origin=[0,0,1])):
            changed={**later,"models":[{**initial["models"][0],**change}]}
            self.assertNotEqual(Studio.collision_snapshot_hash(initial),Studio.collision_snapshot_hash(changed))
        alive={**later,"models":[{**initial["models"][0],"health":199}]}
        self.assertEqual(Studio.collision_snapshot_hash(initial),Studio.collision_snapshot_hash(alive))
        moved_irrelevant={**later,"models":[initial["models"][0],dict(entity=10,modelindex=3,origin=[90,90,90])]}
        self.assertEqual(Studio.collision_snapshot_hash(initial,{(9,2)}),Studio.collision_snapshot_hash(moved_irrelevant,{(9,2)}))

    def test_next_explicit_generation_recovers_same_process_pause_after_app_restart(self):
        previous=dict(token="1"*16,pid=12,identity=self.identity,map="obj_delta")
        (self.s.work/"navstudio-quiet-session.json").write_text(json.dumps(previous))
        self.s.rcon_with_followup.side_effect=["NAVSTUDIO_QUIET_RESTORED","NAVSTUDIO_AUTHORING_ON","NAVSTUDIO_QUIET_OK","kicked"]
        self.s.quiet_generation(self.job)
        self.assertTrue(self.job["quiet_generation_recovered_previous_session"])
        self.assertIn("navstudio-quiet-"+"1"*16+"-restore.gm",self.s.rcon_with_followup.call_args_list[0].args[0])

    def test_terminal_status_is_not_visible_until_restoration_finishes(self):
        self.s.lock=threading.RLock();self.s.busy=True
        def generate(job):
            self.s.quiet_generation(job)
            return dict(proof="new")
        self.s.action_generate=generate
        original=self.s.restore_generation_bots
        def restore(job):
            self.assertEqual(job["status"],"running")
            self.assertTrue(self.s.busy)
            original(job)
        self.s.restore_generation_bots=restore
        self.job.update(action="generate",id="test")
        self.s.run_job(self.job)
        self.assertEqual(self.job["status"],"complete")
        self.assertFalse(self.s.busy)


if __name__=="__main__": main()
