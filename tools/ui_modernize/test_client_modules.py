"""Synthetic ABI carriers exercise real package validation and rejection paths."""
import json
import contextlib
import io
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

import client_modules as tool
from test_loose_binaries import binary_header


class ClientModules(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.manifests={}
        self.production_root=tool.ROOT
        self.resources=self.root/'resource-repo'
        # Exercise the real complete resource reader using a small isolated tree.
        # Never repack the ~900MB production pak for every rejection subcase.
        helpers=tool.helper_paths()
        for name,path in helpers.items():
            destination=self.resources/path.relative_to(self.production_root)
            destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(path.read_bytes())
        destination=self.resources/'src/game/tce_version.h';destination.parent.mkdir(parents=True,exist_ok=True)
        destination.write_text('#define TCE_VERSION "'+tool.VERSION+'"\n')
        for name,data in {'ui/main.menu':b'menuDef { name "main" }', 'scripts/world.shader':b'world shader',
                          'maps/obj_custom.bsp':b'IBSP full map asset',
                          'tce2_ui/background.tga':b'background texture',
                          'fonts/tce2_ui/Barlow-Regular_21.dat':b'classic font cache',
                          **{'levelshots/'+mapname+ending:b'compass '+mapname.encode()+ending.encode()
                             for mapname in ('obj_delta','obj_northport','obj_railhouse','obj_snow','obj_stadtrand','obj_village')
                             for ending in ('_cc.tga','_cc_blueprint.tga')}}.items():
            destination=self.resources/'pak'/name;destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(data)
        # A tiny but real installable Editor graph: the resource reader's actual
        # registry/name/reachability gate remains enabled in ABI carrier tests.
        editor={'ui/main.menu':b'menuDef { name "main" action { open tce_omnibot ; } }',
                'ui/tce_omnibot.menu':b'menuDef { name "tce_omnibot" action { open tce_omnibot_dms ; } }',
                'ui/tce_omnibot_dms.menu':b'menuDef { name "tce_omnibot_dms" action { open tce_ob_goalname ; open tce_ob_groupname ; open tce_ob_waypointname ; open tce_ob_value ; } }'}
        for goal in ('goalname','groupname','waypointname','value'):
            editor['ui/tce_ob_'+goal+'.menu']=('menuDef { name "tce_ob_'+goal+'" action { open tce_omnibot_dms ; } }').encode()
        for name in ('wm_quickmessage.menu','wm_quickmessageAlt.menu','wm_ftquickmessage.menu','wm_ftquickmessageAlt.menu'):
            editor['ui/'+name]=('menuDef { name "'+name[:-5]+'" action { exec "omnibot_menu" ; } }').encode()
        registry=b'{ '+b' '.join(('loadMenu { "'+name+'" }').encode() for name in sorted(editor))+b' }'
        for name in ('menus.txt','menus_gt2.txt','menus_gt5.txt','menus_gt7.txt'):
            destination=self.resources/'pak/ui'/name;destination.parent.mkdir(parents=True,exist_ok=True)
            destination.write_bytes(b'{ loadMenu { "ui/main.menu" } }')
            editor['ui/'+name]=registry
        for name,data in editor.items():
            destination=self.resources/'navstudio/editor-pak'/name
            destination.parent.mkdir(parents=True,exist_ok=True);destination.write_bytes(data)
        patcher=mock.patch.object(tool,'ROOT',self.resources);patcher.start();self.addCleanup(patcher.stop)
        for variant in tool.VARIANTS:
            for target in tool.TARGETS:
                self.add_manifest(variant,target)

    def add_manifest(self,variant,target):
        folder=self.root/variant/target;folder.mkdir(parents=True)
        modules={}
        for role,name in tool.clients.module_names(target).items():
            data=binary_header(target)+b'/'+role.encode()
            if variant=='navstudio':
                literals=tool.UI_AUTHORING if role=='ui' else ('cg_omnibotdrawing','ExportClientFunctionsFromDLL')
                data+=b'\0'+b'\0'.join(s.encode() for s in literals)+b'\0'
            path=folder/name;path.write_bytes(data);modules[role]=str(path)
        # The real CMake manifest has QGame too; it must never be read.
        modules['qagame']=str(folder/'MISSING-DO-NOT-READ-QGAME')
        manifest=folder/'tce-modules-Release.json'
        manifest.write_text(json.dumps({'schema':1,'configuration':'Release','variant':variant,'modules':modules}))
        self.manifests[variant,target]=manifest

    def add_i386_pair(self):
        for variant in tool.VARIANTS:self.add_manifest(variant,tool.OPTIONAL_TARGET)

    def mutate(self,variant,target,change):
        path=self.manifests[variant,target];metadata=json.loads(path.read_text());change(metadata)
        path.write_text(json.dumps(metadata))

    def assets(self,variant):
        return tool.clients.client_assets(tool.VERSION,variant,self.resources)

    def test_actual_canonical_two_abi_resources_boundary_and_no_qgame(self):
        packages,report=tool.create_packages(self.manifests)
        self.assertTrue(report['input_stable']);self.assertTrue(report['shared_assets_equal'])
        for variant,data in packages.items():
            metadata=tool.clients.check_canonical(data,tool.VERSION,variant,tool.TARGETS,assets=self.assets(variant))
            self.assertEqual(metadata['scope'],'local-diagnostic');self.assertEqual(len(metadata['modules']),4)
            files=tool.clients.zip_contents(data)
            self.assertFalse(any('qagame' in path for path in files))
            for name in ('tce2_ui/background.tga','fonts/tce2_ui/Barlow-Regular_21.dat','maps/obj_custom.bsp',
                         'levelshots/obj_snow_cc.tga','levelshots/obj_northport_cc_blueprint.tga'):
                self.assertEqual(files[name],self.assets(variant)[name])
        self.assertTrue({'ui/main.menu','ui/tce_omnibot.menu','ui/tce_ob_goalname.menu'}<=set(report['editor_resource_differences']))
        self.assertTrue(report['resource_overlay_verified'])
        self.assertIn(str((self.resources/'pak/levelshots/obj_snow_cc.tga').resolve()),report['input_files'])

    def test_manifest_variant_debug_schema_missing_role_filename_and_abi_rejected(self):
        for change in (lambda m:m.update(variant='navstudio'),lambda m:m.update(configuration='Debug'),
                       lambda m:m.update(schema=2),lambda m:m['modules'].pop('ui'),
                       lambda m:m['modules'].update(ui=m['modules']['cgame'])):
            path=self.manifests['game','windows-x86'];before=path.read_bytes()
            with self.subTest(change=change):
                self.mutate('game','windows-x86',change)
                with self.assertRaises(ValueError):tool.create_packages(self.manifests)
                path.write_bytes(before)
        m=json.loads(path.read_text());module=Path(m['modules']['ui'])
        module.write_bytes(binary_header('windows-x64'))
        with self.assertRaisesRegex(ValueError,'Wrong native client ABI'):tool.create_packages(self.manifests)

    def test_actual_ui_authoring_mixing_rejected_even_without_obsolete_marker(self):
        for variant,target in (('game','windows-x86'),('game','linux-x86_64'),('navstudio','windows-x86'),('navstudio','linux-x86_64')):
            metadata=json.loads(self.manifests[variant,target].read_text());module=Path(metadata['modules']['ui']);before=module.read_bytes()
            replacement=binary_header(target)
            if variant=='game':replacement+=b'\0omnibot_menu\0omnibot_goalname\0omnibot_goalproperty\0'
            module.write_bytes(replacement)
            with self.subTest(variant=variant,target=target),self.assertRaisesRegex(ValueError,'actual UI authoring boundary'):
                tool.create_packages(self.manifests)
            module.write_bytes(before)

    def test_optional_i386_pair_adds_both_modules_without_changing_two_abi_delivery(self):
        original,original_report=tool.create_packages(self.manifests)
        self.add_i386_pair();expected=tool.TARGETS+(tool.OPTIONAL_TARGET,)
        packages,report=tool.create_packages(self.manifests)
        self.assertEqual(report['targets'],list(expected));self.assertEqual(len(report['inputs']),6)
        for variant,data in packages.items():
            metadata=tool.clients.check_canonical(data,tool.VERSION,variant,expected,assets=self.assets(variant))
            self.assertEqual(len(metadata['modules']),6)
            files=tool.clients.zip_contents(data)
            for role,name in tool.clients.module_names(tool.OPTIONAL_TARGET).items():
                self.assertEqual(metadata['modules'][name]['target'],'linux-i386')
                self.assertEqual(metadata['modules'][name]['role'],role)
                self.assertIn(name,files)
            self.assertFalse(any('qagame' in name.lower() for name in files))
        base={key:path for key,path in self.manifests.items() if key[1] in tool.TARGETS}
        repeated,repeated_report=tool.create_packages(base)
        self.assertEqual(repeated,original)
        self.assertEqual(repeated_report['targets'],original_report['targets'])

    def test_incomplete_or_unselected_optional_pair_rejected(self):
        self.add_manifest('game',tool.OPTIONAL_TARGET)
        with self.assertRaisesRegex(ValueError,'complete Linux-i386 pair'):tool.create_packages(self.manifests)
        self.manifests.pop(('game',tool.OPTIONAL_TARGET))
        self.add_manifest('navstudio',tool.OPTIONAL_TARGET)
        with self.assertRaisesRegex(ValueError,'complete Linux-i386 pair'):tool.create_packages(self.manifests)
        self.manifests.pop(('navstudio',tool.OPTIONAL_TARGET))
        for variant in tool.VARIANTS:self.add_manifest(variant,'linux-armv7')
        with self.assertRaisesRegex(ValueError,'complete Linux-i386 pair'):tool.create_packages(self.manifests)

    def test_i386_pair_variant_target_configuration_abi_and_editor_boundary_rejected(self):
        self.add_i386_pair();target=tool.OPTIONAL_TARGET
        for variant in tool.VARIANTS:
            path=self.manifests[variant,target];before=path.read_bytes()
            for change in (lambda m:m.update(variant='navstudio' if variant=='game' else 'game'),
                           lambda m:m.update(target='linux-x86_64'),lambda m:m.update(configuration='Debug'),
                           lambda m:m.update(schema=2),lambda m:m['modules'].pop('ui'),
                           lambda m:m['modules'].update(ui=m['modules']['cgame'])):
                with self.subTest(variant=variant,change=change):
                    self.mutate(variant,target,change)
                    with self.assertRaises(ValueError):tool.create_packages(self.manifests)
                    path.write_bytes(before)
            metadata=json.loads(before);module=Path(metadata['modules']['ui']);original=module.read_bytes()
            module.write_bytes(binary_header('linux-x86_64'))
            with self.assertRaisesRegex(ValueError,'Wrong native client ABI'):tool.create_packages(self.manifests)
            replacement=binary_header(target)
            if variant=='game':replacement+=b'\0omnibot_menu\0omnibot_goalname\0omnibot_goalproperty\0'
            module.write_bytes(replacement)
            with self.assertRaisesRegex(ValueError,'actual UI authoring boundary'):tool.create_packages(self.manifests)
            module.write_bytes(original)
            cgame=Path(metadata['modules']['cgame']);original=cgame.read_bytes()
            replacement=binary_header(target)+(b'\0wp_add\0' if variant=='game' else b'')
            cgame.write_bytes(replacement)
            with self.assertRaisesRegex(ValueError,'Editor entry point in public|Missing Editor CGame'):
                tool.create_packages(self.manifests)
            cgame.write_bytes(original)

    def test_cli_optional_half_pair_and_optional_input_overwrite_rejected(self):
        self.add_i386_pair();output=self.root/'delivery';report=self.root/'optional-receipt.json'
        cmd=[sys.executable,str(Path(tool.__file__))]
        for flag,key in [('windows-game',('game','windows-x86')),('windows-editor',('navstudio','windows-x86')),
                         ('linux-game',('game','linux-x86_64')),('linux-editor',('navstudio','linux-x86_64'))]:
            cmd+=['--'+flag,str(self.manifests[key])]
        optional=self.manifests['game',tool.OPTIONAL_TARGET];before=optional.read_bytes()
        old=output/'Game/tce2-1.0.3.pk3';old.parent.mkdir(parents=True);old.write_bytes(b'previous owned delivery')
        half=cmd+['--linux-i386-game',str(optional),'--out',str(output),'--report',str(report)]
        result=subprocess.run(half,capture_output=True,text=True)
        self.assertEqual(result.returncode,2,result.stdout+result.stderr)
        self.assertFalse(report.exists());self.assertEqual(old.read_bytes(),b'previous owned delivery')
        paired=cmd+['--linux-i386-game',str(optional),'--linux-i386-editor',str(self.manifests['navstudio',tool.OPTIONAL_TARGET])]
        result=subprocess.run(paired+['--out',str(output),'--report',str(optional)],capture_output=True,text=True)
        self.assertEqual(result.returncode,2,result.stdout+result.stderr)
        self.assertEqual(optional.read_bytes(),before);self.assertEqual(old.read_bytes(),b'previous owned delivery')

    def test_cli_failed_validation_cannot_replace_helper_or_discovered_native_input(self):
        self.add_i386_pair();repo=self.root/'fixture-repo';output=self.root/'delivery'
        helper=repo/'tools/release/targets.json';helper.parent.mkdir(parents=True)
        helper.write_bytes(b'{"owned helper input":true}\n')
        native=self.root/'owned-native-input.json';native.write_bytes(b'owned discovery input')
        self.mutate('navstudio',tool.OPTIONAL_TARGET,lambda m:(m.update(configuration='Debug'),m['modules'].update(ui=str(native))))
        args=[str(Path(tool.__file__))]
        for flag,key in [('windows-game',('game','windows-x86')),('windows-editor',('navstudio','windows-x86')),
                         ('linux-game',('game','linux-x86_64')),('linux-editor',('navstudio','linux-x86_64')),
                         ('linux-i386-game',('game',tool.OPTIONAL_TARGET)),('linux-i386-editor',('navstudio',tool.OPTIONAL_TARGET))]:
            args+=['--'+flag,str(self.manifests[key])]
        for receipt in (helper,native):
            before=receipt.read_bytes();error=io.StringIO()
            with self.subTest(receipt=receipt),mock.patch.object(tool,'ROOT',repo),\
                    mock.patch.object(sys,'argv',args+['--out',str(output),'--report',str(receipt)]),\
                    mock.patch.object(tool,'create_packages',wraps=tool.create_packages) as create,\
                    contextlib.redirect_stderr(error),self.assertRaises(SystemExit) as stopped:
                tool.main()
            self.assertEqual(stopped.exception.code,2)
            self.assertIn('cannot overwrite native/discovery/tool inputs',error.getvalue())
            create.assert_not_called();self.assertEqual(receipt.read_bytes(),before)
            self.assertFalse(output.exists())

    def test_cli_package_destinations_are_preflighted_before_native_failure(self):
        repo=self.root/'fixture-repo';output=self.root/'delivery';report=self.root/'receipt.json'
        destination=output/'Game/tce2-1.0.3.pk3';destination.parent.mkdir(parents=True)
        destination.write_bytes(b'owned helper through output alias')
        self.mutate('game','windows-x86',lambda m:m.update(configuration='Debug'))
        args=[str(Path(tool.__file__))]
        for flag,key in [('windows-game',('game','windows-x86')),('windows-editor',('navstudio','windows-x86')),
                         ('linux-game',('game','linux-x86_64')),('linux-editor',('navstudio','linux-x86_64'))]:
            args+=['--'+flag,str(self.manifests[key])]
        before=destination.read_bytes();error=io.StringIO()
        # Controlled helper-path alias models an existing symlink's resolved
        # destination without touching real helpers or requiring symlink rights.
        with mock.patch.object(tool,'ROOT',repo),mock.patch.object(tool,'helper_paths',return_value={'targets.json':destination}),\
                mock.patch.object(sys,'argv',args+['--out',str(output),'--report',str(report)]),\
                mock.patch.object(tool,'create_packages',wraps=tool.create_packages) as create,\
                contextlib.redirect_stderr(error),self.assertRaises(SystemExit) as stopped:
            tool.main()
        self.assertEqual(stopped.exception.code,2)
        self.assertIn('cannot overwrite native/discovery/tool inputs',error.getvalue())
        create.assert_not_called();self.assertEqual(destination.read_bytes(),before)
        self.assertFalse(report.exists())

    def test_cli_failed_inputs_never_write_or_replace_packages_and_sources_protected(self):
        output=self.root/'delivery';report=self.root/'receipt.json';cmd=[sys.executable,str(Path(tool.__file__))]
        for flag,key in [('windows-game',('game','windows-x86')),('windows-editor',('navstudio','windows-x86')),
                         ('linux-game',('game','linux-x86_64')),('linux-editor',('navstudio','linux-x86_64'))]:
            cmd+=['--'+flag,str(self.manifests[key])]
        self.mutate('navstudio','linux-x86_64',lambda m:m.update(configuration='Debug'))
        for folder in tool.VARIANTS.values():(output/folder).mkdir(parents=True)
        old=output/'Game/tce2-1.0.3.pk3';old.write_bytes(b'previous owned delivery')
        result=subprocess.run(cmd+['--out',str(output),'--report',str(report)],capture_output=True,text=True)
        self.assertEqual(result.returncode,1,result.stdout+result.stderr)
        self.assertFalse(json.loads(report.read_text())['passed']);self.assertEqual(old.read_bytes(),b'previous owned delivery')
        forbidden=self.production_root/'pak_original_tce/ui/client-receipt.json'
        result=subprocess.run(cmd+['--out',str(output),'--report',str(forbidden)],capture_output=True,text=True)
        self.assertEqual(result.returncode,2);self.assertFalse(forbidden.exists())

    def test_resources_are_loaded_once_per_variant_and_transfer_rows_are_module_only(self):
        with mock.patch.object(tool.clients,'client_assets',wraps=tool.clients.client_assets) as load,\
                mock.patch.object(tool.clients,'check_target_payload',wraps=tool.clients.check_target_payload) as transfer:
            tool.create_packages(self.manifests)
        self.assertEqual(load.call_count,2)
        self.assertEqual([call.args[1] for call in load.call_args_list],['game','navstudio'])
        self.assertTrue(all(call.kwargs.get('module_only') for call in transfer.call_args_list))

    def test_changed_resource_during_packaging_is_rejected(self):
        original=tool.clients.canonical_client
        def change_after_pack(*args,**kwargs):
            result=original(*args,**kwargs)
            if args[3]=='navstudio':
                (self.resources/'pak/maps/obj_custom.bsp').write_bytes(b'changed while packaging')
            return result
        with mock.patch.object(tool.clients,'canonical_client',side_effect=change_after_pack),\
                self.assertRaisesRegex(ValueError,'input changed during packaging'):
            tool.create_packages(self.manifests)

    def test_added_resource_during_packaging_is_rejected(self):
        original=tool.clients.canonical_client
        def add_after_pack(*args,**kwargs):
            result=original(*args,**kwargs)
            if args[3]=='navstudio':
                (self.resources/'pak/new.txt').write_bytes(b'new while packaging')
            return result
        with mock.patch.object(tool.clients,'canonical_client',side_effect=add_after_pack),\
                self.assertRaisesRegex(ValueError,'path inventory changed'):
            tool.create_packages(self.manifests)

    def test_editor_change_outside_declared_overlay_is_rejected(self):
        original=tool.clients.client_assets
        def undeclared(*args,**kwargs):
            result=original(*args,**kwargs)
            if args[1]=='navstudio':result['scripts/world.shader']=b'undeclared Editor shader'
            return result
        with mock.patch.object(tool.clients,'client_assets',side_effect=undeclared),\
                self.assertRaisesRegex(ValueError,'Resource changed during collection|outside the explicit editor overlay'):
            tool.create_packages(self.manifests)

    def test_nested_archive_or_native_file_in_complete_resources_is_rejected(self):
        for name in ('old.pk3','old.zip','qagame_mp_x86.dll'):
            path=self.resources/'pak'/name;path.write_bytes(b'forbidden')
            with self.subTest(name=name),self.assertRaisesRegex(ValueError,'Native/generated/nested'):
                tool.create_packages(self.manifests)
            path.unlink()

    def test_incomplete_frozen_resource_payload_is_rejected(self):
        original=tool.clients.client_assets
        def missing(*args,**kwargs):
            result=original(*args,**kwargs);result.pop('levelshots/obj_snow_cc.tga');return result
        with mock.patch.object(tool.clients,'client_assets',side_effect=missing),\
                self.assertRaisesRegex(ValueError,'Incomplete/generated resource closure'):
            tool.create_packages(self.manifests)

    def test_cli_editor_and_background_sources_cannot_be_overwritten(self):
        args=[str(Path(tool.__file__))]
        for flag,key in [('windows-game',('game','windows-x86')),('windows-editor',('navstudio','windows-x86')),
                         ('linux-game',('game','linux-x86_64')),('linux-editor',('navstudio','linux-x86_64'))]:
            args+=['--'+flag,str(self.manifests[key])]
        for relative in ('navstudio/editor-pak/receipt.json','map_pics/receipt.json'):
            receipt=self.resources/relative;receipt.parent.mkdir(parents=True,exist_ok=True);receipt.write_bytes(b'owned source')
            before=receipt.read_bytes();error=io.StringIO()
            with self.subTest(relative=relative),mock.patch.object(sys,'argv',args+['--out',str(self.root/'delivery'),'--report',str(receipt)]),\
                    mock.patch.object(tool,'create_packages') as create,contextlib.redirect_stderr(error),\
                    self.assertRaises(SystemExit) as stopped:
                tool.main()
            self.assertEqual(stopped.exception.code,2);create.assert_not_called()
            self.assertEqual(receipt.read_bytes(),before)


if __name__=='__main__':unittest.main()
