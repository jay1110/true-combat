/* Windows cgame:3002de20 and3002dee0; original-compatible SDK helpers. */
#include "cg_local.h"

/* The native tag transforms use the shared matrix provider. */
static void TCE_TagMatrixMultiply(const vec3_t a[3], const vec3_t b[3], vec3_t out[3])
{
	/* MatrixMultiply retains its SDK signature but only reads its inputs. */
	MatrixMultiply((float (*)[3]) a, (float (*)[3]) b, out);
}

/*
======================
CG_PositionEntityOnTag

Modifies the entities position and axis by the given
tag location
======================
*/
void CG_PositionEntityOnTag(refEntity_t *entity, const refEntity_t *parent, const char *tagName, int startIndex, vec3_t *offset)
{
	int           i;
	orientation_t lerped;

	// lerp the tag
	trap_R_LerpTag(&lerped, parent, tagName, startIndex);

	// allow origin offsets along tag
	VectorCopy(parent->origin, entity->origin);

	if (offset)
	{
		VectorAdd(lerped.origin, *offset, lerped.origin);
	}

	for ( i = 0 ; i < 3 ; i++ )
	{
		VectorMA(entity->origin, lerped.origin[i], parent->axis[i], entity->origin);
	}

	TCE_TagMatrixMultiply(lerped.axis, parent->axis, entity->axis);
}


/*
======================
CG_PositionRotatedEntityOnTag

Modifies the entities position and axis by the given
tag location
======================
*/
void CG_PositionRotatedEntityOnTag(refEntity_t *entity, const refEntity_t *parent, const char *tagName)
{
	int           i;
	orientation_t lerped;
	vec3_t        tempAxis[3];

//AxisClear( entity->axis );
	// lerp the tag
	trap_R_LerpTag(&lerped, parent, tagName, 0);

	// FIXME: allow origin offsets along tag?
	VectorCopy(parent->origin, entity->origin);
	for ( i = 0 ; i < 3 ; i++ )
	{
		VectorMA(entity->origin, lerped.origin[i], parent->axis[i], entity->origin);
	}

	TCE_TagMatrixMultiply(entity->axis, lerped.axis, tempAxis);
	TCE_TagMatrixMultiply(tempAxis, parent->axis, entity->axis);
}

#if defined(__linux__)
/* TC Linux 5d400: offset is a three-row field, not one contiguous vector. */
void CG_PositionRotatedEntityOnTagOffset(refEntity_t *entity,
                                        const refEntity_t *parent,
                                        const char *tagName,
                                        const vec3_t offset[3])
{
	orientation_t lerped;
	vec3_t tempAxis[3];
	long double x, y;
	int i;

	trap_R_LerpTag(&lerped, parent, tagName, 0);
	VectorCopy(parent->origin, entity->origin);
	x = entity->origin[0];
	y = entity->origin[1];
	for (i = 0; i < 3; ++i)
	{
		x += (long double)parent->axis[i][0] * lerped.origin[i];
		y += (long double)parent->axis[i][1] * lerped.origin[i];
		entity->origin[2] = (float)((long double)lerped.origin[i] *
		                            parent->axis[i][2] + entity->origin[2]);
	}
	entity->origin[1] = (float)y;
	entity->origin[0] = (float)x;
	TCE_TagMatrixMultiply(entity->axis, lerped.axis, tempAxis);
	TCE_TagMatrixMultiply(tempAxis, parent->axis, entity->axis);
	TCE_TagMatrixMultiply(lerped.axis, parent->axis, tempAxis);
	for (i = 0; i < 3; ++i)
	{
		entity->origin[0] = (float)((long double)offset[i][0] *
		                            tempAxis[i][0] + entity->origin[0]);
		entity->origin[1] = (float)((long double)offset[i][0] *
		                            tempAxis[i][1] + entity->origin[1]);
		entity->origin[2] = (float)((long double)offset[i][0] *
		                            tempAxis[i][2] + entity->origin[2]);
	}
}
#endif

#if defined(__linux__)
/* TC Linux 5d204. Preserve the original mixed-tag arithmetic, including
 * its final-row expression and negative half-angle rotation. */
void CG_PositionEntityOnMixedTag(refEntity_t *entity, const refEntity_t *parent,
                                 const char *firstTag, const char *secondTag,
                                 int startIndex, const vec3_t *offset)
{
	orientation_t first, second;
	vec3_t otherOrigin, direction;
	long double x, y, z, dot;
	double angle;
	int i;

	trap_R_LerpTag(&first, parent, firstTag, startIndex);
	trap_R_LerpTag(&second, parent, secondTag, startIndex);
	VectorCopy(parent->origin, entity->origin);
	if (offset)
	{
		VectorAdd(*offset, first.origin, first.origin);
	}
	x = entity->origin[0];
	y = entity->origin[1];
	z = entity->origin[2];
	for (i = 0; i < 3; ++i)
	{
		x = (long double)parent->axis[i][0] * first.origin[i] + x;
		y = (long double)parent->axis[i][1] * first.origin[i] + y;
		z = (long double)first.origin[i] * parent->axis[i][2] + entity->origin[2];
		entity->origin[2] = (float)z;
	}
	/* The original keeps only the last iteration's secondary expression. */
	otherOrigin[2] = (float)(z + (long double)parent->axis[2][2] *
	                        (((long double)second.origin[2] * parent->axis[2][0]) *
	                         parent->axis[2][1]));
	otherOrigin[1] = (float)((long double)parent->axis[2][1] + y);
	otherOrigin[0] = (float)((long double)parent->axis[2][0] + x);
	entity->origin[1] = (float)y;
	entity->origin[0] = (float)x;
	TCE_TagMatrixMultiply(first.axis, parent->axis, entity->axis);
	VectorSubtract(otherOrigin, entity->origin, direction);
	VectorNormalize(direction);
	dot = (long double)entity->axis[2][1] * direction[1] +
	      (long double)entity->axis[2][0] * direction[0];
	dot += (long double)entity->axis[2][2] * direction[2];
	angle = (double)atan2l(sqrtl(1.0L - dot * dot), dot);
	RotatePointAroundVector(entity->axis[2], entity->axis[1], entity->axis[2],
	                        (float)((long double)angle * -28.64788975654116));
	CrossProduct(entity->axis[1], entity->axis[2], entity->axis[0]);
}
#endif
