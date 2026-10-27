#include <stdio.h>
#include <string.h>

typedef int qboolean;
enum { qtrue = 1 };
typedef struct { struct { const char *name; } window; } menuDef_t;
static menuDef_t focus;
static menuDef_t *focused;
static char *menuSet;
static char loadedSet[1024], activatedName[1024], trace[128];
static int activates, captureName;
static menuDef_t *Menu_GetFocused(void) { strcat(trace, "F"); return focused; }
static char *UI_Cvar_VariableString(const char *name) { (void)name; strcat(trace, "V"); return menuSet; }
static void String_Init(void) { strcat(trace, "S"); focus.window.name = "invalidated"; }
static void UI_ParseGameInfo(const char *name) { (void)name; strcat(trace, "G"); }
static void UI_LoadArenas(void) { strcat(trace, "A"); }
static void UI_LoadMenus(const char *name, qboolean reset) { (void)reset; strcat(trace, "L"); strcpy(loadedSet, name); }
static void Menus_CloseAll(void) { strcat(trace, "C"); }
static menuDef_t *Menus_ActivateByName(const char *name, qboolean modal) {
    (void)modal; ++activates; strcat(trace, "O");
    /* Baseline no-focus calls are counted without dereferencing uninitialized bytes. */
    if (captureName) strcpy(activatedName, name);
    return NULL;
}

/* PRODUCTION_UI_LOAD */

int main(void) {
    int f, s, checks = 0, failures = 0;
    const char *names[] = { NULL, NULL, "", "ingame_main", "missing_menu" };
    char *sets[] = { NULL, "", "ui/menus_gt2.txt" };
    for (f = 0; f < 5; ++f) for (s = 0; s < 3; ++s) {
        const int expected = f >= 3;
        focused = f ? &focus : NULL; focus.window.name = names[f];
        menuSet = sets[s]; trace[0] = loadedSet[0] = activatedName[0] = 0;
        activates = 0; captureName = expected;
        UI_Load();
#define CHECK(c) do { ++checks; if (!(c)) { ++failures; fprintf(stderr, "focus=%d set=%d: %s\n", f, s, #c); } } while (0)
        CHECK(activates == expected);
        CHECK(!strcmp(loadedSet, s == 2 ? sets[s] : "ui/menus.txt"));
        CHECK(!strcmp(trace, expected ? "FVSGALCO" : "FVSGALC"));
        if (expected) CHECK(!strcmp(activatedName, names[f]));
    }
    printf("%d real UI_Load checks, %d failures\n", checks, failures);
    return failures != 0;
}
