#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_event.c"
#include <windows.h>
#include <stdio.h>
static char message[256];static int calls,type,shader;
static void popup(int t,const char*s,int h){++calls;type=t;shader=h;Q_strncpyz(message,s,sizeof(message));}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b,*item;DWORD old,tmp;int w,t,k,n=0;void(__cdecl*original)(int);const char*names[]={"Health","Ammo","armor","A",""};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x3b140);item=b+0x98f58+60;VirtualProtect(item,60,PAGE_READWRITE,&old);
 hook(b+0x57970,popup);hook(CG_AddPMItemBig,popup);
 for(w=0;w<64;++w)for(t=1;t<=8;++t)for(k=0;k<5;++k){char expected[256];int et,es,ec,ew,etime;
  *(int*)(item+36)=bg_itemlist[1].giType=t;*(int*)(item+40)=bg_itemlist[1].giTag=w;*(char**)(item+28)=bg_itemlist[1].pickup_name=(char*)names[k];*(char**)(item+16)=bg_itemlist[1].world_model[2]=NULL;
  strcpy((char*)(b+0x484af6c+w*0xfd0),names[k]);strcpy(tce_cg_weapons[w].deployMenuShortName,names[k]);
  cg.time=*(int*)(b+0x407dea4)=1000+w;cgs.media.pmImages[PM_MESSAGE]=*(int*)(b+0x259d19c)=713;
  cg.weaponSelect=*(int*)(b+0x407ef70)=29;cg.weaponSelectTime=*(int*)(b+0x4080e44)=-99;
  calls=0;original(1);strcpy(expected,message);ec=calls;et=type;es=shader;ew=*(int*)(b+0x407ef70);etime=*(int*)(b+0x4080e44);
  calls=0;CG_ItemPickup(1);if(calls!=ec||type!=et||shader!=es||strcmp(message,expected)||cg.weaponSelect!=ew||cg.weaponSelectTime!=etime){printf("pickupEvent w%d t%d name%d text%s/%s shader%d/%d\n",w,t,k,message,expected,shader,es);return 1;}++n;
 }
 VirtualProtect(item,60,old,&tmp);printf("%d whole item-pickup event comparisons passed\n",n);return 0;
}
