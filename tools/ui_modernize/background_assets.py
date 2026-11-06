"""Build the user's map pictures as stock-ET-compatible menu backgrounds.

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
        if len(data) > 16*1024*1024 or data[:8] != b'\x89PNG\r\n\x1a\n':
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


def build(source, destination, check=False):
    source,destination = Path(source).resolve(),Path(destination).resolve()
    if destination == source or destination.is_relative_to(source) or source.is_relative_to(destination):
        raise ValueError('Source pictures and generated assets must be separate')
    rows = inventory(source); output = {}; shader = []
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
        shader.append(name+'\n{\n    nomipmaps\n    nopicmip\n    {\n        clampmap '+path+
                      '\n        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA\n        rgbGen vertex\n        alphaGen vertex\n    }\n}\n')
    manifest={'schema':1,'origin':'User-provided map_pics; no generated scene artwork.',
              'generator':'tools/ui_modernize/background_assets.py','generator_sha256':sha(Path(__file__).read_bytes()),
              'resampler_sha256':sha(Path(art.__file__).read_bytes()),'fallback':'tce2_ui/background.tga',
              'selection':'One per engine process, no immediate repeat. Default winter months follow configured country/custom months; optional live-weather mode requires fresh data.',
              'files':rows}
    output['tce2_ui/backgrounds/catalog.json']=(json.dumps(manifest,indent=2)+'\n').encode()
    output['tce2_ui/backgrounds/runtime.cfg']=MARKER
    output['scripts/tce2_backgrounds.shader']='\n'.join(shader).encode('ascii')
    if sum(len(v) for v in output.values()) > 104*1024*1024:
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
    p.add_argument('--source',type=Path,default=ROOT/'map_pics')
    p.add_argument('--out',type=Path,default=ROOT/'ui-modern/assets')
    p.add_argument('--check',action='store_true');a=p.parse_args()
    manifest=build(a.source,a.out,a.check)
    print(json.dumps({'backgrounds':len(manifest['files']),'checked':a.check,
                      'bytes':sum(v['bytes'] for v in manifest['files'])}))


if __name__=='__main__': main()
