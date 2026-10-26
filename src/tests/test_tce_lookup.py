"""Persistent lookup safety, provenance and cross-source search regressions."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('lookup',Path(__file__).parents[1]/'tools/tce_lookup.py')
lookup=importlib.util.module_from_spec(spec);spec.loader.exec_module(lookup)

class LookupTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
  self.write('src/ui/menu.c','void Browse(void) { const char *s="g_antilag"; Call(); }\nvoid Other(void) { int ui_unrelated=1; }\n')
  self.write('src/tests/browser.c','void check(){ Browse(); /* comparison candidate */ }\n')
  self.write('build/reference/ui.so/abc.c','void Browse(){ use("only_linux_literal"); }')
  self.rows=[dict(id='ui.dll:40001000',address='40001000',program='ui.dll',module='ui',name='Browse',aliases=['FUN_40001000'],status='pending',strings=['literal_100%'],calls=['ui.dll:40002000'],callers=[],sdk_candidates=[dict(symbol='Browse',path='src/ui/menu.c',line=1)],linux_candidates=[dict(path='build/reference/ui.so/abc.c',name='Browse')],byte_hash='good',signature='void Browse()'),dict(id='ui.dll:40002000',address='40002000',program='ui.dll',module='ui',name='Call',status='verified',calls=[],callers=['ui.dll:40001000'])]
  self.write('build/bulk-index.json',json.dumps(dict(functions=self.rows)))
 def tearDown(self):self.temp.cleanup()
 def write(self,path,text):
  p=self.root/path;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text);return p
 def build(self):return lookup.build_index(self.root)
 def find(self,q,**kw):return lookup.find(self.root,query=q,**kw)
 def test_name_id_alias_relationships_and_module(self):
  self.build();r=self.find('Browse')['results'][0];self.assertEqual(r['id'],'ui.dll:40001000');self.assertEqual(r['calls'][0]['name'],'Call')
  self.assertEqual(len(self.find('40001000',field='name')['results']),1)
  self.assertEqual(len(self.find('FUN_40001000')['results']),1)
  self.assertEqual(self.find('Browse',module='cgame')['results'],[])
 def test_source_cvar_scoped_to_actual_body(self):
  self.build();self.assertEqual(len(self.find('g_antilag',field='cvar')['results']),1)
  self.assertEqual(self.find('ui_unrelated')['results'],[])
 def test_literal_wildcards_are_not_sql_wildcards(self):
  self.build();self.assertEqual(len(self.find('literal_100%',field='string')['results']),1)
  self.assertEqual(self.find('literalX100Z')['results'],[])
  self.assertEqual(self.find("' OR 1=1 --")['results'],[])
 def test_registered_unprefixed_cvar_literal(self):
  self.write('src/reconstruction/cvars.json',json.dumps({'modules':{'ui':{'rows':[{'name':'developer'}]}}}))
  self.rows[0]['strings'].append('developer');self.write('build/bulk-index.json',json.dumps(dict(functions=self.rows)))
  self.build();self.assertEqual(len(self.find('developer',field='cvar')['results']),1)
 def test_registered_direct_and_ghidra_cvar_symbols(self):
  self.write('src/reconstruction/cvars.json',json.dumps({'modules':{'ui':{'rows':[{'name':'bot_enable','source_variable':'&bot_enable'},{'name':'rate','source_variable':'&customRate'}]}}}))
  self.write('src/ui/menu.c','void Browse(void){ if(bot_enable.integer) Call(); if(customRate.integer) Call(); }')
  self.write('build/reference/ui.so/abc.c','void Browse(){ if (*(int *)PTR_bot_enable_0006abb4) return; }')
  self.build();self.assertEqual(len(self.find('bot_enable',field='cvar')['results']),1)
  self.assertEqual(len(self.find('rate',field='cvar')['results']),1)
 def test_name_field_includes_labeled_candidates_without_promoting_name(self):
  self.rows[0]['name']='FUN_40001000';self.rows[0]['sdk_candidates'][0]['symbol']='CG_Bullet'
  self.rows[0]['linux_candidates'][0]['name']='CG_Bullet'
  self.rows[0]['aliases']=['oldBulletAlias']
  self.write('build/bulk-index.json',json.dumps(dict(functions=self.rows)));self.build()
  result=self.find('CG_Bullet',field='name')['results'][0]
  self.assertEqual(result['name'],'FUN_40001000');self.assertEqual(result['status'],'pending')
  self.assertEqual({r['kind'] for r in result['name_matches']},{'sdk_candidate','linux_candidate'})
  self.assertEqual(len(self.find('oldBulletAlias',field='name')['results']),1)
 def test_linux_and_test_locations(self):
  self.build();r=self.find('only_linux_literal')['results'][0];self.assertEqual(r['hits'][0]['kind'],'linux')
  self.assertEqual(r['tests'],['src/tests/browser.c'])
  card=lookup.show(self.root,key='ui.dll:40001000');self.assertTrue(any(x['kind']=='test' and x['line']==1 for x in card['function']['locations']))
 def test_stale_source_and_new_test_are_visible(self):
  self.build();self.assertFalse(self.find('Browse')['freshness']['stale'])
  self.write('src/ui/menu.c','changed');self.write('src/tests/new.c','Browse();')
  fresh=self.find('Browse')['freshness'];self.assertTrue(fresh['stale']);self.assertIn('src/tests file inventory changed',fresh['changed'])
 def test_new_sdk_file_requires_refresh(self):
  self.build();self.write('src/ui/new.c','void New(void){}')
  self.assertIn('SDK source inventory changed; refresh bulk audit and lookup',self.find('Browse')['freshness']['changed'])
 def test_stale_cache_skipped(self):
  self.write('build/bulk-cache/ui.dll/40001000.json',json.dumps(dict(status='decompiled',byte_hash='bad',signature='void Browse()',code='poisoned_cache')))
  report=self.build();self.assertTrue(report['warnings']);self.assertEqual(self.find('poisoned_cache')['results'],[])
 def test_good_cache_searchable_and_missing_cache_arrival_invalidates(self):
  self.build();self.write('build/bulk-cache/ui.dll/40001000.json',json.dumps(dict(status='decompiled',byte_hash='good',signature='void Browse()',code='cached_windows_symbol')))
  self.assertTrue(self.find('Browse')['freshness']['stale']);self.build()
  self.assertEqual(len(self.find('cached_windows_symbol',field='windows')['results']),1)
 def test_output_and_input_escape_rejected(self):
  with self.assertRaises(ValueError):lookup.build_index(self.root,database='src/bad.sqlite')
  with self.assertRaises(ValueError):lookup.local_path(self.root,'../outside')
 def test_duplicate_ids_do_not_replace_existing_index(self):
  self.build();before=(self.root/'build/tce-lookup.sqlite').read_bytes();self.rows.append(self.rows[0]);self.write('build/bulk-index.json',json.dumps(dict(functions=self.rows)))
  with self.assertRaises(ValueError):self.build()
  self.assertEqual(before,(self.root/'build/tce-lookup.sqlite').read_bytes())
 def test_function_braces_and_comments(self):
  body=lookup.function_body('void f(){ char *s="}"; /* { */ if(1){x();} }\nvoid g(){y();}', 'f',1)
  self.assertNotIn('y()',body[0]);self.assertIn('x()',body[0]);self.assertIsNone(lookup.function_body('void f(){','f',1))
 def test_status_filter_limits_and_unknown_id(self):
  self.build();self.assertEqual(self.find('Browse',field='name',status='verified')['results'],[])
  with self.assertRaises(ValueError):self.find('')
  with self.assertRaises(ValueError):self.find('Browse',limit=201)
  with self.assertRaises(ValueError):lookup.show(self.root,key='unknown')

if __name__=='__main__':unittest.main()
