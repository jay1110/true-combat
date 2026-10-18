"""Serial project-generation trials in the single configured visible game."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time
from urllib.request import Request, urlopen


class Client:
    def __init__(self, port): self.url = f'http://127.0.0.1:{port}'
    def get(self, route):
        with urlopen(self.url + route, timeout=60) as response: return json.load(response)
    def post(self, route, value):
        token = self.get('/api/state')['token']
        request = Request(self.url + route, data=json.dumps(value).encode(),
                          headers={'Content-Type': 'application/json', 'X-Navstudio-Token': token})
        with urlopen(request, timeout=120) as response: return json.load(response)
    def action(self, action, name, gt, output, timeout=1800):
        start = time.monotonic()
        result = self.post('/api/action', dict(action=action, map=name, gametype=gt))
        identity = result['job']['id']; last = None
        while time.monotonic() - start < timeout:
            state = self.get('/api/state')
            job = next((j for j in state['jobs'] if j['id'] == identity), None)
            if job is None: raise RuntimeError('Project/job changed while observing ' + identity)
            status = (job['status'], job['progress'])
            if status != last:
                print(name, gt, action, *status, flush=True); last = status
            # A job may already have its result while its finally block still
            # restores bot population. Never dispatch another mutation early.
            if job['status'] in ('complete', 'failed') and not state['busy']:
                value = dict(job); value['log'] = value.get('log', '')[-6000:]
                (output / (action + '-gt' + str(gt) + '-' + identity + '.json')).write_text(
                    json.dumps(value, ensure_ascii=False, indent=2), encoding='utf8')
                return job
            time.sleep(4)
        raise TimeoutError('Job still running; resume observation, do not submit twice: ' + identity)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--selection', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--port', type=int, default=8766)
    parser.add_argument('--goals', action='store_true')
    parser.add_argument('--live-seconds', type=int, default=0,
                        help='Observe combat on each successfully applied gametype; 0 skips live runs.')
    parser.add_argument('--skip', type=int, default=0)
    args = parser.parse_args()
    if args.live_seconds and not 15 <= args.live_seconds <= 3600:
        parser.error('--live-seconds must be 15..3600 or 0')
    client = Client(args.port); args.out.mkdir(parents=True, exist_ok=True)
    for attempt in range(30):
        try: client.get('/api/state'); break
        except OSError:
            if attempt==29: raise
            time.sleep(2)

    summary_path = args.out / 'summary.json'
    summary = json.loads(summary_path.read_text()) if summary_path.exists() else []
    selection = json.loads(args.selection.read_text())
    for number, chosen in enumerate(selection[args.skip:], args.skip + 1):
        name = chosen['map'].lower(); output = args.out / name; output.mkdir(exist_ok=True)
        state = client.get('/api/state')
        if state['busy']: raise RuntimeError('Wait for active job before starting another batch.')
        projects = client.get('/api/projects')
        if projects['active'] and projects['active']['map'] != name:
            client.post('/api/project', {'op': 'save_close'})
            projects = client.get('/api/projects')
        if not projects['active']:
            existing = next((p for p in projects['projects'] if p['map'] == name and
                             p['name'] == name + ' - Autotest'), None)
            if existing: client.post('/api/project', {'op': 'open', 'id': existing['id']})
            else:
                source = next(c for c in projects['catalog'] if c['map'] == name and c['filename'] == chosen['archive'])
                client.post('/api/project', dict(op='create', name=name + ' - Autotest',
                    map=name, mod='tce2', gametype=2, source_id=source['source_id']))
        active = client.get('/api/projects')['active']
        row = dict(map=name, project_id=active['id'], source=chosen['archive'], sequence=number,
                   complete=False, gametypes={}, screenshots=[])
        print(f'[{number}/{len(selection)}] {name} project {active["id"]}', flush=True)
        try:
            generated = client.action('generate', name, 2, output)
            row['generation'] = generated['status']; row['error'] = generated.get('error')
            if generated['status'] == 'complete':
                # Keep both goal proposals in every map project, even when
                # navigation still needs repair. Deployment remains gated by
                # the complete, identical installed native graph below.
                goals = None
                if args.goals:
                    goals = client.action('generate_goals', name, 2, output)
                    row['goals_generation'] = goals['status']; row['goals_error'] = goals.get('error')
                installed = client.action('install_test', name, 2, output)
                row['installation'] = installed['status']
                row['subset'] = bool((installed.get('result') or {}).get('test_subset'))
                if installed['status']=='complete':
                    captured=client.action('capture_navigation',name,2,output)
                    row['screenshots']=(captured.get('result') or {}).get('screenshots',[])
                    row['capture_error']=captured.get('error')
                if goals and installed['status']=='complete':
                    if goals['status'] == 'complete':
                        for gt in (2, 5):
                            applied = client.action('apply_goals', name, gt, output)
                            row['gametypes'][str(gt)] = dict(status=applied['status'], error=applied.get('error'))
                            if args.live_seconds and applied['status']=='complete':
                                stamp=str(time.time_ns())
                                liveout=output/('live-gt'+str(gt)+'-'+stamp)
                                native=Path(active['paths']['native'])/(name+'_navmesh.json')
                                proof=Path(active['paths']['work'])/(name+'.native-proof.json')
                                runner=Path(__file__).resolve().parent.parent/'tce_nav_livecheck.py'
                                observed=subprocess.run([sys.executable,str(runner),'--map',name,
                                    '--seconds',str(args.live_seconds),'--bots','8','--out',str(liveout),
                                    '--report',str(native),'--way',str(native.with_suffix('.way')),
                                    '--proof',str(proof)]+(['--existing-bots'] if gt==5 else []),capture_output=True,text=True)
                                record=json.loads((liveout/'result.json').read_text()) if (liveout/'result.json').exists() else {}
                                row['gametypes'][str(gt)]['live']=dict(returncode=observed.returncode,
                                    observation_complete=record.get('complete',False),
                                    result=str(liveout/'result.json'),
                                    stderr=observed.stderr[-1200:],
                                    failures=len(record.get('failures',[])),suicides=len(record.get('suicides',[])),
                                    errors=len(record.get('errors',[])),screenshots=record.get('screenshots',[]),
                                    mission_completion=record.get('objective_completions',[]),visually_reviewed=False)
                                print(name,gt,'live observation',observed.returncode,flush=True)
                row['native_only'] = True
                row['complete'] = False  # only a visible bot traversal/objective check can certify this
            summary = [r for r in summary if r['map'] != name] + [row]
            summary_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf8')
        finally:
            # Existing game is stopped before returning the exact leased PK3.
            if not client.get('/api/state')['busy']: client.post('/api/project', {'op': 'save_close'})
        if (args.out/'stop-after-current').is_file():
            print('Checkpoint requested; current project saved and map returned.',flush=True)
            return
    print('Trials recorded:', summary_path, flush=True)


if __name__ == '__main__': main()
