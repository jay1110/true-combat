#ifndef TCE_FLAG_QUERY_H
#define TCE_FLAG_QUERY_H

/* Read the real TC flag state. flagParent survives capture/death, so it is
 * never ownership evidence without the matching live player powerup. */
static int TceFlagPowerup(const gentity_t *flag) {
 if(!flag||!flag->inuse||flag->s.eType!=ET_ITEM||!flag->classname)return 0;
 if(!Q_stricmp(flag->classname,"team_CTF_redflag"))return PW_REDFLAG;
 if(!Q_stricmp(flag->classname,"team_CTF_blueflag"))return PW_BLUEFLAG;
 return 0;
}
static int TceFlagCarrier(const gentity_t *flag) {
 int powerup=TceFlagPowerup(flag),base,i,limit=g_maxclients.integer;
 if(!powerup)return -1;
 base=(flag->flags&FL_DROPPED_ITEM)?flag->s.otherEntityNum:(int)(flag-g_entities);
 if(base<MAX_CLIENTS||base>=level.num_entities||base>=MAX_GENTITIES)return -1;
 if(limit>MAX_CLIENTS)limit=MAX_CLIENTS;
 for(i=0;i<limit;i++) {
  const gentity_t *player=&g_entities[i];
  if(player->inuse&&player->client&&player->health>0&&
     !(player->client->ps.pm_flags&PMF_LIMBO)&&
     player->client->flagParent==base&&player->client->ps.powerups[powerup])return i;
 }
 return -1;
}
static qboolean TceFlagVisible(const gentity_t *flag) {
 return flag->inuse&&flag->s.density>0&&!(flag->flags&FL_NODRAW)&&
  !(flag->r.svFlags&SVF_NOCLIENT)&&(flag->r.contents&CONTENTS_TRIGGER)&&
  flag->entstate!=STATE_INVISIBLE&&flag->entstate!=STATE_UNDERCONSTRUCTION&&
  flag->entstate!=STATE_INACTIVE&&flag->entstate!=STATE_LOCKED?qtrue:qfalse;
}
static FlagState TceReadFlagState(const gentity_t *flag,int *owner) {
 int i,powerup=TceFlagPowerup(flag),base;
 *owner=-1;if(!powerup)return S_FLAG_NOT_A_FLAG;
 *owner=TceFlagCarrier(flag);if(*owner>=0)return S_FLAG_CARRIED;
 if(TceFlagVisible(flag))return flag->flags&FL_DROPPED_ITEM?S_FLAG_DROPPED:S_FLAG_AT_BASE;
 if(!(flag->flags&FL_DROPPED_ITEM)) {
  base=(int)(flag-g_entities);
  for(i=MAX_CLIENTS;i<level.num_entities&&i<MAX_GENTITIES;i++) {
   const gentity_t *drop=&g_entities[i];
   if((drop->flags&FL_DROPPED_ITEM)&&drop->s.otherEntityNum==base&&
      TceFlagPowerup(drop)==powerup&&TceFlagVisible(drop))return S_FLAG_DROPPED;
  }
 }
 return S_FLAG_UNAVAILABLE;
}
#endif
