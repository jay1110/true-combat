#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_effects.c"
#include <windows.h>
#include <stdio.h>
static int idx(void*p,byte*base){return p?(int)(((byte*)p-base)/68):-1;}
static int compare(byte*b){int i;byte*base=b+0x4cf6a0;for(i=0;i<512;++i){byte*r=base+i*68;smokesprite_t*s=&SmokeSprites[i];if(idx(*(void**)r,base)!=idx(s->next,(byte*)SmokeSprites)||idx(*(void**)(r+4),base)!=idx(s->prev,(byte*)SmokeSprites)||memcmp(r+8,(byte*)s+8,60))return 0;}return idx(*(void**)(b+0x537ea0),base)==idx(firstfreesmokesprite,(byte*)SmokeSprites)&&idx(*(void**)(b+0x537ea4),base)==idx(lastusedsmokesprite,(byte*)SmokeSprites)&&*(int*)(b+0x537ea8)==SmokeSpriteCount;}
int main(int argc,char**argv){HMODULE dll;byte*b,*base;void(__cdecl*init)(void);void*(__cdecl*release)(void*);int c,i,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;base=b+0x4cf6a0;init=(void*)(b+0x2dbd0);release=(void*)(b+0x2dc10);
 for(c=0;c<1600;++c){int used=1+c%16,target=c%used;void*r;smokesprite_t*s;memset(base,c&255,512*68);memset(SmokeSprites,c&255,sizeof(SmokeSprites));init();InitSmokeSprites();if(!compare(b)){printf("smoke init%d\n",c);return 1;}++n;
  for(i=0;i<used;++i){byte*p=base+i*68;*(void**)p=i+1<used?p+68:NULL;*(void**)(p+4)=i?p-68:NULL;SmokeSprites[i].next=i+1<used?&SmokeSprites[i+1]:NULL;SmokeSprites[i].prev=i?&SmokeSprites[i-1]:NULL;memset(p+8,0x5a,60);memset((byte*)&SmokeSprites[i]+8,0x5a,60);}
  *(void**)(b+0x537ea0)=base+used*68;firstfreesmokesprite=&SmokeSprites[used];*(void**)(b+0x537ea4)=base+(used-1)*68;lastusedsmokesprite=&SmokeSprites[used-1];*(int*)(b+0x537ea8)=SmokeSpriteCount=used;
  r=release(base+target*68);s=DeAllocSmokeSprite(&SmokeSprites[target]);if(idx(r,base)!=idx(s,(byte*)SmokeSprites)||!compare(b)){printf("smoke free%d\n",c);return 1;}++n;
 }
 printf("%d whole smoke-pool comparisons passed\n",n);return 0;
}
