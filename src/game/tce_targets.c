#include "g_local.h"

/* TC:E encodes environment parameters in existing network-state fields.
 * Client environment rendering is a separate reconstruction task. */
void SP_target_environment(gentity_t *ent) {
    char *name, sound[MAX_QPATH];
    float exposure;
    if (!G_SpawnString("ambient", "NOSOUND", &name)) {
        G_Error("target_environment without an ambient key at %s", vtos(ent->s.origin));
        return;
    }
    Com_sprintf(sound, sizeof(sound), "%s_1.wav", name);
    ent->s.onFireEnd = G_SoundIndex(sound);
    Com_sprintf(sound, sizeof(sound), "%s_2.wav", name);
    ent->s.modelindex2 = G_SoundIndex(sound);
    G_SpawnString("reverb", "NOSOUND", &name);
    Com_sprintf(sound, sizeof(sound), "%s_1.wav", name);
    ent->s.otherEntityNum = G_SoundIndex(sound);
    Com_sprintf(sound, sizeof(sound), "%s_2.wav", name);
    ent->s.effect3Time = G_SoundIndex(sound);
    ent->s.eType = ET_ENVIRONMENT;
    if (ent->spawnflags & 1) {
        ent->r.svFlags |= SVF_BROADCAST;
        ent->s.otherEntityNum2 = 1;
        G_SpawnFloat("eyeadaptation_outdoor", "0.0", &exposure);
        ent->s.frame = (int)((double)exposure * 255.0);
        G_SpawnFloat("eyeadaptation_default", "0.0", &exposure);
        ent->s.effect2Time = (int)((double)exposure * 255.0);
        G_SpawnFloat("eyeadaptation_sun", "0.0", &exposure);
        ent->s.nextWeapon = (int)((double)exposure * 255.0);
    } else {
        G_SpawnFloat("eyeadaptation", "0.0", &exposure);
        ent->s.effect2Time = (int)((double)exposure * 255.0);
    }
    G_SpawnInt("volume", "255", &ent->s.onFireStart);
    if (!ent->s.onFireStart) ent->s.onFireStart = 255;
    ent->s.density = !!(ent->spawnflags & 2);
    ent->s.dmgFlags = ent->count ? ent->count : 256;
    trap_LinkEntity(ent);
}

void target_location_linkup(gentity_t *self) {
    int i, count = 1;
    gentity_t *ent;
    (void)self;
    if (level.locationLinked) return;
    level.locationLinked = qtrue;
    level.locationHead = NULL;
    trap_SetConfigstring(CS_LOCATIONS, "unknown");
    for (i=0; i<level.num_entities; ++i) {
        ent = &g_entities[i];
        if (!ent->classname || Q_stricmp(ent->classname,"target_location")) continue;
        /* The original overruns configstrings for excessive map locations. */
        if (CS_LOCATIONS + count >= MAX_CONFIGSTRINGS) {
            G_Error("Too many target_location entities");
            return;
        }
        ent->health = count;
        trap_SetConfigstring(CS_LOCATIONS + count++, ent->message);
        ent->nextTrain = level.locationHead;
        level.locationHead = ent;
    }
}

void SP_target_location(gentity_t *self) {
    self->think = target_location_linkup;
    self->nextthink = level.time + 200;
    G_SetOrigin(self, self->s.origin);
}
