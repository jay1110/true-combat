"""Validate and archive the frozen standalone Windows NavStudio application."""
import argparse
from pathlib import Path
import sys
from package import ROOT, version, zip_folder


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(ROOT / 'navstudio'))
    from package_preflight import verify
    verify(args.package, ROOT)
    args.out.mkdir(parents=True, exist_ok=True)
    output = args.out / f'TCE2-{version()}-NavStudio-Desktop-Windows-x64.zip'
    zip_folder(args.package, output)
    print(output)


if __name__ == '__main__':
    main()
