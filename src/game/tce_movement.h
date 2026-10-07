#ifndef TCE_MOVEMENT_H
#define TCE_MOVEMENT_H

/* Projected inputs for Windows PM_CmdScale 3000bc40. No hidden state. */
typedef struct {
    int forward, right, up, commandButtons, moveButtons;
    int speed, ducked, ladder, sprintTime, noclip;
    int tactical, weaponWeight, carriedWeight, gametype, movespeed;
    float lean, runScale, sprintScale, crouchScale;
} tce_moveScale_t;

float TCE_PM_CmdScale(const tce_moveScale_t *state);
#endif
