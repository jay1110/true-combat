// bg_slidemove.c -- part of bg_pmove functionality

#include "q_shared.h"
#include "bg_public.h"
#include "bg_local.h"
#include "tce_bg.h"

/*

input: origin, velocity, bounds, groundPlane, trace function

output: origin, velocity, impacts, stairup boolean

*/

/*
==================
PM_SlideMove

Returns qtrue if the velocity was clipped in some way
==================
*/
#define	MAX_CLIP_PLANES	5
/* TC:E Windows300135f0 / Linux000e7cac: four-bump body solver. */
/* TC qagame PM_SlideMove2003b930: exact Windows x87 and collision schedule. */
#if defined(_MSC_VER) && defined(_M_IX86)
typedef char slide834TraceLayout[(sizeof(trace_t)==56 && offsetof(trace_t,allsolid)==0 && offsetof(trace_t,startsolid)==4 && offsetof(trace_t,fraction)==8 && offsetof(trace_t,endpos)==12 && offsetof(trace_t,plane)+offsetof(cplane_t,normal)==24 && offsetof(trace_t,entityNum)==52 && sizeof(vec3_t)==12) ? 1 : -1];
static const unsigned int slide834Const200ac128[2] = {0x00000000u, 0x3fe00000u};
static const unsigned int slide834Const200ac100 = 0x00000000u;
static const unsigned int slide834Const200ac110 = 0x3f800000u;
static const unsigned int slide834Const200ac870[2] = {0x7ae147aeu, 0x3fefae14u};
static const unsigned int slide834Const200ac258[2] = {0x9999999au, 0x3fb99999u};
enum {
    slide834pml223cf484 = offsetof(pml_t, frametime),
    slide834pml223cf490 = offsetof(pml_t, groundPlane),
    slide834pml223cf4ac = offsetof(pml_t, groundTrace)+offsetof(trace_t,plane)+offsetof(cplane_t,normal),
    slide834pml223cf4b0 = offsetof(pml_t, groundTrace)+offsetof(trace_t,plane)+offsetof(cplane_t,normal)+4,
    slide834pml223cf4b4 = offsetof(pml_t, groundTrace)+offsetof(trace_t,plane)+offsetof(cplane_t,normal)+8,
    slide834pml223cf4cc = offsetof(pml_t, impactSpeed),
    slide834pml223cf4f4 = offsetof(pml_t, tceGroundExtension),
    slide834pm0 = offsetof(pmove_t, ps),
    slide834ps20 = offsetof(playerState_t, velocity),
    slide834ps24 = offsetof(playerState_t, velocity)+4,
    slide834ps28 = offsetof(playerState_t, velocity)+8,
    slide834ps38 = offsetof(playerState_t, gravity),
    slide834ps14 = offsetof(playerState_t, origin),
    slide834ps18 = offsetof(playerState_t, origin)+4,
    slide834ps1c = offsetof(playerState_t, origin)+8,
    slide834pm44 = offsetof(pmove_t, tracemask),
    slide834psa0 = offsetof(playerState_t, clientNum),
    slide834pm100 = offsetof(pmove_t, maxs),
    slide834pmf4 = offsetof(pmove_t, mins),
    slide834pm128 = offsetof(pmove_t, trace),
    slide834ps10 = offsetof(playerState_t, pm_time)
};
__declspec(naked) qboolean PM_SlideMove(qboolean gravity) {
    __asm {
        SUB ESP,0xd0
        MOV EAX,[pm]
        MOV ECX,dword ptr [EAX + slide834pm0]
        MOV EDX,dword ptr [ECX + slide834ps20]
        MOV dword ptr [ESP + 0x40],EDX
        MOV EDX,dword ptr [ECX + slide834ps24]
        MOV dword ptr [ESP + 0x44],EDX
        MOV EDX,dword ptr [ECX + slide834ps28]
        MOV dword ptr [ESP + 0x48],EDX
        MOV EDX,dword ptr [ESP + 0xd4]
        TEST EDX,EDX
        JZ slide834_2003ba2c
        MOV ECX,dword ptr [ECX + slide834ps20]
        MOV dword ptr [ESP],ECX
        MOV EDX,dword ptr [EAX + slide834pm0]
        MOV ECX,dword ptr [EDX + slide834ps24]
        MOV dword ptr [ESP + 0x4],ECX
        MOV EDX,dword ptr [EAX + slide834pm0]
        MOV ECX,dword ptr [EDX + slide834ps28]
        MOV dword ptr [ESP + 0x8],ECX
        MOV EDX,dword ptr [EAX + slide834pm0]
        FILD dword ptr [EDX + slide834ps38]
        FMUL dword ptr [pml + slide834pml223cf484]
        FSUBR dword ptr [ESP + 0x8]
        FSTP dword ptr [ESP + 0x8]
        MOV EAX,dword ptr [EAX + slide834pm0]
        FLD dword ptr [ESP + 0x8]
        FADD dword ptr [EAX + slide834ps28]
        FMUL qword ptr [slide834Const200ac128]
        FSTP dword ptr [EAX + slide834ps28]
        MOV EAX,dword ptr [ESP + 0x8]
        MOV dword ptr [ESP + 0x48],EAX
        MOV EAX,[pml + slide834pml223cf490]
        TEST EAX,EAX
        JZ slide834_2003b9cf
        MOV ECX,dword ptr [pm]
        PUSH 0x3f8020c5
        MOV EAX,dword ptr [ECX + slide834pm0]
        ADD EAX,slide834ps20
        PUSH EAX
        PUSH OFFSET pml + slide834pml223cf4ac
        PUSH EAX
        CALL PM_ClipVelocity
        ADD ESP,0x10
slide834_2003b9cf:
        MOV EAX,[pm]
slide834_2003b9d4:
        MOV ECX,dword ptr [pml + slide834pml223cf484]
        PUSH EBX
        MOV dword ptr [ESP + 0x10],ECX
        MOV ECX,dword ptr [pml + slide834pml223cf490]
        PUSH EBP
        PUSH ESI
        TEST ECX,ECX
        PUSH EDI
        JZ slide834_2003ba9e
        MOV ECX,dword ptr [pml + slide834pml223cf4f4]
        TEST ECX,ECX
        JNZ slide834_2003ba9e
        MOV EDX,dword ptr [pml + slide834pml223cf4ac]
        MOV ECX,dword ptr [pml + slide834pml223cf4b0]
        MOV dword ptr [ESP + 0xa4],EDX
        MOV EDX,dword ptr [pml + slide834pml223cf4b4]
        MOV EDI,0x1
        MOV dword ptr [ESP + 0xa8],ECX
        MOV dword ptr [ESP + 0xac],EDX
        JMP slide834_2003baa0
slide834_2003ba2c:
        MOV EDX,dword ptr [pml + slide834pml223cf4f4]
        TEST EDX,EDX
        JZ slide834_2003ba81
        MOV EDX,dword ptr [ECX + slide834ps20]
        MOV dword ptr [ESP],EDX
        MOV ECX,dword ptr [EAX + slide834pm0]
        MOV EDX,dword ptr [ECX + slide834ps24]
        MOV dword ptr [ESP + 0x4],EDX
        MOV ECX,dword ptr [EAX + slide834pm0]
        MOV EDX,dword ptr [ECX + slide834ps28]
        MOV dword ptr [ESP + 0x8],EDX
        MOV ECX,dword ptr [EAX + slide834pm0]
        FILD dword ptr [ECX + slide834ps38]
        FMUL dword ptr [pml + slide834pml223cf484]
        FSUBR dword ptr [ESP + 0x8]
        FSTP dword ptr [ESP + 0x8]
        MOV EAX,dword ptr [EAX + slide834pm0]
        FLD dword ptr [ESP + 0x8]
        FADD dword ptr [EAX + slide834ps28]
        FMUL qword ptr [slide834Const200ac128]
        FSTP dword ptr [EAX + slide834ps28]
        MOV EDX,dword ptr [ESP + 0x8]
        MOV dword ptr [ESP + 0x48],EDX
        JMP slide834_2003b9cf
slide834_2003ba81:
        MOV dword ptr [ESP + 0x8],0x0
        MOV dword ptr [ESP + 0x4],0x0
        MOV dword ptr [ESP],0x0
        JMP slide834_2003b9d4
slide834_2003ba9e:
        XOR EDI,EDI
slide834_2003baa0:
        MOV EAX,dword ptr [EAX + slide834pm0]
        LEA ECX,[EDI + EDI*0x2]
        ADD EAX,slide834ps20
        LEA EDX,[ESP + ECX*0x4 + 0xa4]
        PUSH EDX
        PUSH EAX
        CALL VectorNormalize2
        ADD ESP,0x8
        INC EDI
        XOR EBX,EBX
        LEA ECX,[EDI + EDI*0x2]
        MOV dword ptr [ESP + 0x4c],EBX
        FSTP ST(0)
        LEA ESI,[ESP + ECX*0x4 + 0xa8]
        MOV ECX,dword ptr [pm]
slide834_2003bad2:
        MOV EAX,dword ptr [ECX + slide834pm0]
        FLD dword ptr [ESP + 0x1c]
        FMUL dword ptr [EAX + slide834ps20]
        FADD dword ptr [EAX + slide834ps14]
        FSTP dword ptr [ESP + 0x60]
        MOV EAX,dword ptr [ECX + slide834pm0]
        FLD dword ptr [ESP + 0x1c]
        FMUL dword ptr [EAX + slide834ps24]
        FADD dword ptr [EAX + slide834ps18]
        FSTP dword ptr [ESP + 0x64]
        MOV EAX,dword ptr [ECX + slide834pm0]
        FLD dword ptr [ESP + 0x1c]
        FMUL dword ptr [EAX + slide834ps28]
        FADD dword ptr [EAX + slide834ps1c]
        FSTP dword ptr [ESP + 0x68]
        MOV EAX,dword ptr [ECX + slide834pm0]
        MOV EDX,dword ptr [ECX + slide834pm44]
        PUSH EDX
        MOV EDX,dword ptr [EAX + slide834psa0]
        ADD EAX,slide834ps14
        PUSH EDX
        LEA EDX,[ESP + 0x68]
        PUSH EDX
        LEA EDX,[ECX + slide834pm100]
        PUSH EDX
        LEA EDX,[ECX + slide834pmf4]
        PUSH EDX
        PUSH EAX
        LEA EAX,[ESP + 0x84]
        PUSH EAX
        CALL dword ptr [ECX + slide834pm128]
        MOV EAX,dword ptr [ESP + 0x88]
        XOR EBP,EBP
        ADD ESP,0x1c
        CMP EAX,EBP
        JNZ slide834_2003bf3a
        FLD dword ptr [ESP + 0x74]
        FCOMP dword ptr [slide834Const200ac100]
        FNSTSW AX
        TEST AH,0x41
        JNZ slide834_2003bb89
        MOV ECX,dword ptr [pm]
        MOV EAX,dword ptr [ESP + 0x78]
        MOV EDX,dword ptr [ECX + slide834pm0]
        MOV dword ptr [EDX + slide834ps14],EAX
        MOV ECX,dword ptr [pm]
        MOV EAX,dword ptr [ESP + 0x7c]
        MOV EDX,dword ptr [ECX + slide834pm0]
        MOV dword ptr [EDX + slide834ps18],EAX
        MOV ECX,dword ptr [pm]
        MOV EAX,dword ptr [ESP + 0x80]
        MOV EDX,dword ptr [ECX + slide834pm0]
        MOV dword ptr [EDX + slide834ps1c],EAX
slide834_2003bb89:
        FLD dword ptr [ESP + 0x74]
        FCOMP dword ptr [slide834Const200ac110]
        FNSTSW AX
        TEST AH,0x40
        JNZ slide834_2003bfb7
        MOV ECX,dword ptr [ESP + 0xa0]
        PUSH ECX
        CALL PM_AddTouchEnt
        FLD dword ptr [ESP + 0x78]
        FMUL dword ptr [ESP + 0x20]
        ADD ESP,0x4
        CMP EDI,0x5
        FSUBR dword ptr [ESP + 0x1c]
        FSTP dword ptr [ESP + 0x1c]
        JGE slide834_2003bf55
        XOR ECX,ECX
        CMP EDI,EBP
        JLE slide834_2003bc56
        LEA EDX,[ESP + 0xa8]
slide834_2003bbd8:
        FLD dword ptr [ESP + 0x8c]
        FMUL dword ptr [EDX + 0x4]
        FLD dword ptr [ESP + 0x84]
        FMUL dword ptr [EDX + -0x4]
        FADDP ST(1),ST(0)
        FLD dword ptr [ESP + 0x88]
        FMUL dword ptr [EDX]
        FADDP ST(1),ST(0)
        FCOMP qword ptr [slide834Const200ac870]
        FNSTSW AX
        TEST AH,0x41
        JZ slide834_2003bc10
        INC ECX
        ADD EDX,0xc
        CMP ECX,EDI
        JL slide834_2003bbd8
        JMP slide834_2003bc56
slide834_2003bc10:
        MOV EDX,dword ptr [pm]
        CMP ECX,EDI
        FLD dword ptr [ESP + 0x84]
        MOV EAX,dword ptr [EDX + slide834pm0]
        FADD dword ptr [EAX + slide834ps20]
        FSTP dword ptr [EAX + slide834ps20]
        MOV EAX,[pm]
        FLD dword ptr [ESP + 0x88]
        MOV EAX,dword ptr [EAX + slide834pm0]
        FADD dword ptr [EAX + slide834ps24]
        FSTP dword ptr [EAX + slide834ps24]
        MOV EDX,dword ptr [pm]
        FLD dword ptr [ESP + 0x8c]
        MOV EAX,dword ptr [EDX + slide834pm0]
        FADD dword ptr [EAX + slide834ps28]
        FSTP dword ptr [EAX + slide834ps28]
        JL slide834_2003bf1d
slide834_2003bc56:
        MOV EAX,dword ptr [ESP + 0x84]
        MOV ECX,dword ptr [ESP + 0x88]
        MOV EDX,dword ptr [ESP + 0x8c]
        MOV dword ptr [ESI + -0x4],EAX
        MOV dword ptr [ESI],ECX
        MOV dword ptr [ESI + 0x4],EDX
        INC EDI
        ADD ESI,0xc
        CMP EDI,EBP
        MOV dword ptr [ESP + 0x5c],ESI
        MOV dword ptr [ESP + 0x38],EBP
        JLE slide834_2003bf1d
        MOV ECX,dword ptr [pm]
        LEA EDX,[ESP + 0xa8]
        MOV EBX,dword ptr [ECX + slide834pm0]
slide834_2003bc96:
        FLD dword ptr [EDX + -0x4]
        FMUL dword ptr [EBX + slide834ps20]
        FLD dword ptr [EDX + 0x4]
        FMUL dword ptr [EBX + slide834ps28]
        FADDP ST(1),ST(0)
        FLD dword ptr [EDX]
        FMUL dword ptr [EBX + slide834ps24]
        FADDP ST(1),ST(0)
        FCOM qword ptr [slide834Const200ac258]
        FNSTSW AX
        TEST AH,0x1
        JNZ slide834_2003bccb
        INC EBP
        ADD EDX,0xc
        CMP EBP,EDI
        FSTP ST(0)
        JL slide834_2003bc96
        MOV dword ptr [ESP + 0x38],EBP
        JMP slide834_2003bf23
slide834_2003bccb:
        FCHS
        FCOM dword ptr [pml + slide834pml223cf4cc]
        MOV dword ptr [ESP + 0x38],EBP
        FNSTSW AX
        TEST AH,0x41
        JNZ slide834_2003bce6
        FSTP dword ptr [pml + slide834pml223cf4cc]
        JMP slide834_2003bce8
slide834_2003bce6:
        FSTP ST(0)
slide834_2003bce8:
        MOV ECX,dword ptr [ECX + slide834pm0]
        LEA EBP,[EBP + EBP*0x2]
        SHL EBP,0x2
        LEA EAX,[ESP + 0x20]
        PUSH 0x3f8020c5
        LEA EBX,[ESP + EBP*0x1 + 0xa8]
        PUSH EAX
        ADD ECX,slide834ps20
        PUSH EBX
        PUSH ECX
        CALL PM_ClipVelocity
        LEA EDX,[ESP + 0x50]
        PUSH 0x3f8020c5
        PUSH EDX
        LEA EAX,[ESP + 0x28]
        PUSH EBX
        PUSH EAX
        CALL PM_ClipVelocity
        ADD ESP,0x20
        XOR EAX,EAX
        MOV dword ptr [ESP + 0x3c],EAX
        LEA ESI,[ESP + 0xa4]
slide834_2003bd31:
        CMP EAX,dword ptr [ESP + 0x38]
        JZ slide834_2003beb8
        FLD dword ptr [ESP + 0x28]
        FMUL dword ptr [ESI + 0x8]
        FLD dword ptr [ESP + 0x24]
        FMUL dword ptr [ESI + 0x4]
        FADDP ST(1),ST(0)
        FLD dword ptr [ESP + 0x20]
        FMUL dword ptr [ESI]
        FADDP ST(1),ST(0)
        FCOMP qword ptr [slide834Const200ac258]
        FNSTSW AX
        TEST AH,0x1
        JZ slide834_2003beb8
        LEA ECX,[ESP + 0x20]
        PUSH 0x3f8020c5
        PUSH ECX
        LEA EDX,[ESP + 0x28]
        PUSH ESI
        PUSH EDX
        CALL PM_ClipVelocity
        LEA EAX,[ESP + 0x50]
        PUSH 0x3f8020c5
        PUSH EAX
        LEA ECX,[ESP + 0x58]
        PUSH ESI
        PUSH ECX
        CALL PM_ClipVelocity
        FLD dword ptr [ESP + 0x48]
        FMUL dword ptr [ESP + EBP*0x1 + 0xcc]
        FLD dword ptr [ESP + 0x44]
        FMUL dword ptr [ESP + EBP*0x1 + 0xc8]
        ADD ESP,0x20
        FADDP ST(1),ST(0)
        FLD dword ptr [ESP + 0x20]
        FMUL dword ptr [EBX]
        FADDP ST(1),ST(0)
        FCOMP dword ptr [slide834Const200ac100]
        FNSTSW AX
        TEST AH,0x1
        JZ slide834_2003beb8
        LEA EDX,[ESP + 0x2c]
        PUSH EDX
        PUSH ESI
        PUSH EBX
        CALL CrossProduct
        LEA EAX,[ESP + 0x38]
        PUSH EAX
        CALL VectorNormalize
        MOV ECX,dword ptr [pm]
        LEA EDX,[ESP + 0x3c]
        FSTP ST(0)
        MOV EAX,dword ptr [ECX + slide834pm0]
        PUSH EDX
        FLD dword ptr [ESP + 0x48]
        FMUL dword ptr [EAX + slide834ps28]
        FLD dword ptr [ESP + 0x44]
        FMUL dword ptr [EAX + slide834ps24]
        PUSH ESI
        PUSH EBX
        FADDP ST(1),ST(0)
        FLD dword ptr [ESP + 0x48]
        FMUL dword ptr [EAX + slide834ps20]
        FADDP ST(1),ST(0)
        FLD dword ptr [ESP + 0x48]
        FMUL ST(0),ST(1)
        FSTP dword ptr [ESP + 0x3c]
        FLD dword ptr [ESP + 0x4c]
        FMUL ST(0),ST(1)
        FSTP dword ptr [ESP + 0x40]
        FLD dword ptr [ESP + 0x50]
        FMUL ST(0),ST(1)
        FSTP dword ptr [ESP + 0x44]
        FSTP ST(0)
        CALL CrossProduct
        LEA EAX,[ESP + 0x48]
        PUSH EAX
        CALL VectorNormalize
        FSTP ST(0)
        FLD dword ptr [ESP + 0x54]
        FMUL dword ptr [ESP + 0x38]
        FLD dword ptr [ESP + 0x50]
        FMUL dword ptr [ESP + 0x34]
        ADD ESP,0x20
        XOR ECX,ECX
        LEA EDX,[ESP + 0xa8]
        FADDP ST(1),ST(0)
        FLD dword ptr [ESP + 0x2c]
        FMUL dword ptr [ESP + 0x10]
        FADDP ST(1),ST(0)
        FLD dword ptr [ESP + 0x2c]
        FMUL ST(0),ST(1)
        FSTP dword ptr [ESP + 0x40]
        FLD dword ptr [ESP + 0x30]
        FMUL ST(0),ST(1)
        FSTP dword ptr [ESP + 0x44]
        FLD dword ptr [ESP + 0x34]
        FMUL ST(0),ST(1)
        FSTP dword ptr [ESP + 0x48]
        FSTP ST(0)
slide834_2003be7b:
        CMP ECX,dword ptr [ESP + 0x38]
        JZ slide834_2003beb0
        CMP ECX,dword ptr [ESP + 0x3c]
        JZ slide834_2003beb0
        FLD dword ptr [ESP + 0x28]
        FMUL dword ptr [EDX + 0x4]
        FLD dword ptr [ESP + 0x20]
        FMUL dword ptr [EDX + -0x4]
        FADDP ST(1),ST(0)
        FLD dword ptr [ESP + 0x24]
        FMUL dword ptr [EDX]
        FADDP ST(1),ST(0)
        FCOMP qword ptr [slide834Const200ac258]
        FNSTSW AX
        TEST AH,0x1
        JNZ slide834_2003bf85
slide834_2003beb0:
        INC ECX
        ADD EDX,0xc
        CMP ECX,EDI
        JL slide834_2003be7b
slide834_2003beb8:
        MOV EAX,dword ptr [ESP + 0x3c]
        ADD ESI,0xc
        INC EAX
        CMP EAX,EDI
        MOV dword ptr [ESP + 0x3c],EAX
        JL slide834_2003bd31
        MOV ECX,dword ptr [pm]
        MOV EAX,dword ptr [ESP + 0x20]
        MOV ESI,dword ptr [ESP + 0x5c]
        MOV EDX,dword ptr [ECX + slide834pm0]
        MOV dword ptr [EDX + slide834ps20],EAX
        MOV ECX,dword ptr [pm]
        MOV EAX,dword ptr [ESP + 0x24]
        MOV EDX,dword ptr [ECX + slide834pm0]
        MOV dword ptr [EDX + slide834ps24],EAX
        MOV ECX,dword ptr [pm]
        MOV EAX,dword ptr [ESP + 0x28]
        MOV EDX,dword ptr [ECX + slide834pm0]
        MOV dword ptr [EDX + slide834ps28],EAX
        MOV ECX,dword ptr [ESP + 0x40]
        MOV EDX,dword ptr [ESP + 0x44]
        MOV EAX,dword ptr [ESP + 0x48]
        MOV dword ptr [ESP + 0x10],ECX
        MOV ECX,dword ptr [pm]
        MOV dword ptr [ESP + 0x14],EDX
        MOV dword ptr [ESP + 0x18],EAX
        JMP slide834_2003bf23
slide834_2003bf1d:
        MOV ECX,dword ptr [pm]
slide834_2003bf23:
        MOV EBX,dword ptr [ESP + 0x4c]
        INC EBX
        CMP EBX,0x4
        MOV dword ptr [ESP + 0x4c],EBX
        JL slide834_2003bad2
        JMP slide834_2003bfbd
slide834_2003bf3a:
        MOV ECX,dword ptr [pm]
        POP EDI
        POP ESI
        MOV EAX,0x1
        MOV EDX,dword ptr [ECX + slide834pm0]
        MOV dword ptr [EDX + slide834ps28],EBP
        POP EBP
        POP EBX
        ADD ESP,0xd0
        RET
slide834_2003bf55:
        MOV EAX,[pm]
        POP EDI
        POP ESI
        MOV ECX,dword ptr [EAX + slide834pm0]
        MOV dword ptr [ECX + slide834ps28],EBP
        MOV EDX,dword ptr [pm]
        MOV EAX,dword ptr [EDX + slide834pm0]
        MOV dword ptr [EAX + slide834ps24],EBP
        MOV ECX,dword ptr [pm]
        MOV EAX,0x1
        MOV EDX,dword ptr [ECX + slide834pm0]
        MOV dword ptr [EDX + slide834ps20],EBP
        POP EBP
        POP EBX
        ADD ESP,0xd0
        RET
slide834_2003bf85:
        MOV EAX,[pm]
        POP EDI
        POP ESI
        POP EBP
        MOV ECX,dword ptr [EAX + slide834pm0]
        XOR EAX,EAX
        POP EBX
        MOV dword ptr [ECX + slide834ps28],EAX
        MOV EDX,dword ptr [pm]
        MOV ECX,dword ptr [EDX + slide834pm0]
        MOV dword ptr [ECX + slide834ps24],EAX
        MOV EDX,dword ptr [pm]
        MOV ECX,dword ptr [EDX + slide834pm0]
        MOV dword ptr [ECX + slide834ps20],EAX
        MOV EAX,0x1
        ADD ESP,0xd0
        RET
slide834_2003bfb7:
        MOV ECX,dword ptr [pm]
slide834_2003bfbd:
        MOV EAX,dword ptr [ESP + 0xe4]
        TEST EAX,EAX
        JZ slide834_2003bff5
        MOV EDX,dword ptr [ECX + slide834pm0]
        MOV EAX,dword ptr [ESP + 0x10]
        MOV dword ptr [EDX + slide834ps20],EAX
        MOV ECX,dword ptr [pm]
        MOV EAX,dword ptr [ESP + 0x14]
        MOV EDX,dword ptr [ECX + slide834pm0]
        MOV dword ptr [EDX + slide834ps24],EAX
        MOV ECX,dword ptr [pm]
        MOV EAX,dword ptr [ESP + 0x18]
        MOV EDX,dword ptr [ECX + slide834pm0]
        MOV dword ptr [EDX + slide834ps28],EAX
        MOV ECX,dword ptr [pm]
slide834_2003bff5:
        MOV ECX,dword ptr [ECX + slide834pm0]
        MOV EAX,dword ptr [ECX + slide834ps10]
        TEST EAX,EAX
        JZ slide834_2003c021
        MOV EDX,dword ptr [ESP + 0x50]
        MOV dword ptr [ECX + slide834ps20],EDX
        MOV EAX,[pm]
        MOV EDX,dword ptr [ESP + 0x54]
        MOV ECX,dword ptr [EAX + slide834pm0]
        MOV dword ptr [ECX + slide834ps24],EDX
        MOV EAX,[pm]
        MOV EDX,dword ptr [ESP + 0x58]
        MOV ECX,dword ptr [EAX + slide834pm0]
        MOV dword ptr [ECX + slide834ps28],EDX
slide834_2003c021:
        XOR EAX,EAX
        POP EDI
        TEST EBX,EBX
        POP ESI
        POP EBP
        SETNZ AL
        POP EBX
        ADD ESP,0xd0
        RET
    }
}
#else
qboolean	PM_SlideMove( qboolean gravity ) {
	int			bumpcount, numbumps;
	vec3_t		dir;
	float		d;
	int			numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity;
	vec3_t		clipVelocity;
	int			i, j, k;
	trace_t	trace;
	vec3_t		end;
	float		time_left;
	float		into;
	vec3_t		endVelocity;
	vec3_t		endClipVelocity;
	
	numbumps = 4;

	VectorCopy (pm->ps->velocity, primal_velocity);

	if ( gravity || pml.tceGroundExtension ) {
		VectorCopy( pm->ps->velocity, endVelocity );
		endVelocity[2] -= pm->ps->gravity * pml.frametime;
		pm->ps->velocity[2] = ( pm->ps->velocity[2] + endVelocity[2] ) * 0.5;
		primal_velocity[2] = endVelocity[2];
		if ( gravity && pml.groundPlane ) {
			// slide along the ground plane
			PM_ClipVelocity (pm->ps->velocity, pml.groundTrace.plane.normal, 
				pm->ps->velocity, OVERCLIP );
		}
	} else {
		VectorClear( endVelocity );
	}

	time_left = pml.frametime;

	// never turn against the ground plane
	if ( pml.groundPlane && !pml.tceGroundExtension ) {
		numplanes = 1;
		VectorCopy( pml.groundTrace.plane.normal, planes[0] );
	} else {
		numplanes = 0;
	}

	// never turn against original velocity
	VectorNormalize2( pm->ps->velocity, planes[numplanes] );
	numplanes++;

	for ( bumpcount=0 ; bumpcount < numbumps ; bumpcount++ ) {

		// calculate position we are trying to move to
		VectorMA( pm->ps->origin, time_left, pm->ps->velocity, end );

		// see if we can make it there
        pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, end,
                  pm->ps->clientNum, pm->tracemask);

		if (trace.allsolid) {
			// entity is completely trapped in another solid
			pm->ps->velocity[2] = 0;	// don't build up falling damage, but allow sideways acceleration
			return qtrue;
		}

		if (trace.fraction > 0) {
			// actually covered some distance
			VectorCopy (trace.endpos, pm->ps->origin);
		}

		if (trace.fraction == 1) {
			 break;		// moved the entire distance
		}

		// save entity for contact
		PM_AddTouchEnt( trace.entityNum );

		time_left -= time_left * trace.fraction;

		if (numplanes >= MAX_CLIP_PLANES) {
			// this shouldn't really happen
			VectorClear( pm->ps->velocity );
			return qtrue;
		}

		//
		// if this is the same plane we hit before, nudge velocity
		// out along it, which fixes some epsilon issues with
		// non-axial planes
		//
		for ( i = 0 ; i < numplanes ; i++ ) {
			if ( DotProduct( trace.plane.normal, planes[i] ) > 0.99 ) {
                VectorAdd(trace.plane.normal, pm->ps->velocity, pm->ps->velocity);
				break;
			}
		}
		if ( i < numplanes ) {
			continue;
		}
		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

		//
		// modify velocity so it parallels all of the clip planes
		//

		// find a plane that it enters
		for ( i = 0 ; i < numplanes ; i++ ) {
			into = DotProduct( pm->ps->velocity, planes[i] );
			if ( into >= 0.1 ) {
				continue;		// move doesn't interact with the plane
			}

			// see how hard we are hitting things
			if ( -into > pml.impactSpeed ) {
				pml.impactSpeed = -into;
			}

			// slide along the plane
			PM_ClipVelocity (pm->ps->velocity, planes[i], clipVelocity, OVERCLIP );

			// slide along the plane
			PM_ClipVelocity (endVelocity, planes[i], endClipVelocity, OVERCLIP );

			// see if there is a second plane that the new move enters
			for ( j = 0 ; j < numplanes ; j++ ) {
				if ( j == i ) {
					continue;
				}
				if ( DotProduct( clipVelocity, planes[j] ) >= 0.1 ) {
					continue;		// move doesn't interact with the plane
				}

				// try clipping the move to the plane
				PM_ClipVelocity( clipVelocity, planes[j], clipVelocity, OVERCLIP );
				PM_ClipVelocity( endClipVelocity, planes[j], endClipVelocity, OVERCLIP );

				// see if it goes back into the first clip plane
				if ( DotProduct( clipVelocity, planes[i] ) >= 0 ) {
					continue;
				}

				// slide the original velocity along the crease
				CrossProduct (planes[i], planes[j], dir);
				VectorNormalize( dir );
				d = DotProduct( dir, pm->ps->velocity );
				VectorScale( dir, d, clipVelocity );

				CrossProduct (planes[i], planes[j], dir);
				VectorNormalize( dir );
				d = DotProduct( dir, endVelocity );
				VectorScale( dir, d, endClipVelocity );

				// see if there is a third plane the the new move enters
				for ( k = 0 ; k < numplanes ; k++ ) {
					if ( k == i || k == j ) {
						continue;
					}
					if ( DotProduct( clipVelocity, planes[k] ) >= 0.1 ) {
						continue;		// move doesn't interact with the plane
					}

					// stop dead at a tripple plane interaction
					VectorClear( pm->ps->velocity );
					return qtrue;
				}
			}

			// if we have fixed all interactions, try another move
			VectorCopy( clipVelocity, pm->ps->velocity );
			VectorCopy( endClipVelocity, endVelocity );
			break;
		}
	}

	if ( gravity ) {
		VectorCopy( endVelocity, pm->ps->velocity );
	}

	// don't change velocity if in a timer (FIXME: is this correct?)
	if ( pm->ps->pm_time ) {
		VectorCopy( primal_velocity, pm->ps->velocity );
	}

	return ( bumpcount != 0 );
}
#endif

/*
==================
PM_StepSlideMove

==================
*/
/* TC Windows 30013d00: small steps must fit below a low doorway before
 * trying the full 18-unit step. Shared by prediction and the server. */
static void PM_TCEStepSlideMove( qboolean gravity ) {
	vec3_t start, velocity, up, down, forward;
	trace_t trace;
	float height = 9.0f, delta;
	VectorCopy(pm->ps->origin, start);
	VectorCopy(pm->ps->velocity, velocity);
	if (!PM_SlideMove(gravity)) return;
	VectorCopy(start, down);
	down[2] -= 18.0f;
	pm->trace(&trace, start, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask);
	if (pm->ps->velocity[2] > 0 && (trace.fraction == 1.0f || trace.plane.normal[2] < 0.7)) return;
	VectorCopy(start, up);
	up[2] += height;
	pm->trace(&trace, up, pm->mins, pm->maxs, up, pm->ps->clientNum, pm->tracemask);
	if (trace.allsolid) {
		if (pm->debugLevel) Com_Printf("%i:bend can't step small\n", c_pmove);
		return;
	}
	VectorSet(forward, velocity[0], velocity[1], 0);
	VectorNormalize(forward);
	VectorMA(up, 4.0f, forward, forward);
	pm->trace(&trace, forward, pm->mins, pm->maxs, forward, pm->ps->clientNum, pm->tracemask);
	if (trace.allsolid) {
		height = 18.0f;
		VectorCopy(start, up);
		up[2] += height;
		pm->trace(&trace, up, pm->mins, pm->maxs, up, pm->ps->clientNum, pm->tracemask);
		if (trace.allsolid) {
			if (pm->debugLevel) Com_Printf("%i:bend can't step\n", c_pmove);
			return;
		}
	}
	VectorCopy(up, pm->ps->origin);
	VectorCopy(velocity, pm->ps->velocity);
	PM_SlideMove(gravity);
	VectorCopy(pm->ps->origin, down);
	down[2] -= height;
	pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask);
	if (!trace.allsolid) VectorCopy(trace.endpos, pm->ps->origin);
	if (trace.fraction < 1.0f) PM_ClipVelocity(pm->ps->velocity, trace.plane.normal, pm->ps->velocity, OVERCLIP);
	delta = pm->ps->origin[2] - start[2];
	if (delta > 2.0f) PM_AddEvent(delta < 7.0f ? EV_STEP_4 : delta < 11.0f ? EV_STEP_8 : delta < 15.0f ? EV_STEP_12 : EV_STEP_16);
	if (pm->debugLevel) Com_Printf("%i:stepped\n", c_pmove);
}

void PM_StepSlideMove( qboolean gravity ) {
	vec3_t		start_o, start_v;
	vec3_t		down_o, down_v;
	trace_t		trace;
//	float		down_dist, up_dist;
//	vec3_t		delta, delta2;
	vec3_t		up, down;
	if (gearDef.parsed && !(pm->ps->eFlags & EF_PRONE)) {
		PM_TCEStepSlideMove(gravity);
		return;
	}

	VectorCopy (pm->ps->origin, start_o);
	VectorCopy (pm->ps->velocity, start_v);

	if ( pm->debugLevel ) {
		qboolean wassolid, slidesucceed;
	
		PM_TraceAll( &trace, pm->ps->origin, pm->ps->origin );
		wassolid = trace.allsolid;
	
		slidesucceed = (PM_SlideMove( gravity ) == 0);
	
		PM_TraceAll( &trace, pm->ps->origin, pm->ps->origin );
		if (trace.allsolid && !wassolid)
			Com_Printf("%i:PM_SlideMove solidified! (%f %f %f) -> (%f %f %f)\n", c_pmove,
				start_o[0],
				start_o[1],
				start_o[2],
				pm->ps->origin[0],
				pm->ps->origin[1],
				pm->ps->origin[2]
			);
	
		if (slidesucceed)
			return;
	} else {
		if ( PM_SlideMove( gravity ) == 0 ) {
			return;		// we got exactly where we wanted to go first try
		}
	}

	if ( pm->debugLevel ) {
		Com_Printf("%i:stepping\n", c_pmove);
	}

	VectorCopy(start_o, down);
	down[2] -= STEPSIZE;

	PM_TraceAll( &trace, start_o, down );
	VectorSet(up, 0, 0, 1);
	// never step up when you still have up velocity
	if ( pm->ps->velocity[2] > 0 && (trace.fraction == 1.0 || DotProduct(trace.plane.normal, up) < 0.7)) {
		return;
	}

	VectorCopy (pm->ps->origin, down_o);
	VectorCopy (pm->ps->velocity, down_v);

	VectorCopy (start_o, up);
	up[2] += STEPSIZE;

	// test the player position if they were a stepheight higher
	PM_TraceAll( &trace, up, up );
	if ( trace.allsolid ) {
		if ( pm->debugLevel ) {
			Com_Printf("%i:bend can't step\n", c_pmove);
		}
		return;		// can't step up
	}

	// try slidemove from this position
	VectorCopy (up, pm->ps->origin);
	VectorCopy (start_v, pm->ps->velocity);

	PM_SlideMove( gravity );

	// push down the final amount
	VectorCopy (pm->ps->origin, down);
	down[2] -= STEPSIZE;

	// check legs separately
	if ( pm->ps->eFlags & EF_PRONE ) {
		PM_TraceLegs( &trace, NULL, pm->ps->origin, down, NULL, pm->ps->viewangles, pm->trace, pm->ps->clientNum, pm->tracemask );
		if ( trace.allsolid ) {
			// legs don't step, just fuzz.
			VectorCopy( down_o, pm->ps->origin );
			VectorCopy( down_v, pm->ps->velocity );
			if ( pm->debugLevel ) {
				Com_Printf("%i:legs unsteppable\n", c_pmove);
			}
			return;
		}
	}

	pm->trace( &trace, pm->ps->origin, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask );
	if ( !trace.allsolid ) {
		VectorCopy (trace.endpos, pm->ps->origin);
	}
	if ( trace.fraction < 1.0 ) {
		PM_ClipVelocity( pm->ps->velocity, trace.plane.normal, pm->ps->velocity, OVERCLIP );
	}

#if 0
	// if the down trace can trace back to the original position directly, don't step
	PM_TraceAll( &trace, pm->ps->origin, start_o );
	if ( trace.fraction == 1.0 ) {
		// use the original move
		VectorCopy (down_o, pm->ps->origin);
		VectorCopy (down_v, pm->ps->velocity);
		if ( pm->debugLevel ) {
			Com_Printf("%i:bend\n", c_pmove);
		}
	} else 
#endif
	{
		// use the step move
		float	delta;

		delta = pm->ps->origin[2] - start_o[2];
		if ( delta > 2 ) {
			if ( delta < 7 ) {
				PM_AddEvent( EV_STEP_4 );
			} else if ( delta < 11 ) {
				PM_AddEvent( EV_STEP_8 );
			} else if ( delta < 15 ) {
				PM_AddEvent( EV_STEP_12 );
			} else {
				PM_AddEvent( EV_STEP_16 );
			}
		}
		if ( pm->debugLevel ) {
			Com_Printf("%i:stepped\n", c_pmove);
		}
	}
}



#if !defined(_MSC_VER) || !defined(_M_IX86)
/* TC:E300141c0: body trace plus independent rear-leg support probe.
 * Original repeats this probe on a fraction==1 move; do not coalesce it. */
static qboolean PM_TCEProneProbe(const vec3_t bodyEnd, qboolean firstProbe,
                                const vec3_t mins, const vec3_t maxs, float scale) {
    vec3_t start, end;
    trace_t trace;
    start[0] = bodyEnd[0] - pml.forward[0] * scale * 32.0f;
    start[1] = bodyEnd[1] - pml.forward[1] * scale * 32.0f;
    start[2] = bodyEnd[2] + 24.0f;
    VectorCopy(start, end); end[2] = start[2] - 21.6f - 24.0f;
    pm->trace(&trace, start, mins, maxs, end, pm->ps->clientNum, pm->tracemask);
    if (trace.startsolid) {
        pm->ps->velocity[2] = 0;
        pm->pmext->proneGroundTime = pm->cmd.serverTime;
        return qfalse;
    }
    if (trace.fraction >= 1.0f) pm->pmext->proneLegsOffset = start[2];
    else {
        VectorCopy(trace.endpos, start); VectorCopy(start, end); end[2] += 21.6f;
        pm->trace(&trace, start, mins, maxs, end, pm->ps->clientNum, pm->tracemask);
        if (trace.allsolid) {
            pm->ps->velocity[2] = 0;
            if (firstProbe) pm->pmext->proneTime = -pm->cmd.serverTime;
            return qfalse;
        }
        pm->pmext->proneLegsOffset = start[2] - bodyEnd[2];
    }
    return qtrue;
}
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
/* Windows prone solver. Keep the existing defined no-gravity end velocity;
 * the original reads uninitialized stack data on that path, so this remains
 * an explicit fidelity limit rather than a claim of complete native parity. */
typedef char slideTraceLayout829[(sizeof(trace_t) == 56 && offsetof(trace_t, allsolid) == 0 &&
    offsetof(trace_t, startsolid) == 4 && offsetof(trace_t, fraction) == 8 &&
    offsetof(trace_t, endpos) == 12 && offsetof(trace_t, plane) + offsetof(cplane_t, normal) == 24 &&
    offsetof(trace_t, entityNum) == 52) ? 1 : -1];

static qboolean PM_ProneLegProbeWindows(const vec3_t bodyEnd, qboolean advance,
    const vec3_t mins, const vec3_t maxs, float scale) {
    vec3_t start, end;
    trace_t trace;
    unsigned timeBits;
    int reversedTime;

    start[0] = (float)((long double)bodyEnd[0] - (long double)pml.forward[0] * scale * 32.0f);
    start[1] = (float)((long double)bodyEnd[1] - (long double)pml.forward[1] * scale * 32.0f);
    start[2] = (float)((long double)bodyEnd[2] + 24.0f);
    end[0] = start[0];
    end[1] = start[1];
    end[2] = (float)((long double)start[2] - 21.6f - 24.0f);
    pm->trace(&trace, start, mins, maxs, end, pm->ps->clientNum, pm->tracemask);
    if (trace.startsolid) {
        pm->ps->velocity[2] = 0.0f;
        pm->pmext->proneGroundTime = pm->cmd.serverTime;
        return qfalse;
    }
    if (trace.fraction >= 1.0f) {
        if (advance) VectorCopy(bodyEnd, pm->ps->origin);
        pm->pmext->proneLegsOffset = start[2];
        return qtrue;
    }
    VectorCopy(trace.endpos, start);
    VectorCopy(start, end);
    end[2] = (float)((long double)start[2] + 21.6f);
    pm->trace(&trace, start, mins, maxs, end, pm->ps->clientNum, pm->tracemask);
    if (trace.allsolid) {
        pm->ps->velocity[2] = 0.0f;
        if (advance) {
            timeBits = 0u - (unsigned)pm->cmd.serverTime;
            memcpy(&reversedTime, &timeBits, sizeof(reversedTime));
            pm->pmext->proneTime = reversedTime;
        }
        return qfalse;
    }
    if (advance) VectorCopy(bodyEnd, pm->ps->origin);
    pm->pmext->proneLegsOffset = (float)((long double)start[2] - bodyEnd[2]);
    return qtrue;
}

qboolean PM_SlideMoveProne(qboolean gravity) {
    vec3_t legsMins = {-13.5f, -13.5f, -24.0f};
    vec3_t legsMaxs = {13.5f, 13.5f, -14.4f};
    vec3_t primalVelocity, endVelocity = {0.0f, 0.0f, 0.0f};
    vec3_t planes[MAX_CLIP_PLANES], end, clipVelocity, endClipVelocity, direction;
    trace_t trace;
    float legsScale = 1.0f;
    volatile float timeLeft;
    long double dot, into, distance;
    int bumpcount, numplanes, i, j, k, axis;

    if (pm->ps->stats[STAT_TCE_FLAGS] & 0x200) {
        legsScale = 1.25f;
        for (axis = 0; axis < 3; ++axis) {
            legsMins[axis] = (float)((long double)legsMins[axis] * 1.25f);
        }
        for (axis = 0; axis < 3; ++axis) {
            legsMaxs[axis] = (float)((long double)legsMaxs[axis] * 1.25f);
        }
    }
    VectorCopy(pm->ps->velocity, primalVelocity);
    if (gravity) {
        VectorCopy(pm->ps->velocity, endVelocity);
        endVelocity[2] = (float)((long double)endVelocity[2] -
            (long double)pm->ps->gravity * pml.frametime);
        pm->ps->velocity[2] = (float)(((long double)endVelocity[2] + pm->ps->velocity[2]) * 0.5);
        primalVelocity[2] = endVelocity[2];
        if (pml.groundPlane) {
            PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal, pm->ps->velocity, OVERCLIP);
        }
    }
    timeLeft = pml.frametime;
    numplanes = 0;
    if (pml.groundPlane) {
        VectorCopy(pml.groundTrace.plane.normal, planes[0]);
        numplanes = 1;
    }
    VectorNormalize2(pm->ps->velocity, planes[numplanes]);
    ++numplanes;
    for (bumpcount = 0; bumpcount < 4; ++bumpcount) {
        for (axis = 0; axis < 3; ++axis) {
            end[axis] = (float)((long double)timeLeft * pm->ps->velocity[axis] + pm->ps->origin[axis]);
        }
        pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, end,
            pm->ps->clientNum, pm->tracemask);
        if (trace.allsolid) {
            pm->ps->velocity[2] = 0.0f;
            pm->pmext->proneGroundTime = pm->cmd.serverTime;
            return qtrue;
        }
        if (trace.fraction > 0.0f &&
            !PM_ProneLegProbeWindows(trace.endpos, qtrue, legsMins, legsMaxs, legsScale)) return qtrue;
        /* Native C3 repeats the leg probe for equality or unordered fraction. */
        if (trace.fraction == 1.0f || trace.fraction != trace.fraction) {
            if (!PM_ProneLegProbeWindows(trace.endpos, qfalse, legsMins, legsMaxs, legsScale)) return qtrue;
            break;
        }
        PM_AddTouchEnt(trace.entityNum);
        timeLeft = (float)((long double)timeLeft - (long double)trace.fraction * timeLeft);
        if (numplanes >= MAX_CLIP_PLANES) {
            pm->ps->velocity[2] = 0.0f;
            pm->ps->velocity[1] = 0.0f;
            pm->ps->velocity[0] = 0.0f;
            pm->pmext->proneGroundTime = pm->cmd.serverTime;
            return qtrue;
        }
        for (i = 0; i < numplanes; ++i) {
            dot = (long double)trace.plane.normal[2] * planes[i][2];
            dot += (long double)trace.plane.normal[0] * planes[i][0];
            dot += (long double)trace.plane.normal[1] * planes[i][1];
            if (dot > (long double)0.99) {
                for (axis = 0; axis < 3; ++axis) {
                    pm->ps->velocity[axis] = (float)((long double)trace.plane.normal[axis] + pm->ps->velocity[axis]);
                }
                break;
            }
        }
        if (i < numplanes) continue;
        VectorCopy(trace.plane.normal, planes[numplanes]);
        ++numplanes;
        for (i = 0; i < numplanes; ++i) {
            into = (long double)planes[i][0] * pm->ps->velocity[0];
            into += (long double)planes[i][2] * pm->ps->velocity[2];
            into += (long double)planes[i][1] * pm->ps->velocity[1];
            if (into >= (long double)0.1) continue;
            if (-into > pml.impactSpeed) pml.impactSpeed = (float)-into;
            PM_ClipVelocity(pm->ps->velocity, planes[i], clipVelocity, OVERCLIP);
            PM_ClipVelocity(endVelocity, planes[i], endClipVelocity, OVERCLIP);
            for (j = 0; j < numplanes; ++j) {
                if (j == i) continue;
                dot = (long double)clipVelocity[2] * planes[j][2];
                dot += (long double)clipVelocity[1] * planes[j][1];
                dot += (long double)clipVelocity[0] * planes[j][0];
                if (dot >= (long double)0.1) continue;
                PM_ClipVelocity(clipVelocity, planes[j], clipVelocity, OVERCLIP);
                PM_ClipVelocity(endClipVelocity, planes[j], endClipVelocity, OVERCLIP);
                dot = (long double)clipVelocity[2] * planes[i][2];
                dot += (long double)clipVelocity[1] * planes[i][1];
                dot += (long double)clipVelocity[0] * planes[i][0];
                if (dot >= 0.0L) continue;
                CrossProduct(planes[i], planes[j], direction);
                (void)TCE_VectorNormalizeValue(direction);
                distance = (long double)direction[2] * pm->ps->velocity[2];
                distance += (long double)direction[1] * pm->ps->velocity[1];
                distance += (long double)direction[0] * pm->ps->velocity[0];
                for (axis = 0; axis < 3; ++axis) clipVelocity[axis] = (float)((long double)direction[axis] * distance);
                CrossProduct(planes[i], planes[j], direction);
                (void)TCE_VectorNormalizeValue(direction);
                distance = (long double)direction[2] * endVelocity[2];
                distance += (long double)direction[1] * endVelocity[1];
                distance += (long double)direction[0] * endVelocity[0];
                for (axis = 0; axis < 3; ++axis) endClipVelocity[axis] = (float)((long double)direction[axis] * distance);
                for (k = 0; k < numplanes; ++k) {
                    if (k == i || k == j) continue;
                    dot = (long double)clipVelocity[2] * planes[k][2];
                    dot += (long double)clipVelocity[0] * planes[k][0];
                    dot += (long double)clipVelocity[1] * planes[k][1];
                    if (dot >= (long double)0.1) continue;
                    pm->ps->velocity[2] = 0.0f;
                    pm->ps->velocity[1] = 0.0f;
                    pm->ps->velocity[0] = 0.0f;
                    pm->pmext->proneGroundTime = pm->cmd.serverTime;
                    return qtrue;
                }
            }
            VectorCopy(clipVelocity, pm->ps->velocity);
            VectorCopy(endClipVelocity, endVelocity);
            break;
        }
    }
    if (gravity) VectorCopy(endVelocity, pm->ps->velocity);
    if (pm->ps->pm_time) VectorCopy(primalVelocity, pm->ps->velocity);
    if (!bumpcount && !gravity) pm->pmext->proneGroundTime = pm->cmd.serverTime;
    return bumpcount != 0;
}
#else
qboolean PM_SlideMoveProne( qboolean gravity ) {
	vec3_t legsMins = {-13.5f, -13.5f, -24.0f};
	vec3_t legsMaxs = {13.5f, 13.5f, -14.4f};
	float legsScale = 1.0f;
	int			bumpcount, numbumps;
	vec3_t		dir;
	float		d;
	int			numplanes;
	vec3_t		planes[MAX_CLIP_PLANES];
	vec3_t		primal_velocity;
	vec3_t		clipVelocity;
	int			i, j, k;
	trace_t	trace;
	vec3_t		end;
	float		time_left;
	float		into;
	vec3_t		endVelocity;
	vec3_t		endClipVelocity;
	
	/* Original captures these once, before any collision callback. */
	if (pm->ps->stats[STAT_TCE_FLAGS] & 0x200) {
		legsScale = 1.25f;
		VectorScale(legsMins, legsScale, legsMins);
		VectorScale(legsMaxs, legsScale, legsMaxs);
	}
	numbumps = 4;

	VectorCopy (pm->ps->velocity, primal_velocity);

	if ( gravity ) {
		VectorCopy( pm->ps->velocity, endVelocity );
		endVelocity[2] -= pm->ps->gravity * pml.frametime;
		pm->ps->velocity[2] = ( pm->ps->velocity[2] + endVelocity[2] ) * 0.5;
		primal_velocity[2] = endVelocity[2];
		if ( pml.groundPlane ) {
			// slide along the ground plane
			PM_ClipVelocity (pm->ps->velocity, pml.groundTrace.plane.normal, 
				pm->ps->velocity, OVERCLIP );
		}
	} else {
		VectorClear( endVelocity );
	}

	time_left = pml.frametime;

	// never turn against the ground plane
	if ( pml.groundPlane ) {
		numplanes = 1;
		VectorCopy( pml.groundTrace.plane.normal, planes[0] );
	} else {
		numplanes = 0;
	}

	// never turn against original velocity
	VectorNormalize2( pm->ps->velocity, planes[numplanes] );
	numplanes++;

	for ( bumpcount=0 ; bumpcount < numbumps ; bumpcount++ ) {

		// calculate position we are trying to move to
		VectorMA( pm->ps->origin, time_left, pm->ps->velocity, end );

		// see if we can make it there
        pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, end,
                  pm->ps->clientNum, pm->tracemask);

		if (trace.allsolid) {
			// entity is completely trapped in another solid
			pm->ps->velocity[2] = 0;	// don't build up falling damage, but allow sideways acceleration
			pm->pmext->proneGroundTime = pm->cmd.serverTime; return qtrue;
		}

        if (trace.fraction > 0) {
            if (!PM_TCEProneProbe(trace.endpos, qtrue, legsMins, legsMaxs, legsScale)) return qtrue;
            VectorCopy(trace.endpos, pm->ps->origin);
        }
        if (trace.fraction == 1) {
            if (!PM_TCEProneProbe(trace.endpos, qfalse, legsMins, legsMaxs, legsScale)) return qtrue;
            break;
        }

		// save entity for contact
		PM_AddTouchEnt( trace.entityNum );

		time_left -= time_left * trace.fraction;

		if (numplanes >= MAX_CLIP_PLANES) {
			// this shouldn't really happen
			VectorClear( pm->ps->velocity );
			pm->pmext->proneGroundTime = pm->cmd.serverTime; return qtrue;
		}

		//
		// if this is the same plane we hit before, nudge velocity
		// out along it, which fixes some epsilon issues with
		// non-axial planes
		//
		for ( i = 0 ; i < numplanes ; i++ ) {
			if ( DotProduct( trace.plane.normal, planes[i] ) > 0.99 ) {
                VectorAdd(trace.plane.normal, pm->ps->velocity, pm->ps->velocity);
				break;
			}
		}
		if ( i < numplanes ) {
			continue;
		}
		VectorCopy (trace.plane.normal, planes[numplanes]);
		numplanes++;

		//
		// modify velocity so it parallels all of the clip planes
		//

		// find a plane that it enters
		for ( i = 0 ; i < numplanes ; i++ ) {
			into = DotProduct( pm->ps->velocity, planes[i] );
			if ( into >= 0.1 ) {
				continue;		// move doesn't interact with the plane
			}

			// see how hard we are hitting things
			if ( -into > pml.impactSpeed ) {
				pml.impactSpeed = -into;
			}

			// slide along the plane
			PM_ClipVelocity (pm->ps->velocity, planes[i], clipVelocity, OVERCLIP );

			// slide along the plane
			PM_ClipVelocity (endVelocity, planes[i], endClipVelocity, OVERCLIP );

			// see if there is a second plane that the new move enters
			for ( j = 0 ; j < numplanes ; j++ ) {
				if ( j == i ) {
					continue;
				}
				if ( DotProduct( clipVelocity, planes[j] ) >= 0.1 ) {
					continue;		// move doesn't interact with the plane
				}

				// try clipping the move to the plane
				PM_ClipVelocity( clipVelocity, planes[j], clipVelocity, OVERCLIP );
				PM_ClipVelocity( endClipVelocity, planes[j], endClipVelocity, OVERCLIP );

				// see if it goes back into the first clip plane
				if ( DotProduct( clipVelocity, planes[i] ) >= 0 ) {
					continue;
				}

				// slide the original velocity along the crease
				CrossProduct (planes[i], planes[j], dir);
				VectorNormalize( dir );
				d = DotProduct( dir, pm->ps->velocity );
				VectorScale( dir, d, clipVelocity );

				CrossProduct (planes[i], planes[j], dir);
				VectorNormalize( dir );
				d = DotProduct( dir, endVelocity );
				VectorScale( dir, d, endClipVelocity );

				// see if there is a third plane the the new move enters
				for ( k = 0 ; k < numplanes ; k++ ) {
					if ( k == i || k == j ) {
						continue;
					}
					if ( DotProduct( clipVelocity, planes[k] ) >= 0.1 ) {
						continue;		// move doesn't interact with the plane
					}

					// stop dead at a tripple plane interaction
					VectorClear( pm->ps->velocity );
					pm->pmext->proneGroundTime = pm->cmd.serverTime; return qtrue;
				}
			}

			// if we have fixed all interactions, try another move
			VectorCopy( clipVelocity, pm->ps->velocity );
			VectorCopy( endClipVelocity, endVelocity );
			break;
		}
	}

	if ( gravity ) {
		VectorCopy( endVelocity, pm->ps->velocity );
	}

	// don't change velocity if in a timer (FIXME: is this correct?)
	if ( pm->ps->pm_time ) {
		VectorCopy( primal_velocity, pm->ps->velocity );
	}

	if (!bumpcount && !gravity) pm->pmext->proneGroundTime = pm->cmd.serverTime;
	return ( bumpcount != 0 );
}

#endif

/* TC:E prone step controller: direct trace callbacks retain aliased endpoints.
 * Windows float stores and comparison gates follow the original listings;
 * FPU exception/control state and compiled engine ABI remain separately open. */
#if defined(_MSC_VER) && defined(_M_IX86)
typedef char stepTraceLayout828[(sizeof(trace_t) == 56 && offsetof(trace_t, allsolid) == 0 &&
    offsetof(trace_t, fraction) == 8 && offsetof(trace_t, endpos) == 12 &&
    offsetof(trace_t, plane) + offsetof(cplane_t, normal) == 24) ? 1 : -1];
typedef char stepEventIds828[(EV_STEP_4 == 12 && EV_STEP_8 == 13 && EV_STEP_12 == 14 && EV_STEP_16 == 15) ? 1 : -1];
void PM_StepSlideMoveProne(qboolean gravity) {
    vec3_t startOrigin, startVelocity, up, down;
    trace_t trace;
    long double height;
    volatile float delta;

    VectorCopy(pm->ps->origin, startOrigin);
    VectorCopy(pm->ps->velocity, startVelocity);
    if (!PM_SlideMoveProne(gravity)) return;
    VectorCopy(startOrigin, down);
    down[2] = (float)((long double)startOrigin[2] - 18.0f);
    pm->trace(&trace, startOrigin, pm->mins, pm->maxs, down,
        pm->ps->clientNum, pm->tracemask);
    if (pm->ps->velocity[2] > 0.0f) {
        /* C3 accepts equality or unordered fraction; C0 also rejects
         * an unordered normal when checking the minimum landing slope. */
        if (trace.fraction == 1.0f || trace.fraction != trace.fraction ||
            (long double)trace.plane.normal[2] < (long double)0.7 ||
            trace.plane.normal[2] != trace.plane.normal[2]) return;
    }
    VectorCopy(startOrigin, up);
    up[2] = (float)((long double)startOrigin[2] + 18.0f);
    pm->trace(&trace, up, pm->mins, pm->maxs, up,
        pm->ps->clientNum, pm->tracemask);
    if (trace.allsolid) {
        if (pm->debugLevel) Com_Printf("%i:bend can't step\n", c_pmove);
        return;
    }
    VectorCopy(up, pm->ps->origin);
    VectorCopy(startVelocity, pm->ps->velocity);
    PM_SlideMoveProne(gravity);
    VectorCopy(pm->ps->origin, down);
    down[2] = (float)((long double)down[2] - 18.0f);
    pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, down,
        pm->ps->clientNum, pm->tracemask);
    if (!trace.allsolid) VectorCopy(trace.endpos, pm->ps->origin);
    if (trace.fraction < 1.0f || trace.fraction != trace.fraction) {
        PM_ClipVelocity(pm->ps->velocity, trace.plane.normal, pm->ps->velocity, OVERCLIP);
    }
    height = (long double)pm->ps->origin[2] - startOrigin[2];
    delta = (float)height;
    /* The initial gate uses the held subtraction; event bins use its float store. */
    if (height > 2.0L) {
        PM_AddEvent(delta < 7.0f ? EV_STEP_4 : delta < 11.0f ? EV_STEP_8 :
            delta < 15.0f ? EV_STEP_12 : EV_STEP_16);
    }
    if (pm->debugLevel) Com_Printf("%i:stepped\n", c_pmove);
}
#else
/* Portable fallback; no Linux instruction-level parity claim. */
void PM_StepSlideMoveProne(qboolean gravity) {
    vec3_t startOrigin, startVelocity, up, down;
    trace_t trace;
    float delta;
    VectorCopy(pm->ps->origin, startOrigin); VectorCopy(pm->ps->velocity, startVelocity);
    if (!PM_SlideMoveProne(gravity)) return;
    VectorCopy(startOrigin, down); down[2] -= 18.0f;
    pm->trace(&trace, startOrigin, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask);
    if (pm->ps->velocity[2] > 0 && (trace.fraction == 1.0f || trace.plane.normal[2] < 0.7)) return;
    VectorCopy(startOrigin, up); up[2] += 18.0f;
    pm->trace(&trace, up, pm->mins, pm->maxs, up, pm->ps->clientNum, pm->tracemask);
    if (trace.allsolid) {
        if (pm->debugLevel) Com_Printf("%i:bend can't step\n", c_pmove);
        return;
    }
    VectorCopy(up, pm->ps->origin); VectorCopy(startVelocity, pm->ps->velocity);
    PM_SlideMoveProne(gravity);
    VectorCopy(pm->ps->origin, down); down[2] -= 18.0f;
    pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, down, pm->ps->clientNum, pm->tracemask);
    if (!trace.allsolid) VectorCopy(trace.endpos, pm->ps->origin);
    if (trace.fraction < 1.0f) PM_ClipVelocity(pm->ps->velocity, trace.plane.normal, pm->ps->velocity, OVERCLIP);
    delta = pm->ps->origin[2] - startOrigin[2];
    if (delta > 2.0f) PM_AddEvent(delta < 7.0f ? EV_STEP_4 : delta < 11.0f ? EV_STEP_8 : delta < 15.0f ? EV_STEP_12 : EV_STEP_16);
    if (pm->debugLevel) Com_Printf("%i:stepped\n", c_pmove);
}

#endif
