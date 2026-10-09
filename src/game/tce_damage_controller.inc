/* Recovered TC damage controller. Internal decomposition of Windows200589c0;
 * no original-function completion is implied by the helpers below. */
#include "tce_damage.h"
extern void G_Voice(gentity_t *, gentity_t *, int, const char *, qboolean);

static void TCE_DamageDebugBox(gentity_t *part,const vec3_t start,const vec3_t end,float fraction) {
    vec3_t mins,maxs,hit;gentity_t *event;int i;
    if(g_debugBullets.integer<=2)return;
    VectorAdd(part->r.currentOrigin,part->r.mins,mins);VectorAdd(part->r.currentOrigin,part->r.maxs,maxs);
    event=G_TempEntity(mins,EV_RAILTRAIL);VectorCopy(maxs,event->s.origin2);event->s.dmgFlags=1;
    for(i=0;i<3;++i)hit[i]=start[i]+fraction*(end[i]-start[i]);
    event=G_TempEntity((float *)start,EV_RAILTRAIL);VectorCopy(hit,event->s.origin2);event->s.dmgFlags=0;
}

static int TCE_ApplyBodyDamage(gentity_t *targ,const vec3_t dir,const vec3_t point,int damage) {
    tceDamagePose_t pose;tceDamageVolume_t volumes[3];float fractions[3]={1,1,1};
    gentity_t *part;trace_t tr;vec3_t end;int i,j,amounts[3],take;float scales[3];
    /* Original first diagnostic ray reads uninitialized locals.  Keep its
     * surrounding box/ray events, using the defined incoming ray instead. */
    if(g_debugBullets.integer>2){for(j=0;j<3;++j)end[j]=point[j]+dir[j]*64;
        TCE_DamageDebugBox(targ,point,end,1);}
    part=G_Spawn();G_SetOrigin(part,targ->r.currentOrigin);
    if(targ->client->sess.sessionTeam!=TEAM_AXIS&&targ->client->sess.sessionTeam!=TEAM_ALLIES)return -1;
    VectorCopy(targ->r.currentOrigin,pose.origin);VectorCopy(targ->client->ps.viewangles,pose.angles);
    pose.lean=targ->client->ps.leanf;pose.weapon=targ->client->ps.weapon;
    pose.prone=!!(targ->client->ps.eFlags&EF_PRONE);pose.ducked=!!(targ->client->ps.pm_flags&PMF_DUCKED);
    pose.aiming=!!(targ->client->ps.stats[STAT_TCE_WEAPON_FLAGS]&0x20);
    pose.enlarged=!!(targ->client->ps.stats[STAT_TCE_FLAGS]&0x200);
    if(g_debugDamage.integer){
        if(pose.prone)G_Printf(pose.aiming?"moving proned neck: ":"proned neck: ");
        else if(pose.ducked)G_Printf(pose.aiming?"moving ducked neck: ":"ducked neck: ");
        else {
            G_Printf(pose.aiming?"moving standing neck: ":"standing neck: ");
            if(pose.weapon==1)G_Printf("knife anim: ");
            else if(pose.weapon==4||pose.weapon==9||pose.weapon==30)G_Printf("grenade anim: ");
            else if(pose.weapon==2||pose.weapon==39||pose.weapon==40||pose.weapon==7||pose.weapon==37||pose.weapon==38||
                    pose.weapon==10||pose.weapon==3||pose.weapon==52||pose.weapon==14||pose.weapon==53||pose.weapon==54)
                G_Printf("single handed anim: ");
        }
    }
    TCE_DamageVolumes(&pose,volumes);
    for(j=0;j<3;++j)end[j]=point[j]+dir[j]*(pose.enlarged?80.0f:64.0f);
    for(i=0;i<3;++i){
        if(i)G_SetOrigin(part,targ->r.currentOrigin);
        VectorCopy(volumes[i].origin,part->r.currentOrigin);VectorCopy(volumes[i].origin,part->s.origin);
        VectorCopy(targ->r.currentAngles,part->s.angles);
        VectorCopy(targ->r.currentAngles,part->s.apos.trBase);VectorCopy(targ->r.currentAngles,part->s.apos.trDelta);
        VectorCopy(volumes[i].mins,part->r.mins);VectorCopy(volumes[i].maxs,part->r.maxs);
        part->clipmask=1;part->r.contents=1;trap_LinkEntity(part);
        trap_Trace(&tr,point,NULL,NULL,end,targ->s.number,MASK_MISSILESHOT);
        if(&g_entities[tr.entityNum]==part){fractions[i]=tr.fraction;
            if(g_debugDamage.integer)G_Printf(i==0?"hit head\n":i==1?"hit body\n":"hit legs\n");}
        TCE_DamageDebugBox(part,point,end,tr.fraction);
    }
    G_FreeEntity(part);
    if(fractions[0]==1&&fractions[1]==1&&fractions[2]==1)return -2;
    take=TCE_DamageWeights(volumes,fractions,point,dir,pose.enlarged?1.25f:1.0f,damage,amounts,scales);
    for(i=0;i<3;++i)if(amounts[i]>0)targ->client->ps.holdable[i+2]+=amounts[i];
    if(g_debugDamage.integer)G_Printf("take: %i, scales: headtmp: %f, bodytmp: %f, legstmp: %f\n",take,(double)scales[0],(double)scales[1],(double)scales[2]);
    return take;
}

float G_Damage(gentity_t *targ,gentity_t *inflictor,gentity_t *attacker,
               vec3_t dir,vec3_t point,int damage,int dflags,int mod) {
    gclient_t *client;int take,wasAlive,i;mapEntityData_t *mapEnt;
    if(!targ->takedamage||level.intermissionQueued||g_gamestate.integer!=GS_PLAYING)return 0;
    if(!inflictor)inflictor=&g_entities[ENTITYNUM_WORLD];
    if(!attacker)attacker=&g_entities[ENTITYNUM_WORLD];
    if(targ->entstate==STATE_INVISIBLE||targ->entstate==STATE_UNDERCONSTRUCTION)return 0;
    wasAlive=targ->health>0;
    if(targ->client&&attacker->client&&attacker!=targ&&!OnSameTeam(attacker,targ)){
        targ->client->combatState|=4;attacker->client->combatState|=2;
    }
    if(targ->waterlevel>2&&mod==19)return 0;
    if(targ->s.eType==ET_MOVER&&!targ->isProp&&!targ->scriptName){
        if(targ->use&&targ->moverState==MOVER_POS1)G_UseEntity(targ,inflictor,attacker);return 0;
    }
    if(targ->s.eType==ET_MOVER&&(targ->spawnflags&4)&&!targ->isProp){
        if(!G_WeaponIsExplosive(mod)||G_GetTeamFromEntity(targ)==G_GetTeamFromEntity(inflictor))return 0;
    }else if(targ->s.eType==ET_EXPLOSIVE){
        if((targ->parent&&G_GetWeaponClassForMOD(mod)==2)||G_GetTeamFromEntity(targ)==G_GetTeamFromEntity(inflictor)||
           G_GetWeaponClassForMOD(mod)<targ->constructibleStats.weaponclass)return 0;
    }else if(targ->s.eType==ET_MISSILE&&targ->methodOfDeath==45){
        if(targ->s.modelindex2&&G_WeaponIsExplosive(mod)){
            for(i=0;i<2;++i)if((mapEnt=G_FindMapEntityData(&mapEntityData[i],targ-g_entities))!=NULL)
                G_FreeMapEntityData(&mapEntityData[i],mapEnt);
            if(attacker->client)AddScore(attacker,1);G_ExplodeMissile(targ);
        }return 0;
    }else if(targ->s.eType==ET_CONSTRUCTIBLE){
        if(G_GetTeamFromEntity(targ)==G_GetTeamFromEntity(inflictor)||
           G_GetWeaponClassForMOD(mod)<targ->constructibleStats.weaponclass||
           (mod==26&&!(inflictor->etpro_misc_1&1)))return 0;
    }
    client=targ->client;
    if(client){
        client->tceLastAppliedDamage=0;if(client->noclip)return 0;
        if(attacker->client&&!(dflags&DAMAGE_NO_PROTECTION)&&targ!=attacker&&level.time<client->tceDamageSpawnTime+3000){
            if((g_gametype.integer==7&&!OnSameTeam(targ,attacker))||(g_gametype.integer==5&&OnSameTeam(targ,attacker)))
                G_Damage(attacker,NULL,attacker,NULL,NULL,1000,DAMAGE_NO_PROTECTION,65);
            return 0;
        }
    }
    if(targ->flags&FL_GODMODE)return 0;
    if(damage<1)damage=1;take=damage;
    if(IsHeadShotWeapon(mod)&&client&&wasAlive){
        take=TCE_ApplyBodyDamage(targ,dir,point,damage);if(take==-2)return 1;if(take<0)return 0;
        if(g_gametype.integer==5&&attacker->client&&!OnSameTeam(targ,attacker)){
            if((int)((unsigned)level.time-(unsigned)attacker->client->tceDamageVoiceTime)>0){
                G_Voice(attacker,NULL,SAY_TEAM,"Contact",qfalse);attacker->client->tceDamageVoiceTime=level.time+20000;}
            if((int)((unsigned)level.time-(unsigned)client->tceDamageVoiceTime)>0){
                G_Voice(targ,NULL,SAY_TEAM,"TakingFire",qfalse);client->tceDamageVoiceTime=level.time+20000;}
        }
        if(take<1)return 1;
    }else if(client&&wasAlive){
        client->ps.holdable[4]+=damage;
        if(mod==4||mod==62||mod==18||mod==26){client->ps.holdable[3]+=damage;client->ps.stats[STAT_DEAD_YAW]+=damage/2;}
        else client->ps.holdable[3]+=damage/2;
    }
    if(dir)VectorNormalize(dir);
    if(attacker->client&&targ!=attacker&&targ->health>0)
        attacker->client->ps.persistant[PERS_HITS]+=OnSameTeam(targ,attacker)?-take:take;
    if(g_debugDamage.integer)G_Printf("client:%i health:%i damage:%i mod:%s\n",targ->s.number,targ->health,take,modNames[mod]);
    if(client){
        client->ps.persistant[PERS_ATTACKER]=attacker->s.number;client->damage_blood+=take;
        if(wasAlive)client->tceLastAppliedDamage=take;
        if(dir){VectorCopy(dir,client->damage_from);client->damage_fromWorld=qfalse;}
        else {VectorCopy(targ->r.currentOrigin,client->damage_from);client->damage_fromWorld=qtrue;}
        client->lasthurt_client=attacker->s.number;client->lasthurt_mod=mod;
    }
    if(take){
#ifdef FEATURE_OMNIBOT
        if(client && wasAlive && take > 0) Bot_Event_TakeDamage(targ->s.number,attacker);
#endif
        if(!client)targ->health-=take;
        else {
            if(client->ps.holdable[2]<100&&client->ps.holdable[3]<100&&client->ps.holdable[4]<100)
                targ->health=(int)((double)(100-client->ps.holdable[2])*(100-client->ps.holdable[3])*(100-client->ps.holdable[4])*0.00009999999747378752);
            else if((mod==4||mod==18)&&client->ps.holdable[3]>100)targ->health=100-client->ps.holdable[3];
            else targ->health=-1;
            client->ps.stats[STAT_HEALTH]=targ->health;
        }
        if(targ->health<1&&(targ->r.svFlags&SVF_POW))targ->health=-1;
        if(targ->health<-174)targ->health=-174;
        if(targ->s.eType==ET_MOVER&&!Q_stricmp(targ->classname,"script_mover"))
            targ->s.dl_intensity=(int)((double)targ->health/targ->count*255);
        if(targ->health<1){
            if(client&&!wasAlive){
                targ->flags|=FL_NO_KNOCKBACK;
                if(client->ps.pm_type==PM_DEAD)G_addStats(targ,attacker,take,mod);
                if(targ->health>-175&&client->ps.pm_time<1&&g_gametype.integer!=7)limbo(targ,qtrue);
                if(!client->deathTime)client->deathTime=level.time;
            }else {
                targ->sound1to2=4;targ->sound2to1=mod;targ->sound2to3=dflags&DAMAGE_RADIUS;
                if(client&&G_GetTeamFromEntity(targ)!=G_GetTeamFromEntity(inflictor))G_AddKillSkillPoints(attacker,mod,4,dflags&DAMAGE_RADIUS);
                if(targ->health<-999)targ->health=-999;targ->enemy=attacker;targ->deathType=mod;
                if(targ->die){if(client&&attacker->client)client->tceLastKillingWeapon=attacker->client->ps.weapon;
                    targ->die(targ,inflictor,attacker,take,mod);}
                if((targ->s.eType==ET_MOVER&&!Q_stricmp(targ->classname,"script_mover")&&(targ->spawnflags&8))||!targ->inuse)return 0;
                if(targ->health<1){
                    if(targ->r.svFlags&SVF_BOT){if(wasAlive&&TCE_INTERNAL_BOT(targ))Bot_ScriptEvent(targ->s.number,"death","");}
                    else if((targ->s.eType!=ET_CONSTRUCTIBLE&&targ->s.eType!=ET_EXPLOSIVE)||(targ->s.eType==ET_CONSTRUCTIBLE&&!targ->desstages))G_Script_ScriptEvent(targ,"death","");
                }
                if(targ->s.number<level.maxclients&&(targ->r.svFlags&SVF_BOT)&&TCE_INTERNAL_BOT(targ))BotRecordDeath(targ->s.number,attacker->s.number);
            }
        }else if(targ->pain){
            if(dir){VectorCopy(dir,targ->rotate);VectorCopy(point,targ->pos3);}else {VectorClear(targ->rotate);VectorClear(targ->pos3);}
            targ->pain(targ,attacker,take,point);
        }else G_addStats(targ,attacker,take,mod);
        G_Script_ScriptEvent(targ,"pain",va("%d %d",targ->health,targ->health+take));
        if(targ->s.number<MAX_CLIENTS&&(targ->r.svFlags&SVF_BOT)&&TCE_INTERNAL_BOT(targ))Bot_ScriptEvent(targ->s.number,"pain",va("%d %d",targ->health,targ->health+take));
        if(targ->s.number<level.maxclients&&(targ->r.svFlags&SVF_BOT)&&TCE_INTERNAL_BOT(targ))BotRecordPain(targ->s.number,attacker->s.number,mod);
    }
    if(targ->client){float remaining=(float)(1.0-(double)take/damage);return remaining<0.1f?0.1f:remaining;}
    return 0;
}
