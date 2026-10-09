// cg_event.c -- handle entity events at snapshot or playerstate transitions

#include "cg_local.h"
#include "../game/tce_bg.h"
#include "tce_weapon_media.h"
#include "tce_flash.h"
#include "tce_smoke_grenade.h"
#include "tce_fragment_sound.h"

extern void CG_StartShakeCamera( float param );
extern void CG_ToggleAiming(void);
extern void CG_Tracer( vec3_t source, vec3_t dest, int sparks );
extern void CG_AddBulletParticles( vec3_t origin, vec3_t dir, int speed, int duration, int count, float randScale );
#if defined(_MSC_VER) && defined(_M_IX86)
/* TC 30088628: ST0 to signed64, with the caller consuming EAX. */
static __declspec(naked) void CG_EventTruncateST0(void) {
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
#endif

#if defined(_MSC_VER) && defined(_M_IX86)
/* Extracted Windows event expression; native helper, not an original body. */
static int CG_EventGlobalSoundVolume(void) {
	static const float globalSoundScale = 127.0f;
	float *globalSoundDeafness = &tceFlash.deafness;
	int globalSoundVolume;
	__asm {
		fld1
		mov eax, globalSoundDeafness
		fsub dword ptr [eax]
		fmul globalSoundScale
		call CG_EventTruncateST0
		mov globalSoundVolume, eax
	}
	return globalSoundVolume;
}

/* Shared expression from TC30038bb5 and30038ce9; not a separate original body. */
static int CG_EventFiveSoundChoice(void) {
	int soundRandom = rand() & 0x7fff;
	int soundChoice;
	float soundSample;
	static const float soundRandomScale = 3.0518509447574615e-05f;
	static const float soundFive = 5.0f, soundOne = 1.0f;
	static const float soundTwo = 2.0f, soundThree = 3.0f, soundFour = 4.0f;
	__asm {
		fild soundRandom
		fmul soundRandomScale
		fmul soundFive
		fst soundSample
		fcomp soundOne
		fnstsw ax
		test ah, 1
		jnz soundChoiceZero
		fld soundSample
		fcomp soundTwo
		fnstsw ax
		test ah, 1
		jnz soundChoiceOne
		fld soundSample
		fcomp soundThree
		fnstsw ax
		test ah, 1
		jnz soundChoiceTwo
		fld soundSample
		fcomp soundFour
		fnstsw ax
		test ah, 1
		jnz soundChoiceThree
		mov soundChoice, 4
		jmp soundChoiceDone
	soundChoiceZero:
		mov soundChoice, 0
		jmp soundChoiceDone
	soundChoiceOne:
		mov soundChoice, 1
		jmp soundChoiceDone
	soundChoiceTwo:
		mov soundChoice, 2
		jmp soundChoiceDone
	soundChoiceThree:
		mov soundChoice, 3
	soundChoiceDone:
	}
	return soundChoice;
}
#endif
//==========================================================================

/*
=============
CG_Obituary
=============
*/
/* Windows 3003aa50: event density carries the TC weapon media slot. */
static void CG_Obituary( entityState_t *ent ) {
    int target=ent->otherEntityNum, attacker=ent->otherEntityNum2;
    int mod=ent->eventParm, weapon=ent->density;
    char targetName[32], attackerName[32];
    const char *message=NULL, *weaponText="";
    clientInfo_t *ci, *ca;
    qhandle_t shader=cgs.media.pmImages[PM_DEATH];
    qboolean publicMessage;
    if(target<0 || target>=MAX_CLIENTS) CG_Error("CG_Obituary: target out of range");
    ci=&cgs.clientinfo[target];
    if(attacker<0 || attacker>=MAX_CLIENTS) { attacker=ENTITYNUM_WORLD; ca=NULL; }
    else ca=&cgs.clientinfo[attacker];
    if(!cgs.tceKillMessage && target!=cg.snap->ps.clientNum && cgs.gametype==5) return;
    Q_strncpyz(targetName,ci->name,30);
    Q_strncpyz(targetName,va("%c%c%s",'^',ci->team==2?'4':ci->team==1?'1':'7',targetName),32);
    publicMessage=cgs.gametype==7 || cgs.gametype==2 || (cgs.gametype==5 && cgs.tceKillMessage>0);
    if(publicMessage) {
        switch(mod) {
        case 31: message="drowned"; break;
        case 32: message="died by toxic materials"; break;
        case 33: message="was incinerated"; break;
        case 34: message="was crushed"; break;
        case 35: case 38: case 39: message="was killed"; break;
        case 36: message="fell to his death"; break;
        case 37: message="committed suicide"; break;
        case 54: message="got buried under a pile of rubble"; break;
        case 65: message="^2violated ROE"; break;
        }
        if(attacker==target) {
            switch(mod) {
            case 17: message="vaporized himself"; break;
            case 18: message="dove on his own grenade"; break;
            case 26: message="dynamited himself to pieces"; break;
            case 27: message="obliterated himself"; break;
            case 30: message="fired-for-effect on himself"; break;
            case 40: message="died in his own explosion"; break;
            case 64: return;
            case 65: message="^2violated ROE"; break;
            default: message="killed himself"; break;
            }
        }
        if(message) {
            if(target!=cg.snap->ps.clientNum) {
                CG_AddPMItem(PM_DEATH,va("%s %s.",targetName,message),shader); return;
            }
            if(mod==65) CG_AddPMItem(PM_DEATH,va("%s %s.",targetName,message),shader);
        }
        if(attacker==cg.snap->ps.clientNum && target!=cg.snap->ps.clientNum) {
            message=ci->team==ca->team ? (mod==63?"You swapped places with":"You killed TEAMMATE") : "You killed";
            CG_AddPMItemBig((popupMessageBigType_t)PM_DEATH,va("%s %s",CG_TranslateString(message),targetName),shader);
        }
    }
    if(!ca) strcpy(attackerName,"noname");
    else {
        Q_strncpyz(attackerName,ca->name,30);
        if(target==cg.snap->ps.clientNum) Q_strncpyz(cg.killerName,attackerName,sizeof(cg.killerName));
    }
    Q_strncpyz(attackerName,va("%c%c%s",'^',ca?(ca->team==2?'4':ca->team==1?'1':'7'):'7',attackerName),32);
    if(mod==18 || mod==20) weaponText=va("(%s)","GRENADE");
    else if(mod==26) weaponText=va("(%s)","C4");
    else if(weapon) weaponText=va("(%s)",tce_cg_weapons[weapon].deployMenuShortName);
    if(publicMessage && ca && target!=cg.snap->ps.clientNum) {
        message="<<<";
        if(ci->team==ca->team) { message="was killed by TEAMMATE"; weaponText=""; }
        CG_AddPMItem(PM_DEATH,va("%s %s %s %s",targetName,message,attackerName,weaponText),shader);
        return;
    }
    message=ca && ci->team==ca->team?"You were killed by TEAMMATE":"You were killed by";
    if(mod==65) message="^2You violated ROE";
    else if(attacker!=target && attacker!=ENTITYNUM_WORLD) {
        CG_AddPMItemBig((popupMessageBigType_t)PM_DEATH,va("%s %s %s",message,attackerName,weaponText),shader);
        return;
    } else message="You killed yourself";
    CG_AddPMItemBig((popupMessageBigType_t)PM_DEATH,va("%s",message),shader);
}

//==========================================================================

// from cg_weapons.c
extern int CG_WeaponIndex( int weapnum, int *bank, int *cycle);


/*
================
CG_ItemPickup

A new item was picked up this frame
================
*/
/* Original3003b140: every weapon pickup selects its TC media slot. */
static void CG_ItemPickup( int itemNum ) {
    int weapon = bg_itemlist[itemNum].giTag;
    if (bg_itemlist[itemNum].giType == IT_WEAPON) {
        CG_AddPMItemBig((popupMessageBigType_t)PM_MESSAGE,
            va("Picked up a %s", tce_cg_weapons[weapon].deployMenuShortName),
            cgs.media.pmImages[PM_MESSAGE]);
        cg.weaponSelectTime = cg.time;
        cg.weaponSelect = weapon;
    } else {
        CG_AddPMItemBig((popupMessageBigType_t)PM_MESSAGE,
            va("Picked up %s", CG_PickupItemText(itemNum)),
            cgs.media.pmImages[PM_MESSAGE]);
    }
}


/*
================
CG_PainEvent

Also called by playerstate transition
================
*/
typedef struct {
	char *tag;
	int refEntOfs;
	int anim;
} painAnimForTag_t;

#define	PEFOFS(x) ((int)&(((playerEntity_t *)0)->x))

void CG_PainEvent( centity_t *cent, int health, qboolean crouching ) {
	char	*snd;

	// don't do more than two pain sounds a second
	if ( cg.time - cent->pe.painTime < 500 ) {
		return;
	}

	if ( health < 25 ) {
		snd = "*pain25_1.wav";
	} else if ( health < 50 ) {
		snd = "*pain50_1.wav";
	} else if ( health < 75 ) {
		snd = "*pain75_1.wav";
	} else {
		snd = "*pain100_1.wav";
	}
	trap_S_StartSound( NULL, cent->currentState.number, CHAN_VOICE, 
		CG_CustomSound( cent->currentState.number, snd ) );

	// save pain time for programitic twitch animation
	cent->pe.painTime = cg.time;
	cent->pe.painDirection ^= 1;
}





/*
==============
CG_Explode


	if (cent->currentState.angles2[0] || cent->currentState.angles2[1] || cent->currentState.angles2[2])

==============
*/

#define POSSIBLE_PIECES 6

typedef struct fxSound_s {
	int max;
	qhandle_t sound[3];
	const char *soundfile[3];
} fxSound_t;

static fxSound_t fxSounds[POSSIBLE_PIECES] = {
	// wood
	{ 1, { -1, -1, -1 }, { "sound/world/boardbreak.wav", NULL, NULL } },
	// glass
	{ 3, { -1, -1, -1 }, { "sound/world/glassbreak1.wav", "sound/world/glassbreak2.wav", "sound/world/glassbreak3.wav" } },
	// metal
	{ 1, { -1, -1, -1 }, { "sound/world/metalbreak.wav", NULL, NULL } },
	// gibs
	{ 1, { -1, -1, -1 }, { "sound/world/gibsplit1.wav", NULL, NULL } },
	// brick
	{ 1, { -1, -1, -1 }, { "sound/world/debris1.wav", NULL, NULL } },
	// stone
	{ 1, { -1, -1, -1 }, { "sound/world/stonefall.wav", NULL, NULL } }
};

void CG_PrecacheFXSounds( void ) {
	int i, j;

	for( i = 0; i < POSSIBLE_PIECES; i++ ) {
		for( j = 0; j < fxSounds[i].max; j++ ) {
			fxSounds[i].sound[j] = trap_S_RegisterSound( fxSounds[i].soundfile[j], qfalse );
		}
	}
}

void CG_Explodef(vec3_t origin, vec3_t dir, int mass, int type, qhandle_t sound, int forceLowGrav, qhandle_t shader);
void CG_RubbleFx(vec3_t origin, vec3_t dir, int mass, int type, qhandle_t sound, int forceLowGrav, qhandle_t shader, float speedscale, float sizescale);

/*
==============
CG_Explode
	the old cent-based explode calls will still work with this pass-through
==============
*/
void CG_Explode(centity_t *cent, vec3_t origin, vec3_t dir, qhandle_t shader) {

	qhandle_t		inheritmodel = 0;

	// inherit shader
	// (SA) FIXME: do this at spawn time rather than explode time so any new necessary shaders are created earlier
	if(cent->currentState.eFlags & EF_INHERITSHADER) {
		if(!shader) {
//			inheritmodel = cent->currentState.modelindex;
			inheritmodel = cgs.inlineDrawModel[cent->currentState.modelindex];	// okay, this should be better.
			if(inheritmodel)
				shader = trap_R_GetShaderFromModel(inheritmodel, 0, 0);
		}
	}

	if( !cent->currentState.dl_intensity ) {
		sfxHandle_t sound;

		/* Original FILD(rand&32767), FMUL binary32 reciprocal, FIMUL count.
		 * Keep the product unrounded until conversion to the sound index. */
		sound = (int)((double)(rand() & 0x7fff) *
			(double)(1.0f / 32767.0f) * fxSounds[cent->currentState.frame].max);

		if( fxSounds[cent->currentState.frame].sound[sound] == -1 ) {
			fxSounds[cent->currentState.frame].sound[sound] = trap_S_RegisterSound( fxSounds[cent->currentState.frame].soundfile[sound], qfalse );
		}

		sound = fxSounds[cent->currentState.frame].sound[sound];

		CG_Explodef(	origin, 
						dir, 
						cent->currentState.density,			// mass
						cent->currentState.frame,			// type
						sound,								// sound
						cent->currentState.weapon,			// forceLowGrav
						shader
						);
	} else {
		sfxHandle_t sound;

		if( cent->currentState.dl_intensity == -1 ) {
			sound = 0;
		} else {
			sound = cgs.gameSounds[cent->currentState.dl_intensity];
		}

		CG_Explodef(	origin, 
						dir, 
						cent->currentState.density,			// mass
						cent->currentState.frame,			// type
						sound,								// sound
						cent->currentState.weapon,			// forceLowGrav
						shader
						);
	}

}

/*
==============
CG_Explode
	the old cent-based explode calls will still work with this pass-through
==============
*/
void CG_Rubble(centity_t *cent, vec3_t origin, vec3_t dir, qhandle_t shader) {

	qhandle_t		inheritmodel = 0;

	// inherit shader
	// (SA) FIXME: do this at spawn time rather than explode time so any new necessary shaders are created earlier
	if(cent->currentState.eFlags & EF_INHERITSHADER) {
		if(!shader) {
//			inheritmodel = cent->currentState.modelindex;
			inheritmodel = cgs.inlineDrawModel[cent->currentState.modelindex];	// okay, this should be better.
			if(inheritmodel)
				shader = trap_R_GetShaderFromModel(inheritmodel, 0, 0);
		}
	}

	if( !cent->currentState.dl_intensity ) {
		sfxHandle_t sound;

		sound = random()*fxSounds[cent->currentState.frame].max;

		if( fxSounds[cent->currentState.frame].sound[sound] == -1 ) {
			fxSounds[cent->currentState.frame].sound[sound] = trap_S_RegisterSound( fxSounds[cent->currentState.frame].soundfile[sound], qfalse );
		}

		sound = fxSounds[cent->currentState.frame].sound[sound];

		CG_RubbleFx(	origin, 
						dir, 
						cent->currentState.density,			// mass
						cent->currentState.frame,			// type
						sound,								// sound
						cent->currentState.weapon,			// forceLowGrav
						shader,
						cent->currentState.angles2[0],
						cent->currentState.angles2[1]
						);
	} else {
		sfxHandle_t sound;

		if( cent->currentState.dl_intensity == -1 ) {
			sound = 0;
		} else {
			sound = cgs.gameSounds[cent->currentState.dl_intensity];
		}

		CG_RubbleFx(	origin, 
						dir, 
						cent->currentState.density,			// mass
						cent->currentState.frame,			// type
						sound,								// sound
						cent->currentState.weapon,			// forceLowGrav
						shader,
						cent->currentState.angles2[0],
						cent->currentState.angles2[1]
						);
	}
}


/*
==============
CG_RubbleFx
==============
*/
void CG_RubbleFx(vec3_t origin, vec3_t dir, int mass, int type, sfxHandle_t sound, int forceLowGrav, qhandle_t shader, float speedscale, float sizescale) {
	int i;
	localEntity_t	*le;
	refEntity_t		*re;
	int				howmany, total, totalsounds;
	int				pieces[6];	// how many of each piece
	qhandle_t		modelshader = 0;
	float			materialmul = 1;	// multiplier for different types
	
	memset(&pieces, 0, sizeof(pieces));
	
	pieces[5]	= (int)(mass / 250.0f);
	pieces[4]	= (int)(mass / 76.0f);
	pieces[3]	= (int)(mass / 37.0f);	// so 2 per 75
	pieces[2]	= (int)(mass / 15.0f);
	pieces[1]	= (int)(mass / 10.0f);
	pieces[0]	= (int)(mass / 5.0f);
	
	if(pieces[0] > 20)	pieces[0] = 20;	// cap some of the smaller bits so they don't get out of control
	if(pieces[1] > 15)	pieces[1] = 15;
	if(pieces[2] > 10)	pieces[2] = 10;
	
	if(type == 0 ) {	// cap wood even more since it's often grouped, and the small splinters can add up
		if(pieces[0] > 10)	pieces[0] = 10;
		if(pieces[1] > 10)	pieces[1] = 10;
		if(pieces[2] > 10)	pieces[2] = 10;
	}
	
	totalsounds = 0;
	total = pieces[5] + pieces[4] + pieces[3] + pieces[2] + pieces[1] + pieces[0];
	
	if(sound && !cg.tcePortalScopeRendering) {
		tce_fragmentSoundContext_t context;
		int volume;
		memset(&context, 0, sizeof(context));
		VectorCopy(cg.refdef_current->vieworg, context.listener);
		context.attenuation = tceFlash.deafness;
		context.distanceVariant = tceSmokeNewBBox;
		volume = TCE_CG_SoundVolume(origin, 127.0f, 1200.0f, 0, &context);
		trap_S_StartSoundVControl(origin, -1, CHAN_AUTO, sound, volume);
	}
	
	if(shader)	// shader passed in to use
		modelshader = shader;
	
	for(i=0;i<POSSIBLE_PIECES;i++) {
		leBounceSoundType_t snd = LEBS_NONE;
		int		hmodel = 0;
		float	scale;
		int		endtime;
		for(howmany = 0; howmany < pieces[i]; howmany++) {
			
			scale = 1.0f;
			endtime = 0;	// set endtime offset for faster/slower fadeouts
			
			switch(type) {
			case 0:	// "wood"
				snd = LEBS_WOOD;
				hmodel = cgs.media.debWood[i];
				
				if(i==0)		scale = 0.5f;
				else if(i==1)	scale = 0.6f;
				else if(i==2)	scale = 0.7f;
				else if(i==3)	scale = 0.5f;
				//					else goto pass;
				
				if(i<3)
					endtime = -3000;	// small bits live 3 sec shorter than normal
				break;
				
			case 1:	// "glass"
				// TC bounce bank 6 is glass (the SDK enum calls it BONE).
				snd = (leBounceSoundType_t)6;
				if(i==5)			hmodel = cgs.media.shardGlass1;
				else if(i==4)		hmodel = cgs.media.shardGlass2;
				else if(i==2)		hmodel = cgs.media.shardGlass2;
				else if(i==1) {
					hmodel = cgs.media.shardGlass2;
					scale = 0.5f;
				}
				else	goto pass;
				break;
				
			case 2:	// "metal"
				snd = LEBS_METAL;
				if(i==5)			hmodel = cgs.media.shardMetal1;
				else if(i==4)		hmodel = cgs.media.shardMetal2;
				else if(i==2)		hmodel = cgs.media.shardMetal2;
				else if(i==1) {
					hmodel = cgs.media.shardMetal2;
					scale = 0.5f;
				}
				else	goto pass;
				break;
				
			case 3:	// "gibs"
				snd = LEBS_BLOOD;
				if(i==5)			hmodel = cgs.media.gibIntestine;
				else if(i==4)		hmodel = cgs.media.gibLeg;
				else if(i==2)		hmodel = cgs.media.gibChest;
				else	goto pass;
				break;
				
			case 4:	// "brick"
				snd = LEBS_ROCK;
				hmodel = cgs.media.debBlock[i];
				break;
				
			case 5:	// "rock"
				snd = LEBS_ROCK;
				if(i==5)			hmodel = cgs.media.debRock[2];	// temporarily use the next smallest rock piece
				else if(i==4)		hmodel = cgs.media.debRock[2];
				else if(i==3)		hmodel = cgs.media.debRock[1];
				else if(i==2)		hmodel = cgs.media.debRock[0];
				else if(i==1)		hmodel = cgs.media.debBlock[1];	// temporarily use the small block pieces
				else				hmodel = cgs.media.debBlock[0];	// temporarily use the small block pieces
				
				if(i<=2)
					endtime = -2000;	// small bits live 2 sec shorter than normal
				break;
				
			case 6:	// "fabric"
				if(i==5)			hmodel = cgs.media.debFabric[0];
				else if(i==4)		hmodel = cgs.media.debFabric[1];
				else if(i==2)		hmodel = cgs.media.debFabric[2];
				
				else if(i==1) {
					hmodel = cgs.media.debFabric[2];
					scale = 0.5;
				}
				
				else	goto pass;	// (only do 5, 4, 2 and 1)
				break;
			}
			
			le = CG_AllocLocalEntity();
			re = &le->refEntity;
			
			le->leType				= LE_FRAGMENT;
			le->startTime			= cg.time;
			
			le->endTime				= (le->startTime + 5000 + random() * 5000) + endtime;
			
			// as it turns out, i'm not sure if setting the re->axis here will actually do anything
			//			AxisClear(re->axis);
			//			re->axis[0][0] = 
			//			re->axis[1][1] = 
			//			re->axis[2][2] = scale;
			//
			//			if(scale != 1.0)
			//				re->nonNormalizedAxes = qtrue;
			
			le->sizeScale = scale * sizescale;
			
			if(type == 1) {	// glass
				// Rafael added this because glass looks funky when it fades out
				// TBD: need to look into this so that they fade out correctly
				re->fadeStartTime		= le->endTime;
				re->fadeEndTime			= le->endTime;
			}
			else {
				re->fadeStartTime		= le->endTime - 4000;
				re->fadeEndTime			= le->endTime;
			}
			
			if( total > 5 ) {
				if( totalsounds > 5 || (howmany % 8) != 0 )
					snd = LEBS_NONE;
				else
					totalsounds++;
			}
			
			le->lifeRate	= 1.0/(le->endTime - le->startTime);
			le->leFlags		= LEF_TUMBLE;
			le->leMarkType	= 0;
			
			VectorCopy( origin, re->origin );
			AxisCopy( axisDefault, re->axis );
			
			le->leBounceSoundType = snd;
			re->hModel = hmodel;
			
			// inherit shader
			if(modelshader) {
				re->customShader = modelshader;
			}
			
			re->radius = 1000;
			
			// trying to make this a little more interesting
			if(type == 6) {	// "fabric"
				le->pos.trType = TR_GRAVITY_FLOAT;	// the fabric stuff will change to use something that looks better
			}
			else {
				if(! forceLowGrav && rand()&1)		// if low gravity is not forced and die roll goes our way use regular grav
					le->pos.trType = TR_GRAVITY;
				else
					le->pos.trType = TR_GRAVITY_LOW;
			}
			
			switch(type) {
			case 6:	// fabric
				le->bounceFactor	= 0.0;
				materialmul			= 0.3;	// rotation speed
				break;
			default:
				le->bounceFactor	= 0.4;
				break;
			}
			
			
			// rotation
			le->angles.trType = TR_LINEAR;
			le->angles.trTime = cg.time;
			le->angles.trBase[0] = rand()&31;
			le->angles.trBase[1] = rand()&31;
			le->angles.trBase[2] = rand()&31;
			le->angles.trDelta[0] = ((100 + (rand()&500)) - 300) * materialmul;
			le->angles.trDelta[1] = ((100 + (rand()&500)) - 300) * materialmul;
			le->angles.trDelta[2] = ((100 + (rand()&500)) - 300) * materialmul;
			
			
			//			if(type == 6)	// fabric
			//				materialmul = 1;		// translation speed
			
			
			VectorCopy( origin, le->pos.trBase );
			VectorNormalize(dir);
			le->pos.trTime = cg.time;
			
			// (SA) hoping that was just intended to represent randomness
			//			if (cent->currentState.angles2[0] || cent->currentState.angles2[1] || cent->currentState.angles2[2])
			if (le->angles.trBase[0] == 1 || le->angles.trBase[1] == 1 || le->angles.trBase[2] == 1 ) {
				le->pos.trType = TR_GRAVITY;
				VectorScale(dir, 10 * 8, le->pos.trDelta);
				le->pos.trDelta[0] += ((random() * 400) - 200) * speedscale;
				le->pos.trDelta[1] += ((random() * 400) - 200) * speedscale;
				le->pos.trDelta[2] = ((random() * 400) + 400) * speedscale;

			} else {
				// location
				VectorScale(dir, 200 + mass, le->pos.trDelta);
				le->pos.trDelta[0] += ((random() * 200) - 100);
				le->pos.trDelta[1] += ((random() * 200) - 100);

				if(dir[2])
					le->pos.trDelta[2] = random() * 200 * materialmul;	// randomize sort of a lot so they don't all land together
				else
					le->pos.trDelta[2] = random() * 20;
			}
		}
pass:
		continue;
	}

}

/*
==============
CG_Explodef
	made this more generic for spawning hits and breaks without needing a *cent
==============
*/
/* Whole TC emitter Windows300367c0 / Linux0006882e. */
void CG_Explodef(vec3_t origin, vec3_t dir, int mass, int type, qhandle_t sound,
                 int forceLowGrav, qhandle_t shader) {
    int pieces[6], i, j, total, totalSounds=0;
    float translation=1.0f, rotation=1.0f;
    pieces[5]=(int)(mass*0.004000000189989805f);
    pieces[4]=(int)(mass*0.01315789483487606f);
    pieces[3]=(int)(mass*0.027027027681469917f);
    pieces[2]=(int)(mass*0.06666667014360428f);
    pieces[1]=(int)(mass*0.10000000149011612f);
    pieces[0]=(int)(mass*0.20000000298023224f);
    if(pieces[0]>20)pieces[0]=20;
    if(pieces[1]>15)pieces[1]=15;
    if(pieces[2]>10)pieces[2]=10;
    if(type==0)for(i=0;i<3;i++)if(pieces[i]>10)pieces[i]=10;
    total=pieces[0]+pieces[1]+pieces[2]+pieces[3]+pieces[4]+pieces[5];
    if(sound && !cg.tcePortalScopeRendering) {
        tce_fragmentSoundContext_t context;
        int volume;
        memset(&context,0,sizeof(context));
        VectorCopy(cg.refdef_current->vieworg,context.listener);
        context.attenuation=tceFlash.deafness;
        context.distanceVariant=tceSmokeNewBBox;
        context.disabled=cg.tcePortalScopeRendering;
        volume=TCE_CG_SoundVolume(origin,127.0f,1200.0f,0,&context);
        trap_S_StartSoundVControl(origin,-1,CHAN_AUTO,sound,volume);
    }
    for(i=0;i<6;i++) {
        int bounceSound=0;
        qhandle_t model=0;
        for(j=0;j<pieces[i];j++) {
            localEntity_t *le;
            refEntity_t *re;
            float scale=1.0f, speed;
            int timeOffset=0,k;
            switch(type) {
            case 0:
                bounceSound=3;model=cgs.media.debWood[i];
                scale=.2f;translation=.3f;
                if(i<3)timeOffset=-3000;
                break;
            case 1:
                bounceSound=6;translation=.3f;
                if(i==5)model=cgs.media.shardGlass1;
                else if(i==4 || i==2)model=cgs.media.shardGlass2;
                else if(i==1){model=cgs.media.shardGlass2;scale=.5f;}
                else goto nextSize;
                break;
            case 2:
                bounceSound=4;
                if(i==5)model=cgs.media.shardMetal1;
                else if(i==4 || i==2)model=cgs.media.shardMetal2;
                else if(i==1){model=cgs.media.shardMetal2;scale=.5f;}
                else goto nextSize;
                break;
            case 3:
                bounceSound=1;
                if(i==5)model=cgs.media.gibIntestine;
                else if(i==4)model=cgs.media.gibLeg;
                else if(i==2)model=cgs.media.gibChest;
                else goto nextSize;
                break;
            case 4:bounceSound=2;model=cgs.media.debBlock[i];break;
            case 5:
                bounceSound=2;
                if(i>=4)model=cgs.media.debRock[2];
                else if(i==3)model=cgs.media.debRock[1];
                else if(i==2)model=cgs.media.debRock[0];
                else model=cgs.media.debBlock[i];
                if(i<=2)timeOffset=-2000;
                break;
            case 6:
                if(i==5)model=cgs.media.debFabric[0];
                else if(i==4)model=cgs.media.debFabric[1];
                else if(i==2)model=cgs.media.debFabric[2];
                else if(i==1){model=cgs.media.debFabric[2];scale=.5f;}
                else goto nextSize;
                break;
            case 7:case 8:
                model=cgs.media.tceSplinterModel;
                scale=i==5?2.0f:.5f+.25f*i;
                if(i<3)timeOffset=-3000;
                break;
            }
            le=CG_AllocLocalEntity();re=&le->refEntity;
            le->leType=LE_FRAGMENT;le->startTime=cg.time;
            le->endTime=(int)(cg.time+5000+(rand()&32767)*(1.0f/32767.0f)*5000.0+timeOffset);
            le->sizeScale=scale;
            re->fadeStartTime=le->endTime-(type==1?0:4000);
            re->fadeEndTime=le->endTime;
            if(total>5) {if(totalSounds>5)bounceSound=0;else totalSounds++;}
            le->leFlags=LEF_TUMBLE;le->leMarkType=LEMT_NONE;
            le->lifeRate=1.0f/(le->endTime-le->startTime);
            VectorCopy(origin,re->origin);AxisCopy(axisDefault,re->axis);
            le->leBounceSoundType=(leBounceSoundType_t)bounceSound;
            re->hModel=model;if(shader)re->customShader=shader;
            re->radius=1000;
            le->angles.trType=TR_LINEAR;
            if(type==6)le->pos.trType=TR_GRAVITY_FLOAT;
            else if(type==7) {
                le->pos.trType=(trType_t)15;
                le->pos.trDuration=(int)(((rand()&32767)*(1.0f/32767.0f)+1)*200);
                le->angles.trType=TR_DECCELERATE;le->angles.trDuration=5000;
                le->tceGravity=(float)(i*.3333333432674408f+
                    (le->pos.trDuration*.004999999888241291f-1)*.5f+1);
            } else if(!forceLowGrav && (rand()&1))le->pos.trType=TR_GRAVITY;
            else le->pos.trType=TR_GRAVITY_LOW;
            if(type==6){le->bounceFactor=0;rotation=.3f;}
            else if(type==7){le->bounceFactor=0;rotation=1;translation=.7f;}
            else if(type==8){translation=.1f;le->pos.trType=TR_GRAVITY_FLOAT;}
            else le->bounceFactor=.4f;
            le->angles.trTime=cg.time;
            for(k=0;k<3;k++)le->angles.trBase[k]=rand()&31;
            for(k=0;k<3;k++)le->angles.trDelta[k]=((rand()&500)-200)*rotation;
            VectorCopy(origin,le->pos.trBase);VectorNormalize(dir);
            le->pos.trTime=cg.time;
            for(k=0;k<3;k++)le->pos.trDelta[k]=
                ((rand()&32767)*(1.0f/32767.0f)*2-1)+dir[k];
            speed=(mass+200)*translation;
            VectorScale(le->pos.trDelta,speed,le->pos.trDelta);
        }
nextSize:;
    }
}


/*
==============
CG_Effect
	Quake ed -> target_effect (0 .5 .8) (-6 -6 -6) (6 6 6) fire explode smoke debris gore lowgrav
==============
*/
void CG_Effect(centity_t *cent, vec3_t origin, vec3_t dir)
{
	localEntity_t	*le;
	refEntity_t		*re;
//	int				howmany;
	int				mass;
//	int				large, small;
	vec4_t			projection, color;
	

	VectorSet(dir, 0, 0, 1);	// straight up.

	mass = cent->currentState.density;

//		1 large per 100, 1 small per 24
//	large	= (int)(mass / 100);
//	small	= (int)(mass / 24) + 1;

	if(cent->currentState.eventParm & 1) {	// fire
		CG_MissileHitWall( WP_DYNAMITE, 0, origin, dir, 0 );
		return;
	}

	// (SA) right now force smoke on any explosions
//	if(cent->currentState.eventParm & 4)	// smoke
	if(cent->currentState.eventParm & 7)
	{
		int i, j;
		vec3_t sprVel, sprOrg;
		// explosion sprite animation
		VectorScale( dir, 16, sprVel );
		for (i=0; i<5; i++) {
			for (j=0;j<3;j++)
				sprOrg[j] = origin[j] + 64*dir[j] + 24*crandom();
			sprVel[2] += rand()%50;
//			CG_ParticleExplosion( 2, sprOrg, sprVel, 1000+rand()%250, 20, 40+rand()%60 );
			CG_ParticleExplosion( "blacksmokeanim", sprOrg, sprVel, 3500+rand()%250, 10, 250+rand()%60, qfalse ); // JPW NERVE was smokeanimb
		}
	}


	if(cent->currentState.eventParm & 2)	// explode
	{
		vec3_t sprVel, sprOrg;
		trap_S_StartSound( origin, -1, CHAN_AUTO, cgs.media.sfx_rockexp );

		// new explode	(from rl)
		VectorMA( origin, 16, dir, sprOrg );
		VectorScale( dir, 100, sprVel );
		CG_ParticleExplosion( "explode1", sprOrg, sprVel, 500, 20, 160, qtrue );
		//CG_ParticleExplosion( "blueexp", sprOrg, sprVel, 1200, 9, 300 );

	// (SA) this is done only if the level designer has it marked in the entity.
	//		(see "cent->currentState.eventParm & 64" below)

		// RF, throw some debris
//		CG_AddDebris( origin, dir,
//						280,	// speed
//						1400,	// duration
//						// 15 + rand()%5 );	// count
//						7 + rand()%2 );	// count

		//%	CG_ImpactMark( cgs.media.burnMarkShader, origin, dir, random()*360, 1,1,1,1, qfalse, 64, qfalse, 0xffffffff );
		VectorSet( projection, 0, 0, -1 );
		projection[ 3 ] = 64.0f;
		Vector4Set( color, 1.0f, 1.0f, 1.0f, 1.0f );
		trap_R_ProjectDecal( cgs.media.burnMarkShader, 1, (vec3_t*) origin, projection, color, cg_markTime.integer, (cg_markTime.integer >> 4) );
	}


	if(cent->currentState.eventParm & 8) { // rubble
	// share the cg_explode code with func_explosives
		const char *s;
		qhandle_t	sh = 0;	// shader handle

		vec3_t newdir = {0, 0, 0};

		if (cent->currentState.angles2[0] || cent->currentState.angles2[1] || cent->currentState.angles2[2])
		{
			VectorCopy (cent->currentState.angles2, newdir);
		}

		s = CG_ConfigString( CS_TARGETEFFECT );	// see if ent has a shader specified
		if(s && strlen(s) > 0)
			sh = trap_R_RegisterShader(va("textures/%s", s));	// FIXME: don't do this here.  only for testing

		cent->currentState.eFlags &= ~EF_INHERITSHADER;	// don't try to inherit shader
		cent->currentState.dl_intensity = 0;		// no sound
		CG_Explode(cent, origin, newdir, sh);
	}


	if(cent->currentState.eventParm & 16)	// gore
	{
		le = CG_AllocLocalEntity();
		re = &le->refEntity;

		le->leType = LE_FRAGMENT;
		le->startTime = cg.time;
		le->endTime = le->startTime + 5000 + random() * 3000;
//----(SA)	fading out
			re->fadeStartTime		= le->endTime - 4000;
			re->fadeEndTime			= le->endTime;
//----(SA)	end

		VectorCopy( origin, re->origin );
		AxisCopy( axisDefault, re->axis );
	//	re->hModel = hModel;
		re->hModel = cgs.media.gibIntestine;
		le->pos.trType = TR_GRAVITY;
		VectorCopy( origin, le->pos.trBase );

	//	VectorCopy( velocity, le->pos.trDelta );
		VectorNormalize(dir);
		VectorMA(dir, 200, dir, le->pos.trDelta);

		le->pos.trTime = cg.time;

		le->bounceFactor = 0.3;

		le->leBounceSoundType = LEBS_BLOOD;
		le->leMarkType = LEMT_BLOOD;
	}


	if(cent->currentState.eventParm & 64) // debris trails (the black strip that Ryan did)
	{
		CG_AddDebris( origin, dir,
						280,	// speed
						1400,	// duration
						// 15 + rand()%5 );	// count
						7 + rand()%2 );	// count
	}
}






/*
CG_Shard

	We should keep this separate since there will be considerable differences
	in the physical properties of shard vrs debris. not to mention the fact
	there is no way we can quantify what type of effects the designers will 
	potentially desire. If it is still possible to merge the functionality of
	cg_shard into cg_explode at a latter time I would have no problem with that
	but for now I want to keep it separate
*/
void CG_Shard(centity_t *cent, vec3_t origin, vec3_t dir)
{
	localEntity_t	*le;
	refEntity_t		*re;
	int				type;
	int				howmany;
	int				i;
	int				rval;

	qboolean		isflyingdebris = qfalse;

	type = cent->currentState.density;
	howmany = cent->currentState.frame;

	for(i = 0;i < howmany; i++)
	{
		le = CG_AllocLocalEntity();
		re = &le->refEntity;

		le->leType				= LE_FRAGMENT;
		le->startTime			= cg.time;
		le->endTime				= le->startTime + 5000 + random() * 5000;

//----(SA)	fading out
			re->fadeStartTime		= le->endTime - 1000;
			re->fadeEndTime			= le->endTime;
//----(SA)	end
		
		if (type == 999)
		{
			le->startTime			= cg.time;
			le->endTime				= le->startTime + 100;
			re->fadeStartTime		= le->endTime - 100;
			re->fadeEndTime			= le->endTime;
			type = 1;

			isflyingdebris = qtrue;
		}


		le->lifeRate			= 1.0/(le->endTime - le->startTime);
		le->leFlags				= LEF_TUMBLE;
		le->bounceFactor		= 0.4;
		// le->leBounceSoundType	= LEBS_WOOD;
		le->leMarkType			= 0;

		VectorCopy( origin, re->origin );
		AxisCopy( axisDefault, re->axis );
		
		if (type == FXTYPE_GLASS) // glass
		{
			rval = rand()%2;

			if (rval)
				re->hModel = cgs.media.shardGlass1;
			else
				re->hModel = cgs.media.shardGlass2;
		}
		else if (type == FXTYPE_WOOD) // wood
		{
			rval = rand()%2;

			if (rval)
				re->hModel = cgs.media.shardWood1;
			else
				re->hModel = cgs.media.shardWood2;
		}
		else if (type == FXTYPE_METAL) // metal
		{
			rval = rand()%2;

			if (rval)
				re->hModel = cgs.media.shardMetal1;
			else
				re->hModel = cgs.media.shardMetal2;
		}
		/*else if (type == 3) // ceramic
		{
			rval = rand()%2;

			if (rval)
				re->hModel = cgs.media.shardCeramic1;
			else
				re->hModel = cgs.media.shardCeramic2;
		}*/
		else if (type == FXTYPE_BRICK || type == FXTYPE_STONE) // rubble
		{
			rval = rand()%3;

			if (rval == 1)
				re->hModel = cgs.media.shardRubble1;
			else if (rval == 2)
				re->hModel = cgs.media.shardRubble2;
			else
				re->hModel = cgs.media.shardRubble3;

		}
		else
			CG_Printf( "CG_Debris has an unknown type\n" );			

		// location
		if (isflyingdebris)
			le->pos.trType = TR_GRAVITY_LOW;
		else
			le->pos.trType = TR_GRAVITY;
		
		VectorCopy( origin, le->pos.trBase );
		VectorNormalize(dir);
		VectorScale(dir, 10 * howmany, le->pos.trDelta);
		le->pos.trTime = cg.time;
		le->pos.trDelta[0] += ((random() * 100) - 50);
		le->pos.trDelta[1] += ((random() * 100) - 50);
		if (type)
			le->pos.trDelta[2] = (random() * 200) + 100;	// randomize sort of a lot so they don't all land together
		else // glass
			le->pos.trDelta[2] = (random() * 100) + 50;	// randomize sort of a lot so they don't all land together

		// rotation
		le->angles.trType = TR_LINEAR;
		le->angles.trTime = cg.time;
		le->angles.trBase[0] = rand()&31;
		le->angles.trBase[1] = rand()&31;
		le->angles.trBase[2] = rand()&31;
		le->angles.trDelta[0] = (100 + (rand()&500)) - 300;
		le->angles.trDelta[1] = (100 + (rand()&500)) - 300;
		le->angles.trDelta[2] = (100 + (rand()&500)) - 300;

	}

}


void CG_ShardJunk (centity_t *cent, vec3_t origin, vec3_t dir)
{
	localEntity_t	*le;
	refEntity_t		*re;
#if defined(_MSC_VER) && defined(_M_IX86)
	int sample, baseTime, duration, axis;
	float *shardOutput;
	unsigned short savedCW, truncCW;
	__int64 converted;
	static const float randomUnit = 0.000030518509447574615f;
	static const float lifetime = 5000.0f, speed = 80.0f;
	static const float spread = 100.0f, bias = 50.0f;
	static const double one = 1.0;
#endif
	(void)cent; /* Original does not dereference this argument. */

	le = CG_AllocLocalEntity();
	re = &le->refEntity;

	le->leType				= LE_FRAGMENT;
	le->startTime			= cg.time;
#if defined(_MSC_VER) && defined(_M_IX86)
	sample = rand() & 32767;
	baseTime = (int)((unsigned)le->startTime + 5000u);
	/* Original retained x87 value is consumed by __ftol, not a float cast. */
	__asm {
		fild sample
		fmul randomUnit
		fmul lifetime
		fiadd baseTime
		fstcw savedCW
		fwait
		mov ax, savedCW
		or ah, 0ch
		mov truncCW, ax
		fldcw truncCW
		fistp converted
		fldcw savedCW
	}
	le->endTime = (int)converted;
#else
	le->endTime = le->startTime + 5000 + random() * 5000;
#endif

	re->fadeStartTime = (int)((unsigned)le->endTime - 1000u);
	re->fadeEndTime			= le->endTime;

#if defined(_MSC_VER) && defined(_M_IX86)
	duration = (int)((unsigned)le->endTime - (unsigned)le->startTime);
	shardOutput = &le->lifeRate;
	__asm {
		mov eax, shardOutput
		fild duration
		fdivr one
		fstp dword ptr [eax]
	}
#else
	le->lifeRate = 1.0/(le->endTime - le->startTime);
#endif
	le->leFlags				= LEF_TUMBLE;
	le->bounceFactor		= 0.4;
	le->leMarkType			= 0;

	VectorCopy( origin, re->origin );
	AxisCopy( axisDefault, re->axis );
		
	re->hModel = cgs.media.shardJunk[rand()%MAX_LOCKER_DEBRIS];

	le->pos.trType = TR_GRAVITY;
		
	VectorCopy( origin, le->pos.trBase );
	VectorNormalize(dir);
#if defined(_MSC_VER) && defined(_M_IX86)
	shardOutput = le->pos.trDelta;
	__asm {
		mov eax, dir
		mov edx, shardOutput
		fld dword ptr [eax]
		fmul speed
		fstp dword ptr [edx]
		fld dword ptr [eax+4]
		fmul speed
		fstp dword ptr [edx+4]
		fld dword ptr [eax+8]
		fmul speed
		fstp dword ptr [edx+8]
	}
#else
	VectorScale(dir, 80, le->pos.trDelta);
#endif
	le->pos.trTime = cg.time;
#if defined(_MSC_VER) && defined(_M_IX86)
	for (axis = 0; axis < 2; ++axis) {
		sample = rand() & 32767;
		shardOutput = &le->pos.trDelta[axis];
		__asm {
			mov eax, shardOutput
			fild sample
			fmul randomUnit
			fmul spread
			fsub bias
			fadd dword ptr [eax]
			fstp dword ptr [eax]
		}
	}
	sample = rand() & 32767;
	le->angles.trType = TR_LINEAR;
	shardOutput = &le->pos.trDelta[2];
	__asm {
		mov eax, shardOutput
		fild sample
		fmul randomUnit
		fmul spread
		fadd bias
		fstp dword ptr [eax]
	}
#else
	le->pos.trDelta[0] += ((random() * 100) - 50);
	le->pos.trDelta[1] += ((random() * 100) - 50);
	le->pos.trDelta[2] = (random() * 100) + 50;
#endif
	// rotation
#if !defined(_MSC_VER) || !defined(_M_IX86)
	le->angles.trType = TR_LINEAR;
#endif
	le->angles.trTime = cg.time;
	//le->angles.trBase[0] = rand()&31;
	//le->angles.trBase[1] = rand()&31;
	le->angles.trBase[2] = rand()&31;
	
	//le->angles.trDelta[0] = (100 + (rand()&500)) - 300;
	//le->angles.trDelta[1] = (100 + (rand()&500)) - 300;
	le->angles.trDelta[2] = (100 + (rand()&500)) - 300;

}

// Gordon: debris test
void CG_Debris (centity_t *cent, vec3_t origin, vec3_t dir) {
	localEntity_t	*le;
	refEntity_t		*re;
	int				type;
		
	type = cent->currentState.density;

	le = CG_AllocLocalEntity();
	re = &le->refEntity;

	le->leType				= LE_FRAGMENT;
	le->startTime			= cg.time;
	le->endTime				= le->startTime + 5000 + random() * 5000;

	re->fadeStartTime		= le->endTime - 1000;
	re->fadeEndTime			= le->endTime;

	le->lifeRate			= 1.0/(le->endTime - le->startTime);
	le->leFlags				= LEF_TUMBLE | LEF_TUMBLE_SLOW;
	le->bounceFactor		= 0.4;
	le->leMarkType			= 0;
	le->breakCount			= 1;
	le->sizeScale			= 0.5;

	VectorCopy( origin, re->origin );
	AxisCopy( axisDefault, re->axis );
		
	re->hModel = cgs.inlineDrawModel[cent->currentState.modelindex];

	le->pos.trType = TR_GRAVITY;
		
	VectorCopy( origin, le->pos.trBase );
	VectorCopy( dir, le->pos.trDelta );
	le->pos.trTime = cg.time;
		
	// rotation
	le->angles.trType = TR_LINEAR;
	le->angles.trTime = cg.time;
	le->angles.trBase[2] = rand()&31;
	
	le->angles.trDelta[2] = (100 + (rand()&500)) - 300;
	le->angles.trDelta[2] = (50 + (rand()&400)) - 100;
	le->angles.trDelta[2] = (50 + (rand()&400)) - 100;
}
// ===================

//void CG_BatDeath( centity_t *cent )
//{
//	CG_ParticleExplosion( "blood", cent->lerpOrigin, vec3_origin, 400, 20, 30, qfalse );
//}

void CG_MortarImpact( centity_t *cent, vec3_t origin, int sfx, qboolean dist )
{
	if( sfx >= 0 )
		trap_S_StartSound( origin, -1, CHAN_AUTO, cgs.media.sfx_mortarexp[sfx] );

	if( dist ) {
		vec3_t	gorg, norm;
		float	gdist;

		VectorSubtract( origin, cg.refdef_current->vieworg, norm );
		gdist = VectorNormalize( norm );
		if(gdist > 1200 && gdist < 8000) {	// 1200 is max cam shakey dist (2*600) use gorg as the new sound origin
			VectorMA( cg.refdef_current->vieworg, 800, norm, gorg ); // non-distance falloff makes more sense; sfx2range was gdist*0.2
																// sfx2range is variable to give us minimum volume control different explosion sizes (see mortar, panzerfaust, and grenade)
			trap_S_StartSoundEx( gorg, -1, CHAN_WEAPON, cgs.media.sfx_mortarexpDist, SND_NOCUT); 
		}		

		if( cent->currentState.clientNum == cg.snap->ps.clientNum && cg.mortarImpactTime != -2 ) {
			VectorCopy( origin, cg.mortarImpactPos );
			cg.mortarImpactTime = cg.time;
			cg.mortarImpactOutOfMap = qfalse;
		}
	}
}

void CG_MortarMiss( centity_t *cent, vec3_t origin )
{
	if( cent->currentState.clientNum == cg.snap->ps.clientNum && cg.mortarImpactTime != -2 ) {
		VectorCopy( origin, cg.mortarImpactPos );
		cg.mortarImpactTime = cg.time;
		if( cent->currentState.density ) {
			cg.mortarImpactOutOfMap = qtrue;
		} else {
			cg.mortarImpactOutOfMap = qfalse;
		}
	}
}

// a convenience function for all footstep sound playing
static void CG_StartFootStepSound( bg_playerclass_t* classInfo, entityState_t *es, sfxHandle_t sfx )
{
    tce_fragmentSoundContext_t context;
    int event, volume;
    float range;
    /* TC:E 3003b1e0 / Linux0006f338. classInfo is unused in both originals. */
    (void)classInfo;
    if (cg.tcePortalScopeRendering)
        CG_Printf("ELITE PORTAL: CG_StartFootStepSound\n");
    if (!cg_footsteps.integer || cg.tcePortalScopeRendering) return;
    event = es->event & ~EV_EVENT_BITS;
    if (event == EV_TCE_FOOTSTEP_SPRINT) { volume = 196; range = 2400.f; }
    else if (event == EV_TCE_FOOTSTEP_WALK) { volume = 64; range = 1200.f; }
    else { volume = 127; range = 1800.f; }
    memset(&context, 0, sizeof(context));
    VectorCopy(cg.refdef_current->vieworg, context.listener);
    context.attenuation = tceFlash.deafness;
    context.distanceVariant = tceSmokeNewBBox;
    volume = TCE_CG_SoundVolume(es->pos.trBase, (float)volume, range, 0, &context);
    if (volume)
        trap_S_StartSoundVControl(NULL, es->number, CHAN_BODY, sfx, volume);
}

/*
==============
CG_EntityEvent

An entity has an event value
also called by CG_CheckPlayerstateEvents
==============
*/
extern void CG_AddBulletParticles( vec3_t origin, vec3_t dir, int speed, int duration, int count, float randScale );
// JPW NERVE
void CG_MachineGunEjectBrass( centity_t *cent );
void CG_MachineGunEjectBrassNew( centity_t *cent );
// jpw
#define	DEBUGNAME(x) if(cg_debugEvents.integer){CG_Printf(x"\n");}
void CG_EntityEvent( centity_t *cent, vec3_t position ) {
	entityState_t		*es;
	int					event;
	vec3_t				dir;
	const char			*s;
	int					clientNum;
	clientInfo_t		*ci;
	char				tempStr[MAX_QPATH];
	bg_playerclass_t	*classInfo;
	bg_character_t		*character;
	int tceEventVolume;
	tce_fragmentSoundContext_t tceEventSound;

// JPW NERVE copied here for mg42 SFX event
	vec3_t				gorg, norm;	// player/gun origin
#if !defined(_MSC_VER) || !defined(_M_IX86)
	vec3_t				porg;
	float				gdist;
#endif
// jpw

	static int		footstepcnt = 0;
	static int		splashfootstepcnt = 0;

	es = &cent->currentState;
	event = es->event & ~EV_EVENT_BITS;

	if ( cg_debugEvents.integer ) {
		CG_Printf( "time:%i ent:%3i  event:%3i ", cg.time, es->number, event );
	}

	if ( !event ) {
		DEBUGNAME("ZEROEVENT");
		return;
	}

	clientNum = es->clientNum;
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		clientNum = 0;
	}
	ci = &cgs.clientinfo[ clientNum ];
	classInfo = CG_PlayerClassForClientinfo( ci, cent );
	character = CG_CharacterForClientinfo( ci, cent );

    /* Original common event-volume producer (30037d40), before dispatch. */
    memset(&tceEventSound,0,sizeof(tceEventSound));
    VectorCopy(cg.refdef_current->vieworg,tceEventSound.listener);
    tceEventSound.attenuation=tceFlash.deafness;
    tceEventSound.distanceVariant=tceSmokeNewBBox;
    tceEventVolume=TCE_CG_SoundVolume(es->pos.trBase,127.f,1200.f,0,&tceEventSound);
    if(cg.tcePortalScopeRendering)tceEventVolume=0;

	switch ( event ) {
	//
	// movement generated events
	//
	case EV_FOOTSTEP:
	case EV_TCE_FOOTSTEP_SPRINT:
	case EV_TCE_FOOTSTEP_WALK:
		DEBUGNAME("EV_FOOTSTEP");
		if( es->eventParm != 23 ) { /* TC silent surface; SDK FOOTSTEP_TOTAL is 9. */
			if( es->eventParm ) {
				CG_StartFootStepSound( classInfo, es, cgs.media.footsteps[ es->eventParm ][footstepcnt] );
			} else {
				CG_StartFootStepSound( classInfo, es, cgs.media.footsteps[ character->animModelInfo->footsteps ][footstepcnt] );
			}
		}
		break;
	case EV_FOOTSPLASH:
		DEBUGNAME("EV_FOOTSPLASH");
		CG_StartFootStepSound( classInfo, es, cgs.media.footsteps[ FOOTSTEP_SPLASH ][splashfootstepcnt] );
		break;
	case EV_FOOTWADE:
		DEBUGNAME("EV_FOOTWADE");
		CG_StartFootStepSound( classInfo, es, cgs.media.footsteps[ FOOTSTEP_SPLASH ][splashfootstepcnt] );
		break;
	case EV_SWIM:
		DEBUGNAME("EV_SWIM");
		CG_StartFootStepSound( classInfo, es, cgs.media.footsteps[ FOOTSTEP_SPLASH ][footstepcnt] );
		break;

	case EV_FALL_SHORT:
		DEBUGNAME("EV_FALL_SHORT");
        if(tceEventVolume) {
            if(es->eventParm!=23) {
                int step=es->eventParm ? es->eventParm : character->animModelInfo->footsteps;
                trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.landSound[step],tceEventVolume);
            }
        }
        if(clientNum==cg.predictedPlayerState.clientNum) {
            cg.landChange=-8;
            cg.landTime=cg.time;
        }
        break;

	case EV_FALL_DMG_10:
		DEBUGNAME("EV_FALL_DMG_10");
        if(tceEventVolume) {
            if(es->eventParm!=23) {
                int step=es->eventParm ? es->eventParm : character->animModelInfo->footsteps;
                trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.landSound[step],tceEventVolume);
            }
            trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.landHurt,tceEventVolume);
        }
        cent->pe.painTime=cg.time;
        if(clientNum==cg.predictedPlayerState.clientNum) {
            cg.landChange=-16;
            cg.landTime=cg.time;
        }
        break;
	case EV_FALL_DMG_15:
		DEBUGNAME("EV_FALL_DMG_15");
        if(tceEventVolume) {
            if(es->eventParm!=23) {
                int step=es->eventParm ? es->eventParm : character->animModelInfo->footsteps;
                trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.landSound[step],tceEventVolume);
            }
            trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.landHurt,tceEventVolume);
        }
        cent->pe.painTime=cg.time;
        if(clientNum==cg.predictedPlayerState.clientNum) {
            cg.landChange=-16;
            cg.landTime=cg.time;
        }
        break;
	case EV_FALL_DMG_25:
		DEBUGNAME("EV_FALL_DMG_25");
        if(tceEventVolume) {
            if(es->eventParm!=23) {
                int step=es->eventParm ? es->eventParm : character->animModelInfo->footsteps;
                trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.landSound[step],tceEventVolume);
            }
            trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.landHurt,tceEventVolume);
        }
        cent->pe.painTime=cg.time;
        if(clientNum==cg.predictedPlayerState.clientNum) {
            cg.landChange=-24;
            cg.landTime=cg.time;
        }
        break;
    case EV_TCE_FENCE_TOUCH: {
        int choice;
        DEBUGNAME("EV_TOUCH_FENCE");
        if(tceEventVolume) {
#if defined(_MSC_VER) && defined(_M_IX86)
            choice=CG_EventFiveSoundChoice();
#else
            /* Original random()*5 uses the final slot for the endpoint too. */
            choice=(int)((rand()&0x7fff)*(1.0f/32767.0f)*5.0f);
            if(choice>4)choice=4;
#endif
            trap_S_StartSoundVControl(es->pos.trBase,es->number,CHAN_AUTO,
                cgs.media.tceBulletFence[choice],tceEventVolume);
        }
        break;
    }
    case EV_TCE_FALL_DMG_75: {
        int step=es->eventParm;
        DEBUGNAME("EV_FALL_DMG_75");
        if(tceEventVolume) {
            if(step!=23) {
                if(!step)step=character->animModelInfo->footsteps;
                trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.landSound[step],tceEventVolume);
            }
            trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.landHurt,tceEventVolume);
        }
        cent->pe.painTime=cg.time;
        if(clientNum==cg.predictedPlayerState.clientNum) {
            cg.landChange=-24;cg.landTime=cg.time;
        }
        break;
    }
	case EV_FALL_DMG_50:
		DEBUGNAME("EV_FALL_DMG_50");
        if(tceEventVolume) {
            if(es->eventParm!=23) {
                int step=es->eventParm ? es->eventParm : character->animModelInfo->footsteps;
                trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.landSound[step],tceEventVolume);
            }
            trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.landHurt,tceEventVolume);
        }
        cent->pe.painTime=cg.time;
        if(clientNum==cg.predictedPlayerState.clientNum) {
            cg.landChange=-24;
            cg.landTime=cg.time;
        }
        break;
	case EV_FALL_NDIE:
		DEBUGNAME("EV_FALL_NDIE");
        if(tceEventVolume) {
            if(es->eventParm!=23) {
                int step=es->eventParm ? es->eventParm : character->animModelInfo->footsteps;
                trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.landSound[step],tceEventVolume);
            }
            trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.landHurt,tceEventVolume);
        }
        cent->pe.painTime=cg.time;
        break;
	
	case EV_EXERT1:
		DEBUGNAME("EV_EXERT1");
		trap_S_StartSound (NULL, es->number, CHAN_VOICE, CG_CustomSound( es->number, "*exert1.wav" ) );
		break;
	case EV_EXERT2:
		DEBUGNAME("EV_EXERT2");
		trap_S_StartSound (NULL, es->number, CHAN_VOICE, CG_CustomSound( es->number, "*exert2.wav" ) );
		break;
	case EV_EXERT3:
		DEBUGNAME("EV_EXERT3");
		trap_S_StartSound (NULL, es->number, CHAN_VOICE, CG_CustomSound( es->number, "*exert3.wav" ) );
		break;

	case EV_STEP_4:
	case EV_STEP_8:
	case EV_STEP_12:
	case EV_STEP_16:		// smooth out step up transitions
		DEBUGNAME("EV_STEP");
	{
		int		delta;
		int		step;
#if defined(_MSC_VER) && defined(_M_IX86)
		float *stepOutput = &cg.stepChange;
		static const float stepDecay = 0.005f, stepZero = 0.0f, stepMaximum = 32.0f;
		int stepRemaining;
#else
		float oldStep;
#endif

		if ( clientNum != cg.predictedPlayerState.clientNum ) {
			break;
		}
		// if we are interpolating, we don't need to smooth steps
		if ( cg.demoPlayback || (cg.snap->ps.pm_flags & PMF_FOLLOW) ||
			cg_nopredict.integer ) {
			break;
		}
		// check for stepping up before a previous step is completed
		delta = (int)((unsigned)cg.time - (unsigned)cg.stepTime);
#if defined(_MSC_VER) && defined(_M_IX86)
		/* TC 300384ce..30038542: retain decay in ST0 until the new step
		 * is added; only then store binary32 and compare the stored value. */
		stepRemaining = (int)(200u - (unsigned)delta);
		step = 4 * (event - EV_STEP_4 + 1);
		__asm {
			mov ecx, stepOutput
			cmp delta, 200
			jge stepNoPrevious
			fild stepRemaining
			fmul dword ptr [ecx]
			fmul stepDecay
			jmp stepAddCurrent
		stepNoPrevious:
			fld stepZero
		stepAddCurrent:
			fild step
			fadd st(0), st(1)
			fstp dword ptr [ecx]
			fstp st(0)
			fld dword ptr [ecx]
			fcomp stepMaximum
			fnstsw ax
			test ah, 41h
			jnz stepStoreTime
			mov dword ptr [ecx], 42000000h
		stepStoreTime:
		}
#else
		if (delta < STEP_TIME) {
			oldStep = cg.stepChange * (STEP_TIME - delta) / STEP_TIME;
		} else {
			oldStep = 0;
		}

		// add this amount
		step = 4 * (event - EV_STEP_4 + 1 );
		cg.stepChange = oldStep + step;
		if ( cg.stepChange > MAX_STEP_CHANGE ) {
			cg.stepChange = MAX_STEP_CHANGE;
		}
#endif
		cg.stepTime = cg.time;
		break;
	}

	case EV_JUMP:
		DEBUGNAME("EV_JUMP");
		if(cg.tceAimRequested && es->number==cg.snap->ps.clientNum) CG_ToggleAiming();
		trap_S_StartSound (NULL, es->number, CHAN_VOICE, CG_CustomSound( es->number, "*jump1.wav" ) );
		break;
	case EV_TAUNT:
		DEBUGNAME("EV_TAUNT");
		trap_S_StartSound (NULL, es->number, CHAN_VOICE, CG_CustomSound( es->number, "*taunt.wav" ) );
		break;
    case EV_WATER_TOUCH:
        DEBUGNAME("EV_WATER_TOUCH");
        if(tceEventVolume) trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.watrInSound,tceEventVolume);
        break;
    case EV_WATER_LEAVE:
        DEBUGNAME("EV_WATER_LEAVE");
        if(tceEventVolume) trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.watrOutSound,tceEventVolume);
        break;
    case EV_WATER_UNDER:
        DEBUGNAME("EV_WATER_UNDER");
        if(tceEventVolume) trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.watrUnSound,tceEventVolume);
        if(cg.clientNum==es->number) cg.waterundertime=cg.time+12000;
        break;
    case EV_WATER_CLEAR:
        DEBUGNAME("EV_WATER_CLEAR");
        if(tceEventVolume) {
            trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.watrOutSound,tceEventVolume);
            if(es->eventParm) trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.watrGaspSound,tceEventVolume);
        }
        break;

    case EV_ITEM_PICKUP:
    case EV_ITEM_PICKUP_QUIET:
        DEBUGNAME("EV_ITEM_PICKUP");
        if(es->eventParm>0 && es->eventParm<bg_numItems && es->number==cg.snap->ps.clientNum)
            CG_ItemPickup(es->eventParm);
        break;
    case EV_GLOBAL_ITEM_PICKUP:
        DEBUGNAME("EV_GLOBAL_ITEM_PICKUP");
        if(es->eventParm>0 && es->eventParm<bg_numItems && es->number==cg.snap->ps.clientNum)
            CG_ItemPickup(es->eventParm);
        break;

	//
	// weapon events
	//
	case EV_VENOM:
		DEBUGNAME("EV_VENOM");
//		CG_VenomFire( es, qfalse );
		break;

	case EV_WEAP_OVERHEAT:
		DEBUGNAME("EV_WEAP_OVERHEAT");

		// start weapon idle animation
		if(es->number == cg.snap->ps.clientNum) {
			cg.predictedPlayerState.weapAnim = ( ( cg.predictedPlayerState.weapAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | PM_IdleAnimForWeapon(cg.snap->ps.weapon);
			cent->overheatTime = cg.time;	// used to make the barrels smoke when overheated
		}

        if(!tceEventVolume)break;
		/* Original mounted mask includes the tank bit; no second tank lookup. */
		if( es->eFlags & 0x408020 ) {
			trap_S_StartSoundVControl( NULL, es->number, CHAN_AUTO, cgs.media.hWeaponHeatSnd, 255 );
		} else if( cg_weapons[es->weapon].overheatSound ) {
			trap_S_StartSound (NULL, es->number, CHAN_AUTO, cg_weapons[es->weapon].overheatSound );
		}
		break;

// JPW NERVE
	case EV_SPINUP: 
		DEBUGNAME("EV_SPINUP");
			trap_S_StartSound (NULL, es->number, CHAN_AUTO, cg_weapons[es->weapon].spinupSound );
		break;
// jpw
    case EV_EMPTYCLIP:
        DEBUGNAME("EV_EMPTYCLIP");
        if(es->weapon!=4 && es->weapon!=9 && es->weapon!=15 &&
           es->weapon!=26 && es->weapon!=27 && es->weapon!=28 &&
           es->weapon!=29 && es->weapon!=30 && es->weapon!=12 &&
           es->weapon!=19 && tceEventVolume)
            trap_S_StartSoundVControl(NULL,es->number,CHAN_WEAPON,cgs.media.noAmmoSound,tceEventVolume);
        break;

    /* Original156..160 deliberately have only debug output in CG_EntityEvent.
     * Authoritative objective actions are handled by qagame, not fired again here. */
    case EV_TCE_PLANT: DEBUGNAME("EV_FIRE_WEAPON_DYNAMITE"); break;
    case EV_TCE_DEFUSE: DEBUGNAME("EV_FIRE_WEAPON_DISARM"); break;
    case EV_TCE_OBJECTIVE_START: DEBUGNAME("EV_FIRE_WEAPON_ACTIVATE"); break;
    case EV_TCE_OBJECTIVE_STOP: DEBUGNAME("EV_FIRE_WEAPON_ACTIVATE_STOPPED"); break;
    case EV_TCE_OBJECTIVE_COMPLETE: DEBUGNAME("EV_FIRE_WEAPON_ACTIVATE_COMPLETE"); break;

    case EV_TCE_GRENADE_PRIME: {
        DEBUGNAME("EV_GRENADE_PRIME");
        if(cgs.media.tceGrenadePrime && tceEventVolume)
            trap_S_StartSoundVControl(NULL,es->number,CHAN_WEAPON,cgs.media.tceGrenadePrime,tceEventVolume);
        break;
    }

    case EV_TCE_TOGGLE_AIMING:
        DEBUGNAME("EV_TOGGLE_AIMING");
        if(es->number==cg.snap->ps.clientNum) CG_ToggleAiming();
        break;

    case EV_TCE_FIREMODE: {
        DEBUGNAME("EV_TOGGLE_FIREMODE");
        if(tceEventVolume) trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.tceFiremodeSound,tceEventVolume);
        /* TC30037d40 event135: finish the synthetic firemode selection. */
        if (es->number == cg.snap->ps.clientNum && cg.weaponSelect == 55) {
            cg.weaponSelect = cg.predictedPlayerState.weapon;
            cg.tceFiremodeAnimationTime = cg.time;
        }
        break;
    }

    case EV_TCE_RELOAD_CYCLE:
        DEBUGNAME("EV_RELOAD_CYCLE");
        if(cgs.media.tceReloadSounds[0] && tceEventVolume)
            trap_S_StartSoundVControl(NULL,es->number,CHAN_WEAPON,cgs.media.tceReloadSounds[0],tceEventVolume);
        break;
    case EV_TCE_RELOAD_PUMP:
        DEBUGNAME("EV_RELOAD_PUMP");
        if(cgs.media.tceReloadSounds[1] && tceEventVolume)
            trap_S_StartSoundVControl(NULL,es->number,CHAN_WEAPON,cgs.media.tceReloadSounds[1],tceEventVolume);
        break;
    case EV_TCE_RELOAD_PUMP2:
        DEBUGNAME("EV_RELOAD_PUMP2");
        if(cgs.media.tceReloadSounds[2] && tceEventVolume)
            trap_S_StartSoundVControl(NULL,es->number,CHAN_WEAPON,cgs.media.tceReloadSounds[2],tceEventVolume);
        /* TC reads the mechanical flag after sound submission even when
         * muted. Keep the native table bound; do not invent slots64..66. */
        if(es->weapon>=0 && es->weapon<TCE_MAX_WEAPONS && weaponDef[es->weapon].singleReload)
            cent->tceEjectPending=1;
        break;
    case EV_TCE_RELOAD_BOLT:
        DEBUGNAME("EV_RELOAD_BOLT");
        if(cgs.media.tceReloadSounds[3] && tceEventVolume)
            trap_S_StartSoundVControl(NULL,es->number,CHAN_WEAPON,cgs.media.tceReloadSounds[3],tceEventVolume);
        if(es->weapon>=0 && es->weapon<TCE_MAX_WEAPONS && weaponDef[es->weapon].bolt)
            cent->tceEjectPending=1;
        break;

	case EV_FILL_CLIP:
		DEBUGNAME("EV_FILL_CLIP");
        if(!tceEventVolume)break;
		if( cgs.clientinfo[cg.clientNum].skill[SK_LIGHT_WEAPONS] >= 2 && BG_isLightWeaponSupportingFastReload( es->weapon ) && cg_weapons[es->weapon].reloadFastSound )
			trap_S_StartSoundVControl (NULL, es->number, CHAN_WEAPON, cg_weapons[es->weapon].reloadFastSound, tceEventVolume );
		else if(cg_weapons[es->weapon].reloadSound)
			trap_S_StartSoundVControl (NULL, es->number, CHAN_WEAPON, cg_weapons[es->weapon].reloadSound, tceEventVolume ); // JPW NERVE following sherman's SP fix, should allow killing reload sound when player dies
		break;

// JPW NERVE play a sound when engineer fixes MG42
	case EV_MG42_FIXED:
		DEBUGNAME("EV_MG42_FIXED");
		//trap_S_StartSound(NULL,es->number,CHAN_WEAPON,cg_weapons[WP_MAUSER].reloadSound); // Arnout: needs updating
		break;
// jpw

    case EV_NOAMMO:
    case EV_WEAPONSWITCHED:
        DEBUGNAME("EV_NOAMMO");
        /* Original TC protocol IDs, not the SDK weapon enum aliases. */
        if(es->weapon!=4 && es->weapon!=9 && es->weapon!=15 &&
           es->weapon!=26 && es->weapon!=27 && es->weapon!=28 &&
           es->weapon!=29 && es->weapon!=30 && es->weapon!=12 &&
           es->weapon!=19 && tceEventVolume)
            trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.noAmmoSound,tceEventVolume);
        if(es->number==cg.snap->ps.clientNum &&
           ((cg_noAmmoAutoSwitch.integer>0 && !CG_WeaponSelectable(cg.weaponSelect)) ||
            es->weapon==60 || es->weapon==62 || es->weapon==4 ||
            es->weapon==9 || es->weapon==15 || es->weapon==22 ||
            es->weapon==65 || es->weapon==63 || es->weapon==26 ||
            es->weapon==27 || es->weapon==28 || es->weapon==29 ||
            es->weapon==30 || es->weapon==12 || es->weapon==19))
            CG_OutOfAmmoChange(event!=EV_WEAPONSWITCHED);
        break;
    case EV_CHANGE_WEAPON:
    case EV_CHANGE_WEAPON_2:
        DEBUGNAME("EV_CHANGE_WEAPON");
        if(tceEventVolume)
            trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,cgs.media.selectSound,tceEventVolume);
        cent->tceEjectPending=0;
        if(event==EV_CHANGE_WEAPON_2 && es->number==cg.snap->ps.clientNum) {
            switch(es->weapon) {
            case 57: CG_FinishWeaponChange(57,25); break;
            case 58: CG_FinishWeaponChange(58,32); break;
            case 59: CG_FinishWeaponChange(59,33); break;
            default: break;
            }
        }
        break;

	case EV_FIRE_WEAPON_MOUNTEDMG42:
	case EV_FIRE_WEAPON_MG42:
	{
		int echoAudible;
#if defined(_MSC_VER) && defined(_M_IX86)
		float *echoSource = cent->currentState.pos.trBase;
		float *echoListener = cg.refdef_current->vieworg;
		float *echoDirection = norm, *echoOrigin = gorg;
		static const float echoNear = 512.0f, echoFar = 4096.0f, echoOffset = 64.0f;
		/* TC30039043..300390f8: the normalization return remains on ST0
		 * through both distance gates. Each echo coordinate has one store. */
		__asm {
			mov ecx, echoSource
			mov edx, echoListener
			mov eax, echoDirection
			fld dword ptr [edx]
			fld dword ptr [ecx]
			fsub st(0), st(1)
			fstp dword ptr [eax]
			fstp st(0)
			fld dword ptr [ecx+4]
			fsub dword ptr [edx+4]
			fstp dword ptr [eax+4]
			fld dword ptr [ecx+8]
			fsub dword ptr [edx+8]
			fstp dword ptr [eax+8]
			push eax
			call VectorNormalize
			fcom echoNear
			add esp, 4
			fnstsw ax
			test ah, 41h
			jnz echoRejectPop
			fcomp echoFar
			fnstsw ax
			test ah, 1
			jz echoReject
			mov echoAudible, 1
			jmp echoGateDone
		echoRejectPop:
			fstp st(0)
		echoReject:
			mov echoAudible, 0
		echoGateDone:
		}
		if (echoAudible) {
			/* The original reloads refdef_current after VectorNormalize. */
			echoListener = cg.refdef_current->vieworg;
			__asm {
				mov ecx, echoDirection
				mov edx, echoListener
				mov eax, echoOrigin
				fld dword ptr [ecx]
				fmul echoOffset
				fadd dword ptr [edx]
				fstp dword ptr [eax]
				fld dword ptr [ecx+4]
				fmul echoOffset
				fadd dword ptr [edx+4]
				fstp dword ptr [eax+4]
				fld dword ptr [ecx+8]
				fmul echoOffset
				fadd dword ptr [edx+8]
				fstp dword ptr [eax+8]
			}
		}
#else
		/* Preserve the portable path; no Linux x87 parity claim. */
		VectorCopy(cent->currentState.pos.trBase, gorg);
		VectorCopy(cg.refdef_current->vieworg, porg);
		VectorSubtract(gorg, porg, norm);
		gdist = VectorNormalize(norm);
		echoAudible = gdist > 512 && gdist < 4096;
		if(echoAudible) {
			VectorMA(cg.refdef_current->vieworg, 64, norm, gorg);
		}
#endif
		if(echoAudible) {
			if( cg_entities[cg_entities[cg_entities[ cent->currentState.number ].tagParent].tankparent].currentState.density & 8 ) { // should we use a browning?
				trap_S_StartSoundEx( gorg, cent->currentState.number, CHAN_WEAPON, cgs.media.hWeaponEchoSnd_2, SND_NOCUT);
			} else {
				trap_S_StartSoundEx( gorg, cent->currentState.number, CHAN_WEAPON, cgs.media.hWeaponEchoSnd, SND_NOCUT);
			}
		}
		DEBUGNAME("EV_FIRE_WEAPON_MG42");
		CG_FireWeapon( cent );
		break;
	}
	case EV_FIRE_WEAPON_AAGUN:
		DEBUGNAME("EV_FIRE_WEAPON_AAGUN");
		CG_FireWeapon( cent);
		break;
	case EV_FIRE_WEAPON:
	case EV_FIRE_WEAPONB:
		DEBUGNAME("EV_FIRE_WEAPON");
		if( cent->currentState.clientNum == cg.snap->ps.clientNum && cg.snap->ps.eFlags & EF_ZOOMING ) // to stop airstrike sfx
			break;
		CG_FireWeapon( cent);
		if( event == EV_FIRE_WEAPONB )	// akimbo firing
			cent->akimboFire = qtrue;
		else
			cent->akimboFire = qfalse;
		break;
	case EV_FIRE_WEAPON_LASTSHOT:
		DEBUGNAME("EV_FIRE_WEAPON_LASTSHOT");
		CG_FireWeapon( cent);
		break;

	case EV_NOFIRE_UNDERWATER:
		DEBUGNAME("EV_NOFIRE_UNDERWATER");
		if(cgs.media.noFireUnderwater)
			trap_S_StartSound (NULL, es->number, CHAN_WEAPON, cgs.media.noFireUnderwater);
		break;

	case EV_PLAYER_TELEPORT_IN:
		break;

	case EV_PLAYER_TELEPORT_OUT:
		break;

	case EV_ITEM_POP:
		break;
	case EV_ITEM_RESPAWN:
		break;

    case EV_GRENADE_BOUNCE:
        DEBUGNAME("EV_GRENADE_BOUNCE");
        if(tceEventVolume) {
            sfxHandle_t bounce;
            if(es->weapon==27) bounce=cgs.media.satchelbounce1;
            else if(es->weapon==15) bounce=cgs.media.dynamitebounce1;
            else if(es->weapon==26) bounce=cgs.media.landminebounce1;
            else if(es->eventParm==11) {
                /* The original mask deliberately selects fence samples0/4. */
                bounce=cgs.media.tceBulletFence[rand()&4];
            } else {
                if(es->eventParm==23)break;
                bounce=cgs.media.grenadebounce[es->eventParm][(rand()&1)?0:1];
            }
            trap_S_StartSoundVControl(NULL,es->number,CHAN_AUTO,bounce,tceEventVolume);
        }
        break;

/*	case EV_FLAMEBARREL_BOUNCE:
		DEBUGNAME("EV_FLAMEBARREL_BOUNCE");
		if ( rand() & 1 ) {
			trap_S_StartSound (NULL, es->number, CHAN_AUTO, cgs.media.fbarrelexp1 );
		} else {
			trap_S_StartSound (NULL, es->number, CHAN_AUTO,  cgs.media.fbarrelexp2 );
		}
		break;*/

	case EV_RAILTRAIL:
		CG_RailTrail( &cgs.clientinfo[ es->otherEntityNum2 ], es->origin2, es->pos.trBase, es->dmgFlags, es->effect3Time);	//----(SA)	added 'type' field
		break;

	//
	// missile impacts
	//
	case EV_MISSILE_HIT:
		DEBUGNAME("EV_MISSILE_HIT");
		ByteToDir( es->eventParm, dir );
		CG_MissileHitPlayer( cent, es->weapon, position, dir, es->otherEntityNum );
		if( es->weapon == 60 ) {
			if( !es->legsAnim ) {
				CG_MortarImpact( cent, position, 3, qtrue );
			} else {
				CG_MortarImpact( cent, position, -1, qtrue );
			}
		}
		break;

	case EV_MISSILE_MISS_SMALL:
		DEBUGNAME("EV_MISSILE_MISS");
		ByteToDir( es->eventParm, dir );
		CG_MissileHitWallSmall( es->weapon, 0, position, dir );
		break;

	case EV_MISSILE_MISS:
		DEBUGNAME("EV_MISSILE_MISS");
		ByteToDir( es->eventParm, dir );
		CG_TCEMissileHitWall(es->weapon,0,position,dir,dir,
            es->weapon==1?es->otherEntityNum2:0,es->weapon==1 && es->modelindex2==1,0);
		if( es->weapon == 60 ) {
			if( !es->legsAnim ) {
				CG_MortarImpact( cent, position, 3, qtrue );
			} else {
				CG_MortarImpact( cent, position, -1, qtrue );
			}
		}
		break;

	case EV_MISSILE_MISS_LARGE:
		DEBUGNAME("EV_MISSILE_MISS_LARGE");
		ByteToDir( es->eventParm, dir );
		if( es->weapon == 63 || es->weapon == 22 ) {
			CG_TCEMissileHitWall(es->weapon,0,position,dir,dir,0,0,0);
		} else {
			CG_TCEMissileHitWall(18,0,position,dir,dir,0,0,0);
		}
		break;

	case EV_MORTAR_IMPACT:
		DEBUGNAME("EV_MORTAR_IMPACT");
		CG_MortarImpact( cent, position, rand()%3, qfalse );
		break;
	case EV_MORTAR_MISS:
		DEBUGNAME("EV_MORTAR_MISS");
		CG_MortarMiss( cent, position );
		break;

	case EV_MG42BULLET_HIT_WALL:
		DEBUGNAME("EV_MG42BULLET_HIT_WALL");
		ByteToDir( es->eventParm, dir );
		break;

	case EV_MG42BULLET_HIT_FLESH:
		DEBUGNAME("EV_MG42BULLET_HIT_FLESH");
		break;


    case EV_TCE_BULLET_NEAR_MISS: {
        tce_fragmentSoundContext_t context;
        int volume, choice;
#if !defined(_MSC_VER) || !defined(_M_IX86)
        float sample;
#endif
        DEBUGNAME("EV_BULLET_FLYBY");
        if (es->eventParm != cg.snap->ps.clientNum) break;
        memset(&context,0,sizeof(context));
        VectorCopy(cg.refdef_current->vieworg,context.listener);
        context.attenuation=tceFlash.deafness;
        context.distanceVariant=tceSmokeNewBBox;
        context.disabled=cg.tcePortalScopeRendering;
        volume=TCE_CG_SoundVolume(es->pos.trBase,127.f,300.f,0,&context);
        if (!volume) break;
#if defined(_MSC_VER) && defined(_M_IX86)
        choice=CG_EventFiveSoundChoice();
#else
        sample=(float)(rand()&0x7fff)*(1.f/32767.f)*5.f;
        choice=sample<1.f?0:sample<2.f?1:sample<3.f?2:sample<4.f?3:4;
#endif
        trap_S_StartSoundVControl(es->pos.trBase,es->number,CHAN_AUTO,
            cgs.media.tceBulletFlyby[choice],volume);
        break;
    }

	case EV_BULLET_HIT_WALL:
		DEBUGNAME("EV_BULLET_HIT_WALL");
		ByteToDir( es->eventParm, dir );
		CG_TCEBullet( es->pos.trBase, es->otherEntityNum, dir, qfalse,
            ENTITYNUM_WORLD, es->otherEntityNum2, 0, 0, es->modelindex2, es->origin2, 0 );
		break;

	case EV_BULLET_HIT_FLESH:
		DEBUGNAME("EV_BULLET_HIT_FLESH");
		CG_TCEBullet( es->pos.trBase, es->otherEntityNum, dir, qtrue,
            es->eventParm, es->otherEntityNum2, 0, 0, es->modelindex2, es->origin2, 1 );
		break;

    case EV_TCE_BULLET_PIERCED_WALL:
        DEBUGNAME("EV_BULLET_PIERCED_WALL");
        if (es->otherEntityNum != cg.snap->ps.clientNum || cg_predictBullets.integer < 1) {
            ByteToDir(es->eventParm, dir);
            CG_TCEMissileHitWall(3, 1, es->pos.trBase, dir, dir, es->otherEntityNum2, 0, 1);
        }
        break;

    case EV_TCE_SHOTGUN:
        DEBUGNAME("EV_SHOTGUN");
        CG_TCEShotgunFire(es);
        break;

	case EV_POPUPBOOK:
	case EV_POPUP:
	case EV_GIVEPAGE:
		break;

	case EV_GENERAL_SOUND:
		DEBUGNAME("EV_GENERAL_SOUND");
        if(cg.tcePortalScopeRendering)break;
		// Ridah, check for a sound script
		s = CG_ConfigString( CS_SOUNDS + es->eventParm );
		if( !strstr( s, ".wav" ) ) {
			if( CG_SoundPlaySoundScript( s, NULL, es->number, qfalse ) ) {
				break;
			}
			// try with .wav
			Q_strncpyz( tempStr, s, sizeof(tempStr) );
			Q_strcat( tempStr, sizeof(tempStr), ".wav" );
			s = tempStr;
		}

		// done.
		if ( cgs.gameSounds[ es->eventParm ] ) {
			// xkan, 10/31/2002 - crank up the volume 
			trap_S_StartSoundVControl( NULL, es->number, CHAN_VOICE, cgs.gameSounds[ es->eventParm ], tceEventVolume );
		} else {
			s = CG_ConfigString( CS_SOUNDS + es->eventParm );
			// xkan, 10/31/2002 - crank up the volume 
			trap_S_StartSoundVControl( NULL, es->number, CHAN_VOICE, CG_CustomSound( es->number, s ), tceEventVolume );
		}
		break;

	case EV_FX_SOUND:
        if(cg.tcePortalScopeRendering)break;
		{
			sfxHandle_t sound;

			DEBUGNAME("EV_FX_SOUND");

			#if defined(_MSC_VER) && defined(_M_IX86)
			{
				static const float fxRandomScale = 1.0f / 32767.0f;
				int fxRandomBits = rand() & 0x7fff;
				int *fxMaximum = &fxSounds[ es->eventParm ].max;
				__asm {
					fild fxRandomBits
					fmul fxRandomScale
					mov eax, fxMaximum
					fimul dword ptr [eax]
					call CG_EventTruncateST0
					mov sound, eax
				}
			}
			#else
			sound = random()*fxSounds[ es->eventParm ].max;
			#endif

			if( fxSounds[ es->eventParm ].sound[ sound ] == -1 ) {
				fxSounds[ es->eventParm ].sound[ sound ] = trap_S_RegisterSound( fxSounds[ es->eventParm ].soundfile[ sound ], qfalse );
			}

			sound = fxSounds[ es->eventParm ].sound[ sound ];

			#if defined(_MSC_VER) && defined(_M_IX86)
			{
				static const float fxVolumeScale = 255.0f;
				float *fxDeafness = &tceFlash.deafness;
				int fxVolume;
				__asm {
					fld1
					mov eax, fxDeafness
					fsub dword ptr [eax]
					fmul fxVolumeScale
					call CG_EventTruncateST0
					mov fxVolume, eax
				}
				trap_S_StartSoundVControl( NULL, es->number, CHAN_VOICE, sound, fxVolume );
			}
			#else
			trap_S_StartSoundVControl( NULL, es->number, CHAN_VOICE, sound, (int)((1.0-(double)tceFlash.deafness)*255.0) );
			#endif
		}
		break;
	case EV_GENERAL_SOUND_VOLUME:
        if(cg.tcePortalScopeRendering)break;
		{
			int sound = es->eventParm;
			int eventVolume = es->onFireStart;
			int volume;
#if !defined(_MSC_VER) || !defined(_M_IX86)
			volume = (int)((1.0-(double)tceFlash.deafness)*eventVolume);
#endif

			DEBUGNAME("EV_GENERAL_SOUND_VOLUME");
			// Ridah, check for a sound script
			s = CG_ConfigString( CS_SOUNDS + sound );
			if( !strstr( s, ".wav" ) ) {
				if( CG_SoundPlaySoundScript( s, NULL, es->number, qfalse ) ) {
					break;
				}
				// try with .wav
				Q_strncpyz( tempStr, s, sizeof(tempStr) );
				Q_strcat( tempStr, sizeof(tempStr), ".wav" );
				s = tempStr;
			}
			// done.
			if ( cgs.gameSounds[ sound ] ) {
				#if defined(_MSC_VER) && defined(_M_IX86)
				float *eventDeafness = &tceFlash.deafness;
				__asm {
					fld1
					mov eax, eventDeafness
					fsub dword ptr [eax]
					fimul eventVolume
					call CG_EventTruncateST0
					mov volume, eax
				}
				#endif
				trap_S_StartSoundVControl( NULL, es->number, CHAN_VOICE, cgs.gameSounds[ sound ], volume );
			} else {
				s = CG_ConfigString( CS_SOUNDS + sound );
				#if defined(_MSC_VER) && defined(_M_IX86)
				{
					float *eventDeafness = &tceFlash.deafness;
					__asm {
						fld1
						mov eax, eventDeafness
						fsub dword ptr [eax]
						fimul eventVolume
						call CG_EventTruncateST0
						mov volume, eax
					}
				}
				#endif
				trap_S_StartSoundVControl( NULL, es->number, CHAN_VOICE, CG_CustomSound( es->number, s ), volume );
			}
		}
		break;

		
	case EV_GLOBAL_TEAM_SOUND:
		DEBUGNAME("EV_GLOBAL_TEAM_SOUND");
		if( cgs.clientinfo[ cg.snap->ps.clientNum ].team != es->teamNum ) {
			break;
		}
	case EV_GLOBAL_SOUND:	// play from the player's head so it never diminishes
		DEBUGNAME("EV_GLOBAL_SOUND");
        if(cg.tcePortalScopeRendering)break;
		// Ridah, check for a sound script
		s = CG_ConfigString( CS_SOUNDS + es->eventParm );
		if( !strstr( s, ".wav" ) ) {
			if( CG_SoundPlaySoundScript( s, NULL, -1, qtrue ) ) {
				break;
			}

			// try with .wav
			Q_strncpyz( tempStr, s, sizeof(tempStr) );
			Q_strcat( tempStr, sizeof(tempStr), ".wav" );
			s = tempStr;
		}

		if ( cgs.gameSounds[ es->eventParm ] ) {
			#if defined(_MSC_VER) && defined(_M_IX86)
			int globalSoundHandle = cgs.gameSounds[ es->eventParm ];
			int globalSoundVolume = CG_EventGlobalSoundVolume();
			trap_S_StartSoundVControl( NULL, cg.snap->ps.clientNum, CHAN_AUTO, globalSoundHandle, globalSoundVolume );
			#else
			trap_S_StartSoundVControl( NULL, cg.snap->ps.clientNum, CHAN_AUTO, cgs.gameSounds[ es->eventParm ], (int)((1.0-(double)tceFlash.deafness)*127.0) );
			#endif
		} else {
			s = CG_ConfigString( CS_SOUNDS + es->eventParm );
			#if defined(_MSC_VER) && defined(_M_IX86)
			{
				int globalSoundVolume = CG_EventGlobalSoundVolume();
				int globalSoundHandle = CG_CustomSound( es->number, s );
				trap_S_StartSoundVControl( NULL, cg.snap->ps.clientNum, CHAN_AUTO, globalSoundHandle, globalSoundVolume );
			}
			#else
			trap_S_StartSoundVControl( NULL, cg.snap->ps.clientNum, CHAN_AUTO, CG_CustomSound( es->number, s ), (int)((1.0-(double)tceFlash.deafness)*127.0) );
			#endif
		}
		break;

	// DHM - Nerve
	case EV_GLOBAL_CLIENT_SOUND:
		DEBUGNAME("EV_GLOBAL_CLIENT_SOUND");
        if(cg.tcePortalScopeRendering)break;

		if ( cg.snap->ps.clientNum == es->teamNum ) {
			s = CG_ConfigString( CS_SOUNDS + es->eventParm );
			if ( !strstr( s, ".wav" ) ) {
				if( CG_SoundPlaySoundScript( s, NULL, -1, (es->effect1Time ? qfalse : qtrue) ) ) {
					break;
				}
				// try with .wav
				Q_strncpyz( tempStr, s, sizeof(tempStr) );
				Q_strcat( tempStr, sizeof(tempStr), ".wav" );
				s = tempStr;
			}
			// done.
			if ( cgs.gameSounds[ es->eventParm ] ) {
				#if defined(_MSC_VER) && defined(_M_IX86)
				int globalSoundHandle = cgs.gameSounds[ es->eventParm ];
				int globalSoundVolume = CG_EventGlobalSoundVolume();
				trap_S_StartSoundVControl( NULL, cg.snap->ps.clientNum, CHAN_AUTO, globalSoundHandle, globalSoundVolume );
				#else
				trap_S_StartSoundVControl (NULL, cg.snap->ps.clientNum, CHAN_AUTO, cgs.gameSounds[ es->eventParm ], (int)((1.0-(double)tceFlash.deafness)*127.0) );
				#endif
			} else {
				s = CG_ConfigString( CS_SOUNDS + es->eventParm );
				#if defined(_MSC_VER) && defined(_M_IX86)
				{
					int globalSoundVolume = CG_EventGlobalSoundVolume();
					int globalSoundHandle = CG_CustomSound( es->number, s );
					trap_S_StartSoundVControl( NULL, cg.snap->ps.clientNum, CHAN_AUTO, globalSoundHandle, globalSoundVolume );
				}
				#else
				trap_S_StartSoundVControl (NULL, cg.snap->ps.clientNum, CHAN_AUTO, CG_CustomSound( es->number, s ), (int)((1.0-(double)tceFlash.deafness)*127.0) );
				#endif
			}
		}

		break;
	// dhm - end

	case EV_PAIN:
		// local player sounds are triggered in CG_CheckLocalSounds,
		// so ignore events on the player
		DEBUGNAME("EV_PAIN");
		if ( cent->currentState.number != cg.snap->ps.clientNum ) {
			CG_PainEvent( cent, es->eventParm, qfalse );
		}
		break;

	case EV_CROUCH_PAIN:
		// local player sounds are triggered in CG_CheckLocalSounds,
		// so ignore events on the player
		DEBUGNAME("EV_PAIN");
		if ( cent->currentState.number != cg.snap->ps.clientNum ) {
			CG_PainEvent( cent, es->eventParm, qtrue );
		}
		break;

    case EV_DEATH1:
    case EV_DEATH2:
    case EV_DEATH3:
        DEBUGNAME("EV_DEATHx");
        if(!cg.tcePortalScopeRendering) {
#if defined(_MSC_VER) && defined(_M_IX86)
            int deathVolume = CG_EventGlobalSoundVolume();
            const char *deathName = va("*death%i.wav",event-EV_DEATH1+1);
            int deathSound = CG_CustomSound(es->number,deathName);
            trap_S_StartSoundVControl(NULL,es->number,CHAN_VOICE,deathSound,deathVolume);
#else
            trap_S_StartSoundVControl(NULL,es->number,CHAN_VOICE,
                CG_CustomSound(es->number,va("*death%i.wav",event-EV_DEATH1+1)),
                (int)((1.0-(double)tceFlash.deafness)*127.0));
#endif
        }
        break;

	case EV_OBITUARY:
		DEBUGNAME("EV_OBITUARY");
		CG_Obituary( es );
		break;

	// JPW NERVE -- swiped from SP/Sherman
    case EV_STOPSTREAMINGSOUND:
        DEBUGNAME("EV_STOPLOOPINGSOUND");
        if(!cg.tcePortalScopeRendering) {
#if defined(_MSC_VER) && defined(_M_IX86)
            int stopVolume = CG_EventGlobalSoundVolume();
            trap_S_StartSoundExVControl(NULL,es->number,CHAN_WEAPON,0,8,stopVolume);
#else
            trap_S_StartSoundExVControl(NULL,es->number,CHAN_WEAPON,0,8,
                (int)((1.0-(double)tceFlash.deafness)*127.0));
#endif
        }
        break;
    case EV_LOSE_HAT:
        DEBUGNAME("EV_LOSE_HAT");
        /* TC deliberately does not spawn the SDK helmet effect. */
        break;
    case EV_GIB_PLAYER:
        DEBUGNAME("EV_GIB_PLAYER");
        if(!cg.tcePortalScopeRendering) {
#if defined(_MSC_VER) && defined(_M_IX86)
            int gibVolume = CG_EventGlobalSoundVolume();
            trap_S_StartSoundVControl(es->pos.trBase,-1,CHAN_AUTO,cgs.media.gibSound,gibVolume);
#else
            trap_S_StartSoundVControl(es->pos.trBase,-1,CHAN_AUTO,cgs.media.gibSound,
                (int)((1.0-(double)tceFlash.deafness)*127.0));
#endif
            ByteToDir(es->eventParm,dir);
            CG_GibPlayer(cent,cent->lerpOrigin,dir);
        }
        break;

	case EV_STOPLOOPINGSOUND:
		DEBUGNAME("EV_STOPLOOPINGSOUND");
		es->loopSound = 0;
		break;

	case EV_DEBUG_LINE:
		DEBUGNAME("EV_DEBUG_LINE");
		CG_Beam( cent );
		break;

	// Rafael particles
	case EV_SMOKE:
		DEBUGNAME("EV_SMOKE");
		if (cent->currentState.density == 3) {
			CG_ParticleSmoke (cgs.media.smokePuffShaderdirty, cent);
		} else if (!(cent->currentState.density)) {
			CG_ParticleSmoke (cgs.media.smokePuffShader, cent);
		} else {
			CG_ParticleSmoke (cgs.media.smokePuffShader, cent);
		}
		break;

	case EV_FLAMETHROWER_EFFECT:
				CG_FireFlameChunks( cent, cent->currentState.origin, cent->currentState.apos.trBase, 0.6, 2 );
		break;

	case EV_DUST:
		CG_ParticleDust (cent, cent->currentState.origin, cent->currentState.angles);
		break;

	case EV_RUMBLE_EFX:
		{
			float	pitch, yaw;
			pitch = cent->currentState.angles[0];
			yaw = cent->currentState.angles[1];
			CG_RumbleEfx ( pitch, yaw );
		}
		break;

	case EV_CONCUSSIVE:
		CG_Concussive (cent);
		break;

	case EV_EMITTER:
		{
			localEntity_t	*le;
			le = CG_AllocLocalEntity();
			le->leType = LE_EMITTER;
			le->startTime = cg.time;
			le->endTime = le->startTime + 20000;
			le->pos.trType = TR_STATIONARY;
			VectorCopy( cent->currentState.origin, le->pos.trBase );
			VectorCopy( cent->currentState.origin2, le->angles.trBase );
			le->ownerNum = 0;
		}
		break;

	case EV_OILPARTICLES:
		CG_Particle_OilParticle (cgs.media.oilParticle, cent->currentState.origin, cent->currentState.origin2, cent->currentState.time, cent->currentState.density);
		break;
	case EV_OILSLICK:
		CG_Particle_OilSlick (cgs.media.oilSlick, cent);
		break;
	case EV_OILSLICKREMOVE:
		CG_OilSlickRemove (cent);
		break;

	case EV_MG42EFX:
		CG_MG42EFX (cent);
		break;
	
	case EV_SPARKS_ELECTRIC:
	case EV_SPARKS:
		{
			int numsparks;
			int	i;
			int	duration;
			float	x,y;
			float	speed;
			vec3_t	source, dest;

			if (!(cent->currentState.density))
				cent->currentState.density = 1;
			numsparks = rand()%cent->currentState.density;
			duration = cent->currentState.frame;
			x = cent->currentState.angles2[0];
			y = cent->currentState.angles2[1];
			speed = cent->currentState.angles2[2];

			if (!numsparks)
				numsparks = 1;
			for (i=0; i<numsparks; i++)
			{
				
				if (event == EV_SPARKS_ELECTRIC)
				{
					VectorCopy (cent->currentState.origin, source);
				
					VectorCopy (source, dest);
					dest[0] += ((rand()&31)-16);
					dest[1] += ((rand()&31)-16);
					dest[2] += ((rand()&31)-16);

					CG_Tracer (source, dest, 1);
				}
				else
					CG_ParticleSparks (cent->currentState.origin, cent->currentState.angles, duration, x, y, speed);
				
			}
			
		}
		break;

	case EV_GUNSPARKS:
		{
#if defined(_MSC_VER) && defined(_M_IX86)
			float *gunSparkSpeed = &cent->currentState.angles2[2];
			float *gunSparkOrigin = cent->currentState.origin;
			float *gunSparkDirection = cent->currentState.angles;
			int gunSparkCount = cent->currentState.density;
			/* Original 3003a227..3003a251: six arguments, signed64
			 * truncation followed by low32, not a direct signed32 cast. */
			__asm {
				mov edx, gunSparkCount
				mov eax, gunSparkSpeed
				push 3f800000h
				fld dword ptr [eax]
				push edx
				push 800
				call CG_EventTruncateST0
				push eax
				push gunSparkDirection
				push gunSparkOrigin
				call CG_AddBulletParticles
				add esp, 24
			}
#else
			int	numsparks;
			int	speed;
			//int	count;

			numsparks = cent->currentState.density;
			speed = cent->currentState.angles2[2];
			
			CG_AddBulletParticles( cent->currentState.origin, cent->currentState.angles, speed, 800, numsparks, 1.0f ); 
#endif
				
		}
		break;

	// Rafael snow pvs check
	case EV_SNOW_ON:
		CG_SnowLink (cent, qtrue);
		break;

	case EV_SNOW_OFF:
		CG_SnowLink (cent, qfalse);
		break;

	
	case EV_SNOWFLURRY:
		CG_ParticleSnowFlurry (cgs.media.snowShader, cent);
		break;

	// for func_exploding
	case EV_EXPLODE:
		DEBUGNAME("EV_EXPLODE");
		ByteToDir( es->eventParm, dir );
		CG_Explode(cent, position, dir, 0);
		break;

	case EV_RUBBLE:
		DEBUGNAME("EV_RUBBLE");
		ByteToDir( es->eventParm, dir );
		CG_Rubble(cent, position, dir, 0);
		break;

		// for target_effect
	case EV_EFFECT:
		DEBUGNAME("EV_EFFECT");
		ByteToDir( es->eventParm, dir );
		CG_Effect(cent, position, dir);
		break;

	case EV_MORTAREFX:	// mortar firing
		DEBUGNAME("EV_MORTAREFX");
		CG_MortarEFX (cent);
		break;

	case EV_SHARD:
		ByteToDir( es->eventParm, dir );
		CG_Shard(cent, position, dir);
		break;

	case EV_JUNK:
		ByteToDir (es->eventParm, dir);
		{
			int i;
			int	rval;

			rval = rand()%3 + 3;

			for (i=0; i<rval; i++)
				CG_ShardJunk (cent, position, dir);
		}
		break;

	case EV_DISGUISE_SOUND:
        if(cg.tcePortalScopeRendering)break;
		trap_S_StartSound( NULL, cent->currentState.number, CHAN_WEAPON, cgs.media.uniformPickup );
		break;
	case EV_BUILDDECAYED_SOUND:
        if(cg.tcePortalScopeRendering)break;
		trap_S_StartSound( cent->lerpOrigin, cent->currentState.number, CHAN_AUTO, cgs.media.buildDecayedSound );
		break;

	// Gordon: debris test
	case EV_DEBRIS:
		CG_Debris( cent, position, cent->currentState.origin2 );
		break;
	// ===================

	case EV_SHAKE:
        if(cg.tcePortalScopeRendering)break;
		{
			vec3_t v;
#if defined(_MSC_VER) && defined(_M_IX86)
			float *shakePlayerOrigin = cg.snap->ps.origin;
			float *shakeEventOrigin = cent->lerpOrigin;
			int *shakeRadius = &cent->currentState.onFireStart;
			float shakeStrength;
			static const float shakeOne = 1.0f;
#else
			float len;
#endif
			
			DEBUGNAME("EV_SHAKE");
#if defined(_MSC_VER) && defined(_M_IX86)
			/* Original 3003a478..3003a4f9 keeps VectorLength's ST0 return. */
			__asm {
				mov eax, shakePlayerOrigin
				mov edx, shakeEventOrigin
				lea ecx, v
				fld dword ptr [eax]
				fsub dword ptr [edx]
				fstp dword ptr [ecx]
				fld dword ptr [eax+4]
				fsub dword ptr [edx+4]
				fstp dword ptr [ecx+4]
				fld dword ptr [eax+8]
				fsub dword ptr [edx+8]
				push ecx
				fstp dword ptr [ecx+8]
				call VectorLength
				mov ecx, shakeRadius
				fild dword ptr [ecx]
				fld st(1)
				add esp, 4
				fcomp st(1)
				fnstsw ax
				test ah, 41h
				jz shakeOutsideRadius
				fxch st(1)
				fdiv st(0), st(1)
				fsubr shakeOne
				fstp shakeStrength
				fstp st(0)
				fld shakeOne
				fcomp shakeStrength
				fnstsw ax
				test ah, 1
				jz shakeCallCamera
				mov shakeStrength, 3f800000h
			shakeCallCamera:
				push shakeStrength
				call CG_StartShakeCamera
				add esp, 4
				jmp shakeEventDone
			shakeOutsideRadius:
				fstp st(0)
				fstp st(0)
			shakeEventDone:
			}
#else
			VectorSubtract( cg.snap->ps.origin, cent->lerpOrigin, v );
			len = VectorLength (v);

			if(len > cent->currentState.onFireStart) {
				break;
			}

			len = 1.0f - (len / (float)cent->currentState.onFireStart);
			len = min(1.f, len);

			CG_StartShakeCamera( len );
#endif
		}

		break;

	case EV_ALERT_SPEAKER:
		DEBUGNAME("EV_ALERT_SPEAKER");
		switch( cent->currentState.otherEntityNum2 )
		{
		case 1:		CG_UnsetActiveOnScriptSpeaker( cent->currentState.otherEntityNum );	break;
		case 2:		CG_SetActiveOnScriptSpeaker( cent->currentState.otherEntityNum );	break;
		case 0:
		default:	CG_ToggleActiveOnScriptSpeaker( cent->currentState.otherEntityNum );	break;
		}
		break;

	case EV_POPUPMESSAGE:
		{
			const char* str = CG_GetPMItemText( cent );
			qhandle_t shader = CG_GetPMItemIcon( cent );
			if( str ) {
				CG_AddPMItem( cent->currentState.effect1Time, str, shader );
			}
			CG_PlayPMItemSound( cent );
		}
		break;

	case EV_AIRSTRIKEMESSAGE:
		{
			const char* wav = NULL;

			switch( cent->currentState.density ) {
				case 0: // too many called
					if( cgs.clientinfo[ cg.snap->ps.clientNum ].team == TEAM_AXIS ) {
						wav = "axis_hq_airstrike_denied";
					} else {
						wav = "allies_hq_airstrike_denied";
					}
					break;
				case 1: // aborting can't see target
					if( cgs.clientinfo[ cg.snap->ps.clientNum ].team == TEAM_AXIS ) {
						wav = "axis_hq_airstrike_abort";
					} else {
						wav = "allies_hq_airstrike_abort";
					}
					break;
				case 2: // firing for effect
					if( cgs.clientinfo[ cg.snap->ps.clientNum ].team == TEAM_AXIS ) {
						wav = "axis_hq_airstrike";
					} else {
						wav = "allies_hq_airstrike";
					}
					break;
			}

			if( wav ) {
				CG_SoundPlaySoundScript( wav, NULL, -1, (es->effect1Time ? qfalse : qtrue) );
			}
		}
		break;

	case EV_ARTYMESSAGE:
		{
			const char* wav = NULL;

			switch( cent->currentState.density ) {
				case 0: // too many called
					if( cgs.clientinfo[ cg.snap->ps.clientNum ].team == TEAM_AXIS ) {
						wav = "axis_hq_ffe_denied";
					} else {
						wav = "allies_hq_ffe_denied";
					}
					break;
				case 1: // aborting can't see target
					if( cgs.clientinfo[ cg.snap->ps.clientNum ].team == TEAM_AXIS ) {
						wav = "axis_hq_ffe_abort";
					} else {
						wav = "allies_hq_ffe_abort";
					}
					break;
				case 2: // firing for effect
					if( cgs.clientinfo[ cg.snap->ps.clientNum ].team == TEAM_AXIS ) {
						wav = "axis_hq_ffe";
					} else {
						wav = "allies_hq_ffe";
					}
					break;
			}

			if( wav ) {
				CG_SoundPlaySoundScript( wav, NULL, -1, (es->effect1Time ? qfalse : qtrue) );
			}
		}
		break;

	case EV_MEDIC_CALL:
		switch( cgs.clientinfo[ cent->currentState.number ].team ) {
			case TEAM_AXIS:
				trap_S_StartSound( NULL, cent->currentState.number, CHAN_AUTO, cgs.media.sndMedicCall[0] );
				break;
			case TEAM_ALLIES:
				trap_S_StartSound( NULL, cent->currentState.number, CHAN_AUTO, cgs.media.sndMedicCall[1] );
				break;
			default: // shouldn't happen
				break;
		}

		break;

	default:
		DEBUGNAME("UNKNOWN");
		CG_Error( "Unknown event: %i", event );
		break;
	}

	
	{
		int	rval;

		rval = rand()&3;
				
		if (splashfootstepcnt != rval)
			splashfootstepcnt = rval;
		else
			splashfootstepcnt++;
		
		if (splashfootstepcnt > 3)
			splashfootstepcnt = 0;
	

		if (footstepcnt != rval)
			footstepcnt = rval;
		else
			footstepcnt++;

		if (footstepcnt > 3)
			footstepcnt = 0;
	}
}


/*
==============
CG_CheckEvents

==============
*/
void CG_CheckEvents( centity_t *cent ) {
	int i, event;

	// calculate the position at exactly the frame time
	BG_EvaluateTrajectory( &cent->currentState.pos, cg.snap->serverTime, cent->lerpOrigin, qfalse, cent->currentState.effect2Time );
	CG_SetEntitySoundPosition( cent );

	// check for event-only entities
	if ( cent->currentState.eType > ET_EVENTS ) {
		if ( cent->previousEvent ) {
			//goto skipEvent;
			return;	// already fired
		}
		// if this is a player event set the entity number of the client entity number
//(SA) note: EF_PLAYER_EVENT never set
//		if ( cent->currentState.eFlags & EF_PLAYER_EVENT ) {
//			cent->currentState.number = cent->currentState.otherEntityNum;
//		}

		cent->previousEvent = 1;

		cent->currentState.event = cent->currentState.eType - ET_EVENTS;
	} else {

		// DHM - Nerve :: Entities that make it here are Not TempEntities.
		//		As far as we could tell, for all non-TempEntities, the
		//		circular 'events' list contains the valid events.  So we
		//		skip processing the single 'event' field and go straight
		//		to the circular list.

		goto skipEvent;
		/*
		// check for events riding with another entity
		if ( cent->currentState.event == cent->previousEvent ) {
			goto skipEvent;
			//return;
		}
		cent->previousEvent = cent->currentState.event;
		if ( ( cent->currentState.event & ~EV_EVENT_BITS ) == 0 ) {
			goto skipEvent;
			//return;
		}
		*/
		// dhm - end
	}

	CG_EntityEvent( cent, cent->lerpOrigin );
	// DHM - Nerve :: Temp ents return after processing
	return;

skipEvent:

	// check the sequencial list
	// if we've added more events than can fit into the list, make sure we only add them once
	if (cent->currentState.eventSequence < cent->previousEventSequence) {
		cent->previousEventSequence -= (1 << 8);	// eventSequence is sent as an 8-bit through network stream
	}
	if (cent->currentState.eventSequence - cent->previousEventSequence > MAX_EVENTS) {
		cent->previousEventSequence = cent->currentState.eventSequence - MAX_EVENTS;
	}
	for ( i = cent->previousEventSequence ; i != cent->currentState.eventSequence; i++ ) {
		event = cent->currentState.events[ i & (MAX_EVENTS-1) ];

		cent->currentState.event = event;
		cent->currentState.eventParm = cent->currentState.eventParms[ i & (MAX_EVENTS-1) ];
		CG_EntityEvent( cent, cent->lerpOrigin );
	}
	cent->previousEventSequence = cent->currentState.eventSequence;

	// set the event back so we don't think it's changed next frame (unless it really has)
	cent->currentState.event = cent->previousEvent;
}
