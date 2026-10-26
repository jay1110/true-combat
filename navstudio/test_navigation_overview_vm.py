"""Execute the actual temporary overview draw-state scripts in Omni's GM VM.

The standalone VM supplies table goals with the real bound property names;
it does not run an engine, API, RCON or alter a gameplay file.
"""
import unittest

from server import Studio
import test_quiet_vm as quiet_vm

BOOTSTRAP=quiet_vm.BOOTSTRAP
TOKEN=quiet_vm.TOKEN


class NavigationOverviewRealVMTests(unittest.TestCase):
    setUpClass=classmethod(quiet_vm.QuietRealVMTests.setUpClass.__func__)
    run_scripts=quiet_vm.QuietRealVMTests.run_scripts

    def script(self,token=TOKEN,restore=False):
        return Studio.navigation_overview_draw_script(token,restore)[len('bot script_run '):].replace("'",'"')

    def goals(self):
        return BOOTSTRAP+'''global FixtureGoals={
            {SerialNum=101,RenderGoal=true,RenderRoutes=false},
            {SerialNum=202,RenderGoal=false,RenderRoutes=true},
            {SerialNum=303,RenderGoal=true,RenderRoutes=true}};
            global GetGoals=function(out){foreach(id and g in FixtureGoals){out[id]=g;}};
            global DumpDraw=function(){foreach(id and g in FixtureGoals){
                print("DRAW:"+g.SerialNum+":"+g.RenderGoal+":"+g.RenderRoutes);}};'''

    def test_mixed_per_goal_draw_state_is_restored_and_new_goal_is_untouched(self):
        output,_=self.run_scripts(self.goals(),self.script(),'DumpDraw();',
            'FixtureGoals[3]={SerialNum=404,RenderGoal=true,RenderRoutes=false};',
            self.script(restore=True),'DumpDraw();')
        for marker in ('DRAW:101:0:0','DRAW:202:0:0','DRAW:303:0:0',
                       'DRAW:101:1:0','DRAW:202:0:1','DRAW:303:1:1','DRAW:404:1:0'):
            self.assertIn(marker,output)
        self.assertIn('NAVSTUDIO_OVERVIEW_DRAW_OFF:'+TOKEN,output)
        self.assertIn('NAVSTUDIO_OVERVIEW_DRAW_RESTORED:'+TOKEN,output)

    def test_foreign_token_cannot_replace_restore_or_clear_active_snapshot(self):
        output,_=self.run_scripts(self.goals(),self.script(),self.script('f'*16),
            self.script('f'*16,True),'print("OWNER:"+NavStudioOverviewDrawState.Token);',
            self.script(restore=True),'DumpDraw();')
        for marker in ('NAVSTUDIO_OVERVIEW_DRAW_BUSY','NAVSTUDIO_OVERVIEW_DRAW_NOT_OWNED',
                       'OWNER:'+TOKEN,'DRAW:101:1:0','DRAW:202:0:1','DRAW:303:1:1'):
            self.assertIn(marker,output)

    def test_deleted_goal_and_replacement_serial_never_receive_old_flags(self):
        output,_=self.run_scripts(self.goals(),self.script(),
            'FixtureGoals[0]={SerialNum=999,RenderGoal=false,RenderRoutes=true};',
            self.script(restore=True),'DumpDraw();')
        self.assertIn('DRAW:999:0:1',output)
        self.assertIn('DRAW:202:0:1',output)
        self.assertIn('DRAW:303:1:1',output)


if __name__=='__main__': unittest.main()
