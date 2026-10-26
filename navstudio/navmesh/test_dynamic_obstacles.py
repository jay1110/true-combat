"""Moving brush poses are not baked permanently into the static floor mesh."""
from unittest import TestCase,main
from tce_bsp_navmesh import inline_instances


class DynamicObstacleTests(TestCase):
    def setUp(self):
        self.models=[[-100,-100,-10,100,100,100,0,0,0,0],[-10,-10,0,10,10,40,0,0,0,0]]
        self.entities=[dict(classname="worldspawn"),dict(classname="func_rotating",model="*1")]
        self.state=dict(entity=64,modelindex=1,linked=True,contents=1,origin=[0,0,0],angles=[0,90,0],
                        classname="func_rotating",stationary=False)
    def test_rotating_snapshot_is_a_live_obstacle_and_never_a_static_floor(self):
        result=inline_instances(self.models,self.entities,1,dict(models=[self.state]))
        self.assertFalse(result[0]["enabled"])
        self.assertTrue(result[0]["dynamic_obstacle"])
        self.assertEqual(result[0]["reason"],"dynamic_obstacle_requires_current_native_gate")
        self.assertEqual(result[0]["angles"],(0,90,0))
    def test_stationary_closed_door_is_still_a_dynamic_obstacle(self):
        result=inline_instances(self.models,self.entities,1,dict(models=[{**self.state,"classname":"func_door","stationary":True}]))
        self.assertFalse(result[0]["enabled"])
    def test_immutable_wall_and_solid_stationary_crate_remain_geometry(self):
        for classname in ("func_wall","func_explosive"):
            result=inline_instances(self.models,self.entities,1,dict(models=[{**self.state,"classname":classname,"stationary":True}]))
            self.assertTrue(result[0]["enabled"])
    def test_bsp_only_rotor_uses_the_same_dynamic_classification(self):
        result=inline_instances(self.models,self.entities,1,None)
        self.assertFalse(result[0]["enabled"])
        self.assertTrue(result[0]["dynamic_obstacle"])


if __name__=="__main__":main()
