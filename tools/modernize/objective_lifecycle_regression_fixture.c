#include "src/game/g_local.h"
#include <setjmp.h>
#undef vsnprintf
int Q_vsnprintf(char *dest, int size, const char *format, va_list args)
{
    return vsnprintf(dest, (size_t)size, format, args);
}

level_locals_t level;
static char configstrings[MAX_CONFIGSTRINGS][MAX_STRING_CHARS];
static jmp_buf fatal;
static int errors, writes, checks, failures;
static char errorText[256];

void QDECL Com_Printf(const char *format, ...) { }
void QDECL Com_Error(int code, const char *format, ...) { abort(); }
void QDECL G_Error(const char *format, ...)
{
    va_list args; va_start(args, format); vsnprintf(errorText, sizeof(errorText), format, args); va_end(args);
    ++errors; longjmp(fatal, 1);
}
void trap_GetConfigstring(int index, char *value, int length)
{
    assert(index >= 0 && index < MAX_CONFIGSTRINGS);
    Q_strncpyz(value, configstrings[index], length);
}
void trap_SetConfigstring(int index, const char *value)
{
    assert(index >= 0 && index < MAX_CONFIGSTRINGS);
    Q_strncpyz(configstrings[index], value, sizeof(configstrings[index])); ++writes;
}

/* PRODUCTION_FUNCTIONS */

static void initializeMap(void)
{
    int i; char cs[MAX_INFO_STRING];
    /* Exact production statements, including the existing configstring reset. */
    /* PRODUCTION_MAP_RESET */
}
static void verify(int condition, const char *message)
{
    ++checks;
    if (!condition && !failures++) fprintf(stderr, "FAIL %s\n", message);
}
static void spawn(gentity_t *entity, int index)
{
    memset(entity, 0, sizeof(*entity));
    entity->message = "Fixture spawn"; entity->count2 = TEAM_AXIS;
    VectorSet(entity->s.origin, (float)index, (float)(-index), (float)(index+50));
    objective_Register(entity);
    verify(entity->count == CS_MULTI_SPAWNTARGETS + index, "map-local configstring index");
    verify(entity->use == team_wolf_objective_use, "objective use callback preserved");
    verify(level.numspawntargets == index+1, "map-local registered count");
    verify(level.spawntargets[index][0] == index, "spawn coordinate stored at map-local index");
    verify(atoi(Info_ValueForKey(configstrings[CS_MULTI_INFO], "numspawntargets")) == index+1,
           "published count matches map-local count");
}
int main(int argc, char **argv)
{
    gentity_t entity; int map, i, before;
    const int counts[] = { 0, 1, 2, 8, MAX_MULTI_SPAWNTARGETS };
    if (argc != 2) return 2;
    if (!strcmp(argv[1], "--overflow")) {
        initializeMap();
        if (setjmp(fatal)) { fprintf(stderr, "FAIL error before single-map limit: %s", errorText); return 1; }
        for (i=0; i<MAX_MULTI_SPAWNTARGETS; ++i) spawn(&entity, i);
        before = writes;
        if (!setjmp(fatal)) { objective_Register(&entity); fprintf(stderr,"FAIL seventeenth objective accepted\n"); return 1; }
        verify(errors == 1 && strstr(errorText,"exceeded MAX_MULTI_SPAWNTARGETS"), "single-map overflow still raises original error");
        verify(writes == before && level.numspawntargets == MAX_MULTI_SPAWNTARGETS, "overflow publishes/writes nothing past limit");
        printf("%d checks: single-map limit %d preserved, objective %d rejected\n",checks,MAX_MULTI_SPAWNTARGETS,MAX_MULTI_SPAWNTARGETS+1);
        return failures ? 1 : 0;
    }
    if (strcmp(argv[1], "--cycle")) return 2;
    if (setjmp(fatal)) { fprintf(stderr,"FAIL accumulated objective count across map init: %s",errorText); return 1; }
    for (map=0; map<1000; ++map) {
        initializeMap(); level.ccLayers=map&1;
        verify(level.numspawntargets == 0, "map initialization clears objective count");
        verify(!strcmp(Info_ValueForKey(configstrings[CS_MULTI_INFO],"numspawntargets"),"0"), "map initialization publishes zero count");
        for(i=0;i<counts[map%5];++i) spawn(&entity,i);
    }
    printf("%d checks: 1000 repeated map initializations, no objective accumulation\n",checks);
    return failures ? 1 : 0;
}
