"""Desktop package checks without building or starting any application."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import package_preflight as preflight

HERE=Path(__file__).resolve().parent


class PackagePreflightTests(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory(prefix='navstudio-package-check-')
        self.root=Path(self.temporary.name);self.workspace=self.root/'source';self.package=self.root/'package'
        self.workspace.mkdir();self.package.mkdir()
        server=self.workspace/'src/tools/navstudio/server.py';server.parent.mkdir(parents=True)
        server.write_text("STATIC_FILES={'index.html','project-ui.js','project-ui.css','guide.html'}\n",encoding='utf8')
        sources,payload=preflight.required_files(self.workspace,'TCE2-NavStudio')
        for relative in sources:
            path=self.workspace/relative;path.parent.mkdir(parents=True,exist_ok=True)
            if path!=server:path.write_text('offline fixture: '+relative,encoding='utf8')
            installed=self.package/'_internal'/relative;installed.parent.mkdir(parents=True,exist_ok=True);installed.write_bytes(path.read_bytes())
        for relative in payload:
            path=self.package/relative;path.parent.mkdir(parents=True,exist_ok=True)
            if not path.exists():path.write_bytes(b'offline placeholder')
        (self.package/'README.md').write_bytes((self.workspace/'src/tools/navstudio/README.md').read_bytes())
        self.recast=self.root/'recast.exe';self.recast.write_bytes((self.package/'_internal/bin/tce_navmesh_preview.exe').read_bytes())

    def tearDown(self):self.temporary.cleanup()

    def create(self):return preflight.create(self.package,self.workspace,self.recast)

    def test_complete_desktop_payload_and_source_are_verified(self):
        value=self.create()
        self.assertEqual(value['entry_kind'],'desktop-projects')
        self.assertEqual(preflight.verify(self.package,self.workspace),value)
        for name in ('projects.py','spawns.py','goalgen.py','quiet.py','etsnapshot.py','repair.py','desktop.py'):
            self.assertIn('_internal/src/tools/navstudio/'+name,{p['path'] for p in value['package_files']})

    def test_missing_runtime_dependency_fails_preflight(self):
        (self.package/'_internal'/preflight.WEBVIEW[0]).unlink()
        with self.assertRaisesRegex(ValueError,'Missing package file'):self.create()

    def test_missing_or_stale_voice_editor_page_is_rejected(self):
        self.create()
        page=self.package/'_internal/src/tools/navstudio/editor-ui/tce_omnibot.menu'
        page.unlink()
        with self.assertRaisesRegex(ValueError,'tce_omnibot.menu'):preflight.verify(self.package)

    def test_changed_source_prevents_old_exe_fallback(self):
        self.create();(self.workspace/'src/tools/navstudio/entry.py').write_text('new revision',encoding='utf8')
        with self.assertRaisesRegex(ValueError,'stale source_files'):preflight.verify(self.package,self.workspace)
        preflight.verify(self.package) # a standalone package has no checkout to compare

    def test_changed_payload_or_recast_is_rejected(self):
        self.create();(self.package/'_internal/src/tools/navstudio/project-ui.js').write_text('old UI',encoding='utf8')
        with self.assertRaisesRegex(ValueError,'stale package_files'):preflight.verify(self.package)
        (self.package/'_internal/src/tools/navstudio/project-ui.js').write_bytes((self.workspace/'src/tools/navstudio/project-ui.js').read_bytes())
        self.recast.write_bytes(b'new terrain detail build')
        with self.assertRaisesRegex(ValueError,'Recast executable differs'):self.create()

    def test_old_package_without_desktop_manifest_is_rejected(self):
        with self.assertRaisesRegex(ValueError,'manifest is missing'):preflight.verify(self.package)

    def test_manifest_cannot_hash_outside_package(self):
        self.create();path=self.package/preflight.MANIFEST;value=json.loads(path.read_text())
        value['package_files'][0]['path']='../source/src/tools/navstudio/server.py'
        path.write_text(json.dumps(value),encoding='utf8')
        with self.assertRaisesRegex(ValueError,'Invalid package-relative path'):preflight.verify(self.package)

    def test_powershell_launcher_guard_validates_without_running_package(self):
        self.create()
        command=['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(HERE/'verify-package.ps1'),
                 '-Package',str(self.package),'-Workspace',str(self.workspace)]
        result=subprocess.run(command,capture_output=True,text=True,errors='replace',creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        (self.workspace/'src/tools/navstudio/entry.py').write_text('changed source',encoding='utf8')
        result=subprocess.run(command,capture_output=True,text=True,errors='replace',creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0))
        self.assertNotEqual(result.returncode,0)
        self.assertIn('entry.py',result.stderr)

    def test_builder_lists_lazy_modules_and_source_launcher_checks_before_exe(self):
        script=(HERE/'build-windows.ps1').read_text(encoding='utf8')
        for module in preflight.MODULES:self.assertIn("'--hidden-import', '"+module+"'",script)
        self.assertIn('package_preflight.py',script)
        self.assertIn("Replace('@PACKAGE_NAME@', $PackageName)",script)
        launcher=(HERE/'Start.cmd').read_text(encoding='utf8')
        self.assertLess(launcher.index('where py'),launcher.index('if not exist "%NAVSTUDIO_EXE%"'))
        self.assertLess(launcher.index('verify-package.ps1'),launcher.index('"%NAVSTUDIO_EXE%" --workspace'))


class DefaultMapLibraryTests(unittest.TestCase):
    def test_frozen_recast_wins_over_stale_external_build(self):
        from server import Studio
        import os
        name='tce_navmesh_preview.exe' if os.name=='nt' else 'tce_navmesh_preview'
        with tempfile.TemporaryDirectory(prefix='navstudio-recast-selection-') as directory:
            studio=Studio.__new__(Studio);studio.test=Path(directory)/'game'
            external=studio.test/'navmesh-build/Release'/name
            bundled=Path(directory)/'bundle/bin'/name
            external.parent.mkdir(parents=True);bundled.parent.mkdir(parents=True)
            external.write_bytes(b'old geometry compiler');bundled.write_bytes(b'new detail compiler')
            with patch('sys.frozen',True,create=True),patch('sys._MEIPASS',str(bundled.parent.parent),create=True):
                self.assertEqual(studio.find_recast(),bundled.resolve())
            with patch('sys.frozen',False,create=True):
                self.assertEqual(studio.find_recast(),external.resolve())

    def test_repository_and_game_libraries_are_both_catalogued(self):
        from server import Studio
        with tempfile.TemporaryDirectory(prefix='navstudio-library-check-') as directory:
            studio=Studio.__new__(Studio);studio.workspace=Path(directory)/'checkout';studio.test=Path(directory)/'existing-game'
            projects=studio.make_projects()
            self.assertEqual(projects.libraries,[studio.workspace/'etmain/all_maps',studio.test/'etmain/all_maps'])
            self.assertEqual(projects.etmain,studio.test/'etmain')

    def test_standalone_library_is_next_to_game_and_not_duplicated(self):
        from server import Studio
        with tempfile.TemporaryDirectory(prefix='navstudio-library-check-') as directory:
            studio=Studio.__new__(Studio);studio.workspace=studio.test=Path(directory)
            projects=studio.make_projects()
            self.assertEqual(projects.libraries,[studio.test/'etmain/all_maps'])
            self.assertEqual(projects.directory,studio.test/'projects')


if __name__=='__main__':unittest.main()
