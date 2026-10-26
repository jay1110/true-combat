/* TCE2 Navstudio: local-only client. All generation/validation/install work is
   performed by the backend; drawing a candidate never marks it verified. */
(function () {
  'use strict';
  var $ = function (id) { return document.getElementById(id); };
  var app = {viewer: null, state: null, scene: null, map: '', loading: false,
    serial: 0, actionPending: false, exportPending: false, watchedJob: null, completedJobs: new Set(), collision: null, initialized: false,
    draft: null, sourceScene: null, goalOverlay: null, editing: false, editNode: null,
    editTool: 'select', linkStart: null, drag: null, cameraDrag: null, suppressClick: false};
  var actionNames = {apply_draft: 'Änderungen ins Spiel übernehmen', repair_check: 'Verbindungen reparieren & prüfen', start_test: 'Sichtbares Testspiel starten', load_map: 'Map im Testspiel öffnen',
    generate_goals: 'Goals für GT2 und GT5 generieren', apply_goals: 'Goals ins Spiel übernehmen', apply_spawns: 'GT7-Spawns prüfen und speichern', capture_navigation: 'Wegnetz im Spiel fotografieren',
    generate: 'Netz, Goals & Spawns automatisch erstellen',
    stop_test: 'Testspiel beenden', rebuild: 'Recast & Wegnetz erzeugen',
    native_check: 'Native Bewegungsprüfung', repair: 'Pfade nachbessern', snapshot: 'Mapmodelle erfassen',
    inspect: 'Stand prüfen', install_test: 'Testnavigation installieren', install_production: 'TCE2-Navigation installieren'};
  var busyStates = ['queued', 'running', 'pending', 'starting'];
  var statusNames = {native_stale: 'Native Prüfung veraltet', native_checked_not_live_certified: 'Nativ geprüft · Live-Abnahme offen',
    native_failed: 'Aktuelle Prüfung fehlgeschlagen',
    candidate_not_native_validated: 'Kandidat · native Prüfung offen', manual_draft: 'Bearbeitungsentwurf · nicht geprüft', bsp_only: 'BSP geladen · Navigation fehlt',
    queued: 'wartet', running: 'läuft', complete: 'abgeschlossen', completed: 'abgeschlossen', failed: 'fehlgeschlagen'};
  var accepted = function (item) { return item.accepted !== false && item.valid !== false && item.status !== 'rejected' && item.state !== 'rejected'; };
  function actionRadiusNode(node) {
    var props=node.action_properties||{},physical=(BigInt(node.nav_flags||0)&((1n<<9n)|(1n<<27n)|(1n<<8n)|(1n<<18n)))!==0n;
    return node.generated_guide||node.action_roles?.length||node.editor_role||physical||node.door_recipe||
      ['tce_ascent_release','tce_ascent_alignment','tce_walk_drop','tce_stand_jump','tce_free_climb','tce_door_role'].some(key=>props[key]);
  }
  var position = function (node) { return node && (node.position || node.pos); };
  var text = function (id, value) { $(id).textContent = String(value == null ? '—' : value); };

  async function handledFetch(url, options) {
    try { return await fetch(url, Object.assign({cache: 'no-store'}, options || {})); }
    catch (_) { throw new Error('Der lokale NavStudio-Dienst ist nicht erreichbar. TCE2-NavStudio.exe erneut öffnen und die Seite aktualisieren. Ein eventuell gesendeter Auftrag wird nicht automatisch wiederholt.'); }
  }
  async function request(url, options) {
    var response = await handledFetch(url, options);
    var data;
    try { data = await response.json(); } catch (_) { throw new Error('Ungültige Serverantwort (' + response.status + ')'); }
    if (!response.ok || data.error && !data.jobs && !data.status) {
      throw new Error(typeof data.error === 'string' ? data.error : JSON.stringify(data.error || data));
    }
    return data;
  }
  function availabilityRows(availability) {
    if (!availability) return null;
    var rows = availability.gametypes || {};
    return [2, 5, 7].map(function (gt) {
      var row = Array.isArray(rows) ? rows.find(function (item) { return Number(item.gametype ?? item.id) === gt; }) : rows[String(gt)];
      row = row || {};
      return {gametype: gt, available: Array.isArray(availability.available) ? availability.available.map(Number).includes(gt) : row.available === true,
        reasons: row.reasons || (row.reason ? [row.reason] : [])};
    });
  }
  function configureGametypeSelect(select, availability, preferred, et) {
    var rows = et ? [{gametype: 2, available: true}] : availabilityRows(availability);
    var available = rows ? rows.filter(function (row) { return row.available; }).map(function (row) { return row.gametype; }) : [2, 5, 7];
    Array.from(select.options).forEach(function (option) {
      var gt = Number(option.value), row = rows?.find(function (item) { return item.gametype === gt; });
      option.disabled = !available.includes(gt); option.hidden = !!et && gt !== 2;
      option.textContent = 'GT' + gt + (option.disabled && !et ? ' · nicht vorhanden' : '');
      option.title = (row?.reasons || []).map(function (reason) { return typeof reason === 'string' ? reason : reason.message || reason.reason || ''; }).filter(Boolean).join('\n');
    });
    var desired = preferred == null ? Number(select.value) : Number(preferred);
    if (!available.includes(desired)) desired = available[0];
    select.value = desired == null ? '' : String(desired);
    select.disabled = !!et || !available.length;
    return available;
  }
  function updateGametypeAvailability(preferred) {
    var et = app.state?.game_mode === 'et', select = $('test-gametype');
    var projectGT = app.state?.active_project?.gametype;
    var rows = availabilityRows(app.scene?.gametype_availability);
    if (preferred == null && projectGT != null && (!select.value || rows && !rows.some(function (row) { return row.available && row.gametype === Number(select.value); }))) preferred = projectGT;
    var available = configureGametypeSelect(select, app.scene?.gametype_availability, preferred, et);
    if ($('gametype-availability-status')) {
      var unavailable = rows ? rows.filter(function (row) { return !row.available; }).map(function (row) {
        return 'GT' + row.gametype + ': ' + (row.reasons.length ? row.reasons.map(function (reason) { return typeof reason === 'string' ? reason : reason.message || reason.reason || ''; }).filter(Boolean).join('; ') : 'kein passendes Mapscript');
      }) : [];
      text('gametype-availability-status', et ? 'ET Objective · GT2' : !rows ? '' :
        (available.length ? 'Verfügbar: ' + available.map(function (gt) { return 'GT' + gt; }).join(', ') : 'Kein unterstützter Gametype mit Mapscript vorhanden.') + (unavailable.length ? '\n' + unavailable.join('\n') : ''));
    }
    return available;
  }
  function renderPackageControls() {
    var button = $('project-package-export'); if (!button) return;
    var busy = app.loading || app.actionPending || app.exportPending || app.state?.busy ||
      (app.state?.jobs || []).some(function (job) { return busyStates.includes(job.status); }) || window.navstudioProjects?.model.busy;
    var status = app.scene?.package_status;
    button.disabled = !!busy || !app.map || status?.ready !== true;
    text('project-package-status', app.exportPending ? 'ZIP wird vorbereitet …' : busy ? 'Auftrag läuft. ZIP-Export anschließend verfügbar.' :
      !app.map ? 'Zuerst ein Map-Projekt öffnen.' : status?.ready === true ? 'Wegpunkte, Goals und Mapscripts sind bereit.' :
      status?.reasons?.length ? status.reasons.join('\n') : 'Zuerst Netz, Goals und Spawns automatisch erstellen.');
  }
  async function exportProjectPackage() {
    renderPackageControls(); if ($('project-package-export').disabled) return;
    var map = app.map; app.exportPending = true; renderPackageControls(); notice('');
    try {
      var response = await handledFetch('/api/project-package.zip?map=' + encodeURIComponent(map));
      if (!response.ok) {
        var failure; try { failure = await response.json(); } catch (_) {}
        throw new Error(failure?.error || 'ZIP-Export fehlgeschlagen (' + response.status + ').');
      }
      if (!(response.headers.get('Content-Type') || '').toLowerCase().includes('application/zip')) throw new Error('ZIP-Export lieferte keine ZIP-Datei.');
      var blob = await response.blob(); if (!blob.size) throw new Error('ZIP-Export lieferte eine leere Datei.');
      // ALLOW_DOWNLOADS in the desktop host presents its native Save dialog.
      // Fetch first so JSON validation errors cannot become downloaded ZIPs.
      var link = document.createElement('a'), url = URL.createObjectURL(blob);
      link.href = url; link.download = map + '-navigation.zip'; link.hidden = true;
      document.body.append(link); link.click(); link.remove();
      setTimeout(function () { URL.revokeObjectURL(url); }, 1000);
      notice('ZIP zum Speichern bereit: ' + map + '-navigation.zip');
    } catch (error) { notice(error.message); }
    finally { app.exportPending = false; renderPackageControls(); }
  }
  function notice(message) { $('notice').hidden = !message; text('notice', message || ''); }
  function setLoading(on, message) {
    app.loading = on; $('loading').hidden = !on;
    $('map-select').disabled = on; $('reload-scene').disabled = on;
    if (message) text('loading-detail', message);
    mouseStatus(); renderPackageControls();
  }
  function setMode(inspect) {
    if (!app.viewer) return;
    cancelGesture();
    app.viewer.setNavigationInspect(inspect);
    $('inspect-mode').classList.toggle('active', inspect); $('fly-mode').classList.toggle('active', !inspect);
    $('inspect-mode').setAttribute('aria-pressed', inspect); $('fly-mode').setAttribute('aria-pressed', !inspect);
    text('camera-help', inspect ? 'Links: auswählen / Werkzeug · Rechts ziehen: drehen · Mitte ziehen: verschieben · Mausrad: vor/zurück · Auswahl auch durch Wände' :
      'Ins Bild klicken · WASD bewegen · Maus drehen · Leertaste hoch · C runter · Shift schnell · Esc Maus lösen');
  }
  function navigationData(scene) {
    var nav = Object.assign({nodes: [], edges: []}, scene.navigation || {});
    nav.meshes = Object.assign({}, scene.meshes || nav.meshes || {});
    nav.origin_above_feet=Number(scene.native_summary?.bbox?.origin_above_feet || app.sourceScene?.native_summary?.bbox?.origin_above_feet || (app.state?.game_mode==='et'?24:30));
    // Native checking must not hide names/facing imported into the authored
    // draft. Source IDs are stable; never substitute a guessed WAY UID.
    if(app.draft?.nodes){var authored=new Map(app.draft.nodes.map(n=>[String(n.id),n]));nav.nodes=(nav.nodes||[]).map(function(n){var saved=authored.get(String(n.id));if(!saved)return n;var result=Object.assign({},n);['name','facing','way_guid'].forEach(key=>{if(saved[key]!=null)result[key]=saved[key];});return result;});}
    nav.pairs = ((scene.native_summary || {}).local_shortcuts || {}).cases || [];
    if (app.collision) nav.meshes.collision = app.collision;
    return nav;
  }
  function layerOptions() {
    var options = {edges: $('edge-mode').value, style: $('view-preset').value,
      maxDistance: Number($('nav-distance').value),
      minZ: $('min-z').value === '' ? -Infinity : Number($('min-z').value),
      maxZ: $('max-z').value === '' ? Infinity : Number($('max-z').value)};
    document.querySelectorAll('[data-layer]').forEach(function (el) { options[el.dataset.layer] = el.checked; });
    return options;
  }
  function presetView() {
    var ingame = $('view-preset').value === 'ingame';
    var layers = {bsp: true, nodes: true, accepted: true, rejected: false, xray: false, goals: true, labels: true,
      stand: !ingame, duck: false, collision: false};
    document.querySelectorAll('[data-layer]').forEach(function (el) { el.checked = layers[el.dataset.layer]; });
    $('edge-mode').value = ingame ? 'all' : 'selected';
    text('view-preset-help', ingame ? 'Magenta Pfosten = Wegpunkte · Weiß = gerichtete Verbindungen. Ansicht sofort sichtbar, kein Spielstart nötig.' :
      'Türkis = Stehflächen · Farben zeigen Prüfstatus. Punkt auswählen, um seine gerichteten Verbindungen und Ablehnungen zu sehen.');
    updateLayers();
  }
  async function updateLayers(event) {
    if (!app.viewer) return;
    var collisionToggle = document.querySelector('[data-layer="collision"]');
    if (collisionToggle.checked && !app.collision && app.scene) {
      collisionToggle.disabled = true;
      try {
        var map = app.map, mesh = await request('/api/collision?map=' + encodeURIComponent(map));
        if (app.map !== map) return;
        app.collision = mesh.mesh || mesh;
        if (!app.collision.vertices) throw new Error('Kein Kollisionsmesh vorhanden. Zuerst Recast & Wegnetz erzeugen.');
        app.viewer.setNavigation(navigationData(app.scene));
      } catch (error) { collisionToggle.checked = false; notice(error.message); }
      finally { collisionToggle.disabled = false; }
    }
    app.viewer.setNavigationOptions(layerOptions());
  }
  function reasonOf(item) { return String(item.reason || item.rejection_reason || item.failure || item.status || 'ohne Detailgrund'); }
  var traversalHelp = {
    ladder_base_contact_live_pending: 'Leiteranfahrt: Der Bot richtet sich zum Aufstieg an der Leiter aus.',
    ladder_inside_guidance_not_standable_live_pending: 'Inneres Leiterziel: absichtlich im Leiterkontaktbereich, kein gewöhnlicher Standplatz.',
    ladder_top_clearance_live_pending: 'Oberer Einstieg für den gerichteten Abstieg.',
    ladder_exit_floor_live_pending: 'Leiterausstieg auf geprüftem Boden. Die Bot-Ankunft erfordert Bodenkontakt.',
    ladder_upper_alignment_contact_live_pending: 'Oberer Ausrichtungspunkt: Der Bot steigt mit Leiterkontakt in die seitliche Ausstiegsspur. Er muss den Punkt horizontal treffen und seine Höhe aufwärts überschreiten.',
    ladder_ascent_release_contact_live_pending: 'Ablösepunkt: Erst beim aufwärts gerichteten Höhenübertritt wechselt der Bot zum Ausstieg. Sein Ankunftsradius darf diesen Schritt nicht vorzeitig auslösen.',
    ladder_aligned_ascent_live_pending: 'Gerichteter Aufstieg zur seitlichen Ausstiegsspur; volle Körperhülle und Leiterkontakt nativ geprüft.',
    ladder_release_column_live_pending: 'Senkrechter Aufstieg vom Ausrichtungspunkt zum Ablösepunkt.',
    ladder_ballistic_dismount_live_pending: 'Seitlicher Ausstieg: Leiterkontakt halten, danach fallen und auf geprüftem Boden zum Ausstiegsziel weiterlaufen. Die native Bewegungsprüfung ersetzt keinen erfolgreichen Bot-Livetest.',
    generated_jump_takeoff_live_pending: 'Absprungziel mit JUMP-Flag. Erst ein ausgeführter Bot-Sprung belegt die Spielbarkeit.',
    generated_walk_drop_takeoff_live_pending: 'Kontrollierter kurzer Abstieg ohne Sprungimpuls. Gesperrter Startpunkt mit festem Landeziel; benötigt weiterhin einen Bot-Livetest.',
    walk_drop_landing_anchor: 'Geprüfter Bodenanker für einen kurzen Abstieg ohne JUMP-Flag.',
    walk_drop_approach_validated_live_pending: 'Native Anfahrt zum Start des kontrollierten Abstiegs.',
    walk_drop_hull_validated_live_pending: 'Gerichteter kurzer Abstieg: Körperhülle und Landung nativ geprüft, ohne Sprungimpuls.',
    manual_drop_recipe_conflict_or_budget: 'Abstieg nicht bestätigt: widersprüchliches Landeziel oder ausgeschöpftes natives Aktionsbudget. Die Einbahnverbindung allein beweist keine ausführbare Bewegung.',
    manual_jump_launch_capsule_ground: 'Sprungstart mit tatsächlichem Bodenkontakt der Spielerhülle geprüft; auch eine Position knapp an der Kante bleibt gültig.',
    generated_free_climb_launch_live_pending: 'TCE2-Freiklettern: Absprung und Blick nach oben führen an eine höhere Kante. Kein gewöhnlicher Leiterpunkt; Botbedienung separat testen.',
    free_climb_approach_validated_live_pending: 'Native Anfahrt zum TCE2-Freikletterpunkt mit realer Spielerhülle geprüft.',
    free_climb_retreat_validated_live_pending: 'Geprüfter gewöhnlicher Rückweg vom Freikletterstart; der Aufstieg bleibt gerichtet.',
    free_climb_hull_samples_validated_live_pending: 'Gerichteter TCE2-Kantenaufstieg mit Sprungimpuls und Blick nach oben nativ geprüft; die Landung liegt auf begehbarem Boden.',
    manual_free_climb_recipe_rejected: 'Freiklettern nicht ausführbar: Höhe, Körperfreiraum, Kantenkontakt oder Landung passen nicht zur TCE2-Bewegung.',
    manual_free_climb_recipe_conflict_or_limit: 'Freiklettern nicht bestätigt: widersprüchliches Zielrezept oder erschöpftes natives Aktionsbudget.'

  };
  function addDetail(dl, label, value) {
    var row = document.createElement('div'), dt = document.createElement('dt'), dd = document.createElement('dd');
    dt.textContent = label; dd.textContent = typeof value === 'object' ? JSON.stringify(value) : String(value == null ? '—' : value);
    row.append(dt, dd); dl.appendChild(row);
  }
  function doorFields(item){
    var role=(item.action_roles||[]).find(r=>r.kind==='door'),props=item.action_properties||{},raw=item.door_recipe;
    if(!raw&&!role&&!props.tce_door_role)return null;
    var fields=Object.assign({},raw||{});
    ['entity','use_entity','model','use_model','mode','team','role'].forEach(key=>{
      if(fields[key]==null&&props['tce_door_'+key]!=null)fields[key]=Number(props['tce_door_'+key]);
    });
    fields.role_name=role?.role||({1:'use',2:'entry',3:'exit'}[fields.role]);
    fields.entry=role?.entry_node??fields.entry_source_id??null;
    fields.target=role?.target_node??fields.target_source_id??null;
    fields.use=role?.use_node??fields.use_source_id??null;
    fields.aim=fields.aim||(props.tce_door_aim?props.tce_door_aim.split(/\s+/).map(Number):null);
    return fields;
  }
  function renderDoorSelection(dl,item){
    var door=doorFields(item);if(!door)return;
    addDetail(dl,'Türaktion',{use:'Schalter / Tür bedienen',entry:'Tür betreten',exit:'Tür verlassen'}[door.role_name]||'Türrezept');
    if(door.model!=null)addDetail(dl,'Türmodell','*'+door.model);
    if(door.entity!=null)addDetail(dl,'Tür-Entity',door.entity);
    addDetail(dl,'Aktivierung',door.mode===3?'Seitlich öffnen, vollständig öffnen lassen, ausrichten und durchlaufen':door.use_entity===door.entity?'Direkt an der Tür':'Über zugeordneten Schalter');
    if(door.use_model!=null)addDetail(dl,'Aktivatormodell','*'+door.use_model);
    if(door.use_entity!=null)addDetail(dl,'Aktivator-Entity',door.use_entity);
    if(door.entry!=null&&door.entry>=0)addDetail(dl,'Eintrittspunkt',door.entry);
    if(door.target!=null&&door.target>=0)addDetail(dl,door.role_name==='use'?'Anfahrtspunkt':'Austrittspunkt',door.target);
    if(door.use!=null&&door.use>=0)addDetail(dl,'Bedienpunkt',door.use);
    if(door.aim?.length===3)addDetail(dl,'Aktivierungsziel',door.aim.map(n=>Number(n).toFixed(2)).join(' / '));
    addDetail(dl,'Teamfreigabe',{1:'Terrorists / Axis',2:'Specops / Allies',3:'Beide Teams'}[door.team]||'Prüfen');
    addDetail(dl,'Türprüfstand',app.scene?.report_status?.current?'Native Geometrie geprüft; Botbedienung separat testen.':'Entwurfsrezept – aktuelle Türprüfung fehlt.');
  }
  function selectNavigation(selection) {
    $('selection-details').replaceChildren(); $('selection-neighbors').replaceChildren(); $('focus-selection').hidden = !selection;
    if (!selection) {
      text('selection-title', 'Einen Punkt auswählen');
      text('selection-description', 'Prüfergebnis, Koordinaten und Nachbarverbindungen werden hier angezeigt.');
      return;
    }
    var item = selection.item, node = selection.kind === 'node';
    if(selection.kind==='goal'){
      text('selection-title',item.name||selection.id);text('selection-description','Gespeichertes Goal · Active und dynamische Spielzustände sind nur im laufenden Spiel belegt.');
      var goalDetails=document.createElement('dl');goalDetails.className='detail-list';
      addDetail(goalDetails,'GoalType',item.type);addDetail(goalDetails,'GM-Position · unverändert',item.position);
      Object.entries(item.properties||{}).forEach(([key,value])=>addDetail(goalDetails,key,value));
      Object.entries(item.schema_defaults||{}).forEach(([key,value])=>addDetail(goalDetails,key+' · Schema default',value));
      if(Object.keys(item.schema_defaults||{}).length)addDetail(goalDetails,'Schema-Standardwerte · nicht in GM gespeichert',item.schema_default_source);
      addDetail(goalDetails,'GM-Datei',app.goalOverlay?.file||'—');
      addDetail(goalDetails,'GM-Datei SHA256',app.goalOverlay?.sha256||'—');$('selection-details').appendChild(goalDetails);return;
    }
    text('selection-title', node ? 'Wegpunkt ' + selection.id : 'Verbindung ' + item.from + (selection.kind === 'pair' ? ' ↔ ' : ' → ') + item.to);
    text('selection-description', traversalHelp[item.reason] || item.reason || item.rejection_reason || (accepted(item) ? 'Akzeptierter Eintrag im geladenen Prüfstand.' : 'Abgelehnt durch den Prüfer.'));
    var badge = document.createElement('span'); badge.className = 'result-tag' + (accepted(item) ? '' : ' rejected');
    badge.textContent = accepted(item) ? (item.native_checked === false || item.status === 'candidate' ? 'GEOMETRIEKANDIDAT' : 'AKZEPTIERT') : 'ABGELEHNT';
    if (badge.textContent === 'GEOMETRIEKANDIDAT') badge.classList.add('candidate');
    $('selection-details').appendChild(badge);
    var dl = document.createElement('dl'); dl.className = 'detail-list';
    if (node) {
      addDetail(dl,'Source-ID',selection.id);
      if(Number.isInteger(item.index)&&item.index>=0)addDetail(dl,'Nativer Wegpunktindex',item.index);
      addDetail(dl, 'Fußpunkt', position(item) && position(item).map(function (v) { return Number(v).toFixed(2); }).join(' / '));
      if (item.original_position) addDetail(dl, 'Spielerorigin', item.original_position);
      addDetail(dl, 'Radius', item.radius); addDetail(dl, 'Flags', item.flags);
      if (item.requested_radius != null) addDetail(dl, 'Gewünschter Radius', item.requested_radius);
      if (item.effective_radius != null) addDetail(dl, 'Nativer Radius', item.effective_radius);
      if (item.radius_limit_reason || item.limit_reason) addDetail(dl, 'Radiusbegrenzung', item.radius_limit_reason || item.limit_reason);
      if (item.source_radius != null && item.source_radius !== item.radius) addDetail(dl, 'Radius vor Entwurf', item.source_radius);
      if (item.locked) addDetail(dl, 'Bearbeitung', item.generated_guide ? 'Generatorguide · wird neu erzeugt' : 'Aktions-Bodenanker · bleibt geschützt erhalten');
      if (item.nav_flags != null) addDetail(dl, 'Omni-bot-Flags', item.nav_flags);
      if (traversalHelp[item.reason]) addDetail(dl, 'Aktionsprüfung', 'Native Geometrie / Bot-Live-Abnahme separat');
      if (item.way_guid != null) addDetail(dl, 'WAY-GUID', item.way_guid);
      if(item.name)addDetail(dl,'Name',item.name);
      if(item.facing)addDetail(dl,'Blickrichtung',item.facing);
      renderDoorSelection(dl,item);
      if (item.action_roles && item.action_roles.length) addDetail(dl, 'Aktionsrollen', item.action_roles);
      if (item.action_properties && Object.keys(item.action_properties).length) addDetail(dl, 'Aktionsrezept · WAY-Eigenschaften werden nativ erneuert', item.action_properties);
    } else { addDetail(dl, 'Von → Nach', item.from + ' → ' + item.to);var doorSource=nodesById(item.from);if(doorSource)renderDoorSelection(dl,doorSource); }
    if (selection.kind === 'pair') {
      addDetail(dl, 'Direktweg', Number(item.direct).toFixed(1) + ' Units');
      if (item.detour_before) addDetail(dl, 'Vorher hin/zurück', item.detour_before.map(function (x) { return Number(x).toFixed(1); }).join(' / ') + ' Units');
      if (item.detour_final) addDetail(dl, 'Jetzt hin/zurück', item.detour_final.map(function (x) { return Number(x).toFixed(1); }).join(' / ') + ' Units');
      if (item.native_stand) addDetail(dl, 'Stehprüfung', item.native_stand.join(' / '));
      if (item.native_duck) addDetail(dl, 'Duckprüfung', item.native_duck.join(' / '));
    }
    ['kind', 'topology_protected', 'crouch', 'stance', 'status', 'reason', 'native_checked', 'verified', 'spawn_reachable', 'component'].forEach(function (key) {
      if (item[key] != null) addDetail(dl, key, item[key]);
    });
    $('selection-details').appendChild(dl);
    if (node) {
      var edges = app.scene.navigation.edges || [], neighbors = [];
      edges.forEach(function (edge, index) {
        if (String(edge.from) === String(selection.id) || String(edge.to) === String(selection.id)) neighbors.push({edge: edge, index: index});
      });
      var heading = document.createElement('h3'); heading.textContent = neighbors.length + ' gerichtete Nachbarverbindungen'; $('selection-neighbors').appendChild(heading);
      neighbors.slice(0, 100).forEach(function (record) {
        var button = document.createElement('button'); button.className = 'neighbor-button';
        button.textContent = record.edge.from + ' → ' + record.edge.to + ' · ' + (accepted(record.edge) ? 'akzeptiert' : reasonOf(record.edge));
        button.onclick = function () { $('edge-mode').value = 'all'; app.viewer.setNavigationOptions(layerOptions()); app.viewer.selectNavigation('edge', record.index); };
        $('selection-neighbors').appendChild(button);
      });
    }
  }
  function renderProblems(scene) {
    var audit = scene.audit, list = $('problem-list'); list.replaceChildren();
    var issues = audit && audit.unresolved_nodes || [];
    text('problem-count', audit ? issues.length : '—');
    if($('stat-problems'))text('stat-problems',audit ? issues.length : '—');
    var stale = scene.audit_state === 'stale' || scene.status === 'native_stale';
    text('problem-summary', !audit ? (stale ? 'Änderungen noch nicht geprüft. Alte Fehler ausgeblendet; „Prüfen“ prüft den aktuellen Entwurf.' : 'Noch kein Prüfbericht: „Prüfen“ untersucht das aktuelle Netz.') :
      (stale ? 'Alter Prüfstand – nach Änderungen erneut prüfen. ' : '') +
      (issues.length ? issues.length + ' Punkte haben keinen vollständigen Hin- und Rückweg zum Hauptnetz.' :
      'Keine getrennten Punkte gemeldet. Das bestätigt noch keine vollständige Mapabdeckung.'));
    issues.forEach(function (issue) {
      var button = document.createElement('button'); button.className = 'reason-button';
      button.textContent = 'Punkt ' + issue.source_id + ' · ' +
        (!issue.outgoing ? 'Kein Ausgang' : !issue.incoming ? 'Kein Zugang' : 'Hin- oder Rückweg fehlt') + ' · Kamera hin';
      button.onclick = function () {
        if (!app.viewer) return;
        setMode(true); $('min-z').value = ''; $('max-z').value = '';
        document.querySelector('[data-layer="rejected"]').checked = true;
        $('edge-mode').value = 'all'; updateLayers();
        app.viewer.selectNavigation('node', issue.source_id);
        app.viewer.focusNavigation(issue.position);
        notice('Punkt ' + issue.source_id + ': Verbindung zum Hauptnetz prüfen. Im Mauseditor kannst du Punkte verschieben oder verbinden. Danach erneut nativ prüfen.');
      };
      list.appendChild(button);
    });
    renderGenerationDiagnostics(scene);
    renderDoorDiagnostics(scene);
  }
  function diagnosticPosition(item, data) {
    var p = position(item) || item.from_position;
    if (!p && item.from != null) p = position((data.reference_nodes || []).find(function (n) { return String(n.id) === String(item.from); }));
    return Array.isArray(p) && p.length === 3 && p.every(Number.isFinite) ? p : null;
  }
  function focusDiagnostic(item, data) {
    var p = diagnosticPosition(item, data); if (!app.viewer || !p) return;
    cancelGesture(); setMode(true); $('min-z').value = ''; $('max-z').value = ''; updateLayers();
    app.viewer.focusNavigation(p);
    notice((item.from != null ? 'Scannerverbindung ' + item.from + ' → ' + item.to : 'Scannerpunkt ' + item.id) + ': ' + reasonOf(item) +
      '. Diese Probe gehört zum Generierungsbericht; sie ist keine Verbindung im aktuellen Bearbeitungsnetz.');
  }
  function renderGenerationDiagnostics(scene) {
    var data = scene.generation_diagnostics, panel = $('generation-diagnostics'), pending = $('coverage-list'), rejected = $('scanner-reason-list');
    if (!panel) return;
    panel.hidden = !data; pending.replaceChildren(); rejected.replaceChildren(); if (!data) return;
    var counts = data.counts || {}, nodes = data.nodes || [], edges = data.edges || [];
    var isFloor = function (n) { return n.diagnostic_kind === 'missing_floor_area' || n.floor_accepted === true || n.native_input?.floor_accepted === true; };
    var floor = nodes.filter(function (n) { return isFloor(n) && n.diagnostic_resolved !== true; });
    var coverageCount = counts.connectivity_pending_nodes == null ? floor.length : Number(counts.connectivity_pending_nodes);
    text('coverage-count', coverageCount.toLocaleString('de-DE'));
    text('coverage-status', data.current === true ? 'Bodenproben ohne bestätigte Route. Darunter können Dächer, Mapränder und nicht spielbare Flächen sein; vollständige Mapabdeckung bleibt ungeprüft.' :
      'Historischer Generierungsbericht. Er belegt nicht den Anschlusszustand des aktuellen Entwurfs.');
    var regions = new Map();
    floor.forEach(function (node) {
      var id = node.native_input?.missing_region;
      var key = id == null ? 'unassigned' : String(id); if (!regions.has(key)) regions.set(key, []); regions.get(key).push(node);
    });
    text('coverage-region-count', regions.size);
    Array.from(regions.entries()).sort(function (a,b) { return b[1].length-a[1].length; }).forEach(function (entry) {
      var id=entry[0],group=entry[1],card=document.createElement('div');card.className='coverage-region';
      var record=data.native_input_coverage?.missing_regions?.find(function (r) { return String(r.id)===id; });
      var representative=group.find(function (n) { return n.id===record?.nearest_missing_node; }) || group[0];
      var button=document.createElement('button');button.className='reason-button';
      button.textContent=(id==='unassigned'?'Ohne Gruppierung':'Bereich '+id)+' · '+group.length+' Bodenproben · Kamera hin';
      button.disabled=!diagnosticPosition(representative,data);button.onclick=function () { focusDiagnostic(representative,data); };card.appendChild(button);
      var details=document.createElement('details'),summary=document.createElement('summary');summary.textContent='Einzelne Bodenproben anzeigen';details.appendChild(summary);
      details.addEventListener('toggle',function () {
        if(!details.open||details.children.length>1)return;
        var list=document.createElement('div');list.className='reason-list';
        group.forEach(function (node) {var item=document.createElement('button');item.className='reason-button';item.textContent='Bodenprobe '+node.id+' · Kamera hin';
          item.disabled=!diagnosticPosition(node,data);item.onclick=function () { focusDiagnostic(node,data); };list.appendChild(item);});details.appendChild(list);
      });card.appendChild(details);pending.appendChild(card);
    });
    if (!floor.length) { var empty = document.createElement('p'); empty.className = 'hint'; empty.textContent = coverageCount ?
      'Genauer Standort fehlt im Generierungsbericht; Mapabdeckung bleibt offen.' : 'Keine unverbundenen Bodenproben in diesem Generierungsbericht.'; pending.appendChild(empty); }
    var groups = {};
    nodes.filter(function (n) { return !isFloor(n); }).concat(edges).forEach(function (item) {
      var key = reasonOf(item); (groups[key] || (groups[key] = [])).push(item);
    });
    text('scanner-count', (Number(counts.geometry_rejected_nodes || 0) + Number(counts.rejected_directed_edges || 0)).toLocaleString('de-DE'));
    Object.keys(groups).sort(function (a, b) { return groups[b].length - groups[a].length; }).forEach(function (reason) {
      var group = groups[reason], button = document.createElement('button'), cycle = 0;
      button.className = 'reason-button'; button.textContent = reason + ' · ' + group.length + ' Proben · Kamera hin';
      var located = group.filter(function (item) { return diagnosticPosition(item, data); }); button.disabled = !located.length;
      button.onclick = function () { focusDiagnostic(located[cycle++ % located.length], data); }; rejected.appendChild(button);
    });
    if (!Object.keys(groups).length) { var p = document.createElement('p'); p.className = 'hint'; p.textContent = 'Keine verworfenen Geometrieproben in diesem Generierungsbericht.'; rejected.appendChild(p); }
    var hashes = $('scanner-source'); hashes.textContent = data.native_report_sha256 ? 'Generierungsbericht SHA256: ' + data.native_report_sha256 : '';
  }
  function doorDiagnosis(reason){
    if(reason==='closed_identity_open_pose_native_walk_live_pending'||reason==='door_open_pose_native_walk_live_pending')return 'Durchgang geometrisch geprüft; Botbedienung offen.';
    if(/locked|key_required/.test(reason))return 'Tür verriegelt oder Schlüssel erforderlich.';
    if(/timing|autoclose|close_time|open_time/.test(reason))return 'Öffnungszeit und Rückweg noch nicht belegt.';
    if(/sweep|swept|rotation|wait_body/.test(reason))return 'Türschwenkbereich noch nicht sicher belegt.';
    if(/button_target|button_has_no_target|relay_|scripted_button|unlinked.*(button|switch)|(button|switch).*unlinked/.test(reason))return 'Schalterzuordnung fehlt oder ist nicht eindeutig.';
    if(/reachable.*closed/.test(reason))return 'Schalter bei geschlossener Tür nicht erreichbar.';
    if(/occluded|out_of_range/.test(reason))return 'Aktivator nicht sicher anvisierbar.';
    if(/identity|currently_moving|unknown_door/.test(reason))return 'Türmodell oder Ausgangszustand nicht bestätigt.';
    if(/team_/.test(reason))return 'Teamfreigabe nicht bestätigt.';
    return 'Kein vollständig bestätigter Türdurchgang.';
  }
  function renderDoorDiagnostics(scene){
    var panel=$('door-diagnostics'),list=$('door-diagnostic-list'),data=scene.native_summary?.door_generation;
    var current=scene.report_status?.current===true&&scene.audit_state==='current';
    var stale=scene.audit_state==='stale'||scene.status==='native_stale'||scene.status==='manual_draft';
    panel.hidden=!data&&!stale;list.replaceChildren();
    if(!current){text('door-diagnostic-status',stale?'Türprüfung veraltet – aktuelle Navigation prüfen.':'Noch keine aktuelle native Türprüfung.');return;}
    if(!data)return;
    text('door-diagnostic-status','Aktueller nativer Prüfstand. Botbedienung separat testen.');
    if(/budget/.test(data.stop_reason||'')){var limit=document.createElement('p');limit.className='hint';limit.textContent='Türsuche durch Prüfbudget begrenzt; weitere Durchgänge können fehlen.';list.append(limit);}
    var grouped=new Map();(data.cases||[]).forEach(c=>{
      var key=[c.entity,c.use_entity,c.reason].join(':');if(!grouped.has(key))grouped.set(key,{item:c,count:0});grouped.get(key).count++;
    });
    grouped.forEach(({item:c,count})=>{
      var card=document.createElement('div');card.className='door-diagnostic-item';
      var content=document.createElement('div'),title=document.createElement('strong'),label=document.createElement('span');label.textContent='Tür';title.append(label,document.createTextNode(' '+c.entity));
      var description=document.createElement('p');description.className='hint';description.textContent=doorDiagnosis(c.reason||'');content.append(title,description);card.append(content);
      var nodes=scene.navigation?.nodes||[],node=nodes.find(n=>String(n.id)===String(c.from))||nodes.find(n=>doorFields(n)?.entity===c.entity);
      var target=c.position||c.center||c.origin||(node&&position(node)),camera=document.createElement('button');camera.className='button secondary';camera.textContent='Kamera hin';
      camera.disabled=!Array.isArray(target)||target.length!==3||!target.every(Number.isFinite);
      camera.title=camera.disabled?'Keine genaue Position im Prüfbericht.':'Türbereich ansehen';
      camera.onclick=()=>{setMode(true);$('min-z').value='';$('max-z').value='';updateLayers();if(node)app.viewer.selectNavigation('node',node.id);app.viewer.focusNavigation(target);};card.append(camera);
      var details=document.createElement('details'),summary=document.createElement('summary');summary.textContent='Prüfdetails';var code=document.createElement('code');code.textContent=JSON.stringify({entity:c.entity,use_entity:c.use_entity,from:c.from,to:c.to,team:c.team,reason:c.reason,directions:count});details.append(summary,code);card.append(details);list.append(card);
    });
    if(!grouped.size){var empty=document.createElement('p');empty.className='hint';empty.textContent='Keine Türfälle im aktuellen Prüfbericht.';list.append(empty);}
  }
  function renderSceneInfo(scene) {
    var nav = scene.navigation || {nodes: [], edges: []}, meshes = scene.meshes || {}, nodes = nav.nodes || [], edges = nav.edges || [];
    var rejected = nodes.filter(function (n) { return !accepted(n); }).length + edges.filter(function (e) { return !accepted(e); }).length;
    text('stat-nodes', nodes.filter(accepted).length.toLocaleString('de-DE'));
    text('stat-edges', edges.filter(accepted).length.toLocaleString('de-DE'));
    text('stat-rejected', rejected.toLocaleString('de-DE'));
    text('stat-mesh', Object.values(meshes).reduce(function (n, m) { return n + ((m || {}).polygons || []).length; }, 0).toLocaleString('de-DE'));
    var status = scene.status;
    var label = typeof status === 'string' ? status : status && (status.label || status.state || status.status) || 'Kandidat / ohne Prüfstand';
    label = statusNames[label] || label;
    text('scene-status', label); text('stat-proof', label);
    $('stat-proof').dataset.state=typeof status==='string'?status:'';
    var reasons = {};
    nodes.forEach(function (n, i) { if (!accepted(n)) { var r = reasonOf(n); (reasons[r] || (reasons[r] = [])).push({kind: 'node', id: n.id != null ? n.id : i}); } });
    edges.forEach(function (e, i) { if (!accepted(e)) { var r = reasonOf(e); (reasons[r] || (reasons[r] = [])).push({kind: 'edge', id: i}); } });
    $('reason-list').replaceChildren(); text('reason-count', Object.keys(reasons).length);
    Object.keys(reasons).sort(function (a, b) { return reasons[b].length - reasons[a].length; }).forEach(function (reason) {
      var btn = document.createElement('button'); btn.className = 'reason-button';
      var label = document.createElement('span'), count = document.createElement('span');
      label.textContent = reason; count.className = 'count'; count.textContent = reasons[reason].length; btn.append(label, count);
      var cycle = 0;
      btn.onclick = function () {
        document.querySelector('[data-layer="rejected"]').checked = true;
        $('edge-mode').value = 'all'; app.viewer.setNavigationOptions(layerOptions());
        var item = reasons[reason][cycle++ % reasons[reason].length];
        var selected = app.viewer.selectNavigation(item.kind, item.id);
        if (selected) app.viewer.focusNavigation(item.kind === 'node' ? item.id : selected.item.from);
      };
      $('reason-list').appendChild(btn);
    });
    if (!Object.keys(reasons).length) { var p = document.createElement('p'); p.className = 'hint'; p.textContent = 'Keine abgelehnten Einträge im geladenen Datensatz. Das allein belegt keine vollständige Mapabdeckung.'; $('reason-list').appendChild(p); }
    renderProblems(scene);
    renderShortcuts(scene);
    $('source-info').replaceChildren();
    Object.entries(scene.source_hashes || {}).forEach(function (entry) { var el = document.createElement('div'); el.textContent = entry[0] + ': ' + String(entry[1]); $('source-info').appendChild(el); });
    var geom = scene.geometry || {};
    if (geom.warnings) { var el = document.createElement('div'); el.textContent = 'Geometriehinweise: ' + JSON.stringify(geom.warnings); $('source-info').appendChild(el); }
    if (geom.limitations) { var el = document.createElement('div'); el.textContent = 'Geometriegrenzen: ' + JSON.stringify(geom.limitations); $('source-info').appendChild(el); }
    var limited = nodes.filter(function (n) { return n.requested_radius != null && Number(n.effective_radius == null ? n.radius : n.effective_radius) < Number(n.requested_radius); });
    if (limited.length) { var note = document.createElement('div'); note.textContent = limited.length + ' Radien nativ begrenzt. Punkt auswählen: gewünschter Radius, wirksamer Radius und Grund stehen im Inspektor.'; $('source-info').appendChild(note); }
  }
  function revealNode(node) {
    document.querySelector('[data-layer="nodes"]').checked = true;
    document.querySelector('[data-layer="' + (accepted(node) ? 'accepted' : 'rejected') + '"]').checked = true;
    $('min-z').value = ''; $('max-z').value = '';
    $('edge-mode').value = 'selected';
    app.viewer.setNavigationOptions(layerOptions()); setMode(true);
  }
  function renderShortcuts(scene) {
    var shortcuts = (scene.native_summary || {}).local_shortcuts;
    $('shortcut-list').replaceChildren();
    var cases = shortcuts && shortcuts.cases || [], pairs = cases.filter(function (item) { return item.accepted === true; });
    text('shortcut-count', pairs.length);
    text('shortcut-summary', shortcuts ? pairs.length + ' Paare in beiden Richtungen bestätigt · ' + (shortcuts.checked || 0) + ' geprüft · ' + (shortcuts.rejected || 0) + ' abgelehnt. Klick fokussiert die Verbindung.' : 'Noch keine lokalen Verbindungsprüfungen geladen.');
    pairs.forEach(function (pair) {
      var btn = document.createElement('button'); btn.className = 'shortcut-button';
      var title = document.createElement('strong'), detail = document.createElement('span');
      title.textContent = pair.from + ' ↔ ' + pair.to + (pair.crouch ? ' · Ducken' : '');
      var before = pair.detour_before && Math.max.apply(null, pair.detour_before);
      detail.textContent = (Number.isFinite(before) ? before.toFixed(0) + ' → ' : '') + Number(pair.direct).toFixed(0) + ' Units';
      btn.append(title, detail);
      btn.onclick = function () {
        revealNode(pair);
        app.viewer.selectNavigationPair(pair);
        var a = nodesById(pair.from), b = nodesById(pair.to);
        if (a && b) { var pa = position(a), pb = position(b); app.viewer.focusNavigation(pa.map(function (v, i) { return (v + pb[i]) * 0.5; })); }
        else app.viewer.focusNavigation(pair.from);
      };
      $('shortcut-list').appendChild(btn);
    });
  }
  function nodesById(id) {
    return app.scene && (app.scene.navigation.nodes || []).find(function (node, index) { return String(node.id != null ? node.id : index) === String(id); });
  }
  function editorSelection(selection) {
    var node = selection && selection.kind === 'node' && selection.item;
    app.editNode = node && node.id != null ? node.id : null;
    var editable = node && app.draft && $('edit-show').checked && !node.locked;
    $('edit-update').disabled = !editable; $('edit-delete').disabled = !editable;
    $('edit-properties').disabled = !editable;
    $('edit-name').disabled=!editable;$('edit-facing').disabled=!editable||node?.generated_guide||!!node?.action_roles?.length;
    var jumpSelect=$('edit-standing-jump'),jumpTargets=app.draft?.edges.filter(function(edge){return node&&String(edge.from)===String(node.id);})||[];
    jumpSelect.replaceChildren();var noJump=document.createElement('option');noJump.value='';noJump.textContent='Kein Stand-Sprung';jumpSelect.appendChild(noJump);
    jumpTargets.forEach(function(edge){var option=document.createElement('option');option.value=String(edge.to);option.textContent='Punkt '+edge.to;jumpSelect.appendChild(option);});
    var jumpRole=node?.action_roles?.find(function(role){return role.kind==='standing_jump'&&role.role==='launch';});
    jumpSelect.value=jumpRole?String(jumpRole.landing_node):'';jumpSelect.disabled=!editable;
    var climbSelect=$('edit-free-climb'),climbRole=node?.action_roles?.find(function(role){return role.kind==='free_climb'&&role.role==='launch';});
    climbSelect.replaceChildren();var noClimb=document.createElement('option');noClimb.value='';noClimb.textContent='Kein Freiklettern';climbSelect.appendChild(noClimb);
    jumpTargets.forEach(function(edge){var option=document.createElement('option');option.value=String(edge.to);option.textContent='Punkt '+edge.to;climbSelect.appendChild(option);});
    climbSelect.value=climbRole?String(climbRole.landing_node):'';climbSelect.disabled=!editable||app.state?.game_mode==='et';
    text('edit-selection', node ? 'Punkt ' + node.id + (node.locked ? ' · Generatoraktion, gesperrt' : ' · zum Verschieben mit linker Maustaste ziehen') : 'Punkt oder Verbindung im Bild auswählen.');
    if (node) {
      // A radius/flag-only edit must not round otherwise untouched coordinates.
      position(node).forEach(function (v, i) { $('edit-' + ['x', 'y', 'z'][i]).value = String(Number(v)); });
      $('edit-radius').min=node.generated_guide?'1':'10';
      $('edit-radius').max=actionRadiusNode(node)?'100':'300';
      $('edit-radius').value = node.radius || 35; window.navstudioFlags.set(node.nav_flags || 0);
      $('edit-name').value=node.name||'';$('edit-facing').value=(node.facing||[0,0,0]).join(' ');
    } else if (selection && selection.kind === 'edge') {
      $('edit-from').value = selection.item.from; $('edit-to').value = selection.item.to;
    }
    mouseStatus();
  }
  function editorScene() {
    if (!app.sourceScene || !app.viewer) return;
    app.scene = Object.assign({}, app.sourceScene);
    var checkedRevision = checkedDraftReport();
    if (app.draft && !checkedRevision) {
      app.scene.status = $('edit-show').checked ? 'manual_draft' : 'native_stale';
      app.scene.audit=null;app.scene.audit_state='stale';
      app.scene.report_status=Object.assign({},app.sourceScene.report_status,{current:false});
      app.scene.proof_status=Object.assign({},app.sourceScene.proof_status,{valid:false});
    }
    if (app.draft && $('edit-show').checked) {
      app.scene.status = checkedRevision ? app.sourceScene.status : 'manual_draft';
      // Merely opening the editor must keep every result of this exact revision,
      // including rejected authored links and native generated action guides.
      app.scene.navigation = checkedRevision ? Object.assign({},app.sourceScene.navigation, {
        nodes: app.sourceScene.navigation.nodes.map(function (n) {return Object.assign({},n,{position:position(n)?.slice()});}),
        edges: app.sourceScene.navigation.edges.map(function (e) {return Object.assign({},e);})
      }) : {nodes: app.draft.nodes.map(function (n) {
        return Object.assign({}, n, {accepted: null, state: 'candidate', status: 'candidate', native_checked: false,
          reason: n.locked ? (n.source_reason || 'Generatoraktion · im Entwurf gesperrt, wird neu geprüft') : 'Manueller Entwurf · erneute native Prüfung erforderlich'});
      }), edges: app.draft.edges.map(function (e) { return Object.assign({}, e, {accepted: null, state: 'candidate', status: 'candidate', native_checked: false}); })};
      app.scene.native_summary = checkedRevision ? app.sourceScene.native_summary : {};
      app.scene.source_hashes = Object.assign({}, app.sourceScene.source_hashes, {draft_revision: app.draft.revision, draft_sha256: app.draft.sha256});
    }
    app.viewer.setNavigation(navigationData(app.scene));
    app.viewer.setNavigationOptions(layerOptions()); renderSceneInfo(app.scene);
    selectNavigation(app.viewer._navigationSelection); editorSelection(app.viewer._navigationSelection);
  }
  function checkedDraftReport() {
    var d = app.draft, checked = app.sourceScene?.draft;
    return !!(d && !d.stale && checked && app.sourceScene.report_status?.current === true &&
      typeof d.sha256 === 'string' && d.sha256.length && checked.revision === d.revision && checked.sha256 === d.sha256);
  }
  function editorStatus() {
    var d = app.draft; $('edit-controls').hidden = !d;
    text('edit-status', d ? 'Revision ' + d.revision + (d.saved_revision === d.revision ? ' · gespeichert' : ' · noch nicht gespeichert') +
      (d.stale ? ' · Grundlage geändert! Nur sichern/archivieren.' : '') +
      (checkedDraftReport() ? ' · nativ geprüft' : ' · Prüfung offen') +
      (d.radius_migrations ? ' · ' + d.radius_migrations + ' alte Radien <10 auf35 migriert' : '') : 'Lokalen Entwurf anlegen; vorhandene WAY bleibt erhalten.');
    $('edit-undo').disabled = !d || !d.undo_count; $('edit-redo').disabled = !d || !d.redo_count;
    $('edit-export').disabled = !d || d.saved_revision !== d.revision || d.stale;
    mouseStatus();
  }
  async function editOperation(op, values) {
    if (app.editing || app.loading) return false;
    app.editing = true; mouseStatus();
    try {
      if (!app.state) throw new Error('Zuerst Verbindung zum lokalen Studio abwarten.');
      var map = app.map;
      var result = await request('/api/edit', {method: 'POST', headers: {'Content-Type': 'application/json', 'X-Navstudio-Token': app.state.token},
        body: JSON.stringify(Object.assign({op: op, map: map, revision: app.draft && app.draft.revision}, values || {}))});
      if (map !== app.map) return false;
      app.draft = result.draft; $('edit-show').checked = !!app.draft;
      editorStatus(); editorScene(); notice(result.note);
      if (op === 'save') notice('Änderungen im Projekt gesichert. „Ins Spiel übernehmen“ prüft und lädt diesen Stand.');
      if (op === 'restore') notice('Archivierter Entwurf wieder geöffnet. Originalarchiv bleibt erhalten.');
      if (op === 'export') notice('Prüfkandidat exportiert. Jetzt links „Nativ prüfen“; erst danach installieren.');
      if (op === 'archive') await loadScene(map, false);
      return result;
    } catch (error) { editorScene(); notice(error.message); return false; }
    finally { app.editing = false; mouseStatus(); }
  }
  function editValues() {
    if (['x', 'y', 'z'].some(function (axis) { return $('edit-' + axis).value === ''; })) throw new Error('Alle drei Fußkoordinaten eingeben.');
    return {position: ['x', 'y', 'z'].map(function (axis) { return Number($('edit-' + axis).value); }),
      radius: Number($('edit-radius').value), nav_flags: window.navstudioFlags.get()};
  }
  function newPointRadius(){
    var radius=Number($('edit-radius').value),selected=app.draft?.nodes.find(n=>n.id===app.editNode);
    return selected?.generated_guide&&radius>0&&radius<10?35:radius;
  }
  function mouseReady() {
    return !!(app.viewer && app.draft && $('edit-show').checked && !app.draft.stale && !app.editing && !app.loading && !app.actionPending &&
      !(app.state && (app.state.jobs || []).some(function (j) { return busyStates.includes(j.status); })));
  }
  function unlockedNode(id) {
    var n = app.draft && app.draft.nodes.find(function (node) { return String(node.id) === String(id); });
    return n && !n.locked ? n : null;
  }
  function editableSelection() {
    var s = app.viewer && app.viewer._navigationSelection;
    return s && (s.kind === 'node' ? !!unlockedNode(s.id) : s.kind === 'edge' && !!unlockedNode(s.item.from) && !!unlockedNode(s.item.to));
  }
  function mouseStatus() {
    var ready = mouseReady(), d = app.draft;
    document.querySelectorAll('[data-edit-tool]').forEach(function (button) {
      button.disabled = !ready;
      var chosen = button.dataset.editTool === app.editTool;
      button.classList.toggle('active', chosen); button.setAttribute('aria-pressed', chosen);
    });
    $('mouse-delete').disabled = !ready || !editableSelection();
    $('mouse-undo').disabled = !ready || !d.undo_count;
    $('mouse-redo').disabled = !ready || !d.redo_count;
    $('mouse-both').disabled = !ready;
    $('mouse-begin').hidden = !!d;
    $('mouse-begin').disabled = !app.scene || app.loading || app.editing;
    $('map-select').disabled = app.loading || app.editing;
    $('reload-scene').disabled = app.loading || app.editing;
    var help = !d ? 'Bearbeiten öffnet einen lokalen Entwurf. Rechts ziehen: Kamera drehen · Mausrad: vor/zurück.' :
      d.stale ? 'Grundlage geändert: Entwurf sichern/archivieren und neu öffnen.' : !ready ? 'Bearbeitung pausiert. Entwurf einblenden und laufenden Auftrag abwarten.' :
      app.linkStart != null ? 'Start: Punkt ' + app.linkStart + '. Jetzt Zielpunkt anklicken · Esc bricht ab.' :
      {select: 'Punkt mit linker Maustaste ziehen · Entf löscht die Auswahl · Strg+Z macht rückgängig.',
        add: 'Boden anklicken: Wegpunkt hinzufügen. Radius/Flags rechts einstellen; danach Punkte verbinden.',
        link: 'Zwei Wegpunkte nacheinander anklicken. „Beide Richtungen“ erzeugt Hin- und Rückweg.',
        unlink: 'Verbindung anklicken oder zwei Wegpunkte wählen. „Beide Richtungen“ entfernt Hin- und Rückweg.'}[app.editTool];
    text('mouse-help', help);
    $('viewer').dataset.tool = ready ? app.editTool : 'inspect';
  }
  function cancelGesture() {
    var dragging = app.drag; app.drag = null; app.cameraDrag = null; app.linkStart = null;
    if (app.spawnDrag) { app.spawnDrag = null; window.navstudioProjects?.refreshSpawns(); }
    if (app.viewer) { app.viewer._radiantNavigating = false; app.viewer._keys = {}; }
    if (app.viewer) {
      app.viewer.onNavigationPlace = null;
      if (app.viewer._navigationEditPreview) app.viewer.setNavigationEditPreview(null);
      if (dragging && dragging.moved) editorScene();
    }
    mouseStatus();
  }
  function chooseTool(tool) {
    cancelGesture(); app.editTool = tool; setMode(true); mouseStatus();
    $('viewer').focus({preventScroll: true});
  }
  async function removeSelection() {
    if (!mouseReady()) return;
    var s = app.viewer._navigationSelection;
    if (!editableSelection()) { notice('Generatoraktionen sind geschützt. Einen gewöhnlichen Punkt oder dessen Verbindung wählen.'); return; }
    cancelGesture();
    if (s.kind === 'node') await editOperation('delete_node', {id: s.id});
    else await editOperation('unlink', {from: s.item.from, to: s.item.to, bidirectional: $('mouse-both').checked});
  }
  async function mouseClick(event) {
    if (app.suppressClick) { app.suppressClick = false; return true; }
    if (!app.viewer || app.viewer._navigationInspect === false || app.viewer.onNavigationPlace) return false;
    if (!mouseReady()) return false;
    if (app.editTool === 'select') return false;
    if (app.editTool === 'add') {
      var p = app.viewer.navigationFloorPoint(event.clientX, event.clientY);
      if (!p) { notice('Kein Boden getroffen. Die Kamera so drehen, dass die gewünschte Bodenfläche sichtbar ist.'); return true; }
      var oldIds = new Set(app.draft.nodes.map(function (n) { return String(n.id); })), map = app.map;
      var result = await editOperation('add_node', {position: p, radius: newPointRadius(), nav_flags: window.navstudioFlags.get()});
      if (result && map === app.map) {
        var added = app.draft.nodes.find(function (n) { return !oldIds.has(String(n.id)); });
        if (added) app.viewer.selectNavigation('node', added.id);
      }
      return true;
    }
    var selected = app.viewer.pickNavigation(event.clientX, event.clientY, {nodesOnly: app.editTool === 'link' || app.linkStart != null});
    if (selected && selected.kind === 'edge' && app.editTool === 'unlink') { await removeSelection(); return true; }
    if (!selected || selected.kind !== 'node') { notice('Einen Wegpunkt anklicken. Esc bricht die Verbindungsauswahl ab.'); return true; }
    if (!unlockedNode(selected.id)) { notice('Dieser Leiter-/Sprunganker ist geschützt. Einen gewöhnlichen Wegpunkt wählen.'); return true; }
    if (app.linkStart == null) { app.linkStart = selected.id; mouseStatus(); return true; }
    if (String(app.linkStart) === String(selected.id)) { notice('Einen anderen Zielpunkt anklicken.'); return true; }
    var from = app.linkStart; app.linkStart = null; app.viewer.setNavigationEditPreview(null);
    await editOperation(app.editTool, {from: from, to: selected.id, bidirectional: $('mouse-both').checked});
    mouseStatus(); return true;
  }
  function pointerDown(event) {
    if (!app.viewer || app.viewer._navigationInspect === false || app.loading || app.drag || app.cameraDrag) return;
    if (event.button === 1 || event.button === 2) {
      cancelGesture(); app.cameraDrag = {x: event.clientX, y: event.clientY, pan: event.button === 1, pointer: event.pointerId};
      app.viewer._radiantNavigating = event.button === 2;
    } else if (event.button === 0 && app.editTool === 'select' && window.navstudioProjects?.canEditSpawns() && !app.viewer.onNavigationPlace &&
      app.viewer.pickNavigation(event.clientX, event.clientY, {select:false,spawnsOnly:true})?.item.editable) {
      var spawn = app.viewer.pickNavigation(event.clientX, event.clientY, {spawnsOnly:true});
      var screen = app.viewer.projectNavigationPoint(spawn.item.position, 0);
      if (!screen) return;
      app.spawnDrag = {id:spawn.id,pointer:event.pointerId,x:event.clientX,y:event.clientY,
        offset:[screen[0]-event.clientX,screen[1]-event.clientY],start:spawn.item.position.slice(),moved:false,point:null,
        originOffset:Number(spawn.item.origin_above_feet || window.navstudioProjects?.model.spawns?.origin_above_feet || app.scene?.native_summary?.bbox?.origin_above_feet)||24};
    } else if (event.button === 0 && app.editTool === 'select' && mouseReady() && !app.viewer.onNavigationPlace) {
      var s = app.viewer.pickNavigation(event.clientX, event.clientY);
      if (!s || s.kind !== 'node' || !unlockedNode(s.id)) return;
      var screen = app.viewer.projectNavigationPoint(position(s.item), 0);
      if (!screen) return;
      app.drag = {id: s.id, pointer: event.pointerId, x: event.clientX, y: event.clientY,
        offset: [screen[0]-event.clientX, screen[1]-event.clientY], start: position(s.item).slice(), moved: false,
        map: app.map, revision: app.draft.revision, point: null};
    } else return;
    event.preventDefault(); $('viewer').focus({preventScroll: true}); $('viewer').setPointerCapture(event.pointerId);
  }
  function pointerMove(event) {
    if (app.cameraDrag) {
      var c = app.cameraDrag; if (c.pointer !== event.pointerId) return;
      app.viewer.navigationCameraGesture(event.clientX-c.x, event.clientY-c.y, c.pan); c.x = event.clientX; c.y = event.clientY; return;
    }
    if (app.spawnDrag) {
      var sd=app.spawnDrag;if(sd.pointer!==event.pointerId)return;
      if(!sd.moved&&Math.hypot(event.clientX-sd.x,event.clientY-sd.y)<4)return;
      sd.moved=true;
      var floor=app.viewer.navigationFloorPoint(event.clientX+sd.offset[0],event.clientY+sd.offset[1]);
      if(floor){floor[2]+=sd.originOffset;sd.point=floor;app.viewer.previewNavigationSpawn(sd.id,floor);}
      else sd.point=null;
      return;
    }
    var d = app.drag;
    if (d) {
      if (d.pointer !== event.pointerId) return;
      if (!d.moved && Math.hypot(event.clientX-d.x, event.clientY-d.y) < 4) return;
      d.moved = true;
      d.point = app.viewer.navigationFloorPoint(event.clientX+d.offset[0], event.clientY+d.offset[1]);
      if (d.point) {
        app.viewer.previewNavigationNode(d.id, d.point);
        text('mouse-help', 'Punkt ' + d.id + ' verschieben · Loslassen übernimmt · Esc bricht ab.');
      } else text('mouse-help', 'Kein Boden unter dem Fußpunkt. Hier loslassen bricht die Verschiebung ab.');
    } else if (app.linkStart != null && mouseReady()) {
      var hit = app.viewer.pickNavigation(event.clientX, event.clientY, {select: false, nodesOnly: true});
      var p = hit && unlockedNode(hit.id) ? position(hit.item) : app.viewer.navigationFloorPoint(event.clientX, event.clientY);
      app.viewer.setNavigationEditPreview(p ? {from: app.linkStart, point: p} : null);
    }
  }
  async function pointerUp(event) {
    if (app.cameraDrag && app.cameraDrag.pointer === event.pointerId) {
      app.cameraDrag = null; app.viewer._radiantNavigating = false; app.viewer._keys = {}; return;
    }
    if(app.spawnDrag&&app.spawnDrag.pointer===event.pointerId){
      var sd=app.spawnDrag;app.spawnDrag=null;app.suppressClick=true;
      if($('viewer').hasPointerCapture(event.pointerId))$('viewer').releasePointerCapture(event.pointerId);
      if(sd.moved&&sd.point)window.dispatchEvent(new CustomEvent('navstudio:spawn-move',{detail:{id:sd.id,position:sd.point}}));
      else window.navstudioProjects?.refreshSpawns();
      return;
    }
    var d = app.drag;
    if (!d || d.pointer !== event.pointerId) return;
    app.drag = null; app.suppressClick = true;
    if ($('viewer').hasPointerCapture(event.pointerId)) $('viewer').releasePointerCapture(event.pointerId);
    if (!d.moved) return;
    if (!mouseReady() || d.map !== app.map || d.revision !== app.draft.revision || !d.point) {
      editorScene(); mouseStatus(); notice('Verschiebung abgebrochen: kein gültiger Boden oder Entwurf inzwischen geändert.'); return;
    }
    if (Math.hypot.apply(null, d.point.map(function (v, i) { return v-d.start[i]; })) < 0.01) { editorScene(); return; }
    await editOperation('update_node', {id: d.id, position: d.point});
  }
  function editorKey(event) {
    if (event.code === 'Escape') { cancelGesture(); return; }
    if (app.viewer && app.viewer._radiantNavigating) return;
    if (app.viewer?._navigationSelection?.kind === 'spawn') return;
    if (event.target && (event.target.isContentEditable || /^(INPUT|TEXTAREA|SELECT|BUTTON)$/.test(event.target.tagName)) ||
        $('settings-dialog').open || $('install-dialog').open || !mouseReady() || app.viewer._navigationInspect === false) return;
    var command = event.ctrlKey || event.metaKey, key = event.key.toLowerCase();
    if (command && (key === 'z' || key === 'y')) {
      event.preventDefault(); cancelGesture();
      var op = key === 'y' || event.shiftKey ? 'redo' : 'undo';
      if (app.draft[op + '_count']) editOperation(op); return;
    }
    if (command || event.altKey || event.repeat) return;
    if (event.key === 'Delete') { event.preventDefault(); removeSelection(); }
    else if ({v:'select', a:'add', l:'link', u:'unlink'}[key]) { event.preventDefault(); chooseTool({v:'select', a:'add', l:'link', u:'unlink'}[key]); }
  }
  function editorButton(id, operation) { $(id).onclick = function () { try { operation(); } catch (e) { notice(e.message); } }; }
  editorButton('edit-begin', function () { editOperation('begin'); });
  editorButton('mouse-begin', async function () { if (await editOperation('begin')) chooseTool('select'); });
  document.querySelectorAll('[data-edit-tool]').forEach(function (button) { button.onclick = function () { if (mouseReady()) chooseTool(button.dataset.editTool); }; });
  editorButton('mouse-delete', removeSelection);
  ['undo', 'redo'].forEach(function (op) { editorButton('mouse-' + op, function () { cancelGesture(); editOperation(op); }); });
  $('mouse-both').onchange = function () { $('edit-both').checked = this.checked; };
  $('edit-both').onchange = function () { $('mouse-both').checked = this.checked; };
  $('edit-free-climb').onchange=function(){
    if(this.value!==''){
      $('edit-standing-jump').value='';
      window.navstudioFlags.set((BigInt(window.navstudioFlags.get())&~((1n<<6n)|(1n<<20n)|(1n<<27n)|(1n<<18n)|(1n<<8n)))|(1n<<9n));
    }
  };
  $('edit-standing-jump').onchange=function(){
    if(this.value!==''){
      $('edit-free-climb').value='';
      window.navstudioFlags.set((BigInt(window.navstudioFlags.get())&~((1n<<6n)|(1n<<20n)|(1n<<9n)|(1n<<18n)|(1n<<8n)))|(1n<<27n));
    }
  };
  editorButton('edit-properties', function () {
    var payload={id:app.editNode,radius:Number($('edit-radius').value),nav_flags:window.navstudioFlags.get()},
      node=app.draft?.nodes.find(function(n){return String(n.id)===String(app.editNode);});
    var facing=$('edit-facing').value.trim().split(/\s+/).map(Number),name=$('edit-name').value;
    if(facing.length!==3||!facing.every(Number.isFinite))throw new Error('Blickrichtung benötigt drei endliche Zahlen.');
    if(name!==(node?.name||''))payload.name=name;
    if(facing.some((v,i)=>v!==(node?.facing||[0,0,0])[i]))payload.facing=facing;
    [['edit-standing-jump','standing_jump','standing_jump_target'],['edit-free-climb','free_climb','free_climb_target']].forEach(function(action){
      var target=$(action[0]).value,existing=node?.action_roles?.some(function(r){return r.kind===action[1]&&r.role==='launch';});
      // Empty selectors must not remove ordinary JUMP/CLIMB flags, including actual ladders.
      if(target!==''||existing)payload[action[2]]=target===''?null:Number(target);
    });
    editOperation('update_node',payload);
  });
  editorButton('edit-restore', function () { editOperation('restore'); });
  editorButton('edit-archive', function () { if (window.confirm('Entwurf schließen? Er wird im Archiv gesichert. Danach siehst du wieder das bisherige Netz. Zum Weiterbearbeiten stattdessen Entwurf speichern wählen.')) editOperation('archive'); });
  ['undo', 'redo', 'save', 'export'].forEach(function (op) { editorButton('edit-' + op, function () { editOperation(op); }); });
  editorButton('edit-update', function () { editOperation('update_node', Object.assign({id: app.editNode}, editValues())); });
  editorButton('edit-add', function () { editOperation('add_node', Object.assign(editValues(),{radius:newPointRadius()})); });
  editorButton('edit-delete', function () { editOperation('delete_node', {id: app.editNode}); });
  ['from', 'to'].forEach(function (end) { editorButton('edit-as-' + end, function () { if (app.editNode != null) $('edit-' + end).value = app.editNode; }); });
  ['link', 'unlink'].forEach(function (op) { editorButton('edit-' + op, function () {
    if ($('edit-from').value === '' || $('edit-to').value === '') throw new Error('Von- und Nach-ID eingeben oder Punkte auswählen.');
    editOperation(op, {from: Number($('edit-from').value), to: Number($('edit-to').value), bidirectional: $('edit-both').checked});
  }); });
  editorButton('edit-camera', function () {
    if (!app.viewer) return;
    var p = app.viewer.getCameraState().position.slice(); p[2] -= 64;
    p.forEach(function (v, i) { $('edit-' + ['x', 'y', 'z'][i]).value = v.toFixed(3); });
    notice('Kamerafuß übernommen. Koordinaten prüfen und Punkt hinzufügen/ändern; dies ist noch keine Kollisionsprüfung.');
  });
  editorButton('edit-surface', function () {
    if (!app.viewer) return;
    setMode(true); app.viewer.onNavigationPlace = function (p) {
      if (!p) { notice('Keine begehbare BSP-Fläche getroffen. Boden anklicken oder Koordinaten eingeben.'); return; }
      app.viewer.onNavigationPlace = null;
      p.forEach(function (v, i) { $('edit-' + ['x', 'y', 'z'][i]).value = v.toFixed(3); });
      notice('Fußpunkt gewählt. „Punkt hinzufügen“ oder „Punkt ändern“ übernimmt ihn in den Entwurf.');
    };
    notice('Jetzt eine Bodenfläche im BSP anklicken. Escape bricht die Platzierung ab.');
  });
  editorButton('edit-download', function () {
    if (!app.draft) return;
    var a = document.createElement('a'), url = URL.createObjectURL(new Blob([JSON.stringify(app.draft, null, 2)], {type: 'application/json'}));
    a.href = url; a.download = app.map + '.draft-r' + app.draft.revision + '.json'; a.click(); setTimeout(function () { URL.revokeObjectURL(url); }, 1000);
  });
  $('edit-show').onchange = function () { cancelGesture(); editorStatus(); editorScene(); mouseStatus(); };
  document.addEventListener('keydown', editorKey);
  $('viewer').addEventListener('pointerdown', pointerDown);
  $('viewer').addEventListener('pointermove', pointerMove);
  $('viewer').addEventListener('pointerup', pointerUp);
  $('viewer').addEventListener('pointercancel', cancelGesture);
  $('viewer').addEventListener('lostpointercapture', function () { if (app.drag || app.cameraDrag || app.spawnDrag) cancelGesture(); });
  $('viewer').addEventListener('contextmenu', function (e) { e.preventDefault(); });
  $('viewer').addEventListener('wheel', function (e) {
    if (!app.viewer || app.viewer._navigationInspect === false || app.loading || app.drag) return;
    e.preventDefault(); app.viewer.navigationCameraZoom(e.deltaY * (e.deltaMode === 1 ? 16 : e.deltaMode === 2 ? 400 : 1));
  }, {passive: false});
  window.addEventListener('blur', cancelGesture);
  async function loadScene(map, reloadBsp) {
    if (app.loading) { app.nextScene = {map: map, reloadBsp: reloadBsp}; return; }
    cancelGesture();
    var serial = ++app.serial;
    var sameMap = app.map === map;
    app.map = map; app.collision = null; app.scene = null; app.sourceScene = null; app.draft = null; app.goalOverlay=null; editorStatus();
    text('map-title', map); notice(''); setLoading(true, 'Prüfstand und Geometrie laden …');
    try {
      var scene = await request('/api/scene?map=' + encodeURIComponent(map));
      if (serial !== app.serial) return;
      app.sourceScene = scene; app.scene = scene; app.draft = scene.draft || null; $('edit-show').checked = !!app.draft; editorStatus();
      updateGametypeAvailability();
      if (!app.viewer) {
        app.viewer = new BSPViewer($('viewer'));
        app.viewer.onNavigationSelect = function (selected) {
          if(selected?.kind==='spawn'){
            selectNavigation(null);editorSelection(null);
            window.dispatchEvent(new CustomEvent('navstudio:spawn-select',{detail:selected}));return;
          }
          selectNavigation(selected); editorSelection(selected);
        };
        app.viewer.onNavigationClick = function (event) {
          if (app.suppressClick) { app.suppressClick = false; return true; }
          if (!mouseReady() || app.editTool === 'select' || app.viewer._navigationInspect === false || app.viewer.onNavigationPlace) return false;
          mouseClick(event); return true;
        };
        app.viewer.onProgress = function (loaded, total, asset) { if (app.loading) text('loading-detail', loaded + ' / ' + total + ' · ' + (asset || 'Texturen')); };
      }
      if (!sameMap) app.viewer.selectNavigation(null, null);
      app.viewer.setAssetMap(map);
      app.viewer.setNavigation(navigationData(scene));
      app.viewer.setNavigationOptions(layerOptions());
      await loadGoalOverlay(map,serial);
      selectNavigation(app.viewer._navigationSelection); renderSceneInfo(scene);
      if (reloadBsp || app.viewerMap !== map) {
        await app.viewer.load(scene.bsp_url || '/api/bsp?map=' + encodeURIComponent(map));
        if (serial !== app.serial) return;
        app.viewerMap = map;
      }
      app.viewer.start(); setMode(true);
      editorScene();
      if (reloadBsp) {
        if ($('view-preset').value === 'ingame') app.viewer.enterNavigation();
        else app.viewer.focusNavigation();
      }
      if (scene.warning) notice(scene.warning);
      window.dispatchEvent(new CustomEvent('navstudio:scene', {detail: scene}));
    } catch (error) {
      if (serial !== app.serial) return;
      notice(error.message);
      if (app.viewer && app.scene) { app.viewer.start(); app.viewer.focusNavigation(); }
    } finally {
      if (serial === app.serial) setLoading(false);
      if (app.nextScene) { var next = app.nextScene; app.nextScene = null; loadScene(next.map, next.reloadBsp); }
    }
  }
  async function loadGoalOverlay(map,serial){
    try{
      var gametype=app.state?.game_mode==='et'?'':$('test-gametype').value;
      if (app.state?.game_mode !== 'et' && app.scene?.gametype_availability && !gametype) {
        app.goalOverlay={goals:[]};app.viewer?.setNavigationGoals?.(app.goalOverlay);
        text('goal-layer-status','Kein unterstützter Gametype mit Mapscript vorhanden.');return;
      }
      var bundle=await request('/api/navbundle?map='+encodeURIComponent(map)+(gametype?'&gametype='+gametype:''));
      if(map!==app.map||(serial!=null&&serial!==app.serial))return;
      app.goalOverlay=bundle.goal_overlay||{goals:[]};
      if(app.viewer?.setNavigationGoals)app.viewer.setNavigationGoals(app.goalOverlay);
      if($('goal-layer-status'))text('goal-layer-status',(app.goalOverlay.goals||[]).length+' Goals · '+(gametype?'GT'+gametype:'ET')+' · '+(app.goalOverlay.fallback?'gemeinsames Legacy-Paar':'gespeicherte GM-Datei')+(app.goalOverlay.error?' · '+app.goalOverlay.error:''));
    }catch(error){
      if(map!==app.map)return;
      app.goalOverlay={goals:[]};if(app.viewer?.setNavigationGoals)app.viewer.setNavigationGoals(app.goalOverlay);
      if($('goal-layer-status'))text('goal-layer-status','Goal-Vorschau nicht verfügbar: '+error.message);
    }
  }
  function renderJobs(state) {
    text('mode-info', state.game_mode === 'et' ? 'ET: originaler Omni-bot-Mod. Ansicht und Spieltest verfügbar; native Generierung benötigt einen ET-Prüfadapter.' : 'TCE2: obj_-Maps mit TCE2-Prüfung.');
    var jobs = state.jobs || [], running = jobs.find(function (j) { return busyStates.includes(j.status); });
    var game = state.game || {}, gameStates = {offline: 'Spiel geschlossen', starting: 'Spiel startet / wartet auf Map', running: 'Spiel läuft', unavailable: 'Spielstatus nicht verfügbar', conflict: 'Prozesskonflikt'};
    text('game-status', (gameStates[game.state] || 'Spielstatus unbekannt') + (game.map ? ' · ' + game.map : '') +
      (game.pid ? ' · PID ' + game.pid : '') + (game.mode === 'dedicated' ? ' · ohne Renderer (Map laden startet sichtbar neu)' : game.visible ? ' · Spielfenster vorhanden' : '') + (game.note ? '\n' + game.note : ''));
    text('game-input-status', game.input_note || '');
    document.querySelectorAll('[data-action],#install-production').forEach(function (button) {
      button.disabled = !!running || app.actionPending;
      if (state.game_mode === 'et' && button.id === 'install-production') button.disabled = true;
      if (state.game_mode === 'et' && !state.capabilities?.et_generation && ['apply_draft', 'generate', 'rebuild', 'repair_check', 'repair', 'snapshot', 'native_check', 'install_production', 'install_test', 'generate_goals'].includes(button.dataset.action)) button.disabled = true;
      // A crashed/starting own process may have no UDP port. The backend still
      // verifies its exact identity before closing it; keep that action usable.
      if (button.dataset.action === 'stop_test' && !(game.processes === 1 && Number.isInteger(game.pid) && game.pid > 0)) button.disabled = true;
      if (state.game_mode !== 'et' && app.scene?.gametype_availability && !$('test-gametype').value && button.dataset.action !== 'stop_test' && !['mode_tce2','mode_et'].includes(button.dataset.action)) button.disabled = true;
    });
    renderPackageControls();
    if ($('settings-dialog').open && (running || app.actionPending || game.processes > 0)) {
      $('settings-save').disabled = true;
      text('settings-info', 'Spiel oder Auftrag läuft. Erst beenden; danach „Neu laden“ wählen, um Einstellungen zu ändern.');
    }
    mouseStatus();
    // /api/state reverses the internal deque and publishes oldest first.
    var latest = running || jobs.find(function (j) { return j.id === app.watchedJob; }) || jobs[jobs.length - 1];
    $('job-status').replaceChildren();
    var dot = document.createElement('span'); dot.className = 'status-dot ' + (!latest ? 'idle' : latest.status === 'failed' ? 'failed' : '');
    var label = document.createElement('span'); label.textContent = latest ? (actionNames[latest.action] || latest.action || 'Auftrag') + ' · ' + (statusNames[latest.status] || latest.status) : 'Bereit';
    $('job-status').append(dot, label);
    $('job-progress').hidden = !running;
    if (running && typeof running.progress === 'number') $('job-progress').value = running.progress <= 1 ? running.progress*100 : running.progress;
    else $('job-progress').removeAttribute('value');
    text('job-log', jobs.length ? jobs.slice(-8).reverse().map(function (j) { return JSON.stringify(j, null, 2); }).join('\n\n') : 'Noch keine Aufträge.');
    if (running && typeof running.progress === 'string') label.textContent += ' · ' + running.progress;
    jobs.forEach(function (job) {
      if (busyStates.includes(job.status) || app.completedJobs.has(job.id)) return;
      app.completedJobs.add(job.id);
      if (job.id !== app.watchedJob) return;
      app.watchedJob = null;
      if (job.error || job.status === 'failed') notice(typeof job.error === 'string' ? job.error : JSON.stringify(job.error || 'Auftrag fehlgeschlagen'));
      else if (job.map === app.map) loadScene(app.map, false).then(function () {
        var notes = job.result && [job.result.note, job.result.reload_note].filter(Boolean);
        if (notes && notes.length) notice(notes.join(' '));
      });
    });
  }
  async function refreshState(initial) {
    try {
      var state = await request('/api/state');
      if (app.state && state.game_mode !== app.state.game_mode) app.initialized = false;
      app.state = state;
      ['tce2', 'et'].forEach(function(mode) { var b = $('mode-' + mode); if(b) { b.classList.toggle('active', state.game_mode === mode); b.setAttribute('aria-pressed', String(state.game_mode === mode)); b.disabled = state.busy; } });
      text('connection', new URLSearchParams(location.search).get('desktop') === '1' ? 'Studio bereit' : 'Studio bereit · ' + location.host);
      if (!app.initialized) {
        var maps = state.maps || [];
        $('map-select').replaceChildren();
        if (maps.length) {
          maps.forEach(function (map) { var name = typeof map === 'string' ? map : map.name; var option = document.createElement('option'); option.value = name; option.textContent = name; $('map-select').appendChild(option); });
          var placeholder = document.createElement('option'); placeholder.value = ''; placeholder.textContent = 'Map auswählen / Select map'; $('map-select').prepend(placeholder);
          $('map-select').value = ''; app.map = '';
        } else {
          app.map = ''; notice('Keine Karten für diesen Spielmodus gefunden.');
        }
        (state.jobs || []).forEach(function (job) { if (!busyStates.includes(job.status)) app.completedJobs.add(job.id); });
        app.initialized = true;
        if (app.map) loadScene(app.map, true);
      }
      renderJobs(state);
      window.dispatchEvent(new CustomEvent('navstudio:state', {detail: state}));
    } catch (error) { text('connection', 'Verbindung unterbrochen'); if (!app.initialized) { notice(error.message); setLoading(false); } }
  }
  var prerequisite = null;
  function askPrerequisite(name, extra, archive) {
    prerequisite = {name: name, extra: extra, archive: archive, map: app.map};
    text('prerequisite-message', archive ?
      'Diese Aktion benötigt eine neue Grundlage. Dein aktueller Entwurf wird vollständig als lokale Archivdatei gesichert und danach aus der Bearbeitung genommen. Anschließend wird die gewählte Aktion gestartet.' :
      'Für diese Prüfung muss der aktuelle Entwurf lokal gesichert und als Prüfkandidat exportiert werden. Das erledigt NavStudio jetzt für dich.');
    text('prerequisite-continue', archive ? 'Sichern, archivieren & fortfahren' : 'Sichern, exportieren & fortfahren');
    text('prerequisite-error', '');
    $('prerequisite-dialog').showModal();
  }
  $('prerequisite-cancel').onclick = function () { prerequisite = null; $('prerequisite-dialog').close(); };
  $('prerequisite-save').onclick = async function () {
    if (await editOperation('save')) text('prerequisite-error', 'Lokal gesichert. Zum Fortfahren oben die vorbereitende Aktion bestätigen.');
    else text('prerequisite-error', 'Sichern fehlgeschlagen. Hinweis im Hauptfenster beachten; es wird nichts fortgesetzt.');
  };
  $('prerequisite-continue').onclick = async function () {
    var pending = prerequisite;
    if (!pending || pending.map !== app.map) return;
    $('prerequisite-continue').disabled = true; $('prerequisite-save').disabled = true;
    try {
      // Archive writes the full current revision to disk before detaching it;
      // unlike save/export it is also available for stale geometry.
      var result = pending.archive ? await editOperation('archive') :
        (await editOperation('save') && await editOperation('export'));
      if (!result) { text('prerequisite-error', 'Vorbereitung fehlgeschlagen. Es wird nichts fortgesetzt. Hinweis im Hauptfenster beachten.'); return; }
      prerequisite = null; $('prerequisite-dialog').close();
      await action(pending.name, pending.extra);
    } finally { $('prerequisite-continue').disabled = false; $('prerequisite-save').disabled = false; }
  };
  async function action(name, extra) {
    if (!app.state || app.actionPending) return;
    if (!app.map && name !== 'stop_test') { notice('Bitte zuerst eine Map auswählen.'); return; }
    if (name !== 'stop_test' && app.state.game_mode !== 'et' && app.scene?.gametype_availability && !updateGametypeAvailability().includes(Number($('test-gametype').value))) { notice('Kein unterstützter Gametype mit Mapscript vorhanden.'); return; }
    if (app.draft && ['rebuild','snapshot'].includes(name)) { askPrerequisite(name, extra, true); return; }
    app.actionPending = true; notice(''); renderJobs(app.state);
    try {
      var result = await request('/api/action', {method: 'POST', headers: {'Content-Type': 'application/json', 'X-Navstudio-Token': app.state.token || ''},
        body: JSON.stringify(Object.assign({action: name, map: app.map, gametype: Number($('test-gametype').value)}, extra || {}))});
      app.watchedJob = result.id || result.job_id || result.job && result.job.id;
      await refreshState(false);
      if (!app.watchedJob && !result.job) loadScene(app.map, false);
      return result;
    } catch (error) { notice(error.message); }
    finally { app.actionPending = false; if (app.state) renderJobs(app.state); }
  }
  $('map-select').onchange = function () { loadScene(this.value, true); };
  $('reload-scene').onclick = function () { loadScene(app.map, false); };
  $('project-package-export').onclick = exportProjectPackage;
  document.querySelectorAll('[data-layer]').forEach(function (el) { el.onchange = updateLayers; });
  $('edge-mode').onchange = updateLayers; $('min-z').onchange = updateLayers; $('max-z').onchange = updateLayers;
  $('nav-distance').onchange = updateLayers;
  $('view-preset').onchange = presetView;
  $('reset-height').onclick = function () { $('min-z').value = ''; $('max-z').value = ''; updateLayers(); };
  $('camera-floor').onclick = function () {
    if (!app.viewer) return;
    var feet = app.viewer.getCameraState().position[2] - 64;
    $('min-z').value = Math.round(feet-96); $('max-z').value = Math.round(feet+96); updateLayers();
  };
  $('inspect-mode').onclick = function () { setMode(true); };
  $('fly-mode').onclick = function () { setMode(false); };
  $('eye-level').onclick = function () {
    if (!app.viewer) return;
    var selected = app.viewer._navigationSelection;
    var id = selected && (selected.kind === 'node' ? selected.id : selected.item.from);
    if (app.viewer.enterNavigation(id)) { setMode(false); notice('Augenhöhe gesetzt. Jetzt ins Bild klicken und mit Maus/WASD umsehen. Esc löst die Maus; „Auswählen“ aktiviert den Wegpunktinspektor.'); }
  };
  $('focus-map').onclick = function () { if (app.viewer) app.viewer.focusNavigation(); };
  $('focus-selection').onclick = function () { var selected = app.viewer && app.viewer._navigationSelection; if (selected) app.viewer.focusNavigation(selected.kind === 'node' ? selected.id : selected.kind==='goal'?selected.item.position:selected.item.from); };
  $('test-gametype').addEventListener('change',function(){if(app.map)loadGoalOverlay(app.map,app.serial);});
  $('node-search').onsubmit = function (event) {
    event.preventDefault();
    var id = $('node-id').value.trim(), node = id !== '' && nodesById(id);
    if (!node && id !== '') {
      var data = app.scene?.generation_diagnostics, diagnostic = data?.nodes?.find(function (n) { return String(n.id) === id; });
      if (diagnostic && app.viewer && diagnosticPosition(diagnostic,data)) {
        focusDiagnostic(diagnostic,data); text('node-search-status','Scannerpunkt '+id+' · Generierungsbericht, kein aktueller Wegpunkt.'); return;
      }
    }
    if (!app.viewer || !node) { text('node-search-status', id === '' ? 'Bitte eine Wegpunkt-ID eingeben.' : 'Wegpunkt ' + id + ' ist im geladenen Datensatz nicht vorhanden.'); return; }
    revealNode(node); app.viewer.selectNavigation('node', node.id != null ? node.id : Number(id)); app.viewer.focusNavigation(position(node));
    text('node-search-status', 'Wegpunkt ' + id + ' ausgewählt' + (accepted(node) ? '.' : ' · abgelehnter Prüfkandidat.'));
  };
  document.querySelectorAll('[data-action]').forEach(function (button) { button.onclick = function () { action(button.dataset.action); }; });
  $('install-production').onclick = function () { text('install-map', app.map); text('install-error', ''); $('install-confirm').value = ''; $('install-dialog').showModal(); };
  $('install-confirm-button').onclick = function () {
    if ($('install-confirm').value !== 'INSTALL') { text('install-error', 'Bitte INSTALL eingeben.'); return; }
    $('install-dialog').close(); action('install_production', {confirm: 'INSTALL'});
  };
  async function loadSettings() {
    $('settings-save').disabled = true; text('settings-error', '');
    try {
      var settings = await request('/api/settings');
      $('settings-instance').value = settings.instance_path || '';
      $('settings-executable').value = settings.game_executable || '';
      $('settings-maps').value = (settings.maps_dirs || []).join('\n');
      text('settings-info', (settings.busy || settings.running ? 'Spiel oder Auftrag läuft. Änderungen erst nach dem Beenden möglich. ' : '') +
        'Spielport: ' + settings.rcon_port + (settings.settings_file ? ' · Gespeichert in: ' + settings.settings_file : '') + (settings.note ? '\n' + settings.note : ''));
      $('settings-save').disabled = !!(settings.busy || settings.running);
    } catch (error) { text('settings-error', error.message); }
  }
  $('settings-open').onclick = function () { cancelGesture(); $('settings-dialog').showModal(); loadSettings(); };
  $('settings-close').onclick = function () { $('settings-dialog').close(); };
  $('settings-reload').onclick = loadSettings;
  $('settings-form').onsubmit = async function (event) {
    event.preventDefault(); if (!app.state || $('settings-save').disabled) return;
    $('settings-save').disabled = true; $('settings-reload').disabled = true; text('settings-error', '');
    try {
      await request('/api/settings', {method: 'POST', headers: {'Content-Type': 'application/json', 'X-Navstudio-Token': app.state.token},
        body: JSON.stringify({instance_path: $('settings-instance').value.trim(), game_executable: $('settings-executable').value.trim(),
          maps_dirs: $('settings-maps').value.split(/\r?\n/).map(function (p) { return p.trim(); }).filter(Boolean)})});
      app.initialized = false;
      await refreshState(false); await loadSettings();
      text('settings-info', 'Einstellungen gespeichert. Die Mapliste wurde neu geladen. Das Spiel wird erst mit „Map im Testspiel öffnen“ gestartet.');
    } catch (error) { text('settings-error', error.message); $('settings-save').disabled = false; }
    finally { $('settings-reload').disabled = false; }
  };
  document.addEventListener('pointerlockchange', function () { if (!document.pointerLockElement && app.viewer && app.viewer.getCameraState().mode === 'fly') setMode(true); });
  setInterval(function () { refreshState(false); }, 1800);
  setInterval(function () { if (!app.viewer) return; var p = app.viewer.getCameraState().position; text('camera-position', 'X ' + p[0].toFixed(0) + '  Y ' + p[1].toFixed(0) + '  Z ' + p[2].toFixed(0)); }, 350);
  window.addEventListener('beforeunload', function () { if (app.viewer) app.viewer.destroy(); });
  // Project/desktop shell uses the same action and revision checked editor.
  // No second copy of the graph is kept by the project toolbar.
  window.navstudioClient = {app: app, request: request, refresh: refreshState,
    loadScene: loadScene, loadGoals: function(){return app.map?loadGoalOverlay(app.map,app.serial):Promise.resolve();}, action: action, edit: editOperation, notice: notice,
    focus: function (point) { if (app.viewer) app.viewer.focusNavigation(point); },
    setMode: setMode, cancelGesture: cancelGesture, configureGametypeSelect: configureGametypeSelect,
    updateGametypeAvailability: updateGametypeAvailability, renderPackageControls: renderPackageControls};
  refreshState(true);
})();
