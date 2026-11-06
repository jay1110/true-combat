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

    def test_actual_canonical_two_abi_resources_boundary_and_no_qgame(self):
        packages,report=tool.create_packages(self.manifests)
        self.assertTrue(report['input_stable']);self.assertTrue(report['shared_assets_equal'])
        for variant,data in packages.items():
            metadata=tool.clients.check_canonical(data,tool.VERSION,variant,tool.TARGETS)
            self.assertEqual(metadata['scope'],'local-diagnostic');self.assertEqual(len(metadata['modules']),4)
            files=tool.clients.zip_contents(data)
            self.assertFalse(any('qagame' in path for path in files))
            self.assertFalse(any('modern-ui' in path or path.startswith('tce2_ui/') for path in files))

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
            metadata=tool.clients.check_canonical(data,tool.VERSION,variant,expected)
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
        old=output/'Game/tce2-1.0.2.pk3';old.parent.mkdir(parents=True);old.write_bytes(b'previous owned delivery')
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
        destination=output/'Game/tce2-1.0.2.pk3';destination.parent.mkdir(parents=True)
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
        old=output/'Game/tce2-1.0.2.pk3';old.write_bytes(b'previous owned delivery')
        result=subprocess.run(cmd+['--out',str(output),'--report',str(report)],capture_output=True,text=True)
        self.assertEqual(result.returncode,1,result.stdout+result.stderr)
        self.assertFalse(json.loads(report.read_text())['passed']);self.assertEqual(old.read_bytes(),b'previous owned delivery')
        forbidden=tool.ROOT/'pak/ui/client-receipt.json'
        result=subprocess.run(cmd+['--out',str(output),'--report',str(forbidden)],capture_output=True,text=True)
        self.assertEqual(result.returncode,2);self.assertFalse(forbidden.exists())


if __name__=='__main__':unittest.main()
