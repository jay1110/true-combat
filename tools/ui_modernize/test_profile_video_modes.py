"""Both profile workflows use the existing System/native catalog unchanged."""
from pathlib import Path
import re
import unittest
from unittest.mock import patch
import tempfile
import restyle
import semantic as sem
import video_modes as video
from verify_package import compare_semantics
from test_layout_settings import font_width

ROOT=Path(__file__).resolve().parents[2]


class ProfileVideoModes(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original={p:d for p,d in sem.source_files(ROOT/'pak_original_tce').items()
                      if Path(p).suffix in sem.SEMANTIC_SUFFIXES}

    def candidate(self,path):
        result=dict(self.original)
        result[path]=restyle.menu(self.original[path].decode('latin1'),Path(path)).encode('latin1')
        result[sem.PRIVATE_WIDGETS]=self.original[sem.ORIGINAL_WIDGETS]
        return result

    def test_both_profile_artifacts_and_regeneration_use_the_exact_system_catalog(self):
        expected=video.choices()
        self.assertEqual(len(video.MODES),20)
        self.assertEqual([n for _,n in video.MODES],list(range(3,23)))
        system=(ROOT/'pak/ui/options_system.menu').read_bytes().decode('latin1')
        self.assertEqual(video.resolution(system)[3][8],expected)
        self.assertEqual(video.apply(system,'ui/options_system.menu'),system)
        for path in sorted(video.PAGES):
            source=restyle.menu(restyle.read(ROOT/'pak_original_tce'/path),Path(path))
            artifact=(ROOT/'pak'/path).read_bytes().decode('latin1')
            with self.subTest(path=path):
                self.assertEqual(video.resolution(source)[3][8],expected)
                self.assertEqual(video.resolution(artifact)[3][8],expected)
                self.assertEqual(source,artifact)
                self.assertEqual(video.apply(artifact,path),artifact)
                self.assertEqual(compare_semantics(self.original,self.candidate(path)),76)

    def test_existing_native_dimensions_match_every_reviewed_id(self):
        native=(ROOT/'src/ui/tce_video_modes.c').read_text()
        table=re.search(r'resolutions\[\]\[2\]\s*=\s*\{(.*?)\};',native,re.S)[1]
        dimensions=[tuple(map(int,pair)) for pair in re.findall(r'\{\s*(\d+)\s*,\s*(\d+)\s*\}',table)]
        self.assertEqual(len(dimensions),20)
        for label,mode in video.MODES:
            self.assertEqual(tuple(map(int,re.match(r'(\d+)\*(\d+)',label).groups())),dimensions[mode-3])
        self.assertIn('TCE_UI_ApplyVideoMode',native)

    def test_only_resolution_data_changes_and_profile_apply_sequence_survives(self):
        for path in sorted(video.PAGES):
            source=self.original[path].decode('latin1');output=video.apply(source,path)
            a,b,name,args=video.resolution(source);x,y,other,actual=video.resolution(output)
            self.assertEqual(name,other)
            self.assertEqual(args[:8]+args[9:],actual[:8]+actual[9:])
            # Outside the selector's catalog argument, including the entire
            # CREATE validation/apply/restart script, every byte is preserved.
            actual[8]=args[8]
            self.assertEqual(sem.tokens(output[:x]+other+'( '+', '.join(actual)+' )'+output[y:]),sem.tokens(source))
            for command in ('createProfile','vidSave','applyProfile','systemCvarsApply','profileCvarsApply','vid_restart',
                            'setDefaultProfile' if path.endswith('_initial.menu') else 'clearPID'):
                self.assertIn(command,output)

    def test_catalog_exception_rejects_other_values_commands_and_cvars(self):
        path='ui/profile_create.menu'
        for old,new in ((b'"1920*1200 Wide Screen 16:10" 22',b'"1920*1200 Wide Screen 16:10" 23'),
                        (b'uiScript glCustom',b'exec quit'),
                        (b'uiscript systemCvarsApply',b'uiscript attacker'),
                        (b'"ui_r_mode"',b'"r_fullscreen"'),
                        (b'"High" 0',b'"High" 9')):
            candidate=self.candidate(path);self.assertIn(old,candidate[path]);candidate[path]=candidate[path].replace(old,new,1)
            with self.subTest(new=new),self.assertRaises(ValueError):compare_semantics(self.original,candidate)

    def test_existing_font_labels_fit_both_profile_value_columns(self):
        for path in sorted(video.PAGES):
            text=(ROOT/'pak'/path).read_bytes().decode('latin1');args=video.resolution(text)[3]
            available=float(args[2])*.5-10
            self.assertTrue(all(font_width(label,float(args[5]))<=available for label,_ in video.MODES))

    def test_canonical_source_cannot_hide_commands_behind_unchanged_pairs(self):
        source=video.SYSTEM.read_bytes().decode('latin1');a,b,name,args=video.resolution(source)
        args[8]=args[8][:-1]+' exec quit }'
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'system.menu';path.write_bytes((source[:a]+name+'( '+', '.join(args)+' )'+source[b:]).encode('latin1'))
            with patch.object(video,'SYSTEM',path),self.assertRaises(ValueError):video.choices()


if __name__=='__main__':unittest.main()
