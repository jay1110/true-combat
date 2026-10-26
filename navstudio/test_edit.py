"""Offline editor/API contract controls in a disposable directory; no RCON."""
import copy
import io
import json
from pathlib import Path
import tempfile
import threading
import unittest
from unittest.mock import patch

from edit import Drafts, graph_hash, action_protection
from server import Studio, Handler


class EditorTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="tce-navstudio-edit-")
        self.root = Path(self.temporary.name).resolve()
        self.store = Drafts(self.root / "drafts")
        self.environment = {k: "abc" for k in ("bsp", "stand", "duck", "geometry", "collision", "runtime_snapshot")}
        self.navigation = dict(nodes=[
            dict(id=1, position=[0, 0, 0], radius=4, accepted=True, reason="map_entity_spawn_seed"),
            dict(id=2, position=[100, 0, 0], radius=20, accepted=True),
            dict(id=3, position=[200, 0, 0], radius=6, accepted=True),
            dict(id=5, position=[200, 0, 100], radius=6, nav_flags=512, accepted=True)],
            edges=[dict(**{"from": 1, "to": 2}, accepted=True),
                   dict(**{"from": 2, "to": 3}, accepted=True),
                   dict(**{"from": 3, "to": 5}, accepted=True)])
        self.base = dict(environment=self.environment, protected_anchor_ids=[3], native_report="base-report-hash")
        self.store.start("obj_test", self.navigation, self.base)

    def tearDown(self):
        # TemporaryDirectory owns this exact freshly-created root only.
        self.assertTrue(self.root.name.startswith("tce-navstudio-edit-"))
        self.temporary.cleanup()

    def change(self, op, **fields):
        doc = self.store.get("obj_test")
        return self.store.change("obj_test", dict(op=op, revision=doc["revision"], **fields), self.environment)

    def test_native_metadata_tail_preserves_imported_name_facing_and_uid(self):
        source=copy.deepcopy(self.navigation);source['nodes'][1].update(name='Tür "Front" \\ Switch',facing=[1,0,0],way_guid=2000000000)
        self.store.start('obj_metadata',source,self.base)
        doc=self.store.get('obj_metadata');self.assertEqual(doc['nodes'][1]['way_guid'],2000000000)
        self.assertEqual(doc['nodes'][1]['facing'],[1,0,0]);self.assertEqual(doc['nodes'][1]['name'],'Tür "Front" \\ Switch')
        self.store.save('obj_metadata',0,self.environment)
        path=self.store.export_candidate('obj_metadata',0,self.environment,self.root/'preview')
        rows=path.read_text(encoding='ascii').splitlines();tail=rows.index('TCE_NODE_METADATA 1 1')
        self.assertEqual(rows[tail+1].split()[:5],['2','2000000000','1','0','0'])
        self.assertEqual(bytes.fromhex(rows[tail+1].split()[5]).decode('utf8'),'Tür "Front" \\ Switch')
        metadata=json.loads(path.with_suffix('.json').read_text(encoding='utf8'))
        self.assertEqual(metadata['waypoint_metadata_contract'],'TCE_NODE_METADATA_1')
        reloaded=Drafts(self.root/'drafts').get('obj_metadata');self.assertEqual(reloaded['nodes'],doc['nodes'])

    def test_metadata_edits_are_undoable_and_invalid_literals_leave_graph_unchanged(self):
        original=copy.deepcopy(self.store.get('obj_test'))
        self.change('update_node',id=2,name='Front Door Switch',facing=[0,1,0])
        self.assertEqual(self.store.get('obj_test')['nodes'][1]['name'],'Front Door Switch')
        self.change('undo');self.assertEqual(self.store.get('obj_test')['nodes'],original['nodes'])
        self.change('redo');before=graph_hash(self.store.get('obj_test'))
        for values in (dict(name='x'*256),dict(name='bad\nname'),dict(facing=[1,1,1]),dict(facing=[float('nan'),0,0])):
            with self.assertRaises(ValueError):self.change('update_node',id=2,**values)
            self.assertEqual(graph_hash(self.store.get('obj_test')),before)

    def test_duplicate_import_uid_is_rejected(self):
        source=copy.deepcopy(self.navigation)
        source['nodes'][0]['way_guid']=39;source['nodes'][1]['way_guid']=39
        with self.assertRaises(ValueError):self.store.start('obj_invalid',source,self.base)
        self.assertIsNone(self.store.get('obj_invalid'))

    def test_dms_normal_radius_is_not_clipped_on_import_save_or_export(self):
        for radius in (150,150.125,300):
            with self.subTest(radius=radius):
                name='obj_radius_'+str(radius).replace('.','_')
                self.store.start(name,dict(nodes=[dict(id=8,position=[0,0,0],radius=radius,way_guid=39)],edges=[]),self.base)
                doc=self.store.get(name);self.assertEqual(doc['nodes'][0]['radius'],radius)
                self.store.save(name,0,self.environment)
                self.assertEqual(Drafts(self.root/'drafts').get(name)['nodes'][0]['radius'],radius)
                path=self.store.export_candidate(name,0,self.environment,self.root/'preview')
                row=path.read_text().splitlines()[1].split();self.assertEqual(float(row[7]),radius)
                self.assertEqual(json.loads(path.with_suffix('.json').read_text())['nodes'][0]['radius'],radius)
        self.change('update_node',id=2,radius=300)
        self.assertEqual(self.store.get('obj_test')['nodes'][1]['radius'],300)
        self.change('undo');self.assertEqual(self.store.get('obj_test')['nodes'][1]['radius'],20)

    def test_radius_limits_do_not_relax_action_nodes_or_accept_nonfinite_values(self):
        before=graph_hash(self.store.get('obj_test'))
        for identity,radius in ((2,301),(2,9),(2,float('inf')),(2,float('nan')),(5,150)):
            with self.subTest(identity=identity,radius=radius):
                with self.assertRaises(ValueError):self.change('update_node',id=identity,radius=radius)
                self.assertEqual(graph_hash(self.store.get('obj_test')),before)
        for number,physical in enumerate((dict(action_roles=[dict(kind='door',role='entry')]),
                dict(editor_role=16),dict(nav_flags=1<<27),dict(action_properties={'tce_door_role':'2'}))):
            source=copy.deepcopy(self.navigation);source['nodes'][1].update(radius=150,**physical)
            with self.subTest(physical=physical):
                with self.assertRaisesRegex(ValueError,'Aktionsradius'):self.store.start('obj_action_radius_'+str(number),source,self.base)
        self.store.start('obj_tactical_radius',dict(nodes=[dict(id=8,position=[0,0,0],radius=150,nav_flags=1<<45)],edges=[]),self.base)
        self.assertEqual(self.store.get('obj_tactical_radius')['nodes'][0]['radius'],150)

    def test_signed_native_id_contract_and_derived_action_facing_are_preserved(self):
        source=copy.deepcopy(self.navigation);source['nodes'][1]['way_guid']=2**31
        with self.assertRaises(ValueError):self.store.start('obj_unsigned',source,self.base)
        before=graph_hash(self.store.get('obj_test'))
        with self.assertRaises(ValueError):self.change('update_node',id=5,facing=[1,0,0])
        self.assertEqual(graph_hash(self.store.get('obj_test')),before)

    def test_custom_properties_survive_import_undo_save_and_literal_candidate_export(self):
        source=copy.deepcopy(self.navigation)
        source['nodes'][1]['action_properties']={'paththrough':'snipe:Front_Door','note':'Tür "A" \\ marker','empty':'','tce_node':'2'}
        self.store.start('obj_properties',source,self.base)
        doc=self.store.get('obj_properties');self.store.save('obj_properties',0,self.environment)
        path=self.store.export_candidate('obj_properties',0,self.environment,self.root/'preview')
        rows=path.read_text(encoding='ascii').splitlines();offset=rows.index('TCE_NODE_PROPERTIES 1 3')
        actual={bytes.fromhex(row.split()[1]).decode('utf8'):('' if row.split()[2]=='-' else bytes.fromhex(row.split()[2]).decode('utf8')) for row in rows[offset+1:]}
        self.assertEqual(actual,{'empty':'','note':'Tür "A" \\ marker','paththrough':'snipe:Front_Door'})
        self.assertTrue(all(row.split()[0]=='2' for row in rows[offset+1:]))
        metadata=json.loads(path.with_suffix('.json').read_text(encoding='utf8'))
        self.assertEqual(metadata['custom_property_contract'],'TCE_NODE_PROPERTIES_1')
        self.assertEqual(Drafts(self.root/'drafts').get('obj_properties')['nodes'][1]['action_properties'],doc['nodes'][1]['action_properties'])
        self.store.change('obj_properties',dict(op='update_node',id=2,revision=0,properties={'note':'changed'}),self.environment)
        changed=next(n for n in self.store.get('obj_properties')['nodes'] if n['id']==2)
        self.assertEqual(changed['action_properties'],{'tce_node':'2','note':'changed'})
        self.store.change('obj_properties',dict(op='undo',revision=1),self.environment)
        restored=next(n for n in self.store.get('obj_properties')['nodes'] if n['id']==2)
        self.assertEqual(restored['action_properties'],source['nodes'][1]['action_properties'])

    def test_property_tail_follows_name_tail_and_reserves_native_keys(self):
        source=copy.deepcopy(self.navigation)
        source['nodes'][1].update(name='Named',way_guid=42,action_properties={'tce_node':'2','editor_note':'proof'})
        self.store.start('obj_both',source,self.base);self.store.save('obj_both',0,self.environment)
        path=self.store.export_candidate('obj_both',0,self.environment,self.root/'preview')
        rows=path.read_text(encoding='ascii').splitlines()
        self.assertLess(rows.index('TCE_NODE_METADATA 1 1'),rows.index('TCE_NODE_PROPERTIES 1 1'))
        before=graph_hash(self.store.get('obj_test'))
        for props in ({'tce_node':'99'},{'tce_fake_proof':'1'},{'bad\nkey':'value'},{'key':'bad\nvalue'}, {'k'*128:'v'}, {'k':'v'*1024}, {'k':1}, {str(i):'v' for i in range(33)}):
            with self.subTest(props=repr(props)[:64]):
                with self.assertRaises(ValueError):self.change('update_node',id=2,properties=props)
                self.assertEqual(graph_hash(self.store.get('obj_test')),before)

    def test_unknown_reserved_import_property_is_not_silently_lost(self):
        source=copy.deepcopy(self.navigation);source['nodes'][1]['action_properties']={'tce_not_understood':'1'}
        with self.assertRaisesRegex(ValueError,'Unbekannte reservierte'):self.store.start('obj_unknown',source,self.base)
        self.assertIsNone(self.store.get('obj_unknown'))

    def test_action_properties_use_actual_imported_target_uid_and_keep_custom_values(self):
        native=dict(nodes=[dict(id=10,index=0,way_guid=39,accepted=True,editor_role=128,editor_drop_target=20,
                               custom_properties={'editor_note':'launch'}),
                           dict(id=20,index=1,way_guid=900,accepted=True,editor_role=0)])
        protected=action_protection(native)
        self.assertEqual(protected['action_properties'][10],{'editor_note':'launch','tce_free_climb':'1','tce_free_climb_target':'900'})

    def test_custom_property_global_pool_limit_includes_utf8_bytes_and_terminators(self):
        doc=copy.deepcopy(self.store.get('obj_test'));prototype=doc['nodes'][1]
        doc['edges']=[];doc['nodes']=[]
        # 1024 bytes of value+terminator, 127 bytes of key+terminator:
        # 29 * 32 records exceed the bounded one-MiB native string pool.
        for index in range(29):
            node=copy.deepcopy(prototype);node['id']=index
            node['action_properties']={('k'*123+str(key).zfill(4)):'v'*1023 for key in range(32)}
            doc['nodes'].append(node)
        with self.assertRaisesRegex(ValueError,'1MiB'):self.store.validate(doc,'obj_test')

    def test_custom_property_global_pair_budget_is_independent_of_pool_size(self):
        doc=copy.deepcopy(self.store.get('obj_test'));prototype=doc['nodes'][1]
        doc['edges']=[];doc['nodes']=[]
        for index in range(129):
            node=copy.deepcopy(prototype);node['id']=index
            node['action_properties']={str(key):'' for key in range(32)};doc['nodes'].append(node)
        with self.assertRaisesRegex(ValueError,'4096'):self.store.validate(doc,'obj_test')

    def test_complete_flow_preserves_direction_and_anchors(self):
        doc = self.store.get("obj_test")
        self.assertEqual(doc["nodes"][0]["radius"], 35)
        self.assertTrue(doc["nodes"][0]["spawn"])
        self.assertFalse(doc["nodes"][2]["locked"])
        self.assertFalse(doc["nodes"][2]["generated_guide"])
        self.change("update_node", id=2, position=[110, 4, 0], radius=30, nav_flags=64)
        doc = self.change("add_node", position=[60, 10, 0], radius=15, nav_flags=0)
        new = doc["next_id"]-1
        doc = self.change("link", **{"from": 2, "to": new})
        self.assertIn(dict(zip(("from", "to"), (2, new))), doc["edges"])
        self.assertNotIn(dict(zip(("from", "to"), (new, 2))), doc["edges"])
        self.change("link", **{"from": 2, "to": new, "bidirectional": True})
        self.change("unlink", **{"from": 2, "to": new, "bidirectional": True})
        self.change("undo")
        before = copy.deepcopy(self.store.get("obj_test"))
        self.change("delete_node", id=new)
        restored = self.change("undo")
        self.assertEqual(restored["nodes"], before["nodes"])
        self.assertEqual(restored["edges"], before["edges"])
        self.change("redo")
        doc = self.change("undo")
        self.store.save("obj_test", doc["revision"], self.environment)
        path = self.store.export_candidate("obj_test", doc["revision"], self.environment, self.root / "preview")
        rows = path.read_text().splitlines()
        self.assertEqual(rows[0], "TCE_NAVMESH 5 obj_test 5 5")
        self.assertTrue(any(row.startswith("3 ") and len(row.split()) == 12 for row in rows[1:5]))
        self.assertTrue(any(row.startswith("5 ") for row in rows[1:6]))
        metadata = json.loads(path.with_suffix(".json").read_text())
        self.assertEqual(metadata["candidate_mode"], "manual_draft")
        self.assertTrue(metadata["directed"])
        reloaded = Drafts(self.root / "drafts").get("obj_test")
        self.assertEqual(graph_hash(reloaded), graph_hash(doc))
        self.assertEqual(reloaded["undo"], doc["undo"])
        self.change("update_node", id=2, radius=25)
        self.assertIsNone(self.store.get("obj_test")["export"])
        with self.assertRaises(ValueError):
            self.store.export_candidate("obj_test", self.store.get("obj_test")["revision"], self.environment, self.root / "preview")

    def test_rejects_stale_unsafe_and_unproven_edits(self):
        for op, fields in (("update_node", {"id": 2, "nav_flags": str(1<<64)}), ("update_node", {"id": 2, "radius": 301}),
                           ("update_node", {"id": 2, "position": [float("nan"), 0, 0]}),
                           ("link", {"from": 2, "to": 2}),
                           ("link", {"from": [], "to": 2})):
            before = graph_hash(self.store.get("obj_test"))
            with self.assertRaises(ValueError):
                self.change(op, **fields)
            self.assertEqual(graph_hash(self.store.get("obj_test")), before)
        with self.assertRaises(RuntimeError):
            self.store.change("obj_test", dict(op="undo", revision=42), self.environment)
        with self.assertRaises(RuntimeError):
            self.store.save("obj_test", 0, dict(self.environment, bsp="different-map"))
        with self.assertRaises(ValueError):
            self.store.path("../outside")

    def test_64bit_flags_roundtrip_and_no_generated_point_loss(self):
        from edit import json_flags
        for value in ((1<<54)|64|1, (1<<63)|512, (1<<64)-1):
            doc=self.change("update_node", id=2, nav_flags=str(value))
            self.store.save("obj_test", doc["revision"], self.environment)
            wire=json.loads(json.dumps(json_flags(self.store.view("obj_test", self.environment))))
            self.assertEqual(next(n for n in wire["nodes"] if n["id"]==2)["nav_flags"],str(value))
            candidate=self.store.export_candidate("obj_test",doc["revision"],self.environment,self.root/"preview")
            lines=candidate.read_text().splitlines()
            row=next(r.split() for r in lines[1:6] if r.split()[0]=="2")
            self.assertEqual(int(row[8]) | (int(row[9])<<32),value)
            reloaded=Drafts(self.root/"drafts").get("obj_test")
            self.assertEqual(next(n for n in reloaded["nodes"] if n["id"]==2)["nav_flags"],value)
            self.assertTrue(any(r.startswith("5 ") for r in lines[1:6]))

    def test_file_tamper_and_archive(self):
        self.store.save("obj_test", 0, self.environment)
        target = self.store.path("obj_test")
        original = target.read_bytes()
        target.write_text("{}")
        with self.assertRaises(ValueError):
            Drafts(self.root / "drafts").get("obj_test")
        with self.assertRaises(RuntimeError):
            self.store.save("obj_test", 0, self.environment)
        target.write_bytes(original)
        self.store.archive("obj_test", 0)
        self.assertIsNone(self.store.get("obj_test"))
        self.assertEqual(len(list((self.root / "drafts").glob("*.archive.json"))), 1)

    def test_native_capacity_boundaries_preserve_dense_directed_graph(self):
        doc = copy.deepcopy(self.store.get("obj_test"))
        template = doc["nodes"][0]
        doc["nodes"] = [dict(template, id=i, position=[i, 0, 0]) for i in range(15872)]
        doc["edges"] = [{"from": i, "to": (i + step) % 15872}
                        for i in range(15872) for step in range(1, 17)]
        self.assertEqual(len(doc["edges"]), 253952)
        self.store.validate(doc, "obj_test")
        doc["edges"].append({"from": 0, "to": 17})
        with self.assertRaisesRegex(ValueError, "15872 Punkte und 253952"):
            self.store.validate(doc, "obj_test")
        doc["edges"].pop()
        doc["nodes"].append(dict(template, id=15872))
        with self.assertRaisesRegex(ValueError, "15872 Punkte und 253952"):
            self.store.validate(doc, "obj_test")

    def test_deleted_drop_target_clears_hidden_reference_and_undo_restores_both(self):
        doc=self.store.get("obj_test")
        launch=next(n for n in doc["nodes"] if n["id"]==2)
        launch["action_roles"]=[dict(kind="walk_drop",role="launch",landing_node=3)]
        launch["action_properties"]={"tce_walk_drop":"1","tce_walk_drop_target":"3"}
        launch["editor_role"]=8
        old=copy.deepcopy(doc)
        deleted=self.change("delete_node",id=3)
        launch=next(n for n in deleted["nodes"] if n["id"]==2)
        self.assertEqual(launch["action_roles"],[])
        self.assertEqual(launch["action_properties"],{})
        self.assertEqual(launch["editor_role"],0)
        self.store.save("obj_test",deleted["revision"],self.environment)
        self.store.export_candidate("obj_test",deleted["revision"],self.environment,self.root/"preview")
        restored=self.change("undo")
        self.assertEqual(restored["nodes"],old["nodes"])
        self.assertEqual(restored["edges"],old["edges"])

    def test_walk_drop_without_flags_is_locked_and_regenerated_without_losing_input(self):
        for original_input in (False, True):
            name = "obj_drop_input" if original_input else "obj_drop_generated"
            nodes = [dict(id=i, index=i-10, position=[i*10, 0, 0], accepted=True,
                          radius=35, nav_flags=0) for i in range(10, 16)]
            nodes[1].update(radius=2, reason="generated_walk_drop_takeoff_live_pending")
            nodes[2].update(radius=6, reason="walk_drop_landing_anchor")
            nodes[3].update(radius=8, nav_flags=1 << 27, reason="generated_jump_takeoff_live_pending")
            if original_input:
                nodes[1]["native_input"] = {"floor_accepted": True}
                nodes[2]["reason"] = "map_entity_spawn_seed"
            pairs = [(10, 11), (11, 12), (14, 13), (13, 11), (11, 15)]
            native = dict(nodes=nodes, edges=[{"from": a, "to": b, "accepted": True} for a, b in pairs],
                          traversal_routes=[dict(kind="jump", approach=14, launch=13, landing=11, exported=True),
                                            dict(kind="walk_drop", approach=10, launch=11, landing=12,
                                                 paired_jump_launch=13, exported=True)])
            protection = action_protection(native)
            for node in nodes:
                node["action_roles"] = protection["action_roles"].get(node["id"], [])
                node["action_properties"] = protection["action_properties"].get(node["id"], {})
            base = dict(environment=self.environment, **{k: protection[k] for k in
                        ("protected_anchor_ids", "generated_guide_ids", "protected_action_edges")})
            doc = self.store.start(name, native, base)
            launch = next(n for n in doc["nodes"] if n["id"] == 11)
            self.assertFalse(launch["locked"])
            self.assertEqual(launch["generated_guide"], not original_input)
            self.assertEqual(launch["radius"], 35 if original_input else 2)
            self.assertEqual(launch["source_radius"], 2)
            self.assertEqual(next(n for n in doc["nodes"] if n["id"] == 12)["spawn"], original_input)
            self.assertEqual(launch["action_properties"], {"tce_walk_drop": "1", "tce_walk_drop_target": "3"})
            self.store.save(name, 0, self.environment)
            candidate = self.store.export_candidate(name, 0, self.environment, self.root / "preview")
            metadata = json.loads(candidate.with_suffix(".json").read_text())
            exported = {n["id"] for n in metadata["nodes"]}
            self.assertIn(11, exported)
            self.assertTrue({10, 12, 14, 15} <= exported)
            self.assertTrue(all(n["requested_radius"] >= 1 for n in metadata["nodes"]))
            self.assertEqual({tuple(e) for e in metadata["edges"]}, set(pairs))
            reloaded = Drafts(self.root / "drafts").get(name)
            self.assertEqual(next(n for n in reloaded["nodes"] if n["id"] == 11)["action_properties"], launch["action_properties"])
            for bad in (dict(native, traversal_routes=native["traversal_routes"][1:]),
                        dict(native, traversal_routes=[native["traversal_routes"][0],
                                                       dict(native["traversal_routes"][1], landing=99)])):
                with self.assertRaises(ValueError):
                    action_protection(bad)
    def test_standing_jump_edit_export_delete_undo_keeps_directed_target(self):
        doc=self.change("update_node",id=2,standing_jump_target=3)
        launch=next(n for n in doc["nodes"] if n["id"]==2)
        self.assertEqual(launch["radius"],2)
        self.assertEqual(launch["action_roles"],[dict(kind="standing_jump",role="launch",landing_node=3)])
        self.store.save("obj_test",doc["revision"],self.environment)
        path=self.store.export_candidate("obj_test",doc["revision"],self.environment,self.root/"preview")
        row=next(line for line in path.read_text().splitlines()[1:] if line.startswith("2 "))
        self.assertEqual(row.split()[-2:],["65","4"])
        before=copy.deepcopy(doc)
        doc=self.change("delete_node",id=3)
        launch=next(n for n in doc["nodes"] if n["id"]==2)
        self.assertFalse(launch["action_roles"])
        self.assertNotIn("tce_stand_jump",launch["action_properties"])
        restored=self.change("undo")
        self.assertEqual(restored["nodes"],before["nodes"])
        self.assertEqual(restored["edges"],before["edges"])

    def test_standing_jump_unlink_clears_recipe_and_undo_restores(self):
        doc=self.change("update_node",id=2,standing_jump_target=3)
        before=copy.deepcopy(doc)
        doc=self.change("unlink",**{"from":2,"to":3})
        launch=next(n for n in doc["nodes"] if n["id"]==2)
        self.assertNotIn("tce_stand_jump",launch["action_properties"])
        self.assertFalse(launch["action_roles"])
        restored=self.change("undo")
        self.assertEqual(restored["nodes"],before["nodes"])
        self.assertEqual(restored["edges"],before["edges"])

    def test_free_climb_directed_target_export_unlink_delete_and_action_replacement(self):
        self.change("update_node",id=5,free_climb_target=None)
        self.assertEqual(next(n for n in self.store.get("obj_test")["nodes"] if n["id"]==5)["nav_flags"],512)
        doc=self.change("update_node",id=2,free_climb_target=3)
        launch=next(n for n in doc["nodes"] if n["id"]==2)
        self.assertEqual(launch["action_roles"],[dict(kind="free_climb",role="launch",landing_node=3)])
        self.assertEqual(launch["nav_flags"],512)
        self.store.save("obj_test",doc["revision"],self.environment)
        path=self.store.export_candidate("obj_test",doc["revision"],self.environment,self.root/"preview")
        row=next(line for line in path.read_text().splitlines()[1:] if line.startswith("2 "))
        self.assertEqual(row.split()[-2:],["129","4"])
        before=copy.deepcopy(doc)
        deleted=self.change("delete_node",id=3)
        launch=next(n for n in deleted["nodes"] if n["id"]==2)
        self.assertFalse(launch["action_roles"])
        self.assertNotIn("tce_free_climb",launch["action_properties"])
        self.assertFalse(launch["nav_flags"]&512)
        restored=self.change("undo")
        self.assertEqual(restored["nodes"],before["nodes"])
        self.change("unlink",**{"from":2,"to":3})
        self.assertFalse(next(n for n in self.store.get("obj_test")["nodes"] if n["id"]==2)["action_roles"])
        self.change("undo")
        doc=self.change("update_node",id=2,free_climb_target=None,standing_jump_target=3)
        launch=next(n for n in doc["nodes"] if n["id"]==2)
        self.assertTrue(launch["nav_flags"]&(1<<27));self.assertFalse(launch["nav_flags"]&512)
        doc=self.change("update_node",id=2,standing_jump_target=None,free_climb_target=3)
        launch=next(n for n in doc["nodes"] if n["id"]==2)
        self.assertEqual(launch["nav_flags"],512)
        with self.assertRaisesRegex(ValueError,"Connect"):
            self.change("update_node",id=2,standing_jump_target=5)

    def test_http_token_origin_and_revision_without_game_access(self):
        studio = Studio.__new__(Studio)
        studio.lock = threading.RLock(); studio.busy = False; studio.token = "token"; studio.drafts = self.store
        studio.require_map = lambda name: name if name == "obj_test" else (_ for _ in ()).throw(ValueError("wrong map"))
        studio.draft_environment = lambda name: self.environment
        studio.rcon = lambda *a, **k: (_ for _ in ()).throw(AssertionError("RCON forbidden"))
        class Server:
            server_port = 8766
        server = Server(); server.studio = studio
        def post(body, token="token", origin="http://127.0.0.1:8766"):
            handler = Handler.__new__(Handler); handler.server = server; handler.path = "/api/edit"
            raw = json.dumps(body).encode()
            handler.headers = {"Host": "127.0.0.1:8766", "Origin": origin, "Content-Type": "application/json",
                               "Content-Length": str(len(raw)), "X-Navstudio-Token": token}
            handler.rfile = io.BytesIO(raw); result = []
            handler.send = lambda status, payload, *args: result.append((status, payload))
            handler.do_POST(); return result[0]
        valid = dict(op="update_node", map="obj_test", revision=0, id=2, radius=22)
        self.assertEqual(post(valid, token="wrong")[0], 403)
        self.assertEqual(post(valid, origin="https://elsewhere.invalid")[0], 403)
        self.assertEqual(post(valid)[0], 200)
        self.assertEqual(post(valid)[0], 409)
        studio.busy = True
        self.assertEqual(post(dict(valid, revision=1))[0], 409)

    def test_edits_invalidate_candidate_and_native_receipt_gate(self):
        studio = Studio.__new__(Studio)
        studio.drafts = self.store; studio.preview = self.root / "preview"; studio.runtime = self.root / "runtime"
        studio.work = self.root; studio.bsp_hash = lambda name: "abc"
        studio.draft_environment = lambda name: self.environment
        studio.preview.mkdir()
        (studio.preview / "obj_test.geometry.json").write_text(json.dumps(dict(
            input={"bsp_sha256": "abc"}, output={"obj_sha256": "abc"}, runtime_snapshot={"sha256": "abc"})))
        def fake_hash(path):
            if "candidate_edit" in Path(path).name:
                import hashlib
                return hashlib.sha256(Path(path).read_bytes()).hexdigest()
            return "abc"
        studio.file_hash = fake_hash
        self.assertFalse(studio.proof_status("obj_test", receipt=False)["valid"])
        self.store.save("obj_test", 0, self.environment)
        self.store.export_candidate("obj_test", 0, self.environment, studio.preview)
        self.assertTrue(studio.proof_status("obj_test", receipt=False)["valid"])
        studio.native = lambda name: dict(generation_complete=True, source="navgen/obj_test_candidate_edit.txt")
        self.assertFalse(studio.proof_status("obj_test")["valid"])  # Export is not a native receipt.
        self.change("update_node", id=2, radius=22)
        self.assertFalse(studio.proof_status("obj_test", receipt=False)["valid"])
        self.store.save("obj_test", 1, self.environment)
        self.assertFalse(studio.proof_status("obj_test", receipt=False)["valid"])


if __name__ == "__main__":
    unittest.main()
