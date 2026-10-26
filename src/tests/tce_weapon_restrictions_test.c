/* Complete production restriction chain versus original qagame, no hooks. */
#include "../game/g_local.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>

static gclient_t clients[MAX_CLIENTS];
static unsigned char originals[MAX_CLIENTS][0x16b0];
static unsigned int seed=1;
static unsigned int next(void) { return seed=seed*1664525u+1013904223u; }
extern qboolean G_IsHeavyWeapon(weapon_t);
extern int G_TeamCount(gentity_t *, weapon_t);

int main(int argc, char **argv) {
    HMODULE dll;
    unsigned char *base;
    int (__cdecl *heavy)(int), (__cdecl *count)(void *,int), (__cdecl *disabled)(void *,int);
    const int limits[]={(-2147483647-1),-100,-1,0,1,20,33,50,67,99,100,101,2147483647};
    int run,w,j,r,who,n=0;
    if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;
    base=(unsigned char *)dll;heavy=(void *)(base+0x506b0);
    count=(void *)(base+0x506d0);disabled=(void *)(base+0x50790);
    level.clients=clients;gearDef.parsed=1;
    *(void **)(base+0x7feb80)=originals;
    for(w=-128;w<=256;++w)if(heavy(w)!=(int)G_IsHeavyWeapon(w))return 1;
    for(run=0;run<1024;++run) {
        who=next()%64;
        level.numConnectedClients=run%65;
        *(int *)(base+0x7fec0c)=level.numConnectedClients;
        for(j=0;j<64;++j) {
            clients[j].sess.sessionTeam=next()%4;
            clients[j].sess.playerWeapon=next()%82;
            clients[j].sess.latchPlayerWeapon=next()%82;
            g_entities[j].client=&clients[j];
            *(void **)(base+0x226e2a0+j*0x57c+0x190)=originals[j];
            *(int *)(originals[j]+0xb30)=clients[j].sess.sessionTeam;
            *(int *)(originals[j]+0xb44)=clients[j].sess.playerWeapon;
            *(int *)(originals[j]+0xb54)=clients[j].sess.latchPlayerWeapon;
            /* Permutation exercises order and self-exclusion. */
            level.sortedClients[j]=(j*17+run)%64;
            *(int *)(base+0x7fec18+j*4)=level.sortedClients[j];
        }
        for(w=-1;w<=82;++w) {
            if(count(base+0x226e2a0+who*0x57c,w)!=G_TeamCount(&g_entities[who],w)) {
                fprintf(stderr,"Team count mismatch run%d weapon%d\n",run,w);return 1;
            }
            for(r=0;r<sizeof(limits)/sizeof(limits[0]);++r) {
                g_heavyWeaponRestriction.integer=limits[r];
                *(int *)(base+0x7e22cc)=limits[r];
                if(disabled(base+0x226e2a0+who*0x57c,w)!=(int)G_IsWeaponDisabled(&g_entities[who],w)) {
                    fprintf(stderr,"Restriction mismatch run%d weapon%d limit%d\n",run,w,limits[r]);return 1;
                }
                ++n;
            }
        }
    }
    printf("385 heavy-ID,86016 team-count and%d complete production restriction comparisons matched\n",n);
    FreeLibrary(dll);return 0;
}
