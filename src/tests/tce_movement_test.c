#include "../game/tce_movement.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static unsigned rng = 0x938517;
static unsigned next(void) { rng = rng*1664525u+1013904223u; return rng >> 8; }
static void integer(unsigned char *p, int off, int value) { memcpy(p+off,&value,4); }
static void real(unsigned char *p, int off, float value) { memcpy(p+off,&value,4); }

int main(int argc, char **argv) {
    HMODULE module;
    unsigned char *base, ps[0x500], move[0x140], ext[0x90], cmd[32];
    tce_moveScale_t state, saved;
    int i;
    float (__cdecl *original)(void *);
    if (argc != 2 || !(module=LoadLibraryA(argv[1]))) return 2;
    base=(unsigned char *)module; original=(void *)(base+0xbc40);
    for (i=0; i<100000; ++i) {
        float actual, expected;
        memset(ps,0,sizeof(ps)); memset(move,0,sizeof(move));
        memset(ext,0,sizeof(ext)); memset(cmd,0,sizeof(cmd)); memset(&state,0,sizeof(state));
        state.forward=(int)(next()%256)-128; state.right=(int)(next()%256)-128; state.up=(int)(next()%256)-128;
        if (i%7==0) state.up=0;
        if (i%11==0) state.forward=state.right=state.up=0;
        state.commandButtons=next()%256; state.moveButtons=next()%256;
        state.speed=(int)(next()%700)-20; state.ducked=next()%2; state.ladder=next()%2;
        state.sprintTime=(int)(next()%105)-2; state.noclip=next()%2; state.tactical=next()%2;
        state.weaponWeight=(int)(next()%9)-2; state.carriedWeight=(int)(next()%9)-2;
        state.gametype=(int)(next()%10)-1; state.movespeed=(int)(next()%260)-2;
        state.lean=i%3==0 ? 0 : ((int)(next()%41)-20)*.5f;
        state.runScale=(next()%21)*.1f; state.sprintScale=(next()%21)*.1f; state.crouchScale=(next()%11)*.05f;
        cmd[4]=(unsigned char)state.commandButtons; cmd[20]=(unsigned char)state.forward;
        cmd[21]=(unsigned char)state.right; cmd[22]=(unsigned char)state.up;
        *(void **)move=ps; *(void **)(move+4)=ext; move[16]=(unsigned char)state.moveButtons;
        *(void **)(base+0x4ba9edc)=move;
        integer(base,0x4ba9ecc,state.ladder); integer(base,0x488c18c,state.gametype);
        integer(base,0x48897ec,state.movespeed); integer(ext,16,state.sprintTime);
        integer(ps,0x40,state.speed); integer(ps,0xc,state.ducked);
        integer(ps,4,state.noclip ? 1 : 0); integer(ps,0xf0,state.tactical ? 4 : 0);
        integer(ps,0xa4,8); integer(base,0x4c1df20+8*0x1cc+0x11c,state.weaponWeight);
        integer(ps,0x3b4,state.carriedWeight); real(ps,0x3c,state.lean);
        real(ps,0x404,state.runScale); real(ps,0x408,state.sprintScale); real(ps,0x40c,state.crouchScale);
        saved=state; expected=original(cmd); actual=TCE_PM_CmdScale(&state);
        if (!isfinite(actual) || !isfinite(expected) || fabs(actual-expected)>0.000001+fabs(expected)*0.000001 || memcmp(&state,&saved,sizeof(state))) {
            fprintf(stderr,"movement case %d: %.9g original %.9g\n",i,actual,expected); return 1;
        }
    }
    FreeLibrary(module);
    printf("100000 original movement command-scale comparisons passed.\n");
    return 0;
}
