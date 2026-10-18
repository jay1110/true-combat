"""Process stop and input-preserving starter regressions; window locks removed."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock
from watch import Windows, ProcessIdentityChanged


class NoWindowLocksTests(unittest.TestCase):
    def test_removed_control_endpoints_and_native_mutation_are_unavailable(self):
        from server import Studio
        self.assertNotIn('watch_on', Studio.ACTIONS)
        self.assertNotIn('watch_off', Studio.ACTIONS)
        for name in ('action_watch_on','action_watch_off','capture_inputs','apply_watch'):
            self.assertFalse(hasattr(Studio,name))
        for name in ('block','restore','change','window_state'):
            self.assertFalse(hasattr(Windows,name))
        import watch
        self.assertFalse(hasattr(watch,'WatchGuard'))
        self.assertFalse(hasattr(watch,'InputIsolation'))


class TerminateTests(unittest.TestCase):
    def test_exact_handle_identity_controls_only_authorized_process(self):
        api=Windows.__new__(Windows)
        exe=Path(tempfile.gettempdir())/'own-test'/'etl.exe'
        identity=(os.path.normcase(str(exe)),100)
        api.k=SimpleNamespace(OpenProcess=Mock(return_value=99),CloseHandle=Mock(),
            TerminateProcess=Mock(return_value=True),WaitForSingleObject=Mock(return_value=0))
        api._handle_identity=Mock(return_value=identity)
        result=api.terminate(7,identity,expected_exe=exe)
        self.assertTrue(result['terminated'])
        api.k.TerminateProcess.assert_called_once_with(99,1)
        api.k.WaitForSingleObject.assert_called_once_with(99,5000)
        api.k.CloseHandle.assert_called_once_with(99)
        api.k.TerminateProcess.reset_mock()
        api._handle_identity.return_value=(identity[0],101)
        with self.assertRaises(ProcessIdentityChanged):api.terminate(7,identity,expected_exe=exe)
        api.k.TerminateProcess.assert_not_called()
        api.k.OpenProcess.reset_mock()
        with self.assertRaises(ProcessIdentityChanged):api.terminate(7,identity,expected_exe=exe.parent/'ET.exe')
        api.k.OpenProcess.assert_not_called()

    def test_missing_handle_failure_and_wait_timeout_never_claim_success(self):
        api=Windows.__new__(Windows)
        exe=Path(tempfile.gettempdir())/'ET.exe'
        identity=(os.path.normcase(str(exe)),100)
        api.k=SimpleNamespace(OpenProcess=Mock(return_value=0),CloseHandle=Mock(),
            TerminateProcess=Mock(return_value=False),WaitForSingleObject=Mock(return_value=258))
        api._handle_identity=Mock(return_value=identity)
        with self.assertRaises(RuntimeError):api.terminate(7,identity,expected_exe=exe)
        api.k.TerminateProcess.assert_not_called()
        api.k.OpenProcess.return_value=99
        with self.assertRaises(RuntimeError):api.terminate(7,identity,expected_exe=exe)
        api.k.TerminateProcess.return_value=True
        with self.assertRaises(RuntimeError):api.terminate(7,identity,expected_exe=exe)
        self.assertEqual(api.k.CloseHandle.call_count,2)


@unittest.skipUnless(os.name=='nt','PowerShell-Startertest nur unter Windows')
class LauncherTests(unittest.TestCase):
    def test_real_script_with_mocked_game_start_preserves_config_and_arguments(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp).resolve();mod=root/'tce2';mod.mkdir()
            exe=root/'etl.exe';exe.write_bytes(b'never executed')
            (mod/'navstress.cfg').write_text('set sv_pure 0\nunbindall\nmap old\n')
            original=b'bind F9 "custom action"\nseta in_mouse "-1"\n'
            (mod/'etconfig.cfg').write_bytes(original)
            output=root/'launch.json'
            fixture=root/'test.ps1'
            starter=Path(__file__).with_name('start-test.ps1').resolve()
            # Nur Get-Process und Start-Process ersetzen; Datei-/Backupcode echt.
            fixture.write_text('param($Script,$Instance,$Exe,$Output)\n'
                'function Get-Process { param($Name,$ErrorAction) @() }\n'
                'function Start-Process { param($FilePath,$WorkingDirectory,$ArgumentList,$WindowStyle,[switch]$PassThru)\n'
                ' @{file=$FilePath; cwd=$WorkingDirectory; args=$ArgumentList; style=$WindowStyle} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $Output\n'
                ' [pscustomobject]@{Id=4242}\n}\n'
                '& $Script -Instance $Instance -MapName obj_delta -GameExecutable $Exe\n')
            result=subprocess.run(['powershell','-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-File',str(fixture),
                '-Script',str(starter),'-Instance',str(root),'-Exe',str(exe),'-Output',str(output)],
                stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,
                creationflags=subprocess.CREATE_NO_WINDOW,timeout=20)
            self.assertEqual(result.returncode,0,result.stdout.decode(errors='replace'))
            launch=json.loads(output.read_text(encoding='utf-8-sig'))
            self.assertEqual(Path(launch['file']),exe)
            self.assertNotIn('in_mouse',launch['args'])
            self.assertNotIn('in_joystick',launch['args'])
            self.assertNotIn('navstudio-watch.cfg',launch['args'])
            self.assertEqual((mod/'etconfig.cfg').read_bytes(),original)
            backups=list((mod/'navstudio-input-backups').glob('launch-*/tce2/etconfig.cfg'))
            self.assertEqual(len(backups),1)
            self.assertEqual(backups[0].read_bytes(),original)
            self.assertNotIn('unbindall',(mod/'navstudio-test.cfg').read_text())
            self.assertIn('+devmap',launch['args'])
            self.assertNotIn('+map',launch['args'])
            for setting in ['bind n noclip','bind k kill','set timelimit 999','set g_useralliedrespawntime 1','set g_useraxisrespawntime 1','set g_redlimbotime 1000','set g_bluelimbotime 1000']:
                self.assertIn(setting,(mod/'navstudio-test.cfg').read_text())


if __name__=='__main__':unittest.main()
