#include "../cgame/cg_local.h"
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>

static orientation_t tag;
static const refEntity_t *activeParent;
static const char *activeName;
static int activeIndex,returnValue,calls,cases[2];
static void require(int ok,const char *what){if(!ok){fprintf(stderr,"Tag cases %d/%d: %s\n",cases[0],cases[1],what);exit(1);}}
int trap_R_LerpTag(orientation_t *out,const refEntity_t *parent,const char *name,int startIndex){
    require(parent==activeParent,"parent passed to renderer");require(name==activeName,"tag pointer");require(startIndex==activeIndex,"startIndex");
    *out=tag;++calls;return returnValue;
}
static unsigned rng=0x238451ac;
static float sample(void){rng=rng*1664525u+1013904223u;return ((int)(rng>>16)-32768)/4096.0f;}
static void matrix(vec3_t axis[3],int mode){
    int i,j;vec3_t angles;
    if(mode<3){AxisClear(axis);if(mode==1)axis[0][0]=-1;if(mode==2){axis[0][0]=2;axis[1][1]=0.25f;axis[2][2]=-3;}}
    else if(mode==3){VectorSet(angles,17,73,-28);AnglesToAxis(angles,axis);}
    else for(i=0;i<3;++i)for(j=0;j<3;++j)axis[i][j]=sample();
}
static void compare(unsigned char *base,int rotated,int parentMode,int tagMode,int entityMode,int offsetMode,int index,int result){
    refEntity_t actual,expected,parent,savedParent,untouched;
    orientation_t savedTag;
    vec3_t offset,savedOffset;int i,j;
    void (__cdecl *originalPlain)(void *,void *,const char *,int,void *)=(void *)(base+0x2de20);
    void (__cdecl *originalRotated)(void *,void *,const char *)=(void *)(base+0x2dee0);
    memset(&actual,0x5a,sizeof(actual));memset(&parent,0x35,sizeof(parent));
    for(i=0;i<3;++i){parent.origin[i]=sample()*100;actual.origin[i]=sample();tag.origin[i]=sample();offset[i]=offsetMode==1?0:sample();}
    matrix(parent.axis,parentMode);matrix(tag.axis,tagMode);matrix(actual.axis,entityMode);
    expected=actual;untouched=actual;savedParent=parent;savedTag=tag;VectorCopy(offset,savedOffset);
    activeParent=&parent;activeName=cases[rotated]&1?"tag_flash":"tag_weapon";activeIndex=rotated?0:index;returnValue=result;calls=0;
    if(rotated)originalRotated(&expected,&parent,activeName);else originalPlain(&expected,&parent,activeName,index,offsetMode?offset:NULL);
    require(calls==1,"original LerpTag count");calls=0;
    if(rotated)CG_PositionRotatedEntityOnTag(&actual,&parent,activeName);else CG_PositionEntityOnTag(&actual,&parent,activeName,index,offsetMode?&offset:NULL);
    require(calls==1,"port LerpTag count");
    for(i=0;i<3;++i){
        require(fabs(actual.origin[i]-expected.origin[i])<=0.0001+fabs(expected.origin[i])*0.000002,"position");
        untouched.origin[i]=actual.origin[i];expected.origin[i]=actual.origin[i];
        for(j=0;j<3;++j){require(fabs(actual.axis[i][j]-expected.axis[i][j])<=0.0001+fabs(expected.axis[i][j])*0.000002,"orientation");
            untouched.axis[i][j]=actual.axis[i][j];expected.axis[i][j]=actual.axis[i][j];}
    }
    require(!memcmp(&actual,&untouched,sizeof(actual))&&!memcmp(&actual,&expected,sizeof(actual)),"unrelated render fields");
    require(!memcmp(&parent,&savedParent,sizeof(parent))&&!memcmp(&tag,&savedTag,sizeof(tag))&&!memcmp(offset,savedOffset,sizeof(offset)),"input mutation");++cases[rotated];
}
int main(int argc,char **argv){
    HMODULE dll;unsigned char *base,*entry,saved[5];DWORD p,u;
    const int indices[]={-1,0,1,17,2147483647},returns[]={-1,0,1,17};int a,b,c,d,e,f,g,i;
    require(argc==2,"DLL argument");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load DLL");base=(unsigned char *)dll;entry=base+0x65510;
    memcpy(saved,entry,5);require(VirtualProtect(entry,5,PAGE_EXECUTE_READWRITE,&p),"hook protection");
    entry[0]=0xe9;*(int *)(entry+1)=(unsigned char *)trap_R_LerpTag-entry-5;FlushInstructionCache(GetCurrentProcess(),entry,5);VirtualProtect(entry,5,p,&u);
    for(a=0;a<2;++a)for(b=0;b<5;++b)for(c=0;c<5;++c)for(d=0;d<5;++d)for(e=0;e<3;++e)for(f=0;f<5;++f)for(g=0;g<4;++g)
        compare(base,a,b,c,d,e,indices[f],returns[g]);
    for(a=0;a<2;++a)for(i=0;i<32768;++i)compare(base,a,4,4,4,i%3,indices[i%5],returns[i%4]);
    require(VirtualProtect(entry,5,PAGE_EXECUTE_READWRITE,&p),"unhook protection");memcpy(entry,saved,5);FlushInstructionCache(GetCurrentProcess(),entry,5);VirtualProtect(entry,5,p,&u);FreeLibrary(dll);
    printf("%d plain-tag and %d rotated-tag original comparisons passed.\n",cases[0],cases[1]);return 0;
}
