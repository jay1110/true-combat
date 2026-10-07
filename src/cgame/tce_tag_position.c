/* Windows cgame:3002de20 and3002dee0; original-compatible SDK helpers. */
#include "cg_local.h"

/* Original x87 keeps each dot product precise until the final float store. */
static void TCE_TagMatrixMultiply(const vec3_t a[3], const vec3_t b[3], vec3_t out[3]) {
    int i, j;
    for (i = 0; i < 3; ++i) for (j = 0; j < 3; ++j)
        out[i][j] = (float)((double)a[i][0] * b[0][j] +
            (double)a[i][1] * b[1][j] + (double)a[i][2] * b[2][j]);
}
static void TCE_TagOriginStep(vec3_t origin, float distance, const vec3_t axis) {
    int i;
    for (i = 0; i < 3; ++i) origin[i] = (float)((double)distance * axis[i] + origin[i]);
}

/*
======================
CG_PositionEntityOnTag

Modifies the entities position and axis by the given
tag location
======================
*/
void CG_PositionEntityOnTag( refEntity_t *entity, const refEntity_t *parent, const char *tagName, int startIndex, vec3_t *offset) {
	int				i;
	orientation_t	lerped;

	// lerp the tag
	trap_R_LerpTag( &lerped, parent, tagName, startIndex );

	// allow origin offsets along tag
	VectorCopy( parent->origin, entity->origin );

	if( offset ) {
		VectorAdd( lerped.origin, *offset, lerped.origin );
	}

	for ( i = 0 ; i < 3 ; i++ ) {
		TCE_TagOriginStep( entity->origin, lerped.origin[i], parent->axis[i] );
	}

	TCE_TagMatrixMultiply( lerped.axis, parent->axis, entity->axis );
}


/*
======================
CG_PositionRotatedEntityOnTag

Modifies the entities position and axis by the given
tag location
======================
*/
void CG_PositionRotatedEntityOnTag( refEntity_t *entity, const refEntity_t *parent, const char *tagName ) {
	int				i;
	orientation_t	lerped;
	vec3_t			tempAxis[3];

//AxisClear( entity->axis );
	// lerp the tag
	trap_R_LerpTag( &lerped, parent, tagName, 0 );

	// FIXME: allow origin offsets along tag?
	VectorCopy( parent->origin, entity->origin );
	for ( i = 0 ; i < 3 ; i++ ) {
		TCE_TagOriginStep( entity->origin, lerped.origin[i], parent->axis[i] );
	}

	TCE_TagMatrixMultiply( entity->axis, lerped.axis, tempAxis );
	TCE_TagMatrixMultiply( tempAxis, parent->axis, entity->axis );
}


