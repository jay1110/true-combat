/* Windows cgame CG_RegisterItemVisuals:3006f280 / Sounds:30049a40.
 * Full binary comparisons confirm SDK item structures and behavior here.
 */
#include "cg_local.h"
#include <stddef.h>
typedef char tce_item_size_check[sizeof(gitem_t)==0x3c?1:-1];
typedef char tce_item_type_check[offsetof(gitem_t,giType)==0x24?1:-1];
typedef char tce_item_sound_check[offsetof(gitem_t,sounds)==0x38?1:-1];
typedef char tce_item_media_size_check[sizeof(itemInfo_t)==0x20?1:-1];

void CG_RegisterItemVisuals( int itemNum ) {
	itemInfo_t		*itemInfo;
	gitem_t			*item;
	int				i;

	itemInfo = &cg_items[ itemNum ];
	if ( itemInfo->registered ) {
		return;
	}

	item = &bg_itemlist[ itemNum ];

	/* Original clears only the registered word, preserving cached handles. */
	itemInfo->registered = qfalse;

	if( item->giType == IT_WEAPON ) {
		return;
	}

	for(i=0;i<MAX_ITEM_MODELS;i++) {
		itemInfo->models[i] = trap_R_RegisterModel( item->world_model[i] );
	}

	if( item->icon ) {
		itemInfo->icons[0] = trap_R_RegisterShader( item->icon );
		if(item->giType == IT_HOLDABLE)
		{
			// (SA) register alternate icons (since holdables can have multiple uses, they might have different icons to represent how many uses are left)
			for(i=1;i<MAX_ITEM_ICONS;i++)
				itemInfo->icons[i] = trap_R_RegisterShader( va("%s%i", item->icon, i+1) );
		}
	}

	itemInfo->registered = qtrue;	//----(SA)	moved this down after the registerweapon()
}

void CG_RegisterItemSounds( int itemNum ) {
	gitem_t			*item;
	char			data[MAX_QPATH];
	char			*s, *start;
	int				len;

	item = &bg_itemlist[ itemNum ];

	if( item->pickup_sound && *item->pickup_sound ) {
 		trap_S_RegisterSound( item->pickup_sound, qfalse );
	}

	// parse the space seperated precache string for other media
	s = item->sounds;
	if (!s || !s[0])
		return;

	while (*s) {
		start = s;
		while (*s && *s != ' ') {
			s++;
		}

		len = s-start;
		if (len >= MAX_QPATH || len < 5) {
			CG_Error( "PrecacheItem: %s has bad precache string", 
				item->classname);
			return;
		}
		memcpy (data, start, len);
		data[len] = 0;
		if ( *s ) {
			s++;
		}

		if ( !strcmp(data+len-3, "wav" )) {
			trap_S_RegisterSound( data, qfalse );
		}
	}
}
