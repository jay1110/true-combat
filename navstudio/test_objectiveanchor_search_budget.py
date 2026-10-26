"""Costed ordinary-anchor search; no Engine/process access or map edits."""
import copy
import json
import unittest

import objectiveanchors as anchors
import test_objectiveanchors as existing


class ObjectiveAnchorSearchBudgetTests(unittest.TestCase):
    fixture=existing.ObjectiveAnchorsTests.fixture
    snapshot=existing.ObjectiveAnchorsTests.snapshot
    evidence=existing.ObjectiveAnchorsTests.evidence
    probe=existing.ObjectiveAnchorsTests.probe
    visit=existing.ObjectiveAnchorsTests.visit

    def fanout(self,degree=14,max_probes=anchors.MAX_PROBES,two=False):
        original,doc,native=self.fixture(two=two)
        centre=1952
        for k in range((degree-4)//2):
            node=copy.deepcopy(doc['nodes'][2]);node.update(id=3000+k,way_guid=4000+k,
                position=[1800.+k*16,-300.,-31.75])
            doc['nodes'].append(node)
            native['nodes'].append(dict(node,accepted=True,spawn_reachable=True,
                raw_position=[*node['position'][:2],node['position'][2]+30],custom_properties={}))
            for a,b in((centre,node['id']),(node['id'],centre)):
                doc['edges'].append(dict(from_=a,to=b))
                doc['edges'][-1]['from']=doc['edges'][-1].pop('from_')
                native['edges'].append(dict(doc['edges'][-1],accepted=True))
        return anchors.Reconciler(doc,native,list(original.anchors.values()),(2,5),{},max_probes=max_probes),doc

    def assert_raw_diagnostic(self,r):
        value=r.diagnostic();json.dumps(value,allow_nan=False)
        self.assertEqual(value['status'],'incomplete')
        self.assertNotIn('proposal',value);self.assertNotIn('certificate',value)
        self.assertNotIn('generation_complete',value)
        self.assertEqual(value['query_count'],len(r.queries))
        self.assertEqual(value['max_anchor_probes'],max(c['limit']for c in value['anchor_limits']))
        self.assertEqual(value['budget_policy'],anchors.BUDGET_POLICY)
        for key in('anchor_queries','pools','results','states','snapshot_refs'):
            self.assertIsInstance(value[key],list)
        for query in value['queries']:
            proof=query['proof'];self.assertEqual(anchors.sha(proof['raw_reply'].encode()),proof['reply_sha256'])
        for ref in value['snapshot_refs']:
            self.assertIn('path',ref['evidence']);self.assertIn('sha256',ref['evidence'])
        value['queries'].clear();self.assertEqual(len(r.queries),r.diagnostic()['query_count'])
        return r.diagnostic()

    def test_fourteen_directed_incidents_pass_beyond_old_256_with_all_final_walks_fresh(self):
        r,doc=self.fanout();before=copy.deepcopy(doc)
        self.visit(r,2);self.visit(r,5)
        self.assertGreater(r.anchor_queries[1952],256)
        cost=r.anchor_costs[1952]
        self.assertEqual(cost['directed_incidents'],14);self.assertEqual(cost['coupled_incidents'],0)
        self.assertEqual(cost['candidate_cost'],15);self.assertEqual(cost['candidate_pool_limit'],35)
        self.assertEqual(cost['planned_queries'],1114);self.assertEqual(cost['limit'],1114)
        self.visit(r,2);proposal,moves=r.proposal();self.assertEqual(len(moves),1)
        self.assertEqual(doc,before);self.assertEqual(doc['edges'],proposal['edges'])
        for old,new in zip(doc['nodes'],proposal['nodes']):
            self.assertEqual({k:v for k,v in old.items()if k!='position'},{k:v for k,v in new.items()if k!='position'})
        for gt in(2,5):
            count=len(r.queries);snapshot=self.snapshot(gt)
            r.verify_joint(gt,snapshot,self.evidence(snapshot),self.probe(gt),proposal,moves)
            self.assertEqual(len(r.queries)-count,15)
        cert=r.certificate(proposal,moves)
        self.assertEqual(cert['max_anchor_probes'],1114);self.assertEqual(cert['final_joint_gametypes'],[2,5])
        self.assertEqual(cert['anchor_limits'],[cost]);self.assertLess(cert['query_count'],2048)

    def test_fourteen_incidents_and_coupled_changed_height_neighbors_keep_joint_proof(self):
        r,doc=self.fanout(two=True);before=copy.deepcopy(doc)
        self.visit(r,2);self.visit(r,5);self.visit(r,2)
        self.assertGreater(r.anchor_queries[1952],256)
        self.assertEqual(r.anchor_costs[1952]['coupled_incidents'],2)
        self.assertEqual(r.anchor_costs[1952]['planned_queries'],1254)
        proposal,moves=r.proposal();self.assertEqual(len(moves),2)
        self.assertEqual(doc,before);self.assertEqual(doc['edges'],proposal['edges'])
        for gt in(2,5):
            snapshot=self.snapshot(gt);count=len(r.queries)
            r.verify_joint(gt,snapshot,self.evidence(snapshot),self.probe(gt),proposal,moves)
            self.assertEqual(len(r.queries)-count,18)
        self.assertEqual(r.certificate(proposal,moves)['final_joint_gametypes'],[2,5])

    def test_other_gt_true_candidate_is_prioritized_and_common_success_stops_search(self):
        r,_,_=self.fixture();old=tuple(r.nodes[1952]['position'])
        preferred=(1724.5,-348.,-31.75);unknown=(1726.,-348.,-31.75)
        r.pools[1952].update((preferred,unknown));r.results[2][1952]={old:False,preferred:True}
        self.visit(r,5)
        self.assertEqual(r.queries[0]['start'],list(preferred))
        self.assertEqual(r.anchor_queries[1952],5)
        self.assertNotIn(old,r.results[5][1952]);self.assertNotIn(unknown,r.results[5][1952])
        count=len(r.queries);self.visit(r,5);self.assertEqual(len(r.queries),count)
        _,moves=r.proposal();self.assertEqual(moves[0]['new_position'],list(preferred))

    def test_rejected_literal_is_skipped_but_seed_still_discovers_another_height(self):
        r,_,_=self.fixture();old=tuple(r.nodes[1952]['position']);r.results[2][1952]={old:False}
        self.visit(r,5)
        self.assertNotIn(old,r.results[5][1952])
        corrected=(old[0],old[1],-31.75)
        self.assertIs(r.results[5][1952][corrected],True)
        # The rejected pose is imported once as a seed, never walked again as
        # a literal candidate. Its different-height suggestion is not waived.
        old_self=[q for q in r.queries if q['start']==list(old)==q['end']]
        new_self=[q for q in r.queries if q['start']==list(corrected)==q['end']]
        self.assertEqual(len(old_self),1);self.assertEqual(len(new_self),1)
        with self.assertRaisesRegex(RuntimeError,'Keine gemeinsame'):r.proposal()

    def test_cache_does_not_spend_budget_but_fresh_proof_and_revisit_cannot_renew_it(self):
        r,_,_=self.fixture(max_probes=1);p=r.nodes[1952]['position'];probe=self.probe(2)
        first=r._query(2,1952,p,p,probe)
        self.assertIs(r._query(2,1952,p,p,probe),first);self.assertEqual(len(r.queries),1)
        with self.assertRaisesRegex(RuntimeError,'Suchbudget'):r._query(2,1952,p,p,probe,fresh=True)
        for _ in range(2):
            with self.assertRaisesRegex(RuntimeError,'Suchbudget'):self.visit(r,2)
        value=self.assert_raw_diagnostic(r);self.assertEqual(value['budget'],1)
        self.assertEqual(value['anchor_queries'],[dict(source_id=1952,query_count=1)])
        unlimited,_,_=self.fixture(max_probes=10**9)
        self.assertEqual(unlimited.limit,2048)
        self.assertTrue(all(c['limit']<=2048 for c in unlimited.anchor_costs.values()))

    def test_coupled_anchor_cost_counts_each_directed_discovery_retry(self):
        r,_,_=self.fixture(two=True)
        self.assertEqual(r.anchor_costs[1952]['directed_incidents'],4)
        self.assertEqual(r.anchor_costs[1952]['coupled_incidents'],2)
        self.assertEqual(r.anchor_costs[1952]['candidate_cost'],7)
        self.assertEqual(r.anchor_costs[686]['directed_incidents'],2)
        self.assertEqual(r.anchor_costs[686]['coupled_incidents'],2)

    def test_impossible_common_pose_keeps_sources_and_serializable_partial_diagnostics(self):
        r,doc,_=self.fixture();before=copy.deepcopy(doc)
        self.visit(r,2);self.visit(r,5,unsafe=True);self.visit(r,2)
        with self.assertRaisesRegex(RuntimeError,'Keine gemeinsame'):r.proposal()
        self.assertEqual(doc,before);value=self.assert_raw_diagnostic(r)
        self.assertEqual(value['source_ids'],[1952]);self.assertEqual(value['final_joint_gametypes'],[])
        self.assertTrue(any(row['accepted']is False for row in value['results']))
        self.assertEqual(len(value['snapshot_refs']),3)

    def test_joint_rejection_is_never_replaced_by_discovery_success_in_diagnostic(self):
        r,doc,_=self.fixture(two=True);before=copy.deepcopy(doc)
        self.visit(r,2);self.visit(r,5);self.visit(r,2);proposal,moves=r.proposal()
        snapshot=self.snapshot(2);count=len(r.queries)
        with self.assertRaisesRegex(RuntimeError,'Nachbarkante'):
            r.verify_joint(2,snapshot,self.evidence(snapshot),self.probe(2,deny_pair=True),proposal,moves)
        self.assertGreater(len(r.queries),count);self.assertEqual(doc,before)
        value=self.assert_raw_diagnostic(r);self.assertEqual(value['final_joint_gametypes'],[])
        self.assertEqual(value['snapshot_refs'][-1]['phase'],'joint')
        with self.assertRaisesRegex(RuntimeError,'Nachbarkanten'):r.certificate(proposal,moves)


if __name__=='__main__':unittest.main()
