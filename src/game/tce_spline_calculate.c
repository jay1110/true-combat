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
spline838_30003cd0:
        SUB esp, 0xe4
spline838_30003cd6:
        MOV edx, dword ptr [esp + 0xe8]
spline838_30003cdd:
        PUSH ebx
spline838_30003cde:
        PUSH esi
spline838_30003cdf:
        PUSH edi
spline838_30003ce0:
        MOV eax, dword ptr [edx + spline838Origin]
spline838_30003ce3:
        MOV esi, dword ptr [edx + spline838Count]
spline838_30003ce9:
        MOV ecx, dword ptr [edx + spline838Origin + 4]
spline838_30003cec:
        MOV dword ptr [esp + 0x18], eax
spline838_30003cf0:
        MOV eax, dword ptr [edx + spline838Origin + 8]
spline838_30003cf3:
        XOR edi, edi
spline838_30003cf5:
        TEST esi, esi
spline838_30003cf7:
        LEA ebx, [esi + 2]
spline838_30003cfa:
        MOV dword ptr [esp + 0x1c], ecx
spline838_30003cfe:
        MOV dword ptr [esp + 0x20], eax
spline838_30003d02:
        JLE spline838_30003d2c
spline838_30003d04:
        PUSH ebp
spline838_30003d05:
        LEA ecx, [esp + 0x2c]
spline838_30003d09:
        LEA eax, [edx + spline838ControlY]
spline838_30003d0f:
        MOV edi, esi
spline838_30003d11:
        MOV ebp, dword ptr [eax - 4]
spline838_30003d14:
        ADD eax, spline838ControlStride
spline838_30003d17:
        MOV dword ptr [ecx - 4], ebp
spline838_30003d1a:
        MOV ebp, dword ptr [eax - spline838ControlStride]
spline838_30003d1d:
        MOV dword ptr [ecx], ebp
spline838_30003d1f:
        MOV ebp, dword ptr [eax - spline838ControlStride + 4]
spline838_30003d22:
        MOV dword ptr [ecx + 4], ebp
spline838_30003d25:
        ADD ecx, 0xc
spline838_30003d28:
        DEC esi
spline838_30003d29:
        JNE spline838_30003d11
spline838_30003d2b:
        POP ebp
spline838_30003d2c:
        MOV edx, dword ptr [edx + spline838Next]
spline838_30003d32:
        TEST edx, edx
spline838_30003d34:
        JE spline838_30003df2
spline838_30003d3a:
        MOV ecx, dword ptr [edx + spline838Origin]
spline838_30003d3d:
        LEA eax, [edi + edi*2]
spline838_30003d40:
        SHL eax, 2
spline838_30003d43:
        CMP ebx, 2
spline838_30003d46:
        MOV dword ptr [esp + eax + 0x24], ecx
spline838_30003d4a:
        MOV ecx, dword ptr [edx + spline838Origin + 4]
spline838_30003d4d:
        MOV edx, dword ptr [edx + spline838Origin + 8]
spline838_30003d50:
        MOV dword ptr [esp + eax + 0x28], ecx
spline838_30003d54:
        MOV dword ptr [esp + eax + 0x2c], edx
spline838_30003d58:
        JLE spline838_30003dbc
spline838_30003d5a:
        LEA edx, [ebx - 1]
spline838_30003d5d:
        ADD ebx, -2
spline838_30003d60:
        TEST edx, edx
spline838_30003d62:
        JLE spline838_30003db8
spline838_30003d64:
        LEA eax, [esp + 0x18]
spline838_30003d68:
        MOV ecx, edx
spline838_30003d6a:
        FLD dword ptr [eax + 0xc]
spline838_30003d6d:
        FSUB dword ptr [eax]
spline838_30003d6f:
        FLD dword ptr [eax + 0x10]
spline838_30003d72:
        FSUB dword ptr [eax + 4]
spline838_30003d75:
        ADD eax, 0xc
spline838_30003d78:
        DEC ecx
spline838_30003d79:
        FSTP dword ptr [esp + 0x10]
spline838_30003d7d:
        FLD dword ptr [eax + 8]
spline838_30003d80:
        FSUB dword ptr [eax - 4]
spline838_30003d83:
        FSTP dword ptr [esp + 0x14]
spline838_30003d87:
        FMUL dword ptr [esp + 0x100]
spline838_30003d8e:
        FADD dword ptr [eax - 0xc]
spline838_30003d91:
        FSTP dword ptr [eax - 0xc]
spline838_30003d94:
        FLD dword ptr [esp + 0x10]
spline838_30003d98:
        FMUL dword ptr [esp + 0x100]
spline838_30003d9f:
        FADD dword ptr [eax - 8]
spline838_30003da2:
        FSTP dword ptr [eax - 8]
spline838_30003da5:
        FLD dword ptr [esp + 0x14]
spline838_30003da9:
        FMUL dword ptr [esp + 0x100]
spline838_30003db0:
        FADD dword ptr [eax - 4]
spline838_30003db3:
        FSTP dword ptr [eax - 4]
spline838_30003db6:
        JNE spline838_30003d6a
spline838_30003db8:
        DEC edx
spline838_30003db9:
        DEC ebx
spline838_30003dba:
        JNE spline838_30003d60
spline838_30003dbc:
        FLD dword ptr [esp + 0x18]
spline838_30003dc0:
        MOV eax, dword ptr [esp + 0xf8]
spline838_30003dc7:
        MOV ecx, dword ptr [esp + 0x1c]
spline838_30003dcb:
        MOV edx, dword ptr [esp + 0x20]
spline838_30003dcf:
        FSTP dword ptr [eax]
spline838_30003dd1:
        FLD dword ptr [esp + 0x24]
spline838_30003dd5:
        MOV dword ptr [eax + 4], ecx
spline838_30003dd8:
        MOV ecx, dword ptr [esp + 0x28]
spline838_30003ddc:
        MOV dword ptr [eax + 8], edx
spline838_30003ddf:
        MOV eax, dword ptr [esp + 0xfc]
spline838_30003de6:
        MOV edx, dword ptr [esp + 0x2c]
spline838_30003dea:
        FSTP dword ptr [eax]
spline838_30003dec:
        MOV dword ptr [eax + 4], ecx
spline838_30003def:
        MOV dword ptr [eax + 8], edx
spline838_30003df2:
        POP edi
spline838_30003df3:
        POP esi
spline838_30003df4:
        POP ebx
spline838_30003df5:
        ADD esp, 0xe4
spline838_30003dfb:
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
