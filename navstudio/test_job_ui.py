"""Execute the real status renderer in Node with a small DOM fixture. No browser."""
import json
import os
from collections import deque
from pathlib import Path
import shutil
import subprocess
import threading
from types import SimpleNamespace
import unittest
from unittest.mock import Mock

from server import Studio

HERE = Path(__file__).resolve().parent


@unittest.skipUnless(shutil.which('node'), 'Node is required for the isolated UI status renderer')
class StatusRendererTests(unittest.TestCase):
    def render(self, game=None, jobs=None, dialog=False, pending=False):
        source = (HERE / 'app.js').read_text(encoding='utf-8')
        function = source[source.index('  function renderJobs(state) {'):source.index('  async function refreshState(')]
        fixture = {'game': game or {}, 'jobs': jobs or [], 'dialog': dialog, 'pending': pending}
        script = r'''
const vm=require('node:vm'); const input=JSON.parse(require('node:fs').readFileSync(0,'utf8'));
const element=()=>({children:[],disabled:false,open:false,textContent:'',append(...items){this.children.push(...items)},replaceChildren(){this.children=[]},removeAttribute(){}});
const ids={}; const $=id=>ids[id]||(ids[id]=element());
const actions=['stop_test','generate'];const buttons=actions.map(action=>({...element(),dataset:{action}}));
$('settings-dialog').open=input.fixture.dialog;
const context={$,document:{querySelectorAll:()=>buttons,createElement:element},
 app:{actionPending:input.fixture.pending,watchedJob:null,completedJobs:new Set(input.fixture.jobs.map(j=>j.id))},
 busyStates:['queued','running','pending','starting'],actionNames:{generate:'Toolgestützt erzeugen & prüfen'},statusNames:{failed:'fehlgeschlagen'},
 text:(id,value)=>{$(id).textContent=value},mouseStatus(){},notice(){},loadScene:()=>Promise.resolve()};
vm.createContext(context);vm.runInContext(input.function,context);context.renderJobs(input.fixture);
console.log(JSON.stringify({disabled:Object.fromEntries(buttons.map(b=>[b.dataset.action,b.disabled])),
 label:$('job-status').children[1].textContent,log:$('job-log').textContent,
 settingsDisabled:$('settings-save').disabled,settingsInfo:$('settings-info').textContent}));
'''
        result = subprocess.run([shutil.which('node'), '-e', script],
                                input=json.dumps({'function': function, 'fixture': fixture}),
                                text=True, encoding='utf-8', capture_output=True, timeout=10,
                                creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads(result.stdout)

    def test_exact_single_owned_process_can_close_without_working_port(self):
        for state in ('running', 'starting', 'unavailable'):
            with self.subTest(state=state):
                result = self.render({'state': state, 'processes': 1, 'pid': 123})
                self.assertFalse(result['disabled']['stop_test'])
        for game in ({'state': 'offline', 'processes': 0}, {'state': 'conflict', 'processes': 2, 'pid': 123},
                     {'state': 'unavailable'}, {'state': 'running', 'processes': 1, 'pid': None}):
            with self.subTest(game=game):
                self.assertTrue(self.render(game)['disabled']['stop_test'])

    def test_active_job_still_serializes_stop_and_other_mutations(self):
        result = self.render({'state': 'starting', 'processes': 1, 'pid': 123}, [{'id': 'running', 'status': 'running'}])
        self.assertTrue(result['disabled']['stop_test'])
        self.assertTrue(result['disabled']['generate'])
        self.assertTrue(self.render({'processes': 1, 'pid': 123}, pending=True)['disabled']['stop_test'])

    def test_real_server_state_shows_latest_status_and_latest_eight_logs(self):
        # Exercise the actual API serialization, not a guessed array order.
        # No Studio initializer, asset I/O, game lookup or background job runs.
        studio = Studio.__new__(Studio)
        studio.assets = SimpleNamespace(refresh=Mock(), maps={}, errors=[])
        studio.game_state = Mock(return_value={'state': 'offline', 'processes': 0})
        studio.lock = threading.RLock()
        studio.workspace = studio.test = studio.mod = HERE
        studio.args = SimpleNamespace(rcon_port=27986)
        studio.selected_map, studio.token, studio.busy, studio.recast = 'obj_overkill', 'fixture', False, None
        studio.jobs = deque()
        for i in range(10):
            studio.jobs.appendleft({'id': f'job{i}', 'action': 'generate' if i == 9 else f'older{i}',
                                    'status': 'failed' if i == 9 else 'complete'})
        state = studio.state()
        self.assertEqual([j['id'] for j in state['jobs']], [f'job{i}' for i in range(10)])
        result = self.render(game=state['game'], jobs=state['jobs'])
        self.assertEqual(result['label'], 'Toolgestützt erzeugen & prüfen · fehlgeschlagen')
        logged = [json.loads(value)['id'] for value in result['log'].split('\n\n')]
        self.assertEqual(logged, [f'job{i}' for i in range(9, 1, -1)])
        self.assertEqual(self.render()['label'], 'Bereit')

    def test_open_settings_get_locked_when_a_game_or_job_starts(self):
        for game, jobs in (({'processes': 1, 'pid': 123}, []), ({'processes': 0}, [{'id':'pending', 'status':'queued'}])):
            with self.subTest(game=game):
                result = self.render(game, jobs, dialog=True)
                self.assertTrue(result['settingsDisabled'])
                self.assertIn('Neu laden', result['settingsInfo'])
        self.assertFalse(self.render({'processes': 0}, dialog=True)['settingsDisabled'])


if __name__ == '__main__':
    unittest.main()
