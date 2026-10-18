/* Project desktop shell. All changes use the existing revision-checked APIs. */
(function () {
  'use strict';
  const $ = id => document.getElementById(id), client = window.navstudioClient;
  document.querySelector('[data-action="generate"]').textContent='Netz, Goals & Spawns automatisch erstellen';
  const model = {active:null, projects:[], catalog:[], step:0, busy:false, entered:false, spawns:null, spawn:null,spawnBusy:false};
  const welcome = document.createElement('section'); welcome.id = 'project-welcome';
  welcome.innerHTML = '<div class="welcome-card"><div><p class="eyebrow">TCE2 NAVSTUDIO</p><h1>Eine Map. Ein Projekt.</h1>' +
    '<p>Wegnetz, Goals und Spawns werden zusammen gespeichert. Wähle ein vorhandenes Projekt oder lege ein neues an.</p>' +
    '<button id="welcome-new" class="button primary">Neues Projekt erstellen</button><button id="welcome-current" class="button secondary" hidden>Laufendes Projekt anzeigen</button><button id="welcome-settings" class="button secondary">Spielpfad / Einstellungen</button>' +
    '<p id="project-welcome-error" role="alert"></p></div><div><h2>Vorhandene Projekte</h2><div id="project-recent" class="project-recent-list">Projekte werden geladen …</div></div></div>';
  document.body.append(welcome);
  const wizard = document.createElement('dialog'); wizard.id = 'project-wizard';
  wizard.innerHTML = '<h2>Neues Map-Projekt</h2><div class="wizard-steps"><span>Name</span><span>Spiel</span><span>Map</span><span>Navigation</span></div>' +
    '<section class="wizard-step"><label for="wizard-name">Projektname</label><input id="wizard-name" type="text" maxlength="100" autocomplete="off" placeholder="z. B. Delta – mein Wegnetz">' +
    '<p class="hint">Jedes Projekt erhält einen eigenen Ordner. Andere Karten und ihre Prüfergebnisse bleiben getrennt.</p></section>' +
    '<section class="wizard-step"><p>Für welches Spiel?</p><div class="wizard-mode"><label><input type="radio" name="wizard-mod" value="tce2" checked> TCE2<small>obj_-Maps, Gametype 2 / 5 / 7</small></label>' +
    '<label><input type="radio" name="wizard-mod" value="et"> Enemy Territory<small>ET-Maps mit originalem Omni-bot-Mod</small></label></div></section>' +
    '<section class="wizard-step"><label for="wizard-map-search">Map aus all_maps oder der Installation</label><input id="wizard-map-search" type="search" placeholder="Map suchen …"><select id="wizard-map" size="7"></select>' +
    '<p id="wizard-map-info" class="hint"></p><label for="wizard-gametype">Gametype</label><select id="wizard-gametype"><option value="2">GT2</option><option value="5">GT5</option><option value="7">GT7</option></select></section>' +
    '<section class="wizard-step"><p id="wizard-summary"></p><div class="wizard-mode"><label><input type="radio" name="wizard-navigation" value="generate" checked> Automatisch generieren<small>BSP + Recast, Sprünge, Leitern, Duckstellen und native Prüfung</small></label>' +
    '<label><input type="radio" name="wizard-navigation" value="import"> Vorhandenes Netz importieren<small>.way und passende .gm-Dateien in dieses Projekt übernehmen</small></label></div>' +
    '<p id="wizard-generator-note" class="hint"></p><label class="toggle"><input id="wizard-goals" type="checkbox" checked><span>Goals für GT2 und GT5 erstellen</span></label><p class="hint">Die Generierung öffnet die Map in der bestehenden sichtbaren Testinstanz. Restfehler erscheinen direkt unter der Kartenansicht.</p></section>' +
    '<p id="project-wizard-error" role="alert"></p><div class="wizard-buttons"><button id="wizard-cancel" class="button secondary">Abbrechen</button><button id="wizard-back" class="button secondary">Zurück</button><button id="wizard-next" class="button primary">Weiter</button></div>';
  document.body.append(wizard);
  const spawns = document.createElement('section'); spawns.className = 'panel spawn-panel'; spawns.id = 'spawns-panel';
  spawns.innerHTML = '<p class="eyebrow">SPAWNS</p><label class="toggle"><input id="spawn-visible" type="checkbox" checked><span>Spawns in der Karte anzeigen</span></label>' +
    '<p class="hint">BSP = Blau · eigene Spawns = Grün. Eigene Spawns mit der Maus auswählen und ziehen.</p><div id="spawn-list" class="spawn-list"></div>' +
    '<p id="spawn-selection" class="hint">Spawn auswählen</p><div class="spawn-coordinates"><label>X<input id="spawn-x" type="number" step="1"></label><label>Y<input id="spawn-y" type="number" step="1"></label><label>Z<input id="spawn-z" type="number" step="1"></label></div>' +
    '<label>Blickrichtung<input id="spawn-angle" type="number" step="5" value="0"></label><div class="button-pair"><button id="spawn-move" class="button secondary">Verschieben</button><button id="spawn-delete" class="button secondary">Löschen</button></div>' +
    '<label class="toggle"><input id="spawn-enabled" type="checkbox" checked><span>Eigene GT7-Spawns nutzen</span></label><div class="button-pair"><button id="spawn-generate" class="button secondary">Generieren</button><button id="spawn-apply" class="button secondary">GT7-Script speichern</button></div><p id="spawn-status" class="hint" role="status"></p>';
  document.querySelector('.inspector').append(spawns);
  const error = document.createElement('p'); error.id = 'project-error'; error.hidden = true;
  $('workflow-toolbar').append(error);
  const camera = document.createElement('label'); camera.className = 'toolbar-toggle';
  camera.innerHTML = 'Kamera <select id="camera-speed"><option value="100">Langsam</option><option value="300" selected>Normal</option><option value="600">Schnell</option></select>';
  document.querySelector('.edit-toolbar').append(camera);
  const etInfo=document.createElement('p');etInfo.id='et-gametype-info';etInfo.className='hint';etInfo.hidden=true;
  etInfo.textContent='Normales ET verwendet hier Objective GT2. Goals und Maplogik bilden ein gemeinsames .gm-Paar ohne Gametype-Suffix.';
  $('test-gametype').after(etInfo);
  const tree = [
    ['tree-way','◇','Wegnetz'],['tree-goals-2','⚑','Goals · GT2'],['tree-goals-5','⚑','Goals · GT5'],
    ['tree-spawns','⊕','Spawns'],['tree-script','≡','Map-Script'],['tree-live','◎','Omni-bot Live-Editor']];
  tree.forEach(([id,mark,label]) => { const button=document.createElement('button'); button.id=id; button.type='button';
    const icon=document.createElement('span');icon.textContent=mark; const text=document.createElement('span');text.textContent=label;
    button.append(icon,text); $('project-tree').append(button); });

  function showError(message) {
    $('project-error').textContent=message || ''; $('project-error').hidden=!message;
    $('project-welcome-error').textContent=message || ''; $('project-wizard-error').textContent=message || '';
    if (message && model.entered) client.notice(message);
  }
  async function api(path, payload) {
    const headers={'Content-Type':'application/json'};
    if(payload) {
      if(!client.app.state) await client.refresh(false);
      headers['X-Navstudio-Token']=client.app.state.token;
    }
    return client.request(path, payload ? {method:'POST',headers,body:JSON.stringify(payload)} : undefined);
  }
  function lock(value) {
    model.busy=value;
    ['project-new','project-open','project-save','project-close','welcome-new','wizard-next','wizard-back'].forEach(id => $(id).disabled=value);
    renderSpawnControls();
    client.renderPackageControls();
  }
  function absorb(result) {
    if('active' in result) model.active=result.active;
    if(result.projects)model.projects=result.projects;
    if(result.catalog)model.catalog=result.catalog;
  }
  async function refreshProjects() {
    const result=await api('/api/projects'); absorb(result); renderProjects(); return result;
  }
  function renderProjects() {
    $('project-recent').replaceChildren();
    if(!model.projects.length){ const p=document.createElement('p');p.className='hint';p.textContent='Noch keine Projekte. Links ein neues Projekt erstellen.';$('project-recent').append(p); }
    model.projects.forEach(project=>{
      const button=document.createElement('button'); const name=document.createElement('strong'); name.textContent=project.name;
      const meta=document.createElement('small');meta.textContent=(project.mod==='et'?'ET':'TCE2')+' · '+project.map+' · GT'+project.gametype;
      button.append(name,meta);button.onclick=()=>open(project.id);$('project-recent').append(button);
    });
    if(model.active){
      const et=model.active.mod==='et';
      $('project-title').textContent=model.active.name;
      $('project-map').textContent=model.active.map+' · '+(et?'ET · Objective GT2':'TCE2 · GT'+model.active.gametype);
      $('project-path').textContent=model.active.path || model.active.paths?.root || '';
      client.updateGametypeAvailability();
      document.querySelector('label[for="test-gametype"]').textContent=et?'ET Objective · GT2':'TCE2 Test-Gametype (Neustart)';
      etInfo.hidden=!et;
      $('tree-goals-2').lastElementChild.textContent=et?'ET-Goals':'Goals · GT2';$('tree-goals-5').hidden=et;
    }
    $('project-save').disabled=model.busy || !model.entered;
    $('project-close').disabled=model.busy || !model.entered;
    client.renderPackageControls();
  }
  async function enter(project) {
    if(!project)throw new Error('Projekt wurde nicht geöffnet.');
    if(project.id!==model.enteredId || !model.entered){model.spawn=null;model.spawns=null;model.lastJob=null;model.pendingGoals=null;}
    model.active=project;model.entered=true;model.enteredId=project.id;welcome.hidden=true;document.body.classList.remove('project-inactive');
    client.app.initialized=false;await client.refresh(false);
    $('map-select').value=project.map;$('test-gametype').value=project.mod==='et'?'2':String(project.gametype);
    await client.loadScene(project.map,true);
    renderProjects();await refreshSpawns();
  }
  async function saveClose() {
    const result=await api('/api/project',{op:'save_close'});absorb(result);model.entered=false;model.enteredId=null;model.pendingGoals=null;model.spawn=null;model.spawns=null;
    if(client.app.viewer)client.app.viewer.stop?.();
    client.app.map='';client.app.scene=null;client.app.draft=null;
    document.body.classList.add('project-inactive');welcome.hidden=false;renderProjects();return result;
  }
  async function open(id) {
    if(model.busy)return;lock(true);showError('');
    try {
      // A CLI/batch may have opened the listed project after this window's
      // welcome page loaded. Joining that current project is a read-only act.
      await refreshProjects();
      if(model.active?.id===id){await enter(model.active);return;}
      if(model.active && model.active.id!==id)await saveClose();
      const result=await api('/api/project',{op:'open',id});absorb(result);await enter(result.active);
    }catch(e){showError(e.message);}finally{lock(false);renderProjects();}
  }
  function chosenMod(){return document.querySelector('[name="wizard-mod"]:checked').value;}
  function selectedMap(){return model.catalog.find(m=>m.source_id===$('wizard-map').value);}
  function filterMaps() {
    const et=chosenMod()==='et';$('wizard-gametype').disabled=et;
    document.querySelector('label[for="wizard-gametype"]').textContent=et?'ET Objective · GT2':'Gametype';
    Array.from($('wizard-gametype').options).forEach(o=>{o.disabled=et&&o.value!=='2';o.hidden=et&&o.value!=='2';});
    if(et)$('wizard-gametype').value='2';
    $('wizard-goals').closest('label').hidden=et;
    const generate=document.querySelector('[name="wizard-navigation"][value="generate"]');
    const unavailable=chosenMod()==='et'&&!client.app.state?.capabilities?.et_generation;
    generate.disabled=unavailable;
    if(unavailable)document.querySelector('[name="wizard-navigation"][value="import"]').checked=true;
    $('wizard-generator-note').textContent=unavailable?'ET: Vorhandene WAY-/GM-Dateien importieren. Automatische Generierung benötigt den ET-Prüfadapter.':'';
    const old=$('wizard-map').value, needle=$('wizard-map-search').value.trim().toLowerCase();
    $('wizard-map').replaceChildren();
    model.catalog.filter(m=>m.mod===chosenMod() && (!needle || (m.map+' '+m.filename).toLowerCase().includes(needle))).forEach(map=>{
      const option=document.createElement('option');option.value=map.source_id;option.textContent=map.map+' · '+(map.location==='all_maps'?'all_maps':'installiert');$('wizard-map').append(option);
    });
    if(Array.from($('wizard-map').options).some(o=>o.value===old))$('wizard-map').value=old;
    mapInfo();
  }
  function mapInfo(){ const map=selectedMap();$('wizard-map-info').textContent=map?map.filename+' · '+map.location+' · '+Math.round(map.size/1024/1024)+' MB':'Keine Map ausgewählt.';
    client.configureGametypeSelect($('wizard-gametype'),map?.gametype_availability,null,chosenMod()==='et'); }
  function step(value) {
    model.step=value;
    wizard.querySelectorAll('.wizard-step').forEach((p,i)=>p.hidden=i!==value);
    wizard.querySelectorAll('.wizard-steps span').forEach((p,i)=>p.classList.toggle('active',i===value));
    $('wizard-back').hidden=value===0;$('wizard-next').textContent=value===3?'Projekt erstellen':'Weiter';
    const map=selectedMap();$('wizard-summary').textContent=$('wizard-name').value+' · '+(chosenMod()==='et'?'ET':'TCE2')+' · '+(map?.map || '')+' · GT'+$('wizard-gametype').value;
    if(value===0)$('wizard-name').focus();
  }
  async function newProject() {
    try { await refreshProjects();$('wizard-name').value='';$('wizard-map-search').value='';filterMaps();step(0);showError('');wizard.showModal(); }
    catch(e){showError(e.message);}
  }
  async function finishWizard() {
    const map=selectedMap();if(!map)throw new Error('Bitte eine Map wählen.');
    if(!$('wizard-gametype').value)throw new Error('Kein unterstützter Gametype mit Mapscript vorhanden.');
    lock(true);
    try {
      if(model.active)await saveClose();
      const result=await api('/api/project',{op:'create',name:$('wizard-name').value.trim(),map:map.map,mod:chosenMod(),gametype:chosenMod()==='et'?2:Number($('wizard-gametype').value),source_id:map.source_id});
      absorb(result);await enter(result.active);
      const operation=document.querySelector('[name="wizard-navigation"]:checked').value;
      wizard.close();
      if(operation==='import') {
        const imported=await api('/api/project',{op:'import_bundle'});absorb(imported);await client.loadScene(map.map,false);
        client.notice(imported.imported?.length?'Vorhandenes WAY-/GM-Paket in das Projekt importiert.':
          'Keine vorhandenen WAY-/GM-Dateien gefunden. Mit „Netz generieren“ dieses Projekt vorbereiten.');
      } else {
        const withGoals=chosenMod()==='tce2'&&$('wizard-goals').checked;
        // One server pipeline owns graph, goals and spawns. Do not enqueue a
        // second goal generation when its graph job finishes.
        await client.action('generate', {project_goals:withGoals,goal_gametypes:goalGametypes()});
      }
    }finally{lock(false);renderProjects();}
  }
  $('wizard-next').onclick=async()=>{
    showError('');
    try {
      if(model.step===0&&!$('wizard-name').value.trim())throw new Error('Bitte einen Projektnamen eingeben.');
      if(model.step===2&&!selectedMap())throw new Error('Bitte eine Map wählen.');
      if(model.step===3)await finishWizard();else step(model.step+1);
    }catch(e){showError(e.message);}
  };
  $('wizard-back').onclick=()=>step(Math.max(0,model.step-1));
  $('wizard-cancel').onclick=()=>wizard.close();$('wizard-map-search').oninput=filterMaps;$('wizard-map').onchange=mapInfo;
  document.querySelectorAll('[name="wizard-mod"]').forEach(r=>r.onchange=filterMaps);
  $('project-new').onclick=newProject;$('welcome-new').onclick=newProject;
  $('welcome-current').onclick=()=>{if(model.active)open(model.active.id);};
  $('welcome-settings').onclick=()=>$('settings-open').click();
  $('project-open').onclick=async()=>{await refreshProjects();welcome.hidden=false;};
  $('project-save').onclick=async()=>{
    if(model.busy || !model.entered)return;lock(true);showError('');
    try {
      absorb(await api('/api/project',{op:'save',gametype:Number($('test-gametype').value)}));renderProjects();
      await client.loadScene(model.active.map,false);
      client.notice('Projekt gespeichert: '+model.active.name+'. Mit „Ins Spiel übernehmen“ wird die aktuelle Änderung geladen.');
    }catch(e){showError(e.message);}finally{lock(false);}
  };
  $('project-close').onclick=async()=>{if(model.busy)return;lock(true);showError('');try{await saveClose();}catch(e){showError(e.message);}finally{lock(false);}};
  $('apply-draft').onclick=()=>client.action(client.app.draft?'apply_draft':'install_test');
  $('tree-way').onclick=()=>{$('mouse-begin').click();};
  async function openScript(gt,kind){
    if(!model.entered)return;
    if(window.navstudioBundle)await window.navstudioBundle.open(model.active.map,gt,kind);
    else $('navbundle-open').click();
  }
  $('tree-goals-2').onclick=()=>openScript(2,'goals');$('tree-goals-5').onclick=()=>openScript(5,'goals');
  function goalGametypes(){return (client.app.scene?.gametype_availability?.available||[2,5]).map(Number).filter(gt=>[2,5].includes(gt));}
  const goalGenerate=document.createElement('button');goalGenerate.id='project-generate-goals';goalGenerate.textContent='⚑ Goals GT2 + GT5 generieren';
  goalGenerate.dataset.action='generate_goals';goalGenerate.onclick=()=>{if(!goalGenerate.disabled)client.action('generate_goals',{gametypes:goalGametypes()});};$('project-tree').append(goalGenerate);
  const goalApply=document.createElement('button');goalApply.id='project-apply-goals';goalApply.dataset.action='apply_goals';$('project-tree').append(goalApply);
  const goalHint=document.createElement('p');goalHint.id='project-goals-restart';goalHint.className='hint';
  goalHint.textContent='Goals übernehmen startet die Map mit dem ausgewählten Gametype neu. Vorher das geprüfte Wegnetz ins Spiel übernehmen.';$('project-tree').append(goalHint);
  function renderGoalControls(){
    const et=model.active?.mod==='et',gt=et?2:Number($('test-gametype').value),scene=client.app.scene;
    const busy=model.busy||client.app.actionPending||client.app.loading||client.app.state?.busy||
      (client.app.state?.jobs||[]).some(j=>['queued','running','starting'].includes(j.status));
    const valid=!!(model.entered&&model.active&&[2,5].includes(gt)&&scene?.proof_status?.valid&&
      scene.report_status?.current&&scene.report_status?.generation_complete&&scene.audit_state==='current');
    goalApply.textContent=et?'ET-Goals ins Spiel übernehmen':'Goals für GT'+gt+' ins Spiel übernehmen';goalApply.disabled=busy||!valid;
    goalApply.title=valid?'Startet dieselbe Testinstanz mit diesem Gametype neu.':'Aktuelles Wegnetz zuerst prüfen und ins Spiel übernehmen; GT2 oder GT5 wählen.';
    const types=goalGametypes();
    goalGenerate.textContent='⚑ Goals '+types.map(gt=>'GT'+gt).join(' + ')+' generieren';
    goalGenerate.disabled=busy||!model.entered||model.active?.mod!=='tce2'||!types.length;
    ['2','5'].forEach(value=>{$('tree-goals-'+value).disabled=!et&&!types.includes(Number(value));});
    $('tree-script').disabled=!et&&!$('test-gametype').value;
    goalGenerate.hidden=et;
    goalHint.textContent=et?'Goals übernehmen startet dieselbe ET-Map als Objective GT2 neu. Vorher das geprüfte Wegnetz ins Spiel übernehmen.':
      'Goals übernehmen startet die Map mit dem ausgewählten Gametype neu. Vorher das geprüfte Wegnetz ins Spiel übernehmen.';
  }
  goalApply.onclick=()=>{
    renderGoalControls();if(goalApply.disabled)return;
    client.action('apply_goals',{gametype:Number($('test-gametype').value)});
  };
  $('test-gametype').addEventListener('change',renderGoalControls);
  $('tree-script').onclick=()=>openScript(Number($('test-gametype').value),'mapscript');
  $('tree-spawns').onclick=()=>$('spawns-panel').scrollIntoView({block:'nearest'});$('tree-live').onclick=()=>$('botmenu-open').click();
  $('camera-speed').onchange=()=>{if(client.app.viewer){client.app.viewer._cam.baseSpeed=Number($('camera-speed').value);client.app.viewer.setSpeedMultiplier?.(1);client.app.viewer._cam.speed=Number($('camera-speed').value);}};
  // Keep guide inside the standalone application instead of opening a browser.
  $('guide-open').onclick=event=>{event.preventDefault();let dialog=$('studio-help-dialog');if(!dialog){dialog=document.createElement('dialog');dialog.id='studio-help-dialog';dialog.innerHTML='<button class="button secondary">Schließen</button><iframe src="/guide.html" title="NavStudio Anleitung"></iframe>';dialog.querySelector('button').onclick=()=>dialog.close();document.body.append(dialog);}dialog.showModal();};
  function reloadWindow(){
    sessionStorage.setItem('navstudio-reload-project',model.enteredId||'');
    location.reload();
  }
  const reload=document.createElement('button');reload.id='project-reload-ui';reload.className='button secondary';
  reload.textContent='↻';reload.title='Oberfläche neu laden (F5 / Strg+R)';reload.onclick=reloadWindow;
  $('language-toggle').before(reload);
  document.addEventListener('keydown',event=>{
    if(event.code==='F5'||((event.ctrlKey||event.metaKey)&&event.key.toLowerCase()==='r')){
      event.preventDefault();event.stopImmediatePropagation();reloadWindow();
    }
  },true);
  async function refreshSpawns() {
    if(!model.entered || !model.active)return;
    try {
      const project=model.active.id,map=model.active.map,data=await api('/api/spawns?map='+encodeURIComponent(map));
      if(!model.entered||model.active?.id!==project||model.active?.map!==map)return;
      model.spawns=data;
      renderSpawns();
    }catch(e){$('spawn-status').textContent=e.message;}
  }
  function canEditSpawns(){return !!(model.entered&&model.active?.mod==='tce2'&&!model.busy&&!model.spawnBusy&&
    !client.app.loading&&!client.app.actionPending&&!client.app.state?.busy&&
    !(client.app.state?.jobs||[]).some(j=>['queued','running','starting'].includes(j.status)));}
  function renderSpawnControls(){
    const ready=canEditSpawns(),scene=client.app.scene,proof=scene?.proof_status?.valid&&scene.report_status?.current&&scene.report_status?.generation_complete;
    $('spawn-enabled').disabled=!ready;$('spawn-generate').disabled=!ready||!proof;
    $('spawn-apply').disabled=!ready||!(model.spawns?.nodes||[]).some(n=>n.editable);
    $('spawn-generate').title=model.active?.mod==='et'?'Eigene GT7-Spawns gehören zum TCE2-Mod.':'Aktuelles Wegnetz zuerst nativ prüfen.';
    ['x','y','z','angle','move','delete'].forEach(part=>$('spawn-'+part).disabled=!ready||!model.spawn?.editable);
  }
  function renderSpawns() {
    const data=model.spawns;if(!data)return;
    $('spawn-enabled').checked=data.enabled;$('spawn-list').replaceChildren();
    (data.nodes||[]).forEach(spawn=>{const button=document.createElement('button');button.dataset.spawnId=spawn.id;
      button.textContent=(spawn.kind==='bsp'?'BSP · ':'Eigen · ')+spawn.id+' · '+spawn.classname;
      button.onclick=()=>selectSpawn(spawn.id,true);$('spawn-list').append(button);});
    if(client.app.viewer?.setNavigationSpawns)client.app.viewer.setNavigationSpawns($('spawn-visible').checked?
      data.nodes.map(n=>Object.assign({origin_above_feet:data.origin_above_feet||24},n)):[]);
    const selected=(data.nodes||[]).find(n=>n.id===model.spawn?.id);model.spawn=selected||null;
    renderSpawnSelection();renderSpawnControls();
  }
  function selectSpawn(id,focus){model.spawn=(model.spawns?.nodes||[]).find(s=>s.id===id)||null;renderSpawnSelection();
    if(model.spawn){client.app.viewer?.selectNavigation('spawn',id);if(focus)client.focus(model.spawn.position);}}
  function renderSpawnSelection(){const spawn=model.spawn;
    $('spawn-selection').textContent=spawn?(spawn.editable?'Eigener Spawn · verschieben / löschen':'BSP-Spawn · unveränderlich')+' · '+spawn.id:'Spawn auswählen';
    const editable=spawn?.editable&&canEditSpawns();
    ['x','y','z'].forEach((axis,i)=>{$('spawn-'+axis).value=spawn?spawn.position[i]:'';$('spawn-'+axis).disabled=!editable;});
    $('spawn-angle').value=spawn?.angle||0;$('spawn-angle').disabled=!editable;
    $('spawn-move').disabled=$('spawn-delete').disabled=!editable;
    $('spawn-list').querySelectorAll('button').forEach(b=>b.classList.toggle('spawn-selected',b.dataset.spawnId===spawn?.id));
  }
  async function spawnOperation(op,extra){
    if(!model.spawns || !model.entered)return;
    if(!canEditSpawns()){renderSpawns();return;}
    const project=model.active.id,map=model.active.map;
    model.spawnBusy=true;renderSpawnControls();
    try{const result=await api('/api/spawns',Object.assign({map,revision:model.spawns.revision,op},extra||{}));
      if(!model.entered||model.active?.id!==project||model.active?.map!==map)return;
      if(result.job){client.app.watchedJob=result.job.id;await client.refresh(false);}
      if(result.nodes)model.spawns=result;else await refreshSpawns();renderSpawns();$('spawn-status').textContent=result.note || 'Spawn-Änderung im Projekt gespeichert.';
    }catch(e){
      if(model.entered&&model.active?.id===project){renderSpawns();$('spawn-status').textContent=e.message;}
    }finally{model.spawnBusy=false;renderSpawnControls();}
  }
  $('spawn-visible').onchange=renderSpawns;$('spawn-enabled').onchange=()=>spawnOperation('enabled',{enabled:$('spawn-enabled').checked});
  $('spawn-generate').onclick=()=>spawnOperation('generate',{count:24});$('spawn-apply').onclick=()=>spawnOperation('apply');
  $('spawn-delete').onclick=()=>{if(model.spawn?.editable)spawnOperation('delete',{id:model.spawn.id});};
  $('spawn-move').onclick=()=>{if(model.spawn?.editable)spawnOperation('move',{id:model.spawn.id,position:['x','y','z'].map(axis=>Number($('spawn-'+axis).value)),angle:Number($('spawn-angle').value)});};
  window.addEventListener('navstudio:spawn-select',event=>{model.spawn=event.detail.item;renderSpawnSelection();});
  window.addEventListener('navstudio:spawn-move',event=>spawnOperation('move',{id:event.detail.id,position:event.detail.position,angle:model.spawn?.angle||0}));
  window.addEventListener('navstudio:scene',()=>{if(model.entered)refreshSpawns();renderGoalControls();});
  window.addEventListener('navstudio:state',event=>{
    renderGoalControls();
    renderSpawnControls();
    const remote=event.detail.active_project;
    $('welcome-current').hidden=!remote;
    if(!model.entered && remote && remote.id!==model.active?.id){
      model.active=remote;
      if(!model.projects.some(p=>p.id===remote.id))model.projects.unshift(remote);
      renderProjects();
    }
    if(model.entered && remote && remote.id!==model.enteredId && !model.syncing){
      model.syncing=true;enter(remote).catch(e=>showError(e.message)).finally(()=>model.syncing=false);
    }
    if(model.entered && remote?.id===model.active?.id){
      const jobs=(event.detail.jobs||[]).filter(j=>j.map===model.active.map);
      const last=jobs[jobs.length-1];
      if(last && ['complete','completed','failed'].includes(last.status) && model.lastJob!==last.id){
        model.lastJob=last.id;
        if(!client.app.loading)client.loadScene(model.active.map,false);
      }
    }
  });
  document.addEventListener('keydown',event=>{
    if(!model.entered || /^(INPUT|TEXTAREA|SELECT|BUTTON)$/.test(event.target.tagName) || document.querySelector('dialog[open]'))return;
    if(client.app.viewer?._radiantNavigating)return;
    if((event.ctrlKey||event.metaKey)&&event.key.toLowerCase()==='s'){event.preventDefault();$('project-save').click();}
    if(event.key.toLowerCase()==='f' && !event.ctrlKey){const s=client.app.viewer?._navigationSelection;if(s){client.focus(s.item.position || s.item.pos);event.preventDefault();}}
    if(event.key==='Delete'&&model.spawn?.editable&&client.app.viewer?._navigationSelection?.kind==='spawn'){event.preventDefault();$('spawn-delete').click();}
  });
  window.navstudioProjects={model,showError,refresh:refreshProjects,open,saveClose,refreshSpawns,canEditSpawns};
  const reloadingProject=sessionStorage.getItem('navstudio-reload-project');sessionStorage.removeItem('navstudio-reload-project');
  refreshProjects().then(result=>{if(reloadingProject&&result.active?.id===reloadingProject)enter(result.active).catch(e=>showError(e.message));}).catch(e=>showError(e.message));
}());
