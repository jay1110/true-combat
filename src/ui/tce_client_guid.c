/* Client-only etkey support for native ET 2.60b without PunkBuster.
 * ETLegacy clients already supply cl_guid and are left untouched. */
#include "ui_local.h"
#include "../game/tce_guid.h"
#include <stdio.h>

void TCE_InitClientGUID(void) {
    const char *roots[] = {"fs_homepath", "fs_basepath"};
    char guid[64], root[MAX_OSPATH], path[MAX_OSPATH*2];
    unsigned char key[28]; vmCvar_t cvar; FILE *file; int i,n,valid;
    trap_Cvar_VariableStringBuffer("cl_guid",guid,sizeof(guid));
    valid = strlen(guid)==32;
    for(i=0;valid && i<32;i++) if(!((guid[i]>='0' && guid[i]<='9') ||
        (guid[i]>='a' && guid[i]<='f') || (guid[i]>='A' && guid[i]<='F')))valid=0;
    if(valid && strspn(guid,"0")!=32) {
        trap_Cvar_Register(&cvar,"cl_guid",guid,CVAR_USERINFO);
        return;
    }
    /* Never load keys from downloaded PK3s or create/replace a user's key. */
    for(i=0;i<2;i++) {
        trap_Cvar_VariableStringBuffer(roots[i],root,sizeof(root));
        if(!root[0])continue;
        Com_sprintf(path,sizeof(path),"%s/etmain/etkey",root);
        file=fopen(path,"rb");if(!file)continue;
        n=(int)fread(key,1,sizeof(key),file);fclose(file);
        if(!TCE_GUIDFromETKey(key,n,guid)) {
            memset(key,0,sizeof(key));
            trap_Print("TC:E: etmain/etkey is too short; no GUID generated.\n");
            return;
        }
        memset(key,0,sizeof(key));
        trap_Cvar_Register(&cvar,"cl_guid","",CVAR_USERINFO);
        trap_Cvar_Set("cl_guid",guid);
        trap_Print("TC:E: cl_guid initialized from etkey (ETLegacy compatible).\n");
        return;
    }
    trap_Print("TC:E: no etmain/etkey found; GUID-based administration unavailable.\n");
}
