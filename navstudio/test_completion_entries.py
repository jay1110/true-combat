"""File-bound directed spawn-entry acceptance, with real WAY audit/install.

Temporary native reports are explicit fixtures, not collision or live claims.
No game, API, production navigation, or native module is touched.
"""
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest

TOOLS = Path(__file__).resolve().parent / "workers"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))
import tce_navgen as audit
from completion import main_source_ids, navigation_readiness, spawn_bindings
from goalgen import make_plan, playable_nodes
from test_directed_traversal_proofs import fixture as drop_fixture


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def save_json(path, value):
    raw = json.dumps(value, sort_keys=True, separators=(',', ':')).encode()
    path.write_bytes(raw)
    return raw


class SourceBoundEntryFixture:
    """A real Format5 candidate and sidecars, with a no-return upper spawn."""
    def __init__(self, root):
        self.root = Path(root)
        self.native = self.root / 'native'; self.preview = self.root / 'preview'
        self.work = self.root / 'work'
        self.native.mkdir(); self.preview.mkdir(); self.work.mkdir()
        self.name = 'obj_fixture'
        self.report_path = self.native / (self.name + '_navmesh.json')
        self.candidate = self.native / (self.name + '_candidate_edit.txt')
        self.metadata_path = self.preview / self.candidate.with_suffix('.json').name
        self.geometry_path = self.preview / (self.name + '.geometry.json')
        self.snapshot_path = self.root / (self.name + '_entities.json')
        self.attempt_path = self.work / (self.name + '.native-attempt.json')
        report, _, _, _ = drop_fixture()
        report.update(schema=1, map=self.name, generation_complete=True,
                      source='navgen/' + self.candidate.name,
                      bbox=dict(half_width=16, stand_height=87, crouch_height=61, origin_above_feet=30),
                      native_input_coverage=dict(capacity=dict(input_validation_complete=True, input_truncated=False)))
        report['nodes'].extend([
            dict(id=6842, position=[0,128,132.25], radius=35, nav_flags=0, crouch=False),
            dict(id=1522, position=[284,0,30.25], radius=35, nav_flags=0, crouch=False),
            dict(id=1523, position=[412,0,30.25], radius=35, nav_flags=0, crouch=False)])
        links = [(6841,6842), (6842,6841), (1521,1522), (1522,1521), (1522,1523), (1523,1522)]
        report['edges'].extend(dict(**{'from':a,'to':b}, accepted=True, reason='external_native_validated_walk')
                               for a,b in links)
        for index, node in enumerate(report['nodes']):
            reason = 'map_entity_spawn_seed' if node['id'] in (6841,1523) else 'external_native_validated_floor'
            node.update(index=index, accepted=True, spawn_reachable=True, reason=reason,
                        raw_position=list(node['position']), flags=0, packed=0)
            node['native_input'] = dict(floor_accepted=True, floor_reason=reason,
                                        initial_rejected_incident_links=0)
        self.report = report
        self.snapshot = dict(map=self.name, gametype=2, entities=[])
        self.bsp = self.root / (self.name + '.bsp')
        self.bsp.write_bytes(b'explicit fixture BSP identity; never loaded by an engine')
        self.geometry = dict(map=self.name, entity_state_source='runtime_snapshot',
                             input=dict(bsp_sha256=sha(self.bsp.read_bytes())), spawns=[
            dict(entity=60, classname='team_CTF_redspawn', spawnflags='2', origin_bsp=[0,0,156]),
            dict(entity=61, classname='team_CTF_bluespawn', spawnflags='2', origin_bsp=[412,0,54])])
        self.metadata = dict(map=self.name, candidate_mode='manual_draft', nodes=[], edges=[])
        self.commit()

    def commit(self, update_attempt=True):
        snapshot_raw = save_json(self.snapshot_path, self.snapshot)
        self.geometry['runtime_snapshot'] = dict(path=str(self.snapshot_path), sha256=sha(snapshot_raw))
        geometry_raw = save_json(self.geometry_path, self.geometry)
        nodes = self.report['nodes']; edges = self.report['edges']
        rows = [f'TCE_NAVMESH 5 {self.name} {len(nodes)} {len(edges)}']
        source_nodes = []
        for node in nodes:
            identity = node['id']; feet = list(node['position']); feet[2] -= 30
            spawn = node.get('native_input', {}).get('floor_reason') == 'map_entity_spawn_seed'
            role = node.get('editor_role', 1 if node['radius'] < 10 else 0)
            target = node.get('editor_drop_target', -1)
            rows.append(' '.join(map(str, (identity, *feet, 0, int(spawn), 1, node['radius'], 0, 0,
                                           role, target+1 if target >= 0 else 0))))
            source_nodes.append(dict(id=identity, position=feet, spawn=spawn,
                                     source_reason=node.get('native_input', {}).get('floor_reason')))
        rows.extend(f"{edge['from']} {edge['to']}" for edge in edges)
        content = ('\n'.join(rows)+'\n').encode('ascii')
        self.candidate.write_bytes(content)
        self.metadata.update(candidate_sha256=sha(content), nodes=source_nodes,
                             edges=[[e['from'],e['to']] for e in edges],
                             sources=dict(geometry=dict(sha256=sha(geometry_raw))))
        meta_raw = save_json(self.metadata_path, self.metadata)
        report_raw = save_json(self.report_path, self.report)
        if update_attempt:
            self.attempt = dict(map=self.name, gametype=self.snapshot['gametype'],
                                candidate=self.candidate.name, generation_complete=True,
                                native_report=sha(report_raw), staged_candidate=sha(content),
                                inputs=dict(candidate_metadata=sha(meta_raw), geometry=sha(geometry_raw),
                                            runtime_snapshot=sha(snapshot_raw), bsp=sha(self.bsp.read_bytes())))
            save_json(self.attempt_path, self.attempt)
        self.write_way()

    def binding(self):
        return spawn_bindings(self.report, self.report_path)

    def readiness(self):
        return navigation_readiness(self.report, self.binding())

    def write_way(self):
        def packed(number):
            result = bytearray()
            while number > 127:
                result.append(number & 127); number >>= 7
            result.append(number | 128)
            return bytes(result)
        def string(value):
            raw = value.encode(); return packed(len(raw)) + raw
        nodes = self.report['nodes']; indices = {n['id']:i for i,n in enumerate(nodes)}
        uids = {6841:11, 6842:12, 1521:22, 1522:23, 1523:24}
        data = bytearray(audit.HEADER.pack(9, len(nodes), b'', b'', *([0.]*6)))
        for index, node in enumerate(nodes):
            data.append(1 | 2 | 4 | 16 | 32 | 64)
            data += packed(uids.get(node['id'], node['id']+100))
            data += struct.pack('<3f', *node['position']) + struct.pack('<3f', 0,0,0)
            data += packed(node.get('nav_flags',0)) + struct.pack('<f', node['radius']) + string('')
            properties = {'tce_node':str(node['id'])}
            if node['id'] == 6841:
                properties.update(tce_walk_drop='1', tce_walk_drop_target='22')
            data += packed(len(properties))
            for key,value in sorted(properties.items()):
                data += string(key) + string(value)
            targets = sorted(indices[e['to']] for e in self.report['edges']
                             if e['accepted'] and e['from']==node['id'])
            data += packed(len(targets))
            for target in targets:
                delta = target-index
                data += packed(delta << 1 if delta >= 0 else (~delta << 1) | 1)
        self.report_path.with_suffix('.way').write_bytes(data)


class CompletionEntryTests(unittest.TestCase):
    def fixture(self):
        temp = tempfile.TemporaryDirectory(); self.addCleanup(temp.cleanup)
        return SourceBoundEntryFixture(temp.name)

    def test_actual_file_bindings_accept_spawn_drop_to_unique_main_without_return(self):
        f = self.fixture(); binding = f.binding(); result = f.readiness()
        self.assertIsNotNone(binding)
        self.assertEqual({r['source_id'] for r in binding['rows']}, {6841,1523})
        self.assertEqual(binding['report_sha256'], sha(f.report_path.read_bytes()))
        self.assertEqual(binding['bsp_sha256'], sha(f.bsp.read_bytes()))
        self.assertTrue(result['ready']); self.assertTrue(result['directed_entry_connectivity_pass'])
        self.assertEqual(result['entry_only_source_ids'], [6841,6842])
        self.assertEqual(result['component_sizes'], [3,2])
        self.assertEqual(main_source_ids(f.report), frozenset((1521,1522,1523)))
        self.assertFalse(any(e['from'] in (1521,1522,1523) and e['to'] in (6841,6842) for e in f.report['edges']))

    def test_legacy_report_without_source_bindings_remains_blocked(self):
        f = self.fixture(); result = navigation_readiness(f.report)
        self.assertFalse(result['ready']); self.assertFalse(result['directed_entry_connectivity_pass'])
        self.assertEqual(result['outside_main_source_ids'], [6841,6842])

    def test_main_goals_exclude_the_spawn_entry_and_explicitly_block_its_mission(self):
        f = self.fixture(); self.assertTrue(f.readiness()['ready'])
        self.assertEqual(set(playable_nodes(f.report)[0]), {1521,1522,1523})
        source=dict(tag='spawn_flag', entity_id=99, position=[0,0,132.25], aim_position=[0,0,132.25],
                    owner=1, stealing_team=2, operation='take_flag')
        plan=make_plan(f.report,[source],2)
        self.assertFalse(plan['mission_geometry_complete'])
        self.assertEqual(plan['unresolved'][0]['reason'],'objective_contact_outside_main_component')
        self.assertEqual(plan['unresolved'][0]['source_node_ids'],[6841])

    def test_stale_missing_or_wrong_source_files_fail_closed(self):
        for mode in ('metadata','candidate','geometry','snapshot','missing_meta','missing_snapshot','changed_report'):
            with self.subTest(mode=mode):
                f=self.fixture()
                target={'metadata':f.metadata_path,'candidate':f.candidate,'geometry':f.geometry_path,
                        'snapshot':f.snapshot_path}.get(mode)
                if target: target.write_bytes(target.read_bytes()+b' ')
                elif mode=='missing_meta':f.metadata_path.unlink()
                elif mode=='missing_snapshot':f.snapshot_path.unlink()
                else:f.report['nodes'][0]['raw_position'][0]+=.1
                self.assertIsNone(f.binding())
                self.assertFalse(f.readiness()['ready'])

    def test_snapshot_map_invalid_gt_and_geometry_source_are_not_reinterpreted(self):
        cases=[('map','obj_other'),('gametype',3),('gametype','2'),('gametype',True)]
        for key,value in cases:
            with self.subTest(key=key,value=value):
                f=self.fixture();f.snapshot[key]=value;f.commit()
                self.assertIsNone(f.binding());self.assertFalse(f.readiness()['ready'])
        f=self.fixture();f.geometry['entity_state_source']='offline_bsp_only';f.commit()
        self.assertIsNone(f.binding())

    def test_native_attempt_binds_actual_gt_and_all_source_bytes(self):
        for mode in ('missing','gt','string_gt','map','source','report','staged','metadata','geometry','snapshot','bsp','incomplete'):
            with self.subTest(mode=mode):
                f=self.fixture();attempt=copy.deepcopy(f.attempt)
                if mode=='missing':f.attempt_path.unlink()
                else:
                    if mode=='gt':attempt['gametype']=5
                    elif mode=='string_gt':attempt['gametype']='2'
                    elif mode=='map':attempt['map']='obj_other'
                    elif mode=='source':attempt['candidate']='other_candidate.txt'
                    elif mode=='report':attempt['native_report']='0'*64
                    elif mode=='staged':attempt['staged_candidate']='0'*64
                    elif mode=='incomplete':attempt['generation_complete']=False
                    else:attempt['inputs'][{'metadata':'candidate_metadata','snapshot':'runtime_snapshot'}.get(mode,mode)]='0'*64
                    save_json(f.attempt_path,attempt)
                self.assertIsNone(f.binding());self.assertFalse(f.readiness()['ready'])

    def test_coherently_rehashed_different_gt_snapshot_cannot_reuse_original_native_attempt(self):
        f=self.fixture();old=f.attempt_path.read_bytes()
        f.snapshot['gametype']=5;f.commit(update_attempt=False)
        self.assertEqual(f.attempt_path.read_bytes(),old)
        self.assertIsNone(f.binding());self.assertFalse(f.readiness()['ready'])

    def test_intermission_fake_candidate_or_nonexternal_mode_cannot_be_a_spawn_entry(self):
        for mode in ('intermission','fake_spawn_flag','legacy_native_mode','candidate_map','candidate_hash'):
            with self.subTest(mode=mode):
                f=self.fixture()
                if mode=='intermission':
                    f.geometry['spawns'][0]['classname']='info_player_intermission';f.commit()
                elif mode=='fake_spawn_flag':
                    f.metadata['nodes'][0]['source_reason']='manual';save_json(f.metadata_path,f.metadata)
                elif mode=='legacy_native_mode':
                    f.report['candidate_mode']='spatial_floor_frontier';save_json(f.report_path,f.report)
                elif mode=='candidate_map':
                    f.candidate.write_bytes(f.candidate.read_bytes().replace(b'obj_fixture',b'obj_other'))
                else:
                    f.metadata['candidate_sha256']='0'*64;save_json(f.metadata_path,f.metadata)
                self.assertFalse(f.readiness()['ready'])

    def test_changed_binding_signature_map_contract_or_capacity_never_authorizes(self):
        for mode in ('signature','map','contract','truncated','incomplete'):
            with self.subTest(mode=mode):
                f=self.fixture();binding=f.binding()
                if mode=='signature':binding['report_signature']='0'*64
                elif mode=='map':binding['map']='obj_other'
                elif mode=='contract':binding['contract']='manual_override'
                else:
                    f.report['native_input_coverage']['capacity']['input_truncated' if mode=='truncated' else 'input_validation_complete']=mode=='truncated'
                    f.commit();binding=f.binding()
                self.assertFalse(navigation_readiness(f.report,binding)['ready'])

    def test_unknown_source_component_and_wrong_way_spawn_cycle_stay_blocked(self):
        for mode in ('unknown_root','no_main_spawn','wrong_direction','tie','trap'):
            with self.subTest(mode=mode):
                f=self.fixture()
                if mode=='unknown_root':
                    node=copy.deepcopy(f.report['nodes'][2]);node.update(id=7000,index=5,position=[-128,128,132.25],raw_position=[-128,128,132.25])
                    f.report['nodes'].append(node)
                    f.report['edges'].append(dict(**{'from':7000,'to':6842},accepted=True,reason='external_native_validated_walk'))
                elif mode=='no_main_spawn':
                    f.report['nodes'][4]['native_input']['floor_reason']='external_native_validated_floor'
                elif mode=='wrong_direction':
                    f.report['edges'][0].update(**{'from':1521,'to':6841})
                elif mode=='tie':
                    f.report['nodes']=f.report['nodes'][:4]
                    f.report['edges']=[e for e in f.report['edges'] if 1523 not in (e['from'],e['to'])]
                else:
                    node=copy.deepcopy(f.report['nodes'][2]);node.update(id=7000,index=5,position=[700,0,30.25],raw_position=[700,0,30.25])
                    f.report['nodes'].append(node)
                    f.report['edges'].append(dict(**{'from':1523,'to':7000},accepted=True,reason='external_native_validated_walk'))
                f.commit();self.assertFalse(f.readiness()['ready'])

    def test_missing_unexported_damaging_or_ordinary_fake_drop_is_blocked(self):
        for mode in ('missing','unexported','damage','model','speed','impulse','edge'):
            with self.subTest(mode=mode):
                f=self.fixture();route=f.report['traversal_routes'][0]
                if mode=='missing':f.report['traversal_routes']=[]
                elif mode=='unexported':route['exported']=False
                elif mode=='damage':route['crashland_delta']=38.01
                elif mode=='model':route['model']='ordinary_distance_guess'
                elif mode=='speed':route['sample_count']=2
                elif mode=='impulse':route['impulse_z']=238
                else:f.report['edges'][0]['accepted']=False
                f.commit();self.assertFalse(f.readiness()['ready'])

    def test_full_way_audit_and_install_use_directed_entry_gate_without_override(self):
        f=self.fixture();before={p: p.read_bytes() for p in f.root.rglob('*') if p.is_file()}
        result=audit.inspect(f.report_path)
        self.assertFalse(result['connectivity_pass'])
        self.assertTrue(result['directed_entry_connectivity_pass']);self.assertTrue(result['navigation_ready'])
        self.assertEqual(result['entry_only_source_ids'],[6841,6842])
        self.assertEqual(result['unresolved_nodes'],[])
        self.assertEqual(result['validated_traversal_routes'][0]['kind'],'manual_walk_drop')
        self.assertFalse(result['live_bot_traversal_verified']);self.assertFalse(result['full_map_verified'])
        self.assertEqual(before,{p:p.read_bytes() for p in f.root.rglob('*') if p.is_file()})
        target=f.root/'temporary-install'
        args=SimpleNamespace(install=True,report=f.report_path,allow_disconnected=False,test_main_component=False,nav_directory=target)
        installed=audit.install(args)
        self.assertEqual((target/(f.name+'.way')).read_bytes(),f.report_path.with_suffix('.way').read_bytes())
        self.assertFalse(installed['connectivity_pass']);self.assertTrue(installed['navigation_ready'])
        self.assertFalse(installed.get('test_subset',False))

    def test_way_drop_property_and_missing_source_still_block_actual_audit_install(self):
        for mode in ('way_property','source'):
            with self.subTest(mode=mode):
                f=self.fixture();target=f.root/'must-not-install'
                if mode=='source':f.metadata_path.unlink()
                else:
                    path=f.report_path.with_suffix('.way')
                    path.write_bytes(path.read_bytes().replace(b'tce_walk_drop_target',b'tce_walk_drop_targeX'))
                args=SimpleNamespace(install=True,report=f.report_path,allow_disconnected=False,test_main_component=False,nav_directory=target)
                with self.assertRaises(ValueError):audit.install(args)
                self.assertFalse(target.exists())

    def test_actual_way_audit_install_rejects_incomplete_or_changed_drop_physics(self):
        mutations = [dict(gravity=800), dict(step_seconds=.016),
                     dict(landing_support_samples=8), dict(touchdown_support_model='guessed'),
                     dict(maxtravel=209), dict(crashland_delta=38.01)]
        for mutation in mutations:
            with self.subTest(mutation=mutation):
                f=self.fixture();f.report['traversal_routes'][0].update(mutation);f.commit()
                target=f.root/'must-not-install'
                args=SimpleNamespace(install=True,report=f.report_path,allow_disconnected=False,
                                     test_main_component=False,nav_directory=target)
                with self.assertRaises(ValueError):audit.install(args)
                self.assertFalse(target.exists())


if __name__ == '__main__':
    unittest.main()
