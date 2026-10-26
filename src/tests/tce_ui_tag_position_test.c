#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_players.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
static orientation_t tag;static const refEntity_t*expectedParent;static int calls;static const char*expectedTag;
static int lerp(orientation_t*out,const refEntity_t*parent,const char*name,int start){assert(parent==expectedParent);assert(!strcmp(name,expectedTag));assert(start==0);memcpy(out,&tag,sizeof(tag));++calls;return 1;}
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;int mode,n,i,j,cases=0;refEntity_t parent,a,c;assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;assert(offsetof(refEntity_t,origin)==0x68&&offsetof(refEntity_t,axis)==0x1c&&offsetof(refEntity_t,backlerp)==0xa0);patch(b+0x20910,lerp);patch(trap_CM_LerpTag,lerp);
for(mode=0;mode<2;++mode)for(n=0;n<1000;++n){memset(&parent,0,sizeof(parent));memset(&a,0x35,sizeof(a));for(i=0;i<3;++i){parent.origin[i]=((n+i*7)%33-16)*.125f;tag.origin[i]=((n*3+i)%17-8)*.25f;for(j=0;j<3;++j){parent.axis[i][j]=((n+i+j*3)%9-4)*.25f;tag.axis[i][j]=((n*7+i*2+j)%9-4)*.25f;a.axis[i][j]=((n*2+i+j)%9-4)*.25f;}}if(n>=500){vec3_t angles;angles[0]=(n%79)*.7f;angles[1]=(n%173)*1.1f;angles[2]=(n%53)*.3f;AnglesToAxis(angles,parent.axis);angles[0]*=.37f;angles[1]*=-.63f;angles[2]+=.19f;AnglesToAxis(angles,tag.axis);angles[0]+=.91f;angles[1]*=.43f;AnglesToAxis(angles,a.axis);for(i=0;i<3;++i){parent.origin[i]=(n-750)*.13f+i*.71f;tag.origin[i]=(n%31-15)*.17f+i*.29f;}}parent.backlerp=(n%9)*.125f;c=a;expectedParent=&parent;expectedTag=n%2?"tag_weapon":"tag_head";calls=0;((void(__cdecl*)(refEntity_t*,const refEntity_t*,int,char*))(b+(mode?0x13270:0x131d0)))(&a,&parent,99,(char*)expectedTag);assert(calls==1);calls=0;if(mode)UI_PositionRotatedEntityOnTag(&c,&parent,99,(char*)expectedTag);else UI_PositionEntityOnTag(&c,&parent,99,(char*)expectedTag);assert(calls==1);if(memcmp(&a,&c,sizeof(a))){fprintf(stderr,"UI tag mismatch mode%d case%d\n",mode,n);return 1;}++cases;}
printf("%d original UI tag-position comparisons passed\n",cases);FreeLibrary(h);return 0;}
