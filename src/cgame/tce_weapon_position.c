#include "../game/q_shared.h"
#include "../game/tce_bg.h"
#include "tce_weapon_position.h"

#if defined(_MSC_VER) && defined(_M_IX86)
typedef char tceWeaponPositionNativeLayout[
    sizeof(float) == 4 && sizeof(int) == 4 &&
    offsetof(tce_weaponPosition_t, viewAxis) ==
        offsetof(tce_weaponPosition_t, viewOrigin) + 3 * sizeof(float) ? 1 : -1];
/* Original30073270..3007422d: complete local Windows x87 schedule.
 * State offsets are native projection offsets, not original cg_t addresses.
 * The non-Windows fallback below is not an x87 precision-parity claim. */
enum {
    swayState_viewAngles = offsetof(tce_weaponPosition_t, viewAngles),
    swayState_thirdPerson = offsetof(tce_weaponPosition_t, thirdPerson),
    swayState_weaponState = offsetof(tce_weaponPosition_t, weaponState),
    swayState_mountedPitch = offsetof(tce_weaponPosition_t, mountedPitch),
    swayState_postureModified = offsetof(tce_weaponPosition_t, postureModified),
    swayState_proneMovingTime = offsetof(tce_weaponPosition_t, proneMovingTime),
    swayState_flags_3407dfb0 = offsetof(tce_weaponPosition_t, flags_3407dfb0),
    swayState_stanceTime = offsetof(tce_weaponPosition_t, stanceTime),
    swayState_pmFlags = offsetof(tce_weaponPosition_t, pmFlags),
    swayState_duckTime = offsetof(tce_weaponPosition_t, duckTime),
    swayState_lean = offsetof(tce_weaponPosition_t, lean),
    swayState_leanTime = offsetof(tce_weaponPosition_t, leanTime),
    swayState_shotTime = offsetof(tce_weaponPosition_t, shotTime),
    swayState_count_3407dff8 = offsetof(tce_weaponPosition_t, count_3407dff8),
    swayState_firemodeTime = offsetof(tce_weaponPosition_t, firemodeTime),
    swayState_viewOrigin = offsetof(tce_weaponPosition_t, viewOrigin),
    swayState_time = offsetof(tce_weaponPosition_t, time),
    swayState_weapon = offsetof(tce_weaponPosition_t, weapon),
    swayState_bobCycle = offsetof(tce_weaponPosition_t, bobCycle),
    swayState_speed = offsetof(tce_weaponPosition_t, speed),
    swayState_aiming = offsetof(tce_weaponPosition_t, aiming),
    swayState_scopeEnabled = offsetof(tce_weaponPosition_t, scopeEnabled),
    swayState_swayTime = offsetof(tce_weaponPosition_t, swayTime),
    swayState_priorForward = offsetof(tce_weaponPosition_t, priorForward),
    swayState_stepTime = offsetof(tce_weaponPosition_t, stepTime),
    swayState_eFlags = offsetof(tce_weaponPosition_t, eFlags),
    swayState_velocity = offsetof(tce_weaponPosition_t, velocity),
    swayState_proneTime = offsetof(tce_weaponPosition_t, proneTime),
    swayState_stepChange = offsetof(tce_weaponPosition_t, stepChange),
    swayState_scale_3407dfbc = offsetof(tce_weaponPosition_t, scale_3407dfbc),
    swayState_scale_3407dfc0 = offsetof(tce_weaponPosition_t, scale_3407dfc0),
    swayState_swayHorizontal = offsetof(tce_weaponPosition_t, swayHorizontal),
    swayState_swayVertical = offsetof(tce_weaponPosition_t, swayVertical),
    swayState_bobSin = offsetof(tce_weaponPosition_t, bobSin),
    swayState_viewAxis = offsetof(tce_weaponPosition_t, viewAxis),
    swayState_landTime = offsetof(tce_weaponPosition_t, landTime),
    swayState_landChange = offsetof(tce_weaponPosition_t, landChange),
    swayState_gunViewAngles = offsetof(tce_weaponPosition_t, gunViewAngles),
    swayState_developer = offsetof(tce_weaponPosition_t, developer),
    swayState_developerAngles = offsetof(tce_weaponPosition_t, developerAngles),
    swayState_tacticalScale = offsetof(tce_weaponPosition_t, tacticalScale),
    swayState_tacticalPitch = offsetof(tce_weaponPosition_t, tacticalPitch),
    swayState_tacticalYaw = offsetof(tce_weaponPosition_t, tacticalYaw),
    swayState_kickAngles = offsetof(tce_weaponPosition_t, kickAngles),
    swayWeaponStride = sizeof(tce_weaponDef_t),
    swayWeaponNoTacMode = offsetof(tce_weaponDef_t, noTacMode),
    swayWeaponUsesRecoil = offsetof(tce_weaponDef_t, usesRecoilAnimMod),
    swayWeaponPump = offsetof(tce_weaponDef_t, pump),
    swayWeaponSemiauto = offsetof(tce_weaponDef_t, semiauto),
    swayWeaponUsesPistol = offsetof(tce_weaponDef_t, usesPistolAnimMod),
    swayWeaponScoped = offsetof(tce_weaponDef_t, scoped),
    swayWeaponLimit = offsetof(tce_weaponDef_t, unknown_0f8) + 3 * sizeof(int)
};
static const unsigned int swayConst300922b4 = 0x3f800000u;
static const unsigned int swayConst300924d8 = 0x3a83126fu;
static const unsigned int swayConst30092f8c = 0x3ba3d70au;
static const unsigned int swayConst30092a00 = 0x3dcccccdu;
static const unsigned int swayConst30092d74 = 0x443b8000u;
static const unsigned int swayConst300928b4 = 0x437a0000u;
static const unsigned int swayConst30093030 = 0x3ada740eu;
static const unsigned int swayConst300922b8 = 0x3f000000u;
static const unsigned int swayConst300923ec = 0x40400000u;
static const unsigned int swayConst30092a30[2] = { 0x9999999au, 0x3fc99999u };
static const unsigned int swayConst30092520 = 0x3dcccccdu;
static const unsigned int swayConst30092c40 = 0x3f68ba2eu;
static const unsigned int swayConst30092490 = 0x3e800000u;
static const unsigned int swayConst30092ad0[2] = { 0x47ae147bu, 0x3f747ae1u };
static const unsigned int swayConst30092c70[2] = { 0x47ae147bu, 0x3f847ae1u };
static const unsigned int swayConst300920e0 = 0x00000000u;
static const unsigned int swayConst30093028[2] = { 0xb4e81b4fu, 0x3f5b4e81u };
static const unsigned int swayConst30093020[2] = { 0xb4e81b4fu, 0x3f4b4e81u };
static const unsigned int swayConst300923fc = 0x44fa0000u;
static const unsigned int swayConst300923f8 = 0x3c23d70au;
static const unsigned int swayConst30092318[2] = { 0xd2f1a9fcu, 0x3f50624du };
static const unsigned int swayConst300924e0 = 0x42a00000u;
static const unsigned int swayConst30092858[2] = { 0xaaaaaaabu, 0x3feaaaaau };
static const unsigned int swayConst30092488 = 0x43480000u;
static const unsigned int swayConst30093038 = 0xc1c80000u;
static const unsigned int swayConst3009248c = 0x40a00000u;
static const unsigned int swayConst30092ff0 = 0xc0a00000u;
static const unsigned int swayConst300923d0 = 0x42700000u;
static const unsigned int swayConst300924d0 = 0x41200000u;
static const unsigned int swayConst30092398 = 0x41f00000u;
static const unsigned int swayConst300923d4 = 0xc1a00000u;
static const unsigned int swayConst300922ec = 0x3ba3d70au;
static const unsigned int swayConst300923b8 = 0x3b5a740eu;
static const unsigned int swayConst30092950[2] = { 0x60000000u, 0x400921fbu };
static const unsigned int swayConst30093034 = 0xc0800000u;
static const unsigned int swayConst300922bc = 0x40800000u;
static const unsigned int swayConst30092a68 = 0x3b03126fu;
static const unsigned int swayConst30092c30 = 0x3ca3d70au;
static const unsigned int swayConst30092f88 = 0x3aaec33eu;
static const unsigned int swayConst30092310 = 0x40c90fdbu;
static const unsigned int swayConst300922c8[2] = { 0x00000000u, 0x3ff00000u };
static const unsigned int swayConst300922e0[2] = { 0x00000000u, 0x3fe00000u };
static const unsigned int swayConst30092c28 = 0xc0000000u;
/* Original30088628 consumes ST0, preserves deeper x87 entries and returns
 * truncated int64 in EDX:EAX with the saved control word restored. */
static __declspec(naked) void TCE_WeaponPositionTruncateST0(void) {
    __asm {
        PUSH EBP
        MOV EBP,ESP
        SUB ESP,12
        FWAIT
        FNSTCW word ptr [EBP - 2]
        FWAIT
        MOV AX,word ptr [EBP - 2]
        OR AH,0x0c
        MOV word ptr [EBP - 4],AX
        FLDCW word ptr [EBP - 4]
        FISTP qword ptr [EBP - 12]
        FLDCW word ptr [EBP - 2]
        MOV EAX,dword ptr [EBP - 12]
        MOV EDX,dword ptr [EBP - 8]
        LEAVE
        RET
    }
}

static __declspec(naked) void TCE_WeaponPositionNative(
    tce_weaponPosition_t *s, float *origin, float *angles) {
    __asm {
        PUSH EBX
        PUSH ESI
        PUSH EDI
        PUSH EBP
        MOV EBP,dword ptr [ESP + 20]
        MOV ESI,dword ptr [ESP + 24]
        MOV EDI,dword ptr [ESP + 28]
        SUB ESP,0x40
        MOV dword ptr [ESP + 0x10],0x3f800000
        MOV dword ptr [ESP + 0x14],0x3f800000
        MOV EAX,[EBP + swayState_weapon] // 30073273
        LEA EDX,[EBP + swayState_viewOrigin] // 30073278
        MOV EAX,dword ptr [EDX + 0] // 300732ae
        MOV dword ptr [ESI],EAX // 300732b1
        LEA ECX,[EBP + swayState_viewOrigin] // 300732b3
        MOV EDX,dword ptr [ECX + 4] // 300732b9
        MOV dword ptr [ESI + 0x4],EDX // 300732bc
        LEA EAX,[EBP + swayState_viewOrigin] // 300732bf
        MOV ECX,dword ptr [EAX + 8] // 300732c4
        MOV dword ptr [ESI + 0x8],ECX // 300732c7
        FLD dword ptr [EBP + swayState_viewAngles] // 300732ca
        FSTP dword ptr [EDI] // 300732d0
        MOV EDX,dword ptr [EBP + swayState_viewAngles + 4] // 300732d2
        MOV dword ptr [EDI + 0x4],EDX // 300732d8
        MOV EAX,[EBP + swayState_viewAngles + 8] // 300732db
        MOV dword ptr [EDI + 0x8],EAX // 300732e0
        MOV EAX,[EBP + swayState_eFlags] // 300732e3
        TEST AH,0x80 // 300732e8
        JZ sway300732fb // 300732eb
        FLD dword ptr [EBP + swayState_viewAngles] // 300732ed
        FMUL qword ptr [swayConst30092858] // 300732f3
        FSTP dword ptr [EDI] // 300732f9
sway300732fb:
        MOV EAX,[EBP + swayState_thirdPerson] // 300732fb
        MOV EDX,0x1 // 30073300
        TEST EAX,EAX // 30073305
        JNZ sway30073328 // 30073307
        MOV EAX,[EBP + swayState_weapon] // 30073309
        CMP EAX,0x3c // 3007330e
        JZ sway30073318 // 30073311
        CMP EAX,0x3e // 30073313
        JNZ sway30073328 // 30073316
sway30073318:
        CMP dword ptr [EBP + swayState_weaponState],EDX // 30073318
        JZ sway30073328 // 3007331e
        MOV ECX,dword ptr [EBP + swayState_mountedPitch] // 30073320
        MOV dword ptr [EDI],ECX // 30073326
sway30073328:
        MOV EAX,[EBP + swayState_eFlags] // 30073328
        MOV ECX,dword ptr [EBP + swayState_time] // 3007332d
        TEST EAX,0x100000 // 30073335
        MOV dword ptr [EBP + swayState_postureModified],0x0 // 3007333a
        JZ sway3007343c // 30073344
        MOV EBX,dword ptr [EBP + swayState_proneMovingTime] // 3007334a
        MOV EAX,ECX // 30073350
        SUB EAX,EBX // 30073352
        TEST EAX,EAX // 30073354
        MOV dword ptr [ESP + 0x38],EAX // 30073356
        JLE sway30073431 // 3007335a
        CMP EAX,0xc8 // 30073360
        JLE sway3007336f // 30073365
        FLD dword ptr [swayConst300922b4] // 30073367
        JMP sway3007337f // 3007336d
sway3007336f:
        FILD dword ptr [ESP + 0x38] // 3007336f
        FDIVR dword ptr [swayConst30092488] // 30073373
        FDIVR dword ptr [swayConst300922b4] // 30073379
sway3007337f:
        LEA EAX,[EBP + swayState_viewOrigin] // 3007337f
        FLD ST(0) // 30073384
        FMUL dword ptr [swayConst30093038] // 30073386
        FLD ST(0) // 3007338c
        FMUL dword ptr [EAX + 12] // 3007338e
        FADD dword ptr [ESI] // 30073391
        FSTP dword ptr [ESI] // 30073393
        LEA ECX,[EBP + swayState_viewOrigin] // 30073395
        FLD ST(0) // 3007339b
        FMUL dword ptr [ECX + 16] // 3007339d
        FADD dword ptr [ESI + 0x4] // 300733a0
        FSTP dword ptr [ESI + 0x4] // 300733a3
        LEA EAX,[EBP + swayState_viewOrigin] // 300733a6
        FMUL dword ptr [EAX + 20] // 300733ab
        FADD dword ptr [ESI + 0x8] // 300733ae
        FSTP dword ptr [ESI + 0x8] // 300733b1
        LEA ECX,[EBP + swayState_viewOrigin] // 300733b4
        FLD ST(0) // 300733ba
        FMUL dword ptr [swayConst3009248c] // 300733bc
        FLD ST(0) // 300733c2
        FMUL dword ptr [ECX + 24] // 300733c4
        FADD dword ptr [ESI] // 300733c7
        FSTP dword ptr [ESI] // 300733c9
        LEA EAX,[EBP + swayState_viewOrigin] // 300733cb
        FLD ST(0) // 300733d0
        FMUL dword ptr [EAX + 28] // 300733d2
        FADD dword ptr [ESI + 0x4] // 300733d5
        FSTP dword ptr [ESI + 0x4] // 300733d8
        LEA ECX,[EBP + swayState_viewOrigin] // 300733db
        FMUL dword ptr [ECX + 32] // 300733e1
        FADD dword ptr [ESI + 0x8] // 300733e4
        FSTP dword ptr [ESI + 0x8] // 300733e7
        LEA EAX,[EBP + swayState_viewOrigin] // 300733ea
        FLD ST(0) // 300733ef
        FMUL dword ptr [swayConst30092ff0] // 300733f1
        FLD ST(0) // 300733f7
        FMUL dword ptr [EAX + 36] // 300733f9
        FADD dword ptr [ESI] // 300733fc
        FSTP dword ptr [ESI] // 300733fe
        LEA ECX,[EBP + swayState_viewOrigin] // 30073400
        FLD ST(0) // 30073406
        FMUL dword ptr [ECX + 40] // 30073408
        FADD dword ptr [ESI + 0x4] // 3007340b
        FSTP dword ptr [ESI + 0x4] // 3007340e
        LEA EAX,[EBP + swayState_viewOrigin] // 30073411
        FMUL dword ptr [EAX + 44] // 30073416
        FADD dword ptr [ESI + 0x8] // 30073419
        FSTP dword ptr [ESI + 0x8] // 3007341c
        FMUL dword ptr [swayConst300923d0] // 3007341f
        FADD dword ptr [EDI + 0x4] // 30073425
        FSTP dword ptr [EDI + 0x4] // 30073428
        MOV ECX,dword ptr [EBP + swayState_time] // 3007342b
sway30073431:
        MOV dword ptr [EBP + swayState_postureModified],EDX // 30073431
        JMP sway3007352d // 30073437
sway3007343c:
        MOV EAX,[EBP + swayState_proneMovingTime] // 3007343c
        ADD EAX,ECX // 30073441
        CMP EAX,0xc8 // 30073443
        MOV dword ptr [ESP + 0x38],EAX // 30073448
        JGE sway3007352d // 3007344c
        TEST EAX,EAX // 30073452
        JNZ sway3007345e // 30073454
        FLD dword ptr [swayConst300922b4] // 30073456
        JMP sway30073474 // 3007345c
sway3007345e:
        FILD dword ptr [ESP + 0x38] // 3007345e
        FDIVR dword ptr [swayConst30092488] // 30073462
        FDIVR dword ptr [swayConst300922b4] // 30073468
        FSUBR dword ptr [swayConst300922b4] // 3007346e
sway30073474:
        LEA ECX,[EBP + swayState_viewOrigin] // 30073474
        FLD ST(0) // 3007347a
        FMUL dword ptr [swayConst30093038] // 3007347c
        FLD ST(0) // 30073482
        FMUL dword ptr [ECX + 12] // 30073484
        FADD dword ptr [ESI] // 30073487
        FSTP dword ptr [ESI] // 30073489
        LEA EAX,[EBP + swayState_viewOrigin] // 3007348b
        FLD ST(0) // 30073490
        FMUL dword ptr [EAX + 16] // 30073492
        FADD dword ptr [ESI + 0x4] // 30073495
        FSTP dword ptr [ESI + 0x4] // 30073498
        LEA ECX,[EBP + swayState_viewOrigin] // 3007349b
        FMUL dword ptr [ECX + 20] // 300734a1
        FADD dword ptr [ESI + 0x8] // 300734a4
        FSTP dword ptr [ESI + 0x8] // 300734a7
        LEA EAX,[EBP + swayState_viewOrigin] // 300734aa
        FLD ST(0) // 300734af
        FMUL dword ptr [swayConst3009248c] // 300734b1
        FLD ST(0) // 300734b7
        FMUL dword ptr [EAX + 24] // 300734b9
        FADD dword ptr [ESI] // 300734bc
        FSTP dword ptr [ESI] // 300734be
        LEA ECX,[EBP + swayState_viewOrigin] // 300734c0
        FLD ST(0) // 300734c6
        FMUL dword ptr [ECX + 28] // 300734c8
        FADD dword ptr [ESI + 0x4] // 300734cb
        FSTP dword ptr [ESI + 0x4] // 300734ce
        LEA EAX,[EBP + swayState_viewOrigin] // 300734d1
        FMUL dword ptr [EAX + 32] // 300734d6
        FADD dword ptr [ESI + 0x8] // 300734d9
        FSTP dword ptr [ESI + 0x8] // 300734dc
        LEA ECX,[EBP + swayState_viewOrigin] // 300734df
        FLD ST(0) // 300734e5
        FMUL dword ptr [swayConst30092ff0] // 300734e7
        FLD ST(0) // 300734ed
        FMUL dword ptr [ECX + 36] // 300734ef
        FADD dword ptr [ESI] // 300734f2
        FSTP dword ptr [ESI] // 300734f4
        LEA EAX,[EBP + swayState_viewOrigin] // 300734f6
        FLD ST(0) // 300734fb
        FMUL dword ptr [EAX + 40] // 300734fd
        FADD dword ptr [ESI + 0x4] // 30073500
        FSTP dword ptr [ESI + 0x4] // 30073503
        LEA ECX,[EBP + swayState_viewOrigin] // 30073506
        FMUL dword ptr [ECX + 44] // 3007350c
        FADD dword ptr [ESI + 0x8] // 3007350f
        FSTP dword ptr [ESI + 0x8] // 30073512
        FMUL dword ptr [swayConst300923d0] // 30073515
        FADD dword ptr [EDI + 0x4] // 3007351b
        FSTP dword ptr [EDI + 0x4] // 3007351e
        MOV ECX,dword ptr [EBP + swayState_time] // 30073521
        MOV dword ptr [EBP + swayState_postureModified],EDX // 30073527
sway3007352d:
        MOV EAX,[EBP + swayState_flags_3407dfb0] // 3007352d
        TEST AH,0x40 // 30073532
        JZ sway300735ca // 30073535
        MOV EBX,dword ptr [EBP + swayState_stanceTime] // 3007353b
        MOV EAX,ECX // 30073541
        SUB EAX,EBX // 30073543
        TEST EAX,EAX // 30073545
        MOV dword ptr [ESP + 0x38],EAX // 30073547
        JLE sway300735bf // 3007354b
        CMP EAX,0xc8 // 3007354d
        JLE sway3007355c // 30073552
        FLD dword ptr [swayConst300922b4] // 30073554
        JMP sway3007356c // 3007355a
sway3007355c:
        FILD dword ptr [ESP + 0x38] // 3007355c
        FDIVR dword ptr [swayConst30092488] // 30073560
        FDIVR dword ptr [swayConst300922b4] // 30073566
sway3007356c:
        LEA EAX,[EBP + swayState_viewOrigin] // 3007356c
        FLD ST(0) // 30073571
        FMUL dword ptr [swayConst30092ff0] // 30073573
        FLD ST(0) // 30073579
        FMUL dword ptr [EAX + 36] // 3007357b
        FADD dword ptr [ESI] // 3007357e
        FSTP dword ptr [ESI] // 30073580
        LEA ECX,[EBP + swayState_viewOrigin] // 30073582
        FLD ST(0) // 30073588
        FMUL dword ptr [ECX + 40] // 3007358a
        FADD dword ptr [ESI + 0x4] // 3007358d
        FSTP dword ptr [ESI + 0x4] // 30073590
        LEA EAX,[EBP + swayState_viewOrigin] // 30073593
        FMUL dword ptr [EAX + 44] // 30073598
        FADD dword ptr [ESI + 0x8] // 3007359b
        FSTP dword ptr [ESI + 0x8] // 3007359e
        FLD ST(0) // 300735a1
        FMUL dword ptr [swayConst300924d0] // 300735a3
        FADD dword ptr [EDI + 0x4] // 300735a9
        FSTP dword ptr [EDI + 0x4] // 300735ac
        FMUL dword ptr [swayConst30092398] // 300735af
        FADD dword ptr [EDI] // 300735b5
        FSTP dword ptr [EDI] // 300735b7
        MOV ECX,dword ptr [EBP + swayState_time] // 300735b9
sway300735bf:
        MOV dword ptr [EBP + swayState_postureModified],EDX // 300735bf
        JMP sway30073658 // 300735c5
sway300735ca:
        MOV EAX,[EBP + swayState_stanceTime] // 300735ca
        ADD EAX,ECX // 300735cf
        CMP EAX,0xc8 // 300735d1
        MOV dword ptr [ESP + 0x38],EAX // 300735d6
        JGE sway30073658 // 300735da
        TEST EAX,EAX // 300735dc
        JNZ sway300735e8 // 300735de
        FLD dword ptr [swayConst300922b4] // 300735e0
        JMP sway300735fe // 300735e6
sway300735e8:
        FILD dword ptr [ESP + 0x38] // 300735e8
        FDIVR dword ptr [swayConst30092488] // 300735ec
        FDIVR dword ptr [swayConst300922b4] // 300735f2
        FSUBR dword ptr [swayConst300922b4] // 300735f8
sway300735fe:
        LEA ECX,[EBP + swayState_viewOrigin] // 300735fe
        FLD ST(0) // 30073604
        FMUL dword ptr [swayConst30092ff0] // 30073606
        FLD ST(0) // 3007360c
        FMUL dword ptr [ECX + 36] // 3007360e
        FADD dword ptr [ESI] // 30073611
        FSTP dword ptr [ESI] // 30073613
        LEA EAX,[EBP + swayState_viewOrigin] // 30073615
        FLD ST(0) // 3007361a
        FMUL dword ptr [EAX + 40] // 3007361c
        FADD dword ptr [ESI + 0x4] // 3007361f
        FSTP dword ptr [ESI + 0x4] // 30073622
        LEA ECX,[EBP + swayState_viewOrigin] // 30073625
        FMUL dword ptr [ECX + 44] // 3007362b
        FADD dword ptr [ESI + 0x8] // 3007362e
        FSTP dword ptr [ESI + 0x8] // 30073631
        FLD ST(0) // 30073634
        FMUL dword ptr [swayConst300924d0] // 30073636
        FADD dword ptr [EDI + 0x4] // 3007363c
        FSTP dword ptr [EDI + 0x4] // 3007363f
        FMUL dword ptr [swayConst30092398] // 30073642
        FADD dword ptr [EDI] // 30073648
        FSTP dword ptr [EDI] // 3007364a
        MOV ECX,dword ptr [EBP + swayState_time] // 3007364c
        MOV dword ptr [EBP + swayState_postureModified],EDX // 30073652
sway30073658:
        TEST byte ptr [EBP + swayState_pmFlags],0x4 // 30073658
        JZ sway3007372e // 3007365f
        MOV EBX,dword ptr [EBP + swayState_duckTime] // 30073665
        MOV EAX,ECX // 3007366b
        SUB EAX,EBX // 3007366d
        TEST EAX,EAX // 3007366f
        MOV dword ptr [ESP + 0x38],EAX // 30073671
        JLE sway30073723 // 30073675
        CMP EAX,0xc8 // 3007367b
        JLE sway3007368a // 30073680
        FLD dword ptr [swayConst300922b4] // 30073682
        JMP sway3007369a // 30073688
sway3007368a:
        FILD dword ptr [ESP + 0x38] // 3007368a
        FDIVR dword ptr [swayConst30092488] // 3007368e
        FDIVR dword ptr [swayConst300922b4] // 30073694
sway3007369a:
        LEA EAX,[EBP + swayState_viewOrigin] // 3007369a
        FLD ST(0) // 3007369f
        FMUL dword ptr [swayConst30092ff0] // 300736a1
        FLD ST(0) // 300736a7
        FMUL dword ptr [EAX + 12] // 300736a9
        FADD dword ptr [ESI] // 300736ac
        FSTP dword ptr [ESI] // 300736ae
        LEA ECX,[EBP + swayState_viewOrigin] // 300736b0
        FLD ST(0) // 300736b6
        FMUL dword ptr [ECX + 16] // 300736b8
        FADD dword ptr [ESI + 0x4] // 300736bb
        FSTP dword ptr [ESI + 0x4] // 300736be
        LEA EAX,[EBP + swayState_viewOrigin] // 300736c1
        FMUL dword ptr [EAX + 20] // 300736c6
        FADD dword ptr [ESI + 0x8] // 300736c9
        FSTP dword ptr [ESI + 0x8] // 300736cc
        LEA ECX,[EBP + swayState_viewOrigin] // 300736cf
        FLD ST(0) // 300736d5
        FMUL dword ptr [swayConst300923d4] // 300736d7
        FLD ST(0) // 300736dd
        FMUL dword ptr [ECX + 36] // 300736df
        FADD dword ptr [ESI] // 300736e2
        FSTP dword ptr [ESI] // 300736e4
        LEA EAX,[EBP + swayState_viewOrigin] // 300736e6
        FLD ST(0) // 300736eb
        FMUL dword ptr [EAX + 40] // 300736ed
        FADD dword ptr [ESI + 0x4] // 300736f0
        FSTP dword ptr [ESI + 0x4] // 300736f3
        LEA ECX,[EBP + swayState_viewOrigin] // 300736f6
        FMUL dword ptr [ECX + 44] // 300736fc
        FADD dword ptr [ESI + 0x8] // 300736ff
        FSTP dword ptr [ESI + 0x8] // 30073702
        FLD ST(0) // 30073705
        FMUL dword ptr [swayConst300924d0] // 30073707
        FADD dword ptr [EDI + 0x4] // 3007370d
        FSTP dword ptr [EDI + 0x4] // 30073710
        FMUL dword ptr [swayConst30092398] // 30073713
        FADD dword ptr [EDI] // 30073719
        FSTP dword ptr [EDI] // 3007371b
        MOV ECX,dword ptr [EBP + swayState_time] // 3007371d
sway30073723:
        MOV dword ptr [EBP + swayState_postureModified],EDX // 30073723
        JMP sway300737f5 // 30073729
sway3007372e:
        MOV EAX,[EBP + swayState_duckTime] // 3007372e
        ADD EAX,ECX // 30073733
        CMP EAX,0xc8 // 30073735
        MOV dword ptr [ESP + 0x38],EAX // 3007373a
        JGE sway300737f5 // 3007373e
        TEST EAX,EAX // 30073744
        JNZ sway30073750 // 30073746
        FLD dword ptr [swayConst300922b4] // 30073748
        JMP sway30073766 // 3007374e
sway30073750:
        FILD dword ptr [ESP + 0x38] // 30073750
        FDIVR dword ptr [swayConst30092488] // 30073754
        FDIVR dword ptr [swayConst300922b4] // 3007375a
        FSUBR dword ptr [swayConst300922b4] // 30073760
sway30073766:
        LEA ECX,[EBP + swayState_viewOrigin] // 30073766
        FLD ST(0) // 3007376c
        FMUL dword ptr [swayConst30092ff0] // 3007376e
        FLD ST(0) // 30073774
        FMUL dword ptr [ECX + 12] // 30073776
        FADD dword ptr [ESI] // 30073779
        FSTP dword ptr [ESI] // 3007377b
        LEA EAX,[EBP + swayState_viewOrigin] // 3007377d
        FLD ST(0) // 30073782
        FMUL dword ptr [EAX + 16] // 30073784
        FADD dword ptr [ESI + 0x4] // 30073787
        FSTP dword ptr [ESI + 0x4] // 3007378a
        LEA ECX,[EBP + swayState_viewOrigin] // 3007378d
        FMUL dword ptr [ECX + 20] // 30073793
        FADD dword ptr [ESI + 0x8] // 30073796
        FSTP dword ptr [ESI + 0x8] // 30073799
        LEA EAX,[EBP + swayState_viewOrigin] // 3007379c
        FLD ST(0) // 300737a1
        FMUL dword ptr [swayConst300923d4] // 300737a3
        FLD ST(0) // 300737a9
        FMUL dword ptr [EAX + 36] // 300737ab
        FADD dword ptr [ESI] // 300737ae
        FSTP dword ptr [ESI] // 300737b0
        LEA ECX,[EBP + swayState_viewOrigin] // 300737b2
        FLD ST(0) // 300737b8
        FMUL dword ptr [ECX + 40] // 300737ba
        FADD dword ptr [ESI + 0x4] // 300737bd
        FSTP dword ptr [ESI + 0x4] // 300737c0
        LEA EAX,[EBP + swayState_viewOrigin] // 300737c3
        FMUL dword ptr [EAX + 44] // 300737c8
        FADD dword ptr [ESI + 0x8] // 300737cb
        FSTP dword ptr [ESI + 0x8] // 300737ce
        FLD ST(0) // 300737d1
        FMUL dword ptr [swayConst300924d0] // 300737d3
        FADD dword ptr [EDI + 0x4] // 300737d9
        FSTP dword ptr [EDI + 0x4] // 300737dc
        FMUL dword ptr [swayConst30092398] // 300737df
        FADD dword ptr [EDI] // 300737e5
        FSTP dword ptr [EDI] // 300737e7
        MOV ECX,dword ptr [EBP + swayState_time] // 300737e9
        MOV dword ptr [EBP + swayState_postureModified],EDX // 300737ef
sway300737f5:
        FLD dword ptr [EBP + swayState_lean] // 300737f5
        FCOMP dword ptr [swayConst300920e0] // 300737fb
        FNSTSW AX // 30073801
        TEST AH,0x40 // 30073803
        JNZ sway30073965 // 30073806
        MOV EAX,[EBP + swayState_aiming] // 3007380c
        MOV EBX,dword ptr [EBP + swayState_leanTime] // 30073811
        TEST EAX,EAX // 30073817
        MOV dword ptr [ESP + 0x38],0x3f800000 // 30073819
        MOV EAX,ECX // 30073821
        JZ sway30073848 // 30073823
        SUB EAX,EBX // 30073825
        CMP EAX,0xc8 // 30073827
        MOV dword ptr [ESP + 0x3c],EAX // 3007382c
        JGE sway30073965 // 30073830
        FILD dword ptr [ESP + 0x3c] // 30073836
        FMUL dword ptr [swayConst300922ec] // 3007383a
        FSUBR dword ptr [swayConst300922b4] // 30073840
        JMP sway3007385f // 30073846
sway30073848:
        SUB EAX,EBX // 30073848
        CMP EAX,0xc8 // 3007384a
        MOV dword ptr [ESP + 0x3c],EAX // 3007384f
        JGE sway30073878 // 30073853
        FILD dword ptr [ESP + 0x3c] // 30073855
        FMUL dword ptr [swayConst300922ec] // 30073859
sway3007385f:
        FSTP dword ptr [ESP + 0x38] // 3007385f
        FLD dword ptr [ESP + 0x38] // 30073863
        FCOMP dword ptr [swayConst300920e0] // 30073867
        FNSTSW AX // 3007386d
        TEST AH,0x41 // 3007386f
        JNZ sway30073965 // 30073872
sway30073878:
        FLD dword ptr [EBP + swayState_lean] // 30073878
        FMUL dword ptr [swayConst300922b8] // 3007387e
        LEA ECX,[ESP + 0x28] // 30073884
        LEA EDX,[ESP + 0x1c] // 30073888
        PUSH ECX // 3007388c
        PUSH EDX // 3007388d
        FMUL dword ptr [ESP + 0x40] // 3007388e
        PUSH 0x0 // 30073892
        PUSH EDI // 30073894
        FSUBR dword ptr [EDI + 0x8] // 30073895
        FSTP dword ptr [EDI + 0x8] // 30073898
        CALL AngleVectors // 3007389b
        FLD dword ptr [ESP + 0x48] // 300738a0
        FMUL dword ptr [EDI + 0x8] // 300738a4
        FMUL dword ptr [ESP + 0x2c] // 300738a7
        FADD dword ptr [ESI] // 300738ab
        FSTP dword ptr [ESI] // 300738ad
        FLD dword ptr [ESP + 0x48] // 300738af
        FMUL dword ptr [EDI + 0x8] // 300738b3
        FMUL dword ptr [ESP + 0x30] // 300738b6
        FADD dword ptr [ESI + 0x4] // 300738ba
        FSTP dword ptr [ESI + 0x4] // 300738bd
        FLD dword ptr [ESP + 0x48] // 300738c0
        FMUL dword ptr [EDI + 0x8] // 300738c4
        FMUL dword ptr [ESP + 0x34] // 300738c7
        FADD dword ptr [ESI + 0x8] // 300738cb
        FSTP dword ptr [ESI + 0x8] // 300738ce
        FLD dword ptr [EBP + swayState_lean] // 300738d1
        CALL TCE_WeaponPositionTruncateST0 // 300738d7
        CDQ // 300738dc
        XOR EAX,EDX // 300738dd
        PUSH 0x0 // 300738df
        SUB EAX,EDX // 300738e1
        MOV dword ptr [ESP + 0x50],EAX // 300738e3
        LEA EAX,[ESP + 0x30] // 300738e7
        FILD dword ptr [ESP + 0x50] // 300738eb
        PUSH EAX // 300738ef
        PUSH 0x0 // 300738f0
        LEA EAX,[EBP + swayState_viewAngles]
        PUSH EAX // 300738f2
        FMUL dword ptr [swayConst300922b8] // 300738f7
        FMUL dword ptr [ESP + 0x58] // 300738fd
        FADD dword ptr [EDI] // 30073901
        FSTP dword ptr [EDI] // 30073903
        CALL AngleVectors // 30073905
        FLD dword ptr [EBP + swayState_lean] // 3007390a
        FMUL dword ptr [swayConst30092490] // 30073910
        ADD ESP,0x20 // 30073916
        FMUL dword ptr [ESP + 0x38] // 30073919
        FCHS // 3007391d
        FMUL dword ptr [ESP + 0x1c] // 3007391f
        FADD dword ptr [ESI] // 30073923
        FSTP dword ptr [ESI] // 30073925
        FLD dword ptr [EBP + swayState_lean] // 30073927
        FMUL dword ptr [swayConst30092490] // 3007392d
        FMUL dword ptr [ESP + 0x38] // 30073933
        FCHS // 30073937
        FMUL dword ptr [ESP + 0x20] // 30073939
        FADD dword ptr [ESI + 0x4] // 3007393d
        FSTP dword ptr [ESI + 0x4] // 30073940
        FLD dword ptr [EBP + swayState_lean] // 30073943
        FMUL dword ptr [swayConst30092490] // 30073949
        FMUL dword ptr [ESP + 0x38] // 3007394f
        FCHS // 30073953
        FMUL dword ptr [ESP + 0x24] // 30073955
        FADD dword ptr [ESI + 0x8] // 30073959
        FSTP dword ptr [ESI + 0x8] // 3007395c
        MOV ECX,dword ptr [EBP + swayState_time] // 3007395f
sway30073965:
        MOV EDX,dword ptr [EBP + swayState_weapon] // 30073965
        MOV EBX,dword ptr [EBP + swayState_shotTime] // 3007396b
        IMUL EDX,EDX,swayWeaponStride // 30073971
        MOV EAX,dword ptr [EDX + weaponDef + swayWeaponUsesRecoil]
        MOV dword ptr [ESP + 0x4],EAX // 3007397f
        CMP dword ptr [ESP + 0x4],0 // 30073985
        JZ sway30073a31 // 30073987
        MOV EAX,dword ptr [EDX + weaponDef + swayWeaponPump] // 3007398d
        TEST EAX,EAX // 30073993
        JNZ sway30073a31 // 30073995
        FLD dword ptr [EDX + weaponDef + swayWeaponScoped] // 3007399b
        FCOMP dword ptr [swayConst300922b4] // 300739a1
        FNSTSW AX // 300739a7
        TEST AH,0x41 // 300739a9
        JNZ sway300739c0 // 300739ac
        MOV EAX,[EBP + swayState_scopeEnabled] // 300739ae
        TEST EAX,EAX // 300739b3
        JZ sway300739c0 // 300739b5
        MOV EAX,[EBP + swayState_aiming] // 300739b7
        TEST EAX,EAX // 300739bc
        JNZ sway30073a31 // 300739be
sway300739c0:
        CMP ECX,EBX // 300739c0
        JLE sway30073a31 // 300739c2
        SUB ECX,EBX // 300739c4
        CMP ECX,0x12c // 300739c6
        MOV dword ptr [ESP + 0x38],ECX // 300739cc
        JGE sway30073b2b // 300739d0
        FILD dword ptr [ESP + 0x38] // 300739d6
        LEA ECX,[EBP + swayState_viewOrigin] // 300739da
        FMUL dword ptr [swayConst300923b8] // 300739e0
        FSQRT // 300739e6
        FSQRT // 300739e8
        FMUL qword ptr [swayConst30092950] // 300739ea
        FSIN // 300739f0
        FLD ST(0) // 300739f2
        FMUL dword ptr [swayConst30093034] // 300739f4
        FLD ST(0) // 300739fa
        FMUL dword ptr [ECX + 12] // 300739fc
        FADD dword ptr [ESI] // 300739ff
        FSTP dword ptr [ESI] // 30073a01
        LEA EDX,[EBP + swayState_viewOrigin] // 30073a03
        FLD ST(0) // 30073a09
        FMUL dword ptr [EDX + 16] // 30073a0b
        FADD dword ptr [ESI + 0x4] // 30073a0e
        FSTP dword ptr [ESI + 0x4] // 30073a11
        LEA EAX,[EBP + swayState_viewOrigin] // 30073a14
        FMUL dword ptr [EAX + 20] // 30073a19
        FADD dword ptr [ESI + 0x8] // 30073a1c
        FSTP dword ptr [ESI + 0x8] // 30073a1f
        FMUL dword ptr [swayConst300922bc] // 30073a22
        FSUBR dword ptr [EDI] // 30073a28
        FSTP dword ptr [EDI] // 30073a2a
        JMP sway30073b2b // 30073a2c
sway30073a31:
        CMP dword ptr [ESP + 0x4],0 // 30073a31
        JZ sway30073ac8 // 30073a33
        MOV EAX,dword ptr [EDX + weaponDef + swayWeaponPump] // 30073a39
        TEST EAX,EAX // 30073a3f
        JZ sway30073ac8 // 30073a41
        MOV EAX,dword ptr [EDX + weaponDef + swayWeaponSemiauto] // 30073a47
        TEST EAX,EAX // 30073a4d
        JZ sway30073ac8 // 30073a4f
        MOV EAX,[EBP + swayState_count_3407dff8] // 30073a51
        TEST EAX,EAX // 30073a56
        JLE sway30073ac8 // 30073a58
        CMP ECX,EBX // 30073a5a
        JLE sway30073ac8 // 30073a5c
        SUB ECX,EBX // 30073a5e
        CMP ECX,0x1f4 // 30073a60
        MOV dword ptr [ESP + 0x38],ECX // 30073a66
        JGE sway30073b2b // 30073a6a
        FILD dword ptr [ESP + 0x38] // 30073a70
        LEA ECX,[EBP + swayState_viewOrigin] // 30073a74
        FMUL dword ptr [swayConst30092a68] // 30073a7a
        FSQRT // 30073a80
        FSQRT // 30073a82
        FMUL qword ptr [swayConst30092950] // 30073a84
        FSIN // 30073a8a
        FLD ST(0) // 30073a8c
        FMUL dword ptr [swayConst30093034] // 30073a8e
        FLD ST(0) // 30073a94
        FMUL dword ptr [ECX + 12] // 30073a96
        FADD dword ptr [ESI] // 30073a99
        FSTP dword ptr [ESI] // 30073a9b
        LEA EDX,[EBP + swayState_viewOrigin] // 30073a9d
        FLD ST(0) // 30073aa3
        FMUL dword ptr [EDX + 16] // 30073aa5
        FADD dword ptr [ESI + 0x4] // 30073aa8
        FSTP dword ptr [ESI + 0x4] // 30073aab
        LEA EAX,[EBP + swayState_viewOrigin] // 30073aae
        FMUL dword ptr [EAX + 20] // 30073ab3
        FADD dword ptr [ESI + 0x8] // 30073ab6
        FSTP dword ptr [ESI + 0x8] // 30073ab9
        FMUL dword ptr [swayConst300922bc] // 30073abc
        FSUBR dword ptr [EDI] // 30073ac2
        FSTP dword ptr [EDI] // 30073ac4
        JMP sway30073b2b // 30073ac6
sway30073ac8:
        MOV EAX,dword ptr [EDX + weaponDef + swayWeaponUsesPistol] // 30073ac8
        TEST EAX,EAX // 30073ace
        JZ sway30073b2b // 30073ad0
        CMP ECX,EBX // 30073ad2
        JLE sway30073b2b // 30073ad4
        SUB ECX,EBX // 30073ad6
        CMP ECX,0x32 // 30073ad8
        MOV dword ptr [ESP + 0x38],ECX // 30073adb
        JGE sway30073b2b // 30073adf
        FILD dword ptr [ESP + 0x38] // 30073ae1
        LEA ECX,[EBP + swayState_viewOrigin] // 30073ae5
        FMUL dword ptr [swayConst30092c30] // 30073aeb
        FSQRT // 30073af1
        FSQRT // 30073af3
        FMUL qword ptr [swayConst30092950] // 30073af5
        FSIN // 30073afb
        FMUL dword ptr [swayConst30093034] // 30073afd
        FLD ST(0) // 30073b03
        FMUL dword ptr [ECX + 12] // 30073b05
        FADD dword ptr [ESI] // 30073b08
        FSTP dword ptr [ESI] // 30073b0a
        LEA EDX,[EBP + swayState_viewOrigin] // 30073b0c
        FLD ST(0) // 30073b12
        FMUL dword ptr [EDX + 16] // 30073b14
        FADD dword ptr [ESI + 0x4] // 30073b17
        FSTP dword ptr [ESI + 0x4] // 30073b1a
        LEA EAX,[EBP + swayState_viewOrigin] // 30073b1d
        FMUL dword ptr [EAX + 20] // 30073b22
        FADD dword ptr [ESI + 0x8] // 30073b25
        FSTP dword ptr [ESI + 0x8] // 30073b28
sway30073b2b:
        MOV ECX,dword ptr [EBP + swayState_weapon] // 30073b2b
        PUSH ECX // 30073b31
        CALL BG_FiremodeWeapon // 30073b32
        MOV EBX,dword ptr [EBP + swayState_time] // 30073b37
        ADD ESP,0x4 // 30073b3d
        TEST EAX,EAX // 30073b40
        JZ sway30073c38 // 30073b42
        MOV EDX,dword ptr [EBP + swayState_firemodeTime] // 30073b48
        MOV ECX,dword ptr [EBP + swayState_weapon] // 30073b4e
        MOV EAX,EBX // 30073b54
        SUB EAX,EDX // 30073b56
        CMP EAX,0x2ee // 30073b58
        MOV dword ptr [ESP + 0x38],EAX // 30073b5d
        JGE sway30073c3e // 30073b61
        IMUL EDX,ECX,swayWeaponStride // 30073b67
        FLD dword ptr [EDX + weaponDef + swayWeaponScoped] // 30073b72
        FCOMP dword ptr [swayConst300922b4] // 30073b79
        FNSTSW AX // 30073b7f
        TEST AH,0x41 // 30073b81
        JNZ sway30073b9c // 30073b84
        MOV EAX,[EBP + swayState_aiming] // 30073b86
        TEST EAX,EAX // 30073b8b
        JZ sway30073b9c // 30073b8d
        MOV EAX,[EBP + swayState_scopeEnabled] // 30073b8f
        TEST EAX,EAX // 30073b94
        JNZ sway30073c3e // 30073b96
sway30073b9c:
        FILD dword ptr [ESP + 0x38] // 30073b9c
        LEA EAX,[EBP + swayState_viewOrigin] // 30073ba0
        FMUL dword ptr [swayConst30092f88] // 30073ba5
        FMUL dword ptr [swayConst30092310] // 30073bab
        FCOS // 30073bb1
        FSUBR qword ptr [swayConst300922c8] // 30073bb3
        FMUL qword ptr [swayConst300922e0] // 30073bb9
        FLD ST(0) // 30073bbf
        FMUL dword ptr [swayConst30092c28] // 30073bc1
        FLD ST(0) // 30073bc7
        FMUL dword ptr [EAX + 36] // 30073bc9
        FADD dword ptr [ESI] // 30073bcc
        FSTP dword ptr [ESI] // 30073bce
        LEA ECX,[EBP + swayState_viewOrigin] // 30073bd0
        FLD ST(0) // 30073bd6
        FMUL dword ptr [ECX + 40] // 30073bd8
        FADD dword ptr [ESI + 0x4] // 30073bdb
        FSTP dword ptr [ESI + 0x4] // 30073bde
        LEA EDX,[EBP + swayState_viewOrigin] // 30073be1
        FMUL dword ptr [EDX + 44] // 30073be7
        FADD dword ptr [ESI + 0x8] // 30073bea
        FSTP dword ptr [ESI + 0x8] // 30073bed
        FMUL dword ptr [swayConst300922bc] // 30073bf0
        FLD dword ptr [EDI] // 30073bf6
        FSUB ST(0),ST(1) // 30073bf8
        FSTP dword ptr [EDI] // 30073bfa
        MOV EAX,[EBP + swayState_weapon] // 30073bfc
        CMP EAX,0x2b // 30073c01
        JZ sway30073c24 // 30073c04
        CMP EAX,0x21 // 30073c06
        JZ sway30073c24 // 30073c09
        CMP EAX,0x2d // 30073c0b
        JZ sway30073c24 // 30073c0e
        FLD ST(0) // 30073c10
        FADD dword ptr [EDI + 0x8] // 30073c12
        FSTP dword ptr [EDI + 0x8] // 30073c15
        FLD dword ptr [EDI + 0x4] // 30073c18
        FSUB ST(0),ST(1) // 30073c1b
        FSTP dword ptr [EDI + 0x4] // 30073c1d
        FSTP ST(0) // 30073c20
        JMP sway30073c32 // 30073c22
sway30073c24:
        FLD dword ptr [EDI + 0x8] // 30073c24
        FSUB ST(0),ST(1) // 30073c27
        FSTP dword ptr [EDI + 0x8] // 30073c29
        FADD dword ptr [EDI + 0x4] // 30073c2c
        FSTP dword ptr [EDI + 0x4] // 30073c2f
sway30073c32:
        MOV EBX,dword ptr [EBP + swayState_time] // 30073c32
sway30073c38:
        MOV ECX,dword ptr [EBP + swayState_weapon] // 30073c38
sway30073c3e:
        MOV AL,[EBP + swayState_bobCycle] // 30073c3e
        FLD dword ptr [EBP + swayState_speed] // 30073c43
        TEST AL,0x1 // 30073c49
        JZ sway30073c4f // 30073c4b
        FCHS // 30073c4d
sway30073c4f:
        CMP dword ptr [EBP + swayState_aiming],0 // 30073c55
        JZ sway30073ca8 // 30073c57
        CMP ECX,0x1e // 30073c59
        JZ sway30073ca8 // 30073c5c
        CMP ECX,0x9 // 30073c5e
        JZ sway30073ca8 // 30073c61
        CMP ECX,0x4 // 30073c63
        JZ sway30073ca8 // 30073c66
        IMUL EAX,ECX,swayWeaponStride // 30073c68
        MOV dword ptr [ESP + 0x10],0x3e19999a // 30073c6b
        MOV dword ptr [ESP + 0x14],0x0 // 30073c78
        FLD dword ptr [EAX + weaponDef + swayWeaponScoped] // 30073c83
        FCOMP dword ptr [swayConst300922b4] // 30073c8a
        FNSTSW AX // 30073c90
        TEST AH,0x41 // 30073c92
        JNZ sway30073ca8 // 30073c95
        MOV EAX,[EBP + swayState_scopeEnabled] // 30073c97
        TEST EAX,EAX // 30073c9c
        JZ sway30073ca8 // 30073c9e
        MOV dword ptr [ESP + 0x10],0x0 // 30073ca0
sway30073ca8:
        IMUL EDX,ECX,swayWeaponStride // 30073ca8
        FLD dword ptr [EDX + weaponDef + swayWeaponScoped] // 30073cb6
        FCOMP dword ptr [swayConst300922b4] // 30073cbc
        FNSTSW AX // 30073cc2
        TEST AH,0x41 // 30073cc4
        JNZ sway30073cda // 30073cc7
        CMP dword ptr [EBP + swayState_aiming],0 // 30073cc9
        JZ sway30073cda // 30073ccb
        MOV EAX,[EBP + swayState_scopeEnabled] // 30073ccd
        TEST EAX,EAX // 30073cd2
        JNZ sway3007404b // 30073cd4
sway30073cda:
        MOV ECX,dword ptr [EBP + swayState_swayTime] // 30073cda
        SUB ECX,EBX // 30073ce0
        MOV dword ptr [ESP + 0x38],ECX // 30073ce2
        FILD dword ptr [ESP + 0x38] // 30073ce6
        FMUL dword ptr [swayConst300924d8] // 30073cea
        FCOM dword ptr [swayConst30092f8c] // 30073cf0
        FNSTSW AX // 30073cf6
        TEST AH,0x1 // 30073cf8
        JZ sway30073d05 // 30073cfb
        FSTP ST(0) // 30073cfd
        FLD dword ptr [swayConst30092f8c] // 30073cff
sway30073d05:
        LEA ECX,[EBP + swayState_viewAxis] // 30073d05
        MOV dword ptr [EBP + swayState_swayTime],EBX // 30073d0b
        FLD dword ptr [ECX + 0] // 30073d11
        FSUB dword ptr [EBP + swayState_priorForward] // 30073d14
        FLD dword ptr [ECX + 4] // 30073d1a
        FSUB dword ptr [EBP + swayState_priorForward + 4] // 30073d1d
        MOV EAX,dword ptr [ECX + 0] // 30073d23
        FLD dword ptr [ECX + 8] // 30073d26
        FSUB dword ptr [EBP + swayState_priorForward + 8] // 30073d29
        MOV [EBP + swayState_priorForward],EAX // 30073d2f
        MOV EAX,dword ptr [ECX + 4] // 30073d34
        MOV [EBP + swayState_priorForward + 4],EAX // 30073d37
        MOV EAX,dword ptr [ECX + 8] // 30073d3c
        FLD ST(2) // 30073d3f
        MOV [EBP + swayState_priorForward + 8],EAX // 30073d41
        MOV EAX,EBX // 30073d46
        FMUL dword ptr [ECX + 12] // 30073d48
        FLD ST(1) // 30073d4b
        FMUL dword ptr [ECX + 20] // 30073d4d
        SUB EAX,dword ptr [EBP + swayState_stepTime] // 30073d50
        FADDP ST(1),ST(0) // 30073d56
        FLD ST(2) // 30073d58
        FMUL dword ptr [ECX + 16] // 30073d5a
        MOV dword ptr [ESP + 0x38],EAX // 30073d5d
        MOV EAX,[EBP + swayState_eFlags] // 30073d61
        TEST EAX,0x80000 // 30073d66
        FADDP ST(1),ST(0) // 30073d6b
        FDIV ST(0),ST(4) // 30073d6d
        FCHS // 30073d6f
        FSTP dword ptr [ESP + 0x1c] // 30073d71
        FMUL dword ptr [ECX + 32] // 30073d75
        FXCH ST(1) // 30073d78
        FMUL dword ptr [ECX + 28] // 30073d7a
        FADDP ST(1),ST(0) // 30073d7d
        FXCH ST(1) // 30073d7f
        FMUL dword ptr [ECX + 24] // 30073d81
        FADDP ST(1),ST(0) // 30073d84
        FDIVRP ST(1),ST(0) // 30073d86
        FCHS // 30073d88
        FLD dword ptr [EBP + swayState_velocity + 4] // 30073d8a
        FMUL dword ptr [ECX + 16] // 30073d90
        FLD dword ptr [EBP + swayState_velocity] // 30073d93
        FMUL dword ptr [ECX + 12] // 30073d99
        FADDP ST(1),ST(0) // 30073d9c
        FLD dword ptr [EBP + swayState_velocity + 8] // 30073d9e
        FMUL dword ptr [ECX + 20] // 30073da4
        FADDP ST(1),ST(0) // 30073da7
        FMUL dword ptr [swayConst30092a00] // 30073da9
        FADD dword ptr [ESP + 0x1c] // 30073daf
        FSTP dword ptr [ESP + 0x1c] // 30073db3
        FLD dword ptr [EBP + swayState_velocity + 8] // 30073db7
        FMUL dword ptr [ECX + 8] // 30073dbd
        FLD dword ptr [EBP + swayState_velocity] // 30073dc0
        FMUL dword ptr [ECX + 0] // 30073dc6
        FADDP ST(1),ST(0) // 30073dc9
        FLD dword ptr [EBP + swayState_velocity + 4] // 30073dcb
        FMUL dword ptr [ECX + 4] // 30073dd1
        FADDP ST(1),ST(0) // 30073dd4
        FMUL dword ptr [swayConst30092a00] // 30073dd6
        FADD ST(0),ST(1) // 30073ddc
        FSTP dword ptr [ESP + 0x20] // 30073dde
        FSTP ST(0) // 30073de2
        FILD dword ptr [ESP + 0x38] // 30073de4
        JNZ sway30073df8 // 30073de8
        MOV EAX,[EBP + swayState_proneTime] // 30073dea
        ADD EAX,EBX // 30073def
        CMP EAX,0x2ee // 30073df1
        JGE sway30073e07 // 30073df6
sway30073df8:
        FCOMP dword ptr [swayConst30092d74] // 30073df8
        FNSTSW AX // 30073dfe
        TEST AH,0x1 // 30073e00
        JZ sway30073e22 // 30073e03
        JMP sway30073e14 // 30073e05
sway30073e07:
        FCOMP dword ptr [swayConst300928b4] // 30073e07
        FNSTSW AX // 30073e0d
        TEST AH,0x1 // 30073e0f
        JZ sway30073e22 // 30073e12
sway30073e14:
        FLD dword ptr [EBP + swayState_stepChange] // 30073e14
        FADD dword ptr [ESP + 0x20] // 30073e1a
        FSTP dword ptr [ESP + 0x20] // 30073e1e
sway30073e22:
        CMP dword ptr [EBP + swayState_aiming],0 // 30073e22
        JZ sway30073e3a // 30073e24
        FILD dword ptr [EDX + weaponDef + swayWeaponLimit] // 30073e26
        FMUL dword ptr [swayConst30093030] // 30073e2c
        FLD dword ptr [swayConst300922b8] // 30073e32
        JMP sway30073e46 // 30073e38
sway30073e3a:
        FLD dword ptr [swayConst300923ec] // 30073e3a
        FLD dword ptr [swayConst300922b4] // 30073e40
sway30073e46:
        FILD dword ptr [EBP + swayState_scale_3407dfbc] // 30073e46
        FMUL dword ptr [swayConst300924d8] // 30073e4c
        FILD dword ptr [EBP + swayState_scale_3407dfc0] // 30073e52
        FMUL ST(0),ST(2) // 30073e58
        FMUL dword ptr [swayConst300924d8] // 30073e5a
        FADDP ST(1),ST(0) // 30073e60
        FSTP dword ptr [ESP + 0x38] // 30073e62
        FSTP ST(0) // 30073e66
        FLD dword ptr [ESP + 0x38] // 30073e68
        FCOMP dword ptr [swayConst300922b4] // 30073e6c
        FNSTSW AX // 30073e72
        TEST AH,0x41 // 30073e74
        JNZ sway30073e81 // 30073e77
        MOV dword ptr [ESP + 0x38],0x3f800000 // 30073e79
sway30073e81:
        CMP dword ptr [EBP + swayState_aiming],0 // 30073e81
        JNZ sway30073e9e // 30073e83
        FLD dword ptr [ESP + 0x38] // 30073e85
        FCOMP dword ptr [swayConst300922b8] // 30073e89
        FNSTSW AX // 30073e8f
        TEST AH,0x1 // 30073e91
        JZ sway30073e9e // 30073e94
        MOV dword ptr [ESP + 0x38],0x3f000000 // 30073e96
sway30073e9e:
        LEA EDX,[ESP + 0x1c] // 30073e9e
        MOV EBX,0x2 // 30073ea2
sway30073ea7:
        FLD dword ptr [EDX] // 30073ea7
        FABS // 30073ea9
        FCOMP qword ptr [swayConst30092a30] // 30073eab
        FNSTSW AX // 30073eb1
        TEST AH,0x1 // 30073eb3
        JZ sway30073ec0 // 30073eb6
        MOV dword ptr [EDX],0x0 // 30073eb8
        JMP sway30073ee0 // 30073ebe
sway30073ec0:
        FCOM dword ptr [EDX] // 30073ec0
        FNSTSW AX // 30073ec2
        TEST AH,0x1 // 30073ec4
        JZ sway30073ecd // 30073ec7
        FST dword ptr [EDX] // 30073ec9
        JMP sway30073ee0 // 30073ecb
sway30073ecd:
        FLD ST(0) // 30073ecd
        FCHS // 30073ecf
        FCOM dword ptr [EDX] // 30073ed1
        FNSTSW AX // 30073ed3
        TEST AH,0x41 // 30073ed5
        JNZ sway30073ede // 30073ed8
        FSTP dword ptr [EDX] // 30073eda
        JMP sway30073ee0 // 30073edc
sway30073ede:
        FSTP ST(0) // 30073ede
sway30073ee0:
        ADD EDX,0x4 // 30073ee0
        DEC EBX // 30073ee3
        JNZ sway30073ea7 // 30073ee4
        FLD dword ptr [ESP + 0x1c] // 30073ee6
        FMUL dword ptr [swayConst30092520] // 30073eea
        FADD dword ptr [EBP + swayState_swayHorizontal] // 30073ef0
        FMUL dword ptr [swayConst30092c40] // 30073ef6
        FSTP dword ptr [EBP + swayState_swayHorizontal] // 30073efc
        FLD dword ptr [ESP + 0x20] // 30073f02
        FMUL dword ptr [swayConst30092520] // 30073f06
        FADD dword ptr [EBP + swayState_swayVertical] // 30073f0c
        FMUL dword ptr [swayConst30092c40] // 30073f12
        FST dword ptr [EBP + swayState_swayVertical] // 30073f18
        FCOMP ST(1) // 30073f1e
        FNSTSW AX // 30073f20
        TEST AH,0x41 // 30073f22
        JNZ sway30073f2f // 30073f25
        FST dword ptr [EBP + swayState_swayVertical] // 30073f27
        JMP sway30073f4c // 30073f2d
sway30073f2f:
        FLD ST(0) // 30073f2f
        FCHS // 30073f31
        FLD dword ptr [EBP + swayState_swayVertical] // 30073f33
        FCOMP ST(1) // 30073f39
        FNSTSW AX // 30073f3b
        TEST AH,0x1 // 30073f3d
        JZ sway30073f4a // 30073f40
        FSTP dword ptr [EBP + swayState_swayVertical] // 30073f42
        JMP sway30073f4c // 30073f48
sway30073f4a:
        FSTP ST(0) // 30073f4a
sway30073f4c:
        FLD dword ptr [EBP + swayState_swayHorizontal] // 30073f4c
        FCOMP ST(1) // 30073f52
        FNSTSW AX // 30073f54
        TEST AH,0x41 // 30073f56
        JNZ sway30073f63 // 30073f59
        FSTP dword ptr [EBP + swayState_swayHorizontal] // 30073f5b
        JMP sway30073f7e // 30073f61
sway30073f63:
        FCHS // 30073f63
        FLD dword ptr [EBP + swayState_swayHorizontal] // 30073f65
        FCOMP ST(1) // 30073f6b
        FNSTSW AX // 30073f6d
        TEST AH,0x1 // 30073f6f
        JZ sway30073f7c // 30073f72
        FSTP dword ptr [EBP + swayState_swayHorizontal] // 30073f74
        JMP sway30073f7e // 30073f7a
sway30073f7c:
        FSTP ST(0) // 30073f7c
sway30073f7e:
        FLD dword ptr [EBP + swayState_swayHorizontal] // 30073f7e
        FMUL dword ptr [ESP + 0x38] // 30073f84
        FMUL dword ptr [swayConst30092490] // 30073f88
        FMUL dword ptr [ECX + 12] // 30073f8e
        FADD dword ptr [ESI] // 30073f91
        FSTP dword ptr [ESI] // 30073f93
        FLD dword ptr [EBP + swayState_swayHorizontal] // 30073f95
        FMUL dword ptr [ESP + 0x38] // 30073f9b
        LEA ECX,[EBP + swayState_viewAxis] // 30073f9f
        FMUL dword ptr [swayConst30092490] // 30073fa5
        FMUL dword ptr [ECX + 16] // 30073fab
        FADD dword ptr [ESI + 0x4] // 30073fae
        FSTP dword ptr [ESI + 0x4] // 30073fb1
        FLD dword ptr [EBP + swayState_swayHorizontal] // 30073fb4
        FMUL dword ptr [ESP + 0x38] // 30073fba
        LEA EDX,[EBP + swayState_viewAxis] // 30073fbe
        FMUL dword ptr [swayConst30092490] // 30073fc4
        FMUL dword ptr [EDX + 20] // 30073fca
        FADD dword ptr [ESI + 0x8] // 30073fcd
        FSTP dword ptr [ESI + 0x8] // 30073fd0
        FLD dword ptr [EBP + swayState_swayVertical] // 30073fd3
        FMUL dword ptr [ESP + 0x38] // 30073fd9
        LEA EAX,[EBP + swayState_viewAxis] // 30073fdd
        FMUL dword ptr [swayConst30092490] // 30073fe2
        FMUL dword ptr [EAX + 24] // 30073fe8
        FADD dword ptr [ESI] // 30073feb
        FSTP dword ptr [ESI] // 30073fed
        FLD dword ptr [EBP + swayState_swayVertical] // 30073fef
        FMUL dword ptr [ESP + 0x38] // 30073ff5
        LEA ECX,[EBP + swayState_viewAxis] // 30073ff9
        FMUL dword ptr [swayConst30092490] // 30073fff
        FMUL dword ptr [ECX + 28] // 30074005
        FADD dword ptr [ESI + 0x4] // 30074008
        FSTP dword ptr [ESI + 0x4] // 3007400b
        FLD dword ptr [EBP + swayState_swayVertical] // 3007400e
        FMUL dword ptr [ESP + 0x38] // 30074014
        LEA EDX,[EBP + swayState_viewAxis] // 30074018
        FMUL dword ptr [swayConst30092490] // 3007401e
        FMUL dword ptr [EDX + 32] // 30074024
        FADD dword ptr [ESI + 0x8] // 30074027
        FSTP dword ptr [ESI + 0x8] // 3007402a
        FLD dword ptr [EBP + swayState_swayHorizontal] // 3007402d
        FMUL dword ptr [ESP + 0x38] // 30074033
        FSUBR dword ptr [EDI + 0x4] // 30074037
        FSTP dword ptr [EDI + 0x4] // 3007403a
        FLD dword ptr [EBP + swayState_swayVertical] // 3007403d
        FMUL dword ptr [ESP + 0x38] // 30074043
        FADD dword ptr [EDI] // 30074047
        FSTP dword ptr [EDI] // 30074049
sway3007404b:
        FLD dword ptr [EBP + swayState_bobSin] // 3007404b
        FMUL ST(0),ST(1) // 30074051
        FMUL dword ptr [ESP + 0x10] // 30074055
        FMUL qword ptr [swayConst30092ad0] // 30074059
        FADD dword ptr [EDI + 0x8] // 3007405f
        FSTP dword ptr [EDI + 0x8] // 30074062
        FLD dword ptr [EBP + swayState_bobSin] // 30074065
        FMUL ST(0),ST(1) // 3007406b
        FMUL dword ptr [ESP + 0x10] // 3007406d
        FMUL qword ptr [swayConst30092c70] // 30074071
        FADD dword ptr [EDI + 0x4] // 30074077
        FSTP dword ptr [EDI + 0x4] // 3007407a
        FSTP ST(0) // 3007407d
        FLD dword ptr [EBP + swayState_bobSin] // 3007407f
        FMUL dword ptr [EBP + swayState_speed] // 30074085
        FMUL dword ptr [ESP + 0x10] // 3007408b
        FMUL qword ptr [swayConst30092ad0] // 3007408f
        FADD dword ptr [EDI] // 30074095
        FSTP dword ptr [EDI] // 30074097
        FLD dword ptr [ESP + 0x10] // 30074099
        FCOMP dword ptr [swayConst300920e0] // 3007409d
        FNSTSW AX // 300740a3
        TEST AH,0x41 // 300740a5
        JNZ sway300740fc // 300740a8
        MOV EAX,[EBP + swayState_time] // 300740aa
        MOV EDX,dword ptr [EBP + swayState_landTime] // 300740af
        SUB EAX,EDX // 300740b5
        CMP EAX,0x96 // 300740b7
        MOV dword ptr [ESP + 0x30],EAX // 300740bc
        JGE sway300740d4 // 300740c0
        FLD dword ptr [EBP + swayState_landChange] // 300740c2
        FIMUL dword ptr [ESP + 0x30] // 300740c8
        FMUL qword ptr [swayConst30093028] // 300740cc
        JMP sway300740f6 // 300740d2
sway300740d4:
        CMP EAX,0x1c2 // 300740d4
        JGE sway300740fc // 300740d9
        MOV ECX,0x1c2 // 300740db
        SUB ECX,EAX // 300740e0
        MOV dword ptr [ESP + 0x30],ECX // 300740e2
        FILD dword ptr [ESP + 0x30] // 300740e6
        FMUL dword ptr [EBP + swayState_landChange] // 300740ea
        FMUL qword ptr [swayConst30093020] // 300740f0
sway300740f6:
        FADD dword ptr [ESI + 0x8] // 300740f6
        FSTP dword ptr [ESI + 0x8] // 300740f9
sway300740fc:
        LEA EAX,[EBP + swayState_gunViewAngles] // 300740fc
        FLD dword ptr [EAX + 0] // 30074100
        FADD dword ptr [EDI] // 30074106
        FSTP dword ptr [EDI] // 30074108
        FLD dword ptr [EAX + 4] // 3007410a
        FADD dword ptr [EDI + 0x4] // 30074110
        FSTP dword ptr [EDI + 0x4] // 30074113
        FLD dword ptr [EAX + 8] // 30074116
        FADD dword ptr [EDI + 0x8] // 3007411c
        FSTP dword ptr [EDI + 0x8] // 3007411f
        MOV EAX,[EBP + swayState_developer] // 30074122
        TEST EAX,EAX // 30074127
        JZ sway3007414d // 30074129
        FLD dword ptr [EBP + swayState_developerAngles] // 3007412b
        FADD dword ptr [EDI] // 30074131
        FSTP dword ptr [EDI] // 30074133
        FLD dword ptr [EBP + swayState_developerAngles + 4] // 30074135
        FADD dword ptr [EDI + 0x4] // 3007413b
        FSTP dword ptr [EDI + 0x4] // 3007413e
        FLD dword ptr [EBP + swayState_developerAngles + 8] // 30074141
        FADD dword ptr [EDI + 0x8] // 30074147
        FSTP dword ptr [EDI + 0x8] // 3007414a
sway3007414d:
        MOV ECX,dword ptr [EBP + swayState_weapon] // 3007414d
        IMUL EDX,ECX,swayWeaponStride // 30074153
        MOV EAX,dword ptr [EDX + weaponDef + swayWeaponNoTacMode] // 3007415e
        TEST EAX,EAX // 30074165
        JNZ sway300741bc // 30074167
        FLD dword ptr [EBP + swayState_tacticalScale] // 30074169
        FCOMP dword ptr [swayConst300920e0] // 3007416f
        FNSTSW AX // 30074175
        TEST AH,0x40 // 30074177
        JNZ sway300741bc // 3007417a
        FILD dword ptr [EBP + swayState_tacticalPitch] // 3007417c
        FSUB dword ptr [swayConst300923fc] // 30074182
        FMUL dword ptr [EBP + swayState_tacticalScale] // 30074188
        FMUL dword ptr [swayConst300923f8] // 3007418e
        FADD dword ptr [EDI] // 30074194
        FSTP dword ptr [EDI] // 30074196
        FILD dword ptr [EBP + swayState_tacticalYaw] // 30074198
        FSUB dword ptr [swayConst300923fc] // 3007419e
        FMUL dword ptr [EBP + swayState_tacticalScale] // 300741a4
        FMUL dword ptr [swayConst300923f8] // 300741aa
        FADD dword ptr [EDI + 0x4] // 300741b0
        FSTP dword ptr [EDI + 0x4] // 300741b3
        MOV ECX,dword ptr [EBP + swayState_weapon] // 300741b6
sway300741bc:
        MOV EAX,[EBP + swayState_eFlags] // 300741bc
        TEST AH,0x80 // 300741c1
        JNZ sway30074206 // 300741c4
        CMP ECX,0x3c // 300741c6
        JZ sway30074206 // 300741c9
        CMP ECX,0x3e // 300741cb
        JZ sway30074206 // 300741ce
        FILD dword ptr [EBP + swayState_time] // 300741d0
        FMUL qword ptr [swayConst30092318] // 300741d6
        FSIN // 300741dc
        FMUL dword ptr [swayConst300924e0] // 300741de
        FMUL dword ptr [ESP + 0x14] // 300741e4
        FMUL qword ptr [swayConst30092c70] // 300741e8
        FLD dword ptr [EDI + 0x8] // 300741ee
        FADD ST(0),ST(1) // 300741f1
        FSTP dword ptr [EDI + 0x8] // 300741f3
        FLD dword ptr [EDI + 0x4] // 300741f6
        FADD ST(0),ST(1) // 300741f9
        FSTP dword ptr [EDI + 0x4] // 300741fb
        FLD dword ptr [EDI] // 300741fe
        FADD ST(0),ST(1) // 30074200
        FSTP dword ptr [EDI] // 30074202
        FSTP ST(0) // 30074204
sway30074206:
        FLD dword ptr [EDI] // 30074206
        FSUB dword ptr [EBP + swayState_kickAngles] // 30074208
        FSTP dword ptr [EDI] // 3007420e
        FLD dword ptr [EDI + 0x4] // 30074210
        FSUB dword ptr [EBP + swayState_kickAngles + 4] // 30074213
        FSTP dword ptr [EDI + 0x4] // 30074219
        FLD dword ptr [EDI + 0x8] // 3007421c
        FSUB dword ptr [EBP + swayState_kickAngles + 8] // 3007421f
        FSTP dword ptr [EDI + 0x8] // 30074225
        ADD ESP,0x40
        POP EBP
        POP EDI
        POP ESI
        POP EBX
        RET
    }
}
#endif

/* Portable fallback; its evaluation schedule is not a Windows x87 claim. */
static void translate(float *v, const float *axis, double scale) {
    int i; for (i=0;i<3;++i) v[i]=(float)(v[i]+scale*axis[i]);
}
static double dot(const float *a,const float *b) {
    return (double)a[0]*b[0]+(double)a[1]*b[1]+(double)a[2]*b[2];
}
static float clampSway(float value,float limit) {
    if(value>limit)return limit;
    return value < -limit ? -limit : value;
}
static void posture(tce_weaponPosition_t *s,float *origin,float *angles,int active,int timer,int kind) {
    int delta=active?s->time-timer:s->time+timer;
    float f;
    if(active) {
        s->postureModified=1;
        if(delta<=0)return;
        f=delta>200?1.0f:(float)(1.0/(200.0/delta));
    } else {
        if(delta>=200)return;
        s->postureModified=1;
        f=delta==0?1.0f:(float)(1.0-1.0/(200.0/delta));
    }
    if(kind==0) {
        translate(origin,s->viewAxis[0],(float)(f*-25.0));
        translate(origin,s->viewAxis[1],(float)(f*5.0));
        translate(origin,s->viewAxis[2],(float)(f*-5.0));
        angles[1]=(float)(angles[1]+f*60.0);
    } else {
        if(kind==2)translate(origin,s->viewAxis[0],(float)(f*-5.0));
        translate(origin,s->viewAxis[2],(float)(f*(kind==1?-5.0:-20.0)));
        angles[1]=(float)(angles[1]+f*10.0);
        angles[0]=(float)(angles[0]+f*30.0);
    }
}
void TCE_CG_CalculateWeaponPosition(tce_weaponPosition_t *s,float *origin,float *angles) {
#if defined(_MSC_VER) && defined(_M_IX86)
    TCE_WeaponPositionNative(s,origin,angles);
#else
    const tce_weaponDef_t *w=&weaponDef[s->weapon];
    int i,delta,scoped=s->aiming && s->scopeEnabled && w->scoped>1;
    float f,dt,limit,strength,secondary,bobScale=1,idleScale=1,signedSpeed;
    float right[3],up[3],difference[3],target[2];
    double wave,recoilRate=0; int recoilPitch=0;
    VectorCopy(s->viewOrigin,origin);VectorCopy(s->viewAngles,angles);
    if(s->eFlags&0x8000)angles[0]=(float)(angles[0]*(5.0/6.0));
    if(!s->thirdPerson && (s->weapon==60||s->weapon==62) && s->weaponState!=1)angles[0]=s->mountedPitch;
    s->postureModified=0;
    posture(s,origin,angles,s->eFlags&0x100000,s->proneMovingTime,0);
    posture(s,origin,angles,s->flags_3407dfb0&0x4000,s->stanceTime,1);
    posture(s,origin,angles,s->pmFlags&4,s->duckTime,2);
    delta=s->time-s->leanTime;
    f=s->aiming?(delta<200?(float)(1.0-delta*(double).005f):0.0f):
        (delta<200?(float)(delta*(double).005f):1.0f);
    if(s->lean!=0 && f>0) {
        angles[2]=(float)(angles[2]-s->lean*.5*f);
        AngleVectors(angles,NULL,right,up);
        translate(origin,right,(double)f*angles[2]);
        angles[0]=(float)(angles[0]+abs((int)s->lean)*.5*f);
        AngleVectors(s->viewAngles,NULL,right,NULL);
        translate(origin,right,-s->lean*.25*f);
    }
    delta=s->time-s->shotTime;
    if(w->usesRecoilAnimMod && !w->pump && !scoped && delta>0) {
        if(delta<300){recoilRate=(double)(1.0f/300.0f);recoilPitch=1;}
    } else if(w->usesRecoilAnimMod && w->pump && w->semiauto && s->count_3407dff8>0 && delta>0) {
        if(delta<500){recoilRate=(double).002f;recoilPitch=1;}
    } else if(w->usesPistolAnimMod && delta>0 && delta<50)recoilRate=(double).02f;
    if(recoilRate) {
        wave=sin(sqrt(sqrt(delta*recoilRate))*3.1415927410125732);
        translate(origin,s->viewAxis[0],wave*-4.0);
        if(recoilPitch)angles[0]=(float)(angles[0]-wave*4.0);
    }
    delta=s->time-s->firemodeTime;
    if(BG_FiremodeWeapon(s->weapon) && delta<750 && !scoped) {
        wave=(1.0-cos(delta*(double)(1.0f/750.0f)*(double)6.2831854820251465f))*.5;
        translate(origin,s->viewAxis[2],wave*-2.0);
        angles[0]=(float)(angles[0]-wave*4);
        if(s->weapon==43||s->weapon==33||s->weapon==45)wave=-wave;
        angles[2]=(float)(angles[2]+wave*4);angles[1]=(float)(angles[1]-wave*4);
    }
    signedSpeed=(s->bobCycle&1)?-s->speed:s->speed;
    if(s->aiming && s->weapon!=30 && s->weapon!=9 && s->weapon!=4) {
        bobScale=scoped?0.0f:.15f;idleScale=0;
    }
    if(!scoped) {
        /* The original subtracts current time from previous time. */
        dt=(float)((s->swayTime-s->time)*(double).001f);if(dt<.005f)dt=.005f;
        for(i=0;i<3;++i){difference[i]=s->viewAxis[0][i]-s->priorForward[i];s->priorForward[i]=s->viewAxis[0][i];}
        target[0]=(float)(dot(s->velocity,s->viewAxis[1])*(double).1f-dot(difference,s->viewAxis[1])/dt);
        target[1]=(float)(dot(s->velocity,s->viewAxis[0])*(double).1f-dot(difference,s->viewAxis[2])/dt);
        f=(!(s->eFlags&0x80000) && s->proneTime+s->time>749)?250.0f:750.0f;
        if((float)(s->time-s->stepTime)<f)target[1]+=s->stepChange;
        limit=s->aiming?(float)(w->unknown_0f8[3]*(double)(1.0f/600.0f)):3.0f;
        secondary=s->aiming?.5f:1.0f;
        strength=(float)(s->scale_3407dfc0*(double)secondary*(double).001f+s->scale_3407dfbc*(double).001f);
        if(strength>1)strength=1;if(!s->aiming && strength<.5f)strength=.5f;
        for(i=0;i<2;++i)target[i]=fabs(target[i])<.2?0.0f:clampSway(target[i],limit);
        s->swayHorizontal=clampSway((float)((target[0]*(double).1f+s->swayHorizontal)*(double).9090908765792847f),limit);
        s->swayVertical=clampSway((float)((target[1]*(double).1f+s->swayVertical)*(double).9090908765792847f),limit);
        s->swayTime=s->time;
        translate(origin,s->viewAxis[1],s->swayHorizontal*(double)strength*.25);
        translate(origin,s->viewAxis[2],s->swayVertical*(double)strength*.25);
        angles[1]=(float)(angles[1]-s->swayHorizontal*(double)strength);
        angles[0]=(float)(angles[0]+s->swayVertical*(double)strength);
    }
    angles[2]=(float)(angles[2]+s->bobSin*(double)signedSpeed*bobScale*.005);
    angles[1]=(float)(angles[1]+s->bobSin*(double)signedSpeed*bobScale*.01);
    angles[0]=(float)(angles[0]+s->bobSin*(double)s->speed*bobScale*.005);
    delta=s->time-s->landTime;
    if(bobScale>0 && delta<450) {
        f=(float)(delta<150?s->landChange*(double)delta/600.0:(450-delta)*(double)s->landChange/1200.0);
        origin[2]+=f;
    }
    for(i=0;i<3;++i)angles[i]+=s->gunViewAngles[i];
    if(s->developer)for(i=0;i<3;++i)angles[i]+=s->developerAngles[i];
    if(!w->noTacMode && s->tacticalScale!=0) {
        angles[0]=(float)(angles[0]+(s->tacticalPitch-2000.0)*s->tacticalScale*(double).01f);
        angles[1]=(float)(angles[1]+(s->tacticalYaw-2000.0)*s->tacticalScale*(double).01f);
    }
    if(!(s->eFlags&0x8000) && s->weapon!=60 && s->weapon!=62) {
        wave=sin(s->time*.001)*80.0*idleScale*.01;
        for(i=0;i<3;++i)angles[i]=(float)(angles[i]+wave);
    }
    for(i=0;i<3;++i)angles[i]-=s->kickAngles[i];
#endif
}
