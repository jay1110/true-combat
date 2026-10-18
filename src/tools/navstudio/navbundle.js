/* Script companion editor. GM is shown as text, never evaluated in the browser. */
(function () {
  'use strict';
  var $ = function (id) { return document.getElementById(id); };
  var bundle = null, kind = 'script', dirty = false, busy = false, token = '';
  function status(message) { $('navbundle-status').textContent = message; }
  async function request(url, options) {
    var response = await fetch(url, options), result = await response.json();
    if (!response.ok) throw new Error(result.error || response.statusText);
    return result;
  }
  function render() {
    var file = kind === 'mapscript' ? bundle.mapscript : kind === 'custommapscript' ? bundle.custommapscript : bundle.files[kind];
    if (!file) file={exists:false,path:'Map-Script fehlt',text:''};
    $('navbundle-text').value = file.text || '';
    $('navbundle-text').disabled = !file.exists; $('navbundle-text').readOnly = kind === 'mapscript';
    $('navbundle-save').disabled = !file.exists || busy || kind === 'mapscript';
    $('navbundle-gametype').value = bundle.gametype == null ? '' : String(bundle.gametype);
    $('navbundle-spawns').disabled = bundle.game_mode !== 'tce2' || bundle.gametype !== 7;
    $('navbundle-create').disabled = bundle.game_mode !== 'tce2' || bundle.gametype == null;
    $('navbundle-gametype').disabled = bundle.game_mode !== 'tce2';
    $('navbundle-map').value = bundle.map;
    $('navbundle-file').textContent = file.path + (file.exists ? ' · ' + file.bytes + ' Bytes · ' + file.encoding : ' · fehlt / missing');
    $('navbundle-summary').textContent = Object.values(bundle.files).map(function (f) { return f.name + (f.exists ? ' ✓' : ' —'); }).join(' · ');
    $('navbundle-export').href = '/api/navbundle.zip?map=' + encodeURIComponent(bundle.map) + (bundle.gametype == null ? '' : '&gametype=' + bundle.gametype);
    $('navbundle-export').download = bundle.map + '-omnibot.zip';
    dirty = false;
  }
  async function load(name) {
    busy = true; $('navbundle-save').disabled = true; status('Lade / Loading …');
    try {
      var state = await request('/api/state'); token = state.token;
      bundle = await request('/api/navbundle?map=' + encodeURIComponent(name) + ($('navbundle-gametype').value ? '&gametype=' + $('navbundle-gametype').value : ''));
      var names = bundle.names.slice(); if (!names.includes(name)) names.unshift(name);
      $('navbundle-map').replaceChildren();
      names.forEach(function (value) { var option = document.createElement('option'); option.value = value; option.textContent = value; $('navbundle-map').appendChild(option); });
      busy = false; render(); status(bundle.note + '\nGoal/Skript-Paar: ' + bundle.pair_status + (bundle.gametype != null && bundle.pair_status==='absent' ? ' — Laufzeit nutzt das gemeinsame Legacy-Paar, falls vorhanden.' : ''));
    } catch (error) { status(error.message); } finally { busy = false; }
  }
  function canLeave() { if (busy) return false; if (dirty) { status('Ungespeichert: speichern oder Änderungen verwerfen. / Unsaved: save or discard changes.'); return false; } return true; }
  $('navbundle-open').onclick = function () { $('navbundle-dialog').showModal(); load($('map-select').value); };
  $('navbundle-text').oninput = function () { dirty = true; status('Ungespeichert / Unsaved'); };
  $('navbundle-map').onchange = function () { var selected = this.value; if (canLeave()) load(selected); else this.value = bundle.map; };
  ['script', 'goals', 'mapscript', 'custommapscript'].forEach(function (value) { $('navbundle-' + value).onclick = function () { if (canLeave() && bundle) { kind = value; render(); status(kind==='mapscript' && bundle.mapscript ? 'wm_announce / OnTrigger Treffer: '+(bundle.mapscript.matches||[]).map(x=>x.message+' -> '+x.callback).join(' | ')+'\nAndere Trigger koennen aus Spielereignissen stammen.' : ''); } }; });
  $('navbundle-reload').onclick = function () { if (!busy && bundle) load(bundle.map); };
  $('navbundle-close').onclick = function () { if (canLeave()) $('navbundle-dialog').close(); };
  $('navbundle-dialog').addEventListener('cancel', function (event) { if (!canLeave()) event.preventDefault(); });
  $('navbundle-save').onclick = async function () {
    if (busy || !bundle) return;
    busy = true; this.disabled = true;
    try {
      var result = await request('/api/navbundle', {method:'POST', headers:{'Content-Type':'application/json','X-Navstudio-Token':token},
        body:JSON.stringify({map:bundle.map, game_mode:bundle.game_mode, gametype:bundle.gametype, kind:kind, text:$('navbundle-text').value, sha256:(kind==='custommapscript'?bundle.custommapscript:bundle.files[kind]).sha256})});
      var currentName=bundle.map; busy=false; await load(currentName);
      if(kind==='goals'&&window.navstudioClient?.app.map===currentName)await window.navstudioClient.loadGoals?.();
      status((result.note || 'Unverändert / Unchanged') + (result.backup ? '\nBackup: ' + result.backup : ''));
    } catch (error) { status(error.message); } finally { busy = false; $('navbundle-save').disabled = kind==='mapscript' || !(kind==='custommapscript'?bundle.custommapscript:bundle.files[kind]).exists; }
  };
  $('navbundle-gametype').onchange=function(){ if(canLeave() && bundle) load(bundle.map); else this.value=bundle.gametype==null?'':String(bundle.gametype); };
  $('navbundle-spawns').onclick=async function(){
    if(!canLeave() || !bundle)return;busy=true;
    try{var result=await request('/api/navbundle',{method:'POST',headers:{'Content-Type':'application/json','X-Navstudio-Token':token},body:JSON.stringify({operation:'generate_spawns',map:bundle.map,game_mode:bundle.game_mode,gametype:bundle.gametype})});busy=false;await load(bundle.map);status(result.count+' Spawns: '+result.path+' � '+result.note);}catch(e){status(e.message);}finally{busy=false;}
  };
  $('navbundle-create').onclick=async function(){
    if(!canLeave() || !bundle)return;
    busy=true;
    try {
      var result=await request('/api/navbundle',{method:'POST',headers:{'Content-Type':'application/json','X-Navstudio-Token':token},body:JSON.stringify({operation:'create_pair',map:bundle.map,game_mode:bundle.game_mode,gametype:bundle.gametype})});
      busy=false; await load(bundle.map); status(result.note);
    }catch(e){status(e.message);}finally{busy=false;}
  };
  window.addEventListener('beforeunload', function (event) { if (dirty) { event.preventDefault(); event.returnValue = ''; } });
  window.navstudioBundle={canClose:function(){
    if(busy){status('Datei wird geladen/gespeichert. Vor dem Schließen den Auftrag abwarten. / Loading or saving; wait before closing.');return false;}
    return canLeave();
  },open:async function(name,gametype,fileKind){
    if(!canLeave())return;
    kind=fileKind||'script';$('navbundle-gametype').value=gametype==null?'':String(gametype);
    $('navbundle-dialog').showModal();await load(name);
  },refreshIfOpen:async function(name,gametype){
    if(!$('navbundle-dialog').open||!bundle||bundle.map!==name||bundle.gametype!==(gametype==null?null:Number(gametype)))return false;
    if(dirty||busy){status('Live-Datei gespeichert. Ungespeicherten Text zuerst sichern oder verwerfen; er wird nicht überschrieben.');return false;}
    await load(name);return true;
  }};
}());
