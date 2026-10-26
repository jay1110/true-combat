"""Opaque properties remain bound to source, current report and WAY bytes."""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
from unittest import TestCase,main
sys.path.insert(0,str((Path(__file__).resolve().parent / "workers")))
from tce_navgen import audit_custom_properties


def fixture(d):
    values={10:{'paththrough':'door_route','paththroughparam':'A "Ä" \\ B'},20:{'empty':''}}
    source='''TCE_NAVMESH 5 obj_test 2 2
10 0 0 0 0 1 0 35 0 0 0 0
20 64 0 0 0 0 0 35 0 0 0 0
10 20
20 10
TCE_NODE_PROPERTIES 1 3
'''
    source+=''.join('%d %s %s\n'%(identity,key.encode().hex(),value.encode().hex() or '-')
        for identity,props in values.items() for key,value in props.items())
    path=d/'candidate.txt';path.write_text(source,encoding='ascii')
    report=dict(map='obj_test',source='navgen/candidate.txt',custom_property_contract='TCE_NODE_PROPERTIES_1',
        custom_property_input_count=3,nodes=[dict(id=identity,index=i,accepted=True,custom_properties=props)
        for i,(identity,props) in enumerate(values.items())])
    binary=[dict(properties=dict(tce_node=str(n['id']),**n['custom_properties'])) for n in report['nodes']]
    return report,binary,path


class WaypointPropertiesAuditTests(TestCase):
    def test_exact_literal_properties_and_paththrough_roundtrip(self):
        with tempfile.TemporaryDirectory() as folder:
            d=Path(folder);report,binary,source=fixture(d)
            data=audit_custom_properties(report,d/'report.json',binary)
            self.assertTrue(data['source_bound']);self.assertEqual(data['input_pairs'],3)

    def test_joint_native_binary_changes_do_not_replace_authored_source(self):
        with tempfile.TemporaryDirectory() as folder:
            d=Path(folder);report,binary,source=fixture(d)
            report['nodes'][0]['custom_properties']['paththrough']='different'
            binary[0]['properties']['paththrough']='different'
            with self.assertRaises(ValueError):audit_custom_properties(report,d/'report.json',binary)

    def test_custom_guide_inheritance_and_reserved_namespace_are_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            d=Path(folder);report,binary,source=fixture(d)
            for key in ('inherited','tce_unknown'):
                damaged=copy.deepcopy(binary);damaged[1]['properties'][key]='spoof'
                with self.assertRaises(ValueError):audit_custom_properties(report,d/'report.json',damaged)
            binary[0]['properties']['tce_node']='999'
            with self.assertRaises(ValueError):audit_custom_properties(report,d/'report.json',binary)

    def test_duplicate_or_corrupt_source_and_missing_source_fail(self):
        with tempfile.TemporaryDirectory() as folder:
            d=Path(folder);report,binary,source=fixture(d);original=source.read_text()
            last=original.splitlines()[-1]
            for damaged in (original.replace(last,original.splitlines()[-3]),
                            original.replace('646f6f725f726f757465','00'),original+'trailing',
                            original.replace('TCE_NODE_PROPERTIES 1 3','TCE_NODE_PROPERTIES 1 4')):
                source.write_text(damaged)
                with self.assertRaises(ValueError):audit_custom_properties(report,d/'report.json',binary)
            source.unlink()
            with self.assertRaises(ValueError):audit_custom_properties(report,d/'report.json',binary)

    def test_optional_json_certificate_and_custom_fields_must_match(self):
        with tempfile.TemporaryDirectory() as folder:
            d=Path(folder);report,binary,source=fixture(d)
            data=dict(candidate_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                custom_property_contract='TCE_NODE_PROPERTIES_1',
                nodes=[dict(id=n['id'],action_properties=n['custom_properties']) for n in report['nodes']])
            path=source.with_suffix('.json');path.write_text(json.dumps(data))
            self.assertTrue(audit_custom_properties(report,d/'report.json',binary)['source_bound'])
            data['nodes'][0]['action_properties']={'paththrough':'old'};path.write_text(json.dumps(data))
            with self.assertRaises(ValueError):audit_custom_properties(report,d/'report.json',binary)

    def test_legacy_contract_does_not_claim_property_preservation(self):
        self.assertFalse(audit_custom_properties({},Path('legacy.json'),[])['source_bound'])


if __name__=='__main__':main()
