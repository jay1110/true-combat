"""Real engine-log failure shape; never starts a game or contacts its API."""
import os,tempfile,time
from pathlib import Path
from types import SimpleNamespace
from unittest import TestCase,main
from server import Studio

class MapLoadFailureTests(TestCase):
    def test_fresh_spawn_fatal_is_reported_without_mistaking_missing_waypoints_for_failure(self):
        with tempfile.TemporaryDirectory()as temp:
            path=Path(temp)/'etconsole.log';studio=SimpleNamespace(mod=Path(temp));started=time.time()-1
            path.write_text("Couldn't load waypoints for map obj_test\n^1ERROR Loading Waypoints.\n")
            self.assertIsNone(Studio.map_load_failure(studio,dict(map_load_log_after=started)))
            path.write_text("********************\nERROR: Couldn't find a spawn point\n********************\n----- Server Shutdown -----\n")
            result=Studio.map_load_failure(studio,dict(map_load_log_after=started))
            self.assertEqual(result['message'],"Couldn't find a spawn point");self.assertEqual(result['path'],str(path))
            self.assertIsNone(Studio.map_load_failure(studio,{}))
            os.utime(path,(started-60,started-60))
            self.assertIsNone(Studio.map_load_failure(studio,dict(map_load_log_after=started)))

    def test_error_without_server_shutdown_is_not_a_terminal_map_load_failure(self):
        with tempfile.TemporaryDirectory()as temp:
            path=Path(temp)/'etconsole.log';path.write_text('ERROR: old mod diagnostic\nServer: obj_test\n')
            self.assertIsNone(Studio.map_load_failure(SimpleNamespace(mod=Path(temp)),dict(map_load_log_after=time.time()-1)))

if __name__=='__main__':main()
