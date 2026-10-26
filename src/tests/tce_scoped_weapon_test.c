#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/bg_misc.c"
#include <windows.h>
#include <stdio.h>
#include <limits.h>
int main(int argc,char **argv) {
    HMODULE client,server;
    int (__cdecl *cgOriginal)(int),(__cdecl *gOriginal)(int);
    int i,value,count=0;
    unsigned int random=0x91a361b7u;
    const int boundaries[]={INT_MIN,INT_MIN+1,-1,0,41,42,43,44,56,57,58,59,60,63,64,INT_MAX-1,INT_MAX};
    if(argc!=3 || !(client=LoadLibraryA(argv[1])) || !(server=LoadLibraryA(argv[2])))return 2;
    cgOriginal=(void*)((unsigned char*)client+0x6630);
    gOriginal=(void*)((unsigned char*)server+0x2e8e0);
    for(i=0;i<131217;i++) {
        if(i<17)value=boundaries[i];
        else if(i<217)value=i-117;
        else {random=random*1664525u+1013904223u;value=(int)random;}
        if((int)BG_IsScopedWeapon(value)!=cgOriginal(value) || (int)BG_IsScopedWeapon(value)!=gOriginal(value)) {
            printf("scope predicate mismatch weapon%d\n",value);return 1;
        }
        count+=2;
    }
    printf("%d original Windows cgame/qagame BG_IsScopedWeapon comparisons passed\n",count);
    return 0;
}
