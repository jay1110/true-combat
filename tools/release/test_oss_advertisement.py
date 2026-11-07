"""Keep the server browser's platform claims consistent with release targets.

Bit assignments are the existing Xmod/ETLegacy public convention, not a new TCE
protocol. These checks do not claim cross-platform runtime verification.
"""
import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
PLATFORM_BITS = {
    'windows-x86': 1, 'linux-i386': 2, 'linux-x86_64': 4,
    'macos-x86_64': 8, 'android-arm64': 16, 'linux-armv7': 32,
    'linux-aarch64': 64, 'macos-arm64': 128, 'windows-x64': 256,
    'android-x86': 512, 'android-x86_64': 1024,
}


def advertised_mask(targets):
    result = 0
    for target in targets:
        if target == 'macos-universal':
            result |= PLATFORM_BITS['macos-x86_64'] | PLATFORM_BITS['macos-arm64']
        elif target in ('android-armv7', 'wasm32'):
            continue  # No established bit exists for these ABIs.
        else:
            result |= PLATFORM_BITS[target]
    return result


class PlatformAdvertisement(unittest.TestCase):
    def test_default_claims_exactly_the_standardized_release_platforms(self):
        targets = json.loads((ROOT/'tools/release/targets.json').read_text())['targets']
        source = (ROOT/'src/game/g_main.c').read_text()
        rows = re.findall(r'\{\s*&g_oss,\s*"g_oss",\s*"(\d+)",\s*([^,]+),',source)
        self.assertEqual(len(rows),1)
        value,flags = rows[0]
        self.assertEqual(int(value), advertised_mask(row['id'] for row in targets))
        self.assertEqual(flags.strip(),'CVAR_SERVERINFO')  # Editable, no ROM/latch.

    def test_standard_values_and_partial_client_installations(self):
        self.assertEqual(sum(PLATFORM_BITS.values()),2047)
        self.assertEqual(advertised_mask(('windows-x86','windows-x64','linux-i386','linux-x86_64')),263)
        self.assertEqual(advertised_mask(('android-armv7','wasm32')),0)
        self.assertEqual(advertised_mask(('macos-universal',)),8|128)
        self.assertEqual(2047 & ~511,PLATFORM_BITS['android-x86']|PLATFORM_BITS['android-x86_64'])
        with self.assertRaises(KeyError):advertised_mask(('invented-platform',))


if __name__ == '__main__':
    unittest.main()
