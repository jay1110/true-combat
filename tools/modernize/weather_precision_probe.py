#!/usr/bin/env python3
"""Same-compiler Win32 weather arithmetic attribution and frozen-helper checks."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from compat_validate import msvc_environment
from movement_regression import function

ROOT=Path(__file__).resolve().parents[2]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out",type=Path,default=ROOT/"build/weather-precision-probe")
    p.add_argument("--baseline",type=Path,help="Optional archived production header cross-check; default uses the checked-in frozen body")
    p.add_argument("--configuration",choices=["Debug","Release"],default="Release")
    p.add_argument("--sanitize",action="store_true")
    args=p.parse_args();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    header=ROOT/"compat/legacy/game/tce_weather_precision.h"
    template=Path(__file__).with_suffix(".c")
    source=template.read_text()
    frozen=function(source,"FrozenWeatherOutside")
    if args.baseline:
        archived=function(args.baseline.read_text(encoding="utf-8-sig"),"TCE_FiniteWeatherOutside").replace("TCE_FiniteWeatherOutside(","FrozenWeatherOutside(",1)
        # Ignore indentation only; tokens and arithmetic must remain identical.
        if "".join(archived.split())!="".join(frozen.split()):raise ValueError("Frozen fixture body differs from archived production body")
    path=out/"weather_precision_probe.c";path.write_text(source,encoding="utf-8",newline="\n")
    env,compiler=msvc_environment(out,"x86","14.44");exe=out/"probe.exe"
    command=[compiler,"/nologo","/TC","/O2" if args.configuration=="Release" else "/Od","/fp:strict","/W3","/I"+str(ROOT),str(path),"/Fe:"+str(exe),"/Fo:"+str(out)+"\\"]
    if "TCE_WeatherOutsideFast(" in header.read_text():command.insert(1,"/DTCE_HAVE_WEATHER_FASTPATH")
    if args.sanitize:command.insert(1,"/fsanitize=address")
    built=subprocess.run(command,cwd=out,env=env,capture_output=True,text=True)
    (out/"compile.log").write_text(built.stdout+built.stderr,encoding="utf-8")
    if built.returncode:print(built.stdout+built.stderr);return built.returncode
    run=subprocess.run([str(exe)],cwd=out,env=env,capture_output=True,text=True,timeout=180)
    report={"header_sha256":hashlib.sha256(header.read_bytes()).hexdigest(),"frozen_body_sha256":hashlib.sha256(frozen.encode()).hexdigest(),"archived_header_sha256":hashlib.sha256(args.baseline.read_bytes()).hexdigest() if args.baseline else None,"configuration":args.configuration,"sanitize":args.sanitize,"compiler":compiler,"command":command,"stdout":run.stdout,"stderr":run.stderr,"exit_code":run.returncode,"limits":["Native Win32 same-compiler arithmetic microbenchmark, not game FPS","Fallback timings deliberately select existing approximate C math; not proposed as a parity-safe replacement","Status/control comparisons preserve the previous software helper contract; do not establish original engine FP status parity"]}
    (out/"report.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(run.stdout,end="");print(run.stderr,end="");return run.returncode


if __name__=="__main__":raise SystemExit(main())
