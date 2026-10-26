"""Native startup allocation regression for the complete installed menu set.

The inventory is MSVC-preprocessed real VFS menu content captured locally in
build/tce-standard-20261014/editor-startup-preprocessed.txt. The production
allocator, menu/item constructors, type/tooltip allocations, string interning
and MenuParse_itemDef allocation path run unchanged. Parser/render callbacks
are fixtures; this is not a replacement for a visible engine startup test.
"""
import json
import bisect
import os
from pathlib import Path
import re
import shutil
from unittest import TestCase, main, skipUnless
from test_dms_editor_native import compile_program

ROOT=(Path(__file__).resolve().parents[1] / "src")
INVENTORY=ROOT.parent/'build/tce-standard-20261014/editor-startup-preprocessed.txt'


def function(text,name):
    match=re.search(r'^(?:static )?[^\n;]+\b'+re.escape(name)+r'\([^;]*?\)\s*\{',text,re.M)
    if not match:raise AssertionError('Production function missing: '+name)
    opening=text.index('{',match.start());end=opening+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[match.start():end]


def blocks(text,word):
    # Ignore quoted command braces while walking preprocessed native blocks.
    tokens=list(re.finditer(r'"(?:\\.|[^"\\])*"|[A-Za-z_][A-Za-z_0-9]*|[{}]',text))
    for i,t in enumerate(tokens[:-1]):
        if t.group().lower()!=word.lower() or tokens[i+1].group()!='{':continue
        depth=1;end=i+2
        while depth and end<len(tokens):
            token=tokens[end].group();depth+=(token=='{')-(token=='}');end+=1
        if depth:raise AssertionError('Incomplete native '+word+' block')
        yield text[tokens[i+1].end():tokens[end-1].start()]


def program(old_pool=False,old_guard=False):
    source=(ROOT/'ui/ui_shared.c').read_text()
    pool=int(re.search(r'#define MEM_POOL_SIZE\s+(\d+) \* 1024 // Base TCE',source)[1])*1024
    if old_pool:pool=1536*1024
    native='\n'.join(function(source,n) for n in ('UI_Alloc','UI_InitMemory','UI_OutOfMemory',
        'Window_Init','Item_Init','Item_ValidateTypeData','Tooltip_Initialize','Item_ValidateTooltipData','Menu_Init'))
    native=native.replace('void Item_Init(itemDef_t *item)', 'void Actual_Item_Init(itemDef_t *item)',1)
    start=source.index('#define HASH_TABLE_SIZE 2048')
    end=source.index('void String_Report()',start)
    native+='\n'+source[start:end]
    caller=function(source,'MenuParse_itemDef')
    if old_guard:
        guard='\t\tif (!menu->items[menu->itemCount]) {\n\t\t\tPC_SourceError(handle, "UI item pool exhausted; menu was not loaded");\n\t\t\treturn qfalse;\n\t\t}\n'
        assert guard in caller;caller=caller.replace(guard,'',1)
    native+='\n'+caller+'\n'+function(source,'Menu_New')
    data=INVENTORY.read_text(encoding='latin1');menus=list(blocks(data,'menuDef'))
    rows=[];lengths=[]
    for menu in menus:
        items=list(blocks(menu,'itemDef'));lengths.append(len(items))
        for item in items:
            kind=re.search(r'\btype\s+(-?\d+)\b',item)
            validate=bool(kind) or bool(re.search(r'\b(cvar|maxChars|maxPaintChars|model_origin|model_fovx|model_fovy)\b',item))
            rows.append((int(kind[1]) if kind else 0,int(validate),int(bool(re.search(r'\btooltip\b',item,re.I)))))
    # These unique native strings also exercise their allocator-backed nodes.
    strings=sorted(set(re.findall(r'"((?:\\.|[^"\\])*)"',data)))
    # Native PC_Script_Parse interns entire action/execKey scripts, not only
    # their quoted argument tokens. Include their exact formatting footprint.
    script_strings=[]
    script_tokens=list(re.finditer(r'"(?:\\.|[^"\\])*"|[{}]|[^\s{}"]+',data))
    script_starts=[token.start() for token in script_tokens]
    for match in re.finditer(r'\b(?:onOpen|onClose|onEsc|onEnter|onTimeout|action|mouseEnter|mouseExit|mouseEnterText|mouseExitText|onFocus|leaveFocus|execKey)\b[^{}]*\{',data,re.I):
        depth=1;parts=[];start=bisect.bisect_left(script_starts,match.end())
        for lex in script_tokens[start:]:
            token=lex.group();depth+=(token=='{')-(token=='}')
            if not depth:break
            if token.startswith('"'):
                try:token=json.loads(token)
                except ValueError:token=token[1:-1]
            parts.append(('"'+token+'"' if len(token)>1 else token)+' ')
        script_strings.append(''.join(parts))
    strings=sorted(set(strings)|set(script_strings))
    string_literals=','.join(json.dumps(x,ensure_ascii=True) for x in strings)
    header=(ROOT/'ui/ui_shared.h').as_posix()
    return r'''
#define UIDLL
#include "''' + header + r'''"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
displayContextDef_t context={0},*DC=&context;
#define MEM_POOL_SIZE '''+str(pool)+r'''
static char memoryPool[MEM_POOL_SIZE];static int allocPoint,outOfMemory;
menuDef_t Menus[MAX_MENUS];int menuCount=0;
struct Row{int kind,validate,tooltip;};
static const struct Row rows[]={'''+','.join('{'+','.join(map(str,row))+'}' for row in rows)+r'''};
static const int lengths[]={'''+','.join(map(str,lengths))+r'''};
static const char*strings[]={'''+string_literals+r'''};
static int rowIndex,errors,parserCalls,nullInitCalls;
static void printMessage(const char*format,...){}
void PC_SourceError(int handle,const char*format,...){errors++;}
qboolean Item_Parse(int,itemDef_t*item);
void Item_InitControls(itemDef_t*item){}
qboolean Menu_Parse(int,menuDef_t*menu);
void Menu_PostParse(menuDef_t*menu){}
int Q_strlenInt(const char*text){return (int)strlen(text);}
''' + native + r'''
void Item_Init(itemDef_t*item){
 // Catch the old source's NULL call safely; don't manufacture an OS crash.
 if(!item){nullInitCalls++;return;}Actual_Item_Init(item);
}
qboolean Item_Parse(int handle,itemDef_t*item){
 if(!item)return qfalse;
 const struct Row*r=&rows[rowIndex++];parserCalls++;item->type=r->kind;
 if(r->validate)Item_ValidateTypeData(item);
 if(r->tooltip&&!Item_ValidateTooltipData(item))return qfalse;
 return UI_OutOfMemory()?qfalse:qtrue;
}
qboolean Menu_Parse(int handle,menuDef_t*menu){
 int i;for(i=0;i<lengths[menuCount];i++)if(!MenuParse_itemDef((itemDef_t*)menu,handle))return qfalse;
 return qtrue;
}
#define REQUIRE(x,msg) if(!(x)){fprintf(stderr,"%s\n",msg);return 12;}
int main(){
 int i,before;menuDef_t menu={0};itemDef_t edit={0};
 context.Print=printMessage;UI_InitMemory();
 for(i=0;i<(int)(sizeof(strings)/sizeof(strings[0]));i++)REQUIRE(String_Alloc(strings[i])!=NULL,"complete startup string inventory must fit");
 for(i=0;i<(int)(sizeof(lengths)/sizeof(lengths[0]));i++){
  Menu_New(37);REQUIRE(menuCount==i+1,"complete startup menu inventory must fit");
 }
 REQUIRE(!UI_OutOfMemory()&&errors==0,"startup did not exhaust item/type/tooltip pool");
 REQUIRE(parserCalls==(int)(sizeof(rows)/sizeof(rows[0])),"all real asset items reached allocation path");
 printf("Actual native allocation path: %d menus, %d items, item bytes %zu, pool %d / %d.\n",menuCount,parserCalls,sizeof(itemDef_t),allocPoint,MEM_POOL_SIZE);
 // After genuine exhaustion the item parser must not invoke Item_Init(NULL).
 while(UI_Alloc(sizeof(itemDef_t))){}
 before=parserCalls;REQUIRE(!MenuParse_itemDef((itemDef_t*)&menu,37),"exhausted item allocation returns false");
 REQUIRE(parserCalls==before&&menu.itemCount==0,"failed item was never parsed or committed");
 REQUIRE(nullInitCalls==0,"allocation failure must not invoke Item_Init(NULL)");
 while(UI_Alloc(16)){}
 edit.type=ITEM_TYPE_EDITFIELD;Item_ValidateTypeData(&edit);
 REQUIRE(edit.typeData==NULL,"exhausted typed data handled without null memset");
 REQUIRE(String_Alloc("unique string after exhausted allocator")==NULL,"exhausted intern node handled without null write");
 puts("Exhaustion: item, typed data and string nodes rejected safely.");return 0;
}
'''


def voice_program():
    source=(ROOT/'cgame/cg_consolecmds.c').read_text()
    native='\n'.join(function(source,n) for n in ('CG_NavStudioVoiceEditor','CG_QuickMessage_f','CG_QuickFireteamMessage_f','CG_QuickFireteams_f'))
    return r'''
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef int qboolean;enum{qfalse=0,qtrue=1,TEAM_SPECTATOR=3,CGAME_EVENT_NONE=0,CGAME_EVENT_FIRETEAMMSG=1,
 UIMENU_WM_QUICKMESSAGEALT=10,UIMENU_WM_QUICKMESSAGE=11,UIMENU_WM_FTQUICKMESSAGEALT=12,UIMENU_WM_FTQUICKMESSAGE=13};
static struct{int clientNum,showFireteamMenu;}cg;
static struct{struct{int team;}clientinfo[1];int ftMenuMode;}cgs;
static struct{int integer;}cg_quickMessageAlt;
static int local=1,editor=1,cheats=1,popup,event;
static void trap_Cvar_VariableStringBuffer(const char*key,char*out,int){sprintf(out,"%d",!strcmp(key,"sv_running")?local:!strcmp(key,"navstudio_editor")?editor:cheats);}
static void CG_EventHandling(int e,int){event=e;}
static void trap_UI_Popup(int p){popup=p;}
static int CG_IsOnFireteam(int){return 0;}
''' + native + r'''
#define REQUIRE(x,msg) if(!(x)){fprintf(stderr,"%s\n",msg);return 12;}
int main(){
 cgs.clientinfo[0].team=TEAM_SPECTATOR;
 for(int l=0;l<2;l++)for(int e=0;e<2;e++)for(int c=0;c<2;c++)for(int alt=0;alt<2;alt++){
  local=l;editor=e;cheats=c;cg_quickMessageAlt.integer=alt;popup=-1;CG_QuickMessage_f();
  REQUIRE(popup==((l&&e&&c)?(alt?10:11):-1),"spectator voice requires all local-editor-cheat gates");
  popup=-1;CG_QuickFireteams_f();REQUIRE(popup==((l&&e&&c)?(alt?12:13):-1),"spectator fireteam shortcut uses voice editor only locally");
 }
 cgs.clientinfo[0].team=1;local=editor=cheats=0;popup=-1;CG_QuickMessage_f();
 REQUIRE(popup==10,"ordinary playing client's existing voice behavior unchanged");
 puts("Actual native voice callbacks: all 16 spectator gate/style combinations and playing client passed.");return 0;
}
'''


@skipUnless(os.name=='nt' and shutil.which('cmake'),'Windows CMake/MSVC required')
class DmsStartupNativeTests(TestCase):
    @skipUnless(INVENTORY.is_file(),'Full installed VFS preprocessing inventory required')
    def test_full_startup_allocations_and_null_failure_path(self):
        actual=compile_program(self,program(),language='C');self.assertEqual(actual.returncode,0,actual.stdout+actual.stderr)
        print(actual.stdout.strip())
        # This parser fixture does not execute gameinfo/arena/profile bootstrap.
        # Its old-pool result is a capacity measurement, not a claimed replay
        # of the actual UI_Init crash. The real engine reproduction belongs to
        # root's visible startup evidence.
        old_pool=compile_program(self,program(old_pool=True),language='C');self.assertEqual(old_pool.returncode,0,old_pool.stdout+old_pool.stderr)
        print('Old-pool fixture only: '+old_pool.stdout.strip())
        control=compile_program(self,program(old_guard=True),language='C');self.assertEqual(control.returncode,12,control.stdout+control.stderr)
        self.assertIn('must not invoke Item_Init(NULL)',control.stderr)

    def test_spectator_voice_gate_preserves_public_behavior(self):
        result=compile_program(self,voice_program());self.assertEqual(result.returncode,0,result.stdout+result.stderr)


if __name__=='__main__':main()
