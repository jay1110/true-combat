"""Doppelklick und expliziter Start müssen dieselbe Workspace-Identität haben."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import server


class WorkspaceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.project = self.root/'project'
        for part in ('src/tools/navstudio', 'tce2', 'etmain'):
            (self.project/part).mkdir(parents=True)
        self.combined = self.project/'build/navgen-overkill'
        self.installation(self.combined)
        self.other = self.root/'other'
        self.installation(self.other)

    def installation(self, path):
        for part in ('tce2', 'etmain'):
            (path/part).mkdir(parents=True)
        (path/'TCE2-NavStudio.exe').write_bytes(b'fixture, never executed')

    def discover(self, executable_folder, cwd, explicit=None, frozen=True):
        with patch.object(server.sys, 'frozen', frozen, create=True), \
             patch.object(server.sys, 'executable', str(executable_folder/'TCE2-NavStudio.exe')), \
             patch.object(server.Path, 'cwd', return_value=cwd):
            return server.discover_workspace(str(explicit) if explicit else None)

    def test_double_click_in_combined_folder_matches_explicit_project(self):
        self.assertEqual(self.discover(self.combined, self.combined), self.project)
        self.assertEqual(self.discover(self.combined, self.other), self.project)
        self.assertEqual(self.discover(self.combined, self.combined, self.project), self.project)

    def test_explicit_workspace_keeps_precedence(self):
        self.assertEqual(self.discover(self.combined, self.combined, self.other), self.other)
        with self.assertRaises(ValueError):
            self.discover(self.combined, self.combined, self.root/'absent')

    def test_standalone_prefers_own_game_folder_to_working_directory(self):
        standalone = self.project/'portable'
        self.installation(standalone)
        self.assertEqual(self.discover(standalone, self.other), standalone)
        self.assertEqual(self.discover(standalone, self.project), standalone)

    def test_named_folder_without_source_project_remains_standalone(self):
        standalone = self.other/'build/navgen-overkill'
        self.installation(standalone)
        self.assertEqual(self.discover(standalone, self.project), standalone)

    def test_nested_noncanonical_installation_is_not_mistaken_for_project(self):
        standalone = self.project/'navgen-overkill'
        self.installation(standalone)
        self.assertEqual(self.discover(standalone, self.project), standalone)

    def test_source_invocation_preserves_existing_working_directory_search(self):
        self.assertEqual(self.discover(self.combined, self.other, frozen=False), self.other)


if __name__ == '__main__':
    unittest.main()
