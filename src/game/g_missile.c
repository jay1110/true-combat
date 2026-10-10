#include "g_local.h"
#include "tce_trajectory.h"

/* All original missile controllers call the six-argument TC evaluator.
 * In particular trajectory13 is ballistic, not the SDK spline enum alias. */
static void TCE_MissilePosition(gentity_t *ent,int time,vec3_t out) {
    TCE_BG_EvaluateTrajectory(&ent->s.pos,time,out,qfalse,ent->s.effect2Time,1);
}
static void TCE_MissileVelocity(gentity_t *ent,int time,vec3_t out) {
    TCE_BG_EvaluateTrajectoryDelta(&ent->s.pos,time,out,qfalse,ent->s.effect2Time,1);
}

#define	MISSILE_PRESTEP_TIME	50


extern void gas_think (gentity_t *gas);
extern void gas_touch (gentity_t *gas, gentity_t *other, trace_t *trace);
extern void SP_target_smoke (gentity_t *ent);

void M_think (gentity_t *ent);
void G_ExplodeMissile( gentity_t *ent );

/*
================
G_BounceMissile

================
*/
/* TC Windows2006a3d0 / Linux000cb27c. */
void G_BounceMissile(gentity_t *ent,trace_t *trace) {
    vec3_t velocity;
    double reflectedScale;
    int hitTime,i;
    gentity_t *ground;
    if(ent->s.weapon==55 || ent->s.weapon==56) {
        ent->s.effect1Time=qtrue;
        if(ent->nextthink-level.time<3250) { G_ExplodeMissile(ent);return; }
    }
    /* Windows2006a443/446 retain fractional time until __ftol. */
    hitTime=(int)((double)level.previousTime+(double)(level.time-level.previousTime)*trace->fraction);
    TCE_MissileVelocity(ent,hitTime,velocity);
    /* Original x87 order is X+Z+Y, with no float spill of the reflection scale. */
    reflectedScale=-2.0*((double)velocity[0]*trace->plane.normal[0]+
        (double)velocity[2]*trace->plane.normal[2]+(double)velocity[1]*trace->plane.normal[1]);
    for(i=0;i<3;i++)ent->s.pos.trDelta[i]=(float)(velocity[i]+reflectedScale*trace->plane.normal[i]);
    if(trace->plane.normal[2]>.2)ent->s.groundEntityNum=trace->entityNum;
    ground=ent->s.groundEntityNum==-1?NULL:&g_entities[ent->s.groundEntityNum];
    if(ent->s.groundEntityNum!=ENTITYNUM_WORLD && ground)
        for(i=0;i<3;i++)ent->s.pos.trDelta[i]=(float)((double)ent->s.pos.trDelta[i]+(double).85f*ground->instantVelocity[i]);
    if(ent->s.eFlags&EF_BOUNCE_HALF) {
        /* TC coefficients differ from SDK .35/.65. */
        VectorScale(ent->s.pos.trDelta,(ent->s.eFlags&EF_BOUNCE)?.25f:.5f,ent->s.pos.trDelta);
        if(ent->s.groundEntityNum!=ENTITYNUM_WORLD)
            VectorScale(ent->s.pos.trDelta,.5f,ent->s.pos.trDelta);
        if(trace->plane.normal[2]>.2 &&
            (double)ent->s.pos.trDelta[0]*ent->s.pos.trDelta[0]+
            (double)ent->s.pos.trDelta[1]*ent->s.pos.trDelta[1]+
            (double)ent->s.pos.trDelta[2]*ent->s.pos.trDelta[2]<1600) {
            if(ent->s.weapon==15 || ent->s.weapon==26 || ent->s.weapon==27 || ent->s.weapon==29 || ent->s.weapon==30)
                ent->r.ownerNum=ENTITYNUM_WORLD;
            G_SetOrigin(ent,trace->endpos);ent->s.time=level.time;
            if(ent->s.weapon==55 || ent->s.weapon==56)ent->nextthink-=3250;
            else if(ent->s.weapon==15) {
                VectorSet(ent->r.mins,-16,-16,0);VectorCopy(ent->r.mins,ent->r.absmin);
                VectorSet(ent->r.maxs,16,16,20);VectorCopy(ent->r.maxs,ent->r.absmax);
            }
            return;
        }
    }
    SnapVector(ent->s.pos.trDelta);
    VectorAdd(ent->r.currentOrigin,trace->plane.normal,ent->r.currentOrigin);
    VectorCopy(ent->r.currentOrigin,ent->s.pos.trBase);
    SnapVector(ent->s.pos.trBase);ent->s.pos.trTime=level.time;
}

/*
================
G_MissileImpact
	impactDamage is how much damage the impact will do to func_explosives
================
*/
/* TC Windows2006a770: whole impact controller, same damage callback ABI
 * as the reconstructed float-returning G_Damage (return intentionally unused). */
void G_MissileImpact(gentity_t *ent,trace_t *trace,int impactDamage) {
    gentity_t *other=&g_entities[trace->entityNum],*temp;
    vec3_t velocity;
    int event,parm,othernum=0;
    if(other->classname && !Q_stricmp(other->classname,"func_explosive") &&
        other->health && other->health<=impactDamage) {
        if(other->takedamage) {
            TCE_MissileVelocity(ent,level.time,velocity);
            G_Damage(other,ent,&g_entities[ent->r.ownerNum],velocity,ent->s.origin,impactDamage,0,ent->methodOfDeath);
        }
        if(other->health<1)return;
    }
    if((!other->takedamage || !ent->damage) && (ent->s.eFlags&(EF_BOUNCE|EF_BOUNCE_HALF))) {
        G_BounceMissile(ent,trace);
        if(!Q_stricmp(ent->classname,"WP"))return;
        G_AddEvent(ent,EV_GRENADE_BOUNCE,BG_FootstepForSurface(trace->surfaceFlags));return;
    }
    if(other->takedamage || other->dmgparent) {
        if(!ent->damage) { G_BounceMissile(ent,trace);return; }
        AccuracyHit(other,&g_entities[ent->r.ownerNum]);
        TCE_MissileVelocity(ent,level.time,velocity);
        if(!VectorLengthSquared(velocity))velocity[2]=1;
        G_Damage(other->dmgparent?other->dmgparent:other,ent,&g_entities[ent->r.ownerNum],
            velocity,ent->s.origin,ent->damage,0,ent->methodOfDeath);
    }
    if(other->takedamage && other->client) {
        event=EV_MISSILE_HIT;parm=DirToByte(trace->plane.normal);othernum=other->s.number;
    } else {
        TCE_MissileVelocity(ent,level.time,velocity);
        BG_GetMarkDir(velocity,trace->plane.normal,velocity);
        event=EV_MISSILE_MISS;parm=DirToByte(velocity);
    }
    temp=G_TempEntity(trace->endpos,event);temp->s.eventParm=parm;
    temp->s.otherEntityNum=othernum;temp->s.weapon=ent->s.weapon;temp->s.clientNum=ent->r.ownerNum;
    if(ent->s.weapon==60) { temp->s.legsAnim=ent->s.legsAnim;temp->r.svFlags|=SVF_BROADCAST; }
    if(ent->splashDamage)G_RadiusDamage(trace->endpos,ent,ent->parent,ent->splashDamage,
        ent->splashRadius,other,ent->splashMethodOfDeath);
    G_FreeEntity(ent);
}

/*
==============
Concussive_think
==============
*/

/*
==============
M_think
==============
*/
void M_think (gentity_t *ent)
{
	gentity_t *tent;
	
	ent->count ++;

	if (ent->count == ent->health)
		ent->think = G_FreeEntity;

	tent = G_TempEntity (ent->s.origin, EV_SMOKE);
	VectorCopy (ent->s.origin, tent->s.origin);
	if (ent->s.density == 1)
		tent->s.origin[2]+=16;
	else
		// tent->s.origin[2]+=32;
		// Note to self Maxx said to lower the spawn loc for the smoke 16 units
		tent->s.origin[2]+=16;

	tent->s.time = 3000;
	tent->s.time2 = 100;
	tent->s.density = 0;
	if (ent->s.density == 1)
		tent->s.angles2[0] = 16;
	else
		// Note to self Maxx changed this to 24
		tent->s.angles2[0] = 24;
	tent->s.angles2[1] = 96;
	tent->s.angles2[2] = 50;

	ent->nextthink = level.time + FRAMETIME;

}

/*
================
G_ExplodeMissile

Explode a missile without an impact
================
*/
/* TC Windows2006aab0 / Linux G_ExplodeMissile: complete controller.
 * Events are translated to this source's event enum; weapon numbers are TC. */
void G_ExplodeMissile( gentity_t *ent ) {
    vec3_t origin,dir;
    int etype,i;
    qboolean smoke;
    if(ent->s.weapon==22 && ent->active)
        level.numActiveAirstrikes[ent->s.teamNum==TEAM_AXIS?0:1]--;
    etype=ent->s.eType;
    ent->s.eType=ET_GENERAL;
    TCE_MissilePosition(ent,level.time,origin);
    SnapVector(origin);
    smoke=!Q_stricmp(ent->classname,"smoke_grenade");
    if(!smoke)G_SetOrigin(ent,origin);
    VectorSet(dir,0,0,1);
    if(ent->accuracy==3) {
        ent->freeAfterEvent=qtrue;trap_LinkEntity(ent);return;
    }
    G_AddEvent(ent,ent->accuracy==1?EV_MISSILE_MISS_SMALL:
        ent->accuracy==2?EV_MISSILE_MISS_LARGE:EV_MISSILE_MISS,DirToByte(dir));
    if(ent->accuracy!=1 && ent->accuracy!=2)ent->s.clientNum=ent->r.ownerNum;
    if(smoke) {
        ent->think=G_FreeEntity;ent->s.eType=ET_MISSILE;
        ent->nextthink=level.time+40000;ent->s.otherEntityNum=17;
        ent->freeAfterEvent=qfalse;ent->s.time=level.time;
    } else ent->freeAfterEvent=qtrue;
    trap_LinkEntity(ent);
    if(etype!=ET_MISSILE && etype!=ET_BOMB)return;
    if(ent->s.weapon==26) {
        mapEntityData_t *m;
        for(i=0;i<2;i++) {
            m=G_FindMapEntityData(&mapEntityData[i],(int)(ent - g_entities));
            if(m)G_FreeMapEntityData(&mapEntityData[i],m);
        }
    } else if(ent->s.weapon==15) {
        vec3_t mins,maxs;
        int touch[MAX_GENTITIES],num;
        ent->free=NULL;
        for(i=0;i<3;i++) { mins[i]=ent->r.currentOrigin[i]-64;maxs[i]=ent->r.currentOrigin[i]+64; }
        num=trap_EntitiesInBox(mins,maxs,touch,MAX_GENTITIES);
        for(i=0;i<num;i++) {
            gentity_t *hit=&g_entities[touch[i]];
            if(!hit->target || hit->s.eType!=ET_OID_TRIGGER ||
                !(hit->spawnflags&(AXIS_OBJECTIVE|ALLIED_OBJECTIVE)))continue;
            if(hit->target_ent && (hit->target_ent->s.eType!=ET_EXPLOSIVE ||
                hit->target_ent->constructibleStats.weaponclass<1))continue;
            if(!(((hit->spawnflags&AXIS_OBJECTIVE)&&ent->s.teamNum==TEAM_ALLIES)||
                ((hit->spawnflags&ALLIED_OBJECTIVE)&&ent->s.teamNum==TEAM_AXIS)))continue;
            /* Original dereferences target_ent in this scoring path; preserve
             * valid-map behavior and avoid a crash on an absent target. */
            if(ent->parent && ent->parent->client && hit->target_ent &&
                G_GetWeaponClassForMOD(26)>=hit->target_ent->constructibleStats.weaponclass)
                G_AddKillSkillPointsForDestruction(ent->parent,26,&hit->target_ent->constructibleStats);
            level.tceExitRulesNotBefore=level.time+1000;
            G_UseTargets(hit,ent);
            hit->think=G_FreeEntity;hit->nextthink=level.time+100;
        }
    }
    /* TC applies splash after linking/event generation/objective activation. */
    if(ent->splashDamage) {
        trace_t tr;
        VectorCopy(ent->r.currentOrigin,origin);
        if(ent->s.weapon==15)origin[2]+=4;
        trap_Trace(&tr,origin,vec3_origin,vec3_origin,origin,ENTITYNUM_NONE,MASK_MISSILESHOT);
        if((ent->s.weapon==15 && (ent->etpro_misc_1&1)) || ent->s.weapon==27) {
            etpro_RadiusDamage(origin,ent,ent->parent,ent->splashDamage,ent->splashRadius,ent,ent->splashMethodOfDeath,qtrue);
            G_TempTraceIgnorePlayersAndBodies();
            etpro_RadiusDamage(origin,ent,ent->parent,ent->splashDamage,ent->splashRadius,ent,ent->splashMethodOfDeath,qfalse);
            G_ResetTempTraceIgnoreEnts();
        } else G_RadiusDamage(origin,ent,ent->parent,ent->splashDamage,ent->splashRadius,ent,ent->splashMethodOfDeath);
    }
    switch(ent->s.weapon) {
    case 15:case 65:case 9:case 17:case 63:case 22:case 26:case 27:case 29: {
        gentity_t *shake=G_TempEntity(ent->r.currentOrigin,EV_SHAKE);
        shake->s.onFireStart=ent->splashDamage*4;shake->r.svFlags|=SVF_BROADCAST;break;
    }
    default:break;
    }
}

/*
================
G_MissileDie
================
*/
void G_MissileDie( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int mod ) {
	if (inflictor == self)
		return;
	self->takedamage	= qfalse;
	self->think			= G_ExplodeMissile;
	self->nextthink		= level.time + 10;
}

/*
================
G_ExplodeMissilePoisonGas

Explode a missile without an impact
================
*/
/*void G_ExplodeMissilePoisonGas( gentity_t *ent ) {
	vec3_t		dir;
	vec3_t		origin;

	BG_EvaluateTrajectory( &ent->s.pos, level.time, origin );
	SnapVector( origin );
	G_SetOrigin( ent, origin );

	// we don't have a valid direction, so just point straight up
	dir[0] = dir[1] = 0;
	dir[2] = 1;

	ent->freeAfterEvent = qtrue;


	{
		gentity_t *gas;

		gas = G_Spawn();
		gas->think = gas_think;
		gas->nextthink = level.time + FRAMETIME;
		gas->r.contents = CONTENTS_TRIGGER;
		gas->touch = gas_touch;
		gas->health = 100;
		G_SetOrigin (gas, origin); 
		
		trap_LinkEntity (gas);
	}
	
}*/

/*
================
G_RunBomb
================
*/

// just sits about doing nothing but tracing
void G_RunBomb( gentity_t *ent ) {
	G_RunThink( ent );
}

/*
=================
Landmine_Check_Ground

=================
*/

void Landmine_Check_Ground (gentity_t *self)
{
	vec3_t	mins, maxs;
	vec3_t	start, end;
	trace_t	tr;

	VectorCopy (self->r.currentOrigin, start);
	VectorCopy (self->r.currentOrigin, end);

	end[2] -= 4;
	
	VectorCopy (self->r.mins, mins);
	VectorCopy (self->r.maxs, maxs);

	trap_Trace( &tr, start, mins, maxs, end, self->s.number, MASK_MISSILESHOT );

	if( tr.fraction == 1 )
		self->s.groundEntityNum = -1;

	/*vec3_t		oldorigin;
	vec3_t		origin;
	trace_t		tr;

	// Backup origin
	VectorCopy( self->r.currentOrigin, oldorigin );

	// See if we can fall down this frame
	self->s.pos.trType = TR_GRAVITY;
	self->s.pos.trTime = level.time;
	BG_EvaluateTrajectory( &self->s.pos, level.time, origin, qfalse, self->s.effect2Time );

	// This is likely overkill, but just in case (synced to G_RunMissile)
	if( (self->clipmask & CONTENTS_BODY) && (self->s.weapon == WP_DYNAMITE || self->s.weapon == WP_ARTY  || ent->s.weapon == WP_SMOKE_MARKER
		|| self->s.weapon == WP_GRENADE_LAUNCHER || self->s.weapon == WP_GRENADE_PINEAPPLE
		|| self->s.weapon == WP_LANDMINE || self->s.weapon == WP_SATCHEL || self->s.weapon == WP_SMOKE_BOMB
		) ) {

		if( !self->s.pos.trDelta[0] && !self->s.pos.trDelta[1] && !self->s.pos.trDelta[2] ) {
			self->clipmask &= ~CONTENTS_BODY;
		}
	}

	// trace a line from the previous position to the current position,
	// ignoring interactions with the missile owner
	trap_Trace( &tr, self->r.currentOrigin, self->r.mins, self->r.maxs, origin, 
		self->r.ownerNum, self->clipmask );

	if (tr.fraction == 1)
		self->s.groundEntityNum = -1;

	// Reset origin
	G_SetOrigin( self, oldorigin );*/
}


/*
================
G_RunMissile
================
*/
/* TC Windows2006b060 / Linux000cc406: full controller with TC slot IDs. */
void G_RunMissile( gentity_t *ent ) {
	vec3_t		origin;
	trace_t		tr;
	int			impactDamage;

	if( ent->s.weapon == 26 || ent->s.weapon == 15 || ent->s.weapon == 27 ) {
		Landmine_Check_Ground( ent );

		if ( ent->s.groundEntityNum == -1 ) {
			if ( ent->s.pos.trType != TR_GRAVITY ) {
				ent->s.pos.trType = TR_GRAVITY;
				ent->s.pos.trTime = level.time;
			}
		}
	}

	// get current position
	TCE_MissilePosition(ent,level.time,origin);

	if( (ent->clipmask & CONTENTS_BODY) && (ent->s.weapon == 15 || ent->s.weapon == 63 || ent->s.weapon == 22
		|| ent->s.weapon == 4 || ent->s.weapon == 9
		|| ent->s.weapon == 26 || ent->s.weapon == 27 || ent->s.weapon == 30
		) ) {

		if( !ent->s.pos.trDelta[0] && !ent->s.pos.trDelta[1] && !ent->s.pos.trDelta[2] ) {
			ent->clipmask &= ~CONTENTS_BODY;
		}
	}

	if( level.tracemapLoaded &&
		( ent->s.weapon == 60 ||
		  ent->s.weapon == 55 ||
		  ent->s.weapon == 56 ||
		  ent->s.weapon == 4 ||
		  ent->s.weapon == 9 ) ) {
		if( ent->count ) {
			if( ent->r.currentOrigin[0] < level.mapcoordsMins[0] ||
				ent->r.currentOrigin[1] > level.mapcoordsMins[1] ||
				ent->r.currentOrigin[0] > level.mapcoordsMaxs[0] ||
				ent->r.currentOrigin[1] < level.mapcoordsMaxs[1] ) {
				gentity_t *tent;

				tent = G_TempEntity( ent->r.currentOrigin, EV_MORTAR_MISS );
				tent->s.clientNum = ent->r.ownerNum;
				tent->r.svFlags |= SVF_BROADCAST;
				tent->s.density = 1;	// angular

				G_FreeEntity( ent );
				return;
			} else {
				float skyHeight = BG_GetSkyHeightAtPoint( origin );

				if( origin[2] < BG_GetTracemapGroundFloor() ) {
					gentity_t *tent;

					tent = G_TempEntity( ent->r.currentOrigin, EV_MORTAR_MISS );
					tent->s.clientNum = ent->r.ownerNum;
					tent->r.svFlags |= SVF_BROADCAST;
					tent->s.density = 0;	// direct

					G_FreeEntity( ent );
					return;			
				}

				// are we in worldspace again - or did we hit a ceiling from the outside of the world
				if( skyHeight == 65536 ) {
		//			if( BG_GetSkyGroundHeightAtPoint( origin ) >= origin[2] ) {
		//				G_FreeEntity( ent );
		//				return;
		//			} else {
						G_RunThink( ent );
						VectorCopy( origin, ent->r.currentOrigin );
		//				trap_LinkEntity( ent );
						return; // keep flying
		//			}
				}

				if( skyHeight <= origin[2] ) {
					G_RunThink( ent );
					return; // keep flying
				}

				// back in the world, keep going like normal
				VectorCopy( origin, ent->r.currentOrigin );
				ent->count = 0;
				ent->count2 = 1;
			}
		} else if( !ent->count2 && BG_GetSkyHeightAtPoint( origin ) - BG_GetGroundHeightAtPoint( origin ) > 512 ) {
			vec3_t delta;

			VectorSubtract( origin, ent->r.currentOrigin, delta );
			if( delta[2] < 0 )
				ent->count2 = 1;
		}
	}

	// trace a line from the previous position to the current position,
	// ignoring interactions with the missile owner
	/* TC glass surfaces are traced through their playerclip material first. */
    trap_Trace(&tr,ent->r.currentOrigin,ent->r.mins,ent->r.maxs,origin,ent->r.ownerNum,0x10000);
    if(tr.fraction==1 || (tr.surfaceFlags&0xff000000)!=0x14000000)
        trap_Trace(&tr,ent->r.currentOrigin,ent->r.mins,ent->r.maxs,origin,ent->r.ownerNum,ent->clipmask);
    else tr.surfaceFlags&=~SURF_NOIMPACT;

	if( ent->s.weapon == 60 && ent->count2 == 1 ) {
		if( ent->r.currentOrigin[2] > origin[2] && origin[2] - BG_GetGroundHeightAtPoint(origin) < 512 ) {
			vec3_t impactpos;
			trace_t mortar_tr;

			VectorSubtract( origin, ent->r.currentOrigin, impactpos );
			VectorMA( origin, 8, impactpos, impactpos );

			trap_Trace( &mortar_tr, origin, ent->r.mins, ent->r.maxs, impactpos, 
				ent->r.ownerNum, ent->clipmask );

			if( mortar_tr.fraction != 1 ) {
				gentity_t *tent;

				impactpos[2] = BG_GetGroundHeightAtPoint(impactpos);

				tent = G_TempEntity( impactpos, EV_MORTAR_IMPACT );
				tent->s.clientNum = ent->r.ownerNum;
				tent->r.svFlags |= SVF_BROADCAST;

				ent->count2 = 2;
				ent->s.legsAnim = 1;


				/*{
					gentity_t *tent;
					
					tent = G_TempEntity( origin, EV_RAILTRAIL );
					VectorCopy( impactpos, tent->s.origin2 );
					tent->s.dmgFlags = 0;

					tent = G_TempEntity( origin, EV_RAILTRAIL );
					VectorCopy( ent->r.currentOrigin, tent->s.origin2 );
					tent->s.dmgFlags = 0;
				}*/
			}
		}
	}

	VectorCopy( tr.endpos, ent->r.currentOrigin );

	if ( tr.startsolid ) {
		tr.fraction = 0;
	}

	trap_LinkEntity( ent );

	if ( tr.fraction != 1 ) {
		if( level.tracemapLoaded &&
			( ent->s.weapon == 60 ||
			  ent->s.weapon == 55 ||
			  ent->s.weapon == 56 || 
			  ent->s.weapon == 4 ||
              ent->s.weapon == 30 ||
			  ent->s.weapon == 9 )
			&& tr.surfaceFlags & SURF_SKY ) {
			// goes through sky
			ent->count = 1;
			trap_UnlinkEntity( ent );
			G_RunThink( ent );
			return; // keep flying
		} else
		// never explode or bounce on sky
		if( tr.surfaceFlags & SURF_NOIMPACT ) {
			// If grapple, reset owner
			if (ent->parent && ent->parent->client && ent->parent->client->hook == ent)
				ent->parent->client->hook = NULL;
			G_FreeEntity( ent );
			return;
		}

//		G_SetOrigin( ent, tr.endpos );

		if( ent->s.weapon == 65 || ent->s.weapon == 60 )
			impactDamage = 999;	// goes through pretty much any func_explosives
		else
			impactDamage = 20;	// "grenade"/"dynamite"		// probably adjust this based on velocity

		if( ent->s.weapon == 15 || ent->s.weapon == 26 || ent->s.weapon == 27 ) {
			if( ent->s.pos.trType != TR_STATIONARY )
				G_MissileImpact( ent, &tr, impactDamage );
		} else {
			G_MissileImpact( ent, &tr, impactDamage );
		}

		if ( ent->s.eType != ET_MISSILE ) {
			gentity_t* tent = G_TempEntity(ent->r.currentOrigin, EV_SHAKE);
			tent->s.onFireStart = ent->splashDamage * 4;
			tent->r.svFlags |= SVF_BROADCAST;
			return;		// exploded
		}
	} else if (VectorLengthSquared(ent->s.pos.trDelta)) {	// free fall/no intersection
		ent->s.groundEntityNum = ENTITYNUM_NONE;
	}

	// check think function after bouncing
	G_RunThink( ent );
}

/*
================
G_PredictBounceMissile

================
*/
void G_PredictBounceMissile( gentity_t *ent, trajectory_t *pos, trace_t *trace, int time ) {
	vec3_t	velocity, origin;
	float	dot;
	int		hitTime;

	TCE_BG_EvaluateTrajectory( pos, time, origin, qfalse, ent->s.effect2Time, 1.0f );

	// reflect the velocity on the trace plane
	hitTime = time;
	TCE_BG_EvaluateTrajectoryDelta( pos, hitTime, velocity, qfalse, ent->s.effect2Time, 1.0f );
	dot = DotProduct( velocity, trace->plane.normal );
	VectorMA( velocity, -2*dot, trace->plane.normal, pos->trDelta );

	if ( ent->s.eFlags & EF_BOUNCE_HALF ) {
		if(ent->s.eFlags & EF_BOUNCE) {		// both flags marked, do a third type of bounce
			VectorScale( pos->trDelta, 0.25f, pos->trDelta );
		} else {
			VectorScale( pos->trDelta, 0.5f, pos->trDelta );
		}

		// check for stop
		if ( trace->plane.normal[2] > 0.2f && VectorLengthSquared( pos->trDelta ) < SQR(40) ) {
			VectorCopy( trace->endpos, pos->trBase );
			return;
		}
	}

	VectorAdd( origin, trace->plane.normal, pos->trBase);
	pos->trTime = time;
}

/*
================
G_PredictMissile

  selfNum is the character that is checking to see what the missile is going to do

  returns qfalse if the missile won't explode, otherwise it'll return the time is it expected to explode
================
*/
int G_PredictMissile( gentity_t *ent, int duration, vec3_t endPos, qboolean allowBounce ) {
	vec3_t		origin;
	trace_t		tr;
	int			time;
	trajectory_t	pos;
	vec3_t		org;
	gentity_t	backupEnt;

	pos = ent->s.pos;
	TCE_BG_EvaluateTrajectory( &pos, level.time, org, qfalse, ent->s.effect2Time, 1.0f );

	backupEnt = *ent;

	for (time = level.time + FRAMETIME; time < level.time + duration; time+=FRAMETIME) {

		// get current position
		TCE_BG_EvaluateTrajectory( &pos, time, origin, qfalse, ent->s.effect2Time, 1.0f );

		// trace a line from the previous position to the current position,
		// ignoring interactions with the missile owner
		trap_Trace( &tr, org, ent->r.mins, ent->r.maxs, origin, 
			ent->r.ownerNum, ent->clipmask );

		VectorCopy( tr.endpos, org );

		if ( tr.startsolid ) {
			*ent = backupEnt;
			return qfalse;
		}

		if ( tr.fraction != 1 ) {
			// never explode or bounce on sky
			if	( tr.surfaceFlags & SURF_NOIMPACT ) {
				*ent = backupEnt;
				return qfalse;
			}

			if ( allowBounce && (ent->s.eFlags & ( EF_BOUNCE | EF_BOUNCE_HALF )) ) {
				G_PredictBounceMissile( ent, &pos, &tr, time - FRAMETIME + (int)((float)FRAMETIME*tr.fraction) );
				pos.trTime = time;
				continue;
			}

			// exploded, so drop out of loop
			break;
		}
	}
/*
	if (!allowBounce && tr.fraction < 1 && tr.entityNum > level.maxclients) {
		// go back a bit in time, so we can catch it in the air
		time -= 200;
		if (time < level.time + FRAMETIME)
			time = level.time + FRAMETIME;
		BG_EvaluateTrajectory( &pos, time, org );
	}
*/

	// get current position
	VectorCopy( org, endPos );
	// set the entity data back
	*ent = backupEnt;
	//
	if ( allowBounce && (ent->s.eFlags & ( EF_BOUNCE | EF_BOUNCE_HALF )) ) {
		return ent->nextthink;
	} else {	// it will probably explode before it times out
		return time;
	}
}

//=============================================================================
// DHM - Nerve :: Server side Flamethrower
//=============================================================================

// copied from cg_flamethrower.c
#define	FLAME_START_SIZE		1.0
#define	FLAME_START_MAX_SIZE	100.0	// when the flame is spawned, it should endevour to reach this size
#define	FLAME_START_SPEED		1200.0	// speed of flame as it leaves the nozzle
#define	FLAME_MIN_SPEED			60.0

// these are calculated (don't change)
#define	FLAME_LENGTH			(FLAMETHROWER_RANGE + 50.0)	// NOTE: only modify the range, since this should always reflect that range

#define	FLAME_LIFETIME			(int)((FLAME_LENGTH/FLAME_START_SPEED)*1000)	// life duration in milliseconds
#define	FLAME_FRICTION_PER_SEC	(2.0f*FLAME_START_SPEED)
#define	GET_FLAME_SIZE_SPEED(x)	(((float)x / FLAME_LIFETIME) / 0.3)	// x is the current sizeMax

#define	FLAME_THRESHOLD	50

void G_BurnTarget( gentity_t *self, gentity_t *body, qboolean directhit )
{
	int			i;
	float		radius, dist;
	vec3_t		point, v;
	trace_t		tr;

	if ( !body->takedamage )
		return;

	/* TC 2006b9f0 keeps invulnerability here; team damage is decided by
	 * G_Damage, not by an SDK-only early rejection of the burn effect. */
	if (body->client) {
		if (body->client->ps.powerups[PW_INVULNERABLE] >= level.time) {
			body->flameQuota = 0;
			body->s.onFireEnd = level.time-1;
			return;
		}

//		if( !self->count2 && body == self->parent )
//			return;

	}
// jpw

// JPW NERVE don't catch fire if under water or invulnerable
	if (body->waterlevel >= 3) {
		body->flameQuota = 0;
		body->s.onFireEnd = level.time-1;
		return;
	}
// jpw

	if (!body->r.bmodel) {
		VectorCopy( body->r.currentOrigin, point );
		if ( body->client )
			point[2] += body->client->ps.viewheight;
		VectorSubtract( point, self->r.currentOrigin, v );
	}
	else {
		for ( i = 0 ; i < 3 ; i++ ) {
			if ( self->s.origin[i] < body->r.absmin[i] ) {
				v[i] = body->r.absmin[i] - self->r.currentOrigin[i];
			} else if ( self->r.currentOrigin[i] > body->r.absmax[i] ) {
				v[i] = self->r.currentOrigin[i] - body->r.absmax[i];
			} else {
				v[i] = 0;
			}
		}
	}

	radius = self->speed;

	dist = VectorLength( v );

	// The person who shot the flame only burns when within 1/2 the radius
	if ( body->s.number == self->r.ownerNum && dist >= (radius*0.5) )
		return;
	if ( !directhit && dist >= radius )
		return;

	// Non-clients that take damage get damaged here
	if ( !body->client ) {
		if ( body->health > 0 )
			G_Damage( body, self->parent, self->parent, vec3_origin, self->r.currentOrigin, 2, 0, MOD_FLAMETHROWER );
		return;
	}

	// JPW NERVE -- do a trace to see if there's a wall btwn. body & flame centroid -- prevents damage through walls
	trap_Trace (&tr, self->r.currentOrigin, NULL, NULL, point, body->s.number, MASK_SHOT);
	if (tr.fraction < 1.0)
		return;
	// jpw

	// now check the damageQuota to see if we should play a pain animation
	// first reduce the current damageQuota with time
	if (body->flameQuotaTime && body->flameQuota > 0) {
		body->flameQuota -= (int)((double)(level.time - body->flameQuotaTime) * (double)0.001f * 2.5);
		if (body->flameQuota < 0)
			body->flameQuota = 0;
	}

	G_BurnMeGood( self, body );
}

void G_FlameDamage( gentity_t *self, gentity_t *ignoreent ) {
	gentity_t	*body;
	int			entityList[MAX_GENTITIES];
	int			i, e, numListedEntities;
	float		radius;
	double		boxradius; /* Windows2006bc74..: no float spill before bounds. */
	vec3_t		mins, maxs;

	radius = self->speed;
	boxradius = 1.41421356 * radius; // radius * sqrt(2) for bounding box enlargement

	for ( i = 0 ; i < 3 ; i++ ) {
		mins[i] = self->r.currentOrigin[i] - boxradius;
		maxs[i] = self->r.currentOrigin[i] + boxradius;
	}

	numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

	for ( e = 0 ; e < numListedEntities ; e++ ) {
		body = &g_entities[entityList[ e ]];

		if( body == ignoreent )
			continue;

		G_BurnTarget( self, body, qfalse );
	}
}

void G_RunFlamechunk( gentity_t *ent ) {
	vec3_t	vel, add;
	vec3_t	neworg;
	trace_t	tr;
	float	speed, dot;
	gentity_t *ignoreent = NULL;

	// TAT 11/12/2002
	//		vel was only being set if (level.time - ent->timestamp > 50
	//		However, below, it was being used when we hit something and it was
	//		uninitialized
	VectorCopy( ent->s.pos.trDelta, vel );

	// Adust the current speed of the chunk
	if ( level.time - ent->timestamp > 50 ) {
		speed = VectorNormalize( vel );
		/* Original folds the fixed 50ms friction step to exactly 120. */
		speed -= 120.0;
	
		if ( speed < FLAME_MIN_SPEED )
			speed = FLAME_MIN_SPEED;

		VectorScale( vel, speed, ent->s.pos.trDelta );
	}
	else
		speed = FLAME_START_SPEED;

	// Move the chunk
	VectorScale( ent->s.pos.trDelta, 50.f/1000.f, add );
	VectorAdd( ent->r.currentOrigin, add, neworg );

	trap_Trace (&tr, ent->r.currentOrigin, ent->r.mins, ent->r.maxs, neworg, ent->r.ownerNum, MASK_SHOT | MASK_WATER); // JPW NERVE

	if ( tr.startsolid ) {
		VectorClear( ent->s.pos.trDelta );
		ent->count2++;
	} else if ( tr.fraction != 1.0f && !(tr.surfaceFlags & SURF_NOIMPACT) ) {
		VectorCopy( tr.endpos, ent->r.currentOrigin );

		dot = DotProduct( vel, tr.plane.normal );
		VectorMA( vel, -2*dot, tr.plane.normal, vel );
		VectorNormalize( vel );
		speed = ((dot + 1.0) * 0.5 * 0.75 + 0.25) * speed * 0.5;
		VectorScale( vel, speed, ent->s.pos.trDelta );

		if( tr.entityNum != ENTITYNUM_WORLD && tr.entityNum != ENTITYNUM_NONE ) {
			ignoreent = &g_entities[ tr.entityNum ];
			G_BurnTarget( ent, ignoreent, qtrue );			
		}

		ent->count2++;
	}
	else
		VectorCopy( neworg, ent->r.currentOrigin );

	// Do damage to nearby entities, every 100ms
	if ( ent->flameQuotaTime <= level.time ) {
		ent->flameQuotaTime = level.time + 100;
		G_FlameDamage( ent, ignoreent );
	}

	// Show debugging bbox
	if(g_debugBullets.integer > 3) {
		gentity_t *bboxEnt;
		float size = ent->speed / 2;
		vec3_t b1, b2;
		vec3_t temp;
		VectorSet( temp, -size, -size, -size );
		VectorCopy(ent->r.currentOrigin, b1);
		VectorCopy(ent->r.currentOrigin, b2);
		VectorAdd(b1, temp, b1);
		VectorSet( temp, size, size, size );
		VectorAdd(b2, temp, b2);
		bboxEnt = G_TempEntity( b1, EV_RAILTRAIL );
		VectorCopy(b2, bboxEnt->s.origin2);
		bboxEnt->s.dmgFlags = 1;	// ("type")
	}

	// Adjust the size
	if ( ent->speed < FLAME_START_MAX_SIZE ) {
		ent->speed += 10.f;

		if ( ent->speed > FLAME_START_MAX_SIZE )
			ent->speed = FLAME_START_MAX_SIZE;
	}

	// Remove after 2 seconds
	if ( level.time - ent->timestamp > (FLAME_LIFETIME-150) ) { // JPW NERVE increased to 350 from 250 to match visuals better
		G_FreeEntity( ent );
		return;
	}

	G_RunThink( ent );
}

/*
=================
fire_flamechunk
=================
*/
gentity_t *fire_flamechunk (gentity_t *self, vec3_t start, vec3_t dir) {
	gentity_t	*bolt;

	// Only spawn every other frame
	if ( self->count2 ) {
		self->count2--;
		return NULL;
	}

	self->count2 = 1;
	VectorNormalize (dir);

	bolt = G_Spawn();
	bolt->classname = "flamechunk";
	
	bolt->timestamp = level.time;
	bolt->flameQuotaTime = level.time + 50;
	bolt->s.eType = ET_FLAMETHROWER_CHUNK;
	bolt->r.svFlags = SVF_NOCLIENT;	
	bolt->s.weapon = self->s.weapon;
	bolt->r.ownerNum = self->s.number;
	bolt->parent = self;
	bolt->methodOfDeath = MOD_FLAMETHROWER;
	bolt->clipmask = MASK_MISSILESHOT;
	bolt->count2 = 0;	// how often it bounced off of something

	bolt->s.pos.trType = TR_DECCELERATE;
	bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	bolt->s.pos.trDuration = 800;

	// 'speed' will be the current size radius of the chunk
	bolt->speed = FLAME_START_SIZE;
	VectorSet (bolt->r.mins, -4, -4, -4);
	VectorSet (bolt->r.maxs, 4, 4, 4);
	VectorCopy( start, bolt->s.pos.trBase );
	VectorScale(dir, FLAME_START_SPEED, bolt->s.pos.trDelta);

	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth
	VectorCopy (start, bolt->r.currentOrigin);

	return bolt;
}

//=============================================================================

//----(SA) removed unused quake3 weapons.

int G_GetWeaponDamage( int weapon ); // JPW NERVE

void DynaSink( gentity_t *self ) {

	self->clipmask = 0;
	self->r.contents = 0;

	if ( self->timestamp < level.time ) {
		self->think = G_FreeEntity;
		self->nextthink = level.time + FRAMETIME;
		return;
	}

	self->s.pos.trBase[2] -= 0.5f;
	self->nextthink = level.time + 50;
}

void DynaFree( gentity_t* self ) {
	// Gordon - see if the dynamite was planted near a constructable object that would have been destroyed
	int		entityList[MAX_GENTITIES];
	int		numListedEntities;
	int		e;
	vec3_t  org;
	gentity_t* hit;

	self->free = NULL;

	if(self->think != G_ExplodeMissile) {
		return; // we weren't armed, so no defused event
	}

	VectorCopy( self->r.currentOrigin, org );
	org[2] += 4;	// move out of ground

	numListedEntities = EntsThatRadiusCanDamage( org, self->splashRadius, entityList );

	for( e = 0; e < numListedEntities; e++ ) {
		hit = &g_entities[entityList[ e ]];

		if( hit->s.eType != ET_CONSTRUCTIBLE )
			continue;

		// invulnerable
		if( hit->spawnflags & 2 )
			continue;

		// not dynamite-able
		if( !(hit->spawnflags & 32) ) {
			continue;
		}

		G_Script_ScriptEvent( hit, "defused", "" );
	}
}

/*
==========
G_FadeItems
==========
*/

// remove any items that the player should no longer have, on disconnect/class change etc
// Gordon: changed to just set the parent to NULL
void G_FadeItems(gentity_t* ent, int modType) {
	gentity_t* e;
	int i;

	e = &g_entities[MAX_CLIENTS];
	for ( i = MAX_CLIENTS ; i<level.num_entities ; i++, e++) {
		if ( !e->inuse ) {
			continue;
		}

		if ( e->s.eType != ET_MISSILE) {
			continue;
		}

		if ( e->methodOfDeath != modType) {
			continue;
		}

		if ( e->parent != ent ) {
			continue;
		}

		e->parent = NULL;
		e->r.ownerNum = ENTITYNUM_NONE;

		G_FreeEntity(e);
	}
}

/* TC Windows2006c3f0 / Linux000cdcac: count only armed team-coded mines. */
int G_CountTeamLandmines ( team_t team ) {
	gentity_t* e;
	int i;
	int cnt = 0;

	e = &g_entities[MAX_CLIENTS];
	for( i = MAX_CLIENTS ; i<level.num_entities ; i++, e++ ) {
		if ( !e->inuse ) {
			continue;
		}

		if ( e->s.eType != ET_MISSILE) {
			continue;
		}

		if ( e->methodOfDeath != MOD_LANDMINE) {
			continue;
		}

		if ( e->s.teamNum % 4 == team && e->s.teamNum < 4) {
			cnt++;
		}
	}

	return cnt;
}

/* TC Windows2006c450: radius squared is stored as float, but the
 * VectorLengthSquared return stays in x87 until FCOMP2006c4e8. */
qboolean G_SweepForLandmines( vec3_t origin, float radius, int team ) {
	gentity_t* e;
	int i;
	vec3_t dist;

	radius *= radius;

	e = &g_entities[MAX_CLIENTS];
	for( i = MAX_CLIENTS; i < level.num_entities; i++, e++) {
		if( !e->inuse ) {
			continue;
		}

		if( e->s.eType != ET_MISSILE) {
			continue;
		}

		if( e->methodOfDeath != MOD_LANDMINE) {
			continue;
		}

		if( e->s.teamNum % 4 != team && e->s.teamNum < 4) {
			VectorSubtract( origin, e->r.currentOrigin, dist );
			if( (double)dist[0]*dist[0] + (double)dist[1]*dist[1] +
				(double)dist[2]*dist[2] > (double)radius ) {
				continue;
			}

			return( qtrue ); // found one
		}
	}

	return( qfalse );
}

/* TC Windows2006c530: first live missile with satchel MOD and exact parent. */
gentity_t *G_FindSatchel(gentity_t* ent) {
	gentity_t* e;
	int i;

	e = &g_entities[MAX_CLIENTS];
	for ( i = MAX_CLIENTS ; i<level.num_entities ; i++, e++) {
		if ( !e->inuse ) {
			continue;
		}

		if ( e->s.eType != ET_MISSILE) {
			continue;
		}

		if ( e->methodOfDeath != MOD_SATCHEL) {
			continue;
		}

		if ( e->parent != ent ) {
			continue;
		}

		return e;
	}

	return NULL;
}

/*
==========
G_FindDroppedItem
==========
*/

qboolean G_HasDroppedItem(gentity_t* ent, int modType) {
	gentity_t* e;
	int i;

	e = &g_entities[MAX_CLIENTS];
	for ( i = MAX_CLIENTS ; i<level.num_entities ; i++, e++) {
		if ( !e->inuse ) {
			continue;
		}

		if ( e->s.eType != ET_MISSILE) {
			continue;
		}

		if ( e->methodOfDeath != modType) {
			continue;
		}

		if ( e->parent != ent ) {
			continue;
		}

		return qtrue;
	}
	return qfalse;
}

/*
==========
G_ExplodeMines
==========
*/
// removes any weapon objects lying around in the map when they disconnect/switch team
void G_ExplodeMines(gentity_t* ent) {
	G_FadeItems(ent, MOD_LANDMINE);
//	G_FadeItems(ent, MOD_TRIPMINE);
}

/*
==========
G_ExplodeSatchels
==========
*/
qboolean G_ExplodeSatchels(gentity_t* ent) {
	gentity_t* e;
	vec3_t dist;
	int i;
	qboolean blown = qfalse;

	e = &g_entities[MAX_CLIENTS];
	for( i = MAX_CLIENTS ; i < level.num_entities; i++, e++ ) {
		if( !e->inuse ) {
			continue;
		}

		if( e->s.eType != ET_MISSILE) {
			continue;
		}

		if( e->methodOfDeath != MOD_SATCHEL) {
			continue;
		}

		VectorSubtract(e->r.currentOrigin, ent->r.currentOrigin, dist);
		/* TC2006c601 compares the x87 VectorLengthSquared return directly,
		 * without rounding it through a float result first. */
		if( (double)dist[0]*dist[0] + (double)dist[1]*dist[1] +
			(double)dist[2]*dist[2] > SQR(2000)) {
			continue;
		}

		if ( e->parent != ent ) {
			continue;
		}

		G_ExplodeMissile(e);
		blown = qtrue;
	}

	return blown;
}

void G_FreeSatchel( gentity_t* ent ) {
	gentity_t* other;

	ent->free = NULL;

	if( ent->s.eType != ET_MISSILE ) {
		return;
	}
		
	other = &g_entities[ent->s.clientNum];

	if( !other->client || other->client->pers.connected != CON_CONNECTED ) {
		return;
	}

	if( other->client->sess.playerType != PC_COVERTOPS ) {
		return;
	}

	other->client->ps.ammo[WP_SATCHEL_DET] = 0;
	other->client->ps.ammoclip[WP_SATCHEL_DET] = 0;
	other->client->ps.ammoclip[WP_SATCHEL] = 1;
	if( other->client->ps.weapon == WP_SATCHEL_DET ) {
		G_AddEvent( other, EV_NOAMMO, 0 );
	}
}

/*
==========
LandMineTrigger
==========
*/
void LandminePostThink( gentity_t *self );

void LandMineTrigger(gentity_t* self) {
	self->r.contents = CONTENTS_CORPSE;
	trap_LinkEntity( self );
	self->nextthink = level.time + FRAMETIME;
	self->think = LandminePostThink;
	self->s.teamNum += 8;
	// rain - communicate trigger time to client
	self->s.time = level.time;
}

void LandMinePostTrigger(gentity_t* self) {
	self->nextthink = level.time + 300;
	self->think = G_ExplodeMissile;
}


/*
==========
G_TripMineThink
==========
*/

void G_TripMineThink(gentity_t* ent) {
	trace_t trace;
	vec3_t start, end;
	gentity_t* traceEnt;

	VectorMA(ent->r.currentOrigin, 2, ent->s.origin2, start);
	VectorMA(start, 2048, ent->s.origin2, end);

	/* TC 2006c768: keep the placement/beam collision masks identical. */
	trap_Trace(&trace, start, NULL, NULL, end, ent->s.number, MASK_MISSILESHOT);

	ent->nextthink = level.time + FRAMETIME;

	if(!(trace.fraction < 1.f || trace.fraction > 1.f)) { /* TC x87 C3: equal or unordered. */
/*		ent->nextthink = level.time;
		ent->think = DynaSink;
		ent->timestamp = level.time + 1500;*/
		return;
	}

	if(trace.entityNum >= ENTITYNUM_NONE) {
		return;
	}
	
	traceEnt = &g_entities[trace.entityNum];

	if(!Q_stricmp(traceEnt->classname, "player")) {
		ent->think = G_ExplodeMissile;
//		return;
	}
}

/*
==========
G_TripMinePrime
==========
*/

void G_TripMinePrime(gentity_t* ent) {
	ent->think = G_TripMineThink;
	ent->nextthink = level.time + 500;
}

/*107     11      20      0       0       0       0       //fire gren 

==========
G_LandmineThink
==========
*/

// TAT 11/20/2002
//		Function to check if an entity will set off a landmine
#define LANDMINE_TRIGGER_DIST 64.0f

qboolean sEntWillTriggerMine(gentity_t *ent, gentity_t *mine)
{
	// player types are the only things that set off mines (human and bot)
	if (ent->s.eType == ET_PLAYER && ent->client)
	{
		vec3_t dist;
		VectorSubtract(mine->r.currentOrigin, ent->r.currentOrigin, dist);
		// have to be within the trigger distance AND on the ground -- if we jump over a mine, we don't set it off
		//		(or if we fly by after setting one off)
		/* TC2006c890 retains the squared-length result in x87 until comparison. */
		if ( ((double)dist[0]*dist[0] + (double)dist[1]*dist[1] +
			(double)dist[2]*dist[2] <= SQR(LANDMINE_TRIGGER_DIST)) && (fabs(dist[2]) < 45.f) )
		{
			return qtrue;
		}
	}

	return qfalse;
}

// Gordon: Landmine waits for 2 seconds then primes, which sets think to checking for "enemies"
void G_LandmineThink( gentity_t *self ) {
	int entityList[MAX_GENTITIES];
	int i, cnt;
	vec3_t range = {LANDMINE_TRIGGER_DIST, LANDMINE_TRIGGER_DIST, LANDMINE_TRIGGER_DIST};
	vec3_t mins, maxs;
	qboolean trigger = qfalse;
	gentity_t* ent;

	self->nextthink = level.time + FRAMETIME;

	if( level.time - self->missionLevel > 200 ) {
		self->s.density = 0; // Gordon: time out the covert ops visibile thing, or we could get other clients being able to see mine later, etc
	}

	VectorSubtract(self->r.currentOrigin, range, mins);
	VectorAdd(self->r.currentOrigin, range, maxs);

	cnt = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

	for( i = 0; i < cnt; i++) {
		ent = &g_entities[entityList[i]];

		if( !ent->client ) {
			continue;
		}

		//%	if( !g_friendlyFire.integer && G_LandmineTeam( self ) == ent->client->sess.sessionTeam ) {
		//%		continue;
		//%	}

		// TAT 11/20/2002 use the unified trigger check to see if we are close enough to prime the mine
		if( sEntWillTriggerMine( ent, self ) ) {
			trigger = qtrue;
			break;
		}
	}

	if( trigger ) {
		LandMineTrigger( self );
	}
}

void LandminePostThink( gentity_t *self ) {
	int entityList[MAX_GENTITIES];
	int i, cnt;
	vec3_t range = {LANDMINE_TRIGGER_DIST, LANDMINE_TRIGGER_DIST, LANDMINE_TRIGGER_DIST};
	vec3_t mins, maxs;
	qboolean trigger = qfalse;
	gentity_t* ent;

	self->nextthink = level.time + FRAMETIME;

	if( level.time - self->missionLevel > 5000 ) {
		self->s.density = 0; // Gordon: time out the covert ops visibile thing, or we could get other clients being able to see mine later, etc
	}

	VectorSubtract(self->r.currentOrigin, range, mins);
	VectorAdd(self->r.currentOrigin, range, maxs);

	cnt = trap_EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

	for( i = 0; i < cnt; i++) {
		ent = &g_entities[entityList[i]];

		// TAT 11/20/2002 use the unifed trigger check to see if we're still standing on the mine, so we don't set it off
		if (sEntWillTriggerMine(ent, self))
		{
			trigger = qtrue;
			break;
		}
	}

	if(!trigger) {
		LandMinePostTrigger(self);
	}
}

/*
==========
G_LandminePrime
==========
*/

void G_LandminePrime( gentity_t *self ) {
	self->nextthink = level.time + FRAMETIME;
	self->think = G_LandmineThink;
}

qboolean G_LandmineSnapshotCallback( int entityNum, int clientNum ) {
	gentity_t* ent		= &g_entities[ entityNum ];
	gentity_t* clEnt	= &g_entities[ clientNum ];
	team_t team;

	if( clEnt->client->sess.skill[ SK_BATTLE_SENSE ] >= 4 ) {
		return qtrue;
	}

	if( !G_LandmineArmed( ent ) ) {
		return qtrue;
	}

	if( G_LandmineSpotted( ent ) ) {
		return qtrue;
	}

	team = G_LandmineTeam( ent );
	if( team == clEnt->client->sess.sessionTeam ) {
		return qtrue;
	}

	//bani - fix for covops spotting
	if( clEnt->client->sess.playerType == PC_COVERTOPS && clEnt->client->ps.eFlags & EF_ZOOMING && ( clEnt->client->ps.stats[STAT_KEYS] & ( 1 << INV_BINOCS ) ) ) {
		return qtrue;
	}

	return qfalse;
}

/*
=================
fire_grenade

	NOTE!!!! NOTE!!!!!

	This accepts a /non-normalized/ direction vector to allow specification
	of how hard it's thrown.  Please scale the vector before calling.

=================
*/
/* TC Windows2006cc30. Numeric slots are intentional: SDK weapon IDs collide. */
gentity_t *fire_grenade(gentity_t *self,vec3_t start,vec3_t dir,int grenadeWPID) {
    gentity_t *bolt=G_Spawn();
    qboolean noExplode=qfalse;
    int team=self->client?self->client->sess.sessionTeam:TEAM_FREE;
    bolt->nextthink=level.time+(self->client && self->client->ps.grenadeTimeLeft?self->client->ps.grenadeTimeLeft:2500);
    if(grenadeWPID==15 || grenadeWPID==26) {
        noExplode=qtrue;bolt->nextthink=level.time+15000;
        bolt->think=DynaSink;bolt->timestamp=level.time+16500;
        if(grenadeWPID==15)bolt->free=DynaFree;
    } else if(grenadeWPID==27) {
        noExplode=qtrue;bolt->nextthink=0;
        bolt->s.clientNum=self->s.clientNum;bolt->free=G_FreeSatchel;
    } else if(grenadeWPID==60) { noExplode=qtrue;bolt->nextthink=0; }
    if(self->client)self->client->ps.grenadeTimeLeft=0;
    if(!noExplode)bolt->think=G_ExplodeMissile;
    bolt->s.eType=ET_MISSILE;bolt->r.svFlags=SVF_BROADCAST;
    bolt->s.weapon=grenadeWPID;bolt->r.ownerNum=self->s.number;
    bolt->parent=self;bolt->s.teamNum=team;
    bolt->damage=G_GetWeaponDamage(grenadeWPID);
    bolt->splashDamage=G_GetWeaponDamage(grenadeWPID);
    bolt->s.pos.trType=(trType_t)(g_newbbox.integer?13:7);
    bolt->s.eFlags=self->client && (self->client->ps.stats[STAT_TCE_WEAPON_FLAGS]&4)?EF_BOUNCE_HALF:EF_BOUNCE_HALF|EF_BOUNCE;
    switch(grenadeWPID) {
    case 4:case 9:
        bolt->classname=grenadeWPID==4?"flashbang":"grenade";
        bolt->splashRadius=300;bolt->methodOfDeath=bolt->splashMethodOfDeath=18;break;
    case 22:
        bolt->classname="grenade";bolt->s.eFlags=EF_BOUNCE_HALF|EF_BOUNCE;
        bolt->methodOfDeath=bolt->splashMethodOfDeath=62;break;
    case 30:
        bolt->classname="smoke_grenade";bolt->splashRadius=300;
        bolt->methodOfDeath=bolt->splashMethodOfDeath=62;break;
    case 55:case 56:
        bolt->classname=grenadeWPID==55?"gpg40_grenade":"m7_grenade";
        bolt->splashRadius=300;bolt->methodOfDeath=bolt->splashMethodOfDeath=grenadeWPID==55?43:44;
        bolt->s.eFlags=EF_BOUNCE_HALF|EF_BOUNCE;bolt->nextthink=level.time+4000;break;
    case 60:
        bolt->classname="mortar_grenade";bolt->splashRadius=800;
        bolt->methodOfDeath=bolt->splashMethodOfDeath=57;bolt->s.eFlags=0;break;
    case 15:case 26:case 27:
        bolt->accuracy=0;bolt->health=5;bolt->damage=0;
        bolt->s.eFlags=EF_BOUNCE_HALF|EF_BOUNCE;
        bolt->r.contents=CONTENTS_CORPSE;bolt->takedamage=grenadeWPID==26;
        if(grenadeWPID==26) {
            bolt->s.teamNum=team+4;bolt->classname="landmine";bolt->splashRadius=225;
            bolt->methodOfDeath=bolt->splashMethodOfDeath=45;bolt->r.snapshotCallback=qtrue;
            VectorSet(bolt->r.mins,-16,-16,0);VectorSet(bolt->r.maxs,16,16,16);
        } else {
            bolt->classname=grenadeWPID==15?"dynamite":"satchel_charge";
            bolt->splashRadius=grenadeWPID==15?400:300;
            bolt->methodOfDeath=bolt->splashMethodOfDeath=grenadeWPID==15?26:46;
            VectorSet(bolt->r.mins,-12,-12,0);VectorSet(bolt->r.maxs,12,12,20);
        }
        VectorCopy(bolt->r.mins,bolt->r.absmin);VectorCopy(bolt->r.maxs,bolt->r.absmax);
        if(grenadeWPID==15) {
            vec3_t origin,mins,maxs;
            int touch[MAX_GENTITIES],num,i;
            bolt->timestamp=level.time+1000;bolt->s.teamNum=team;
            bolt->s.effect1Time=level.time;bolt->nextthink=level.time+45000;
            bolt->think=G_ExplodeMissile;
            VectorCopy(self->r.currentOrigin,origin);SnapVector(origin);
            VectorSet(mins,origin[0]-12,origin[1]-12,origin[2]);
            VectorSet(maxs,origin[0]+12,origin[1]+12,origin[2]+20);
            num=trap_EntitiesInBox(mins,maxs,touch,MAX_GENTITIES);
            for(i=0;i<num;i++) {
                gentity_t *hit=&g_entities[touch[i]],*sound,*popup;
                qboolean enemy;
                if(!(hit->r.contents&CONTENTS_TRIGGER) || strcmp(hit->classname,"trigger_objective_info") ||
                    !(hit->spawnflags&(AXIS_OBJECTIVE|ALLIED_OBJECTIVE)))continue;
                sound=G_TempEntity(self->r.currentOrigin,EV_GLOBAL_TEAM_SOUND);
                enemy=((hit->spawnflags&AXIS_OBJECTIVE)&&team==TEAM_ALLIES)||
                      ((hit->spawnflags&ALLIED_OBJECTIVE)&&team==TEAM_AXIS);
                if(enemy)sound->s.eventParm=G_SoundIndex(team==TEAM_ALLIES?
                    "sound/multiplayer/allies/a-dynamite_planted.wav":"sound/multiplayer/axis/g-dynamite_planted.wav");
                if(hit->spawnflags&AXIS_OBJECTIVE) {
                    sound->s.teamNum=TEAM_AXIS;
                    if(team==TEAM_ALLIES)bolt->accuracy=hit->accuracy;
                } else {
                    sound->s.teamNum=TEAM_ALLIES;
                    if(team==TEAM_AXIS)bolt->accuracy=hit->accuracy;
                }
                sound->r.svFlags|=SVF_BROADCAST;
                if(enemy) {
                    level.tceBombPlanted=qtrue;
                    self->client->ps.stats[STAT_TCE_WEAPON_FLAGS]&=~0x100;
                    self->client->tceBombPossessionOrder=0;
                    level.tceBombCarrierCount--;
                    popup=G_PopupMessage(PM_DYNAMITE);popup->s.effect2Time=0;
                    popup->s.effect3Time=hit->s.teamNum;popup->s.teamNum=team;
                    G_Script_ScriptEvent(hit,"dynamited","");
                    if(hit->target_ent)G_Script_ScriptEvent(hit->target_ent,"dynamited","");
                    if(sound->s.teamNum && sound->s.teamNum!=team) {
                        /* Original target entity+0x2e4 is the script score,
                         * not the SDK count field (e.g. crate health/count150). */
                        if(hit->target_ent && hit->target_ent->tceObjectiveScore) {
                            if(g_gametype.integer==5)AddKillScore(bolt->parent,hit->target_ent->tceObjectiveScore);
                            else AddScore(bolt->parent,hit->target_ent->tceObjectiveScore);
                        }
                        if(bolt->parent && bolt->parent->client)
                            G_LogPrintf("Dynamite_Plant: %d\n",(int)(bolt->parent - g_entities));
                        bolt->parent=self;
                    }
                }
                break;
            }
        }
        break;
    default:break;
    }
    bolt->splashRadius=G_GetWeaponDamage(grenadeWPID);
    bolt->clipmask=MASK_MISSILESHOT;
    bolt->s.pos.trTime=grenadeWPID==15?level.time:level.time-MISSILE_PRESTEP_TIME;
    VectorCopy(start,bolt->s.pos.trBase);VectorCopy(dir,bolt->s.pos.trDelta);
    if(self->s.groundEntityNum!=ENTITYNUM_NONE && self->s.groundEntityNum!=ENTITYNUM_WORLD)
        VectorAdd(bolt->s.pos.trDelta,g_entities[self->s.groundEntityNum].instantVelocity,bolt->s.pos.trDelta);
    SnapVector(bolt->s.pos.trDelta);VectorCopy(start,bolt->r.currentOrigin);
    bolt->awaitingHelpTime=level.time;return bolt;
}

//=============================================================================

/*
=================
fire_rocket
=================
*/
gentity_t *fire_rocket (gentity_t *self, vec3_t start, vec3_t dir) {
	gentity_t	*bolt;

	VectorNormalize (dir);

	bolt = G_Spawn();
	bolt->classname = "rocket";
	bolt->nextthink = level.time + 20000;	// push it out a little
	bolt->think = G_ExplodeMissile;
	bolt->accuracy = 4;
	bolt->s.eType = ET_MISSILE;
	bolt->r.svFlags = SVF_BROADCAST;	
	
	//DHM - Nerve :: Use the correct weapon in multiplayer
	bolt->s.weapon = self->s.weapon;

	bolt->r.ownerNum = self->s.number;
	bolt->parent = self;
	/* TC2006d797/7a4 use map-rocket protocol65, not SDK weapon5. */
	bolt->damage = G_GetWeaponDamage(65);
	bolt->splashDamage = G_GetWeaponDamage(65);
	bolt->splashRadius = 300; //G_GetWeaponDamage(WP_PANZERFAUST);	// Arnout : hardcoded bleh hack
	bolt->methodOfDeath = MOD_PANZERFAUST;
	bolt->splashMethodOfDeath = MOD_PANZERFAUST;
//	bolt->clipmask = MASK_SHOT;
	bolt->clipmask = MASK_MISSILESHOT;

	bolt->s.pos.trType = TR_LINEAR;
	bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	VectorCopy( start, bolt->s.pos.trBase );
// JPW NERVE
	VectorScale(dir,2500,bolt->s.pos.trDelta);
// jpw
	/* Windows2006d821: reload stored32, __ftol64, consume lowEAX. */
#if defined(_MSC_VER) && defined(_M_IX86)
	{
		int rocketAxis;
		for (rocketAxis = 0; rocketAxis < 3; ++rocketAxis) {
			float *rocketComponent = &bolt->s.pos.trDelta[rocketAxis];
			unsigned short rocketCW, rocketTruncCW;
			__int64 rocketInteger;
			int rocketLow;
			__asm {
				mov ecx, rocketComponent
				fld dword ptr [ecx]
				fwait
				fnstcw rocketCW
				fwait
				mov ax, rocketCW
				or ax, 0c00h
				mov rocketTruncCW, ax
				fldcw rocketTruncCW
				fistp qword ptr rocketInteger
				fldcw rocketCW
				mov eax, dword ptr rocketInteger
				mov rocketLow, eax
				fild rocketLow
				fstp dword ptr [ecx]
			}
		}
	}
#else
	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth
#endif
	VectorCopy (start, bolt->r.currentOrigin);

	if(self->client) {
		bolt->s.teamNum = self->client->sess.sessionTeam;
	}

	return bolt;
}

// Rafael flamebarrel
/*
======================
fire_flamebarrel
======================
*/

gentity_t *fire_flamebarrel (gentity_t *self, vec3_t start, vec3_t dir) {
	gentity_t	*bolt;

	VectorNormalize (dir);

	bolt = G_Spawn();

	// Gordon: for explosion type
	bolt->accuracy		= 3;

	bolt->classname = "flamebarrel";
	bolt->nextthink = level.time + 3000;
	bolt->think = G_ExplodeMissile;
	bolt->s.eType = ET_FLAMEBARREL;
	bolt->s.eFlags = EF_BOUNCE_HALF;
	bolt->r.svFlags = SVF_BLANK;
	bolt->s.weapon = 65; /* TC2006d907 map-projectile protocol, not SDK5. */
	bolt->r.ownerNum = self->s.number;
	bolt->parent = self;
	bolt->damage = 100;
	bolt->splashDamage = 20;
	bolt->splashRadius = 60;
	
	bolt->methodOfDeath = MOD_ROCKET;
	bolt->splashMethodOfDeath = MOD_ROCKET;
	
	bolt->clipmask = MASK_MISSILESHOT;

	bolt->s.pos.trType = TR_GRAVITY;
	bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	VectorCopy( start, bolt->s.pos.trBase );
#if defined(_MSC_VER) && defined(_M_IX86)
	{
		int flameAxis;
		const float flameReciprocal = 3.0518509447574615e-05f;
		const double flameHalf = 0.5, flameRange = 100.0, flameSpeed = 900.0;
		/* Three independent draws and stores, then three conversions. */
		for (flameAxis = 0; flameAxis < 3; ++flameAxis) {
			int flameRandom = rand() & 0x7fff;
			float *flameDirection = &dir[flameAxis];
			float *flameVelocity = &bolt->s.pos.trDelta[flameAxis];
			__asm {
				fild flameRandom
				fmul flameReciprocal
				fsub flameHalf
				fadd st(0), st(0)
				fmul flameRange
				fadd flameSpeed
				mov ecx, flameDirection
				fmul dword ptr [ecx]
				mov ecx, flameVelocity
				fstp dword ptr [ecx]
			}
		}
		for (flameAxis = 0; flameAxis < 3; ++flameAxis) {
			float *flameVelocity = &bolt->s.pos.trDelta[flameAxis];
			unsigned short flameCW, flameTruncCW;
			__int64 flameInteger;
			int flameLow;
			__asm {
				mov ecx, flameVelocity
				fld dword ptr [ecx]
				fwait
				fnstcw flameCW
				fwait
				mov ax, flameCW
				or ax, 0c00h
				mov flameTruncCW, ax
				fldcw flameTruncCW
				fistp qword ptr flameInteger
				fldcw flameCW
				mov eax, dword ptr flameInteger
				mov flameLow, eax
				fild flameLow
				fstp dword ptr [ecx]
			}
		}
	}
#else
	VectorScale( dir, 900 + (crandom() * 100), bolt->s.pos.trDelta );
	SnapVector( bolt->s.pos.trDelta );
#endif
	VectorCopy (start, bolt->r.currentOrigin);

	return bolt;
}

// Rafael sniper
/*
=================
fire_lead
=================
*/

void fire_lead (gentity_t *self, vec3_t start, vec3_t dir, int damage) {

	trace_t		tr;
	vec3_t		end;
	gentity_t		*tent;
	gentity_t		*traceEnt;
	vec3_t		forward, right, up;
	vec3_t		angles;
	float		r , u;
//	qboolean anti_tank_enable = qfalse;

	r = crandom()*self->random;
	u = crandom()*self->random;

	vectoangles (dir, angles);
	AngleVectors (angles, forward, right, up);

	VectorMA (start, 8192, forward, end);
	VectorMA (end, r, right, end);
	VectorMA (end, u, up, end);

	trap_Trace (&tr, start, NULL, NULL, end, self->s.number, MASK_SHOT);
	if ( tr.surfaceFlags & SURF_NOIMPACT) {
		return;
	}

	traceEnt = &g_entities[ tr.entityNum ];

	// snap the endpos to integers, but nudged towards the line
	SnapVectorTowards( tr.endpos, start );

	// send bullet impact
	if ( traceEnt->takedamage && traceEnt->client ) {
		tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_FLESH );
		tent->s.eventParm = traceEnt->s.number;
	} else {
		// Ridah, bullet impact should reflect off surface
		vec3_t	reflect;
		float	dot;

		tent = G_TempEntity( tr.endpos, EV_BULLET_HIT_WALL );

		dot = DotProduct( forward, tr.plane.normal );
		VectorMA( forward, -2*dot, tr.plane.normal, reflect );
		VectorNormalize( reflect );

		tent->s.eventParm = DirToByte( reflect );
		// done.
	}
	tent->s.otherEntityNum = self->s.number;

	if ( traceEnt->takedamage) {
		G_Damage( traceEnt, self, self, forward, tr.endpos,
			damage, 0, MOD_MACHINEGUN);
	}
}


// Rafael sniper
// visible

/*
==============
visible
==============
*/
qboolean visible (gentity_t *self, gentity_t *other)
{
//	vec3_t		spot1;
//	vec3_t		spot2;
	trace_t		tr;
	gentity_t	*traceEnt;

	trap_Trace (&tr, self->r.currentOrigin, NULL, NULL, other->r.currentOrigin, self->s.number, MASK_SHOT);

	traceEnt = &g_entities[ tr.entityNum ];

	if (traceEnt == other)
		return qtrue;

	return qfalse;	

}



/*
==============
fire_mortar
	dir is a non-normalized direction/power vector
==============
*/
gentity_t *fire_mortar(gentity_t *self, vec3_t start, vec3_t dir) {
	gentity_t	*bolt;

//	VectorNormalize (dir);

	if(self->spawnflags) {
		gentity_t	*tent;
		tent = G_TempEntity (self->s.pos.trBase, EV_MORTAREFX);
		tent->s.density = self->spawnflags;	// send smoke and muzzle flash flags
		VectorCopy (self->s.pos.trBase, tent->s.origin);
		VectorCopy (self->s.apos.trBase, tent->s.angles);
	}

	bolt = G_Spawn();
	bolt->classname = "mortar";
	bolt->nextthink = level.time + 20000;	// push it out a little
	bolt->think = G_ExplodeMissile;

	// Gordon: for explosion type
	bolt->accuracy = 4;

	bolt->s.eType = ET_MISSILE;

	bolt->r.svFlags = SVF_BROADCAST;	// broadcast sound.  not multiplayer friendly, but for mortars it should be okay
	bolt->s.weapon = WP_MAPMORTAR;
	bolt->r.ownerNum = self->s.number;
	bolt->parent = self;
	bolt->damage = G_GetWeaponDamage(WP_MAPMORTAR); // JPW NERVE
	bolt->splashDamage = G_GetWeaponDamage(WP_MAPMORTAR); // JPW NERVE
	bolt->splashRadius = 120;
	bolt->methodOfDeath = MOD_MAPMORTAR;
	bolt->splashMethodOfDeath = MOD_MAPMORTAR_SPLASH;
	bolt->clipmask = MASK_MISSILESHOT;

	bolt->s.pos.trType = TR_GRAVITY;
	bolt->s.pos.trTime = level.time - MISSILE_PRESTEP_TIME;		// move a bit on the very first frame
	VectorCopy( start, bolt->s.pos.trBase );
//	VectorScale( dir, 900, bolt->s.pos.trDelta );
	VectorCopy(dir, bolt->s.pos.trDelta);
	SnapVector( bolt->s.pos.trDelta );			// save net bandwidth
	VectorCopy (start, bolt->r.currentOrigin);

	return bolt;
}
