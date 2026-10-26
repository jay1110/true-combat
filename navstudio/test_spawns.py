"""Spawn edits must survive project reload and preserve BSP/script ownership."""
from copy import deepcopy
from pathlib import Path
import struct
import tempfile
from unittest import TestCase, main

from spawns import SpawnStore, coordinates, script_block, START, END, checked_spawn, checked_document


class SpawnTests(TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.path = Path(self.temp.name) / 'project' / 'spawns.json'
        self.store = SpawnStore(self.path, 'obj_test')
        # Exported native positions are player origins, not floor coordinates.
        self.report = dict(generation_complete=True, nodes=[dict(id=i,
            position=[i * 256, 0, 30], accepted=True, spawn_reachable=True,
            crouch=False, nav_flags=0) for i in range(5)], edges=[
            dict(**{'from': i, 'to': (i + d) % 5}, accepted=True)
            for i in range(5) for d in (-1, 1)])

    def generate(self):
        return self.store.change(dict(op='generate', revision=0, count=4), self.report)

    def test_generated_origins_are_retained_after_move_delete_and_reopen(self):
        doc = self.generate()
        self.assertEqual(len(doc['nodes']), 4)
        self.assertTrue(all(n['position'][2] == 30 for n in doc['nodes']))
        identity = doc['nodes'][0]['id']
        doc = self.store.change(dict(op='move', revision=doc['revision'], id=identity,
                                    position=[48, 80, 30], angle=-45))
        self.assertEqual(doc['nodes'][0]['angle'], 315)
        self.assertEqual(doc['nodes'][0]['status'], 'edited_requires_native_check')
        deleted = doc['nodes'][1]['id']
        doc = self.store.change(dict(op='delete', revision=doc['revision'], id=deleted))
        reopened = SpawnStore(self.path, 'obj_test').read()
        self.assertEqual(reopened, doc)
        self.assertEqual(reopened['nodes'][0]['position'], [48.0, 80.0, 30.0])
        self.assertNotIn(deleted, [n['id'] for n in reopened['nodes']])

    def test_stale_revision_does_not_overwrite_new_edit(self):
        doc = self.generate()
        before = self.path.read_bytes()
        with self.assertRaisesRegex(RuntimeError, 'inzwischen'):
            self.store.change(dict(op='delete', revision=0, id=doc['nodes'][0]['id']))
        self.assertEqual(self.path.read_bytes(), before)

    def test_bsp_spawn_cannot_be_moved_or_deleted(self):
        doc = self.generate()
        before = self.path.read_bytes()
        for operation in ('move', 'delete'):
            with self.assertRaisesRegex(ValueError, 'automatisch'):
                self.store.change(dict(op=operation, revision=doc['revision'],
                                       id='bsp-12', position=[0, 0, 30]))
        self.assertEqual(self.path.read_bytes(), before)

    def test_generation_excludes_crouching_unreachable_and_action_nodes(self):
        report = deepcopy(self.report)
        report['nodes'][0]['crouch'] = True
        report['nodes'][1]['spawn_reachable'] = False
        report['nodes'][2]['nav_flags'] = 1 << 9
        doc = self.store.change(dict(op='generate', revision=0), report)
        self.assertEqual({n['source_node'] for n in doc['nodes']}, {3, 4})

    def test_missing_complete_report_does_not_create_file(self):
        with self.assertRaisesRegex(ValueError, 'geprüfte Navigation'):
            self.store.change(dict(op='generate', revision=0), dict(generation_complete=False))
        self.assertFalse(self.path.exists())

    def test_project_mismatch_and_invalid_coordinates_are_rejected(self):
        self.generate()
        with self.assertRaisesRegex(ValueError, 'anderen Karte'):
            SpawnStore(self.path, 'obj_other').read()
        for value in ([0, 0], [float('nan'), 0, 0], [0, 0, float('inf')],
                      [True, 0, 0], [131073, 0, 0]):
            with self.assertRaises(ValueError): coordinates(value)

    def test_script_block_is_replaced_once_and_preserves_mission_script(self):
        doc = self.generate()
        text = '// game_manager { spawn { fake\n game_manager\n{\n spawn\n {\n wm_announce "Keep me"\n }\n}\n'
        first = script_block(text, doc)
        self.assertEqual(first.count(START), 1)
        self.assertEqual(first.count('tce_dmspawn '), 4)
        doc['nodes'].pop()
        doc['enabled'] = False
        second = script_block(first, doc)
        self.assertEqual(second.count(START), 1)
        self.assertEqual(second.count('tce_dmspawn '), 3)
        self.assertIn('tce_dmspawns 0', second)
        self.assertIn('wm_announce "Keep me"', second)
        self.assertTrue(second.endswith(' }\n}\n'))

    def test_ambiguous_or_missing_script_block_is_not_modified(self):
        doc = self.generate()
        for text in ('not a mapscript', START + START + END, END + START):
            with self.assertRaises(ValueError): script_block(text, doc)


class NativeSpawnEchoTests(TestCase):
    def setUp(self):
        self.name='obj_test'
        self.report=dict(map=self.name,generation_complete=True,
            bbox=dict(half_width=16,stand_height=87,crouch_height=61,origin_above_feet=30),
            nodes=[dict(id=i,position=[i*256+.123456,-40.55555,30.25003],accepted=True,
                        spawn_reachable=True,crouch=False,nav_flags=0)for i in(1,2)],
            edges=[dict(**{'from':1,'to':2},accepted=True),dict(**{'from':2,'to':1},accepted=True)])
        self.previous=dict(schema=1,map=self.name,revision=4,next_id=20,enabled=False,nodes=[])

    def check(self,node,offset=30):
        # Independently model the existing C command: four-decimal argv ->
        # atof/float origin, unchanged native subtraction, five-decimal printf.
        f32=lambda value:struct.unpack('f',struct.pack('f',value))[0]
        native=[f32(float('%.4f'%v))for v in node['position']]
        origin=[float('%.5f'%v)for v in native]
        return dict(accepted=True,map=self.name,origin=origin,
            feet=[origin[0],origin[1],float('%.5f'%f32(native[2]-offset))],
            origin_above_feet=offset,traces=10,reason='native_standing_floor_validated')

    def document(self,nodes=None,attempts=None,report=None,previous=None):
        nodes=self.report['nodes']if nodes is None else nodes
        if attempts is None:attempts=[dict(source_id=n['id'],check=self.check(n))for n in nodes]
        return checked_document(self.name,self.previous if previous is None else previous,nodes,
                                self.report if report is None else report,attempts)

    def test_actual_echo_binds_exact_profile_and_preserves_source_and_origin(self):
        before=deepcopy((self.report,self.previous))
        nodes=self.report['nodes'];attempts=[dict(source_id=n['id'],check=self.check(n))for n in nodes]
        doc=self.document(attempts=attempts)
        self.assertEqual(doc['revision'],5);self.assertEqual(doc['next_id'],22);self.assertFalse(doc['enabled'])
        self.assertEqual([n['source_node']for n in doc['nodes']],[1,2])
        self.assertEqual([n['position']for n in doc['nodes']],[coordinates(n['position'])for n in nodes])
        self.assertEqual((self.report,self.previous),before)
        attempts[0]['check']['origin'][0]=999
        self.assertNotEqual(doc['nodes'][0]['native_check']['origin'][0],999)

    def test_float32_large_coordinates_match_exact_printed_echo_without_radius(self):
        node=deepcopy(self.report['nodes'][0]);node['position']=[100000.123456,-100000.654321,30000.246813]
        check=self.check(node)
        self.assertNotEqual(coordinates(node['position']),check['origin'])
        self.assertTrue(checked_spawn(self.name,node,self.report,check))
        check['origin'][0]=coordinates(node['position'])[0]
        self.assertFalse(checked_spawn(self.name,node,self.report,check))

    def test_ordinary_legacy_body_profile_is_explicit_and_not_inferred_from_echo(self):
        report=deepcopy(self.report);report['bbox']=dict(half_width=14,stand_height=70,crouch_height=49,origin_above_feet=24)
        node=report['nodes'][0];check=self.check(node,24)
        self.assertTrue(checked_spawn(self.name,node,report,check))
        for key,value in (('half_width',16),('stand_height',87),('crouch_height',61),('origin_above_feet',30)):
            wrong=deepcopy(report);wrong['bbox'][key]=value
            self.assertFalse(checked_spawn(self.name,node,wrong,check),key)
        self.assertFalse(checked_spawn(self.name,node,self.report,check))

    def test_wrong_map_accepted_missing_trace_or_offset_never_authorize_document(self):
        node=self.report['nodes'][0]
        for changes in (dict(map='obj_other'),dict(accepted=1),dict(accepted=False),dict(traces=0),dict(traces=True),
                        dict(traces=-1),dict(origin_above_feet=24),dict(origin_above_feet=None)):
            check=dict(self.check(node),**changes)
            self.assertFalse(checked_spawn(self.name,node,self.report,check),changes)
            with self.assertRaises(ValueError):self.document(nodes=[node],attempts=[dict(source_id=1,check=check)])
        report=deepcopy(self.report);report['map']='obj_other'
        self.assertFalse(checked_spawn(self.name,node,report,self.check(node)))

    def test_origin_and_feet_echo_cannot_be_another_candidate_or_projected_point(self):
        node=self.report['nodes'][0];check=self.check(node)
        for field,axis,delta in (('origin',0,.00001),('origin',2,1),('feet',0,.00001),('feet',1,3),('feet',2,1.0001)):
            wrong=deepcopy(check);wrong[field][axis]+=delta
            self.assertFalse(checked_spawn(self.name,node,self.report,wrong),(field,axis,delta))
        self.assertFalse(checked_spawn(self.name,node,self.report,self.check(self.report['nodes'][1])))
        moved=deepcopy(node);moved['position'][0]+=1
        with self.assertRaises(ValueError):self.document(nodes=[moved],attempts=[dict(source_id=1,check=self.check(moved))])

    def test_native_one_unit_floor_rule_does_not_move_spawn_origin(self):
        node=self.report['nodes'][0]
        for delta in (-1,-.125,.125,1):
            check=self.check(node);check['feet'][2]+=delta
            self.assertTrue(checked_spawn(self.name,node,self.report,check),delta)
            doc=self.document(nodes=[node],attempts=[dict(source_id=1,check=check)])
            self.assertEqual(doc['nodes'][0]['position'],coordinates(node['position']))

    def test_invalid_nonfinite_missing_and_boolean_vectors_fail_closed(self):
        node=self.report['nodes'][0]
        for vector in (None,[1,2],[True,0,0],[float('nan'),0,0],[0,float('inf'),0],'1 2 3'):
            for field in('origin','feet'):
                check=self.check(node);check[field]=vector
                self.assertFalse(checked_spawn(self.name,node,self.report,check),(field,vector))
        wrong=deepcopy(node);wrong['position']=[float('nan'),0,0]
        self.assertFalse(checked_spawn(self.name,wrong,self.report,self.check(node)))

    def test_duplicate_mixed_attempts_unreachable_action_or_incomplete_report_rejected(self):
        node=self.report['nodes'][0];attempt=dict(source_id=1,check=self.check(node))
        with self.assertRaises(ValueError):self.document(nodes=[node],attempts=[deepcopy(attempt),deepcopy(attempt)])
        failed=deepcopy(attempt);failed['check']['accepted']=False
        with self.assertRaises(ValueError):self.document(nodes=[node],attempts=[failed,attempt])
        with self.assertRaises(ValueError):self.document(nodes=[node],attempts=[dict(source_id=2,check=self.check(node))])
        for changes in (dict(accepted=False),dict(spawn_reachable=False),dict(crouch=True),dict(nav_flags=1<<9),dict(editor_role=9)):
            wrong=dict(node,**changes)
            self.assertFalse(checked_spawn(self.name,wrong,self.report,self.check(node)))
        report=dict(self.report,generation_complete=False)
        self.assertFalse(checked_spawn(self.name,node,report,self.check(node)))
        with self.assertRaises(ValueError):self.document(previous=dict(self.previous,map='obj_other'))


if __name__ == '__main__': main()
