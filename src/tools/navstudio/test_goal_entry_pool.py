"""Directed spawn entrances are transit origins, never global goal targets.

These graph fixtures test goal selection only. Their presence does not certify
native spawn provenance, drop physics, a WAY, or installation readiness.
"""
import copy
import unittest

from completion import main_source_ids
from goalgen import (StationarySupport, corridor_routes, distributed_tactics,
                     door_routes, make_plan, playable_nodes, stationary_binding,
                     tactical_graph)


def report():
    points = {i: [i * 128, 0, 30.25] for i in range(6)}
    points.update({10: [-512, 0, 82.25], 11: [-384, 0, 82.25],
                   12: [-256, 0, 82.25], 13: [-128, 0, 30.25]})
    nodes = [dict(id=i, position=p, accepted=True, spawn_reachable=True,
                  nav_flags=0, radius=35, crouch=False) for i, p in points.items()]
    nodes[6]['reason'] = 'map_entity_spawn_seed'
    links = [(a, b) for a in range(6) for b in range(6) if abs(a-b) == 1]
    links += [(10, 11), (11, 10), (11, 12), (12, 13), (13, 0)]
    return dict(generation_complete=True,
                bbox=dict(half_width=16, stand_height=87, crouch_height=61,
                          origin_above_feet=30), nodes=nodes,
                edges=[dict(**{'from': a, 'to': b}, accepted=True) for a, b in links])


def objective(operation='plant_defuse', entry=False):
    x, z = (-512, 82.25) if entry else (256, 30.25)
    feet = z-30
    return dict(tag='mission', entity_id=42, position=[x, 0, z],
                aim_position=[x, 0, z+30], bounds=[[x-16, -16, feet-1], [x+16, 16, feet+64]],
                owner=1, operation=operation, capture_teams=4, stealing_team=2,
                target='target', target_class='trigger_flagonly_multiple')


def door_report():
    positions = [[0, 0, 30], [100, 0, 30], [0, 128, 30], [200, 0, 30],
                 [320, 0, 30], [448, 0, 30], [576, 0, 30]]
    nodes = [dict(id=i, position=p, accepted=True, spawn_reachable=True,
                  nav_flags=0, radius=35, crouch=False) for i, p in enumerate(positions)]
    recipe = dict(entity=96, use_entity=97, mode=2, team=3, model=1, use_model=2,
                  use_source_id=2, entry_source_id=1, aim=[0, 150, 80], validated=True, exported=True)
    for identity, role, target in ((1, 2, 3), (2, 1, 0), (3, 3, -1)):
        nodes[identity].update(nav_flags=128, door_recipe=dict(recipe, role=role, target_source_id=target))
    links = [(0, 1), (1, 0), (0, 2), (2, 0), (1, 3), (3, 4), (4, 3),
             (4, 5), (5, 4), (5, 6), (6, 5)]
    return dict(generation_complete=True, bbox=dict(origin_above_feet=30), nodes=nodes,
                edges=[dict(**{'from': a, 'to': b}, accepted=True) for a, b in links])


class DirectedEntryGoalPoolTests(unittest.TestCase):
    def test_entry_remains_in_transit_graph_but_not_in_goal_or_stationary_pool(self):
        value = report(); main = frozenset(range(6))
        self.assertEqual(main_source_ids(value), main)
        self.assertEqual(set(playable_nodes(value)[0]), main)
        self.assertEqual(set(playable_nodes(value, destination_scope=False)[0]), set(range(6)) | {10, 11, 12, 13})
        transit, forward, _ = tactical_graph(value, 1)
        self.assertIn(10, transit)
        self.assertEqual([i for i, _ in forward[12]], [13])
        binding = stationary_binding('obj_test', value, 'a'*64, {'game_module': 'b'*64})
        self.assertEqual(set(StationarySupport(value, binding).nodes), main)

    def test_entry_never_becomes_camp_attack_defend_or_mission_destination(self):
        for gt in (2, 5):
            with self.subTest(gametype=gt):
                plan = make_plan(report(), [objective()], gt)
                self.assertTrue(plan['mission_geometry_complete'])
                self.assertTrue(plan['goals'])
                self.assertTrue(all(g['source_node'] in range(6) for g in plan['goals']))
                self.assertFalse(plan['mission_behavior_complete'])

    def test_actual_objective_on_entry_platform_is_explicitly_unresolved(self):
        for operation in ('take_flag', 'capture', 'activate_capture', 'plant_defuse'):
            with self.subTest(operation=operation):
                plan = make_plan(report(), [objective(operation, entry=True)], 2)
                self.assertFalse(plan['mission_geometry_complete'])
                self.assertEqual(plan['mission_goal_count'], 0)
                missing = plan['unresolved'][0]
                self.assertEqual(missing['reason'], 'objective_contact_outside_main_component')
                self.assertEqual(missing['entity_id'], 42)
                self.assertEqual(missing['source_node_ids'], [10])
                self.assertTrue(all(g['source_node'] in range(6) for g in plan['goals']))

    def test_ambiguous_equal_components_do_not_select_arbitrary_goal_core(self):
        value = report()
        value['nodes'] = [n for n in value['nodes'] if n['id'] in (0, 1, 10, 11)]
        value['edges'] = [e for e in value['edges'] if e['from'] in (0, 1, 10, 11) and e['to'] in (0, 1, 10, 11)]
        self.assertEqual(main_source_ids(value), frozenset())
        self.assertFalse(playable_nodes(value)[0])
        with self.assertRaisesRegex(ValueError, 'Keine verbundenen'):
            make_plan(value, [objective()], 5)

    def test_all_route_builders_reject_an_entry_only_destination(self):
        goal = dict(name='DEFEND_entry', type='Defend', source_node=10,
                    position=[-512, 0, 82.25], teams=6,
                    tactical_attack_teams=6, tactical_defend_teams=6)
        self.assertFalse(distributed_tactics(report(), [goal], []))
        self.assertFalse(corridor_routes(report(), [goal])[1])
        self.assertFalse(door_routes(report(), [goal])[1])

    def test_real_door_entry_route_stays_usable_with_its_end_in_main(self):
        value = door_report()
        target = dict(name='TCEOBJECTIVE_site', type='TCEOBJECTIVE', source_node=5,
                      position=[448, 0, 30], teams=6)
        definitions, routes, unresolved = door_routes(value, [target])
        self.assertFalse(unresolved)
        self.assertEqual(len(routes), 2)
        by_name = {g['name']: g for g in definitions}
        for route in routes:
            self.assertEqual(by_name[route['start']]['source_node'], 1)
            self.assertNotIn(1, main_source_ids(value))
            self.assertIn(by_name[route['end']]['source_node'], main_source_ids(value))
        unreachable = dict(target, source_node=0, position=[0, 0, 30])
        self.assertFalse(door_routes(value, [unreachable])[1])

    def test_original_report_is_not_annotated_or_pruned_by_goal_selection(self):
        value = report(); before = copy.deepcopy(value)
        make_plan(value, [objective()], 5)
        self.assertEqual(value, before)


if __name__ == '__main__':
    unittest.main()
