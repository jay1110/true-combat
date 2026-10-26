#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <stddef.h>
#include "../game/q_shared.h"
#include "../game/bg_public.h"

void BG_CalculateSpline_r(splinePath_t *, vec3_t, vec3_t, float);
static int cases;
static unsigned seed = 391237;
static float sample(void) {
    seed = seed * 1664525u + 1013904223u;
    return ((int)(seed >> 8) - 8388608) * .00123f;
}
static void require(int ok, const char *why) {
    if (!ok) { fprintf(stderr, "case %d: %s\n", cases, why); exit(1); }
}
int main(int argc, char **argv) {
    HMODULE dll;
    void (__cdecl *original)(splinePath_t *,float *,float *,float);
    splinePath_t path, next, saved, savedNext;
    struct { unsigned before; float values[6]; unsigned after; } actual, expected;
    const float tensions[] = {-2,-.001f,0,.00001f,.1f,.33333334f,.5f,.9f,.99999f,1,1.001f,2};
    int controls, linked, t, variant, alias, i, j;
    require(sizeof(path)==0x394 && offsetof(splinePath_t,next)==0x8c &&
        offsetof(splinePath_t,numControls)==0x1c4 && offsetof(pathCorner_t,origin)==0x40,
        "original spline layout");
    require(argc==2,"DLL path"); dll=LoadLibraryA(argv[1]); require(dll!=NULL,"load original");
    original=(void *)((unsigned char *)dll+0x3cd0);
    for(controls=0;controls<=MAX_SPLINE_CONTROLS;++controls)
    for(linked=0;linked<3;++linked)for(t=0;t<12;++t)
    for(variant=0;variant<256;++variant)for(alias=0;alias<2;++alias) {
        memset(&path,0x35,sizeof(path));memset(&next,0x53,sizeof(next));
        path.numControls=controls;path.next=linked==0?NULL:linked==1?&next:&path;
        for(i=0;i<3;++i){
            path.point.origin[i]=sample();next.point.origin[i]=sample();
            for(j=0;j<MAX_SPLINE_CONTROLS;++j)
                path.controls[j].origin[i]=variant==0?path.point.origin[i]:sample();
        }
        saved=path;savedNext=next;memset(&actual,0x35,sizeof(actual));expected=actual;
        original(&path,expected.values,expected.values+(alias?0:3),tensions[t]);
        require(!memcmp(&path,&saved,sizeof(path))&&!memcmp(&next,&savedNext,sizeof(next)),"original input mutation");
        BG_CalculateSpline_r(&path,actual.values,actual.values+(alias?0:3),tensions[t]);
        require(actual.before==expected.before&&actual.after==expected.after,"output guards");
        for(i=0;i<6;++i) {
            if(!_finite(actual.values[i])||fabs(actual.values[i]-expected.values[i])>
                .0001+fabs(expected.values[i])*.000002) {
                fprintf(stderr,"controls %d link %d tension %g axis %d: %.9g vs %.9g\n",
                    controls,linked,tensions[t],i,actual.values[i],expected.values[i]);
                require(0,"spline result");
            }
        }
        if(!linked)require(!memcmp(&actual,&expected,sizeof(actual)),"missing target preserves output");
        require(!memcmp(&path,&saved,sizeof(path))&&!memcmp(&next,&savedNext,sizeof(next)),"port input mutation");
        ++cases;
    }
    FreeLibrary(dll);printf("%d complete BG_CalculateSpline_r original comparisons passed.\n",cases);
    return 0;
}
