#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_draw.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
typedef struct { float x,y,w,h; int shader; } pic_t;
static pic_t pics[4];static int count;static void *entry;
static void draw(float x,float y,float w,float h,int shader) { pic_t *p=&pics[count++];p->x=x;p->y=y;p->w=w;p->h=h;p->shader=shader; }
static int shader(const char *name) { if(!strcmp(name,"sprites/backpackcarrier.tga"))return 12;if(!strcmp(name,"sprites/bombcarrier.tga"))return 11;if(!strcmp(name,"sprites/VIP.tga"))return 13;assert(!strcmp(name,"sprites/hostage.tga"));return 14; }
static void hook(void *at,void *fn) { DWORD old,t;byte *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t); }
__declspec(naked) static void originalBlock(void *ps,float bottom) { __asm {
 push ebp
 mov ebp,[esp+8]
 mov eax,[esp+12]
 sub esp,12
 mov [esp+8],eax
 push esi
 jmp entry
} }
__declspec(naked) static void originalExit(void) { __asm { add esp,12
 pop ebp
 ret
} }
int main(int argc,char **argv) { HMODULE module;byte *b,psraw[0x500];playerState_t ps;pic_t expected[4];int k,n;float bottom;
 if(argc!=2 || !(module=LoadLibraryA(argv[1])))return 2;b=(byte*)module;entry=b+0x28c0b;hook(b+0x28cf1,originalExit);hook(b+0x29ea0,draw);hook(CG_DrawPic,draw);hook(trap_R_RegisterShader,shader);
 *(int*)(b+0x258825c)=11;*(int*)(b+0x2588260)=12;*(int*)(b+0x2588264)=13;*(int*)(b+0x2588268)=14;
 for(k=0;k<192;k++) { memset(&ps,0,sizeof(ps));memset(psraw,0,sizeof(psraw));bottom=k/64==0?480.f:k/64==1?506.67f:560.f;
  ps.stats[8]=*(int*)(psraw+0xf0)=(k&1)?0x100:0;ps.stats[9]=*(int*)(psraw+0xf4)=((k&2)?0x100:0)|((k&4)?0x400:0);
  ps.powerups[PW_REDFLAG]=*(int*)(psraw+0x168)=(k&8)?-1:0;ps.powerups[PW_BLUEFLAG]=*(int*)(psraw+0x16c)=(k&16)?123:0;
  if(k&32){ps.stats[8]|=4;ps.stats[9]|=2;*(int*)(psraw+0xf0)|=4;*(int*)(psraw+0xf4)|=2;}
  count=0;originalBlock(psraw,bottom);n=count;memcpy(expected,pics,sizeof(pics));count=0;CG_TCEDrawCarrierStatus(&ps,bottom);
  if(n!=count || memcmp(expected,pics,n*sizeof(pic_t))){printf("carrier block case%d count%d/%d\n",k,n,count);return 1;}
 } puts("192 original carrier HUD block comparisons passed (not whole PlayerStatus)");return 0;
}
