#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_event.c"
#include <windows.h>
#include <stdio.h>
static int n,result,events[8][16];static byte*module;
static void print(const char*f,...){events[n][0]=1;events[n++][1]=strcmp(f,"ELITE PORTAL: CG_StartFootStepSound\n");}
static int volume(const vec3_t org,float base,float range,int flags,const tce_fragmentSoundContext_t*c){int*p=events[n++];p[0]=2;memcpy(p+1,org,12);memcpy(p+4,&base,4);memcpy(p+5,&range,4);p[6]=flags;memcpy(p+7,c->listener,12);memcpy(p+10,&c->attenuation,4);p[11]=c->distanceVariant;p[12]=c->disabled;return result;}
static int originalVolume(const vec3_t org,float base,float range,int flags){tce_fragmentSoundContext_t c;memset(&c,0,sizeof(c));memcpy(c.listener,module+0x407f01c,12);c.attenuation=*(float*)(module+0x40a49cc);c.distanceVariant=*(int*)(module+0x3fdf380);return volume(org,base,range,flags,&c);}
static void sound(vec3_t origin,int entity,int channel,int sfx,int value){int*p=events[n++];p[0]=3;p[1]=origin==NULL;p[2]=entity;p[3]=channel;p[4]=sfx;p[5]=value;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE m;void(__cdecl*f)(void*,entityState_t*,int);entityState_t es,orig;refdef_t ref;int k,count,copy[8][16];
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;module=(byte*)m;f=(void*)(module+0x3b1e0);hook(module+0x44b50,originalVolume);hook(TCE_CG_SoundVolume,volume);hook(module+0x47b10,print);hook(CG_Printf,print);hook(module+0x64fe0,sound);hook(trap_S_StartSoundVControl,sound);cg.refdef_current=&ref;
 for(k=0;k<8192;k++){memset(&es,0,sizeof(es));es.number=k%1024;es.event=((k%4)==0?EV_TCE_FOOTSTEP_SPRINT:(k%4)==1?EV_TCE_FOOTSTEP_WALK:1)|((k%4)<<8);es.pos.trBase[0]=k*.13f;es.pos.trBase[1]=-3.7f;orig=es;orig.event=((k%4)==0?138:(k%4)==1?139:1)|((k%4)<<8);cg_footsteps.integer=*(int*)(module+0x488d5cc)=(k/4)%2;cg.tcePortalScopeRendering=*(int*)(module+0x40a4a1c)=(k/8)%2;result=(k/16)%4-1;ref.vieworg[0]=1.31f;ref.vieworg[1]=-2.71f;ref.vieworg[2]=3.11f;memcpy(module+0x407f01c,ref.vieworg,12);tceFlash.deafness=*(float*)(module+0x40a49cc)=(k%5)*.13f;tceSmokeNewBBox=*(int*)(module+0x3fdf380)=(k/32)%2;
 n=0;memset(events,0,sizeof(events));f((void*)0x1234,&orig,k%31);count=n;memcpy(copy,events,sizeof(copy));n=0;memset(events,0,sizeof(events));CG_StartFootStepSound((void*)0x1234,&es,k%31);if(count!=n||memcmp(copy,events,sizeof(copy))){printf("footstep case%d calls%d/%d\n",k,count,n);return 1;}}
 puts("8192 whole footstep controller comparisons passed");return 0;}
