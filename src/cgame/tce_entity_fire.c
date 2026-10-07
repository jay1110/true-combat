#include "cg_local.h"

/* TC:E Windows 30052cd0; Linux reference 00094e54.
 * Local players use the snapshot timer, other entities their network interval.
 * Both endpoints are exclusive; a zero snapshot start is never on fire. */
qboolean CG_EntOnFire( centity_t *cent ) {
	if ( cent->currentState.number == cg.snap->ps.clientNum ) {
		// TAT 11/15/2002 - the player is always starting out on fire, which is easily seen in cinematics
		//		so make sure onFireStart is not 0
		return	( cg.snap->ps.onFireStart
			&& (cg.snap->ps.onFireStart < cg.time)
			&& ((cg.snap->ps.onFireStart+2000) > cg.time) );
	}
	else {
		return	(	(cent->currentState.onFireStart < cg.time) &&
					(cent->currentState.onFireEnd > cg.time));
	}
}
