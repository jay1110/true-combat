"""Offline checks for the local static user guide and its entry points."""
from html.parser import HTMLParser
from pathlib import Path
import unittest

HERE = Path(__file__).resolve().parent


class Document(HTMLParser):
    def __init__(self, value):
        super().__init__()
        self.ids, self.links, self.scripts, self.actions = [], [], [], []
        self.feed(value)

    def handle_starttag(self, tag, attrs):
        attrs = dict(attrs)
        if 'id' in attrs: self.ids.append(attrs['id'])
        if tag == 'a': self.links.append(attrs)
        if tag == 'script': self.scripts.append(attrs)
        if 'data-action' in attrs: self.actions.append(attrs['data-action'])


class GuideTests(unittest.TestCase):
    def test_guide_is_static_local_and_all_contents_links_resolve(self):
        guide = Document((HERE / 'guide.html').read_text(encoding='utf-8'))
        self.assertFalse(guide.scripts)
        self.assertEqual(len(guide.ids), len(set(guide.ids)))
        self.assertGreaterEqual(len(guide.ids), 15)
        for link in guide.links:
            target = link.get('href', '')
            if target.startswith('#'): self.assertIn(target[1:], guide.ids)
            else: self.assertEqual(target, '/')

    def test_header_help_and_combined_generation_keep_manual_steps(self):
        page = Document((HERE / 'index.html').read_text(encoding='utf-8'))
        help_link = next(a for a in page.links if a.get('id') == 'guide-open')
        self.assertEqual(help_link['href'], '/guide.html')
        self.assertEqual(help_link['rel'], 'noopener')
        self.assertEqual(page.actions.count('generate'), 1)
        self.assertEqual(page.actions.count('rebuild'), 1)
        self.assertEqual(page.actions.count('load_map'), 1)
        self.assertNotIn('start_test', page.actions)

    def test_readme_lifecycle_and_limits_are_current(self):
        text = (HERE / 'README.md').read_text(encoding='utf-8')
        for required in ('build/navgen-overkill/TCE2-NavStudio.exe', 'Neues Projekt erstellen',
                         'Netz generieren', 'Laufendes Projekt anzeigen', 'Diagnose / Erweitert',
                         'Live-Abnahme', 'all_maps', 'GT7-Script speichern', 'Save GT7 script',
                         'Goals für GT… ins Spiel übernehmen', 'Apply GT… goals to game',
                         'Nativ geprüft', 'Spawns', '300 Karten'):
            self.assertIn(required, text.replace('\n', ' '))
        for obsolete in ('Die Freigabe stellt keine Tastenbelegungen wieder her',
                         'bei unklarer Antwort wird nicht zwangsbeendet',
                         'Das aktuelle Recast-Profil setzt `g_newbbox 1` voraus',
                         'Die Oberfläche öffnet sich unter', 'Unter der Mapauswahl öffnet'):
            self.assertNotIn(obsolete, text)

    def test_guide_matches_readme_and_both_languages(self):
        from build_guide import render
        self.assertEqual((HERE/'guide.html').read_text(encoding='utf8'),render((HERE/'README.md').read_text(encoding='utf8')))
        guide=(HERE/'guide.html').read_text(encoding='utf8')
        for identity in ('de-deutsch','en-english','de-kamera-und-darstellung','en-camera-and-display',
                         'de-bsp-spawns-und-eigene-gt7-spawns','en-bsp-spawns-and-custom-gt7-spawns'):
            self.assertIn('id="'+identity+'"',guide)


if __name__ == '__main__':
    unittest.main()
