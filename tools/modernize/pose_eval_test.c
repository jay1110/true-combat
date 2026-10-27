/* Focused actual-source tests. Fixture bytes are generated from local assets;
 * captured renderer anchors and independent oracle thighs remain separate. */
#include "tce_pose_math.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pose_eval_fixtures.inc"

static tcePoseSkeleton_t models[TCE_POSE_FIXTURE_MODEL_COUNT];
static tcePoseMesh_t meshes[TCE_POSE_FIXTURE_MESH_COUNT];
static tcePoseMath_t mathState;
static tcePoseRegistry_t registry;
static unsigned int checks, failures;
static double originMax, axisMax, thighMax;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); } } while(0)
static unsigned int u32(const unsigned char *p) { return p[0]|(unsigned int)p[1]<<8|(unsigned int)p[2]<<16|(unsigned int)p[3]<<24; }
static void put32(unsigned char *p,unsigned int x) { unsigned int i; for(i=0;i<4;++i) p[i]=(unsigned char)(x>>(i*8)); }
static void inputFor(unsigned int index,tcePoseInput_t *input) {
    const tcePoseFixtureCase_t *f=&tcePoseFixtureCases[index];
    memset(input,0,sizeof(*input));
    input->legs.currentModel=&models[f->model[0]]; input->legs.oldModel=&models[f->model[1]];
    input->torso.currentModel=&models[f->model[2]]; input->torso.oldModel=&models[f->model[3]];
    input->legs.currentFrame=f->frame[0]; input->legs.oldFrame=f->frame[1];
    input->torso.currentFrame=f->frame[2]; input->torso.oldFrame=f->frame[3];
    input->legs.backlerp=f->backlerp[0]; input->torso.backlerp=f->backlerp[1];
    memcpy(input->origin,f->origin,sizeof(input->origin));
    memcpy(input->axis,f->axis,sizeof(input->axis)); memcpy(input->torsoAxis,f->torsoAxis,sizeof(input->torsoAxis));
    input->mode=TCE_POSE_MATH_OBSERVED_WINDOWS;
}
static void captured(void) {
    unsigned int n,i,j,k;
    for(n=0;n<TCE_POSE_FIXTURE_CASE_COUNT;++n) {
        const tcePoseFixtureCase_t *f=&tcePoseFixtureCases[n];
        tcePoseInput_t input; tcePoseAttachment_t attachment; tcePoseResult_t result;
        tcePoseStatus_t status;
        inputFor(n,&input);
        CHECK(TCE_PosePrepareAttachment(&attachment,&meshes[f->mesh],input.legs.currentModel)==TCE_POSE_OK);
        CHECK(attachment.closureCount==13);
        status=TCE_PoseEvaluate(&mathState,&attachment,&input,&result);
        CHECK(status==TCE_POSE_OK);
        if(status) { fprintf(stderr,"case%u %s\n",n,TCE_PoseStatusString(status)); continue; }
        CHECK(result.evaluatedBones==13);
        for(i=0;i<TCE_POSE_ANCHORS;++i) {
            double error=0;
            for(j=0;j<3;++j) {
                double d=result.model[i].origin[j]-f->expectedOrigin[i][j];
                error+=d*d;
                for(k=0;k<3;++k) {
                    double a=fabs(result.model[i].axis[j][k]-f->expectedAxis[i][j][k]);
                    if(a>axisMax) axisMax=a;
                    CHECK(a<0.00002);
                }
                { double world=input.origin[j];
                  for(k=0;k<3;++k) world+=(double)input.axis[k][j]*result.model[i].origin[k];
                  CHECK(fabs(result.world[i].origin[j]-world)<0.00025); }
            }
            error=sqrt(error);
            if(f->capturedMask&(1u<<i)) { if(error>originMax) originMax=error; }
            else { if(error>thighMax) thighMax=error; }
            if(error>=0.0002) fprintf(stderr,"case%u id%d tag%u origin error %.9g\n",n,f->originalId,i,error);
            CHECK(error<0.0002);
        }
    }
}
static void permutedHierarchy(void) {
    tcePoseSkeleton_t alternate[TCE_POSE_FIXTURE_MODEL_COUNT];
    unsigned char *bytes[TCE_POSE_FIXTURE_MODEL_COUNT];
    tcePoseMesh_t mesh=meshes[0];
    unsigned int m,n,i,j,count=models[0].boneCount;
    /* Reverse every global index, metadata entry and channel. A valid parent
     * now appears after its child, proving the closure is derived by topology. */
    for(m=0;m<TCE_POSE_FIXTURE_MODEL_COUNT;++m) {
        const tcePoseFixtureAsset_t *a=&tcePoseFixtureModels[m];
        unsigned int boneOffset=u32(a->bytes+84);
        bytes[m]=(unsigned char*)malloc(a->size); CHECK(bytes[m]!=NULL); if(!bytes[m]) exit(2);
        memcpy(bytes[m],a->bytes,a->size);
        put32(bytes[m]+88,count-1-models[m].torsoParent);
        for(i=0;i<count;++i) {
            unsigned int target=count-1-i;
            int parent=models[m].bones[i].parent;
            memcpy(bytes[m]+boneOffset+80u*target,a->bytes+boneOffset+80u*i,80);
            put32(bytes[m]+boneOffset+80u*target+64,parent<0?0xffffffffu:count-1-(unsigned int)parent);
            for(n=0;n<models[m].frameCount;++n) {
                size_t base=models[m].frameOffset+n*models[m].frameStride+52;
                memcpy(bytes[m]+base+12u*target,a->bytes+base+12u*i,12);
            }
        }
        CHECK(TCE_PoseLoadSkeleton(&alternate[m],bytes[m],a->size)==TCE_POSE_OK);
    }
    for(i=0;i<mesh.tagCount;++i) mesh.tags[i].bone=count-1-mesh.tags[i].bone;
    for(n=0;n<TCE_POSE_FIXTURE_CASE_COUNT;++n) {
        const tcePoseFixtureCase_t *f=&tcePoseFixtureCases[n];
        tcePoseAttachment_t original,permuted;
        tcePoseInput_t input;
        tcePoseResult_t expected,actual;
        inputFor(n,&input);
        CHECK(TCE_PosePrepareAttachment(&original,&meshes[0],input.legs.currentModel)==TCE_POSE_OK);
        CHECK(TCE_PoseEvaluate(&mathState,&original,&input,&expected)==TCE_POSE_OK);
        input.legs.currentModel=&alternate[f->model[0]]; input.legs.oldModel=&alternate[f->model[1]];
        input.torso.currentModel=&alternate[f->model[2]]; input.torso.oldModel=&alternate[f->model[3]];
        CHECK(TCE_PosePrepareAttachment(&permuted,&mesh,input.legs.currentModel)==TCE_POSE_OK);
        CHECK(permuted.closureCount==13);
        CHECK(TCE_PoseEvaluate(&mathState,&permuted,&input,&actual)==TCE_POSE_OK);
        for(i=0;i<TCE_POSE_ANCHORS;++i) for(j=0;j<3;++j) {
            CHECK(actual.model[i].origin[j]==expected.model[i].origin[j]);
            CHECK(!memcmp(actual.model[i].axis[j],expected.model[i].axis[j],sizeof(actual.model[i].axis[j])));
        }
    }
    for(m=0;m<TCE_POSE_FIXTURE_MODEL_COUNT;++m) free(bytes[m]);
}
static void tableAndPoseBoundaries(void) {
    tcePoseAttachment_t attachment;
    tcePoseInput_t input;
    tcePoseResult_t result;
    unsigned int n,i,j,k;
    float previous;
    inputFor(0,&input);
    CHECK(TCE_PosePrepareAttachment(&attachment,&meshes[0],&models[0])==TCE_POSE_OK);
    /* Both interpolation endpoints and equal-frame collapse must be finite,
     * deterministic, and retain the compact work bound. */
    for(n=0;n<2;++n) {
        input.legs.backlerp=(float)n; input.torso.backlerp=(float)n;
        CHECK(TCE_PoseEvaluate(&mathState,&attachment,&input,&result)==TCE_POSE_OK);
        CHECK(result.evaluatedBones==13);
    }
    previous=mathState.sine[0]; CHECK(previous==0);
    CHECK(fabs(mathState.sine[1024]-1)<0.00001);
    CHECK(fabs(mathState.sine[3072]+1)<0.00001);
    /* Every short/table boundary, including signed wrap, is exercised by a
     * real parsed model rather than a reimplementation of the decision. */
    {
        const tcePoseFixtureAsset_t *a=&tcePoseFixtureModels[0];
        unsigned char *bytes=(unsigned char*)malloc(a->size);
        tcePoseSkeleton_t altered;
        CHECK(bytes!=NULL); if(!bytes) return;
        memcpy(bytes,a->bytes,a->size);
        unsigned int head=attachment.tags[TCE_POSE_HEAD].bone;
        CHECK(models[0].bones[head].torsoWeight==1);
        for(n=0;n<8192;++n) {
            unsigned int value=(n/2*16+(n%2?65535u:0))&65535u;
            unsigned int table=value>>4;
            double sp=mathState.sine[table],cp=mathState.sine[(table+1024)&4095];
            double cy=mathState.sine[1024];
            float matrix[3][3]={{0}};
            matrix[0][0]=(float)(cp*cy); matrix[0][2]=(float)-sp;
            matrix[1][1]=(float)(cy*cy);
            matrix[2][0]=(float)(cy*sp*cy); matrix[2][2]=(float)(cy*cp);
            for(i=0;i<2;++i) {
                size_t where=models[0].frameOffset+i*models[0].frameStride+52+12u*head;
                memset(bytes+where,0,6);
                bytes[where]=(unsigned char)value; bytes[where+1]=(unsigned char)(value>>8);
            }
            CHECK(TCE_PoseLoadSkeleton(&altered,bytes,a->size)==TCE_POSE_OK);
            input.legs.currentModel=input.legs.oldModel=input.torso.currentModel=input.torso.oldModel=&altered;
            input.legs.currentFrame=input.torso.currentFrame=0;
            input.legs.oldFrame=input.torso.oldFrame=1;
            input.legs.backlerp=input.torso.backlerp=.25f;
            memset(input.torsoAxis,0,sizeof(input.torsoAxis));
            for(i=0;i<3;++i) input.torsoAxis[i][i]=1;
            CHECK(TCE_PoseEvaluate(&mathState,&attachment,&input,&result)==TCE_POSE_OK);
            for(i=0;i<3;++i) for(j=0;j<3;++j) {
                double expected=0;
                for(k=0;k<3;++k) expected+=(double)matrix[j][k]*attachment.tags[TCE_POSE_HEAD].axis[i][k];
                CHECK(fabs(result.model[TCE_POSE_HEAD].axis[i][j]-expected)<0.000003);
            }
        }
        free(bytes);
    }
}
static void malformed(void) {
    const tcePoseFixtureAsset_t *asset=&tcePoseFixtureModels[0];
    unsigned char *copy=(unsigned char*)malloc(asset->size);
    tcePoseSkeleton_t model;
    tcePoseMesh_t mesh;
    size_t i;
    unsigned int bones=u32(asset->bytes+84), frames=u32(asset->bytes+80);
    CHECK(copy!=NULL); if(!copy) return;
    /* Every truncated prefix must fail without permitting evaluate. */
    for(i=0;i<asset->size;++i) {
        CHECK(TCE_PoseLoadSkeleton(&model,asset->bytes,i)!=TCE_POSE_OK);
        CHECK(!model.ready);
    }
#define BAD32(offset,value,expected) do { memcpy(copy,asset->bytes,asset->size); put32(copy+(offset),(value)); CHECK(TCE_PoseLoadSkeleton(&model,copy,asset->size)==(expected)); CHECK(!model.ready); } while(0)
    BAD32(72,0xffffffffu,TCE_POSE_LIMIT);
    BAD32(76,129,TCE_POSE_LIMIT);
    BAD32(80,0xffffffffu,TCE_POSE_BAD_RANGE);
    BAD32(84,frames,TCE_POSE_BAD_RANGE);
    BAD32(88,128,TCE_POSE_BAD_TOPOLOGY);
    BAD32(bones+64,0,TCE_POSE_BAD_TOPOLOGY);
    BAD32(bones+80+64,2,TCE_POSE_BAD_TOPOLOGY);
    BAD32(bones+68,0x7fc00000u,TCE_POSE_NONFINITE);
    BAD32(frames+40,0x7f800000u,TCE_POSE_NONFINITE);
#undef BAD32
    free(copy);
    asset=&tcePoseFixtureMeshes[0];
    copy=(unsigned char*)malloc(asset->size); CHECK(copy!=NULL); if(!copy) return;
    for(i=0;i<asset->size;++i) {
        CHECK(TCE_PoseLoadMesh(&mesh,asset->bytes,i)!=TCE_POSE_OK);
        CHECK(!mesh.ready);
    }
#define BADM(offset,value) do { memcpy(copy,asset->bytes,asset->size); put32(copy+(offset),(value)); CHECK(TCE_PoseLoadMesh(&mesh,copy,asset->size)!=TCE_POSE_OK); CHECK(!mesh.ready); } while(0)
    BADM(88,129); BADM(92,0xffffffffu); BADM(u32(asset->bytes+92)+100,128);
    BADM(u32(asset->bytes+92)+64,0x7fc00000u); BADM(u32(asset->bytes+92)+124,0xffffffffu);
    BADM(u32(asset->bytes+84)+144,6001); BADM(u32(asset->bytes+84)+148,0xffffffffu);
#undef BADM
    free(copy);
}
static unsigned int loaderCalls;
static int loader(void *user,const char *name,const unsigned char **bytes,size_t *size) {
    const tcePoseFixtureAsset_t *asset=(const tcePoseFixtureAsset_t*)user;
    (void)name; ++loaderCalls; *bytes=asset->bytes; *size=asset->size; return 1;
}
static void cacheAndInputs(void) {
    const tcePoseSkeleton_t *s,*same;
    const tcePoseMesh_t *mesh;
    tcePoseAttachment_t attachment;
    tcePoseInput_t input;
    tcePoseResult_t result,zero;
    unsigned int i;
    char name[32];
    TCE_PoseRegistryInit(&registry); loaderCalls=0;
    CHECK(TCE_PoseRegistrySkeleton(&registry,"body",loader,(void*)&tcePoseFixtureModels[0],&s)==TCE_POSE_OK);
    CHECK(TCE_PoseRegistrySkeleton(&registry,"body",NULL,NULL,&same)==TCE_POSE_OK);
    CHECK(s==same && loaderCalls==1);
    for(i=1;i<TCE_POSE_MAX_SKELETONS;++i) {
        sprintf(name,"model%u",i);
        CHECK(TCE_PoseRegistrySkeleton(&registry,name,loader,(void*)&tcePoseFixtureModels[0],&s)==TCE_POSE_OK);
    }
    CHECK(TCE_PoseRegistrySkeleton(&registry,"overflow",loader,NULL,&s)==TCE_POSE_LIMIT);
    CHECK(s==NULL && loaderCalls==TCE_POSE_MAX_SKELETONS);
    for(i=0;i<TCE_POSE_MAX_MESHES;++i) {
        sprintf(name,"mesh%u",i);
        CHECK(TCE_PoseRegistryMesh(&registry,name,loader,(void*)&tcePoseFixtureMeshes[0],&mesh)==TCE_POSE_OK);
    }
    CHECK(TCE_PoseRegistryMesh(&registry,"overflow",loader,NULL,&mesh)==TCE_POSE_LIMIT);
    CHECK(mesh==NULL);
    TCE_PoseRegistryInit(&registry);
    registry.borrowedBytes=TCE_POSE_MAX_CACHE_BYTES;
    CHECK(TCE_PoseRegistrySkeleton(&registry,"full",loader,(void*)&tcePoseFixtureModels[0],&s)==TCE_POSE_LIMIT);
    CHECK(!registry.skeletonCount && s==NULL);
    inputFor(0,&input); CHECK(TCE_PosePrepareAttachment(&attachment,&meshes[0],&models[0])==TCE_POSE_OK);
    memset(&zero,0,sizeof(zero));
#define BADINPUT(change,expected) do { inputFor(0,&input); change; memset(&result,0xa5,sizeof(result)); CHECK(TCE_PoseEvaluate(&mathState,&attachment,&input,&result)==(expected)); CHECK(!memcmp(&result,&zero,sizeof(result))); } while(0)
    BADINPUT(input.legs.currentFrame=0xffffffffu,TCE_POSE_BAD_RANGE);
    BADINPUT(input.torso.backlerp=-.1f,TCE_POSE_BAD_RANGE);
    BADINPUT(input.torso.currentModel=NULL,TCE_POSE_INCOMPATIBLE);
    BADINPUT(memset(input.axis,0,sizeof(input.axis)),TCE_POSE_BAD_ARGUMENT);
    BADINPUT(input.mode=(tcePoseMathMode_t)99,TCE_POSE_BAD_ARGUMENT);
    BADINPUT(input.axis[0][0]=FLT_MAX,TCE_POSE_NONFINITE);
#undef BADINPUT
    CHECK(loaderCalls==TCE_POSE_MAX_SKELETONS+TCE_POSE_MAX_MESHES+1);
    {
        tcePoseMesh_t absent=meshes[0];
        absent.tagCount=0;
        CHECK(TCE_PosePrepareAttachment(&attachment,&absent,&models[0])==TCE_POSE_NOT_FOUND);
        CHECK(!attachment.ready);
        absent=meshes[0]; absent.maxReferencedBone=127;
        CHECK(TCE_PosePrepareAttachment(&attachment,&absent,&models[0])==TCE_POSE_INCOMPATIBLE);
        CHECK(!attachment.ready);
    }
    {
        const tcePoseFixtureAsset_t *asset=&tcePoseFixtureModels[0];
        unsigned char *copy=(unsigned char*)malloc(asset->size);
        tcePoseSkeleton_t deep;
        unsigned int bones=u32(asset->bytes+84);
        CHECK(copy!=NULL);
        if(copy) {
            memcpy(copy,asset->bytes,asset->size);
            for(i=0;i<models[0].boneCount;++i) put32(copy+bones+80u*i+64,i?i-1:0xffffffffu);
            CHECK(TCE_PoseLoadSkeleton(&deep,copy,asset->size)==TCE_POSE_OK);
            CHECK(TCE_PosePrepareAttachment(&attachment,&meshes[0],&deep)==TCE_POSE_LIMIT);
            CHECK(!attachment.ready);
            free(copy);
        }
    }
}
static void fullAssets(int argc,char **argv) {
    int i;
    for(i=1;i<argc;++i) {
        FILE *file=fopen(argv[i],"rb");
        unsigned char *bytes;
        long size;
        tcePoseStatus_t status;
        CHECK(file!=NULL); if(!file) continue;
        CHECK(!fseek(file,0,SEEK_END)); size=ftell(file);
        CHECK(size>=4 && size<=TCE_POSE_MAX_ASSET_BYTES);
        if(size<4 || size>TCE_POSE_MAX_ASSET_BYTES) { fclose(file); continue; }
        CHECK(!fseek(file,0,SEEK_SET));
        bytes=(unsigned char*)malloc((size_t)size); CHECK(bytes!=NULL);
        if(!bytes) { fclose(file); continue; }
        CHECK(fread(bytes,1,(size_t)size,file)==(size_t)size); fclose(file);
        if(!memcmp(bytes,"MDXW",4)) {
            tcePoseSkeleton_t model;
            status=TCE_PoseLoadSkeleton(&model,bytes,(size_t)size);
        } else {
            tcePoseMesh_t mesh;
            status=TCE_PoseLoadMesh(&mesh,bytes,(size_t)size);
        }
        fprintf(stderr,"full asset %s: %s\n",argv[i],TCE_PoseStatusString(status));
        CHECK(status==TCE_POSE_OK);
        free(bytes);
    }
}
int main(int argc,char **argv) {
    unsigned int i;
    CHECK(TCE_PoseMathInit(&mathState)==TCE_POSE_OK);
    for(i=0;i<TCE_POSE_FIXTURE_MODEL_COUNT;++i) {
        tcePoseStatus_t status=TCE_PoseLoadSkeleton(&models[i],tcePoseFixtureModels[i].bytes,tcePoseFixtureModels[i].size);
        fprintf(stderr,"model%u %s\n",i,TCE_PoseStatusString(status)); CHECK(status==TCE_POSE_OK);
    }
    for(i=0;i<TCE_POSE_FIXTURE_MESH_COUNT;++i) {
        tcePoseStatus_t status=TCE_PoseLoadMesh(&meshes[i],tcePoseFixtureMeshes[i].bytes,tcePoseFixtureMeshes[i].size);
        fprintf(stderr,"mesh%u %s\n",i,TCE_PoseStatusString(status)); CHECK(status==TCE_POSE_OK);
    }
    if(failures) return 1;
    captured(); permutedHierarchy(); tableAndPoseBoundaries(); malformed(); cacheAndInputs(); fullAssets(argc,argv);
    printf("{\"checks\":%u,\"failures\":%u,\"frames\":%u,\"captured_origin_max\":%.10g,\"captured_or_oracle_axis_max\":%.10g,\"oracle_thigh_origin_max\":%.10g}\n",checks,failures,TCE_POSE_FIXTURE_CASE_COUNT,originMax,axisMax,thighMax);
    return failures?1:0;
}
