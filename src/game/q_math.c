// Copyright (C) 1999-2000 Id Software, Inc.
//
// q_math.c -- stateless support routines that are included in each code module
#include "q_shared.h"


vec3_t	vec3_origin = {0,0,0};
vec3_t	axisDefault[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };


vec4_t	colorBlack		=	{0, 0, 0, 1};
vec4_t	colorRed		=	{1, 0, 0, 1};
vec4_t	colorGreen		=	{0, 1, 0, 1};
vec4_t	colorBlue		=	{0, 0, 1, 1};
vec4_t	colorYellow		=	{1, 1, 0, 1};
vec4_t	colorOrange		=	{1, 0.5, 0, 1};
vec4_t	colorMagenta	=	{1, 0, 1, 1};
vec4_t	colorCyan		=	{0, 1, 1, 1};
vec4_t	colorWhite		=	{1, 1, 1, 1};
vec4_t	colorLtGrey		=	{0.75, 0.75, 0.75, 1};
vec4_t	colorMdGrey		=	{0.5, 0.5, 0.5, 1};
vec4_t	colorDkGrey		=	{0.25, 0.25, 0.25, 1};
vec4_t	colorMdRed		=	{0.5, 0, 0, 1};
vec4_t	colorMdGreen	=	{0, 0.5, 0, 1};
vec4_t	colorDkGreen	=	{0, 0.20, 0, 1};
vec4_t	colorMdCyan		=	{0, 0.5, 0.5, 1};
vec4_t	colorMdYellow	=	{0.5, 0.5, 0, 1};
vec4_t	colorMdOrange	=	{0.5, 0.25, 0, 1};
vec4_t	colorMdBlue		=	{0, 0, 0.5, 1};

vec4_t		clrBrown =			{0.68f,			0.68f,			0.56f,			1.f};
vec4_t		clrBrownDk =		{0.58f * 0.75f,	0.58f * 0.75f,	0.46f * 0.75f,	1.f};
vec4_t		clrBrownLine =		{0.0525f,		0.05f,			0.025f,			0.2f};
vec4_t		clrBrownLineFull =	{0.0525f,		0.05f,			0.025f,			1.f};

vec4_t		clrBrownTextLt2 =	{108*1.8/255.f,		88*1.8/255.f,	62*1.8/255.f,	1.f};
vec4_t		clrBrownTextLt =	{108*1.3/255.f,		88*1.3/255.f,	62*1.3/255.f,	1.f};
vec4_t		clrBrownText =		{108/255.f,			88/255.f,		62/255.f,		1.f};
vec4_t		clrBrownTextDk =	{20/255.f,			2/255.f,		0/255.f,		1.f};
vec4_t		clrBrownTextDk2 =	{108*0.75/255.f,	88*0.75/255.f,	62*0.75/255.f,	1.f};

vec4_t	g_color_table[32] =
	{
		{ 0.0,	0.0,	0.0,	1.0 },	// 0 - black		0
		{ 1.0,	0.0,	0.0,	1.0 },	// 1 - red			1
		{ 0.0,	1.0,	0.0,	1.0 },	// 2 - green		2
		{ 1.0,	1.0,	0.0,	1.0 },	// 3 - yellow		3
		{ 0.0,	0.0,	1.0,	1.0 },	// 4 - blue			4
		{ 0.0,	1.0,	1.0,	1.0 },	// 5 - cyan			5
		{ 1.0,	0.0,	1.0,	1.0 },	// 6 - purple		6
		{ 1.0,	1.0,	1.0,	1.0 },	// 7 - white		7
		{ 1.0,	0.5,	0.0,	1.0 },	// 8 - orange		8
		{ 0.5,	0.5,	0.5,	1.0 },	// 9 - md.grey		9
		{ 0.75,	0.75,	0.75,	1.0 },	// : - lt.grey		10		// lt grey for names
		{ 0.75, 0.75,	0.75,	1.0 },	// ; - lt.grey		11
		{ 0.0,	0.5,	0.0,	1.0 },	// < - md.green		12
		{ 0.5,	0.5,	0.0,	1.0 },	// = - md.yellow	13
		{ 0.0,	0.0,	0.5,	1.0 },	// > - md.blue		14
		{ 0.5,	0.0,	0.0,	1.0 },	// ? - md.red		15
		{ 0.5,	0.25,	0.0,	1.0 },	// @ - md.orange	16
		{ 1.0,	0.6f,	0.1f,	1.0 },	// A - lt.orange	17
		{ 0.0,	0.5,	0.5,	1.0 },	// B - md.cyan		18
		{ 0.5,	0.0,	0.5,	1.0 },	// C - md.purple	19
		{ 0.0,	0.5,	1.0,	1.0 },	// D				20
		{ 0.5,	0.0,	1.0,	1.0 },	// E				21
		{ 0.2f,	0.6f,	0.8f,	1.0 },	// F				22
		{ 0.8f,	1.0,	0.8f,	1.0 },	// G				23
		{ 0.0,	0.4,	0.2f,	1.0 },	// H				24
		{ 1.0,	0.0,	0.2f,	1.0 },	// I				25
		{ 0.7f,	0.1f,	0.1f,	1.0 },	// J				26
		{ 0.6f,	0.2f,	0.0,	1.0 },	// K				27
		{ 0.8f,	0.6f,	0.2f,	1.0 },	// L				28
		{ 0.6f,	0.6f,	0.2f,	1.0 },	// M				29
		{ 1.0,	1.0,	0.75,	1.0 },	// N				30
		{ 1.0,	1.0,	0.5,	1.0 },	// O				31
	};



vec3_t	bytedirs[NUMVERTEXNORMALS] =
{
{-0.525731, 0.000000, 0.850651}, {-0.442863, 0.238856, 0.864188}, 
{-0.295242, 0.000000, 0.955423}, {-0.309017, 0.500000, 0.809017}, 
{-0.162460, 0.262866, 0.951056}, {0.000000, 0.000000, 1.000000}, 
{0.000000, 0.850651, 0.525731}, {-0.147621, 0.716567, 0.681718}, 
{0.147621, 0.716567, 0.681718}, {0.000000, 0.525731, 0.850651}, 
{0.309017, 0.500000, 0.809017}, {0.525731, 0.000000, 0.850651}, 
{0.295242, 0.000000, 0.955423}, {0.442863, 0.238856, 0.864188}, 
{0.162460, 0.262866, 0.951056}, {-0.681718, 0.147621, 0.716567}, 
{-0.809017, 0.309017, 0.500000},{-0.587785, 0.425325, 0.688191}, 
{-0.850651, 0.525731, 0.000000},{-0.864188, 0.442863, 0.238856}, 
{-0.716567, 0.681718, 0.147621},{-0.688191, 0.587785, 0.425325}, 
{-0.500000, 0.809017, 0.309017}, {-0.238856, 0.864188, 0.442863}, 
{-0.425325, 0.688191, 0.587785}, {-0.716567, 0.681718, -0.147621}, 
{-0.500000, 0.809017, -0.309017}, {-0.525731, 0.850651, 0.000000}, 
{0.000000, 0.850651, -0.525731}, {-0.238856, 0.864188, -0.442863}, 
{0.000000, 0.955423, -0.295242}, {-0.262866, 0.951056, -0.162460}, 
{0.000000, 1.000000, 0.000000}, {0.000000, 0.955423, 0.295242}, 
{-0.262866, 0.951056, 0.162460}, {0.238856, 0.864188, 0.442863}, 
{0.262866, 0.951056, 0.162460}, {0.500000, 0.809017, 0.309017}, 
{0.238856, 0.864188, -0.442863},{0.262866, 0.951056, -0.162460}, 
{0.500000, 0.809017, -0.309017},{0.850651, 0.525731, 0.000000}, 
{0.716567, 0.681718, 0.147621}, {0.716567, 0.681718, -0.147621}, 
{0.525731, 0.850651, 0.000000}, {0.425325, 0.688191, 0.587785}, 
{0.864188, 0.442863, 0.238856}, {0.688191, 0.587785, 0.425325}, 
{0.809017, 0.309017, 0.500000}, {0.681718, 0.147621, 0.716567}, 
{0.587785, 0.425325, 0.688191}, {0.955423, 0.295242, 0.000000}, 
{1.000000, 0.000000, 0.000000}, {0.951056, 0.162460, 0.262866}, 
{0.850651, -0.525731, 0.000000},{0.955423, -0.295242, 0.000000}, 
{0.864188, -0.442863, 0.238856}, {0.951056, -0.162460, 0.262866}, 
{0.809017, -0.309017, 0.500000}, {0.681718, -0.147621, 0.716567}, 
{0.850651, 0.000000, 0.525731}, {0.864188, 0.442863, -0.238856}, 
{0.809017, 0.309017, -0.500000}, {0.951056, 0.162460, -0.262866}, 
{0.525731, 0.000000, -0.850651}, {0.681718, 0.147621, -0.716567}, 
{0.681718, -0.147621, -0.716567},{0.850651, 0.000000, -0.525731}, 
{0.809017, -0.309017, -0.500000}, {0.864188, -0.442863, -0.238856}, 
{0.951056, -0.162460, -0.262866}, {0.147621, 0.716567, -0.681718}, 
{0.309017, 0.500000, -0.809017}, {0.425325, 0.688191, -0.587785}, 
{0.442863, 0.238856, -0.864188}, {0.587785, 0.425325, -0.688191}, 
{0.688191, 0.587785, -0.425325}, {-0.147621, 0.716567, -0.681718}, 
{-0.309017, 0.500000, -0.809017}, {0.000000, 0.525731, -0.850651}, 
{-0.525731, 0.000000, -0.850651}, {-0.442863, 0.238856, -0.864188}, 
{-0.295242, 0.000000, -0.955423}, {-0.162460, 0.262866, -0.951056}, 
{0.000000, 0.000000, -1.000000}, {0.295242, 0.000000, -0.955423}, 
{0.162460, 0.262866, -0.951056}, {-0.442863, -0.238856, -0.864188}, 
{-0.309017, -0.500000, -0.809017}, {-0.162460, -0.262866, -0.951056}, 
{0.000000, -0.850651, -0.525731}, {-0.147621, -0.716567, -0.681718}, 
{0.147621, -0.716567, -0.681718}, {0.000000, -0.525731, -0.850651}, 
{0.309017, -0.500000, -0.809017}, {0.442863, -0.238856, -0.864188}, 
{0.162460, -0.262866, -0.951056}, {0.238856, -0.864188, -0.442863}, 
{0.500000, -0.809017, -0.309017}, {0.425325, -0.688191, -0.587785}, 
{0.716567, -0.681718, -0.147621}, {0.688191, -0.587785, -0.425325}, 
{0.587785, -0.425325, -0.688191}, {0.000000, -0.955423, -0.295242}, 
{0.000000, -1.000000, 0.000000}, {0.262866, -0.951056, -0.162460}, 
{0.000000, -0.850651, 0.525731}, {0.000000, -0.955423, 0.295242}, 
{0.238856, -0.864188, 0.442863}, {0.262866, -0.951056, 0.162460}, 
{0.500000, -0.809017, 0.309017}, {0.716567, -0.681718, 0.147621}, 
{0.525731, -0.850651, 0.000000}, {-0.238856, -0.864188, -0.442863}, 
{-0.500000, -0.809017, -0.309017}, {-0.262866, -0.951056, -0.162460}, 
{-0.850651, -0.525731, 0.000000}, {-0.716567, -0.681718, -0.147621}, 
{-0.716567, -0.681718, 0.147621}, {-0.525731, -0.850651, 0.000000}, 
{-0.500000, -0.809017, 0.309017}, {-0.238856, -0.864188, 0.442863}, 
{-0.262866, -0.951056, 0.162460}, {-0.864188, -0.442863, 0.238856}, 
{-0.809017, -0.309017, 0.500000}, {-0.688191, -0.587785, 0.425325}, 
{-0.681718, -0.147621, 0.716567}, {-0.442863, -0.238856, 0.864188}, 
{-0.587785, -0.425325, 0.688191}, {-0.309017, -0.500000, 0.809017}, 
{-0.147621, -0.716567, 0.681718}, {-0.425325, -0.688191, 0.587785}, 
{-0.162460, -0.262866, 0.951056}, {0.442863, -0.238856, 0.864188}, 
{0.162460, -0.262866, 0.951056}, {0.309017, -0.500000, 0.809017}, 
{0.147621, -0.716567, 0.681718}, {0.000000, -0.525731, 0.850651}, 
{0.425325, -0.688191, 0.587785}, {0.587785, -0.425325, 0.688191}, 
{0.688191, -0.587785, 0.425325}, {-0.955423, 0.295242, 0.000000}, 
{-0.951056, 0.162460, 0.262866}, {-1.000000, 0.000000, 0.000000}, 
{-0.850651, 0.000000, 0.525731}, {-0.955423, -0.295242, 0.000000}, 
{-0.951056, -0.162460, 0.262866}, {-0.864188, 0.442863, -0.238856}, 
{-0.951056, 0.162460, -0.262866}, {-0.809017, 0.309017, -0.500000}, 
{-0.864188, -0.442863, -0.238856}, {-0.951056, -0.162460, -0.262866}, 
{-0.809017, -0.309017, -0.500000}, {-0.681718, 0.147621, -0.716567}, 
{-0.681718, -0.147621, -0.716567}, {-0.850651, 0.000000, -0.525731}, 
{-0.688191, 0.587785, -0.425325}, {-0.587785, 0.425325, -0.688191}, 
{-0.425325, 0.688191, -0.587785}, {-0.425325, -0.688191, -0.587785}, 
{-0.587785, -0.425325, -0.688191}, {-0.688191, -0.587785, -0.425325}
};

//==============================================================

/* TC 748: original modulo-32 seed update and platform x87 return/store chain. */
#if defined(_MSC_VER) && defined(_M_IX86)
static const float tceRandomScale748 = 0.0000152587890625f;
static const double tceRandomHalf748 = 0.5;
__declspec(naked) int Q_rand(int *seed) {
 __asm {
  mov edx,dword ptr [esp + 0x4]
  mov ecx,dword ptr [edx]
  lea eax,[ecx + ecx*0x4]
  shl eax,0x8
  sub eax,ecx
  lea eax,[eax + eax*0x8]
  lea eax,[ecx + eax*0x2]
  lea eax,[eax + eax*0x2 + 0x1]
  mov dword ptr [edx],eax
  ret
 }
}
__declspec(naked) float Q_random(int *seed) {
 __asm {
  mov eax,dword ptr [esp + 0x4]
  push eax
  call Q_rand
  and eax,0xffff
  add esp,0x4
  mov dword ptr [esp + 0x4],eax
  fild dword ptr [esp + 0x4]
  fmul dword ptr tceRandomScale748
  ret
 }
}
__declspec(naked) float Q_crandom(int *seed) {
 __asm {
  mov eax,dword ptr [esp + 0x4]
  push eax
  call Q_random
  fsub qword ptr tceRandomHalf748
  add esp,0x4
  fadd st(0),st(0)
  ret
 }
}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
__attribute__((naked)) int Q_rand(int *seed) {
 __asm__(
 ".intel_syntax noprefix\n"
 "mov ecx,dword ptr [esp + 0x4]\n"
 "mov eax,dword ptr [ecx]\n"
 "imul eax,eax,0x10dcd\n"
 "inc eax\n"
 "mov dword ptr [ecx],eax\n"
 "ret\n"
 ".att_syntax prefix\n"
 );
}
__attribute__((naked)) float Q_random(int *seed) {
 __asm__(
 ".intel_syntax noprefix\n"
 "sub esp,0xc\n"
 "mov edx,dword ptr [esp + 0x10]\n"
 "mov dword ptr [esp + 0x8],ebx\n"
 "mov dword ptr [esp],edx\n"
 "call Q_rand\n"
 "and eax,0xffff\n"
 "push eax\n"
 "fild dword ptr [esp]\n"
 "mov dword ptr [esp+8],0x37800000\n"
 "fmul dword ptr [esp+8]\n"
 "mov ebx,dword ptr [esp + 0xc]\n"
 "add esp,0x10\n"
 "ret\n"
 ".att_syntax prefix\n"
 );
}
__attribute__((naked)) float Q_crandom(int *seed) {
 __asm__(
 ".intel_syntax noprefix\n"
 "sub esp,0xc\n"
 "mov edx,dword ptr [esp + 0x10]\n"
 "mov dword ptr [esp + 0x8],ebx\n"
 "mov dword ptr [esp],edx\n"
 "call Q_random\n"
 "mov dword ptr [esp+4],0x3f000000\n"
 "fld dword ptr [esp+4]\n"
 "mov ebx,dword ptr [esp + 0x8]\n"
 "fsubp st(1),st(0)\n"
 "fadd st(0),st(0)\n"
 "fstp dword ptr [esp + 0x4]\n"
 "fld dword ptr [esp + 0x4]\n"
 "add esp,0xc\n"
 "ret\n"
 ".att_syntax prefix\n"
 );
}
#else
int		Q_rand( int *seed ) {
	*seed = (69069 * *seed + 1);
	return *seed;
}

float	Q_random( int *seed ) {
	return ( Q_rand( seed ) & 0xffff ) / (float)0x10000;
}

float	Q_crandom( int *seed ) {
	return 2.0 * ( Q_random( seed ) - 0.5 );
}
#endif


//=======================================================

signed char ClampChar( int i ) {
	if ( i < -128 ) {
		return -128;
	}
	if ( i > 127 ) {
		return 127;
	}
	return i;
}

signed short ClampShort( int i ) {
	if ( i < -32768 ) {
		return -32768;
	}
	if ( i > 0x7fff ) {
		return 0x7fff;
	}
	return i;
}


// this isn't a real cheap function to call!
int DirToByte( vec3_t dir ) {
#if defined(_MSC_VER) && defined(_M_IX86)
	int i, best = 0;
	float bestd = 0.f;
	if ( !dir ) return 0;
	for ( i = 0; i < NUMVERTEXNORMALS; ++i ) {
		const float *row = bytedirs[i];
		int replace;
		__asm {
			mov ecx,row
			mov edx,dir
			fld dword ptr [ecx]
			fmul dword ptr [edx]
			fld dword ptr [ecx+8]
			fmul dword ptr [edx+8]
			faddp st(1),st(0)
			fld dword ptr [edx+4]
			fmul dword ptr [ecx+4]
			faddp st(1),st(0)
			fcom bestd
			fnstsw ax
			test ah,041h
			setz al
			movzx eax,al
			mov replace,eax
			jz dirbyte_replace
			fstp st(0)
			jmp dirbyte_done
		dirbyte_replace:
			fstp bestd
		dirbyte_done:
		}
		if ( replace ) best = i;
	}
	return best;
#elif defined(__GNUC__) && defined(__i386__)
	int best;
	if ( !dir ) return 0;
	/* Linux keeps the winning dot product extended across all162 rows. */
	__asm__ volatile (
		".intel_syntax noprefix\n\t"
		"fldz\n\t" "xor esi,esi\n\t" "xor ecx,ecx\n\t"
		"fld dword ptr [ebx]\n\t" "fld dword ptr [ebx+4]\n\t" "fld dword ptr [ebx+8]\n\t"
		"xor edx,edx\n\t"
		"1: fld st(2)\n\t" "fld st(2)\n\t"
		"fmul dword ptr [edi+edx+4]\n\t" "fxch st(1)\n\t"
		"fmul dword ptr [edi+edx]\n\t" "faddp st(1),st(0)\n\t"
		"fld st(1)\n\t" "fmul dword ptr [edi+edx+8]\n\t"
		"faddp st(1),st(0)\n\t" "fcom st(4)\n\t" "fnstsw ax\n\t" "sahf\n\t"
		"jbe 2f\n\t" "fstp st(4)\n\t" "mov esi,ecx\n\t" "jmp 3f\n\t"
		"2: fstp st(0)\n\t"
		"3: inc ecx\n\t" "add edx,12\n\t" "cmp ecx,161\n\t" "jle 1b\n\t"
		"fstp st(0)\n\t" "fstp st(0)\n\t" "fstp st(0)\n\t" "fstp st(0)\n\t"
		".att_syntax prefix"
		: "=S" (best) : "b" (dir), "D" (bytedirs)
		: "eax", "ecx", "edx", "cc", "memory", "st", "st(1)", "st(2)", "st(3)", "st(4)", "st(5)"
	);
	return best;
#else

	int		i, best;
	float	d, bestd;

	if ( !dir ) {
		return 0;
	}

	bestd = 0;
	best = 0;
	for (i=0 ; i<NUMVERTEXNORMALS ; i++)
	{
		d = DotProduct (dir, bytedirs[i]);
		if (d > bestd)
		{
			bestd = d;
			best = i;
		}
	}

	return best;
#endif
}

#if defined(_MSC_VER) && defined(_M_IX86) && defined(CGAMEDLL)
/* TC3007dda0: raw component copies; invalid x alone travels through x87. */
__declspec(naked) void ByteToDir(int b, vec3_t dir) {
 __asm {
  mov eax,dword ptr [esp+4]
  test eax,eax
  jl tceByteInvalid824
  cmp eax,162
  jge tceByteInvalid824
  mov ecx,dword ptr [esp+8]
  lea eax,[eax+eax*2]
  shl eax,2
  mov edx,dword ptr bytedirs[eax]
  mov dword ptr [ecx],edx
  mov edx,dword ptr bytedirs[eax+4]
  mov dword ptr [ecx+4],edx
  mov eax,dword ptr bytedirs[eax+8]
  mov dword ptr [ecx+8],eax
  ret
 tceByteInvalid824:
  fld dword ptr vec3_origin
  mov eax,dword ptr [esp+8]
  fstp dword ptr [eax]
  mov ecx,dword ptr vec3_origin[4]
  mov dword ptr [eax+4],ecx
  mov edx,dword ptr vec3_origin[8]
  mov dword ptr [eax+8],edx
  ret
 }
}
#else
void ByteToDir( int b, vec3_t dir ) {
	if ( b < 0 || b >= NUMVERTEXNORMALS ) {
		VectorCopy( vec3_origin, dir );
		return;
	}
	VectorCopy (bytedirs[b], dir);
}
#endif


unsigned ColorBytes3 (float r, float g, float b) {
	unsigned	i;

	( (byte *)&i )[0] = r * 255;
	( (byte *)&i )[1] = g * 255;
	( (byte *)&i )[2] = b * 255;

	return i;
}

unsigned ColorBytes4 (float r, float g, float b, float a) {
	unsigned	i;

	( (byte *)&i )[0] = r * 255;
	( (byte *)&i )[1] = g * 255;
	( (byte *)&i )[2] = b * 255;
	( (byte *)&i )[3] = a * 255;

	return i;
}

float NormalizeColor( const vec3_t in, vec3_t out ) {
	float	max;
	
	max = in[0];
	if ( in[1] > max ) {
		max = in[1];
	}
	if ( in[2] > max ) {
		max = in[2];
	}

	if ( !max ) {
		VectorClear( out );
	} else {
		out[0] = in[0] / max;
		out[1] = in[1] / max;
		out[2] = in[2] / max;
	}
	return max;
}


/*
=====================
PlaneFromPoints

Returns false if the triangle is degenrate.
The normal will point out of the clock for clockwise ordered points
=====================
*/
qboolean PlaneFromPoints( vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c ) {
	vec3_t	d1, d2;

	VectorSubtract( b, a, d1 );
	VectorSubtract( c, a, d2 );
	CrossProduct( d2, d1, plane );
	if ( VectorNormalize( plane ) == 0 ) {
		return qfalse;
	}

	plane[3] = DotProduct( a, plane );
	return qtrue;
}

/*
===============
RotatePointAroundVector

This is not implemented very well...
===============
*/
#if defined(_MSC_VER) && defined(_M_IX86)
static const unsigned int tceRotate770Pi=0x40490fdbu,tceRotate770Div180=0x3bb60b61u;
__declspec(naked) void RotatePointAroundVector(vec3_t dst,const vec3_t dir,const vec3_t point,float degrees) {
 __asm {
SUB ESP,0xdc
MOV EAX,dword ptr [ESP + 0xe4]
PUSH ESI
PUSH EDI
PUSH EAX
MOV EDX,dword ptr [EAX + 0x4]
MOV ECX,dword ptr [EAX]
MOV dword ptr [ESP + 0x10],EDX
MOV dword ptr [ESP + 0xc],ECX
MOV ECX,dword ptr [EAX + 0x8]
LEA EDX,[ESP + 0x1c]
PUSH EDX
MOV dword ptr [ESP + 0x18],ECX
call PerpendicularVector
LEA EAX,[ESP + 0x2c]
LEA ECX,[ESP + 0x10]
PUSH EAX
LEA EDX,[ESP + 0x24]
PUSH ECX
PUSH EDX
call CrossProduct
FLD dword ptr [ESP + 0x108]
FMUL dword ptr tceRotate770Pi
MOV ECX,dword ptr [ESP + 0x30]
MOV EAX,dword ptr [ESP + 0x2c]
MOV EDX,dword ptr [ESP + 0x34]
MOV dword ptr [ESP + 0x50],ECX
FMUL dword ptr tceRotate770Div180
MOV ECX,dword ptr [ESP + 0x3c]
MOV dword ptr [ESP + 0x44],EAX
MOV EAX,dword ptr [ESP + 0x38]
MOV dword ptr [ESP + 0x5c],EDX
FST dword ptr [ESP + 0x28]
FCOS
MOV EDX,dword ptr [ESP + 0x40]
MOV dword ptr [ESP + 0x54],ECX
MOV ECX,dword ptr [ESP + 0x20]
MOV dword ptr [ESP + 0x48],EAX
MOV EAX,dword ptr [ESP + 0x1c]
MOV dword ptr [ESP + 0x60],EDX
MOV EDX,dword ptr [ESP + 0x24]
MOV dword ptr [ESP + 0x58],ECX
MOV ECX,0x9
LEA ESI,[ESP + 0x44]
LEA EDI,[ESP + 0x68]
MOV dword ptr [ESP + 0x4c],EAX
MOV EAX,dword ptr [ESP + 0x30]
MOV dword ptr [ESP + 0x64],EDX
rep movsd
MOV ECX,dword ptr [ESP + 0x34]
MOV dword ptr [ESP + 0x6c],EAX
MOV EAX,dword ptr [ESP + 0x40]
MOV dword ptr [ESP + 0x70],ECX
MOV ECX,dword ptr [ESP + 0x1c]
MOV dword ptr [ESP + 0x7c],EAX
MOV dword ptr [ESP + 0x80],ECX
MOV ECX,0x9
XOR EAX,EAX
LEA EDI,[ESP + 0x8c]
rep stosd
MOV EDX,dword ptr [ESP + 0x38]
LEA EAX,[ESP + 0xb0]
MOV dword ptr [ESP + 0x74],EDX
MOV EDX,dword ptr [ESP + 0x20]
MOV dword ptr [ESP + 0x84],EDX
LEA ECX,[ESP + 0x8c]
PUSH EAX
LEA EDX,[ESP + 0x48]
PUSH ECX
PUSH EDX
MOV dword ptr [ESP + 0xb8],0x3f800000
FST dword ptr [ESP + 0x98]
FLD dword ptr [ESP + 0x34]
FSIN
FST dword ptr [ESP + 0x9c]
FCHS
FSTP dword ptr [ESP + 0xa4]
FSTP dword ptr [ESP + 0xa8]
call MatrixMultiply
LEA EAX,[ESP + 0xe0]
LEA ECX,[ESP + 0x74]
PUSH EAX
LEA EDX,[ESP + 0xc0]
PUSH ECX
PUSH EDX
call MatrixMultiply
MOV ECX,dword ptr [ESP + 0x11c]
MOV EDX,dword ptr [ESP + 0x114]
ADD ESP,0x2c
LEA EAX,[ESP + 0xc4]
MOV ESI,0x3
tce_rotate770_loop:
FLD dword ptr [EAX + -0x4]
FMUL dword ptr [ECX]
FLD dword ptr [EAX + 0x4]
FMUL dword ptr [ECX + 0x8]
ADD EAX,0xc
ADD EDX,0x4
DEC ESI
faddp st(1),st(0)
FLD dword ptr [EAX + -0xc]
FMUL dword ptr [ECX + 0x4]
faddp st(1),st(0)
FSTP dword ptr [EDX + -0x4]
JNZ tce_rotate770_loop
POP EDI
POP ESI
ADD ESP,0xdc
RET
 }
}
#else
void RotatePointAroundVector( vec3_t dst, const vec3_t dir, const vec3_t point,
							 float degrees ) {
	float	m[3][3];
	float	im[3][3];
	float	zrot[3][3];
	float	tmpmat[3][3];
	float	rot[3][3];
	int	i;
	vec3_t vr, vup, vf;
	float	rad;

	vf[0] = dir[0];
	vf[1] = dir[1];
	vf[2] = dir[2];

	PerpendicularVector( vr, dir );
	CrossProduct( vr, vf, vup );

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vup[0];
	m[1][1] = vup[1];
	m[2][1] = vup[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	memcpy( im, m, sizeof( im ) );

	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

	memset( zrot, 0, sizeof( zrot ) );
	zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0F;

	rad = DEG2RAD( degrees );
	zrot[0][0] = cos( rad );
	zrot[0][1] = sin( rad );
	zrot[1][0] = -sin( rad );
	zrot[1][1] = cos( rad );

	MatrixMultiply( m, zrot, tmpmat );
	MatrixMultiply( tmpmat, im, rot );

	for ( i = 0; i < 3; i++ ) {
		dst[i] = rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2] * point[2];
	}
}
#endif

/*
===============
RotatePointArountVertex

Rotate a point around a vertex
===============
*/
void RotatePointAroundVertex ( vec3_t pnt, float rot_x, float rot_y, float rot_z, const vec3_t origin ) {
	float tmp[11];
	//float rad_x, rad_y, rad_z;

	/*rad_x = DEG2RAD( rot_x );
	rad_y = DEG2RAD( rot_y );
	rad_z = DEG2RAD( rot_z );*/

	// move pnt to rel{0,0,0}
	VectorSubtract( pnt, origin, pnt );

	// init temp values
	tmp[0] = sin( rot_x );
	tmp[1] = cos( rot_x );
	tmp[2] = sin( rot_y );
	tmp[3] = cos( rot_y );
	tmp[4] = sin( rot_z );
	tmp[5] = cos( rot_z );
	tmp[6] = pnt[1] * tmp[5];
	tmp[7] = pnt[0] * tmp[4];
	tmp[8] = pnt[0] * tmp[5];
	tmp[9] = pnt[1] * tmp[4];
	tmp[10] = pnt[2] * tmp[3];

	// rotate point
	pnt[0] = ( tmp[3] * ( tmp[8] - tmp[9] ) + pnt[3] * tmp[2] );
	pnt[1] = ( tmp[0] * ( tmp[2] * tmp[8] - tmp[2] * tmp[9] - tmp[10] ) + tmp[1] * ( tmp[7] + tmp[6] ) );
	pnt[2] = ( tmp[1] * ( -tmp[2] * tmp[8] + tmp[2] * tmp[9] + tmp[10] ) + tmp[0] * ( tmp[7] + tmp[6] ) );

	// move pnt back
	VectorAdd( pnt, origin, pnt );
}

/*
===============
RotateAroundDirection
===============
*/
#if defined(_MSC_VER) && defined(_M_IX86)
/* TC3007dfb0: C3 skips both zero and unordered yaw; preserve integer copies. */
static const float tceRotateZero822 = 0.0f;
__declspec(naked) void RotateAroundDirection(vec3_t axis[3], float yaw) {
 __asm {
  SUB ESP,0xc
  PUSH ESI
  MOV ESI,dword ptr [ESP + 0x14]
  PUSH EDI
  PUSH ESI
  LEA EDI,[ESI + 0xc]
  PUSH EDI
  call PerpendicularVector
  FLD dword ptr [ESP + 0x24]
  fcomp dword ptr tceRotateZero822
  ADD ESP,0x8
  FNSTSW AX
  TEST AH,0x40
  jnz tceRotateCross822
  MOV EAX,dword ptr [EDI]
  MOV ECX,dword ptr [ESI + 0x10]
  MOV EDX,dword ptr [ESI + 0x14]
  MOV dword ptr [ESP + 0x8],EAX
  MOV EAX,dword ptr [ESP + 0x1c]
  MOV dword ptr [ESP + 0xc],ECX
  LEA ECX,[ESP + 0x8]
  PUSH EAX
  PUSH ECX
  PUSH ESI
  PUSH EDI
  MOV dword ptr [ESP + 0x20],EDX
  call RotatePointAroundVector
  ADD ESP,0x10
  tceRotateCross822:
LEA EDX,[ESI + 0x18]
  PUSH EDX
  PUSH EDI
  PUSH ESI
  call CrossProduct
  ADD ESP,0xc
  POP EDI
  POP ESI
  ADD ESP,0xc
  RET
 }
}
#else
void RotateAroundDirection( vec3_t axis[3], float yaw ) {

	// create an arbitrary axis[1] 
	PerpendicularVector( axis[1], axis[0] );

	// rotate it around axis[0] by yaw
	if ( yaw ) {
		vec3_t	temp;

		VectorCopy( axis[1], temp );
		RotatePointAroundVector( axis[1], axis[0], temp, yaw );
	}

	// cross to get axis[2]
	CrossProduct( axis[0], axis[1], axis[2] );
}
#endif



/* TC vectoangles: preserve platform-specific x87 staging and degree constants.
 * Evidence: reconstruction/evidence/vectoangles_739.md. */
#if defined(_MSC_VER) && defined(_M_IX86)
static const float tceVecAnglesZero = 0.0f, tceVecAngles90 = 90.0f;
static const float tceVecAngles270 = 270.0f, tceVecAngles360 = 360.0f;
static const double tceVecAnglesDegrees = 57.29577791868204;
__declspec(naked) void vectoangles(const vec3_t value1, vec3_t angles) {
    __asm {
        tce_va_200a05c0:
            mov ecx,dword ptr [esp + 0x4]
        tce_va_200a05c4:
            fld dword ptr [ecx + 0x4]
        tce_va_200a05c7:
            fcomp dword ptr tceVecAnglesZero
        tce_va_200a05cd:
            fnstsw ax
        tce_va_200a05cf:
            test ah,0x40
        tce_va_200a05d2:
            jz tce_va_200a062b
        tce_va_200a05d4:
            fld dword ptr [ecx]
        tce_va_200a05d6:
            fcomp dword ptr tceVecAnglesZero
        tce_va_200a05dc:
            fnstsw ax
        tce_va_200a05de:
            test ah,0x40
        tce_va_200a05e1:
            jz tce_va_200a062b
        tce_va_200a05e3:
            fld dword ptr tceVecAnglesZero
        tce_va_200a05e9:
            fld dword ptr [ecx + 0x8]
        tce_va_200a05ec:
            fcomp dword ptr tceVecAnglesZero
        tce_va_200a05f2:
            fnstsw ax
        tce_va_200a05f4:
            test ah,0x41
        tce_va_200a05f7:
            jnz tce_va_200a0612
        tce_va_200a05f9:
            fld dword ptr tceVecAngles90
        tce_va_200a05ff:
            mov eax,dword ptr [esp + 0x8]
        tce_va_200a0603:
            fchs
        tce_va_200a0605:
            fstp dword ptr [eax]
        tce_va_200a0607:
            mov dword ptr [eax + 0x8],0x0
        tce_va_200a060e:
            fstp dword ptr [eax + 0x4]
        tce_va_200a0611:
            ret
        tce_va_200a0612:
            fld dword ptr tceVecAngles270
        tce_va_200a0618:
            mov eax,dword ptr [esp + 0x8]
        tce_va_200a061c:
            fchs
        tce_va_200a061e:
            fstp dword ptr [eax]
        tce_va_200a0620:
            mov dword ptr [eax + 0x8],0x0
        tce_va_200a0627:
            fstp dword ptr [eax + 0x4]
        tce_va_200a062a:
            ret
        tce_va_200a062b:
            fld dword ptr [ecx]
        tce_va_200a062d:
            fcomp dword ptr tceVecAnglesZero
        tce_va_200a0633:
            fnstsw ax
        tce_va_200a0635:
            test ah,0x40
        tce_va_200a0638:
            jnz tce_va_200a06a6
        tce_va_200a063a:
            fld dword ptr [ecx + 0x4]
        tce_va_200a063d:
            fld dword ptr [ecx]
        tce_va_200a063f:
            fpatan
        tce_va_200a0641:
            fmul qword ptr tceVecAnglesDegrees
        tce_va_200a0647:
            fcom dword ptr tceVecAnglesZero
        tce_va_200a064d:
            fnstsw ax
        tce_va_200a064f:
            test ah,0x1
        tce_va_200a0652:
            jz tce_va_200a065a
        tce_va_200a0654:
            fadd dword ptr tceVecAngles360
        tce_va_200a065a:
            fld dword ptr [ecx + 0x4]
        tce_va_200a065d:
            fld dword ptr [ecx]
        tce_va_200a065f:
            fld dword ptr [ecx + 0x8]
        tce_va_200a0662:
            fld st(1)
        tce_va_200a0664:
            fmul st(0),st(2)
        tce_va_200a0666:
            fld st(3)
        tce_va_200a0668:
            fmul st(0),st(4)
        tce_va_200a066a:
            faddp st(1),st(0)
        tce_va_200a066c:
            fsqrt
        tce_va_200a066e:
            fstp st(3)
        tce_va_200a0670:
            fxch st(1)
        tce_va_200a0672:
            fxch st(2)
        tce_va_200a0674:
            fpatan
        tce_va_200a0676:
            fmul qword ptr tceVecAnglesDegrees
        tce_va_200a067c:
            fxch st(1)
        tce_va_200a067e:
            fstp st(0)
        tce_va_200a0680:
            fcom dword ptr tceVecAnglesZero
        tce_va_200a0686:
            fnstsw ax
        tce_va_200a0688:
            test ah,0x1
        tce_va_200a068b:
            jz tce_va_200a0693
        tce_va_200a068d:
            fadd dword ptr tceVecAngles360
        tce_va_200a0693:
            mov eax,dword ptr [esp + 0x8]
        tce_va_200a0697:
            fchs
        tce_va_200a0699:
            fstp dword ptr [eax]
        tce_va_200a069b:
            mov dword ptr [eax + 0x8],0x0
        tce_va_200a06a2:
            fstp dword ptr [eax + 0x4]
        tce_va_200a06a5:
            ret
        tce_va_200a06a6:
            fld dword ptr [ecx + 0x4]
        tce_va_200a06a9:
            fcomp dword ptr tceVecAnglesZero
        tce_va_200a06af:
            fnstsw ax
        tce_va_200a06b1:
            test ah,0x41
        tce_va_200a06b4:
            jnz tce_va_200a06be
        tce_va_200a06b6:
            fld dword ptr tceVecAngles90
        tce_va_200a06bc:
            jmp tce_va_200a065a
        tce_va_200a06be:
            fld dword ptr tceVecAngles270
        tce_va_200a06c4:
            jmp tce_va_200a065a
    }
}
#elif defined(__GNUC__) && defined(__i386__)
__attribute__((naked)) void vectoangles(const vec3_t value1, vec3_t angles) {
    __asm__ volatile (
        ".intel_syntax noprefix\n"
        ".Ltce_va_00113e3c:\n"
        "push ebx\n"
        ".Ltce_va_00113e3d:\n"
        "sub esp,0x28\n"
        "mov dword ptr [esp+0x10],0\n"
        "mov dword ptr [esp+0x14],0x42b40000\n"
        "mov dword ptr [esp+0x18],0x43870000\n"
        "mov dword ptr [esp+0x1c],0x43b40000\n"
        "mov dword ptr [esp+0x20],0x1a63c1f8\n"
        "mov dword ptr [esp+0x24],0x404ca5dc\n"
        ".Ltce_va_00113e40:\n"
        "mov ecx,dword ptr [esp + 0x30]\n"
        ".Ltce_va_00113e4f:\n"
        "fld dword ptr [esp + 0x10]\n"
        ".Ltce_va_00113e55:\n"
        "mov edx,dword ptr [esp + 0x34]\n"
        ".Ltce_va_00113e59:\n"
        "fld dword ptr [ecx + 0x4]\n"
        ".Ltce_va_00113e5c:\n"
        "fcom st(1)\n"
        ".Ltce_va_00113e5e:\n"
        "fnstsw ax\n"
        ".Ltce_va_00113e60:\n"
        "sahf\n"
        ".Ltce_va_00113e61:\n"
        "jnz .Ltce_va_00113f22\n"
        ".Ltce_va_00113e67:\n"
        "fld dword ptr [ecx]\n"
        ".Ltce_va_00113e69:\n"
        "fcom st(2)\n"
        ".Ltce_va_00113e6b:\n"
        "fnstsw ax\n"
        ".Ltce_va_00113e6d:\n"
        "sahf\n"
        ".Ltce_va_00113e6e:\n"
        "jnz .Ltce_va_00113eac\n"
        ".Ltce_va_00113e70:\n"
        "fstp st(0)\n"
        ".Ltce_va_00113e72:\n"
        "fstp st(0)\n"
        ".Ltce_va_00113e74:\n"
        "fld dword ptr [ecx + 0x8]\n"
        ".Ltce_va_00113e77:\n"
        "fld st(1)\n"
        ".Ltce_va_00113e79:\n"
        "fxch st(1)\n"
        ".Ltce_va_00113e7b:\n"
        "fcomp st(2)\n"
        ".Ltce_va_00113e7d:\n"
        "fnstsw ax\n"
        ".Ltce_va_00113e7f:\n"
        "fstp st(1)\n"
        ".Ltce_va_00113e81:\n"
        "sahf\n"
        ".Ltce_va_00113e82:\n"
        "jbe .Ltce_va_00113ea4\n"
        ".Ltce_va_00113e84:\n"
        "fld dword ptr [esp + 0x14]\n"
        ".Ltce_va_00113e8a:\n"
        "fxch st(1)\n"
        ".Ltce_va_00113e8c:\n"
        "fxch st(1)\n"
        ".Ltce_va_00113e8e:\n"
        "fchs\n"
        ".Ltce_va_00113e90:\n"
        "fxch st(1)\n"
        ".Ltce_va_00113e92:\n"
        "fstp dword ptr [edx + 0x4]\n"
        ".Ltce_va_00113e95:\n"
        "mov dword ptr [edx + 0x8],0x0\n"
        ".Ltce_va_00113e9c:\n"
        "fstp dword ptr [edx]\n"
        ".Ltce_va_00113e9e:\n"
        "add esp,0x28\n"
        ".Ltce_va_00113ea1:\n"
        "pop ebx\n"
        ".Ltce_va_00113ea2:\n"
        "ret\n"
        ".Ltce_va_00113ea4:\n"
        "fld dword ptr [esp + 0x18]\n"
        ".Ltce_va_00113eaa:\n"
        "jmp .Ltce_va_00113e8a\n"
        ".Ltce_va_00113eac:\n"
        "fld st(1)\n"
        ".Ltce_va_00113eae:\n"
        "fld st(1)\n"
        ".Ltce_va_00113eb0:\n"
        "fpatan\n"
        ".Ltce_va_00113eb2:\n"
        "fstp qword ptr [esp + 0x8]\n"
        ".Ltce_va_00113eb6:\n"
        "fld qword ptr [esp + 0x8]\n"
        ".Ltce_va_00113eba:\n"
        "fld qword ptr [esp + 0x20]\n"
        ".Ltce_va_00113ec0:\n"
        "fmul st(1),st(0)\n"
        ".Ltce_va_00113ec2:\n"
        "fxch st(1)\n"
        ".Ltce_va_00113ec4:\n"
        "fstp dword ptr [esp + 0x4]\n"
        ".Ltce_va_00113ec8:\n"
        "fld dword ptr [esp + 0x4]\n"
        ".Ltce_va_00113ecc:\n"
        "fcom st(4)\n"
        ".Ltce_va_00113ece:\n"
        "fnstsw ax\n"
        ".Ltce_va_00113ed0:\n"
        "sahf\n"
        ".Ltce_va_00113ed1:\n"
        "jnc .Ltce_va_00113ed9\n"
        ".Ltce_va_00113ed3:\n"
        "fadd dword ptr [esp + 0x1c]\n"
        ".Ltce_va_00113ed9:\n"
        "fxch st(3)\n"
        ".Ltce_va_00113edb:\n"
        "fmul st(0),st(0)\n"
        ".Ltce_va_00113edd:\n"
        "fxch st(2)\n"
        ".Ltce_va_00113edf:\n"
        "fmul st(0),st(0)\n"
        ".Ltce_va_00113ee1:\n"
        "faddp st(2),st(0)\n"
        ".Ltce_va_00113ee3:\n"
        "fxch st(1)\n"
        ".Ltce_va_00113ee5:\n"
        "fsqrt\n"
        ".Ltce_va_00113ee7:\n"
        "fstp dword ptr [esp + 0x4]\n"
        ".Ltce_va_00113eeb:\n"
        "fld dword ptr [esp + 0x4]\n"
        ".Ltce_va_00113eef:\n"
        "fld dword ptr [ecx + 0x8]\n"
        ".Ltce_va_00113ef2:\n"
        "fxch st(1)\n"
        ".Ltce_va_00113ef4:\n"
        "fpatan\n"
        ".Ltce_va_00113ef6:\n"
        "fstp qword ptr [esp + 0x8]\n"
        ".Ltce_va_00113efa:\n"
        "fld qword ptr [esp + 0x8]\n"
        ".Ltce_va_00113efe:\n"
        "fmulp st(1),st(0)\n"
        ".Ltce_va_00113f00:\n"
        "fstp dword ptr [esp + 0x4]\n"
        ".Ltce_va_00113f04:\n"
        "fld dword ptr [esp + 0x4]\n"
        ".Ltce_va_00113f08:\n"
        "fcom st(2)\n"
        ".Ltce_va_00113f0a:\n"
        "fnstsw ax\n"
        ".Ltce_va_00113f0c:\n"
        "fstp st(2)\n"
        ".Ltce_va_00113f0e:\n"
        "sahf\n"
        ".Ltce_va_00113f0f:\n"
        "jnc .Ltce_va_00113e8c\n"
        ".Ltce_va_00113f15:\n"
        "fxch st(1)\n"
        ".Ltce_va_00113f17:\n"
        "fadd dword ptr [esp + 0x1c]\n"
        ".Ltce_va_00113f1d:\n"
        "jmp .Ltce_va_00113e8a\n"
        ".Ltce_va_00113f22:\n"
        "fld dword ptr [ecx]\n"
        ".Ltce_va_00113f24:\n"
        "fcom st(2)\n"
        ".Ltce_va_00113f26:\n"
        "fnstsw ax\n"
        ".Ltce_va_00113f28:\n"
        "sahf\n"
        ".Ltce_va_00113f29:\n"
        "jnz .Ltce_va_00113eac\n"
        ".Ltce_va_00113f2b:\n"
        "fxch st(1)\n"
        ".Ltce_va_00113f2d:\n"
        "fcom st(2)\n"
        ".Ltce_va_00113f2f:\n"
        "fnstsw ax\n"
        ".Ltce_va_00113f31:\n"
        "sahf\n"
        ".Ltce_va_00113f32:\n"
        "jbe .Ltce_va_00113f4a\n"
        ".Ltce_va_00113f34:\n"
        "fld dword ptr [esp + 0x14]\n"
        ".Ltce_va_00113f3a:\n"
        "fld qword ptr [esp + 0x20]\n"
        ".Ltce_va_00113f40:\n"
        "fxch st(1)\n"
        ".Ltce_va_00113f42:\n"
        "fxch st(2)\n"
        ".Ltce_va_00113f44:\n"
        "fxch st(3)\n"
        ".Ltce_va_00113f46:\n"
        "fxch st(2)\n"
        ".Ltce_va_00113f48:\n"
        "jmp .Ltce_va_00113ecc\n"
        ".Ltce_va_00113f4a:\n"
        "fld dword ptr [esp + 0x18]\n"
        ".Ltce_va_00113f50:\n"
        "jmp .Ltce_va_00113f3a\n"
        ".att_syntax prefix\n"
    );
}
#else
void vectoangles( const vec3_t value1, vec3_t angles ) {
	float	forward;
	float	yaw, pitch;
	
	if ( value1[1] == 0 && value1[0] == 0 ) {
		yaw = 0;
		if ( value1[2] > 0 ) {
			pitch = 90;
		}
		else {
			pitch = 270;
		}
	}
	else {
		if ( value1[0] ) {
			yaw = ( atan2 ( value1[1], value1[0] ) * 180 / M_PI );
		}
		else if ( value1[1] > 0 ) {
			yaw = 90;
		}
		else {
			yaw = 270;
		}
		if ( yaw < 0 ) {
			yaw += 360;
		}

		forward = sqrt ( value1[0]*value1[0] + value1[1]*value1[1] );
		pitch = ( atan2(value1[2], forward) * 180 / M_PI );
		if ( pitch < 0 ) {
			pitch += 360;
		}
	}

	angles[PITCH] = -pitch;
	angles[YAW] = yaw;
	angles[ROLL] = 0;
}
#endif


/*
=================
AnglesToAxis
=================
*/
void AnglesToAxis( const vec3_t angles, vec3_t axis[3] ) {
	vec3_t	right;

	// angle vectors returns "right" instead of "y axis"
	AngleVectors( angles, axis[0], right, axis[2] );
#if defined(_MSC_VER) && defined(_M_IX86)
	__asm {
		lea ecx,right
		mov edx,axis
		add edx,12
		lea eax,vec3_origin
		fld dword ptr [eax]
		fsub dword ptr [ecx]
		fstp dword ptr [edx]
		fld dword ptr [eax+4]
		fsub dword ptr [ecx+4]
		fstp dword ptr [edx+4]
		fld dword ptr [eax+8]
		fsub dword ptr [ecx+8]
		fstp dword ptr [edx+8]
	}
#elif defined(__GNUC__) && defined(__i386__)
	__asm__ volatile (
		"flds 0(%%ecx)\n\t" "fsubrs 0(%%eax)\n\t" "fstps 0(%%edx)\n\t"
		"flds 4(%%ecx)\n\t" "fsubrs 4(%%eax)\n\t" "fstps 4(%%edx)\n\t"
		"flds 8(%%ecx)\n\t" "fsubrs 8(%%eax)\n\t" "fstps 8(%%edx)"
		: : "c" (right), "a" (vec3_origin), "d" (axis[1]) : "memory", "st"
	);
#else
	VectorSubtract( vec3_origin, right, axis[1] );
#endif
}

void AxisClear( vec3_t axis[3] ) {
	axis[0][0] = 1;
	axis[0][1] = 0;
	axis[0][2] = 0;
	axis[1][0] = 0;
	axis[1][1] = 1;
	axis[1][2] = 0;
	axis[2][0] = 0;
	axis[2][1] = 0;
	axis[2][2] = 1;
}

#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) void AxisCopy( vec3_t in[3], vec3_t out[3] ) {
	__asm {
		mov eax,[esp+4]
		mov ecx,[esp+8]
		mov edx,dword ptr [eax]
		mov dword ptr [ecx],edx
		mov edx,dword ptr [eax+4]
		mov dword ptr [ecx+4],edx
		mov edx,dword ptr [eax+8]
		mov dword ptr [ecx+8],edx
		mov edx,dword ptr [eax+12]
		mov dword ptr [ecx+12],edx
		mov edx,dword ptr [eax+16]
		mov dword ptr [ecx+16],edx
		mov edx,dword ptr [eax+20]
		mov dword ptr [ecx+20],edx
		mov edx,dword ptr [eax+24]
		mov dword ptr [ecx+24],edx
		mov edx,dword ptr [eax+28]
		mov dword ptr [ecx+28],edx
		mov eax,dword ptr [eax+32]
		mov dword ptr [ecx+32],eax
		ret
	}
}
#elif defined(__GNUC__) && defined(__i386__)
__attribute__((naked)) void AxisCopy( vec3_t in[3], vec3_t out[3] ) {
	__asm__ volatile (
		".intel_syntax noprefix\n\t"
		"mov edx,[esp+4]\n\t"
		"mov ecx,[esp+8]\n\t"
		"mov eax,dword ptr [edx]\n\t"
		"mov dword ptr [ecx],eax\n\t"
		"mov eax,dword ptr [edx+4]\n\t"
		"mov dword ptr [ecx+4],eax\n\t"
		"mov eax,dword ptr [edx+8]\n\t"
		"mov dword ptr [ecx+8],eax\n\t"
		"mov eax,dword ptr [edx+12]\n\t"
		"mov dword ptr [ecx+12],eax\n\t"
		"mov eax,dword ptr [edx+16]\n\t"
		"mov dword ptr [ecx+16],eax\n\t"
		"mov eax,dword ptr [edx+20]\n\t"
		"mov dword ptr [ecx+20],eax\n\t"
		"mov eax,dword ptr [edx+24]\n\t"
		"mov dword ptr [ecx+24],eax\n\t"
		"mov eax,dword ptr [edx+28]\n\t"
		"mov dword ptr [ecx+28],eax\n\t"
		"mov eax,dword ptr [edx+32]\n\t"
		"mov dword ptr [ecx+32],eax\n\t"
		"ret\n\t"
		".att_syntax prefix\n\t"
	);
}
#else
void AxisCopy( vec3_t in[3], vec3_t out[3] ) {
	VectorCopy( in[0], out[0] );
	VectorCopy( in[1], out[1] );
	VectorCopy( in[2], out[2] );
}
#endif

#if defined(_MSC_VER) && defined(_M_IX86)
static const float tceProjectOne = 1.f;
__declspec(naked) void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal ) {
	__asm {
		SUB ESP,0ch
		MOV EAX,dword ptr [ESP + 018h]
		MOV ECX,dword ptr [ESP + 014h]
		FLD dword ptr [EAX + 08h]
		FLD dword ptr [EAX + 04h]
		FLD dword ptr [EAX]
		FLD st(0)
		fmul st(0),st(1)
		FLD st(2)
		fmul st(0),st(3)
		faddp st(1),st(0)
		FLD st(3)
		fmul st(0),st(4)
		faddp st(1),st(0)
		FDIVR tceProjectOne
		FSTP st(3)
		FSTP st(0)
		FSTP st(0)
		FLD dword ptr [ECX + 08h]
		FMUL dword ptr [EAX + 08h]
		FLD dword ptr [ECX + 04h]
		FMUL dword ptr [EAX + 04h]
		faddp st(1),st(0)
		FLD dword ptr [ECX]
		FMUL dword ptr [EAX]
		faddp st(1),st(0)
		fmul st(0),st(1)
		FSTP dword ptr [ESP + 018h]
		FLD st(0)
		FMUL dword ptr [EAX]
		FSTP dword ptr [ESP]
		FLD st(0)
		FMUL dword ptr [EAX + 04h]
		FSTP dword ptr [ESP + 04h]
		FMUL dword ptr [EAX + 08h]
		FLD dword ptr [ESP]
		FMUL dword ptr [ESP + 018h]
		MOV EAX,dword ptr [ESP + 010h]
		FSUBR dword ptr [ECX]
		FSTP dword ptr [EAX]
		FLD dword ptr [ESP + 04h]
		FMUL dword ptr [ESP + 018h]
		FSUBR dword ptr [ECX + 04h]
		FSTP dword ptr [EAX + 04h]
		FMUL dword ptr [ESP + 018h]
		FSUBR dword ptr [ECX + 08h]
		FSTP dword ptr [EAX + 08h]
		ADD ESP,0ch
		RET
	}
}
#elif defined(__GNUC__) && defined(__i386__)
__attribute__((naked)) void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal ) {
	__asm__ volatile (
		".intel_syntax noprefix\n\t"
		"PUSH EBX\n\t"
		"SUB ESP,0x18\n\t"
		"MOV EAX,dword ptr [ESP + 0x28]\n\t"
		"mov dword ptr [esp],0x3f800000\n\t"
		"MOV EDX,dword ptr [ESP + 0x24]\n\t"
		"MOV ECX,dword ptr [ESP + 0x20]\n\t"
		"FLD dword ptr [EAX]\n\t"
		"FLD dword ptr [EAX + 0x4]\n\t"
		"FLD dword ptr [EAX + 0x8]\n\t"
		"FLD st(2)\n\t"
		"FLD st(2)\n\t"
		"FLD dword ptr [EDX]\n\t"
		"fxch st(1)\n\t"
		"fmul st(0),st(4)\n\t"
		"FXCH st(2)\n\t"
		"fmul st(0),st(5)\n\t"
		"FLD st(5)\n\t"
		"fmul st(0),st(2)\n\t"
		"fxch st(1)\n\t"
		"FADDP st(3),st(0)\n\t"
		"FLD st(3)\n\t"
		"fmul st(0),st(4)\n\t"
		"FADDP st(3),st(0)\n\t"
		"FLD st(4)\n\t"
		"FXCH st(3)\n\t"
		"FDIVR dword ptr [esp]\n\t"
		"FXCH st(3)\n\t"
		"FMUL dword ptr [EDX + 0x4]\n\t"
		"FXCH st(6)\n\t"
		"fmul st(0),st(3)\n\t"
		"FXCH st(5)\n\t"
		"fmul st(0),st(3)\n\t"
		"fxch st(1)\n\t"
		"FADDP st(6),st(0)\n\t"
		"FLD st(3)\n\t"
		"fmul st(0),st(3)\n\t"
		"FXCH st(4)\n\t"
		"FMUL dword ptr [EDX + 0x8]\n\t"
		"FADDP st(6),st(0)\n\t"
		"FXCH st(5)\n\t"
		"FMULP st(2),st(0)\n\t"
		"FXCH st(3)\n\t"
		"fmul st(0),st(1)\n\t"
		"FXCH st(4)\n\t"
		"fmul st(0),st(1)\n\t"
		"fxch st(1)\n\t"
		"FMULP st(2),st(0)\n\t"
		"FXCH st(2)\n\t"
		"FSUBRP st(3),st(0)\n\t"
		"FXCH st(2)\n\t"
		"FSTP dword ptr [ECX]\n\t"
		"FSUBR dword ptr [EDX + 0x4]\n\t"
		"FSTP dword ptr [ECX + 0x4]\n\t"
		"FSUBR dword ptr [EDX + 0x8]\n\t"
		"FSTP dword ptr [ECX + 0x8]\n\t"
		"ADD ESP,0x18\n\t"
		"POP EBX\n\t"
		"RET\n\t"
		".att_syntax prefix\n\t"
	);
}
#else
void ProjectPointOnPlane( vec3_t dst, const vec3_t p, const vec3_t normal )
{
	float d;
	vec3_t n;
	float inv_denom;

	inv_denom = 1.0F / DotProduct( normal, normal );

	d = DotProduct( normal, p ) * inv_denom;

	n[0] = normal[0] * inv_denom;
	n[1] = normal[1] * inv_denom;
	n[2] = normal[2] * inv_denom;

	dst[0] = p[0] - d * n[0];
	dst[1] = p[1] - d * n[1];
	dst[2] = p[2] - d * n[2];
}
#endif

/*
================
MakeNormalVectors

Given a normalized forward vector, create two
other perpendicular vectors
================
*/
void MakeNormalVectors( const vec3_t forward, vec3_t right, vec3_t up) {
	float		d;

	// this rotate and negate guarantees a vector
	// not colinear with the original
	right[1] = -forward[0];
	right[2] = forward[1];
	right[0] = forward[2];

	d = DotProduct (right, forward);
	VectorMA (right, -d, forward, right);
	VectorNormalize (right);
	CrossProduct (right, forward, up);
}


void VectorRotate( vec3_t in, vec3_t matrix[3], vec3_t out )
{
	out[0] = DotProduct( in, matrix[0] );
	out[1] = DotProduct( in, matrix[1] );
	out[2] = DotProduct( in, matrix[2] );
}

//============================================================================

/*
** float q_rsqrt( float number )
*/
#if defined(_MSC_VER) && defined(_M_IX86)
static const float tceRsqrtHalf = .5f, tceRsqrtThreeHalfs = 1.5f;
__declspec(naked) float Q_rsqrt( float number ) {
	__asm {
		PUSH ECX
		MOV EAX,dword ptr [ESP + 08h]
		MOV EDX,05f3759dfh
		FLD dword ptr [ESP + 08h]
		FMUL tceRsqrtHalf
		MOV ECX,EAX
		MOV dword ptr [ESP],EAX
		SAR ECX,01h
		SUB EDX,ECX
		MOV dword ptr [ESP],EDX
		FMUL dword ptr [ESP]
		FMUL dword ptr [ESP]
		FSUBR tceRsqrtThreeHalfs
		FMUL dword ptr [ESP]
		POP ECX
		RET
	}
}
#elif defined(__GNUC__) && defined(__i386__)
__attribute__((naked)) float Q_rsqrt( float number ) {
	__asm__ volatile (
		".intel_syntax noprefix\n\t"
		"sub esp,12\n\t"
		"mov dword ptr [esp+4],0x3f000000\n\t"
		"mov dword ptr [esp+8],0x3fc00000\n\t"
		"MOV EAX,dword ptr [ESP + 0x10]\n\t"
		"MOV dword ptr [ESP],EAX\n\t"
		"MOV EDX,EAX\n\t"
		"SAR EDX,0x1\n\t"
		"FLD dword ptr [ESP]\n\t"
		"MOV EAX,0x5f3759df\n\t"
		"SUB EAX,EDX\n\t"
		"FMUL dword ptr [esp+4]\n\t"
		"MOV dword ptr [ESP],EAX\n\t"
		"FLD dword ptr [ESP]\n\t"
		"FLD st(0)\n\t"
		"FXCH st(2)\n\t"
		"fmul st(0),st(1)\n\t"
		"fmulp st(1),st(0)\n\t"
		"FSUBR dword ptr [esp+8]\n\t"
		"add esp,12\n\t"
		"fmulp st(1),st(0)\n\t"
		"RET\n\t"
		".att_syntax prefix\n\t"
	);
}
#else
float Q_rsqrt( float number )
{
	long i;
	float x2, y;
	const float threehalfs = 1.5F;

	x2 = number * 0.5F;
	y  = number;
	i  = * ( long * ) &y;						// evil floating point bit level hacking
	i  = 0x5f3759df - ( i >> 1 );               // what the fuck?
	y  = * ( float * ) &i;
	y  = y * ( threehalfs - ( x2 * y * y ) );   // 1st iteration
//	y  = y * ( threehalfs - ( x2 * y * y ) );   // 2nd iteration, this can be removed

	return y;
}
#endif

/* Original x86 branch/staging contract: line_744.md. */
#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) float Q_fabs(float f) {
    __asm {
        tce_744_200a07f0:
        mov eax,dword ptr [esp + 0x4]
        tce_744_200a07f4:
        and eax,0x7fffffff
        tce_744_200a07f9:
        mov dword ptr [esp + 0x4],eax
        tce_744_200a07fd:
        fld dword ptr [esp + 0x4]
        tce_744_200a0801:
        ret
    }
}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
__attribute__((naked)) float Q_fabs(float f) {
    __asm__(
        ".intel_syntax noprefix\n"
        ".Ltce_744_001141e6:\n"
        "sub esp,0x4\n"
        ".Ltce_744_001141e9:\n"
        "mov edx,dword ptr [esp + 0x8]\n"
        ".Ltce_744_001141ed:\n"
        "and edx,0x7fffffff\n"
        ".Ltce_744_001141f3:\n"
        "mov dword ptr [esp],edx\n"
        ".Ltce_744_001141f6:\n"
        "fld dword ptr [esp]\n"
        ".Ltce_744_001141f9:\n"
        "pop edx\n"
        ".Ltce_744_001141fa:\n"
        "ret\n"
        ".att_syntax prefix\n"
    );
}
#else
float Q_fabs( float f ) {
	int tmp = (*(int*)&f) & 0x7FFFFFFF;
	return *(float*)&tmp;
}
#endif

#if id386 && !( (defined __linux__ || defined __FreeBSD__ || defined __GNUC__ ) && (defined __i386__ ) ) // rb010123
long myftol( float f ) {
	static int tmp;
	__asm fld f
	__asm fistp tmp
	__asm mov eax, tmp
}
#endif

//============================================================

/*
===============
LerpAngle

===============
*/
#if defined(_MSC_VER) && defined(_M_IX86)
static const float la763Pos=180.f, la763Neg=-180.f, la763Turn=360.f;
__declspec(naked) float LerpAngle(float from,float to,float frac) {
 __asm {
FLD dword ptr [ESP + 0x8]
FLD st(0)
FSUB dword ptr [ESP + 0x4]
FCOMP dword ptr la763Pos
FNSTSW AX
TEST AH,041h
JNZ la763_3007e31d
FSUB dword ptr la763Turn
la763_3007e31d:
FLD st(0)
FSUB dword ptr [ESP + 0x4]
FCOMP dword ptr la763Neg
FNSTSW AX
TEST AH,01h
JZ la763_3007e336
FADD dword ptr la763Turn
la763_3007e336:
FSUB dword ptr [ESP + 0x4]
FMUL dword ptr [ESP + 0xc]
FADD dword ptr [ESP + 0x4]
RET
 }
}
#elif defined(__GNUC__) && defined(__i386__)
__attribute__((naked)) float LerpAngle(float from,float to,float frac) {
 __asm__ volatile (
 ".intel_syntax noprefix\n\t"
 "sub esp,12\n\t"
 "mov dword ptr [esp],0x43340000\n\t"
 "mov dword ptr [esp+4],0x43b40000\n\t"
 "mov dword ptr [esp+8],0xc3340000\n\t"
 "FLD dword ptr [esp+20]\n\t"
 "FLD dword ptr [esp+16]\n\t"
 "FLD dword ptr [esp+24]\n\t"
 "FLD st(2)\n\t"
 "FSUB st(0),st(2)\n\t"
 "FCOM dword ptr [esp]\n\t"
 "FNSTSW AX\n\t"
 "SAHF\n\t"
 "JBE la763_000ecc42\n\t"
 "FSTP st(0)\n\t"
 "FXCH st(2)\n\t"
 "FSUB dword ptr [esp+4]\n\t"
 "FLD st(0)\n\t"
 "FSUB st(0),st(2)\n\t"
 "FXCH st(1)\n\t"
 "FXCH st(3)\n\t"
 "FXCH st(1)\n\t"
 "la763_000ecc42:\n\t"
 "FCOM dword ptr [esp+8]\n\t"
 "FNSTSW AX\n\t"
 "SAHF\n\t"
 "JNC la763_000ecc60\n\t"
 "FSTP st(0)\n\t"
 "FXCH st(2)\n\t"
 "FADD dword ptr [esp+4]\n\t"
 "FSUB st(0),st(1)\n\t"
 "FXCH st(2)\n\t"
 "la763_000ecc5b:\n\t"
 "FMULP st(2),st(0)\n\t"
 "FADDP st(1),st(0)\n\t"
 "ADD ESP,12\n\t"
 "RET\n\t"
 "la763_000ecc60:\n\t"
 "FSTP st(3)\n\t"
 "JMP la763_000ecc5b\n\t"
 ".att_syntax prefix\n\t"
 );
}
#else
float LerpAngle(float from,float to,float frac) {
 if(to-from>180)to-=360;
 if(to-from< -180)to+=360;
 return from+frac*(to-from);
}
#endif

/*
=================
LerpPosition

=================
*/

/* Original platform interpolation stores: lerp_746.md. */
#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) void LerpPosition(vec3_t start, vec3_t end, float frac, vec3_t result) {
    __asm {
        sub esp,0xc
        mov ecx,dword ptr [esp + 0x14]
        mov eax,dword ptr [esp + 0x10]
        fld dword ptr [ecx]
        fsub dword ptr [eax]
        fld dword ptr [ecx + 0x4]
        fsub dword ptr [eax + 0x4]
        fstp dword ptr [esp + 0x4]
        fld dword ptr [ecx + 0x8]
        fsub dword ptr [eax + 0x8]
        mov ecx,dword ptr [esp + 0x1c]
        fstp dword ptr [esp + 0x8]
        fmul dword ptr [esp + 0x18]
        fadd dword ptr [eax]
        fstp dword ptr [ecx]
        fld dword ptr [esp + 0x4]
        fmul dword ptr [esp + 0x18]
        fadd dword ptr [eax + 0x4]
        fstp dword ptr [ecx + 0x4]
        fld dword ptr [esp + 0x8]
        fmul dword ptr [esp + 0x18]
        fadd dword ptr [eax + 0x8]
        fstp dword ptr [ecx + 0x8]
        add esp,0xc
        ret
    }
}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
__attribute__((naked)) void LerpPosition(vec3_t start, vec3_t end, float frac, vec3_t result) {
    __asm__(
        ".intel_syntax noprefix\n"
        "sub esp,0x1c\n"
        "mov edx,dword ptr [esp + 0x20]\n"
        "mov eax,dword ptr [esp + 0x24]\n"
        "fld dword ptr [esp + 0x28]\n"
        "mov ecx,dword ptr [esp + 0x2c]\n"
        "fld dword ptr [edx]\n"
        "fld dword ptr [eax]\n"
        "fld dword ptr [edx + 0x4]\n"
        "fsubr dword ptr [eax + 0x4]\n"
        "fxch st(1)\n"
        "fsub st(0),st(2)\n"
        "fld dword ptr [edx + 0x8]\n"
        "fsubr dword ptr [eax + 0x8]\n"
        "fxch st(1)\n"
        "fmul st(0),st(4)\n"
        "fxch st(2)\n"
        "fmul st(0),st(4)\n"
        "fxch st(1)\n"
        "fmulp st(4),st(0)\n"
        "fxch st(2)\n"
        "faddp st(1),st(0)\n"
        "fstp dword ptr [ecx]\n"
        "fadd dword ptr [edx + 0x4]\n"
        "fstp dword ptr [ecx + 0x4]\n"
        "fadd dword ptr [edx + 0x8]\n"
        "fstp dword ptr [ecx + 0x8]\n"
        "add esp,0x1c\n"
        "ret\n"
        ".att_syntax prefix\n"
    );
}
#else
void LerpPosition( vec3_t start, vec3_t end, float frac, vec3_t out) {
	vec3_t dist;

	VectorSubtract( end, start, dist );
	VectorMA(start, frac, dist, out);
}
#endif

/*
=================
AngleSubtract

Always returns a value from -180 to 180
=================
*/
#if defined(_MSC_VER) && defined(_M_IX86)
static const float tceAngleHalfTurn = 180.0f;
static const float tceAngleNegativeHalfTurn = -180.0f;
static const float tceAngleTurn = 360.0f;
/* Naked x86 entry preserves the original extended ST0 result until its caller stores it. */
__declspec(naked) float AngleSubtract( float a1, float a2 )
{
    __asm {
        fld dword ptr [esp + 4]
        fsub dword ptr [esp + 8]
        fcom tceAngleHalfTurn
        fnstsw ax
        test ah, 041h
        jnz angle_sub_lower
    angle_sub_upper:
        fsub tceAngleTurn
        fcom tceAngleHalfTurn
        fnstsw ax
        test ah, 041h
        jz angle_sub_upper
    angle_sub_lower:
        fcom tceAngleNegativeHalfTurn
        fnstsw ax
        test ah, 1
        jz angle_sub_done
    angle_sub_add:
        fadd tceAngleTurn
        fcom tceAngleNegativeHalfTurn
        fnstsw ax
        test ah, 1
        jnz angle_sub_add
    angle_sub_done:
        ret
    }
}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
__attribute__((naked)) float AngleSubtract( float a1, float a2 )
{
    /* Stack-local constants keep this whole original sequence position independent. */
    __asm__(
        ".intel_syntax noprefix\n"
        "sub esp, 12\n"
        "mov dword ptr [esp], 0x43340000\n"
        "mov dword ptr [esp+4], 0xc3340000\n"
        "mov dword ptr [esp+8], 0x43b40000\n"
        "fld dword ptr [esp+20]\n"
        "fsubr dword ptr [esp+16]\n"
        "fld dword ptr [esp]\n"
        "fcom st(1)\n"
        "fnstsw ax\n"
        "sahf\n"
        "jnc 5f\n"
        "fld dword ptr [esp+8]\n"
        "1: fsub st(2), st(0)\n"
        "fxch st(2)\n"
        "fcom st(1)\n"
        "fnstsw ax\n"
        "sahf\n"
        "jbe 2f\n"
        "fxch st(2)\n"
        "jmp 1b\n"
        "2: fstp st(1)\n"
        "fstp st(1)\n"
        "3: fld dword ptr [esp+4]\n"
        "fcom st(1)\n"
        "fnstsw ax\n"
        "sahf\n"
        "jbe 6f\n"
        "fld dword ptr [esp+8]\n"
        "4: fadd st(2), st(0)\n"
        "fxch st(2)\n"
        "fcom st(1)\n"
        "fnstsw ax\n"
        "sahf\n"
        "jnc 7f\n"
        "fxch st(2)\n"
        "jmp 4b\n"
        "5: fstp st(0)\n"
        "jmp 3b\n"
        "6: fstp st(0)\n"
        "jmp 8f\n"
        "7: fstp st(1)\n"
        "fstp st(1)\n"
        "8: add esp, 12\n"
        "ret\n"
        ".att_syntax prefix\n"
    );
}
#else
/* Portable fallback does not claim the original x87 exceptional-input contract. */
float AngleSubtract( float a1, float a2 )
{
    float a = a1 - a2;
    while ( a > 180 ) { a -= 360; }
    while ( a < -180 ) { a += 360; }
    return a;
}
#endif


void AnglesSubtract( vec3_t v1, vec3_t v2, vec3_t v3 )
{
	v3[0] = AngleSubtract( v1[0], v2[0] );
	v3[1] = AngleSubtract( v1[1], v2[1] );
	v3[2] = AngleSubtract( v1[2], v2[2] );
}


#if defined(_MSC_VER) && defined(_M_IX86)
static const double tceAngleToShort = 65536.0 / 360.0;
static const double tceShortToAngle = 360.0 / 65536.0;
__declspec(naked) float AngleMod(float a)
{
    __asm {
        fld dword ptr [esp + 4]
        fmul tceAngleToShort
        sub esp, 16
        /* Original __ftol uses a truncating 64-bit conversion and restores the CW. */
        fstcw word ptr [esp + 12]
        fwait
        mov ax, word ptr [esp + 12]
        or ah, 0ch
        mov word ptr [esp + 14], ax
        fldcw word ptr [esp + 14]
        fistp qword ptr [esp]
        fldcw word ptr [esp + 12]
        mov eax, dword ptr [esp]
        and eax, 0ffffh
        mov dword ptr [esp], eax
        fild dword ptr [esp]
        fmul tceShortToAngle
        add esp, 16
        ret
    }
}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
__attribute__((naked)) float AngleMod(float a)
{
    __asm__(
        ".intel_syntax noprefix\n"
        "sub esp, 24\n"
        "mov dword ptr [esp+8], 0x16c16c17\n"
        "mov dword ptr [esp+12], 0x4066c16c\n"
        "mov dword ptr [esp+16], 0x3bb40000\n"
        "fnstcw word ptr [esp+4]\n"
        "movzx ecx, word ptr [esp+4]\n"
        "fld qword ptr [esp+8]\n"
        "fmul dword ptr [esp+28]\n"
        "or cx, 0x0c00\n"
        "mov word ptr [esp+6], cx\n"
        "fldcw word ptr [esp+6]\n"
        "fistp dword ptr [esp]\n"
        "fldcw word ptr [esp+4]\n"
        "mov ecx, dword ptr [esp]\n"
        "and ecx, 0xffff\n"
        "mov dword ptr [esp], ecx\n"
        "fild dword ptr [esp]\n"
        "fmul dword ptr [esp+16]\n"
        "fstp dword ptr [esp]\n"
        "fld dword ptr [esp]\n"
        "add esp, 24\n"
        "ret\n"
        ".att_syntax prefix\n"
    );
}
#else
float AngleMod(float a)
{
    return((360.0 / 65536) * ((int)(a * (65536 / 360.0)) & 65535));
}
#endif

/*
=================
AngleNormalize2Pi

returns angle normalized to the range [0 <= angle < 2*M_PI]
=================
*/
float AngleNormalize2Pi ( float angle ) {
	return DEG2RAD( AngleNormalize360( RAD2DEG( angle ) ) );
}

/*
=================
AngleNormalize360

returns angle normalized to the range [0 <= angle < 360]
=================
*/
/* Same original conversion as AngleMod, with no C float-return spill. */
#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) float AngleNormalize360(float angle) {
    __asm { jmp AngleMod }
}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
__attribute__((naked)) float AngleNormalize360(float angle) {
    __asm__("jmp AngleMod");
}
#else
float AngleNormalize360 ( float angle ) {
	return (360.0 / 65536) * ((int)(angle * (65536 / 360.0)) & 65535);
}
#endif


/*
=================
AngleNormalize180

returns angle normalized to the range [-180 < angle <= 180]
=================
*/
/* Original caller consumes Normalize360 directly in ST0. */
#if defined(_MSC_VER) && defined(_M_IX86)
static const double tceNormalizeHalf = 180.0, tceNormalizeTurn = 360.0;
__declspec(naked) float AngleNormalize180(float angle) {
    __asm {
        mov eax, dword ptr [esp+4]
        push eax
        call AngleNormalize360
        fcom qword ptr tceNormalizeHalf
        add esp, 4
        fnstsw ax
        test ah, 41h
        jnz tce_normalize180_done
        fsub qword ptr tceNormalizeTurn
    tce_normalize180_done:
        ret
    }
}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
__attribute__((naked)) float AngleNormalize180(float angle) {
    __asm__(
        ".intel_syntax noprefix\n"
        "sub esp,20\n"
        "fld dword ptr [esp+24]\n"
        "mov dword ptr [esp+8],ebx\n"
        "mov dword ptr [esp+12],0x43340000\n"
        "mov dword ptr [esp+16],0x43b40000\n"
        "fstp dword ptr [esp]\n"
        "call AngleNormalize360\n"
        "fld st(0)\n"
        "fld dword ptr [esp+12]\n"
        "fcomp st(1)\n"
        "fnstsw ax\n"
        "sahf\n"
        "jnc 1f\n"
        "fstp st(1)\n"
        "fsub dword ptr [esp+16]\n"
        "fstp dword ptr [esp+4]\n"
        "fld dword ptr [esp+4]\n"
        "jmp 2f\n"
        "1: fstp st(0)\n"
        "2: mov ebx,dword ptr [esp+8]\n"
        "add esp,20\n"
        "ret\n"
        ".att_syntax prefix\n"
    );
}
#else
float AngleNormalize180 ( float angle ) {
	angle = AngleNormalize360( angle );
	if ( angle > 180.0 ) {
		angle -= 360.0;
	}
	return angle;
}
#endif


/*
=================
AngleDelta

returns the normalized delta from angle1 to angle2
=================
*/
/* Original x86 argument staging and retained return: shared_742.md. */
#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) float AngleDelta(float angle1, float angle2) {
    __asm {
        fld dword ptr [esp + 0x4]
        fsub dword ptr [esp + 0x8]
        push ecx
        fstp dword ptr [esp]
        call AngleNormalize180
        pop ecx
        ret
    }
}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
__attribute__((naked)) float AngleDelta(float angle1, float angle2) {
    __asm__(
        ".intel_syntax noprefix\n"
        "sub esp,0xc\n"
        "fld dword ptr [esp + 0x14]\n"
        "fsubr dword ptr [esp + 0x10]\n"
        "mov dword ptr [esp + 0x8],ebx\n"
        "fstp dword ptr [esp]\n"
        "call AngleNormalize180\n"
        "mov ebx,dword ptr [esp + 0x8]\n"
        "add esp,0xc\n"
        "ret\n"
        ".att_syntax prefix\n"
    );
}
#else
float AngleDelta ( float angle1, float angle2 ) {
	return AngleNormalize180( angle1 - angle2 );
}
#endif


//============================================================


/*
=================
SetPlaneSignbits
=================
*/
void SetPlaneSignbits (cplane_t *out) {
	int	bits, j;

	// for fast box on planeside test
	bits = 0;
	for (j=0 ; j<3 ; j++) {
		if (out->normal[j] < 0) {
			bits |= 1<<j;
		}
	}
	out->signbits = bits;
}


/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2

// this is the slow, general version
int BoxOnPlaneSide2 (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	int		i;
	float	dist1, dist2;
	int		sides;
	vec3_t	corners[2];

	for (i=0 ; i<3 ; i++)
	{
		if (p->normal[i] < 0)
		{
			corners[0][i] = emins[i];
			corners[1][i] = emaxs[i];
		}
		else
		{
			corners[1][i] = emins[i];
			corners[0][i] = emaxs[i];
		}
	}
	dist1 = DotProduct (p->normal, corners[0]) - p->dist;
	dist2 = DotProduct (p->normal, corners[1]) - p->dist;
	sides = 0;
	if (dist1 >= 0)
		sides = 1;
	if (dist2 < 0)
		sides |= 2;

	return sides;
}

==================
*/
#if !(defined __linux__ && defined __i386__ && !defined C_ONLY)
#if defined __LCC__ || defined C_ONLY || !id386 || __GNUC__
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	float	dist1, dist2;
	int		sides;

// fast axial cases
	if (p->type < 3)
	{
		if (p->dist <= emins[p->type])
			return 1;
		if (p->dist >= emaxs[p->type])
			return 2;
		return 3;
	}

// general case
	switch (p->signbits)
	{
	case 0:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		break;
	case 1:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		break;
	case 2:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		break;
	case 3:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		break;
	case 4:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		break;
	case 5:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emaxs[2];
		break;
	case 6:
		dist1 = p->normal[0]*emaxs[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emins[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		break;
	case 7:
		dist1 = p->normal[0]*emins[0] + p->normal[1]*emins[1] + p->normal[2]*emins[2];
		dist2 = p->normal[0]*emaxs[0] + p->normal[1]*emaxs[1] + p->normal[2]*emaxs[2];
		break;
	default:
		dist1 = dist2 = 0;		// shut up compiler
		break;
	}

	sides = 0;
	if (dist1 >= p->dist)
		sides = 1;
	if (dist2 < p->dist)
		sides |= 2;

	return sides;
}
#else
#pragma warning( disable: 4035 )

__inline __declspec( naked ) int BoxOnPlaneSide_fast (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	static int bops_initialized;
	static int Ljmptab[8];

	__asm {

		push ebx
			
		cmp bops_initialized, 1
		je  initialized
		mov bops_initialized, 1
		
		mov Ljmptab[0*4], offset Lcase0
		mov Ljmptab[1*4], offset Lcase1
		mov Ljmptab[2*4], offset Lcase2
		mov Ljmptab[3*4], offset Lcase3
		mov Ljmptab[4*4], offset Lcase4
		mov Ljmptab[5*4], offset Lcase5
		mov Ljmptab[6*4], offset Lcase6
		mov Ljmptab[7*4], offset Lcase7
			
initialized:

		mov edx,dword ptr[4+12+esp]
		mov ecx,dword ptr[4+4+esp]
		xor eax,eax
		mov ebx,dword ptr[4+8+esp]
		mov al,byte ptr[17+edx]
		cmp al,8
		jge Lerror
		fld dword ptr[0+edx]
		fld st(0)
		jmp dword ptr[Ljmptab+eax*4]
Lcase0:
		fmul dword ptr[ebx]
		fld dword ptr[0+4+edx]
		fxch st(2)
		fmul dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4+ebx]
		fld dword ptr[0+8+edx]
		fxch st(2)
		fmul dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase1:
		fmul dword ptr[ecx]
		fld dword ptr[0+4+edx]
		fxch st(2)
		fmul dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4+ebx]
		fld dword ptr[0+8+edx]
		fxch st(2)
		fmul dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase2:
		fmul dword ptr[ebx]
		fld dword ptr[0+4+edx]
		fxch st(2)
		fmul dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4+ecx]
		fld dword ptr[0+8+edx]
		fxch st(2)
		fmul dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase3:
		fmul dword ptr[ecx]
		fld dword ptr[0+4+edx]
		fxch st(2)
		fmul dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4+ecx]
		fld dword ptr[0+8+edx]
		fxch st(2)
		fmul dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase4:
		fmul dword ptr[ebx]
		fld dword ptr[0+4+edx]
		fxch st(2)
		fmul dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4+ebx]
		fld dword ptr[0+8+edx]
		fxch st(2)
		fmul dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase5:
		fmul dword ptr[ecx]
		fld dword ptr[0+4+edx]
		fxch st(2)
		fmul dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4+ebx]
		fld dword ptr[0+8+edx]
		fxch st(2)
		fmul dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase6:
		fmul dword ptr[ebx]
		fld dword ptr[0+4+edx]
		fxch st(2)
		fmul dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4+ecx]
		fld dword ptr[0+8+edx]
		fxch st(2)
		fmul dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase7:
		fmul dword ptr[ecx]
		fld dword ptr[0+4+edx]
		fxch st(2)
		fmul dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[4+ecx]
		fld dword ptr[0+8+edx]
		fxch st(2)
		fmul dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
LSetSides:
		faddp st(2),st(0)
		fcomp dword ptr[12+edx]
		xor ecx,ecx
		fnstsw ax
		fcomp dword ptr[12+edx]
		and ah,1
		xor ah,1
		add cl,ah
		fnstsw ax
		and ah,1
		add ah,ah
		add cl,ah
		pop ebx
		mov eax,ecx
		ret
Lerror:
		int 3
	}
}

int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *p) {
	// fast axial cases

	if (p->type < 3) {
		if (p->dist <= emins[p->type])
			return 1;
		if (p->dist >= emaxs[p->type])
			return 2;
		return 3;
	}

	return BoxOnPlaneSide_fast( emins, emaxs, p );
}

#pragma warning( default: 4035 )

#endif
#endif

/*
=================
RadiusFromBounds
=================
*/
/* TC749: platform-specific unordered selection and x87 helper boundaries. */
#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) float RadiusFromBounds(const vec3_t mins, const vec3_t maxs) {
 __asm {
  tce749_200a0900:
  sub esp,0xc
  tce749_200a0903:
  push ebx
  tce749_200a0904:
  push ebp
  tce749_200a0905:
  mov ebp,dword ptr [esp + 0x18]
  tce749_200a0909:
  push esi
  tce749_200a090a:
  mov esi,dword ptr [esp + 0x20]
  tce749_200a090e:
  push edi
  tce749_200a090f:
  lea edi,[esp + 0x10]
  tce749_200a0913:
  sub ebp,esi
  tce749_200a0915:
  sub edi,esi
  tce749_200a0917:
  mov ebx,0x3
  tce749_200a091c:
  mov eax,dword ptr [esi + ebp*0x1]
  tce749_200a091f:
  push eax
  tce749_200a0920:
  call Q_fabs
  tce749_200a0925:
  mov ecx,dword ptr [esi]
  tce749_200a0927:
  fstp dword ptr [esp + 0x28]
  tce749_200a092b:
  push ecx
  tce749_200a092c:
  call Q_fabs
  tce749_200a0931:
  fld dword ptr [esp + 0x2c]
  tce749_200a0935:
  fcomp st(1)
  tce749_200a0937:
  add esp,0x8
  tce749_200a093a:
  fnstsw ax
  tce749_200a093c:
  test ah,0x41
  tce749_200a093f:
  jnz tce749_200a0947
  tce749_200a0941:
  fstp st(0)
  tce749_200a0943:
  fld dword ptr [esp + 0x24]
  tce749_200a0947:
  fstp dword ptr [edi + esi*0x1]
  tce749_200a094a:
  add esi,0x4
  tce749_200a094d:
  dec ebx
  tce749_200a094e:
  jnz tce749_200a091c
  tce749_200a0950:
  lea edx,[esp + 0x10]
  tce749_200a0954:
  push edx
  tce749_200a0955:
  call VectorLength
  tce749_200a095a:
  add esp,0x4
  tce749_200a095d:
  pop edi
  tce749_200a095e:
  pop esi
  tce749_200a095f:
  pop ebp
  tce749_200a0960:
  pop ebx
  tce749_200a0961:
  add esp,0xc
  tce749_200a0964:
  ret
 }
}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
__attribute__((naked)) float RadiusFromBounds(const vec3_t mins, const vec3_t maxs) {
 __asm__(
 ".intel_syntax noprefix\n"
 ".Ltce749_00114530:\n"
 "push ebp\n"
 ".Ltce749_00114531:\n"
 "push edi\n"
 ".Ltce749_00114532:\n"
 "push esi\n"
 ".Ltce749_00114533:\n"
 "xor esi,esi\n"
 ".Ltce749_00114535:\n"
 "push ebx\n"
 ".Ltce749_00114536:\n"
 "sub esp,0x3c\n"
 ".Ltce749_00114539:\n"
 "mov ebp,dword ptr [esp + 0x50]\n"
 ".Ltce749_0011453d:\n"
 "mov edi,dword ptr [esp + 0x54]\n"
 ".Ltce749_0011454c:\n"
 "mov edx,dword ptr [ebp + esi*0x4]\n"
 ".Ltce749_00114550:\n"
 "mov dword ptr [esp],edx\n"
 ".Ltce749_00114553:\n"
 "call Q_fabs\n"
 ".Ltce749_00114558:\n"
 "fstp dword ptr [esp + 0x1c]\n"
 ".Ltce749_0011455c:\n"
 "mov edx,dword ptr [edi + esi*0x4]\n"
 ".Ltce749_0011455f:\n"
 "mov dword ptr [esp],edx\n"
 ".Ltce749_00114562:\n"
 "call Q_fabs\n"
 ".Ltce749_00114567:\n"
 "fcom dword ptr [esp + 0x1c]\n"
 ".Ltce749_0011456b:\n"
 "fnstsw ax\n"
 ".Ltce749_0011456d:\n"
 "sahf\n"
 ".Ltce749_0011456e:\n"
 "jnc .Ltce749_00114576\n"
 ".Ltce749_00114570:\n"
 "fstp st(0)\n"
 ".Ltce749_00114572:\n"
 "fld dword ptr [esp + 0x1c]\n"
 ".Ltce749_00114576:\n"
 "fstp dword ptr [esp + esi*0x4 + 0x20]\n"
 ".Ltce749_0011457a:\n"
 "inc esi\n"
 ".Ltce749_0011457b:\n"
 "cmp esi,0x2\n"
 ".Ltce749_0011457e:\n"
 "jle .Ltce749_0011454c\n"
 ".Ltce749_00114580:\n"
 "lea ecx,[esp + 0x20]\n"
 ".Ltce749_00114584:\n"
 "mov dword ptr [esp],ecx\n"
 ".Ltce749_00114587:\n"
 "call VectorLength\n"
 ".Ltce749_0011458c:\n"
 "add esp,0x3c\n"
 ".Ltce749_0011458f:\n"
 "pop ebx\n"
 ".Ltce749_00114590:\n"
 "pop esi\n"
 ".Ltce749_00114591:\n"
 "pop edi\n"
 ".Ltce749_00114592:\n"
 "pop ebp\n"
 ".Ltce749_00114593:\n"
 "ret\n"
 ".att_syntax prefix\n"
 );
}
#else
float RadiusFromBounds( const vec3_t mins, const vec3_t maxs ) {
	int		i;
	vec3_t	corner;
	float	a, b;

	for (i=0 ; i<3 ; i++) {
		a = Q_fabs( mins[i] );
		b = Q_fabs( maxs[i] );
		corner[i] = a > b ? a : b;
	}

	return VectorLength (corner);
}
#endif


void ClearBounds( vec3_t mins, vec3_t maxs ) {
	mins[0] = mins[1] = mins[2] = 99999;
	maxs[0] = maxs[1] = maxs[2] = -99999;
}

/* TC750: original ordered/unordered bounds gates and platform store contract. */
#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) void AddPointToBounds(const vec3_t v, vec3_t mins, vec3_t maxs) {
 __asm {
  tce750_200a0970:
  mov ecx,dword ptr [esp + 0x4]
  tce750_200a0974:
  push esi
  tce750_200a0975:
  mov esi,dword ptr [esp + 0xc]
  tce750_200a0979:
  fld dword ptr [ecx]
  tce750_200a097b:
  fcomp dword ptr [esi]
  tce750_200a097d:
  fnstsw ax
  tce750_200a097f:
  test ah,0x1
  tce750_200a0982:
  jz tce750_200a0988
  tce750_200a0984:
  mov eax,dword ptr [ecx]
  tce750_200a0986:
  mov dword ptr [esi],eax
  tce750_200a0988:
  mov edx,dword ptr [esp + 0x10]
  tce750_200a098c:
  fld dword ptr [ecx]
  tce750_200a098e:
  fcomp dword ptr [edx]
  tce750_200a0990:
  fnstsw ax
  tce750_200a0992:
  test ah,0x41
  tce750_200a0995:
  jnz tce750_200a099b
  tce750_200a0997:
  mov eax,dword ptr [ecx]
  tce750_200a0999:
  mov dword ptr [edx],eax
  tce750_200a099b:
  fld dword ptr [ecx + 0x4]
  tce750_200a099e:
  fcomp dword ptr [esi + 0x4]
  tce750_200a09a1:
  fnstsw ax
  tce750_200a09a3:
  test ah,0x1
  tce750_200a09a6:
  jz tce750_200a09ae
  tce750_200a09a8:
  mov eax,dword ptr [ecx + 0x4]
  tce750_200a09ab:
  mov dword ptr [esi + 0x4],eax
  tce750_200a09ae:
  fld dword ptr [ecx + 0x4]
  tce750_200a09b1:
  fcomp dword ptr [edx + 0x4]
  tce750_200a09b4:
  fnstsw ax
  tce750_200a09b6:
  test ah,0x41
  tce750_200a09b9:
  jnz tce750_200a09c1
  tce750_200a09bb:
  mov eax,dword ptr [ecx + 0x4]
  tce750_200a09be:
  mov dword ptr [edx + 0x4],eax
  tce750_200a09c1:
  fld dword ptr [ecx + 0x8]
  tce750_200a09c4:
  fcomp dword ptr [esi + 0x8]
  tce750_200a09c7:
  fnstsw ax
  tce750_200a09c9:
  test ah,0x1
  tce750_200a09cc:
  jz tce750_200a09d4
  tce750_200a09ce:
  mov eax,dword ptr [ecx + 0x8]
  tce750_200a09d1:
  mov dword ptr [esi + 0x8],eax
  tce750_200a09d4:
  fld dword ptr [ecx + 0x8]
  tce750_200a09d7:
  fcomp dword ptr [edx + 0x8]
  tce750_200a09da:
  pop esi
  tce750_200a09db:
  fnstsw ax
  tce750_200a09dd:
  test ah,0x41
  tce750_200a09e0:
  jnz tce750_200a09e8
  tce750_200a09e2:
  mov ecx,dword ptr [ecx + 0x8]
  tce750_200a09e5:
  mov dword ptr [edx + 0x8],ecx
  tce750_200a09e8:
  ret
 }
}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
__attribute__((naked)) void AddPointToBounds(const vec3_t v, vec3_t mins, vec3_t maxs) {
 __asm__(
 ".intel_syntax noprefix\n"
 ".Ltce750_001145c6:\n"
 "push esi\n"
 ".Ltce750_001145c7:\n"
 "mov edx,dword ptr [esp + 0x8]\n"
 ".Ltce750_001145cb:\n"
 "mov esi,dword ptr [esp + 0xc]\n"
 ".Ltce750_001145cf:\n"
 "mov ecx,dword ptr [esp + 0x10]\n"
 ".Ltce750_001145d3:\n"
 "fld dword ptr [edx]\n"
 ".Ltce750_001145d5:\n"
 "fcom dword ptr [esi]\n"
 ".Ltce750_001145d7:\n"
 "fnstsw ax\n"
 ".Ltce750_001145d9:\n"
 "sahf\n"
 ".Ltce750_001145da:\n"
 "jnc .Ltce750_001145e0\n"
 ".Ltce750_001145dc:\n"
 "fstp dword ptr [esi]\n"
 ".Ltce750_001145de:\n"
 "fld dword ptr [edx]\n"
 ".Ltce750_001145e0:\n"
 "fcom dword ptr [ecx]\n"
 ".Ltce750_001145e2:\n"
 "fnstsw ax\n"
 ".Ltce750_001145e4:\n"
 "sahf\n"
 ".Ltce750_001145e5:\n"
 "jbe .Ltce750_0011462c\n"
 ".Ltce750_001145e7:\n"
 "fstp dword ptr [ecx]\n"
 ".Ltce750_001145e9:\n"
 "fld dword ptr [edx + 0x4]\n"
 ".Ltce750_001145ec:\n"
 "fcom dword ptr [esi + 0x4]\n"
 ".Ltce750_001145ef:\n"
 "fnstsw ax\n"
 ".Ltce750_001145f1:\n"
 "sahf\n"
 ".Ltce750_001145f2:\n"
 "jnc .Ltce750_001145fa\n"
 ".Ltce750_001145f4:\n"
 "fstp dword ptr [esi + 0x4]\n"
 ".Ltce750_001145f7:\n"
 "fld dword ptr [edx + 0x4]\n"
 ".Ltce750_001145fa:\n"
 "fcom dword ptr [ecx + 0x4]\n"
 ".Ltce750_001145fd:\n"
 "fnstsw ax\n"
 ".Ltce750_001145ff:\n"
 "sahf\n"
 ".Ltce750_00114600:\n"
 "jbe .Ltce750_00114628\n"
 ".Ltce750_00114602:\n"
 "fstp dword ptr [ecx + 0x4]\n"
 ".Ltce750_00114605:\n"
 "fld dword ptr [edx + 0x8]\n"
 ".Ltce750_00114608:\n"
 "fcom dword ptr [esi + 0x8]\n"
 ".Ltce750_0011460b:\n"
 "fnstsw ax\n"
 ".Ltce750_0011460d:\n"
 "sahf\n"
 ".Ltce750_0011460e:\n"
 "jnc .Ltce750_00114616\n"
 ".Ltce750_00114610:\n"
 "fstp dword ptr [esi + 0x8]\n"
 ".Ltce750_00114613:\n"
 "fld dword ptr [edx + 0x8]\n"
 ".Ltce750_00114616:\n"
 "fcom dword ptr [ecx + 0x8]\n"
 ".Ltce750_00114619:\n"
 "fnstsw ax\n"
 ".Ltce750_0011461b:\n"
 "sahf\n"
 ".Ltce750_0011461c:\n"
 "jbe .Ltce750_00114624\n"
 ".Ltce750_0011461e:\n"
 "fstp dword ptr [ecx + 0x8]\n"
 ".Ltce750_00114621:\n"
 "pop esi\n"
 ".Ltce750_00114622:\n"
 "ret\n"
 ".Ltce750_00114624:\n"
 "fstp st(0)\n"
 ".Ltce750_00114626:\n"
 "jmp .Ltce750_00114621\n"
 ".Ltce750_00114628:\n"
 "fstp st(0)\n"
 ".Ltce750_0011462a:\n"
 "jmp .Ltce750_00114605\n"
 ".Ltce750_0011462c:\n"
 "fstp st(0)\n"
 ".Ltce750_0011462e:\n"
 "jmp .Ltce750_001145e9\n"
 ".att_syntax prefix\n"
 );
}
#else
void AddPointToBounds( const vec3_t v, vec3_t mins, vec3_t maxs ) {
	if ( v[0] < mins[0] ) {
		mins[0] = v[0];
	}
	if ( v[0] > maxs[0]) {
		maxs[0] = v[0];
	}

	if ( v[1] < mins[1] ) {
		mins[1] = v[1];
	}
	if ( v[1] > maxs[1]) {
		maxs[1] = v[1];
	}

	if ( v[2] < mins[2] ) {
		mins[2] = v[2];
	}
	if ( v[2] > maxs[2]) {
		maxs[2] = v[2];
	}
}
#endif

/* TC750: original ordered/unordered bounds gates and platform store contract. */
#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) qboolean PointInBounds(const vec3_t v, const vec3_t mins, const vec3_t maxs) {
 __asm {
  tce750_200a09f0:
  mov ecx,dword ptr [esp + 0x4]
  tce750_200a09f4:
  push esi
  tce750_200a09f5:
  mov esi,dword ptr [esp + 0xc]
  tce750_200a09f9:
  fld dword ptr [ecx]
  tce750_200a09fb:
  fcomp dword ptr [esi]
  tce750_200a09fd:
  fnstsw ax
  tce750_200a09ff:
  test ah,0x1
  tce750_200a0a02:
  jz tce750_200a0a08
  tce750_200a0a04:
  xor eax,eax
  tce750_200a0a06:
  pop esi
  tce750_200a0a07:
  ret
  tce750_200a0a08:
  mov edx,dword ptr [esp + 0x10]
  tce750_200a0a0c:
  fld dword ptr [ecx]
  tce750_200a0a0e:
  fcomp dword ptr [edx]
  tce750_200a0a10:
  fnstsw ax
  tce750_200a0a12:
  test ah,0x41
  tce750_200a0a15:
  jnz tce750_200a0a1b
  tce750_200a0a17:
  xor eax,eax
  tce750_200a0a19:
  pop esi
  tce750_200a0a1a:
  ret
  tce750_200a0a1b:
  fld dword ptr [ecx + 0x4]
  tce750_200a0a1e:
  fcomp dword ptr [esi + 0x4]
  tce750_200a0a21:
  fnstsw ax
  tce750_200a0a23:
  test ah,0x1
  tce750_200a0a26:
  jz tce750_200a0a2c
  tce750_200a0a28:
  xor eax,eax
  tce750_200a0a2a:
  pop esi
  tce750_200a0a2b:
  ret
  tce750_200a0a2c:
  fld dword ptr [ecx + 0x4]
  tce750_200a0a2f:
  fcomp dword ptr [edx + 0x4]
  tce750_200a0a32:
  fnstsw ax
  tce750_200a0a34:
  test ah,0x41
  tce750_200a0a37:
  jnz tce750_200a0a3d
  tce750_200a0a39:
  xor eax,eax
  tce750_200a0a3b:
  pop esi
  tce750_200a0a3c:
  ret
  tce750_200a0a3d:
  fld dword ptr [ecx + 0x8]
  tce750_200a0a40:
  fcomp dword ptr [esi + 0x8]
  tce750_200a0a43:
  fnstsw ax
  tce750_200a0a45:
  test ah,0x1
  tce750_200a0a48:
  jz tce750_200a0a4e
  tce750_200a0a4a:
  xor eax,eax
  tce750_200a0a4c:
  pop esi
  tce750_200a0a4d:
  ret
  tce750_200a0a4e:
  fld dword ptr [ecx + 0x8]
  tce750_200a0a51:
  fcomp dword ptr [edx + 0x8]
  tce750_200a0a54:
  fnstsw ax
  tce750_200a0a56:
  test ah,0x41
  tce750_200a0a59:
  jnz tce750_200a0a5f
  tce750_200a0a5b:
  xor eax,eax
  tce750_200a0a5d:
  pop esi
  tce750_200a0a5e:
  ret
  tce750_200a0a5f:
  mov eax,0x1
  tce750_200a0a64:
  pop esi
  tce750_200a0a65:
  ret
 }
}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
__attribute__((naked)) qboolean PointInBounds(const vec3_t v, const vec3_t mins, const vec3_t maxs) {
 __asm__(
 ".intel_syntax noprefix\n"
 ".Ltce750_00114630:\n"
 "sub esp,0x8\n"
 ".Ltce750_00114633:\n"
 "mov edx,dword ptr [esp + 0xc]\n"
 ".Ltce750_00114637:\n"
 "mov dword ptr [esp],esi\n"
 ".Ltce750_0011463a:\n"
 "mov ecx,dword ptr [esp + 0x10]\n"
 ".Ltce750_0011463e:\n"
 "xor esi,esi\n"
 ".Ltce750_00114640:\n"
 "mov dword ptr [esp + 0x4],edi\n"
 ".Ltce750_00114644:\n"
 "mov edi,dword ptr [esp + 0x14]\n"
 ".Ltce750_00114648:\n"
 "fld dword ptr [edx]\n"
 ".Ltce750_0011464a:\n"
 "fcom dword ptr [ecx]\n"
 ".Ltce750_0011464c:\n"
 "fnstsw ax\n"
 ".Ltce750_0011464e:\n"
 "sahf\n"
 ".Ltce750_0011464f:\n"
 "jc .Ltce750_00114690\n"
 ".Ltce750_00114651:\n"
 "fcomp dword ptr [edi]\n"
 ".Ltce750_00114653:\n"
 "fnstsw ax\n"
 ".Ltce750_00114655:\n"
 "sahf\n"
 ".Ltce750_00114656:\n"
 "ja .Ltce750_00114682\n"
 ".Ltce750_00114658:\n"
 "fld dword ptr [edx + 0x4]\n"
 ".Ltce750_0011465b:\n"
 "fcom dword ptr [ecx + 0x4]\n"
 ".Ltce750_0011465e:\n"
 "fnstsw ax\n"
 ".Ltce750_00114660:\n"
 "sahf\n"
 ".Ltce750_00114661:\n"
 "jc .Ltce750_00114690\n"
 ".Ltce750_00114663:\n"
 "fcomp dword ptr [edi + 0x4]\n"
 ".Ltce750_00114666:\n"
 "fnstsw ax\n"
 ".Ltce750_00114668:\n"
 "sahf\n"
 ".Ltce750_00114669:\n"
 "ja .Ltce750_00114682\n"
 ".Ltce750_0011466b:\n"
 "fld dword ptr [edx + 0x8]\n"
 ".Ltce750_0011466e:\n"
 "fcom dword ptr [ecx + 0x8]\n"
 ".Ltce750_00114671:\n"
 "fnstsw ax\n"
 ".Ltce750_00114673:\n"
 "sahf\n"
 ".Ltce750_00114674:\n"
 "jc .Ltce750_00114690\n"
 ".Ltce750_00114676:\n"
 "fcomp dword ptr [edi + 0x8]\n"
 ".Ltce750_00114679:\n"
 "fnstsw ax\n"
 ".Ltce750_0011467b:\n"
 "sahf\n"
 ".Ltce750_0011467c:\n"
 "setbe dl\n"
 ".Ltce750_0011467f:\n"
 "movzx esi,dl\n"
 ".Ltce750_00114682:\n"
 "mov eax,esi\n"
 ".Ltce750_00114684:\n"
 "mov edi,dword ptr [esp + 0x4]\n"
 ".Ltce750_00114688:\n"
 "mov esi,dword ptr [esp]\n"
 ".Ltce750_0011468b:\n"
 "add esp,0x8\n"
 ".Ltce750_0011468e:\n"
 "ret\n"
 ".Ltce750_00114690:\n"
 "fstp st(0)\n"
 ".Ltce750_00114692:\n"
 "jmp .Ltce750_00114682\n"
 ".att_syntax prefix\n"
 );
}
#else
qboolean PointInBounds( const vec3_t v, const vec3_t mins, const vec3_t maxs ) {
	if ( v[0] < mins[0] ) {
		return qfalse;
	}
	if ( v[0] > maxs[0]) {
		return qfalse;
	}

	if ( v[1] < mins[1] ) {
		return qfalse;
	}
	if ( v[1] > maxs[1]) {
		return qfalse;
	}

	if ( v[2] < mins[2] ) {
		return qfalse;
	}
	if ( v[2] > maxs[2]) {
		return qfalse;
	}

	return qtrue;
}
#endif


/* TC751: original C3-only comparisons include unordered operands. */
#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) int VectorCompare(const vec3_t v1, const vec3_t v2) {
 __asm {
  tce751_200a0a70:
  mov ecx,dword ptr [esp + 0x4]
  tce751_200a0a74:
  mov edx,dword ptr [esp + 0x8]
  tce751_200a0a78:
  fld dword ptr [ecx]
  tce751_200a0a7a:
  fcomp dword ptr [edx]
  tce751_200a0a7c:
  fnstsw ax
  tce751_200a0a7e:
  test ah,0x40
  tce751_200a0a81:
  jz tce751_200a0aa3
  tce751_200a0a83:
  fld dword ptr [ecx + 0x4]
  tce751_200a0a86:
  fcomp dword ptr [edx + 0x4]
  tce751_200a0a89:
  fnstsw ax
  tce751_200a0a8b:
  test ah,0x40
  tce751_200a0a8e:
  jz tce751_200a0aa3
  tce751_200a0a90:
  fld dword ptr [ecx + 0x8]
  tce751_200a0a93:
  fcomp dword ptr [edx + 0x8]
  tce751_200a0a96:
  fnstsw ax
  tce751_200a0a98:
  test ah,0x40
  tce751_200a0a9b:
  jz tce751_200a0aa3
  tce751_200a0a9d:
  mov eax,0x1
  tce751_200a0aa2:
  ret
  tce751_200a0aa3:
  xor eax,eax
  tce751_200a0aa5:
  ret
 }
}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
__attribute__((naked)) int VectorCompare(const vec3_t v1, const vec3_t v2) {
 __asm__(
 ".intel_syntax noprefix\n"
 ".Ltce751_00114694:\n"
 "mov ecx,dword ptr [esp + 0x4]\n"
 ".Ltce751_00114698:\n"
 "mov edx,dword ptr [esp + 0x8]\n"
 ".Ltce751_0011469c:\n"
 "fld dword ptr [ecx]\n"
 ".Ltce751_0011469e:\n"
 "fcomp dword ptr [edx]\n"
 ".Ltce751_001146a0:\n"
 "fnstsw ax\n"
 ".Ltce751_001146a2:\n"
 "sahf\n"
 ".Ltce751_001146a3:\n"
 "jnz .Ltce751_001146c0\n"
 ".Ltce751_001146a5:\n"
 "fld dword ptr [ecx + 0x4]\n"
 ".Ltce751_001146a8:\n"
 "fcomp dword ptr [edx + 0x4]\n"
 ".Ltce751_001146ab:\n"
 "fnstsw ax\n"
 ".Ltce751_001146ad:\n"
 "sahf\n"
 ".Ltce751_001146ae:\n"
 "jnz .Ltce751_001146c0\n"
 ".Ltce751_001146b0:\n"
 "fld dword ptr [ecx + 0x8]\n"
 ".Ltce751_001146b3:\n"
 "mov ecx,0x1\n"
 ".Ltce751_001146b8:\n"
 "fcomp dword ptr [edx + 0x8]\n"
 ".Ltce751_001146bb:\n"
 "fnstsw ax\n"
 ".Ltce751_001146bd:\n"
 "sahf\n"
 ".Ltce751_001146be:\n"
 "jz .Ltce751_001146c2\n"
 ".Ltce751_001146c0:\n"
 "xor ecx,ecx\n"
 ".Ltce751_001146c2:\n"
 "mov eax,ecx\n"
 ".Ltce751_001146c4:\n"
 "ret\n"
 ".att_syntax prefix\n"
 );
}
#else
int VectorCompare( const vec3_t v1, const vec3_t v2 ) {
	if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2]) {
		return 0;
	}
			
	return 1;
}
#endif


/* Original TC x86 return precision differs by platform; no C return spill. */
#if defined(_MSC_VER) && defined(_M_IX86)
static const float tceNormalizeZero = 0.0f, tceNormalizeOne = 1.0f;
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) vec_t VectorNormalize( vec3_t v ) {
    __asm {
        mov ecx, [esp+4]
        fld dword ptr [ecx+8]
        fld dword ptr [ecx+4]
        fld dword ptr [ecx]
        fld st(0)
        fmul st(0), st(1)
        fld st(2)
        fmul st(0), st(3)
        faddp st(1), st(0)
        fld st(3)
        fmul st(0), st(4)
        faddp st(1), st(0)
        fsqrt
        fstp st(3)
        fstp st(0)
        fstp st(0)
        fcom dword ptr tceNormalizeZero
        fnstsw ax
        test ah,40h
        jnz zeroLength
        fld dword ptr tceNormalizeOne
        fdiv st(0),st(1)
        fld st(0)
        fmul dword ptr [ecx]
        fstp dword ptr [ecx]
        fld st(0)
        fmul dword ptr [ecx+4]
        fstp dword ptr [ecx+4]
        fmul dword ptr [ecx+8]
        fstp dword ptr [ecx+8]
        ret
zeroLength:
        ret
    }
}
#elif defined(__GNUC__) && defined(__i386__)
__attribute__((naked)) vec_t VectorNormalize( vec3_t v ) {
    __asm__ volatile (
        ".intel_syntax noprefix\n\t"
        "sub esp,12\n\t"
        "mov edx,[esp+16]\n\t"
        "mov dword ptr [esp+4],0\n\t"
        "mov dword ptr [esp+8],0x3f800000\n\t"
        "fld dword ptr [edx]\n\t"
        "fld dword ptr [edx+4]\n\t"
        "fld dword ptr [edx+8]\n\t"
        "fld st(2)\n\t"
        "fld st(2)\n\t"
        "fmul st,st(3)\n\t"
        "fxch st(1)\n\t"
        "fmul st,st(4)\n\t"
        "faddp st(1),st\n\t"
        "fld st(1)\n\t"
        "fmul st,st(2)\n\t"
        "faddp st(1),st\n\t"
        "fsqrt\n\t"
        "fstp dword ptr [esp]\n\t"
        "fld dword ptr [esp]\n\t"
        "fcom dword ptr [esp+4]\n\t"
        "fnstsw ax\n\t"
        "sahf\n\t"
        "jz 1f\n\t"
        "fld dword ptr [esp+8]\n\t"
        "fdiv st,st(1)\n\t"
        "fmul st(4),st\n\t"
        "fmul st(3),st\n\t"
        "fmulp st(2),st\n\t"
        "fxch st(3)\n\t"
        "fstp dword ptr [edx]\n\t"
        "fxch st(1)\n\t"
        "fstp dword ptr [edx+4]\n\t"
        "fstp dword ptr [edx+8]\n\t"
        "add esp,12\n\t"
        "ret\n\t"
        "1:\n\t"
        "fstp st(1)\n\t"
        "fstp st(1)\n\t"
        "fstp st(1)\n\t"
        "add esp,12\n\t"
        "ret\n\t"
        ".att_syntax prefix\n\t"
    );
}
#else
vec_t VectorNormalize( vec3_t v ) {
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = sqrt (length);

	if ( length ) {
		ilength = 1/length;
		v[0] *= ilength;
		v[1] *= ilength;
		v[2] *= ilength;
	}
		
	return length;
}
#endif

//
// fast vector normalize routine that does not check to make sure
// that length != 0, nor does it return length
//
#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) void VectorNormalizeFast( vec3_t v ) {
	__asm {
		PUSH ESI
		MOV ESI,dword ptr [ESP + 08h]
		PUSH ECX
		FLD dword ptr [ESI + 08h]
		FLD dword ptr [ESI + 04h]
		FLD dword ptr [ESI]
		FLD st(0)
		fmul st(0),st(1)
		FLD st(2)
		fmul st(0),st(3)
		faddp st(1),st(0)
		FLD st(3)
		fmul st(0),st(4)
		faddp st(1),st(0)
		FSTP dword ptr [ESP]
		FSTP st(0)
		FSTP st(0)
		FSTP st(0)
		call Q_rsqrt
		FLD st(0)
		FMUL dword ptr [ESI]
		ADD ESP,04h
		FSTP dword ptr [ESI]
		FLD st(0)
		FMUL dword ptr [ESI + 04h]
		FSTP dword ptr [ESI + 04h]
		FMUL dword ptr [ESI + 08h]
		FSTP dword ptr [ESI + 08h]
		POP ESI
		RET
	}
}
#elif defined(__GNUC__) && defined(__i386__)
__attribute__((naked)) void VectorNormalizeFast( vec3_t v ) {
	__asm__ volatile (
		".intel_syntax noprefix\n\t"
		"SUB ESP,0xc\n\t"
		"MOV dword ptr [ESP + 0x8],ESI\n\t"
		"MOV ESI,dword ptr [ESP + 0x10]\n\t"
		"MOV dword ptr [ESP + 0x4],EBX\n\t"
		"FLD dword ptr [ESI]\n\t"
		"FLD dword ptr [ESI + 0x4]\n\t"
		"fxch st(1)\n\t"
		"fmul st(0),st(0)\n\t"
		"fxch st(1)\n\t"
		"fmul st(0),st(0)\n\t"
		"faddp st(1),st(0)\n\t"
		"FLD dword ptr [ESI + 0x8]\n\t"
		"fmul st(0),st(0)\n\t"
		"faddp st(1),st(0)\n\t"
		"FSTP dword ptr [ESP]\n\t"
		"call Q_rsqrt\n\t"
		"FLD dword ptr [ESI]\n\t"
		"fmul st(0),st(1)\n\t"
		"FSTP dword ptr [ESI]\n\t"
		"FLD dword ptr [ESI + 0x4]\n\t"
		"fmul st(0),st(1)\n\t"
		"fxch st(1)\n\t"
		"FMUL dword ptr [ESI + 0x8]\n\t"
		"fxch st(1)\n\t"
		"FSTP dword ptr [ESI + 0x4]\n\t"
		"FSTP dword ptr [ESI + 0x8]\n\t"
		"MOV EBX,dword ptr [ESP + 0x4]\n\t"
		"MOV ESI,dword ptr [ESP + 0x8]\n\t"
		"ADD ESP,0xc\n\t"
		"RET\n\t"
		".att_syntax prefix\n\t"
	);
}
#else
void VectorNormalizeFast( vec3_t v )
{
	float ilength;

	ilength = Q_rsqrt( DotProduct( v, v ) );

	v[0] *= ilength;
	v[1] *= ilength;
	v[2] *= ilength;
}
#endif

#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) vec_t VectorNormalize2( const vec3_t v, vec3_t out ) {
    __asm {
        mov ecx, [esp+4]
        fld dword ptr [ecx+8]
        fld dword ptr [ecx+4]
        fld dword ptr [ecx]
        fld st(0)
        fmul st(0), st(1)
        fld st(2)
        fmul st(0), st(3)
        faddp st(1), st(0)
        fld st(3)
        fmul st(0), st(4)
        faddp st(1), st(0)
        fsqrt
        fstp st(3)
        fstp st(0)
        fstp st(0)
        fcom dword ptr tceNormalizeZero
        fnstsw ax
        test ah,40h
        jnz zeroLength
        fld dword ptr tceNormalizeOne
        fdiv st(0),st(1)
        mov edx,[esp+8]
        fld st(0)
        fmul dword ptr [ecx]
        fstp dword ptr [edx]
        fld st(0)
        fmul dword ptr [ecx+4]
        fstp dword ptr [edx+4]
        fmul dword ptr [ecx+8]
        fstp dword ptr [edx+8]
        ret
zeroLength:
        mov edx,[esp+8]
        mov dword ptr [edx+8],0
        mov dword ptr [edx+4],0
        mov dword ptr [edx],0
        ret
    }
}
#elif defined(__GNUC__) && defined(__i386__)
__attribute__((naked)) vec_t VectorNormalize2( const vec3_t v, vec3_t out ) {
    __asm__ volatile (
        ".intel_syntax noprefix\n\t"
        "sub esp,12\n\t"
        "mov ecx,[esp+16]\n\t"
        "mov edx,[esp+20]\n\t"
        "mov dword ptr [esp+4],0\n\t"
        "mov dword ptr [esp+8],0x3f800000\n\t"
        "fld dword ptr [ecx]\n\t"
        "fld dword ptr [ecx+4]\n\t"
        "fld st(1)\n\t"
        "fmul st,st(2)\n\t"
        "fxch st(1)\n\t"
        "fmul st,st\n\t"
        "faddp st(1),st\n\t"
        "fld dword ptr [ecx+8]\n\t"
        "fmul st,st\n\t"
        "faddp st(1),st\n\t"
        "fsqrt\n\t"
        "fstp dword ptr [esp]\n\t"
        "fld dword ptr [esp]\n\t"
        "fcom dword ptr [esp+4]\n\t"
        "fnstsw ax\n\t"
        "sahf\n\t"
        "jz 1f\n\t"
        "fld dword ptr [esp+8]\n\t"
        "fdiv st,st(1)\n\t"
        "fmul st(2),st\n\t"
        "fxch st(2)\n\t"
        "fstp dword ptr [edx]\n\t"
        "fld dword ptr [ecx+4]\n\t"
        "fmul st,st(2)\n\t"
        "fstp dword ptr [edx+4]\n\t"
        "fxch st(1)\n\t"
        "fmul dword ptr [ecx+8]\n\t"
        "fstp dword ptr [edx+8]\n\t"
        "add esp,12\n\t"
        "ret\n\t"
        "1:\n\t"
        "fstp st(1)\n\t"
        "mov dword ptr [edx+8],0\n\t"
        "mov dword ptr [edx+4],0\n\t"
        "mov dword ptr [edx],0\n\t"
        "add esp,12\n\t"
        "ret\n\t"
        ".att_syntax prefix\n\t"
    );
}
#else
vec_t VectorNormalize2( const vec3_t v, vec3_t out) {
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = sqrt (length);

	if (length)
	{
		ilength = 1/length;
		out[0] = v[0]*ilength;
		out[1] = v[1]*ilength;
		out[2] = v[2]*ilength;
	} else {
		VectorClear( out );
	}
		
	return length;

}
#endif

void _VectorMA( const vec3_t veca, float scale, const vec3_t vecb, vec3_t vecc) {
	vecc[0] = veca[0] + scale*vecb[0];
	vecc[1] = veca[1] + scale*vecb[1];
	vecc[2] = veca[2] + scale*vecb[2];
}


vec_t _DotProduct( const vec3_t v1, const vec3_t v2 ) {
	return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

void _VectorSubtract( const vec3_t veca, const vec3_t vecb, vec3_t out ) {
	out[0] = veca[0]-vecb[0];
	out[1] = veca[1]-vecb[1];
	out[2] = veca[2]-vecb[2];
}

void _VectorAdd( const vec3_t veca, const vec3_t vecb, vec3_t out ) {
	out[0] = veca[0]+vecb[0];
	out[1] = veca[1]+vecb[1];
	out[2] = veca[2]+vecb[2];
}

void _VectorCopy( const vec3_t in, vec3_t out ) {
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
}

void _VectorScale( const vec3_t in, vec_t scale, vec3_t out ) {
	out[0] = in[0]*scale;
	out[1] = in[1]*scale;
	out[2] = in[2]*scale;
}

#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross ) {
	__asm {
		MOV ECX,dword ptr [ESP + 08h]
		MOV EAX,dword ptr [ESP + 04h]
		MOV EDX,dword ptr [ESP + 0ch]
		FLD dword ptr [ECX + 08h]
		FMUL dword ptr [EAX + 04h]
		FLD dword ptr [EAX + 08h]
		FMUL dword ptr [ECX + 04h]
		fsubp st(1),st(0)
		FSTP dword ptr [EDX]
		FLD dword ptr [EAX + 08h]
		FMUL dword ptr [ECX]
		FLD dword ptr [EAX]
		FMUL dword ptr [ECX + 08h]
		fsubp st(1),st(0)
		FSTP dword ptr [EDX + 04h]
		FLD dword ptr [EAX]
		FMUL dword ptr [ECX + 04h]
		FLD dword ptr [ECX]
		FMUL dword ptr [EAX + 04h]
		fsubp st(1),st(0)
		FSTP dword ptr [EDX + 08h]
		RET
	}
}
#elif defined(__GNUC__) && defined(__i386__)
__attribute__((naked)) void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross ) {
	__asm__ volatile (
		".intel_syntax noprefix\n\t"
		"MOV EAX,dword ptr [ESP + 0x8]\n\t"
		"MOV EDX,dword ptr [ESP + 0x4]\n\t"
		"MOV ECX,dword ptr [ESP + 0xc]\n\t"
		"FLD dword ptr [EAX + 0x8]\n\t"
		"FLD dword ptr [EAX + 0x4]\n\t"
		"FMUL dword ptr [EDX + 0x8]\n\t"
		"fxch st(1)\n\t"
		"FMUL dword ptr [EDX + 0x4]\n\t"
		"fsubrp st(1),st(0)\n\t"
		"FSTP dword ptr [ECX]\n\t"
		"FLD dword ptr [EAX]\n\t"
		"FLD dword ptr [EAX + 0x8]\n\t"
		"FMUL dword ptr [EDX]\n\t"
		"fxch st(1)\n\t"
		"FMUL dword ptr [EDX + 0x8]\n\t"
		"fsubrp st(1),st(0)\n\t"
		"FSTP dword ptr [ECX + 0x4]\n\t"
		"FLD dword ptr [EAX + 0x4]\n\t"
		"FLD dword ptr [EAX]\n\t"
		"fxch st(1)\n\t"
		"FMUL dword ptr [EDX]\n\t"
		"fxch st(1)\n\t"
		"FMUL dword ptr [EDX + 0x4]\n\t"
		"fsubp st(1),st(0)\n\t"
		"FSTP dword ptr [ECX + 0x8]\n\t"
		"RET\n\t"
		".att_syntax prefix\n\t"
	);
}
#else
void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross ) {
	cross[0] = v1[1]*v2[2] - v1[2]*v2[1];
	cross[1] = v1[2]*v2[0] - v1[0]*v2[2];
	cross[2] = v1[0]*v2[1] - v1[1]*v2[0];
}
#endif

#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) vec_t VectorLength( const vec3_t v ) {
	__asm {
		MOV EAX,dword ptr [ESP + 04h]
		FLD dword ptr [EAX + 08h]
		FLD dword ptr [EAX + 04h]
		FLD dword ptr [EAX]
		FLD st(0)
		fmul st(0),st(1)
		FLD st(2)
		fmul st(0),st(3)
		faddp st(1),st(0)
		FLD st(3)
		fmul st(0),st(4)
		faddp st(1),st(0)
		FSQRT
		FSTP st(3)
		FSTP st(0)
		FSTP st(0)
		RET
	}
}
#elif defined(__GNUC__) && defined(__i386__)
__attribute__((naked)) vec_t VectorLength( const vec3_t v ) {
	__asm__ volatile (
		".intel_syntax noprefix\n\t"
		"SUB ESP,0x4\n\t"
		"MOV EDX,dword ptr [ESP + 0x8]\n\t"
		"FLD dword ptr [EDX]\n\t"
		"FLD dword ptr [EDX + 0x4]\n\t"
		"fxch st(1)\n\t"
		"fmul st(0),st(0)\n\t"
		"fxch st(1)\n\t"
		"fmul st(0),st(0)\n\t"
		"faddp st(1),st(0)\n\t"
		"FLD dword ptr [EDX + 0x8]\n\t"
		"fmul st(0),st(0)\n\t"
		"faddp st(1),st(0)\n\t"
		"FSQRT\n\t"
		"FSTP dword ptr [ESP]\n\t"
		"FLD dword ptr [ESP]\n\t"
		"POP EAX\n\t"
		"RET\n\t"
		".att_syntax prefix\n\t"
	);
}
#else
vec_t VectorLength( const vec3_t v ) {
	return sqrt (v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}
#endif

#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) vec_t VectorLengthSquared( const vec3_t v ) {
    __asm {
        mov ecx, [esp+4]
        fld dword ptr [ecx+8]
        fld dword ptr [ecx+4]
        fld dword ptr [ecx]
        fld st(0)
        fmul st(0), st(1)
        fld st(2)
        fmul st(0), st(3)
        faddp st(1), st(0)
        fld st(3)
        fmul st(0), st(4)
        faddp st(1), st(0)
        fstp st(3)
        fstp st(0)
        fstp st(0)
        ret
    }
}
#elif defined(__GNUC__) && defined(__i386__)
__attribute__((naked)) vec_t VectorLengthSquared( const vec3_t v ) {
    __asm__ volatile (
        ".intel_syntax noprefix\n\t"
        "mov edx,[esp+4]\n\t"
        "fld dword ptr [edx]\n\t"
        "fld dword ptr [edx+4]\n\t"
        "fxch st(1)\n\t"
        "fmul st,st\n\t"
        "fxch st(1)\n\t"
        "fmul st,st\n\t"
        "faddp st(1),st\n\t"
        "fld dword ptr [edx+8]\n\t"
        "fmul st,st\n\t"
        "faddp st(1),st\n\t"
        "ret\n\t"
        ".att_syntax prefix\n\t"
    );
}
#else
vec_t VectorLengthSquared( const vec3_t v ) {
	return (v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}
#endif

#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) vec_t Distance( const vec3_t p1, const vec3_t p2 ) {
	__asm {
		SUB ESP,0ch
		MOV EAX,dword ptr [ESP + 014h]
		MOV ECX,dword ptr [ESP + 010h]
		FLD dword ptr [EAX]
		FSUB dword ptr [ECX]
		FSTP dword ptr [ESP]
		FLD dword ptr [EAX + 04h]
		FSUB dword ptr [ECX + 04h]
		FSTP dword ptr [ESP + 04h]
		FLD dword ptr [EAX + 08h]
		FSUB dword ptr [ECX + 08h]
		LEA EAX,[ESP]
		PUSH EAX
		FSTP dword ptr [ESP + 0ch]
		call VectorLength
		ADD ESP,010h
		RET
	}
}
#elif defined(__GNUC__) && defined(__i386__)
__attribute__((naked)) vec_t Distance( const vec3_t p1, const vec3_t p2 ) {
	__asm__ volatile (
		".intel_syntax noprefix\n\t"
		"SUB ESP,0x2c\n\t"
		"MOV EDX,dword ptr [ESP + 0x30]\n\t"
		"MOV dword ptr [ESP + 0x28],EBX\n\t"
		"MOV ECX,dword ptr [ESP + 0x34]\n\t"
		"FLD dword ptr [EDX]\n\t"
		"FSUBR dword ptr [ECX]\n\t"
		"FSTP dword ptr [ESP + 0x10]\n\t"
		"FLD dword ptr [EDX + 0x4]\n\t"
		"FSUBR dword ptr [ECX + 0x4]\n\t"
		"FSTP dword ptr [ESP + 0x14]\n\t"
		"FLD dword ptr [EDX + 0x8]\n\t"
		"LEA EDX,[ESP + 0x10]\n\t"
		"FSUBR dword ptr [ECX + 0x8]\n\t"
		"MOV dword ptr [ESP],EDX\n\t"
		"FSTP dword ptr [ESP + 0x18]\n\t"
		"call VectorLength\n\t"
		"MOV EBX,dword ptr [ESP + 0x28]\n\t"
		"ADD ESP,0x2c\n\t"
		"RET\n\t"
		".att_syntax prefix\n\t"
	);
}
#else
vec_t Distance( const vec3_t p1, const vec3_t p2 ) {
	vec3_t	v;

	VectorSubtract (p2, p1, v);
	return VectorLength( v );
}
#endif

#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) vec_t DistanceSquared( const vec3_t p1, const vec3_t p2 ) {
	__asm {
		MOV EAX,dword ptr [ESP + 08h]
		MOV ECX,dword ptr [ESP + 04h]
		FLD dword ptr [EAX]
		FSUB dword ptr [ECX]
		FLD dword ptr [EAX + 04h]
		FSUB dword ptr [ECX + 04h]
		FLD dword ptr [EAX + 08h]
		FSUB dword ptr [ECX + 08h]
		FLD st(0)
		fmul st(0),st(1)
		FLD st(2)
		fmul st(0),st(3)
		faddp st(1),st(0)
		FLD st(3)
		fmul st(0),st(4)
		faddp st(1),st(0)
		FSTP st(3)
		FSTP st(0)
		FSTP st(0)
		RET
	}
}
#elif defined(__GNUC__) && defined(__i386__)
__attribute__((naked)) vec_t DistanceSquared( const vec3_t p1, const vec3_t p2 ) {
	__asm__ volatile (
		".intel_syntax noprefix\n\t"
		"SUB ESP,0x1c\n\t"
		"MOV EDX,dword ptr [ESP + 0x20]\n\t"
		"MOV ECX,dword ptr [ESP + 0x24]\n\t"
		"FLD dword ptr [EDX]\n\t"
		"FLD dword ptr [EDX + 0x4]\n\t"
		"fxch st(1)\n\t"
		"FSUBR dword ptr [ECX]\n\t"
		"fxch st(1)\n\t"
		"FSUBR dword ptr [ECX + 0x4]\n\t"
		"FLD dword ptr [EDX + 0x8]\n\t"
		"FSUBR dword ptr [ECX + 0x8]\n\t"
		"FXCH st(2)\n\t"
		"fmul st(0),st(0)\n\t"
		"fxch st(1)\n\t"
		"ADD ESP,0x1c\n\t"
		"fmul st(0),st(0)\n\t"
		"FXCH st(2)\n\t"
		"fmul st(0),st(0)\n\t"
		"fxch st(1)\n\t"
		"FADDP st(2),st(0)\n\t"
		"faddp st(1),st(0)\n\t"
		"RET\n\t"
		".att_syntax prefix\n\t"
	);
}
#else
vec_t DistanceSquared( const vec3_t p1, const vec3_t p2 ) {
	vec3_t	v;

	VectorSubtract (p2, p1, v);
	return v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
}
#endif


#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) void VectorInverse( vec3_t v ) {
	__asm {
		MOV EAX,dword ptr [ESP + 04h]
		FLD dword ptr [EAX]
		FCHS
		FSTP dword ptr [EAX]
		FLD dword ptr [EAX + 04h]
		FCHS
		FSTP dword ptr [EAX + 04h]
		FLD dword ptr [EAX + 08h]
		FCHS
		FSTP dword ptr [EAX + 08h]
		RET
	}
}
#elif defined(__GNUC__) && defined(__i386__)
__attribute__((naked)) void VectorInverse( vec3_t v ) {
	__asm__ volatile (
		".intel_syntax noprefix\n\t"
		"MOV EDX,dword ptr [ESP + 0x4]\n\t"
		"XOR byte ptr [EDX + 0x3],0x80\n\t"
		"XOR byte ptr [EDX + 0x7],0x80\n\t"
		"XOR byte ptr [EDX + 0xb],0x80\n\t"
		"RET\n\t"
		".att_syntax prefix\n\t"
	);
}
#else
void VectorInverse( vec3_t v ){
	v[0] = -v[0];
	v[1] = -v[1];
	v[2] = -v[2];
}
#endif

void Vector4Scale( const vec4_t in, vec_t scale, vec4_t out ) {
	out[0] = in[0]*scale;
	out[1] = in[1]*scale;
	out[2] = in[2]*scale;
	out[3] = in[3]*scale;
}


int Q_log2( int val ) {
	int answer;

	answer = 0;
	while ( ( val>>=1 ) != 0 ) {
		answer++;
	}
	return answer;
}



/*
=================
PlaneTypeForNormal
=================
*/
/*
int	PlaneTypeForNormal (vec3_t normal) {
	if ( normal[0] == 1.0 )
		return PLANE_X;
	if ( normal[1] == 1.0 )
		return PLANE_Y;
	if ( normal[2] == 1.0 )
		return PLANE_Z;
	
	return PLANE_NON_AXIAL;
}
*/


/*
================
MatrixMultiply
================
*/
void MatrixMultiply(float in1[3][3], float in2[3][3], float result[3][3]) {
#if defined(_MSC_VER) && defined(_M_IX86)
	/* UI40001910: preserve x87 evaluation, operand loads and per-store rounding. */
	__asm {
		mov eax, in1
		mov ecx, in2
		mov edx, result
		fld dword ptr [eax]
		fmul dword ptr [ecx]
		fld dword ptr [eax + 0x4]
		fmul dword ptr [ecx + 0xc]
		faddp st(1), st(0)
		fld dword ptr [ecx + 0x18]
		fmul dword ptr [eax + 0x8]
		faddp st(1), st(0)
		fstp dword ptr [edx]
		fld dword ptr [eax + 0x4]
		fmul dword ptr [ecx + 0x10]
		fld dword ptr [ecx + 0x1c]
		fmul dword ptr [eax + 0x8]
		faddp st(1), st(0)
		fld dword ptr [ecx + 0x4]
		fmul dword ptr [eax]
		faddp st(1), st(0)
		fstp dword ptr [edx + 0x4]
		fld dword ptr [eax + 0x4]
		fmul dword ptr [ecx + 0x14]
		fld dword ptr [ecx + 0x20]
		fmul dword ptr [eax + 0x8]
		faddp st(1), st(0)
		fld dword ptr [ecx + 0x8]
		fmul dword ptr [eax]
		faddp st(1), st(0)
		fstp dword ptr [edx + 0x8]
		fld dword ptr [eax + 0x10]
		fmul dword ptr [ecx + 0xc]
		fld dword ptr [eax + 0xc]
		fmul dword ptr [ecx]
		faddp st(1), st(0)
		fld dword ptr [eax + 0x14]
		fmul dword ptr [ecx + 0x18]
		faddp st(1), st(0)
		fstp dword ptr [edx + 0xc]
		fld dword ptr [eax + 0x14]
		fmul dword ptr [ecx + 0x1c]
		fld dword ptr [eax + 0x10]
		fmul dword ptr [ecx + 0x10]
		faddp st(1), st(0)
		fld dword ptr [eax + 0xc]
		fmul dword ptr [ecx + 0x4]
		faddp st(1), st(0)
		fstp dword ptr [edx + 0x10]
		fld dword ptr [eax + 0x14]
		fmul dword ptr [ecx + 0x20]
		fld dword ptr [eax + 0x10]
		fmul dword ptr [ecx + 0x14]
		faddp st(1), st(0)
		fld dword ptr [eax + 0xc]
		fmul dword ptr [ecx + 0x8]
		faddp st(1), st(0)
		fstp dword ptr [edx + 0x14]
		fld dword ptr [eax + 0x18]
		fmul dword ptr [ecx]
		fld dword ptr [ecx + 0x18]
		fmul dword ptr [eax + 0x20]
		faddp st(1), st(0)
		fld dword ptr [eax + 0x1c]
		fmul dword ptr [ecx + 0xc]
		faddp st(1), st(0)
		fstp dword ptr [edx + 0x18]
		fld dword ptr [ecx + 0x1c]
		fmul dword ptr [eax + 0x20]
		fld dword ptr [ecx + 0x10]
		fmul dword ptr [eax + 0x1c]
		faddp st(1), st(0)
		fld dword ptr [ecx + 0x4]
		fmul dword ptr [eax + 0x18]
		faddp st(1), st(0)
		fstp dword ptr [edx + 0x1c]
		fld dword ptr [ecx + 0x20]
		fmul dword ptr [eax + 0x20]
		fld dword ptr [ecx + 0x14]
		fmul dword ptr [eax + 0x1c]
		faddp st(1), st(0)
		fld dword ptr [ecx + 0x8]
		fmul dword ptr [eax + 0x18]
		faddp st(1), st(0)
		fstp dword ptr [edx + 0x20]
	}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
	/* Linux UI00055b22 has a different evaluation schedule. */
	__asm__ __volatile__(
		"flds (%%eax)\n\t"
		"flds 0xc(%%eax)\n\t"
		"fmuls 0x4(%%edx)\n\t"
		"fxch %%st(1)\n\t"
		"fmuls (%%edx)\n\t"
		"faddp %%st, %%st(1)\n\t"
		"flds 0x18(%%eax)\n\t"
		"fmuls 0x8(%%edx)\n\t"
		"faddp %%st, %%st(1)\n\t"
		"fstps (%%ecx)\n\t"
		"flds 0x4(%%eax)\n\t"
		"flds 0x10(%%eax)\n\t"
		"fmuls 0x4(%%edx)\n\t"
		"fxch %%st(1)\n\t"
		"fmuls (%%edx)\n\t"
		"faddp %%st, %%st(1)\n\t"
		"flds 0x1c(%%eax)\n\t"
		"fmuls 0x8(%%edx)\n\t"
		"faddp %%st, %%st(1)\n\t"
		"fstps 0x4(%%ecx)\n\t"
		"flds 0x8(%%eax)\n\t"
		"flds 0x14(%%eax)\n\t"
		"fmuls 0x4(%%edx)\n\t"
		"fxch %%st(1)\n\t"
		"fmuls (%%edx)\n\t"
		"faddp %%st, %%st(1)\n\t"
		"flds 0x20(%%eax)\n\t"
		"fmuls 0x8(%%edx)\n\t"
		"faddp %%st, %%st(1)\n\t"
		"fstps 0x8(%%ecx)\n\t"
		"flds (%%eax)\n\t"
		"flds 0xc(%%eax)\n\t"
		"fmuls 0x10(%%edx)\n\t"
		"fxch %%st(1)\n\t"
		"fmuls 0xc(%%edx)\n\t"
		"faddp %%st, %%st(1)\n\t"
		"flds 0x18(%%eax)\n\t"
		"fmuls 0x14(%%edx)\n\t"
		"faddp %%st, %%st(1)\n\t"
		"fstps 0xc(%%ecx)\n\t"
		"flds 0x4(%%eax)\n\t"
		"flds 0x10(%%eax)\n\t"
		"fmuls 0x10(%%edx)\n\t"
		"fxch %%st(1)\n\t"
		"fmuls 0xc(%%edx)\n\t"
		"faddp %%st, %%st(1)\n\t"
		"flds 0x1c(%%eax)\n\t"
		"fmuls 0x14(%%edx)\n\t"
		"faddp %%st, %%st(1)\n\t"
		"fstps 0x10(%%ecx)\n\t"
		"flds 0x8(%%eax)\n\t"
		"flds 0x14(%%eax)\n\t"
		"fmuls 0x10(%%edx)\n\t"
		"fxch %%st(1)\n\t"
		"fmuls 0xc(%%edx)\n\t"
		"faddp %%st, %%st(1)\n\t"
		"flds 0x20(%%eax)\n\t"
		"fmuls 0x14(%%edx)\n\t"
		"faddp %%st, %%st(1)\n\t"
		"fstps 0x14(%%ecx)\n\t"
		"flds (%%eax)\n\t"
		"flds 0xc(%%eax)\n\t"
		"fmuls 0x1c(%%edx)\n\t"
		"fxch %%st(1)\n\t"
		"fmuls 0x18(%%edx)\n\t"
		"faddp %%st, %%st(1)\n\t"
		"flds 0x18(%%eax)\n\t"
		"fmuls 0x20(%%edx)\n\t"
		"faddp %%st, %%st(1)\n\t"
		"fstps 0x18(%%ecx)\n\t"
		"flds 0x4(%%eax)\n\t"
		"flds 0x10(%%eax)\n\t"
		"fmuls 0x1c(%%edx)\n\t"
		"fxch %%st(1)\n\t"
		"fmuls 0x18(%%edx)\n\t"
		"faddp %%st, %%st(1)\n\t"
		"flds 0x1c(%%eax)\n\t"
		"fmuls 0x20(%%edx)\n\t"
		"faddp %%st, %%st(1)\n\t"
		"fstps 0x1c(%%ecx)\n\t"
		"flds 0x8(%%eax)\n\t"
		"flds 0x14(%%eax)\n\t"
		"fmuls 0x1c(%%edx)\n\t"
		"fxch %%st(1)\n\t"
		"fmuls 0x18(%%edx)\n\t"
		"faddp %%st, %%st(1)\n\t"
		"flds 0x20(%%eax)\n\t"
		"fmuls 0x20(%%edx)\n\t"
		"faddp %%st, %%st(1)\n\t"
		"fstps 0x20(%%ecx)\n\t"
		:
		: "a" (in2), "d" (in1), "c" (result)
		: "memory", "st", "st(1)", "st(2)"
	);
#else
	/* Portable fallback; exact original x87 parity is limited to the paths above. */
	result[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
	result[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
	result[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
	result[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
	result[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
	result[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
	result[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
	result[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
	result[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
#endif
}


/* TC Windows qagame200a0de0/cgame3007e850/ui40001a00 all use the
 * same float32 radians constant, but do not round its product before sin/cos.
 * The SDK float angle temporary can reverse prone leg geometry at +/-90. */
void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up) {
    const float radians = 0.01745329238474369049072265625f;
    double angle;
    float sr, sp, sy, cr, cp, cy;
    angle = (double)angles[YAW] * (double)radians;
    sy = (float)sin(angle); cy = (float)cos(angle);
    angle = (double)angles[PITCH] * (double)radians;
    sp = (float)sin(angle); cp = (float)cos(angle);
    angle = (double)angles[ROLL] * (double)radians;
    sr = (float)sin(angle); cr = (float)cos(angle);
    if (forward) {
        forward[0] = cp * cy;
        forward[1] = cp * sy;
        forward[2] = -sp;
    }
    if (right) {
        right[0] = (float)((double)cr * sy - (double)sr * sp * cy);
        right[1] = (float)(-((double)cr * cy + (double)sr * sp * sy));
        right[2] = (float)(-(double)sr * cp);
    }
    if (up) {
        up[0] = (float)((double)sr * sy + (double)cr * sp * cy);
        up[1] = (float)((double)cr * sp * sy - (double)sr * cy);
        up[2] = cr * cp;
    }
}

/*
** assumes "src" is normalized
*/
/* TC 747: preserve the original C0-only (including unordered) axis gate,
 * helper-call float stores and x87 return disposal on each original x86 ABI. */
#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) void PerpendicularVector(vec3_t dst, const vec3_t src) {
 __asm {
  tce_747_200a0f30:
  sub esp,0x10
  tce_747_200a0f33:
  push ebx
  tce_747_200a0f34:
  push ebp
  tce_747_200a0f35:
  mov ebp,dword ptr [esp + 0x20]
  tce_747_200a0f39:
  push esi
  tce_747_200a0f3a:
  push edi
  tce_747_200a0f3b:
  xor ebx,ebx
  tce_747_200a0f3d:
  mov dword ptr [esp + 0x10],0x3f800000
  tce_747_200a0f45:
  xor esi,esi
  tce_747_200a0f47:
  mov edi,ebp
  tce_747_200a0f49:
  mov eax,dword ptr [edi]
  tce_747_200a0f4b:
  push eax
  tce_747_200a0f4c:
  call Q_fabs
  tce_747_200a0f51:
  fcomp dword ptr [esp + 0x14]
  tce_747_200a0f55:
  add esp,0x4
  tce_747_200a0f58:
  fnstsw ax
  tce_747_200a0f5a:
  test ah,0x1
  tce_747_200a0f5d:
  jz tce_747_200a0f70
  tce_747_200a0f5f:
  mov ecx,dword ptr [edi]
  tce_747_200a0f61:
  mov ebx,esi
  tce_747_200a0f63:
  push ecx
  tce_747_200a0f64:
  call Q_fabs
  tce_747_200a0f69:
  fstp dword ptr [esp + 0x14]
  tce_747_200a0f6d:
  add esp,0x4
  tce_747_200a0f70:
  inc esi
  tce_747_200a0f71:
  add edi,0x4
  tce_747_200a0f74:
  cmp esi,0x3
  tce_747_200a0f77:
  jl tce_747_200a0f49
  tce_747_200a0f79:
  mov esi,dword ptr [esp + 0x24]
  tce_747_200a0f7d:
  lea edx,[esp + 0x14]
  tce_747_200a0f81:
  push ebp
  tce_747_200a0f82:
  push edx
  tce_747_200a0f83:
  mov dword ptr [esp + 0x24],0x0
  tce_747_200a0f8b:
  mov dword ptr [esp + 0x20],0x0
  tce_747_200a0f93:
  mov dword ptr [esp + 0x1c],0x0
  tce_747_200a0f9b:
  push esi
  tce_747_200a0f9c:
  mov dword ptr [esp + ebx*0x4 + 0x20],0x3f800000
  tce_747_200a0fa4:
  call ProjectPointOnPlane
  tce_747_200a0fa9:
  push esi
  tce_747_200a0faa:
  call VectorNormalize
  tce_747_200a0faf:
  add esp,0x10
  tce_747_200a0fb2:
  fstp st(0)
  tce_747_200a0fb4:
  pop edi
  tce_747_200a0fb5:
  pop esi
  tce_747_200a0fb6:
  pop ebp
  tce_747_200a0fb7:
  pop ebx
  tce_747_200a0fb8:
  add esp,0x10
  tce_747_200a0fbb:
  ret
 }
}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
__attribute__((naked)) void PerpendicularVector(vec3_t dst, const vec3_t src) {
 __asm__(
 ".intel_syntax noprefix\n"
 ".Ltce_747_00114cac:\n"
 "push ebp\n"
 ".Ltce_747_00114cad:\n"
 "xor ebp,ebp\n"
 ".Ltce_747_00114caf:\n"
 "push edi\n"
 ".Ltce_747_00114cb0:\n"
 "push esi\n"
 ".Ltce_747_00114cb1:\n"
 "xor esi,esi\n"
 ".Ltce_747_00114cb3:\n"
 "push ebx\n"
 ".Ltce_747_00114cb4:\n"
 "sub esp,0x3c\n"
 ".Ltce_747_00114cb7:\n"
 "mov edi,dword ptr [esp + 0x54]\n"
 ".Ltce_747_00114cc6:\n"
 "mov dword ptr [esp+0x30],0x3f800000\n"
 "fld dword ptr [esp+0x30]\n"
 ".Ltce_747_00114ccc:\n"
 "fstp dword ptr [esp + 0x1c]\n"
 ".Ltce_747_00114cd0:\n"
 "mov edx,dword ptr [edi + esi*0x4]\n"
 ".Ltce_747_00114cd3:\n"
 "mov dword ptr [esp],edx\n"
 ".Ltce_747_00114cd6:\n"
 "call Q_fabs\n"
 ".Ltce_747_00114cdb:\n"
 "fcomp dword ptr [esp + 0x1c]\n"
 ".Ltce_747_00114cdf:\n"
 "fnstsw ax\n"
 ".Ltce_747_00114ce1:\n"
 "sahf\n"
 ".Ltce_747_00114ce2:\n"
 "jc .Ltce_747_00114d34\n"
 ".Ltce_747_00114ce4:\n"
 "inc esi\n"
 ".Ltce_747_00114ce5:\n"
 "cmp esi,0x2\n"
 ".Ltce_747_00114ce8:\n"
 "jle .Ltce_747_00114cd0\n"
 ".Ltce_747_00114cea:\n"
 "mov ecx,0\n"
 ".Ltce_747_00114cf0:\n"
 "mov edx,0x3f800000\n"
 ".Ltce_747_00114cf6:\n"
 "mov dword ptr [esp + 0x28],ecx\n"
 ".Ltce_747_00114cfa:\n"
 "mov dword ptr [esp + 0x24],ecx\n"
 ".Ltce_747_00114cfe:\n"
 "mov dword ptr [esp + 0x20],ecx\n"
 ".Ltce_747_00114d02:\n"
 "mov dword ptr [esp + ebp*0x4 + 0x20],edx\n"
 ".Ltce_747_00114d06:\n"
 "mov esi,dword ptr [esp + 0x50]\n"
 ".Ltce_747_00114d0a:\n"
 "mov dword ptr [esp + 0x8],edi\n"
 ".Ltce_747_00114d0e:\n"
 "lea edi,[esp + 0x20]\n"
 ".Ltce_747_00114d12:\n"
 "mov dword ptr [esp + 0x4],edi\n"
 ".Ltce_747_00114d16:\n"
 "mov dword ptr [esp],esi\n"
 ".Ltce_747_00114d19:\n"
 "call ProjectPointOnPlane\n"
 ".Ltce_747_00114d1e:\n"
 "mov ecx,dword ptr [esp + 0x50]\n"
 ".Ltce_747_00114d22:\n"
 "mov dword ptr [esp],ecx\n"
 ".Ltce_747_00114d25:\n"
 "call VectorNormalize\n"
 ".Ltce_747_00114d2a:\n"
 "fstp st(0)\n"
 ".Ltce_747_00114d2c:\n"
 "add esp,0x3c\n"
 ".Ltce_747_00114d2f:\n"
 "pop ebx\n"
 ".Ltce_747_00114d30:\n"
 "pop esi\n"
 ".Ltce_747_00114d31:\n"
 "pop edi\n"
 ".Ltce_747_00114d32:\n"
 "pop ebp\n"
 ".Ltce_747_00114d33:\n"
 "ret\n"
 ".Ltce_747_00114d34:\n"
 "mov edx,dword ptr [edi + esi*0x4]\n"
 ".Ltce_747_00114d37:\n"
 "mov ebp,esi\n"
 ".Ltce_747_00114d39:\n"
 "mov dword ptr [esp],edx\n"
 ".Ltce_747_00114d3c:\n"
 "call Q_fabs\n"
 ".Ltce_747_00114d41:\n"
 "fstp dword ptr [esp + 0x1c]\n"
 ".Ltce_747_00114d45:\n"
 "jmp .Ltce_747_00114ce4\n"
 ".att_syntax prefix\n"
 );
}
#else
void PerpendicularVector( vec3_t dst, const vec3_t src )
{
	int	pos;
	int i;
	float minelem = 1.0F;
	vec3_t tempvec;

	/*
	** find the smallest magnitude axially aligned vector
	*/
	for ( pos = 0, i = 0; i < 3; i++ )
	{
		if ( Q_fabs( src[i] ) < minelem )
		{
			pos = i;
			minelem = Q_fabs( src[i] );
		}
	}
	tempvec[0] = tempvec[1] = tempvec[2] = 0.0F;
	tempvec[pos] = 1.0F;

	/*
	** project the point onto the plane defined by src
	*/
	ProjectPointOnPlane( dst, tempvec, src );

	/*
	** normalize the result
	*/
	VectorNormalize( dst );
}
#endif

// Ridah
/*
=================
GetPerpendicularViewVector

  Used to find an "up" vector for drawing a sprite so that it always faces the view as best as possible
=================
*/
#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) void GetPerpendicularViewVector( const vec3_t point, const vec3_t p1, const vec3_t p2, vec3_t up ) {
    __asm {
        sub esp,0x18
        mov eax,dword ptr [esp + 0x20]
        push esi
        mov esi,dword ptr [esp + 0x20]
        fld dword ptr [esi]
        fsub dword ptr [eax]
        fstp dword ptr [esp + 0x10]
        fld dword ptr [esi + 0x4]
        fsub dword ptr [eax + 0x4]
        fstp dword ptr [esp + 0x14]
        fld dword ptr [esi + 0x8]
        fsub dword ptr [eax + 0x8]
        lea eax,[esp + 0x10]
        push eax
        fstp dword ptr [esp + 0x1c]
        call VectorNormalize
        mov eax,dword ptr [esp + 0x2c]
        lea ecx,[esp + 0x8]
        fstp st(0)
        fld dword ptr [esi]
        fsub dword ptr [eax]
        push ecx
        fstp dword ptr [esp + 0xc]
        fld dword ptr [esi + 0x4]
        fsub dword ptr [eax + 0x4]
        fstp dword ptr [esp + 0x10]
        fld dword ptr [esi + 0x8]
        fsub dword ptr [eax + 0x8]
        fstp dword ptr [esp + 0x14]
        call VectorNormalize
        mov esi,dword ptr [esp + 0x34]
        lea edx,[esp + 0xc]
        push esi
        lea eax,[esp + 0x1c]
        push edx
        push eax
        fstp st(0)
        call CrossProduct
        push esi
        call VectorNormalize
        add esp,0x18
        fstp st(0)
        pop esi
        add esp,0x18
        ret
    }
}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
__attribute__((naked)) void GetPerpendicularViewVector( const vec3_t point, const vec3_t p1, const vec3_t p2, vec3_t up ) {
    __asm__(
        ".intel_syntax noprefix\n"
        "push ebp\n"
        "push edi\n"
        "push esi\n"
        "push ebx\n"
        "sub esp,0x3c\n"
        "mov ecx,dword ptr [esp + 0x54]\n"
        "mov esi,dword ptr [esp + 0x50]\n"
        "lea ebp,[esp + 0x20]\n"
        "mov edi,dword ptr [esp + 0x58]\n"
        "fld dword ptr [ecx]\n"
        "fsubr dword ptr [esi]\n"
        "fstp dword ptr [esp + 0x20]\n"
        "fld dword ptr [ecx + 0x4]\n"
        "fsubr dword ptr [esi + 0x4]\n"
        "fstp dword ptr [esp + 0x24]\n"
        "fld dword ptr [ecx + 0x8]\n"
        "fsubr dword ptr [esi + 0x8]\n"
        "mov dword ptr [esp],ebp\n"
        "fstp dword ptr [esp + 0x28]\n"
        "call VectorNormalize\n"
        "fstp st(0)\n"
        "fld dword ptr [edi]\n"
        "fsubr dword ptr [esi]\n"
        "fstp dword ptr [esp + 0x10]\n"
        "fld dword ptr [edi + 0x4]\n"
        "fsubr dword ptr [esi + 0x4]\n"
        "fstp dword ptr [esp + 0x14]\n"
        "fld dword ptr [edi + 0x8]\n"
        "lea edi,[esp + 0x10]\n"
        "fsubr dword ptr [esi + 0x8]\n"
        "mov dword ptr [esp],edi\n"
        "fstp dword ptr [esp + 0x18]\n"
        "call VectorNormalize\n"
        "fstp st(0)\n"
        "mov dword ptr [esp + 0x4],edi\n"
        "mov edx,dword ptr [esp + 0x5c]\n"
        "mov dword ptr [esp],ebp\n"
        "mov dword ptr [esp + 0x8],edx\n"
        "call CrossProduct\n"
        "mov eax,dword ptr [esp + 0x5c]\n"
        "mov dword ptr [esp],eax\n"
        "call VectorNormalize\n"
        "fstp st(0)\n"
        "add esp,0x3c\n"
        "pop ebx\n"
        "pop esi\n"
        "pop edi\n"
        "pop ebp\n"
        "ret\n"
        ".att_syntax prefix\n"
    );
}
#else
void GetPerpendicularViewVector( const vec3_t point, const vec3_t p1, const vec3_t p2, vec3_t up )
{
	vec3_t	v1, v2;

	VectorSubtract( point, p1, v1 );
	VectorNormalize( v1 );

	VectorSubtract( point, p2, v2 );
	VectorNormalize( v2 );

	CrossProduct( v1, v2, up );
	VectorNormalize( up );
}
#endif

/*
================
ProjectPointOntoVector
================
*/
/* TC projection: original platform-specific dot accumulation stays in x87. */
#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) void ProjectPointOntoVector(vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj) {
    __asm {
        sub esp,0x18
        mov eax,dword ptr [esp + 0x1c]
        push esi
        mov esi,dword ptr [esp + 0x24]
        fld dword ptr [eax]
        fsub dword ptr [esi]
        fstp dword ptr [esp + 0x10]
        fld dword ptr [eax + 0x4]
        fsub dword ptr [esi + 0x4]
        fstp dword ptr [esp + 0x14]
        fld dword ptr [eax + 0x8]
        fsub dword ptr [esi + 0x8]
        mov eax,dword ptr [esp + 0x28]
        fstp dword ptr [esp + 0x18]
        fld dword ptr [eax]
        fsub dword ptr [esi]
        fstp dword ptr [esp + 0x4]
        fld dword ptr [eax + 0x4]
        fsub dword ptr [esi + 0x4]
        fstp dword ptr [esp + 0x8]
        fld dword ptr [eax + 0x8]
        fsub dword ptr [esi + 0x8]
        lea eax,[esp + 0x4]
        push eax
        fstp dword ptr [esp + 0x10]
        call VectorNormalize
        fstp st(0)
        fld dword ptr [esp + 0x10]
        fmul dword ptr [esp + 0x1c]
        fld dword ptr [esp + 0xc]
        fmul dword ptr [esp + 0x18]
        mov eax,dword ptr [esp + 0x30]
        add esp,0x4
        faddp st(1),st(0)
        fld dword ptr [esp + 0x4]
        fmul dword ptr [esp + 0x10]
        faddp st(1),st(0)
        fld st(0)
        fmul dword ptr [esp + 0x4]
        fadd dword ptr [esi]
        fstp dword ptr [eax]
        fld st(0)
        fmul dword ptr [esp + 0x8]
        fadd dword ptr [esi + 0x4]
        fstp dword ptr [eax + 0x4]
        fmul dword ptr [esp + 0xc]
        fadd dword ptr [esi + 0x8]
        pop esi
        fstp dword ptr [eax + 0x8]
        add esp,0x18
        ret
    }
}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
__attribute__((naked)) void ProjectPointOntoVector(vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj) {
    __asm__(
        ".intel_syntax noprefix\n"
        "push edi\n"
        "push esi\n"
        "push ebx\n"
        "sub esp,0x30\n"
        "mov esi,dword ptr [esp + 0x44]\n"
        "mov edx,dword ptr [esp + 0x40]\n"
        "mov ecx,dword ptr [esp + 0x48]\n"
        "mov edi,dword ptr [esp + 0x4c]\n"
        "fld dword ptr [esi]\n"
        "fld dword ptr [edx]\n"
        "fsub st(0),st(1)\n"
        "fstp dword ptr [esp + 0x20]\n"
        "fld dword ptr [esi + 0x4]\n"
        "fld dword ptr [edx + 0x4]\n"
        "fsub st(0),st(1)\n"
        "fstp dword ptr [esp + 0x24]\n"
        "fld dword ptr [esi + 0x8]\n"
        "fld dword ptr [edx + 0x8]\n"
        "lea edx,[esp + 0x10]\n"
        "fsub st(0),st(1)\n"
        "fstp dword ptr [esp + 0x28]\n"
        "fxch st(2)\n"
        "fsubr dword ptr [ecx]\n"
        "fstp dword ptr [esp + 0x10]\n"
        "fsubr dword ptr [ecx + 0x4]\n"
        "fstp dword ptr [esp + 0x14]\n"
        "fsubr dword ptr [ecx + 0x8]\n"
        "mov dword ptr [esp],edx\n"
        "fstp dword ptr [esp + 0x18]\n"
        "call VectorNormalize\n"
        "fstp st(0)\n"
        "fld dword ptr [esp + 0x10]\n"
        "fld dword ptr [esp + 0x14]\n"
        "fld dword ptr [esp + 0x20]\n"
        "fld dword ptr [esp + 0x24]\n"
        "fld dword ptr [esp + 0x18]\n"
        "fxch st(2)\n"
        "fmul st(0),st(4)\n"
        "fxch st(1)\n"
        "fmul st(0),st(3)\n"
        "faddp st(1),st(0)\n"
        "fld dword ptr [esp + 0x28]\n"
        "fmul st(0),st(2)\n"
        "faddp st(1),st(0)\n"
        "fmul st(3),st(0)\n"
        "fmul st(2),st(0)\n"
        "fmulp st(1),st(0)\n"
        "fxch st(2)\n"
        "fadd dword ptr [esi]\n"
        "fstp dword ptr [edi]\n"
        "fadd dword ptr [esi + 0x4]\n"
        "fstp dword ptr [edi + 0x4]\n"
        "fadd dword ptr [esi + 0x8]\n"
        "fstp dword ptr [edi + 0x8]\n"
        "add esp,0x30\n"
        "pop ebx\n"
        "pop esi\n"
        "pop edi\n"
        "ret\n"
        ".att_syntax prefix\n"
    );
}
#else
void ProjectPointOntoVector( vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj )
{
	vec3_t pVec, vec;

	VectorSubtract( point, vStart, pVec );
	VectorSubtract( vEnd, vStart, vec );
	VectorNormalize( vec );
	// project onto the directional vector for this segment
	VectorMA( vStart, DotProduct( pVec, vec ), vec, vProj );
}
#endif

/*
================
ProjectPointOntoVectorBounded
================
*/
void ProjectPointOntoVectorBounded( vec3_t point, vec3_t vStart, vec3_t vEnd, vec3_t vProj )
{
	vec3_t pVec, vec;
	int j;

	VectorSubtract( point, vStart, pVec );
	VectorSubtract( vEnd, vStart, vec );
	VectorNormalize( vec );
	// project onto the directional vector for this segment
	VectorMA( vStart, DotProduct( pVec, vec ), vec, vProj );
	// check bounds
	for (j = 0; j < 3; j++) 
		if ((vProj[j] > vStart[j] && vProj[j] > vEnd[j]) ||
			(vProj[j] < vStart[j] && vProj[j] < vEnd[j]))
			break;
	if (j < 3) {
		if (Q_fabs(vProj[j] - vStart[j]) < Q_fabs(vProj[j] - vEnd[j]))
			VectorCopy(vStart, vProj);
		else
			VectorCopy(vEnd, vProj);
	}
}

/*
================
DistanceFromLineSquared
================
*/
/* Original x86 branch/staging contract: line_744.md. */
#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) float DistanceFromLineSquared(vec3_t p, vec3_t lp1, vec3_t lp2) {
    __asm {
        tce_744_200a1060:
        sub esp,0x18
        tce_744_200a1063:
        mov ecx,dword ptr [esp + 0x1c]
        tce_744_200a1067:
        lea eax,[esp + 0xc]
        tce_744_200a106b:
        push ebx
        tce_744_200a106c:
        mov ebx,dword ptr [esp + 0x28]
        tce_744_200a1070:
        push ebp
        tce_744_200a1071:
        mov ebp,dword ptr [esp + 0x28]
        tce_744_200a1075:
        push esi
        tce_744_200a1076:
        push edi
        tce_744_200a1077:
        push eax
        tce_744_200a1078:
        push ebx
        tce_744_200a1079:
        push ebp
        tce_744_200a107a:
        push ecx
        tce_744_200a107b:
        call ProjectPointOntoVector
        tce_744_200a1080:
        mov edx,ebp
        tce_744_200a1082:
        lea eax,[esp + 0x2c]
        tce_744_200a1086:
        sub edx,eax
        tce_744_200a1088:
        mov edi,ebx
        tce_744_200a108a:
        lea eax,[esp + 0x2c]
        tce_744_200a108e:
        add esp,0x10
        tce_744_200a1091:
        xor esi,esi
        tce_744_200a1093:
        lea ecx,[esp + 0x1c]
        tce_744_200a1097:
        sub edi,eax
        tce_744_200a1099:
        fld dword ptr [ecx]
        tce_744_200a109b:
        fcomp dword ptr [edx + ecx*0x1]
        tce_744_200a109e:
        fnstsw ax
        tce_744_200a10a0:
        test ah,0x41
        tce_744_200a10a3:
        jnz tce_744_200a10b1
        tce_744_200a10a5:
        fld dword ptr [ecx]
        tce_744_200a10a7:
        fcomp dword ptr [edi + ecx*0x1]
        tce_744_200a10aa:
        fnstsw ax
        tce_744_200a10ac:
        test ah,0x41
        tce_744_200a10af:
        jz tce_744_200a110b
        tce_744_200a10b1:
        fld dword ptr [ecx]
        tce_744_200a10b3:
        fcomp dword ptr [edx + ecx*0x1]
        tce_744_200a10b6:
        fnstsw ax
        tce_744_200a10b8:
        test ah,0x1
        tce_744_200a10bb:
        jz tce_744_200a10c9
        tce_744_200a10bd:
        fld dword ptr [ecx]
        tce_744_200a10bf:
        fcomp dword ptr [edi + ecx*0x1]
        tce_744_200a10c2:
        fnstsw ax
        tce_744_200a10c4:
        test ah,0x1
        tce_744_200a10c7:
        jnz tce_744_200a110b
        tce_744_200a10c9:
        inc esi
        tce_744_200a10ca:
        add ecx,0x4
        tce_744_200a10cd:
        cmp esi,0x3
        tce_744_200a10d0:
        jl tce_744_200a1099
        tce_744_200a10d2:
        mov eax,dword ptr [esp + 0x2c]
        tce_744_200a10d6:
        lea edx,[esp + 0x10]
        tce_744_200a10da:
        push edx
        tce_744_200a10db:
        fld dword ptr [eax]
        tce_744_200a10dd:
        fsub dword ptr [esp + 0x20]
        tce_744_200a10e1:
        fstp dword ptr [esp + 0x14]
        tce_744_200a10e5:
        fld dword ptr [eax + 0x4]
        tce_744_200a10e8:
        fsub dword ptr [esp + 0x24]
        tce_744_200a10ec:
        fstp dword ptr [esp + 0x18]
        tce_744_200a10f0:
        fld dword ptr [eax + 0x8]
        tce_744_200a10f3:
        fsub dword ptr [esp + 0x28]
        tce_744_200a10f7:
        fstp dword ptr [esp + 0x1c]
        tce_744_200a10fb:
        call VectorLengthSquared
        tce_744_200a1100:
        add esp,0x4
        tce_744_200a1103:
        pop edi
        tce_744_200a1104:
        pop esi
        tce_744_200a1105:
        pop ebp
        tce_744_200a1106:
        pop ebx
        tce_744_200a1107:
        add esp,0x18
        tce_744_200a110a:
        ret
        tce_744_200a110b:
        cmp esi,0x3
        tce_744_200a110e:
        jge tce_744_200a10d2
        tce_744_200a1110:
        fld dword ptr [esp + esi*0x4 + 0x1c]
        tce_744_200a1114:
        fsub dword ptr [ebp + esi*0x4]
        tce_744_200a1118:
        push ecx
        tce_744_200a1119:
        fstp dword ptr [esp]
        tce_744_200a111c:
        call Q_fabs
        tce_744_200a1121:
        fstp dword ptr [esp + 0x38]
        tce_744_200a1125:
        fld dword ptr [esp + esi*0x4 + 0x20]
        tce_744_200a1129:
        fsub dword ptr [ebx + esi*0x4]
        tce_744_200a112c:
        fstp dword ptr [esp]
        tce_744_200a112f:
        call Q_fabs
        tce_744_200a1134:
        fcomp dword ptr [esp + 0x38]
        tce_744_200a1138:
        add esp,0x4
        tce_744_200a113b:
        fnstsw ax
        tce_744_200a113d:
        test ah,0x41
        tce_744_200a1140:
        mov eax,dword ptr [esp + 0x2c]
        tce_744_200a1144:
        fld dword ptr [eax]
        tce_744_200a1146:
        jnz tce_744_200a1178
        tce_744_200a1148:
        fsub dword ptr [ebp]
        tce_744_200a114b:
        lea ecx,[esp + 0x10]
        tce_744_200a114f:
        push ecx
        tce_744_200a1150:
        fstp dword ptr [esp + 0x14]
        tce_744_200a1154:
        fld dword ptr [eax + 0x4]
        tce_744_200a1157:
        fsub dword ptr [ebp + 0x4]
        tce_744_200a115a:
        fstp dword ptr [esp + 0x18]
        tce_744_200a115e:
        fld dword ptr [eax + 0x8]
        tce_744_200a1161:
        fsub dword ptr [ebp + 0x8]
        tce_744_200a1164:
        fstp dword ptr [esp + 0x1c]
        tce_744_200a1168:
        call VectorLengthSquared
        tce_744_200a116d:
        add esp,0x4
        tce_744_200a1170:
        pop edi
        tce_744_200a1171:
        pop esi
        tce_744_200a1172:
        pop ebp
        tce_744_200a1173:
        pop ebx
        tce_744_200a1174:
        add esp,0x18
        tce_744_200a1177:
        ret
        tce_744_200a1178:
        fsub dword ptr [ebx]
        tce_744_200a117a:
        lea ecx,[esp + 0x10]
        tce_744_200a117e:
        push ecx
        tce_744_200a117f:
        fstp dword ptr [esp + 0x14]
        tce_744_200a1183:
        fld dword ptr [eax + 0x4]
        tce_744_200a1186:
        fsub dword ptr [ebx + 0x4]
        tce_744_200a1189:
        fstp dword ptr [esp + 0x18]
        tce_744_200a118d:
        fld dword ptr [eax + 0x8]
        tce_744_200a1190:
        fsub dword ptr [ebx + 0x8]
        tce_744_200a1193:
        fstp dword ptr [esp + 0x1c]
        tce_744_200a1197:
        call VectorLengthSquared
        tce_744_200a119c:
        add esp,0x4
        tce_744_200a119f:
        pop edi
        tce_744_200a11a0:
        pop esi
        tce_744_200a11a1:
        pop ebp
        tce_744_200a11a2:
        pop ebx
        tce_744_200a11a3:
        add esp,0x18
        tce_744_200a11a6:
        ret
    }
}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
__attribute__((naked)) float DistanceFromLineSquared(vec3_t p, vec3_t lp1, vec3_t lp2) {
    __asm__(
        ".intel_syntax noprefix\n"
        ".Ltce_744_00114fe4:\n"
        "push ebp\n"
        ".Ltce_744_00114fe5:\n"
        "push edi\n"
        ".Ltce_744_00114fe6:\n"
        "push esi\n"
        ".Ltce_744_00114fe7:\n"
        "xor esi,esi\n"
        ".Ltce_744_00114fe9:\n"
        "push ebx\n"
        ".Ltce_744_00114fea:\n"
        "sub esp,0x4c\n"
        ".Ltce_744_00114fed:\n"
        "lea edx,[esp + 0x30]\n"
        ".Ltce_744_00114ff1:\n"
        "mov dword ptr [esp + 0xc],edx\n"
        ".Ltce_744_00114ff5:\n"
        "mov ebp,dword ptr [esp + 0x60]\n"
        ".Ltce_744_00115004:\n"
        "mov edi,dword ptr [esp + 0x68]\n"
        ".Ltce_744_00115008:\n"
        "mov edx,dword ptr [esp + 0x64]\n"
        ".Ltce_744_0011500c:\n"
        "mov dword ptr [esp],ebp\n"
        ".Ltce_744_0011500f:\n"
        "mov dword ptr [esp + 0x8],edi\n"
        ".Ltce_744_00115013:\n"
        "mov dword ptr [esp + 0x4],edx\n"
        ".Ltce_744_00115017:\n"
        "call ProjectPointOntoVector\n"
        ".Ltce_744_0011501c:\n"
        "fld dword ptr [esp + esi*0x4 + 0x30]\n"
        ".Ltce_744_00115020:\n"
        "mov ecx,dword ptr [esp + 0x64]\n"
        ".Ltce_744_00115024:\n"
        "fld dword ptr [ecx + esi*0x4]\n"
        ".Ltce_744_00115027:\n"
        "fcom st(1)\n"
        ".Ltce_744_00115029:\n"
        "fnstsw ax\n"
        ".Ltce_744_0011502b:\n"
        "sahf\n"
        ".Ltce_744_0011502c:\n"
        "jnc .Ltce_744_0011503e\n"
        ".Ltce_744_0011502e:\n"
        "fxch st(1)\n"
        ".Ltce_744_00115030:\n"
        "fcom dword ptr [edi + esi*0x4]\n"
        ".Ltce_744_00115033:\n"
        "fnstsw ax\n"
        ".Ltce_744_00115035:\n"
        "sahf\n"
        ".Ltce_744_00115036:\n"
        "ja .Ltce_744_00115101\n"
        ".Ltce_744_0011503c:\n"
        "fxch st(1)\n"
        ".Ltce_744_0011503e:\n"
        "fcomp st(1)\n"
        ".Ltce_744_00115040:\n"
        "fnstsw ax\n"
        ".Ltce_744_00115042:\n"
        "sahf\n"
        ".Ltce_744_00115043:\n"
        "jbe .Ltce_744_001150fa\n"
        ".Ltce_744_00115049:\n"
        "fcomp dword ptr [edi + esi*0x4]\n"
        ".Ltce_744_0011504c:\n"
        "fnstsw ax\n"
        ".Ltce_744_0011504e:\n"
        "sahf\n"
        ".Ltce_744_0011504f:\n"
        "jc .Ltce_744_0011508c\n"
        ".Ltce_744_00115051:\n"
        "inc esi\n"
        ".Ltce_744_00115052:\n"
        "cmp esi,0x2\n"
        ".Ltce_744_00115055:\n"
        "jle .Ltce_744_0011501c\n"
        ".Ltce_744_00115057:\n"
        "fld dword ptr [esp + 0x30]\n"
        ".Ltce_744_0011505b:\n"
        "fsubr dword ptr [ebp]\n"
        ".Ltce_744_0011505e:\n"
        "fstp dword ptr [esp + 0x20]\n"
        ".Ltce_744_00115062:\n"
        "fld dword ptr [esp + 0x34]\n"
        ".Ltce_744_00115066:\n"
        "fsubr dword ptr [ebp + 0x4]\n"
        ".Ltce_744_00115069:\n"
        "fstp dword ptr [esp + 0x24]\n"
        ".Ltce_744_0011506d:\n"
        "fld dword ptr [esp + 0x38]\n"
        ".Ltce_744_00115071:\n"
        "fsubr dword ptr [ebp + 0x8]\n"
        ".Ltce_744_00115074:\n"
        "lea edi,[esp + 0x20]\n"
        ".Ltce_744_00115078:\n"
        "mov dword ptr [esp],edi\n"
        ".Ltce_744_0011507b:\n"
        "fstp dword ptr [esp + 0x28]\n"
        ".Ltce_744_0011507f:\n"
        "call VectorLengthSquared\n"
        ".Ltce_744_00115084:\n"
        "add esp,0x4c\n"
        ".Ltce_744_00115087:\n"
        "pop ebx\n"
        ".Ltce_744_00115088:\n"
        "pop esi\n"
        ".Ltce_744_00115089:\n"
        "pop edi\n"
        ".Ltce_744_0011508a:\n"
        "pop ebp\n"
        ".Ltce_744_0011508b:\n"
        "ret\n"
        ".Ltce_744_0011508c:\n"
        "cmp esi,0x2\n"
        ".Ltce_744_0011508f:\n"
        "jg .Ltce_744_00115057\n"
        ".Ltce_744_00115091:\n"
        "mov edx,dword ptr [esp + 0x64]\n"
        ".Ltce_744_00115095:\n"
        "fld dword ptr [edx + esi*0x4]\n"
        ".Ltce_744_00115098:\n"
        "fsubr dword ptr [esp + esi*0x4 + 0x30]\n"
        ".Ltce_744_0011509c:\n"
        "fstp dword ptr [esp]\n"
        ".Ltce_744_0011509f:\n"
        "call Q_fabs\n"
        ".Ltce_744_001150a4:\n"
        "fstp dword ptr [esp + 0x1c]\n"
        ".Ltce_744_001150a8:\n"
        "fld dword ptr [edi + esi*0x4]\n"
        ".Ltce_744_001150ab:\n"
        "fsubr dword ptr [esp + esi*0x4 + 0x30]\n"
        ".Ltce_744_001150af:\n"
        "fstp dword ptr [esp]\n"
        ".Ltce_744_001150b2:\n"
        "call Q_fabs\n"
        ".Ltce_744_001150b7:\n"
        "fld dword ptr [esp + 0x1c]\n"
        ".Ltce_744_001150bb:\n"
        "fcompp\n"
        ".Ltce_744_001150bd:\n"
        "fnstsw ax\n"
        ".Ltce_744_001150bf:\n"
        "sahf\n"
        ".Ltce_744_001150c0:\n"
        "jnc .Ltce_744_001150de\n"
        ".Ltce_744_001150c2:\n"
        "mov esi,dword ptr [esp + 0x64]\n"
        ".Ltce_744_001150c6:\n"
        "fld dword ptr [esi]\n"
        ".Ltce_744_001150c8:\n"
        "fsubr dword ptr [ebp]\n"
        ".Ltce_744_001150cb:\n"
        "fstp dword ptr [esp + 0x20]\n"
        ".Ltce_744_001150cf:\n"
        "fld dword ptr [esi + 0x4]\n"
        ".Ltce_744_001150d2:\n"
        "fsubr dword ptr [ebp + 0x4]\n"
        ".Ltce_744_001150d5:\n"
        "fstp dword ptr [esp + 0x24]\n"
        ".Ltce_744_001150d9:\n"
        "fld dword ptr [esi + 0x8]\n"
        ".Ltce_744_001150dc:\n"
        "jmp .Ltce_744_00115071\n"
        ".Ltce_744_001150de:\n"
        "fld dword ptr [edi]\n"
        ".Ltce_744_001150e0:\n"
        "fsubr dword ptr [ebp]\n"
        ".Ltce_744_001150e3:\n"
        "fstp dword ptr [esp + 0x20]\n"
        ".Ltce_744_001150e7:\n"
        "fld dword ptr [edi + 0x4]\n"
        ".Ltce_744_001150ea:\n"
        "fsubr dword ptr [ebp + 0x4]\n"
        ".Ltce_744_001150ed:\n"
        "fstp dword ptr [esp + 0x24]\n"
        ".Ltce_744_001150f1:\n"
        "fld dword ptr [edi + 0x8]\n"
        ".Ltce_744_001150f4:\n"
        "jmp .Ltce_744_00115071\n"
        ".Ltce_744_001150fa:\n"
        "fstp st(0)\n"
        ".Ltce_744_001150fc:\n"
        "jmp .Ltce_744_00115051\n"
        ".Ltce_744_00115101:\n"
        "fstp st(0)\n"
        ".Ltce_744_00115103:\n"
        "fstp st(0)\n"
        ".Ltce_744_00115105:\n"
        "jmp .Ltce_744_0011508c\n"
        ".att_syntax prefix\n"
    );
}
#else
float DistanceFromLineSquared(vec3_t p, vec3_t lp1, vec3_t lp2) {
	vec3_t proj, t;
	int j;

	ProjectPointOntoVector(p, lp1, lp2, proj);
	for (j = 0; j < 3; j++) 
		if ((proj[j] > lp1[j] && proj[j] > lp2[j]) ||
			(proj[j] < lp1[j] && proj[j] < lp2[j]))
			break;
	if (j < 3) {
		if (Q_fabs(proj[j] - lp1[j]) < Q_fabs(proj[j] - lp2[j]))
			VectorSubtract(p, lp1, t);
		else
			VectorSubtract(p, lp2, t);
		return VectorLengthSquared(t);
	}
	VectorSubtract(p, proj, t);
	return VectorLengthSquared(t);
}
#endif

/*
================
DistanceFromVectorSquared
================
*/
float DistanceFromVectorSquared(vec3_t p, vec3_t lp1, vec3_t lp2) {
	vec3_t proj, t;

	ProjectPointOntoVector(p, lp1, lp2, proj);
	VectorSubtract(p, proj, t);
	return VectorLengthSquared(t);
}

/* TC vectoyaw: retain original ST0 result; see vectoyaw_740.md. */
#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) float vectoyaw(const vec3_t vec) {
    __asm {
        tce_vy_200a11b0:
            mov ecx,dword ptr [esp + 0x4]
        tce_vy_200a11b4:
            fld dword ptr [ecx + 0x4]
        tce_vy_200a11b7:
            fcomp dword ptr tceVecAnglesZero
        tce_vy_200a11bd:
            fnstsw ax
        tce_vy_200a11bf:
            test ah,0x40
        tce_vy_200a11c2:
            jz tce_vy_200a11da
        tce_vy_200a11c4:
            fld dword ptr [ecx]
        tce_vy_200a11c6:
            fcomp dword ptr tceVecAnglesZero
        tce_vy_200a11cc:
            fnstsw ax
        tce_vy_200a11ce:
            test ah,0x40
        tce_vy_200a11d1:
            jz tce_vy_200a11da
        tce_vy_200a11d3:
            fld dword ptr tceVecAnglesZero
        tce_vy_200a11d9:
            ret
        tce_vy_200a11da:
            fld dword ptr [ecx]
        tce_vy_200a11dc:
            fcomp dword ptr tceVecAnglesZero
        tce_vy_200a11e2:
            fnstsw ax
        tce_vy_200a11e4:
            test ah,0x40
        tce_vy_200a11e7:
            jnz tce_vy_200a120a
        tce_vy_200a11e9:
            fld dword ptr [ecx + 0x4]
        tce_vy_200a11ec:
            fld dword ptr [ecx]
        tce_vy_200a11ee:
            fpatan
        tce_vy_200a11f0:
            fmul qword ptr tceVecAnglesDegrees
        tce_vy_200a11f6:
            fcom dword ptr tceVecAnglesZero
        tce_vy_200a11fc:
            fnstsw ax
        tce_vy_200a11fe:
            test ah,0x1
        tce_vy_200a1201:
            jz tce_vy_200a1209
        tce_vy_200a1203:
            fadd dword ptr tceVecAngles360
        tce_vy_200a1209:
            ret
        tce_vy_200a120a:
            fld dword ptr [ecx + 0x4]
        tce_vy_200a120d:
            fcomp dword ptr tceVecAnglesZero
        tce_vy_200a1213:
            fnstsw ax
        tce_vy_200a1215:
            test ah,0x41
        tce_vy_200a1218:
            jnz tce_vy_200a1221
        tce_vy_200a121a:
            fld dword ptr tceVecAngles90
        tce_vy_200a1220:
            ret
        tce_vy_200a1221:
            fld dword ptr tceVecAngles270
        tce_vy_200a1227:
            ret
    }
}
#elif defined(__GNUC__) && defined(__i386__)
__attribute__((naked)) float vectoyaw(const vec3_t vec) {
    __asm__ volatile (
        ".intel_syntax noprefix\n"
        ".Ltce_vy_0011517a:\n"
        "sub esp,0x2c\n"
        "mov dword ptr [esp+0x14],0\n"
        "mov dword ptr [esp+0x18],0x42b40000\n"
        "mov dword ptr [esp+0x1c],0x43870000\n"
        "mov dword ptr [esp+0x20],0x43b40000\n"
        "mov dword ptr [esp+0x24],0x1a63c1f8\n"
        "mov dword ptr [esp+0x28],0x404ca5dc\n"
        ".Ltce_vy_0011517d:\n"
        "mov edx,dword ptr [esp + 0x30]\n"
        ".Ltce_vy_0011518c:\n"
        "fld dword ptr [esp + 0x14]\n"
        ".Ltce_vy_00115192:\n"
        "fld dword ptr [edx + 0x4]\n"
        ".Ltce_vy_00115195:\n"
        "fcom st(1)\n"
        ".Ltce_vy_00115197:\n"
        "fnstsw ax\n"
        ".Ltce_vy_00115199:\n"
        "sahf\n"
        ".Ltce_vy_0011519a:\n"
        "jnz .Ltce_vy_001151de\n"
        ".Ltce_vy_0011519c:\n"
        "fld dword ptr [edx]\n"
        ".Ltce_vy_0011519e:\n"
        "fld st(2)\n"
        ".Ltce_vy_001151a0:\n"
        "fxch st(1)\n"
        ".Ltce_vy_001151a2:\n"
        "fcom st(3)\n"
        ".Ltce_vy_001151a4:\n"
        "fnstsw ax\n"
        ".Ltce_vy_001151a6:\n"
        "sahf\n"
        ".Ltce_vy_001151a7:\n"
        "jz .Ltce_vy_001151d6\n"
        ".Ltce_vy_001151a9:\n"
        "fstp st(1)\n"
        ".Ltce_vy_001151ab:\n"
        "fpatan\n"
        ".Ltce_vy_001151ad:\n"
        "fstp qword ptr [esp + 0x8]\n"
        ".Ltce_vy_001151b1:\n"
        "fld qword ptr [esp + 0x8]\n"
        ".Ltce_vy_001151b5:\n"
        "fmul qword ptr [esp + 0x24]\n"
        ".Ltce_vy_001151bb:\n"
        "fstp dword ptr [esp + 0x4]\n"
        ".Ltce_vy_001151bf:\n"
        "fld dword ptr [esp + 0x4]\n"
        ".Ltce_vy_001151c3:\n"
        "fcom st(1)\n"
        ".Ltce_vy_001151c5:\n"
        "fnstsw ax\n"
        ".Ltce_vy_001151c7:\n"
        "fstp st(1)\n"
        ".Ltce_vy_001151c9:\n"
        "sahf\n"
        ".Ltce_vy_001151ca:\n"
        "jnc .Ltce_vy_001151d2\n"
        ".Ltce_vy_001151cc:\n"
        "fadd dword ptr [esp + 0x20]\n"
        ".Ltce_vy_001151d2:\n"
        "add esp,0x2c\n"
        ".Ltce_vy_001151d5:\n"
        "ret\n"
        ".Ltce_vy_001151d6:\n"
        "fstp st(0)\n"
        ".Ltce_vy_001151d8:\n"
        "fstp st(1)\n"
        ".Ltce_vy_001151da:\n"
        "fstp st(1)\n"
        ".Ltce_vy_001151dc:\n"
        "jmp .Ltce_vy_001151d2\n"
        ".Ltce_vy_001151de:\n"
        "fld dword ptr [edx]\n"
        ".Ltce_vy_001151e0:\n"
        "fcom st(2)\n"
        ".Ltce_vy_001151e2:\n"
        "fnstsw ax\n"
        ".Ltce_vy_001151e4:\n"
        "sahf\n"
        ".Ltce_vy_001151e5:\n"
        "jnz .Ltce_vy_001151ab\n"
        ".Ltce_vy_001151e7:\n"
        "fstp st(0)\n"
        ".Ltce_vy_001151e9:\n"
        "fcomp st(1)\n"
        ".Ltce_vy_001151eb:\n"
        "fnstsw ax\n"
        ".Ltce_vy_001151ed:\n"
        "sahf\n"
        ".Ltce_vy_001151ee:\n"
        "jbe .Ltce_vy_001151f8\n"
        ".Ltce_vy_001151f0:\n"
        "fld dword ptr [esp + 0x18]\n"
        ".Ltce_vy_001151f6:\n"
        "jmp .Ltce_vy_001151c3\n"
        ".Ltce_vy_001151f8:\n"
        "fld dword ptr [esp + 0x1c]\n"
        ".Ltce_vy_001151fe:\n"
        "jmp .Ltce_vy_001151c3\n"
        ".att_syntax prefix\n"
    );
}
#else
float vectoyaw( const vec3_t vec ) {
	float	yaw;
	
	if (vec[YAW] == 0 && vec[PITCH] == 0) {
		yaw = 0;
	} else {
		if (vec[PITCH]) {
			yaw = ( atan2( vec[YAW], vec[PITCH]) * 180 / M_PI );
		} else if (vec[YAW] > 0) {
			yaw = 90;
		} else {
			yaw = 270;
		}
		if (yaw < 0) {
			yaw += 360;
		}
	}

	return yaw;
}
#endif

/*
=================
AxisToAngles

  Used to convert the MD3 tag axis to MDC tag angles, which are much smaller

  This doesn't have to be fast, since it's only used for conversion in utils, try to avoid
  using this during gameplay
=================
*/
#if defined(_MSC_VER) && defined(_M_IX86)
static const float tceAxis769Zero=0.f,tceAxis769Neg90=-90.f,tceAxis769Pos90=90.f;
__declspec(naked) void AxisToAngles(vec3_t axis[3], vec3_t angles) {
 __asm {
SUB ESP,0x24
PUSH ESI
MOV ESI,dword ptr [ESP + 0x2c]
PUSH EDI
MOV EDI,dword ptr [ESP + 0x34]
PUSH EDI
PUSH ESI
call vectoangles
MOV EAX,dword ptr [ESI + 0xc]
MOV ECX,dword ptr [ESI + 0x10]
FLD dword ptr [EDI + 0x4]
MOV EDX,dword ptr [ESI + 0x14]
ADD ESP,0x4
MOV dword ptr [ESP + 0xc],EAX
LEA EAX,[ESP + 0xc]
FCHS
FSTP dword ptr [ESP]
MOV dword ptr [ESP + 0x10],ECX
PUSH EAX
LEA ECX,[ESP + 0x1c]
lea eax,axisDefault
add eax,24
push eax
PUSH ECX
MOV dword ptr [ESP + 0x20],EDX
call RotatePointAroundVector
FLD dword ptr [EDI]
ADD ESP,0xc
LEA EDX,[ESP + 0x18]
FCHS
FSTP dword ptr [ESP]
PUSH EDX
LEA EAX,[ESP + 0x10]
lea ecx,axisDefault
add ecx,12
push ecx
PUSH EAX
call RotatePointAroundVector
LEA ECX,[ESP + 0x30]
LEA EDX,[ESP + 0x18]
PUSH ECX
PUSH EDX
call vectoangles
MOV EAX,dword ptr [ESP + 0x38]
PUSH EAX
call AngleNormalize180
FLD dword ptr [axisDefault+20]
FMUL dword ptr [ESP + 0x2c]
FLD dword ptr [axisDefault+16]
FMUL dword ptr [ESP + 0x28]
ADD ESP,0x1c
faddp st(1),st(0)
FLD dword ptr [axisDefault+12]
FMUL dword ptr [ESP + 0x8]
faddp st(1),st(0)
FCOMP dword ptr tceAxis769Zero
FNSTSW AX
TEST AH,0x1
JZ tce_axis769_3007ec41
FCOM dword ptr tceAxis769Zero
FNSTSW AX
TEST AH,0x1
JZ tce_axis769_3007ec35
FSUBR dword ptr tceAxis769Neg90
FSUB dword ptr tceAxis769Pos90
FCHS
FSTP dword ptr [EDI + 0x8]
POP EDI
POP ESI
ADD ESP,0x24
RET
tce_axis769_3007ec35:
FSUBR dword ptr tceAxis769Pos90
FADD dword ptr tceAxis769Pos90
tce_axis769_3007ec41:
FCHS
FSTP dword ptr [EDI + 0x8]
POP EDI
POP ESI
ADD ESP,0x24
RET
 }
}
#else
void AxisToAngles( vec3_t axis[3], vec3_t angles ) {
	vec3_t right, roll_angles, tvec;

	// first get the pitch and yaw from the forward vector
	vectoangles( axis[0], angles );

	// now get the roll from the right vector
	VectorCopy( axis[1], right );
	// get the angle difference between the tmpAxis[2] and axis[2] after they have been reverse-rotated
	RotatePointAroundVector( tvec, axisDefault[2], right, -angles[YAW] );
	RotatePointAroundVector( right, axisDefault[1], tvec, -angles[PITCH] );
	// now find the angles, the PITCH is effectively our ROLL
	vectoangles( right, roll_angles );
	roll_angles[PITCH] = AngleNormalize180( roll_angles[PITCH] );
	// if the yaw is more than 90 degrees difference, we should adjust the pitch
	if (DotProduct( right, axisDefault[1] ) < 0) {
		if (roll_angles[PITCH] < 0)
			roll_angles[PITCH] = -90 + (-90 - roll_angles[PITCH]);
		else
			roll_angles[PITCH] =  90 + ( 90 - roll_angles[PITCH]);
	}

	angles[ROLL] = -roll_angles[PITCH];
}
#endif

float VectorDistance(vec3_t v1, vec3_t v2)
{
	vec3_t dir;

	VectorSubtract(v2, v1, dir);
	return VectorLength(dir);
}

/* Original x86 argument staging and retained return: shared_742.md. */
#if defined(_MSC_VER) && defined(_M_IX86)
__declspec(naked) float VectorDistanceSquared(vec3_t v1, vec3_t v2) {
    __asm {
        sub esp,0xc
        mov eax,dword ptr [esp + 0x14]
        mov ecx,dword ptr [esp + 0x10]
        fld dword ptr [eax]
        fsub dword ptr [ecx]
        fstp dword ptr [esp]
        fld dword ptr [eax + 0x4]
        fsub dword ptr [ecx + 0x4]
        fstp dword ptr [esp + 0x4]
        fld dword ptr [eax + 0x8]
        fsub dword ptr [ecx + 0x8]
        lea eax,[esp]
        push eax
        fstp dword ptr [esp + 0xc]
        call VectorLengthSquared
        add esp,0x10
        ret
    }
}
#elif defined(__GNUC__) && defined(__i386__) && defined(__linux__)
__attribute__((naked)) float VectorDistanceSquared(vec3_t v1, vec3_t v2) {
    __asm__(
        ".intel_syntax noprefix\n"
        "sub esp,0x2c\n"
        "mov edx,dword ptr [esp + 0x30]\n"
        "mov dword ptr [esp + 0x28],ebx\n"
        "mov ecx,dword ptr [esp + 0x34]\n"
        "fld dword ptr [edx]\n"
        "fsubr dword ptr [ecx]\n"
        "fstp dword ptr [esp + 0x10]\n"
        "fld dword ptr [edx + 0x4]\n"
        "fsubr dword ptr [ecx + 0x4]\n"
        "fstp dword ptr [esp + 0x14]\n"
        "fld dword ptr [edx + 0x8]\n"
        "lea edx,[esp + 0x10]\n"
        "fsubr dword ptr [ecx + 0x8]\n"
        "mov dword ptr [esp],edx\n"
        "fstp dword ptr [esp + 0x18]\n"
        "call VectorLengthSquared\n"
        "mov ebx,dword ptr [esp + 0x28]\n"
        "add esp,0x2c\n"
        "ret\n"
        ".att_syntax prefix\n"
    );
}
#else
float VectorDistanceSquared(vec3_t v1, vec3_t v2) {
	vec3_t dir;

	VectorSubtract(v2, v1, dir);
	return VectorLengthSquared(dir);
}
#endif
// done.
