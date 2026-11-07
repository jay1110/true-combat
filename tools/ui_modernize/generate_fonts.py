#!/usr/bin/env python3
"""Bake Barlow into the original ET 2.60b fontInfo_t/TGA format.

Authoring only: install requirements-fonts.txt into a private Python environment,
then run this script (or add that environment using --freetype-python). The game
loads the committed DAT/TGA assets and needs neither Python nor FreeType.

The ABI is documented by id-Software/Enemy-Territory src/renderer/tr_font.c and
src/game/q_shared.h: 256 * (7 int32, 4 float32, handle32, shaderName[32]), followed
by glyphScale float32 and name[64]. No engine source is imported into the mod.
"""
import argparse
import ctypes
import hashlib
import importlib.metadata
import json
from pathlib import Path
import struct
import sys

from generate_assets import tga

ROOT = Path(__file__).resolve().parents[2]
DEST = ROOT/'build/ui-fonts'
ASSETS = ROOT/'pak'
GLYPH = struct.Struct('<7i4fi32s')
FONT_BYTES = GLYPH.size*256+4+64
PAGE = 512
DPI = 72  # original renderer density avoids undersampling small no-mipmap text
EMBOLDEN = 24  # 26.6 units:0.375pixel, only in the classic pre-rendered fallback
VARIANTS = (
    ('BarlowCondensed-SemiBold', 16, 'c16'),
    ('BarlowCondensed-SemiBold', 27, 'c27'),
    ('Barlow-Regular', 21, 'r21'),
    ('Barlow-Regular', 24, 'r24'),
    ('Barlow-SemiBold', 28, 's28'),
    ('Barlow-SemiBold', 30, 's30'),
)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def font_data(glyphs, point_size, name):
    """Serialize fixed-width little-endian disk fields, never Python/C pointers."""
    if len(glyphs) != 256 or len(name.encode('ascii')) >= 64:
        raise ValueError('Invalid ET font glyph count/name')
    data = b''.join(GLYPH.pack(*g) for g in glyphs)
    data += struct.pack('<f64s', 72/DPI*48/point_size, name.encode('ascii'))
    assert len(data) == FONT_BYTES == 20548
    return data


def raster(ft, face, code):
    # Classic UI uses grayscale alpha and GL_LINEAR without mipmaps. A2x/LCD
    # bake loses thin stems at small resolutions; normal hinting at72DPI keeps
    # them continuous. The scalable ETLegacy TTFs remain entirely unchanged.
    face.load_char(code, ft.FT_LOAD_DEFAULT | ft.FT_LOAD_FORCE_AUTOHINT | ft.FT_LOAD_TARGET_NORMAL)
    slot = face.glyph
    m = slot.metrics
    advance = (m.horiAdvance >> 6)+1
    outline = slot.outline._FT_Outline
    error = ft.FT_Outline_Embolden(ctypes.byref(outline),EMBOLDEN)
    if error:
        raise RuntimeError('FreeType outline embolden error '+str(error))
    # Emboldening changes the outline, not its advance. Measure the new bounds
    # so the added ink is never cut off by stale pre-embolden glyph metrics.
    bounds = slot.outline.get_cbox()
    left = (bounds.xMin-1) & -64
    right = (bounds.xMax+64) & -64
    top = (bounds.yMax+64) & -64
    bottom = (bounds.yMin-1) & -64
    width, height = (right-left) >> 6, (top-bottom) >> 6
    pitch = (width+3) & -4
    buffer = (ctypes.c_ubyte*(pitch*height))()
    bitmap = ft.FT_Bitmap()
    bitmap.width, bitmap.rows, bitmap.pitch = width, height, pitch
    bitmap.buffer = ctypes.cast(buffer, ctypes.POINTER(ctypes.c_ubyte))
    bitmap.pixel_mode, bitmap.num_grays = ft.FT_PIXEL_MODE_GRAY, 256
    ft.FT_Outline_Translate(ctypes.byref(outline), -left, -bottom)
    error = ft.FT_Outline_Get_Bitmap(ft.get_handle(), ctypes.byref(outline), ctypes.byref(bitmap))
    if error:
        raise RuntimeError('FreeType outline error '+str(error))
    # ET's UI uses glyph.pitch as horizontal bearing, not bitmap row stride.
    return [height, top >> 6, bottom, left >> 6, advance, pitch, height], bytes(buffer)


def bake(ft, name, points, short):
    face = ft.Face(str(ASSETS/'fonts/tce2_ui'/(name+'.ttf')))
    face.set_char_size(points*64, points*64, DPI, DPI)
    rendered = [raster(ft, face, code) for code in range(256)]
    line_height = max(g[0] for g, _ in rendered)
    pages = [bytearray(PAGE*PAGE*4)]
    glyphs = []
    x = y = 1
    for metrics, bitmap in rendered:
        height, top, bottom, bearing, advance, width, _ = metrics
        if x+width+1 >= PAGE:
            x, y = 1, y+line_height+2
        if y+line_height+1 >= PAGE:
            pages.append(bytearray(PAGE*PAGE*4))
            x = y = 1
        shader = 'fonts/tce2_ui/'+short+'_'+str(len(pages)-1)
        if len(shader.encode('ascii')) >= 32:
            raise ValueError('ET glyph shaderName[32] overflow')
        for row in range(height):
            at = ((y+row)*PAGE+x)*4
            pixels = bytes(v for alpha in bitmap[row*width:(row+1)*width]
                           for v in (255,255,255,alpha))
            pages[-1][at:at+len(pixels)] = pixels
        glyphs.append((*metrics, x/PAGE, y/PAGE, (x+width)/PAGE,
                       (y+height)/PAGE, 0, shader.encode('ascii')))
        x += width+2
    dat = 'fonts/tce2_ui/'+name+'_'+str(points)+'.dat'
    files = {dat:font_data(glyphs, points, dat)}
    atlases = []
    for i, pixels in enumerate(pages):
        path = 'fonts/tce2_ui/'+short+'_'+str(i)+'.tga'
        files[path] = tga(PAGE, PAGE, pixels)
        atlases.append(path)
    return files, {'font':name,'point_size':points,'dat':dat,
                   'atlases':atlases,'glyph_scale':72/DPI*48/points}


def arguments(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--freetype-python',type=Path)
    p.add_argument('--check',action='store_true')
    p.add_argument('--out',type=Path)
    args = p.parse_args(argv)
    args.out=args.out or (ASSETS if args.check else DEST)
    return args


def main():
    args=arguments()
    if args.freetype_python:
        sys.path.insert(0,str(args.freetype_python.resolve()))
    try:
        import freetype as ft
    except ImportError as error:
        raise SystemExit('Install authoring-only requirements-fonts.txt before baking.') from error
    if importlib.metadata.version('freetype-py') != '2.5.1' or ft.version() != (2,13,2):
        raise SystemExit('Use pinned freetype-py2.5.1 / FreeType2.13.2 for reproducible assets.')
    files, variants = {}, []
    for name, points, short in VARIANTS:
        baked, row = bake(ft,name,points,short)
        files.update(baked)
        variants.append(row)
    provenance = {'schema':1,'format':'ET 2.60b fontInfo_t; 20548-byte little-endian DAT / native RGBA TGA',
                  'source_reference':'https://github.com/id-Software/Enemy-Territory/blob/master/src/renderer/tr_font.c',
                  'generator':'tools/ui_modernize/generate_fonts.py',
                  'generator_sha256':sha(Path(__file__).read_bytes()),
                  'raster_writer':'tools/ui_modernize/generate_assets.py',
                  'raster_writer_sha256':sha(Path(__file__).with_name('generate_assets.py').read_bytes()),
                  'freetype_py':'2.5.1','freetype':'2.13.2','dpi':DPI,'atlas_size':PAGE,
                  'raster_mode':'FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_NORMAL',
                  'outline_embolden_26_6':EMBOLDEN,
                  'advance':'Original hinted horiAdvance+1; embolden does not increase advance',
                  'license':'Derived from the unchanged OFL Barlow TTFs; see LICENSE-Barlow*.txt.',
                  'ttf_sha256':{name+'.ttf':sha((ASSETS/'fonts/tce2_ui'/(name+'.ttf')).read_bytes())
                                for name in sorted({row[0] for row in VARIANTS})},
                  'variants':variants,
                  'files':[{'path':path,'bytes':len(data),'sha256':sha(data)}
                           for path,data in sorted(files.items())],
                  'limits':['Byte glyph repertoire 0..255 follows original ET font ABI; no Unicode extension.',
                            'ET 2.60b registers glyph handles0..254; its historical byte255 limitation remains.',
                            'TTF-capable engines keep using the unchanged scalable fonts.']}
    files['fonts/tce2_ui/PORTABLE.json'] = (json.dumps(provenance,indent=2)+'\n').encode()
    source_path = ASSETS/'fonts/tce2_ui/SOURCE.json'
    source = json.loads(source_path.read_bytes())
    source['portable'] = {'manifest':'fonts/tce2_ui/PORTABLE.json',
                          'usage':'ET 2.60b loads fonts/NAME_POINTSIZE.dat and short-named TGA atlases; no runtime FreeType required.'}
    files['fonts/tce2_ui/SOURCE.json'] = (json.dumps(source,indent=2)+'\n').encode()
    for path,data in sorted(files.items()):
        target = args.out/path
        if args.check:
            if not target.is_file() or target.read_bytes() != data:
                raise SystemExit('Stale portable font: '+path)
        else:
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_bytes(data)
    print(json.dumps({'passed':True,'checked':args.check,'font_sets':len(variants),
                      'files':len(files),'bytes':sum(map(len,files.values()))}))


if __name__ == '__main__':
    main()
