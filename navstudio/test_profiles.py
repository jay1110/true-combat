"""Koerperprofil-Gates ohne Engine, RCON oder gestartete Prozesse pruefen."""
import json
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest.mock import Mock

from server import Studio, MOVEMENT_PROFILES, native_body_profile
from edit import Drafts


class ProfileTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.s = Studio.__new__(Studio)
        self.s.preview = self.root / 'preview'; self.s.preview.mkdir()
        self.s.runtime = self.root / 'runtime'; self.s.runtime.mkdir()
        self.s.work = self.root / 'work'; self.s.work.mkdir()
        self.s.update = lambda job, **fields: job.update(fields)
        self.s.require_loaded = Mock()
        self.s.quiet_generation = Mock()
        self.s.verify_collision_snapshot = Mock(return_value=dict(unchanged=True))
        self.s.rcon = Mock(side_effect=AssertionError('Kein echter RCON-Aufruf'))
        self.s.rcon_with_followup = Mock(return_value='fixture reply')
        self.job = dict(map='obj_delta', log='')
        self.candidate = self.s.path('obj_delta', '_candidate.txt')
        self.candidate.write_text('offline fixture')
        self.s.candidate = Mock(return_value=self.candidate)
        self.s.proof_status = Mock(return_value=dict(valid=True, inputs=dict(candidate='hash')))
        self.s.file_hash = Mock(return_value='hash')
        self.s.inspect = Mock(return_value=dict(binary_report_match=True))

    def meshes(self, flag):
        profile = MOVEMENT_PROFILES[flag]
        for kind, height in (('stand', profile['standing_height']), ('duck', profile['crouching_height'])):
            self.s.path('obj_delta', '.' + kind + '.json').write_text(json.dumps(dict(profile=dict(
                height=height, radius=profile['radius'], step=18, cell=4, cell_height=2))))
        self.candidate.with_suffix('.json').write_text(json.dumps(dict(movement_profile=profile, counts={})))
        return profile

    @staticmethod
    def native(flag):
        p = MOVEMENT_PROFILES[flag]
        return dict(map='obj_delta', source='navgen/obj_delta_candidate.txt', generation_complete=True,
            bbox=dict(half_width=p['radius'], stand_height=p['standing_height'], crouch_height=p['crouching_height'], origin_above_feet=p['origin_above_feet']))

    def test_g_newbbox_zero_and_one_are_read_only_supported(self):
        for flag in (0, 1):
            self.s.rcon = Mock(return_value=f'^3"g_newbbox" is:"{flag}" default:"1"')
            self.assertEqual(self.s.require_bbox(self.job), MOVEMENT_PROFILES[flag])
            self.s.rcon.assert_called_once_with('g_newbbox', read_only=True)
        for reply in ('unknown', '"g_newbbox" is:"2"', '"g_newbbox" is:"-1"'):
            self.s.rcon = Mock(return_value=reply)
            with self.assertRaises(RuntimeError): self.s.require_bbox(self.job)

    def test_mesh_profiles_and_candidate_profile_must_all_match(self):
        for flag in (0, 1):
            profile = self.meshes(flag)
            self.s.require_profile_inputs('obj_delta', profile, self.candidate)
            with self.assertRaises(RuntimeError):
                self.s.require_profile_inputs('obj_delta', MOVEMENT_PROFILES[1-flag], self.candidate)
        self.candidate.with_suffix('.json').write_text('{}')
        with self.assertRaisesRegex(RuntimeError, 'Spawn-Hoehenversatz'):
            self.s.require_profile_inputs('obj_delta', MOVEMENT_PROFILES[1], self.candidate)

    def test_mixed_mesh_radius_or_height_is_rejected(self):
        for wrong in (dict(height=49,radius=16,step=18), dict(height=61,radius=14,step=18), dict(height=49,radius=14,step=17)):
            self.meshes(0)
            self.s.path('obj_delta', '.duck.json').write_text(json.dumps(dict(profile=wrong)))
            with self.assertRaises(RuntimeError): self.s.recast_body_profile('obj_delta')

    def test_native_profile_rejects_partial_or_mixed_bbox(self):
        for flag in (0, 1): self.assertEqual(native_body_profile(self.native(flag)), MOVEMENT_PROFILES[flag])
        for bbox in ({}, dict(half_width=14,stand_height=87,crouch_height=49,origin_above_feet=24)):
            with self.assertRaises(RuntimeError): native_body_profile(dict(bbox=bbox))

    def test_mismatch_blocks_import_and_staged_file(self):
        self.meshes(1)
        self.s.require_bbox = Mock(return_value=MOVEMENT_PROFILES[0])
        with self.assertRaises(RuntimeError): self.s.action_native_check(self.job)
        self.s.rcon_with_followup.assert_not_called()
        self.assertFalse((self.s.runtime / self.candidate.name).exists())

    def test_profile_change_after_import_creates_no_proof(self):
        self.meshes(0)
        self.s.require_bbox = Mock(side_effect=[MOVEMENT_PROFILES[0],MOVEMENT_PROFILES[1]])
        self.s.wait_fresh = Mock(return_value=self.native(0))
        with self.assertRaisesRegex(RuntimeError, 'waehrend'):
            self.s.action_native_check(self.job)
        self.assertFalse((self.s.work / 'obj_delta.native-proof.json').exists())
        self.s.inspect.assert_not_called()

    def test_native_bbox_mismatch_creates_no_proof(self):
        self.meshes(0)
        self.s.require_bbox = Mock(return_value=MOVEMENT_PROFILES[0])
        self.s.wait_fresh = Mock(return_value=self.native(1))
        with self.assertRaisesRegex(RuntimeError, 'anderes Koerperprofil'):
            self.s.action_native_check(self.job)
        self.assertFalse((self.s.work / 'obj_delta.native-proof.json').exists())

    def test_matching_native_profile_is_hash_bound_in_receipt(self):
        self.meshes(0)
        self.s.require_bbox = Mock(return_value=MOVEMENT_PROFILES[0])
        self.s.wait_fresh = Mock(return_value=self.native(0))
        result = self.s.action_native_check(self.job)
        proof = json.loads(Path(result['proof']).read_text())
        self.assertEqual(proof['movement_profile'], MOVEMENT_PROFILES[0])
        self.assertEqual(proof['inputs'], dict(candidate='hash'))
        self.assertEqual(self.s.require_bbox.call_count, 2)

    def test_snapshot_preserves_engine_file_and_checks_profile_twice(self):
        snapshot = self.s.runtime / 'obj_delta_entities.json'
        snapshot.write_text('{"map":"obj_delta","entities":[]}')
        before = snapshot.read_bytes()
        self.s.require_bbox = Mock(return_value=MOVEMENT_PROFILES[0])
        self.s.wait_fresh = Mock(return_value=json.loads(before))
        _, result = self.s.snapshot(self.job)
        self.assertEqual(result['movement_profile'], MOVEMENT_PROFILES[0])
        self.assertEqual(snapshot.read_bytes(), before)
        self.assertEqual(self.s.require_bbox.call_count, 2)

    def test_rebuild_passes_matching_height_and_radius(self):
        for flag in (0, 1):
            p = self.meshes(flag)
            self.s.recast = self.root / 'recast.exe'
            self.s.assets = SimpleNamespace(maps={'obj_delta':dict(path=self.root/'map.pk3')})
            self.s.snapshot = Mock(return_value=(self.root/'snapshot.json',dict(movement_profile=p)))
            self.s.worker = lambda *args: list(map(str,args))
            self.s.command = Mock()
            self.s.rebuild_recast = Mock(return_value='fixture manifest')
            self.s.action_rebuild(self.job)
            self.s.rebuild_recast.assert_called_once_with(self.job, p)
            self.assertIn('--vertical-raster-domain', self.s.command.call_args_list[0].args[1])

    def test_editor_export_keeps_body_profile(self):
        store = Drafts(self.root/'drafts')
        environment = {key:'sha' for key in ('bsp','stand','duck','geometry','collision','runtime_snapshot')}
        p = MOVEMENT_PROFILES[0]
        store.start('obj_delta', dict(nodes=[dict(id=0,position=[0,0,0],radius=35)],edges=[]),
            dict(environment=environment,movement_profile=p))
        store.save('obj_delta',0,environment)
        path = store.export_candidate('obj_delta',0,environment,self.s.preview)
        self.assertEqual(json.loads(path.with_suffix('.json').read_text())['movement_profile'],p)


if __name__ == '__main__':
    unittest.main()
