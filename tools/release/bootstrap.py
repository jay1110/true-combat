"""Fetch checksum-pinned development headers; never fetch game assets."""
import argparse
import hashlib
import json
from pathlib import Path
import tarfile
import urllib.request

BOOST = {
    'version': '1.85.0', 'folder': 'boost_1_85_0',
    'url': 'https://archives.boost.io/release/1.85.0/source/boost_1_85_0.tar.bz2',
    'sha256': '7009fe1faa1697476bdc7027703a2badb84e849b7b0baad5086b087b971f8617',
    'provenance': 'https://archives.boost.io/release/1.85.0/source/boost_1_85_0.tar.bz2.json',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--destination', type=Path, required=True)
    args = parser.parse_args()
    destination = args.destination.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / 'boost_1_85_0.tar.bz2'
    if not archive.is_file():
        with urllib.request.urlopen(BOOST['url'], timeout=120) as response, archive.open('wb') as stream:
            while block := response.read(1024 * 1024):
                stream.write(block)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != BOOST['sha256']:
        raise ValueError('Boost archive checksum mismatch; refusing extraction')
    folder = destination / BOOST['folder']
    if not folder.is_dir():
        with tarfile.open(archive) as source:
            # The data filter rejects links/device files/path escapes on Python3.12+.
            source.extractall(destination, filter='data')
    if not (folder / 'boost/version.hpp').is_file():
        raise ValueError('Boost header closure is missing')
    (destination / 'boost-source.json').write_text(json.dumps(BOOST, indent=2) + '\n', encoding='utf-8')
    print(folder)


if __name__ == '__main__':
    main()
