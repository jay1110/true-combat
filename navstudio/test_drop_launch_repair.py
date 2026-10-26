"""Typed physical DROP guides never retype or move ordinary source anchors."""
import copy
import math
import tempfile
import unittest
from pathlib import Path

from repair import parse_native_probe,propose_descents
from test_descent_import_jump import raw_reply,jump_records
from test_native_descent_repair import quarantined_floor_fixture


def action_reply(a,b,value,jumps=None):
    reply=raw_reply(a,b,value,jumps)
    vector=lambda p:'('+' '.join('%.5f'%v for v in p)+')'
    for index,key in enumerate(('start','end')):
        row=value['action_imports'][key]['drop_launch']
        reply+=(f'navgen probe action_import kind=drop_launch endpoint={index} accepted={int(row["accepted"])} '
                f'exact={int(row["exact"])} reason={row["reason"]} feet={vector(row["feet"])}\n')
    for index,key in enumerate(('start','end')):
        row=value['action_walks'][key]['drop_launch']
        reply+=(f'navgen probe action_walk kind=drop_launch endpoint={index} accepted={int(row["accepted"])} '
                f'reason={row["reason"]}\n')
    return reply


class ActionProbeParserTests(unittest.TestCase):
    def reply(self):
        a,b=[0.,0.,41.25],[12.,-20.,31.25]
        value={'imports':{key:{stance:dict(accepted=True,exact=True,feet=p)
                             for stance in ('stand','crouch')} for key,p in (('start',a),('end',b))}}
        for name in ('action_imports','action_walks'):value[name]={}
        for key,p in (('start',a),('end',b)):
            value['action_imports'][key]={'drop_launch':dict(accepted=True,exact=True,reason='accepted',feet=p)}
            value['action_walks'][key]={'drop_launch':dict(accepted=True,reason='accepted')}
        return a,b,action_reply(a,b,value)

    def test_exact_typed_import_and_walk_remain_separate_from_ordinary_walk(self):
        a,b,reply=self.reply()
        reply=reply.replace('stand=accepted','stand=different_floor')
        parsed=parse_native_probe(reply,a,b,24)
        self.assertEqual(parsed['stand'],'different_floor')
        self.assertTrue(parsed['action_imports']['end']['drop_launch']['exact'])
        self.assertTrue(parsed['action_walks']['end']['drop_launch']['accepted'])

    def test_missing_duplicate_malformed_foreign_or_nonfinite_records_fail_closed(self):
        for mode in ('missing_import','missing_walk','no_imports','no_walks','no_ordinary_imports',
                     'duplicate_import','duplicate_walk','endpoint','kind',
                     'bad_bool','nan','infinite','far','foreign_exact','reason','exact_rejected',
                     'walk_unimported','walk_reason','trailing','unknown_record'):
            a,b,reply=self.reply();lines=reply.splitlines()
            action_index=next(i for i,line in enumerate(lines)if 'action_import' in line)
            if mode=='missing_import':lines.pop(action_index)
            elif mode=='missing_walk':lines.pop()
            elif mode=='no_imports':lines=[line for line in lines if 'action_import' not in line]
            elif mode=='no_walks':lines=[line for line in lines if 'action_walk' not in line]
            elif mode=='no_ordinary_imports':lines=[line for line in lines if 'probe import' not in line]
            elif mode=='duplicate_import':lines.append(lines[action_index])
            elif mode=='duplicate_walk':lines.append(lines[-1])
            elif mode=='endpoint':lines[action_index]=lines[action_index].replace('endpoint=0','endpoint=2')
            elif mode=='kind':lines[action_index]=lines[action_index].replace('kind=drop_launch','kind=jump')
            elif mode=='bad_bool':lines[action_index]=lines[action_index].replace('accepted=1','accepted=true')
            elif mode=='nan':lines[action_index]=lines[action_index].replace('0.00000','nan',1)
            elif mode=='infinite':lines[action_index]=lines[action_index].replace('0.00000','1e999',1)
            elif mode=='far':lines[action_index]=lines[action_index].replace('0.00000','131073',1)
            elif mode=='foreign_exact':lines[action_index]=lines[action_index].replace('41.25000','41.27000')
            elif mode=='reason':lines[action_index]=lines[action_index].replace('reason=accepted','reason=unsupported')
            elif mode=='exact_rejected':lines[action_index]=lines[action_index].replace('accepted=1','accepted=0').replace('reason=accepted','reason=unsupported')
            elif mode=='walk_unimported':lines[action_index]=lines[action_index].replace('exact=1','exact=0')
            elif mode=='walk_reason':lines[-1]=lines[-1].replace('reason=accepted','reason=blocked_sweep')
            elif mode=='trailing':lines[action_index]+=' unchecked=yes'
            elif mode=='unknown_record':lines.append('navgen probe action_walk bad record')
            with self.subTest(mode=mode),self.assertRaises(RuntimeError):
                parse_native_probe('\n'.join(lines)+'\n',a,b,24)

    def test_nonexact_positive_import_is_a_suggestion_and_cannot_authorize_walk(self):
        a,b,reply=self.reply()
        reply=reply.replace('endpoint=1 accepted=1 exact=1','endpoint=1 accepted=1 exact=0')
        reply=reply.replace('reason=accepted feet=(12.00000 -20.00000 31.25000)\nnavgen probe action_walk',
                            'reason=accepted feet=(12.00000 -20.00000 29.29000)\nnavgen probe action_walk')
        reply=reply.replace('action_walk kind=drop_launch endpoint=1 accepted=1 reason=accepted',
                            'action_walk kind=drop_launch endpoint=1 accepted=0 reason=import_nonexact')
        parsed=parse_native_probe(reply,a,b,24)
        self.assertFalse(parsed['action_imports']['end']['drop_launch']['exact'])
        self.assertFalse(parsed['action_walks']['end']['drop_launch']['accepted'])


class PhysicalDropRepairTests(unittest.TestCase):
    def scene(self,mode=None,translation=(0.,0.,0.),budget=96):
        doc,native=quarantined_floor_fixture()
        old=[0.,0.,40.25];road=[64.,-110.,.25]
        alternate=isinstance(mode,str) and mode.startswith('alternate')
        narrow=alternate or mode in ('narrow','narrow_twelve')
        street_z=.25
        if narrow:
            # Relative poses from the independently inspected short hood
            # interval. Absolute placement remains irrelevant to the search.
            old=[0.,0.,21.1364];road=[-40.,-32.,-4.3104];street_z=-19.75
        doc['nodes'][0]['position']=old[:];doc['nodes'][1]['position']=road[:]
        native['bbox']['origin_above_feet']=24;native['traversal_physics']['gravity']=800
        magnitude=math.hypot(road[0],road[1]);direction=[road[i]/magnitude for i in (0,1)]
        # A coherent support interval can be narrower than the ordinary
        # spacing. Sixteen units already leaves that finite interval.
        stage_length=(8 if mode=='narrow' or alternate else 12) if narrow else 24
        xy=[round(direction[i]*stage_length,5) for i in (0,1)]
        normal=xy+[33.21];physical=xy+[31.25]
        if narrow:
            normal[2]=15.07085 if stage_length==8 else 14.69637
            physical[2]=13.23506 if stage_length==8 else 12.86057
        near=lambda a,b:math.dist(a,b)<.001
        alternate_street=[round(physical[k]+direction[k]*48,5) for k in (0,1)]+[street_z]
        connector=[alternate_street[0]-direction[1]*20,alternate_street[1]+direction[0]*20,street_z]
        if alternate:
            node=copy.deepcopy(doc['nodes'][1]);node.update(id=100,position=connector,name='ordinary connector',way_guid=100)
            doc['nodes'].insert(2,node)
            row=copy.deepcopy(native['nodes'][1]);row['id']=100;native['nodes'].insert(2,row)
            for source,target in ((99,100),(100,99)):
                doc['edges'].append({'from':source,'to':target})
                native['edges'].append(dict(**{'from':source,'to':target},accepted=True,crouch=False,reason='external_native_validated_walk'))
            if mode=='alternate_graph_reverse':native['edges'][-1]['accepted']=False
            if mode=='alternate_graph_action':node['nav_flags']=1<<27
        for node in doc['nodes']:node['position']=[node['position'][i]+translation[i]for i in range(3)]
        calls=[]
        def probe(a,b):
            calls.append((a,b));a0=[a[i]-translation[i]for i in range(3)];b0=[b[i]-translation[i]for i in range(3)]
            def ordinary(p):
                q=list(p);accepted=near(p,old) or near(p,road) or p[2]==street_z
                if narrow and abs(p[2]-road[2])<.001 and not near(p,road):q[2]=street_z;accepted=True
                if near(p[:2],xy):q[2]=normal[2];accepted=True
                row=dict(accepted=accepted,exact=accepted and near(p,q),feet=[q[i]+translation[i]for i in range(3)])
                return dict(stand=row,crouch=copy.deepcopy(row))
            def action(p):
                q=list(p);accepted=False
                if near(p[:2],xy):
                    q=physical[:];accepted=True
                    if mode=='over_two':q[2]=31.20
                    if mode=='changed_xy':q[0]+=.02
                if mode=='false':accepted=False
                exact=accepted and near(p,q)
                if mode=='fresh_nonexact' and near(p,physical):exact=False
                if mode=='fresh_failure' and near(p,physical):accepted=False;exact=False
                return dict(accepted=accepted,exact=exact,reason='accepted' if accepted else 'unsupported',
                            feet=[q[i]+translation[i]for i in range(3)])
            value=dict(stand='blocked_clearance',imports=dict(start=ordinary(a0),end=ordinary(b0)),
                       action_imports=dict(start={'drop_launch':action(a0)},end={'drop_launch':action(b0)}),
                       action_walks={key:{'drop_launch':dict(accepted=False,reason='blocked_clearance')}
                                     for key in ('start','end')})
            if near(a0,old) and near(b0,normal) or near(a0,normal) and near(b0,old):value['stand']='accepted'
            if a0[2]<=road[2] and b0[2]<=road[2]:value['stand']='accepted'
            if alternate and ((near(a0,alternate_street) and near(b0,road)) or
                              (near(b0,alternate_street) and near(a0,road))):value['stand']='blocked_clearance'
            if mode=='alternate_walk_forward' and near(a0,alternate_street) and near(b0,connector):value['stand']='blocked_clearance'
            if mode=='alternate_walk_reverse' and near(b0,alternate_street) and near(a0,connector):value['stand']='blocked_clearance'
            if mode=='alternate_normal_import' and near(b0,alternate_street):value['imports']['end']['stand']['exact']=False
            for key,point,other in (('start',a0,b0),('end',b0,a0)):
                if near(point,physical) and near(other,old):
                    value['stand']='different_floor'
                    allowed=not (mode=='forward_walk' and key=='end' or mode=='reverse_walk' and key=='start')
                    imported=value['action_imports'][key]['drop_launch']
                    allowed=allowed and imported['accepted'] and imported['exact']
                    value['action_walks'][key]['drop_launch']=dict(accepted=allowed,reason='accepted' if allowed else 'blocked_clearance')
            if a0[2]-b0[2]>18.25:
                value['drop']=dict(accepted=near(a0,physical) and b0[2]==street_z,delta=10.,max_drop=118.75)
                if mode=='launch_import' and near(a0,physical):value['action_imports']['start']['drop_launch'].update(exact=False)
                if mode=='landing_import' and near(a0,physical):value['imports']['end']['stand']['exact']=False
            jumps=None
            if b0[2]-a0[2]>18.25:
                accepted=near(b0,physical) and a0[2]==street_z and mode!='jump'
                if alternate:accepted=accepted and near(a0,alternate_street) and mode!='alternate_jump'
                jumps=jump_records(a,b,accepted)
                if mode=='return_import' and accepted:value['action_imports']['end']['drop_launch']['exact']=False
                if mode=='alternate_typed_import' and accepted:value['action_imports']['end']['drop_launch']['exact']=False
                if mode=='alternate_source_import' and accepted:value['imports']['start']['stand']['exact']=False
            return parse_native_probe(action_reply(a,b,value,jumps),a,b,24)
        before=copy.deepcopy(doc)
        proposal,evidence=propose_descents(doc,native,probe,max_probes=budget)
        self.assertEqual(doc,before)
        return doc,proposal,evidence,calls,normal,physical

    def test_new_role9_uses_fresh_physical_import_and_both_typed_body_walks(self):
        doc,proposal,evidence,calls,normal,physical=self.scene()
        self.assertEqual(evidence['added_nodes'],2)
        route=evidence['routes'][0];self.assertEqual(route['upper_approach'],physical)
        self.assertEqual(route['return_proof']['kind'],'standing_jump')
        nodes={n['id']:n for n in proposal['nodes']}
        for old in doc['nodes']:self.assertEqual(nodes[old['id']],old)
        guide=next(n for n in proposal['nodes']if n['source_reason']=='automatic_repair_native_upper_drop_launch')
        self.assertEqual(guide['editor_role'],9);self.assertEqual(guide['position'],physical)
        self.assertLess(calls.index((doc['nodes'][0]['position'],normal)),calls.index((doc['nodes'][0]['position'],physical)))
        self.assertIn((physical,doc['nodes'][0]['position']),calls)
        self.assertEqual(route['return_chain'][-1],3);self.assertLessEqual(len(calls),96)

    def test_false_loose_foreign_unproved_or_unsafe_action_is_never_used(self):
        for mode in ('false','fresh_nonexact','fresh_failure','changed_xy','over_two','forward_walk',
                     'reverse_walk','launch_import','landing_import','jump','return_import'):
            doc,proposal,evidence,calls,_,_=self.scene(mode)
            with self.subTest(mode=mode):
                self.assertEqual(evidence['added_nodes'],0)
                self.assertEqual(proposal['nodes'],doc['nodes'])
                self.assertEqual(evidence['replaced_rejected_edges'],[])
                self.assertLessEqual(len(calls),96)

    def test_new_direction_and_physical_pose_are_translation_invariant(self):
        _,_,first,_,_,_=self.scene();shift=(321.,-741.,17.)
        _,_,second,_,_,_=self.scene(translation=shift)
        self.assertEqual([round(first['routes'][0]['upper_approach'][i]+shift[i],5)for i in range(3)],
                         second['routes'][0]['upper_approach'])

    def test_short_samples_find_narrow_coherent_support_before_sixteen(self):
        for mode,stage_length in (('narrow',8),('narrow_twelve',12)):
          for shift in ((0.,0.,0.),(2107.2,1975.2,0.)):
            doc,proposal,evidence,calls,normal,physical=self.scene(mode,translation=shift)
            with self.subTest(translation=shift,length=stage_length):
                self.assertEqual(evidence['added_nodes'],2)
                expected=[round(physical[i]+shift[i],5) for i in range(3)]
                self.assertEqual(evidence['routes'][0]['upper_approach'],expected)
                old=doc['nodes'][0]['position']
                first_stage=next(end for begin,end in calls if begin==old and end[2]==old[2] and end[:2]!=old[:2])
                self.assertAlmostEqual(math.dist(first_stage[:2],old[:2]),8.,places=4)
                self.assertAlmostEqual(math.dist(physical[:2],[0.,0.]),stage_length,places=4)
                nodes={n['id']:n for n in proposal['nodes']}
                for original in doc['nodes']:self.assertEqual(nodes[original['id']],original)
                self.assertLessEqual(len(calls),96)

    def test_budget_cannot_publish_half_of_the_physical_drop_and_return_chain(self):
        for budget in (1,12,18):
            doc,proposal,evidence,calls,_,_=self.scene(budget=budget)
            with self.subTest(budget=budget):
                self.assertLessEqual(len(calls),budget);self.assertEqual(proposal['nodes'],doc['nodes'])
                self.assertEqual(evidence['added_nodes'],0);self.assertEqual(evidence['replaced_rejected_edges'],[])

    def test_physical_role9_pose_roundtrips_existing_format5_and_undo(self):
        from edit import Drafts
        doc,proposal,evidence,_,_,physical=self.scene()
        with tempfile.TemporaryDirectory() as folder:
            store=Drafts(Path(folder)/'drafts');store.documents[doc['map']]=copy.deepcopy(doc)
            current=store.apply_repair(doc['map'],doc['revision'],doc['base']['environment'],proposal)
            store.save(doc['map'],current['revision'],doc['base']['environment'])
            candidate=store.export_candidate(doc['map'],current['revision'],doc['base']['environment'],Path(folder)/'preview')
            lines=candidate.read_text().splitlines();count=int(lines[0].split()[3])
            rows={int(row[0]):row for line in lines[1:1+count]if(row:=line.split())}
            guide=evidence['routes'][0]['chain'][1]
            self.assertEqual(int(rows[guide][10]),9)
            self.assertEqual(list(map(float,rows[guide][1:4])),physical)
            restored=store.change(doc['map'],dict(op='undo',revision=current['revision']),doc['base']['environment'])
            self.assertEqual(restored['nodes'],doc['nodes'])


if __name__=='__main__':unittest.main()
