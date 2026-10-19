#ifndef TCE_NAV_LADDER_H
#define TCE_NAV_LADDER_H

/* The same request classification is used by the native traversal model and
 * the Omni usercmd adapter. On a ladder, walking toward its face means climb
 * upward; horizontal forward motion only starts after contact is lost.
 * Sideways and outward dismounts retain their separate existing handling. */
static int TCE_NavLadderForwardExit(float dx, float dy, float nx, float ny)
{
	float length2 = dx * dx + dy * dy, tangent = ny * dx - nx * dy;
	return length2 > .000001f && -(nx * dx + ny * dy) > 0.f && tangent * tangent <= .25f * length2;
}

#endif
