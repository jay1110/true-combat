#ifndef TCE_UI_TEXT_PRECISION_H
#define TCE_UI_TEXT_PRECISION_H

/* Win32 UI400061a4/400061c1 keeps text_x minus integer width through
 * FADD(rect.x), then stores once. Preserve that PC53 store boundary without
 * changing the SDK shared text/render interfaces. Other targets keep the
 * prior Float intermediate used by the reconstructed caller. */
static __inline float TCE_UiDescriptionX(float textX, int width, float originX)
{
#if defined(_WIN32) && !defined(Q3_VM)
	volatile double aligned = (double)textX - width;
	return (float)(aligned + originX);
#else
	float aligned = textX - width;
	return aligned + originX;
#endif
}

#endif
