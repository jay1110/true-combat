#!/usr/bin/env python3
"""Local actual-input binding controls; no process/game/commands are accessed."""
from copy import deepcopy
from pathlib import Path
import argparse
import hashlib
import json
import pose_eval_fixtures as o
import pose_skin_bound_inputs as bound

ROOT=Path(__file__).resolve().parents[2]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',type=Path,action='append',required=True)
    parser.add_argument('--mod-pk3',type=Path,default=ROOT/'build/navgen-overkill/tce2/pak2.pk3')
    parser.add_argument('--base-pk3',type=Path,default=ROOT/'build/navgen-overkill/etmain/pak0.pk3')
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();checks=0;results=[]
    assets=o.Assets(args.mod_pk3,args.base_pk3)
    body='models/players/temperate/allied/engineer/body.mdm';head='models/players/hud/head.md3'
    try:
        for path in args.capture:
            raw=path.read_bytes();document=json.loads(raw)
            samples,summary=bound.samples(document,assets,path.stem,body,head)
            assert len(samples)==sum(len(phase['consistent_samples']) for phase in document['phases']);checks+=1
            for sample,row in zip(samples,(row for phase in document['phases'] for row in phase['consistent_samples'])):
                assert sample['origin']==row['render']['body']['origin'];checks+=1
                assert [x for axis in sample['axis'] for x in axis]==row['render']['body']['axis'];checks+=1
                assert sample['head_origin']==row['render']['head']['origin'];checks+=1
                assert sample['face']==[row['render']['head'][key] for key in ('frame','oldframe','backlerp')];checks+=1
            original=next(row for phase in document['phases'] for row in phase['consistent_samples'])
            changes=[
                ('missing-binding',lambda row:row.pop('cgame_character_binding')),
                ('not-draw-eligible',lambda row:row['cgame_character_binding'].__setitem__('draw_eligible',False)),
                ('unbounded-character',lambda row:row['cgame_character_binding']['player']['character'].__setitem__('bounded_storage',False)),
                ('body-handle',lambda row:row['render']['body'].__setitem__('hModel',251)),
                ('head-handle',lambda row:row['render']['head'].__setitem__('hModel',229)),
                ('unknown-old-model',lambda row:row['render']['body'].__setitem__('oldframeModel',999)),
                ('metadata-handle',lambda row:row['lerp']['legs']['animationMetadata'].__setitem__('mdxFile',999)),
                ('unbound-animation-name',lambda row:row['lerp']['legs']['animationMetadata'].__setitem__('name','not_installed')),
                ('model-frame-cap',lambda row:row['render']['body'].__setitem__('frame',2**30)),
                ('negative-frame',lambda row:row['render']['body'].__setitem__('oldframe',-1)),
                ('bad-axis',lambda row:row['render']['body']['axis'].__setitem__(0,float('nan'))),
                ('bad-backlerp',lambda row:row['render']['body'].__setitem__('backlerp',1.01)),
                ('hidden-head',lambda row:row['cgame_character_binding'].__setitem__('head_expected_submitted',False)),
                ('carrier-override',lambda row:row['cgame_character_binding'].__setitem__('carrier_override',True)),
                ('different-animation-character',lambda row:row['cgame_character_binding']['animation'].__setitem__('pointer',1)),
            ]
            rejected=[]
            for label,change in changes:
                row=deepcopy(original);change(row)
                try:bound.sample(row,assets,label,body,head)
                except ValueError:rejected.append(label);checks+=1
                else:raise AssertionError('accepted misbound input: '+label)
            for label,wrong_body,wrong_head in [('other-body-family','models/players/temperate/allied/soldier/body.mdm',head),
                                                 ('wrong-head-asset',body,'models/not-the-recorded-head.md3')]:
                try:bound.sample(original,assets,label,wrong_body,wrong_head)
                except ValueError:rejected.append(label);checks+=1
                else:raise AssertionError('accepted mismatched fixture asset: '+label)
            results.append({'file':path.name,'sha256':hashlib.sha256(raw).hexdigest(),'count':len(samples),
                            'phases':summary,'rejected_controls':rejected})
        report={'checks':checks,'failures':0,'inputs':results,'assets':assets.hashes,
                'adapter_sha256':hashlib.sha256(Path(bound.__file__).read_bytes()).hexdigest(),
                'scope':'Input binding/transform preservation only; no measured skinned vertices or game execution'}
        args.out.parent.mkdir(parents=True,exist_ok=True);args.out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        print(json.dumps({'checks':checks,'failures':0,'cases':sum(row['count'] for row in results)}))
    finally:assets.close()


if __name__=='__main__':main()
