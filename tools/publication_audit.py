"""Audit tracked publication inputs; never scan local games or rewrite evidence.

Every JSON input has a reviewed role. Historical/original hashes and engine VFS
names are not assumed to be missing repository files. This is a source hygiene
check, not a guarantee that no secret exists or a replacement for runtime tests.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
from urllib.parse import unquote


ROOT = Path(__file__).resolve().parents[1]
JSON_ROLES = {
    'CMakePresets.json': ('build', 'Native/cross configure, build and test presets.'),
    'navstudio/editor-pak/tce2_ui/package.json': ('resources', 'Editor resource closure; not public authoring runtime.'),
    'navstudio/fixtures/door-baseline-a13.json': ('test', 'Native door fixture geometry and original fixture identity.'),
    'tools/assets/base-game-reference.json': ('reference', 'Original package/resource signatures; no original payload.'),
    'tools/assets/base-ui-reference.json': ('reference', 'Original UI signatures/contracts; no original payload.'),
    'tools/assets/pak-sources.json': ('resources', 'Canonical Game/Editor delta resource closure.'),
    'tools/modernize/compat_implementations.json': ('test', 'Frozen relocated compatibility implementation hashes.'),
    'tools/modernize/compat_policy.json': ('test', 'Compatibility classification and validation policy.'),
    'tools/modernize/compat_word_oracle.json': ('test', 'Frozen integer-oracle provenance; not a run report.'),
    'tools/modernize/pose_presentation_inputs.json': ('test', 'Normalized deterministic pose regression inputs.'),
    'tools/modernize/validation_map.example.json': ('example', 'Documented validation-map template.'),
    'tools/parity/reference-policy.json': ('test', 'Authenticated optional local original-binary comparison policy.'),
    'tools/release/runtime-policy.json': ('build', 'Public/editor GM command and payload separation policy.'),
    'tools/release/server-lua.json': ('build', 'Reviewed server Lua scripts and default TOML payload closure.'),
    'tools/release/targets.json': ('build', 'Supported release ABI matrix.'),
    'tools/ui_modernize/source/background-industrial-v1.json': ('provenance', 'Original image generation receipt and master identity.'),
    'tools/ui_modernize/source/original-inventory.json': ('reference', 'Original menu semantic inventory and macro contracts.'),
    'vendor/dependencies.lock.json': ('build', 'Pinned vendored snapshots and reviewed provenance history.'),
    'vendor/luasql/SOURCE.json': ('provenance', 'LuaSQL upstream origin, license, original hashes and integration.'),
    'vendor/omni-bot/runtime/SOURCE.json': ('provenance', 'Authenticated imported runtime-script/navigation source receipt.'),
    'vendor/sqlite/SOURCE.json': ('provenance', 'SQLite upstream archive checksum and license origin.'),
    'pak/fonts/tce2_ui/PORTABLE.json': ('resources', 'Portable ET font ABI/atlas generation and metrics.'),
    'pak/fonts/tce2_ui/SOURCE.json': ('provenance', 'Font revision, licenses and original asset identities.'),
    'pak/tce2_ui/SOURCE.json': ('provenance', 'Generated UI artwork source/generator identities.'),
    'pak/tce2_ui/backgrounds/catalog.json': ('resources', 'Runtime background selection catalog.'),
    'pak/tce2_ui/package.json': ('resources', 'Public UI resource closure.'),
}
for _prefix in ('pak', 'src/assets'):
    for _part in ('gfx/flags', 'gfx/tce2/fireteam', 'gfx/tce2/nav', 'levelshots'):
        JSON_ROLES[f'{_prefix}/{_part}/SOURCE.json'] = (
            'provenance', 'Resource provenance/hash receipt; mirrored source metadata is retained.')

# Exact upstream documentation examples, not developer identities. Do not alter
# vendored SDK/SQLite bodies to remove their documented URI examples.
UPSTREAM_EXAMPLES = {
    'vendor/sqlite/sqlite3.c': {'fred', 'user1', 'me', 'bill'},
    'vendor/sqlite/sqlite3.h': {'fred'},
    'vendor/omni-bot/source/Omnibot/dependencies/gmscriptex/gmsrc_ex/src/3rdParty/sqlite/sqlite3.c': {'fred', 'user1', 'me', 'bill'},
    'vendor/omni-bot/source/Omnibot/dependencies/gmscriptex/gmsrc_ex/src/3rdParty/sqlite/sqlite3.h': {'fred'},
    # Flex 1995 upstream SCM Header receipt, not a project developer path.
    'vendor/omni-bot/source/Omnibot/dependencies/gmscriptex/gmsrc_ex/src/gm/gmScanner.cpp': {'daffy'},
}
HOME = re.compile(r'''(?:[A-Za-z]:[/\\]Users[/\\]|(?<![\w.])/(?:home|Users)/)([^/\\\s"'<>]+)''', re.I)
TOKEN = re.compile(r'\b(?:gh[pousr]_[A-Za-z0-9]{32,}|github_pat_[A-Za-z0-9_]{50,}|sk-(?:proj-)?[A-Za-z0-9_-]{30,}|AKIA[A-Z0-9]{16})\b')
PRIVATE_KEY = re.compile(r'-----BEGIN (?:RSA |EC |OPENSSH |DSA )?PRIVATE KEY-----')
CREDENTIAL = re.compile(r'''(?ix)^\s*(?:set[a-z]*\s+)?["']?
    (?:password|passwd|rconpassword|refereepassword|api[_-]?key|access[_-]?token|secret)
    ["']?\s*(?:=|:|\s)\s*(?:"([^"\r\n]*)"|'([^'\r\n]*)'|([^\s,;\#]+))''')
CONFIG_SUFFIXES = {'.cfg', '.toml', '.ini', '.json', '.yaml', '.yml', '.env'}
PLACEHOLDERS = {'username', 'user', '<user>', '${user}', '$user', 'yourname', 'your-user'}
ROOT_PREFIXES = ('tools/', 'src/', 'pak/', 'vendor/', 'compat/', 'platform/', 'navstudio/', 'cmake/')


def publication_files(repo, include_untracked=False):
    command = ['git', 'ls-files', '-z', '--cached']
    if include_untracked:
        command += ['--others', '--exclude-standard']
    return sorted(set(name for name in subprocess.check_output(command, cwd=repo).decode('utf-8').split('\0') if name))


def scan_text(name, text):
    """Return locations/types only: potential credentials are never echoed."""
    findings = []
    decoded = unquote(text.replace('\\\\', '\\'))
    for match in HOME.finditer(decoded):
        user = match[1].rstrip(':;,').casefold()
        if user in PLACEHOLDERS or user in UPSTREAM_EXAMPLES.get(name, set()):
            continue
        findings.append({'path': name, 'line': decoded.count('\n', 0, match.start()) + 1,
                         'kind': 'personal-home-path', 'severity': 'error'})
    for kind, pattern in (('credential-token', TOKEN), ('private-key', PRIVATE_KEY)):
        findings.extend({'path': name, 'line': text.count('\n', 0, match.start()) + 1,
                         'kind': kind, 'severity': 'error'} for match in pattern.finditer(text))
    if Path(name).suffix.lower() in CONFIG_SUFFIXES:
        for line, source in enumerate(text.splitlines(), 1):
            match = CREDENTIAL.match(source)
            if not match:
                continue
            value = next((value for value in match.groups() if value is not None), '')
            if value and not value.startswith(('$', '<')) and value.casefold() not in {
                    'null', 'none', 'false', 'true', 'example', 'changeme', 'change-me'}:
                findings.append({'path': name, 'line': line, 'kind': 'credential-assignment-review',
                                 'severity': 'warning'})
    return findings


def reject_constant(value):
    raise ValueError('Non-JSON constant')


def path_issue(repo, manifest, value, base='', pointer=''):
    """Check only declared repo inputs, not external source/VFS/historical names."""
    if not isinstance(value, str):
        return {'path': manifest, 'pointer': pointer, 'kind': 'invalid-input-path', 'severity': 'error'}
    relative = PurePosixPath(value)
    target = repo / base / value
    if ('\\' in value or ':' in value or relative.is_absolute() or
            not target.resolve().is_relative_to(repo.resolve())):
        return {'path': manifest, 'pointer': pointer, 'kind': 'unsafe-input-path', 'severity': 'error'}
    if not target.exists():
        return {'path': manifest, 'pointer': pointer, 'kind': 'missing-input-path',
                'reference': (PurePosixPath(base) / relative).as_posix(), 'severity': 'error'}
    return None


def declared_inputs(name, data, repo=ROOT):
    """Yield (value, repo-relative base, JSON pointer) for current input schemas."""
    if not isinstance(data, dict):
        return
    for key in ('generator', 'raster_writer', 'rasterizer', 'master', 'background_source',
                'output', 'validation_inputs', 'validation_runner'):
        value = data.get(key)
        if isinstance(value, str) and value.startswith(ROOT_PREFIXES):
            yield value, '', '/' + key
    if name == 'CMakePresets.json':
        for i, preset in enumerate(data.get('configurePresets', [])):
            value = preset.get('cacheVariables', {}).get('CMAKE_TOOLCHAIN_FILE', '')
            if isinstance(value, str) and value.startswith('${sourceDir}/'):
                yield value[len('${sourceDir}/'):], '', f'/configurePresets/{i}/CMAKE_TOOLCHAIN_FILE'
    elif name == 'tools/assets/pak-sources.json':
        for key in ('game', 'editor_overlay'):
            yield data[key], '', '/' + key
        for key in ('stock_reference', 'stock_ui_reference'):
            yield data[key]['path'], '', '/' + key + '/path'
        for key, base in (('game_files', data['game']), ('editor_files', data['editor_overlay'])):
            for value in data[key]:
                yield value, base, '/' + key + '/' + value
    elif name == 'vendor/dependencies.lock.json':
        for i, row in enumerate(data['packages']):
            for key in ('path', 'notice'):
                if key in row:
                    yield row[key], '', f'/packages/{i}/{key}'
        for key, field in (('optional_fetched_dependencies', 'declared_in'),
                           ('external_configuration', 'requirements')):
            for i, row in enumerate(data.get(key, [])):
                if field in row:
                    yield row[field], '', f'/{key}/{i}/{field}'
    elif name == 'vendor/luasql/SOURCE.json':
        for i, value in enumerate(data['integration']['sources']):
            yield value, 'vendor/luasql', f'/integration/sources/{i}'
    elif name == 'tools/parity/reference-policy.json':
        # required_windows/Linux keys deliberately name private, optional local
        # original binaries. Only the redistributable wrapper is a repo input.
        for key, row in data['approved_wrappers'].items():
            yield row['script'], '', '/approved_wrappers/' + key + '/script'
    elif name == 'tools/release/server-lua.json':
        for i, value in enumerate(data['files']):
            yield value, 'src/lua', f'/files/{i}'
    elif name == 'tools/modernize/compat_implementations.json':
        for i, row in enumerate(data['files']):
            yield row['implementation'], '', f'/files/{i}/implementation'
    elif name == 'tools/modernize/compat_word_oracle.json':
        for i, row in enumerate(data['functions']):
            yield row['source'], '', f'/functions/{i}/source'
    elif name == 'tools/modernize/compat_policy.json':
        for value in data['facades']:
            yield value, '', '/facades/' + value
    elif name == 'vendor/omni-bot/runtime/SOURCE.json':
        for i, row in enumerate(data['files']):
            yield row['path'], 'vendor/omni-bot/runtime', f'/files/{i}/path'
    elif name.startswith(('pak/', 'src/assets/')):
        if isinstance(data.get('files'), list):
            for i, row in enumerate(data['files']):
                value = row.get('path') or row.get('asset')
                if value:
                    base = 'pak' if row.get('path') else str(PurePosixPath(name).parent)
                    yield value, base, f'/files/{i}'
        elif isinstance(data.get('files'), dict):
            for value in data['files']:
                yield value, 'pak', '/files/' + value
        if data.get('asset'):
            yield data['asset'], str(PurePosixPath(name).parent), '/asset'
    elif name == 'navstudio/editor-pak/tce2_ui/package.json':
        for value in data['files']:
            # Editor manifest is the explicit overlay plus inherited public art.
            yield value, ('navstudio/editor-pak' if (repo / 'navstudio/editor-pak' / value).is_file()
                          else 'pak'), '/files/' + value


def audit(repo, names):
    repo = Path(repo).resolve()
    published = set(names)
    inventory, findings = [], []
    for name in names:
        path = repo / name
        if not path.is_file():
            findings.append({'path': name, 'kind': 'missing-tracked-file', 'severity': 'error'})
            continue
        if not path.resolve().is_relative_to(repo):
            findings.append({'path': name, 'kind': 'escaped-tracked-file', 'severity': 'error'})
            continue
        raw = path.read_bytes()
        if b'\0' in raw[:8192]:
            continue
        text = raw.decode('utf-8', errors='replace')
        findings.extend(scan_text(name, text))
        if path.suffix.lower() != '.json':
            continue
        role, reason = JSON_ROLES.get(name, ('unclassified', 'New JSON requires publication-role review.'))
        row = {'path': name, 'role': role, 'reason': reason, 'bytes': len(raw),
               'sha256': hashlib.sha256(raw).hexdigest()}
        inventory.append(row)
        if role == 'unclassified':
            findings.append({'path': name, 'kind': 'unclassified-json', 'severity': 'error'})
        try:
            data = json.loads(text, parse_constant=reject_constant)
            if not isinstance(data, dict):
                raise ValueError('Reviewed input schemas require JSON objects')
            row['valid'] = True
            for value, base, pointer in declared_inputs(name, data, repo):
                issue = path_issue(repo, name, value, base, pointer)
                if issue:
                    findings.append(issue)
                elif (repo / base / value).is_file():
                    relative = (repo / base / value).resolve().relative_to(repo).as_posix()
                    if relative not in published:
                        findings.append({'path': name, 'pointer': pointer,
                                         'reference': relative, 'kind': 'unpublished-input-path', 'severity': 'error'})
        except (ValueError, KeyError, TypeError) as error:
            row['valid'] = False
            findings.append({'path': name, 'kind': 'invalid-json-schema', 'severity': 'error',
                             'exception': type(error).__name__})
    return {'schema': 1, 'scope': 'tracked source publication inputs, not Git history or ignored local evidence',
            'files_scanned': len(names), 'json_files': inventory,
            'roles': dict(sorted(Counter(row['role'] for row in inventory).items())),
            'findings': findings, 'passed': not any(row['severity'] == 'error' for row in findings),
            'reviewed_upstream_path_examples': {name: sorted(users) for name, users in UPSTREAM_EXAMPLES.items()},
            'limits': ['Pattern-based credential detection cannot prove absence of every secret.',
                       'Optional original DLLs, upstream source paths, historical receipt hashes and engine VFS names are classified separately from current repo inputs.',
                       'Dedicated resource, vendor and runtime validators remain required.']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--out', type=Path, required=True, help='Local report path (keep outside tracked inputs)')
    parser.add_argument('--include-untracked', action='store_true', help='Review newly authored, nonignored source before staging')
    args = parser.parse_args()
    names = publication_files(args.root, args.include_untracked)
    if args.out.resolve() in {(args.root / name).resolve() for name in names}:
        parser.error('Report output must not overwrite a publication input')
    report = audit(args.root, names)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({key: report[key] for key in ('files_scanned', 'roles', 'passed')}))
    for finding in report['findings']:
        print(f"{finding['path']}:{finding.get('line', finding.get('pointer', ''))}: {finding['kind']}")
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
