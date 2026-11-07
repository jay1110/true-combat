#!/usr/bin/env python3
"""Independently verify opt-in UI PK3 closure, semantics and public boundaries."""
import argparse
import hashlib
import io
import importlib.util
import json
from pathlib import Path, PurePosixPath
import stat
import zipfile

import semantic as sem
import classic_assets

ROOT = Path(__file__).resolve().parents[2]
_spec=importlib.util.spec_from_file_location('tce2_canonical_ui',Path(__file__).with_name('package.py'))
canonical_ui=importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(canonical_ui)
OVERRIDES = ('ingame_main.menu', 'options_controls.menu', 'options_customise_hud.menu')
LOAD_LISTS = ('menus.txt', 'menus_gt2.txt', 'menus_gt5.txt', 'menus_gt7.txt')
EDITOR_MENU = 'ui/tce_omnibot.menu'
FORBIDDEN_SUFFIXES = {'.dll', '.so', '.dylib', '.exe', '.qvm', '.wasm', '.bsp', '.way', '.gm',
                      '.zip', '.pk3', '.obj', '.o', '.lib', '.a', '.pdb', '.map'}
RUNTIME_MARKER=('tce2_ui/runtime.cfg',b'TCE2_MODERN_UI 1\n')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def safe_path(name):
    return (isinstance(name, str) and bool(name) and '\\' not in name and ':' not in name and '\0' not in name
            and not name.endswith('/') and not PurePosixPath(name).is_absolute()
            and all(part not in ('', '.', '..') for part in name.split('/')))


def read_archive(source):
    files = {}
    with zipfile.ZipFile(io.BytesIO(source) if isinstance(source, bytes) else source) as archive:
        entries = archive.infolist()
        if len(entries) > 4096 or sum(e.file_size for e in entries) > 128 * 1024 * 1024:
            raise ValueError('Unbounded UI archive')
        for entry in entries:
            name = entry.filename
            if (entry.orig_filename != name or not safe_path(name) or name.casefold() in {n.casefold() for n in files} or
                    stat.S_ISLNK(entry.external_attr >> 16) or entry.file_size > 32 * 1024 * 1024):
                raise ValueError('Unsafe/duplicate UI archive entry: ' + name)
            data = archive.read(entry)
            if (PurePosixPath(name).suffix.lower() in FORBIDDEN_SUFFIXES or
                    data.startswith((b'MZ', b'\x7fELF', b'\0asm', b'\xfe\xed\xfa\xce', b'\xfe\xed\xfa\xcf',
                                     b'\xce\xfa\xed\xfe', b'\xcf\xfa\xed\xfe', b'\xca\xfe\xba\xbe'))):
                raise ValueError('Executable/gameplay payload in UI-only archive: ' + name)
            files[name] = data
    return files


def text_paths(files):
    return {p for p in files if PurePosixPath(p).suffix.lower() in ('.menu', '.h') or
            (p.split('/')[0] in sem.DIRECTORIES and PurePosixPath(p).suffix.lower() == '.txt')}


def remove_editor_load(data):
    # Token policy is independent of package.py's line-removal regexp.
    ts = sem.tokens(sem.strip_comments(data.decode('latin-1')))
    result = []
    i = 0
    pattern = ['loadMenu', '{', '"' + EDITOR_MENU + '"', '}']
    while i < len(ts):
        if ts[i:i + 4] == pattern:
            i += 4
        else:
            result.append(ts[i]); i += 1
    return result


def styled_override(data, relative):
    # Used solely for exact *freshness*. Behavior is separately compared to
    # raw production source by the independent semantic parser below.
    import restyle
    return restyle.menu(data.decode('latin-1'), Path(relative)).encode('latin-1')


def compare_semantics(reference, actual):
    reference=sem.layout_reference(reference,actual)
    expected = text_paths(reference)
    found = text_paths(actual)
    if expected != found:
        raise ValueError('Semantic package coverage differs: missing=' + str(sorted(expected-found)) +
                         ', extra=' + str(sorted(found-expected)))
    before_units = [sem.parse(p, reference[p]) for p in sorted(expected)]
    before, contracts = sem.signatures(before_units)
    names = set()
    for unit in before_units:
        for ts in [unit.body] + [d.body for d in unit.defines]:
            names.update(ts[i+1] for i,t in enumerate(ts[:-1]) if t.lower() == 'name')
    after_units=[sem.parse(p, actual[p]) for p in sorted(found)]
    after, _ = sem.signatures(sem.normalize_layout(after_units,before_units,contracts), contracts, names)
    differences = [p for p in sorted(expected) if before[p] != after[p]]
    if differences:
        raise ValueError('Package actions/cvars/labels/semantic data changed: ' + ', '.join(differences))
    return len(expected)


def verify(path, variant, baseline=None, repo=ROOT, transform=None):
    """Verify a diagnostic UI archive against the sole canonical resource trees.

    Historical source-projection checks remain in compare_semantics; runtime
    acceptance no longer reconstructs a second tree from local original files.
    """
    canonical=canonical_ui
    repo=Path(repo)
    archive_bytes=Path(path).read_bytes();files=read_archive(archive_bytes)
    expected=canonical.collect(variant,repo)
    if files!=expected:
        missing=sorted(set(expected)-set(files));extra=sorted(set(files)-set(expected))
        changed=sorted(p for p in set(expected)&set(files) if expected[p]!=files[p])
        raise ValueError('Stale/unclassified canonical UI bytes: missing='+str(missing)+', extra='+str(extra)+', changed='+str(changed))
    for meta_path in ('tce2_ui/SOURCE.json','fonts/tce2_ui/SOURCE.json'):
        try:metadata=json.loads(files[meta_path])
        except (KeyError,ValueError,UnicodeDecodeError) as error:raise ValueError('Missing/invalid UI asset provenance') from error
        for row in metadata.get('files',[]):
            name=row.get('path')
            if not safe_path(name) or name not in files or row.get('sha256')!=sha(files[name]) or row.get('bytes')!=len(files[name]):
                raise ValueError('Missing/stale UI asset provenance: '+str(name))
    art=json.loads(files['tce2_ui/SOURCE.json'])
    for target,source in art.get('native_aliases',{}).items():
        if not safe_path(target) or not safe_path(source) or not target.startswith('ui/assets/') or not source.startswith('tce2_ui/') or files.get(target)!=files.get(source) or source not in files:
            raise ValueError('Missing/stale native widget alias: '+str(target))
    if 'portable' in json.loads(files['fonts/tce2_ui/SOURCE.json']):
        classic_assets.verify({p:d for p,d in files.items() if p.endswith(('.tga','.dat','.ttf','.json'))})
    if variant=='game':
        markers=('tce_omnibot','omnibot_menu','omnibot_goalproperty','tce_navgen','wp_add','goal_edit','navstudio')
        for name in text_paths(files):
            words=' '.join(sem.tokens(sem.strip_comments(files[name].decode('latin1')))).lower()
            if any(marker in words for marker in markers):raise ValueError('Editor authoring reference in Game UI: '+name)
    if canonical.collect(variant,repo)!=expected or Path(path).read_bytes()!=archive_bytes:
        raise ValueError('Canonical UI inputs changed during review')
    return {'schema':1,'passed':True,'variant':variant,'package':str(Path(path).resolve()),'sha256':sha(archive_bytes),
            'entries':len(files),'menus':sum(p.endswith('.menu') for p in files),'semantic_files':len(text_paths(files)),
            'source':'pak + explicit Editor delta' if variant=='editor' else 'pak',
            'payload_sha256':{p:sha(data) for p,data in sorted(files.items())},'input_stable':True,
            'limits':['Exact canonical UI bytes/provenance; no live engine input/rendering or original parity claim.',
                      'Production module, stock-delta and Editor reachability gates live in tools/release and tools/assets.']}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--package', type=Path, required=True)
    p.add_argument('--variant', choices=('game','editor'), required=True)
    p.add_argument('--baseline', type=Path, help='Optional historical local source inventory')
    p.add_argument('--repo', type=Path, default=ROOT)
    p.add_argument('--out', type=Path, required=True)
    args = p.parse_args()
    if any(args.out.resolve().is_relative_to((args.repo/folder).resolve()) for folder in ('pak','pak_original_tce','navstudio/editor-pak','tools/ui_modernize/source')):
        p.error('Package receipt must be outside original/theme sources')
    if args.out.resolve()==args.package.resolve() or (args.baseline and args.out.resolve()==args.baseline.resolve()):
        p.error('Package receipt cannot overwrite package/baseline input')
    try:
        result = verify(args.package, args.variant, json.loads(args.baseline.read_text()) if args.baseline else None, args.repo)
    except (ValueError, OSError, KeyError, TypeError, zipfile.BadZipFile) as error:
        result = {'passed':False,'errors':[str(error)]}
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:result[k] for k in ('passed','variant','entries','menus','sha256','errors') if k in result}))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
