#!/usr/bin/env python3
"""Compile real weather generation/loop and tracemap lookup from installed Snow data."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import zipfile
from compat_validate import msvc_environment
from movement_regression import function

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out",type=Path,default=ROOT/"build/weather-retry-regression")
    parser.add_argument("--source",type=Path,default=ROOT/"src/cgame/cg_atmospheric.c")
    parser.add_argument("--weather-header",type=Path,default=ROOT/"compat/legacy/game/tce_weather_precision.h",help="Production precision helper or archived helper for exact state comparison")
    parser.add_argument("--arch",choices=["x86","x64"],default="x86")
    parser.add_argument("--sanitize",action="store_true")
    args=parser.parse_args();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    source=args.source.read_text(encoding="utf-8-sig")
    trace=(ROOT/"src/game/bg_tracemap.c").read_text(encoding="utf-8-sig")
    maps={}; tga=None
    for pak in sorted((ROOT/"build/navgen-overkill/tce2").glob("*.pk3")):
        with zipfile.ZipFile(pak) as archive:
            for name in ("obj_snow","obj_village","obj_railhouse"):
                entry="maps/"+name+".bsp"
                if entry not in archive.namelist():continue
                data=archive.read(entry);assert struct.unpack_from("<4sI",data)==(b"IBSP",47)
                offset,size=struct.unpack_from("<II",data,8)
                entities=[dict(re.findall(r'"([^"\n]*)"\s*"([^"\n]*)"',block)) for block in re.findall(r"\{([^{}]*)\}",data[offset:offset+size].decode("latin1"))]
                maps[name]={"pak":pak.name,"sha256":hashlib.sha256(data).hexdigest(),"world":entities[0]}
                if name=="obj_snow":
                    maps[name]["spawn"]=next(e["origin"] for e in entities if e.get("classname")=="team_CTF_bluespawn")
            if "maps/obj_snow_tracemap.tga" in archive.namelist():tga=archive.read("maps/obj_snow_tracemap.tga")
    assert set(maps)=={"obj_snow","obj_village","obj_railhouse"} and tga
    density=int(re.search(r"\bD=(\d+)",maps["obj_snow"]["world"]["atmosphere"])[1])
    tgapath=out/"obj_snow_tracemap.tga";tgapath.write_bytes(tga)
    def vector(value):return "{ "+", ".join(str(float(x))+"f" for x in value.split())+" }"
    config=(f"#define SNOW_DENSITY {density}\nstatic vec2_t snowMapMins="+vector(maps["obj_snow"]["world"]["mapcoordsmins"])+";\nstatic vec2_t snowMapMaxs="+vector(maps["obj_snow"]["world"]["mapcoordsmaxs"])+";\nstatic vec3_t snowSpawn="+vector(maps["obj_snow"]["spawn"])+";\n")
    a=source.index("typedef enum\n{\n\tACT_NOT,");b=source.index("static cg_atmosphericEffect_t cg_atmFx;",a)+len("static cg_atmosphericEffect_t cg_atmFx;")
    c=trace.index("typedef struct tracemap_s");d=trace.index("static vec2_t one_over_mapgrid_factor;",c)+len("static vec2_t one_over_mapgrid_factor;")
    types=source[a:b]+"\n"+trace[c:d]+"\n"+config
    trnames=["BG_TraceMapReadInteger","BG_LoadTraceMap","BG_ClampPointToTracemapExtends","BG_TracemapQueryIndices","BG_GetSkyHeightAtPoint","BG_GetSkyGroundHeightAtPoint","etpro_FinalizeTracemapClamp"]
    trcode="void etpro_FinalizeTracemapClamp(int*,int*);\n"+"\n\n".join(function(trace,n) for n in trnames)
    index=(0 if args.arch=="x86" else 2) if os.name=="nt" else 1
    particle="\n\n".join(function(source,n,index if n=="CG_SnowParticleCheckVisible" else 0) for n in ["CG_SetParticleActive","CG_SnowParticleGenerate","CG_SnowParticleCheckVisible"])
    if "CG_AtmosphericRetryReady(" in source:particle+="\n"+function(source,"CG_AtmosphericRetryReady")
    loop=function(source,"CG_AddAtmosphericEffects",0 if os.name=="nt" and args.arch=="x86" else 1)
    match=re.search(r"for \( count = 0; count < cg_atmFx.numDrops; count\+\+ \)\s*\n\s*cg_atmFx.particles\[count\].nextDropTime[^;]*;",source)
    if not match:raise ValueError("Cannot find production initial deadline loop")
    fixture=Path(__file__).with_name("weather_retry_fixture.c")
    generated=fixture.read_text().replace("/* PRODUCTION_TYPES */",types).replace("/* PRODUCTION_TRACEMAP_FUNCTIONS */",trcode).replace("/* PRODUCTION_PARTICLE_FUNCTIONS */",particle).replace("/* PRODUCTION_WEATHER_LOOP */",loop).replace("/* PRODUCTION_INITIAL_DEADLINES */",match[0])
    generated=generated.replace('#include "src/game/tce_weather_precision.h"','#include "'+args.weather_header.resolve().as_posix()+'"')
    path=out/"weather_retry_fixture.c";path.write_text(generated,encoding="utf-8",newline="\n")
    if os.name=="nt":
        env,compiler=msvc_environment(out,args.arch,"14.44");exe=out/"fixture.exe"
        command=[compiler,"/nologo","/TC","/O2","/W3","/DWIN32","/DCGAMEDLL","/D_CRT_SECURE_NO_WARNINGS","/I"+str(ROOT),"/I"+str(ROOT/"compat/legacy/game"),str(path),str(ROOT/"src/game/q_math.c"),str(ROOT/"src/game/q_shared.c"),"/Fe:"+str(exe),"/Fo:"+str(out)+"\\"]
        if args.sanitize:command.insert(1,"/fsanitize=address")
    else:
        env=None;compiler=shutil.which("clang") or shutil.which("gcc");exe=out/"fixture"
        command=[compiler,"-std=c99","-O2","-DCGAMEDLL","-I"+str(ROOT),"-I"+str(ROOT/"compat/legacy/game"),str(path),str(ROOT/"src/game/q_math.c"),str(ROOT/"src/game/q_shared.c"),"-lm","-o",str(exe)]
        if args.sanitize:command[1:1]=["-fsanitize=address,undefined","-fno-omit-frame-pointer"]
    build=subprocess.run(command,cwd=out,env=env,capture_output=True,text=True);(out/"compile.log").write_text(build.stdout+build.stderr,encoding="utf-8")
    if build.returncode:print(build.stdout+build.stderr);return build.returncode
    run=subprocess.run([str(exe),str(tgapath)],cwd=out,env=env,capture_output=True,text=True,timeout=45)
    report={"source":str(args.source.resolve()),"source_sha256":hashlib.sha256(source.encode()).hexdigest(),"loop_sha256":hashlib.sha256(loop.encode()).hexdigest(),"weather_header_sha256":hashlib.sha256(args.weather_header.read_bytes()).hexdigest(),"arch":args.arch,"sanitize":args.sanitize,"command":command,"stdout":run.stdout,"stderr":run.stderr,"exit_code":run.returncode,"map_inputs":maps,"tracemap_sha256":hashlib.sha256(tga).hexdigest(),"tracemap_no_sky_fraction":sum(tga[18+4*i]==0 for i in range(6,65536))/65530,"limits":["Actual weather loop, Snow generation and tracemap loader/lookup; gust service and renderer callbacks controlled","Real Snow spawn workload plus explicit wholly blocked-sky control; not measured game FPS","Village/Rail BSPs do not enable weather in these assets; general FPS/firelag remain open"]}
    (out/"report.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(run.stdout,end="");print(run.stderr,end="");return run.returncode


if __name__=="__main__":raise SystemExit(main())
