"""Exact source/native/WAY metadata binding and immutable UID reservation."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
from unittest import TestCase, main

sys.path.insert(0,str((Path(__file__).resolve().parent / "workers")))
from tce_navgen import audit_waypoint_metadata


def fixture(directory):
    name='Deck "Ä" \\'
    source='''TCE_NAVMESH 5 obj_test 2 2
10 0 0 0 0 1 0 35 0 0 0 0
20 64 0 0 0 0 0 35 0 0 0 0
10 20
20 10
TCE_NODE_METADATA 1 2
10 5 0 0 0 '''+name.encode().hex()+'''
20 0 0 1 0 746172676574
'''
    candidate=directory/'candidate.txt';candidate.write_text(source,encoding='ascii')
    report=dict(map='obj_test',source='navgen/candidate.txt',
        waypoint_metadata_contract='TCE_NODE_METADATA_1',metadata_input_count=2,
        nodes=[dict(id=10,index=0,accepted=True,metadata_supplied=True,import_uid=5,
                    way_guid=5,name=name,facing=[0,0,0]),
               dict(id=20,index=1,accepted=True,metadata_supplied=True,import_uid=0,
                    way_guid=2,name='target',facing=[0,1,0])])
    binary=[dict(uid=n['way_guid'],name=n['name'],facing=tuple(n['facing'])) for n in report['nodes']]
    return report,binary,candidate


class WaypointMetadataAuditTests(TestCase):
    def test_exact_native_and_source_names_facing_and_uid(self):
        with tempfile.TemporaryDirectory() as folder:
            d=Path(folder);report,binary,candidate=fixture(d)
            result=audit_waypoint_metadata(report,d/'report.json',binary)
            self.assertTrue(result['source_bound']);self.assertEqual(result['input_records'],2)
            self.assertEqual(result['source_sha256'],hashlib.sha256(candidate.read_bytes()).hexdigest())

    def test_report_and_binary_cannot_jointly_override_source_metadata(self):
        with tempfile.TemporaryDirectory() as folder:
            d=Path(folder);report,binary,candidate=fixture(d)
            for field,value in [('name','different'),('facing',[1,0,0]),('uid',6)]:
                with self.subTest(field=field):
                    r,b=copy.deepcopy(report),copy.deepcopy(binary)
                    b[0][field]=value
                    if field=='uid':r['nodes'][0].update(way_guid=value,import_uid=value)
                    else:r['nodes'][0][field]=value
                    with self.assertRaises(ValueError):audit_waypoint_metadata(r,d/'report.json',b)

    def test_unavailable_or_altered_source_fails(self):
        with tempfile.TemporaryDirectory() as folder:
            d=Path(folder);report,binary,candidate=fixture(d);original=candidate.read_text()
            for source in [original.replace('10 5 0 0 0','10 6 0 0 0'),
                           original.replace('20 0 0 1 0','20 0 1 0 0'),
                           original.replace('746172676574','00'),
                           original.replace('TCE_NODE_METADATA 1 2','TCE_NODE_METADATA 1 1'),
                           original+'trailing']:
                with self.subTest(source=source[-40:]):
                    candidate.write_text(source)
                    with self.assertRaises(ValueError):audit_waypoint_metadata(report,d/'report.json',binary)
            candidate.unlink()
            with self.assertRaises(ValueError):audit_waypoint_metadata(report,d/'report.json',binary)

    def test_optional_json_certificate_and_fields_stay_bound(self):
        with tempfile.TemporaryDirectory() as folder:
            d=Path(folder);report,binary,candidate=fixture(d)
            data=dict(candidate_sha256=hashlib.sha256(candidate.read_bytes()).hexdigest(),
                waypoint_metadata_contract='TCE_NODE_METADATA_1',
                nodes=[dict(id=n['id'],way_guid=n['import_uid'],name=n['name'],facing=n['facing']) for n in report['nodes']])
            path=candidate.with_suffix('.json');path.write_text(json.dumps(data))
            self.assertTrue(audit_waypoint_metadata(report,d/'report.json',binary)['source_bound'])
            for mutation in (lambda n:n.update(candidate_sha256='0'*64),
                             lambda n:n['nodes'][0].update(name='old draft'),
                             lambda n:n['nodes'][0].update(way_guid=8)):
                damaged=copy.deepcopy(data);mutation(damaged);path.write_text(json.dumps(damaged))
                with self.assertRaises(ValueError):audit_waypoint_metadata(report,d/'report.json',binary)

    def test_rejected_identity_stays_reserved_for_source_and_future_guides(self):
        with tempfile.TemporaryDirectory() as folder:
            d=Path(folder);report,binary,candidate=fixture(d)
            report['metadata_input_count']=3
            report['nodes'].append(dict(id=30,index=-1,accepted=False,metadata_supplied=True,
                import_uid=2,way_guid=0,name='unreachable',facing=[0,0,0]))
            # UID2 already belongs to the generated second output. Reject
            # its alias even though the owning source was not exported.
            with self.assertRaises(ValueError):audit_waypoint_metadata(report,d/'report.json',binary)

    def test_legacy_reports_remain_readable_without_claiming_source_binding(self):
        self.assertFalse(audit_waypoint_metadata({'nodes':[]},Path('legacy.json'),[])['source_bound'])


if __name__=='__main__':main()
