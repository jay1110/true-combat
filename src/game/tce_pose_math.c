/* Independent mod attachment evaluator. Asset layout is read explicitly; no
 * engine loader, renderer code, native disk structs, or SDK ABI are imported.
 * The observed arithmetic is an explicit compatibility profile, not a claim
 * that another renderer backend has identical animation or float semantics.
 */
#include "tce_pose_math.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <string.h>

#define POSE_READY UINT32_C(0x54435031)
#define POSE_PI 3.14159265358979323846264338327950288

typedef char poseRequireBinary32[(CHAR_BIT == 8 && sizeof(float) == 4 &&
    FLT_RADIX == 2 && FLT_MANT_DIG == 24) ? 1 : -1];

static const char *const poseNames[TCE_POSE_ANCHORS] = {
    "tag_head", "tag_chest", "tag_torso", "tag_legleft", "tag_legright",
    "tag_footleft", "tag_footright"
};

/* These reads are alignment independent, little endian, and alias safe. */
static uint32_t PoseU32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
           (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int PoseI32(const unsigned char *p) {
    uint32_t x = PoseU32(p);
    return x <= INT32_MAX ? (int)x : -1 - (int)(UINT32_MAX - x);
}
static int PoseI16(const unsigned char *p) {
    unsigned int x = (unsigned int)p[0] | (unsigned int)p[1] << 8;
    return x <= 32767u ? (int)x : -1 - (int)(65535u - x);
}
static float PoseFloat(const unsigned char *p) {
    uint32_t bits = PoseU32(p);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}
static int PoseFinite(double x) { return x >= -FLT_MAX && x <= FLT_MAX; }
static int PoseRange(size_t at, size_t count, size_t size) {
    return at <= size && count <= size - at;
}
static int PoseArray(size_t at, size_t count, size_t stride, size_t size) {
    return at <= size && (stride == 0 || count <= (size - at) / stride);
}
static int PoseDisjoint(size_t a, size_t na, size_t b, size_t nb) {
    return na == 0 || nb == 0 || (a <= b ? na <= b-a : nb <= a-b);
}
static int PoseName(char out[TCE_POSE_NAME_BYTES], const unsigned char *p) {
    size_t n;
    for (n = 0; n < TCE_POSE_NAME_BYTES; ++n) {
        if (!p[n]) {
            if (!n) return 0;
            memcpy(out, p, n);
            memset(out+n, 0, TCE_POSE_NAME_BYTES-n);
            return 1;
        }
        if (p[n] < 32 || p[n] > 126) return 0;
    }
    return 0;
}
static int PoseCString(const char *name) {
    unsigned int n;
    if (!name) return 0;
    for (n = 0; n < TCE_POSE_NAME_BYTES; ++n) {
        if (!name[n]) return n != 0;
        if ((unsigned char)name[n] < 32 || (unsigned char)name[n] > 126) return 0;
    }
    return 0;
}
static int PoseValidAxis(const float a[3][3]);

const char *TCE_PoseStatusString(tcePoseStatus_t status) {
    static const char *const names[] = { "ok", "bad argument", "bad format",
        "bad range", "nonfinite", "bad topology", "limit", "incompatible", "not found" };
    return (unsigned int)status < sizeof(names)/sizeof(names[0]) ? names[status] : "unknown";
}
const char *TCE_PoseAnchorName(unsigned int anchor) {
    return anchor < TCE_POSE_ANCHORS ? poseNames[anchor] : NULL;
}

tcePoseStatus_t TCE_PoseLoadSkeleton(tcePoseSkeleton_t *out, const void *data, size_t size) {
    const unsigned char *p = (const unsigned char *)data;
    size_t frames, bones, stride;
    unsigned int i, j, roots = 0;
    if (!out) return TCE_POSE_BAD_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!data) return TCE_POSE_BAD_ARGUMENT;
    if (size > TCE_POSE_MAX_ASSET_BYTES) return TCE_POSE_LIMIT;
    if (size < 96) return TCE_POSE_BAD_RANGE;
    if (memcmp(p, "MDXW", 4) || PoseU32(p+4) != 2) return TCE_POSE_BAD_FORMAT;
    out->frameCount = PoseU32(p+72);
    out->boneCount = PoseU32(p+76);
    out->torsoParent = PoseU32(p+88);
    if (!out->frameCount || out->frameCount > TCE_POSE_MAX_FRAMES ||
        !out->boneCount || out->boneCount > TCE_POSE_MAX_BONES) return TCE_POSE_LIMIT;
    frames = PoseU32(p+80); bones = PoseU32(p+84);
    stride = 52 + 12 * (size_t)out->boneCount;
    if (PoseU32(p+92) != size || frames < 96 || bones < 96 ||
        !PoseArray(frames, out->frameCount, stride, size) ||
        !PoseArray(bones, out->boneCount, 80, size) ||
        !PoseDisjoint(frames, out->frameCount*stride, bones, out->boneCount*80u))
        return TCE_POSE_BAD_RANGE;
    if (out->torsoParent >= out->boneCount) return TCE_POSE_BAD_TOPOLOGY;
    for (i = 0; i < out->boneCount; ++i) {
        const unsigned char *b = p+bones+80u*i;
        tcePoseBone_t *bone = &out->bones[i];
        if (!PoseName(bone->name, b)) return TCE_POSE_BAD_FORMAT;
        bone->parent = PoseI32(b+64);
        bone->torsoWeight = PoseFloat(b+68);
        bone->parentDistance = PoseFloat(b+72);
        bone->flags = PoseU32(b+76);
        if (!PoseFinite(bone->torsoWeight) || !PoseFinite(bone->parentDistance))
            return TCE_POSE_NONFINITE;
        if (bone->torsoWeight < 0 || bone->torsoWeight > 1 || bone->parentDistance < 0)
            return TCE_POSE_BAD_RANGE;
        if (bone->parent < -1 || bone->parent >= (int)out->boneCount || bone->parent == (int)i)
            return TCE_POSE_BAD_TOPOLOGY;
        roots += bone->parent == -1;
        for (j = 0; j < i; ++j) if (!strcmp(bone->name, out->bones[j].name))
            return TCE_POSE_BAD_TOPOLOGY;
    }
    if (roots != 1) return TCE_POSE_BAD_TOPOLOGY;
    /* Permit index permutations; bounded parent walks establish acyclicity. */
    for (i = 0; i < out->boneCount; ++i) {
        int node = (int)i;
        for (j = 0; node >= 0 && j <= out->boneCount; ++j) node = out->bones[node].parent;
        if (node >= 0) return TCE_POSE_BAD_TOPOLOGY;
    }
    for (i = 0; i < out->frameCount; ++i) {
        const unsigned char *frame = p+frames+stride*i;
        for (j = 0; j < 13; ++j) if (!PoseFinite(PoseFloat(frame+4u*j)))
            return TCE_POSE_NONFINITE;
        /* Shipped MDX clips contain reversed bounds. Attachments consume only
         * the finite parent offset; do not reinterpret unused legacy bounds. */
        if (PoseFloat(frame+36) < 0) return TCE_POSE_BAD_RANGE;
    }
    out->bytes = p; out->byteCount = size; out->frameOffset = frames; out->frameStride = stride;
    out->ready = POSE_READY;
    return TCE_POSE_OK;
}

/* MDM surfaces are not retained, but validate their variable weight records so
 * a malformed file cannot become a seemingly valid attachment asset. */
static tcePoseStatus_t PoseSurface(const unsigned char *p, size_t size, size_t at,
        size_t *next, unsigned int *maxReferencedBone) {
    unsigned int verts, tris, refs, i, j;
    size_t end, vo, to, co, ro, pos;
    char name[TCE_POSE_NAME_BYTES];
    if (!PoseRange(at, 176, size)) return TCE_POSE_BAD_RANGE;
    if (PoseI32(p+at+140) != -(int)at || !PoseName(name, p+at+4) ||
        !memchr(p+at+68, 0, 64)) return TCE_POSE_BAD_FORMAT;
    verts = PoseU32(p+at+144); vo = PoseU32(p+at+148);
    tris = PoseU32(p+at+152); to = PoseU32(p+at+156);
    co = PoseU32(p+at+160); refs = PoseU32(p+at+164); ro = PoseU32(p+at+168);
    end = PoseU32(p+at+172);
    if (verts > 6000 || tris > 8192 || refs > TCE_POSE_MAX_BONES) return TCE_POSE_LIMIT;
    if (end < 176 || !PoseRange(at, end, size) || vo < 176 || to < 176 || co < 176 || ro < 176 ||
        !PoseRange(vo, 0, end) || !PoseArray(to, tris, 12, end) ||
        !PoseArray(co, verts, 4, end) || !PoseArray(ro, refs, 4, end)) return TCE_POSE_BAD_RANGE;
    for (i = 0; i < refs; ++i) {
        unsigned int ref = PoseU32(p+at+ro+4u*i);
        if (ref >= TCE_POSE_MAX_BONES) return TCE_POSE_BAD_RANGE;
        if (ref > *maxReferencedBone) *maxReferencedBone = ref;
        for (j = 0; j < i; ++j) if (ref == PoseU32(p+at+ro+4u*j)) return TCE_POSE_BAD_TOPOLOGY;
    }
    for (i = 0; i < tris*3; ++i) if (PoseU32(p+at+to+4u*i) >= verts) return TCE_POSE_BAD_RANGE;
    for (i = 0; i < verts; ++i) if (PoseU32(p+at+co+4u*i) >= verts) return TCE_POSE_BAD_RANGE;
    pos = vo;
    for (i = 0; i < verts; ++i) {
        unsigned int weights;
        double sum = 0;
        if (!PoseRange(pos, 24, end)) return TCE_POSE_BAD_RANGE;
        for (j = 0; j < 5; ++j) if (!PoseFinite(PoseFloat(p+at+pos+4u*j))) return TCE_POSE_NONFINITE;
        weights = PoseU32(p+at+pos+20);
        if (!weights || weights > TCE_POSE_MAX_BONES) return TCE_POSE_LIMIT;
        if (!PoseArray(pos+24, weights, 20, end)) return TCE_POSE_BAD_RANGE;
        for (j = 0; j < weights; ++j) {
            const unsigned char *w = p+at+pos+24+20u*j;
            unsigned int k, bone = PoseU32(w);
            float weight = PoseFloat(w+4);
            if (!PoseFinite(weight)) return TCE_POSE_NONFINITE;
            if (weight < 0 || weight > 1) return TCE_POSE_BAD_RANGE;
            for (k = 0; k < 3; ++k) if (!PoseFinite(PoseFloat(w+8+4u*k))) return TCE_POSE_NONFINITE;
            for (k = 0; k < refs && PoseU32(p+at+ro+4u*k) != bone; ++k) { }
            if (k == refs) return TCE_POSE_BAD_TOPOLOGY;
            sum += weight;
        }
        if (fabs(sum-1) >= .002) return TCE_POSE_BAD_RANGE;
        pos += 24+20u*weights;
    }
    if (!PoseDisjoint(vo, pos-vo, to, tris*12u) || !PoseDisjoint(vo, pos-vo, co, verts*4u) ||
        !PoseDisjoint(vo, pos-vo, ro, refs*4u) || !PoseDisjoint(to, tris*12u, co, verts*4u) ||
        !PoseDisjoint(to, tris*12u, ro, refs*4u) || !PoseDisjoint(co, verts*4u, ro, refs*4u))
        return TCE_POSE_BAD_RANGE;
    *next = at+end;
    return TCE_POSE_OK;
}

tcePoseStatus_t TCE_PoseLoadMesh(tcePoseMesh_t *out, const void *data, size_t size) {
    const unsigned char *p = (const unsigned char *)data;
    unsigned int surfaces, i, j;
    size_t at, surfaceStart, surfaceEnd, tagStart;
    if (!out) return TCE_POSE_BAD_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!data) return TCE_POSE_BAD_ARGUMENT;
    if (size > TCE_POSE_MAX_ASSET_BYTES) return TCE_POSE_LIMIT;
    if (size < 100) return TCE_POSE_BAD_RANGE;
    if (memcmp(p, "MDMW", 4) || PoseU32(p+4) != 3) return TCE_POSE_BAD_FORMAT;
    if (!PoseFinite(PoseFloat(p+72)) || !PoseFinite(PoseFloat(p+76))) return TCE_POSE_NONFINITE;
    surfaces = PoseU32(p+80); surfaceStart = PoseU32(p+84);
    out->tagCount = PoseU32(p+88); tagStart = PoseU32(p+92);
    if (surfaces > 32 || out->tagCount > TCE_POSE_MAX_TAGS) return TCE_POSE_LIMIT;
    if (PoseU32(p+96) != size || surfaceStart < 100 || tagStart < 100 ||
        surfaceStart > size || tagStart > size) return TCE_POSE_BAD_RANGE;
    at = surfaceStart;
    for (i = 0; i < surfaces; ++i) {
        tcePoseStatus_t status = PoseSurface(p, size, at, &at, &out->maxReferencedBone);
        if (status) return status;
    }
    surfaceEnd = at;
    at = tagStart;
    for (i = 0; i < out->tagCount; ++i) {
        tcePoseTag_t *tag = &out->tags[i];
        unsigned int refs;
        size_t ro, end;
        if (!PoseRange(at, 128, size)) return TCE_POSE_BAD_RANGE;
        if (!PoseName(tag->name, p+at)) return TCE_POSE_BAD_FORMAT;
        tag->bone = PoseU32(p+at+100);
        refs = PoseU32(p+at+116); ro = PoseU32(p+at+120); end = PoseU32(p+at+124);
        if (tag->bone >= TCE_POSE_MAX_BONES || refs > TCE_POSE_MAX_BONES) return TCE_POSE_LIMIT;
        if (tag->bone > out->maxReferencedBone) out->maxReferencedBone = tag->bone;
        if (end < 128 || ro < 128 || !PoseRange(at, end, size) || !PoseArray(ro, refs, 4, end))
            return TCE_POSE_BAD_RANGE;
        for (j = 0; j < 9; ++j) {
            float value = PoseFloat(p+at+64+4u*j);
            if (!PoseFinite(value)) return TCE_POSE_NONFINITE;
            tag->axis[j/3][j%3] = value;
        }
        for (j = 0; j < 3; ++j) {
            tag->offset[j] = PoseFloat(p+at+104+4u*j);
            if (!PoseFinite(tag->offset[j])) return TCE_POSE_NONFINITE;
        }
        if (!PoseValidAxis(tag->axis)) return TCE_POSE_BAD_FORMAT;
        {
            int containsBone = 0;
            for (j = 0; j < refs; ++j) {
                unsigned int k, ref = PoseU32(p+at+ro+4u*j);
                if (ref >= TCE_POSE_MAX_BONES) return TCE_POSE_BAD_RANGE;
                if (ref > out->maxReferencedBone) out->maxReferencedBone = ref;
                containsBone |= ref == tag->bone;
                for (k = 0; k < j; ++k) if (ref == PoseU32(p+at+ro+4u*k)) return TCE_POSE_BAD_TOPOLOGY;
            }
            if (!containsBone) return TCE_POSE_BAD_TOPOLOGY;
        }
        for (j = 0; j < i; ++j) if (!strcmp(tag->name, out->tags[j].name)) return TCE_POSE_BAD_FORMAT;
        at += end;
    }
    if (!PoseDisjoint(surfaceStart, surfaceEnd-surfaceStart, tagStart, at-tagStart))
        return TCE_POSE_BAD_RANGE;
    out->byteCount = size; out->ready = POSE_READY;
    return TCE_POSE_OK;
}

static tcePoseStatus_t PoseAddParents(tcePoseAttachment_t *out, unsigned int bone) {
    unsigned int chain[TCE_POSE_MAX_BONES], count = 0;
    int node = (int)bone;
    while (node >= 0 && out->boneToClosure[node] < 0) {
        if (count == TCE_POSE_MAX_BONES) return TCE_POSE_BAD_TOPOLOGY;
        chain[count++] = (unsigned int)node;
        node = out->skeleton->bones[node].parent;
    }
    while (count) {
        unsigned int index = chain[--count];
        if (out->closureCount == TCE_POSE_MAX_CLOSURE) return TCE_POSE_LIMIT;
        out->boneToClosure[index] = (int)out->closureCount;
        out->closure[out->closureCount++] = index;
    }
    return TCE_POSE_OK;
}
tcePoseStatus_t TCE_PosePrepareAttachment(tcePoseAttachment_t *out,
        const tcePoseMesh_t *mesh, const tcePoseSkeleton_t *skeleton) {
    unsigned int i, j;
    tcePoseStatus_t status;
    if (!out) return TCE_POSE_BAD_ARGUMENT;
    memset(out, 0, sizeof(*out));
    if (!mesh || !skeleton || mesh->ready != POSE_READY || skeleton->ready != POSE_READY)
        return TCE_POSE_BAD_ARGUMENT;
    if (mesh->maxReferencedBone >= skeleton->boneCount) return TCE_POSE_INCOMPATIBLE;
    out->skeleton = skeleton;
    for (i = 0; i < TCE_POSE_MAX_BONES; ++i) out->boneToClosure[i] = -1;
    for (i = 0; i < TCE_POSE_ANCHORS; ++i) {
        for (j = 0; j < mesh->tagCount && strcmp(mesh->tags[j].name, poseNames[i]); ++j) { }
        if (j == mesh->tagCount) return TCE_POSE_NOT_FOUND;
        if (mesh->tags[j].bone >= skeleton->boneCount) return TCE_POSE_INCOMPATIBLE;
        out->tags[i] = mesh->tags[j];
        status = PoseAddParents(out, mesh->tags[j].bone);
        if (status) return status;
    }
    status = PoseAddParents(out, skeleton->torsoParent);
    if (status) return status;
    out->ready = POSE_READY;
    return TCE_POSE_OK;
}

/* Explicit round-to-float stores centralize this measured compatibility
 * arithmetic. No FPU control changes, assembly, or platform branches. */
static float PoseRound(double x) {
    /* Rejectable extreme inputs must not require an out-of-range C float
     * conversion. Nonfinite intermediates propagate to the result gate. */
    if (!PoseFinite(x)) {
        uint32_t bits = x != x ? UINT32_C(0x7fc00000) :
            x < 0 ? UINT32_C(0xff800000) : UINT32_C(0x7f800000);
        float invalid;
        memcpy(&invalid, &bits, sizeof(invalid));
        return invalid;
    }
    {
        volatile float stored = (float)x;
        return stored;
    }
}
tcePoseStatus_t TCE_PoseMathInit(tcePoseMath_t *math) {
    unsigned int i;
    if (!math || sizeof(float) != 4 || FLT_RADIX != 2 || FLT_MANT_DIG != 24)
        return TCE_POSE_BAD_ARGUMENT;
    memset(math, 0, sizeof(*math));
    for (i = 0; i < TCE_POSE_TABLE_SIZE; ++i) {
        float degrees = PoseRound(i*360.0/(TCE_POSE_TABLE_SIZE-1));
        float product = PoseRound((double)degrees * PoseRound(POSE_PI));
        math->sine[i] = PoseRound(sin(product/180.0));
    }
    math->ready = POSE_READY;
    return TCE_POSE_OK;
}
static int PoseShortDiff(int a, int b) { return (int)(((unsigned int)(a-b)+32768u)&65535u)-32768; }
static void PoseSinCos(const tcePoseMath_t *math, double value, int table, double *s, double *c) {
    if (table) {
        unsigned int index = ((unsigned int)(int)value & 65535u) >> 4;
        *s = math->sine[index]; *c = math->sine[(index+1024)&4095];
    } else {
        double radians = value * (2*POSE_PI/65536.0);
        *s = sin(radians); *c = cos(radians);
    }
}
static void PoseAxis(const tcePoseMath_t *math, const double angles[3], int table, float out[3][3]) {
    double sp,cp,sy,cy,sr,cr;
    PoseSinCos(math, angles[0], table, &sp, &cp);
    PoseSinCos(math, angles[1], table, &sy, &cy);
    PoseSinCos(math, angles[2], table, &sr, &cr);
    out[0][0]=PoseRound(cp*cy); out[0][1]=PoseRound(cp*sy); out[0][2]=PoseRound(-sp);
    out[1][0]=PoseRound(sr*sp*cy-cr*sy); out[1][1]=PoseRound(sr*sp*sy+cr*cy); out[1][2]=PoseRound(sr*cp);
    out[2][0]=PoseRound(cr*sp*cy+sr*sy); out[2][1]=PoseRound(cr*sp*sy-sr*cy); out[2][2]=PoseRound(cr*cp);
}
static void PoseDirection(const tcePoseMath_t *math, const int values[6], int table, float out[3]) {
    double sp,cp,sy,cy;
    PoseSinCos(math, values[4], table, &sp, &cp); PoseSinCos(math, values[5], table, &sy, &cy);
    out[0]=PoseRound(cp*cy); out[1]=PoseRound(cp*sy); out[2]=PoseRound(-sp);
}
static int PoseNormalize(float vector[3], int fast) {
    double length2 = 0;
    unsigned int i;
    float inverse;
    for (i=0;i<3;++i) length2 += fast ? PoseRound((double)vector[i]*vector[i]) : (double)vector[i]*vector[i];
    if (!(length2 > 1e-20) || !PoseFinite(length2)) return 0;
    if (fast) {
        float number=PoseRound(length2), initial,half;
        uint32_t bits;
        memcpy(&bits,&number,4); bits=UINT32_C(0x5f3759df)-(bits>>1); memcpy(&initial,&bits,4);
        half=PoseRound(.5*(double)number);
        inverse=PoseRound(initial*(double)PoseRound(1.5-PoseRound((double)PoseRound((double)half*initial)*initial)));
    } else inverse=PoseRound(1.0/sqrt(length2));
    for(i=0;i<3;++i) vector[i]=PoseRound((double)vector[i]*inverse);
    return 1;
}
static int PoseBlendDirection(const float old[3], const float current[3], double front, int fast, float out[3]) {
    unsigned int i;
    for(i=0;i<3;++i) out[i]=PoseRound((double)PoseRound(old[i]*(double)PoseRound(1-front))+
                                    PoseRound(current[i]*(double)PoseRound(front)));
    return PoseNormalize(out,fast);
}
static int PoseCompatible(const tcePoseSkeleton_t *a,const tcePoseSkeleton_t *b) {
    unsigned int i;
    if(!a || !b || a->ready!=POSE_READY || b->ready!=POSE_READY ||
       a->boneCount!=b->boneCount || a->torsoParent!=b->torsoParent) return 0;
    if(a==b) return 1;
    for(i=0;i<a->boneCount;++i) if(strcmp(a->bones[i].name,b->bones[i].name) ||
        a->bones[i].parent!=b->bones[i].parent || a->bones[i].torsoWeight!=b->bones[i].torsoWeight ||
        a->bones[i].flags!=b->bones[i].flags) return 0;
    /* Parent distances intentionally belong to current legs. Shipped split
     * clips differ in their lengths, so distance equality is not compatibility. */
    return 1;
}
static int PoseValidAxis(const float a[3][3]) {
    unsigned int i,j;
    double det;
    for(i=0;i<3;++i) for(j=0;j<3;++j) if(!PoseFinite(a[i][j])) return 0;
    det=(double)a[0][0]*(a[1][1]*(double)a[2][2]-a[1][2]*(double)a[2][1])-
        (double)a[0][1]*(a[1][0]*(double)a[2][2]-a[1][2]*(double)a[2][0])+
        (double)a[0][2]*(a[1][0]*(double)a[2][1]-a[1][1]*(double)a[2][0]);
    return PoseFinite(det) && fabs(det)>1e-12;
}

/* Attachment and full-skin paths share the exact measured bone arithmetic.
 * Selection storage/caps remain separate: anchors32, full skin128. */
static tcePoseStatus_t PoseEvaluateBones(const tcePoseMath_t *math,
        const tcePoseSkeleton_t *reference, unsigned int count,
        const unsigned int *closure, const int *boneToClosure,
        const tcePoseInput_t *input, float positions[][3], float matrices[][3][3]) {
    const tcePoseSkeleton_t *model[4],*skeleton;
    const unsigned char *frame[4];
    unsigned int indices[4],i,j,k,n;
    float root[3],pivot[3];
    double back[2];
    int observed,interpolate;
    if(!math || math->ready!=POSE_READY || !reference || reference->ready!=POSE_READY ||
       !input || !count || count>TCE_POSE_MAX_BONES)
        return TCE_POSE_BAD_ARGUMENT;
    if(input->mode!=TCE_POSE_MATH_ANALYTIC && input->mode!=TCE_POSE_MATH_OBSERVED_WINDOWS)
        return TCE_POSE_BAD_ARGUMENT;
    model[0]=input->legs.currentModel; model[1]=input->legs.oldModel;
    model[2]=input->torso.currentModel; model[3]=input->torso.oldModel;
    indices[0]=input->legs.currentFrame; indices[1]=input->legs.oldFrame;
    indices[2]=input->torso.currentFrame; indices[3]=input->torso.oldFrame;
    skeleton=model[0];
    if(!PoseCompatible(reference,skeleton)) return TCE_POSE_INCOMPATIBLE;
    for(i=0;i<4;++i) {
        if(!PoseCompatible(skeleton,model[i])) return TCE_POSE_INCOMPATIBLE;
        if(indices[i]>=model[i]->frameCount) return TCE_POSE_BAD_RANGE;
        frame[i]=model[i]->bytes+model[i]->frameOffset+indices[i]*model[i]->frameStride;
    }
    back[0]=input->legs.backlerp; back[1]=input->torso.backlerp;
    for(i=0;i<2;++i) {
        if(!PoseFinite(back[i])) return TCE_POSE_NONFINITE;
        if(back[i]<0 || back[i]>1) return TCE_POSE_BAD_RANGE;
    }
    for(i=0;i<3;++i) if(!PoseFinite(input->origin[i])) return TCE_POSE_NONFINITE;
    if(!PoseValidAxis(input->axis) || !PoseValidAxis(input->torsoAxis)) return TCE_POSE_BAD_ARGUMENT;
    observed=input->mode==TCE_POSE_MATH_OBSERVED_WINDOWS;
    if(model[0]==model[1] && indices[0]==indices[1]) back[0]=0;
    if(indices[2]==indices[3] && model[3]==model[observed?1:2]) back[1]=0;
    interpolate=back[0]!=0 || back[1]!=0;
    for(i=0;i<3;++i) root[i]=PoseRound((1-back[0])*PoseFloat(frame[0]+40+4u*i)+back[0]*PoseFloat(frame[1]+40+4u*i));
    for(n=0;n<count;++n) {
        unsigned int boneIndex=closure[n];
        const tcePoseBone_t *bone=&skeleton->bones[boneIndex];
        int channels[4][6];
        double angles[3];
        float dir[4][3],vector[3],lower[3],upper[3];
        for(i=0;i<4;++i) for(j=0;j<6;++j) channels[i][j]=PoseI16(frame[i]+52+12u*boneIndex+2u*j);
        for(j=0;j<3;++j) {
            int leg=(int)(channels[0][j]-back[0]*PoseShortDiff(channels[0][j],channels[1][j]));
            int torso=(int)(channels[2][j]-back[1]*PoseShortDiff(channels[2][j],channels[3][j]));
            if(observed && interpolate) angles[j]=(int)(leg+(double)bone->torsoWeight*PoseShortDiff(torso,leg));
            else if(observed) angles[j]=channels[0][j]+(double)bone->torsoWeight*PoseShortDiff(channels[2][j],channels[0][j]);
            else {
                double la=channels[0][j]-back[0]*PoseShortDiff(channels[0][j],channels[1][j]);
                double ta=channels[2][j]-back[1]*PoseShortDiff(channels[2][j],channels[3][j]);
                double delta=fmod(ta-la+32768.0,65536.0); if(delta<0) delta+=65536.0;
                angles[j]=la+(double)bone->torsoWeight*(delta-32768.0);
            }
        }
        PoseAxis(math,angles,observed&&interpolate,matrices[n]);
        for(i=0;i<4;++i) PoseDirection(math,channels[i],observed,dir[i]);
        if(bone->torsoWeight==0) {
            if(interpolate) { if(!PoseBlendDirection(dir[1],dir[0],1-back[0],observed,vector)) return TCE_POSE_BAD_RANGE; }
            else memcpy(vector,dir[0],sizeof(vector));
        } else if(bone->torsoWeight==1) {
            if(interpolate) { if(!PoseBlendDirection(dir[3],dir[2],1-back[1],observed,vector)) return TCE_POSE_BAD_RANGE; }
            else memcpy(vector,dir[2],sizeof(vector));
        } else {
            if(interpolate) {
                if(!PoseBlendDirection(dir[1],dir[0],1-back[0],observed,lower)) return TCE_POSE_BAD_RANGE;
                if(!PoseBlendDirection(dir[observed?2:3],dir[2],1-back[1],observed,upper)) return TCE_POSE_BAD_RANGE;
            } else { memcpy(lower,dir[0],sizeof(lower)); memcpy(upper,dir[2],sizeof(upper)); }
            if(!PoseBlendDirection(lower,upper,bone->torsoWeight,observed,vector)) return TCE_POSE_BAD_RANGE;
        }
        for(j=0;j<3;++j) positions[n][j]=bone->parent<0?root[j]:
            PoseRound((double)positions[boneToClosure[bone->parent]][j]+PoseRound((double)bone->parentDistance*vector[j]));
    }
    memcpy(pivot,positions[boneToClosure[skeleton->torsoParent]],sizeof(pivot));
    for(n=0;n<count;++n) {
        float weight=skeleton->bones[closure[n]].torsoWeight;
        float rotation[3][3],matrix[3][3],position[3];
        if(!weight) continue;
        for(i=0;i<3;++i) for(j=0;j<3;++j) rotation[i][j]=PoseRound((1-(double)weight)*(i==j)+(double)weight*input->torsoAxis[j][i]);
        for(i=0;i<3;++i) {
            double value=pivot[i];
            for(k=0;k<3;++k) value+=rotation[i][k]*((double)positions[n][k]-pivot[k]);
            position[i]=PoseRound(value);
            for(j=0;j<3;++j) {
                value=0; for(k=0;k<3;++k) value+=(double)rotation[i][k]*matrices[n][k][j];
                matrix[i][j]=PoseRound(value);
            }
        }
        memcpy(positions[n],position,sizeof(position)); memcpy(matrices[n],matrix,sizeof(matrix));
    }
    return TCE_POSE_OK;
}

tcePoseStatus_t TCE_PoseEvaluate(const tcePoseMath_t *math,const tcePoseAttachment_t *attachment,
        const tcePoseInput_t *input,tcePoseResult_t *out) {
    unsigned int i,j,k,n;
    float positions[TCE_POSE_MAX_CLOSURE][3], matrices[TCE_POSE_MAX_CLOSURE][3][3];
    tcePoseResult_t result;
    tcePoseStatus_t status;
    if(!out) return TCE_POSE_BAD_ARGUMENT;
    memset(out,0,sizeof(*out));
    if(!attachment || attachment->ready!=POSE_READY || attachment->closureCount>TCE_POSE_MAX_CLOSURE)
        return TCE_POSE_BAD_ARGUMENT;
    status=PoseEvaluateBones(math,attachment->skeleton,attachment->closureCount,
                            attachment->closure,attachment->boneToClosure,input,positions,matrices);
    if(status) return status;
    memset(&result,0,sizeof(result));
    result.evaluatedBones=attachment->closureCount;
    for(n=0;n<TCE_POSE_ANCHORS;++n) {
        const tcePoseTag_t *tag=&attachment->tags[n];
        int packed=attachment->boneToClosure[tag->bone];
        for(i=0;i<3;++i) {
            double value=positions[packed][i];
            for(k=0;k<3;++k) value+=(double)matrices[packed][i][k]*tag->offset[k];
            result.model[n].origin[i]=PoseRound(value);
            for(j=0;j<3;++j) {
                value=0; for(k=0;k<3;++k) value+=(double)matrices[packed][j][k]*tag->axis[i][k];
                result.model[n].axis[i][j]=PoseRound(value);
            }
        }
        for(i=0;i<3;++i) {
            double value=input->origin[i];
            for(k=0;k<3;++k) value+=(double)input->axis[k][i]*result.model[n].origin[k];
            if(!PoseFinite(value)) return TCE_POSE_NONFINITE;
            result.world[n].origin[i]=PoseRound(value);
            for(j=0;j<3;++j) {
                value=0; for(k=0;k<3;++k) value+=(double)result.model[n].axis[i][k]*input->axis[k][j];
                if(!PoseFinite(value) || !PoseFinite(result.model[n].axis[i][j])) return TCE_POSE_NONFINITE;
                result.world[n].axis[i][j]=PoseRound(value);
            }
            if(!PoseFinite(result.model[n].origin[i])) return TCE_POSE_NONFINITE;
        }
    }
    *out=result;
    return TCE_POSE_OK;
}

void TCE_PoseRegistryInit(tcePoseRegistry_t *registry) { if(registry) memset(registry,0,sizeof(*registry)); }
tcePoseStatus_t TCE_PoseRegistrySkeleton(tcePoseRegistry_t *registry,const char *name,
        tcePoseLoadBytes_t load,void *user,const tcePoseSkeleton_t **out) {
    unsigned int i;
    const unsigned char *bytes=NULL;
    size_t size=0;
    tcePoseStatus_t status;
    if(out) *out=NULL;
    if(!registry || !out || !PoseCString(name)) return TCE_POSE_BAD_ARGUMENT;
    for(i=0;i<registry->skeletonCount;++i) if(!strcmp(registry->skeletons[i].name,name)) {
        *out=&registry->skeletons[i].model; return TCE_POSE_OK;
    }
    if(!load) return TCE_POSE_BAD_ARGUMENT;
    if(registry->skeletonCount>=TCE_POSE_MAX_SKELETONS) return TCE_POSE_LIMIT;
    if(!load(user,name,&bytes,&size)) return TCE_POSE_NOT_FOUND;
    if(registry->borrowedBytes>TCE_POSE_MAX_CACHE_BYTES || size>TCE_POSE_MAX_CACHE_BYTES-registry->borrowedBytes)
        return TCE_POSE_LIMIT;
    status=TCE_PoseLoadSkeleton(&registry->skeletons[i].model,bytes,size);
    if(status) return status;
    strcpy(registry->skeletons[i].name,name);
    registry->borrowedBytes+=size; ++registry->skeletonCount; *out=&registry->skeletons[i].model;
    return TCE_POSE_OK;
}
tcePoseStatus_t TCE_PoseRegistryMesh(tcePoseRegistry_t *registry,const char *name,
        tcePoseLoadBytes_t load,void *user,const tcePoseMesh_t **out) {
    unsigned int i;
    const unsigned char *bytes=NULL;
    size_t size=0;
    tcePoseStatus_t status;
    if(out) *out=NULL;
    if(!registry || !out || !PoseCString(name)) return TCE_POSE_BAD_ARGUMENT;
    for(i=0;i<registry->meshCount;++i) if(!strcmp(registry->meshes[i].name,name)) {
        *out=&registry->meshes[i].model; return TCE_POSE_OK;
    }
    if(!load) return TCE_POSE_BAD_ARGUMENT;
    if(registry->meshCount>=TCE_POSE_MAX_MESHES) return TCE_POSE_LIMIT;
    if(!load(user,name,&bytes,&size)) return TCE_POSE_NOT_FOUND;
    if(registry->borrowedBytes>TCE_POSE_MAX_CACHE_BYTES || size>TCE_POSE_MAX_CACHE_BYTES-registry->borrowedBytes)
        return TCE_POSE_LIMIT;
    status=TCE_PoseLoadMesh(&registry->meshes[i].model,bytes,size);
    if(status) return status;
    strcpy(registry->meshes[i].name,name);
    registry->borrowedBytes+=size; ++registry->meshCount; *out=&registry->meshes[i].model;
    return TCE_POSE_OK;
}

/* Full-surface assets use separate bounded metadata, leaving attachment/cache
 * layouts unchanged. Borrowed bytes and prepared metadata must stay immutable. */
static tcePoseStatus_t PoseLoadSkin(tcePoseSkinMesh_t *out,const unsigned char *p,
        size_t size,const tcePoseSkeleton_t *skeleton) {
    tcePoseMesh_t checked;
    unsigned char leg[TCE_POSE_MAX_BONES];
    unsigned int i,j,k,legRoots=0;
    size_t at;
    tcePoseStatus_t status;
    if(!skeleton || skeleton->ready!=POSE_READY) return TCE_POSE_BAD_ARGUMENT;
    status=TCE_PoseLoadMesh(&checked,p,size);
    if(status) return status;
    if(checked.maxReferencedBone>=skeleton->boneCount) return TCE_POSE_INCOMPATIBLE;
    for(i=0;i<skeleton->boneCount;++i) {
        int node=(int)i;
        if(!strcmp(skeleton->bones[i].name,"Bip01 L Thigh") ||
           !strcmp(skeleton->bones[i].name,"Bip01 R Thigh")) ++legRoots;
        leg[i]=0;
        while(node>=0) {
            const char *name=skeleton->bones[node].name;
            if(!strcmp(name,"Bip01 L Thigh") || !strcmp(name,"Bip01 R Thigh")) leg[i]=1;
            node=skeleton->bones[node].parent;
        }
    }
    out->surfaceCount=PoseU32(p+80); at=PoseU32(p+84);
    for(i=0;i<out->surfaceCount;++i) {
        tcePoseSurface_t *surface=&out->surfaces[i];
        size_t pos=at+PoseU32(p+at+148),tri=at+PoseU32(p+at+156);
        unsigned int verts=PoseU32(p+at+144),tris=PoseU32(p+at+152);
        if(verts>TCE_POSE_SKIN_MAX_VERTICES-out->vertexCount ||
           tris>TCE_POSE_SURFACE_MAX_TRIANGLES-out->triangleCount) return TCE_POSE_LIMIT;
        if(!PoseName(surface->name,p+at+4)) return TCE_POSE_BAD_FORMAT;
        if(strncmp(surface->name,"u_",2) && strncmp(surface->name,"l_",2))
            return TCE_POSE_NOT_FOUND;
        if(surface->name[0]=='l' && legRoots!=2) return TCE_POSE_INCOMPATIBLE;
        surface->firstVertex=out->vertexCount; surface->vertexCount=verts;
        surface->firstTriangle=out->triangleCount; surface->triangleCount=tris;
        for(j=0;j<verts;++j) {
            unsigned int weights=PoseU32(p+pos+20);
            if(weights>TCE_POSE_SKIN_MAX_WEIGHTS-out->weightCount) return TCE_POSE_LIMIT;
            out->vertexOffset[out->vertexCount++]=pos;
            out->weightCount+=weights;
            pos+=24+20u*weights;
        }
        for(j=0;j<tris;++j) {
            unsigned int index=out->triangleCount++;
            double lower=0,upper=0;
            for(k=0;k<3;++k) {
                unsigned int vertex=surface->firstVertex+PoseU32(p+tri+12u*j+4u*k),w;
                size_t offset=out->vertexOffset[vertex];
                out->triangle[index][k]=(uint16_t)vertex;
                for(w=0;w<PoseU32(p+offset+20);++w) {
                    const unsigned char *weight=p+offset+24+20u*w;
                    if(leg[PoseU32(weight)]) lower+=PoseFloat(weight+4);
                    else upper+=PoseFloat(weight+4);
                }
            }
            out->region[index]=(unsigned char)(surface->name[0]=='l' && lower>upper ?
                TCE_POSE_REGION_LEGS:TCE_POSE_REGION_BODY);
        }
        at+=PoseU32(p+at+172);
    }
    if(!out->vertexCount || !out->triangleCount) return TCE_POSE_BAD_RANGE;
    out->bytes=p; out->byteCount=size; out->skeleton=skeleton; out->ready=POSE_READY;
    return TCE_POSE_OK;
}
tcePoseStatus_t TCE_PoseLoadSkin(tcePoseSkinMesh_t *out,const void *data,size_t size,
        const tcePoseSkeleton_t *skeleton) {
    tcePoseStatus_t status;
    if(!out) return TCE_POSE_BAD_ARGUMENT;
    memset(out,0,sizeof(*out));
    status=PoseLoadSkin(out,(const unsigned char *)data,size,skeleton);
    if(status) memset(out,0,sizeof(*out));
    return status;
}

static tcePoseStatus_t PoseLoadHead(tcePoseHeadMesh_t *out,const unsigned char *p,size_t size) {
    size_t at,frames,tags,surfaceStart;
    unsigned int frameCount,tagCount,surfaces,i,j,k;
    if(!p) return TCE_POSE_BAD_ARGUMENT;
    if(size>TCE_POSE_MAX_ASSET_BYTES) return TCE_POSE_LIMIT;
    if(size<108) return TCE_POSE_BAD_RANGE;
    if(memcmp(p,"IDP3",4) || PoseU32(p+4)!=15) return TCE_POSE_BAD_FORMAT;
    frameCount=PoseU32(p+76); tagCount=PoseU32(p+80); surfaces=PoseU32(p+84);
    /* MD3 header has flags at72, followed by frames/tags/surfaces/skins. */
    frames=PoseU32(p+92); tags=PoseU32(p+96); surfaceStart=PoseU32(p+100);
    if(!frameCount || frameCount>TCE_POSE_MAX_FRAMES || tagCount>TCE_POSE_MAX_TAGS ||
       surfaces>TCE_POSE_SURFACE_MAX_SURFACES) return TCE_POSE_LIMIT;
    if(PoseU32(p+104)!=size || frames<108 || tags<108 || surfaceStart<108 || surfaceStart>size ||
       !PoseArray(frames,frameCount,56,size) || !PoseArray(tags,frameCount*(size_t)tagCount,112,size) ||
       !PoseDisjoint(frames,frameCount*56u,tags,frameCount*(size_t)tagCount*112u))
        return TCE_POSE_BAD_RANGE;
    for(i=0;i<frameCount;++i) {
        for(j=0;j<10;++j) if(!PoseFinite(PoseFloat(p+frames+56u*i+4u*j))) return TCE_POSE_NONFINITE;
        if(PoseFloat(p+frames+56u*i+36)<0) return TCE_POSE_BAD_RANGE;
    }
    for(i=0;i<frameCount*tagCount;++i) {
        char name[TCE_POSE_NAME_BYTES];
        if(!PoseName(name,p+tags+112u*i)) return TCE_POSE_BAD_FORMAT;
        for(j=0;j<12;++j) if(!PoseFinite(PoseFloat(p+tags+112u*i+64+4u*j))) return TCE_POSE_NONFINITE;
    }
    out->frameCount=frameCount; out->surfaceCount=surfaces; at=surfaceStart;
    for(i=0;i<surfaces;++i) {
        tcePoseSurface_t *surface=&out->surfaces[i];
        size_t offsets[4],lengths[4],end;
        unsigned int verts,tris,shaders;
        if(!PoseRange(at,108,size)) return TCE_POSE_BAD_RANGE;
        if(memcmp(p+at,"IDP3",4) || !PoseName(surface->name,p+at+4)) return TCE_POSE_BAD_FORMAT;
        shaders=PoseU32(p+at+76); verts=PoseU32(p+at+80); tris=PoseU32(p+at+84);
        end=PoseU32(p+at+104);
        if(verts>TCE_POSE_HEAD_MAX_VERTICES-out->vertexCount ||
           tris>TCE_POSE_SURFACE_MAX_TRIANGLES-out->triangleCount || shaders>256) return TCE_POSE_LIMIT;
        if(PoseU32(p+at+72)!=frameCount || end<108 || !PoseRange(at,end,size)) return TCE_POSE_BAD_RANGE;
        for(j=0;j<4;++j) offsets[j]=PoseU32(p+at+88+4u*j);
        lengths[0]=tris*12u; lengths[1]=shaders*68u; lengths[2]=verts*8u;
        lengths[3]=frameCount*(size_t)verts*8u;
        for(j=0;j<4;++j) {
            if(offsets[j]<108 || !PoseRange(offsets[j],lengths[j],end)) return TCE_POSE_BAD_RANGE;
            for(k=0;k<j;++k) if(!PoseDisjoint(offsets[j],lengths[j],offsets[k],lengths[k])) return TCE_POSE_BAD_RANGE;
        }
        for(j=0;j<shaders;++j) if(!memchr(p+at+offsets[1]+68u*j,0,64)) return TCE_POSE_BAD_FORMAT;
        for(j=0;j<verts*2;++j) if(!PoseFinite(PoseFloat(p+at+offsets[2]+4u*j))) return TCE_POSE_NONFINITE;
        surface->firstVertex=out->vertexCount; surface->vertexCount=verts;
        surface->firstTriangle=out->triangleCount; surface->triangleCount=tris;
        surface->vertexOffset=at+offsets[3];
        for(j=0;j<tris;++j) {
            unsigned int index=out->triangleCount++;
            for(k=0;k<3;++k) {
                unsigned int vertex=PoseU32(p+at+offsets[0]+12u*j+4u*k);
                if(vertex>=verts) return TCE_POSE_BAD_RANGE;
                out->triangle[index][k]=(uint16_t)(out->vertexCount+vertex);
            }
        }
        out->vertexCount+=verts; at+=end;
    }
    if(!out->vertexCount || !out->triangleCount ||
       !PoseDisjoint(frames,frameCount*56u,surfaceStart,at-surfaceStart) ||
       !PoseDisjoint(tags,frameCount*(size_t)tagCount*112u,surfaceStart,at-surfaceStart))
        return TCE_POSE_BAD_RANGE;
    out->bytes=p; out->byteCount=size; out->ready=POSE_READY;
    return TCE_POSE_OK;
}
tcePoseStatus_t TCE_PoseLoadHead(tcePoseHeadMesh_t *out,const void *data,size_t size) {
    tcePoseStatus_t status;
    if(!out) return TCE_POSE_BAD_ARGUMENT;
    memset(out,0,sizeof(*out));
    status=PoseLoadHead(out,(const unsigned char *)data,size);
    if(status) memset(out,0,sizeof(*out));
    return status;
}
static void PoseBoundsInit(tcePoseBounds_t *bounds) {
    unsigned int i;
    for(i=0;i<3;++i) { bounds->mins[i]=FLT_MAX; bounds->maxs[i]=-FLT_MAX; }
}
static void PoseBoundsAdd(tcePoseBounds_t *bounds,const float point[3]) {
    unsigned int i;
    for(i=0;i<3;++i) {
        if(point[i]<bounds->mins[i]) bounds->mins[i]=point[i];
        if(point[i]>bounds->maxs[i]) bounds->maxs[i]=point[i];
    }
}
static int PoseSkinReady(const tcePoseSkinMesh_t *mesh) {
    return mesh && mesh->ready==POSE_READY && mesh->bytes && mesh->skeleton &&
        mesh->vertexCount && mesh->vertexCount<=TCE_POSE_SKIN_MAX_VERTICES &&
        mesh->triangleCount && mesh->triangleCount<=TCE_POSE_SURFACE_MAX_TRIANGLES &&
        mesh->surfaceCount<=TCE_POSE_SURFACE_MAX_SURFACES;
}
static int PoseHeadReady(const tcePoseHeadMesh_t *mesh) {
    return mesh && mesh->ready==POSE_READY && mesh->bytes &&
        mesh->vertexCount && mesh->vertexCount<=TCE_POSE_HEAD_MAX_VERTICES &&
        mesh->triangleCount && mesh->triangleCount<=TCE_POSE_SURFACE_MAX_TRIANGLES &&
        mesh->surfaceCount<=TCE_POSE_SURFACE_MAX_SURFACES;
}
static tcePoseStatus_t PoseSkinEvaluate(const tcePoseMath_t *math,const tcePoseSkinMesh_t *body,
        const tcePoseHeadMesh_t *head,const tcePoseInput_t *input,
        const tcePoseHeadInput_t *headInput,tcePoseSkin_t *out) {
    unsigned int closure[TCE_POSE_MAX_BONES],count=0,pass,i,j,k,s;
    int lookup[TCE_POSE_MAX_BONES];
    float positions[TCE_POSE_MAX_BONES][3],matrices[TCE_POSE_MAX_BONES][3][3];
    tcePoseStatus_t status;
    if(!PoseSkinReady(body) || !PoseHeadReady(head) || !headInput) return TCE_POSE_BAD_ARGUMENT;
    if(headInput->currentFrame>=head->frameCount || headInput->oldFrame>=head->frameCount)
        return TCE_POSE_BAD_RANGE;
    if(!PoseFinite(headInput->backlerp)) return TCE_POSE_NONFINITE;
    if(headInput->backlerp<0 || headInput->backlerp>1) return TCE_POSE_BAD_RANGE;
    for(i=0;i<3;++i) if(!PoseFinite(headInput->origin[i])) return TCE_POSE_NONFINITE;
    if(!PoseValidAxis(headInput->axis)) return TCE_POSE_BAD_ARGUMENT;
    for(i=0;i<TCE_POSE_MAX_BONES;++i) lookup[i]=-1;
    for(pass=0;pass<body->skeleton->boneCount && count<body->skeleton->boneCount;++pass)
        for(i=0;i<body->skeleton->boneCount;++i) {
            int parent=body->skeleton->bones[i].parent;
            if(lookup[i]<0 && (parent<0 || lookup[parent]>=0)) {
                lookup[i]=(int)count; closure[count++]=i;
            }
        }
    if(count!=body->skeleton->boneCount) return TCE_POSE_BAD_TOPOLOGY;
    status=PoseEvaluateBones(math,body->skeleton,count,closure,lookup,input,positions,matrices);
    if(status) return status;
    out->evaluatedBones=count;
    PoseBoundsInit(&out->bounds);
    for(s=0;s<body->surfaceCount;++s) {
        const tcePoseSurface_t *surface=&body->surfaces[s];
        PoseBoundsInit(&out->bodyBounds[s]);
        for(i=surface->firstVertex;i<surface->firstVertex+surface->vertexCount;++i) {
            const unsigned char *vertex=body->bytes+body->vertexOffset[i];
            unsigned int w,weights=PoseU32(vertex+20);
            double model[3]={0,0,0};
            for(w=0;w<weights;++w) {
                const unsigned char *weight=vertex+24+20u*w;
                int bone=lookup[PoseU32(weight)];
                for(j=0;j<3;++j) {
                    double value=positions[bone][j];
                    for(k=0;k<3;++k) value+=(double)matrices[bone][j][k]*PoseFloat(weight+8+4u*k);
                    model[j]+=value*PoseFloat(weight+4);
                }
            }
            for(j=0;j<3;++j) {
                double value=input->origin[j];
                for(k=0;k<3;++k) value+=(double)input->axis[k][j]*model[k];
                if(!PoseFinite(value)) return TCE_POSE_NONFINITE;
                out->vertices[i][j]=PoseRound(value);
            }
            PoseBoundsAdd(&out->bodyBounds[s],out->vertices[i]);
            PoseBoundsAdd(&out->bounds,out->vertices[i]);
        }
    }
    for(s=0;s<head->surfaceCount;++s) {
        const tcePoseSurface_t *surface=&head->surfaces[s];
        PoseBoundsInit(&out->headBounds[s]);
        for(i=0;i<surface->vertexCount;++i) {
            unsigned int index=body->vertexCount+surface->firstVertex+i;
            double model[3];
            for(j=0;j<3;++j) {
                size_t current=surface->vertexOffset+8u*(headInput->currentFrame*(size_t)surface->vertexCount+i)+2u*j;
                size_t old=surface->vertexOffset+8u*(headInput->oldFrame*(size_t)surface->vertexCount+i)+2u*j;
                model[j]=((1-(double)headInput->backlerp)*PoseI16(head->bytes+current)+
                    (double)headInput->backlerp*PoseI16(head->bytes+old))*(1.0/64.0);
            }
            for(j=0;j<3;++j) {
                double value=headInput->origin[j];
                for(k=0;k<3;++k) value+=(double)headInput->axis[k][j]*model[k];
                if(!PoseFinite(value)) return TCE_POSE_NONFINITE;
                out->vertices[index][j]=PoseRound(value);
            }
            PoseBoundsAdd(&out->headBounds[s],out->vertices[index]);
            PoseBoundsAdd(&out->bounds,out->vertices[index]);
        }
    }
    out->vertexCount=body->vertexCount+head->vertexCount;
    out->body=body; out->head=head; out->ready=POSE_READY;
    return TCE_POSE_OK;
}
tcePoseStatus_t TCE_PoseSkinEvaluate(const tcePoseMath_t *math,const tcePoseSkinMesh_t *body,
        const tcePoseHeadMesh_t *head,const tcePoseInput_t *input,
        const tcePoseHeadInput_t *headInput,tcePoseSkin_t *out) {
    tcePoseStatus_t status;
    if(!out) return TCE_POSE_BAD_ARGUMENT;
    memset(out,0,sizeof(*out));
    status=PoseSkinEvaluate(math,body,head,input,headInput,out);
    if(status) memset(out,0,sizeof(*out));
    return status;
}

static int PoseRayBounds(const double start[3],const double delta[3],double limit,
        const tcePoseBounds_t *bounds) {
    double low=0,high=limit;
    unsigned int i;
    for(i=0;i<3;++i) {
        if(delta[i]==0) {
            if(start[i]<bounds->mins[i] || start[i]>bounds->maxs[i]) return 0;
        } else {
            double a=(bounds->mins[i]-start[i])/delta[i],b=(bounds->maxs[i]-start[i])/delta[i];
            if(a>b) { double swap=a; a=b; b=swap; }
            if(a>low) low=a;
            if(b<high) high=b;
            if(low>high) return 0;
        }
    }
    return 1;
}
static double PoseDot(const double a[3],const double b[3]) {
    return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
static void PoseCross(const double a[3],const double b[3],double out[3]) {
    out[0]=a[1]*b[2]-a[2]*b[1]; out[1]=a[2]*b[0]-a[0]*b[2]; out[2]=a[0]*b[1]-a[1]*b[0];
}
static void PoseRayTriangle(const float a[3],const float b[3],const float c[3],
        const double start[3],const double delta[3],double limit,
        tcePoseRegion_t region,unsigned int surface,unsigned int triangle,tcePoseSurfaceHit_t *out) {
    double edge1[3],edge2[3],h[3],s[3],q[3],det,u,v,t;
    unsigned int i;
    ++out->trianglesTested;
    for(i=0;i<3;++i) { edge1[i]=(double)b[i]-a[i]; edge2[i]=(double)c[i]-a[i]; s[i]=start[i]-a[i]; }
    PoseCross(delta,edge2,h); det=PoseDot(edge1,h);
    if(det==0) return;
    u=PoseDot(s,h)/det;
    if(u < -1e-10 || u > 1+1e-10) return;
    PoseCross(s,edge1,q); v=PoseDot(delta,q)/det;
    if(v < -1e-10 || u+v > 1+1e-10) return;
    t=PoseDot(edge2,q)/det;
    if(t<0 || t>limit) return;
    if(!(out->hitMask&(1u<<region)) || t<out->regionFraction[region]) out->regionFraction[region]=t;
    if(!out->hitMask || t<out->fraction || (t==out->fraction && region<out->region)) {
        double normal[3],length;
        out->fraction=t; out->region=region; out->surface=surface; out->triangle=triangle;
        PoseCross(edge1,edge2,normal); length=sqrt(PoseDot(normal,normal));
        if(PoseDot(normal,delta)>0) length=-length;
        for(i=0;i<3;++i) {
            out->point[i]=PoseRound(start[i]+t*delta[i]); out->normal[i]=PoseRound(normal[i]/length);
        }
    }
    out->hitMask|=1u<<region;
}
tcePoseStatus_t TCE_PoseSkinRaycast(const tcePoseSkin_t *skin,const float from[3],
        const float to[3],double maxFraction,tcePoseSurfaceHit_t *out) {
    double start[3],delta[3];
    unsigned int i,s,n;
    if(!out) return TCE_POSE_BAD_ARGUMENT;
    memset(out,0,sizeof(*out));
    if(!skin || skin->ready!=POSE_READY || !PoseSkinReady(skin->body) || !PoseHeadReady(skin->head) ||
       skin->vertexCount!=skin->body->vertexCount+skin->head->vertexCount || !from || !to)
        return TCE_POSE_BAD_ARGUMENT;
    if(!PoseFinite(maxFraction)) return TCE_POSE_NONFINITE;
    if(maxFraction<0 || maxFraction>1) return TCE_POSE_BAD_RANGE;
    for(i=0;i<3;++i) {
        if(!PoseFinite(from[i]) || !PoseFinite(to[i])) return TCE_POSE_NONFINITE;
        start[i]=from[i]; delta[i]=(double)to[i]-from[i];
    }
    if(PoseDot(delta,delta)==0) return TCE_POSE_BAD_RANGE;
    out->region=TCE_POSE_REGION_NONE; out->fraction=maxFraction;
    out->surface=out->triangle=UINT32_MAX;
    for(i=0;i<3;++i) out->regionFraction[i]=maxFraction;
    if(!PoseRayBounds(start,delta,maxFraction,&skin->bounds)) return TCE_POSE_OK;
    for(s=0;s<skin->body->surfaceCount;++s) {
        const tcePoseSurface_t *surface=&skin->body->surfaces[s];
        ++out->surfacesTested;
        if(!PoseRayBounds(start,delta,maxFraction,&skin->bodyBounds[s])) continue;
        for(n=surface->firstTriangle;n<surface->firstTriangle+surface->triangleCount;++n) {
            const uint16_t *tri=skin->body->triangle[n];
            PoseRayTriangle(skin->vertices[tri[0]],skin->vertices[tri[1]],skin->vertices[tri[2]],
                start,delta,maxFraction,(tcePoseRegion_t)skin->body->region[n],s,n,out);
        }
    }
    for(s=0;s<skin->head->surfaceCount;++s) {
        const tcePoseSurface_t *surface=&skin->head->surfaces[s];
        ++out->surfacesTested;
        if(!PoseRayBounds(start,delta,maxFraction,&skin->headBounds[s])) continue;
        for(n=surface->firstTriangle;n<surface->firstTriangle+surface->triangleCount;++n) {
            const uint16_t *tri=skin->head->triangle[n];
            i=skin->body->vertexCount;
            PoseRayTriangle(skin->vertices[i+tri[0]],skin->vertices[i+tri[1]],skin->vertices[i+tri[2]],
                start,delta,maxFraction,TCE_POSE_REGION_HEAD,s,n,out);
        }
    }
    return TCE_POSE_OK;
}

void TCE_PoseSurfaceRegistryInit(tcePoseSurfaceRegistry_t *registry) {
    if(registry) memset(registry,0,sizeof(*registry));
}
tcePoseStatus_t TCE_PoseSurfaceRegistryBody(tcePoseSurfaceRegistry_t *registry,const char *name,
        const tcePoseSkeleton_t *skeleton,tcePoseLoadBytes_t load,void *user,const tcePoseSkinMesh_t **out) {
    unsigned int i;
    const unsigned char *bytes=NULL;
    size_t size=0;
    tcePoseStatus_t status;
    if(out) *out=NULL;
    if(!registry || !out || !skeleton || !PoseCString(name)) return TCE_POSE_BAD_ARGUMENT;
    if(registry->meshCount>TCE_POSE_SURFACE_MAX_MESHES || registry->headCount>TCE_POSE_SURFACE_MAX_HEADS)
        return TCE_POSE_BAD_ARGUMENT;
    for(i=0;i<registry->meshCount;++i) if(!strcmp(registry->meshes[i].name,name)) {
        if(!PoseCompatible(registry->meshes[i].model.skeleton,skeleton)) return TCE_POSE_INCOMPATIBLE;
        *out=&registry->meshes[i].model; return TCE_POSE_OK;
    }
    if(!load) return TCE_POSE_BAD_ARGUMENT;
    if(registry->meshCount==TCE_POSE_SURFACE_MAX_MESHES) return TCE_POSE_LIMIT;
    if(!load(user,name,&bytes,&size)) return TCE_POSE_NOT_FOUND;
    if(registry->borrowedBytes>TCE_POSE_MAX_CACHE_BYTES || size>TCE_POSE_MAX_CACHE_BYTES-registry->borrowedBytes)
        return TCE_POSE_LIMIT;
    status=TCE_PoseLoadSkin(&registry->meshes[i].model,bytes,size,skeleton);
    if(status) return status;
    strcpy(registry->meshes[i].name,name);
    registry->borrowedBytes+=size; ++registry->meshCount; *out=&registry->meshes[i].model;
    return TCE_POSE_OK;
}
tcePoseStatus_t TCE_PoseSurfaceRegistryHead(tcePoseSurfaceRegistry_t *registry,const char *name,
        tcePoseLoadBytes_t load,void *user,const tcePoseHeadMesh_t **out) {
    unsigned int i;
    const unsigned char *bytes=NULL;
    size_t size=0;
    tcePoseStatus_t status;
    if(out) *out=NULL;
    if(!registry || !out || !PoseCString(name)) return TCE_POSE_BAD_ARGUMENT;
    if(registry->meshCount>TCE_POSE_SURFACE_MAX_MESHES || registry->headCount>TCE_POSE_SURFACE_MAX_HEADS)
        return TCE_POSE_BAD_ARGUMENT;
    for(i=0;i<registry->headCount;++i) if(!strcmp(registry->heads[i].name,name)) {
        *out=&registry->heads[i].model; return TCE_POSE_OK;
    }
    if(!load) return TCE_POSE_BAD_ARGUMENT;
    if(registry->headCount==TCE_POSE_SURFACE_MAX_HEADS) return TCE_POSE_LIMIT;
    if(!load(user,name,&bytes,&size)) return TCE_POSE_NOT_FOUND;
    if(registry->borrowedBytes>TCE_POSE_MAX_CACHE_BYTES || size>TCE_POSE_MAX_CACHE_BYTES-registry->borrowedBytes)
        return TCE_POSE_LIMIT;
    status=TCE_PoseLoadHead(&registry->heads[i].model,bytes,size);
    if(status) return status;
    strcpy(registry->heads[i].name,name);
    registry->borrowedBytes+=size; ++registry->headCount; *out=&registry->heads[i].model;
    return TCE_POSE_OK;
}
