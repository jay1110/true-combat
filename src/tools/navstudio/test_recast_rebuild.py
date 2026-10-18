"""Speicher-Retry, Domänenbeleg und Veröffentlichungsgrenzen ohne Engine testen."""
import copy
import json
from pathlib import Path
import subprocess
import struct
import tempfile
import unittest
from unittest.mock import Mock, patch

from server import Studio, MOVEMENT_PROFILES, CommandFailure, digest, recast_height_domain, recast_memory_failure


MEMORY_ERROR = "rcBuildCompactHeightfield: Out of memory 'chf.cells' (4069488)\nCompact field failed\n"


class RecastRebuildTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name).resolve()
        self.s = Studio.__new__(Studio)
        self.s.preview = self.root / 'preview'; self.s.preview.mkdir()
        self.s.work = self.root / 'work'; self.s.work.mkdir()
        self.s.recast = self.root / 'recast.exe'; self.s.recast.write_bytes(b'offline compiler fixture')
        self.s.workspace = self.root
        self.s.update = lambda job, **fields: job.update(fields)
        self.job = dict(map='obj_railhouse', log='')
        self.collision = self.s.path(self.job['map'], '.collision.obj')
        self.collision.write_text('offline collision fixture')
        self.geometry_path = self.s.path(self.job['map'], '.geometry.json')
        self.geometry = dict(map=self.job['map'], output=dict(obj=str(self.collision), obj_sha256=digest(self.collision)),
            vertical_raster_domain=dict(coordinate_system='BSP_Z_equals_OBJ_Y', minimum=-304, maximum=936,
                padding=128, rule='playable_model_and_spawn_bounds_plus_hull_step_padding',
                collision_geometry_modified=False, add_cut_caps=False,
                basis=[dict(kind='world_model', model=0, minimum=-176, maximum=808),
                       dict(kind='spawn', entity=1, minimum=32, maximum=32)]))
        self.write_geometry()
        for kind in ('stand', 'duck'):
            self.s.path(self.job['map'], '.'+kind+'.json').write_bytes(b'previous verified mesh '+kind.encode())
        self.before = self.published()
        self.calls = []

    def write_geometry(self):
        self.geometry_path.write_text(json.dumps(self.geometry))

    def published(self):
        return {kind:self.s.path(self.job['map'], '.'+kind+'.json').read_bytes() for kind in ('stand','duck')}

    def manifest(self):
        return json.loads(Path(self.job['recast_evidence']).read_text(encoding='utf-8'))

    def succeed(self, job, command, label, timeout=300):
        self.calls.append(command)
        self.assertEqual(timeout, 300)
        self.assertEqual(command[4], '4')
        args = {command[i]:command[i+1] for i in (5,)}
        index = command.index('--height-domain')
        height_domain = dict(requested_minimum=float(command[index+1]), requested_maximum=float(command[index+2]),
            rule='original_triangles_intersected_by_raster_bounds_no_caps', native_validation_required=True)
        mesh = dict(schema=1, status='geometry_candidate_not_engine_validated', coordinate_system='TCE_Z_up',
                    profile=dict(height=int(command[3]), radius=int(args['--radius']), step=18, cell=4, cell_height=2),
                    vertices=[[0,0,0],[128,0,0],[0,128,0]], polygons=[dict(vertices=[0,1,2], neighbors=[-1,-1,-1])],
                    tiling=dict(enabled='--tile-size' in command, tile_cells=512 if '--tile-size' in command else 0),
                    height_domain=height_domain)
        Path(command[2]).write_text(json.dumps(mesh))
        return '3 vertices, 1 polygons; candidate only\n'

    def test_normal_build_uses_exact_profile_domain_and_hash_bound_artifacts(self):
        for flag in (0,1):
            self.calls = []
            self.s.command = self.succeed
            self.s.rebuild_recast(self.job, MOVEMENT_PROFILES[flag])
            p = MOVEMENT_PROFILES[flag]
            self.assertEqual([int(c[3]) for c in self.calls], [p['standing_height'],p['crouching_height']])
            self.assertTrue(all(c[5:]==['--radius',str(p['radius']),'--height-domain','-304','936'] for c in self.calls))
            report = self.manifest()
            self.assertEqual(report['status'], 'complete')
            self.assertEqual(len(report['attempts']), 2)
            for attempt in report['attempts']:
                self.assertEqual(attempt['output_sha256'], digest(Path(attempt['output'])))
                self.assertEqual(attempt['log_sha256'], digest(Path(attempt['log'])))
            for kind in ('stand','duck'):
                self.assertEqual(report['published'][kind]['sha256'], digest(Path(report['published'][kind]['path'])))

    def test_memory_failure_retries_once_with_fresh_output_and_unchanged_domain(self):
        invoked = []
        def run(job, command, label, timeout):
            invoked.append(command)
            if len(invoked)==1:
                Path(command[2]).write_text('{"partial":true}')
                raise CommandFailure(label,1,MEMORY_ERROR)
            return self.succeed(job,command,label,timeout)
        self.s.command = run
        self.s.rebuild_recast(self.job,MOVEMENT_PROFILES[1])
        self.assertEqual(len(invoked),3)
        self.assertNotEqual(invoked[0][2],invoked[1][2])
        self.assertEqual(invoked[1][3:],invoked[0][3:]+['--tile-size','512'])
        report = self.manifest()
        self.assertEqual(report['attempts'][0]['status'],'failed')
        self.assertTrue(report['attempts'][0]['memory_retry'])
        self.assertNotEqual(report['attempts'][0]['output_sha256'],report['published']['stand']['sha256'])
        self.assertEqual(Path(report['attempts'][0]['log']).read_text(),MEMORY_ERROR)

    def test_retry_is_bounded_and_preserves_previous_meshes(self):
        self.s.command = Mock(side_effect=CommandFailure('Recast',1,MEMORY_ERROR))
        with self.assertRaises(CommandFailure): self.s.rebuild_recast(self.job,MOVEMENT_PROFILES[1])
        self.assertEqual(self.s.command.call_count,2)
        self.assertEqual(self.published(),self.before)
        self.assertEqual(self.manifest()['status'],'failed')
        self.assertEqual(self.manifest()['published'],{})

    def test_both_holds_can_retry_but_each_has_only_two_attempts(self):
        seen=set()
        def run(job,command,label,timeout):
            key=command[3]
            if key not in seen:
                seen.add(key)
                raise CommandFailure(label,1,MEMORY_ERROR)
            self.assertEqual(self.published(),self.before)
            return self.succeed(job,command,label,timeout)
        self.s.command=run
        self.s.rebuild_recast(self.job,MOVEMENT_PROFILES[1])
        self.assertEqual([a['attempt'] for a in self.manifest()['attempts']],[1,2,1,2])

    def test_float32_domain_rounding_matches_actual_builder_serialization(self):
        domain=self.geometry['vertical_raster_domain']
        domain['basis'][0]['maximum']=808.123456789
        domain['maximum']=936.123456789
        self.write_geometry()
        def run(job,command,label,timeout):
            output=self.succeed(job,command,label,timeout)
            mesh=json.loads(Path(command[2]).read_text())
            value=struct.unpack('f',struct.pack('f',domain['maximum']))[0]
            mesh['height_domain']['requested_maximum']=float(format(value,'.10g'))
            Path(command[2]).write_text(json.dumps(mesh))
            return output
        self.s.command=run
        self.s.rebuild_recast(self.job,MOVEMENT_PROFILES[1])
        self.assertEqual(self.manifest()['status'],'complete')

    def test_other_failures_never_retry_or_publish(self):
        errors = [CommandFailure('Recast',1,'Non-leaf zero-area contour remains'),
                  CommandFailure('Recast',1,'Compact field failed'),
                  CommandFailure('Recast',-1073741819,MEMORY_ERROR),
                  RuntimeError(MEMORY_ERROR), subprocess.TimeoutExpired('recast',300,output=MEMORY_ERROR.encode())]
        for error in errors:
            with self.subTest(error=repr(error)):
                self.s.command = Mock(side_effect=error)
                with self.assertRaises(type(error)): self.s.rebuild_recast(self.job,MOVEMENT_PROFILES[1])
                self.assertEqual(self.s.command.call_count,1)
                self.assertEqual(self.published(),self.before)

    def test_duck_failure_does_not_replace_successful_stand(self):
        def run(job,command,label,timeout):
            if command[3]=='61': raise CommandFailure(label,1,'Polygon mesh failed')
            return self.succeed(job,command,label,timeout)
        self.s.command = run
        with self.assertRaises(CommandFailure): self.s.rebuild_recast(self.job,MOVEMENT_PROFILES[1])
        self.assertEqual(self.published(),self.before)

    def test_success_without_fresh_output_cannot_reuse_old_mesh(self):
        self.s.command = Mock(return_value='success text, no file')
        with self.assertRaisesRegex(RuntimeError,'Frische Recast-Ausgabe'):
            self.s.rebuild_recast(self.job,MOVEMENT_PROFILES[1])
        self.assertEqual(self.published(),self.before)

    def test_wrong_profile_domain_or_ignored_tiling_blocks_publication(self):
        for wrong in ('profile','height_domain','tiling'):
            with self.subTest(wrong=wrong):
                calls=[]
                def run(job,command,label,timeout):
                    calls.append(command)
                    if wrong=='tiling' and len(calls)==1: raise CommandFailure(label,1,MEMORY_ERROR)
                    output = self.succeed(job,command,label,timeout)
                    mesh = json.loads(Path(command[2]).read_text())
                    if wrong=='profile': mesh['profile']['radius']=14
                    elif wrong=='height_domain': mesh['height_domain']['requested_maximum']=935
                    else: mesh['tiling']['enabled']=False
                    Path(command[2]).write_text(json.dumps(mesh))
                    return output
                self.s.command=run
                with self.assertRaises(RuntimeError): self.s.rebuild_recast(self.job,MOVEMENT_PROFILES[1])
                self.assertEqual(self.published(),self.before)

    def test_changed_input_or_builder_during_run_is_rejected(self):
        for path in (self.collision,self.geometry_path,self.s.recast):
            with self.subTest(path=path):
                before=path.read_bytes()
                def run(job,command,label,timeout):
                    result=self.succeed(job,command,label,timeout)
                    path.write_bytes(before+b' ')
                    return result
                self.s.command=run
                with self.assertRaisesRegex(RuntimeError,'verändert'):
                    self.s.rebuild_recast(self.job,MOVEMENT_PROFILES[1])
                path.write_bytes(before)
                self.assertEqual(self.published(),self.before)

    def test_domain_validation_rejects_guessed_malformed_or_wrong_source(self):
        good=copy.deepcopy(self.geometry)
        changes=[lambda d:d.pop('vertical_raster_domain'), lambda d:d.update(map='wrong_map'),
                 lambda d:d['output'].update(obj_sha256='stale'),
                 lambda d:d['vertical_raster_domain'].update(minimum=-303),
                 lambda d:d['vertical_raster_domain'].update(maximum=float('inf')),
                 lambda d:d['vertical_raster_domain'].update(add_cut_caps=True),
                 lambda d:d['vertical_raster_domain'].update(collision_geometry_modified=True),
                 lambda d:d['vertical_raster_domain'].update(basis=[dict(kind='spawn',minimum=-176,maximum=808)]),
                 lambda d:d['vertical_raster_domain'].update(padding=0)]
        for change in changes:
            bad=copy.deepcopy(good); change(bad)
            with self.assertRaises(RuntimeError): recast_height_domain(bad,self.job['map'],self.collision)
        self.assertEqual(recast_height_domain(good,self.job['map'],self.collision),dict(minimum=-304,maximum=936))

    def test_only_exact_recast_memory_diagnostics_enable_retry(self):
        for text in (MEMORY_ERROR,'Recast allocation limit exceeded\n','Recast allocation limit exceeded during contour retry'):
            self.assertTrue(recast_memory_failure(CommandFailure('Recast',1,text)))
        for text in ('out of memory','rcThing: Not Out of memory','Memory allocation failed','Geometry limit exceeded'):
            self.assertFalse(recast_memory_failure(CommandFailure('Recast',1,text)))

    def test_command_preserves_typed_failure_output_for_classifier(self):
        with patch('server.subprocess.run',return_value=subprocess.CompletedProcess(['fixture'],1,MEMORY_ERROR.encode())):
            with self.assertRaises(CommandFailure) as raised:
                self.s.command(self.job,['fixture'],'offline fixture')
        self.assertEqual(raised.exception.output,MEMORY_ERROR)
        self.assertEqual(raised.exception.returncode,1)


if __name__=='__main__':
    unittest.main()
