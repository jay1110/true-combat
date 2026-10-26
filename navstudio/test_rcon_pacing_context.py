"""Exact-engine pacing and same-call native state; no sockets or game started."""
import copy
import json
from pathlib import Path
import socket
import sys
from types import SimpleNamespace
from unittest import TestCase,main
from unittest.mock import Mock,patch

import server
import test_objective_anchor_workflow as workflow_fixture
sys.path.insert(0,str(Path(__file__).resolve().parent.parent))
from repair import parse_native_probe


KNOWN='94e8aa5b52a05dbb240811616d1b8766dfbc95f02991137e3e1e54f6b27880ed'
ENDPOINT=('127.0.0.1',27986)


def context(profile=1,gametype=2,gravity=800.):
    body=server.MOVEMENT_PROFILES[profile]
    return dict(schema=1,map='obj_test',gametype=gametype,g_newbbox=profile,
                bbox=dict(half_width=body['radius'],stand_height=body['standing_height'],
                          crouch_height=body['crouching_height'],origin_above_feet=body['origin_above_feet']),gravity=gravity)


def probe_reply(current=None,start=(0,0,.25),end=(128,0,.25)):
    vector=lambda p:'('+' '.join('%.5f'%v for v in p)+')'
    result='navgen probe context: '+json.dumps(current or context())+'\n'
    result+='navgen probe -1->-1 feet='+vector(start)+'->'+vector(end)+' stand=accepted crouch=accepted traces=18 quarantined=0/0 (diagnostic only; graph unchanged)\n'
    for endpoint,point in enumerate((start,end)):
        for posture in (0,1):
            result+='navgen probe import endpoint=%d crouch=%d accepted=1 exact=1 reason=accepted feet=%s\n'%(endpoint,posture,vector(point))
    return result


class FakeClock:
    def __init__(self):self.now=100.;self.sleeps=[]
    def monotonic(self):return self.now
    def sleep(self,seconds):
        if seconds<0:raise AssertionError('negative sleep')
        self.sleeps.append(seconds);self.now+=seconds


class FakeSocket:
    def __init__(self,network,replies):self.network=network;self.replies=list(replies);self.timeout=0.
    def __enter__(self):return self
    def __exit__(self,*args):return False
    def settimeout(self,seconds):self.timeout=seconds
    def sendto(self,packet,endpoint):self.network.sends.append((self.network.clock.now,packet,endpoint))
    def recvfrom(self,size):
        if size!=65535:raise AssertionError('UDP receive unexpectedly truncates the engine payload')
        if self.replies:return self.replies.pop(0)
        self.network.clock.now+=self.timeout
        raise socket.timeout()


class FakeNetwork:
    def __init__(self,clock,exchanges):self.clock=clock;self.exchanges=list(exchanges);self.sends=[]
    def socket(self,*args):
        if args!=(socket.AF_INET,socket.SOCK_DGRAM):raise AssertionError('Unexpected socket type')
        if not self.exchanges:raise AssertionError('Unexpected extra network call')
        return FakeSocket(self,self.exchanges.pop(0))


class RconPacingTests(TestCase):
    def studio(self,digest=KNOWN):
        instance=server.Studio.__new__(server.Studio)
        instance.game_executable=Path('fixture-engine.exe')
        instance.file_hash=Mock(return_value=digest)
        instance.args=SimpleNamespace(rcon_port=27986)
        instance.test=Path('fixture-game');instance.rcon_last=0.
        return instance

    def test_only_exact_known_canonical_engine_digest_selects_short_interval(self):
        self.assertEqual(server.rcon_min_interval(KNOWN),.55)
        for value in (KNOWN.upper(),KNOWN+' ',KNOWN[:-1]+'0','',None,True,1,[],{},set(),b'known'):
            with self.subTest(value=value):self.assertEqual(server.rcon_min_interval(value),2.1)

    def test_current_executable_hash_is_rechecked_and_missing_or_failed_reads_stay_conservative(self):
        instance=self.studio();instance.file_hash.side_effect=[KNOWN,'0'*64,KNOWN,None,OSError('unavailable')]
        self.assertEqual([instance.rcon_interval()for _ in range(5)],[.55,2.1,.55,2.1,2.1])
        self.assertEqual(instance.file_hash.call_count,5)
        for call in instance.file_hash.call_args_list:self.assertEqual(call.args,(Path('fixture-engine.exe'),))
        del instance.game_executable
        self.assertEqual(instance.rcon_interval(),2.1)
        self.assertEqual(instance.file_hash.call_count,5)

    def transport(self,digest,exchanges,callback):
        instance=self.studio(digest);clock=FakeClock();network=FakeNetwork(clock,exchanges)
        with patch('server.time.monotonic',side_effect=clock.monotonic),patch('server.time.sleep',side_effect=clock.sleep),\
             patch('server.socket.socket',side_effect=network.socket),patch.dict('server.os.environ',{'TCE_NAVSTUDIO_RCON_PASSWORD':'fixture'}):
            result=callback(instance)
        self.assertEqual(network.exchanges,[])
        self.assertTrue(all(row[2]==ENDPOINT for row in network.sends))
        return result,clock,network

    def test_two_real_rcon_transport_calls_share_one_pacing_deadline_without_double_wait(self):
        packet=(b'\xff\xff\xff\xffprint\nreply\n',ENDPOINT)
        for digest,interval in ((KNOWN,.55),('0'*64,2.1)):
            with self.subTest(digest=digest):
                result,clock,network=self.transport(digest,[[packet],[packet]],lambda s:[s.rcon('mapname',True),s.rcon('g_gametype',True)])
                self.assertEqual(result,['reply\n','reply\n'])
                self.assertEqual(len(network.sends),2)
                self.assertAlmostEqual(network.sends[1][0]-network.sends[0][0],.8+interval,places=8)
                self.assertAlmostEqual(sum(clock.sleeps),interval,places=8)
                self.assertIn(b'rcon fixture mapname',network.sends[0][1])

    def test_readonly_retry_uses_same_exact_engine_interval_and_mutations_are_not_retried(self):
        packet=(b'\xff\xff\xff\xffprint\nsecond answer',ENDPOINT)
        result,clock,network=self.transport(KNOWN,[[],[packet]],lambda s:s.rcon('mapname',True))
        self.assertEqual(result,'second answer');self.assertEqual(len(network.sends),2)
        self.assertAlmostEqual(network.sends[1][0]-network.sends[0][0],1.35,places=8)
        self.assertAlmostEqual(sum(clock.sleeps),.55,places=8)
        result,_,network=self.transport(KNOWN,[[]],lambda s:s.rcon('fixture_mutation'))
        self.assertEqual(result,'');self.assertEqual(len(network.sends),1)

    def test_public_poll_and_authenticated_request_share_the_transport_spacing(self):
        status=(b'statusResponse\nmapname obj_test',ENDPOINT)
        authenticated=(b'\xff\xff\xff\xffprint\nanswer',ENDPOINT)
        def calls(instance):
            public=instance.udp_exchange(b'\xff\xff\xff\xffgetstatus\n',.3)
            return public,instance.rcon('mapname',True)
        result,clock,network=self.transport(KNOWN,[[status],[authenticated]],calls)
        self.assertEqual(result,('statusResponse\nmapname obj_test','answer'))
        self.assertAlmostEqual(network.sends[1][0]-network.sends[0][0],.55,places=8)
        self.assertAlmostEqual(sum(clock.sleeps),.55,places=8)

    def test_many_native_udp_chunks_preserve_one_complete_context_and_all_import_rows(self):
        reply=probe_reply();raw=reply.encode('ascii')
        packets=[(b'foreign',('127.0.0.2',27986))]
        packets +=[(b'\xff\xff\xff\xffprint\n'+raw[i:i+239],ENDPOINT)for i in range(0,len(raw),239)]
        result,_,network=self.transport(KNOWN,[packets],lambda s:s.rcon('tce_navgen_probe feet 0 0 .25 128 0 .25',True))
        self.assertEqual(result,reply);self.assertEqual(len(network.sends),1)
        self.assertEqual(server.checked_probe_context(result,'obj_test',2,server.MOVEMENT_PROFILES[1],800.),context())
        parsed=parse_native_probe(result,[0,0,.25],[128,0,.25],30)
        self.assertEqual(parsed['stand'],'accepted')
        self.assertTrue(all(parsed['imports'][endpoint][posture]['exact']for endpoint in('start','end')for posture in('stand','crouch')))


class ProbeContextTests(TestCase):
    def check(self,reply,profile=1,gametype=2,gravity=800.):
        return server.checked_probe_context(reply,'obj_test',gametype,server.MOVEMENT_PROFILES[profile],gravity)

    def test_same_reply_binds_both_body_profiles_gametypes_and_fractional_gravity(self):
        for profile in (0,1):
            for gt in (2,5,7):
                current=context(profile,gt,812.375)
                with self.subTest(profile=profile,gt=gt):
                    self.assertEqual(self.check(probe_reply(current),profile,gt,812.375),current)

    def test_wrong_or_missing_context_cannot_authorize_an_accepted_body_probe(self):
        good=probe_reply()
        for reply in ('\n'.join(good.splitlines()[1:]),good+good.splitlines()[0]+'\n',good.replace('context: {','context: broken{'),
                      'navgen probe context: null\n','navgen probe context: []\n',b'not unicode',None):
            with self.subTest(reply=reply),self.assertRaises(server.ProcessIdentityChanged):self.check(reply)

    def test_identity_body_schema_and_gravity_mismatches_are_fail_closed(self):
        mutations=(dict(schema=2),dict(schema=True),dict(map='obj_other'),dict(gametype=5),dict(gametype=2.),
                   dict(g_newbbox=0),dict(g_newbbox=True),dict(gravity=801),dict(gravity='800'),dict(gravity=True),
                   dict(gravity=float('nan')),dict(gravity=float('inf')),dict(unexpected=0))
        for change in mutations:
            current=context();current.update(change)
            with self.subTest(change=change),self.assertRaises(server.ProcessIdentityChanged):self.check(probe_reply(current))
        for key in context():
            current=context();del current[key]
            with self.subTest(missing=key),self.assertRaises(server.ProcessIdentityChanged):self.check(probe_reply(current))
        for key in context()['bbox']:
            for value in ('16',True,None,float('nan'),float('inf'),-1,999):
                current=context();current['bbox'][key]=value
                with self.subTest(key=key,value=value),self.assertRaises(server.ProcessIdentityChanged):self.check(probe_reply(current))
        for change in ('missing','extra'):
            current=context()
            if change=='missing':del current['bbox']['half_width']
            else:current['bbox']['extra']=0
            with self.subTest(change=change),self.assertRaises(server.ProcessIdentityChanged):self.check(probe_reply(current))

    def test_duplicate_json_fields_and_invalid_expected_physics_are_rejected(self):
        good=probe_reply()
        for reply in (good.replace('"schema": 1','"schema": 1, "schema": 1'),
                      good.replace('"half_width": 16','"half_width": 16, "half_width": 16')):
            with self.subTest(reply=reply),self.assertRaises(server.ProcessIdentityChanged):self.check(reply)
        for gravity in (None,True,'800',float('nan'),float('inf')):
            with self.subTest(gravity=gravity),self.assertRaises(server.ProcessIdentityChanged):self.check(good,gravity=gravity)
        wrong=dict(server.MOVEMENT_PROFILES[1],radius=16.01)
        with self.assertRaises(server.ProcessIdentityChanged):server.checked_probe_context(good,'obj_test',2,wrong,800.)


class AnchorSameReplyTests(TestCase):
    def fixture(self):
        fixture=workflow_fixture.ObjectiveAnchorVisitTests();self.addCleanup(fixture.doCleanups)
        studio,job,events,reconciler=fixture.fixture(2)
        studio.require_loaded=Mock(side_effect=AssertionError('No extra per-query map RCON'))
        studio.live_gametype=Mock(side_effect=AssertionError('No extra per-query GT RCON'))
        replies=[]
        def rcon(command,read_only=False):
            self.assertTrue(read_only);self.assertTrue(command.startswith('tce_navgen_probe feet '))
            values=[float(v)for v in command.split()[2:]];self.assertEqual(len(values),6)
            reply=probe_reply(context(gametype=job['gametype']),values[:3],values[3:]);replies.append(reply)
            return reply
        studio.rcon=Mock(side_effect=rcon)
        checked=[]
        def observe(gt,snapshot,evidence,probe):
            reconciler.visits.append(dict(gametype=gt))
            for start,end in (([0,0,.25],[0,0,.25]),([0,0,.25],[128,0,.25])):
                proof=probe(start,end);checked.append(proof);reconciler.queries.append(dict(proof=proof))
        reconciler.observe.side_effect=observe
        return studio,job,events,reconciler,checked,replies

    def run_reconcile(self,studio,job,reconciler):
        with patch('objectiveanchors.archive_anchors',return_value=[dict(source_id=1)]),\
             patch('objectiveanchors.Reconciler',return_value=reconciler),patch('goalgen.objectives',return_value=[]):
            return studio.reconcile_objective_anchors(job)

    def test_each_probe_uses_one_context_bound_reply_and_retains_phase_snapshots_and_pid_checks(self):
        studio,job,events,reconciler,checked,replies=self.fixture()
        result=self.run_reconcile(studio,job,reconciler)
        self.assertEqual(len(checked),6);self.assertEqual(studio.rcon.call_count,6)
        self.assertEqual([p['context']['gametype']for p in checked],[2,2,5,5,2,2])
        self.assertTrue(all(p['raw_reply']==raw for p,raw in zip(checked,replies)))
        self.assertEqual([gt for action,gt in events if action=='snapshot'],[2,2,5,5,2,2])
        self.assertEqual(studio.process_api.identity.call_count,3)
        self.assertEqual(studio.own_test_process.call_count,4)
        self.assertEqual(studio.process_api.probe_owner.call_count,6)
        for call in studio.process_api.probe_owner.call_args_list:
            self.assertEqual(call.args,(12,['fixture-game',123]))
            self.assertEqual(call.kwargs,dict(expected_exe=studio.game_executable,port=27986))
        self.assertEqual(result['restored_gametype'],2)
        self.assertNotIn('quiet_generation',job)
        studio.require_loaded.assert_not_called();studio.live_gametype.assert_not_called()
        studio.drafts.apply_repair.assert_not_called()

    def test_mismatching_same_call_context_aborts_before_second_probe_or_certificate(self):
        studio,job,_,reconciler,checked,_=self.fixture()
        studio.rcon.return_value=probe_reply(context(gametype=5));studio.rcon.side_effect=None
        with self.assertRaises(server.ProcessIdentityChanged):self.run_reconcile(studio,job,reconciler)
        self.assertEqual(studio.rcon.call_count,1);self.assertEqual(checked,[])
        self.assertFalse((studio.work/'obj_test.objective-anchors.json').exists())
        self.assertEqual(json.loads((studio.work/'obj_test.objective-anchors-failed.json').read_bytes())['status'],'failed')
        self.assertNotIn('quiet_generation',job)
        studio.drafts.apply_repair.assert_not_called()

    def test_pid_or_creation_identity_change_fails_before_sending_the_probe(self):
        for mode in ('pid','identity','port','duplicate'):
            studio,job,_,reconciler,checked,_=self.fixture()
            studio.process_api.probe_owner.side_effect=server.ProcessIdentityChanged(mode+' changed')
            with self.subTest(mode=mode),self.assertRaises(server.ProcessIdentityChanged):self.run_reconcile(studio,job,reconciler)
            studio.rcon.assert_not_called();self.assertEqual(checked,[])
            self.assertFalse((studio.work/'obj_test.objective-anchors.json').exists())
            self.assertNotIn('quiet_generation',job)


if __name__=='__main__':main()
