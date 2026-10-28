"""Small synthetic carriers check the real two-ABI delivery and rejection path."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import client_modules as tool
from test_loose_binaries import binary_header


class ClientModules(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.manifests={}
        for variant in tool.VARIANTS:
            for target in tool.TARGETS:
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
