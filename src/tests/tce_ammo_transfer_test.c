#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
extern int AddToClip(playerState_t *,int,int,qboolean);
static gentity_t ent;static gclient_t client;static byte rawEnt[0x57c],raw[0x2000];
int main(int argc,char**argv){HMODULE dll;byte*b;int(__cdecl*clip)(void*,int,int,int);void(__cdecl*fill)(void*,int);int(__cdecl*add)(void*,int,int,int);int mode,k,j,w,n=0;int values[]={-30,-1,0,1,29,30,998,999,1000};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;clip=(void*)(b+0x5c540);fill=(void*)(b+0x5c5f0);add=(void*)(b+0x5c610);ent.client=&client;*(void**)(rawEnt+0x190)=raw;
 for(mode=0;mode<3;++mode)for(k=0;k<64000;++k){int x=0,y=0,count=values[(k/64)%9],reserve=(k/576)%2;
  w=k%64;memset(&client,0x5a,sizeof(client));memset(raw,0x5a,sizeof(raw));
  for(j=0;j<64;++j){client.ps.ammo[j]=(k+j)%71-5;client.ps.ammoclip[j]=(k*3+j)%41-5;weaponDef[j].maxclip=(k+j)%35;weaponDef[j].maxammo=(k+j)%101;}
  memcpy(b+0x24435c0,weaponDef,sizeof(weaponDef));memcpy(raw+0x190,client.ps.ammo,256);memcpy(raw+0x290,client.ps.ammoclip,256);memcpy(raw+0x3d4,client.ps.weapons,8);
  if(mode==0){x=clip(raw,w,count,reserve);y=AddToClip(&client.ps,w,count,reserve);}
  else if(mode==1){fill(raw,w);Fill_Clip(&client.ps,w);}
  else{x=add(rawEnt,w,count,reserve);y=Add_Ammo(&ent,w,count,reserve);}
  if(x!=y||memcmp(raw+0x190,client.ps.ammo,256)||memcmp(raw+0x290,client.ps.ammoclip,256)||memcmp(raw+0x3d4,client.ps.weapons,8)){fprintf(stderr,"ammo transfer mode%d case%d w%d count%d reserve%d mismatch\n",mode,k,w,count,reserve);return 1;}++n;
 }
 printf("%d whole AddToClip/FillClip/AddAmmo original comparisons passed\n",n);return 0;
}
