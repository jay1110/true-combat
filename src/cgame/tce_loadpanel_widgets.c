#include "cg_local.h"
#include "../ui/ui_shared.h"

const char* CG_LoadPanel_GameTypeName( gametype_t gt ) {
	switch( gt ) {
		case GT_SINGLE_PLAYER:
			return "Single Player";
		case GT_COOP:
			return "Co-op";
		case GT_WOLF:
			return "Reinforced Objective";
		case GT_WOLF_STOPWATCH:
			return "Stopwatch";
		case GT_WOLF_CAMPAIGN:
			return "Campaign";
		case GT_WOLF_LMS:
			return "Objective";
        case 7:
            return "Bodycount";
		default:
			break;
	}

	return "Invalid";
}

void CG_LoadPanel_RenderLoadingBar( panel_button_t* button ) {
	int hunkused, hunkexpected;
	float frac;

	trap_GetHunkData( &hunkused, &hunkexpected );

	if( hunkexpected <= 0 ) {
		return;
	}

	frac = hunkused/(float)hunkexpected;
	if( frac < 0.f ) {
		frac = 0.f;
	}
	if( frac > 1.f ) {
		frac = 1.f;
	}

	CG_DrawPicST( button->rect.x + (1.f - frac) * button->rect.w * .5f, button->rect.y, button->rect.w * frac, button->rect.h, 0, 0, frac, 1, button->hShaderNormal );
}

