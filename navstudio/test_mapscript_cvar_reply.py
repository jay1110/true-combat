"""Classic ET console presentation must not corrupt actual VFS directories."""
import json
import os
from pathlib import Path
import re
import shutil
import tempfile
from types import SimpleNamespace
import unittest

from mapscript import availability,cvar_value,read


NAME='g_mapScriptDirectory'


class CvarReplyTests(unittest.TestCase):
    def test_observed_classic_et_reset_inside_value_quotes(self):
        reply='"g_mapScriptDirectory" is:"mapscripts^7" default:"^7"\n'
        self.assertEqual(cvar_value(reply,NAME),'mapscripts')

    def test_native_colors_in_name_value_and_labels_not_just_final_reset(self):
        reply='print\n^3"g_map^2ScriptDirectory^7" ^1is:^7"^6My^xScripts/Sub^7" ^5default:^7"^2maps^7"\r\n'
        self.assertEqual(cvar_value(reply,NAME),'MyScripts/Sub')

    def test_actual_empty_value_does_not_become_default_or_latched_directory(self):
        reply='"g_mapScriptDirectory" is:"^7" default:"maps^7"\nlatched: "future^7"\n'
        self.assertEqual(cvar_value(reply,NAME),'')
        self.assertEqual(cvar_value('"g_newbbox" is:"1^7" default:"0^7"','g_newbbox'),'1')

    def test_value_preserves_path_case_slashes_spaces_and_unicode_without_normalization(self):
        for value in('My Maps/Sub_Folder//../',' custom/ ','C:\\Custom\\','Grüße/日本語'):
            with self.subTest(value=value):
                self.assertEqual(cvar_value('"'+NAME+'" is:"'+value+'^7"',NAME),value)

    def test_caret_escape_and_dangling_caret_match_q_is_color_string(self):
        for value,expected in(('part^^7','part^'),('part^^','part^^'),('part^','part^'),
                               ('a^^b','a^'),('^^^7','^^')):
            with self.subTest(value=value):
                self.assertEqual(cvar_value('"'+NAME+'" is:"'+value+'^7"',NAME),expected)

    def test_exact_name_case_insensitive_but_never_other_or_substring_cvar(self):
        self.assertEqual(cvar_value('"G_MAPSCRIPTDIRECTORY" is:"mapscripts^7"',NAME),'mapscripts')
        for other in('g_mapScriptDirectory2','prefix_g_mapScriptDirectory','g_gametype'):
            with self.subTest(other=other),self.assertRaises(RuntimeError):
                cvar_value('"'+other+'" is:"mapscripts^7"',NAME)

    def test_duplicate_identical_conflicting_or_other_cvar_reply_is_ambiguous(self):
        good='"'+NAME+'" is:"mapscripts^7"'
        for extra in(good,'"'+NAME+'" is:"other^7"','"g_newbbox" is:"1^7"'):
            with self.subTest(extra=extra),self.assertRaises(RuntimeError):
                cvar_value(good+'\n'+extra,NAME)

    def test_unquoted_missing_quote_extra_field_or_malformed_second_reply_fails(self):
        good='"'+NAME+'" is:"mapscripts^7"'
        cases=[NAME+' is: mapscripts',good[:-1],good+' trailing',
               good+' default:"unterminated',good+'\n"'+NAME+'" is:"unterminated',
               '"'+NAME+'" is:"map"scripts"','"'+NAME+'" is:"line\nbreak"']
        for reply in cases:
            with self.subTest(reply=reply),self.assertRaises(RuntimeError):
                cvar_value(reply,NAME)

    def test_non_cvar_noise_is_allowed_but_is_never_a_value_source(self):
        reply='server log: map loaded\nprint\n"'+NAME+'" is:"mapscripts^7"\nordinary server log'
        self.assertEqual(cvar_value(reply,NAME),'mapscripts')
        with self.assertRaises(RuntimeError):cvar_value('server log: mapscripts',NAME)

    def test_invalid_types_control_bytes_and_query_names_fail_with_runtime_error(self):
        good='"'+NAME+'" is:"mapscripts^7"'
        for reply,name in((None,NAME),(b'bytes',NAME),(good,None),(good,[]),(good,'x; quit'),
                           (good+'\x00',NAME),(good+'\x0b',NAME),(good,NAME+'\n')):
            with self.subTest(reply=reply,name=name),self.assertRaises(RuntimeError):cvar_value(reply,name)

    def test_real_custom_override_is_selected_after_native_colored_reply_decode(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'maps').mkdir();(root/'MapScripts').mkdir()
            (root/'maps/obj_test_gt5.script').write_bytes(b'PK3-style maps fallback')
            custom=root/'MapScripts/Obj_Test_GT5.script';custom.write_bytes(b'actual custom override')
            assets=SimpleNamespace(roots=[root])
            directory=cvar_value('"'+NAME+'" is:"mapscripts^7" default:"^7"',NAME)
            value=availability(assets,'obj_test',directory)
            self.assertEqual(value['available'],[5])
            self.assertEqual(value['gametypes']['5']['resolved']['selected']['relative'],'mapscripts/obj_test_gt5.script')
            self.assertEqual(read(assets,'obj_test',gametype=5,directory=directory)['text'],'actual custom override')

    def test_actual_custom_generic_after_color_decode_never_proves_fallback_gt(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);(root/'maps').mkdir();(root/'mapscripts').mkdir()
            for gt in(2,5):(root/'maps'/('obj_test_gt'+str(gt)+'.script')).write_bytes(b'original exact GT')
            (root/'mapscripts/obj_test.script').write_bytes(b'custom generic supersedes both exact maps scripts')
            assets=SimpleNamespace(roots=[root]);directory=cvar_value('"'+NAME+'" is:"mapscripts^7"',NAME)
            value=availability(assets,'obj_test',directory)
            self.assertEqual(value['available'],[])
            self.assertTrue(all(row['resolved']['selected']['relative']=='mapscripts/obj_test.script' for row in value['gametypes'].values()))


@unittest.skipUnless(os.name=='nt' and shutil.which('cmake'),'Existing Windows CMake/MSVC required')
class NativeQ3ColorContractTests(unittest.TestCase):
    def test_parser_colors_match_actual_sdk_q_cleanstr_and_q_is_color_string(self):
        from test_door_runtime_native import body,compile_run
        root=(Path(__file__).resolve().parents[1] / "src")
        header=(root/'game/q_shared.h').read_text()
        source=(root/'game/q_shared.c').read_text()
        color=re.search(r'^#define Q_IsColorString\(p\)[^\r\n]*',header,re.M).group()
        function=body(source,'char *Q_CleanStr(')
        values=['part^'+chr(c)+'Tail' for c in range(33,127) if c!=94]
        values.extend(['part^^7','part^^','part^','a^^b','^^^7','plain/Directory'])
        checks=[]
        for value in values:
            parsed=cvar_value('"'+NAME+'" is:"'+value+'^7"',NAME)
            # Include the engine's real trailing white reset in the same
            # buffer: doubled carets must follow its sequential scan exactly.
            checks.append('{char value[]='+json.dumps(value+'^7')+';assert(!strcmp(Q_CleanStr(value),'+json.dumps(parsed)+'));}')
        program='#ifdef NDEBUG\n#undef NDEBUG\n#endif\n#include <cassert>\n#include <cstring>\n'
        program+="#define Q_COLOR_ESCAPE '^'\n"+color+'\n'+function+'\nint main(){'+''.join(checks)+'return 0;}\n'
        compile_run(self,program)


if __name__=='__main__':unittest.main()
