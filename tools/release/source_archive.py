"""Archive tracked build inputs, excluding local reports, engines and secrets."""
import argparse
from pathlib import Path
import subprocess
import zipfile
from package import ROOT, digest, version

PREFIXES = ('src/', 'vendor/', 'compat/', 'platform/', 'cmake/', 'tools/',
            'tce/', 'navstudio/', 'omnibot/', 'Docs/', 'docs/', '.github/workflows/')
ROOT_FILES = {'CMakeLists.txt', 'CMakePresets.json', 'BUILDING.md', 'README.md',
              'LICENSE', 'CHANGES', 'EULA_TrueCombat_Elite.txt', '.gitignore', '.gitattributes'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--include-untracked', action='store_true', help='Local review only; GitHub releases archive tracked inputs')
    args = parser.parse_args()
    command = ['git', 'ls-files', '-z', '--cached']
    if args.include_untracked:
        command += ['--others', '--exclude-standard']
    names = subprocess.check_output(command, cwd=ROOT).decode().split('\0')
    selected = sorted({name for name in names if name and (name in ROOT_FILES or name.startswith(PREFIXES))
                       and '/reconstruction/' not in name and Path(name).name != 'AGENTS.md'
                       and not any(part in ('__pycache__', 'build', '.venv', 'node_modules') for part in Path(name).parts)
                       and Path(name).suffix.lower() not in ('.dll', '.so', '.exe', '.pdb', '.zip', '.pk3', '.dmp', '.pyc')})
    for required in ('tools/release/build.py', 'vendor/omni-bot/runtime/SOURCE.json', 'EULA_TrueCombat_Elite.txt'):
        if required not in selected:
            raise ValueError('Required release source is not tracked: ' + required)
    args.out.mkdir(parents=True, exist_ok=True)
    output = args.out / f'TCE2-{version()}-Source.zip'
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name in selected:
            archive.write(ROOT / name, name)
    output.with_suffix('.zip.sha256').write_text(digest(output) + '  ' + output.name + '\n', encoding='ascii')
    print(str(output), len(selected), 'files')


if __name__ == '__main__':
    main()
