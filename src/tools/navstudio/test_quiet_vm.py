"""Execute quiet.py in Omni-bot's actual GameMonkey VM, no engine/network hooks."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

import quiet


ROOT=Path(__file__).resolve().parents[3]
DRIVER=ROOT/'build/navstudio-gmvm/gm-quiet-vm.exe'
TOKEN='0123456789abcdef'
BOOTSTRAP='''global Server={MinBots=8,MaxBots=12,BalanceTeams=true,SaveConfigChanges=true};
global BotTable={};global RestoreCount=0;global RestoredBots={};
global AddBot=function(bot){global RestoreCount;global RestoredBots;RestoredBots[RestoreCount]=bot;RestoreCount+=1;};
BotTable[1]={Team=1,Class=2,Name="^1One",GetTeam=function(){return this.Team;},GetClass=function(){return this.Class;}};
BotTable[2]={Team=2,Class=3,Name="Two ' literal",GetTeam=function(){return this.Team;},GetClass=function(){return this.Class;}};
'''


class QuietRealVMTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if os.name!='nt' or not DRIVER.is_file():raise unittest.SkipTest('Run build_quiet_vm.py with existing Omni-bot Win32 library first.')
        metadata=json.loads(DRIVER.with_name('build.json').read_text(encoding='utf8'))
        for key,path in [('source_sha256',Path(__file__).with_name('gm_quiet_vm.cpp')),('executable_sha256',DRIVER)]:
            if metadata[key]!=hashlib.sha256(path.read_bytes()).hexdigest():raise AssertionError('Rebuild the changed GameMonkey VM driver.')

    def run_scripts(self,*scripts):
        with tempfile.TemporaryDirectory(prefix='navstudio-real-gm-') as temporary:
            paths=[]
            for index,script in enumerate(scripts):
                if script=='--reset-vm':paths.append(script);continue
                path=Path(temporary)/(str(index)+'.gm');path.write_text(script,encoding='ascii');paths.append(str(path))
            result=subprocess.run([str(DRIVER),*paths],capture_output=True,text=True,errors='replace',timeout=10,
                                  creationflags=subprocess.CREATE_NO_WINDOW)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            states=[dict(re.findall(r'(\w+)=([^\s]+)',line)) for line in result.stdout.splitlines() if line.startswith('STATE ')]
            return result.stdout,states

    def begin(self,token=TOKEN):return quiet.file_source(quiet.begin_script(token))
    def restore(self,token=TOKEN):return quiet.file_source(quiet.restore_script(token))

    def test_begin_count_gc_global_redeclaration_restore_preserves_exact_snapshot(self):
        output,states=self.run_scripts(BOOTSTRAP,self.begin(),quiet.file_source(quiet.count_script()),
            'global BotTable={};local garbage={};local i=0;for(i=0;i<3000;i+=1){garbage[i]={payload=i};}',
            quiet.file_source(quiet.count_script()),'global NavStudioQuietState;print("PROBE:"+NavStudioQuietState.Token);',
            self.restore(),'print("RESTORE_COUNT:"+RestoreCount);print("NAME_1:"+RestoredBots[1].Name);print("TEAM_CLASS_0:"+RestoredBots[0].Team+":"+RestoredBots[0].Class);')
        for state in states[1:6]:
            self.assertEqual(state['token'],TOKEN);self.assertEqual(state['savedbots'],'2')
            self.assertEqual((state['minbots'],state['maxbots'],state['balance'],state['saveconfig']),('-1','0','0','0'))
        self.assertEqual(states[4]['botcount'],'0')
        self.assertEqual((states[-1]['quiet_type'],states[-1]['minbots'],states[-1]['maxbots'],states[-1]['balance'],states[-1]['saveconfig']),('0','8','12','1','1'))
        self.assertIn('RESTORE_COUNT:2',output);self.assertIn("NAME_1:Two ' literal",output);self.assertIn('TEAM_CLASS_0:1:2',output)

    def test_second_begin_never_replaces_owned_snapshot(self):
        output,states=self.run_scripts(BOOTSTRAP,self.begin(),self.begin('f'*16))
        self.assertIn('NAVSTUDIO_QUIET_BUSY',output);self.assertEqual(states[-1]['token'],TOKEN)

    def test_foreign_token_cannot_restore_or_clear_active_snapshot(self):
        output,states=self.run_scripts(BOOTSTRAP,self.begin(),self.restore('f'*16),'print("RESTORE_COUNT:"+RestoreCount);')
        self.assertIn('NAVSTUDIO_QUIET_TOKEN_MISSING',output);self.assertEqual(states[-1]['token'],TOKEN)
        self.assertIn('RESTORE_COUNT:0',output)

    def test_vm_reset_reproduces_lost_state_without_explicit_restore(self):
        output,states=self.run_scripts(BOOTSTRAP,self.begin(),'--reset-vm',BOOTSTRAP,self.restore())
        self.assertEqual(states[1]['token'],TOKEN);self.assertEqual(states[2]['quiet_type'],'0')
        self.assertIn('NAVSTUDIO_QUIET_NOT_ACTIVE',output);self.assertNotIn('NAVSTUDIO_QUIET_RESTORED',output)


if __name__=='__main__':unittest.main()
