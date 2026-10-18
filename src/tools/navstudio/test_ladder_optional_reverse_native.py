"""Execute the production ladder commit and real NG_Walk optional-edge gate.

Fixed prevalidated ascent recipes isolate graph commitment; this test does not
claim to discover or live-test a ladder. Collision/support of the optional
reverse uses the actual C walk against independent floor/ledge fixtures.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
from test_generator_capsule_support_native import program as support_program
from test_traversal_reset_native import function
ROOT=Path(__file__).resolve().parents[2]


def program():
    source=(ROOT/'game/tce_navgen.c').read_text()+'\n'+(ROOT/'game/tce_navgen_traversal.inc').read_text()
    prefix=support_program().split('int main(',1)[0]
    structs=[s for s in re.findall(r'typedef struct\s*\{[^}]+\}\s*\w+;',source,re.S)
             if s.rsplit('}',1)[1].strip(' ;') in ('ngEdge','ngLadderProof')]
    add=function(source,'NG_TrAddLadder')
    needed=set(re.findall(r'\bng\w+\b',add))
    declarations=[line for line in source.splitlines() if re.match(r'^static [^;()]+;',line)
                  and needed.intersection(re.findall(r'\bng\w+\b',line)) and line not in prefix]
    # Execute the exact production JSON statement as well as the commit.
    start=source.index('NG_Text(va("%s{\\"kind\\":\\"ladder\\"')
    report=source[start:source.index('));',start)+3]
    return prefix+'\n#define NG_NAV_CLIMB (1u<<9)\n'+'\n'.join(structs+declarations)+r'''
// Discovery is outside this commit regression. Supply one unchanged stored
// recipe; optional reverse collision is NOT stubbed and executes NG_Walk.
static qboolean NG_TrSpecialInput(int a,int b){return qfalse;}
static int NG_TrReuseOrdinaryFloor(const ngNode *p){return -1;}
static qboolean NG_TrLadder(int i,int j,ngNode *bottom,ngNode *top,qboolean *reverse){
 *bottom=ngNodes[i];*top=ngNodes[j];top->feet[0]=0;
 bottom->navFlags=top->navFlags=NG_NAV_CLIMB;
 memset(&ngLadderProofWorking,0,sizeof(ngLadderProofWorking));
 ngLadderProofWorking.ballistic=qtrue;
 ngLadderProofWorking.ascentRelease=ngLadderProofWorking.ascentAlignment=-1;
 VectorCopy(ngNodes[j].feet,ngLadderProofWorking.landing);
 VectorCopy(top->feet,ngLadderProofWorking.launch);ngLadderProofWorking.launch[1]+=24.f;
 *reverse=qtrue;return qtrue;
}
static void NG_TrLink(int from,int to,const char *reason){
 ngEdge *edge=&ngEdges[ngEdgeCount++];memset(edge,0,sizeof(*edge));
 edge->from=from;edge->to=to;edge->accepted=qtrue;edge->reason=reason;
}
'''+add+r'''
static char receipt[4096];
static const char *va(const char *format,...){
 static char formatted[4096];va_list args;va_start(args,format);
 vsnprintf(formatted,sizeof(formatted),format,args);va_end(args);return formatted;
}
static void NG_Text(const char *text){strcpy(receipt,text);}
static void report_ladder(const ngLadderProof *p){int i=0;qboolean exported=qtrue;
'''+report+r'''
}
static int linked(int a,int b){int k;for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].from==a&&ngEdges[k].to==b&&ngEdges[k].accepted)return 1;return 0;}
int main(int argc,char **argv){
 int maxid=2,top,exit,inside,release,k,expected;ngNode saved[2];
 scenario=argc>1?atoi(argv[1]):0;expected=scenario==0;
 g_newbbox.integer=1;ngWidth=16;ngStand=87;ngDuck=61;ngOrigin=30;ngTraceBudget=2000000;
 ngCount=2;ngNodes[0].id=1;ngNodes[1].id=2;
 ngNodes[0].accepted=ngNodes[1].accepted=qtrue;
 ngNodes[0].reason=ngNodes[1].reason="input_anchor";
 VectorSet(ngNodes[0].feet,48,24,.125f);VectorSet(ngNodes[1].feet,64,0,scenario==5?40.125f:.125f);
 saved[0]=ngNodes[0];saved[1]=ngNodes[1];
 NG_TrAddLadder(0,1,&maxid);
 REQUIRE(ngLadderProofCount==1&&ngCount==7,"all four guides and separate ascent release retained");
 top=ngLadderProofs[0].node[2];exit=ngLadderProofs[0].node[3];inside=ngLadderProofs[0].node[1];release=ngLadderProofs[0].ascentRelease;
 REQUIRE(linked(top,exit)==expected,"only freshly supported optional reverse committed");
 REQUIRE(ngLadderProofs[0].liftedExit==expected,"explicit optional direction matches its actual full capsule proof");
 report_ladder(&ngLadderProofs[0]);
 REQUIRE(strstr(receipt,expected?"\"lifted_exit\":true,":"\"lifted_exit\":false,")!=NULL,"actual native JSON preserves exact proved optional direction");
 REQUIRE(linked(exit,top)&&linked(top,inside),"Exit to Top to Interior descent unchanged");
 REQUIRE(linked(ngLadderProofs[0].node[0],release)&&linked(release,exit),"independent ascent and ballistic dismount unchanged");
 REQUIRE(linked(exit,1)&&linked(1,exit),"existing upper anchor remains connected both directions");
 REQUIRE(ngEdgeCount==10+expected&&ngLadderLinks==ngEdgeCount,"reported links count only committed directions");
 for(k=0;k<2;k++)REQUIRE(ngNodes[k].id==saved[k].id&&VectorCompare(ngNodes[k].feet,saved[k].feet),"source IDs and coordinates preserved");
 REQUIRE(body_calls>0,"optional reverse executed actual full capsule walk");
 printf("scenario%d optional%d nodes%d edges%d bodyqueries%d\n",scenario,expected,ngCount,ngEdgeCount,body_calls);return 0;
}
'''


@skipUnless(os.name=='nt' and shutil.which('cmake'),'Windows CMake/MSVC required')
class LadderOptionalReverseNativeTests(TestCase):
    def test_optional_reverse_full_capsule_proof_and_routes(self):
        with tempfile.TemporaryDirectory(prefix='tce-ladder-reverse-') as folder:
            directory=Path(folder)
            (directory/'main.c').write_text(program())
            (directory/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.15)\nproject(LadderReverse LANGUAGES C)\nadd_executable(reverse main.c)\n')
            options=dict(capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
            for cmd in (['cmake','-S',folder,'-B',str(directory/'build'),'-A','Win32'],['cmake','--build',str(directory/'build'),'--config','Release']):
                result=subprocess.run(cmd,**options);self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            for scenario in (0,6,5):
                with self.subTest(scenario=scenario):
                    result=subprocess.run([str(directory/'build/Release/reverse.exe'),str(scenario)],**options)
                    self.assertEqual(result.returncode,0,result.stdout+result.stderr)


if __name__=='__main__':main()
