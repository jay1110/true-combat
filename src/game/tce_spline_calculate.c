#include "q_shared.h"
#include "bg_public.h"

#include <stddef.h>
#if defined(_MSC_VER) && defined(_M_IX86)
/* TC cgame30003cd0: retained x87 X, explicit Y/Z difference stores.
 * Original object displacements are relocated to native spline layout. */
enum {
    spline838Origin = offsetof(splinePath_t, point) + offsetof(pathCorner_t, origin),
    spline838Count = offsetof(splinePath_t, numControls),
    spline838Next = offsetof(splinePath_t, next),
    spline838ControlY = offsetof(splinePath_t, controls) + offsetof(pathCorner_t, origin) + sizeof(float),
    spline838ControlStride = sizeof(pathCorner_t)
};
typedef char spline838LocalVectorLayout[(sizeof(vec3_t) == 12 && sizeof(float) == 4) ? 1 : -1];
__declspec(naked) void BG_CalculateSpline_r(splinePath_t *spline, vec3_t out1, vec3_t out2, float tension) {
    __asm {
        SUB esp, 0xe4
        MOV edx, dword ptr [esp + 0xe8]
        PUSH ebx
        PUSH esi
        PUSH edi
        MOV eax, dword ptr [edx + spline838Origin]
        MOV esi, dword ptr [edx + spline838Count]
        MOV ecx, dword ptr [edx + spline838Origin + 4]
        MOV dword ptr [esp + 0x18], eax
        MOV eax, dword ptr [edx + spline838Origin + 8]
        XOR edi, edi
        TEST esi, esi
        LEA ebx, [esi + 2]
        MOV dword ptr [esp + 0x1c], ecx
        MOV dword ptr [esp + 0x20], eax
        JLE spline838_30003d2c
        PUSH ebp
        LEA ecx, [esp + 0x2c]
        LEA eax, [edx + spline838ControlY]
        MOV edi, esi
spline838_30003d11:
        MOV ebp, dword ptr [eax - 4]
        ADD eax, spline838ControlStride
        MOV dword ptr [ecx - 4], ebp
        MOV ebp, dword ptr [eax - spline838ControlStride]
        MOV dword ptr [ecx], ebp
        MOV ebp, dword ptr [eax - spline838ControlStride + 4]
        MOV dword ptr [ecx + 4], ebp
        ADD ecx, 0xc
        DEC esi
        JNE spline838_30003d11
        POP ebp
spline838_30003d2c:
        MOV edx, dword ptr [edx + spline838Next]
        TEST edx, edx
        JE spline838_30003df2
        MOV ecx, dword ptr [edx + spline838Origin]
        LEA eax, [edi + edi*2]
        SHL eax, 2
        CMP ebx, 2
        MOV dword ptr [esp + eax + 0x24], ecx
        MOV ecx, dword ptr [edx + spline838Origin + 4]
        MOV edx, dword ptr [edx + spline838Origin + 8]
        MOV dword ptr [esp + eax + 0x28], ecx
        MOV dword ptr [esp + eax + 0x2c], edx
        JLE spline838_30003dbc
        LEA edx, [ebx - 1]
        ADD ebx, -2
spline838_30003d60:
        TEST edx, edx
        JLE spline838_30003db8
        LEA eax, [esp + 0x18]
        MOV ecx, edx
spline838_30003d6a:
        FLD dword ptr [eax + 0xc]
        FSUB dword ptr [eax]
        FLD dword ptr [eax + 0x10]
        FSUB dword ptr [eax + 4]
        ADD eax, 0xc
        DEC ecx
        FSTP dword ptr [esp + 0x10]
        FLD dword ptr [eax + 8]
        FSUB dword ptr [eax - 4]
        FSTP dword ptr [esp + 0x14]
        FMUL dword ptr [esp + 0x100]
        FADD dword ptr [eax - 0xc]
        FSTP dword ptr [eax - 0xc]
        FLD dword ptr [esp + 0x10]
        FMUL dword ptr [esp + 0x100]
        FADD dword ptr [eax - 8]
        FSTP dword ptr [eax - 8]
        FLD dword ptr [esp + 0x14]
        FMUL dword ptr [esp + 0x100]
        FADD dword ptr [eax - 4]
        FSTP dword ptr [eax - 4]
        JNE spline838_30003d6a
spline838_30003db8:
        DEC edx
        DEC ebx
        JNE spline838_30003d60
spline838_30003dbc:
        FLD dword ptr [esp + 0x18]
        MOV eax, dword ptr [esp + 0xf8]
        MOV ecx, dword ptr [esp + 0x1c]
        MOV edx, dword ptr [esp + 0x20]
        FSTP dword ptr [eax]
        FLD dword ptr [esp + 0x24]
        MOV dword ptr [eax + 4], ecx
        MOV ecx, dword ptr [esp + 0x28]
        MOV dword ptr [eax + 8], edx
        MOV eax, dword ptr [esp + 0xfc]
        MOV edx, dword ptr [esp + 0x2c]
        FSTP dword ptr [eax]
        MOV dword ptr [eax + 4], ecx
        MOV dword ptr [eax + 8], edx
spline838_30003df2:
        POP edi
        POP esi
        POP ebx
        ADD esp, 0xe4
        RET 
    }
}
#else
/* TC cgame 30003cd0. Reduce the control polygon to its final two points.
 * The Windows original retains the X difference in x87, but stores Y/Z
 * differences as floats before multiplying by tension. */
void BG_CalculateSpline_r(splinePath_t *spline, vec3_t out1, vec3_t out2, float tension) {
    vec3_t points[18];
    int i, count = spline->numControls + 2;
    VectorCopy(spline->point.origin, points[0]);
    for (i = 0; i < spline->numControls; ++i) {
        VectorCopy(spline->controls[i].origin, points[i + 1]);
    }
    if (!spline->next) return;
    VectorCopy(spline->next->point.origin, points[i + 1]);
    while (count > 2) {
        for (i = 0; i < count - 1; ++i) {
            float dy = points[i + 1][1] - points[i][1];
            float dz = points[i + 1][2] - points[i][2];
            points[i][0] = (float)(((double)points[i + 1][0] - points[i][0]) * tension + points[i][0]);
            points[i][1] = (float)((double)dy * tension + points[i][1]);
            points[i][2] = (float)((double)dz * tension + points[i][2]);
        }
        --count;
    }
    VectorCopy(points[0], out1);
    VectorCopy(points[1], out2);
}

#endif
