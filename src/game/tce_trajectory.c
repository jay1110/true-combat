#include "q_shared.h"
#include "bg_public.h"
#include "tce_trajectory.h"

void BG_CalculateSpline_r(splinePath_t *, vec3_t, vec3_t, float);
qboolean BG_TraverseSpline(float *, splinePath_t **);
static int elapsed(int a,int b){return (int)((unsigned)a-(unsigned)b);}
static void interpolate(const vec3_t a,const vec3_t b,float fraction,vec3_t result){
    result[2]=b[2]-a[2];
    result[0]=(float)(((double)b[0]-a[0])*fraction+a[0]);
    result[1]=(float)(((double)b[1]-a[1])*fraction+a[1]);
    result[2]=(float)((double)fraction*result[2]+a[2]);
}
void TCE_BG_EvaluateTrajectory(const trajectory_t *tr,int atTime,vec3_t result,
    qboolean isAngle,int splinePath,float gravityScale){
    double time=elapsed(atTime,tr->trTime)*.001,scale,roll;
    float t,acceleration,frac,t2;
    int i,index,end=(int)((unsigned)tr->trTime+(unsigned)tr->trDuration);
    vec3_t a,b,v;
    splinePath_t *path,*other;
    qboolean backwards=qfalse,dampIn=qfalse,dampOut=qfalse;
    switch((int)tr->trType){
    case 0:case 1:case 9:VectorCopy(tr->trBase,result);return;
    case 2:break;
    case 3:
        if(atTime>end)time=elapsed(end,tr->trTime)*.001;
        if(time<0)time=0;
        break;
    case 5:time=sin(((double)elapsed(atTime,tr->trTime)/tr->trDuration)*(double)6.2831854820251465f);break;
    case 6:case 7:case 8:case 13:
        for(i=0;i<2;++i)result[i]=(float)(time*tr->trDelta[i]+tr->trBase[i]);
        scale=tr->trType==6?400:tr->trType==7?200:tr->trType==8?100:250;
        result[2]=(float)((time*tr->trDelta[2]+tr->trBase[2])-time*time*scale);
        return;
    case 14:
        t=(float)time;
        if(time<0)t=0;else if(t>tr->trDuration*.001)t=(float)(tr->trDuration*.001);
        for(i=0;i<3;++i)result[i]=(float)((double)t*tr->trDelta[i]+tr->trBase[i]);
        result[2]=(float)(result[2]-((double)tr->trDuration*.005f)*time*8.0f);
        return;
    case 10:case 11:case 15:
        if(tr->trType!=15 && atTime>end)time=elapsed(end,tr->trTime)*.001;
        t=(float)time;
        if(tr->trType==15){if(time<0)t=0;else if(t>tr->trDuration*.001)t=(float)(tr->trDuration*.001);}
        scale=sqrt((double)tr->trDelta[0]*tr->trDelta[0]+(double)tr->trDelta[1]*tr->trDelta[1]+(double)tr->trDelta[2]*tr->trDelta[2]);
        acceleration=(float)(scale/(tr->trDuration*.001));
        VectorNormalize2(tr->trDelta,result);
        scale=(tr->trType==10?(double)acceleration:-(double)acceleration)*t*t*.5;
        for(i=0;i<3;++i){
            double base=tr->trBase[i];
            if(tr->trType!=10){base+=(double)t*tr->trDelta[i];if(i==1)base=(float)base;}
            result[i]=(float)(base+result[i]*scale);
        }
        if(tr->trType==15)result[2]=(float)(result[2]-elapsed(atTime,tr->trTime)*.001*gravityScale*16.0f);
        return;
    case 12:case 16:
        path=BG_GetSplineData(splinePath,&backwards);if(!path)return;
        t=tr->trDuration?(float)((double)elapsed(atTime,tr->trTime)/tr->trDuration):0;
        if(t<0)t=0;else if(t>1)t=1;
        if(backwards)t=1-t;
        if(tr->trType==16){
            index=(int)floor((double)t*16.0f);
            if(index>=16){index=15;frac=path->segments[index].length;}
            else frac=(float)(((double)t*16-index)*path->segments[index].length);
            if(isAngle&&!tr->trBase[0]){vectoangles(path->segments[index].v_norm,result);return;}
            for(i=0;i<3;++i)result[i]=(float)((double)frac*path->segments[index].v_norm[i]+path->segments[index].start[i]);
            if(!isAngle)return;
            VectorCopy(result,v);
            BG_LinearPathOrigin2(tr->trBase[0],&path,&t,v,backwards);
            if(tr->trBase[0]<0)VectorSubtract(v,result,result);else VectorSubtract(result,v,result);
            vectoangles(result,result);return;
        }
        t2=t;BG_CalculateSpline_r(path,a,b,t);
        if(!isAngle){interpolate(a,b,t,result);return;}
        if(tr->trBase[0]){
            other=path;t2=(float)((double)tr->trBase[0]/path->length+t2);
            if(BG_TraverseSpline(&t2,&other)){
                interpolate(a,b,t,result);BG_CalculateSpline_r(other,a,b,t2);interpolate(a,b,t2,v);
                if(tr->trBase[0]<0)VectorSubtract(result,v,result);else VectorSubtract(v,result,result);
            }else VectorSubtract(b,a,result);
        }else VectorSubtract(b,a,result);
        vectoangles(result,result);
        roll=tr->trBase[1];
        if(roll>=10000||roll< -10000){dampIn=qtrue;roll+=roll<0?10000:-10000;}
        if(roll>=1000||roll< -1000){dampOut=qtrue;roll+=roll<0?1000:-1000;}
        if(dampIn&&dampOut)scale=(sin(((double)t*2-1)*1.5707963705062866f)+1)*tr->trBase[2]*.5;
        else if(dampIn)scale=sin((double)t*1.5707963705062866f)*tr->trBase[2];
        else if(dampOut)scale=(1-sin((1.0-t)*1.5707963705062866f))*tr->trBase[2];
        else scale=(double)t*tr->trBase[2];
        result[2]=(float)(roll+scale);return;
    default:Com_Error(ERR_DROP,"BG_EvaluateTrajectory: unknown trType: %i",tr->trTime);return;
    }
    for(i=0;i<3;++i)result[i]=(float)(time*tr->trDelta[i]+tr->trBase[i]);
}
