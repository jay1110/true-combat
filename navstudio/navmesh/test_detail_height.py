"""Recast detail floors preserve interior terrain height lost by corner fans."""
import json
from pathlib import Path
import tempfile
from unittest import TestCase,main
from waygraph import Mesh


class DetailHeightTests(TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.path=Path(self.temp.name)/"terrain.json"
        self.data=dict(coordinate_system="TCE_Z_up",profile=dict(height=87,radius=16,step=18,cell=4,cell_height=2),
            floor_height_model="recast_compact_field_detail_triangles_v1",
            vertices=[[0,0,0],[128,0,0],[128,128,0],[0,128,0]],
            polygons=[dict(vertices=[0,1,2,3],neighbors=[-1,-1,-1,-1],detail_triangles=[
                [[0,0,0],[128,0,0],[64,64,60]],[[128,0,0],[128,128,0],[64,64,60]],
                [[128,128,0],[0,128,0],[64,64,60]],[[0,128,0],[0,0,0],[64,64,60]]])])
    def mesh(self):
        self.path.write_text(json.dumps(self.data));return Mesh(self.path)
    def test_interior_height_uses_raster_detail_not_polygon_corner_fan(self):
        mesh=self.mesh()
        self.assertAlmostEqual(mesh.height(0,[64,64,0]),60)
        self.assertAlmostEqual(mesh.height(0,[64,32,0]),30)
        self.assertEqual(mesh.centers[0][2],60)
        self.assertEqual(mesh.locate([64,64,60]),0)
        self.assertIsNone(mesh.locate([64,64,0]))
    def test_legacy_planar_mesh_remains_readable(self):
        self.data.pop("floor_height_model");self.data["polygons"][0].pop("detail_triangles")
        self.assertEqual(self.mesh().height(0,[64,64,0]),0)
    def test_declared_detail_model_cannot_silently_fall_back_to_corner_height(self):
        self.data["polygons"][0]["detail_triangles"]=[]
        with self.assertRaisesRegex(ValueError,"detail height data missing"):self.mesh()
    def test_nonfinite_or_malformed_detail_is_rejected(self):
        for bad in ([[0,0,0],[1,0,0]],[[0,0,float("nan")],[1,0,0],[0,1,0]]):
            self.data["polygons"][0]["detail_triangles"]=[bad]
            with self.assertRaisesRegex(ValueError,"invalid Recast detail"):self.mesh()


if __name__=="__main__":main()
