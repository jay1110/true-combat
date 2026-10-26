"""Actual draft edits and current file-bound UID adoption, without a game.

Native receipts/WAY bytes are independent serializer fixtures. Production
proof gates, metadata audit, mutation, history and candidate export run here.
This is not a collision or live navigation test.
"""
import copy
import hashlib
import json
from pathlib import Path
import shutil
import struct
import tempfile
import threading
from unittest import TestCase,main
from unittest.mock import Mock,patch

from edit import Drafts,graph_hash,waypoint_metadata,candidate_recipe_nodes,native_way_uid
from server import Studio,MOVEMENT_PROFILES


def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def packed(value):
    result=bytearray()
    while value>127:result.append(value&127);value>>=7
    result.append(value|128);return bytes(result)


def literal(value):
    raw=value.encode('utf8');return packed(len(raw))+raw


def way_bytes(nodes,edges):
    result=bytearray(struct.pack('<BI32s232s6f',9,len(nodes),b'',b'',0,0,0,256,0,24))
    for index,node in enumerate(nodes):
        mask=32|4|(2 if node['name'] else 0)|(64 if any(node['facing']) else 0)
        result.append(mask);result+=packed(node['way_guid'])
        result+=struct.pack('<3f',*node['position'])
        if mask&64:result+=struct.pack('<3f',*node['facing'])
        if mask&2:result+=literal(node['name'])
        result+=packed(1)+literal('tce_node')+literal(str(node['id']))
        targets=sorted(b for a,b in edges if a==index);result+=packed(len(targets))
        for target in targets:
            delta=target-index;result+=packed(delta*2 if delta>=0 else (~delta)*2+1)
    return bytes(result)


class GeneratedIdentityTests(TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.name='obj_test';self.s=Studio.__new__(Studio)
        for field in ('work','preview','runtime','mod'):
            folder=self.root/field;folder.mkdir();setattr(self.s,field,folder)
        self.s.hash_cached={};self.s.native_cached={};self.s.lock=threading.RLock();self.s.busy=False
        self.s.game_mode='tce2';self.s.require_map=lambda n:n
        self.s.bsp_hash=lambda n:'bsp-fixture'
        self.s.drafts=Drafts(self.s.work/'drafts')
        for suffix in ('.stand.json','.duck.json'):self.s.path(self.name,suffix).write_text('{}')
        self.s.path(self.name,'.collision.obj').write_text('independent fixture collision bytes')
        self.s.snapshot_path(self.name).write_text('{}')
        self.s.path(self.name,'.geometry.json').write_text(json.dumps(dict(
            input=dict(bsp_sha256='bsp-fixture'),output=dict(obj_sha256=digest(self.s.path(self.name,'.collision.obj'))),
            runtime_snapshot=dict(sha256=digest(self.s.snapshot_path(self.name))))))
        (self.s.mod/'qagame_mp_x86.dll').write_bytes(b'fixture-module-binding-not-an-executable')
        self.s.draft_environment=lambda n:dict(bsp='bsp-fixture',**{key:self.s.file_hash(self.s.path(n,suffix)) for key,suffix in
            (('stand','.stand.json'),('duck','.duck.json'),('geometry','.geometry.json'),('collision','.collision.obj'))},
            runtime_snapshot=self.s.file_hash(self.s.snapshot_path(n)))
        graph=dict(nodes=[dict(id=i,position=[x,0,0],radius=35) for i,x in ((10,0),(20,64),(30,128))],
                   edges=[dict(zip(('from','to'),edge)) for edge in ((10,20),(20,10),(20,30),(30,20))])
        self.s.drafts.start(self.name,graph,dict(environment=self.s.draft_environment(self.name),movement_profile=MOVEMENT_PROFILES[0]))
        self.export();self.publish_native()

    def export(self):return self.s.prepare_draft(self.name)

    def publish_native(self):
        """Independently encode output, using production UID planner only for IDs.

        That planner is separately compared to actual NG_AssignOutputUIDs in
        test_waypoint_metadata_native. No C/game invocation occurs here.
        """
        doc=self.s.drafts.get(self.name);candidate=self.s.candidate(self.name)
        nodes=[dict(id=n['id'],index=i,position=[*n['position'][:2],n['position'][2]+24],radius=n['radius'],
                    name=n.get('name',''),facing=n.get('facing',[0,0,0]),nav_flags=n['nav_flags'],
                    accepted=True,metadata_supplied=bool(waypoint_metadata(n)[2] or n.get('name') or any(n.get('facing',[0,0,0]))),
                    import_uid=waypoint_metadata(n)[2],way_guid=0) for i,n in enumerate(doc['nodes'])]
        planning=dict(generation_complete=False,nodes=nodes,traversal_routes=[dict(exported=True,kind='walk_drop')])
        planned=candidate_recipe_nodes(planning)
        for node in nodes:node['way_guid']=native_way_uid(planned[node['id']])
        by_id={n['id']:n['index'] for n in nodes};edges={(by_id[e['from']],by_id[e['to']]) for e in doc['edges']}
        report=dict(map=self.name,source='navgen/'+candidate.name,candidate_mode='manual_draft',generation_complete=True,
                    waypoint_metadata_contract='TCE_NODE_METADATA_1',metadata_input_count=sum(n['metadata_supplied'] for n in nodes),
                    nodes=nodes,edges=[dict(e,accepted=True) for e in doc['edges']],bbox=dict(origin_above_feet=24))
        path=self.s.native_path(self.name);path.write_text(json.dumps(report));path.with_suffix('.way').write_bytes(way_bytes(nodes,edges))
        shutil.copyfile(candidate,self.s.staged_candidate_path(candidate))
        inputs=self.s.proof_status(self.name,candidate,receipt=False)
        self.assertTrue(inputs['valid'],inputs)
        proof=dict(schema=1,map=self.name,candidate=candidate.name,inputs=inputs['inputs'],
                   staged_candidate=digest(self.s.staged_candidate_path(candidate)),native_report=digest(path),way=digest(path.with_suffix('.way')))
        (self.s.work/(self.name+'.native-proof.json')).write_text(json.dumps(proof))
        self.s.path(self.name,'.native-audit.json').write_text(json.dumps(dict(binary_report_match=True,
            sha256=proof['way'],report_binding=dict(native_report=proof['native_report'],candidate=self.s.candidate_identity(candidate)))))
        self.assertTrue(self.s.proof_status(self.name)['valid'])
        return report

    def edit(self,op,**values):
        return self.s.edit(dict(map=self.name,op=op,revision=self.s.drafts.get(self.name)['revision'],**values))['draft']

    def uids(self,doc=None):return {n['id']:waypoint_metadata(n)[2] for n in (doc or self.s.drafts.get(self.name))['nodes']}

    def test_delete_pins_survivors_then_export_recheck_and_reload_keep_uids(self):
        self.assertEqual(self.uids(),{10:0,20:0,30:0})
        original_disk=self.s.drafts.path(self.name).read_bytes()
        result=self.edit('delete_node',id=10)
        self.assertEqual(self.uids(result),{20:2,30:3})
        self.assertEqual(self.s.drafts.path(self.name).read_bytes(),original_disk) # Unsaved edit, no hidden write.
        self.assertFalse(self.s.proof_status(self.name)['valid'])
        candidate=self.export()
        self.assertIn('TCE_NODE_METADATA 1 2',candidate.read_text())
        metadata=json.loads(candidate.with_suffix('.json').read_text())
        self.assertEqual({n['id']:n['way_guid'] for n in metadata['nodes']},{20:2,30:3})
        report=self.publish_native()
        self.assertEqual({n['id']:n['way_guid'] for n in report['nodes']},{20:2,30:3})
        self.s.drafts=Drafts(self.s.work/'drafts')
        self.assertEqual(self.uids(),{20:2,30:3})
        self.edit('undo');self.assertEqual(self.uids(),{10:1,20:2,30:3})
        self.edit('redo');self.assertEqual(self.uids(),{20:2,30:3})

    def test_ordinary_edit_and_undo_history_pin_identity_without_pin_on_read(self):
        before=copy.deepcopy(self.s.drafts.get(self.name));disk=self.s.drafts.path(self.name).read_bytes()
        self.edit('begin')
        self.assertEqual(self.s.drafts.get(self.name),before)
        self.assertEqual(self.s.drafts.path(self.name).read_bytes(),disk)
        self.edit('update_node',id=20,name='Door')
        self.assertEqual(self.uids(),{10:1,20:2,30:3})
        self.edit('undo');self.assertEqual(self.uids(),{10:1,20:2,30:3})
        self.assertEqual(next(n for n in self.s.drafts.get(self.name)['nodes'] if n['id']==20).get('name',''),'')

    def test_noop_update_and_repair_do_not_call_identity_provider_or_change_proof(self):
        before=copy.deepcopy(self.s.drafts.get(self.name))
        with patch.object(self.s,'native_edit_identities',side_effect=AssertionError('No adoption on no-op')):
            self.edit('update_node',id=20,radius=35)
            self.edit('update_node',id=20,radius=35,name='',facing=[0,0,0],properties={},nav_flags='0')
            result=self.s.drafts.apply_repair(self.name,before['revision'],before['base']['environment'],copy.deepcopy(before),
                identity_provider=lambda:self.s.native_edit_identities(self.name))
        self.assertEqual(result,before);self.assertEqual(self.s.drafts.get(self.name),before)
        self.assertTrue(self.s.proof_status(self.name)['valid'])

    def test_changed_module_or_way_or_candidate_cannot_supply_uid_adoption(self):
        for kind in ('module','way','candidate'):
            with self.subTest(kind=kind):
                before=copy.deepcopy(self.s.drafts.get(self.name))
                path={'module':self.s.mod/'qagame_mp_x86.dll','way':self.s.native_path(self.name).with_suffix('.way'),
                      'candidate':self.s.candidate(self.name)}[kind]
                raw=path.read_bytes();path.write_bytes(raw+b'changed')
                with self.assertRaisesRegex(RuntimeError,'Identitäten|identitäten'):self.edit('delete_node',id=10)
                self.assertEqual(self.s.drafts.get(self.name),before)
                path.write_bytes(raw)

    def test_partial_report_never_reads_old_way_or_adopts_false_export_ids(self):
        path=self.s.native_path(self.name);report=json.loads(path.read_text())
        report['generation_complete']=False
        for n in report['nodes']:n['way_guid']=0
        path.write_text(json.dumps(report))
        with patch('tce_navgen.read_way',side_effect=AssertionError('Partial graph must not read old WAY')):
            self.assertEqual(self.s.native_edit_identities(self.name),{})
        self.assertEqual(self.uids(),{10:0,20:0,30:0})

    def test_wrong_audit_candidate_and_identity_conflict_reject_without_mutating_history(self):
        path=self.s.path(self.name,'.native-audit.json');raw=path.read_bytes();audit=json.loads(raw)
        audit['report_binding']['candidate']['name']='obj_other_candidate.txt';path.write_text(json.dumps(audit))
        before=copy.deepcopy(self.s.drafts.get(self.name))
        with self.assertRaisesRegex(RuntimeError,'Audit'):self.edit('delete_node',id=10)
        self.assertEqual(self.s.drafts.get(self.name),before)
        path.write_bytes(raw)
        store=self.s.drafts
        with self.assertRaisesRegex(ValueError,'Doppelte'):
            store.change(self.name,dict(op='delete_node',id=10,revision=0),before['base']['environment'],
                         identity_provider=lambda:{10:1,20:1,30:3})
        self.assertEqual(store.get(self.name),before)

    def test_repair_preserves_existing_uids_and_new_points_stay_zero_until_certified(self):
        doc=self.s.drafts.get(self.name);proposal=copy.deepcopy(doc)
        node=copy.deepcopy(proposal['nodes'][-1]);node.update(id=40,position=[192,0,0]);proposal['nodes'].append(node)
        proposal['edges'].extend([dict(zip(('from','to'),edge)) for edge in ((30,40),(40,30))]);proposal['next_id']=41
        result=self.s.drafts.apply_repair(self.name,doc['revision'],doc['base']['environment'],proposal,
                                         identity_provider=lambda:self.s.native_edit_identities(self.name))
        self.assertEqual(self.uids(result),{10:1,20:2,30:3,40:0})
        self.export();self.publish_native()
        self.edit('update_node',id=40,name='New certified point')
        self.assertEqual(self.uids(),{10:1,20:2,30:3,40:4})


if __name__=='__main__':main()
