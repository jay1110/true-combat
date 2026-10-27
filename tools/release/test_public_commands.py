"""Keep the compiled public console boundary aligned with reviewed GM inputs."""
import json
from pathlib import Path
import re
import unittest

from verify_omnibot_boundary import (NATIVE_PUBLIC_COMMANDS, OPTIONAL_PUBLIC_COMMANDS,
                                    NATIVE_AUTHORING_COMMANDS, verify_module_bytes)

ROOT = Path(__file__).resolve().parents[2]
COMMON = ROOT / 'vendor/omni-bot/source/Omnibot/Common'


class PublicCommands(unittest.TestCase):
    def test_every_allowed_command_is_gameplay_administration(self):
        policy = json.loads((ROOT / 'tools/release/runtime-policy.json').read_text())
        public = set(NATIVE_PUBLIC_COMMANDS + OPTIONAL_PUBLIC_COMMANDS)
        public.update(name for names in policy['public_commands'].values() for name in names)
        header = (COMMON / 'PublicBotCommands.h').read_text()
        body = re.search(r'commands\[\]\s*=\s*\{(.*?)\};', header, re.S)[1]
        compiled = set(re.findall(r'"([^"\n]+)"', body))
        self.assertEqual(compiled, public)
        forbidden = set(NATIVE_AUTHORING_COMMANDS)
        forbidden.update(name for names in policy['editor_commands'].values() for name in names)
        self.assertFalse(compiled & forbidden)

    def test_module_boundary_rejects_registered_commands_and_drawing_cvars(self):
        for role, text in (('cgame', 'wp_add'), ('cgame', 'cg_omnibotdrawing'),
                           ('qagame', 'listbotgoals'), ('qagame', 'setsniperspot'),
                           ('qagame', 'bot_editWaypoints')):
            data = b'MZ\0' + text.encode() + b'\0'
            with self.subTest(role=role, text=text):
                with self.assertRaisesRegex(ValueError, 'Editor entry point'):
                    verify_module_bytes(role, data, 'game')
                verify_module_bytes(role, data, 'navstudio')


if __name__ == '__main__':
    unittest.main()
