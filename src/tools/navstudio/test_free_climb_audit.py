"""Receipt-contract checks; physical lip proof lives in the native C test."""
from copy import deepcopy
from pathlib import Path
import sys
from unittest import TestCase, main

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tce_navgen import audit_free_climbs


def fixture(rise=60, new_contract=True):
    launch = dict(id=101, index=0, position=[0, 0, 30.125], radius=2, nav_flags=512, crouch=False)
    landing = dict(id=102, index=1, position=[64, 0, 30.125+rise], crouch=False)
    route = dict(kind='free_climb', approach=101, launch=101, landing=102, exported=True,
                 model='tce_free_climb_capsule_speed_samples_v1', wishspeed_samples=[114, 160, 204],
                 sample_count=3, step_seconds=.008, initial_stamina=2500, initial_impulse_z=337.5,
                 max_ledge=126.25, launch_radius=2, live_verified=False, duration_seconds=.664,
                 climb_seconds=.4, stamina_used=500, landing_speed_z=173, crashland_delta=3.8,
                 touchdown=[49, 0, 30.125+rise], direction=[1, 0, 0])
    if new_contract:
        route.update(endpoint_rule='measured_lip_and_landing_floor_v1', contact_min_ledge=63,
                     minimum_endpoint_rise=18.25)
    report = dict(bbox={'origin_above_feet': 30}, traversal_routes=[route],
                  edges=[dict(accepted=True, **{'from': 101, 'to': 102},
                              reason='free_climb_hull_samples_validated_live_pending')])
    binary = [dict(uid=501, properties={'tce_free_climb': '1', 'tce_free_climb_target': '502'}),
              dict(uid=502, properties={})]
    return report, {101: launch, 102: landing}, binary, {(0, 1)}


class FreeClimbAuditTests(TestCase):
    def test_sixty_floor_has_explicit_unchanged_measured_lip_contract(self):
        self.assertEqual(len(audit_free_climbs(*fixture())), 1)

    def test_old_receipt_cannot_claim_new_sub63_endpoint(self):
        with self.assertRaisesRegex(ValueError, 'landing-floor range'):
            audit_free_climbs(*fixture(new_contract=False))
        self.assertEqual(len(audit_free_climbs(*fixture(rise=102, new_contract=False))), 1)

    def test_measured_lip_gate_cannot_be_lowered_in_receipt(self):
        args = fixture()
        args[0]['traversal_routes'][0]['contact_min_ledge'] = 60
        with self.assertRaisesRegex(ValueError, 'native lip'):
            audit_free_climbs(*args)

    def test_native_physics_samples_and_step_range_remain_required(self):
        with self.assertRaisesRegex(ValueError, 'landing-floor range'):
            audit_free_climbs(*fixture(rise=18.25))
        for key, value in (('sample_count', 1), ('max_ledge', 130), ('initial_impulse_z', 350)):
            with self.subTest(key=key):
                args = fixture()
                args[0]['traversal_routes'][0][key] = value
                with self.assertRaisesRegex(ValueError, 'physics/action contract'):
                    audit_free_climbs(*args)


if __name__ == '__main__':
    main()
