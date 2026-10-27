#include "bg_tce_pose_origin.h"
#include <limits.h>
#include <math.h>
#include <string.h>
#include "../../compat/compat_pose_origin.h"

static int PoseOriginFinite(const float value[3]) {
    int i;for(i=0;i<3;++i)if(!isfinite(value[i]))return 0;return 1;
}
static int PoseOriginTime(uint32_t time) {int result;memcpy(&result,&time,sizeof(result));return result;}
void BG_TCEPoseMoverState(const entityState_t *state,tcePoseMoverState_t *out) {
    memset(out,0,sizeof(*out));out->position=state->pos;out->angles=state->apos;
    out->number=state->number;out->type=state->eType;out->flags=state->eFlags;
    out->parent=state->torsoAnim;out->spline=state->effect2Time;out->distance=state->angles2[0];
}
void BG_TCEPoseOriginState(const entityState_t *state,tcePoseOriginState_t *out) {
    memset(out,0,sizeof(*out));out->position=state->pos;out->number=state->number;
    out->ground=state->groundEntityNum;out->spline=state->effect2Time;
}
static int PoseMoverRead(tcePoseMoverRead_t read,void *user,int number,tcePoseMoverState_t *out) {
    return read&&number>=0&&number<ENTITYNUM_MAX_NORMAL&&read(user,number,out)&&
        out->number==number&&out->type<ET_EVENTS&&isfinite(out->distance);
}

static int PoseMoverFollow(const tcePoseMoverState_t *child,splinePath_t **path,
    float *fraction,qboolean backwards,vec3_t point,vec3_t origin,vec3_t angles) {
    vec3_t direction;
    VectorCopy(point,origin);
    (void)backwards;
    if(child->distance&&!TCE_PosePathOffset(child->distance,path,fraction,point))return 0;
    if(child->distance<0) {VectorSubtract(point,origin,direction);vectoangles(direction,angles);}
    else if(child->distance>0) {VectorSubtract(origin,point,direction);vectoangles(direction,angles);}
    else VectorClear(angles);
    return 1;
}
int BG_TCEPoseMoverEvaluate(int number,uint32_t time,tcePoseMoverRead_t read,
                            void *user,float outOrigin[3],float outAngles[3]) {
    int chain[ENTITYNUM_MAX_NORMAL],count=0,index,i,position,elapsed;
    unsigned char seen[ENTITYNUM_MAX_NORMAL];
    tcePoseMoverState_t node,parent;
    vec3_t origin,angles,point;float fraction=0,part;
    splinePath_t *path=NULL;qboolean backwards=qfalse;int moving=0;
    if(!outOrigin||!outAngles)return 0;
    memset(seen,0,sizeof(seen));index=number;
    for(;;) {
        if(index<0||index>=ENTITYNUM_MAX_NORMAL||seen[index]||count>=ENTITYNUM_MAX_NORMAL||
           !PoseMoverRead(read,user,index,&node))goto failure;
        seen[index]=1;chain[count++]=index;
        if(!(node.flags&EF_PATH_LINK))break;
        index=node.parent;
    }
    if(count==1) {
        BG_EvaluateTrajectory(&node.position,PoseOriginTime(time),origin,qfalse,node.spline);
        BG_EvaluateTrajectory(&node.angles,PoseOriginTime(time),angles,qtrue,node.spline);
    } else {
        for(i=count-2;i>=0;--i) {
            if(!PoseMoverRead(read,user,chain[i],&node)||!PoseMoverRead(read,user,chain[i+1],&parent))goto failure;
            if(!(parent.flags&EF_PATH_LINK)) {
                if((int)parent.position.trType!=16) {
                    moving=0;VectorCopy(node.position.trBase,origin);VectorCopy(node.angles.trBase,angles);continue;
                }
                path=BG_GetSplineData(parent.spline,&backwards);if(!path)goto failure;
                elapsed=PoseOriginTime(time-(uint32_t)parent.position.trTime);
                fraction=parent.position.trDuration?elapsed/((float)parent.position.trDuration):0;
                if(fraction<0)fraction=0;else if(fraction>1)fraction=1;
                if(backwards)fraction=1-fraction;
                position=(int)floor(fraction*MAX_SPLINE_SEGMENTS);
                if(position>=MAX_SPLINE_SEGMENTS) {
                    position=MAX_SPLINE_SEGMENTS-1;part=path->segments[position].length;
                } else part=((fraction*MAX_SPLINE_SEGMENTS)-position)*path->segments[position].length;
                VectorMA(path->segments[position].start,part,path->segments[position].v_norm,point);
                if(parent.angles.trBase[0]&&!TCE_PosePathOffset(parent.angles.trBase[0],&path,&fraction,point))goto failure;
                if(!PoseMoverFollow(&node,&path,&fraction,backwards,point,origin,angles))goto failure;
                moving=1;
            } else if(moving) {
                if(!PoseMoverFollow(&node,&path,&fraction,backwards,point,origin,angles))goto failure;
            } else {
                VectorCopy(node.position.trBase,origin);VectorCopy(node.angles.trBase,angles);
            }
        }
    }
    if(!PoseOriginFinite(origin)||!PoseOriginFinite(angles))goto failure;
    VectorCopy(origin,outOrigin);VectorCopy(angles,outAngles);return 1;
failure:
    VectorClear(outOrigin);VectorClear(outAngles);return 0;
}
int BG_TCEPosePresentationOrigin(const tcePoseOriginState_t *older,
    const tcePoseOriginState_t *newer,uint32_t from,uint32_t nextTime,uint32_t time,
    tcePoseMoverRead_t read,void *user,float out[3]) {
    tcePoseOriginState_t base,next;
    tcePoseMoverState_t mover;
    vec3_t origin,future,oldMover,newMover,oldAngles,newAngles,deltaAngles;
    vec3_t relative,rotated,shift,translation,matrix[3],transpose[3];
    uint32_t elapsed=time-from,interval=nextTime-from;int i;
    if(!out)return 0;
    if(!older||elapsed>INT32_MAX)goto failure;
    base=*older;if(newer)next=*newer;
    if(newer&&interval&&interval<=INT32_MAX&&elapsed<=interval&&
       (base.position.trType==TR_INTERPOLATE||
        (base.position.trType==TR_LINEAR_STOP&&base.number<MAX_CLIENTS))) {
        float fraction=(float)((double)elapsed/interval);
        BG_EvaluateTrajectory(&base.position,PoseOriginTime(from),origin,qfalse,base.spline);
        BG_EvaluateTrajectory(&next.position,PoseOriginTime(nextTime),future,qfalse,base.spline);
        for(i=0;i<3;++i)origin[i]=TCE_PoseOriginInterpolate(origin[i],future[i],fraction);
    } else {
        BG_EvaluateTrajectory(&base.position,PoseOriginTime(time),origin,qfalse,base.spline);
        if(base.ground>0&&base.ground<ENTITYNUM_MAX_NORMAL) {
            if(!PoseMoverRead(read,user,base.ground,&mover))goto failure;
            if(mover.type==ET_MOVER) {
                if(!BG_TCEPoseMoverEvaluate(base.ground,from,read,user,oldMover,oldAngles)||
                   !BG_TCEPoseMoverEvaluate(base.ground,time,read,user,newMover,newAngles))goto failure;
                VectorSubtract(newAngles,oldAngles,deltaAngles);
                BG_CreateRotationMatrix(deltaAngles,transpose);BG_TransposeMatrix((const vec3_t *)transpose,matrix);
                /* G_MoverPush has already moved the pusher before invoking
                 * G_TryPushingEntity. The client target is still unshifted. */
                VectorSubtract(origin,newMover,relative);VectorCopy(relative,rotated);
                BG_RotatePoint(rotated,(const vec3_t *)matrix);VectorSubtract(rotated,relative,shift);
                VectorSubtract(newMover,oldMover,translation);
                VectorAdd(origin,translation,origin);VectorAdd(origin,shift,origin);
            }
        }
    }
    if(!PoseOriginFinite(origin))goto failure;
    VectorCopy(origin,out);return 1;
failure:
    VectorClear(out);return 0;
}
