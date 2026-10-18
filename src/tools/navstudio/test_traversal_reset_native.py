"""Execute the actual C import reset and Format5 parser without a game process.

The collision engine is not involved: this regression proves that recipes and
report counters from an earlier import cannot survive an authored re-import.
The C functions, structs, dimensions and storage declarations come directly
from production sources. A control executable omitting the new reset block
must fail the identical test, so a vacuous parser-only test cannot pass.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless


ROOT = Path(__file__).resolve().parents[2]
FILES = ('tce_navgen.c', 'tce_navgen_traversal.inc',
         'tce_navgen_external.inc', 'tce_navgen_portals.inc',
         'tce_navgen_simplify.inc', 'tce_navgen_doors.inc', 'tce_navgen_pathrepair.inc')
COUNTERS = (
    'ngMetadataInputCount', 'ngCustomPropertyCount', 'ngCustomPropertyBytes',
    'ngDropProofCount', 'ngJumpProofCount', 'ngLadderProofCount', 'ngFreeClimbProofCount',
    'ngJumpCandidates', 'ngJumpLinks', 'ngLadderCandidates', 'ngLadderLinks',
    'ngJumpPrefilterChecks', 'ngJumpPrefilterRejected', 'ngBspTraversalTraces',
    'ngGenericLadderTraces', 'ngJumpTraversalTraces', 'ngFrontJumpCaseCount',
    'ngDoorCaseCount', 'ngDoorCandidates', 'ngDoorLinks', 'ngDoorTraceStart', 'ngDoorProofTraces',
    'ngFrontJumpTraces', 'ngFrontJumpConnected', 'ngFrontJumpArcChecks',
    'ngFrontJumpNoTraceRejected', 'ngFrontReturnDamageRejected',
    'ngFrontReturnLandingRejected', 'ngLandingProbes', 'ngLandingGrounded',
    'ngLandingSupported', 'ngLandingArcs', 'ngLandingWalks',
    'ngLowerTransferTests', 'ngLowerTransferSuccesses', 'ngDropTests',
    'ngDropAccepted', 'ngBspLadderPairCount', 'ngBspLadderSurfaces',
    'ngSimpleTopologyRejected',
    'ngSpatialChordCount', 'ngSpatialChordChecks',
)
ARRAYS = ('ngJumpProofs', 'ngLadderProofs', 'ngDropProofs', 'ngFreeClimbProofs', 'ngFrontJumpCases',
          'ngLadderFailure', 'ngExitFailures', 'ngTrPairCache', 'ngDoorCases',
          'ngSpatialChords', 'ngSpatialActionNode')


def function(source, name):
    """Extract a complete production function, skipping its forward prototype."""
    match = re.search(r'^static [^\n;]+\b' + re.escape(name) + r'\([^;]*?\)\s*\{',
                      source, re.M)
    if not match:
        raise AssertionError('Actual C function missing: ' + name)
    # These small parser/reset functions have no braces in comments or strings.
    start = match.start()
    opening = source.index('{', match.start())
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def program(control=False):
    sources = {name: (ROOT / 'game' / name).read_text() for name in FILES}
    all_source = '\n'.join(sources.values())
    names = ('NG_TrCacheReset', 'NG_ExternalShortcutsReset', 'NG_SpatialSparseReset', 'NG_PathRepairReset',
             'NG_ExternalJunctionsReset', 'NG_Find', 'NG_ExternalToken',
             'NG_ExternalFlagWord', 'NG_ExternalInt', 'NG_ExternalFloat',
             'NG_UIDHash', 'NG_UIDInsert', 'NG_ExternalHexDigit', 'NG_ExternalTextToken', 'NG_ExternalName',
             'NG_ExternalPropertiesRead', 'NG_ExternalMetadataRead',
             'NG_ExternalReset', 'NG_ExternalRead',
             'NG_TrDropTarget', 'NG_TrStandTarget')
    functions = [function(all_source, name) for name in names]
    if control:
        # Reproduce the old Format5 path: discovery-only state was left intact.
        # The production files themselves are never modified by this control.
        reset = functions[names.index('NG_ExternalReset')]
        beginning = reset.index(' ngDropProofCount=')
        ending = reset.index(' memset(ngNodes,', beginning)
        functions[names.index('NG_ExternalReset')] = reset[:beginning] + reset[ending:]
    needed = (set(re.findall(r'\bng\w+\b', '\n'.join(functions)))
              | set(COUNTERS) | set(ARRAYS) | {'ngFrontJumpStop'})
    # Keep exact production declarations. Extra scalars sharing a declaration
    # are harmless; dimensions resolve through the original #defines below.
    declarations = [line for line in all_source.splitlines()
                    if re.match(r'^static [^;()]+;', line)
                    and needed.intersection(re.findall(r'\bng\w+\b', line))]
    structs = re.findall(r'typedef struct\s*\{[^}]+\}\s*\w+;', all_source, re.S)
    types = set(re.findall(r'^static (\w+) ', '\n'.join(declarations), re.M)) | {'ngDoorRecipe'}
    structures = [text for text in structs if text.rsplit('}', 1)[1].strip(' ;') in types]
    defines = '\n'.join(line for line in all_source.splitlines()
                        if re.match(r'^#define NG_\w+\s', line))
    seed = '\n'.join(f' {name}=73;' for name in COUNTERS)
    seed += '\n' + '\n'.join(f' memset({name},0x6d,sizeof({name}));' for name in ARRAYS)
    check = '\n'.join(f' REQUIRE({name}==0,"{name}");' for name in COUNTERS)
    check += '\n' + '\n'.join(f' REQUIRE(zero_bytes({name},sizeof({name})),"{name}");'
                              for name in ARRAYS)
    return r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
typedef int qboolean,fileHandle_t;
typedef float vec3_t[3];
enum {qfalse=0,qtrue=1,FS_READ=0,MAX_GENTITIES=1024,MAX_QPATH=64,MAX_MODELS=256,MAX_CLIENTS=64,ENTITYNUM_WORLD=1022};
#define Q_stricmp _stricmp
#define VectorCopy(a,b) memcpy((b),(a),sizeof(vec3_t))
#define VectorLengthSquared(a) ((a)[0]*(a)[0]+(a)[1]*(a)[1]+(a)[2]*(a)[2])
struct {int integer;} g_newbbox;
struct {char rawmapname[MAX_QPATH];} level;
static const char *fixture;
static int trap_FS_FOpenFile(const char *path,fileHandle_t *file,int mode) {
 (void)path;(void)mode;*file=1;return (int)strlen(fixture);
}
static void trap_FS_Read(void *out,int size,fileHandle_t file) {
 (void)file;memcpy(out,fixture,(size_t)size);
}
static void trap_FS_FCloseFile(fileHandle_t file) {(void)file;}
static qboolean NG_WorldBounds(void) {return qtrue;}
static void require(int good,const char *name) {
 if(!good){fprintf(stderr,"RESET REGRESSION: %s\n",name);exit(9);}
}
#define REQUIRE(good,name) require((good),(name))
static int zero_bytes(const void *p,size_t size) {
 const unsigned char *bytes=p;size_t i;for(i=0;i<size;i++)if(bytes[i])return 0;return 1;
}
''' + defines + '\n' + '\n'.join(structures + declarations + functions) + r'''
int main(void) {
 int i;
 strcpy(level.rawmapname,"obj_test");g_newbbox.integer=1;
 fixture="TCE_NAVMESH 4 obj_test 3 4\n"
         "1001 0 0 0 0 1 0\n1002 128 0 0 0 0 0\n1003 256 0 0 0 0 0\n"
         "1001 1002\n1002 1001\n1002 1003\n1003 1002\n";
 NG_ExternalReset();REQUIRE(NG_ExternalRead("first"),"Format4 parser");
 REQUIRE(ngCount==3&&ngEdgeCount==4,"first import dimensions");
 REQUIRE(!ngExternalManualDraft&&ngExternalSpatialImport,"Format4 mode");
 for(i=0;i<ngCount;i++)ngNodes[i].accepted=qtrue;
 // A previous discovery produced real index-based action receipts. The next
 // graph reuses array indices for different source IDs, the exact stale risk.
 ngDropProofCount=ngJumpProofCount=ngLadderProofCount=1;
 ngJumpProofs[0].approach=0;ngJumpProofs[0].launch=1;
 ngJumpProofs[0].landing=2;ngJumpProofs[0].standing=qtrue;
 ngDropProofs[0].approach=0;ngDropProofs[0].launch=1;
 ngDropProofs[0].landing=2;ngDropProofs[0].pairedJumpLaunch=0;
 REQUIRE(NG_TrStandTarget(1)==2,"old actual standing recipe active");
 REQUIRE(NG_TrDropTarget(1)==2,"old actual drop recipe active");
''' + seed + r'''
 ngTrCacheFailed=qtrue;
 fixture="TCE_NAVMESH 5 obj_test 3 4\n"
         "2001 0 0 0 0 1 0 35 0 0 0 0\n"
         "2002 128 0 0 0 0 0 60 0 0 0 0\n"
         "2003 256 0 0 0 0 0 80 0 0 0 0\n"
         "2001 2002\n2002 2001\n2002 2003\n2003 2002\n";
 // The actual authored-import entry point resets before reading; Format5
 // intentionally does not run NG_Traversal discovery afterward.
 NG_ExternalReset();
''' + check + r'''
 REQUIRE(!ngTrCacheFailed,"traversal pair cache failure reset");
 REQUIRE(!strcmp(ngFrontJumpStop,"authored_graph_no_rediscovery"),"front stop reason");
 REQUIRE(NG_ExternalRead("second"),"Format5 parser");
 REQUIRE(ngExternalVersion==5&&ngExternalManualDraft&&!ngExternalSpatialImport,"Format5 mode");
 REQUIRE(ngCount==3&&ngEdgeCount==4,"second import dimensions");
 REQUIRE(ngExternalSuppliedCount==3&&ngExternalSuppliedEdges==4,"new input counters");
 for(i=0;i<ngCount;i++){
  REQUIRE(ngNodes[i].id==2001+i&&ngNodes[i].authored,"new authored IDs");
  REQUIRE(ngNodes[i].editFlags==0&&ngNodes[i].editRole==0,"ordinary new metadata");
  ngNodes[i].accepted=qtrue;
 }
 REQUIRE(NG_Find(1001)==-1&&NG_Find(1002)==-1&&NG_Find(1003)==-1,"deleted old IDs absent");
 REQUIRE(NG_TrStandTarget(1)==-1,"no inherited standing jump target");
 REQUIRE(NG_TrDropTarget(1)==-1,"no inherited drop target");
''' + check + r'''
 // A third authored read also starts with empty receipts and fresh counters.
 NG_ExternalReset();REQUIRE(NG_ExternalRead("third"),"repeat Format5 parser");
''' + check + r'''
 printf("Actual C reset: stale receipts, caches and report counters cleared; Format4 -> Format5 -> Format5 passed.\n");
 return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class TraversalResetNativeTests(TestCase):
    def test_authored_reimport_cannot_inherit_discovered_traversal_proofs(self):
        with tempfile.TemporaryDirectory(prefix='tce-reset-regression-') as folder:
            directory = Path(folder)
            (directory / 'main.c').write_text(program())
            (directory / 'control.c').write_text(program(control=True))
            (directory / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.15)\n'
                'project(TraversalResetRegression LANGUAGES C)\n'
                'add_executable(actual-reset main.c)\n'
                'add_executable(stale-control control.c)\n')
            options = dict(capture_output=True, text=True, timeout=90,
                           creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake', '-S', folder, '-B', str(directory / 'build'), '-A', 'Win32'],
                            ['cmake', '--build', str(directory / 'build'), '--config', 'Release']):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            actual = subprocess.run([str(directory / 'build/Release/actual-reset.exe')], **options)
            self.assertEqual(actual.returncode, 0, actual.stdout + actual.stderr)
            self.assertIn('Format4 -> Format5 -> Format5 passed', actual.stdout)
            control = subprocess.run([str(directory / 'build/Release/stale-control.exe')], **options)
            self.assertEqual(control.returncode, 9, control.stdout + control.stderr)
            self.assertIn('ngDropProofCount', control.stderr)


if __name__ == '__main__':
    main()
