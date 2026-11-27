#ifndef TCE_PMEXT_EXTENSION_TEST_H
#define TCE_PMEXT_EXTENSION_TEST_H

#include <stddef.h>
#include <string.h>

/* The original DLL has 112 bytes. The port additionally resets its own
 * freelook, reload and weapon-wall prediction history on restart/respawn. */
static int TCE_AcceptedPmextLayout(void)
{
	return offsetof(pmoveExt_t, tceFreelookRecoil) == 112 &&
	       sizeof(((pmoveExt_t *)0)->tceFreelookRecoil) == 8 &&
	       offsetof(pmoveExt_t, tceReloadWeapon) == 120 &&
	       offsetof(pmoveExt_t, tceReloadRequestedAt) == 124 &&
	       offsetof(pmoveExt_t, tceWeaponWallAmount) == 128 &&
	       offsetof(pmoveExt_t, tceWeaponWallWeapon) == 132 &&
	       offsetof(pmoveExt_t, tceWeaponWallProneYaw) == 136 &&
	       offsetof(pmoveExt_t, tceWeaponWallProneAnchorYaw) == 140 &&
	       offsetof(pmoveExt_t, tceWeaponWallProneSide) == 144 &&
	       offsetof(pmoveExt_t, tceWeaponWallProneTurning) == 148 &&
	       sizeof(pmoveExt_t) == 152;
}

static int TCE_PmextExtensionCleared(const pmoveExt_t *ext)
{
	const size_t start = offsetof(pmoveExt_t, tceFreelookRecoil);
	static const unsigned char zero[sizeof(pmoveExt_t)] = {0};
	return memcmp((const unsigned char *)ext + start, zero, sizeof(*ext) - start) == 0;
}

#endif
