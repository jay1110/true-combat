"""Standing receipts stay bound to authored recipes and serialized WAY edges."""
import copy
from pathlib import Path
import sys
from unittest import TestCase, main

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tce_navgen import audit_standing_jumps


def fixture(manual=True):
    launch=dict(id=10,index=0,position=[0,0,24],radius=2,crouch=False,
                editor_role=65 if manual else 0,editor_drop_target=20,
                native_input=dict(initial_rejected_incident_links=0))
    landing=dict(id=20,index=1,position=[64,0,56],radius=6,crouch=False)
    receipt=dict(kind='jump',standing_launch=True,exported=True,launch=10,landing=20,
        arc_model='low_velocity_forward_jump_capsule_samples_v1',impulse_z=270,
        launch_speed_samples=[36,40,44],wishspeed_samples=[114,160,204],
        launch_offset_samples=[0,-2,2],sample_count=27,step_seconds=.008,
        prime_wishspeed=40,stop_offset=8,launch_radius=2,duration_seconds=.456,
        touchdown=[32,0,56],ground_continuation=32,landing_speed_z=111,
        crashland_delta=2.5,live_verified=False)
    reason='manual_native_transition_live_pending' if manual else 'jump_hull_arc_validated_live_pending'
    report=dict(candidate_mode='manual_draft' if manual else 'spatial_samples',
        bbox=dict(origin_above_feet=24),nodes=[launch,landing],
        edges=[dict(**{'from':10,'to':20},accepted=True,reason=reason)],
        traversal_routes=[receipt])
    binary=[dict(uid=1,properties=dict(tce_stand_jump='1',tce_stand_jump_target='2')),
            dict(uid=2,properties={})]
    return report,binary,{(0,1)}


def audit(report,binary,edges):
    return audit_standing_jumps(report,{n['id']:n for n in report['nodes']},binary,edges)


class StandingJumpAuditTests(TestCase):
    def test_authored_full_receipt_is_verified_once(self):
        report,binary,edges=fixture()
        self.assertEqual(audit(report,binary,edges),[dict(kind='manual_standing_jump',
            launch=10,landing=20,native_samples=27,live_verified=False)])

    def test_generated_receipt_preserves_its_distinct_contract(self):
        report,binary,edges=fixture(False)
        self.assertEqual(audit(report,binary,edges)[0]['kind'],'standing_jump')
        report['edges'][0]['reason']='manual_native_transition_live_pending'
        with self.assertRaises(ValueError):audit(report,binary,edges)

    def test_authoring_metadata_cannot_waive_recipe_or_binary_binding(self):
        mutations=[lambda r,b,e:r.update(candidate_mode='spatial_samples'),
            lambda r,b,e:r['nodes'][0].update(editor_role=1),
            lambda r,b,e:r['nodes'][0].update(editor_drop_target=999),
            lambda r,b,e:r['nodes'][0]['native_input'].update(initial_rejected_incident_links=1),
            lambda r,b,e:r['edges'][0].update(reason='external_native_validated_walk'),
            lambda r,b,e:b[0]['properties'].update(tce_stand_jump_target='999'),
            lambda r,b,e:e.clear(),lambda r,b,e:r['nodes'][1].update(crouch=True)]
        for change in mutations:
            with self.subTest(change=change):
                report,binary,edges=fixture();change(report,binary,edges)
                with self.assertRaises(ValueError):audit(report,binary,edges)

    def test_authored_full_receipt_keeps_every_physics_check(self):
        wrong={'impulse_z':337.5,'sample_count':26,'prime_wishspeed':41,
               'launch_speed_samples':[35,40,44],'wishspeed_samples':[114,160],
               'launch_offset_samples':[0,-2],'step_seconds':.016,'stop_offset':0,
               'launch_radius':35,'duration_seconds':2,'landing_speed_z':float('nan'),
               'crashland_delta':39,'ground_continuation':31,'touchdown':[31,0,56],
               'live_verified':True}
        for key,value in wrong.items():
            with self.subTest(field=key):
                report,binary,edges=fixture();report['traversal_routes'][0][key]=value
                with self.assertRaises(ValueError):audit(report,binary,edges)

    def test_duplicate_receipt_fails(self):
        report,binary,edges=fixture();report['traversal_routes'].append(copy.deepcopy(report['traversal_routes'][0]))
        with self.assertRaises(ValueError):audit(report,binary,edges)


if __name__=='__main__':main()
