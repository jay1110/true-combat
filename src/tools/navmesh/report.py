#!/usr/bin/env python3
"""Create an offline, dependency-free floor preview for TCE Recast candidates.

Usage: python report.py mesh.json --geometry obj_overkill.geometry.json --way-report obj_overkill_auto.json --out preview.html
This compares candidate geometry with existing navigation. It never creates WAY
files, installs navigation, or certifies engine traversal / complete map coverage.
"""
import argparse
from collections import Counter
import json
import math
from pathlib import Path


def finite(value, label):
    if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value):
        raise ValueError("invalid finite number: " + label)
    return float(value)


def integer(value, label):
    if isinstance(value, bool) or not isinstance(value, int):
        raise ValueError("invalid integer: " + label)
    return value


def point(value, label):
    if not isinstance(value, list) or len(value) != 3:
        raise ValueError("expected XYZ point: " + label)
    return [finite(v, label) for v in value]


def polygon_areas(points):
    projected = abs(sum(a[0] * b[1] - b[0] * a[1]
                        for a, b in zip(points, points[1:] + points[:1]))) * .5
    surface = 0.0
    origin = points[0]
    for i in range(1, len(points) - 1):
        a = [points[i][j] - origin[j] for j in range(3)]
        b = [points[i + 1][j] - origin[j] for j in range(3)]
        cross = (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
        surface += math.sqrt(sum(v * v for v in cross)) * .5
    return projected, surface


def prepare(mesh_path, way_path=None, geometry_path=None):
    mesh = json.loads(mesh_path.read_text(encoding="utf-8-sig"))
    if mesh.get("schema") != 1 or mesh.get("coordinate_system") != "TCE_Z_up":
        raise ValueError("expected schema 1 mesh in TCE_Z_up coordinates")
    if mesh.get("status") != "geometry_candidate_not_engine_validated":
        raise ValueError("unsupported mesh status; expected geometry candidate")
    profile = mesh.get("profile", {})
    for name in ("height", "radius", "step", "cell", "cell_height"):
        if finite(profile.get(name), "profile." + name) <= 0:
            raise ValueError("profile values must be positive")
    vertices = [point(v, "mesh vertex") for v in mesh["vertices"]]
    source_polygons = mesh["polygons"]
    if not vertices or not source_polygons:
        raise ValueError("mesh has no vertices or polygons")
    parents = list(range(len(source_polygons)))

    def root(i):
        while parents[i] != i:
            parents[i] = parents[parents[i]]
            i = parents[i]
        return i

    polygons, adjacency, floors = [], set(), Counter()
    regions = set()
    for i, poly in enumerate(source_polygons):
        indices = poly["vertices"]
        if not 3 <= len(indices) <= 64 or len(set(indices)) != len(indices):
            raise ValueError("invalid polygon vertex count/duplicates")
        for index in indices:
            if not 0 <= integer(index, "polygon vertex") < len(vertices):
                raise ValueError("polygon vertex index out of range")
        neighbors = poly["neighbors"]
        if len(neighbors) != len(indices):
            raise ValueError("polygon needs one neighbor entry per edge")
        for neighbor in neighbors:
            if not -1 <= integer(neighbor, "neighbor") < len(source_polygons) or neighbor == i:
                raise ValueError("polygon neighbor index out of range/self-link")
            if neighbor >= 0:
                adjacency.add((i, neighbor))
                parents[root(i)] = root(neighbor)
        regions.add(integer(poly["region"], "region"))
        points = [vertices[index] for index in indices]
        projected, surface = polygon_areas(points)
        zs = [p[2] for p in points]
        average = sum(zs) / len(zs)
        floors[int(math.floor(average / 32 + .5)) * 32] += projected
        # Vertices, component, minimum Z, maximum Z, average Z, projected area.
        polygons.append([indices, -1, min(zs), max(zs), average, projected, surface])
    grouped = {}
    for i in range(len(polygons)):
        grouped.setdefault(root(i), []).append(i)
    groups = sorted(grouped.values(), key=lambda group: (-sum(polygons[i][5] for i in group), group[0]))
    components = []
    for component, group in enumerate(groups):
        for i in group:
            polygons[i][1] = component
        components.append(dict(id=component, polygons=len(group),
                               projected_area=sum(polygons[i][5] for i in group),
                               surface_area=sum(polygons[i][6] for i in group),
                               min_z=min(polygons[i][2] for i in group),
                               max_z=max(polygons[i][3] for i in group)))
    warnings = []
    geometry = None
    geometry_complete = None
    if geometry_path:
        manifest = json.loads(geometry_path.read_text(encoding="utf-8-sig"))
        if manifest.get("format") != "tce2_bsp_collision_geometry_v1":
            raise ValueError("unsupported geometry manifest format")
        geometry_complete = manifest.get("geometry_complete")
        if not isinstance(geometry_complete, bool):
            raise ValueError("geometry manifest must explicitly state geometry_complete")
        counts = manifest.get("counts", {})
        exclusions = {}
        for count_name, array_name in (("unsupported_world_patches", "unsupported_collision_patches"),
                                       ("excluded_inline_models", "excluded_inline_models"),
                                       ("failed_collision_brushes", "failed_brushes")):
            count = integer(counts.get(count_name), count_name)
            if count < 0 or not isinstance(manifest.get(array_name), list) or len(manifest[array_name]) != count:
                raise ValueError("geometry exclusion count/list mismatch: " + count_name)
            exclusions[count_name] = count
        if geometry_complete and any(exclusions.values()):
            raise ValueError("geometry manifest claims completeness while excluding collision geometry")
        geometry = dict(source=str(geometry_path.resolve()), map=manifest.get("map"),
                        geometry_complete=geometry_complete, exclusions=exclusions,
                        failed_brushes=manifest["failed_brushes"],
                        unsupported_collision_patches=manifest["unsupported_collision_patches"],
                        excluded_inline_models=manifest["excluded_inline_models"],
                        collision_filter=manifest.get("collision_filter"), input=manifest.get("input"),
                        output=manifest.get("output"), limitations=manifest.get("limitations", []),
                        mesh_provenance_independently_verified=False)
        if not geometry_complete:
            warnings.append("INCOMPLETE COLLISION GEOMETRY: "
                            f"{exclusions['unsupported_world_patches']} unsupported patches, "
                            f"{exclusions['excluded_inline_models']} excluded inline models, "
                            f"{exclusions['failed_collision_brushes']} failed collision brushes. "
                            "Missing obstacles/floors can create false passages and missing surfaces.")
    else:
        warnings.append("No geometry manifest supplied: excluded collision geometry is unknown.")
    asymmetric = sum((b, a) not in adjacency for a, b in adjacency)
    if asymmetric:
        warnings.append(f"{asymmetric} directed neighbor entries lack reverse entries; components use weak connectivity.")
    way_nodes, way_edges, way_summary = [], [], None
    if way_path:
        way = json.loads(way_path.read_text(encoding="utf-8-sig"))
        if geometry and geometry.get("map") and way.get("map") and geometry["map"] != way["map"]:
            raise ValueError("geometry manifest and WAY report refer to different maps")
        offset = finite(way.get("bbox", {}).get("origin_above_feet", 0), "way origin offset")
        if "origin_above_feet" not in way.get("bbox", {}):
            warnings.append("WAY report has no origin_above_feet: Z coordinates shown without adjustment.")
        ids = {}
        for node in way["nodes"]:
            if not node.get("accepted"):
                continue
            node_id = integer(node["id"], "waypoint ID")
            if node_id in ids:
                raise ValueError("duplicate accepted waypoint ID")
            xyz = point(node["position"], "waypoint position")
            xyz[2] -= offset
            ids[node_id] = len(way_nodes)
            way_nodes.append(xyz + [node_id, bool(node.get("crouch")), str(node.get("name", ""))])
        for edge in way["edges"]:
            if edge.get("accepted"):
                if edge["from"] not in ids or edge["to"] not in ids:
                    raise ValueError("accepted WAY edge references excluded/unknown endpoint")
                way_edges.append([ids[edge["from"]], ids[edge["to"]]])
        way_edges = sorted({tuple(e) for e in way_edges})
        way_summary = dict(source=str(way_path.resolve()), map=way.get("map"),
                           accepted_nodes=len(way_nodes), directed_links=len(way_edges),
                           origin_above_feet_adjustment=offset,
                           quarantined_nodes=way.get("quarantined_unreachable_nodes"),
                           source_coverage=way.get("coverage"), source_traversal=way.get("traversal"))
    bounds = [[min(p[axis] for p in vertices), max(p[axis] for p in vertices)] for axis in range(3)]
    summary = dict(schema=1, status="geometry_candidate_not_engine_validated", source=str(mesh_path.resolve()),
                   coordinate_system="TCE_Z_up", profile=profile, vertices=len(vertices), polygons=len(polygons),
                   regions=len(regions), weak_components=len(components), components=components,
                   directed_neighbor_entries=len(adjacency), asymmetric_neighbor_entries=asymmetric,
                   projected_polygon_area=sum(p[5] for p in polygons), surface_polygon_area=sum(p[6] for p in polygons),
                   area_units="square TCE map units; projected areas sum all storeys, not unique map footprint",
                   bounds=bounds, waypoints=way_summary, geometry=geometry,
                   geometry_complete=geometry_complete, warnings=warnings,
                   engine_traversal_verified=False, whole_map_coverage_verified=False,
                   limitations=["Polygon adjacency is geometric, not proof of bot or player traversal.",
                                "Candidate polygon area is not independently certified playable area.",
                                "No coverage percentage can be inferred from this preview.",
                                "Doors, movers, jumps, ladders, game scripts and tactical behavior need separate validation."])
    data = dict(vertices=vertices, polygons=polygons, components=components, floors=sorted(floors.items()),
                bounds=bounds, way_nodes=way_nodes, way_edges=way_edges, summary=summary)
    return summary, data


HTML = r'''<!doctype html>
<html lang="de"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>TCE2 · BSP-Navigationsvorschau</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#111720;color:#e1e8f1;font:14px system-ui,sans-serif}header{padding:15px 20px;background:#192330;border-bottom:1px solid #334557}h1{font-size:20px;margin:0 0 7px}.notice{color:#ffd08b;line-height:1.5}.layout{display:grid;grid-template-columns:300px 1fr;height:calc(100vh - 115px);min-height:520px}aside{padding:15px;overflow:auto;border-right:1px solid #334557}label{display:block;margin:12px 0 5px}select,input[type=range]{width:100%}select,button{background:#253448;color:#e1e8f1;border:1px solid #52647a;padding:6px;border-radius:4px}button{cursor:pointer}p{line-height:1.5}.muted{color:#a4b3c6;font-size:12px}#stage{position:relative;min-width:0}canvas{display:block;width:100%;height:100%;touch-action:none}#viewstats{position:absolute;top:12px;left:15px;background:#111d;padding:8px;border-radius:4px;pointer-events:none}#hover{position:absolute;bottom:12px;left:15px;right:15px;background:#111d;padding:8px;pointer-events:none}#counts{line-height:1.7}input[type=checkbox]{margin-right:7px}.swatch{display:inline-block;width:10px;height:10px;margin-right:5px}.row{display:flex;gap:8px}.row button{flex:1}summary{cursor:pointer}pre{white-space:pre-wrap;overflow-wrap:anywhere;font-size:11px}@media(max-width:760px){.layout{grid-template-columns:230px 1fr}header{padding:10px}h1{font-size:17px}}
</style>
<header><h1>TCE2 · BSP-Navigationsvorschau</h1><div class="notice">Geometrie-Kandidat — keine freigegebenen Waypoints. Bewegung in der Engine und vollständige Mapabdeckung sind nicht bestätigt.</div><div class="notice" id="geometrywarning"></div><div class="muted" id="filename"></div></header>
<div class="layout"><aside>
<div id="counts"></div>
<label><input type="checkbox" id="allfloors">Alle Höhen gleichzeitig</label>
<label for="floor">Bodenhöhe / Etage</label><select id="floor"></select>
<input id="height" type="range" step="2" aria-label="Bodenhöhe"><div id="heightvalue" class="muted"></div>
<label for="band">Höhenband (± Units)</label><select id="band"><option>16</option><option>32</option><option selected>48</option><option>64</option><option>128</option></select>
<label for="component">Geometrische Komponente</label><select id="component"><option value="-1">Alle Komponenten</option></select>
<label><input type="checkbox" id="mesh" checked>Polygone anzeigen</label>
<label><input type="checkbox" id="borders" checked>Polygonränder anzeigen</label>
<label><input type="checkbox" id="points" checked>Bestehende Waypoints</label>
<label><input type="checkbox" id="links">Bestehende Verbindungen</label>
<div class="row"><button id="fit">Ansicht einpassen</button><button id="full">Gesamte Map</button></div>
<p class="muted">Ziehen: verschieben · Mausrad: zoomen.<br>Farben unterscheiden geometrische Komponenten. Weiße Punkte: Waypoints, orange: Duckpunkte, pink: benannte Punkte. Verbindungen sind die vorhandenen WAY-Links.</p>
<p class="muted">Das Höhenband schneidet nach Polygonhöhen. Rampen können in mehreren Bändern erscheinen. Die angezeigte Fläche summiert Polygone und Etagen; sie ist kein Nachweis begehbarer Gesamtfläche.</p>
<details><summary>Profil und Einschränkungen</summary><pre id="details"></pre></details>
</aside><div id="stage"><canvas id="canvas"></canvas><div id="viewstats"></div><div id="hover">Ansicht von oben · X rechts / Y oben · Fußhöhe Z</div></div></div>
<script id="data" type="application/json">__DATA__</script>
<script>
'use strict';
const D=JSON.parse(document.getElementById('data').textContent),$=id=>document.getElementById(id),C=$('canvas'),ctx=C.getContext('2d');
const nf=n=>Math.round(n).toLocaleString('de-DE'),color=i=>`hsl(${(i*137.508+178)%360} 60% 59%)`;
let W=1,H=1,scale=1,ox=0,oy=0,drawPending=false,visiblePolys=[],visibleNodes=[],drag=null,moved=false;
$('filename').textContent=D.summary.source;
if(D.summary.geometry){const g=D.summary.geometry,e=g.exclusions;$('geometrywarning').textContent=g.geometry_complete?'Geometrie laut Manifest vollständig; Engine-Prüfung weiterhin offen.':`Kollisionsgeometrie UNVOLLSTÄNDIG: ${e.unsupported_world_patches} Patches, ${e.excluded_inline_models} Inline-Modelle und ${e.failed_collision_brushes} fehlerhafte Brushes fehlen. Scheinbare Durchgänge können dadurch falsch sein.`;}else $('geometrywarning').textContent='Kein Geometrie-Manifest: fehlende Kollisionsflächen unbekannt.';
$('counts').innerHTML=`<b>${nf(D.polygons.length)} Polygone</b><br>${nf(D.components.length)} geometrische Komponenten<br>${nf(D.way_nodes.length)} vorhandene Waypoints<br>${nf(D.way_edges.length)} gerichtete WAY-Links`;
$('details').textContent=JSON.stringify({profile:D.summary.profile,area_units:D.summary.area_units,geometry_complete:D.summary.geometry_complete,geometry_exclusions:D.summary.geometry?.exclusions,warnings:D.summary.warnings,limitations:D.summary.limitations,geometry_limitations:D.summary.geometry?.limitations},null,2);
for(const c of D.components){const op=document.createElement('option');op.value=c.id;op.textContent=`${c.id+1}: ${nf(c.polygons)} Polygone`; $('component').appendChild(op);}
let largest=D.floors.reduce((a,b)=>a[1]>=b[1]?a:b);
for(const [z,area] of D.floors){let op=document.createElement('option');op.value=z;op.textContent=`Z ${z} · ca. ${nf(area)} u²`;$('floor').appendChild(op);}
$('floor').value=largest[0];$('height').min=Math.floor(D.bounds[2][0])-32;$('height').max=Math.ceil(D.bounds[2][1])+32;$('height').value=largest[0];
if(!D.way_nodes.length){$('points').checked=false;$('points').disabled=true;$('links').disabled=true;}
function band(){return [Number($('height').value),Number($('band').value),$('allfloors').checked];}
function refresh(){let [z,b,all]=band(),comp=Number($('component').value);visiblePolys=D.polygons.filter(p=>(comp<0||p[1]===comp)&&(all||p[2]<=z+b&&p[3]>=z-b));visibleNodes=D.way_nodes.filter(p=>all||Math.abs(p[2]-z)<=b);$('heightvalue').textContent=all?'Alle Bodenhöhen':`Z ${z} · Bereich ${z-b} bis ${z+b}`;$('height').disabled=$('floor').disabled=$('band').disabled=all;requestDraw();}
function requestDraw(){if(!drawPending){drawPending=true;requestAnimationFrame(()=>{drawPending=false;draw();});}}
const screen=p=>[ox+p[0]*scale,oy-p[1]*scale];
function draw(){ctx.clearRect(0,0,W,H);ctx.fillStyle='#101720';ctx.fillRect(0,0,W,H);
 const grid=Math.pow(2,Math.ceil(Math.log2(65/scale))),xl=-ox/scale,xr=(W-ox)/scale,yb=(oy-H)/scale,yt=oy/scale;
 ctx.strokeStyle='#223041';ctx.lineWidth=1;ctx.beginPath();for(let x=Math.ceil(xl/grid)*grid;x<xr;x+=grid){let s=ox+x*scale;ctx.moveTo(s,0);ctx.lineTo(s,H);}for(let y=Math.ceil(yb/grid)*grid;y<yt;y+=grid){let s=oy-y*scale;ctx.moveTo(0,s);ctx.lineTo(W,s);}ctx.stroke();
 if($('mesh').checked)for(const p of visiblePolys){ctx.beginPath();p[0].forEach((index,i)=>{let [x,y]=screen(D.vertices[index]);if(i)ctx.lineTo(x,y);else ctx.moveTo(x,y);});ctx.closePath();ctx.fillStyle=color(p[1]);ctx.globalAlpha=.32;ctx.fill();ctx.globalAlpha=1;if($('borders').checked){ctx.strokeStyle=color(p[1]);ctx.lineWidth=.7;ctx.stroke();}}
 let [z,b,all]=band();if($('links').checked){ctx.strokeStyle='#d5deef';ctx.globalAlpha=.28;ctx.lineWidth=.75;ctx.beginPath();for(const [a,bn] of D.way_edges){let p=D.way_nodes[a],q=D.way_nodes[bn];if(!all&&(Math.abs(p[2]-z)>b||Math.abs(q[2]-z)>b))continue;let [x,y]=screen(p),[u,v]=screen(q);ctx.moveTo(x,y);ctx.lineTo(u,v);}ctx.stroke();ctx.globalAlpha=1;}
 if($('points').checked)for(const p of visibleNodes){let [x,y]=screen(p);if(x<-5||x>W+5||y<-5||y>H+5)continue;ctx.beginPath();ctx.arc(x,y,p[5]?3.8:2.6,0,Math.PI*2);ctx.fillStyle=p[5]?'#f99ded':p[4]?'#ffb856':'#fff';ctx.fill();ctx.strokeStyle='#111';ctx.lineWidth=.8;ctx.stroke();}
 $('viewstats').textContent=`Sichtbar: ${nf(visiblePolys.length)} Polygone · ${nf(visibleNodes.length)} Waypoints · Raster ${nf(grid)} u`;
 ctx.fillStyle='#b4c6da';ctx.font='12px system-ui';ctx.fillText(`${nf(grid)} u`,16,H-44);ctx.strokeStyle='#b4c6da';ctx.beginPath();ctx.moveTo(16,H-35);ctx.lineTo(16+grid*scale,H-35);ctx.stroke();
}
function fit(full=false){let ids=new Set();for(const p of visiblePolys)for(const i of p[0])ids.add(i);let points=full?D.vertices:Array.from(ids,i=>D.vertices[i]);if(!points.length)points=visibleNodes;if(!points.length)points=D.vertices;let lo=[Infinity,Infinity],hi=[-Infinity,-Infinity];for(const p of points)for(let i=0;i<2;i++){lo[i]=Math.min(lo[i],p[i]);hi[i]=Math.max(hi[i],p[i]);}scale=Math.min((W-80)/Math.max(hi[0]-lo[0],32),(H-80)/Math.max(hi[1]-lo[1],32));scale=Math.max(scale,.001);ox=W/2-(lo[0]+hi[0])/2*scale;oy=H/2+(lo[1]+hi[1])/2*scale;requestDraw();}
function resize(){const r=C.getBoundingClientRect(),dpr=window.devicePixelRatio||1;W=r.width;H=r.height;C.width=Math.round(W*dpr);C.height=Math.round(H*dpr);ctx.setTransform(dpr,0,0,dpr,0,0);requestDraw();}
for(const id of ['allfloors','band','component','mesh','borders','points','links'])$(id).addEventListener('change',refresh);
$('height').addEventListener('input',()=>{let near=D.floors.reduce((a,b)=>Math.abs(a[0]-$('height').value)<Math.abs(b[0]-$('height').value)?a:b);$('floor').value=near[0];refresh();});
$('floor').addEventListener('change',()=>{$('height').value=$('floor').value;refresh();});$('fit').onclick=()=>fit();$('full').onclick=()=>fit(true);
C.addEventListener('wheel',e=>{e.preventDefault();let r=C.getBoundingClientRect(),x=e.clientX-r.left,y=e.clientY-r.top,f=Math.exp(-Math.max(-300,Math.min(300,e.deltaY))*.0015),next=Math.max(.002,Math.min(20,scale*f));f=next/scale;ox=x+(ox-x)*f;oy=y+(oy-y)*f;scale=next;requestDraw();},{passive:false});
C.addEventListener('pointerdown',e=>{drag=[e.clientX,e.clientY,ox,oy];moved=false;C.setPointerCapture(e.pointerId);});
C.addEventListener('pointermove',e=>{if(drag){ox=drag[2]+e.clientX-drag[0];oy=drag[3]+e.clientY-drag[1];moved=true;requestDraw();return;}let r=C.getBoundingClientRect(),x=e.clientX-r.left,y=e.clientY-r.top,world=[(x-ox)/scale,(oy-y)/scale],best=null,dist=100;for(const p of visibleNodes){let s=screen(p),d=(s[0]-x)**2+(s[1]-y)**2;if(d<dist){dist=d;best=p;}}$('hover').textContent=best?`Waypoint ${best[3]} · ${best.slice(0,3).map(v=>v.toFixed(1)).join(', ')}${best[4]?' · Ducken':''}${best[5]?' · '+best[5]:''}`:`X ${world[0].toFixed(0)} · Y ${world[1].toFixed(0)} · Z-Höhenband über Regler wählen`;});
for(const name of ['pointerup','pointercancel'])C.addEventListener(name,()=>{drag=null;});
new ResizeObserver(resize).observe($('stage'));resize();refresh();fit();
</script></html>'''


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("mesh", type=Path)
    parser.add_argument("--way-report", type=Path)
    parser.add_argument("--geometry", type=Path, help="BSP collision export manifest; exposes missing collision geometry")
    parser.add_argument("--out", required=True, type=Path, help="standalone HTML; numeric summary is written beside it")
    args = parser.parse_args()
    summary_path = args.out.with_suffix(".summary.json")
    inputs = {args.mesh.resolve()}
    if args.way_report:
        inputs.add(args.way_report.resolve())
    if args.geometry:
        inputs.add(args.geometry.resolve())
    if args.out.resolve() in inputs or summary_path.resolve() in inputs or summary_path.resolve() == args.out.resolve():
        parser.error("outputs must not overwrite input files or each other")
    try:
        summary, data = prepare(args.mesh, args.way_report, args.geometry)
        payload = json.dumps(data, ensure_ascii=True, separators=(",", ":"), allow_nan=False).replace("<", "\\u003c")
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(HTML.replace("__DATA__", payload), encoding="utf-8")
        summary_path.write_text(json.dumps(summary, indent=2, allow_nan=False) + "\n", encoding="utf-8")
        print(json.dumps(dict(html=str(args.out.resolve()), summary=str(summary_path.resolve()),
                              polygons=summary["polygons"], weak_components=summary["weak_components"],
                              status=summary["status"]), indent=2))
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, "navmesh report: " + str(error) + "\n")


if __name__ == "__main__":
    main()
