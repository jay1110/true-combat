"""Exact WAY target binding for newly sampled standing jumps; no game."""
import copy
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"tools"))
from tce_navgen import audit_standing_jumps,audit_water_policy

class StandingAudit(unittest.TestCase):
 def setUp(self):
  self.by={1:dict(id=1,index=0,position=[0,0,30],radius=2),2:dict(id=2,index=1,position=[60,0,62],radius=8)}
  self.way=[dict(uid=1,properties={"tce_stand_jump":"1","tce_stand_jump_target":"2"}),dict(uid=2,properties={})]
  self.r=dict(bbox={"origin_above_feet":30},edges=[dict(**{"from":1,"to":2},accepted=True,reason="jump_hull_arc_validated_live_pending")],
   traversal_routes=[dict(kind="jump",standing_launch=True,exported=True,launch=1,landing=2,
    arc_model="low_velocity_forward_jump_capsule_samples_v1",impulse_z=337.5,launch_speed_samples=[36,40,44],wishspeed_samples=[114,160,204],launch_offset_samples=[0,-2,2],
    sample_count=27,step_seconds=.008,prime_wishspeed=40,stop_offset=8,launch_radius=2,live_verified=False,
    duration_seconds=.56,touchdown=[35,0,62],ground_continuation=25,landing_speed_z=222,crashland_delta=6)])
 def test_roundtrip_requires_full_recipe_and_exact_current_uid(self):
  self.assertEqual(audit_standing_jumps(self.r,self.by,self.way,{(0,1)})[0]["native_samples"],27)
  self.way[0]["properties"]["tce_stand_jump_target"]="999"
  with self.assertRaises(ValueError):audit_standing_jumps(self.r,self.by,self.way,{(0,1)})
 def test_no_property_only_or_increased_running_impulse(self):
  for key,value in (("arc_model","running_jump_hull_arc_v1"),("sample_count",1),("prime_wishspeed",200),("live_verified",True),("crashland_delta",39)):
   r=copy.deepcopy(self.r);r["traversal_routes"][0][key]=value
   with self.assertRaises(ValueError):audit_standing_jumps(r,self.by,self.way,{(0,1)})
  r=copy.deepcopy(self.r);r["traversal_routes"]=[]
  with self.assertRaises(ValueError):audit_standing_jumps(r,self.by,self.way,{(0,1)})
 def test_current_manual_proof_and_source_id_to_way_uid(self):
  r=dict(candidate_mode="manual_draft",traversal_routes=[],edges=[dict(**{"from":1,"to":2},accepted=True,reason="manual_native_transition_live_pending")])
  by=copy.deepcopy(self.by);by[1].update(editor_role=64,editor_drop_target=2,native_input={"initial_rejected_incident_links":0})
  self.assertEqual(audit_standing_jumps(r,by,self.way,{(0,1)})[0]["kind"],"manual_standing_jump")
  by[1]["native_input"]["initial_rejected_incident_links"]=1
  with self.assertRaises(ValueError):audit_standing_jumps(r,by,self.way,{(0,1)})
class WaterPolicyAudit(unittest.TestCase):
 def test_current_policy_keeps_posture_and_body_profile(self):
  for origin,standing,crouching in ((30,41,28),(24,33,22)):
   r=dict(bbox={"origin_above_feet":origin},water_policy=dict(model="grounded_pm_middle_sample_wading_v2",foot_hazard_probe_height=0,
    pm_first_sample_height=1,standing_middle_sample_height=standing,crouching_middle_sample_height=crouching,
    integer_sample_truncation=True,wet_middle_sample_rejected=True,swimming_supported=False,lava_slime_hurt_rejected=True))
   self.assertTrue(audit_water_policy(r)["native_policy_evidence_available"])
   r["water_policy"]["standing_middle_sample_height"]=18
   with self.assertRaises(ValueError):audit_water_policy(r)
 def test_old_conservative_policy_is_explicitly_historical(self):
  self.assertFalse(audit_water_policy({})["native_policy_evidence_available"])
  r=dict(water_policy=dict(model="grounded_step_depth_wading_v1",dry_probe_above_feet=18,swimming_supported=False))
  self.assertTrue(audit_water_policy(r)["legacy_conservative_policy"])
if __name__=="__main__":unittest.main()
