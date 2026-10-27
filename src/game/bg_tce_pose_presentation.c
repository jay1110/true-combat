#include "bg_tce_pose_presentation.h"
#include "bg_tce_pose_clock.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int PresentKey(const tcePosePresentationKey_t *key, int channel) {
    return key && key->profile && (channel==0 || channel==1) &&
        key->animation[0]>=0 && key->animation[0]<1024 &&
        key->animation[1]>=0 && key->animation[1]<1024;
}
static int PresentClock(const tcePosePresentationClip_t *clip, uint32_t phaseQ8,
                         tcePoseClock_t *clock, tcePoseClockSample_t *sample) {
    tcePoseClockClip_t raw;
    uint64_t span,room;
    int minimum,maximum;
    if(!clip) return 0;
    memset(&raw,0,sizeof(raw));
    raw.firstFrame=clip->firstFrame; raw.numFrames=clip->numFrames;
    raw.loopFrames=clip->loopFrames; raw.frameLerp=clip->frameLerp;
    raw.initialLerp=clip->initialLerp; raw.reversed=clip->reversed;
    /* Validate every playable frame, including the legacy N+1 cycle's
     * outlier when loopFrames exceeds numFrames. Sampling only phase0 would
     * miss an invalid later frame. The shared clock owns these bounds. */
    if(!BG_TCEPoseClockFrameBounds(&raw,&minimum,&maximum) ||
       minimum<0 || maximum>=8192) return 0;
    /* Shared clock owns clip/tail validity and canonical phase semantics.
     * model0 is an unused private placeholder, never a serialized model ID. */
    BG_TCEPoseClockReset(clock,1,0);
    if(!BG_TCEPoseClockAssign(clock,1,0,&raw,0)) return 0;
    /* Conservative codec capacity, not a second loop-validity rule. Covers
     * the initial sequence plus a complete shared-clock tail representation.
     * The actual shipped profiles fit comfortably; never truncate a phase. */
    room=TCE_POSE_PHASE_MASK/256u;
    span=(uint64_t)raw.numFrames+(uint64_t)raw.loopFrames;
    if((uint64_t)raw.initialLerp>room ||
       span>(room-(uint64_t)raw.initialLerp)/(uint64_t)raw.frameLerp) return 0;
    clock->elapsedQ16=(uint64_t)phaseQ8<<8;
    return BG_TCEPoseClockSample(clock,0,sample) &&
        sample->elapsedQ16==((uint64_t)phaseQ8<<8) && sample->oldFrame>=0 &&
        sample->oldFrame<8192 && sample->frame>=0 && sample->frame<8192;
}
int BG_TCEPosePhasePack(const tcePosePresentationKey_t *key, int channel,
                        const tcePosePresentationClip_t *clip,
                        uint64_t elapsedQ8, uint32_t *word) {
    tcePoseClock_t clock;
    tcePoseClockSample_t sample;
    if(word) *word=0;
    if(!word || !PresentKey(key,channel) || elapsedQ8>TCE_POSE_PHASE_MASK ||
       !PresentClock(clip,(uint32_t)elapsedQ8,&clock,&sample)) return 0;
    *word=TCE_POSE_PHASE_VALID|TCE_POSE_PHASE_VERSION_1|(uint32_t)elapsedQ8;
    return 1;
}
int BG_TCEPosePhaseRead(uint32_t word, int channel,
                        const tcePosePresentationKey_t *observed,
                        const tcePosePresentationKey_t *expected,
                        const tcePosePresentationClip_t *clip,
                        tcePosePresentationSample_t *sample) {
    tcePoseClock_t clock;
    tcePoseClockSample_t raw;
    uint32_t elapsed=word&TCE_POSE_PHASE_MASK;
    if(sample) memset(sample,0,sizeof(*sample));
    if(!sample || !PresentKey(observed,channel) || !PresentKey(expected,channel) ||
       observed->profile!=expected->profile || observed->animation[channel]!=expected->animation[channel] ||
       !(word&TCE_POSE_PHASE_VALID) ||
       (word&TCE_POSE_PHASE_VERSION_MASK)!=TCE_POSE_PHASE_VERSION_1 ||
       !PresentClock(clip,elapsed,&clock,&raw)) return 0;
    sample->elapsedQ8=elapsed; sample->oldFrame=raw.oldFrame; sample->frame=raw.frame;
    sample->backlerp=raw.backlerp; sample->held=raw.held; sample->finished=raw.finished;
    return 1;
}
int BG_TCEPosePhaseStep(uint32_t word, int channel,
                        const tcePosePresentationKey_t *observed,
                        const tcePosePresentationKey_t *expected,
                        const tcePosePresentationClip_t *clip,
                        uint32_t fromTime, uint32_t toTime, uint32_t rateQ8,
                        uint32_t *advanced) {
    tcePosePresentationSample_t sample;
    tcePoseClock_t clock;
    tcePoseClockSample_t raw;
    if(advanced) *advanced=0;
    if(!advanced || toTime-fromTime>INT32_MAX || rateQ8>TCE_POSE_PHASE_RATE_MAX ||
       !BG_TCEPosePhaseRead(word,channel,observed,expected,clip,&sample) ||
       !PresentClock(clip,sample.elapsedQ8,&clock,&raw)) return 0;
    clock.time=fromTime; clock.rateQ16=rateQ8<<8;
    if(!BG_TCEPoseClockAdvance(&clock,toTime)) return 0;
    return BG_TCEPosePhasePack(observed,channel,clip,clock.elapsedQ16>>8,advanced);
}
static int CompactClip(const tcePosePresentationClip_t *clip,tcePoseClockClip_t *raw,
                        uint64_t *maximum,uint64_t *error) {
    int minimum,last;
    uint64_t bound,frame;
    if(!clip) return 0;
    memset(raw,0,sizeof(*raw));raw->firstFrame=clip->firstFrame;raw->numFrames=clip->numFrames;
    raw->loopFrames=clip->loopFrames;raw->frameLerp=clip->frameLerp;
    raw->initialLerp=clip->initialLerp;raw->reversed=clip->reversed;
    if(!BG_TCEPoseClockFrameBounds(raw,&minimum,&last) || last>=8192 ||
       !BG_TCEPoseClockPhaseLimit(raw,maximum)) return 0;
    /* Rounded decoding adds at most one Q16 unit to half a grid interval.
     * Division first avoids overflowing even for valid giant metadata. */
    bound=*maximum/(2u*TCE_POSE_COMPACT_MASK)+
          !!(*maximum%(2u*TCE_POSE_COMPACT_MASK))+1u;
    frame=(uint64_t)raw->frameLerp<<16;
    if(bound>frame/128u) return 0;
    if(error) *error=bound;
    return 1;
}
/* Exact round(maximum*ordinal/16383), without a wide product or FP rounding. */
static uint64_t CompactDecode(uint64_t maximum,uint32_t ordinal) {
    return (maximum/TCE_POSE_COMPACT_MASK)*ordinal+
        ((maximum%TCE_POSE_COMPACT_MASK)*ordinal+TCE_POSE_COMPACT_MASK/2u)/TCE_POSE_COMPACT_MASK;
}
int BG_TCEPoseCompactPrecision(const tcePosePresentationClip_t *clip,uint64_t *maximumErrorQ16) {
    tcePoseClockClip_t raw;uint64_t maximum,error;
    if(maximumErrorQ16) *maximumErrorQ16=0;
    if(!maximumErrorQ16 || !CompactClip(clip,&raw,&maximum,&error)) return 0;
    *maximumErrorQ16=error;return 1;
}
int BG_TCEPoseCompactPack(const tcePosePresentationClip_t *clip,uint64_t elapsedQ16,uint16_t *code) {
    tcePoseClockClip_t raw;uint64_t maximum,canonical,lowerValue,upperValue;
    uint32_t low=0,high=TCE_POSE_COMPACT_MASK,upper,ordinal;
    if(code) *code=0;
    if(!code || !CompactClip(clip,&raw,&maximum,NULL) ||
       !BG_TCEPoseClockNormalizePhase(&raw,elapsedQ16,&canonical) || canonical!=elapsedQ16) return 0;
    /* First decoded grid point at/above phase. This works for arbitrarily
     * large valid spans without maximum*16383 overflowing. */
    while(low<high) {
        uint32_t middle=low+(high-low)/2u;
        if(CompactDecode(maximum,middle)<elapsedQ16) low=middle+1u;else high=middle;
    }
    upper=low;ordinal=upper;
    if(upper) {
        lowerValue=CompactDecode(maximum,upper-1u);upperValue=CompactDecode(maximum,upper);
        if(elapsedQ16-lowerValue<=upperValue-elapsedQ16) ordinal=upper-1u;
    }
    *code=(uint16_t)(TCE_POSE_COMPACT_VALID|TCE_POSE_COMPACT_VERSION|ordinal);return 1;
}
int BG_TCEPoseCompactSample(const tcePosePresentationClip_t *clip,uint16_t code,
                            uint32_t fromTime,uint32_t toTime,uint32_t rateQ8,
                            tcePoseCompactSample_t *out) {
    tcePoseClockClip_t raw;tcePoseClock_t clock;tcePoseClockSample_t sample;
    uint64_t maximum;
    if(out) memset(out,0,sizeof(*out));
    if(!out || (code&~TCE_POSE_COMPACT_MASK)!=(TCE_POSE_COMPACT_VALID|TCE_POSE_COMPACT_VERSION) ||
       rateQ8>TCE_POSE_PHASE_RATE_MAX || toTime-fromTime>INT32_MAX ||
       !CompactClip(clip,&raw,&maximum,NULL)) return 0;
    BG_TCEPoseClockReset(&clock,1,fromTime);
    if(!BG_TCEPoseClockAssign(&clock,1,0,&raw,fromTime)) return 0;
    clock.elapsedQ16=CompactDecode(maximum,code&TCE_POSE_COMPACT_MASK);clock.rateQ16=rateQ8<<8;
    if(!BG_TCEPoseClockSample(&clock,toTime,&sample)) return 0;
    out->elapsedQ16=sample.elapsedQ16;out->oldFrame=sample.oldFrame;out->frame=sample.frame;
    out->backlerp=sample.backlerp;out->held=sample.held;out->finished=sample.finished;return 1;
}
static uint32_t PresentHashWord(uint32_t hash,uint32_t value) {
    int i;for(i=0;i<4;++i) {hash^=value&255u;hash*=16777619u;value>>=8;}return hash;
}
static uint32_t PresentHashText(uint32_t hash,const char *text) {
    int i;for(i=0;i<MAX_QPATH;++i) {
        unsigned char c=(unsigned char)text[i];
        if(c>='A'&&c<='Z') c=(unsigned char)(c+'a'-'A');
        if(c=='\\') c='/';
        hash^=c;hash*=16777619u;if(!c) return hash;
    }return 0;
}
uint32_t BG_TCEPosePresentationProfile(const bg_character_t *character,const bg_characterDef_t *definition) {
    const animModelInfo_t *model;uint32_t hash=2166136261u;int i;
    if(!character||!definition)return 0;
    model=character->animModelInfo;
    if(!model||model->numAnimations<1||model->numAnimations>MAX_MODEL_ANIMATIONS) return 0;
    if(!character->characterFile[0]||!definition->mesh[0]||!definition->hudhead[0]||
       !model->animationGroup[0]||!model->animationScript[0])return 0;
    hash=PresentHashText(hash,character->characterFile);if(!hash)return 0;
    hash=PresentHashText(hash,definition->mesh);if(!hash)return 0;
    hash=PresentHashText(hash,definition->hudhead);if(!hash)return 0;
    hash=PresentHashText(hash,model->animationGroup);if(!hash)return 0;
    hash=PresentHashText(hash,model->animationScript);if(!hash)return 0;
    hash=PresentHashWord(hash,(uint32_t)model->numAnimations);
    for(i=0;i<model->numAnimations;++i) {
        const animation_t *a=model->animations[i];if(!a) return 0;
        hash=PresentHashText(hash,a->name);if(!hash)return 0;
        hash=PresentHashWord(hash,(uint32_t)a->firstFrame);
        hash=PresentHashWord(hash,(uint32_t)a->numFrames);hash=PresentHashWord(hash,(uint32_t)a->loopFrames);
        hash=PresentHashWord(hash,(uint32_t)a->frameLerp);hash=PresentHashWord(hash,(uint32_t)a->initialLerp);
        hash=PresentHashWord(hash,(uint32_t)a->flags);hash=PresentHashWord(hash,(uint32_t)a->movetype);
    }return hash?hash:1u;
}
uint32_t BG_TCEPosePresentationSpeedProfile(uint32_t rawProfile,const int *speeds,int count) {
    uint32_t hash;int i;
    if(!rawProfile||!speeds||count<1||count>MAX_MODEL_ANIMATIONS)return 0;
    hash=PresentHashWord(rawProfile,0x53504432u);hash=PresentHashWord(hash,(uint32_t)count);
    for(i=0;i<count;++i) {
        if(speeds[i]!=INT_MIN&&(speeds[i]<0||speeds[i]>8192))return 0;
        hash=PresentHashWord(hash,(uint32_t)speeds[i]);
    }return hash?hash:1u;
}
uint32_t BG_TCEPosePresentationPairProfile(uint32_t renderProfile,uint32_t animationRawProfile) {
    uint32_t hash;
    if(!renderProfile||!animationRawProfile)return 0;
    hash=PresentHashWord(renderProfile,0x50414932u);
    hash=PresentHashWord(hash,animationRawProfile);return hash?hash:1u;
}
int BG_TCEPosePresentationManifest(const uint32_t *profiles,int count,char *out,int size) {
    static const char hex[]="0123456789abcdef";
    uint32_t sorted[TCE_POSE_PRESENTATION_PROFILES];int i,j,n=0,offset=2;
    if(out&&size>0)out[0]=0;
    if(!out||!profiles||count<1||count>TCE_POSE_PRESENTATION_PROFILES||size<3)return 0;
    for(i=0;i<count;++i) {
        uint32_t value=profiles[i];if(!value)return 0;
        for(j=0;j<n&&sorted[j]<value;++j) {}
        if(j<n&&sorted[j]==value)continue;
        memmove(&sorted[j+1],&sorted[j],(size_t)(n-j)*sizeof(sorted[0]));sorted[j]=value;++n;
    }
    if(size<3+9*n)return 0;
    out[0]='2';out[1]=';';
    for(i=0;i<n;++i) {
        for(j=7;j>=0;--j)out[offset++]=hex[(sorted[i]>>(j*4))&15u];
        out[offset++]=',';
    }out[offset]=0;return 1;
}
static uint32_t PresentationManifestCheck(const char *manifest,const uint32_t *profiles,int count,int match) {
    uint32_t digest=2166136261u,previous=0;int i,j,offset=2,found,entries=0;
    if(!manifest||manifest[0]!='2'||manifest[1]!=';'||
       (match&&(!profiles||count<1||count>TCE_POSE_PRESENTATION_PROFILES)))return 0;
    digest=(digest^(unsigned)'2')*16777619u;digest=(digest^(unsigned)';')*16777619u;
    while(entries<TCE_POSE_PRESENTATION_PROFILES) {
        uint32_t value=0;
        if(!manifest[offset])return entries?digest?digest:1u:0;
        for(i=0;i<8;++i) {
            unsigned char c=(unsigned char)manifest[offset++];unsigned digit;
            if(c>='0'&&c<='9')digit=c-'0';else if(c>='a'&&c<='f')digit=c-'a'+10u;else return 0;
            digest=(digest^c)*16777619u;value=(value<<4)|digit;
        }
        if(manifest[offset++]!=','||!value||value<=previous)return 0;
        digest=(digest^(unsigned)',')*16777619u;previous=value;++entries;
        if(match) {
            found=0;for(j=0;j<count;++j)if(profiles[j]==value) {found=1;break;}
            if(!found)return 0;
        }
    }
    return !manifest[offset]?(digest?digest:1u):0;
}
uint32_t BG_TCEPosePresentationManifestDigest(const char *manifest) {
    return PresentationManifestCheck(manifest,NULL,0,0);
}
uint32_t BG_TCEPosePresentationManifestMatch(const char *manifest,const uint32_t *profiles,int count) {
    return PresentationManifestCheck(manifest,profiles,count,1);
}
int BG_TCEPosePresentationMoveSpeed(int firstFrame,int numFrames,int frameLerp,
                                   int ladder,tcePoseFeet_t feet,void *user,int *outSpeed) {
    float positions[2][3];int previous[2][3],current[2][3],frame,foot,axis,contact;
    uint64_t distance=0,denominator,speed;
    if(!outSpeed||!feet||firstFrame<0||numFrames<2||numFrames>8192||
       firstFrame>8192-numFrames||frameLerp<1||frameLerp>65535||
       (ladder!=0&&ladder!=1))return 0;
    for(frame=0;frame<numFrames;++frame) {
        if(!feet(user,firstFrame+frame,positions))return 0;
        for(foot=0;foot<2;++foot)for(axis=0;axis<3;++axis) {
            double value=positions[foot][axis];
            if(!(value>=-8192&&value<=8192))return 0;
            value*=64;current[foot][axis]=(int)(value<0?ceil(value-.5):floor(value+.5));
        }
        if(frame) {
            contact=ladder?(current[0][0]>current[1][0]?0:1):
                           (current[0][2]<current[1][2]?0:1);
            axis=ladder?2:0;
            distance+=(uint64_t)abs(current[contact][axis]-previous[contact][axis]);
        }
        memcpy(previous,current,sizeof(previous));
    }
    denominator=(uint64_t)(numFrames-1)*(uint64_t)frameLerp*64u;
    speed=(distance*1000u+denominator/2u)/denominator;
    if(speed>8192u)return 0;
    *outSpeed=(int)speed;return 1;
}
static int PresentNameContains(const char *text,const char *part) {
    int i,j;if(!text) return 0;
    for(i=0;i<MAX_QPATH&&text[i];++i) {
        for(j=0;part[j]&&i+j<MAX_QPATH;++j) {
            unsigned char c=(unsigned char)text[i+j];if(c>='A'&&c<='Z')c=(unsigned char)(c+'a'-'A');
            if(c!=(unsigned char)part[j])break;
        }if(!part[j])return 1;
    }return 0;
}
int BG_TCEPosePresentationMode(const char *legsName,const char *torsoName,int prone,int ladder) {
    if(prone||PresentNameContains(legsName,"prone")||PresentNameContains(torsoName,"prone"))return TCE_POSE_PRESENT_PRONE;
    if(ladder||PresentNameContains(legsName,"climb")||PresentNameContains(torsoName,"climb"))return TCE_POSE_PRESENT_LADDER;
    return 0;
}
uint32_t BG_TCEPosePresentationRate(int mode,const float velocity[3],int moveSpeed) {
    double speed,rate;int i;
    if(!velocity||mode<TCE_POSE_PRESENT_LADDER||mode>TCE_POSE_PRESENT_PRONE)return 0;
    for(i=0;i<3;++i)if(!(velocity[i]>=-8192&&velocity[i]<=8192))return 0;
    if(moveSpeed<0)return 0;
    if(moveSpeed==0)return TCE_POSE_PHASE_RATE_ONE;
    speed=mode==TCE_POSE_PRESENT_PRONE?sqrt((double)velocity[0]*velocity[0]+(double)velocity[1]*velocity[1]):fabs(velocity[2]);
    rate=speed*256.0/moveSpeed;if(rate>=TCE_POSE_PHASE_RATE_MAX)return TCE_POSE_PHASE_RATE_MAX;
    return (uint32_t)(rate+.5);
}
static int PresentFinite(float value, float bound) {
    return value==value && value>=-bound && value<=bound;
}
static float PresentAngle(float angle) {
    double value=fmod((double)angle,360.0);
    if(value>180.0) value-=360.0;
    if(value<-180.0) value+=360.0;
    return (float)value;
}
int BG_TCEPosePresentationInterpolate(const tcePosePresentationAxesInput_t *older,
                                     const tcePosePresentationAxesInput_t *newer,
                                     float fraction,tcePosePresentationAxesInput_t *out) {
    tcePosePresentationAxesInput_t result;int i;
    if(!older||!newer||!out||!(fraction>=0&&fraction<=1)||older->mode!=newer->mode||
       older->scale!=newer->scale||older->leanMode!=newer->leanMode)return 0;
    result=*older;
    for(i=0;i<3;++i) {
        result.aim[i]=older->aim[i]+fraction*PresentAngle(newer->aim[i]-older->aim[i]);
        result.velocity[i]=older->velocity[i]+fraction*(newer->velocity[i]-older->velocity[i]);
    }
    result.movementYaw=PresentAngle(older->movementYaw+fraction*PresentAngle(newer->movementYaw-older->movementYaw));
    result.lean=older->lean+fraction*(newer->lean-older->lean);
    for(i=0;i<2;++i)result.freelook[i]=older->freelook[i]+fraction*PresentAngle(newer->freelook[i]-older->freelook[i]);
    *out=result;return 1;
}
static void PresentAxis(const float angles[3], float axis[3][3]) {
    const double radians=0.017453292519943295769;
    double p=angles[0]*radians,y=angles[1]*radians,r=angles[2]*radians;
    double sp=sin(p),cp=cos(p),sy=sin(y),cy=cos(y),sr=sin(r),cr=cos(r);
    axis[0][0]=(float)(cp*cy); axis[0][1]=(float)(cp*sy); axis[0][2]=(float)-sp;
    axis[1][0]=(float)(sr*sp*cy-cr*sy); axis[1][1]=(float)(sr*sp*sy+cr*cy); axis[1][2]=(float)(sr*cp);
    axis[2][0]=(float)(cr*sp*cy+sr*sy); axis[2][1]=(float)(cr*sp*sy-sr*cy); axis[2][2]=(float)(cr*cp);
}
int BG_TCEPosePresentationAxes(const tcePosePresentationAxesInput_t *input,
                              tcePosePresentationAxes_t *out) {
    tcePosePresentationAxes_t result;
    float legs[3]={0,0,0},torso[3]={0,0,0},head[3],axis[3][3],lean;
    float velocity[3],speed,length,side;
    int i,j;
    if(out) memset(out,0,sizeof(*out));
    if(!input || !out || input->mode<TCE_POSE_PRESENT_LADDER || input->mode>TCE_POSE_PRESENT_PRONE ||
       (input->leanMode!=0 && input->leanMode!=1) || !PresentFinite(input->scale,4) ||
       input->scale<=0 || !PresentFinite(input->movementYaw,180) || !PresentFinite(input->lean,90)) return 0;
    for(i=0;i<3;++i) if(!PresentFinite(input->aim[i],3600) || !PresentFinite(input->velocity[i],8192)) return 0;
    for(i=0;i<2;++i) if(!PresentFinite(input->freelook[i],180)) return 0;
    memset(&result,0,sizeof(result));
    for(i=0;i<3;++i) { head[i]=PresentAngle(input->aim[i]); velocity[i]=input->velocity[i]; }
    legs[1]=PresentAngle(head[1]+input->movementYaw); torso[1]=head[1];
    torso[0]=input->mode==TCE_POSE_PRESENT_PRONE?-3.0f:head[0]*.75f;
    length=(float)sqrt((double)velocity[0]*velocity[0]+(double)velocity[1]*velocity[1]+(double)velocity[2]*velocity[2]);
    if(length>0) {
        for(i=0;i<3;++i) velocity[i]/=length;
        speed=length*.05f; PresentAxis(legs,axis);
        side=speed*(velocity[0]*axis[1][0]+velocity[1]*axis[1][1]+velocity[2]*axis[1][2]); legs[2]-=side;
        side=speed*(velocity[0]*axis[0][0]+velocity[1]*axis[0][1]+velocity[2]*axis[0][2]); legs[0]+=side;
    }
    lean=input->leanMode?input->lean:0;
    if(lean<0) lean*=1.65f;
    torso[2]+=lean; head[2]+=lean;
    head[0]+=input->freelook[0]*.7f; head[1]+=input->freelook[1]*.7f;
    for(i=0;i<3;++i) {
        result.bodyAngles[i]=PresentAngle(legs[i]);
        result.torsoRelativeAngles[i]=PresentAngle(torso[i]-legs[i]);
        result.headRelativeAngles[i]=PresentAngle(head[i]-torso[i]);
    }
    PresentAxis(result.bodyAngles,result.body); PresentAxis(result.torsoRelativeAngles,result.torso);
    PresentAxis(result.headRelativeAngles,result.head);
    for(i=0;i<3;++i) for(j=0;j<3;++j) result.body[i][j]*=input->scale;
    *out=result;
    return 1;
}
