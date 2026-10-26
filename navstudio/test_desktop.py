"""Native window lifecycle must save/return a map, never silently lose it."""
import io
import json
from pathlib import Path
import unittest
from unittest.mock import Mock, patch

import desktop


def response(value):
    result=io.BytesIO(json.dumps(value).encode('utf8'))
    return result


class DesktopLifecycleTests(unittest.TestCase):
    def test_existing_service_reuses_only_same_workspace(self):
        root=Path('.').resolve()
        with patch.object(desktop,'urlopen',return_value=response({'app':'tce2-navstudio','workspace':str(root)})):
            self.assertIsNotNone(desktop.existing_service('http://127.0.0.1:8766/',root))
        with patch.object(desktop,'urlopen',return_value=response({'app':'tce2-navstudio','workspace':str(root/'other')})):
            self.assertIsNone(desktop.existing_service('http://127.0.0.1:8766/',root))

    def test_closing_saves_active_project_before_destroy(self):
        host=desktop.DesktopLifecycle('http://127.0.0.1:8766/');host.window=Mock()
        with patch.object(desktop,'urlopen',side_effect=[response({'token':'local'}),response({'active':{'id':'one'}}),response({'active':None})]) as call:
            host._save_close()
            request=call.call_args_list[2].args[0]
            self.assertEqual(json.loads(request.data),{'op':'save_close'})
            self.assertEqual(request.get_header('X-navstudio-token'),'local')
            host.window.destroy.assert_called_once()
            self.assertTrue(host.may_close)

    def test_failed_project_close_keeps_window_and_reports_error(self):
        host=desktop.DesktopLifecycle('http://127.0.0.1:8766/');host.window=Mock();host.closing=True
        with patch.object(desktop,'urlopen',side_effect=OSError('Mapdatei noch gesperrt')):
            host._save_close()
        host.window.destroy.assert_not_called()
        self.assertFalse(host.may_close);self.assertFalse(host.closing)
        self.assertIn('Mapdatei noch gesperrt',host.window.evaluate_js.call_args.args[0])

    def test_unsaved_companion_text_blocks_native_close_before_project_save(self):
        host=desktop.DesktopLifecycle('http://127.0.0.1:8766/');host.window=Mock();host.closing=True
        host.window.evaluate_js.return_value=False
        with patch.object(desktop,'urlopen') as call:host._save_close()
        call.assert_not_called();host.window.destroy.assert_not_called()
        self.assertFalse(host.may_close);self.assertFalse(host.closing)
        self.assertIn('navstudioBundle.canClose()',host.window.evaluate_js.call_args.args[0])

    def test_headless_workers_and_automation_keep_explicit_service_mode(self):
        args=desktop.arguments(['--no-browser','--workspace','C:/MapTest'])
        self.assertTrue(args.no_browser);self.assertEqual(args.workspace,'C:/MapTest')


if __name__=='__main__':
    unittest.main()
