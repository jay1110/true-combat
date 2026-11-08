"""Publication audits must distinguish real leaks from reviewed source receipts."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from publication_audit import audit, path_issue, publication_files, scan_text


class PublicationAudit(unittest.TestCase):
    def test_git_inventory_includes_all_tracked_json_and_skips_ignored_local_reports(self):
        with tempfile.TemporaryDirectory() as folder:
            repo = Path(folder)
            subprocess.run(['git', 'init', '-q', str(repo)], check=True)
            for name in ('CMakePresets.json', 'vendor/a space/雪.json', '.gitignore', 'local.json'):
                path = repo / name; path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('/local.json\n' if name == '.gitignore' else '{}', encoding='utf-8')
            subprocess.run(['git', 'add', 'CMakePresets.json', 'vendor', '.gitignore'], cwd=repo, check=True)
            self.assertEqual(publication_files(repo), ['.gitignore', 'CMakePresets.json', 'vendor/a space/雪.json'])
            self.assertEqual(publication_files(repo, True), publication_files(repo))
            report = audit(repo, publication_files(repo))
            self.assertEqual(len(report['json_files']), 2)
            self.assertFalse(report['passed'])  # Unreviewed vendor JSON cannot silently publish.
            self.assertEqual(report['findings'][0]['kind'], 'unclassified-json')

    def test_personal_paths_both_slashes_unix_and_url_escaped_are_reported(self):
        paths = ['C:' + '/Users/' + 'private-person/project',
                 'C:' + '\\Users\\' + 'private-person\\project',
                 '/' + 'home/private-person/project',
                 'file:///' + 'C%3A%2FUsers%2F' + 'private-person%2Fproject']
        for value in paths:
            with self.subTest(value=value):
                self.assertEqual(scan_text('tools/data.json', value)[0]['kind'], 'personal-home-path')
                self.assertEqual(scan_text('tools/data.json', json.dumps({'nested': [value]}))[0]['kind'], 'personal-home-path')

    def test_credentials_are_redacted_and_upstream_uri_examples_are_not_rewritten(self):
        token = 'ghp_' + 'a' * 36
        findings = scan_text('config.cfg', token)
        self.assertEqual(findings[0]['kind'], 'credential-token')
        self.assertNotIn(token, json.dumps(findings))
        example = '/' + 'home/fred/data.db'
        self.assertEqual(scan_text('vendor/sqlite/sqlite3.c', example), [])
        self.assertTrue(scan_text('tools/new.json', example))
        self.assertEqual(scan_text('source.cpp', 'C:' + '/Users/username/Documents'), [])

    def test_configuration_secret_assignments_are_reviewed_but_templates_are_allowed(self):
        text = '\n'.join(['# password = "documented placeholder"', 'set rconpassword "local-value"',
                          '"api_key": "private-value",', 'password = ""',
                          'password = "<configure-locally>"', 'token = "${{ secrets.TOKEN }}"'])
        findings = scan_text('default.cfg', text)
        self.assertEqual([row['line'] for row in findings], [2, 3])
        self.assertTrue(all(row['severity'] == 'warning' for row in findings))
        self.assertNotIn('local-value', json.dumps(findings))

    def test_required_paths_cannot_escape_and_missing_paths_fail(self):
        with tempfile.TemporaryDirectory() as folder:
            repo = Path(folder)
            self.assertEqual(path_issue(repo, 'manifest.json', '../private.txt')['kind'], 'unsafe-input-path')
            self.assertEqual(path_issue(repo, 'manifest.json', 'src/missing.c')['kind'], 'missing-input-path')
            (repo / 'src').mkdir(); (repo / 'src/exists.c').write_text('source')
            self.assertIsNone(path_issue(repo, 'manifest.json', 'src/exists.c'))

    def test_missing_parity_wrapper_is_failure_but_private_original_dll_is_optional(self):
        with tempfile.TemporaryDirectory() as folder:
            repo = Path(folder); policy = repo / 'tools/parity/reference-policy.json'
            policy.parent.mkdir(parents=True)
            policy.write_text(json.dumps({'required_windows': {'tcetest/private.dll': 'a' * 64},
                                          'approved_wrappers': {'test': {'script': 'tools/parity/wrapper.py'}}}))
            names = ['tools/parity/reference-policy.json']
            report = audit(repo, names)
            self.assertEqual([f['kind'] for f in report['findings']], ['missing-input-path'])
            wrapper = repo / 'tools/parity/wrapper.py'; wrapper.write_text('# wrapper')
            self.assertEqual(audit(repo, names)['findings'][0]['kind'], 'unpublished-input-path')
            self.assertTrue(audit(repo, names + ['tools/parity/wrapper.py'])['passed'])

    def test_malformed_json_fails_with_location_without_echoing_document(self):
        with tempfile.TemporaryDirectory() as folder:
            repo = Path(folder); (repo / 'CMakePresets.json').write_text('{"secret":')
            report = audit(repo, ['CMakePresets.json'])
            self.assertFalse(report['passed'])
            self.assertEqual(report['findings'][0]['kind'], 'invalid-json-schema')


if __name__ == '__main__':
    unittest.main()
