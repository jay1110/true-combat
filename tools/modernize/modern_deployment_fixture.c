/* Production deployment layout/callbacks and native dispatcher, controlled services. */
#include "src/cgame/cg_local.h"
#include "src/cgame/tce_weapon_media.h"
#include "src/game/tce_bg.h"
#include "src/game/tce_numeric.h"
#include "src/ui/tce_modern_ui.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef vsnprintf

cg_t cg;
cgs_t cgs;
tce_weaponInfo_t tce_cg_weapons[64];
displayContextDef_t cgDC;
displayContextDef_t *DC = &cgDC;
qboolean tce_uiCoordinates;
panel_button_t *bg_focusButton;
team_t teamOrder[3] = { TEAM_ALLIES, TEAM_AXIS, TEAM_SPECTATOR };
static int checks, failures, modern, menuDepth, requests, count = 4, eventType, eventCount;
static int disabled[64];
static char commands[8][256], savedName[256], editName[256];
static int commandCount;
static const char *caseName;
vec4_t tceModernWhite = { 1,1,1,1 }, tceModernMuted = { .5f,.5f,.5f,1 }, tceModernAmber = { 1,.7f,0,1 };
static struct { char value[128]; float x, y; } painted[256];
static int paintedCount;
static int fontRegistrations;
static float registeredFontScale = 1;

#define SOUND_SELECT ((void)0)
#define SOUND_CANCEL ((void)0)
#define SOUND_FOCUS ((void)0)

static void expect(int condition, const char *name)
{
	++checks;
	if (!condition)
	{
		++failures;
		fprintf(stderr, "%s: %s\n", caseName, name);
	}
}
void QDECL Com_sprintf(char *dest, int size, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vsnprintf(dest, (size_t)size, format, args);
	va_end(args);
	if (size > 0) dest[size - 1] = 0;
}
char *QDECL va(const char *format, ...)
{
	static char values[16][2048];
	static unsigned index;
	char *text = values[index++ & 15];
	va_list args;
	va_start(args, format);
	vsnprintf(text, 2048, format, args);
	va_end(args);
	return text;
}
qboolean TCE_ModernUIEnabled(void) { return modern; }
/* Palette behavior is source-bound in modern_theme_regression; this fixture
 * isolates deployment layout/state/input and retains its controlled colors. */
void TCE_ModernThemeColor(const float *source, float *out, int surface)
{ (void)surface;memcpy(out,source,sizeof(vec4_t)); }
/* Actual transform nesting/projection is checked by viewport_regression. */
void CG_MenuViewportBegin(void) { ++menuDepth; }
void CG_MenuViewportEnd(void) { expect(menuDepth>0,"Native menu scope is matched");--menuDepth; }
void trap_SendClientCommand(const char *text)
{
	if (commandCount < 8) Com_sprintf(commands[commandCount++], 256, "%s", text);
}
void trap_Cvar_VariableStringBuffer(const char *name, char *buffer, int size)
{
	expect(!strcmp(name, "limboname"), "name editor retains original limboname cvar");
	Com_sprintf(buffer, size, "%s", editName);
}
void trap_Cvar_Set(const char *name, const char *value)
{
	expect(!strcmp(name, "name"), "name finish writes original player-name cvar");
	Com_sprintf(savedName, sizeof(savedName), "%s", value);
}
qhandle_t trap_R_RegisterShaderNoMip(const char *name) { (void)name; return 1; }
void trap_R_RegisterFont(const char *name, int pointSize, fontInfo_t *font)
{
	expect(!strcmp(name, "tce2_ui/Barlow-Regular") && pointSize == 24,
	       "name editor registers packaged regular Barlow font at native size");
	++fontRegistrations;
	font->glyphScale = registeredFontScale;
}
void CG_EventHandling(int type, qboolean forced)
{
	(void)forced;
	eventType = type;
	++eventCount;
}
void CG_AddPMItemBig(popupMessageBigType_t type, const char *message, qhandle_t shader)
{
	(void)type; (void)message; (void)shader;
}
void CG_LimboPanel_RequestWeaponStats(void) { ++requests; }
qboolean CG_CommandCentreSpawnPointClick(void) { return qfalse; }
static void getKeysForBinding(const char *binding, int *first, int *second)
{ (void)binding; *first = *second = -1; }
int BG_WolfClassToTCE(int playerClass) { return playerClass / 2; }
weaponType_t *WM_FindWeaponTypeForWeapon(weapon_t weapon) { (void)weapon; return NULL; }
int CG_LimboPanel_RenderCounter_ValueForButton(panel_button_t *button) { return button->data[1] + 4; }
void trap_R_SetColor(const float *color) { (void)color; }
void CG_DrawPic(float x, float y, float w, float h, qhandle_t shader)
{ (void)x; (void)y; (void)w; (void)h; (void)shader; }
void TCE_ModernFill(float x, float y, float w, float h, const float *color)
{ (void)x; (void)y; (void)w; (void)h; (void)color; }
void TCE_ModernPanel(float x, float y, float w, float h, float alpha)
{ (void)x; (void)y; (void)w; (void)h; (void)alpha; }
void TCE_ModernImage(const char *name, float x, float y, float w, float h)
{ (void)name; (void)x; (void)y; (void)w; (void)h; }
void TCE_ModernText(float x, float y, float scale, float width, const float *color, const char *text, int strong)
{
	(void)scale; (void)width; (void)color; (void)strong;
	if (paintedCount < 256)
	{
		Com_sprintf(painted[paintedCount].value, 128, "%s", text ? text : "");
		painted[paintedCount].x = x;
		painted[paintedCount++].y = y;
	}
}
int CG_LimboPanel_GetMaxObjectives(void) { return cgs.ccSelectedTeam == 2 ? 0 : 2; }
bg_playerclass_t *CG_LimboPanel_GetPlayerClass(void)
{
	static bg_playerclass_t selected;
	return &selected;
}
int CG_LimboPanel_WeaponCount(void) { return count; }
qboolean CG_LimboPanel_WeaponIsDisabled(int number)
{
	int weapon = number + cgs.ccSelectedWeaponNumber * 10;
	return weapon >= 0 && weapon < 64 && disabled[weapon];
}
qboolean CG_LimboPanel_RealWeaponIsDisabled(weapon_t weapon)
{
	return (int)weapon >= 0 && (int)weapon < 64 && disabled[weapon];
}
weapon_t CG_LimboPanel_GetWeaponForNumber(int number, int slot, qboolean ignoreDisabled)
{
	(void)ignoreDisabled;
	return (weapon_t)(number + slot * 10);
}

/* Paint functions are inert: production button data and handlers remain real. */
#define PAINT(name) void name(panel_button_t *button) { (void)button; }
PAINT(BG_PanelButtonsRender_Img)
PAINT(BG_PanelButtonsRender_Text)
PAINT(BG_PanelButton_RenderEdit)
PAINT(CG_LimboPanel_RenderClassButton)
PAINT(CG_LimboPanel_RenderTeamButton)
PAINT(CG_LimboPanel_RenderHead)
PAINT(CG_LimboPanel_Border_Draw)
PAINT(CG_LimboPanel_WeaponLights)
PAINT(CG_LimboPanel_WeaponPanel)
static void CG_LimboPanel_RenderTeamCounter(panel_button_t *button) { (void)button; }
qboolean BG_PanelButton_EditClick(panel_button_t *button, int key)
{
	if (key != K_MOUSE1) return qfalse;
	BG_PanelButtons_SetFocusButton(button);
	return qtrue;
}

/* ACTUAL_DECLARATIONS */
/* ACTUAL_PANEL */
/* ACTUAL_HANDLERS */
/* ACTUAL_LAYOUT */
/* ACTUAL_INIT */
/* ACTUAL_KEY_FLOW */

static int insideCanvas(rectDef_t rect)
{
	return rect.w > 0 && rect.h > 0 && rect.x >= 0 && rect.y >= 0 &&
	       rect.x + rect.w <= 852.01f && rect.y + rect.h <= 480.01f;
}
static int overlap(rectDef_t a, rectDef_t b)
{
	return a.x < b.x + b.w && a.x + a.w > b.x &&
	       a.y < b.y + b.h && a.y + a.h > b.y;
}
static void cursor(rectDef_t rect)
{
	DC->cursorx = rect.x + rect.w * .5f;
	DC->cursory = rect.y + rect.h * .5f;
}
static qboolean click(panel_button_t *button)
{
	cursor(button->rect);
	return BG_PanelButtonsKeyEvent(K_MOUSE1, qtrue, limboPanelButtons);
}
static void clearServices(void)
{
	requests = commandCount = eventCount = 0;
	eventType = -1;
	BG_PanelButtons_SetFocusButton(NULL);
}
static void selected(int slot, int number)
{
	cgs.ccSelectedWeaponNumber = slot;
	if (slot == 0) cgs.ccSelectedWeapon2 = number;
	else if (slot == 1) cgs.ccSelectedWeapon = number;
	else cgs.ccSelectedWeapon3 = number;
}
static int textExists(const char *text)
{
	int i;
	for (i = 0; i < paintedCount; ++i)
		if (!strcmp(painted[i].value, text)) return 1;
	return 0;
}

int main(void)
{
	panel_button_t baseline[64];
	panel_button_t *classes[] = { &classButton0, &classButton1, &classButton2,
	                             &classButton3, &classButton4, &classButton5 };
	panel_button_t *teams[] = { &teamButton0, &teamButton1, &teamButton2 };
	panel_button_t *slots[] = { &weaponLight2, &weaponLight1, &weaponLight3 };
	int i, j, n = 0, slot, current, target;
	memset(&cg, 0, sizeof(cg));
	memset(&cgs, 0, sizeof(cgs));
	cgDC.getKeysForBinding = getKeysForBinding;
	for (i = 0; i < 64; ++i)
		Com_sprintf(tce_cg_weapons[i].deployMenuShortName, 64, "Weapon%02i", i);
	caseName = "original fallback";
	modern = 0;
	CG_LimboPanel_Init();
	for (i = 0; limboPanelButtons[i]; ++i)
	{
		expect(i < 64, "bounded native button pool");
		baseline[i] = *limboPanelButtons[i];
		++n;
	}
	expect(n == 35, "complete native deployment control pool");
	expect(teamButton0.rect.x == 641 && teamButton0.rect.y == 197,
	       "theme absent retains original team rectangle");
	expect(weaponPanel.rect.x == 642 && weaponPanel.rect.y == 346 && weaponPanel.rect.h == 30,
	       "theme absent retains original loadout rectangle");
	caseName = "modern native control contract";
	nameEditFont.font = &cgs.media.limboFont2;
	nameEditFont.scalex = .22f; nameEditFont.scaley = .24f;
	modern = 1;
	CG_LimboPanel_Init();
	expect(fontRegistrations == 1 && nameEdit.font != &nameEditFont &&
	       nameEdit.font->font == &modernLimboNameFont,
	       "modern name edit uses private registered font without replacing original SDK font");
	expect(nameEdit.font->scalex == nameEditFont.scalex && nameEdit.font->scaley == nameEditFont.scaley,
	       "name edit font clone preserves native input metrics");
	{
		panel_button_text_t originalFont = nameEditFont;
		registeredFontScale = 0;
		TCE_ModernLimboLayout();
		expect(nameEdit.font == &modernLimboNameText && nameEdit.font->font == nameEditFont.font,
		       "missing modern glyphs safely fall back to original font");
		expect(!memcmp(&nameEditFont, &originalFont, sizeof(originalFont)),
		       "private modern font setup never mutates original name editor font");
		registeredFontScale = 1;
		TCE_ModernLimboLayout();
	}
	for (i = 0; i < n; ++i)
	{
		panel_button_t *button = limboPanelButtons[i];
		expect(button->onKeyDown == baseline[i].onKeyDown && button->onKeyUp == baseline[i].onKeyUp,
		       "modern layout retains original native click handlers");
		expect(button->onFinish == baseline[i].onFinish,
		       "modern layout retains name-commit handler");
		expect(!memcmp(button->data, baseline[i].data, sizeof(button->data)),
		       "modern layout retains class/team/slot payloads");
		if (button->onKeyDown)
		{
			expect(insideCanvas(button->rect), "each clickable rectangle is on canvas");
			cursor(button->rect);
			expect(BG_PanelButtonsGetHighlightButton(limboPanelButtons) == button,
			       "native hit testing reaches each modern control");
			for (j = 0; j < i; ++j)
				if (limboPanelButtons[j]->onKeyDown)
					expect(!overlap(button->rect, limboPanelButtons[j]->rect),
					       "closed-state interactive rectangles never overlap");
		}
	}
	expect(teamButton0.rect.w >= 80 && nameEdit.rect.w >= 120,
	       "modern team and name targets remain usable");
	caseName = "team mouse callbacks";
	for (i = 0; i < 3; ++i)
	{
		clearServices();
		cgs.ccSelectedTeam = (i + 1) % 3;
		cgs.ccSelectedObjective = CG_LimboPanel_GetMaxObjectives();
		expect(click(teams[i]), "native mouse dispatcher consumes team click");
		expect(cgs.ccSelectedTeam == i && CG_LimboPanel_GetTeam() == teamOrder[i],
		       "modern card retains original team order");
		expect(cgs.limboLoadoutModified && requests == 1,
		       "team selection still requests stats and marks loadout changed");
	}
	caseName = "class and gear mouse callbacks";
	cgs.ccSelectedTeam = 0;
	for (i = 0; i < 6; ++i)
	{
		clearServices();
		cgs.ccSelectedClass = -1;
		expect(click(classes[i]) && cgs.ccSelectedClass == i && requests == 1,
		       "all six native class identities remain selectable by mouse");
	}
	for (slot = 0; slot < 3; ++slot)
	{
		clearServices();
		cgs.ccSelectedWeaponNumber = -1;
		expect(click(slots[slot]) && cgs.ccSelectedWeaponNumber == slot && requests == 1,
		       "sidearm/primary/special retain independent slot IDs");
	}
	cgs.ccSelectedTeam = 2;
	for (i = 0; i < 6; ++i)
	{
		cgs.ccSelectedClass = 4;
		expect(!click(classes[i]) && cgs.ccSelectedClass == 4,
		       "spectator cannot select a class");
	}
	for (slot = 0; slot < 3; ++slot)
	{
		cgs.ccSelectedWeaponNumber = 1;
		expect(!click(slots[slot]) && cgs.ccSelectedWeaponNumber == 1,
		       "spectator cannot select gear");
	}
	expect(!click(&weaponPanel) && !BG_PanelButtons_GetFocusButton(),
	       "spectator cannot open weapon dropdown");
	caseName = "native dropdown selection and restrictions";
	cgs.ccSelectedTeam = 0;
	for (slot = 0; slot < 3; ++slot)
	for (count = 2; count <= MAX_WEAPS_PER_CLASS; count += 2)
	for (current = 0; current < count; current += count - 1)
	for (target = 0; target < count; ++target)
	{
		rectDef_t row;
		int rowNumber = target < current ? target + 1 : target > current ? target : 0;
		clearServices();
		selected(slot, current);
		TCE_ModernLimboGeometry();
		row = weaponPanel.rect;
		row.y -= rowNumber * row.h;
		expect(insideCanvas(row), "every expanded row remains on screen at maximum native count");
		expect(click(&weaponPanel) && BG_PanelButtons_GetFocusButton() == &weaponPanel,
		       "native press opens and focuses actual modern dropdown");
		paintedCount = 0;
		TCE_ModernLimboWeaponPanel(&weaponPanel, CG_LimboPanel_GetSelectedWeaponForSlot(slot), count);
		/* Locate the actual painted weapon name and put the cursor on that row.
		 * Special card IDs (19/20/21/36) intentionally use the separate loader;
		 * other test rows expose metadata through the real media structure. */
		{
			int weapon = target + slot * 10, matches = 0, p;
			for (p = 0; p < paintedCount; ++p)
				if (!strcmp(painted[p].value, va("Weapon%02i", weapon)))
				{
					float actualRowY = painted[p].y - weaponPanel.rect.h * .68f;
					expect(actualRowY > row.y - .01f && actualRowY < row.y + .01f,
					       "actual modern paint and native release use the same row position");
					++matches;
				}
			if (weapon != 19 && weapon != 20 && weapon != 21 && weapon != 36)
				expect(matches == 1, "each eligible weapon is painted exactly once");
		}
		cursor(row);
		expect(BG_PanelButtonsKeyEvent(K_MOUSE1, qfalse, limboPanelButtons),
		       "native release reaches focused dropdown outside collapsed rectangle");
		expect(CG_LimboPanel_GetSelectedWeaponNum() == target && !BG_PanelButtons_GetFocusButton(),
		       "rendered row order and native release choose the same weapon");
	}
	count = 4;
	selected(1, 0);
	TCE_ModernLimboGeometry();
	clearServices();
	disabled[12] = 1;
	click(&weaponPanel);
	paintedCount = 0;
	TCE_ModernLimboWeaponPanel(&weaponPanel, CG_LimboPanel_GetSelectedWeaponForSlot(1), count);
	expect(textExists("3 / 4  LOCKED"), "restricted choice is visibly labelled LOCKED");
	{
		rectDef_t row = weaponPanel.rect;
		row.y -= 2 * row.h;
		cursor(row);
		BG_PanelButtonsKeyEvent(K_MOUSE1, qfalse, limboPanelButtons);
	}
	expect(CG_LimboPanel_GetSelectedWeaponNum() == 0 && !BG_PanelButtons_GetFocusButton(),
	       "server-restricted loadout remains unselectable");
	disabled[12] = 0;
	click(&weaponPanel);
	DC->cursorx = 1; DC->cursory = 1;
	BG_PanelButtonsKeyEvent(K_MOUSE1, qfalse, limboPanelButtons);
	expect(CG_LimboPanel_GetSelectedWeaponNum() == 0 && !BG_PanelButtons_GetFocusButton(),
	       "release outside choices closes without changing weapon");
	caseName = "public key entry synchronizes modern geometry";
	count = MAX_WEAPS_PER_CLASS; selected(1, 0); clearServices();
	weaponPanel.rect.h = 30;
	cursor(weaponPanel.rect);
	DC->cursory = weaponPanel.rect.y + 2; /* Inside both old and updated row. */
	tce_uiCoordinates = qfalse;
	CG_LimboPanel_KeyHandling(K_MOUSE1, qtrue);
	expect(weaponPanel.rect.h < 13 && BG_PanelButtons_GetFocusButton() == &weaponPanel,
	       "public key path updates row geometry before handling press");
	expect(!tce_uiCoordinates, "public key path restores SDK coordinate state");
	{
		rectDef_t row = weaponPanel.rect;
		row.y -= (count - 1) * row.h;
		cursor(row);
		CG_LimboPanel_KeyHandling(K_MOUSE1, qfalse);
	}
	expect(CG_LimboPanel_GetSelectedWeaponNum() == count - 1 && !BG_PanelButtons_GetFocusButton(),
	       "public key path chooses last visible row through original handler");
	caseName = "rendered spectator and loadout identity";
	paintedCount = 0; cgs.ccSelectedTeam = 2;
	TCE_ModernLimboWeaponPanel(&weaponPanel, WP_NONE, count);
	expect(textExists("SPECTATOR") && paintedCount == 1,
	       "spectator view displays status without offering a weapon");
	paintedCount = 0; cgs.ccSelectedTeam = 1; cgs.ccSelectedClass = 4;
	TCE_ModernLimboRender();
	expect(textExists("SPECOPS") && textExists("TERRORISTS") && textExists("SPECTATOR"),
	       "all original team choices remain labelled in modern deployment");
	expect(textExists("Assault") && textExists("Recon") && textExists("Sniper"),
	       "all original identity roles remain labelled");
	expect(textExists("Primary") && textExists("Sidearm") && textExists("Special"),
	       "all original loadout categories remain labelled");
	caseName = "native apply and cancel";
	clearServices();
	cgs.ccSelectedTeam = 0; cgs.ccSelectedClass = 5;
	cgs.clientinfo[0].team = TEAM_SPECTATOR;
	selected(1, 3); cgs.ccSelectedWeapon2 = 2; cgs.ccSelectedWeapon3 = 1;
	expect(click(&okButton), "apply target reaches native handler");
	expect(commandCount == 1 && !strcmp(commands[0], "team b 5 13 2 21 0\n"),
	       "apply preserves all three weapons and exact native team command");
	expect(eventCount == 1 && eventType == CGAME_EVENT_NONE && cgs.limboLoadoutSelected,
	       "apply closes deployment and retains native loadout state");
	clearServices();
	cgs.ccSelectedTeam = 2; cgs.clientinfo[0].team = TEAM_ALLIES;
	click(&okButton);
	expect(commandCount == 1 && !strcmp(commands[0], "team s 0 0 0 0\n"),
	       "spectator apply uses original team transition command");
	clearServices(); cgs.limboLoadoutModified = 1; cgs.limboLoadoutSelected = 1;
	expect(click(&cancelButton) && commandCount == 1 && !strcmp(commands[0], "rs"),
	       "cancel retains native reset request");
	expect(!cgs.limboLoadoutSelected && eventType == CGAME_EVENT_NONE,
	       "cancel clears selected loadout and closes deployment");
	clearServices(); cgs.limboLoadoutModified = 0;
	click(&cancelButton);
	expect(!commandCount && eventCount == 1, "unmodified cancel does not emit a reset request");
	caseName = "name commit and fallback restoration";
	Com_sprintf(editName, sizeof(editName), "^3Native Player");
	nameEdit.onFinish(&nameEdit);
	expect(!strcmp(savedName, editName), "name input preserves original commit behavior and color codes");
	modern = 0; CG_LimboPanel_Init();
	expect(nameEdit.font == &nameEditFont && nameEdit.font->font == &cgs.media.limboFont2,
	       "marker-disabled reinitialization restores original name font pointer");
	for (i = 0; i < n; ++i)
		expect(!memcmp(&limboPanelButtons[i]->rect, &baseline[i].rect, sizeof(rectDef_t)),
		       "marker-disabled reinitialization restores every original rectangle");
	expect(menuDepth == 0, "all native input paths leave the menu viewport scope");
	printf("%d deployment checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
