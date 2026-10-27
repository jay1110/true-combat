#include "src/cgame/cg_local.h"
#include <setjmp.h>
#include <stdio.h>

cg_t cg;
markPoly_t cg_activeMarkPolys, *cg_freeMarkPolys, cg_markPolys[MAX_MARK_POLYS];
static jmp_buf failedPool;
static int frees, checks, failures;
static void freeMarkImpl(markPoly_t *mark);
void CG_Error(const char *format, ...) { longjmp(failedPool, 1); }
void CG_FreeMarkPoly(markPoly_t *mark)
{
    if (mark == &cg_activeMarkPolys || ++frees > MAX_MARK_POLYS) { longjmp(failedPool, 1); }
    freeMarkImpl(mark);
}

/* PRODUCTION_FUNCTIONS */

int main(void)
{
    int i, phase; const int times[] = {0, 1, 10000};
    for (phase = 0; phase < 3; ++phase) {
        CG_InitMarkPolys(); frees = 0; ++checks;
        if (setjmp(failedPool)) { ++failures; printf("FAIL mark pool sentinel recycled at time %d\n", times[phase]); continue; }
        for (i = 0; i <= MAX_MARK_POLYS; ++i) {
            markPoly_t *mark = CG_AllocMark(times[phase] + 1000); mark->time = times[phase];
        }
        if (frees != MAX_MARK_POLYS || cg_activeMarkPolys.nextMark == &cg_activeMarkPolys) { ++failures; }
    }
    printf("mark pool: %d checks, %d failures\n", checks, failures); return failures ? 1 : 0;
}
