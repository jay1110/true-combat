"""CI-side byte validation receipts; small reports never replace archive checks.

The release receipt opens the actual promoted archives. The loose receipt
downloads and opens the exact Actions artifact ZIP after its upload. Both bind
the checks to every tracked release input in the clean checkout and the manual
run identity. This is packaging evidence, not behavioral parity or reproducible
compiler output proof.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import urllib.parse
import urllib.request
import zipfile

from package import ROOT, version
from source_archive import PREFIXES, ROOT_FILES
from client_packages import check_canonical, check_module, target_table
from loose_binaries import flat_names, read_target
from validate import archive as validate_archive, safe_names

SCHEMA = 1
VARIANTS = ('game', 'navstudio')


def encoded(value):
    return (json.dumps(value, sort_keys=True, separators=(',', ':')) + '\n').encode()


def sha(data):
    return hashlib.sha256(data).hexdigest()


def hash_file(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def release_paths(repo=ROOT, include_untracked=False):
    """All tracked build, dependency, policy and resource inputs, including data."""
    command = ['git', 'ls-files', '-z', '--cached']
    if include_untracked:
        command += ['--others', '--exclude-standard']
    names = subprocess.check_output(command, cwd=repo).decode().split('\0')
    return sorted({name for name in names if name and
                   (name in ROOT_FILES or name.startswith(PREFIXES)) and
                   '/reconstruction/' not in name and Path(name).name != 'AGENTS.md' and
                   not any(part in ('__pycache__', 'build', '.venv', 'node_modules')
                           for part in Path(name).parts)})


def source_identity(repo=ROOT, include_untracked=False):
    files = {}
    for name in release_paths(repo, include_untracked):
        path = repo / name
        if path.is_symlink() or not path.is_file():
            raise ValueError('Missing/symlink release input: ' + name)
        files[name] = {'bytes': path.stat().st_size, 'sha256': hash_file(path)}
    return {'sha256': sha(encoded(files)), 'files': files}


def ci_identity(kind):
    values = {key: os.environ.get(key, '') for key in
              ('GITHUB_REPOSITORY', 'GITHUB_SHA', 'GITHUB_RUN_ID', 'GITHUB_RUN_ATTEMPT',
               'GITHUB_EVENT_NAME', 'GITHUB_REF', 'GITHUB_WORKFLOW_REF')}
    workflow = 'release.yml' if kind == 'release' else 'loose-binaries.yml'
    if (values['GITHUB_EVENT_NAME'] != 'workflow_dispatch' or
            not re.fullmatch(r'[a-f0-9]{40}', values['GITHUB_SHA']) or
            not values['GITHUB_RUN_ID'].isdigit() or not values['GITHUB_RUN_ATTEMPT'].isdigit() or
            not values['GITHUB_WORKFLOW_REF'].startswith(
                values['GITHUB_REPOSITORY'] + '/.github/workflows/' + workflow + '@')):
        raise ValueError('Closure receipt requires its actual manual Actions workflow identity')
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    if head != values['GITHUB_SHA']:
        raise ValueError('Checkout HEAD differs from workflow source')
    changed = subprocess.check_output(['git', 'diff', '--name-only', 'HEAD', '--', *PREFIXES,
                                       *sorted(ROOT_FILES)], cwd=ROOT, text=True)
    if changed.strip():
        raise ValueError('CI release inputs changed after checkout')
    return values


def fingerprint(path):
    return {'name': path.name, 'bytes': path.stat().st_size, 'sha256': hash_file(path)}


def verify_zip_records(path, records):
    expected = {row['path']: row for row in records}
    if len(expected) != len(records):
        raise ValueError('Duplicate expected ZIP records')
    with zipfile.ZipFile(path) as zipped:
        if safe_names(zipped) != set(expected):
            raise ValueError('ZIP entry closure mismatch: ' + path.name)
        for name, row in expected.items():
            with zipped.open(name) as stream:
                actual = hashlib.file_digest(stream, 'sha256').hexdigest()
            if zipped.getinfo(name).is_dir() or zipped.getinfo(name).file_size != row['bytes'] or actual != row['sha256']:
                raise ValueError('ZIP byte mismatch: ' + name)
    return fingerprint(path)


def release_closure(directory, inputs):
    stage = directory / 'bundle'
    manifest_path = stage / 'release-manifest.json'
    manifest = json.loads(manifest_path.read_text())
    targets = set(target_table())
    pairs = sorted([[target, variant] for target in targets for variant in VARIANTS])
    if (manifest.get('complete') is not True or manifest.get('client_inputs_promoted') is not True or
            manifest.get('github_sha') != os.environ['GITHUB_SHA'] or
            manifest.get('expected_targets') != pairs or manifest.get('collected_targets') != pairs or
            set(manifest.get('client_packages', {})) != set(VARIANTS) or
            manifest.get('version') != version()):
        raise ValueError('Release is not a complete source-bound two-variant ten-ABI collection')
    records = manifest['files']
    if len({row['path'] for row in records}) != len(records):
        raise ValueError('Duplicate release manifest records')
    for row in records:
        path = stage / row['path']
        if not path.resolve().is_relative_to(stage.resolve()) or path.is_symlink() or not path.is_file():
            raise ValueError('Missing/escaped aggregate file')
        if fingerprint(path) != {'name': path.name, 'bytes': row['bytes'], 'sha256': row['sha256']}:
            raise ValueError('Aggregate file fingerprint mismatch: ' + row['path'])
    expected_files = {row['path'] for row in records} | {'release-manifest.json'}
    if {path.relative_to(stage).as_posix() for path in stage.rglob('*') if path.is_file()} != expected_files:
        raise ValueError('Unmanifested aggregate file')
    shared, modules, reports = {}, [], []
    for variant in VARIANTS:
        path = stage / manifest['client_packages'][variant]['archive']
        with zipfile.ZipFile(path) as zipped:
            if safe_names(zipped) != {f'tce2/tce2-{version()}.pk3', 'client-package.json', 'README.txt'}:
                raise ValueError('Wrong shared client archive closure')
            data = zipped.read(f'tce2/tce2-{version()}.pk3')
            canonical = check_canonical(data, version(), variant)
            if canonical != json.loads(zipped.read('client-package.json')):
                raise ValueError('Shared metadata differs from actual canonical PK3')
        if manifest['client_packages'][variant]['sha256'] != sha(data):
            raise ValueError('Shared client aggregate hash mismatch')
        shared[variant] = {'archive': fingerprint(path), 'pk3_sha256': sha(data),
                           'targets': canonical['targets'], 'modules': canonical['modules'],
                           'assets': canonical['assets']}
        for target, config in sorted(target_table().items()):
            path = stage / 'archives' / f'TCE2-{version()}-{target}-{variant}.zip'
            result = validate_archive(path)  # Actual runtime, symbol, resources and boundary checks.
            if (result.get('target') != target or result.get('variant') != variant or
                    not result['complete_runtime'] or result['client_package_scope'] != 'shared'):
                raise ValueError('Target archive lacks full runtime/shared client')
            with zipfile.ZipFile(path) as zipped:
                metadata = json.loads(zipped.read('manifest.json'))
                if (metadata.get('configuration') != 'Release' or
                        metadata.get('authoring') is not (variant == 'navstudio') or
                        sha(zipped.read(f'tce2/tce2-{version()}.pk3')) != shared[variant]['pk3_sha256']):
                    raise ValueError('Wrong target variant/configuration/client bytes')
                for role in ('cgame', 'ui', 'qagame'):
                    name = config.get('prefix', '') + role + config['suffix']
                    data = zipped.read('tce2/' + name)
                    check_module(data, role, target, variant)
                    modules.append({'target': target, 'variant': variant, 'role': role,
                                    'name': name, 'bytes': len(data), 'sha256': sha(data)})
            reports.append({**result, **fingerprint(path)})
    # Bind actual compiler invocations/result metadata, not only matrix status.
    builds = []
    for path in sorted((stage / 'reports').rglob('build-report.json')):
        row = json.loads(path.read_text())
        if row.get('github_sha') != os.environ['GITHUB_SHA'] or row.get('complete_gameplay_runtime') is not True:
            raise ValueError('Missing/stale complete build provenance')
        steps = row.get('steps', [])
        if ({step.get('step') for step in steps} != {'configure-modules', 'build-modules', 'configure-core', 'build-core'} or
                len(steps) != 4 or any(step.get('exit_code') != 0 or not step.get('command') for step in steps)):
            raise ValueError('Incomplete actual build invocation evidence')
        builds.append({'target': row['target'], 'variant': row['variant'],
                       'sha256': hash_file(path), 'steps': steps})
    if sorted([[row['target'], row['variant']] for row in builds]) != pairs:
        raise ValueError('Missing/duplicate target build reports')
    source = stage / 'archives' / f'TCE2-{version()}-Source.zip'
    expected_source = {name: row for name, row in inputs['files'].items()
                       if Path(name).suffix.lower() not in
                       ('.dll', '.so', '.exe', '.pdb', '.zip', '.pk3', '.dmp', '.pyc')}
    verify_zip_records(source, [{'path': name, **row} for name, row in expected_source.items()])
    complete = directory / f'TCE2-{version()}-All-Binaries-And-Source.zip'
    closure = verify_zip_records(complete, records + [{'path': 'release-manifest.json',
                                'bytes': manifest_path.stat().st_size, 'sha256': hash_file(manifest_path)}])
    return {'targets': sorted(targets), 'variants': list(VARIANTS), 'target_archives': reports,
            'modules': modules, 'shared_clients': shared, 'builds': builds,
            'source_archive': fingerprint(source), 'complete_archive': closure,
            'release_manifest_sha256': hash_file(manifest_path)}


def flat_zip(path, records):
    if len(records) != 40 or len({(row['target'], row['role']) for row in records}) != 40:
        raise ValueError('Flat normal archive requires exactly forty target/role records')
    if {row['target'] for row in records} != set(target_table()) or any(
            row['role'] not in {'cgame', 'ui', 'qagame', 'omnibot'} for row in records):
        raise ValueError('Incomplete flat target/role closure')
    names = [row['archive_name'] for row in records]
    if names != flat_names(records) or any('/' in name or '\\' in name for name in names):
        raise ValueError('Noncanonical/nonflat normal filenames')
    return verify_zip_records(path, [{'path': row['archive_name'], 'bytes': row['bytes'],
                                     'sha256': row['sha256']} for row in records])


class PublicRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        parsed = urllib.parse.urlsplit(newurl)
        if parsed.scheme != 'https' or parsed.username or parsed.password:
            raise ValueError('Unsafe artifact redirect')
        redirected = super().redirect_request(req, fp, code, msg, headers, newurl)
        if redirected is not None:
            redirected.remove_header('Authorization')  # Signed storage URLs need no API token.
        return redirected


def download_artifact(artifact_id, destination):
    repository = os.environ['GITHUB_REPOSITORY']
    if not str(artifact_id).isdigit():
        raise ValueError('Invalid uploaded artifact ID')
    headers = {'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
               'Accept': 'application/vnd.github+json', 'X-GitHub-Api-Version': '2022-11-28'}
    base = f'https://api.github.com/repos/{repository}/actions/artifacts/{artifact_id}'
    with urllib.request.urlopen(urllib.request.Request(base, headers=headers), timeout=60) as response:
        metadata = json.load(response)
    if (metadata.get('expired') or metadata.get('workflow_run', {}).get('id') != int(os.environ['GITHUB_RUN_ID']) or
            metadata.get('name') != f'TCE2-{version()}-Game-Loose-Binaries'):
        raise ValueError('Uploaded flat artifact identity mismatch')
    opener = urllib.request.build_opener(PublicRedirect())
    request = urllib.request.Request(base + '/zip', headers=headers)
    with opener.open(request, timeout=120) as response, destination.open('wb') as stream:
        total = 0
        while block := response.read(1024 * 1024):
            total += len(block)
            if total > 2 * 1024 ** 3:
                raise ValueError('Unexpectedly large uploaded flat archive')
            stream.write(block)
    if metadata.get('digest') != 'sha256:' + hash_file(destination):
        raise ValueError('GitHub artifact digest differs from downloaded ZIP')
    return {key: metadata.get(key) for key in ('id', 'name', 'size_in_bytes', 'digest', 'created_at', 'workflow_run')}


def loose_closure(inputs, directory, artifact_id):
    records, seen = [], set()
    for manifest in sorted(inputs.rglob('loose-target.json')):
        target, rows = read_target(manifest, target_table(), version())
        if target in seen:
            raise ValueError('Duplicate collected loose target')
        seen.add(target)
        records.extend(rows)
    if seen != set(target_table()):
        raise ValueError('Incomplete normal loose target collection')
    names = flat_names(records)
    expected = [{key: row[key] for key in ('target', 'role', 'name', 'sha256', 'bytes')} |
                {'archive_name': name} for row, name in zip(records, names)]
    expected.sort(key=lambda row: row['archive_name'])
    report = json.loads((directory / 'loose-report.json').read_text())
    if (report.get('files') != expected or report.get('binary_count') != 40 or
            report.get('metadata_inside_archive') is not False or report.get('variant') != 'game' or
            report.get('version') != version()):
        raise ValueError('Normal loose report differs from actual target inputs')
    local = flat_zip(directory / report['archive'], expected)
    with tempfile.TemporaryDirectory(prefix='tce2-actual-artifact-') as temporary:
        path = Path(temporary) / 'github-flat.zip'
        metadata = download_artifact(artifact_id, path)
        uploaded = flat_zip(path, expected)
    return {'targets': sorted(seen), 'variant': 'game', 'binary_count': 40,
            'metadata_inside_archive': False, 'files': expected, 'local_zip': local,
            'github_artifact': metadata, 'uploaded_zip': uploaded}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('kind', choices=('release', 'loose'))
    parser.add_argument('--directory', type=Path, required=True)
    parser.add_argument('--input', type=Path)
    parser.add_argument('--artifact-id')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    identity = ci_identity(args.kind)
    inputs = source_identity()
    if args.kind == 'release':
        result = release_closure(args.directory, inputs)
    else:
        if not args.input or not args.artifact_id:
            parser.error('loose requires --input and --artifact-id')
        result = loose_closure(args.input, args.directory, args.artifact_id)
    if inputs != source_identity():
        raise ValueError('Release source inputs changed during validation')
    output = {'schema': SCHEMA, 'kind': args.kind, 'passed': True, 'version': version(),
              'ci': identity, 'inputs': inputs, 'closure': result,
              'limits': ['Byte packaging and trusted workflow source provenance only; no gameplay parity claim',
                         'Not an independent reproducible compiler build comparison']}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(output, indent=2, sort_keys=True) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'passed': True, 'kind': args.kind, 'report': str(args.out), 'input_sha256': inputs['sha256']}))


if __name__ == '__main__':
    main()
