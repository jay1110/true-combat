"""Android target, real CMake/loader selection and negative ELF contracts.

Synthetic ELF headers are not executable modules. CMake script evaluation and
host preprocessing prove configuration dispatch, not an Android NDK build.
"""
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

import client_packages as clients
import loose_binaries as loose
from build_loose import build_commands
from test_loose_binaries import binary_header, CORE_NAMES

ROOT = Path(__file__).resolve().parents[2]
ANDROID = {'android-armv7': ('armeabi-v7a','armv7'),
           'android-arm64': ('arm64-v8a','aarch64'),
           'android-x86': ('x86','i386'), 'android-x86_64': ('x86_64','x86_64')}


class AndroidTargets(unittest.TestCase):
    def test_matrix_presets_and_build_commands_include_all_four_android_abis(self):
        targets = clients.target_table()
        self.assertEqual(len(targets),12)
        presets = json.loads((ROOT/'CMakePresets.json').read_text())
        configure = {row['name']:row for row in presets['configurePresets']}
        builds = {row['name']:row for row in presets['buildPresets']}
        for target,(abi,_arch) in ANDROID.items():
            with self.subTest(target=target):
                row=targets[target]
                self.assertEqual(row['suffix'],'.mp.android.'+abi+'.so')
                self.assertEqual(row['prefix'],'lib')
                self.assertTrue(row['core'])
                self.assertEqual(loose.CORE_IDENTITIES[target],('Android',abi))
                self.assertEqual(CORE_NAMES[target],'omnibot_et.android.'+abi+'.so')
                self.assertEqual(configure[target]['toolchainFile'],'${sourceDir}/cmake/toolchains/'+row['toolchain'])
                self.assertEqual(set(builds[target]['targets']),{'tce_modules','omnibot-et'})
                commands=dict(build_commands(target,Path('build-fixture'),Path('boost-fixture')))
                self.assertIn('-DCMAKE_TOOLCHAIN_FILE='+str(ROOT/'cmake/toolchains'/row['toolchain']),commands['configure-modules'])
                self.assertIn('-DFEATURE_NAVSTUDIO=OFF',commands['configure-modules'])

    def test_real_toolchains_and_platform_classifiers(self):
        cmake=shutil.which('cmake')
        if not cmake:self.skipTest('CMake unavailable for real configuration dispatch')
        platform=(ROOT/'cmake/TCEPlatform.cmake').read_text().split('add_library(tce_build_options',1)[0]
        source=(ROOT/'vendor/omni-bot/source/Omnibot/CMakeLists.txt').read_text()
        core=source[source.index('string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}"'):source.index('set(DEPENDENCIES_PATH')]
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);ndk=root/'ndk/build/cmake';ndk.mkdir(parents=True)
            # A controlled include endpoint records the real wrapper's ABI/API;
            # it deliberately supplies no compiler/sysroot or NDK build claim.
            (ndk/'android.toolchain.cmake').write_text('set(CMAKE_ANDROID_ARCH_ABI "${ANDROID_ABI}")\n')
            for target,(abi,arch) in ANDROID.items():
                wrapper=(ROOT/'cmake/toolchains'/(target+'.cmake')).as_posix()
                script=root/(target+'.cmake')
                script.write_text('set(TCE_ANDROID_NDK "'+(root/'ndk').as_posix()+'")\n'
                    'include("'+wrapper+'")\nset(ANDROID TRUE)\nset(WIN32 FALSE)\n'
                    'set(CMAKE_SYSTEM_PROCESSOR "'+arch+'")\n'+platform+'\n'+core+'\n'
                    'if(NOT ANDROID_PLATFORM STREQUAL "android-23" OR NOT TCE_TARGET_ARCH STREQUAL "'+arch+'"\n'
                    ' OR NOT TCE_MODULE_ARCH STREQUAL "'+abi+'" OR NOT OMNIBOT_TARGET_ARCH STREQUAL "'+abi+'"\n'
                    ' OR NOT MODULE_SUFFIX STREQUAL ".android.'+abi+'")\nmessage(FATAL_ERROR "Android classifier mismatch")\nendif()\n')
                result=subprocess.run([cmake,'-P',str(script)],capture_output=True,text=True)
                with self.subTest(target=target):self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            script=root/'unsupported.cmake'
            script.write_text('set(ANDROID TRUE)\nset(ANDROID_ABI mips)\n'+platform)
            result=subprocess.run([cmake,'-P',str(script)],capture_output=True,text=True)
            self.assertNotEqual(result.returncode,0)
            self.assertIn('Supported Android mod ABIs',result.stderr)

    def test_android_x86_core_loader_names_use_android_not_linux_suffixes(self):
        text=(ROOT/'vendor/omnibot/Common/BotLoadLibrary.cpp').read_text()
        start=text.index('#if defined(__EMSCRIPTEN__)')
        block=text[start:text.index('\n#endif',start)+len('\n#endif')]
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);env=os.environ.copy()
            if os.name=='nt':
                sys.path.insert(0,str(ROOT/'tools/modernize'))
                try:
                    from compat_validate import msvc_environment
                    env,compiler=msvc_environment(root,'x86','14.44')
                finally:sys.path.pop(0)
                args=[compiler,'/nologo','/TC','/EP','/u']
                define='/D'
            else:
                compiler=shutil.which('cc') or shutil.which('clang')
                if not compiler:self.skipTest('Host C preprocessor unavailable')
                args=[compiler,'-E','-P','-undef','-x','c'];define='-D'
            for target,macro in [('android-x86','__i386__'),('android-x86_64','__x86_64__')]:
                path=root/(target+'.c');path.write_text(block+'\nconst char *core_suffix = SUFFIX;\n')
                result=subprocess.run([*args,define+'__ANDROID__',define+macro,str(path)],env=env,capture_output=True,text=True)
                with self.subTest(target=target):
                    self.assertEqual(result.returncode,0,result.stdout+result.stderr)
                    self.assertIn('".android.'+ANDROID[target][0]+'"',result.stdout)

    def test_new_elf_gates_reject_cross_architecture_headers(self):
        for target in ('android-x86','android-x86_64'):
            good=binary_header(target)
            bad_class=bytearray(good);bad_class[4]=2 if good[4]==1 else 1
            bad_machine=bytearray(good);struct.pack_into('<H',bad_machine,18,40)
            bad_type=bytearray(good);struct.pack_into('<H',bad_type,16,2)
            bad_endian=bytearray(good);bad_endian[5]=2
            for role in ('cgame','ui','qagame','omnibot'):
                clients.check_module(good,role,target,'game')
                for bad in (bad_class,bad_machine,bad_type,bad_endian):
                    with self.subTest(target=target,role=role),self.assertRaises(ValueError):
                        clients.check_module(bad,role,target,'game')
            with tempfile.TemporaryDirectory() as temporary:
                path=Path(temporary)/CORE_NAMES[target]
                path.write_bytes(good);loose.check_binary(path,target,'omnibot',clients.target_table()[target])
                path.write_bytes(bad_machine)
                with self.assertRaises(ValueError):loose.check_binary(path,target,'omnibot',clients.target_table()[target])


if __name__=='__main__':unittest.main()
