#ifndef TCE_NAV_FREECLIMB_QUERY_H
#define TCE_NAV_FREECLIMB_QUERY_H
/* Readonly PM_CheckJump/PM_CheckLadderMove gate diagnostics.  Returning a
 * mask never changes the player's movement, stamina or weapon state. */
static int TceNavFreeClimbBlockers(const gentity_t *entity,int omnibotRegistered) {
 const playerState_t *ps;int mask=0;
 if(!entity||!entity->client)return 1;
 ps=&entity->client->ps;
 if(ps->stats[STAT_HEALTH]<=0)mask|=1;
 if(ps->groundEntityNum<MAX_CLIENTS||ps->groundEntityNum==ENTITYNUM_NONE)mask|=2;
 if(ps->pm_flags&(PMF_DUCKED|PMF_LADDER|PMF_RESPAWNED|PMF_JUMP_HELD))mask|=4;
 if(ps->eFlags&(EF_PRONE|EF_PRONE_MOVING))mask|=8;
 if(ps->stats[STAT_TCE_WEAPON_FLAGS]&0x1000)mask|=16;
 if((ps->stats[STAT_TCE_FLAGS]&0x800)&&(!(entity->r.svFlags&SVF_BOT)||!omnibotRegistered))mask|=32;
 if(ps->weaponstate>=9&&ps->weaponstate<=11)mask|=64;
 if(!(ps->stats[STAT_TCE_FLAGS]&0x40)&&ps->pm_time>500)mask|=128;
 if(entity->client->pmext.sprintTime<TCE_NAV_FREECLIMB_READY_STAMINA)mask|=256;
 return mask;
}
#endif
