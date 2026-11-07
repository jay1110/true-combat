"""Validate frozen backgrounds, or generate them from explicit external PNGs.

No original picture or fallback background is changed. The manifest records
the complete source-to-native mapping; the runtime uses its stable b01..b16 IDs.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import generate_assets as art
import classic_assets

ROOT = Path(__file__).resolve().parents[2]
PHASES = {**dict.fromkeys(range(1,4),'winter'), **dict.fromkeys(range(4,6),'morning'),
          **dict.fromkeys(range(6,11),'night'), **dict.fromkeys(range(11,14),'day'),
          **dict.fromkeys(range(14,17),'twilight')}
TOKENS = {'winter':'Winter','morning':'Herbstmorgen','night':'Nacht',
          'day':'Sommer','twilight':'Herbst_Abendrot'}
MARKER = b'TCE2_BACKGROUNDS 1\n'
CATALOG = 'tce2_ui/backgrounds/catalog.json'
SHADERS = 'scripts/tce2_backgrounds.shader'
MAX_BYTES = 104*1024*1024


def sha(data):
    return hashlib.sha256(data).hexdigest()


def inventory(source):
    rows = []
    for path in sorted(Path(source).glob('*.png')):
        match = re.fullmatch(r'(\d\d)_(obj_[A-Za-z0-9]+)_(.+)\.png',path.name)
        if not match or int(match[1]) not in PHASES:
            raise ValueError('Unknown map picture: '+path.name)
        index = int(match[1]); phase = PHASES[index]
        if match[3] != TOKENS[phase]:
            raise ValueError('Picture phase/name differs: '+path.name)
        data = path.read_bytes()
        if not 24 <= len(data) <= 16*1024*1024 or data[:8] != b'\x89PNG\r\n\x1a\n':
            raise ValueError('Invalid/unbounded PNG: '+path.name)
        width,height = struct.unpack_from('>II',data,16)
        if not 1 <= width <= 4096 or not 1 <= height <= 4096 or width*height > 4096*2048:
            raise ValueError('Unbounded picture dimensions: '+path.name)
        rows.append({'id':index,'source':path.name,'map':match[2],'phase':phase,
                     'source_sha256':sha(data),'source_dimensions':[width,height]})
    if sorted(row['id'] for row in rows) != list(range(1,17)):
        raise ValueError('Exactly one of each map picture01..16 is required')
    return rows


def render(path):
    width,height,rgba = art.power_of_two_image(*art.png_read(path))
    data = art.tga(width,height,rgba,opaque=True)
    classic_assets.tga_info(data)
    return data,width,height


def shader_text(rows):
    return '\n'.join(row['shader']+'\n{\n    nomipmaps\n    nopicmip\n    {\n        clampmap '+row['path']+
                     '\n        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA\n        rgbGen vertex\n        alphaGen vertex\n    }\n}\n'
                     for row in rows).encode('ascii')


def validate(destination):
    """Verify checked-in runtime bytes and provenance without source pictures.

    Source filenames/hashes remain historical provenance. Only an explicit
    --source asks to check the external source-to-asset relationship again.
    """
    destination = Path(destination)
    data = (destination/CATALOG).read_bytes()
    if len(data) > 64*1024:
        raise ValueError('Background catalog is unbounded')
    manifest = json.loads(data)
    if (manifest.get('schema') != 1 or manifest.get('generator') != 'tools/ui_modernize/background_assets.py' or
            manifest.get('generator_sha256') != sha(Path(__file__).read_bytes()) or
            manifest.get('resampler_sha256') != sha(Path(art.__file__).read_bytes()) or
            manifest.get('fallback') != 'tce2_ui/background.tga'):
        raise ValueError('Stale background generator/resampler provenance')
    rows = manifest.get('files')
    if not isinstance(rows, list) or len(rows) != 16:
        raise ValueError('Exactly one of each background01..16 is required')
    ids = []
    total = len(data)
    for row in rows:
        if not isinstance(row,dict):
            raise ValueError('Invalid background row')
        index = row.get('id')
        if type(index) is not int or index not in PHASES:
            raise ValueError('Invalid background ID')
        ids.append(index)
        name = 'tce2_ui/backgrounds/b%02d'%index
        source = row.get('source', '')
        match = re.fullmatch(r'(\d\d)_(obj_[A-Za-z0-9]+)_(.+)\.png', source)
        if (row.get('path') != name+'.tga' or row.get('shader') != name or
                row.get('phase') != PHASES[index] or not match or int(match[1]) != index or
                match[2] != row.get('map') or match[3] != TOKENS[PHASES[index]] or
                not re.fullmatch(r'[0-9a-f]{64}',row.get('source_sha256',''))):
            raise ValueError('Invalid background identity/provenance')
        dimensions = row.get('source_dimensions')
        if (not isinstance(dimensions,list) or len(dimensions) != 2 or
                any(type(v) is not int or not 1 <= v <= 4096 for v in dimensions) or
                dimensions[0]*dimensions[1] > 4096*2048):
            raise ValueError('Invalid source dimensions')
        path = destination/row['path']
        if (path.stat().st_size != row.get('bytes') or not 18 <= path.stat().st_size <= MAX_BYTES or
                total + path.stat().st_size > MAX_BYTES):
            raise ValueError('Background size differs: '+row['path'])
        image = path.read_bytes()
        width,height,_,_ = classic_assets.tga_info(image)
        if row.get('dimensions') != [width,height] or sha(image) != row.get('sha256'):
            raise ValueError('Background payload differs: '+row['path'])
        total += len(image)
    if sorted(ids) != list(range(1,17)):
        raise ValueError('Exactly one of each background01..16 is required')
    expected = {row['path'] for row in rows}
    actual = {p.relative_to(destination).as_posix() for p in (destination/'tce2_ui/backgrounds').glob('*.tga')}
    if actual != expected:
        raise ValueError('Unexpected or missing background images')
    for name, contents in [('tce2_ui/backgrounds/runtime.cfg',MARKER),(SHADERS,shader_text(rows))]:
        if (destination/name).read_bytes() != contents:
            raise ValueError('Stale background asset: '+name)
        total += len(contents)
    if total > MAX_BYTES:
        raise ValueError('Background catalog exceeds its native package budget')
    return manifest


def build(source, destination, check=False):
    source,destination = Path(source).resolve(),Path(destination).resolve()
    if destination == source or destination.is_relative_to(source) or source.is_relative_to(destination):
        raise ValueError('Source pictures and generated assets must be separate')
    rows = inventory(source); output = {}
    for row in rows:
        name = 'tce2_ui/backgrounds/b%02d'%row['id']; path = name+'.tga'
        existing = destination/path
        # --check verifies frozen bytes/source hashes without repeating all
        # bilinear resampling; generator changes invalidate the manifest below.
        if check:
            data = existing.read_bytes(); width,height,_,_ = classic_assets.tga_info(data)
        else:
            data,width,height = render(source/row['source'])
        row.update(path=path,shader=name,dimensions=[width,height],bytes=len(data),sha256=sha(data))
        output[path]=data
    manifest={'schema':1,'origin':'User-provided map_pics; no generated scene artwork.',
              'generator':'tools/ui_modernize/background_assets.py','generator_sha256':sha(Path(__file__).read_bytes()),
              'resampler_sha256':sha(Path(art.__file__).read_bytes()),'fallback':'tce2_ui/background.tga',
              'selection':'One per engine process, no immediate repeat. Default winter months follow configured country/custom months; optional live-weather mode requires fresh data.',
              'files':rows}
    output[CATALOG]=(json.dumps(manifest,indent=2)+'\n').encode()
    output['tce2_ui/backgrounds/runtime.cfg']=MARKER
    output[SHADERS]=shader_text(rows)
    if sum(len(v) for v in output.values()) > MAX_BYTES:
        raise ValueError('Background catalog exceeds its native package budget')
    if check:
        for name,data in output.items():
            if (destination/name).read_bytes()!=data: raise ValueError('Stale background asset: '+name)
    else:
        for name,data in output.items():
            target=destination/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    return manifest


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source',type=Path,help='Explicit external PNG directory; required only for generation/source checking')
    p.add_argument('--out',type=Path,help='Asset root (default: pak for checks; build/ui-backgrounds for generation)')
    p.add_argument('--check',action='store_true');a=p.parse_args()
    if a.source is None and not a.check:
        p.error('Generation requires an explicit external --source PNG directory')
    destination = a.out or ROOT/('pak' if a.check else 'build/ui-backgrounds')
    manifest=validate(destination) if a.source is None else build(a.source,destination,a.check)
    print(json.dumps({'backgrounds':len(manifest['files']),'checked':a.check,
                      'bytes':sum(v['bytes'] for v in manifest['files'])}))


if __name__=='__main__': main()
