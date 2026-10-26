"""Real WAY decoder imports ledge target UIDs into stable editor source IDs."""
import sys
from pathlib import Path
import struct
from tempfile import TemporaryDirectory
from types import SimpleNamespace
import unittest
from unittest.mock import Mock
from server import Studio
sys.path.insert(0,str(Path(__file__).resolve().parent.parent))
from tce_navgen import HEADER


def packed(value):
    result=bytearray()
    while value>127:
        result.append(value&127);value>>=7
    result.append(value|128)
    return bytes(result)


def string(value):
    raw=value.encode('ascii')
    return packed(len(raw))+raw


def fixture(target):
    data=bytearray(HEADER.pack(9,2,b'fixture',b'ledge import test',0,0,0,100,100,150))
    for i,(uid,source,z) in enumerate(((40,6841,30),(99,6900,130))):
        props={'tce_node':str(source)}
        if i==0:props.update(tce_free_climb='1',tce_free_climb_target=str(target))
        data.append(32|4|1)
        data+=packed(uid)+struct.pack('<3f',i*100,0,z)+packed(512 if i==0 else 0)
        data+=packed(len(props))
        for key,value in props.items():data+=string(key)+string(value)
        data+=packed(1 if i==0 else 0)
        if i==0:data+=packed(2) # next index, forward link only
    return bytes(data)


class FreeClimbImportTests(unittest.TestCase):
    def test_uid_target_maps_to_source_id_and_missing_target_is_rejected(self):
        with TemporaryDirectory() as directory:
            root=Path(directory);way=root/'obj_test.way'
            s=Studio.__new__(Studio);s.active_project={'id':'fixture'};s.game_mode='tce2'
            s.nav_bundles=lambda:SimpleNamespace(path=lambda *args:way)
            s.path=lambda *args:root/'missing-profile.json';s.file_hash=lambda path:'fixture'
            s.recast_body_profile=Mock(side_effect=RuntimeError('no current capture'))
            way.write_bytes(fixture(99))
            nav=s.imported_navigation('obj_test')
            self.assertEqual(nav['nodes'][0]['id'],6841)
            self.assertEqual(nav['nodes'][0]['action_roles'],[dict(kind='free_climb',role='launch',landing_node=6900)])
            self.assertEqual(nav['nodes'][0]['position'],[0,0,0])
            self.assertEqual([(e['from'],e['to']) for e in nav['edges']],[(6841,6900)])
            way.write_bytes(fixture(123))
            with self.assertRaisesRegex(ValueError,'kein gültiges WAY-Landeziel'):
                s.imported_navigation('obj_test')


if __name__=='__main__':unittest.main()
