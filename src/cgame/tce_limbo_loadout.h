#ifndef TCE_LIMBO_LOADOUT_H
#define TCE_LIMBO_LOADOUT_H
typedef struct {
    int team, playerClass, rating, previousRating, scoreClass;
    int lightSkill, heavySkill, primary, gametype;
} tce_limboLoadout_t;
int TCE_LimboWeaponCount(const tce_limboLoadout_t *state, int slot);
int TCE_LimboWeaponForNumber(const tce_limboLoadout_t *state, int number, int slot);
#endif
