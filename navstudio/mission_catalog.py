"""Read-only BSP/script shortlist for maps with actual GT2 and GT5 missions.

Presence of flags, target volumes and script callbacks is a selection filter,
not a native collision, route, hint, use or mission-success certificate.
"""
import argparse
import json
from pathlib import Path
import re
import sys
import zipfile
from goalgen import objectives
from spawns import bsp_entities


class ArchiveAssets:
    def __init__(self, archive):
        self.archive=archive
        self.names={name.lower():name for name in archive.namelist()}

    def read(self,path,maximum):
        name=self.names[path.lower()]
        if self.archive.getinfo(name).file_size>maximum:raise ValueError('Archive member exceeds its size limit.')
        return self.archive.read(name)


def callback(script,name):
    visible=re.sub(r'"(?:\\.|[^"\\])*"','',without_comments(script))
    return len(re.findall(r'\b(?:trigger\s+)?'+re.escape(name)+r'\s*\{',visible))


def without_comments(script):
    tokens=r'"(?:\\.|[^"\\])*"|/\*[\s\S]*?\*/|//[^\r\n]*'
    return re.sub(tokens,lambda m:m[0] if m[0].startswith('"')else re.sub(r'[^\r\n]',' ',m[0]),script)


def script_blocks(script):
    """Top-level entity blocks, ignoring braces/comments inside string text."""
    clean=without_comments(script)
    masked=re.sub(r'"(?:\\.|[^"\\])*"',lambda m:'"'+(' '*(len(m[0])-2))+'"',clean)
    blocks={};cursor=0
    pattern=re.compile(r'\s*([^\s{}]+|"[^"]+")\s*\{')
    while cursor<len(clean):
        match=pattern.search(clean,cursor)
        if not match:break
        opening=clean.index('{',match.start());depth=1;end=opening+1
        while end<len(masked)and depth:
            depth+=(masked[end]=='{')-(masked[end]=='}');end+=1
        if depth:break
        blocks[match[1].strip('"')]=clean[opening+1:end-1]
        cursor=end
    return blocks


def scan(directories,limit=20,excluded=()):
    excluded={name.lower()for name in excluded};seen=set();result=[];rejected=[]
    archives={p.resolve()for directory in directories for p in Path(directory).glob('*.pk3')}
    for path in sorted(archives,key=lambda p:(p.stat().st_size,p.name.lower())):
        try:
            with zipfile.ZipFile(path)as archive:
                assets=ArchiveAssets(archive)
                members=[name for name in assets.names if re.fullmatch(r'maps/obj_[a-z0-9_-]+\.bsp',name)]
                for member in members:
                    name=Path(member).stem
                    if name in excluded or name in seen:continue
                    scripts={gt:'maps/'+name+'_gt'+str(gt)+'.script'for gt in (2,5)}
                    if not all(p in assets.names for p in scripts.values()):continue
                    first,second=objectives(assets,name,2),objectives(assets,name,5)
                    flags=[o for o in first if o['operation']=='take_flag']
                    caps=[o for o in first if o['operation']=='capture']
                    if len(flags)<2 or len(caps)<2 or len(second)<2:
                        rejected.append(dict(map=name,archive=path.name,flags=len(flags),captures=len(caps),bombsites=len(second)))
                        continue
                    script={gt:assets.read(p,1024*1024).decode('latin1')for gt,p in scripts.items()}
                    clean={gt:without_comments(text)for gt,text in script.items()}
                    if callback(clean[2],'captured')<2 or callback(clean[5],'dynamited')<2 or callback(clean[5],'defused')<2:
                        rejected.append(dict(map=name,archive=path.name,reason='missing_actual_mission_script_callbacks'))
                        continue
                    entities=list(bsp_entities(assets,name));targets={e.get('targetname'):e for e in entities if e.get('targetname')}
                    blocks=script_blocks(script[5]);verified=[]
                    for site in second:
                        target=targets[site['target']];scriptname=target.get('scriptname');body=blocks.get(scriptname,'')
                        if not all(callback(body,event)>0 for event in ('dynamited','defused','death')):break
                        class3=bool(re.search(r'\bconstructible_class\s+3\b|\bconstructible_weaponclass\s+3\b',body))
                        class3|=target.get('constructible_class')=='3'or target.get('constructible_weaponclass')=='3'
                        if not class3:break
                        verified.append(scriptname)
                    if len(verified)!=len(second):
                        rejected.append(dict(map=name,archive=path.name,reason='bomb_target_missing_matching_activation_script'))
                        continue
                    row=dict(map=name,archive=path.name,source=str(path),bytes=path.stat().st_size,
                        bsp_bytes=archive.getinfo(assets.names[member]).file_size,
                        spawns=sum(e.get('classname','').startswith('info_player_')or
                            e.get('classname')in('team_CTF_redspawn','team_CTF_bluespawn')for e in entities),
                        flags=len(flags),captures=len(caps),bombsites=len(second),
                        bomb_targets=[dict(tag=o['tag'],target=o['target'],
                            scriptname=targets[o['target']].get('scriptname'))for o in second],
                        scripts=[assets.names[p]for p in scripts.values()],
                        gt2_capture_callbacks=callback(clean[2],'captured'),
                        gt5_dynamited_callbacks=callback(clean[5],'dynamited'),
                        gt5_defused_callbacks=callback(clean[5],'defused'),
                        gt5_constructible_class3_commands=len(re.findall(r'\bconstructible_class\s+3\b',clean[5])),
                        verified_bomb_target_script_blocks=verified,
                        native_verified=False,mission_behavior_complete=False)
                    result.append(row)
                    seen.add(name)
                    if len(result)>=limit:return result,rejected
        except (zipfile.BadZipFile,KeyError,ValueError,OverflowError)as error:
            rejected.append(dict(archive=path.name,reason=str(error)))
    return result,rejected


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directories',nargs='+',type=Path)
    parser.add_argument('--limit',type=int,default=20)
    parser.add_argument('--exclude-selection',type=Path)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    excluded=[r['map']for r in json.loads(args.exclude_selection.read_text())]if args.exclude_selection else []
    rows,rejected=scan(args.directories,args.limit,excluded)
    args.out.write_text(json.dumps(dict(scope='BSP_script_selection_only_not_native_acceptance',
        candidates=rows,rejected=rejected),ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(str(args.out)+': '+str(len(rows))+' candidates; '+str(len(rejected))+' rejected/diagnostic maps.')


if __name__=='__main__':main()
