#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_local.h"
#include <windows.h>
#include <stdio.h>
extern void TCE_UI_PlayerInfo_SetWeapon(playerInfo_t *,int);
static int failMode, calls;
static char events[8192];
static int model(const char *path) {
    unsigned hash=1;const char *p=path;char line[512];int result;
    if(path)while(*p)hash=hash*33+(unsigned char)*p++;
    result=path && !(failMode==1 || (failMode==2 && calls==0) ||
        (failMode==3 && strstr(path,"_flash"))) ? (hash&0x7fffffff)|1 : 0;
    sprintf(line,"%s=%d;",path?path:"<NULL>",result);strcat(events,line);++calls;return result;
}
static void patch(void *address,void *fn) {
    unsigned char *p=address;DWORD old,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);
    p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);
    VirtualProtect(p,5,old,&tmp);
}
int main(int argc,char **argv) {
    HMODULE dll;unsigned char *b,raw[0x6000];playerInfo_t pi;char expected[8192];
    int w,seed,count=0;
    if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)dll;
    patch(b+0x206d0,model);patch(trap_R_RegisterModel,model);
    for(w=-4;w<80;++w)for(failMode=0;failMode<4;++failMode)for(seed=0;seed<3;++seed) {
        memset(raw,seed,sizeof(raw));memset(&pi,seed,sizeof(pi));calls=0;events[0]=0;
        ((void (__cdecl *)(void *,int))(b+0x13c30))(raw,w);strcpy(expected,events);
        calls=0;events[0]=0;TCE_UI_PlayerInfo_SetWeapon(&pi,w);
        if(strcmp(events,expected) || pi.currentWeapon!=*(int *)(raw+0x5804) ||
           pi.realWeapon!=*(int *)(raw+0x5844) || pi.weaponModel!=*(int *)(raw+0x57d0) ||
           pi.barrelModel!=*(int *)(raw+0x57d4) || pi.flashModel!=*(int *)(raw+0x57d8) ||
           memcmp(pi.flashDlightColor,raw+0x57dc,12)) {
            printf("UI preview mismatch weapon%d fail%d seed%d\noriginal:%s\nsource:%s\n",w,failMode,seed,expected,events);return 1;
        }
        ++count;
    }
    printf("%d complete original UI weapon previews passed (model/flash failure and fallback included)\n",count);
    return 0;
}
