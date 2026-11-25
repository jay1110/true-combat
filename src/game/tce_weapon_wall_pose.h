/* Shared low-ready pose selection. Use declared animation metadata, never
 * stock frame numbers: user animation packs retain their own MDX/frame map. */
#ifndef TCE_WEAPON_WALL_POSE_H
#define TCE_WEAPON_WALL_POSE_H
static __inline int TCE_WeaponWallLowAnimation(const animModelInfo_t *model)
{
    int i;
    if (!model) return -1;
    for (i=0;i<model->numAnimations && i<MAX_MODEL_ANIMATIONS;++i)
        if (model->animations[i] && model->animations[i]->numFrames>0 &&
            !Q_stricmp(model->animations[i]->name,"relaxed_idle_1h_1")) return i;
    return -1;
}
static __inline int TCE_WeaponWallLowFrame(const animation_t *animation)
{
    return animation->firstFrame + ((animation->flags & ANIMFL_REVERSED) ? animation->numFrames-1 : 0);
}
#endif
