// Ridah, cg_trails.c - draws a trail using multiple junction points

#include "cg_local.h"

typedef struct trailJunc_s
{
	struct trailJunc_s *nextGlobal, *prevGlobal;	// next junction in the global list it is in (free or used)
	struct trailJunc_s *nextJunc;					// next junction in the trail
	struct trailJunc_s *nextHead, *prevHead;		// next head junc in the world

	void		*usedby; // rain - zinx's trail fix
	qboolean	inuse, freed;
	int			ownerIndex;
	qhandle_t	shader;

	int		sType;
	int		flags;
	float	sTex;
	vec3_t	pos;
	int		spawnTime, endTime;
	float	alphaStart, alphaEnd;
	vec3_t	colorStart, colorEnd;
	float	widthStart, widthEnd;

	// current settings
	float	alpha;
	float	width;
	vec3_t	color;

} trailJunc_t;

#define MAX_TRAILJUNCS	4096

trailJunc_t trailJuncs[MAX_TRAILJUNCS];
trailJunc_t *freeTrails, *activeTrails;
trailJunc_t	*headTrails;

qboolean initTrails = qfalse;

int numTrailsInuse;

/*
===============
CG_ClearTrails
===============
*/
void CG_ClearTrails (void)
{
	int		i;

	memset( trailJuncs, 0, sizeof(trailJunc_t) * MAX_TRAILJUNCS );

	freeTrails = trailJuncs;
	activeTrails = NULL;
	headTrails = NULL;

	for (i=0 ;i<MAX_TRAILJUNCS ; i++)
	{
		trailJuncs[i].nextGlobal = &trailJuncs[i+1];

		if (i>0)
			trailJuncs[i].prevGlobal = &trailJuncs[i-1];
		else
			trailJuncs[i].prevGlobal = NULL;

		trailJuncs[i].inuse = qfalse;
	}
	trailJuncs[MAX_TRAILJUNCS-1].nextGlobal = NULL;

	initTrails = qtrue;
	numTrailsInuse = 0;
}

/*
===============
CG_SpawnTrailJunc
===============
*/
trailJunc_t *CG_SpawnTrailJunc( trailJunc_t *headJunc )
{
	trailJunc_t	*j;

	if (!freeTrails)
		return NULL;

	if ( cg_paused.integer )
		return NULL;

	// select the first free trail, and remove it from the list
	j = freeTrails;
	freeTrails = j->nextGlobal;
	if (freeTrails)
		freeTrails->prevGlobal = NULL;

	j->nextGlobal = activeTrails;
	if (activeTrails)
		activeTrails->prevGlobal = j;
	activeTrails = j;
	j->prevGlobal = NULL;
	j->inuse = qtrue;
	j->freed = qfalse;

	// if this owner has a headJunc, add us to the start
	if (headJunc) {
		// remove the headJunc from the list of heads
		if (headJunc == headTrails) {
			headTrails = headJunc->nextHead;
			if (headTrails)
				headTrails->prevHead = NULL;
		} else {
			if (headJunc->nextHead)
				headJunc->nextHead->prevHead = headJunc->prevHead;
			if (headJunc->prevHead)
				headJunc->prevHead->nextHead = headJunc->nextHead;
		}
		headJunc->prevHead = NULL;
		headJunc->nextHead = NULL;
	}
	// make us the headTrail
	if (headTrails)
		headTrails->prevHead = j;
	j->nextHead = headTrails;
	j->prevHead = NULL;
	headTrails = j;

	j->nextJunc = headJunc;	// if headJunc is NULL, then we'll just be the end of the list

	numTrailsInuse++;

	// debugging
//	CG_Printf( "NumTrails: %i\n", numTrailsInuse );

	return j;
}


/*
===============
CG_AddTrailJunc

  returns the index of the trail junction created

  Used for generic trails
===============
*/

#if defined(_MSC_VER) && defined(_M_IX86)
enum {
	TrailNative_usedby = offsetof(trailJunc_t, usedby),
	TrailNative_inuse = offsetof(trailJunc_t, inuse),
	TrailNative_shader = offsetof(trailJunc_t, shader),
	TrailNative_sType = offsetof(trailJunc_t, sType),
	TrailNative_flags = offsetof(trailJunc_t, flags),
	TrailNative_sTex = offsetof(trailJunc_t, sTex),
	TrailNative_pos = offsetof(trailJunc_t, pos),
	TrailNative_spawnTime = offsetof(trailJunc_t, spawnTime),
	TrailNative_endTime = offsetof(trailJunc_t, endTime),
	TrailNative_alphaStart = offsetof(trailJunc_t, alphaStart),
	TrailNative_alphaEnd = offsetof(trailJunc_t, alphaEnd),
	TrailNative_colorStart = offsetof(trailJunc_t, colorStart),
	TrailNative_colorEnd = offsetof(trailJunc_t, colorEnd),
	TrailNative_widthStart = offsetof(trailJunc_t, widthStart),
	TrailNative_widthEnd = offsetof(trailJunc_t, widthEnd),
	TrailNativeSize = sizeof(trailJunc_t),
	TrailNativeTime = offsetof(cg_t, time)
};
static const double trailNativeOneD = 1.0, trailNativeZeroD = 0.0, trailNativeMillis = 0.001;
static const float trailNativeOneF = 1.0f, trailNativeZeroF = 0.0f;
/* TC30065c60: native pool layout, original x87 clamps and texture progression. */
__declspec(naked) int CG_AddTrailJunc(int headJuncIndex, void *usedby, qhandle_t shader, int spawnTime, int sType, vec3_t pos, int trailLife, float alphaStart, float alphaEnd, float startWidth, float endWidth, int flags, vec3_t colorStart, vec3_t colorEnd, float sRatio, float animSpeed)
{
	__asm {
		MOV EAX,dword ptr [ESP + 04h]
		PUSH EBX
		PUSH ESI
		PUSH EDI
		TEST EAX,EAX
		JL trailNative_30065dff
		CMP EAX,01000h
		JGE trailNative_30065dff
		MOV EBX,dword ptr [ESP + 014h]
		TEST EAX,EAX
		JLE trailNative_30065ca0
		DEC EAX
		IMUL EAX,EAX,TrailNativeSize
		LEA EDI,[trailJuncs + EAX]
		MOV EAX,dword ptr [EDI + TrailNative_inuse]
		TEST EAX,EAX
		JZ trailNative_30065ca0
		CMP dword ptr [EDI + TrailNative_usedby],EBX
		JZ trailNative_30065ca2
trailNative_30065ca0:
		XOR EDI,EDI
trailNative_30065ca2:
		PUSH EDI
		CALL CG_SpawnTrailJunc
		MOV ESI,EAX
		ADD ESP,04h
		TEST ESI,ESI
		JZ trailNative_30065dff
		FLD dword ptr [ESP + 02ch]
		FCOMP qword ptr [trailNativeOneD]
		MOV dword ptr [ESI + TrailNative_usedby],EBX
		FNSTSW AX
		TEST AH,041h
		JNZ trailNative_30065cd1
		MOV dword ptr [ESP + 02ch],03f800000h
trailNative_30065cd1:
		FLD dword ptr [ESP + 02ch]
		FCOMP qword ptr [trailNativeZeroD]
		FNSTSW AX
		TEST AH,01h
		JZ trailNative_30065cea
		MOV dword ptr [ESP + 02ch],00h
trailNative_30065cea:
		FLD dword ptr [ESP + 030h]
		FCOM qword ptr [trailNativeOneD]
		FNSTSW AX
		TEST AH,041h
		JNZ trailNative_30065d03
		FSTP st(0)
		FLD dword ptr [trailNativeOneF]
trailNative_30065d03:
		FCOM qword ptr [trailNativeZeroD]
		FNSTSW AX
		TEST AH,01h
		JZ trailNative_30065d18
		FSTP st(0)
		FLD dword ptr [trailNativeZeroF]
trailNative_30065d18:
		MOV EDX,dword ptr [ESP + 018h]
		MOV EAX,dword ptr [ESP + 024h]
		MOV dword ptr [ESI + TrailNative_shader],EDX
		MOV EDX,dword ptr [ESP + 020h]
		MOV dword ptr [ESI + TrailNative_sType],EDX
		MOV ECX,dword ptr [EAX]
		MOV dword ptr [ESI + TrailNative_pos],ECX
		MOV ECX,dword ptr [EAX + 04h]
		MOV dword ptr [ESI + TrailNative_pos + 4],ECX
		MOV ECX,dword ptr [EAX + 08h]
		MOV EBX,dword ptr [ESP + 028h]
		MOV dword ptr [ESI + TrailNative_pos + 8],ECX
		MOV ECX,dword ptr [ESP + 03ch]
		MOV dword ptr [ESI + TrailNative_flags],ECX
		MOV ECX,dword ptr [ESP + 01ch]
		MOV dword ptr [ESI + TrailNative_spawnTime],ECX
		ADD ECX,EBX
		MOV dword ptr [ESI + TrailNative_endTime],ECX
		MOV ECX,dword ptr [ESP + 040h]
		CMP EDX,01h
		MOV EBX,dword ptr [ECX]
		MOV dword ptr [ESI + TrailNative_colorStart],EBX
		MOV EBX,dword ptr [ECX + 04h]
		MOV dword ptr [ESI + TrailNative_colorStart + 4],EBX
		MOV ECX,dword ptr [ECX + 08h]
		MOV dword ptr [ESI + TrailNative_colorStart + 8],ECX
		MOV ECX,dword ptr [ESP + 044h]
		MOV EBX,dword ptr [ECX]
		MOV dword ptr [ESI + TrailNative_colorEnd],EBX
		MOV EBX,dword ptr [ECX + 04h]
		MOV dword ptr [ESI + TrailNative_colorEnd + 4],EBX
		MOV ECX,dword ptr [ECX + 08h]
		MOV dword ptr [ESI + TrailNative_colorEnd + 8],ECX
		MOV ECX,dword ptr [ESP + 02ch]
		FSTP dword ptr [ESI + TrailNative_alphaEnd]
		MOV dword ptr [ESI + TrailNative_alphaStart],ECX
		MOV ECX,dword ptr [ESP + 034h]
		MOV dword ptr [ESI + TrailNative_widthStart],ECX
		MOV ECX,dword ptr [ESP + 038h]
		MOV dword ptr [ESI + TrailNative_widthEnd],ECX
		JNZ trailNative_30065de2
		TEST EDI,EDI
		JZ trailNative_30065db6
		LEA EDX,[EDI + TrailNative_pos]
		PUSH EAX
		PUSH EDX
		CALL Distance
		FDIV dword ptr [ESP + 050h]
		ADD ESP,08h
		FDIV dword ptr [ESI + TrailNative_widthEnd]
		FADD dword ptr [EDI + TrailNative_sTex]
		JMP trailNative_30065ddf
trailNative_30065db6:
		MOV EAX,[cg + TrailNativeTime]
		MOV ECX,03e8h
		CDQ
		IDIV ECX
		MOV dword ptr [ESP + 02ch],EDX
		FILD dword ptr [ESP + 02ch]
		FMUL qword ptr [trailNativeMillis]
		FSUBR qword ptr [trailNativeOneD]
		FMUL dword ptr [ESP + 04ch]
		FDIV dword ptr [ESP + 048h]
trailNative_30065ddf:
		FSTP dword ptr [ESI + TrailNative_sTex]
trailNative_30065de2:
		MOV EAX,ESI
		SUB EAX,OFFSET trailJuncs
		CDQ
		MOV ECX,TrailNativeSize
		IDIV ECX
		INC EAX
		POP EDI
		POP ESI
		POP EBX
		RET
trailNative_30065dff:
		POP EDI
		POP ESI
		XOR EAX,EAX
		POP EBX
		RET
	}
}
#else
int CG_AddTrailJunc(int headJuncIndex, void *usedby, qhandle_t shader, int spawnTime, int sType, vec3_t pos, int trailLife, float alphaStart, float alphaEnd, float startWidth, float endWidth, int flags, vec3_t colorStart, vec3_t colorEnd, float sRatio, float animSpeed)
{
	trailJunc_t	*j, *headJunc;

	if (headJuncIndex < 0 || headJuncIndex >= MAX_TRAILJUNCS) {
	    return 0;
	}
	
	if (headJuncIndex > 0) {
		headJunc = &trailJuncs[headJuncIndex-1];

		// rain - zinx's trail fix
		if (!headJunc->inuse || headJunc->usedby != usedby)
			headJunc = NULL;
	}
	else
		headJunc = NULL;

	j = CG_SpawnTrailJunc(headJunc);
	if (!j) {
//		CG_Printf("couldnt spawn trail junc\n");
		return 0;
	}

	// rain - zinx's trail fix - mark who's using this trail so that
	// we can handle the someone-else-stole-our-trail case
	j->usedby = usedby;

	if (alphaStart > 1.0) alphaStart = 1.0;
	if (alphaStart < 0.0) alphaStart = 0.0;
	if (alphaEnd > 1.0) alphaEnd = 1.0;
	if (alphaEnd < 0.0) alphaEnd = 0.0;

	// setup the trail junction
	j->shader = shader;
	j->sType = sType;
	VectorCopy( pos, j->pos );
	j->flags = flags;

	j->spawnTime = spawnTime;
	j->endTime = spawnTime + trailLife;

	VectorCopy( colorStart, j->colorStart );
	VectorCopy( colorEnd, j->colorEnd );

	j->alphaStart = alphaStart;
	j->alphaEnd = alphaEnd;

	j->widthStart = startWidth;
	j->widthEnd = endWidth;

	if (sType == STYPE_REPEAT) {
		if (headJunc) {
			j->sTex = headJunc->sTex + ((Distance( headJunc->pos, pos ) / sRatio) / j->widthEnd);
		} else {
			// FIXME: need a way to specify offset timing
			j->sTex = (animSpeed * (1.0 - ((float)(cg.time%1000) / 1000.0))) / (sRatio);
//			j->sTex = 0;
		}
	}

	return ((int)(j - trailJuncs) + 1);
}
#endif


/*
===============
CG_AddSparkJunc

  returns the index of the trail junction created
===============
*/
int CG_AddSparkJunc(int headJuncIndex, void *usedby, qhandle_t shader, vec3_t pos, int trailLife, float alphaStart, float alphaEnd, float startWidth, float endWidth)
{
	trailJunc_t	*j, *headJunc;

	if (headJuncIndex < 0 || headJuncIndex >= MAX_TRAILJUNCS) {
	    return 0;
	}
	
	if (headJuncIndex > 0) {
		headJunc = &trailJuncs[headJuncIndex-1];

		// rain - zinx's trail fix
		if (!headJunc->inuse || headJunc->usedby != usedby)
			headJunc = NULL;
	}
	else
		headJunc = NULL;

	j = CG_SpawnTrailJunc(headJunc);
	if (!j)
		return 0;

	j->usedby = usedby;

	// setup the trail junction
	j->shader = shader;
	j->sType = STYPE_STRETCH;
	VectorCopy( pos, j->pos );
	j->flags = TJFL_NOCULL;		// don't worry about fading up close

	j->spawnTime = cg.time;
	j->endTime = cg.time + trailLife;

	VectorSet(j->colorStart, 1.0, 0.8 + 0.2 * alphaStart, 0.4 + 0.4 * alphaStart);
	VectorSet(j->colorEnd, 1.0, 0.8 + 0.2 * alphaEnd, 0.4 + 0.4 * alphaEnd);
//	VectorScale( j->colorStart, alphaStart, j->colorStart );
//	VectorScale( j->colorEnd, alphaEnd, j->colorEnd );

	j->alphaStart = alphaStart*2;
	j->alphaEnd = alphaEnd*2;
//	j->alphaStart = 1.0;
//	j->alphaEnd = 1.0;

	j->widthStart = startWidth;
	j->widthEnd = endWidth;

	return ((int)(j - trailJuncs) + 1);
}

/*
===============
CG_AddSmokeJunc

  returns the index of the trail junction created
===============
*/
int CG_AddSmokeJunc(int headJuncIndex, void *usedby, qhandle_t shader, vec3_t pos, int trailLife, float alpha, float startWidth, float endWidth)
{
#define	ST_RATIO	4.0		// sprite image: width / height
	trailJunc_t	*j, *headJunc;

	if (headJuncIndex < 0 || headJuncIndex >= MAX_TRAILJUNCS) {
	    return 0;
	}
	
	if (headJuncIndex > 0) {
		headJunc = &trailJuncs[headJuncIndex-1];

		// rain - zinx's trail fix
		if (!headJunc->inuse || headJunc->usedby != usedby)
			headJunc = NULL;
	}
	else
		headJunc = NULL;

	j = CG_SpawnTrailJunc(headJunc);
	if (!j)
		return 0;

	j->usedby = usedby;

	// setup the trail junction
	j->shader = shader;
	j->sType = STYPE_REPEAT;
	VectorCopy( pos, j->pos );
	j->flags = TJFL_FADEIN;

	j->spawnTime = cg.time;
	j->endTime = cg.time + trailLife;

		VectorSet(j->colorStart, 0.7, 0.7, 0.7);
		VectorSet(j->colorEnd, 0.0, 0.0, 0.0);

	j->alphaStart = alpha;
	j->alphaEnd = 0.0;

	j->widthStart = startWidth;
	j->widthEnd = endWidth;

	if (headJunc) {
		j->sTex = headJunc->sTex + ((Distance( headJunc->pos, pos ) / ST_RATIO) / j->widthEnd);
	} else {
		// first junction, so this will become the "tail" very soon, make it fade out
		j->sTex = 0;
		j->alphaStart = 0.0;
		j->alphaEnd = 0.0;
	}

	return ((int)(j - trailJuncs) + 1);
}

void CG_KillTrail( trailJunc_t *t );

/*
===========
CG_FreeTrailJunc
===========
*/
void CG_FreeTrailJunc( trailJunc_t *junc )
{
	// kill any juncs after us, so they aren't left hanging
	if (junc->nextJunc)
		CG_KillTrail( junc );

	// make it non-active
	junc->inuse = qfalse;
	junc->freed = qtrue;
	if (junc->nextGlobal)
		junc->nextGlobal->prevGlobal = junc->prevGlobal;
	if (junc->prevGlobal)
		junc->prevGlobal->nextGlobal = junc->nextGlobal;
	if (junc == activeTrails)
		activeTrails = junc->nextGlobal;

	// if it's a head, remove it
	if (junc == headTrails)
		headTrails = junc->nextHead;
	if (junc->nextHead)
		junc->nextHead->prevHead = junc->prevHead;
	if (junc->prevHead)
		junc->prevHead->nextHead = junc->nextHead;
	junc->nextHead = NULL;
	junc->prevHead = NULL;

	// stick it in the free list
	junc->prevGlobal = NULL;
	junc->nextGlobal = freeTrails;
	if (freeTrails)
		freeTrails->prevGlobal = junc;
	freeTrails = junc;

	numTrailsInuse--;
}

/*
===========
CG_KillTrail
===========
*/
void CG_KillTrail( trailJunc_t *t )
{
    trailJunc_t	*next;
    if (!t->inuse && t->freed) {
	return;
    }
    next = t->nextJunc;
    if (next<&trailJuncs[0] || next>=&trailJuncs[MAX_TRAILJUNCS]) {
	next = NULL;
    }
    t->nextJunc = NULL;
    /* Original Windows/Linux dereference NULL after the range guard. Preserve
       valid-chain behaviour while defining the invalid/tail-chain case. */
    if (next && next->nextJunc && next->nextJunc == t) {
	next->nextJunc = NULL;
    }
    if (next) {
	CG_FreeTrailJunc( next );
    }
}

/*
==============
CG_AddTrailToScene

  TODO: this can do with some major optimization
==============
*/
static vec3_t vforward, vright, vup;
#define	MAX_TRAIL_VERTS		2048
static	polyVert_t	verts[MAX_TRAIL_VERTS];
static	polyVert_t	outVerts[MAX_TRAIL_VERTS*3];

#if defined(_MSC_VER) && defined(_M_IX86)
/* TC30066160: exact local x87 graph. External object fields use native layouts;
 * the stack-resident polyVert_t and SIMD-sized arrays have the engine ABI below. */
typedef char trailScenePolyLayout[(sizeof(polyVert_t) == 24 &&
 offsetof(polyVert_t, xyz) == 0 && offsetof(polyVert_t, st) == 12 &&
 offsetof(polyVert_t, modulate) == 20 && sizeof(vec4_t) == 16 &&
 sizeof(vec2_t) == 8 && offsetof(polyBuffer_t, xyz) == 0) ? 1 : -1];
enum {
	TrailSceneSparkShader = offsetof(cgs_t, media) + offsetof(cgMedia_t, sparkFlareShader),
	TrailSceneRefdef = offsetof(cg_t, refdef_current),
	TrailSceneVieworg = offsetof(refdef_t, vieworg),
	TrailScenePolySize = sizeof(polyVert_t),
	TrailScenePolyST = offsetof(polyVert_t, st),
	TrailScenePolyColor = offsetof(polyVert_t, modulate),
	TrailScene_flags = offsetof(trailJunc_t, flags),
	TrailScenePB_701c = offsetof(polyBuffer_t, numVerts),
	TrailScene_pos = offsetof(trailJunc_t, pos),
	TrailScene_width = offsetof(trailJunc_t, width),
	TrailScenePB_4008 = offsetof(polyBuffer_t, st) - 8,
	TrailScenePB_400c = offsetof(polyBuffer_t, st) - 4,
	TrailScenePB_4010 = offsetof(polyBuffer_t, st) + 0,
	TrailScenePB_4014 = offsetof(polyBuffer_t, st) + 4,
	TrailScenePB_6018 = offsetof(polyBuffer_t, color) + 0,
	TrailScenePB_6019 = offsetof(polyBuffer_t, color) + 1,
	TrailScenePB_601a = offsetof(polyBuffer_t, color) + 2,
	TrailScene_alpha = offsetof(trailJunc_t, alpha),
	TrailScenePB_601b = offsetof(polyBuffer_t, color) + 3,
	TrailScenePB_d038 = offsetof(polyBuffer_t, numIndicies),
	TrailScenePB_7020 = offsetof(polyBuffer_t, indicies) + 0,
	TrailScenePB_7024 = offsetof(polyBuffer_t, indicies) + 4,
	TrailScenePB_7028 = offsetof(polyBuffer_t, indicies) + 8,
	TrailScenePB_702c = offsetof(polyBuffer_t, indicies) + 12,
	TrailScenePB_7030 = offsetof(polyBuffer_t, indicies) + 16,
	TrailScenePB_7034 = offsetof(polyBuffer_t, indicies) + 20,
	TrailScene_inuse = offsetof(trailJunc_t, inuse),
	TrailScene_nextJunc = offsetof(trailJunc_t, nextJunc),
	TrailScene_freed = offsetof(trailJunc_t, freed),
	TrailScene_sType = offsetof(trailJunc_t, sType),
	TrailScene_sTex = offsetof(trailJunc_t, sTex),
	TrailScene_widthEnd = offsetof(trailJunc_t, widthEnd),
	TrailScene_color = offsetof(trailJunc_t, color),
	TrailScene_shader = offsetof(trailJunc_t, shader)
};
static const unsigned int trailSceneConst30092404[] = { 0xbf800000 };
static const unsigned int trailSceneConst30092920[] = { 0x00000000, 0xbfe00000 };
static const unsigned int trailSceneConst30092c28[] = { 0xc0000000 };
static const unsigned int trailSceneConst300927e8[] = { 0x00000000, 0x406fe000 };
static const unsigned int trailSceneConst30092b40[] = { 0x33333333, 0x3fd33333 };
static const unsigned int trailSceneConst300922bc[] = { 0x40800000 };
static const unsigned int trailSceneConst30092f28[] = { 0x00000000, 0x40700000 };
static const unsigned int trailSceneConst30092e58[] = { 0x00000000, 0x40500000 };
static const unsigned int trailSceneConst300920e0[] = { 0x00000000 };
static const unsigned int trailSceneConst30092f20[] = { 0x00000000, 0x3f700000 };
static const unsigned int trailSceneConst300922e0[] = { 0x00000000, 0x3fe00000 };
static const unsigned int trailSceneConst300922c8[] = { 0x00000000, 0x3ff00000 };
static const unsigned int trailSceneConst300923f0[] = { 0x00000000, 0x3fd00000 };
__declspec(naked) static int CG_TrailSceneTruncateST0(void)
{
	__asm {
		push ebp
		mov ebp, esp
		sub esp, 12
		fwait
		fnstcw word ptr [ebp-2]
		fwait
		mov ax, word ptr [ebp-2]
		or ah, 0ch
		mov word ptr [ebp-4], ax
		fldcw word ptr [ebp-4]
		fistp qword ptr [ebp-12]
		fldcw word ptr [ebp-2]
		mov eax, dword ptr [ebp-12]
		mov edx, dword ptr [ebp-8]
		leave
		ret
	}
}
__declspec(naked) void CG_AddTrailToScene(trailJunc_t *trail, int iteration, int numJuncs)
{
	__asm {
		SUB ESP,084h
		PUSH EBX
		PUSH EBP
		MOV EBP,dword ptr [ESP + 090h]
		PUSH ESI
		PUSH EDI
		MOV EDI,03f800000h
		TEST byte ptr [EBP + TrailScene_flags],010h
		JZ trailScene_300664e5
		MOV EAX,[cgs + TrailSceneSparkShader]
		PUSH 06h
		PUSH 04h
		PUSH EAX
		CALL CG_PB_FindFreePolyBuffer
		MOV ESI,EAX
		XOR EBX,EBX
		ADD ESP,0ch
		CMP ESI,EBX
		JZ trailScene_300664e7
		MOV EAX,dword ptr [ESI + TrailScenePB_701c]
		MOV EDX,dword ptr [EBP + TrailScene_pos]
		MOV ECX,EAX
		SHL ECX,04h
		ADD ECX,ESI
		INC EAX
		MOV dword ptr [ECX],EDX
		MOV EDX,dword ptr [EBP + TrailScene_pos + 4]
		MOV dword ptr [ECX + 04h],EDX
		MOV EDX,dword ptr [EBP + TrailScene_pos + 8]
		MOV dword ptr [ECX + 08h],EDX
		FLD dword ptr [EBP + TrailScene_width]
		FMUL dword ptr [trailSceneConst30092c28]
		FMUL dword ptr [vup]
		FADD dword ptr [ECX]
		FSTP dword ptr [ECX]
		FLD dword ptr [EBP + TrailScene_width]
		FMUL dword ptr [trailSceneConst30092c28]
		FMUL dword ptr [vup + 4]
		FADD dword ptr [ECX + 04h]
		FSTP dword ptr [ECX + 04h]
		FLD dword ptr [EBP + TrailScene_width]
		FMUL dword ptr [trailSceneConst30092c28]
		FMUL dword ptr [vup + 8]
		FADD dword ptr [ECX + 08h]
		FSTP dword ptr [ECX + 08h]
		FLD dword ptr [EBP + TrailScene_width]
		FMUL dword ptr [trailSceneConst30092c28]
		FMUL dword ptr [vright]
		FADD dword ptr [ECX]
		FSTP dword ptr [ECX]
		FLD dword ptr [EBP + TrailScene_width]
		FMUL dword ptr [trailSceneConst30092c28]
		FMUL dword ptr [vright + 4]
		FADD dword ptr [ECX + 04h]
		FSTP dword ptr [ECX + 04h]
		FLD dword ptr [EBP + TrailScene_width]
		FMUL dword ptr [trailSceneConst30092c28]
		FMUL dword ptr [vright + 8]
		FADD dword ptr [ECX + 08h]
		FSTP dword ptr [ECX + 08h]
		MOV dword ptr [ESI + EAX*08h + TrailScenePB_4008],EBX
		MOV dword ptr [ESI + EAX*08h + TrailScenePB_400c],EBX
		MOV EDX,dword ptr [EBP + TrailScene_pos]
		MOV ECX,EAX
		SHL ECX,04h
		ADD ECX,ESI
		MOV dword ptr [ECX],EDX
		MOV EDX,dword ptr [EBP + TrailScene_pos + 4]
		MOV dword ptr [ECX + 04h],EDX
		MOV EDX,dword ptr [EBP + TrailScene_pos + 8]
		MOV dword ptr [ECX + 08h],EDX
		FLD dword ptr [EBP + TrailScene_width]
		FMUL dword ptr [trailSceneConst30092c28]
		FMUL dword ptr [vup]
		FADD dword ptr [ECX]
		FSTP dword ptr [ECX]
		FLD dword ptr [EBP + TrailScene_width]
		FMUL dword ptr [trailSceneConst30092c28]
		FMUL dword ptr [vup + 4]
		FADD dword ptr [ECX + 04h]
		FSTP dword ptr [ECX + 04h]
		FLD dword ptr [EBP + TrailScene_width]
		FMUL dword ptr [trailSceneConst30092c28]
		FMUL dword ptr [vup + 8]
		FADD dword ptr [ECX + 08h]
		FSTP dword ptr [ECX + 08h]
		FLD dword ptr [EBP + TrailScene_width]
		FADD st(0),st(0)
		FMUL dword ptr [vright]
		FADD dword ptr [ECX]
		FSTP dword ptr [ECX]
		FLD dword ptr [EBP + TrailScene_width]
		FADD st(0),st(0)
		INC EAX
		FMUL dword ptr [vright + 4]
		FADD dword ptr [ECX + 04h]
		FSTP dword ptr [ECX + 04h]
		FLD dword ptr [EBP + TrailScene_width]
		FADD st(0),st(0)
		FMUL dword ptr [vright + 8]
		FADD dword ptr [ECX + 08h]
		FSTP dword ptr [ECX + 08h]
		MOV dword ptr [ESI + EAX*08h + TrailScenePB_4008],EBX
		MOV dword ptr [ESI + EAX*08h + TrailScenePB_400c],EDI
		MOV EDX,dword ptr [EBP + TrailScene_pos]
		MOV ECX,EAX
		SHL ECX,04h
		ADD ECX,ESI
		INC EAX
		MOV dword ptr [ECX],EDX
		MOV EDX,dword ptr [EBP + TrailScene_pos + 4]
		MOV dword ptr [ECX + 04h],EDX
		MOV EDX,dword ptr [EBP + TrailScene_pos + 8]
		MOV dword ptr [ECX + 08h],EDX
		FLD dword ptr [EBP + TrailScene_width]
		FADD st(0),st(0)
		FMUL dword ptr [vup]
		FADD dword ptr [ECX]
		FSTP dword ptr [ECX]
		FLD dword ptr [EBP + TrailScene_width]
		FADD st(0),st(0)
		FMUL dword ptr [vup + 4]
		FADD dword ptr [ECX + 04h]
		FSTP dword ptr [ECX + 04h]
		FLD dword ptr [EBP + TrailScene_width]
		FADD st(0),st(0)
		FMUL dword ptr [vup + 8]
		FADD dword ptr [ECX + 08h]
		FSTP dword ptr [ECX + 08h]
		FLD dword ptr [EBP + TrailScene_width]
		FADD st(0),st(0)
		FMUL dword ptr [vright]
		FADD dword ptr [ECX]
		FSTP dword ptr [ECX]
		FLD dword ptr [EBP + TrailScene_width]
		FADD st(0),st(0)
		FMUL dword ptr [vright + 4]
		FADD dword ptr [ECX + 04h]
		FSTP dword ptr [ECX + 04h]
		FLD dword ptr [EBP + TrailScene_width]
		FADD st(0),st(0)
		FMUL dword ptr [vright + 8]
		FADD dword ptr [ECX + 08h]
		FSTP dword ptr [ECX + 08h]
		MOV dword ptr [ESI + EAX*08h + TrailScenePB_4008],EDI
		MOV dword ptr [ESI + EAX*08h + TrailScenePB_400c],EDI
		MOV EDX,dword ptr [EBP + TrailScene_pos]
		MOV ECX,EAX
		SHL ECX,04h
		ADD ECX,ESI
		MOV dword ptr [ECX],EDX
		MOV EDX,dword ptr [EBP + TrailScene_pos + 4]
		MOV dword ptr [ECX + 04h],EDX
		MOV EDX,dword ptr [EBP + TrailScene_pos + 8]
		MOV dword ptr [ECX + 08h],EDX
		FLD dword ptr [EBP + TrailScene_width]
		FADD st(0),st(0)
		FMUL dword ptr [vup]
		FADD dword ptr [ECX]
		FSTP dword ptr [ECX]
		FLD dword ptr [EBP + TrailScene_width]
		FADD st(0),st(0)
		FMUL dword ptr [vup + 4]
		FADD dword ptr [ECX + 04h]
		FSTP dword ptr [ECX + 04h]
		FLD dword ptr [EBP + TrailScene_width]
		FADD st(0),st(0)
		FMUL dword ptr [vup + 8]
		FADD dword ptr [ECX + 08h]
		FSTP dword ptr [ECX + 08h]
		FLD dword ptr [EBP + TrailScene_width]
		FMUL dword ptr [trailSceneConst30092c28]
		FMUL dword ptr [vright]
		FADD dword ptr [ECX]
		FSTP dword ptr [ECX]
		FLD dword ptr [EBP + TrailScene_width]
		FMUL dword ptr [trailSceneConst30092c28]
		FMUL dword ptr [vright + 4]
		FADD dword ptr [ECX + 04h]
		FSTP dword ptr [ECX + 04h]
		FLD dword ptr [EBP + TrailScene_width]
		FMUL dword ptr [trailSceneConst30092c28]
		FMUL dword ptr [vright + 8]
		FADD dword ptr [ECX + 08h]
		FSTP dword ptr [ECX + 08h]
		MOV dword ptr [ESI + EAX*08h + TrailScenePB_4010],EDI
		MOV dword ptr [ESI + EAX*08h + TrailScenePB_4014],EBX
		XOR EDI,EDI
		MOV BL,0ffh
trailScene_300663fb:
		MOV EAX,dword ptr [ESI + TrailScenePB_701c]
		ADD EAX,EDI
		MOV byte ptr [ESI + EAX*04h + TrailScenePB_6018],BL
		MOV ECX,dword ptr [ESI + TrailScenePB_701c]
		ADD ECX,EDI
		MOV byte ptr [ESI + ECX*04h + TrailScenePB_6019],BL
		MOV EDX,dword ptr [ESI + TrailScenePB_701c]
		ADD EDX,EDI
		MOV byte ptr [ESI + EDX*04h + TrailScenePB_601a],BL
		FLD dword ptr [EBP + TrailScene_alpha]
		FMUL qword ptr [trailSceneConst300927e8]
		CALL CG_TrailSceneTruncateST0
		MOV ECX,dword ptr [ESI + TrailScenePB_701c]
		ADD ECX,EDI
		INC EDI
		CMP EDI,04h
		MOV byte ptr [ESI + ECX*04h + TrailScenePB_601b],AL
		JL trailScene_300663fb
		MOV EDX,dword ptr [ESI + TrailScenePB_d038]
		MOV EAX,dword ptr [ESI + TrailScenePB_701c]
		MOV dword ptr [ESI + EDX*04h + TrailScenePB_7020],EAX
		MOV ECX,dword ptr [ESI + TrailScenePB_701c]
		MOV EDX,dword ptr [ESI + TrailScenePB_d038]
		INC ECX
		MOV dword ptr [ESI + EDX*04h + TrailScenePB_7024],ECX
		MOV EAX,dword ptr [ESI + TrailScenePB_701c]
		MOV ECX,dword ptr [ESI + TrailScenePB_d038]
		ADD EAX,02h
		MOV dword ptr [ESI + ECX*04h + TrailScenePB_7028],EAX
		MOV EDX,dword ptr [ESI + TrailScenePB_701c]
		MOV EAX,dword ptr [ESI + TrailScenePB_d038]
		ADD EDX,02h
		MOV dword ptr [ESI + EAX*04h + TrailScenePB_702c],EDX
		MOV ECX,dword ptr [ESI + TrailScenePB_701c]
		MOV EDX,dword ptr [ESI + TrailScenePB_d038]
		ADD ECX,03h
		MOV dword ptr [ESI + EDX*04h + TrailScenePB_7030],ECX
		MOV EAX,dword ptr [ESI + TrailScenePB_d038]
		MOV ECX,dword ptr [ESI + TrailScenePB_701c]
		MOV dword ptr [ESI + EAX*04h + TrailScenePB_7034],ECX
		MOV ECX,dword ptr [ESI + TrailScenePB_701c]
		MOV EAX,dword ptr [ESI + TrailScenePB_d038]
		ADD ECX,04h
		ADD EAX,06h
		MOV dword ptr [ESI + TrailScenePB_701c],ECX
		MOV dword ptr [ESI + TrailScenePB_d038],EAX
trailScene_300664e5:
		XOR EBX,EBX
trailScene_300664e7:
		MOV EDI,dword ptr [ESP + 0a0h]
		MOV dword ptr [ESP + 04ch],00h
		CMP EDI,EBX
		JNZ trailScene_30066565
		XOR EDI,EDI
		CMP EBP,EBX
		MOV ESI,EBP
		MOV dword ptr [ESP + 04ch],EDI
		JZ trailScene_30066fa9
trailScene_3006650a:
		MOV EAX,dword ptr [ESI + TrailScene_inuse]
		INC EDI
		CMP EAX,EBX
		JNZ trailScene_30066529
		MOV EAX,dword ptr [ESI + TrailScene_nextJunc]
		CMP EAX,EBX
		JZ trailScene_30066529
		CMP dword ptr [EAX + TrailScene_inuse],EBX
		JNZ trailScene_30066529
		PUSH ESI
		CALL CG_KillTrail
		ADD ESP,04h
		JMP trailScene_30066538
trailScene_30066529:
		MOV EAX,dword ptr [ESI + TrailScene_nextJunc]
		CMP EAX,EBX
		JZ trailScene_30066538
		CMP dword ptr [EAX + TrailScene_freed],EBX
		JZ trailScene_30066538
		MOV dword ptr [ESI + TrailScene_nextJunc],EBX
trailScene_30066538:
		MOV EAX,dword ptr [ESI + TrailScene_nextJunc]
		CMP EAX,EBX
		JZ trailScene_30066557
		LEA EDX,[ESI + TrailScene_pos]
		ADD EAX,TrailScene_pos
		PUSH EDX
		PUSH EAX
		CALL Distance
		FADD dword ptr [ESP + 054h]
		ADD ESP,08h
		FSTP dword ptr [ESP + 04ch]
trailScene_30066557:
		MOV ESI,dword ptr [ESI + TrailScene_nextJunc]
		CMP ESI,EBX
		JNZ trailScene_3006650a
		MOV dword ptr [ESP + 0a0h],EDI
trailScene_30066565:
		CMP EDI,02h
		JL trailScene_30066fa9
		MOV EAX,dword ptr [EBP + TrailScene_sType]
		MOV dword ptr [ESP + 02ch],00h
		CMP EAX,EBX
		JNZ trailScene_30066587
		MOV dword ptr [ESP + 02ch],03d4ccccdh
		JMP trailScene_30066593
trailScene_30066587:
		CMP EAX,01h
		JNZ trailScene_30066593
		MOV EAX,dword ptr [EBP + TrailScene_sTex]
		MOV dword ptr [ESP + 02ch],EAX
trailScene_30066593:
		MOV EBX,dword ptr [EBP + TrailScene_nextJunc]
		XOR EAX,EAX
		TEST EBX,EBX
		MOV dword ptr [ESP + 01ch],EBP
		MOV dword ptr [ESP + 018h],EAX
		JZ trailScene_30066d33
		MOV EBP,OFFSET verts + 8
		MOV EDI,OFFSET verts + 4
		MOV EAX,EBP
		MOV ESI,OFFSET verts
		SUB EAX,TrailScenePolySize * 3
		MOV dword ptr [ESP + 020h],OFFSET verts + TrailScenePolyColor + 3
		MOV dword ptr [ESP + 034h],EAX
		MOV EAX,EDI
		SUB EAX,TrailScenePolySize * 3
		MOV dword ptr [ESP + 024h],OFFSET verts + TrailScenePolyColor
		MOV dword ptr [ESP + 038h],EAX
		MOV EAX,ESI
		SUB EAX,TrailScenePolySize * 3
		MOV dword ptr [ESP + 028h],OFFSET verts + TrailScenePolyST + 4
		MOV dword ptr [ESP + 030h],OFFSET verts + TrailScenePolyST
		MOV dword ptr [ESP + 03ch],EAX
trailScene_300665f2:
		MOV ECX,dword ptr [ESP + 01ch]
		LEA EDX,[ESP + 040h]
		LEA EAX,[EBX + TrailScene_pos]
		PUSH EDX
		PUSH EAX
		MOV EAX,[cg + TrailSceneRefdef]
		ADD ECX,TrailScene_pos
		ADD EAX,TrailSceneVieworg
		PUSH ECX
		PUSH EAX
		MOV dword ptr [ESP + 070h],ECX
		CALL GetPerpendicularViewVector
		MOV ECX,dword ptr [ESP + 02ch]
		ADD ESP,010h
		MOV EAX,dword ptr [ECX + TrailScene_flags]
		TEST AL,02h
		JZ trailScene_30066727
		MOV EAX,dword ptr [ESP + 09ch]
		TEST EAX,EAX
		JLE trailScene_300667ef
		MOV ECX,dword ptr [cg + TrailSceneRefdef]
		LEA EDX,[ESP + 088h]
		LEA EAX,[EBX + TrailScene_pos]
		PUSH EDX
		PUSH EAX
		MOV EAX,dword ptr [ESP + 068h]
		ADD ECX,TrailSceneVieworg
		PUSH EAX
		PUSH ECX
		CALL ProjectPointOntoVector
		MOV EAX,[cg + TrailSceneRefdef]
		LEA EDX,[ESP + 074h]
		PUSH EDX
		FLD dword ptr [EAX + TrailSceneVieworg + 0]
		FSUB dword ptr [ESP + 09ch]
		FSTP dword ptr [ESP + 078h]
		FLD dword ptr [EAX + TrailSceneVieworg + 4]
		FSUB dword ptr [ESP + 0a0h]
		FSTP dword ptr [ESP + 07ch]
		FLD dword ptr [EAX + TrailSceneVieworg + 8]
		FSUB dword ptr [ESP + 0a4h]
		FSTP dword ptr [ESP + 080h]
		CALL VectorNormalize
		MOV EAX,dword ptr [ESP + 0b0h]
		ADD ESP,014h
		CMP EAX,01h
		FSTP st(0)
		JNZ trailScene_300666d7
		FLD dword ptr [ESP + 064h]
		FMUL qword ptr [trailSceneConst30092b40]
		FADD dword ptr [ESP + 040h]
		FSTP dword ptr [ESP + 040h]
		FLD dword ptr [ESP + 068h]
		FMUL qword ptr [trailSceneConst30092b40]
		FADD dword ptr [ESP + 044h]
		FSTP dword ptr [ESP + 044h]
		FLD dword ptr [ESP + 06ch]
		FMUL qword ptr [trailSceneConst30092b40]
		FADD dword ptr [ESP + 048h]
		JMP trailScene_3006670f
trailScene_300666d7:
		FLD dword ptr [ESP + 040h]
		FLD dword ptr [ESP + 064h]
		FMUL qword ptr [trailSceneConst30092b40]
		FSUBP st(1),st(0)
		FSTP dword ptr [ESP + 040h]
		FLD dword ptr [ESP + 044h]
		FLD dword ptr [ESP + 068h]
		FMUL qword ptr [trailSceneConst30092b40]
		FSUBP st(1),st(0)
		FSTP dword ptr [ESP + 044h]
		FLD dword ptr [ESP + 048h]
		FLD dword ptr [ESP + 06ch]
		FMUL qword ptr [trailSceneConst30092b40]
		FSUBP st(1),st(0)
trailScene_3006670f:
		FSTP dword ptr [ESP + 048h]
		LEA EAX,[ESP + 040h]
		PUSH EAX
		CALL VectorNormalize
		FSTP st(0)
		ADD ESP,04h
		JMP trailScene_300667ef
trailScene_30066727:
		TEST AL,04h
		JNZ trailScene_300667ef
		MOV ECX,dword ptr [ESP + 01ch]
		FLD dword ptr [ECX + TrailScene_widthEnd]
		FCOMP dword ptr [trailSceneConst300922bc]
		FNSTSW AX
		TEST AH,041h
		JZ trailScene_30066757
		FLD dword ptr [EBX + TrailScene_widthEnd]
		FCOMP dword ptr [trailSceneConst300922bc]
		FNSTSW AX
		TEST AH,041h
		JNZ trailScene_300667ef
trailScene_30066757:
		MOV ECX,dword ptr [cg + TrailSceneRefdef]
		LEA EDX,[ESP + 088h]
		LEA EAX,[EBX + TrailScene_pos]
		PUSH EDX
		PUSH EAX
		MOV EAX,dword ptr [ESP + 068h]
		ADD ECX,TrailSceneVieworg
		PUSH EAX
		PUSH ECX
		CALL ProjectPointOntoVector
		MOV EDX,dword ptr [cg + TrailSceneRefdef]
		LEA EAX,[ESP + 098h]
		ADD EDX,TrailSceneVieworg
		PUSH EDX
		PUSH EAX
		CALL Distance
		FST dword ptr [ESP + 02ch]
		FCOMP qword ptr [trailSceneConst30092f28]
		ADD ESP,018h
		FNSTSW AX
		TEST AH,01h
		JZ trailScene_300667ef
		FLD dword ptr [ESP + 014h]
		FCOMP qword ptr [trailSceneConst30092e58]
		FNSTSW AX
		TEST AH,01h
		JZ trailScene_300667bb
		FLD dword ptr [trailSceneConst300920e0]
		JMP trailScene_300667cb
trailScene_300667bb:
		FLD dword ptr [ESP + 014h]
		FSUB qword ptr [trailSceneConst30092e58]
		FMUL qword ptr [trailSceneConst30092f20]
trailScene_300667cb:
		MOV ECX,dword ptr [ESP + 01ch]
		FLD st(0)
		FCOMP dword ptr [ECX + TrailScene_alpha]
		FNSTSW AX
		TEST AH,01h
		JZ trailScene_300667de
		FST dword ptr [ECX + TrailScene_alpha]
trailScene_300667de:
		FCOM dword ptr [EBX + TrailScene_alpha]
		FNSTSW AX
		TEST AH,01h
		JZ trailScene_300667ed
		FSTP dword ptr [EBX + TrailScene_alpha]
		JMP trailScene_300667ef
trailScene_300667ed:
		FSTP st(0)
trailScene_300667ef:
		MOV EAX,dword ptr [ESP + 01ch]
		MOV ECX,dword ptr [ESP + 060h]
		MOV dword ptr [ESP + 014h],00h
		FLD dword ptr [EAX + TrailScene_width]
		FMUL qword ptr [trailSceneConst300922e0]
		FLD dword ptr [ESP + 040h]
		ADD EAX,TrailScene_color
		FMUL st(0),st(1)
		MOV dword ptr [ESP + 010h],EAX
		FADD dword ptr [ECX]
		FSTP dword ptr [ESP + 050h]
		FLD dword ptr [ESP + 044h]
		FMUL st(0),st(1)
		MOV EDX,dword ptr [ESP + 050h]
		FADD dword ptr [EAX + TrailScene_pos + 4 - TrailScene_color]
		FSTP dword ptr [ESP + 054h]
		MOV ECX,dword ptr [ESP + 054h]
		FMUL dword ptr [ESP + 048h]
		FADD dword ptr [EAX + TrailScene_pos + 8 - TrailScene_color]
		MOV dword ptr [ESI],EDX
		MOV EDX,dword ptr [ESP + 02ch]
		MOV dword ptr [EDI],ECX
		MOV ECX,dword ptr [ESP + 030h]
		FST dword ptr [EBP]
		MOV dword ptr [ECX],EDX
		MOV EDX,dword ptr [ESP + 028h]
		MOV dword ptr [EDX],03f800000h
trailScene_30066852:
		MOV EAX,dword ptr [ESP + 010h]
		FLD dword ptr [EAX]
		FMUL qword ptr [trailSceneConst300927e8]
		CALL CG_TrailSceneTruncateST0
		MOV EDX,dword ptr [ESP + 024h]
		MOV ECX,dword ptr [ESP + 014h]
		MOV byte ptr [EDX + ECX*01h],AL
		MOV EDX,dword ptr [ESP + 010h]
		INC ECX
		ADD EDX,04h
		CMP ECX,03h
		MOV dword ptr [ESP + 014h],ECX
		MOV dword ptr [ESP + 010h],EDX
		JL trailScene_30066852
		MOV EAX,dword ptr [ESP + 01ch]
		FLD dword ptr [EAX + TrailScene_alpha]
		FMUL qword ptr [trailSceneConst300927e8]
		CALL CG_TrailSceneTruncateST0
		MOV ECX,dword ptr [ESP + 020h]
		MOV EDX,dword ptr [ESP + 098h]
		MOV byte ptr [ECX],AL
		MOV EAX,dword ptr [ESP + 01ch]
		CMP EAX,EDX
		JZ trailScene_300668f7
		FLD dword ptr [ESI + -TrailScenePolySize]
		FADD dword ptr [ESI]
		FSTP dword ptr [ESI]
		FLD dword ptr [EDI]
		FADD dword ptr [EDI + -TrailScenePolySize]
		FSTP dword ptr [EDI]
		FLD dword ptr [EBP + -TrailScenePolySize]
		FADD dword ptr [EBP]
		FSTP dword ptr [EBP]
		FLD dword ptr [ESI]
		FMUL qword ptr [trailSceneConst300922e0]
		FSTP dword ptr [ESI]
		FLD dword ptr [EDI]
		FMUL qword ptr [trailSceneConst300922e0]
		FSTP dword ptr [EDI]
		FLD dword ptr [EBP]
		FMUL qword ptr [trailSceneConst300922e0]
		FSTP dword ptr [EBP]
		MOV EAX,dword ptr [ESI]
		MOV dword ptr [ESI + -TrailScenePolySize],EAX
		MOV ECX,dword ptr [EDI]
		MOV EAX,dword ptr [ESP + 01ch]
		MOV dword ptr [EDI + -TrailScenePolySize],ECX
		MOV EDX,dword ptr [EBP]
		MOV dword ptr [EBP + -TrailScenePolySize],EDX
		JMP trailScene_30066908
trailScene_300668f7:
		MOV EAX,dword ptr [ESP + 01ch]
		TEST byte ptr [EAX + TrailScene_flags],01h
		JZ trailScene_30066908
		MOV ECX,dword ptr [ESP + 020h]
		MOV byte ptr [ECX],00h
trailScene_30066908:
		FLD dword ptr [EAX + TrailScene_width]
		MOV EDX,dword ptr [ESP + 018h]
		MOV ECX,dword ptr [ESP + 03ch]
		FMUL dword ptr [trailSceneConst30092404]
		INC EDX
		MOV dword ptr [ESP + 014h],00h
		MOV dword ptr [ESP + 018h],EDX
		MOV EDX,TrailScenePolySize
		FLD st(0)
		FMUL dword ptr [ESP + 040h]
		ADD ECX,EDX
		ADD ESI,EDX
		MOV dword ptr [ESP + 03ch],ECX
		MOV ECX,dword ptr [ESP + 038h]
		FADD dword ptr [ESP + 050h]
		ADD ECX,EDX
		ADD EDI,EDX
		FLD st(1)
		FMUL dword ptr [ESP + 044h]
		MOV dword ptr [ESP + 038h],ECX
		MOV ECX,dword ptr [ESP + 034h]
		ADD ECX,EDX
		ADD EBP,EDX
		FADD dword ptr [ESP + 054h]
		MOV dword ptr [ESP + 034h],ECX
		MOV ECX,dword ptr [ESP + 030h]
		ADD ECX,EDX
		MOV EDX,dword ptr [ESP + 028h]
		ADD EDX,TrailScenePolySize
		MOV dword ptr [ESP + 030h],ECX
		FSTP dword ptr [ESP + 054h]
		FXCH
		FMUL dword ptr [ESP + 048h]
		MOV dword ptr [ESP + 028h],EDX
		MOV EDX,dword ptr [ESP + 024h]
		ADD EDX,TrailScenePolySize
		FADD st(0),st(2)
		MOV dword ptr [ESP + 024h],EDX
		MOV EDX,dword ptr [ESP + 020h]
		ADD EDX,TrailScenePolySize
		ADD EAX,TrailScene_color
		MOV dword ptr [ESP + 020h],EDX
		MOV EDX,dword ptr [ESP + 054h]
		FSTP dword ptr [ESP + 058h]
		MOV dword ptr [ESP + 010h],EAX
		FSTP dword ptr [ESI]
		MOV dword ptr [EDI],EDX
		MOV EDX,dword ptr [ESP + 058h]
		MOV dword ptr [EBP],EDX
		MOV EDX,dword ptr [ESP + 02ch]
		MOV dword ptr [ECX],EDX
		MOV ECX,dword ptr [ESP + 028h]
		FSTP st(0)
		MOV dword ptr [ECX],00h
trailScene_300669c2:
		MOV EDX,dword ptr [ESP + 010h]
		FLD dword ptr [EDX]
		FMUL qword ptr [trailSceneConst300927e8]
		CALL CG_TrailSceneTruncateST0
		MOV EDX,dword ptr [ESP + 024h]
		MOV ECX,dword ptr [ESP + 014h]
		MOV byte ptr [EDX + ECX*01h],AL
		MOV EDX,dword ptr [ESP + 010h]
		INC ECX
		ADD EDX,04h
		CMP ECX,03h
		MOV dword ptr [ESP + 014h],ECX
		MOV dword ptr [ESP + 010h],EDX
		JL trailScene_300669c2
		MOV EAX,dword ptr [ESP + 01ch]
		FLD dword ptr [EAX + TrailScene_alpha]
		FMUL qword ptr [trailSceneConst300927e8]
		CALL CG_TrailSceneTruncateST0
		MOV ECX,dword ptr [ESP + 020h]
		MOV EDX,dword ptr [ESP + 098h]
		MOV byte ptr [ECX],AL
		MOV EAX,dword ptr [ESP + 01ch]
		CMP EAX,EDX
		JZ trailScene_30066a6d
		MOV EAX,dword ptr [ESP + 03ch]
		MOV ECX,dword ptr [ESP + 038h]
		MOV EDX,dword ptr [ESP + 034h]
		FLD dword ptr [EAX]
		FADD dword ptr [ESI]
		FSTP dword ptr [ESI]
		FLD dword ptr [EDI]
		FADD dword ptr [ECX]
		FSTP dword ptr [EDI]
		FLD dword ptr [EDX]
		FADD dword ptr [EBP]
		FSTP dword ptr [EBP]
		FLD dword ptr [ESI]
		FMUL qword ptr [trailSceneConst300922e0]
		FSTP dword ptr [ESI]
		FLD dword ptr [EDI]
		FMUL qword ptr [trailSceneConst300922e0]
		FSTP dword ptr [EDI]
		FLD dword ptr [EBP]
		FMUL qword ptr [trailSceneConst300922e0]
		FSTP dword ptr [EBP]
		MOV EDX,dword ptr [ESI]
		MOV dword ptr [EAX],EDX
		MOV EAX,dword ptr [EDI]
		MOV EDX,dword ptr [ESP + 034h]
		MOV dword ptr [ECX],EAX
		MOV ECX,dword ptr [EBP]
		MOV dword ptr [EDX],ECX
		JMP trailScene_30066a7e
trailScene_30066a6d:
		MOV EAX,dword ptr [ESP + 01ch]
		TEST byte ptr [EAX + TrailScene_flags],01h
		JZ trailScene_30066a7e
		MOV ECX,dword ptr [ESP + 020h]
		MOV byte ptr [ECX],00h
trailScene_30066a7e:
		MOV EAX,dword ptr [ESP + 018h]
		MOV EDX,dword ptr [ESP + 03ch]
		INC EAX
		MOV ECX,dword ptr [ESP + 038h]
		MOV dword ptr [ESP + 018h],EAX
		MOV EAX,TrailScenePolySize
		ADD EDX,EAX
		ADD ECX,EAX
		MOV dword ptr [ESP + 03ch],EDX
		MOV EDX,dword ptr [ESP + 034h]
		ADD EDX,EAX
		MOV dword ptr [ESP + 038h],ECX
		MOV ECX,dword ptr [ESP + 028h]
		MOV dword ptr [ESP + 034h],EDX
		MOV EDX,dword ptr [ESP + 030h]
		ADD ECX,EAX
		ADD EDX,EAX
		MOV dword ptr [ESP + 028h],ECX
		MOV ECX,dword ptr [ESP + 020h]
		MOV dword ptr [ESP + 030h],EDX
		MOV EDX,dword ptr [ESP + 024h]
		ADD ESI,EAX
		ADD EDX,EAX
		ADD EDI,EAX
		MOV dword ptr [ESP + 024h],EDX
		MOV EDX,dword ptr [ESP + 098h]
		ADD EBP,EAX
		ADD ECX,EAX
		MOV EAX,dword ptr [EDX + TrailScene_sType]
		MOV dword ptr [ESP + 020h],ECX
		CMP EAX,01h
		JNZ trailScene_30066af0
		MOV EAX,dword ptr [EBX + TrailScene_sTex]
		MOV dword ptr [ESP + 02ch],EAX
		JMP trailScene_30066b22
trailScene_30066af0:
		MOV ECX,dword ptr [ESP + 060h]
		LEA EAX,[EBX + TrailScene_pos]
		PUSH EAX
		PUSH ECX
		CALL Distance
		FDIV dword ptr [ESP + 054h]
		ADD ESP,08h
		FADD dword ptr [ESP + 02ch]
		FST dword ptr [ESP + 02ch]
		FCOMP qword ptr [trailSceneConst300922c8]
		FNSTSW AX
		TEST AH,041h
		JNZ trailScene_30066b22
		MOV dword ptr [ESP + 02ch],03f800000h
trailScene_30066b22:
		FLD dword ptr [EBX + TrailScene_width]
		FMUL qword ptr [trailSceneConst30092920]
		FLD dword ptr [ESP + 040h]
		MOV ECX,dword ptr [ESP + 02ch]
		MOV dword ptr [ESP + 014h],00h
		FMUL st(0),st(1)
		FADD dword ptr [EBX + TrailScene_pos]
		FSTP dword ptr [ESP + 050h]
		FLD dword ptr [ESP + 044h]
		FMUL st(0),st(1)
		MOV EDX,dword ptr [ESP + 050h]
		FADD dword ptr [EBX + TrailScene_pos + 4]
		FSTP dword ptr [ESP + 054h]
		MOV EAX,dword ptr [ESP + 054h]
		FMUL dword ptr [ESP + 048h]
		FADD dword ptr [EBX + TrailScene_pos + 8]
		MOV dword ptr [ESI],EDX
		MOV EDX,dword ptr [ESP + 030h]
		MOV dword ptr [EDI],EAX
		MOV EAX,dword ptr [ESP + 028h]
		FST dword ptr [EBP]
		MOV dword ptr [EDX],ECX
		MOV dword ptr [EAX],00h
		LEA EAX,[EBX + TrailScene_color]
		MOV dword ptr [ESP + 010h],EAX
trailScene_30066b7e:
		MOV ECX,dword ptr [ESP + 010h]
		FLD dword ptr [ECX]
		FMUL qword ptr [trailSceneConst300927e8]
		CALL CG_TrailSceneTruncateST0
		MOV EDX,dword ptr [ESP + 024h]
		MOV ECX,dword ptr [ESP + 014h]
		MOV byte ptr [EDX + ECX*01h],AL
		MOV EDX,dword ptr [ESP + 010h]
		INC ECX
		ADD EDX,04h
		CMP ECX,03h
		MOV dword ptr [ESP + 014h],ECX
		MOV dword ptr [ESP + 010h],EDX
		JL trailScene_30066b7e
		FLD dword ptr [EBX + TrailScene_alpha]
		FMUL qword ptr [trailSceneConst300927e8]
		CALL CG_TrailSceneTruncateST0
		MOV ECX,dword ptr [ESP + 020h]
		MOV EDX,dword ptr [ESP + 018h]
		INC EDX
		MOV dword ptr [ESP + 014h],00h
		MOV byte ptr [ECX],AL
		MOV ECX,dword ptr [ESP + 03ch]
		FLD dword ptr [EBX + TrailScene_width]
		FMUL dword ptr [ESP + 040h]
		MOV EAX,dword ptr [ESP + 038h]
		MOV dword ptr [ESP + 018h],EDX
		MOV EDX,TrailScenePolySize
		FADD dword ptr [ESP + 050h]
		FLD dword ptr [EBX + TrailScene_width]
		FMUL dword ptr [ESP + 044h]
		ADD ECX,EDX
		ADD EAX,EDX
		MOV dword ptr [ESP + 03ch],ECX
		MOV ECX,dword ptr [ESP + 034h]
		FADD dword ptr [ESP + 054h]
		ADD ECX,EDX
		MOV dword ptr [ESP + 038h],EAX
		MOV EAX,dword ptr [ESP + 030h]
		MOV dword ptr [ESP + 034h],ECX
		MOV ECX,dword ptr [ESP + 028h]
		ADD ESI,EDX
		FSTP dword ptr [ESP + 054h]
		FLD dword ptr [EBX + TrailScene_width]
		FMUL dword ptr [ESP + 048h]
		ADD EDI,EDX
		ADD EBP,EDX
		ADD EAX,EDX
		ADD ECX,EDX
		MOV EDX,dword ptr [ESP + 024h]
		MOV dword ptr [ESP + 030h],EAX
		ADD EDX,TrailScenePolySize
		MOV dword ptr [ESP + 028h],ECX
		FADD st(0),st(2)
		MOV dword ptr [ESP + 024h],EDX
		MOV EDX,dword ptr [ESP + 020h]
		ADD EDX,TrailScenePolySize
		MOV dword ptr [ESP + 020h],EDX
		MOV EDX,dword ptr [ESP + 054h]
		FSTP dword ptr [ESP + 058h]
		FSTP dword ptr [ESI]
		MOV dword ptr [EDI],EDX
		MOV EDX,dword ptr [ESP + 058h]
		MOV dword ptr [EBP],EDX
		MOV EDX,dword ptr [ESP + 02ch]
		MOV dword ptr [EAX],EDX
		LEA EAX,[EBX + TrailScene_color]
		FSTP st(0)
		MOV dword ptr [ECX],03f800000h
		MOV dword ptr [ESP + 010h],EAX
trailScene_30066c72:
		MOV EAX,dword ptr [ESP + 010h]
		FLD dword ptr [EAX]
		FMUL qword ptr [trailSceneConst300927e8]
		CALL CG_TrailSceneTruncateST0
		MOV EDX,dword ptr [ESP + 024h]
		MOV ECX,dword ptr [ESP + 014h]
		MOV byte ptr [EDX + ECX*01h],AL
		MOV EDX,dword ptr [ESP + 010h]
		INC ECX
		ADD EDX,04h
		CMP ECX,03h
		MOV dword ptr [ESP + 014h],ECX
		MOV dword ptr [ESP + 010h],EDX
		JL trailScene_30066c72
		FLD dword ptr [EBX + TrailScene_alpha]
		FMUL qword ptr [trailSceneConst300927e8]
		CALL CG_TrailSceneTruncateST0
		MOV EDX,dword ptr [ESP + 03ch]
		MOV ECX,dword ptr [ESP + 020h]
		ADD EDX,TrailScenePolySize
		ADD ESI,TrailScenePolySize
		MOV dword ptr [ESP + 03ch],EDX
		MOV EDX,dword ptr [ESP + 038h]
		ADD EDX,TrailScenePolySize
		MOV byte ptr [ECX],AL
		MOV dword ptr [ESP + 038h],EDX
		MOV EDX,dword ptr [ESP + 034h]
		ADD EDX,TrailScenePolySize
		MOV EAX,dword ptr [ESP + 018h]
		MOV dword ptr [ESP + 034h],EDX
		MOV EDX,dword ptr [ESP + 030h]
		ADD EDX,TrailScenePolySize
		INC EAX
		MOV dword ptr [ESP + 030h],EDX
		MOV EDX,dword ptr [ESP + 028h]
		ADD EDX,TrailScenePolySize
		ADD EDI,TrailScenePolySize
		MOV dword ptr [ESP + 028h],EDX
		MOV EDX,dword ptr [ESP + 024h]
		ADD EBP,TrailScenePolySize
		ADD EDX,TrailScenePolySize
		ADD ECX,TrailScenePolySize
		CMP EAX,07fch
		MOV dword ptr [ESP + 018h],EAX
		MOV dword ptr [ESP + 024h],EDX
		MOV dword ptr [ESP + 020h],ECX
		JG trailScene_30066d28
		MOV dword ptr [ESP + 01ch],EBX
		MOV EBX,dword ptr [EBX + TrailScene_nextJunc]
		TEST EBX,EBX
		JNZ trailScene_300665f2
trailScene_30066d28:
		MOV EAX,dword ptr [ESP + 018h]
		MOV EBP,dword ptr [ESP + 098h]
trailScene_30066d33:
		MOV ECX,dword ptr [EBP + TrailScene_flags]
		TEST CL,08h
		JZ trailScene_30066f38
		XOR EBX,EBX
		TEST EAX,EAX
		JLE trailScene_30066ee3
		ADD EAX,03h
		MOV ECX,OFFSET verts + 8
		SHR EAX,02h
		MOV dword ptr [ESP + 018h],ECX
		MOV dword ptr [ESP + 04ch],EAX
		JMP trailScene_30066d62
trailScene_30066d5e:
		MOV ECX,dword ptr [ESP + 018h]
trailScene_30066d62:
		FLD dword ptr [ECX + -08h]
		FLD dword ptr [ECX + -04h]
		FLD dword ptr [ECX]
		FLD dword ptr [ECX + 04h]
		FLD dword ptr [ECX + 08h]
		XOR ESI,ESI
		LEA EDI,[ESP + 050h]
trailScene_30066d76:
		XOR EAX,EAX
		ADD EDI,04h
		MOV AL,byte ptr [ECX + ESI*01h + 0ch]
		INC ESI
		MOV dword ptr [ESP + 010h],EAX
		CMP ESI,04h
		FILD dword ptr [ESP + 010h]
		FSTP dword ptr [EDI + -04h]
		JL trailScene_30066d76
		LEA EAX,[ECX + 014h]
		MOV EDI,03h
trailScene_30066d98:
		FXCH st(4)
		FADD dword ptr [EAX + -04h]
		FXCH st(4)
		FXCH st(3)
		FADD dword ptr [EAX]
		FXCH st(3)
		FXCH st(2)
		FADD dword ptr [EAX + 04h]
		FXCH st(2)
		FXCH
		FADD dword ptr [EAX + 08h]
		FXCH
		FADD dword ptr [EAX + 0ch]
		XOR ECX,ECX
		LEA ESI,[ESP + 050h]
trailScene_30066dbc:
		XOR EDX,EDX
		ADD ESI,04h
		MOV DL,byte ptr [ECX + EAX*01h + 010h]
		INC ECX
		MOV dword ptr [ESP + 010h],EDX
		CMP ECX,04h
		FILD dword ptr [ESP + 010h]
		FADD dword ptr [ESI + -04h]
		FSTP dword ptr [ESI + -04h]
		JL trailScene_30066dbc
		ADD EAX,TrailScenePolySize
		DEC EDI
		JNZ trailScene_30066d98
		FSTP dword ptr [ESP + 080h]
		XOR ESI,ESI
		LEA EDI,[ESP + 050h]
		FSTP dword ptr [ESP + 07ch]
		FSTP dword ptr [ESP + 078h]
		FSTP dword ptr [ESP + 074h]
		FMUL qword ptr [trailSceneConst300923f0]
		FSTP dword ptr [ESP + 070h]
		FLD dword ptr [ESP + 074h]
		FMUL qword ptr [trailSceneConst300923f0]
		FSTP dword ptr [ESP + 074h]
		FLD dword ptr [ESP + 078h]
		FMUL qword ptr [trailSceneConst300923f0]
		FSTP dword ptr [ESP + 078h]
		FLD dword ptr [ESP + 07ch]
		FMUL qword ptr [trailSceneConst300923f0]
		FSTP dword ptr [ESP + 07ch]
		FLD dword ptr [ESP + 080h]
		FMUL qword ptr [trailSceneConst300923f0]
		FSTP dword ptr [ESP + 080h]
trailScene_30066e40:
		FLD dword ptr [EDI]
		FMUL qword ptr [trailSceneConst300923f0]
		CALL CG_TrailSceneTruncateST0
		MOV byte ptr [ESP + ESI*01h + 084h],AL
		INC ESI
		ADD EDI,04h
		CMP ESI,04h
		JL trailScene_30066e40
		MOV ECX,dword ptr [ESP + 018h]
		LEA EAX,[EBX + EBX*02h]
		MOV dword ptr [ESP + 010h],00h
		LEA EAX,[EAX*08h + outVerts]
		LEA EDX,[ECX + 010h]
trailScene_30066e76:
		LEA ESI,[EDX + -TrailScenePolySize]
		MOV ECX,TrailScenePolySize / 4
		MOV EDI,EAX
		ADD EAX,TrailScenePolySize
		REP MOVSD
		MOV ECX,TrailScenePolySize / 4
		LEA ESI,[ESP + 070h]
		MOV EDI,EAX
		INC EBX
		REP MOVSD
		MOV ECX,dword ptr [ESP + 010h]
		INC EBX
		ADD EAX,TrailScenePolySize
		CMP ECX,03h
		MOV ECX,TrailScenePolySize / 4
		JGE trailScene_30066ea9
		MOV ESI,EDX
		JMP trailScene_30066eb0
trailScene_30066ea9:
		MOV ESI,dword ptr [ESP + 018h]
		ADD ESI,-08h
trailScene_30066eb0:
		MOV EDI,EAX
		INC EBX
		REP MOVSD
		MOV ECX,dword ptr [ESP + 010h]
		ADD EAX,TrailScenePolySize
		INC ECX
		ADD EDX,TrailScenePolySize
		CMP ECX,04h
		MOV dword ptr [ESP + 010h],ECX
		JL trailScene_30066e76
		MOV ECX,dword ptr [ESP + 018h]
		MOV EAX,dword ptr [ESP + 04ch]
		ADD ECX,TrailScenePolySize * 4
		DEC EAX
		MOV dword ptr [ESP + 018h],ECX
		MOV dword ptr [ESP + 04ch],EAX
		JNZ trailScene_30066d5e
trailScene_30066ee3:
		TEST byte ptr [EBP + TrailScene_flags],020h
		MOV EAX,055555556h
		JNZ trailScene_30066f0d
		IMUL EBX
		MOV ECX,dword ptr [EBP + TrailScene_shader]
		MOV EAX,EDX
		SHR EAX,01fh
		ADD EDX,EAX
		PUSH EDX
		PUSH OFFSET outVerts
		PUSH 03h
		PUSH ECX
		CALL trap_R_AddPolysToScene
		ADD ESP,010h
		JMP trailScene_30066f84
trailScene_30066f0d:
		IMUL EBX
		MOV EAX,EDX
		SHR EAX,01fh
		ADD EDX,EAX
		TEST EDX,EDX
		JLE trailScene_30066f84
		MOV ESI,OFFSET outVerts
		MOV EDI,EDX
trailScene_30066f21:
		MOV ECX,dword ptr [EBP + TrailScene_shader]
		PUSH ESI
		PUSH 03h
		PUSH ECX
		CALL trap_R_AddPolyToScene
		ADD ESP,0ch
		ADD ESI,TrailScenePolySize * 3
		DEC EDI
		JNZ trailScene_30066f21
		JMP trailScene_30066f84
trailScene_30066f38:
		TEST CL,020h
		CDQ
		JNZ trailScene_30066f5c
		AND EDX,03h
		ADD EAX,EDX
		MOV EDX,dword ptr [EBP + TrailScene_shader]
		SAR EAX,02h
		PUSH EAX
		PUSH OFFSET verts
		PUSH 04h
		PUSH EDX
		CALL trap_R_AddPolysToScene
		ADD ESP,010h
		JMP trailScene_30066f84
trailScene_30066f5c:
		AND EDX,03h
		ADD EAX,EDX
		SAR EAX,02h
		TEST EAX,EAX
		JLE trailScene_30066f84
		MOV ESI,OFFSET verts
		MOV EDI,EAX
trailScene_30066f6f:
		MOV EAX,dword ptr [EBP + TrailScene_shader]
		PUSH ESI
		PUSH 04h
		PUSH EAX
		CALL trap_R_AddPolyToScene
		ADD ESP,0ch
		ADD ESI,TrailScenePolySize * 4
		DEC EDI
		JNZ trailScene_30066f6f
trailScene_30066f84:
		TEST byte ptr [EBP + TrailScene_flags],02h
		JZ trailScene_30066fa9
		MOV EAX,dword ptr [ESP + 09ch]
		CMP EAX,02h
		JGE trailScene_30066fa9
		MOV ECX,dword ptr [ESP + 0a0h]
		INC EAX
		PUSH ECX
		PUSH EAX
		PUSH EBP
		CALL CG_AddTrailToScene
		ADD ESP,0ch
trailScene_30066fa9:
		POP EDI
		POP ESI
		POP EBP
		POP EBX
		ADD ESP,084h
		RET
	}
}
#else

void CG_AddTrailToScene( trailJunc_t *trail, int iteration, int numJuncs )
{
	int		k, i, n, l, numOutVerts;
	polyVert_t	mid;
	float	mod[4];
	float	sInc, s;
	trailJunc_t	*j, *jNext;
	vec3_t	fwd, up, p, v;
	// clipping vars
	#define	TRAIL_FADE_CLOSE_DIST	64.0
	#define	TRAIL_FADE_FAR_SCALE	4.0
	vec3_t	viewProj;
	float	viewDist, fadeAlpha;

	// add spark shader at head position
	if (trail->flags & TJFL_SPARKHEADFLARE) {
		polyBuffer_t* pPolyBuffer = CG_PB_FindFreePolyBuffer( cgs.media.sparkFlareShader, 4, 6 );
		if(pPolyBuffer) {
			int pos = pPolyBuffer->numVerts;

			j = trail;

			VectorCopy( j->pos, pPolyBuffer->xyz[pos] );
			VectorMA (pPolyBuffer->xyz[pos], -j->width*2, vup, pPolyBuffer->xyz[pos]);	
			VectorMA (pPolyBuffer->xyz[pos], -j->width*2, vright, pPolyBuffer->xyz[pos]);	
			pPolyBuffer->st[pos][0] = 0;
			pPolyBuffer->st[pos][1] = 0;
			pos++;
			
			VectorCopy( j->pos, pPolyBuffer->xyz[pos] );
			VectorMA (pPolyBuffer->xyz[pos], -j->width*2, vup, pPolyBuffer->xyz[pos]);	
			VectorMA (pPolyBuffer->xyz[pos], j->width*2, vright, pPolyBuffer->xyz[pos]);	
			pPolyBuffer->st[pos][0] = 0;
			pPolyBuffer->st[pos][1] = 1;
			pos++;

			VectorCopy( j->pos, pPolyBuffer->xyz[pos] );
			VectorMA (pPolyBuffer->xyz[pos], j->width*2, vup, pPolyBuffer->xyz[pos]);	
			VectorMA (pPolyBuffer->xyz[pos], j->width*2, vright, pPolyBuffer->xyz[pos]);	
			pPolyBuffer->st[pos][0] = 1;
			pPolyBuffer->st[pos][1] = 1;
			pos++;

			VectorCopy( j->pos, pPolyBuffer->xyz[pos] );
			VectorMA (pPolyBuffer->xyz[pos], j->width*2, vup, pPolyBuffer->xyz[pos]);	
			VectorMA (pPolyBuffer->xyz[pos], -j->width*2, vright, pPolyBuffer->xyz[pos]);	
			pPolyBuffer->st[pos][0] = 1;
			pPolyBuffer->st[pos][1] = 0;
			pos++;

			for(i = 0; i < 4; i++) {
				pPolyBuffer->color[pPolyBuffer->numVerts + i][0] = 255;
				pPolyBuffer->color[pPolyBuffer->numVerts + i][1] = 255;
				pPolyBuffer->color[pPolyBuffer->numVerts + i][2] = 255;
				pPolyBuffer->color[pPolyBuffer->numVerts + i][3] = (unsigned char)(j->alpha*255.0);
			}

			pPolyBuffer->indicies[pPolyBuffer->numIndicies + 0] = pPolyBuffer->numVerts + 0;
			pPolyBuffer->indicies[pPolyBuffer->numIndicies + 1] = pPolyBuffer->numVerts + 1;
			pPolyBuffer->indicies[pPolyBuffer->numIndicies + 2] = pPolyBuffer->numVerts + 2;

			pPolyBuffer->indicies[pPolyBuffer->numIndicies + 3] = pPolyBuffer->numVerts + 2;
			pPolyBuffer->indicies[pPolyBuffer->numIndicies + 4] = pPolyBuffer->numVerts + 3;
			pPolyBuffer->indicies[pPolyBuffer->numIndicies + 5] = pPolyBuffer->numVerts + 0;

			pPolyBuffer->numVerts += 4;
			pPolyBuffer->numIndicies += 6;
		}
	}

//	if (trail->flags & TJFL_CROSSOVER && iteration < 1) {
//		iteration = 1;
//	}

	sInc = 0;
	
	if (!numJuncs) {
		// first count the number of juncs in the trail
		j = trail;
		numJuncs = 0;
		sInc = 0;
		while (j) {
			numJuncs++;

			// check for a dead next junc
			if (!j->inuse && j->nextJunc && !j->nextJunc->inuse) {
				CG_KillTrail( j );
			} else if (j->nextJunc && j->nextJunc->freed) {
				// not sure how this can happen, but it does, and causes infinite loops
				j->nextJunc = NULL;
			}

			if (j->nextJunc)
				sInc += VectorDistance( j->nextJunc->pos, j->pos );

			j = j->nextJunc;
		}
	}

	if (numJuncs < 2) {
		return;
	}

	s = 0;
	if (trail->sType == STYPE_STRETCH) {
		//sInc = ((1.0 - 0.1) / (float)(numJuncs));	// hack, the end of funnel shows a bit of the start (looping)
		s = 0.05;
		//s = 0.05;
	} else if (trail->sType == STYPE_REPEAT) {
		s = trail->sTex;
	}

	// now traverse the list
	j = trail;
	jNext = j->nextJunc;
	i = 0;
	while (jNext) {

		// first get the directional vectors to the next junc
		VectorSubtract( jNext->pos, j->pos, fwd );
		GetPerpendicularViewVector( cg.refdef_current->vieworg, j->pos, jNext->pos, up );

		// if it's a crossover, draw it twice
		if (j->flags & TJFL_CROSSOVER) {
			if (iteration > 0) {
				ProjectPointOntoVector( cg.refdef_current->vieworg, j->pos, jNext->pos, viewProj );
				VectorSubtract( cg.refdef_current->vieworg, viewProj, v );
				VectorNormalize( v );

				if (iteration == 1) {
					VectorMA( up, 0.3, v, up );
				} else {
					VectorMA( up, -0.3, v, up );
				}
				VectorNormalize( up );
			}
		}
		// do fading when moving towards the projection point onto the trail segment vector
		else if (!(j->flags & TJFL_NOCULL) && (j->widthEnd > 4 || jNext->widthEnd > 4)) {
			ProjectPointOntoVector( cg.refdef_current->vieworg, j->pos, jNext->pos, viewProj );
			viewDist = Distance( viewProj, cg.refdef_current->vieworg );
			if (viewDist < (TRAIL_FADE_CLOSE_DIST * TRAIL_FADE_FAR_SCALE)) {
				if (viewDist < TRAIL_FADE_CLOSE_DIST) {
					fadeAlpha = 0.0;
				} else {
					fadeAlpha = (viewDist - TRAIL_FADE_CLOSE_DIST) / (TRAIL_FADE_CLOSE_DIST * TRAIL_FADE_FAR_SCALE);
				}
				if (fadeAlpha < j->alpha) {
					j->alpha = fadeAlpha;
				}
				if (fadeAlpha < jNext->alpha) {
					jNext->alpha = fadeAlpha;
				}
			}
		}

		// now output the QUAD for this segment

		// 1 ----
		VectorMA( j->pos, 0.5*j->width, up, p );
		VectorCopy( p, verts[i].xyz );
		verts[i].st[0] = s;
		verts[i].st[1] = 1.0;
		for (k=0; k<3; k++)
			verts[i].modulate[k] = (unsigned char)(j->color[k]*255.0);
		verts[i].modulate[3] = (unsigned char)(j->alpha*255.0);

		// blend this with the previous junc
		if (j != trail) {
			VectorAdd( verts[i].xyz, verts[i-1].xyz, verts[i].xyz );
			VectorScale( verts[i].xyz, 0.5, verts[i].xyz );
			VectorCopy( verts[i].xyz, verts[i-1].xyz );
		} else if (j->flags & TJFL_FADEIN) {
			verts[i].modulate[3] = 0;	// fade in
		}

		i++;

		// 2 ----
		VectorMA( p, -1*j->width, up, p );
		VectorCopy( p, verts[i].xyz );
		verts[i].st[0] = s;
		verts[i].st[1] = 0.0;
		for (k=0; k<3; k++)
			verts[i].modulate[k] = (unsigned char)(j->color[k]*255.0);
		verts[i].modulate[3] = (unsigned char)(j->alpha*255.0);

		// blend this with the previous junc
		if (j != trail) {
			VectorAdd( verts[i].xyz, verts[i-3].xyz, verts[i].xyz );
			VectorScale( verts[i].xyz, 0.5, verts[i].xyz );
			VectorCopy( verts[i].xyz, verts[i-3].xyz );
		} else if (j->flags & TJFL_FADEIN) {
			verts[i].modulate[3] = 0;	// fade in
		}

		i++;

		if (trail->sType == STYPE_REPEAT)
			s = jNext->sTex;
		else {
			//s += sInc;
			s += VectorDistance( j->pos, jNext->pos ) / sInc;
			if (s > 1.0) s = 1.0;
		}

		// 3 ----
		VectorMA( jNext->pos, -0.5*jNext->width, up, p );
		VectorCopy( p, verts[i].xyz );
		verts[i].st[0] = s;
		verts[i].st[1] = 0.0;
		for (k=0; k<3; k++)
			verts[i].modulate[k] = (unsigned char)(jNext->color[k]*255.0);
		verts[i].modulate[3] = (unsigned char)(jNext->alpha*255.0);
		i++;

		// 4 ----
		VectorMA( p, jNext->width, up, p );
		VectorCopy( p, verts[i].xyz );
		verts[i].st[0] = s;
		verts[i].st[1] = 1.0;
		for (k=0; k<3; k++)
			verts[i].modulate[k] = (unsigned char)(jNext->color[k]*255.0);
		verts[i].modulate[3] = (unsigned char)(jNext->alpha*255.0);
		i++;

		if (i+4 > MAX_TRAIL_VERTS)
			break;

		j = jNext;
		jNext = j->nextJunc;
	}

	if (trail->flags & TJFL_FIXDISTORT) {
		// build the list of outVerts, by dividing up the QUAD's into 4 Tri's each, so as to allow
		//	any shaped (convex) Quad without bilinear distortion
		for (k=0, numOutVerts=0; k<i; k+=4) {
			VectorCopy(verts[k].xyz, mid.xyz);
			mid.st[0] = verts[k].st[0];
			mid.st[1] = verts[k].st[1];
			for (l=0; l<4; l++) {
				mod[l] = (float)verts[k].modulate[l];
			}
			for (n=1; n<4; n++) {
				VectorAdd( verts[k+n].xyz, mid.xyz, mid.xyz );
				mid.st[0] += verts[k+n].st[0];
				mid.st[1] += verts[k+n].st[1];
				for (l=0; l<4; l++) {
					mod[l] += (float)verts[k+n].modulate[l];
				}
			}
			VectorScale( mid.xyz, 0.25, mid.xyz );
			mid.st[0] *= 0.25;
			mid.st[1] *= 0.25;
			for (l=0; l<4; l++) {
				mid.modulate[l] = (unsigned char)(mod[l]/4.0);
			}

			// now output the tri's
			for (n=0; n<4; n++) {
				outVerts[numOutVerts++] = verts[k+n];
				outVerts[numOutVerts++] = mid;
				if (n<3) {
					outVerts[numOutVerts++] = verts[k+n+1];
				} else {
					outVerts[numOutVerts++] = verts[k];
				}
			}
			
		}

		if (!(trail->flags & TJFL_NOPOLYMERGE)) {
			trap_R_AddPolysToScene( trail->shader, 3, &outVerts[0], numOutVerts/3 );
		} else {
			int k;
			for (k=0; k<numOutVerts/3; k++) {
				trap_R_AddPolyToScene( trail->shader, 3, &outVerts[k*3] );
			}
		}
	}
	else
	{
		// send the polygons
		// FIXME: is it possible to send a GL_STRIP here? We are actually sending 2x the verts we really need to
		if (!(trail->flags & TJFL_NOPOLYMERGE)) {
			trap_R_AddPolysToScene( trail->shader, 4, &verts[0], i/4 );
		} else {
			int k;
			for (k=0; k<i/4; k++) {
				trap_R_AddPolyToScene( trail->shader, 4, &verts[k*4] );
			}
		}
	}

	// do we need to make another pass?
	if (trail->flags & TJFL_CROSSOVER) {
		if (iteration < 2) {
			CG_AddTrailToScene( trail, iteration + 1, numJuncs );
		}
	}

}

#endif

/*
===============
CG_AddTrails
===============
*/
void CG_AddTrails(void)
{
	float	lifeFrac;
	trailJunc_t	*j, *jNext;

	if (!initTrails) {
		CG_ClearTrails();
	}

	//AngleVectors( cg.snap->ps.viewangles, vforward, vright, vup );
	VectorCopy( cg.refdef_current->viewaxis[0], vforward );
	VectorCopy( cg.refdef_current->viewaxis[1], vright );
	VectorCopy( cg.refdef_current->viewaxis[2], vup );

	// update the settings for each junc
	j = activeTrails;
	while (j) {
		lifeFrac = (float)(cg.time - j->spawnTime) / (float)(j->endTime - j->spawnTime);
		if (lifeFrac >= 1.0) {
			j->inuse = qfalse;			// flag it as dead
			j->width = j->widthEnd;
			j->alpha = j->alphaEnd;
			if (j->alpha > 1.0)	j->alpha = 1.0;
			else if (j->alpha < 0.0) j->alpha = 0.0;
			VectorCopy( j->colorEnd, j->color );
		} else {
			j->width = j->widthStart + (j->widthEnd - j->widthStart) * lifeFrac;
			j->alpha = j->alphaStart + (j->alphaEnd - j->alphaStart) * lifeFrac;
			if (j->alpha > 1.0) j->alpha = 1.0;
			else if (j->alpha < 0.0) j->alpha = 0.0;
			VectorSubtract( j->colorEnd, j->colorStart, j->color );
			VectorMA( j->colorStart, lifeFrac, j->color, j->color );
		}

		j = j->nextGlobal;
	}

	// draw the trailHeads
	j = headTrails;
	while (j) {
		jNext = j->nextHead;		// in case it gets removed
		if (!j->inuse) {
			CG_FreeTrailJunc(j);
		} else {
			CG_AddTrailToScene( j, 0, 0 );
		}
		j = jNext;
	}
}
