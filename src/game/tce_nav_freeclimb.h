#ifndef TCE_NAV_FREECLIMB_H
#define TCE_NAV_FREECLIMB_H
/* Directed navigation input only. The native adapter sends ordinary
 * forward/jump usercmds; PM_CheckLadderMove/PM_ClimbSlideMove perform the
 * actual TC free-climb. No player velocity, flags or origin are assigned. */
#define TCE_NAV_FREECLIMB_BUTTON 61
#define TCE_NAV_FREECLIMB_READY_STAMINA 2500
#endif
