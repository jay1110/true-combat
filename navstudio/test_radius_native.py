"""Actual C radius parser, native region proof, v9 serializer and Python audit.

Independent flat-floor/side-wall sweeps are not a whole-map or live-bot proof.
No existing project, navigation, game process or installation is modified.
"""
import copy
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from unittest import TestCase, main, skipUnless
from test_doors_native import ROOT, FIXTURE

sys.path.insert(0, str(ROOT / 'tools'))
from tce_navgen import audit_arrival_regions, read_way


def candidate(radii=(150, 300, 150.25), version=5, flags=0, role=0, door=False):
    rows = [f'TCE_NAVMESH {version} obj_radius 3 4']
    for index, (y, radius) in enumerate(zip((0, 768, -768), radii), 1):
        row = f'{index} 0 {y} .25 0 {int(index == 1)} 1'
        if version != 4:
            row += f' {radius}'
            row += ' 0' if version == 3 else f' {flags if index == 1 else 0} 0 {role if index == 1 else 0} 0'
        if version == 6:
            row += (' 65 65 2 1 3 2 1 1 1 1 0 0 0' if door and index == 1 else ' ' + ' '.join(['0'] * 13))
        rows.append(row)
    return '\n'.join(rows + ['1 2', '2 1', '1 3', '3 1']) + '\n'


def program(legacy_writer=False):
    source = (ROOT / 'game/tce_navgen.c').read_text()
    if legacy_writer:
        # Actual former byte-only encoding is a regression control solely
        # for integral radii<=100. It cannot represent the new large/fractional
        # requests and is deliberately never used for those cases.
        source = source.replace('4|(byteRadius?8:16)|(flags?', '4|8|(flags?', 1)
        source = source.replace('if(byteRadius)NG_Byte((unsigned int)radius);else NG_Real(radius);',
                                'NG_Byte((unsigned int)radius);', 1)
    fixture = FIXTURE.split('int main(', 1)[0]
    marker = 'if(scenario==3){VectorSet(lo,-5,-80,0);'
    assert marker in fixture
    fixture = fixture.replace(marker,
        'if(scenario==30){VectorSet(lo,200,-5000,0);VectorSet(hi,208,5000,200);brush(tr,a,mi,ma,b,lo,hi,ENTITYNUM_WORLD);}\n ' + marker, 1)
    return source + fixture + r'''
int main(int argc,char **argv){
 static char text[8192];FILE *file;int i,length,okay;char path[1024];
 file=fopen(argv[1],"rb");REQUIRE(file!=NULL,"candidate input");length=(int)fread(text,1,sizeof(text)-1,file);fclose(file);text[length]=0;
 inputDocument=text;strcpy(level.rawmapname,"obj_radius");g_newbbox.integer=1;NG_ExternalReset();
 okay=NG_ExternalRead("candidate");
 if(!strcmp(argv[3],"reject")){REQUIRE(!okay,"invalid ordinary/action radius rejected before geometry/output");return 0;}
 REQUIRE(okay,"actual radius parser accepts supported contract");
 if(!strcmp(argv[3],"parse"))return 0;
 scenario=!strcmp(argv[3],"wall")?30:0;
 for(i=0;i<ngCount;i++){
  NG_Node(&ngNodes[i]);REQUIRE(ngNodes[i].accepted,"actual native standing floor and capsule");ngNodes[i].index=i;
  ngExternalFloorAccepted[i]=1;ngExternalFloorReason[i]=ngNodes[i].reason;
 }
 ngNodes[0].reason="map_entity_spawn_seed";ngSpawnSeeds=1;
 for(i=0;i<ngEdgeCount;i++){
  REQUIRE(NG_EditorEdge(&ngEdges[i])==NULL,"every actual native directed walk");
  ngEdges[i].accepted=qtrue;ngEdges[i].reason="external_native_validated_walk";
 }
 NG_FinalizeArrivalRadii();NG_SpawnReachability();
 for(i=0;i<ngCount;i++){
  ngArrivalProof *proof=&ngArrivalProofs[i];
  if(!strcmp(argv[3],"auto")){REQUIRE(ngNodes[i].radius==100&&proof->requested==35,"automatic generation retains exact old35/60/80/100 policy");}
  else REQUIRE(ngNodes[i].radius<=ngNodes[i].requestedRadius,"manual request never increased");
  if(scenario==30&&i==1){REQUIRE(ngNodes[i].radius==100&&proof->requested==300,"blocked300 request conservatively falls back to actually proved100");}
  else if(strcmp(argv[3],"auto"))REQUIRE(ngNodes[i].radius==ngNodes[i].requestedRadius,"proved ordinary request preserved exactly");
  REQUIRE(ngNodes[i].radius<=35||proof->verified,"large tolerance has actual expanded hull/grid/perimeter proof");
 }
 REQUIRE(ngArrivalTraces<=NG_ARRIVAL_TRACE_BUDGET&&!ngArrivalBudgetExhausted,"unchanged bounded native arrival budget");
 REQUIRE(NG_Save("radius.way",ngCount),"actual v9 writer");
 snprintf(path,sizeof(path),"%s.way",argv[2]);file=fopen(path,"wb");REQUIRE(file!=NULL,"WAY output");fwrite(output,1,outputLength,file);fclose(file);
 REQUIRE(NG_Report("radius.json","candidate","obj_radius",qtrue),"actual report writer");
 snprintf(path,sizeof(path),"%s.json",argv[2]);file=fopen(path,"wb");REQUIRE(file!=NULL,"report output");fwrite(output,1,outputLength,file);fclose(file);
 puts("actual native radius parser, region proof and v9 roundtrip complete");return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class RadiusNativeTests(TestCase):
    def test_native_ordinary_large_fractional_action_guard_and_region_evidence(self):
        with tempfile.TemporaryDirectory(prefix='tce-radius-native-') as folder:
            d = Path(folder)
            (d / 'main.c').write_text(program(), encoding='utf8')
            (d / 'legacy.c').write_text(program(legacy_writer=True), encoding='utf8')
            sources = ' '.join('"' + (ROOT / 'game' / name).as_posix() + '"' for name in ('q_shared.c', 'q_math.c', 'tce_movement.c'))
            (d / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.15)\nproject(Radius LANGUAGES C)\n'
                'foreach(target radius legacy-radius)\nif(target STREQUAL "radius")\nset(entry main.c)\nelse()\nset(entry legacy.c)\nendif()\n'
                'add_executable(${target} ${entry} ' + sources + ')\ntarget_include_directories(${target} PRIVATE "' + (ROOT / 'game').as_posix() + '")\n'
                'target_compile_options(${target} PRIVATE /Gy)\ntarget_link_options(${target} PRIVATE /OPT:REF)\nendforeach()\n')
            options = dict(capture_output=True, text=True, timeout=90, creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake', '-S', folder, '-B', str(d / 'build'), '-A', 'Win32'],
                            ['cmake', '--build', str(d / 'build'), '--config', 'Release']):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            exe = d / 'build/Release/radius.exe'
            path = d / 'candidate.txt'
            for mode, radii in (('open', (150, 300, 150.25)), ('wall', (150, 300, 150.25)), ('legacy', (35, 35, 35)), ('auto', (35, 35, 35))):
                with self.subTest(mode=mode):
                    path.write_text(candidate(radii, version=4 if mode == 'auto' else 5), encoding='ascii')
                    result = subprocess.run([str(exe), str(path), str(d / mode), mode], **options)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    report = json.loads((d / (mode + '.json')).read_text(encoding='utf8'))
                    nodes, edges = read_way(d / (mode + '.way'))
                    expected = [35, 35, 35] if mode == 'legacy' else [100, 100, 100] if mode == 'auto' else [150, 100 if mode == 'wall' else 300, 150.25]
                    self.assertEqual([n['radius'] for n in nodes], expected)
                    self.assertEqual(edges, {(0, 1), (1, 0), (0, 2), (2, 0)})
                    self.assertEqual(report['arrival_regions']['max_ordinary_radius'], 300)
                    by_id = {n['id']: n for n in report['nodes'] if n['accepted']}
                    self.assertTrue(audit_arrival_regions(report, by_id, nodes)['region_evidence_available'])
                    if mode == 'legacy':
                        report['arrival_regions']['max_ordinary_radius'] = 100
                        self.assertTrue(audit_arrival_regions(report, by_id, nodes)['region_evidence_available'])
                    if mode in ('legacy', 'auto'):
                        result = subprocess.run([str(d / 'build/Release/legacy-radius.exe'), str(path), str(d / (mode + '-old')), mode], **options)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                        self.assertEqual((d / (mode + '.way')).read_bytes(), (d / (mode + '-old.way')).read_bytes())
                    for field, value in (('region_verified', False), ('hull_checks', 0), ('floor_samples', 0), ('transition_checks', 0)):
                        if mode == 'legacy':
                            continue
                        bad = copy.deepcopy(report)
                        bad['nodes'][0]['arrival'][field] = value
                        with self.subTest(missing_evidence=field):
                            with self.assertRaises(ValueError):
                                audit_arrival_regions(bad, {n['id']: n for n in bad['nodes'] if n['accepted']}, nodes)
                    for maximum in (200, 301):
                        bad = copy.deepcopy(report)
                        bad['arrival_regions']['max_ordinary_radius'] = maximum
                        with self.assertRaises(ValueError):
                            audit_arrival_regions(bad, by_id, nodes)
            good = [candidate((150, 300, 100), version=3), candidate(version=6),
                    candidate((100, 35, 35), flags=1 << 27), candidate((100, 35, 35), role=1)]
            bad = [candidate((301, 35, 35)), candidate((9, 35, 35)), candidate(('nan', 35, 35)),
                   candidate((150, 35, 35), flags=1 << 27), candidate((150, 35, 35), role=1),
                   candidate((150, 35, 35), version=6, flags=128, door=True)]
            for accept, docs in ((True, good), (False, bad)):
                for i, document in enumerate(docs):
                    with self.subTest(parser_accept=accept, case=i):
                        path.write_text(document, encoding='ascii')
                        result = subprocess.run([str(exe), str(path), str(d / 'parse'), 'parse' if accept else 'reject'], **options)
                        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    main()
