"""The requested new Appearance page cannot relax old menu behavior gates."""
from pathlib import Path
import unittest

import restyle
import semantic as sem
from verify_package import compare_semantics

ROOT=Path(__file__).resolve().parents[2]


class AppearanceContract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original={p:d for p,d in sem.source_files(ROOT/'pak').items() if Path(p).suffix in sem.SEMANTIC_SUFFIXES}
        cls.page=(ROOT/'ui-modern'/sem.APPEARANCE).read_bytes()

    def candidate(self):
        result=dict(self.original);result[sem.APPEARANCE]=self.page
        for path in sem.APPEARANCE_LISTS:
            result[path]+=b'\nloadMenu { "ui/modern_appearance.menu" }\n'
        path='ui/options.menu'
        result[path]=restyle.menu(restyle.read(ROOT/'pak'/path),Path(path)).encode('latin1')
        result[sem.PRIVATE_WIDGETS]=self.original[sem.ORIGINAL_WIDGETS]
        return result

    def test_reviewed_page_full_lists_and_exact_options_link_are_accepted(self):
        self.assertEqual(sem.appearance_signature(self.page,self.original),sem.APPEARANCE_SEMANTIC_SHA256)
        self.assertEqual(compare_semantics(self.original,self.candidate()),77)

    def test_appearance_changed_scripts_cvars_limits_list_values_and_return_are_rejected(self):
        for old,new in ((b'"ui_themePreset"',b'"r_fullscreen"'),
                        (b'"Custom" 4',b'"Custom" 9'),
                        (b'"ui_themeOpacity" .85 .25 1',b'"ui_themeOpacity" .85 0 1'),
                        (b'setcvar ui_themePreset 0',b'exec quit ; setcvar ui_themePreset 0'),
                        (b'close modern_appearance ; open options ;',b'close modern_appearance ; open main ;')):
            with self.subTest(new=new):
                candidate=self.candidate();self.assertIn(old,candidate[sem.APPEARANCE])
                candidate[sem.APPEARANCE]=candidate[sem.APPEARANCE].replace(old,new,1)
                with self.assertRaisesRegex(ValueError,'Appearance page differs'):
                    compare_semantics(self.original,candidate)

    def test_paint_can_change_without_weakening_appearance_behavior_contract(self):
        candidate=self.candidate()
        candidate[sem.APPEARANCE]=candidate[sem.APPEARANCE].replace(b'rect 0 20 852 424',b'rect 10 30 830 410')
        self.assertEqual(compare_semantics(self.original,candidate),77)

    def test_slider_labels_align_to_native_fixed_track_without_moving_multi_labels(self):
        text=self.page.decode('latin1')
        sliders=list(restyle.macro_calls(text,{'SLIDER'}))
        multis=list(restyle.macro_calls(text,{'MULTI'}))
        self.assertEqual(len(sliders),9);self.assertEqual(len(multis),2)
        # Native Item_Slider_Paint keeps its fixed track near rowY+1. Use the
        # same14px label baseline as the working System settings sliders.
        self.assertEqual({args[6] for _,_,_,args in sliders},{'14'})
        self.assertEqual({args[6] for _,_,_,args in multis},{'21'})
        old=self.page.replace(b'.23, 14,',b'.23, 21,')
        # Baseline correction is paint-only: original scripts/ranges stay exact.
        self.assertEqual(sem.appearance_signature(old,self.original),sem.APPEARANCE_SEMANTIC_SHA256)

    def test_missing_duplicate_or_misdirected_appearance_navigation_is_rejected(self):
        for bad in ('missing_load','duplicate_load','missing_link','duplicate_link','wrong_link','extra_action'):
            candidate=self.candidate()
            if bad=='missing_load':candidate[sem.APPEARANCE_LISTS[0]]=self.original[sem.APPEARANCE_LISTS[0]]
            elif bad=='duplicate_load':candidate[sem.APPEARANCE_LISTS[0]]+=b'loadMenu { "ui/modern_appearance.menu" }\n'
            elif bad=='missing_link':candidate['ui/options.menu']=self.original['ui/options.menu']
            elif bad=='duplicate_link':candidate['ui/options.menu']+=b'\nitemDef { name "modern_appearance_link" }'
            elif bad=='wrong_link':candidate['ui/options.menu']=candidate['ui/options.menu'].replace(b'open modern_appearance ;',b'open attacker ;')
            else:candidate['ui/options.menu']=candidate['ui/options.menu'].replace(b'open modern_appearance ;',b'open modern_appearance ; exec quit ;')
            with self.subTest(bad=bad),self.assertRaises(ValueError):compare_semantics(self.original,candidate)

    def test_existing_functionality_cannot_hide_behind_new_appearance_exception(self):
        candidate=self.candidate()
        candidate['ui/options.menu']=candidate['ui/options.menu'].replace(b'open options_controls',b'open attacker',1)
        with self.assertRaisesRegex(ValueError,'semantic data changed'):
            compare_semantics(self.original,candidate)


if __name__=='__main__':unittest.main()
