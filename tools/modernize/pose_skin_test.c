/* Actual optional exact-surface API, with independent local-asset expectations.
 * No engine service, production provider, or live game is needed. */
#include "tce_pose_math.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pose_skin_cases.inc"

static tcePoseMath_t mathState;
static tcePoseSkeleton_t models[SKIN_MODEL_COUNT],incompatible;
static tcePoseSkinMesh_t body,badBody,zeroBody;
static tcePoseHeadMesh_t head,badHead,zeroHead;
static tcePoseSkin_t pose,wide,second,wideSecond,zeroPose;
static tcePoseSkeleton_t permutedModels[SKIN_MODEL_COUNT];
static tcePoseSurfaceRegistry_t registry;
static unsigned char *modelBytes[SKIN_MODEL_COUNT],*bodyBytes,*headBytes,*expectBytes;
static size_t bodySize,headSize;
static unsigned int checks,failures,loadCalls,gaps,legControls,coverControls;
static unsigned int boundHeads,boundCovers;
#ifndef SKIN_BOUND_CASE_COUNT
#define SKIN_BOUND_CASE_COUNT 0
#endif
static unsigned long totalFilteredTriangles,totalUnfilteredTriangles;
static double vertexMax,vertexDistanceMax,rayMax,evalMicros,pairFilteredMicros,pairFullMicros;
#define CHECK(x) do { ++checks; if(!(x)) { if(failures<35) fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x); ++failures; } } while(0)
static unsigned int u32(const unsigned char *p) { return (unsigned int)p[0]|(unsigned int)p[1]<<8|(unsigned int)p[2]<<16|(unsigned int)p[3]<<24; }
static void put32(unsigned char *p,unsigned int x) { unsigned int i; for(i=0;i<4;++i)p[i]=(unsigned char)(x>>(i*8)); }
static float f32(const unsigned char *p) { unsigned int bits=u32(p);float x;memcpy(&x,&bits,4);return x; }
static int i16(const unsigned char *p) { unsigned int x=p[0]|(unsigned int)p[1]<<8;return x<=32767?(int)x:-1-(int)(65535-x); }
static unsigned char *readFile(const char *directory,const char *name,size_t *size) {
    char path[4096];FILE *file;long amount;unsigned char *bytes;
    if(snprintf(path,sizeof(path),"%s/%s",directory,name)<0) exit(2);
    file=fopen(path,"rb");if(!file) { fprintf(stderr,"cannot read %s\n",path);exit(2); }
    CHECK(!fseek(file,0,SEEK_END));amount=ftell(file);CHECK(amount>0);CHECK(!fseek(file,0,SEEK_SET));
    bytes=(unsigned char *)malloc((size_t)amount);if(!bytes)exit(2);
    CHECK(fread(bytes,1,(size_t)amount,file)==(size_t)amount);fclose(file);*size=(size_t)amount;return bytes;
}
static void inputFor(unsigned int n,tcePoseInput_t *input) {
    const SkinCase *f=&skinCases[n];memset(input,0,sizeof(*input));
    input->legs.currentModel=&models[f->model[0]];input->legs.oldModel=&models[f->model[1]];
    input->torso.currentModel=&models[f->model[2]];input->torso.oldModel=&models[f->model[3]];
    input->legs.currentFrame=f->frame[0];input->legs.oldFrame=f->frame[1];
    input->torso.currentFrame=f->frame[2];input->torso.oldFrame=f->frame[3];
    input->legs.backlerp=f->back[0];input->torso.backlerp=f->back[1];
    memcpy(input->origin,f->origin,sizeof(input->origin));memcpy(input->axis,f->axis,sizeof(input->axis));
    memcpy(input->torsoAxis,f->torso,sizeof(input->torsoAxis));input->mode=TCE_POSE_MATH_OBSERVED_WINDOWS;
}
static void noBroadphase(tcePoseSkin_t *skin) {
    unsigned int s,i;
    for(i=0;i<3;++i) { skin->bounds.mins[i]=-FLT_MAX;skin->bounds.maxs[i]=FLT_MAX; }
    for(s=0;s<TCE_POSE_SURFACE_MAX_SURFACES;++s) for(i=0;i<3;++i) {
        skin->bodyBounds[s].mins[i]=skin->headBounds[s].mins[i]=-FLT_MAX;
        skin->bodyBounds[s].maxs[i]=skin->headBounds[s].maxs[i]=FLT_MAX;
    }
}
static void captured(void) {
    unsigned int n,i,j,r;
    for(n=0;n<SKIN_CASE_COUNT;++n) {
        tcePoseInput_t input;inputFor(n,&input);
        CHECK(TCE_PoseSkinEvaluate(&mathState,&body,&head,&input,&skinCases[n].head,&pose)==TCE_POSE_OK);
        CHECK(pose.evaluatedBones==53);CHECK(pose.vertexCount==SKIN_BODY_VERTICES+SKIN_HEAD_VERTICES);
        for(i=0;i<pose.vertexCount;++i) {
            double distance=0;
            for(j=0;j<3;++j) {
                double error=fabs((double)pose.vertices[i][j]-f32(expectBytes+12u*((size_t)n*pose.vertexCount+i)+4u*j));
                if(error>vertexMax)vertexMax=error;
                distance+=error*error;
                CHECK(error<.0004);
                CHECK(pose.vertices[i][j]>=pose.bounds.mins[j] && pose.vertices[i][j]<=pose.bounds.maxs[j]);
            }
            distance=sqrt(distance);if(distance>vertexDistanceMax)vertexDistanceMax=distance;
        }
        wide=pose;noBroadphase(&wide);
        for(r=0;r<SKIN_RAY_COUNT;++r) if(skinRays[r].pose==n) {
            const SkinRay *f=&skinRays[r];tcePoseSurfaceHit_t hit,full;
            CHECK(TCE_PoseSkinRaycast(&pose,f->start,f->end,f->limit,&hit)==TCE_POSE_OK);
            CHECK(TCE_PoseSkinRaycast(&wide,f->start,f->end,f->limit,&full)==TCE_POSE_OK);
            if(hit.hitMask!=f->mask || (int)hit.region!=f->region)
                fprintf(stderr,"ray%u case%u %s mask%u/%u region%d/%d\n",r,n,f->name,hit.hitMask,f->mask,hit.region,f->region);
            CHECK(hit.hitMask==f->mask);CHECK((int)hit.region==f->region);
            { double error=fabs(hit.fraction-f->fraction);if(error>rayMax)rayMax=error;CHECK(error<.00002); }
            for(i=0;i<3;++i) CHECK(fabs(hit.regionFraction[i]-f->perRegion[i])<.00002);
            CHECK(hit.hitMask==full.hitMask && hit.region==full.region && hit.fraction==full.fraction);
            for(i=0;i<3;++i)CHECK(hit.regionFraction[i]==full.regionFraction[i]);
            CHECK(full.trianglesTested==1990);CHECK(hit.trianglesTested<=full.trianglesTested);
            totalFilteredTriangles+=hit.trianglesTested;totalUnfilteredTriangles+=full.trianglesTested;
            if(!strcmp(f->name,"leg-gap")) { CHECK(!hit.hitMask);++gaps; }
            if(!strcmp(f->name,"leg-positive")) { CHECK(hit.region==TCE_POSE_REGION_LEGS);++legControls; }
            if(!strcmp(f->name,"standing-cover") || !strcmp(f->name,"prone-cover")) { CHECK(!hit.hitMask);++coverControls; }
            if(!strcmp(f->name,"prone-head")) CHECK(hit.region==TCE_POSE_REGION_HEAD);
            if(!strcmp(f->name,"bound-head-surface")) { CHECK(hit.region==TCE_POSE_REGION_HEAD);++boundHeads; }
            if(!strcmp(f->name,"bound-head-cover")) { CHECK(!hit.hitMask);++boundCovers; }
            if(!strcmp(f->name,"far-miss")) CHECK(hit.trianglesTested==0 && hit.surfacesTested==0);
            if(hit.hitMask) {
                double length=0;
                for(i=0;i<3;++i)length+=(double)hit.normal[i]*hit.normal[i];
                CHECK(fabs(length-1)<.000002);
            }
        }
    }
    CHECK(gaps==4 && legControls==8 && coverControls==2);
    CHECK(boundHeads==SKIN_BOUND_CASE_COUNT && boundCovers==SKIN_BOUND_CASE_COUNT);
}
static void faceFrames(void) {
    tcePoseInput_t input;tcePoseHeadInput_t face;unsigned int f,s,i,j;
    inputFor(0,&input);memset(&face,0,sizeof(face));face.axis[0][0]=face.axis[1][1]=face.axis[2][2]=1;
    for(f=0;f<head.frameCount;++f) {
        face.currentFrame=face.oldFrame=f;
        CHECK(TCE_PoseSkinEvaluate(&mathState,&body,&head,&input,&face,&pose)==TCE_POSE_OK);
        for(s=0;s<head.surfaceCount;++s) {
            const tcePoseSurface_t *surface=&head.surfaces[s];
            for(i=0;i<surface->vertexCount;++i) for(j=0;j<3;++j) {
                const unsigned char *point=headBytes+surface->vertexOffset+8u*((size_t)f*surface->vertexCount+i)+2u*j;
                CHECK(pose.vertices[body.vertexCount+surface->firstVertex+i][j]==i16(point)*(1.f/64.f));
            }
        }
    }
    face.currentFrame=0;face.oldFrame=head.frameCount-1;face.backlerp=.375f;
    CHECK(TCE_PoseSkinEvaluate(&mathState,&body,&head,&input,&face,&pose)==TCE_POSE_OK);
    for(s=0;s<head.surfaceCount;++s) {
        const tcePoseSurface_t *surface=&head.surfaces[s];
        for(i=0;i<surface->vertexCount;++i) for(j=0;j<3;++j) {
            size_t at=surface->vertexOffset+8u*i+2u*j;
            float expected=(float)((.625*i16(headBytes+at)+.375*i16(headBytes+at+8u*(head.frameCount-1)*(size_t)surface->vertexCount))/64);
            CHECK(pose.vertices[body.vertexCount+surface->firstVertex+i][j]==expected);
        }
    }
}
static void malformed(void) {
    unsigned int i;size_t at=u32(bodyBytes+84),vertex=at+u32(bodyBytes+at+148),headAt=u32(headBytes+100);
    unsigned char *copy=(unsigned char *)malloc(headSize>bodySize?headSize:bodySize);
    CHECK(copy!=NULL);if(!copy)exit(2);
    for(i=0;i<256;++i) {
        CHECK(TCE_PoseLoadSkin(&badBody,bodyBytes,i,&models[0])!=TCE_POSE_OK);
        CHECK(!memcmp(&badBody,&zeroBody,sizeof(badBody)));
        CHECK(TCE_PoseLoadHead(&badHead,headBytes,i)!=TCE_POSE_OK);
        CHECK(!memcmp(&badHead,&zeroHead,sizeof(badHead)));
    }
#define BAD_BODY(offset,value) do { memcpy(copy,bodyBytes,bodySize);put32(copy+(offset),(value));CHECK(TCE_PoseLoadSkin(&badBody,copy,bodySize,&models[0])!=TCE_POSE_OK);CHECK(!memcmp(&badBody,&zeroBody,sizeof(badBody))); } while(0)
    BAD_BODY(80,33);BAD_BODY(96,(unsigned int)bodySize-1);BAD_BODY(at+144,0xffffffffu);
    BAD_BODY(at+148,172);BAD_BODY(at+156,u32(bodyBytes+at+148));BAD_BODY(at+164,129);
    BAD_BODY(vertex+20,0);BAD_BODY(vertex+20,129);BAD_BODY(vertex+24,127);
    BAD_BODY(vertex+28,0x7fc00000u);BAD_BODY(vertex+32,0x7f800000u);
    BAD_BODY(at+u32(bodyBytes+at+156),u32(bodyBytes+at+144));
    memcpy(copy,bodyBytes,bodySize);copy[at+4]='x';
    CHECK(TCE_PoseLoadSkin(&badBody,copy,bodySize,&models[0])==TCE_POSE_NOT_FOUND);
    CHECK(!memcmp(&badBody,&zeroBody,sizeof(badBody)));
#undef BAD_BODY
#define BAD_HEAD(offset,value) do { memcpy(copy,headBytes,headSize);put32(copy+(offset),(value));CHECK(TCE_PoseLoadHead(&badHead,copy,headSize)!=TCE_POSE_OK);CHECK(!memcmp(&badHead,&zeroHead,sizeof(badHead))); } while(0)
    BAD_HEAD(4,14);BAD_HEAD(76,8193);BAD_HEAD(84,33);BAD_HEAD(104,(unsigned int)headSize-1);
    BAD_HEAD(92,0xffffffffu);BAD_HEAD(u32(headBytes+92),0x7fc00000u);
    BAD_HEAD(headAt+72,0);BAD_HEAD(headAt+80,4097);BAD_HEAD(headAt+84,8193);
    BAD_HEAD(headAt+100,u32(headBytes+headAt+96));BAD_HEAD(headAt+88,104);
    BAD_HEAD(headAt+u32(headBytes+headAt+88),u32(headBytes+headAt+80));
    memcpy(copy,headBytes,headSize);memset(copy+headAt+4,'a',64);
    CHECK(TCE_PoseLoadHead(&badHead,copy,headSize)==TCE_POSE_BAD_FORMAT);
    CHECK(!memcmp(&badHead,&zeroHead,sizeof(badHead)));
#undef BAD_HEAD
    CHECK(TCE_PoseLoadHead(&badHead,headBytes,headSize-1)!=TCE_POSE_OK);
    CHECK(TCE_PoseLoadSkin(&badBody,bodyBytes,bodySize-1,&models[0])!=TCE_POSE_OK);
    CHECK(TCE_PoseLoadSkin(&badBody,NULL,bodySize,&models[0])==TCE_POSE_BAD_ARGUMENT);
    CHECK(TCE_PoseLoadHead(&badHead,NULL,headSize)==TCE_POSE_BAD_ARGUMENT);
    CHECK(TCE_PoseLoadSkin(&badBody,bodyBytes,(size_t)-1,&models[0])==TCE_POSE_LIMIT);
    CHECK(TCE_PoseLoadHead(&badHead,headBytes,(size_t)-1)==TCE_POSE_LIMIT);
    incompatible=models[0];strcpy(incompatible.bones[45].name,"unknown left leg");
    CHECK(TCE_PoseLoadSkin(&badBody,bodyBytes,bodySize,&incompatible)==TCE_POSE_INCOMPATIBLE);
    free(copy);
}
static void permuted(void) {
    unsigned int m,i,j,n,count=models[0].boneCount;
    unsigned char *copies[SKIN_MODEL_COUNT],*copy=(unsigned char *)malloc(bodySize);
    size_t at;
    CHECK(copy!=NULL);if(!copy)exit(2);
    for(m=0;m<SKIN_MODEL_COUNT;++m) {
        size_t size=models[m].byteCount,bone=u32(modelBytes[m]+84);
        copies[m]=(unsigned char *)malloc(size);CHECK(copies[m]!=NULL);if(!copies[m])exit(2);
        memcpy(copies[m],modelBytes[m],size);put32(copies[m]+88,count-1-models[m].torsoParent);
        for(i=0;i<count;++i) {
            unsigned int dest=count-1-i;int parent=models[m].bones[i].parent;
            memcpy(copies[m]+bone+80u*dest,modelBytes[m]+bone+80u*i,80);
            put32(copies[m]+bone+80u*dest+64,parent<0?0xffffffffu:count-1-(unsigned int)parent);
            for(n=0;n<models[m].frameCount;++n) {
                size_t base=models[m].frameOffset+models[m].frameStride*n+52;
                memcpy(copies[m]+base+12u*dest,modelBytes[m]+base+12u*i,12);
            }
        }
        CHECK(TCE_PoseLoadSkeleton(&permutedModels[m],copies[m],size)==TCE_POSE_OK);
    }
    memcpy(copy,bodyBytes,bodySize);at=u32(copy+84);
    for(i=0;i<u32(copy+80);++i) {
        size_t pos=at+u32(copy+at+148),refs=at+u32(copy+at+168);
        for(j=0;j<u32(copy+at+164);++j)put32(copy+refs+4u*j,count-1-u32(copy+refs+4u*j));
        for(j=0;j<u32(copy+at+144);++j) {
            unsigned int w,weights=u32(copy+pos+20);
            for(w=0;w<weights;++w)put32(copy+pos+24+20u*w,count-1-u32(copy+pos+24+20u*w));
            pos+=24+20u*weights;
        }
        at+=u32(copy+at+172);
    }
    at=u32(copy+92);
    for(i=0;i<u32(copy+88);++i) {
        size_t refs=at+u32(copy+at+120);
        put32(copy+at+100,count-1-u32(copy+at+100));
        for(j=0;j<u32(copy+at+116);++j)put32(copy+refs+4u*j,count-1-u32(copy+refs+4u*j));
        at+=u32(copy+at+124);
    }
    CHECK(TCE_PoseLoadSkin(&badBody,copy,bodySize,&permutedModels[0])==TCE_POSE_OK);
    CHECK(!memcmp(badBody.region,body.region,body.triangleCount));
    for(n=0;n<SKIN_CASE_COUNT;++n) {
        const SkinCase *f=&skinCases[n];tcePoseInput_t input;inputFor(n,&input);
        CHECK(TCE_PoseSkinEvaluate(&mathState,&body,&head,&input,&f->head,&pose)==TCE_POSE_OK);
        input.legs.currentModel=&permutedModels[f->model[0]];input.legs.oldModel=&permutedModels[f->model[1]];
        input.torso.currentModel=&permutedModels[f->model[2]];input.torso.oldModel=&permutedModels[f->model[3]];
        CHECK(TCE_PoseSkinEvaluate(&mathState,&badBody,&head,&input,&f->head,&wide)==TCE_POSE_OK);
        CHECK(!memcmp(pose.vertices,wide.vertices,pose.vertexCount*sizeof(pose.vertices[0])));
    }
    for(m=0;m<SKIN_MODEL_COUNT;++m)free(copies[m]);
    free(copy);
}
/* A real variable-weight record stream at and beyond the aggregate cache cap;
 * unlike a malformed count alone, all referenced records/arrays fit the file. */
static unsigned char *limitBody(unsigned int verts,unsigned int weights,size_t *size) {
    size_t record=24+20u*weights,vo=176,to=vo+verts*record,co=to+12,ro=co+verts*4u,end=ro+weights*4u;
    unsigned char *p;unsigned int i,j,one=0x3f800000u;float weight=1.f/weights;unsigned int bits;
    *size=100+end;p=(unsigned char *)calloc(*size,1);if(!p)exit(2);
    memcpy(p,"MDMW",4);put32(p+4,3);put32(p+80,1);put32(p+84,100);put32(p+92,(unsigned int)*size);put32(p+96,(unsigned int)*size);
    memcpy(p+104,"u_limit",8);put32(p+240,0u-100u);put32(p+244,verts);put32(p+248,(unsigned int)vo);
    put32(p+252,1);put32(p+256,(unsigned int)to);put32(p+260,(unsigned int)co);put32(p+264,weights);
    put32(p+268,(unsigned int)ro);put32(p+272,(unsigned int)end);memcpy(&bits,&weight,4);
    for(i=0;i<verts;++i) {
        unsigned char *v=p+100+vo+i*record;put32(v+8,one);put32(v+20,weights);
        for(j=0;j<weights;++j) { put32(v+24+20u*j,j);put32(v+28+20u*j,bits); }
        put32(p+100+co+4u*i,i);
    }
    put32(p+100+to,0);put32(p+100+to+4,1);put32(p+100+to+8,2);
    for(i=0;i<weights;++i)put32(p+100+ro+4u*i,i);
    return p;
}
static void limits(void) {
    size_t size;unsigned char *bytes;
    bytes=limitBody(4096,8,&size);
    CHECK(TCE_PoseLoadSkin(&badBody,bytes,size,&models[0])==TCE_POSE_OK);
    CHECK(badBody.vertexCount==4096 && badBody.weightCount==32768);free(bytes);
    bytes=limitBody(4096,9,&size);
    CHECK(TCE_PoseLoadSkin(&badBody,bytes,size,&models[0])==TCE_POSE_LIMIT);
    CHECK(!memcmp(&badBody,&zeroBody,sizeof(badBody)));free(bytes);
    bytes=limitBody(4097,8,&size);
    CHECK(TCE_PoseLoadSkin(&badBody,bytes,size,&models[0])==TCE_POSE_LIMIT);free(bytes);
}
static void invalidInputs(void) {
    tcePoseInput_t input;tcePoseHeadInput_t face;tcePoseSurfaceHit_t hit,zeroHit;
    float start[3]={0,0,0},end[3]={1,0,0};unsigned int nanBits=0x7fc00000u;float nanValue;
    memcpy(&nanValue,&nanBits,4);memset(&zeroHit,0,sizeof(zeroHit));
#define BAD_INPUT(change,status) do { inputFor(0,&input);face=skinCases[0].head;change;memset(&pose,0xa5,sizeof(pose));CHECK(TCE_PoseSkinEvaluate(&mathState,&body,&head,&input,&face,&pose)==(status));CHECK(!memcmp(&pose,&zeroPose,sizeof(pose))); } while(0)
    BAD_INPUT(face.currentFrame=head.frameCount,TCE_POSE_BAD_RANGE);
    BAD_INPUT(face.backlerp=-1,TCE_POSE_BAD_RANGE);BAD_INPUT(face.backlerp=nanValue,TCE_POSE_NONFINITE);
    BAD_INPUT(face.origin[0]=nanValue,TCE_POSE_NONFINITE);BAD_INPUT(memset(face.axis,0,sizeof(face.axis)),TCE_POSE_BAD_ARGUMENT);
    BAD_INPUT(input.legs.currentFrame=0xffffffffu,TCE_POSE_BAD_RANGE);
    BAD_INPUT(input.torso.backlerp=2,TCE_POSE_BAD_RANGE);
    BAD_INPUT(input.mode=(tcePoseMathMode_t)99,TCE_POSE_BAD_ARGUMENT);
    incompatible=models[0];strcpy(incompatible.bones[0].name,"different");
    BAD_INPUT(input.torso.currentModel=&incompatible,TCE_POSE_INCOMPATIBLE);
#undef BAD_INPUT
    inputFor(0,&input);CHECK(TCE_PoseSkinEvaluate(&mathState,&body,&head,&input,&skinCases[0].head,&pose)==TCE_POSE_OK);
#define BAD_RAY(limit,status) do { memset(&hit,0xa5,sizeof(hit));CHECK(TCE_PoseSkinRaycast(&pose,start,end,(limit),&hit)==(status));CHECK(!memcmp(&hit,&zeroHit,sizeof(hit))); } while(0)
    BAD_RAY(-1,TCE_POSE_BAD_RANGE);BAD_RAY(2,TCE_POSE_BAD_RANGE);BAD_RAY(nanValue,TCE_POSE_NONFINITE);
    start[0]=nanValue;BAD_RAY(1,TCE_POSE_NONFINITE);start[0]=0;end[0]=0;BAD_RAY(1,TCE_POSE_BAD_RANGE);
#undef BAD_RAY
}
static int loader(void *user,const char *name,const unsigned char **bytes,size_t *size) {
    ++loadCalls;
    if(!strcmp(name,"missing"))return 0;
    if(!strncmp(name,"head",4)) { *bytes=headBytes;*size=headSize; }
    else if(user) { *bytes=(const unsigned char *)user;*size=u32(*bytes+96); }
    else { *bytes=bodyBytes;*size=bodySize; }
    return 1;
}
static void caches(void) {
    const tcePoseSkinMesh_t *mesh,*same;const tcePoseHeadMesh_t *h,*sameHead;
    unsigned int calls,i;char name[64];
    TCE_PoseSurfaceRegistryInit(&registry);
    CHECK(TCE_PoseSurfaceRegistryBody(&registry,"body",&models[0],loader,NULL,&mesh)==TCE_POSE_OK);
    CHECK(TCE_PoseSurfaceRegistryHead(&registry,"head",loader,NULL,&h)==TCE_POSE_OK);
    calls=loadCalls;
    CHECK(TCE_PoseSurfaceRegistryBody(&registry,"body",&models[1],NULL,NULL,&same)==TCE_POSE_OK && same==mesh);
    CHECK(TCE_PoseSurfaceRegistryHead(&registry,"head",NULL,NULL,&sameHead)==TCE_POSE_OK && sameHead==h);
    CHECK(loadCalls==calls);CHECK(registry.borrowedBytes==bodySize+headSize);
    CHECK(TCE_PoseSurfaceRegistryBody(&registry,"body",&incompatible,NULL,NULL,&same)==TCE_POSE_INCOMPATIBLE && same==NULL);
    CHECK(TCE_PoseSurfaceRegistryHead(&registry,"missing",loader,NULL,&sameHead)==TCE_POSE_NOT_FOUND && sameHead==NULL);
    CHECK(registry.headCount==1 && registry.meshCount==1);
    TCE_PoseSurfaceRegistryInit(&registry);
    for(i=0;i<TCE_POSE_SURFACE_MAX_MESHES;++i) { snprintf(name,sizeof(name),"body%u",i);CHECK(TCE_PoseSurfaceRegistryBody(&registry,name,&models[0],loader,NULL,&mesh)==TCE_POSE_OK); }
    CHECK(TCE_PoseSurfaceRegistryBody(&registry,"overflow",&models[0],loader,NULL,&mesh)==TCE_POSE_LIMIT && mesh==NULL);
    TCE_PoseSurfaceRegistryInit(&registry);
    for(i=0;i<TCE_POSE_SURFACE_MAX_HEADS;++i) { snprintf(name,sizeof(name),"head%u",i);CHECK(TCE_PoseSurfaceRegistryHead(&registry,name,loader,NULL,&h)==TCE_POSE_OK); }
    CHECK(TCE_PoseSurfaceRegistryHead(&registry,"head-overflow",loader,NULL,&h)==TCE_POSE_LIMIT && h==NULL);
    TCE_PoseSurfaceRegistryInit(&registry);registry.borrowedBytes=TCE_POSE_MAX_CACHE_BYTES;
    CHECK(TCE_PoseSurfaceRegistryBody(&registry,"full",&models[0],loader,NULL,&mesh)==TCE_POSE_LIMIT && mesh==NULL);
    CHECK(TCE_PoseSurfaceRegistryHead(&registry,"head-full",loader,NULL,&h)==TCE_POSE_LIMIT && h==NULL);
    CHECK(!registry.meshCount && !registry.headCount);
    {
        size_t size;unsigned char *large=limitBody(4096,8,&size);tcePoseStatus_t status=TCE_POSE_OK;
        TCE_PoseSurfaceRegistryInit(&registry);
        CHECK(TCE_PoseSurfaceRegistryHead(&registry,"head",loader,NULL,&h)==TCE_POSE_OK);
        for(i=0;i<TCE_POSE_SURFACE_MAX_MESHES && status==TCE_POSE_OK;++i) {
            snprintf(name,sizeof(name),"large%u",i);
            status=TCE_PoseSurfaceRegistryBody(&registry,name,&models[0],loader,large,&mesh);
        }
        CHECK(status==TCE_POSE_LIMIT && registry.meshCount<TCE_POSE_SURFACE_MAX_MESHES);
        CHECK(registry.borrowedBytes==headSize+registry.meshCount*size && registry.borrowedBytes<=TCE_POSE_MAX_CACHE_BYTES);
        calls=loadCalls;
        CHECK(TCE_PoseSurfaceRegistryBody(&registry,"large0",&models[0],NULL,NULL,&mesh)==TCE_POSE_OK);
        CHECK(loadCalls==calls);
        TCE_PoseSurfaceRegistryInit(&registry);free(large);
    }
}
static void edgeCases(void) {
    float a[3]={.25f,.25f,-1},b[3]={.25f,.25f,1};tcePoseSurfaceHit_t hit;
    unsigned int i;
    badBody=body;badHead=head;badBody.surfaceCount=badHead.surfaceCount=1;
    badBody.vertexCount=badHead.vertexCount=3;badBody.triangleCount=badHead.triangleCount=1;
    badBody.surfaces[0].firstVertex=badBody.surfaces[0].firstTriangle=0;
    badHead.surfaces[0].firstVertex=badHead.surfaces[0].firstTriangle=0;
    badBody.surfaces[0].vertexCount=badHead.surfaces[0].vertexCount=3;
    badBody.surfaces[0].triangleCount=badHead.surfaces[0].triangleCount=1;
    for(i=0;i<3;++i)badBody.triangle[0][i]=badHead.triangle[0][i]=(uint16_t)i;
    badBody.region[0]=TCE_POSE_REGION_BODY;
    pose.body=&badBody;pose.head=&badHead;pose.vertexCount=6;
    memset(pose.vertices,0,sizeof(pose.vertices));pose.vertices[1][0]=pose.vertices[2][1]=1;
    for(i=0;i<3;++i) { pose.vertices[3+i][0]=pose.vertices[i][0]+10;pose.vertices[3+i][1]=pose.vertices[i][1]+10;pose.vertices[3+i][2]=10; }
    noBroadphase(&pose);
    CHECK(TCE_PoseSkinRaycast(&pose,a,b,1,&hit)==TCE_POSE_OK && hit.region==TCE_POSE_REGION_BODY && hit.fraction==.5);
    CHECK(hit.normal[2]==-1 && hit.point[2]==0);
    CHECK(TCE_PoseSkinRaycast(&pose,a,b,.49,&hit)==TCE_POSE_OK && !hit.hitMask);
    CHECK(TCE_PoseSkinRaycast(&pose,a,b,.5,&hit)==TCE_POSE_OK && hit.fraction==.5);
    CHECK(TCE_PoseSkinRaycast(&pose,b,a,1,&hit)==TCE_POSE_OK && hit.fraction==.5 && hit.normal[2]==1);
    a[0]=b[0]=a[1]=b[1]=.5f;
    CHECK(TCE_PoseSkinRaycast(&pose,a,b,1,&hit)==TCE_POSE_OK && hit.fraction==.5);
    a[0]=b[0]=a[1]=b[1]=.25f;a[2]=0;
    CHECK(TCE_PoseSkinRaycast(&pose,a,b,1,&hit)==TCE_POSE_OK && hit.fraction==0);
    a[2]=b[2]=0;b[0]=.5f;
    CHECK(TCE_PoseSkinRaycast(&pose,a,b,1,&hit)==TCE_POSE_OK && !hit.hitMask);
    b[0]=a[0];a[2]=-1;b[2]=1;
    pose.vertices[2][1]=0;
    CHECK(TCE_PoseSkinRaycast(&pose,a,b,1,&hit)==TCE_POSE_OK && !hit.hitMask);
    pose.vertices[2][1]=1;
    for(i=0;i<3;++i)memcpy(pose.vertices[3+i],pose.vertices[i],sizeof(pose.vertices[0]));
    CHECK(TCE_PoseSkinRaycast(&pose,a,b,1,&hit)==TCE_POSE_OK && hit.region==TCE_POSE_REGION_HEAD && hit.hitMask==3);
    for(i=0;i<3;++i)pose.vertices[3+i][2]=.5f;
    CHECK(TCE_PoseSkinRaycast(&pose,a,b,1,&hit)==TCE_POSE_OK && hit.region==TCE_POSE_REGION_BODY && hit.hitMask==3);
    CHECK(TCE_PoseSkinRaycast(&pose,b,a,1,&hit)==TCE_POSE_OK && hit.region==TCE_POSE_REGION_HEAD && hit.fraction==.25);
    CHECK(TCE_PoseSkinRaycast(&pose,a,b,.5,&hit)==TCE_POSE_OK && hit.region==TCE_POSE_REGION_BODY && hit.hitMask==2);
}
static void benchmark(void) {
    tcePoseInput_t input;tcePoseSurfaceHit_t hit;clock_t begin;unsigned int i;
    const SkinRay *ray=&skinRays[0];float start[3],end[3];
    inputFor(0,&input);begin=clock();
    for(i=0;i<200;++i)CHECK(TCE_PoseSkinEvaluate(&mathState,&body,&head,&input,&skinCases[0].head,&pose)==TCE_POSE_OK);
    evalMicros=1e6*(clock()-begin)/(CLOCKS_PER_SEC*200.);
    second=pose;
    for(i=0;i<second.vertexCount;++i)second.vertices[i][0]+=64;
    second.bounds.mins[0]+=64;second.bounds.maxs[0]+=64;
    for(i=0;i<TCE_POSE_SURFACE_MAX_SURFACES;++i) {
        second.bodyBounds[i].mins[0]+=64;second.bodyBounds[i].maxs[0]+=64;
        second.headBounds[i].mins[0]+=64;second.headBounds[i].maxs[0]+=64;
    }
    for(i=0;i<3;++i) { start[i]=ray->start[i];end[i]=ray->end[i]; }
    /* Two real 1,990-triangle player surfaces, with conservative broadphase
     * disabled only in a test copy for the full3,980-triangle workload. */
    wide=pose;wideSecond=second;noBroadphase(&wide);noBroadphase(&wideSecond);
    begin=clock();
    for(i=0;i<1000;++i) {
        CHECK(TCE_PoseSkinRaycast(&wide,start,end,1,&hit)==TCE_POSE_OK);
        CHECK(TCE_PoseSkinRaycast(&wideSecond,start,end,1,&hit)==TCE_POSE_OK);
    }
    pairFullMicros=1e6*(clock()-begin)/(CLOCKS_PER_SEC*1000.);
    begin=clock();
    for(i=0;i<1000;++i) {
        CHECK(TCE_PoseSkinRaycast(&second,start,end,1,&hit)==TCE_POSE_OK);
        CHECK(TCE_PoseSkinRaycast(&pose,start,end,1,&hit)==TCE_POSE_OK);
    }
    pairFilteredMicros=1e6*(clock()-begin)/(CLOCKS_PER_SEC*1000.);
}
int main(int argc,char **argv) {
    size_t size;unsigned int i;char name[64];
    if(argc!=2) { fprintf(stderr,"usage: pose_skin_test FIXTURE_DIRECTORY\n");return 2; }
    CHECK(TCE_PoseMathInit(&mathState)==TCE_POSE_OK);
    for(i=0;i<SKIN_MODEL_COUNT;++i) {
        snprintf(name,sizeof(name),"model-%u.mdx",i);modelBytes[i]=readFile(argv[1],name,&size);
        CHECK(TCE_PoseLoadSkeleton(&models[i],modelBytes[i],size)==TCE_POSE_OK);
    }
    bodyBytes=readFile(argv[1],"body.mdm",&bodySize);headBytes=readFile(argv[1],"head.md3",&headSize);
    expectBytes=readFile(argv[1],"skin_expected.bin",&size);
    CHECK(size==(size_t)SKIN_CASE_COUNT*(SKIN_BODY_VERTICES+SKIN_HEAD_VERTICES)*12);
    CHECK(TCE_PoseLoadSkin(&body,bodyBytes,bodySize,&models[0])==TCE_POSE_OK);
    CHECK(TCE_PoseLoadHead(&head,headBytes,headSize)==TCE_POSE_OK);
    CHECK(body.vertexCount==875 && body.triangleCount==1268 && head.vertexCount==490 && head.triangleCount==722);
    for(i=0;i<body.triangleCount;++i)CHECK(body.region[i]==expectedBodyRegions[i]);
    if(!failures) { captured();faceFrames();permuted();malformed();limits();invalidInputs();caches();edgeCases();benchmark(); }
    printf("{\"checks\":%u,\"failures\":%u,\"captured_poses\":176,\"frozen_climb_poses\":4,\"bound_readonly_poses\":%u,\"total_poses\":%u,\"head_frames\":401,\"rays\":%u,\"gap_negatives\":%u,\"leg_positive_controls\":%u,\"cover_negatives\":%u,\"bound_head_positive_controls\":%u,\"bound_head_cover_negatives\":%u,\"vertex_coordinate_max\":%.10g,\"vertex_distance_max\":%.10g,\"ray_fraction_max\":%.10g,\"filtered_triangles\":%lu,\"unfiltered_triangles\":%lu,\"pose_bytes\":%zu,\"registry_bytes\":%zu,\"evaluate_microseconds\":%.3f,\"two_players_3980_triangles_microseconds\":%.3f,\"two_filtered_rays_microseconds\":%.3f}\n",
        checks,failures,(unsigned int)SKIN_BOUND_CASE_COUNT,(unsigned int)SKIN_CASE_COUNT,SKIN_RAY_COUNT,gaps,legControls,coverControls,boundHeads,boundCovers,vertexMax,vertexDistanceMax,rayMax,totalFilteredTriangles,totalUnfilteredTriangles,sizeof(pose),sizeof(registry),evalMicros,pairFullMicros,pairFilteredMicros);
    for(i=0;i<SKIN_MODEL_COUNT;++i)free(modelBytes[i]);
    free(bodyBytes);free(headBytes);free(expectBytes);
    return failures?1:0;
}
