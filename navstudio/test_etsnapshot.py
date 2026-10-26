"""Original ET observations never invent a model-id or erase unknown brushes."""
from unittest import TestCase,main
from etsnapshot import join_models


class ETSnapshotTests(TestCase):
    def setUp(self):
        self.models=[[-1024,-1024,-128,1024,1024,512,0,0,0,0],[-20,-10,0,20,10,40,0,0,0,0]]
        self.entities=[dict(classname="worldspawn"),dict(classname="func_explosive",model="*1",origin="100 200 0",scriptname="box")]
        self.state=dict(entity=50,name="box",local_mins=[-20,-10,0],local_maxs=[20,10,40],
                        origin=[100,200,0],angles=[0,0,0],collision_observed=True)

    def test_unique_bounds_and_pose_match_real_engine_id(self):
        result=join_models(self.models,self.entities,dict(engine_entities=[self.state]),1)
        self.assertEqual(result["models"][0]["entity"],50)
        self.assertEqual(result["models"][0]["modelindex"],1)
        self.assertEqual(result["et_model_join"]["matched"],1)
        self.assertFalse(result["et_model_join"]["complete_authoritative_collision"])

    def test_duplicate_or_missing_engine_entity_retains_provisional_collision(self):
        for rows in ([],[self.state,{**self.state,"entity":51}]):
            result=join_models(self.models,self.entities,dict(engine_entities=rows),1)
            self.assertTrue(result["models"][0]["linked"])
            self.assertEqual(result["models"][0]["entity"],-2)
            self.assertTrue(result["models"][0]["native_body_validation_required"])
            self.assertEqual(result["et_model_join"]["provisional"],1)

    def test_an_unrelated_runtime_box_is_not_a_bsp_model_match(self):
        result=join_models(self.models,self.entities,dict(engine_entities=[{**self.state,"name":"other","origin":[500,600,0]}]),1)
        self.assertEqual(result["et_model_join"]["models"][0]["status"],"unmatched")

    def test_a_missed_collision_ray_does_not_prove_the_brush_unlinked(self):
        result=join_models(self.models,self.entities,dict(engine_entities=[{**self.state,"collision_observed":False}]),1)
        self.assertTrue(result["models"][0]["linked"])
        self.assertFalse(result["models"][0]["collision_state_observed"])
        self.assertIn("provisional",result["models"][0]["state_source"])


if __name__=="__main__":main()
