"""Independent checks for ET 2.60b-readable artwork and pre-rendered fonts."""
import hashlib
import json
import math
from pathlib import PurePosixPath
import struct
from functools import lru_cache

FONT_SETS = {('BarlowCondensed-SemiBold',16),('BarlowCondensed-SemiBold',27),
             ('Barlow-Regular',21),('Barlow-Regular',24),
             ('Barlow-SemiBold',28),('Barlow-SemiBold',30)}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def tga_info(data, require_portable=True):
    """Validate the actual native image header and complete pixel payload."""
    if len(data) < 18:
        raise ValueError('Truncated TGA header')
    header = struct.unpack_from('<BBBHHBHHHHBB',data)
    ident, mapped, kind, cm_start, cm_length, cm_bits, ox, oy, w, h, bits, attr = header
    if (ident or mapped or kind != 2 or cm_start or cm_length or cm_bits or ox or oy
            or bits not in (24,32) or not w or not h or w > 4096 or h > 4096):
        raise ValueError('TGA uses unsupported ET image encoding/dimensions')
    if require_portable and ((w & (w-1)) or (h & (h-1)) or attr != (8 if bits == 32 else 0)):
        raise ValueError('TGA must be power-of-two, bottom-origin with native alpha')
    channels = bits//8
    end = 18+w*h*channels
    if len(data) < end:
        raise ValueError('Truncated TGA pixels')
    return w,h,channels,attr


@lru_cache(maxsize=16)
def tga_pixels(data, require_portable=True):
    """Decode actual uncompressed disk rows as both ET and ETL would load them."""
    w,h,channels,attr = tga_info(data,require_portable)
    # Classic ET always assumes bottom-origin. ETL honours bit5; reject it above.
    rows = []
    for y in range(h-1,-1,-1):
        row = data[18+y*w*channels:18+(y+1)*w*channels]
        if channels == 4:
            rows.append(bytes(v for i in range(0,len(row),4)
                              for v in (row[i+2],row[i+1],row[i],row[i+3])))
        else:
            rows.append(bytes(v for i in range(0,len(row),3)
                              for v in (row[i+2],row[i+1],row[i],255)))
    if not require_portable and attr & 0x20:
        rows.reverse()
    return w,h,b''.join(rows)


def font_table(data):
    if len(data) != 20548:
        raise ValueError('ET fontInfo_t DAT must contain exactly20548bytes')
    rows = [struct.unpack_from('<7i4fi32s',data,n*80) for n in range(256)]
    scale,name = struct.unpack_from('<f64s',data,20480)
    if not math.isfinite(scale) or not 0 < scale <= 8 or b'\0' not in name:
        raise ValueError('Invalid ET font scale/name')
    for row in rows:
        height,top,bottom,bearing,advance,width,imageheight,s,t,s2,t2,handle,shader = row
        if (not all(math.isfinite(x) for x in (s,t,s2,t2)) or
                not (0 <= s <= s2 <= 1 and 0 <= t <= t2 <= 1) or
                not (0 <= height <= 256 and height == imageheight and 0 <= width <= 256
                     and -256 <= bearing <= 256 and 0 <= advance <= 256) or
                handle != 0 or b'\0' not in shader):
            raise ValueError('Invalid ET glyph metric/UV/shader')
    return rows,scale,name.split(b'\0',1)[0].decode('ascii')


def verify(assets, require_fonts=True):
    """Check real packaged bytes, independent of provenance assertions alone."""
    tgas = {}
    for path,data in assets.items():
        if path.endswith('.tga'):
            tgas[path] = tga_info(data)[:2]
    if not require_fonts:
        return {'tga_images':len(tgas),'font_sets':0}
    try:
        meta = json.loads(assets['fonts/tce2_ui/PORTABLE.json'])
        variants = meta['variants']
    except (KeyError,ValueError,TypeError) as error:
        raise ValueError('Missing/invalid portable ET font manifest') from error
    if (meta.get('schema') != 1 or meta.get('dpi') != 72 or meta.get('atlas_size') != 512 or
            meta.get('raster_mode') != 'FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_NORMAL' or
            meta.get('outline_embolden_26_6') != 24 or
            len(variants) != 6 or {(r['font'],r['point_size']) for r in variants} != FONT_SETS):
        raise ValueError('Wrong portable ET font coverage')
    owned = set()
    for variant in variants:
        name,point = variant['font'],variant['point_size']
        dat = 'fonts/tce2_ui/'+name+'_'+str(point)+'.dat'
        if variant['dat'] != dat or dat not in assets:
            raise ValueError('Missing/wrong portable font data: '+dat)
        glyphs,scale,disk_name = font_table(assets[dat])
        if disk_name != dat or abs(scale-48/point) > 1e-6:
            raise ValueError('ET font name/48pt scale differs: '+dat)
        referenced = set()
        for row in glyphs:
            try:
                shader = row[12].split(b'\0',1)[0].decode('ascii')
            except UnicodeDecodeError as error:
                raise ValueError('Non-ASCII ET font shader path') from error
            if (len(shader) >= 32 or not shader.startswith('fonts/tce2_ui/') or
                    '..' in PurePosixPath(shader).parts or '\\' in shader or
                    shader+'.tga' not in tgas):
                raise ValueError('Missing/unsafe ET glyph atlas: '+shader)
            atlas = shader+'.tga'
            referenced.add(atlas)
            w,h = tgas[atlas]
            if (w,h) != (512,512) or abs((row[9]-row[7])*w-row[5]) > 1e-5 or abs((row[10]-row[8])*h-row[6]) > 1e-5:
                raise ValueError('ET glyph UV does not match its raster dimensions')
        if referenced != set(variant['atlases']):
            raise ValueError('ET glyph atlas closure differs: '+dat)
        # This catches an empty raster masquerading as a valid metric table.
        for letter in 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789ÄÖÜäöüßé':
            glyph = glyphs[ord(letter)]
            w,h,pixels = tga_pixels(assets[glyph[12].split(b'\0',1)[0].decode() + '.tga'])
            x,y = round(glyph[7]*w),round(glyph[8]*h)
            if not any(any(pixels[((y+row)*w+x)*4+3:((y+row)*w+x+glyph[5])*4:4])
                       for row in range(glyph[6])):
                raise ValueError('Missing Latin glyph pixels: '+dat+' '+letter)
        owned.update(referenced)
        owned.add(dat)
    actual_font_rasters={path for path in assets if path.startswith('fonts/tce2_ui/') and path.endswith(('.dat','.tga'))}
    if actual_font_rasters != owned:
        raise ValueError('Orphan/stale portable ET font data or atlas')
    inventory = meta.get('files',[])
    if len(inventory) != len(owned) or {row.get('path') for row in inventory} != owned:
        raise ValueError('Portable ET font inventory differs')
    for row in inventory:
        data = assets[row['path']]
        if row.get('sha256') != sha(data) or row.get('bytes') != len(data):
            raise ValueError('Portable ET font provenance differs: '+row['path'])
    for name,digest in meta.get('ttf_sha256',{}).items():
        if sha(assets['fonts/tce2_ui/'+name]) != digest:
            raise ValueError('Portable font has stale source TTF')
    return {'tga_images':len(tgas),'font_sets':6,'glyphs':1536,
            'atlas_images':len(owned)-6,'format_bytes':20548,'bottom_origin':True}
