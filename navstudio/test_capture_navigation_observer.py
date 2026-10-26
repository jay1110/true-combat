"""Real capture caller rejects follow-state overwrites and camera drift.

No game, API or screenshot tool is started. Literal RCON replies independently
model the observer's actual position before and after each recorded image.
"""
import copy
import json
import os
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock

from server import Studio


class NavigationObserverCaptureTests(unittest.TestCase):
    def fixture(self, count=4, change=None):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup)
        studio=Studio.__new__(Studio);studio.mod=Path(temp.name)
        studio.work=studio.mod;studio.test=studio.mod;studio.game_executable=studio.mod/'ET.exe'
        identity=[os.path.normcase(str(studio.game_executable.resolve())),1234]
        studio.process_api=SimpleNamespace(identity=Mock(return_value=identity))
        studio.own_test_process=Mock(return_value={'pid':12,'owns_port':True})
        studio.live_gametype=Mock(return_value=5)
        (studio.mod/'screenshots').mkdir()
        studio.active_project={'id':'01234567fixture'}
        studio.require_loaded=Mock();studio.update=Mock()
        studio.proof_status=Mock(return_value={'valid':True})
        nodes=[dict(id=i,position=[i*100,0,24],accepted=True,spawn_reachable=True,nav_flags=0) for i in range(count)]
        report=dict(generation_complete=True,nodes=nodes,
                    edges=[{'from':i,'to':(i+1)%count,'accepted':True} for i in range(count)])
        studio.native=Mock(return_value=report)
        commands=[];state={'position':[-100.,-100.,30.],'probe':0,'jpeg':0,'slot':0,'team':3,'hold':0}
        def command(text,**kwargs):
            commands.append(text)
            if text=='tce_navgen_authoring': return 'NAVSTUDIO_AUTHORING active='+str(state['hold'])+'\n'
            if text=='tce_navgen_authoring on': state['hold']=1;return 'NAVSTUDIO_AUTHORING_ON\n'
            if text=='tce_navgen_authoring off': state['hold']=0;return 'NAVSTUDIO_AUTHORING_OFF\n'
            if text=='navprobe':
                state['probe']+=1
                current=copy.deepcopy(state)
                if change: change(current,commands)
                position=' '.join(str(v) for v in current['position'])
                observer=(f"NAVPOS {current['slot']} {position} 100 {current['team']} NavStudioObserver\n"
                          if not current.get('missing') else '')
                if current.get('duplicate'): observer+=observer
                return f"NAVSTATE 0 GT {current.get('gametype',5)}\n"+observer+'NAVPOS 1 50 50 20 100 1 Chan\n'
            if text.startswith('tce_navgen_camera '):
                x,y,z,height=map(float,text.split()[1:])
                return 'navgen camera: '+json.dumps({'accepted':True,'map':'obj_test','position':[x,y,z+height]})+'\n'
            if text.startswith('navcamera '):
                state['position']=list(map(float,text.split()[1:4]))
                return 'NAVCAMERA local spectator moved; bot positions unchanged\n'
            if text.startswith('screenshotJPEG '):
                state['jpeg']+=1
                path=studio.mod/'screenshots'/(text.split()[1]+'.jpg')
                path.write_bytes(b'fixture image bytes')
                return 'Wrote screenshots/'+path.name+'\n'
            if text in ('follow','bot waypoint_view on'): return ''
            raise AssertionError('Unexpected mutation: '+text)
        studio.rcon=Mock(side_effect=command)
        studio.rcon_with_followup=Mock(side_effect=command)
        def script(command,saved,stage):
            commands.append(stage)
            return ('NAVSTUDIO_OVERVIEW_DRAW_RESTORED:' if stage.endswith('restore') else
                    'NAVSTUDIO_OVERVIEW_DRAW_OFF:')+saved['token']
        studio.run_quiet_script=Mock(side_effect=script)
        return studio,commands,state

    def capture(self, studio):
        return studio.action_capture_navigation(dict(map='obj_test',gametype=5))

    def test_each_image_has_unfollow_then_checked_camera_and_two_independent_pose_proofs(self):
        studio,commands,state=self.fixture()
        result=self.capture(studio)
        self.assertEqual(len(result['screenshots']),4)
        self.assertEqual(state['probe'],9)
        self.assertEqual(commands[:5],['tce_navgen_authoring','tce_navgen_authoring on','overview-begin','navprobe','bot waypoint_view on'])
        for number,item in enumerate(result['screenshots']):
            calls=commands[5+number*6:5+(number+1)*6]
            self.assertEqual(calls[0],'follow')
            self.assertTrue(calls[1].startswith('tce_navgen_camera '))
            self.assertTrue(calls[2].startswith('navcamera '))
            self.assertEqual(calls[3],'navprobe')
            self.assertTrue(calls[4].startswith('screenshotJPEG '))
            self.assertEqual(calls[5],'navprobe')
            for key in ('observer_before_screenshot','observer_after_screenshot'):
                proof=item[key]
                self.assertIn('NAVPOS 0 ',proof['reply'])
                self.assertEqual(proof['observer']['name'],'NavStudioObserver')
                self.assertEqual(proof['observer']['team'],3)
                self.assertEqual(proof['observer']['slot'],0)
                self.assertEqual(proof['observer']['position'],item['camera'][:3])
                self.assertEqual(proof['observer']['distance_from_checked_camera'],0)
        self.assertEqual(commands[-2:],['overview-restore','tce_navgen_authoring off'])
        self.assertEqual(state['hold'],0)
        self.assertFalse(result['overview_capture_state']['authoring_acquired'])
        self.assertFalse(result['overview_capture_state']['draw_restore_pending'])

    def test_acknowledged_move_still_following_player_is_rejected_before_jpeg(self):
        def change(row,commands):
            if row['probe']==2: row['position']=[50.,50.,20.]
        studio,commands,state=self.fixture(change=change)
        with self.assertRaisesRegex(RuntimeError,'folgt noch einem Spieler'):
            self.capture(studio)
        self.assertEqual(state['jpeg'],0)
        self.assertTrue(any(c.startswith('navcamera ') for c in commands))

    def test_missing_duplicate_wrong_team_nonfinite_or_wrong_gametype_observer_stops_before_camera(self):
        cases=({'missing':True},{'duplicate':True},{'team':1},
               {'position':[float('nan'),0,0]},{'gametype':2})
        for fields in cases:
            with self.subTest(fields=fields):
                studio,commands,state=self.fixture(change=lambda row,commands:row.update(fields))
                with self.assertRaisesRegex(RuntimeError,'nicht eindeutig bestätigt'):
                    self.capture(studio)
                self.assertEqual(commands,['tce_navgen_authoring','tce_navgen_authoring on','overview-begin','navprobe','overview-restore','tce_navgen_authoring off'])
                self.assertEqual(state['jpeg'],0)

    def test_observer_must_keep_same_slot_before_and_after_screenshot(self):
        for probe in (2,3):
            with self.subTest(probe=probe):
                def change(row,commands):
                    if row['probe']==probe: row['slot']=2
                studio,_,state=self.fixture(change=change)
                with self.assertRaisesRegex(RuntimeError,'nicht eindeutig bestätigt'):
                    self.capture(studio)
                self.assertEqual(state['jpeg'],0 if probe==2 else 1)

    def test_new_round_or_user_camera_move_after_jpeg_does_not_produce_success(self):
        def change(row,commands):
            if row['probe']==3: row['position'][0]+=200
        studio,_,state=self.fixture(change=change)
        with self.assertRaisesRegex(RuntimeError,'Aufnahme nicht bestätigt'):
            self.capture(studio)
        self.assertEqual(state['jpeg'],1)

    def test_two_unit_three_dimensional_bound_is_enforced(self):
        for distance,accepted in ((2.,True),(2.001,False)):
            with self.subTest(distance=distance):
                def change(row,commands):
                    if row['probe']>1: row['position'][2]+=distance
                studio,_,state=self.fixture(count=1,change=change)
                if accepted:
                    result=self.capture(studio)
                    self.assertEqual(result['screenshots'][0]['observer_before_screenshot']['observer']['distance_from_checked_camera'],2.)
                else:
                    with self.assertRaisesRegex(RuntimeError,'Aufnahme nicht bestätigt'):
                        self.capture(studio)
                    self.assertEqual(state['jpeg'],0)

    def test_existing_hold_is_neither_renewed_nor_released(self):
        studio,commands,state=self.fixture(count=1);state['hold']=1
        result=self.capture(studio)
        self.assertTrue(result['overview_capture_state']['authoring_prior_active'])
        self.assertEqual(state['hold'],1)
        self.assertNotIn('tce_navgen_authoring on',commands)
        self.assertNotIn('tce_navgen_authoring off',commands)
        self.assertEqual(commands[-1],'overview-restore')

    def test_original_et_does_not_issue_tce_authoring_commands(self):
        studio,commands,_=self.fixture(count=1);studio.game_mode='et'
        result=self.capture(studio)
        self.assertEqual(len(result['screenshots']),1)
        self.assertFalse(any(c.startswith('tce_navgen_authoring') for c in commands))

    def test_round_reset_fixture_only_occurs_without_authoring_hold(self):
        def change(row,commands):
            if row['probe']==3 and not row['hold']: row['position']=[1300,1200,80]
        studio,_,state=self.fixture(count=2,change=change)
        self.assertEqual(len(self.capture(studio)['screenshots']),2)
        self.assertEqual(state['hold'],0)

    def test_camera_failure_restores_draw_flags_then_releases_new_hold(self):
        studio,commands,state=self.fixture(change=lambda row,_:row['position'].__setitem__(0,9999) if row['probe']==2 else None)
        with self.assertRaisesRegex(RuntimeError,'Aufnahme nicht bestätigt'): self.capture(studio)
        self.assertEqual(commands[-2:],['overview-restore','tce_navgen_authoring off'])
        self.assertEqual(state['hold'],0)

    def test_draw_restore_failure_still_releases_hold_and_fails_capture(self):
        studio,commands,state=self.fixture(count=1)
        original=studio.run_quiet_script.side_effect
        studio.run_quiet_script.side_effect=lambda command,saved,stage: 'VM lost its snapshot' if stage.endswith('restore') else original(command,saved,stage)
        with self.assertRaisesRegex(RuntimeError,'Goal-Anzeige nach Übersicht'): self.capture(studio)
        self.assertEqual(commands[-1],'tce_navgen_authoring off')
        self.assertEqual(state['hold'],0)

    def test_ambiguous_hold_acquisition_is_released_without_photograph(self):
        studio,commands,state=self.fixture(count=1)
        def mutation(command):
            commands.append(command)
            if command.endswith('on'): state['hold']=1;return 'Transporthinweis: acknowledgement lost'
            if command.endswith('off'): state['hold']=0;return 'NAVSTUDIO_AUTHORING_OFF'
            raise AssertionError(command)
        studio.rcon_with_followup.side_effect=mutation
        with self.assertRaisesRegex(RuntimeError,'Rundenhalt für Übersicht'): self.capture(studio)
        self.assertEqual(state['jpeg'],0)
        self.assertEqual(commands[-1],'tce_navgen_authoring off')
        self.assertEqual(state['hold'],0)
        studio.run_quiet_script.assert_not_called()

    def test_foreign_creation_identity_gets_no_restore_or_hold_release_mutation(self):
        studio,commands,_=self.fixture(count=1)
        original=studio.run_quiet_script.side_effect
        def script(command,saved,stage):
            result=original(command,saved,stage)
            if stage=='overview-begin': studio.process_api.identity.return_value=[saved['identity'][0],5678]
            return result
        studio.run_quiet_script.side_effect=script
        with self.assertRaisesRegex(RuntimeError,'Testprozess wurde ersetzt'): self.capture(studio)
        self.assertNotIn('overview-restore',commands)
        self.assertNotIn('tce_navgen_authoring off',commands)

    def test_foreign_draw_snapshot_is_not_replaced_or_restored(self):
        studio,commands,state=self.fixture(count=1)
        studio.run_quiet_script.side_effect=lambda *args:'NAVSTUDIO_OVERVIEW_DRAW_BUSY'
        with self.assertRaisesRegex(RuntimeError,'frühere Übersicht'): self.capture(studio)
        studio.run_quiet_script.assert_called_once()
        self.assertEqual(state['hold'],0)

    def test_unknown_prior_hold_aborts_before_mutation(self):
        studio,commands,state=self.fixture(count=1)
        studio.rcon.side_effect=lambda *args,**kwargs:'NAVSTUDIO_AUTHORING active=0\nNAVSTUDIO_AUTHORING active=1\n'
        with self.assertRaisesRegex(RuntimeError,'nicht eindeutig bestätigt'): self.capture(studio)
        studio.rcon_with_followup.assert_not_called()
        studio.run_quiet_script.assert_not_called()
        self.assertEqual(state['jpeg'],0)


if __name__=='__main__':
    unittest.main()
