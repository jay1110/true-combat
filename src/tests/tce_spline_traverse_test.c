#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <stddef.h>
#include "../game/q_shared.h"
#include "../game/bg_public.h"

qboolean BG_TraverseSpline(float *, splinePath_t **);
static int cases;
static void require(int ok,const char *why) {
    if(!ok){fprintf(stderr,"case %d: %s\n",cases,why);exit(1);}
}
int main(int argc,char **argv) {
    HMODULE dll;
    qboolean (__cdecl *original)(float *,splinePath_t **);
    splinePath_t paths[9],saved[9],*actualPath,*expectedPath;
    struct {unsigned before;float value;unsigned after;} actual,expected;
    const float fractions[]={-100,-9,-3.125f,-1.0000001f,-1,-.99999994f,-.001f,
        -.00000001f,-0.0f,0,.00000001f,.5f,.99999994f,1,1.0000001f,1.001f,2,3.125f,9,100};
    const float lengths[]={.001f,.1f,1,1.0000001f,3.125f,17.123f,1000};
    int shape,start,zero,length,t,i,variant; qboolean a,b;
    require(sizeof(splinePath_t)==0x394&&offsetof(splinePath_t,length)==0x388&&
        offsetof(splinePath_t,next)==0x8c&&offsetof(splinePath_t,prev)==0x90,"layout");
    require(argc==2,"DLL path");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load original");
    original=(void *)((unsigned char *)dll+0x3e00);
    for(shape=0;shape<3;++shape)for(start=0;start<9;++start)
    for(zero=-1;zero<9;++zero)for(length=0;length<7;++length)
    for(t=0;t<20;++t)for(variant=0;variant<2;++variant) {
        memset(paths,0,sizeof(paths));
        for(i=0;i<9;++i) {
            paths[i].next=i<8?&paths[i+1]:NULL;
            paths[i].prev=i>0?&paths[i-1]:NULL;
            if(shape==1&&i==4)paths[i].next=NULL;
            if(shape==2&&i==4)paths[i].prev=NULL;
            paths[i].length=i==zero?(variant?-0.0f:0):lengths[variant?(length+i)%7:length];
        }
        memcpy(saved,paths,sizeof(paths));actualPath=expectedPath=&paths[start];
        actual.before=0x12345678;actual.after=0xfedcba98;actual.value=fractions[t];expected=actual;
        b=original(&expected.value,&expectedPath);
        require(!memcmp(paths,saved,sizeof(paths)),"original mutated path graph");
        a=BG_TraverseSpline(&actual.value,&actualPath);
        require(a==b&&actualPath==expectedPath,"return value or final path");
        require(actual.before==expected.before&&actual.after==expected.after,"guards");
        if(!_finite(actual.value)||fabs(actual.value-expected.value)>.000001+fabs(expected.value)*.000001){
            fprintf(stderr,"shape %d start %d zero %d length %d t %.9g: %.9g vs %.9g\n",shape,start,zero,length,fractions[t],actual.value,expected.value);require(0,"fraction");
        }
        require(!memcmp(paths,saved,sizeof(paths)),"port mutated path graph");++cases;
    }
    FreeLibrary(dll);printf("%d complete BG_TraverseSpline original comparisons passed.\n",cases);return 0;
}
