"""Literal Village pedestal/removal and bounded pure proposal contracts."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import zipfile
import unittest

import objectiveanchors as anchors
from navmesh.objective_contact import flag_pickup_contact


class ObjectiveAnchorsTests(unittest.TestCase):
    def fixture(self,two=False,max_probes=2048):
        nodes=[dict(id=i,way_guid=u,position=p,nav_flags=0,crouch=False,radius=35,locked=False,
            editor_role=0,generated_guide=False,action_roles=[],action_properties={},name='keep',facing=[0,0,0])for i,u,p in
            [(1952,674,[1720.,-348.,-29.75]),(686,506,[1728.,-332.5538,-31.75]),(1954,676,[1688.,-316.,-31.75])]]
        if two:nodes[1]['position']=[1720.,-350.,-29.75]
        pairs=[(1952,686),(686,1952),(1952,1954),(1954,1952)]
        doc=dict(map='obj_village',revision=4,base=dict(environment={}),nodes=nodes,edges=[dict(zip(('from','to'),p))for p in pairs])
        native=dict(map='obj_village',generation_complete=True,bbox=dict(half_width=16,stand_height=87,crouch_height=61,origin_above_feet=30),
            nodes=[dict(n,accepted=True,spawn_reachable=True,raw_position=[n['position'][0],n['position'][1],n['position'][2]+30],custom_properties={})for n in nodes],
            edges=[dict(e,accepted=True)for e in doc['edges']])
        records=[dict(source_id=1952,objective_entities=[146])]
        if two:records.append(dict(source_id=686,objective_entities=[146]))
        specs={2:[dict(entity_id=146,tag='FlagB',target='flagonlyB',operation='capture')],5:[]}
        r=anchors.Reconciler(doc,native,records,(2,5),specs,max_probes=max_probes)
        return r,doc,native

    def snapshot(self,gt):
        post=dict(entity=244,model='*15',modelindex=15,classname='func_static',linked=True,contents=-1,
            mins=[1674,-364,-34],maxs=[1708,-332,30])
        obj=dict(entity=241,classname='trigger_objective_info',track='FlagB',position=[1688,-348,-4],spawnflags=66,
            native_goal_kind='capture',target=dict(entity=243,classname='trigger_flagonly_multiple',targetname='flagonlyB',
                position=[1688,-348,-4],bounds=[[1664,-372,-32],[1712,-324,24]],spawnflags=6))
        return dict(map='obj_village',gametype=gt,models=[post]if gt==2 else[],objective_entities=[obj]if gt==2 else[])

    def evidence(self,snapshot):
        folder=tempfile.TemporaryDirectory();self.addCleanup(folder.cleanup);path=Path(folder.name)/'snapshot.json'
        raw=json.dumps(snapshot).encode();path.write_bytes(raw)
        return dict(map='obj_village',gametype=snapshot['gametype'],path=str(path),sha256=anchors.sha(raw))

    def probe(self,gt,unsafe=False,deny_pair=False):
        def floor(p):
            return -29.75 if gt==2 and p[0]<=1724 and -364<=p[1]<-332 else -31.75
        def query(a,b):
            valid=all(abs(p[2]-floor(p))<.011 for p in(a,b))and not unsafe
            if deny_pair and a[0]>1724 and b[0]>1724:valid=False
            imports={endpoint:{stance:dict(accepted=True,exact=abs(p[2]-floor(p))<.011,reason='accepted',feet=[p[0],p[1],floor(p)])
                for stance in('stand','crouch')}for endpoint,p in(('start',a),('end',b))}
            raw=json.dumps([gt,a,b,valid,imports],sort_keys=True)
            return dict(stand='accepted'if valid else'different_floor',crouch='accepted'if valid else'different_floor',
                imports=imports,raw_reply=raw,reply_sha256=anchors.sha(raw.encode()))
        return query

    def visit(self,r,gt,**kwargs):
        s=self.snapshot(gt);r.observe(gt,s,self.evidence(s),self.probe(gt,**kwargs))

    def test_literal_gt2_pedestal_gt5_remove_chooses_common_exact_feet_and_preserves_everything_else(self):
        r,doc,native=self.fixture();before=copy.deepcopy(doc)
        self.visit(r,2);self.visit(r,5);self.visit(r,2)
        proposal,moves=r.proposal();self.assertEqual(doc,before);self.assertEqual(len(moves),1)
        move=moves[0];self.assertEqual(move['source_id'],1952);self.assertEqual(move['way_guid'],674)
        self.assertEqual(move['new_position'][2],-31.75);self.assertGreater(move['new_position'][0],1724)
        for old,new in zip(doc['nodes'],proposal['nodes']):
            self.assertEqual({k:v for k,v in old.items()if k!='position'},{k:v for k,v in new.items()if k!='position'})
        self.assertEqual(doc['edges'],proposal['edges'])
        with self.assertRaisesRegex(RuntimeError,'Nachbarkanten'):r.certificate(proposal,moves)
        for gt in(2,5):
            s=self.snapshot(gt);r.verify_joint(gt,s,self.evidence(s),self.probe(gt),proposal,moves)
        cert=r.certificate(proposal,moves);self.assertEqual(cert['gametypes'],[2,5]);self.assertFalse(cert['actual_use_or_capture_proven'])
        self.assertEqual(cert['scope'],'generated_objective_anchors_and_incident_walks_only')

    def test_unchanged_common_anchor_is_byte_preserved_and_all_states_are_needed(self):
        r,doc,_=self.fixture();old=r.nodes[1952]['position'];old[:]=[1724.5,-348,-31.75]
        r.doc['nodes'][0]['position']=old[:];r.pools[1952]={tuple(old)}
        self.visit(r,2)
        with self.assertRaisesRegex(RuntimeError,'Source-ID 1952'):r.proposal()
        self.visit(r,5);proposal,moves=r.proposal();self.assertEqual(moves,[]);self.assertEqual(proposal,r.doc)

    def test_unsafe_or_budget_exhausted_never_moves_or_erases_the_source(self):
        r,doc,_=self.fixture();before=copy.deepcopy(doc);self.visit(r,2);self.visit(r,5,unsafe=True);self.visit(r,2)
        with self.assertRaisesRegex(RuntimeError,'Source-ID 1952.*UID 674.*Kamera'):r.proposal()
        self.assertEqual(doc,before)
        r,doc,_=self.fixture(max_probes=1);before=copy.deepcopy(doc)
        with self.assertRaisesRegex(RuntimeError,'Suchbudget'):self.visit(r,2)
        self.assertEqual(doc,before)

    def test_final_neighbor_pair_is_checked_at_both_new_positions(self):
        r,doc,_=self.fixture(two=True)
        self.visit(r,2);self.visit(r,5);self.visit(r,2);proposal,moves=r.proposal()
        self.assertEqual(len(moves),2)
        s=self.snapshot(2)
        with self.assertRaisesRegex(RuntimeError,'Nachbarkante'):
            r.verify_joint(2,s,self.evidence(s),self.probe(2,deny_pair=True),proposal,moves)
        self.assertNotIn(2,r.joint_verified)

    def test_missing_or_changed_snapshot_provenance_is_rejected(self):
        r,_,_=self.fixture();s=self.snapshot(2)
        with self.assertRaisesRegex(RuntimeError,'Provenienz'):r.observe(2,s,{},self.probe(2))
        evidence=self.evidence(s);Path(evidence['path']).write_text('{}')
        with self.assertRaisesRegex(RuntimeError,'Rohbeleg'):r.observe(2,s,evidence,self.probe(2))
        self.assertEqual(r.queries,[])

    def test_scanner_archive_binds_objective_identity_and_cannot_guess_a_manual_point(self):
        r,doc,native=self.fixture();candidate=b'candidate\n';metadata=dict(map='obj_village',candidate_sha256=anchors.sha(candidate),
            nodes=[dict(id=1952,kind='objective_contact_anchor',objective_entities=[146]),dict(id=686,kind='spatial_lattice_sample')])
        scanner=dict(map='obj_village',nodes=[dict(id=1952,accepted=True)])
        bundle=dict(candidate=candidate,metadata=json.dumps(metadata).encode(),scanner=json.dumps(scanner).encode())
        manifest=dict(map='obj_village',source='automatic_spatial_scanner',environment={},native_report_sha256=anchors.sha(bundle['scanner']),
            candidate=dict(sha256=anchors.sha(candidate),metadata=anchors.sha(bundle['metadata'])))
        bundle['manifest']=json.dumps(manifest).encode();doc['base']['generation_diagnostics']=anchors.sha(bundle['manifest'])
        self.assertEqual(anchors.archive_anchors(doc,native,bundle),[dict(source_id=1952,objective_entities=[146])])
        with self.assertRaisesRegex(RuntimeError,'fehlt'):anchors.archive_anchors(doc,native,{})
        changed=dict(bundle,metadata=b'{}')
        with self.assertRaises(RuntimeError):anchors.archive_anchors(doc,native,changed)

    def test_flag_pickup_uses_the_existing_native_origin_box_margin(self):
        self.assertTrue(flag_pickup_contact([33.875,0,0],[0,0,0]))
        self.assertFalse(flag_pickup_contact([33.876,0,0],[0,0,0]))
        self.assertFalse(flag_pickup_contact([float('nan'),0,0],[0,0,0]))

    def test_mapscript_gt_override_raw_bytes_and_empty_fallback_are_bound(self):
        with tempfile.TemporaryDirectory()as folder:
            root=Path(folder);base=root/'etmain';mod=root/'tce2';base.mkdir();mod.mkdir();(mod/'mapscripts').mkdir()
            with zipfile.ZipFile(mod/'pak3.pk3','w')as z:
                z.writestr('maps/obj_village_gt2.script','flagpostterrorists { spawn { } }')
                z.writestr('maps/obj_village_gt5.script','flagpostterrorists { spawn { remove } }')
            custom=mod/'mapscripts/obj_village_gt5.script';custom.write_bytes(b'custom GT5 original')
            before=anchors.map_script_binding([base,mod],'obj_village',5,'mapscripts')
            self.assertEqual(before['selected']['relative'],'mapscripts/obj_village_gt5.script')
            custom.write_bytes(b'custom GT5 changed')
            self.assertNotEqual(before,anchors.map_script_binding([base,mod],'obj_village',5,'mapscripts'))
            custom.write_bytes(b'')
            fallback=anchors.map_script_binding([base,mod],'obj_village',5,'mapscripts')
            self.assertEqual(fallback['selected']['relative'],'maps/obj_village_gt5.script')
            self.assertNotEqual(fallback['selected']['sha256'],anchors.map_script_binding([base,mod],'obj_village',2,'mapscripts')['selected']['sha256'])
            (mod/'maps').mkdir();(mod/'maps/obj_village_gt5.script').write_text('actual loose map script override')
            loose=anchors.map_script_binding([base,mod],'obj_village',5,'mapscripts')
            self.assertEqual(loose['selected']['sources'][0]['member'],None)
            self.assertEqual(loose['selected']['sha256'],anchors.sha(b'actual loose map script override'))


if __name__=='__main__':unittest.main()
