"""Incomplete native graph identities remain plans, never export/proof waivers."""
import copy
from pathlib import Path
import tempfile
from unittest import TestCase,main
from edit import candidate_recipe_nodes,native_way_uid,action_protection,Drafts


def fixture():
    return dict(generation_complete=False,candidate_mode='automatic_spatial',
        nodes=[dict(id=10,index=0,accepted=True,way_guid=0,import_uid=900,position=[0,0,0],radius=2,nav_flags=1<<27),
               dict(id=20,index=1,accepted=True,way_guid=0,import_uid=0,position=[64,0,32],radius=6),
               dict(id=30,index=-1,accepted=False,way_guid=0,import_uid=2,position=[0,128,0],radius=35)],
        edges=[dict(**{'from':10,'to':20},accepted=True)],
        traversal_routes=[dict(kind='jump',standing_launch=True,exported=True,approach=10,launch=10,landing=20)])


class PartialRecipeUIDTests(TestCase):
    def test_partial_plan_preserves_imports_and_reserves_rejected_sources(self):
        native=fixture();before=copy.deepcopy(native);nodes=candidate_recipe_nodes(native)
        self.assertEqual(native_way_uid(nodes[10]),900)
        self.assertEqual(native_way_uid(nodes[20]),1) # UID2 is reserved, though rejected.
        self.assertEqual(action_protection(native)['action_properties'][10]['tce_stand_jump_target'],'1')
        self.assertEqual(native,before)
        self.assertTrue(all(n['way_guid']==0 for n in native['nodes']))
        self.assertFalse(native['generation_complete'])

    def test_complete_zero_uid_never_falls_back_to_a_plan_or_index(self):
        native=fixture();native['generation_complete']=True
        native['nodes'][1]['_candidate_recipe_uid']=77 # Untrusted/transient marker cannot survive the explicit entry point.
        with self.assertRaises(ValueError):action_protection(native)

    def test_bad_indices_or_duplicate_imports_do_not_create_provisional_references(self):
        mutations=[lambda n:n['nodes'][1].update(index=0),
                   lambda n:n['nodes'][0].update(index=-1),
                   lambda n:n['nodes'][2].update(import_uid=900),
                   lambda n:n['nodes'][2].update(import_uid=2**31),
                   lambda n:n['nodes'][2].update(accepted=1)]
        for mutation in mutations:
            native=fixture();mutation(native)
            with self.assertRaises(ValueError):candidate_recipe_nodes(native)

    def test_no_way_or_provisional_uid_is_persisted_for_new_points(self):
        native=fixture();protection=action_protection(native);navigation=copy.deepcopy(native)
        for node in navigation['nodes']:
            node['action_roles']=protection['action_roles'].get(node['id'],[])
            node['action_properties']=protection['action_properties'].get(node['id'],{})
        environment={'bsp':'source'};base=dict(environment=environment)
        with tempfile.TemporaryDirectory() as folder:
            d=Path(folder);store=Drafts(d/'drafts');doc=store.start('obj_test',navigation,base)
            self.assertEqual(next(n for n in doc['nodes'] if n['id']==10)['way_guid'],900)
            target=next(n for n in doc['nodes'] if n['id']==20)
            self.assertEqual(target['way_guid'],0)
            self.assertNotIn('_candidate_recipe_uid',target)
            store.save('obj_test',0,environment);path=store.export_candidate('obj_test',0,environment,d/'preview')
            # Native target remains a source-ID recipe, not a provisional UID.
            launch=next(line for line in path.read_text().splitlines() if line.startswith('10 '))
            self.assertEqual(launch.split()[10:12],['65','21'])
            self.assertFalse(native['generation_complete'])


if __name__=='__main__':main()
