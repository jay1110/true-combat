"""Synthetic ABI/resource closure tests; carriers are not executable modules."""
from contextlib import contextmanager
import hashlib
import io
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

import client_packages as clients
import single_mod
from resource_assets import resource_assets, resource_fingerprint, resource_paths, package_directory
from package import version, package_modules
from test_loose_binaries import binary_header, CORE_NAMES
from install_mod import install
import source_archive

VERSION = version()


def mini_editor_assets():
    """A real, closed Editor menu graph for synthetic ABI carrier tests."""
    goals=('tce_ob_goalname','tce_ob_groupname','tce_ob_waypointname','tce_ob_value')
    voices=('wm_quickmessage','wm_quickmessageAlt','wm_ftquickmessage','wm_ftquickmessageAlt')
    files={'ui/tce_omnibot.menu':b'menuDef { name "tce_omnibot" itemDef { action { open tce_omnibot_dms ; } } }',
           'ui/tce_omnibot_dms.menu':('menuDef { name "tce_omnibot_dms" itemDef { action { '+
               ' '.join('open '+goal+' ;' for goal in goals)+' } } }').encode()}
    files.update({'ui/'+goal+'.menu':('menuDef { name "'+goal+'" }').encode() for goal in goals})
    files.update({'ui/'+voice+'.menu':('menuDef { name "'+voice+'" itemDef { action { exec "omnibot_menu" ; } } }').encode()
                  for voice in voices})
    registry='\n'.join('loadMenu { "'+name+'" }' for name in ['ui/main.menu',*files]).encode()
    files.update({'ui/'+name:registry for name in ('menus.txt','menus_gt2.txt','menus_gt5.txt','menus_gt7.txt')})
    return files


def fixture_repo(root):
    root = Path(root)
    files = {'pak/ui/main.menu': b'public main menu', 'pak/ui/menus.txt': b'load main',
             'pak/models/player.md3': b'model', 'pak/sound/weapon.wav': b'sound',
             'pak/scripts/level.script': b'map script', 'pak/fonts/ui_16.dat': b'font',
             'pak/textures/stone.tga': b'pixels', 'pak/ui_obj/deployment.menu': b'objective menu',
             'src/game/tce_version.h': ('#define TCE_VERSION "' + VERSION + '"\n').encode()}
    files.update({'navstudio/editor-pak/'+name:data for name,data in mini_editor_assets().items()})
    for mapname in ('delta', 'northport', 'railhouse', 'snow', 'stadtrand', 'village'):
        for suffix in ('_cc.tga', '_cc_blueprint.tga'):
            files['pak/levelshots/obj_' + mapname + suffix] = (mapname + suffix).encode()
    for name, data in files.items():
        path = root/name;path.parent.mkdir(parents=True, exist_ok=True);path.write_bytes(data)
    for name in ('targets.json', 'install_mod.py'):
        path=root/'tools/release'/name;path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(clients.ROOT/'tools/release'/name,path)
    for name in ('start_game.py', 'start_game.ps1', 'start_game.sh', 'README.md'):
        path=root/'tools/launcher'/name;path.parent.mkdir(parents=True, exist_ok=True);path.write_bytes(b'fixture launcher')
    policy=root/'tools/release/runtime-policy.json';policy.write_text(json.dumps({'editor_files': []}))
    stock_data=b'original stock resource, absent from every shipped mod delta'
    stock={'schema':1,'release_version':VERSION,'baseline_id':'tce-0.49b-pak0-4',
           'files':{'textures/base.tga':{'sha256':clients.sha(stock_data),'bytes':len(stock_data),'source_package':'pak0.pk3'}},
           'source_packages':[{'name':f'pak{i}.pk3','bytes':len(('synthetic stock '+str(i)).encode()),
                               'sha256':clients.sha(('synthetic stock '+str(i)).encode())} for i in range(5)]}
    baseline=root/'tools/assets/base-game-reference.json';baseline.parent.mkdir(parents=True,exist_ok=True)
    baseline.write_text(json.dumps(stock))
    (baseline.parent/'base-ui-reference.json').write_text(json.dumps({'schema':1,'release_version':VERSION,
        'baseline_id':stock['baseline_id'],'files':{'ui/base_shared.menu':{'sha256':clients.sha(b'stock menu'),
        'bytes':10,'menus':[{'name':'base_shared','opens':[]}]}}}))
    for name in ('LICENSE','EULA_TrueCombat_Elite.txt','vendor/dependencies.lock.json',
                 'vendor/lua/IMPORT-NOTICE.txt','vendor/lua/lua.h','vendor/sqlite/IMPORT-NOTICE.txt',
                 'vendor/luasql/license.html'):
        path=root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(b'fixture license')
    (root/'src/lua').mkdir(parents=True);(root/'src/lua/config.lua').write_bytes(b'fixture = true')
    return root


def runtime_payload():
    return {'global_scripts/fixture.gm': b'// synthetic script closure only\n'}


@contextmanager
def synthetic_runtime():
    def check_core(boundary, data):
        if boundary['sha256'] != clients.sha(data):
            raise ValueError('Synthetic core hash mismatch')
    with patch.object(single_mod, 'prepare_runtime', return_value=(runtime_payload(), {}, {}, {}, {})), \
            patch.object(single_mod, 'validate_symbol_evidence', side_effect=check_core):
        yield


def input_archive(folder, target, variant='game', overrides=None, configuration='Release', repo=clients.ROOT):
    folder.mkdir(parents=True, exist_ok=True)
    payload = {name: binary_header(target) + (target+'/'+variant+'/'+role).encode()
               for role,name in clients.module_names(target, repo).items()}
    payload.update(overrides or {})
    files={'tce2/'+name:data for name,data in payload.items()}
    config=clients.target_table(repo)[target]
    files['tce2/'+config.get('prefix','')+'qagame'+config['suffix']]=binary_header(target)+b'qagame'
    files.update({clients.INPUT_PREFIX+name:data for name,data in payload.items()})
    core=binary_header(target)+b'\0'.join(s.encode() for s in single_mod.RUNTIME_BINDINGS)+b'\0'
    if variant=='navstudio':core+=b'ExportClientFunctionsFromDLL\0'
    core_name=CORE_NAMES[target];files['tce2/omni-bot/'+core_name]=core
    script_files=[]
    for relative,data in runtime_payload().items():
        files['tce2/omni-bot/'+relative]=data
        script_files.append({'path':relative,'sha256':clients.sha(data)})
    files['tce2/omni-bot/runtime-manifest.json']=clients.json_bytes({'schema':2,'files':script_files,
        'mod_files':[],'command_registry':{},'diagnostics':{},
        'policy_sha256':clients.sha((repo/'tools/release/runtime-policy.json').read_bytes())})
    metadata={'schema':2,'target':target,'version':VERSION,'variant':variant,
              'configuration':configuration,'authoring':variant=='navstudio',
              'resource_tree':resource_fingerprint(variant,repo),
              'client_package':{'scope':'target-input','targets':[target]},
              'gameplay_bot_runtime':True,'bot_core_boundary':{'library':core_name,'variant':variant,
                  'sha256':clients.sha(core),'navigation_diagnostic_formats':{'TCE2PATH':False}},
              'files':[{'path':name,'sha256':clients.sha(data),'bytes':len(data)} for name,data in sorted(files.items())]}
    files['manifest.json']=clients.json_bytes(metadata)
    path=folder/f'TCE2-{VERSION}-{target}-{variant}-Client-Inputs.zip';path.write_bytes(clients.zip_bytes(files));return path


class SharedClients(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory();self.addCleanup(self.temporary.cleanup)
        self.root=Path(self.temporary.name);self.repo=fixture_repo(self.root/'repo')

    def row(self,target,variant='game',**kwargs):
        path=input_archive(self.root/'inputs'/(target+'-'+variant),target,variant,repo=self.repo,**kwargs)
        return clients.read_target(path,VERSION,self.repo)

    def test_every_resource_and_all_twelve_stock_compass_images_are_included(self):
        assets=clients.client_assets(VERSION,'game',self.repo)
        self.assertEqual(set(assets)-{f'tce2/{VERSION}/client.manifest',f'tce2/{VERSION}/base-game.json'},set(resource_paths('game',self.repo)))
        self.assertEqual(len([name for name in assets if name.startswith('levelshots/')]),12)
        self.assertIn('models/player.md3',assets);self.assertIn('sound/weapon.wav',assets)

    def test_editor_overlay_stays_out_of_public_variant(self):
        game=resource_assets(VERSION,'game',self.repo);editor=resource_assets(VERSION,'navstudio',self.repo)
        self.assertNotIn('ui/tce_omnibot.menu',game);self.assertIn('ui/tce_omnibot.menu',editor)
        self.assertNotEqual(game['ui/menus.txt'],editor['ui/menus.txt'])
        self.assertEqual(game['models/player.md3'],editor['models/player.md3'])

    def test_new_file_is_included_without_whitelist_change_and_invalidates_transfer(self):
        row=self.row('windows-x86');old=resource_fingerprint('game',self.repo)
        (self.repo/'pak/new_asset.cfg').write_bytes(b'new asset')
        self.assertNotEqual(resource_fingerprint('game',self.repo),old)
        self.assertIn('new_asset.cfg',resource_assets(VERSION,'game',self.repo))
        with self.assertRaisesRegex(ValueError,'stale mod delta'):
            clients.read_target(row['path'],VERSION,self.repo)

    def test_assets_have_no_src_assets_fallback(self):
        shutil.rmtree(self.repo/'pak')
        (self.repo/'src/assets').mkdir(parents=True);(self.repo/'src/assets/legacy.tga').write_bytes(b'stale')
        with self.assertRaisesRegex(ValueError,'Missing mod delta resource'):
            resource_assets(VERSION,'game',self.repo)

    def test_clean_checkout_has_no_original_dependency_or_implicit_original_fallback(self):
        self.assertFalse((self.repo/'pak_original_tce').exists())
        baseline=resource_fingerprint('game',self.repo)
        expected=resource_assets(VERSION,'game',self.repo)
        original=self.repo/'pak_original_tce/private/original.tga'
        original.parent.mkdir(parents=True);original.write_bytes(b'local original resource')
        self.assertEqual(resource_fingerprint('game',self.repo),baseline)
        self.assertEqual(resource_assets(VERSION,'game',self.repo),expected)
        self.assertNotIn('private/original.tga',expected)
        shutil.rmtree(self.repo/'pak_original_tce')
        modules=self.root/'clean-native';modules.mkdir()
        for name in clients.module_names('windows-x86').values():(modules/name).write_bytes(binary_header('windows-x86'))
        package_directory(modules,'game',modules/'clean.pk3',self.repo)
        self.assertEqual(set(clients.zip_contents((modules/'clean.pk3').read_bytes())),
                         set(expected)|set(clients.module_names('windows-x86').values()))

    def test_unchanged_stock_is_rejected_case_insensitively_but_changed_asset_is_kept(self):
        path=self.repo/'pak/textures/BASE.TGA'
        path.write_bytes(b'original stock resource, absent from every shipped mod delta')
        with self.assertRaisesRegex(ValueError,'Unchanged original TCE resource'):
            resource_assets(VERSION,'game',self.repo)
        path.write_bytes(b'changed project texture')
        self.assertEqual(resource_assets(VERSION,'game',self.repo)['textures/BASE.TGA'],b'changed project texture')

    def test_missing_or_wrong_version_stock_signatures_never_load_local_original(self):
        path=self.repo/'tools/assets/base-game-reference.json'
        reference=json.loads(path.read_bytes());reference['release_version']='0.0.0';path.write_text(json.dumps(reference))
        with self.assertRaisesRegex(ValueError,'version-bound stock'):
            resource_paths('game',self.repo)
        path.unlink()
        with self.assertRaisesRegex(ValueError,'checked-in stock'):
            resource_paths('game',self.repo)

    def test_source_zip_excludes_original_even_if_stale_git_index_lists_it(self):
        for name,data in {'tools/release/build.py':b'# tiny build input',
                          'vendor/omni-bot/runtime/SOURCE.json':b'{}','map_pics/new.png':b'new image',
                          'pak_original_tce/ui/base.menu':b'private original menu',
                          'pak_original_tce/base.pk3':b'private original archive',
                          'pak_original_tce/.DS_Store':b'private original metadata'}.items():
            path=self.repo/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        names=[path.relative_to(self.repo).as_posix() for path in self.repo.rglob('*') if path.is_file()]
        with patch.object(source_archive,'ROOT',self.repo),patch.object(sys,'argv',['source','--out',str(self.root/'source-out'),'--include-untracked']), \
                patch.object(source_archive.subprocess,'check_output',return_value=('\0'.join(names)+'\0').encode()):
            source_archive.main()
        with zipfile.ZipFile(self.root/'source-out'/f'TCE2-{VERSION}-Source.zip') as archive:
            self.assertFalse(any(name.startswith('pak_original_tce/') for name in archive.namelist()))
            self.assertEqual(archive.read('pak/models/player.md3'),b'model')
            self.assertIn('tools/assets/base-game-reference.json',archive.namelist())

    def test_resource_source_rejects_native_nested_archives_and_generated_manifests(self):
        for name in ('ui_mp_x86.dll','base.pk3','assets.zip','client.manifest','client-abis.json','lib.so.1'):
            path=self.repo/'pak'/name;path.write_bytes(b'invalid')
            with self.subTest(name=name),self.assertRaises(ValueError):resource_paths('game',self.repo)
            path.unlink()
        path=self.repo/'pak/ui/tce_omnibot.menu';path.write_bytes(b'editor')
        with self.assertRaisesRegex(ValueError,'Editor menu'):resource_paths('game',self.repo)

    def test_every_matrix_client_abi_round_trips_and_zip_order_is_deterministic(self):
        rows=[self.row(target) for target in clients.target_table(self.repo)]
        data,metadata=clients.canonical_client(rows,clients.target_table(self.repo),VERSION,'game',self.repo)
        self.assertEqual(clients.check_canonical(data,VERSION,'game',repo=self.repo),metadata)
        self.assertEqual(len(metadata['modules']),20)
        reverse,_=clients.canonical_client(rows[::-1],clients.target_table(self.repo),VERSION,'game',self.repo)
        self.assertEqual(data,reverse)

    def test_missing_duplicate_wrong_abi_or_mixed_variant_is_rejected(self):
        row=self.row('windows-x86')
        for rows in ([row],[row,row]):
            with self.assertRaisesRegex(ValueError,'Missing/duplicate'):
                clients.canonical_client(rows,['windows-x86','linux-x86_64'],VERSION,'game',self.repo)
        with self.assertRaisesRegex(ValueError,'Wrong native client ABI'):
            self.row('linux-x86_64',overrides={'ui.mp.x86_64.so':binary_header('windows-x86')})
        with self.assertRaisesRegex(ValueError,'Mixed client'):
            clients.canonical_client([self.row('windows-x86','navstudio')],['windows-x86'],VERSION,'game',self.repo)

    def test_frozen_resource_snapshot_reuses_exact_bytes_and_version(self):
        row=self.row('windows-x86');frozen=clients.client_assets(VERSION,'game',self.repo)
        data,metadata=clients.canonical_client([row],['windows-x86'],VERSION,'game',self.repo,assets=frozen)
        self.assertEqual(clients.check_canonical(data,VERSION,'game',['windows-x86'],self.repo,assets=frozen),metadata)
        frozen=dict(frozen);frozen[f'tce2/{VERSION}/client.manifest']=b'old'
        with self.assertRaisesRegex(ValueError,'frozen resource version'):
            clients.check_canonical(data,VERSION,'game',['windows-x86'],self.repo,assets=frozen)

    def test_transfer_contains_only_native_pair_and_asset_fingerprint(self):
        row=self.row('windows-x86')
        self.assertEqual(set(row['payload']),set(clients.module_names('windows-x86').values()))
        self.assertFalse(any(name.endswith('.pk3') or name.endswith('.tga') for name in row['files']))
        with self.assertRaisesRegex(ValueError,'Wrong client inputs/assets'):
            self.row('windows-x86',overrides={'extra.tga':b'stale'})

    def test_real_target_packager_avoids_full_resource_copy_in_transfer(self):
        modules=self.root/'modules';modules.mkdir();paths={}
        for role in ('cgame','ui','qagame'):
            path=modules/(role+'_mp_x86.dll');path.write_bytes(binary_header('windows-x86'));paths[role]=str(path)
        manifest=modules/'tce-modules-Release.json'
        manifest.write_text(json.dumps({'schema':1,'variant':'game','configuration':'Release','modules':paths}))
        archive=package_modules(None,self.root/'transfer','windows-x86','game',repo=self.repo,module_manifest=manifest)
        row=clients.read_target(archive,VERSION,self.repo)
        self.assertFalse(row['metadata']['gameplay_bot_runtime'])
        self.assertLess(archive.stat().st_size,50000)
        self.assertFalse(any(name.endswith('.pk3') or name.endswith('.tga') for name in row['files']))

    def test_local_cmake_resource_entrypoint_uses_same_complete_reader(self):
        modules=self.root/'native';modules.mkdir()
        for name in clients.module_names('windows-x86').values():(modules/name).write_bytes(binary_header('windows-x86'))
        output=modules/f'tce2-{VERSION}.pk3'
        package_directory(modules,'game',output,self.repo)
        payload=clients.zip_contents(output.read_bytes())
        self.assertEqual(set(payload),set(resource_assets(VERSION,'game',self.repo))|set(clients.module_names('windows-x86').values()))
        (modules/'ui_mp_x86.dll').unlink()
        with self.assertRaisesRegex(ValueError,'Incomplete native client pair'):
            package_directory(modules,'game',output,self.repo)

    def bundle(self,variant='game'):
        targets=['windows-x86','linux-i386','linux-x86_64'];rows=[self.row(target,variant) for target in targets]
        client,metadata=clients.canonical_client(rows,targets,VERSION,variant,self.repo)
        data,bundle=single_mod.build_bundle(rows,client,metadata,VERSION,variant,self.repo,targets)
        return data,bundle,client,targets

    def test_single_pk3_with_all_server_abis_and_no_duplicate_loose_clients(self):
        with synthetic_runtime():
            data,metadata,client,targets=self.bundle()
            self.assertEqual(single_mod.check_bundle(data,VERSION,'game',self.repo,targets),metadata)
        files=clients.zip_contents(data)
        self.assertEqual([name for name in files if name.endswith('.pk3')],[f'tce2/tce2-{VERSION}.pk3'])
        self.assertEqual(files[f'tce2/tce2-{VERSION}.pk3'],client)
        for target in targets:
            for name in clients.module_names(target).values():self.assertNotIn('tce2/'+name,files)
            config=clients.target_table()[target]
            self.assertIn('server/'+target+'/'+config.get('prefix','')+'qagame'+config['suffix'],files)
        self.assertIn('server/common/omni-bot/global_scripts/fixture.gm',files)
        self.assertIn('launcher/start_game.py',files)

    def test_changed_bundle_bytes_extra_pk3_missing_server_and_core_are_rejected(self):
        with synthetic_runtime():
            data,metadata,_,targets=self.bundle()
            original=clients.zip_contents(data)
            for name in [f'tce2/tce2-{VERSION}.pk3','server/windows-x86/qagame_mp_x86.dll',
                         'server/windows-x86/omni-bot/'+CORE_NAMES['windows-x86']]:
                files=dict(original);files[name]+=b'changed'
                with self.subTest(name=name),self.assertRaises(ValueError):
                    single_mod.check_bundle(clients.zip_bytes(files),VERSION,'game',self.repo,targets)
            files=dict(original);files['extra.pk3']=b'bad'
            manifest=json.loads(files['manifest.json']);manifest['files'].append({'path':'extra.pk3','bytes':3,'sha256':clients.sha(b'bad')})
            files['manifest.json']=clients.json_bytes(manifest)
            with self.assertRaisesRegex(ValueError,'Exactly one'):
                single_mod.check_bundle(clients.zip_bytes(files),VERSION,'game',self.repo,targets)

    def test_wrong_server_or_core_abi_fails_even_with_updated_outer_hashes(self):
        with synthetic_runtime():
            data,_,_,targets=self.bundle();original=clients.zip_contents(data)
            for name in ('server/windows-x86/qagame_mp_x86.dll',
                         'server/windows-x86/omni-bot/'+CORE_NAMES['windows-x86']):
                files=dict(original);files[name]=binary_header('linux-x86_64')
                manifest=json.loads(files['manifest.json'])
                for row in manifest['files']:
                    if row['path']==name:row.update(bytes=len(files[name]),sha256=clients.sha(files[name]))
                if 'omni-bot/' in name:
                    manifest['target_inputs']['windows-x86']['metadata']['bot_core_boundary']['sha256']=clients.sha(files[name])
                files['manifest.json']=clients.json_bytes(manifest)
                with self.subTest(name=name),self.assertRaisesRegex(ValueError,'Wrong native client ABI'):
                    single_mod.check_bundle(clients.zip_bytes(files),VERSION,'game',self.repo,targets)

    def test_installer_selects_one_server_abi_and_optionally_extracts_same_pk3_clients(self):
        data,_,client,_=self.bundle();folder=self.root/'unpacked';folder.mkdir()
        for name,payload in clients.zip_contents(data).items():
            path=folder/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(payload)
        mod=self.root/'existing-game/tce2';mod.mkdir(parents=True)
        with self.assertRaisesRegex(ValueError,'original TCE 0.49b base first'):
            install(folder,'windows-x86',mod)
        for i in range(5):(mod/f'pak{i}.pk3').write_bytes(('synthetic stock '+str(i)).encode())
        (mod/'qagame_mp_x86.dll').write_bytes(b'old server')
        result=install(folder,'windows-x86',mod)
        self.assertIsNotNone(result['backup']);self.assertFalse((mod/'ui_mp_x86.dll').exists())
        self.assertEqual((Path(result['backup'])/'qagame_mp_x86.dll').read_bytes(),b'old server')
        self.assertFalse((mod/'qagame.mp.x86_64.so').exists())
        install(folder,'windows-x86',mod,True)
        self.assertEqual((mod/'ui_mp_x86.dll').read_bytes(),clients.zip_contents(client)['ui_mp_x86.dll'])
        with self.assertRaisesRegex(ValueError,'absent'):install(folder,'unknown',mod)
        with self.assertRaisesRegex(ValueError,'existing tce2'):install(folder,'windows-x86',self.root/'new-game')


if __name__=='__main__':unittest.main()
