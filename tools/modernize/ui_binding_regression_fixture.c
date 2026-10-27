#include "src/ui/ui_local.h"
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#undef vsnprintf
int Q_vsnprintf(char *dest, int size, const char *format, va_list args)
{
    return vsnprintf(dest, size, format, args);
}
#endif

displayContextDef_t context, *DC = &context;
qboolean g_waitingForKey;
itemDef_t *g_bindItem;
char g_nameBind1[32], g_nameBind2[32];
static char engineBindings[512][128];
static int writes, restarts, keyQueries, checks, failures;

void QDECL Com_Error(int level, const char *format, ...) { (void)level; (void)format; abort(); }
void QDECL Com_Printf(const char *format, ...) { (void)format; }
static void keys(const char *command, int *first, int *second)
{
    int i;
    ++keyQueries; *first = *second = -1;
    for (i = 0; i < 512; ++i) if (!Q_stricmp(command, engineBindings[i])) {
        if (*first < 0) *first = i;
        else { *second = i; break; }
    }
}
static void binding(int key, const char *command)
{
    if (key < 0 || key >= 512) abort();
    ++writes; Q_strncpyz(engineBindings[key], command, sizeof(engineBindings[key]));
}
static void execute(int mode, const char *text)
{
    if (mode != EXEC_APPEND || strcmp(text, "in_restart\n")) abort();
    ++restarts;
}
static void keyname(int key, char *out, int size)
{
    char name[32]; sprintf(name, "key%d", key); Q_strncpyz(out, name, size);
}
static char *translate(const char *text) { return (char *)text; }
qboolean Rect_ContainsPoint(rectDef_t *rect, float x, float y)
{
    return x > rect->x && x < rect->x + rect->w && y > rect->y && y < rect->y + rect->h;
}

/* PRODUCTION_BINDINGS */

/* PRODUCTION_FUNCTIONS */

#define CHECK(c) do { ++checks; if (!(c)) { ++failures; fprintf(stderr, "line %d: %s\n", __LINE__, #c); } } while (0)
static void custom_keys_preserved(void)
{
    CHECK(!strcmp(engineBindings['n'], "noclip"));
    CHECK(!strcmp(engineBindings['k'], "kill"));
}
static void capture(itemDef_t *item, int key)
{
    CHECK(Item_Bind_HandleKey(item, K_MOUSE1, qtrue));
    CHECK(g_waitingForKey && g_bindItem == item);
    CHECK(Item_Bind_HandleKey(item, key, qtrue));
    CHECK(!g_waitingForKey && !g_bindItem);
}
int main(void)
{
    int id, originalForward, w, i;
    itemDef_t item;
    memset(&item, 0, sizeof(item)); item.cvar = "+freelook";
    item.window.rect.x = item.window.rect.y = 0;
    item.window.rect.w = item.window.rect.h = 100;
    context.cursorx = context.cursory = 50;
    context.getKeysForBinding = keys; context.setBinding = binding;
    context.executeText = execute; context.keynumToStringBuf = keyname;
    context.translateString = translate;
    for(i=0;i<(int)(sizeof(menuBindings)/sizeof(menuBindings[0]));++i)CHECK(BindingIDFromName(menuBindings[i])>=0);
    id = BindingIDFromName("+freelook");
    CHECK(id == 61); CHECK(BindingIDFromName("+FREELOOK") == id);
    CHECK(BindingIDFromName("-freelook") == -1);
    CHECK(g_bindings[id].bind1 == -1 && g_bindings[id].bind2 == -1);
    CHECK(g_bindings[id].defaultbind1_right == -1 && g_bindings[id].defaultbind2_right == -1);
    CHECK(g_bindings[id].defaultbind1_left == -1 && g_bindings[id].defaultbind2_left == -1);
    strcpy(engineBindings['n'], "noclip"); strcpy(engineBindings['k'], "kill");
    strcpy(engineBindings['w'], "+forward");
    Controls_GetConfig(); CHECK(keyQueries == g_bindCount);
    CHECK(!strcmp(BindingFromName("+freelook"), "(?" "?" "?)"));
    CHECK(!Item_Bind_HandleKey(&item, 'p', qfalse)); CHECK(!g_waitingForKey);

    capture(&item, 'p'); CHECK(!strcmp(engineBindings['p'], "+freelook"));
    CHECK(!strcmp(BindingFromName("+freelook"), "KEY112")); custom_keys_preserved();
    Controls_GetConfig(); CHECK(g_bindings[id].bind1 == 'p' && g_bindings[id].bind2 == -1);
    capture(&item, 'o'); CHECK(!strcmp(engineBindings['o'], "+freelook"));
    CHECK(!strcmp(BindingFromName("+freelook"), "KEY111 or KEY112"));
    /* A third choice replaces the previous pair through the production path. */
    capture(&item, 'i'); CHECK(!engineBindings['p'][0] && !engineBindings['o'][0]);
    CHECK(!strcmp(engineBindings['i'], "+freelook")); custom_keys_preserved();

    w = writes;
    CHECK(Item_Bind_HandleKey(&item, K_ENTER, qtrue));
    CHECK(Item_Bind_HandleKey(&item, K_CHAR_FLAG | 'x', qtrue));
    CHECK(g_waitingForKey && writes == w);
    CHECK(Item_Bind_HandleKey(&item, K_ESCAPE, qtrue));
    CHECK(!g_waitingForKey && !g_bindItem && writes == w);

    /* Backspace must clear the real engine state, including both bindings. */
    capture(&item, 'o');
    capture(&item, K_BACKSPACE);
    CHECK(!engineBindings['i'][0] && !engineBindings['o'][0]);
    Controls_GetConfig(); CHECK(g_bindings[id].bind1 == -1 && g_bindings[id].bind2 == -1);
    CHECK(!strcmp(BindingFromName("+freelook"), "(?" "?" "?)"));
    custom_keys_preserved();

    /* Clearing an ordinary action uses the same path; no regression at row 0. */
    originalForward = BindingIDFromName("+forward"); item.cvar = "+forward";
    CHECK(g_bindings[originalForward].bind1 == 'w');
    capture(&item, K_BACKSPACE); CHECK(!engineBindings['w'][0]);
    Controls_GetConfig(); CHECK(g_bindings[originalForward].bind1 == -1);
    custom_keys_preserved();

    for (i = 0; i < 2; ++i) {
        Controls_SetDefaults(i);
        CHECK(g_bindings[id].bind1 == -1 && g_bindings[id].bind2 == -1);
    }
    /* Restore actual keys before saving; no default reset of user bindings. */
    strcpy(engineBindings['p'], "+freelook");
    Controls_GetConfig(); writes = restarts = 0;
    Controls_SetConfig(qfalse); CHECK(writes == 1 && restarts == 0);
    Controls_SetConfig(qtrue); CHECK(writes == 2 && restarts == 1);
    CHECK(!strcmp(engineBindings['p'], "+freelook")); custom_keys_preserved();

    /* The new stats row uses the same actual capture/save/clear path. No
     * default ALT assignment may overwrite an existing user binding. */
    id = BindingIDFromName("+wstats"); CHECK(id >= 0);
    if (id >= 0) {
        CHECK(g_bindings[id].defaultbind1_right == -1 && g_bindings[id].defaultbind1_left == -1);
        CHECK(g_bindings[id].bind1 == -1 && g_bindings[id].bind2 == -1);
        item.cvar = "+wstats"; strcpy(engineBindings[K_ALT], "+zoom");
        Controls_GetConfig(); capture(&item, 'j');
        CHECK(!strcmp(engineBindings['j'], "+wstats")); CHECK(!strcmp(engineBindings[K_ALT], "+zoom"));
        Controls_GetConfig(); CHECK(g_bindings[id].bind1 == 'j');
        capture(&item, K_BACKSPACE); CHECK(!engineBindings['j'][0]);
        CHECK(!strcmp(engineBindings[K_ALT], "+zoom")); custom_keys_preserved();
    }
    id = BindingIDFromName("dropobj");CHECK(id>=0);
    if(id>=0){
        CHECK(g_bindings[id].defaultbind1_right==-1&&g_bindings[id].defaultbind2_right==-1);
        CHECK(g_bindings[id].defaultbind1_left==-1&&g_bindings[id].defaultbind2_left==-1);
        item.cvar="dropobj";Controls_GetConfig();capture(&item,'h');
        CHECK(!strcmp(engineBindings['h'],"dropobj"));custom_keys_preserved();
        Controls_GetConfig();CHECK(g_bindings[id].bind1=='h');
        capture(&item,K_BACKSPACE);CHECK(!engineBindings['h'][0]);custom_keys_preserved();
    }
    printf("%d real UI binding checks, %d failures\n", checks, failures);
    return failures != 0;
}
