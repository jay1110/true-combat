"""One reviewed menu catalog matching the existing native TCE video IDs3..22."""
from pathlib import Path
import re

ROOT=Path(__file__).resolve().parents[2]
SYSTEM=ROOT/'src/assets/ui/options_system.menu'
PAGES=frozenset(('ui/profile_create.menu','ui/profile_create_initial.menu'))
MODES=(
    ('640*480 Standard Screen 4:3',3), ('800*600 Standard Screen 4:3',4),
    ('960*720 Standard Screen 4:3',5), ('1024*768 Standard Screen 4:3',6),
    ('1152*864 Standard Screen 4:3',7), ('1280*1024 Standard Screen 5:4',8),
    ('1600*1200 Standard Screen 4:3',9), ('2048*1536 Standard Screen 4:3',10),
    ('856*480 Wide Screen 16:9',11), ('1024*576 Wide Screen 16:9',12),
    ('1280*720 Wide Screen 16:9',13), ('1360*768 Wide Screen 16:9',14),
    ('1600*900 Wide Screen 16:9',15), ('1920*1080 Wide Screen 16:9',16),
    ('1024*640 Wide Screen 16:10',17), ('1280*800 Wide Screen 16:10',18),
    ('1440*900 Wide Screen 16:10',19), ('1600*1000 Wide Screen 16:10',20),
    ('1680*1050 Wide Screen 16:10',21), ('1920*1200 Wide Screen 16:10',22),
)


def _tokens(value):
    return re.findall(r'"(?:[^"\\]|\\.)*"|[A-Za-z_]\w*|-?\d+|[^\s]',value)


def choices():
    value=resolution(SYSTEM.read_bytes().decode('latin1'))[3][8]
    pairs=tuple((label,int(mode)) for label,mode in re.findall(r'"([^"]+)"\s+(-?\d+)',value))
    reviewed='cvarFloatList { '+' '.join('"%s" %d'%row for row in MODES)+' }'
    if pairs!=MODES or _tokens(value)!=_tokens(reviewed):
        raise ValueError('System/native video catalog differs from reviewed20 mode IDs/labels')
    return value


def resolution(text):
    from restyle import macro_calls
    rows=[row for row in macro_calls(text,{'MULTIACTION'}) if row[3][7].strip('"')=='ui_r_mode']
    if len(rows)!=1:raise ValueError('Expected one ui_r_mode resolution selector')
    return rows[0]


def apply(text,relative):
    if Path(relative).as_posix() not in PAGES:return text
    start,end,name,args=resolution(text)
    if not re.fullmatch(r'cvarFloatList\s*\{.*\}',args[8],re.S):
        raise ValueError('Resolution selector has an unknown catalog')
    if args[8]!=choices():
        args[8]=choices();text=text[:start]+name+'( '+', '.join(args)+' )'+text[end:]
    return text


def validate(text,relative):
    if Path(relative).as_posix() not in PAGES:return
    if _tokens(resolution(text)[3][8])!=_tokens(choices()):
        raise ValueError('Profile/System must use the exact shared video catalog')
