"""Compile the actual shared generation/runtime ground-lifetime predicate."""
import argparse
from pathlib import Path
import subprocess

PRELUDE=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define MAX_CLIENTS 64
#define MAX_GENTITIES 1024
#define ENTITYNUM_WORLD 1022
#define ENTITYNUM_NONE 1023
#define NG_MASK 65537
#define TR_STATIONARY 0
#define Q_stricmp _stricmp
#define SURF_SLICK 2
typedef int qboolean;
typedef struct {
 int inuse,takedamage,health;
 const char *classname;
 struct {int linked,bmodel,contents;} r;
 struct {struct {int trType;} pos,apos;} s;
} gentity_t;
static gentity_t g_entities[MAX_GENTITIES];
static struct {int num_entities;} level={128};
'''
CASES=r'''
int main(void) {
 gentity_t *ent=&g_entities[65];
 assert(TCE_NavGroundKind(ENTITYNUM_WORLD)==1);
 assert(TCE_NavGroundKind(ENTITYNUM_NONE)==0);
 assert(TCE_NavGroundKind(-1)==0);
 assert(TCE_NavGroundKind(0)==0);
 assert(TCE_NavGroundKind(128)==0);
 ent->inuse=ent->r.linked=ent->r.bmodel=1;ent->r.contents=NG_MASK;
 ent->classname="func_static";assert(TCE_NavGroundKind(65)==1);
 ent->classname="func_wall";assert(TCE_NavGroundKind(65)==1);
 ent->takedamage=1;assert(TCE_NavGroundKind(65)==0);
 ent->classname="func_explosive";ent->health=100;assert(TCE_NavGroundKind(65)==2);
 ent->health=0;assert(TCE_NavGroundKind(65)==0);
 ent->health=-10;assert(TCE_NavGroundKind(65)==0);
 ent->health=100;ent->r.linked=0;assert(TCE_NavGroundKind(65)==0);
 ent->r.linked=1;ent->r.contents=0;assert(TCE_NavGroundKind(65)==0);
 ent->r.contents=NG_MASK;ent->s.pos.trType=2;assert(TCE_NavGroundKind(65)==0);
 ent->s.pos.trType=0;ent->s.apos.trType=2;assert(TCE_NavGroundKind(65)==0);
 ent->s.apos.trType=0;ent->r.bmodel=0;assert(TCE_NavGroundKind(65)==0);
 ent->r.bmodel=1;ent->inuse=0;assert(TCE_NavGroundKind(65)==0);
 ent->inuse=1;ent->classname="func_door";assert(TCE_NavGroundKind(65)==0);
 ent->classname="func_train";assert(TCE_NavGroundKind(65)==0);
 ent->classname="script_mover";assert(TCE_NavGroundKind(65)==0);
 ent->classname="func_explosive";ent->takedamage=0;ent->health=0;
 assert(TCE_NavGroundKind(65)==2); /* script-triggered destructible crate */
 assert(NG_OrdinarySurface(0,1.f));
 assert(NG_OrdinarySurface(SURF_SLICK,1.f));
 assert(NG_OrdinarySurface(SURF_SLICK,.9999f));
 assert(!NG_OrdinarySurface(SURF_SLICK,.999f));
 assert(!NG_OrdinarySurface(SURF_SLICK,.7f));
 assert(!NG_OrdinarySurface(0,.69f));
 assert(NG_OrdinarySurface(0,.7f));
 printf("28 native ground lifetime and surface checks passed\n");
 return 0;
}
'''

def main():
    parser=argparse.ArgumentParser();parser.add_argument("--vcvars",type=Path);args=parser.parse_args()
    root=Path(__file__).resolve().parents[2]
    source=(root/"src/game/tce_navgen.c").read_text()
    start=source.index("int TCE_NavGroundKind(")
    body=source[start:source.index("static qboolean NG_StaticGround(",start)]
    start=source.index("static qboolean NG_OrdinarySurface(")
    body+=source[start:source.index("static const char *NG_Ground(",start)]
    directory=root/"build/navgen-overkill/native-ground-unit";directory.mkdir(parents=True,exist_ok=True)
    fixture=directory/"ground.c";fixture.write_text(PRELUDE+body+CASES)
    exe=directory/"ground.exe";script=directory/"run.cmd"
    lines=["@echo off"]
    if args.vcvars:
        lines += [f'call "{args.vcvars.resolve()}" -arch=x64 -host_arch=x64 >nul',"if errorlevel 1 exit /b %errorlevel%"]
    lines += [f'cl /nologo /W4 /WX /O2 /Fe"{exe}" /Fo"{directory/"ground.obj"}" "{fixture}"',
              "if errorlevel 1 exit /b %errorlevel%",f'"{exe}"']
    script.write_text("\n".join(lines)+"\n")
    subprocess.run(["cmd.exe","/d","/c",str(script)],check=True,cwd=directory)

if __name__=="__main__":main()
