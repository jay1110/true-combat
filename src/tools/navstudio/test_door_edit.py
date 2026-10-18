"""Door action identities survive export indices, edits and project reloads."""
import copy
import sys
import tempfile
from pathlib import Path
import unittest

from edit import Drafts, action_protection, door_role, graph_hash
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tce_navgen import audit_doors


class DoorEditTests(unittest.TestCase):
    def test_staged_local_door_keeps_mode_three_and_cannot_skip_use(self):
        native=copy.deepcopy(self.native)
        for n in native['nodes']:
            n['door_recipe'].update(mode=3,use_entity=70,use_model=4,validated=True,exported=True)
        native['edges']=[e for e in native['edges'] if (e['from'],e['to'])!=(30,20)]
        protection=action_protection(native)
        nav=copy.deepcopy(native)
        for n in nav['nodes']:
            n['action_properties']=protection['action_properties'][n['id']]
            n['action_roles']=protection['action_roles'][n['id']]
        other=Drafts(self.root/'staged')
        doc=other.start('staged',nav,dict(environment=self.env,**protection))
        other.save('staged',doc['revision'],self.env)
        rows=other.export_candidate('staged',doc['revision'],self.env,self.root/'staged-preview').read_text().splitlines()
        self.assertTrue(all(int(row.split()[15])==3 for row in rows[1:4]))
        binary=[dict(flags=n['nav_flags'],properties=protection['action_properties'][n['id']]) for n in native['nodes']]
        for e in native['edges']:e['reason']='external_native_validated_walk'
        by_id={n['id']:n for n in native['nodes']}
        pairs={(by_id[e['from']]['index'],by_id[e['to']]['index']) for e in native['edges']}
        self.assertEqual(len(audit_doors(native,by_id,binary,pairs)),3)
        native['edges'].append(dict(**{'from':30,'to':20},accepted=True))
        with self.assertRaisesRegex(ValueError,'without safe USE'):
            audit_doors(native,by_id,binary,pairs|{(2,1)})

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="navstudio-door-edit-")
        self.root = Path(self.temp.name)
        self.env = dict(bsp="fixture", stand="stand", duck="duck", geometry="geometry")
        self.native = dict(nodes=[], edges=[])
        for identity, index, role, target in [(10, 0, 1, 20), (20, 1, 2, 30), (30, 2, 3, -1)]:
            recipe = dict(entity=70, use_entity=71, target_source_id=target,
                          mode=2, team=3, role=role, model=4, use_model=5,
                          use_source_id=10, entry_source_id=20, aim=[90, 40, 48])
            self.native['nodes'].append(dict(id=identity, index=index, position=[index*100, 0, 0],
                radius=8, nav_flags=128 if role != 3 else 0, accepted=True, door_recipe=recipe))
        self.native['edges'] = [dict(**{'from':a,'to':b},accepted=True) for a,b in [(10,20),(20,30),(30,20),(20,10)]]
        protection = action_protection(self.native)
        nav = copy.deepcopy(self.native)
        for node in nav['nodes']:
            node['action_properties'] = protection['action_properties'][node['id']]
            node['action_roles'] = protection['action_roles'][node['id']]
        self.store = Drafts(self.root/'drafts')
        self.store.start('navstudio_doors',nav,dict(environment=self.env,**protection))

    def tearDown(self):
        self.temp.cleanup()

    def change(self, op, **kwargs):
        doc = self.store.get('navstudio_doors')
        return self.store.change('navstudio_doors',dict(op=op,revision=doc['revision'],**kwargs),self.env)

    def export(self):
        doc = self.store.get('navstudio_doors')
        self.store.save('navstudio_doors',doc['revision'],self.env)
        return self.store.export_candidate('navstudio_doors',doc['revision'],self.env,self.root/'preview')

    def test_format6_preserves_graph_ids_instead_of_way_uids(self):
        rows = self.export().read_text().splitlines()
        self.assertEqual(rows[0],'TCE_NAVMESH 6 navstudio_doors 3 4')

        use, entry, exit = [row.split() for row in rows[1:4]]
        self.assertEqual(len(entry),25)
        # Native entity slots +1, target source ID +1, mode/team/role,
        # door/use brush models, use/entry source IDs +1, absolute aim.
        self.assertEqual(list(map(int,entry[12:22])),[71,72,31,2,3,2,4,5,11,21])
        self.assertEqual(list(map(float,entry[22:])),[90,40,48])
        self.assertEqual(int(exit[14]),0)
        loaded = Drafts(self.root/'drafts').get('navstudio_doors')
        self.assertEqual(graph_hash(loaded),graph_hash(self.store.get('navstudio_doors')))

    def test_actual_nonsequential_way_uids_are_used_in_door_properties_and_roles(self):
        updated=copy.deepcopy(self.native)
        for node,uid in zip(updated['nodes'],[39,90,800]):node['way_guid']=uid
        updated['nodes'][0]['custom_properties']={'editor_note':'Switch A'}
        protection=action_protection(updated);props=protection['action_properties'][10]
        self.assertEqual(props['tce_door_target'],'90');self.assertEqual(props['tce_door_use_nav'],'39')
        self.assertEqual(props['tce_door_entry'],'90');self.assertEqual(props['editor_note'],'Switch A')
        self.assertEqual(protection['action_roles'][10][0]['target_node'],20)

    def test_way_import_uses_current_uid_mapping(self):
        props = self.store.get('navstudio_doors')['nodes'][1]['action_properties']
        self.assertEqual(props['tce_door_target'],'3')
        self.assertEqual(door_role(props,{1:10,2:20,3:30}),dict(kind='door',role='entry',target_node=30,use_node=10,entry_node=20))
        with self.assertRaises(ValueError):
            door_role(props,{1:10,2:20})

    def test_delete_activation_point_clears_entire_recipe_and_undo_restores(self):
        before = copy.deepcopy(self.store.get('navstudio_doors'))
        after = self.change('delete_node',id=10)
        for n in after['nodes']:
            self.assertFalse(any(k.startswith('tce_door_') for k in n['action_properties']))
            self.assertFalse(n['nav_flags']&128)
        restored = self.change('undo')
        self.assertEqual(restored['nodes'],before['nodes'])
        self.assertEqual(restored['edges'],before['edges'])
        self.assertTrue(self.export().read_text().startswith('TCE_NAVMESH 6'))

    def test_unlink_door_crossing_clears_recipe_and_undo_restores(self):
        before = copy.deepcopy(self.store.get('navstudio_doors'))
        doc = self.change('unlink',**{'from':20,'to':30})
        self.assertTrue(all(not n['action_roles'] for n in doc['nodes']))
        restored = self.change('undo')
        self.assertEqual(restored['nodes'],before['nodes'])

    def test_removing_door_flag_invalidates_activation_sequence(self):
        doc = self.change('update_node',id=20,nav_flags=0)
        self.assertTrue(all(not n['action_roles'] for n in doc['nodes']))

    def test_bounded_entity_and_model_fields(self):
        for key,value in [('entity','2048'),('model','0'),('mode','9')]:
            doc = self.store.get('navstudio_doors')
            old = doc['nodes'][1]['action_properties'][f'tce_door_{key}']
            doc['nodes'][1]['action_properties'][f'tce_door_{key}'] = value
            with self.assertRaises(ValueError):self.export()
            doc['nodes'][1]['action_properties'][f'tce_door_{key}'] = old

    def test_native_audit_rejects_stale_use_model_and_way_reference(self):
        native = copy.deepcopy(self.native)
        binary = [dict(flags=n['nav_flags'],properties=copy.deepcopy(d['action_properties']))
                  for n,d in zip(native['nodes'],self.store.get('navstudio_doors')['nodes'])]
        for node in native['nodes']:
            node['door_recipe'].update(validated=True,exported=True)
        for edge in native['edges']:
            edge['reason']='door_open_pose_native_walk_live_pending' if edge['from']==20 and edge['to']==30 else 'external_native_validated_walk'
        by_id={n['id']:n for n in native['nodes']}
        pairs={(by_id[e['from']]['index'],by_id[e['to']]['index']) for e in native['edges']}
        self.assertEqual(len(audit_doors(native,by_id,binary,pairs)),3)
        binary[1]['properties']['tce_door_use_model']='99'
        with self.assertRaisesRegex(ValueError,'mismatch'):
            audit_doors(native,by_id,binary,pairs)
        binary[1]['properties']['tce_door_use_model']='5'
        binary[1]['properties']['tce_door_use_nav']='999'
        with self.assertRaisesRegex(ValueError,'mismatch'):
            audit_doors(native,by_id,binary,pairs)

    def test_audit_does_not_accept_a_recipe_without_native_passage_edge(self):
        native = copy.deepcopy(self.native)
        binary = [dict(flags=n['nav_flags'],properties=copy.deepcopy(d['action_properties']))
                  for n,d in zip(native['nodes'],self.store.get('navstudio_doors')['nodes'])]
        for node in native['nodes']:node['door_recipe'].update(validated=True,exported=True)
        by_id={n['id']:n for n in native['nodes']}
        with self.assertRaisesRegex(ValueError,'edge missing'):
            audit_doors(native,by_id,binary,set())


if __name__ == '__main__':
    unittest.main()
