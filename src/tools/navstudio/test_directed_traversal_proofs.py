"""Directed action proof/UID contract and draft roundtrips; no live mutation."""
import copy
import importlib.util
from pathlib import Path
import unittest
from edit import action_protection

path = Path(__file__).resolve().parents[1] / 'tce_navgen.py'
spec = importlib.util.spec_from_file_location('directed_proof_audit', path)
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def fixture(kind='walk_drop'):
    climb = kind == 'free_climb'
    nodes = [dict(id=6841, index=0, accepted=True, radius=2, nav_flags=512 if climb else 0,
                  position=[0,0,30.25 if climb else 132.25], crouch=False,
                  reason='map_entity_spawn_seed', editor_role=128 if climb else 8,
                  editor_drop_target=1521, native_input=dict(initial_rejected_incident_links=1)),
             dict(id=1521, index=1, accepted=True, radius=6, nav_flags=0,
                  position=[64 if climb else 156,0,132.25 if climb else 30.25], crouch=False,
                  reason='external_native_validated_floor')]
    route = dict(kind=kind, exported=True, approach=6841, launch=6841, landing=1521,
                 wishspeed_samples=[114,160,204], sample_count=3, step_seconds=.008,
                 launch_radius=2, duration_seconds=1.2, touchdown=nodes[1]['position'][:],
                 landing_speed_z=400, crashland_delta=20.48, live_verified=False)
    if climb:
        route.update(model='tce_free_climb_capsule_speed_samples_v1', initial_stamina=2500,
                     initial_impulse_z=337.5, max_ledge=126.25, climb_seconds=.6,
                     stamina_used=750, direction=[1,0,0])
    else:
        route.update(paired_jump_launch=6841, model='tce_ground_then_fall_speed_samples_v3',
                     launch_support_model='stationary_capsule_ground_trace_025',
                     initial_velocity=[0,0,0], impulse_z=0, max_horizontal=160,
                     max_drop=148.4375, gravity=1000, crashland_scale=.64, crashland_limit=38,
                     max_endpoint_horizontal=160, max_airborne_overshoot=48, maxtravel=208,
                     contact_seconds=.5, ground_continuation=0, landing_radius=6,
                     landing_support_samples=9, touchdown_support_model='native_walk_center_support_v2',
                     intermediate_contacts=0, intermediate_contact_limit=8)
    report = dict(candidate_mode='manual_draft', bbox=dict(origin_above_feet=30),
                  traversal_physics=dict(gravity=1000), nodes=nodes,
                  traversal_routes=[route], edges=[{'from':6841,'to':1521,'accepted':True,
                                                   'reason':'manual_native_transition_live_pending'}])
    prefix = 'tce_free_climb' if climb else 'tce_walk_drop'
    binary = [dict(uid=11, properties={prefix:'1',prefix+'_target':'22'}),dict(uid=22, properties={})]
    return report, {n['id']:n for n in nodes}, binary, {(0,1)}


class DirectedTraversalProofTests(unittest.TestCase):
    def test_harmless102_descent_has_exact_directed_impact_receipt_without_reciprocal_jump(self):
        args=fixture()
        value=audit.audit_walk_drops(*args)
        self.assertEqual(value[0]['kind'],'manual_walk_drop')
        self.assertFalse(value[0]['live_verified'])
        # A different rejected incident link does not erase this exact proof.
        self.assertEqual(args[1][6841]['native_input']['initial_rejected_incident_links'],1)
        protection=action_protection(args[0])
        self.assertEqual(protection['action_properties'][6841]['tce_walk_drop_target'],'2')
        self.assertIn((6841,1521),map(tuple,protection['protected_action_edges']))

    def test_descent_rejects_fake_bound_damaging_contact_missing_receipt_and_stale_uid(self):
        for change in ('gravity','damage','target','edge','receipt'):
            with self.subTest(change=change):
                report,by_id,binary,edges=fixture()
                if change=='gravity':report['traversal_routes'][0]['gravity']=800
                elif change=='damage':report['traversal_routes'][0]['crashland_delta']=38.1
                elif change=='target':binary[0]['properties']['tce_walk_drop_target']='1'
                elif change=='edge':edges=set()
                elif change=='receipt':report['traversal_routes']=[]
                with self.assertRaises(ValueError):audit.audit_walk_drops(report,by_id,binary,edges)

    def test_airborne_overshoot_has_checked_ground_return_and_fixed_corridor_limits(self):
        report,by_id,binary,edges=fixture()
        route=report['traversal_routes'][0]
        route['touchdown']=[188,0,30.25];route['ground_continuation']=32
        self.assertEqual(audit.audit_walk_drops(report,by_id,binary,edges)[0]['kind'],'manual_walk_drop')
        route['max_airborne_overshoot']=49
        with self.assertRaises(ValueError):audit.audit_walk_drops(report,by_id,binary,edges)
        route['max_airborne_overshoot']=48
        route['touchdown']=[210,0,30.25];route['ground_continuation']=54
        with self.assertRaises(ValueError):audit.audit_walk_drops(report,by_id,binary,edges)

    def test_freeclimb_receipt_requires_exact_uid_native_action_and_body_scaled_physics(self):
        args=fixture('free_climb')
        self.assertEqual(audit.audit_free_climbs(*args)[0]['kind'],'free_climb')
        protection=action_protection(args[0])
        self.assertEqual(protection['action_properties'][6841]['tce_free_climb_target'],'2')
        self.assertEqual(len([r for r in protection['action_roles'][6841] if r['kind']=='free_climb']),1)
        for change in ('impulse','target','climb','stamina','receipt'):
            report,by_id,binary,edges=copy.deepcopy(args)
            if change=='impulse':report['traversal_routes'][0]['initial_impulse_z']=270
            elif change=='target':binary[0]['properties']['tce_free_climb_target']='1'
            elif change=='climb':by_id[6841]['nav_flags']=0
            elif change=='stamina':report['traversal_routes'][0]['stamina_used']=0
            elif change=='receipt':report['traversal_routes']=[]
            with self.subTest(change=change),self.assertRaises(ValueError):
                audit.audit_free_climbs(report,by_id,binary,edges)


if __name__=='__main__':unittest.main()
