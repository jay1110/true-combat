/* Live Omni-bot editing uses the existing listen player, not browser camera. */
(() => {
  const $ = id => document.getElementById(id);
  let context, pending = false, propertySpecs = {};
  const valueChoices=document.createElement('select'); valueChoices.id='botmenu-value-choices';
  $('botmenu-value').after(valueChoices); valueChoices.hidden=true;
  valueChoices.onchange=()=>{$('botmenu-value').value=valueChoices.value;};
  const help=document.createElement('p'); help.id='botmenu-property-help';
  $('botmenu-set').after(help);
  const wayNames=document.createElement('div');
  const wayLabel=document.createElement('h3');wayLabel.textContent='Wegpunktname / Waypoint name';
  const wayName=document.createElement('input');wayName.id='botmenu-waypoint-name';wayName.maxLength=64;
  wayName.placeholder='Front Door Switch';
  const wayApply=document.createElement('button');wayApply.type='button';wayApply.textContent='Naechsten Punkt benennen / Name nearest point';
  wayApply.onclick=()=>run({action:'waypoint_name',name:wayName.value});
  wayNames.append(wayLabel,wayName,wayApply);help.after(wayNames);
  function propertyChanged(){
    const spec=propertySpecs[$('botmenu-property').value]||{}, input=$('botmenu-value');
    valueChoices.replaceChildren();valueChoices.hidden=spec.kind!=='enum';input.hidden=spec.kind==='enum';
    input.type=(spec.kind==='number'||spec.kind==='integer')?'number':'text';
    input.step=spec.kind==='integer'?'1':'any';input.maxLength=64;
    if(spec.minimum!=null)input.min=String(spec.minimum);else input.removeAttribute('min');
    if(spec.maximum!=null)input.max=String(spec.maximum);else input.removeAttribute('max');
    if(spec.kind==='enum'){
      for(const value of spec.values){const option=document.createElement('option');option.value=value;option.textContent=spec.labels?.[value]??value;valueChoices.append(option);}
      input.value=valueChoices.value;
    }else input.value='';
    help.textContent=(spec.label||'')+' · Goal-Eigenschaften haengen vom ausgewaehlten Typ ab. Goal Help zeigt dessen gueltige Optionen.';
  }
  $('botmenu-property').onchange=propertyChanged;
  async function request(path, options) {
    const r = await fetch(path, options), body = await r.json();
    if (!r.ok) throw new Error(body.error || r.statusText);
    return body;
  }
  async function run(data) {
    if (pending) return;
    pending = true; $('botmenu-result').textContent = 'Befehl wird einmal gesendet …';
    try {
      const r = await request('/api/botmenu', {method:'POST', headers:{'Content-Type':'application/json', 'X-Navstudio-Token':context.token},
        body:JSON.stringify({...data, map:context.map, game_mode:context.game_mode,gametype:context.gametype})});
      $('botmenu-result').textContent = r.command + '\n' + (r.saved ? 'Gespeichert / Saved: ' + r.saved + '\n' : '') +
        (r.backup ? 'Backup: ' + r.backup + '\n' : '') + (r.reply || 'Gesendet; Ergebnis in der Spielkonsole pruefen.');
      if(r.saved){
        if(window.navstudioClient?.app.map===r.map)await window.navstudioClient.loadScene(r.map,false);
        await window.navstudioBundle?.refreshIfOpen(r.map,context.gametype);
        if(r.note)$('botmenu-result').textContent+='\n'+r.note;
      }
    } catch(e) { $('botmenu-result').textContent = e.message; }
    finally { pending = false; }
  }
  $('botmenu-open').onclick = async () => {
    $('botmenu-dialog').showModal();
    try {
      const state = await request('/api/state');
      context = {token:state.token, game_mode:state.game_mode, map:$('map-select').value};
      if(context.game_mode==='tce2') context.gametype=(await request('/api/gamecontext')).gametype;
      $('botmenu-context').textContent = context.map + ' · ' + context.game_mode + (context.gametype==null?'':' · GT '+context.gametype+' → '+context.map+'_gt'+context.gametype+'_goals.gm');
      const catalog = await request('/api/botmenu');
      $('botmenu-actions').replaceChildren(); $('botmenu-type').replaceChildren();
      propertySpecs=catalog.properties||{};$('botmenu-property').replaceChildren();
      for(const [key,spec] of Object.entries(propertySpecs)){const option=document.createElement('option');option.value=key;option.textContent=spec.label+' ('+key+')';$('botmenu-property').append(option);}
      propertyChanged();
      for (const type of catalog.types) { const o=document.createElement('option'); o.value=o.textContent=type; $('botmenu-type').append(o); }
      let group, container;
      for (const a of catalog.actions) {
        if (group !== a.group) {
          group=a.group; const section=document.createElement('details'), title=document.createElement('summary');
          title.textContent=group; section.append(title); container=document.createElement('div');
          container.style.cssText='display:flex;flex-wrap:wrap;gap:6px;margin:8px 0'; section.append(container); $('botmenu-actions').append(section);
          if (group==='Goals') section.open=true;
        }
        const b=document.createElement('button'); b.type='button'; b.className='button secondary'; b.textContent=a.label;
        b.title='bot '+a.command; b.onclick=()=>{if(!a.confirm||window.confirm(a.label+'?\n bot '+a.command))run({action:a.id});}; container.append(b);
      }
      $('botmenu-result').textContent='Bereit. Aktionen gelten fuer das laufende Spiel und dessen Spielerposition/Fadenkreuz.';
    } catch(e) { $('botmenu-result').textContent=e.message; }
  };
  $('botmenu-create').onclick=()=>run({action:'goal_create_named',type:$('botmenu-type').value,name:$('botmenu-name').value});
  $('botmenu-set').onclick=()=>run({action:'goal_property',property:$('botmenu-property').value,value:$('botmenu-value').value});
  $('botmenu-close').onclick=()=>{if(!pending)$('botmenu-dialog').close();};
  $('botmenu-dialog').addEventListener('cancel',e=>{if(pending)e.preventDefault();});
})();
