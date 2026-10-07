/* Windows cgame:3006e870. Preserve original finite-input behavior, including
 * zero-filled remaining animation fields when the body ends early.
 */
#include "cg_local.h"
#include "tce_weapon_media.h"

int TCE_CG_ParseWeaponConfig(const char *filename, tce_weaponInfo_t *info) {
    char text[20000], *cursor, *previous, *token;
    fileHandle_t file;
    int length, i, newFormat = 0;
    length = trap_FS_FOpenFile(filename, &file, FS_READ);
    if (length <= 0) return 0;
    if (length >= sizeof(text) - 1) {
        /* Original returns without closing this oversized source. */
        CG_Printf("File %s too long\n", filename);
        return 0;
    }
    trap_FS_Read(text, length, file); text[length] = 0;
    trap_FS_FCloseFile(file);
    cursor = text;
    for (;;) {
        previous = cursor; token = COM_Parse(&cursor);
        if (!token[0]) {
            /* Original loops forever printing empty unknown tokens here.
             * End the malformed header explicitly; no valid file is affected. */
            CG_Printf("Error parsing weapon animation file: %s", filename);
            return 0;
        }
        if (!Q_stricmp(token, "newfmt")) newFormat = 1;
        else if (token[0] >= '0' && token[0] <= '9') { cursor = previous; break; }
        else CG_Printf("unknown token in weapon cfg '%s' is %s\n", token, filename);
    }
    for (i = 0; i < 13; ++i) {
        tce_weaponAnimation_t *animation = &info->animations[i];
        double fps;
        animation->firstFrame = atoi(COM_Parse(&cursor));
        animation->numFrames = atoi(COM_Parse(&cursor));
        fps = atof(COM_Parse(&cursor));
        if (fps == 0) fps = 1;
        /* Windows keeps atof's double in x87; no intermediate float cast. */
        animation->frameLerp = animation->initialLerp = (int)(1000.0 / fps);
        animation->loopFrames = atoi(COM_Parse(&cursor));
        if (animation->loopFrames > animation->numFrames) animation->loopFrames = animation->numFrames;
        else if (animation->loopFrames < 0) animation->loopFrames = 0;
        animation->moveSpeed = 0;
        if (newFormat) {
            animation->moveSpeed = atoi(COM_Parse(&cursor));
            if (atoi(COM_Parse(&cursor))) animation->moveSpeed |= 0x80;
            animation->moveSpeed |= (unsigned int)atoi(COM_Parse(&cursor)) << 8;
        }
    }
    return 1;
}
