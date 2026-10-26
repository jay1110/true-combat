"""Identity checks using the exact colour/slot mismatch seen in GT5 telemetry."""
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str((Path(__file__).resolve().parent / "workers")))
from tce_nav_livecheck import clean, positions, registered_players


class LiveIdentityTests(unittest.TestCase):
    def test_native_registry_and_navprobe_names_match(self):
        reply = ('NAVSTATE 0 GT 5\n'
                 'NAVPOS 0 1 2 3 100 3 NavStudioObserver\n'
                 'NAVPOS 1 1 2 3 91 1 ^0Bean\n'
                 'NAVPOS 3 1 2 3 100 1 ^rRazor\n'
                 'NAVPOS 12 1 2 3 95 2 ^1Korv\n')
        rows = positions(reply)
        bots = registered_players(rows, {1: '^0Bean', 3: '^rRazor', 12: '^1Korv'})
        self.assertEqual([r['name'] for r in bots], ['Bean', 'Razor', 'Korv'])
        self.assertEqual(len(bots), 3)

    def test_replacement_player_at_registered_slot_is_rejected(self):
        with self.assertRaisesRegex(RuntimeError, 'Unregistered'):
            registered_players([dict(slot=1, name='Human', team=1)], {1: '^0Bean'})

    def test_unregistered_slot_is_rejected_even_with_a_bot_name(self):
        with self.assertRaisesRegex(RuntimeError, 'Unregistered'):
            registered_players([dict(slot=2, name='Bean', team=1)], {1: '^0Bean'})

    def test_spectator_name_does_not_exempt_a_team_player(self):
        with self.assertRaisesRegex(RuntimeError, 'Unregistered'):
            registered_players([dict(slot=0, name='NavStudioObserver', team=1)], {})

    def test_extended_et_colours_and_newlines(self):
        self.assertEqual(clean('^aSean\n^+Morgriff\n^#Garbok'), 'Sean\nMorgriff\nGarbok')


if __name__ == '__main__':
    unittest.main()
