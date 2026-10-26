#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void G_CalcClientAccuracies(void);
static byte rawClients[64*0x16b0];
static gclient_t clients[64];
int main(int argc,char **argv){HMODULE dll;byte *b;void(__cdecl *original)(void);int k,i,j;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x54520);
 level.clients=clients;*(void**)(b+0x7feb80)=rawClients;
 for(k=0;k<2048;++k){
  for(i=0;i<64;++i){byte *r=rawClients+i*0x16b0;g_entities[i].inuse=*(int*)(b+0x226e434+i*0x57c)=(i+k)%3!=0;
   for(j=0;j<WS_MAX;++j){int shots=(k*31+i*7+j*13)%10003,hits=(k*17+i*23+j)%10003;
    if(k%7==0)shots=0;if(k%11==0)hits=0;
    clients[i].sess.aWeaponStats[j].atts=*(int*)(r+0xc14+j*20)=shots;
    clients[i].sess.aWeaponStats[j].hits=*(int*)(r+0xc20+j*20)=hits;
   }
   clients[i].acc=*(float*)(r+0x12ec)=-123.0f;
  }
  original();G_CalcClientAccuracies();
  for(i=0;i<64;++i)if(memcmp(&clients[i].acc,rawClients+i*0x16b0+0x12ec,4)){printf("accuracy mismatch fixture%d client%d %.9g %.9g\n",k,i,clients[i].acc,*(float*)(rawClients+i*0x16b0+0x12ec));return 1;}
 }
 puts("2048 whole G_CalcClientAccuracies calls /131072 client outputs passed, exact float bytes");return 0;
}
