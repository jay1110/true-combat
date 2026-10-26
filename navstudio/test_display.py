"""Run the exact test-display Lua callbacks offline; no engine or RCON."""
from pathlib import Path
import unittest

try:
    from lupa.lua54 import LuaRuntime
except ImportError:
    LuaRuntime = None


@unittest.skipIf(LuaRuntime is None, "Set PYTHONPATH to existing build/lua-check-runtime for Lua54 checks")
class DisplayTests(unittest.TestCase):
    def setUp(self):
        self.lua = LuaRuntime(unpack_returned_tuples=True)
        self.lua.execute("""
            commands = {}; writes = {}; names = {}; logs = {}; enabled = '1'
            et = {
                RegisterModname = function(name) table.insert(names, name) end,
                trap_Cvar_Get = function(name)
                    assert(name == 'navstudio_waypoint_view'); return enabled
                end,
                trap_Cvar_Set = function(name, value) table.insert(writes, {name, value}) end,
                trap_SendConsoleCommand = function(mode, value) table.insert(commands, {mode, value}) end,
                G_Print = function(value) table.insert(logs, value) end
            }
        """)
        self.lua.execute(Path(__file__).with_name("navstudio-display.lua").read_text())
        self.g = self.lua.globals()

    def test_once_after_each_map_or_round_without_input_or_gameplay_changes(self):
        for start, restart in ((0, 0), (15000, 1), (0, 0)):
            before = len(self.g.commands)
            self.g.et_InitGame(start, 123, restart)
            for offset in (0, 100, 1000, 2999):
                self.g.et_RunFrame(start + offset)
            self.assertEqual(len(self.g.commands), before)
            self.g.et_RunFrame(start + 3000)
            for offset in (3000, 4000, 10000):
                self.g.et_RunFrame(start + offset)
            self.assertEqual(len(self.g.commands), before + 1)
            self.assertEqual(list(self.g.commands[before + 1].values()), [2, "bot waypoint_view on\n"])
            self.assertEqual(list(self.g.writes[before + 1].values()), ["cg_omnibotdrawing", "1"])
        self.assertEqual(len(self.g.logs), 3)

    def test_opt_out_and_restart_before_due_do_not_issue_old_requests(self):
        self.g.et_InitGame(0, 123, 0)
        self.g.et_RunFrame(2000)
        self.g.et_InitGame(2000, 123, 1)
        self.g.et_RunFrame(3000)
        self.assertEqual(len(self.g.commands), 0)
        self.g.enabled = "0"
        self.g.et_InitGame(3000, 123, 1)
        self.g.et_RunFrame(60000)
        self.assertEqual(len(self.g.commands), 0)


if __name__ == "__main__":
    unittest.main()
