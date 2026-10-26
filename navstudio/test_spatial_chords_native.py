"""Execute production automatic directed-chord pruning on fixed graph fixtures.

These graph-level regressions start with native-accepted walking edges. They
prove reachability, final persistent witnesses and protected traversal/manual
inputs; they do not claim collision or live-map coverage for these fixtures.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless

try:
    from .test_action_anchor_reuse_native import program as action_program
    from .test_traversal_reset_native import function
except ImportError:
    from test_action_anchor_reuse_native import program as action_program
    from test_traversal_reset_native import function

ROOT = (Path(__file__).resolve().parents[1] / "src")


def program():
    source = '\n'.join((ROOT / f'game/{name}').read_text() for name in
                       ('tce_navgen.c', 'tce_navgen_traversal.inc', 'tce_navgen_simplify.inc'))
    prefix = action_program().split('int main(', 1)[0] + '\n#define QDECL\n'
    names = ('NG_SpatialSparseReset', 'NG_SparseCompare', 'NG_SparseDuck',
             'NG_SpatialSparseNode', 'NG_SpatialSparseEdge', 'NG_SpatialSparseActions', 'NG_SpatialSparseWalks')
    functions = [function(source, name) for name in names]
    needed = set(re.findall(r'\bng\w+\b', '\n'.join(functions)))
    declarations = [line for line in source.splitlines()
                    if re.match(r'^static [^;()]+;', line)
                    and needed.intersection(re.findall(r'\bng\w+\b', line))
                    and line not in prefix]
    structures = [text for text in re.findall(r'typedef struct\s*\{[^}]+\}\s*\w+;', source, re.S)
                  if text.rsplit('}', 1)[1].strip(' ;') in ('ngSpatialChord', 'ngJumpProof', 'ngLadderProof')
                  and text not in prefix]
    defines = '\n'.join(line for line in source.splitlines()
                        if re.match(r'^#define NG_SPATIAL_', line))
    return prefix + defines + '\n' + '\n'.join(structures + declarations + functions) + r'''
static void fixture(void) {
 int i;memset(ngNodes,0,sizeof(ngNodes));memset(ngEdges,0,sizeof(ngEdges));
 memset(ngTrPairCache,0,sizeof(ngTrPairCache));NG_SpatialSparseReset();
 ngCount=3;ngEdgeCount=0;ngSparseRemoved=0;ngExternalSpatialImport=1;ngExternalManualDraft=0;
 ngFreeClimbProofCount=ngJumpProofCount=ngDropProofCount=ngLadderProofCount=0;
 for(i=0;i<3;i++){ngNodes[i].accepted=1;ngNodes[i].id=100+i;ngNodes[i].reason="external_native_validated_floor";VectorSet(ngNodes[i].feet,i*64,0,0);}
 NG_TrLink(0,1,"external_native_validated_walk");NG_TrLink(1,2,"external_native_validated_walk");
 NG_TrLink(0,2,"external_native_validated_walk");
}
static void reach(unsigned char out[3][3]) {
 int a,b,k,change;memset(out,0,9);
 for(a=0;a<3;a++)out[a][a]=1;
 for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted)out[ngEdges[k].from][ngEdges[k].to]=1;
 do{change=0;for(a=0;a<3;a++)for(b=0;b<3;b++)for(k=0;k<3;k++)
  if(!out[a][b]&&out[a][k]&&out[k][b]){out[a][b]=1;change=1;}}while(change);
}
int main(void) {
 unsigned char before[3][3],after[3][3];ngNode nodes[3];int i,count,checks;
 fixture();memcpy(nodes,ngNodes,sizeof(nodes));reach(before);NG_SpatialSparseWalks();reach(after);
 REQUIRE(ngSparseRemoved==1&&ngSpatialChordCount==1&&!ngEdges[2].accepted,"one redundant directed chord removed");
 REQUIRE(ngCount==3&&!memcmp(nodes,ngNodes,sizeof(nodes)),"every source point ID/position/flag preserved");
 REQUIRE(!memcmp(before,after,sizeof(before))&&!after[2][0],"directed reachability preserved without invented reverse edges");
 REQUIRE(NG_TrAccepted(0,1)&&NG_TrAccepted(1,2)&&!NG_TrAccepted(0,2),"pair cache matches exact final graph");
 REQUIRE(ngSpatialChords[0].first==0&&ngSpatialChords[0].second==1&&ngEdges[0].accepted&&ngEdges[1].accepted,"persistent two-hop native walk witness recorded");
 count=ngSparseRemoved;NG_SpatialSparseWalks();REQUIRE(ngSparseRemoved==count&&ngSpatialChordCount==1,"idempotent pruning retains witness/count");
 for(i=0;i<14;i++){
  fixture();
  switch(i){
   case 0:ngExternalManualDraft=1;break;
   case 1:ngExternalSpatialImport=0;break;
   case 2:ngNodes[0].authored=1;break;
   case 3:ngNodes[2].navFlags=NG_NAV_CLIMB;break;
   case 4:ngNodes[0].editRole=8;break;
   case 5:ngNodes[1].door.mode=1;break;
   case 6:ngNodes[1].flags=1;break;
   case 7:ngNodes[1].reason="jump_landing_anchor";break;
   case 8:ngEdges[0].reason="ladder_floor_walk_validated_live_pending";break;
   case 9:ngEdges[0].crouch=1;break;
   case 10:ngEdges[1].accepted=0;break;
   case 11:ngEdges[1].from=2;ngEdges[1].to=1;break;
   case 12:ngNodes[1].feet[1]=128;break;
   case 13:ngJumpProofCount=1;ngJumpProofs[0].approach=0;ngJumpProofs[0].launch=1;ngJumpProofs[0].landing=2;break;
  }
  NG_SpatialSparseWalks();REQUIRE(ngEdges[2].accepted&&ngSparseRemoved==0,"manual/action/special/duck/unproved/reversed/long witness guard");
 }
 fixture();NG_TrLink(2,1,"external_native_validated_walk");NG_TrLink(1,0,"external_native_validated_walk");
 NG_TrLink(2,0,"external_native_validated_walk");NG_SpatialSparseWalks();
 REQUIRE(ngSparseRemoved==2&&ngSpatialChordCount==2,"reciprocal chords each have separate directed witnesses");
 for(i=0;i<ngSpatialChordCount;i++)REQUIRE(ngEdges[ngSpatialChords[i].first].accepted&&ngEdges[ngSpatialChords[i].second].accepted,"final witnesses remain accepted");
 fixture();ngSpatialChordChecks=NG_SPATIAL_CHORD_CHECK_LIMIT;checks=ngSpatialChordChecks;NG_SpatialSparseWalks();
 REQUIRE(ngEdges[2].accepted&&ngSparseRemoved==0&&ngSpatialChordChecks==checks,"operation bound leaves unchecked graph intact");
 puts("Format4 directed chord persistence/reachability/manual/action/exact-cache/idempotence/budget controls passed");return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class SpatialChordsNativeTests(TestCase):
    def test_actual_bounded_pruner_preserves_directed_graph_and_action_sources(self):
        with tempfile.TemporaryDirectory(prefix='tce-spatial-chords-') as folder:
            directory = Path(folder)
            (directory / 'main.c').write_text(program())
            (directory / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.15)\nproject(SpatialChords LANGUAGES C)\n'
                'add_executable(spatial-chords main.c)\n')
            options = dict(capture_output=True, text=True, timeout=90,
                           creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake', '-S', folder, '-B', str(directory / 'build'), '-A', 'Win32'],
                            ['cmake', '--build', str(directory / 'build'), '--config', 'Release'],
                            [str(directory / 'build/Release/spatial-chords.exe')]):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    main()
