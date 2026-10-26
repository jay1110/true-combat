"""Bounded symbol-evidence contracts, independent of local original binaries."""
import copy
import struct
import tempfile
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch

import verify_omnibot_boundary as boundary


class SymbolEvidence(unittest.TestCase):
    def test_nm_undefined_imports_and_data_are_not_definitions(self):
        output = (
            '_ZN5IGame9cmdAddbotERK T 10 20\n'
            '_ZN5IGame10cmdKickbotERK U\n'
            '_ZN5IGame10cmdKickbotERK U 0000\n'
            '_ZN5IGame10cmdKickbotERK w\n'
            '_ZN5IGame10cmdKickbotERK D 20 10\n'
            '_ZN5IGame10cmdKickAllERK W 30 20\n'
        )
        process = SimpleNamespace(returncode=0, stdout=output, stderr='')
        with patch.object(boundary, 'nm_tools', return_value=['fixture-nm']), \
                patch.object(boundary.subprocess, 'run', return_value=process):
            evidence = boundary.defined_symbols(Path('fixture.elf'), b'fixture')
        symbols = evidence['default']['symbols']
        self.assertEqual([item['type'] for item in symbols], ['T', 'W'])
        self.assertFalse(any('cmdKickbot' in item['symbol'] for item in symbols))

    def test_msvc_map_requires_function_definition_row(self):
        source = (
            ' 0001:00000010 ?cmdHelp@CommandReciever@@QEAAXXZ 0000000140001010 f core.obj\n'
            ' 0002:00000020 __imp_?cmdAddbot@IGame@@QEAAXXZ 0000000140002020 imports.obj\n'
            ' 0002:00000030 ?cmdAddbot@IGame@@QEAAXXZ 0000000140002030 data.obj\n'
        )
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'fixture.map'
            path.write_text(source)
            evidence = boundary.defined_symbols(Path('fixture.dll'), b'MZ', path)
        self.assertEqual([item['symbol'] for item in evidence['default']['symbols']],
                         ['?cmdHelp@CommandReciever@@QEAAXXZ'])

    def test_mangled_names_reject_import_prefixes_and_longer_identifiers(self):
        for symbol, expected in (
            ('?cmdHelp@CommandReciever@@QEAAXXZ', True),
            ('__imp_?cmdHelp@CommandReciever@@QEAAXXZ', False),
            ('_ZN15CommandReciever7cmdHelpERK', True),
            ('_ZN15CommandReciever11cmdHelpMoreERK', False),
        ):
            with self.subTest(symbol=symbol):
                self.assertEqual(boundary.method_matches(symbol, 'cmdHelp'), expected)

    def test_fat_macho_header_enumerates_each_architecture_in_both_endians(self):
        for magic, endian in ((b'\xca\xfe\xba\xbe', '>'), (b'\xbe\xba\xfe\xca', '<')):
            header = (magic + struct.pack(endian + 'I', 2)
                      + struct.pack(endian + 'IIIII', 0x01000007, 3, 48, 1, 0)
                      + struct.pack(endian + 'IIIII', 0x0100000c, 0, 49, 1, 0))
            with self.subTest(endian=endian):
                self.assertEqual(boundary.macho_architectures(header), ['x86_64', 'arm64'])

    def test_missing_runtime_handler_in_second_architecture_is_rejected(self):
        # This logical fixture exercises adjudication across slices. It does
        # not claim that the minimal carrier is an executable Mach-O binary.
        data = b'MZ\0ExportBotFunctionsFromDLL\0' + b'\0'.join(
            name.encode() for name in boundary.RUNTIME_BINDINGS) + b'\0'
        symbols = [{'symbol': '?' + name + '@Fixture@@QEAAXXZ',
                    'type': 'T', 'address': format(index + 16, 'x')}
                   for index, name in enumerate(boundary.RUNTIME_METHODS)]
        good = {'kind': 'logical-fixture', 'tool': 'none', 'sha256': '0' * 64,
                'symbols': symbols}
        bad = copy.deepcopy(good)
        bad['symbols'] = [item for item in bad['symbols']
                          if not boundary.method_matches(item['symbol'], 'cmdHelp')]
        with tempfile.TemporaryDirectory() as temporary:
            core = Path(temporary) / 'fixture.dll'
            core.write_bytes(data)
            with patch.object(boundary, 'defined_symbols',
                              return_value={'x86_64': copy.deepcopy(good), 'arm64': bad}):
                with self.assertRaisesRegex(ValueError, 'runtime_methods for arm64'):
                    boundary.verify_core(core, 'game')

    def test_wasm_link_map_only_admits_code_not_import_or_data_rows(self):
        source = (
            '    Addr Off Size Out In Symbol\n'
            ' - 10 10 IMPORT\n'
            ' - 11 2 imported.o:(_ZN5IGame10cmdKickbotERK)\n'
            ' - 20 10 CODE\n'
            ' - 21 2 core.o:(_ZN15CommandReciever7cmdHelpERK)\n'
            ' - 21 2 CommandReciever::cmdHelp()\n'
            ' - 30 10 DATA\n'
            ' - 31 2 data.o:(_ZN5IGame9cmdAddbotERK)\n'
        )
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'fixture.map'
            path.write_text(source)
            evidence = boundary.defined_symbols(Path('fixture.wasm'), b'\0asm', path)
        self.assertEqual([item['symbol'] for item in evidence['default']['symbols']],
                         ['_ZN15CommandReciever7cmdHelpERK'])


if __name__ == '__main__':
    unittest.main()
