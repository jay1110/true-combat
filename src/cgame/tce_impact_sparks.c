#include "cg_local.h"
#include "tce_impact_sparks.h"
#include "../game/tce_trajectory.h"
extern void CG_FreeLocalEntity(localEntity_t *);
static double sparkRandom(void){return (rand()&32767)*(double)(1.0f/32767.0f);}
static void normalizeSpark(vec3_t v){double d=sqrt((double)v[0]*v[0]+(double)v[1]*v[1]+(double)v[2]*v[2]);int j;if(d)for(j=0;j<3;j++)v[j]=(float)(v[j]/d);}
void TCE_CG_FlatSparks(vec3_t origin,vec3_t direction,int count,float intensity,qhandle_t shader){int i,j;vec3_t v;localEntity_t*le;
 for(i=0;i<count;i++){
  le=CG_AllocLocalEntity();le->leType=(leType_t)TCE_LE_FLAT_SPARK;le->startTime=cg.time;
  le->endTime=cg.time-(int)((sparkRandom()+1)*.5*-100);le->fadeInTime=le->endTime-50;
  le->color[0]=le->color[1]=le->color[2]=(float)((sparkRandom()+1)*intensity*.5);
  le->color[2]=(float)((sparkRandom()+1)*le->color[2]*.5);VectorCopy(origin,le->refEntity.origin);
  for(j=0;j<3;j++)v[j]=(float)((sparkRandom()-.5)*2);normalizeSpark(v);
  if((double)v[0]*direction[0]+(double)v[1]*direction[1]+(double)v[2]*direction[2]<0)VectorInverse(v);
  for(j=0;j<3;j++)v[j]=(float)((sparkRandom()*2+1)*5.333333492279053f*v[j]+origin[j]);
  VectorCopy(v,le->refEntity.oldorigin);le->refEntity.radius=1;le->refEntity.customShader=shader;
 }
}
void TCE_CG_GlowSparks(vec3_t origin,vec3_t direction,int count,float intensity,qhandle_t shader){int i,j;vec3_t v;localEntity_t*le;
 for(i=0;i<count;i++){
  le=CG_AllocLocalEntity();le->leType=(leType_t)TCE_LE_GLOW_SPARK;le->leFlags=128;le->startTime=cg.time;
  le->endTime=cg.time-(int)((sparkRandom()+1)*.5*-2000);le->fadeInTime=le->endTime-1000;
  le->color[0]=le->color[1]=le->color[2]=(float)((sparkRandom()+1)*intensity*.5);
  le->color[2]=(float)((sparkRandom()+1)*le->color[2]*.5);le->pos.trType=TR_GRAVITY_LOW;le->pos.trTime=cg.time-(rand()&15);
  le->bounceFactor=(float)(sparkRandom()*.2f+.3f);VectorCopy(origin,le->refEntity.origin);VectorCopy(origin,le->pos.trBase);
  for(j=0;j<3;j++)v[j]=(float)((sparkRandom()-.5)*2);normalizeSpark(v);VectorAdd(v,direction,v);
  for(j=0;j<3;j++)le->pos.trDelta[j]=(float)((sparkRandom()+2)*20*v[j]);
  le->refEntity.radius=.5f;le->refEntity.customShader=shader;
 }
}
static float sparkFade(localEntity_t*le){float f=1;if(cg.time>le->fadeInTime){double rate=1.0/(le->endTime-le->fadeInTime);le->lifeRate=(float)rate;f=(float)(1-(cg.time-le->fadeInTime)*rate);if(f<0)f=0;}return f;}
static void sparkQuad(polyVert_t*p,localEntity_t*le,float fade,int glow){int i,j;float factors[4]={fade,fade,fade,fade};const float st[4][2]={{1,1},{0,1},{0,0},{1,0}};if(glow && (le->leFlags&128) && cg.time>le->fadeInTime){factors[0]=(float)sqrt(fade);factors[2]=fade*fade;}for(i=0;i<4;i++){p[i].st[0]=st[i][0];p[i].st[1]=st[i][1];for(j=0;j<4;j++)p[i].modulate[j]=(byte)(int)((double)factors[j]*le->color[j]*255);}trap_R_AddPolyToScene(le->refEntity.customShader,4,p);}
void TCE_CG_AddFlatSpark(localEntity_t*le){polyVert_t p[4];vec3_t delta,start,end,a,b,width;float fade=sparkFade(le);double t=sqrt((double)(cg.time-le->startTime)/(le->endTime-le->startTime));int j;
 VectorSubtract(le->refEntity.oldorigin,le->refEntity.origin,delta);
 for(j=0;j<3;j++){start[j]=(float)(t*.5*delta[j]+le->refEntity.origin[j]);end[j]=(float)((t+.5)*delta[j]+le->refEntity.origin[j]);}
 VectorSubtract(le->refEntity.origin,cg.refdef.vieworg,a);normalizeSpark(a);VectorSubtract(le->refEntity.oldorigin,cg.refdef.vieworg,b);normalizeSpark(b);CrossProduct(a,b,width);normalizeSpark(width);
 memset(p,0,sizeof(p));for(j=0;j<3;j++){p[0].xyz[j]=(float)((double)le->refEntity.radius*width[j]+start[j]);p[1].xyz[j]=(float)(-(double)le->refEntity.radius*width[j]+start[j]);p[2].xyz[j]=(float)(-(double)le->refEntity.radius*width[j]+end[j]);p[3].xyz[j]=(float)((double)le->refEntity.radius*width[j]+end[j]);}sparkQuad(p,le,fade,0);
}
void TCE_CG_AddGlowSpark(localEntity_t*le){polyVert_t p[4];vec3_t end,top;trace_t tr;float fade;int j;
 if(le->pos.trType!=TR_STATIONARY){TCE_BG_EvaluateTrajectory(&le->pos,cg.time,end,qfalse,-1,1.0f);CG_Trace(&tr,le->refEntity.origin,NULL,NULL,end,-1,CONTENTS_SOLID);if(tr.fraction==1){VectorCopy(end,le->refEntity.origin);}else{if(trap_CM_PointContents(tr.endpos,0)&CONTENTS_NODROP){CG_FreeLocalEntity(le);return;}CG_ReflectVelocity(le,&tr);}}
 fade=sparkFade(le);memset(p,0,sizeof(p));for(j=0;j<3;j++){top[j]=(float)((double)(le->refEntity.radius+le->refEntity.radius)*cg.refdef.viewaxis[2][j]+le->refEntity.origin[j]);p[0].xyz[j]=(float)((double)le->refEntity.radius*cg.refdef.viewaxis[1][j]+le->refEntity.origin[j]);p[1].xyz[j]=(float)(-(double)le->refEntity.radius*cg.refdef.viewaxis[1][j]+le->refEntity.origin[j]);p[2].xyz[j]=(float)(-(double)le->refEntity.radius*cg.refdef.viewaxis[1][j]+top[j]);p[3].xyz[j]=(float)((double)le->refEntity.radius*cg.refdef.viewaxis[1][j]+top[j]);}sparkQuad(p,le,fade,1);
}
void CG_FlatSparks(vec3_t origin,vec3_t direction,int count){TCE_CG_FlatSparks(origin,direction,count,cg.tceSparkIntensity,cgs.media.tceFlatSparkShader);}
void CG_GlowSparks(vec3_t origin,vec3_t direction,int count){TCE_CG_GlowSparks(origin,direction,count,cg.tceSparkIntensity,cgs.media.tceGlowSparkShader);}
