/* Editor layout: one workflow toolbar, project/layers left, properties right.
   Move existing controls so every button keeps its tested event handler. */
(function () {
  'use strict';
  const left = document.querySelector('.sidebar'), right = document.querySelector('.inspector');
  const sections = Array.from(left.children), properties = Array.from(right.children);
  const bar = document.createElement('nav'); bar.id = 'workflow-toolbar';
  bar.setAttribute('aria-label', 'Projekt und Arbeitsablauf');
  bar.innerHTML = '<div class="workflow-project"><button id="project-new" class="button secondary">Neues Projekt</button>' +
    '<button id="project-open" class="button secondary">Projekt öffnen</button><button id="project-save" class="button secondary">Speichern</button>' +
    '<button id="project-close" class="button secondary">Schließen</button></div><div id="workflow-actions"></div>';
  document.querySelector('.topbar').after(bar);
  const actions = document.getElementById('workflow-actions');
  const move = (selector, label, target=actions) => {
    const control = document.querySelector(selector); if (!control) return;
    if (label) control.textContent = label;
    control.classList.remove('full', 'text-button'); control.classList.add('button', 'secondary');
    target.append(control);
  };
  move('[data-action="generate"]', 'Netz generieren');
  move('[data-action="native_check"]', 'Prüfen');
  move('[data-action="repair_check"]', 'Automatisch reparieren');
  move('#apply-draft', 'Ins Spiel übernehmen');
  move('#open-test-map', 'Testspiel starten');
  move('[data-action="stop_test"]', 'Spiel beenden');
  const project = document.createElement('section'); project.className = 'panel project-panel';
  project.innerHTML = '<p class="eyebrow">PROJEKT</p><h2 id="project-title">Kein Projekt geöffnet</h2>' +
    '<p id="project-map" class="hint"></p><p id="project-path" class="hint"></p><div id="project-tree"></div>';
  left.prepend(project);
  const maps = sections[0]; maps.classList.add('project-game-panel');
  maps.querySelector('.eyebrow').textContent = 'SPIEL UND GAMETYPE';
  maps.querySelector('label[for="map-select"]').hidden = true;
  document.getElementById('map-select').parentElement.hidden = true;
  document.getElementById('mode-info').hidden = true;
  maps.querySelector('p.hint:last-child').hidden = true;
  ['mode-tce2', 'mode-et'].forEach(id => { document.getElementById(id).hidden = true; });
  sections[1].classList.add('advanced-generation');
  sections[1].querySelector('.eyebrow').textContent = 'ARBEITSSTATUS';
  sections[1].querySelector('.hint').hidden = true;
  sections[3].hidden = true;
  document.getElementById('reload-scene').hidden = false;
  sections[2].querySelector('.eyebrow').textContent = 'DARSTELLUNG';
  properties[0].querySelector('.eyebrow').textContent = 'PUNKT-EIGENSCHAFTEN';
  ['edit-begin', 'edit-restore', 'edit-export', 'edit-save', 'edit-download', 'edit-archive'].forEach(id => {
    document.getElementById(id).classList.add('legacy-edit-control');
  });
  properties[0].querySelectorAll('#edit-controls > p.hint').forEach(p => { if(!p.id)p.hidden = true; });
  const results = document.createElement('section'); results.id = 'results-dock';
  results.append(properties[2],properties[4]);
  document.querySelector('.workspace').append(results);
  const diagnose=document.createElement('details');diagnose.id='diagnostics';diagnose.className='diagnostics panel';
  const summary=document.createElement('summary');summary.textContent='Diagnose / Erweitert';diagnose.append(summary);
  const explanation=document.createElement('p');explanation.className='hint';
  explanation.textContent='Prüfproben, technische Werkzeuge und Quelldateien. Offene Verbindungen stehen direkt unter der Karte.';
  diagnose.append(explanation);
  const technicalActions=document.createElement('div');technicalActions.className='diagnostic-actions';
  sections[1].querySelectorAll('button').forEach(b=>technicalActions.append(b));diagnose.append(technicalActions);
  const totals=document.createElement('div');totals.className='diagnostic-totals';
  totals.append(document.getElementById('stat-mesh').parentElement);
  diagnose.append(totals,properties[3],properties[5]);left.append(diagnose);
  properties[5].classList.add('provenance-panel');
  const openIssues=document.createElement('div');openIssues.innerHTML='<strong id="stat-problems">0</strong><span>Offene Stellen</span>';
  document.querySelector('.stats-strip').insertBefore(openIssues,document.querySelector('.stat-proof'));
  right.hidden = false;
  document.body.classList.remove('central-menu'); document.body.classList.add('project-editor');
})();
