"""Separate, bounded street launches preserve the proved DROP and old graph."""
import hashlib
import json
from pathlib import Path
import tempfile
import copy
import unittest

import test_drop_launch_repair as physical_fixtures


class AlternateStandingReturnTests(unittest.TestCase):
    def scene(self,mode='alternate',translation=(0.,0.,0.),budget=96):
        return physical_fixtures.PhysicalDropRepairTests().scene(mode,translation,budget)

    def test_separate_launch_keeps_drop_landing_ordinary_and_returns_through_proved_connector(self):
        doc,proposal,evidence,calls,_,physical=self.scene()
        self.assertEqual(evidence['added_nodes'],3)
        route=evidence['routes'][0];proof=route['return_proof'];detail=proof['proof']
        self.assertEqual(proof['kind'],'standing_jump_alternate_launch')
        self.assertEqual(detail['model'],'native_standing_jump_alternate_launch_v1')
        self.assertEqual(detail['selected_distance'],48)
        self.assertEqual(detail['drop_launch'],physical)
        self.assertEqual(detail['from_descent_connector']['source_ids'],[99,100])
        self.assertEqual(detail['to_descent_connector']['source_ids'],[100,99])
        self.assertTrue(all(edge['accepted'] for path in ('from_descent_connector','to_descent_connector')
                            for edge in detail[path]['native_edges']))
        self.assertEqual(proof['action_probe'],detail['action_probe'])
        self.assertTrue(proof['action_probe']['jump']['standing']['accepted'])
        self.assertEqual(proof['action_probe']['jump']['standing_proof']['samples'],27)
        self.assertTrue(proof['action_probe']['action_imports']['end']['drop_launch']['exact'])
        nodes={n['id']:n for n in proposal['nodes']}
        source,drop_source,landing,connection=route['chain']
        street=proof['street_launch_source_id']
        self.assertEqual(route['return_chain'],[99,100,street,drop_source,3])
        self.assertEqual(nodes[street]['action_roles'],[dict(kind='standing_jump',role='launch',landing_node=drop_source)])
        self.assertEqual((nodes[street]['nav_flags'],nodes[street]['editor_role'],nodes[street]['radius']),
                         (1<<27,65,2))
        self.assertEqual(nodes[landing]['nav_flags'],0);self.assertEqual(nodes[landing]['editor_role'],1)
        self.assertEqual(nodes[drop_source]['editor_role'],9)
        self.assertEqual({tuple(e) for e in evidence['replaced_rejected_edges']},{(3,99)})
        for old in doc['nodes']:self.assertEqual(nodes[old['id']],old)
        self.assertEqual(route['return_chain_proof_sha256'],hashlib.sha256(json.dumps(proof,sort_keys=True,separators=(',',':')).encode()).hexdigest())
        self.assertLessEqual(len(calls),96)

    def test_translated_literal_poses_use_the_same_finite_launch_distance(self):
        _,_,first,_,_,_=self.scene();shift=(2107.2,1975.2,17.)
        doc,proposal,second,calls,_,_=self.scene(translation=shift)
        a=first['routes'][0]['return_proof']['proof'];b=second['routes'][0]['return_proof']['proof']
        for key in ('street','drop_launch'):
            self.assertEqual([round(a[key][i]+shift[i],5)for i in range(3)],b[key])
        self.assertEqual(a['selected_distance'],b['selected_distance'])
        for old in doc['nodes']:self.assertEqual(next(n for n in proposal['nodes']if n['id']==old['id']),old)
        self.assertLessEqual(len(calls),96)

    def test_missing_normal_or_typed_import_action_or_either_walk_never_publishes_return(self):
        for mode in ('alternate_normal_import','alternate_typed_import','alternate_source_import','alternate_jump',
                     'alternate_walk_forward','alternate_walk_reverse','alternate_graph_reverse','alternate_graph_action'):
            doc,proposal,evidence,calls,_,_=self.scene(mode)
            with self.subTest(mode=mode):
                self.assertEqual(evidence['added_nodes'],0);self.assertEqual(proposal['nodes'],doc['nodes'])
                self.assertEqual(evidence['replaced_rejected_edges'],[]);self.assertLessEqual(len(calls),96)

    def test_exhausted_shared_budget_preserves_both_graph_and_source_ids(self):
        for budget in (12,20,24):
            doc,proposal,evidence,calls,_,_=self.scene(budget=budget)
            with self.subTest(budget=budget):
                self.assertLessEqual(len(calls),budget);self.assertEqual(evidence['added_nodes'],0)
                self.assertEqual(proposal['nodes'],doc['nodes']);self.assertEqual(evidence['replaced_rejected_edges'],[])

    def test_actual_format5_export_and_undo_keep_one_separate_standing_launch(self):
        from edit import Drafts
        doc,proposal,evidence,_,_,_=self.scene()
        with tempfile.TemporaryDirectory() as folder:
            store=Drafts(Path(folder)/'drafts');store.documents[doc['map']]=copy.deepcopy(doc)
            current=store.apply_repair(doc['map'],doc['revision'],doc['base']['environment'],proposal)
            store.save(doc['map'],current['revision'],doc['base']['environment'])
            candidate=store.export_candidate(doc['map'],current['revision'],doc['base']['environment'],Path(folder)/'preview')
            lines=candidate.read_text().splitlines();count=int(lines[0].split()[3])
            rows={int(row[0]):row for line in lines[1:1+count]if(row:=line.split())}
            route=evidence['routes'][0];street=route['return_proof']['street_launch_source_id']
            drop_source,landing=route['chain'][1:3]
            self.assertEqual((int(rows[street][8]),int(rows[street][10]),int(rows[street][11])),(1<<27,65,drop_source+1))
            self.assertEqual((int(rows[drop_source][8]),int(rows[drop_source][10]),int(rows[drop_source][11])),(0,9,landing+1))
            self.assertEqual((int(rows[landing][8]),int(rows[landing][10])),(0,1))
            restored=store.change(doc['map'],dict(op='undo',revision=current['revision']),doc['base']['environment'])
            self.assertEqual(restored['nodes'],doc['nodes'])
            self.assertEqual({(e['from'],e['to'])for e in restored['edges']},{(e['from'],e['to'])for e in doc['edges']})


if __name__=='__main__':unittest.main()
