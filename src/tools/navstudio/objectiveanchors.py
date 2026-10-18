"""Bounded common objective-anchor positions; no process or Engine access.

Native imports may normalize one shared WAY differently when a gametype script
removes a flag pedestal or bomb model. A suggested import Z is never proof:
the same final literal feet and every incident WALK must pass in every state.
Only generated ordinary objective anchors can move; no topology is removed.
"""
import copy
import hashlib
import json
import math
from pathlib import Path
import sys
from types import SimpleNamespace

from edit import graph_hash,custom_way_properties
TOOLS=Path(__file__).resolve().parent.parent
if str(TOOLS)not in sys.path:sys.path.insert(0,str(TOOLS))
from navmesh.objective_contact import capture_contact,plant_contact,flag_pickup_contact

CONTRACT="generated_objective_anchors_common_exact_feet_v1"
MAX_PROBES=2048
MAX_CANDIDATES=17
BUDGET_POLICY='gt_count*(17+(1+17*gt_count)*(1+directed_incidents+coupled_incidents)+(1+directed_incidents));shared_hard_limit_2048'


def sha(raw):return hashlib.sha256(raw).hexdigest()


def require(condition,message):
    if not condition:raise RuntimeError("Gemeinsame Objectiveanker: "+message)


def archive_anchors(doc,native,bundle):
    """Recover provenance lost by native-to-draft normalization, never guess IDs."""
    require(isinstance(bundle,dict),"Generierungsherkunft fehlt.")
    required=("manifest","scanner","candidate","metadata")
    require(all(type(bundle.get(k))is bytes for k in required),"Vollständiges Scannerarchiv fehlt.")
    manifest=json.loads(bundle['manifest']);scanner=json.loads(bundle['scanner']);meta=json.loads(bundle['metadata'])
    require(doc['base'].get('generation_diagnostics')==sha(bundle['manifest']) and
        manifest.get('map')==doc['map']==native.get('map')==meta.get('map')==scanner.get('map') and
        manifest.get('source')=='automatic_spatial_scanner' and
        manifest.get('environment')==doc['base']['environment'],"Scannerarchiv gehört nicht zum aktuellen Projekt.")
    identity=manifest.get('candidate',{})
    require(manifest.get('native_report_sha256')==sha(bundle['scanner']) and identity.get('sha256')==sha(bundle['candidate'])and
        identity.get('metadata')==sha(bundle['metadata']) and
        meta.get('candidate_sha256')==sha(bundle['candidate'].decode('ascii').replace('\r\n','\n').replace('\r','\n').encode('ascii')),
        "Scannerarchiv-Hashes stimmen nicht überein.")
    rows={n['id']:n for n in native['nodes']};old={n['id']:n for n in scanner['nodes']};nodes={n['id']:n for n in doc['nodes']}
    protected={i for edge in doc['base'].get('protected_action_edges',[])for i in edge}
    result=[]
    for source in meta.get('nodes',[]):
        if source.get('kind')!='objective_contact_anchor' or source['id']not in nodes:continue
        i=source['id'];node=nodes[i];row=rows.get(i,{})
        require(i in old and old[i].get('accepted')is True and source.get('objective_entities'),"Objectiveanker ohne ursprünglichen akzeptierten Scannerbeleg: "+str(i))
        if (i in protected or node.get('editor_role',0) or node.get('generated_guide')or node.get('action_roles')or
            node.get('action_properties')or row.get('door_recipe')or node.get('nav_flags',0)&~64):continue
        require(row.get('accepted')is True and row.get('spawn_reachable')is True,"Objectiveanker ist nicht vollständig nativ erreichbar: "+str(i))
        result.append(dict(source_id=i,objective_entities=source['objective_entities']))
    return result


def collision_sha(snapshot):
    return sha(json.dumps(dict(map=snapshot.get('map'),models=sorted(snapshot.get('models',[]),
        key=lambda m:(m.get('modelindex',0),m.get('entity',0)))),sort_keys=True,separators=(',',':'),allow_nan=False).encode())


def map_script_binding(roots,name,gametype,directory):
    """Share the canonical actual VFS resolver; fallback is not GT evidence."""
    from mapscript import binding
    return binding(SimpleNamespace(roots=list(roots)),name,gametype,directory)


class Reconciler:
    def __init__(self,doc,native,anchors,gametypes,objectives_by_gt,max_probes=MAX_PROBES):
        self.doc=copy.deepcopy(doc);self.native=copy.deepcopy(native);self.nodes={n['id']:n for n in doc['nodes']}
        self.anchors={r['source_id']:copy.deepcopy(r)for r in anchors};self.gametypes=tuple(gametypes)
        require(bool(self.gametypes)and len(set(self.gametypes))==len(self.gametypes)and set(self.gametypes)<={2,5,7},"Ungültiger Gametypeumfang.")
        require(native.get('generation_complete')is True,"Vollständige native Ausgangsprüfung fehlt.")
        self.profile=copy.deepcopy(native['bbox']);self.origin=self.profile['origin_above_feet']
        rows={n['id']:n for n in native['nodes']}
        require(set(rows)==set(self.nodes),"Aktueller Entwurf und Nativeprüfung haben andere Punkte.")
        require({(e['from'],e['to'])for e in doc['edges']}=={(e['from'],e['to'])for e in native['edges']if e.get('accepted')is True},"Aktueller Entwurf und Nativeprüfung haben andere Verbindungen.")
        for i,n in self.nodes.items():
            row=rows[i];raw=list(row.get('raw_position',[]))
            if len(raw)==3:raw[2]-=self.origin
            require(len(raw)==3 and math.dist(raw,n['position'])<=.011 and row.get('nav_flags')==n.get('nav_flags')and
                row.get('arrival',{}).get('requested_radius',row.get('radius'))==n.get('radius')and row.get('name','')==n.get('name','')and
                row.get('facing',[0,0,0])==n.get('facing',[0,0,0])and row.get('custom_properties',{})==custom_way_properties(n)and
                (not n.get('way_guid')or row.get('way_guid')==n['way_guid']),"Ungeprüfter Entwurf/Punktmetadaten bei Source-ID "+str(i))
        self.objectives=objectives_by_gt;self.limit=min(MAX_PROBES,max(0,int(max_probes)))
        self.queries=[];self.cache={};self.anchor_queries={};self.states={};self.visits=[];self.joint_verified=set();self.results={g:{}for g in self.gametypes};self.pools={i:{tuple(n['position'])}for i,n in self.nodes.items()if i in self.anchors}
        self.pairs=sorted({(e['from'],e['to'])for e in doc['edges']})
        for i in self.anchors:
            require(i in self.nodes and any(i in pair for pair in self.pairs),"Objectiveanker ohne Anschluss: "+str(i))
            for a,b in self.pairs:
                if i not in(a,b):continue
                require(all(self.nodes[j].get('nav_flags',0)&~64==0 and not self.nodes[j].get('editor_role',0)and
                    not self.nodes[j].get('action_roles')and not self.nodes[j].get('action_properties')for j in(a,b)),
                    "Objectiveanker berührt ein geschütztes Aktionsrezept: "+str(i))
        # Each GT can import a different height for each seed. Budget the real
        # directed WALK fanout, including one bounded neighbour-height retry,
        # instead of giving a fourteen-edge anchor the same quota as a leaf.
        # This never raises the shared hard limit or substitutes for a proof.
        self.anchor_costs={}
        for i in self.anchors:
            incident=[p for p in self.pairs if i in p]
            coupled=sum((b if a==i else a)in self.anchors for a,b in incident)
            candidate_cost=1+len(incident)+coupled;joint_cost=1+len(incident)
            pool_limit=1+MAX_CANDIDATES*len(self.gametypes)
            planned=len(self.gametypes)*(MAX_CANDIDATES+pool_limit*candidate_cost+joint_cost)
            self.anchor_costs[i]=dict(source_id=i,directed_incidents=len(incident),coupled_incidents=coupled,
                candidate_cost=candidate_cost,joint_cost=joint_cost,candidate_pool_limit=pool_limit,
                planned_queries=planned,limit=min(self.limit,planned))

    def failure(self,i,reason):
        n=self.nodes[i]
        return RuntimeError("Gemeinsame Objectiveanker: Source-ID %d / UID %s bei Kamera (%s): %s. Punkt/Verbindungen erhalten; keine Goals oder Navigation freigegeben."%
            (i,n.get('way_guid',0),', '.join('%.3f'%v for v in [n['position'][0],n['position'][1],n['position'][2]+self.origin]),reason))

    def _query(self,gt,i,a,b,probe,fresh=False):
        a=[round(float(v),5)for v in a];b=[round(float(v),5)for v in b];key=(gt,*a,*b)
        require(all(math.isfinite(v)and abs(v)<=131072 for v in a+b),"Ungültige Probenkoordinaten.")
        if not fresh and key in self.cache:return self.cache[key]
        if len(self.queries)>=self.limit or self.anchor_queries.get(i,0)>=self.anchor_costs[i]['limit']:raise self.failure(i,"Natives Suchbudget ausgeschöpft")
        value=probe(a,b)
        require(isinstance(value,dict)and isinstance(value.get('raw_reply'),str)and
            sha(value['raw_reply'].encode())==value.get('reply_sha256'),"Native Rohantwort ist nicht eindeutig gebunden.")
        self.queries.append(dict(gametype=gt,source_id=i,start=a,end=b,proof=copy.deepcopy(value)))
        self.anchor_queries[i]=self.anchor_queries.get(i,0)+1;self.cache[key]=value
        return value

    def _exact(self,value,endpoint,node,feet):
        stance='crouch'if node.get('crouch')else'stand';row=value.get('imports',{}).get(endpoint,{}).get(stance,{})
        return (row.get('accepted')is True and row.get('exact')is True and row.get('feet')is not None and
            math.dist(row['feet'],feet)<=.011)

    def _contacts(self,gt,i,point,snapshot):
        if gt==7:return True
        from goalgen import observed_objectives
        observed=observed_objectives(snapshot,self.doc['map'],gt)
        require(observed is not None,"Aktuelle native Objective-Entities fehlen.")
        node=self.nodes[i];height=self.profile['crouch_height']if node.get('crouch')else self.profile['stand_height'];radius=self.profile['half_width']
        for spec in self.objectives.get(gt,[]):
            if spec.get('entity_id')not in self.anchors[i]['objective_entities']:continue
            matches=[o for o in observed if all(o.get(k)==spec.get(k)for k in('tag','target','operation'))]
            # GT scripts can remove the BSP flag or its capture target. The
            # matching current inventory is authoritative; a removed mission
            # imposes no phantom contact obligation on its retained WAY node.
            if not matches:
                if any(o.get('tag')==spec.get('tag')and o.get('target')==spec.get('target')for o in observed):return False
                continue
            if len(matches)!=1:return False
            obj=matches[0];operation=obj['operation']
            if operation=='capture':
                if not capture_contact(point,obj['bounds'],radius,height):return False
            elif operation=='plant_defuse':
                if not plant_contact(point,self.origin,obj['bounds'],radius,height,obj.get('target_bounds'),obj.get('contact_top')):return False
            elif operation=='take_flag':
                if not flag_pickup_contact([point[0],point[1],point[2]+self.origin],obj['position']):return False
            elif operation=='activate_capture':
                bounds=obj.get('bounds')
                if not bounds or not(bounds[0][0]-12<=point[0]<=bounds[1][0]+12 and bounds[0][1]-12<=point[1]<=bounds[1][1]+12 and bounds[0][2]-32<=point[2]<=bounds[1][2]):return False
            else:return False
        return True

    def _valid(self,gt,i,point,snapshot,probe,positions=None,fresh=False,discover_neighbors=False):
        n=self.nodes[i];stance='crouch'if n.get('crouch')else'stand'
        if not self._contacts(gt,i,point,snapshot):return False
        value=self._query(gt,i,point,point,probe,fresh)
        if value.get(stance)!='accepted'or not all(self._exact(value,k,n,point)for k in('start','end')):return False
        current=positions or {j:v['position']for j,v in self.nodes.items()}
        for a,b in self.pairs:
            if i not in(a,b):continue
            start=point if a==i else current[a];end=point if b==i else current[b]
            value=self._query(gt,i,start,end,probe,fresh)
            posture='crouch'if self.nodes[a].get('crouch')or self.nodes[b].get('crouch')else'stand'
            accepted=(value.get(posture)=='accepted'and self._exact(value,'start',self.nodes[a],start)and self._exact(value,'end',self.nodes[b],end))
            if not accepted and discover_neighbors:
                # Two adjacent generated anchors can both need reconciliation.
                # An imported neighbour Z is only a discovery guide, followed
                # by a new literal WALK. Final joint checking forbids guides
                # and uses both actual selected new coordinates in every GT.
                other=b if a==i else a
                if other in self.anchors:
                    endpoint='end'if b==other else'start';stance='crouch'if self.nodes[other].get('crouch')else'stand'
                    imported=value.get('imports',{}).get(endpoint,{}).get(stance,{})
                    guide=imported.get('feet');old=self.nodes[other]['position']
                    if (imported.get('accepted')is True and isinstance(guide,list)and len(guide)==3 and
                        all(type(v)in(int,float)and math.isfinite(v)for v in guide)and math.dist(guide[:2],old[:2])<=.011 and abs(guide[2]-old[2])<=8):
                        start=point if a==i else guide;end=point if b==i else guide
                        value=self._query(gt,i,start,end,probe,fresh)
                        accepted=(value.get(posture)=='accepted'and self._exact(value,'start',self.nodes[a],start)and self._exact(value,'end',self.nodes[b],end))
            if not accepted:return False
        return True

    def _state(self,gt,snapshot,evidence):
        require(gt in self.gametypes and snapshot.get('map')==self.doc['map']and snapshot.get('gametype')==gt,"Falscher aktueller GT-Snapshot.")
        require(evidence.get('map')==self.doc['map']and evidence.get('gametype')==gt and evidence.get('sha256')and evidence.get('path'),"Snapshot-Provenienz fehlt.")
        path=Path(evidence['path']);raw=path.read_bytes()if path.is_file()else None
        require(raw is not None and sha(raw)==evidence['sha256']and json.loads(raw)==snapshot,"Archivierter Snapshot passt nicht zu seinem Rohbeleg.")
        collision=collision_sha(snapshot)
        require(gt not in self.states or self.states[gt]==collision,"GT-Kollisionszustand änderte sich während der Suche.")
        self.states[gt]=collision;self.visits.append(dict(gametype=gt,collision_sha256=collision,evidence=copy.deepcopy(evidence)))

    def observe(self,gt,snapshot,evidence,probe):
        self._state(gt,snapshot,evidence)
        self.visits[-1]['phase']='candidate'
        for i in sorted(self.anchors):
            row=self.results[gt].setdefault(i,{});old=self.nodes[i]['position']
            if self._common(i):continue
            for point in sorted(self.pools[i],key=lambda p:(-sum(self.results[g].get(i,{}).get(p)is True for g in self.gametypes if g!=gt),math.dist(old,p),p)):
                if self._eligible(gt,i,point)and point not in row:row[point]=self._valid(gt,i,list(point),snapshot,probe,discover_neighbors=True)
                if self._common(i):break
            if self._common(i)or row.get(tuple(old))is True:continue
            bound=min(16.,float(self.nodes[i]['radius'])*.5)
            offsets=[(0,0)]+[(x,y)for r in(4.5,8.)for x,y in((r,0),(-r,0),(0,r),(0,-r),(r,r),(-r,r),(r,-r),(-r,-r))]
            for dx,dy in offsets[:MAX_CANDIDATES]:
                if math.hypot(dx,dy)>bound:continue
                seed=[round(old[0]+dx,5),round(old[1]+dy,5),old[2]]
                value=self._query(gt,i,seed,seed,probe);stance='crouch'if self.nodes[i].get('crouch')else'stand'
                imported=value.get('imports',{}).get('end',{}).get(stance,{})
                point=imported.get('feet')
                if imported.get('accepted')is not True or not isinstance(point,list)or len(point)!=3:continue
                point=tuple(round(float(v),5)for v in point)
                if not all(math.isfinite(v)for v in point)or math.dist(seed[:2],point[:2])>.011 or abs(point[2]-old[2])>8 or math.dist(old[:2],point[:2])>bound:continue
                self.pools[i].add(point)
                # A rejected literal pose cannot become a common proposal.
                # Its seed import above still runs: a different GT can reveal
                # a new height, which needs its own exact literal proof.
                if self._eligible(gt,i,point)and point not in row:row[point]=self._valid(gt,i,list(point),snapshot,probe,discover_neighbors=True)
                if self._common(i):break

    def _eligible(self,gt,i,point):
        return not any(self.results[g].get(i,{}).get(point)is False for g in self.gametypes if g!=gt)

    def _common(self,i):
        return any(all(self.results[g].get(i,{}).get(p)is True for g in self.gametypes)for p in self.pools[i])

    def diagnostic(self):
        """Return partial evidence only, including the original native replies."""
        return dict(schema=1,status='incomplete',contract=CONTRACT,map=self.doc['map'],gametypes=list(self.gametypes),
            source_graph_sha256=graph_hash(self.doc),source_ids=sorted(self.anchors),body_profile=copy.deepcopy(self.profile),
            budget=self.limit,query_count=len(self.queries),max_candidates_per_gt=MAX_CANDIDATES,
            max_anchor_probes=max((r['limit']for r in self.anchor_costs.values()),default=0),budget_policy=BUDGET_POLICY,
            anchor_limits=[copy.deepcopy(self.anchor_costs[i])for i in sorted(self.anchors)],
            anchor_queries=[dict(source_id=i,query_count=self.anchor_queries.get(i,0))for i in sorted(self.anchors)],
            pools=[dict(source_id=i,points=[list(p)for p in sorted(self.pools[i])])for i in sorted(self.anchors)],
            results=[dict(gametype=g,source_id=i,point=list(p),accepted=accepted)
                for g in self.gametypes for i in sorted(self.results[g])for p,accepted in sorted(self.results[g][i].items())],
            states=[dict(gametype=g,collision_sha256=value)for g,value in sorted(self.states.items())],
            snapshot_refs=[dict(gametype=v['gametype'],phase=v['phase'],evidence=copy.deepcopy(v['evidence']),
                verification_snapshot=copy.deepcopy(v.get('verification_snapshot')))for v in self.visits],
            final_joint_gametypes=sorted(self.joint_verified),visits=copy.deepcopy(self.visits),queries=copy.deepcopy(self.queries))

    def proposal(self):
        result=copy.deepcopy(self.doc);moves=[]
        for i in sorted(self.anchors):
            old=self.nodes[i]['position'];common=[p for p in self.pools[i]if all(self.results[g].get(i,{}).get(p)is True for g in self.gametypes)]
            if not common:raise self.failure(i,"Keine gemeinsame exakt gestützte Position mit Objectivekontakt und sämtlichen Anschlusswegen")
            point=list(min(common,key=lambda p:(math.dist(old,p),p)))
            if math.dist(old,point)>.00001:
                next(n for n in result['nodes']if n['id']==i)['position']=point
                moves.append(dict(source_id=i,way_guid=self.nodes[i].get('way_guid',0),old_position=old,new_position=point))
        return result,moves

    def verify_joint(self,gt,snapshot,evidence,probe,proposal,moves):
        expected,expected_moves=self.proposal()
        require(graph_hash(expected)==graph_hash(proposal)and expected_moves==moves,"Endvorschlag verändert andere Punkte oder Metadaten.")
        self._state(gt,snapshot,evidence);positions={n['id']:n['position']for n in proposal['nodes']}
        self.visits[-1]['phase']='joint'
        for move in moves:
            i=move['source_id']
            if not self._valid(gt,i,positions[i],snapshot,probe,positions,fresh=True):raise self.failure(i,"Gemeinsame Endposition/verschobene Nachbarkante besteht die finale GT%d-Prüfung nicht"%gt)
        self.joint_verified.add(gt)

    def certificate(self,proposal,moves):
        expected,expected_moves=self.proposal()
        require(graph_hash(expected)==graph_hash(proposal)and expected_moves==moves,"Beleg gehört zu anderem Endvorschlag.")
        require(not moves or self.joint_verified==set(self.gametypes),"Gemeinsame Nachbarkanten wurden nicht abschließend in allen GTs geprüft.")
        return dict(schema=1,contract=CONTRACT,map=self.doc['map'],gametypes=list(self.gametypes),scope='generated_objective_anchors_and_incident_walks_only',
            source_graph_sha256=graph_hash(self.doc),proposal_graph_sha256=graph_hash(proposal),source_ids=sorted(self.anchors),moves=copy.deepcopy(moves),
            anchor_records=copy.deepcopy(list(self.anchors.values())),objectives_by_gt=copy.deepcopy(self.objectives),body_profile=copy.deepcopy(self.profile),
            final_joint_gametypes=sorted(self.joint_verified),max_anchor_probes=max((r['limit']for r in self.anchor_costs.values()),default=0),max_candidates=MAX_CANDIDATES,
            anchor_limits=copy.deepcopy([self.anchor_costs[i]for i in sorted(self.anchors)]),budget_policy=BUDGET_POLICY,
            visits=copy.deepcopy(self.visits),queries=copy.deepcopy(self.queries),query_count=len(self.queries),budget=self.limit,
            ids_flags_radii_properties_and_topology_preserved=True,actual_use_or_capture_proven=False)
