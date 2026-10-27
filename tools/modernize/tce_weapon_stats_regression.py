"""Bounded actual private TCE weapon counters/request protocol regression."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from compat_validate import msvc_environment
from weapon_regression import function

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--optimized', action='store_true')
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    paths = ['src/game/g_tce_weapon_stats.c', 'src/game/g_tce_weapon_stats.h',
             'src/game/tce_weapon_stats.h', 'src/game/g_weapon.c',
             'src/game/tce_damage_controller.inc', 'src/game/g_match.c', 'src/game/g_main.c',
             'tools/modernize/tce_weapon_stats_fixture.c']
    sources = {p: (ROOT / p).read_text(encoding='utf-8-sig') for p in paths}
    fire = function(sources['src/game/g_weapon.c'], 'FireWeapon')
    begin = fire.index('statsShot = G_TCEWeaponStatsBegin(')
    end = fire.index('G_TCEWeaponStatsEnd(')
    assert fire.index('TCE_LuaWeaponFire(') < begin < fire.index('case 1: Weapon_Knife(') < end
    assert 'return;' not in fire[begin:end]
    damage = function(sources['src/game/tce_damage_controller.inc'], 'TCE_DamageController')
    assert damage.index('TCE_LuaDamage(') < damage.index('headBefore =') < damage.index('G_TCEWeaponStatsDamage(') < damage.index('targ->die(targ,')
    assert 'G_TCEWeaponStatsReset(nClient)' in function(sources['src/game/g_match.c'], 'G_deleteStats')
    assert 'G_TCEWeaponStatsRequest(ent)' in function(sources['src/game/g_match.c'], 'G_statsPrint')
    assert 'G_TCEWeaponStatsResetAll();' in function(sources['src/game/g_main.c'], 'G_InitGame')
    # The SDK's g_local.h has no include guard; the fixture already read it.
    module = sources[paths[0]].replace('#include "g_local.h"\n', '', 1)
    generated = sources[paths[-1]].replace('/* ACTUAL_MODULE */', module)
    source = out / 'fixture.c'; source.write_text(generated, encoding='utf-8', newline='\n')
    env, cc = msvc_environment(out, 'x64' if args.sanitize else 'x86', '14.44')
    exe = out / 'fixture.exe'
    command = [cc, '/nologo', '/TC', '/O2' if args.optimized else '/Od', '/DGAMEDLL', '/DWIN32', '/D_CRT_SECURE_NO_WARNINGS',
               '/I' + str(ROOT), '/I' + str(ROOT / 'src/game'), str(source),
               str(ROOT / 'src/game/q_shared.c'), '/Fe:' + str(exe)]
    if args.sanitize:
        command[1:1] = ['/fsanitize=address', '/Z7']
        command += ['/link', '/DEBUG']
    compiled = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
    (out / 'compile.log').write_text(compiled.stdout + compiled.stderr, encoding='utf-8')
    report = dict(source_hashes={p: hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},
                  generated_sha256=hashlib.sha256(generated.encode()).hexdigest(), command=command,
                  build_returncode=compiled.returncode,
                  limits=['Actual private counter/request bodies and real SDK structs; controlled accepted-fire/damage observations.',
                          'Production hook ordering/reset dispatch checked from actual function bodies; separate full bullet regression covers damage semantics.',
                          'No game transport, live weapon capture or native Linux ABI claim.'])
    if compiled.returncode:
        print(compiled.stdout + compiled.stderr)
    else:
        run = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True)
        report.update(returncode=run.returncode, stdout=run.stdout, stderr=run.stderr)
        print(run.stdout + run.stderr, end='')
    (out/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return int(bool(compiled.returncode or report.get('returncode')))


if __name__ == '__main__':
    raise SystemExit(main())
