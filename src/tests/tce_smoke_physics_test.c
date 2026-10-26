#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_effects.c"
#include <windows.h>
#include <stdio.h>
typedef char smoke_layout[(sizeof(smokesprite_t)==68)?1:-1];
int main(int argc,char**argv){HMODULE dll;byte*b;int(__cdecl*original)(void*,float);int c,j;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x2dc70);
 for(c=0;c<80000;++c){smokesprite_t source,reference;float distance=(float)(c%513-256)*.0625f;int er;memset(&source,0x55,sizeof(source));for(j=0;j<3;++j){source.pos[j]=(float)((c*(j+1))%1001-500)*.125f;source.dir[j]=(float)((c+j*71)%101-50)*.03125f;}source.dist=(float)(c%701-300)*.0625f;source.size=(float)(c%1401-100)*.125f;if(c>=40000){distance=(float)(c%513-256)/37.0f;for(j=0;j<3;++j){source.pos[j]=(float)((c*(j+1))%1001-500)/7.0f;source.dir[j]=(float)((c+j*71)%101-50)/13.0f;}VectorNormalize(source.dir);source.dist=(float)(c%701-300)/11.0f;source.size=(float)(c%1401-100)/9.0f;}reference=source;tceSmokeNewBBox=*(int*)(b+0x3fdf380)=c%4-1;er=original(&reference,distance);if(er!=CG_SmokeSpritePhysics(&source,distance)||memcmp(&reference,&source,sizeof(source))){printf("smoke physics%d size%g/%g dist%g/%g\n",c,reference.size,source.size,reference.dist,source.dist);return 1;}}
 printf("80000 whole smoke-physics comparisons passed\n");return 0;
}

