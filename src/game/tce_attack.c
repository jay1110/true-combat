#include "tce_attack.h"
int TCE_PM_AttackAnimForWeapon(int weapon) {
    switch(weapon) {
    case 28:case 55:case 56:case 61:case 62:return 3;
    default:return 2;
    }
}
int TCE_PM_LastAttackAnimForWeapon(int weapon) {
    switch(weapon) {
    case 55:case 56:case 62:return 3;
    case 60:return 2;
    default:return 4;
    }
}

/* TC:E cgame PM_RaiseAnimForWeapon:300088c0. SDK IDs must not be substituted. */
int TCE_PM_RaiseAnimForWeapon(int weapon) {
    switch (weapon) {
    case 28: return 8;
    case 55: case 56: return 9;
    case 62: return 12;
    default: return 6;
    }
}

int TCE_PM_DropAnimForWeapon(int weapon) {
    switch(weapon) {
    case 28:return 7;
    case 55:case 56:return 12;
    default:return 5;
    }
}
