#!/usr/bin/env python3
"""Original native TCE2 UI art: reproducible vectors and frozen AI background.

No external Python dependencies. Raster source is decoded losslessly from PNG;
all packaged art is native TGA, with unique tce2_ui/ shader names.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import zlib

ROOT = Path(__file__).resolve().parents[2]
DEST = ROOT / "ui-modern/assets"
SOURCE = ROOT / "ui-modern/source/background-industrial-v1.png"
S = 2
WHITE = (228, 233, 237, 255)
MUTED = (150, 164, 176, 255)
AMBER = (239, 192, 70, 255)
BORDER = (64, 79, 93, 220)


class Canvas:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.width, self.height = w * S, h * S
        self.data = bytearray(self.width * self.height * 4)

    def rect(self, x, y, w, h, color):
        left, top = max(0, round(x*S)), max(0, round(y*S))
        right, bottom = min(self.width, round((x+w)*S)), min(self.height, round((y+h)*S))
        if left >= right or top >= bottom: return
        row = bytes(color) * (right-left)
        for py in range(top, bottom):
            at = (py*self.width+left)*4
            self.data[at:at+len(row)] = row

    def polygon(self, points, color):
        points = [(x*S,y*S) for x,y in points]
        for y in range(max(0,math.floor(min(p[1] for p in points))),min(self.height,math.ceil(max(p[1] for p in points)))):
            edges=[]
            for (x1,y1),(x2,y2) in zip(points,points[1:]+points[:1]):
                if y1<=y+.5<y2 or y2<=y+.5<y1:edges.append(x1+(y+.5-y1)*(x2-x1)/(y2-y1))
            edges.sort()
            for left,right in zip(edges[::2],edges[1::2]):
                a,b=max(0,math.ceil(left-.5)),min(self.width,math.ceil(right-.5))
                if b>a:
                    at=(y*self.width+a)*4;self.data[at:at+(b-a)*4]=bytes(color)*(b-a)

    def line(self, points, color, width=2):
        for (x1,y1),(x2,y2) in zip(points,points[1:]):
            length=math.hypot(x2-x1,y2-y1)
            if not length:continue
            dx,dy=-(y2-y1)/length*width/2,(x2-x1)/length*width/2
            self.polygon([(x1+dx,y1+dy),(x2+dx,y2+dy),(x2-dx,y2-dy),(x1-dx,y1-dy)],color)
        for x,y in points:self.circle(x,y,width/2,color)

    def circle(self,x,y,r,color):
        self.polygon([(x+r*math.cos(i*math.tau/64),y+r*math.sin(i*math.tau/64)) for i in range(64)],color)

    def gradient(self,top,bottom):
        for y in range(self.h):
            t=y/max(1,self.h-1)
            self.rect(0,y,self.w,1,tuple(round(a+(b-a)*t) for a,b in zip(top,bottom)))

    def finish(self):
        out=bytearray(self.w*self.h*4)
        for y in range(self.h):
            for x in range(self.w):
                total=[0,0,0,0]
                for sy in range(S):
                    for sx in range(S):
                        at=(((y*S+sy)*self.width)+x*S+sx)*4;r,g,b,a=self.data[at:at+4]
                        total[0]+=r*a;total[1]+=g*a;total[2]+=b*a;total[3]+=a
                at=(y*self.w+x)*4
                if total[3]:out[at:at+4]=bytes([round(v/total[3]) for v in total[:3]]+[round(total[3]/S**2)])
        return bytes(out)


def png_read(path):
    data=path.read_bytes();assert data[:8]==b"\x89PNG\r\n\x1a\n"
    at=8;compressed=bytearray();w=h=kind=None
    while at<len(data):
        length=struct.unpack_from(">I",data,at)[0];name=data[at+4:at+8];payload=data[at+8:at+8+length];at+=12+length
        if name==b"IHDR":
            w,h,bits,kind,compression,filtering,interlace=struct.unpack(">IIBBBBB",payload)
            assert bits==8 and kind in (2,6) and not interlace
        elif name==b"IDAT":compressed.extend(payload)
        elif name==b"IEND":break
    channels=3 if kind==2 else 4;stride=w*channels;raw=zlib.decompress(compressed);out=bytearray();previous=bytearray(stride);pos=0
    for _ in range(h):
        f=raw[pos];pos+=1;row=bytearray(raw[pos:pos+stride]);pos+=stride
        for i in range(stride):
            a=row[i-channels] if i>=channels else 0;b=previous[i];c=previous[i-channels] if i>=channels else 0
            if f==1:predict=a
            elif f==2:predict=b
            elif f==3:predict=(a+b)//2
            elif f==4:
                p=a+b-c;da,db,dc=abs(p-a),abs(p-b),abs(p-c);predict=a if da<=db and da<=dc else b if db<=dc else c
            else:assert f==0;predict=0
            row[i]=(row[i]+predict)&255
        if channels==4:out.extend(row)
        else:
            for i in range(0,len(row),3):out.extend(row[i:i+3]);out.append(255)
        previous=row
    return w,h,bytes(out)


def tga(w,h,pixels,opaque=False):
    channels=3 if opaque else 4
    header=struct.pack("<BBBHHBHHHHBB",0,0,2,0,0,0,0,0,w,h,channels*8,0x20 if opaque else 0x28)
    if opaque:body=bytes(v for i in range(0,len(pixels),4) for v in (pixels[i+2],pixels[i+1],pixels[i]))
    else:body=bytes(v for i in range(0,len(pixels),4) for v in (pixels[i+2],pixels[i+1],pixels[i],pixels[i+3]))
    return header+body+bytes(8)+b"TRUEVISION-XFILE.\0"


def png_write(path,w,h,pixels):
    def chunk(name,data):return struct.pack(">I",len(data))+name+data+struct.pack(">I",zlib.crc32(name+data)&0xffffffff)
    rows=b"".join(b"\0"+pixels[y*w*4:(y+1)*w*4] for y in range(h))
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_bytes(b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",w,h,8,6,0,0,0))+chunk(b"IDAT",zlib.compress(rows,9))+chunk(b"IEND",b""))


def outline(c,color=BORDER,width=1):
    c.rect(0,0,c.w,width,color);c.rect(0,c.h-width,c.w,width,color)
    c.rect(0,0,width,c.h,color);c.rect(c.w-width,0,width,c.h,color)


def widget(name):
    dims={"panel":(512,256),"panel_header":(512,64),"card":(512,256),"separator":(256,4),
          "slider_track":(256,16),"slider_thumb":(32,48),"slider_thumb_hover":(32,48),
          "scroll_track":(32,256),"scroll_thumb":(32,64),"scroll_thumb_hover":(32,64)}
    if name.startswith(("check_","radio_")):w,h=64,64
    elif name.startswith("tab"):w,h=256,64
    else:w,h=dims.get(name,(256,64))
    c=Canvas(w,h)
    if name=="panel":
        c.gradient((21,30,41,245),(11,17,24,246));outline(c,(76,89,100,210),1)
        c.rect(1,1,w-2,1,(110,122,133,75))
    elif name=="panel_header":
        c.gradient((29,40,53,248),(20,29,40,240));c.rect(0,h-2,w,2,AMBER)
    elif name=="card":
        c.gradient((25,35,47,228),(15,23,32,233));outline(c,(61,76,89,180),1)
    elif name=="separator":c.rect(0,1,w,1,(112,126,138,100))
    elif name=="focus":
        for x,y,dx,dy in ((0,0,1,1),(w,0,-1,1),(w,h,-1,-1),(0,h,1,-1)):
            c.line([(x+dx*12,y),(x,y),(x,y+dy*12)],AMBER,2)
    elif name.startswith("slider_track"):
        c.rect(0,6,w,4,(11,17,24,244));c.rect(0,6,w,1,(70,85,98,220));c.rect(0,9,w,1,(56,68,81,200))
        for x in range(0,w,32):c.rect(x,12,1,2,(88,100,112,130))
    elif name.startswith("slider_thumb"):
        color=AMBER if name.endswith("hover") else (187,198,207,255)
        c.rect(5,5,w-10,h-10,(12,18,26,255));c.rect(7,5,w-14,h-10,color)
        c.rect(14,12,3,h-24,(54,63,71,240))
    elif name=="scroll_track":c.rect(11,0,10,h,(11,17,24,150));c.rect(15,0,2,h,(57,71,83,160))
    elif name.startswith("scroll_thumb"):
        color=AMBER if name.endswith("hover") else (133,149,161,230)
        c.rect(7,2,w-14,h-4,(24,34,46,245));c.rect(10,3,w-20,h-6,color)
    elif name.startswith("check_"):
        c.rect(8,8,48,48,(16,24,33,248));c.line([(8,8),(56,8),(56,56),(8,56),(8,8)],AMBER if name=="check_on" else (112,130,145,255),3)
        if name=="check_on":c.line([(18,32),(28,42),(47,22)],AMBER,6)
        elif name=="check_mixed":c.rect(19,29,26,6,MUTED)
    elif name.startswith("radio_"):
        c.circle(32,32,25,(110,128,144,240));c.circle(32,32,21,(17,25,34,255))
        if name=="radio_on":c.circle(32,32,12,AMBER)
    elif name.startswith("tab"):
        active=name=="tab_active";hover=name=="tab_hover"
        c.gradient((35,47,61,245) if active or hover else (21,30,42,190),(19,28,39,240) if active else (15,23,32,190))
        c.rect(0,h-3,w,3,AMBER if active else (80,96,109,190));c.rect(w-1,8,1,h-16,(79,94,107,100))
    else:
        disabled=name=="button_disabled";hover=name.endswith(("hover","focus"));pressed=name=="button_pressed"
        c.gradient((17,25,34,180) if disabled else (17,25,34,255) if pressed else (40,55,70,245) if hover else (25,36,49,235),
                   (12,18,25,180) if disabled else (27,37,48,255) if pressed else (26,38,50,248) if hover else (17,25,35,240))
        outline(c,(49,61,72,155) if disabled else (173,149,84,235) if hover else (71,87,101,220),1)
        if hover or pressed:c.rect(0,0,4,h,AMBER if not pressed else (187,142,39,255))
        if name.startswith("selector"):
            c.line([(w-26,h/2-4),(w-20,h/2+2),(w-14,h/2-4)],AMBER if hover else MUTED,2)
    return w,h,c.finish()


def icon(name):
    c=Canvas(64,64);ink=WHITE;w=3.4
    def line(points):c.line(points,ink,w)
    def rect(x,y,a,b):line([(x,y),(x+a,y),(x+a,y+b),(x,y+b),(x,y)])
    if name=="cursor":
        # Native UI draws this32x32 at the exact click coordinate, no offset.
        # The tip therefore touches pixel(0,0); never center a pointer glyph.
        c.polygon([(0,0),(0,48),(12,36),(22,58),(32,53),(22,32),(41,31)],(5,9,14,255))
        c.polygon([(3,7),(3,41),(12,31),(24,53),(28,51),(18,29),(33,28)],WHITE)
    elif name.startswith("chevron_"):
        shapes={"left":[(39,15),(22,32),(39,49)],"right":[(25,15),(42,32),(25,49)],"up":[(15,39),(32,22),(49,39)],"down":[(15,25),(32,42),(49,25)]};line(shapes[name.split('_')[1]])
    elif name=="play":c.polygon([(21,14),(51,32),(21,50)],ink)
    elif name=="close":line([(18,18),(46,46)]);line([(18,46),(46,18)])
    elif name=="check":line([(13,33),(26,46),(51,19)])
    elif name=="multiplayer":
        c.circle(23,23,8,ink);c.circle(44,26,6,ink);line([(9,49),(11,41),(17,36),(29,36),(36,42),(37,49)]);line([(39,38),(48,38),(54,43),(55,49)])
    elif name in ("host","server"):
        for y in (12,29,46):rect(12,y,40,9);c.circle(18,y+4.5,1.5,AMBER)
    elif name=="replays":rect(9,14,46,36);line([(18,14),(18,50)]);line([(46,14),(46,50)]);c.polygon([(27,24),(39,32),(27,40)],ink)
    elif name in ("settings","graphics"):
        if name=="graphics":rect(9,12,46,32);line([(25,52),(39,52)]);line([(32,44),(32,52)])
        else:
            for angle in range(0,360,45):
                t=angle*math.pi/180;line([(32+20*math.cos(t),32+20*math.sin(t)),(32+26*math.cos(t),32+26*math.sin(t))])
            c.circle(32,32,21,ink);c.circle(32,32,16,(0,0,0,0));c.circle(32,32,7,ink)
    elif name=="profile":c.circle(32,22,10,ink);line([(13,53),(15,42),(23,36),(41,36),(49,42),(51,53)])
    elif name in ("credits","info"):
        c.circle(32,32,23,ink);c.circle(32,32,19,(0,0,0,0));c.circle(32,20,2.5,ink);line([(32,29),(32,45)])
    elif name=="exit":rect(12,12,27,40);line([(28,32),(56,32)]);line([(46,22),(56,32),(46,42)])
    elif name=="audio":c.polygon([(11,26),(21,26),(33,16),(33,48),(21,38),(11,38)],ink);line([(42,23),(48,28),(48,36),(42,42)]);line([(48,14),(57,23),(59,32),(57,41),(48,50)])
    elif name=="controls":rect(7,17,50,31);line([(14,26),(22,26)]);line([(30,26),(36,26)]);line([(44,26),(50,26)]);line([(14,38),(22,38)]);line([(29,38),(48,38)])
    elif name=="network":rect(24,7,16,13);line([(32,20),(32,31)]);line([(13,31),(51,31)]);line([(13,31),(13,42)]);line([(51,31),(51,42)]);rect(5,42,16,13);rect(43,42,16,13)
    elif name=="search":c.circle(27,26,17,ink);c.circle(27,26,13,(0,0,0,0));line([(39,39),(54,54)])
    elif name=="refresh":line([(48,18),(38,11),(24,12),(14,22),(12,35),(19,47),(32,52),(45,48)]);line([(48,9),(48,21),(36,21)])
    elif name=="folder":line([(8,18),(24,18),(29,24),(56,24),(53,51),(9,51),(8,18)])
    elif name=="download":line([(32,8),(32,39)]);line([(20,28),(32,40),(44,28)]);line([(11,43),(11,54),(53,54),(53,43)])
    elif name=="lock":rect(15,28,34,28);line([(22,28),(22,18),(27,10),(37,10),(42,18),(42,28)]);c.circle(32,40,3,ink);line([(32,42),(32,48)])
    elif name=="warning":line([(32,8),(58,54),(6,54),(32,8)]);line([(32,24),(32,39)]);c.circle(32,46,2,ink)
    else:raise ValueError(name)
    return 64,64,c.finish()


WIDGETS=("panel","panel_header","card","button","button_hover","button_pressed","button_disabled","tab","tab_hover","tab_active","selector","selector_focus","slider_track","slider_thumb","slider_thumb_hover","scroll_track","scroll_thumb","scroll_thumb_hover","check_off","check_on","check_mixed","radio_off","radio_on","separator","focus")
ICONS=("cursor","play","multiplayer","host","replays","settings","profile","credits","exit","graphics","audio","controls","network","chevron_up","chevron_down","chevron_left","chevron_right","close","check","search","refresh","folder","download","lock","warning","info","server")
ALIASES={"ui/assets/scrollbar.tga":"tce2_ui/scroll_track.tga","ui/assets/scrollbar_arrow_dwn_a.tga":"tce2_ui/icons/chevron_down.tga","ui/assets/scrollbar_arrow_up_a.tga":"tce2_ui/icons/chevron_up.tga","ui/assets/scrollbar_arrow_left.tga":"tce2_ui/icons/chevron_left.tga","ui/assets/scrollbar_arrow_right.tga":"tce2_ui/icons/chevron_right.tga","ui/assets/scrollbar_thumb.tga":"tce2_ui/scroll_thumb.tga","ui/assets/slider2.tga":"tce2_ui/slider_track.tga","ui/assets/sliderbutt_1.tga":"tce2_ui/slider_thumb.tga","ui/assets/check.tga":"tce2_ui/check_on.tga","ui/assets/check_not.tga":"tce2_ui/check_mixed.tga","ui/assets/check_no.tga":"tce2_ui/check_off.tga"}


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--check",action="store_true");parser.add_argument("--preview",type=Path);args=parser.parse_args()
    output={};images={};records=[]
    w,h,pixels=png_read(SOURCE);images["background"]=(w,h,pixels);output["tce2_ui/background.tga"]=tga(w,h,pixels,True)
    for name in WIDGETS:
        w,h,pixels=widget(name);images[name]=(w,h,pixels);output["tce2_ui/"+name+".tga"]=tga(w,h,pixels)
    for name in ICONS:
        key="cursor" if name=="cursor" else "icons/"+name
        w,h,pixels=icon(name);images[key]=(w,h,pixels);output["tce2_ui/"+key+".tga"]=tga(w,h,pixels)
    shader="// Original TCE2 UI. Native alpha blending; forecolor/backcolor should be white.\n"
    for name,(w,h,pixels) in images.items():
        asset="tce2_ui/"+name+".tga"
        shader_name="tce2_ui/cursor" if name=="icons/cursor" else "tce2_ui/"+name
        shader+=f"\n{shader_name}\n{{\n\tnopicmip\n\tnomipmaps\n\tnocompress\n\t{{\n\t\tclampmap {asset}\n\t\tblendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA\n\t\trgbGen vertex\n\t\talphaGen vertex\n\t}}\n}}\n"
        records.append({"path":asset,"shader":shader_name,"width":w,"height":h,"channels":24 if name=="background" else 32,"bytes":len(output[asset]),"sha256":hashlib.sha256(output[asset]).hexdigest()})
    output["scripts/tce2_ui.shader"]=shader.encode()
    source={"schema":1,"origin":"Original code-created interface art and original imagegen industrial background. No game/reference artwork copied.","generator":"tools/ui_modernize/generate_assets.py","generator_sha256":hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),"background_source":"ui-modern/source/background-industrial-v1.png","background_sha256":hashlib.sha256(SOURCE.read_bytes()).hexdigest(),"cursor_hotspot":{"asset":"tce2_ui/cursor.tga","shader":"tce2_ui/cursor","x":0,"y":0,"native_draw":"32x32 at UI cursor position"},"palette":{"panel":"#0e151d","hover":"#24313e","text":"#e4e9ed","muted":"#96a4b0","accent":"#efc046"},"files":records,"native_aliases":ALIASES}
    output["tce2_ui/SOURCE.json"]=(json.dumps(source,indent=2)+"\n").encode()
    for name,data in output.items():
        path=DEST/name
        if args.check:assert path.is_file() and path.read_bytes()==data,name
        else:path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    if args.preview:
        # Contact sheet at source pixel scale, composited on a dark neutral UI.
        width,height=1200,900;board=bytearray(bytes((14,20,28,255))*width*height)
        def paste(name,x,y,maxw=None,maxh=None):
            sw,sh,data=images[name];tw,th=sw,sh
            if maxw and tw>maxw:th=round(th*maxw/tw);tw=maxw
            if maxh and th>maxh:tw=round(tw*maxh/th);th=maxh
            for py in range(th):
                sy=py*sh//th
                for px in range(tw):
                    sx=px*sw//tw;at=(sy*sw+sx)*4;to=((y+py)*width+x+px)*4
                    a=data[at+3]
                    for k in range(3):board[to+k]=(data[at+k]*a+board[to+k]*(255-a)+127)//255
        paste("background",0,0,1200,340)
        for index,name in enumerate(("button","button_hover","button_pressed","button_disabled","tab","tab_active","selector","selector_focus")):
            paste(name,20+index%4*295,360+index//4*82,256,64)
        for index,name in enumerate(("check_off","check_on","check_mixed","radio_off","radio_on")):paste(name,20+index*80,545)
        paste("slider_track",450,560);paste("slider_thumb",560,544);paste("scroll_track",760,530,32,150);paste("scroll_thumb",760,565)
        for index,name in enumerate(ICONS):paste("cursor" if name=="cursor" else "icons/"+name,20+index%14*82,700+index//14*82)
        png_write(args.preview,width,height,board)
    print(json.dumps({"checked":args.check,"assets":len(records),"runtime_files":len(output),"background_dimensions":list(images['background'][:2]),"directory":str(DEST)}))


if __name__=="__main__":main()
