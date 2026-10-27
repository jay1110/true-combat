/* Mod-owned, checked MDX/MDM attachment evaluation. No engine structure changes.
 * Loaded byte buffers are borrowed immutable storage and must outlive the cache.
 * Loaded objects and the registry must not be moved after attachments or frame
 * pairs retain their addresses; callers serialize registry loading themselves.
 * Evaluation allocates nothing and never calls a loader or filesystem service.
 */
#ifndef TCE_POSE_MATH_H
#define TCE_POSE_MATH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    TCE_POSE_ANCHORS = 7,
    TCE_POSE_MAX_BONES = 128,
    TCE_POSE_MAX_TAGS = 128,
    TCE_POSE_MAX_FRAMES = 8192,
    TCE_POSE_MAX_CLOSURE = 32,
    /* Shipped animation groups use ten clips; fourteen player meshes plus two
     * spare corpse/team variants fit the bounded eight MiB borrowed-byte cap. */
    TCE_POSE_MAX_SKELETONS = 10,
    TCE_POSE_MAX_MESHES = 16,
    TCE_POSE_NAME_BYTES = 64,
    TCE_POSE_TABLE_SIZE = 4096,
    TCE_POSE_MAX_ASSET_BYTES = 8 * 1024 * 1024,
    TCE_POSE_MAX_CACHE_BYTES = 8 * 1024 * 1024
};

typedef enum {
    TCE_POSE_HEAD, TCE_POSE_CHEST, TCE_POSE_TORSO,
    TCE_POSE_THIGH_LEFT, TCE_POSE_THIGH_RIGHT,
    TCE_POSE_FOOT_LEFT, TCE_POSE_FOOT_RIGHT
} tcePoseAnchorIndex_t;

typedef enum {
    TCE_POSE_OK = 0, TCE_POSE_BAD_ARGUMENT, TCE_POSE_BAD_FORMAT,
    TCE_POSE_BAD_RANGE, TCE_POSE_NONFINITE, TCE_POSE_BAD_TOPOLOGY,
    TCE_POSE_LIMIT, TCE_POSE_INCOMPATIBLE, TCE_POSE_NOT_FOUND
} tcePoseStatus_t;

typedef enum {
    TCE_POSE_MATH_ANALYTIC = 0,
    /* Explicit observed Windows renderer profile: short/table arithmetic,
     * fast direction normalization, current-only partial torso offsets, and
     * the reference old-torso/old-legs equal-frame test. Captures establish
     * its active paths, not every engine/backend or hypothetical input. */
    TCE_POSE_MATH_OBSERVED_WINDOWS = 1
} tcePoseMathMode_t;

typedef struct {
    char name[TCE_POSE_NAME_BYTES];
    int parent;
    float torsoWeight, parentDistance;
    uint32_t flags;
} tcePoseBone_t;

typedef struct {
    const unsigned char *bytes;
    size_t byteCount, frameOffset, frameStride;
    unsigned int frameCount, boneCount, torsoParent;
    uint32_t ready;
    tcePoseBone_t bones[TCE_POSE_MAX_BONES];
} tcePoseSkeleton_t;

typedef struct {
    char name[TCE_POSE_NAME_BYTES];
    unsigned int bone;
    float offset[3], axis[3][3];
} tcePoseTag_t;

typedef struct {
    size_t byteCount;
    unsigned int tagCount, maxReferencedBone;
    uint32_t ready;
    tcePoseTag_t tags[TCE_POSE_MAX_TAGS];
} tcePoseMesh_t;

typedef struct {
    const tcePoseSkeleton_t *skeleton;
    unsigned int closureCount, closure[TCE_POSE_MAX_CLOSURE];
    int boneToClosure[TCE_POSE_MAX_BONES];
    tcePoseTag_t tags[TCE_POSE_ANCHORS];
    uint32_t ready;
} tcePoseAttachment_t;

typedef struct {
    uint32_t ready;
    float sine[TCE_POSE_TABLE_SIZE];
} tcePoseMath_t;

typedef struct {
    const tcePoseSkeleton_t *currentModel, *oldModel;
    unsigned int currentFrame, oldFrame;
    float backlerp;
} tcePoseFramePair_t;

typedef struct {
    tcePoseFramePair_t legs, torso;
    /* refEntity convention: rows are basis vectors. Body axis may be scaled. */
    float origin[3], axis[3][3], torsoAxis[3][3];
    tcePoseMathMode_t mode;
} tcePoseInput_t;

typedef struct { float origin[3], axis[3][3]; } tcePoseAnchor_t;
typedef struct {
    tcePoseAnchor_t model[TCE_POSE_ANCHORS], world[TCE_POSE_ANCHORS];
    unsigned int evaluatedBones;
} tcePoseResult_t;

/* The callback runs only during registry loading, never evaluation. Success
 * supplies immutable bytes; ownership remains with the caller. */
typedef int (*tcePoseLoadBytes_t)(void *user, const char *name,
                                const unsigned char **bytes, size_t *size);
typedef struct {
    char name[TCE_POSE_NAME_BYTES];
    tcePoseSkeleton_t model;
} tcePoseSkeletonEntry_t;
typedef struct {
    char name[TCE_POSE_NAME_BYTES];
    tcePoseMesh_t model;
} tcePoseMeshEntry_t;
typedef struct {
    size_t borrowedBytes;
    unsigned int skeletonCount, meshCount;
    tcePoseSkeletonEntry_t skeletons[TCE_POSE_MAX_SKELETONS];
    tcePoseMeshEntry_t meshes[TCE_POSE_MAX_MESHES];
} tcePoseRegistry_t;

const char *TCE_PoseStatusString(tcePoseStatus_t status);
const char *TCE_PoseAnchorName(unsigned int anchor);
tcePoseStatus_t TCE_PoseMathInit(tcePoseMath_t *math);
tcePoseStatus_t TCE_PoseLoadSkeleton(tcePoseSkeleton_t *out, const void *bytes, size_t size);
tcePoseStatus_t TCE_PoseLoadMesh(tcePoseMesh_t *out, const void *bytes, size_t size);
tcePoseStatus_t TCE_PosePrepareAttachment(tcePoseAttachment_t *out,
                                         const tcePoseMesh_t *mesh,
                                         const tcePoseSkeleton_t *skeleton);
tcePoseStatus_t TCE_PoseEvaluate(const tcePoseMath_t *math,
                                const tcePoseAttachment_t *attachment,
                                const tcePoseInput_t *input,
                                tcePoseResult_t *out);
void TCE_PoseRegistryInit(tcePoseRegistry_t *registry);
tcePoseStatus_t TCE_PoseRegistrySkeleton(tcePoseRegistry_t *registry, const char *name,
                                        tcePoseLoadBytes_t load, void *user,
                                        const tcePoseSkeleton_t **out);
tcePoseStatus_t TCE_PoseRegistryMesh(tcePoseRegistry_t *registry, const char *name,
                                    tcePoseLoadBytes_t load, void *user,
                                    const tcePoseMesh_t **out);

/* Optional exact surface evaluator. It is not an active damage provider.
 * Existing attachment layouts and the 32-bone attachment closure are unchanged.
 * Asset/cache objects remain immutable at stable addresses after registration.
 * All storage is caller-owned; evaluation/raycasting allocate and load nothing. */
enum {
    TCE_POSE_SKIN_MAX_VERTICES = 4096,
    TCE_POSE_HEAD_MAX_VERTICES = 4096,
    TCE_POSE_SURFACE_MAX_TRIANGLES = 8192,
    TCE_POSE_SURFACE_MAX_SURFACES = 32,
    TCE_POSE_SKIN_MAX_WEIGHTS = 32768,
    TCE_POSE_SURFACE_MAX_MESHES = 16,
    TCE_POSE_SURFACE_MAX_HEADS = 4,
    TCE_POSE_SURFACE_MAX_VERTICES = TCE_POSE_SKIN_MAX_VERTICES + TCE_POSE_HEAD_MAX_VERTICES
};
typedef enum { TCE_POSE_REGION_NONE = -1, TCE_POSE_REGION_HEAD = 0,
    TCE_POSE_REGION_BODY = 1, TCE_POSE_REGION_LEGS = 2 } tcePoseRegion_t;
typedef struct {
    char name[TCE_POSE_NAME_BYTES];
    unsigned int firstVertex, vertexCount, firstTriangle, triangleCount;
    size_t vertexOffset;
} tcePoseSurface_t;
typedef struct {
    const unsigned char *bytes;
    size_t byteCount;
    const tcePoseSkeleton_t *skeleton;
    unsigned int surfaceCount, vertexCount, triangleCount, weightCount;
    tcePoseSurface_t surfaces[TCE_POSE_SURFACE_MAX_SURFACES];
    size_t vertexOffset[TCE_POSE_SKIN_MAX_VERTICES];
    uint16_t triangle[TCE_POSE_SURFACE_MAX_TRIANGLES][3];
    unsigned char region[TCE_POSE_SURFACE_MAX_TRIANGLES];
    uint32_t ready;
} tcePoseSkinMesh_t;
typedef struct {
    const unsigned char *bytes;
    size_t byteCount;
    unsigned int frameCount, surfaceCount, vertexCount, triangleCount;
    tcePoseSurface_t surfaces[TCE_POSE_SURFACE_MAX_SURFACES];
    uint16_t triangle[TCE_POSE_SURFACE_MAX_TRIANGLES][3];
    uint32_t ready;
} tcePoseHeadMesh_t;
typedef struct {
    unsigned int currentFrame, oldFrame;
    float backlerp;
    /* Explicit head refEntity transform, including head-relative orientation.
     * A body tag alone does not supply the head/face animation contract. */
    float origin[3], axis[3][3];
} tcePoseHeadInput_t;
typedef struct { float mins[3], maxs[3]; } tcePoseBounds_t;
typedef struct {
    const tcePoseSkinMesh_t *body;
    const tcePoseHeadMesh_t *head;
    unsigned int vertexCount, evaluatedBones;
    float vertices[TCE_POSE_SURFACE_MAX_VERTICES][3];
    tcePoseBounds_t bounds;
    tcePoseBounds_t bodyBounds[TCE_POSE_SURFACE_MAX_SURFACES];
    tcePoseBounds_t headBounds[TCE_POSE_SURFACE_MAX_SURFACES];
    uint32_t ready;
} tcePoseSkin_t;
typedef struct {
    unsigned int hitMask;
    tcePoseRegion_t region;
    double fraction, regionFraction[3];
    unsigned int surface, triangle;
    float point[3], normal[3];
    /* Diagnostics distinguish whole/surface broadphase work from exact tests. */
    unsigned int surfacesTested, trianglesTested;
} tcePoseSurfaceHit_t;
typedef struct { char name[TCE_POSE_NAME_BYTES]; tcePoseSkinMesh_t model; } tcePoseSkinEntry_t;
typedef struct { char name[TCE_POSE_NAME_BYTES]; tcePoseHeadMesh_t model; } tcePoseHeadEntry_t;
typedef struct {
    size_t borrowedBytes;
    unsigned int meshCount, headCount;
    tcePoseSkinEntry_t meshes[TCE_POSE_SURFACE_MAX_MESHES];
    tcePoseHeadEntry_t heads[TCE_POSE_SURFACE_MAX_HEADS];
} tcePoseSurfaceRegistry_t;

tcePoseStatus_t TCE_PoseLoadSkin(tcePoseSkinMesh_t *out, const void *bytes, size_t size,
                               const tcePoseSkeleton_t *skeleton);
tcePoseStatus_t TCE_PoseLoadHead(tcePoseHeadMesh_t *out, const void *bytes, size_t size);
tcePoseStatus_t TCE_PoseSkinEvaluate(const tcePoseMath_t *math, const tcePoseSkinMesh_t *body,
                                   const tcePoseHeadMesh_t *head, const tcePoseInput_t *input,
                                   const tcePoseHeadInput_t *headInput, tcePoseSkin_t *out);
/* Two-sided finite surface intersections, including endpoints. Parallel and
 * coplanar triangles do not count. Starting inside a mesh finds its exit, not
 * a synthetic solid-volume t=0. maxFraction is an explicit world-cover clamp.
 * Surface names u_* map BODY; l_* triangles map by summed skin-weight ancestry
 * beneath named left/right thigh roots, keeping pelvis/waist triangles BODY.
 * These anatomy semantics are explicit and are not legacy damage-weight parity. */
tcePoseStatus_t TCE_PoseSkinRaycast(const tcePoseSkin_t *skin, const float start[3],
                                  const float end[3], double maxFraction,
                                  tcePoseSurfaceHit_t *out);
void TCE_PoseSurfaceRegistryInit(tcePoseSurfaceRegistry_t *registry);
tcePoseStatus_t TCE_PoseSurfaceRegistryBody(tcePoseSurfaceRegistry_t *registry,
    const char *name, const tcePoseSkeleton_t *skeleton, tcePoseLoadBytes_t load,
    void *user, const tcePoseSkinMesh_t **out);
tcePoseStatus_t TCE_PoseSurfaceRegistryHead(tcePoseSurfaceRegistry_t *registry,
    const char *name, tcePoseLoadBytes_t load, void *user, const tcePoseHeadMesh_t **out);

#ifdef __cplusplus
}
#endif
#endif
