import unittest
from waygraph import navigation_spawns

class SpawnSeedTests(unittest.TestCase):
    def fixture(self):
        return {"spawns": [
            {"entity": 1, "classname": "team_CTF_redspawn", "spawnflags": "2"},
            {"entity": 2, "classname": "team_CTF_bluespawn", "spawnflags": "2"},
            {"entity": 3, "classname": "info_player_deathmatch", "spawnflags": "0"},
            {"entity": 4, "classname": "info_player_intermission", "spawnflags": "0"}]}

    def test_team_map_does_not_force_fallback_pit_reachable(self):
        selected, excluded = navigation_spawns(self.fixture())
        self.assertEqual([s["entity"] for s in selected], [1, 2])
        self.assertEqual(excluded[0]["reason"], "deathmatch_fallback_not_team_navigation_seed")

    def test_missing_active_team_retains_fallback(self):
        g = self.fixture(); g["spawns"][1]["spawnflags"] = "0"
        selected, _ = navigation_spawns(g)
        self.assertIn(3, [s["entity"] for s in selected])

    def test_deathmatch_only_retains_playable_seed(self):
        g = self.fixture(); g["spawns"] = g["spawns"][2:]
        selected, _ = navigation_spawns(g)
        self.assertEqual([s["entity"] for s in selected], [3])

if __name__ == "__main__": unittest.main()
