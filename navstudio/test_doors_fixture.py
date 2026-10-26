"""Read the compiled fixture's actual BSP/PK3; no map launch or engine mutation."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
import sys
sys.path.insert(0,str((Path(__file__).resolve().parent / "workers")))
import zipfile

from tce_bsp_navmesh import parse_entities,read_lumps


ROOT=Path(__file__).resolve().parents[1]


def fixture_archive(record,root=ROOT):
    """A live project leases all_maps; the exact compiler output stays readable."""
    candidates=(root/'build/navstudio-doorfixture/obj_navstudio_doors.pk3',
                root/'etmain/all_maps/obj_navstudio_doors.pk3',
                root/'build/navgen-overkill/etmain/obj_navstudio_doors.pk3')
    for path in candidates:
        if path.is_file() and hashlib.sha256(path.read_bytes()).hexdigest()==record['pk3_sha256']:
            return path
    raise FileNotFoundError('No hash-matching compiled obj_navstudio_doors fixture in cache/library/test lease.')


class DoorFixtureTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path=ROOT/'build/navstudio-doorfixture/fixture.json'
        if not path.is_file():raise unittest.SkipTest('Build obj_navstudio_doors first.')
        cls.record=json.loads(path.read_text());cls.archive=fixture_archive(cls.record)
        with zipfile.ZipFile(cls.archive) as archive:
            cls.members=archive.namelist();data=archive.read('maps/obj_navstudio_doors.bsp')
        lumps=read_lumps(data);lump=lumps[0]
        cls.entities=parse_entities(data[lump['offset']:lump['offset']+lump['length']])
        cls.named={e['targetname']:e for e in cls.entities if e.get('targetname')}

    def test_only_one_obj_map_and_no_mod_modules_or_navigation_are_bundled(self):
        self.assertEqual({n for n in self.members if n.endswith('.bsp')},{'maps/obj_navstudio_doors.bsp'})
        self.assertFalse(any(n.endswith(('.dll','.so','.wasm','.way','.wps')) for n in self.members))
        self.assertEqual(hashlib.sha256(self.archive.read_bytes()).hexdigest(),self.record['pk3_sha256'])

    def test_closed_direct_and_remote_rotating_cases_have_explicit_keys(self):
        self.assertEqual(self.named['navstudio_sliding']['key'],'0')
        for name,flag in [('navstudio_rotating','0'),('navstudio_remote','1'),('navstudio_locked','0')]:
            entity=self.named[name]
            self.assertEqual(entity['classname'],'func_door_rotating');self.assertEqual(entity['angles'],'0 0 0')
            self.assertEqual(entity['degrees'],'90');self.assertEqual(entity['spawnflags'],flag)
            self.assertEqual(entity['wait'],'4');self.assertIn('origin',entity)
        self.assertEqual(self.named['navstudio_remote']['key'],'0')
        self.assertEqual(self.named['navstudio_locked']['key'],'-1')

    def test_explicit_use_buttons_share_one_unrestricted_relay_binding(self):
        buttons=[e for e in self.entities if e.get('classname')=='func_button']
        self.assertEqual({e['targetname'] for e in buttons},{'navstudio_button','navstudio_button_near_left','navstudio_button_near_right'})
        for button in buttons:
            self.assertEqual(button['target'],'navstudio_relay')
            self.assertEqual(int(button.get('spawnflags','0'))&8,0,'Button must require explicit USE, never automatic TOUCH.')
            self.assertEqual(button['speed'],'40');self.assertEqual(button['wait'],'1')
        relay=self.named['navstudio_relay']
        self.assertEqual(relay['classname'],'target_relay');self.assertEqual(relay['target'],'navstudio_remote')
        self.assertEqual(relay['key'],'0');self.assertEqual(relay['spawnflags'],'0')

    def test_existing_distant_button_doors_and_entity_ids_are_unchanged(self):
        old_path=Path(__file__).with_name('fixtures')/'door-baseline-a13.json'
        previous=json.loads(old_path.read_text())
        current={e['properties'].get('targetname'):e for e in self.record['entities'] if e['properties'].get('targetname')}
        for old in previous['entities']:
            name=old['properties'].get('targetname')
            if name:
                self.assertEqual(current[name]['bsp_entity'],old['bsp_entity'])
                self.assertEqual(current[name]['properties'],old['properties'])
        self.assertEqual(current['navstudio_button']['world_center'],[1034.0,264.0,56.0])
        self.assertEqual(current['navstudio_button_near_left']['world_center'],[1120.0,122.0,56.0])
        self.assertEqual(current['navstudio_button_near_right']['world_center'],[1248.0,122.0,56.0])

    def test_spawns_are_physical_left_side_and_no_script_unlocks_exist(self):
        spawns=[e for e in self.entities if e.get('classname')=='info_player_deathmatch']
        self.assertEqual(len(spawns),8)
        for spawn in spawns:
            x,y,z=map(float,spawn['origin'].split());self.assertLessEqual(x,224);self.assertLessEqual(abs(y),64);self.assertEqual(z,32)
        with zipfile.ZipFile(self.archive) as archive:script=archive.read('maps/obj_navstudio_doors_gt7.script').decode('ascii')
        self.assertIn('tce_dmspawns 1',script)
        for name in ('navstudio_sliding','navstudio_rotating','navstudio_remote','navstudio_locked'):
            self.assertNotIn(name,script)
        self.assertFalse((ROOT/'build/navstudio-doorfixture/etmain/maps/obj_navstudio_doors.lin').exists())


class FixtureArchiveLocationTests(unittest.TestCase):
    def test_leased_library_uses_unchanged_build_cache(self):
        with tempfile.TemporaryDirectory(prefix='navstudio-fixture-location-') as directory:
            root=Path(directory);cache=root/'build/navstudio-doorfixture/obj_navstudio_doors.pk3'
            cache.parent.mkdir(parents=True);data=b'isolated location fixture, never executed';cache.write_bytes(data)
            self.assertFalse((root/'etmain/all_maps/obj_navstudio_doors.pk3').exists())
            self.assertEqual(fixture_archive({'pk3_sha256':hashlib.sha256(data).hexdigest()},root),cache)

    def test_changed_cache_never_replaces_exact_matching_lease(self):
        with tempfile.TemporaryDirectory(prefix='navstudio-fixture-location-') as directory:
            root=Path(directory);cache=root/'build/navstudio-doorfixture/obj_navstudio_doors.pk3'
            lease=root/'build/navgen-overkill/etmain/obj_navstudio_doors.pk3'
            cache.parent.mkdir(parents=True);lease.parent.mkdir(parents=True)
            cache.write_bytes(b'changed');data=b'exact owned lease';lease.write_bytes(data)
            record={'pk3_sha256':hashlib.sha256(data).hexdigest()}
            self.assertEqual(fixture_archive(record,root),lease)
            lease.write_bytes(b'also changed')
            with self.assertRaises(FileNotFoundError):fixture_archive(record,root)


if __name__=='__main__':unittest.main()
