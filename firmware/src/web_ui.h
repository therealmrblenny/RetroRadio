#pragma once
// Single-page web UI served from flash. Dark "stealth" theme with a live LED marquee mirror.
static const char INDEX_HTML[] = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<title>RetroRadio</title>
<style>
:root{--bg:#07090c;--panel:#0f131a;--line:#1d2430;--fg:#dfe6f1;--muted:#7f8ba0;--led:#4d8dff;--led-dim:#0d1a33;--warn:#f0a440;--bad:#ff6b6b;
--mono:ui-monospace,"SF Mono",Menlo,Consolas,monospace;--sans:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;color-scheme:dark}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.45 var(--sans)}
.wrap{max-width:720px;margin:0 auto;padding:16px max(16px,env(safe-area-inset-left)) 48px;display:flex;flex-direction:column;gap:18px}
.face{background:#000;border:1px solid var(--line);border-radius:12px;padding:18px 16px 14px;display:flex;flex-direction:column;gap:12px}
canvas{width:100%;height:auto;display:block;image-rendering:pixelated}
.np{display:flex;flex-direction:column;gap:2px;min-width:0}
.np .st{font-weight:600;font-size:17px;overflow-wrap:anywhere}
.np .ti{color:var(--muted);overflow-wrap:anywhere}
.meta{font:12px var(--mono);color:var(--muted);display:flex;gap:12px;flex-wrap:wrap}
.meta b{color:var(--led);font-weight:500}
section{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:14px;display:flex;flex-direction:column;gap:12px}
section.hi{border-color:var(--led)}
h2{margin:0;font:600 11px var(--sans);letter-spacing:.14em;text-transform:uppercase;color:var(--muted)}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
button,select{font:500 14px var(--sans);color:var(--fg);background:#161c26;border:1px solid var(--line);border-radius:8px;padding:9px 13px;cursor:pointer;min-height:40px}
button:hover{border-color:var(--led)}button:active{background:var(--led-dim)}
button.on{background:var(--led-dim);border-color:var(--led);color:#fff}
button.sm,select.sm{padding:5px 9px;min-height:32px;font-size:13px}
button:disabled{opacity:.5;cursor:default}
button:focus-visible,input:focus-visible,select:focus-visible{outline:2px solid var(--led);outline-offset:2px}
input[type=text],input[type=password],input[type=search]{font:14px var(--sans);color:var(--fg);background:#0a0d12;border:1px solid var(--line);border-radius:8px;padding:9px 11px;min-width:0;flex:1}
input[type=range]{flex:1;accent-color:var(--led);min-width:120px}
label.lbl{font-size:13px;color:var(--muted);min-width:80px}
ol,ul{list-style:none;margin:0;padding:0}
.list li{display:flex;align-items:center;gap:8px;padding:8px 0;border-top:1px solid var(--line)}
.list li:first-child{border-top:0}
.list .n{font:12px var(--mono);color:var(--muted);width:22px;text-align:right;flex:none}
.list .nm{flex:1;min-width:0;overflow-wrap:anywhere}
.list .nm small{display:block;color:var(--muted);font:11px var(--mono);overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.list li.cur .nm{color:var(--led)}
.list .acts{display:flex;gap:6px;flex-wrap:wrap;justify-content:flex-end}
.tabs{display:flex;gap:6px;flex-wrap:wrap}
.tabs button{border-radius:999px;padding:6px 12px;min-height:34px;font-size:13px}
.tabs button .c{color:var(--muted);font:11px var(--mono);margin-left:6px}
.tabs button.on .c{color:#b8cdf5}
.knob{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
.seg{display:inline-flex;border:1px solid var(--line);border-radius:8px;overflow:hidden}
.seg button{border:0;border-radius:0;min-height:36px}
.note{font-size:13px;color:var(--muted);margin:0}
.err{color:var(--bad)}
@media(max-width:480px){.knob{grid-template-columns:repeat(2,1fr)}.list li{flex-wrap:wrap}.list .acts{width:100%}}
</style></head><body><div class="wrap">

<div class="face">
  <canvas id="mx" width="664" height="164" aria-label="LED display mirror"></canvas>
  <div class="np"><div class="st" id="st">&nbsp;</div><div class="ti" id="ti">&nbsp;</div></div>
  <div class="meta"><span id="codec"></span><span>VOL <b id="volv">-</b></span><span id="net"></span></div>
</div>

<section id="wifisec" hidden class="hi">
  <h2>Connect to your WiFi</h2>
  <p class="note">The radio couldn't join a network, so it started its own hotspot. Pick your WiFi, enter the password and it will restart and join it.</p>
  <div id="wifislot"></div>
</section>

<section>
  <h2>Controls</h2>
  <div class="row"><label class="lbl" for="vol">Volume</label><input type="range" id="vol" min="0" max="21"></div>
  <div class="row">
    <button id="prev">&#9664; Prev</button><button id="next">Next &#9654;</button>
    <button id="sb">Standby</button>
  </div>
</section>

<section>
  <h2>Virtual knob</h2>
  <p class="note">Same as the real knob: turn = volume, press = browse/tune, hold = standby. With more than one list, turn past the last station to reach "Lists".</p>
  <div class="knob">
    <button data-k="ccw">&#8634; Turn left</button><button data-k="cw">Turn right &#8635;</button>
    <button data-k="press">Press</button><button data-k="long">Hold</button>
  </div>
</section>

<section>
  <h2>Stations</h2>
  <div class="tabs" id="tabs" role="tablist" aria-label="Station lists"></div>
  <div class="row" id="listbar"></div>
  <ol class="list" id="stations"></ol>
  <form class="row" id="addf">
    <input type="text" id="an" placeholder="Name" aria-label="Station name">
    <input type="text" id="au" placeholder="Stream URL (http://...)" aria-label="Stream URL">
    <button type="submit">Add</button>
  </form>
  <p class="note err" id="serr" hidden></p>
</section>

<section>
  <h2>Find stations</h2>
  <div class="row">
    <button id="hot">&#128293; What's hot</button><button id="top">Most played</button><button id="rnd">&#127922; Random</button>
  </div>
  <form class="row" id="sf">
    <input type="search" id="sq" placeholder="Search by name or genre, e.g. triple j, jazz" aria-label="Search stations">
    <input type="text" id="sc" placeholder="Country (AU)" style="flex:0 0 110px" aria-label="Country code" maxlength="2">
    <button type="submit">Search</button>
  </form>
  <p class="note" id="sinfo">From the radio-browser.info directory. "Try" plays it without saving; "Add" puts it in the list selected above.</p>
  <ul class="list" id="results"></ul>
</section>

<section>
  <h2>Settings</h2>
  <div class="row"><label class="lbl" for="br">Brightness</label><input type="range" id="br" min="0" max="7"></div>
  <div class="row"><span class="lbl">Text style</span>
    <div class="seg" role="group" aria-label="Text style"><button id="fc">COMPACT CAPS</button><button id="fl">Classic</button></div>
  </div>
  <div id="wifihome">
  <form class="row" id="wf">
    <select id="wl" aria-label="Nearby WiFi networks" hidden></select>
    <input type="text" id="ws" placeholder="WiFi name" aria-label="WiFi name" autocomplete="off">
    <input type="password" id="wp" placeholder="Password" aria-label="WiFi password">
    <button type="button" id="scan">Scan</button>
    <button type="submit">Save &amp; restart</button>
  </form>
  </div>
  <p class="note" id="info"></p>
</section>

</div><script>
const $=id=>document.getElementById(id);
const api=(p,o)=>fetch(p,o).then(r=>{if(!r.ok)return r.text().then(t=>{throw new Error(t||r.status)});return r.headers.get('content-type')?.includes('json')?r.json():r.text()});
const cmd=(c,v,x='')=>api('/api/cmd?c='+c+(v!==undefined?'&v='+encodeURIComponent(v):'')+x,{method:'POST'}).catch(()=>{});
let st={},lists=[],tab=0,volDrag=false,brDrag=false;

// ---- LED marquee mirror ----
const cv=$('mx'),cx=cv.getContext('2d'),P=20,R=7.2,GAP=16;   // column 16 = the hidden join between the modules
function drawFb(hex){
  cx.fillStyle='#000';cx.fillRect(0,0,cv.width,cv.height);
  const n=hex.length/2;
  for(let x=0;x<n;x++){if(n>32&&x===GAP)continue;const c=parseInt(hex.substr(x*2,2),16)||0;
    for(let y=0;y<8;y++){const on=(c>>y)&1,px=x*P+12,py=y*P+12;
      cx.beginPath();cx.arc(px,py,R,0,7);
      if(on){cx.shadowColor='#4d8dff';cx.shadowBlur=14;cx.fillStyle='#8fb8ff';}else{cx.shadowBlur=0;cx.fillStyle='#0b1320';}
      cx.fill();}}
  cx.shadowBlur=0;
}

// ---- state polling ----
let setupShown=null;
async function poll(){
  if(document.hidden){setTimeout(poll,1000);return}
  try{
    st=await api('/api/state');
    drawFb(st.fb);
    $('st').textContent=st.mode==='setup'?'Setup hotspot':(st.name||'—');
    $('ti').textContent=st.mode==='standby'?'Standby':st.mode==='setup'?'Choose your WiFi below':(st.title||(st.playing&&!st.running?'Tuning…':' '));
    $('codec').textContent=st.codec?st.codec+(st.bitrate?' '+Math.round(st.bitrate/1000)+'k':''):'';
    $('volv').textContent=st.vol;
    $('net').textContent=st.net==='online'?('WiFi '+st.rssi+' dBm'):st.net;
    if(!volDrag)$('vol').value=st.vol;
    if(!brDrag)$('br').value=st.bright;
    $('vol').max=st.volMax;
    $('sb').classList.toggle('on',st.mode==='standby');
    $('sb').textContent=st.mode==='standby'?'Wake':'Standby';
    $('fc').classList.toggle('on',st.compact);$('fl').classList.toggle('on',!st.compact);
    $('info').textContent=`IP ${st.ip} · WiFi "${st.ssid}" · free heap ${Math.round(st.heap/1024)} kB · PSRAM ${Math.round(st.psram/1024)} kB · up ${Math.round(st.uptime/60)} min · fw ${st.fw}`;
    markCurrent();
    const setup=st.mode==='setup';
    if(setup!==setupShown){setupShown=setup;   // in hotspot mode, move the WiFi form to the top
      $('wifisec').hidden=!setup;(setup?$('wifislot'):$('wifihome')).appendChild($('wf'));
      if(setup&&!$('wl').options.length)scan();}
  }catch(e){$('ti').textContent='Radio not reachable…'}
  setTimeout(poll,250);
}

// ---- controls ----
$('vol').oninput=e=>{volDrag=true;cmd('vol',e.target.value)};
$('vol').onchange=()=>volDrag=false;
$('br').oninput=e=>{brDrag=true;cmd('bright',e.target.value)};
$('br').onchange=()=>brDrag=false;
$('prev').onclick=()=>cmd('prev');$('next').onclick=()=>cmd('next');
$('sb').onclick=()=>cmd('standby',st.mode==='standby'?0:1);
$('fc').onclick=()=>cmd('font',1);$('fl').onclick=()=>cmd('font',0);
document.querySelectorAll('[data-k]').forEach(b=>b.onclick=()=>cmd('key',b.dataset.k));

// ---- stations & lists ----
function esc(s){return String(s).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
const cur=()=>lists[tab]||{name:'',stations:[]};
function markCurrent(){document.querySelectorAll('#stations li').forEach((li,i)=>li.classList.toggle('cur',tab===st.list&&i===st.idx))}
function renderTabs(){
  $('tabs').innerHTML=lists.map((l,i)=>`<button role="tab" aria-selected="${i===tab}" class="${i===tab?'on':''}" data-t="${i}">${esc(l.name)}<span class="c">${l.stations.length}</span></button>`).join('')
    +`<button data-t="new" aria-label="New list">+ New list</button>`;
  $('listbar').innerHTML=`<button class="sm" data-l="ren">Rename</button>`
    +(lists.length>1?`<button class="sm" data-l="left" aria-label="Move list left">&#9664;</button><button class="sm" data-l="right" aria-label="Move list right">&#9654;</button><button class="sm" data-l="del">Delete list</button>`:'')
    +`<span class="note">${lists.length>1?'On the radio: turn past the last station to reach "Lists".':'Add a second list to group stations; the knob gets a "Lists" entry.'}</span>`;
}
function renderStations(){
  renderTabs();
  const others=lists.map((l,i)=>i===tab?'':`<option value="${i}">${esc(l.name)}</option>`).join('');
  $('stations').innerHTML=cur().stations.map((s,i)=>`<li><span class="n">${i+1}</span>
   <span class="nm">${esc(s.name)}<small>${esc(s.url)}</small></span><span class="acts">
   <button class="sm" data-a="play" data-i="${i}" aria-label="Play">&#9654;</button>
   <button class="sm" data-a="up" data-i="${i}" aria-label="Move up">&#9650;</button>
   <button class="sm" data-a="down" data-i="${i}" aria-label="Move down">&#9660;</button>
   ${others?`<select class="sm" data-a="move" data-i="${i}" aria-label="Move to list"><option value="">Move to…</option>${others}</select>`:''}
   <button class="sm" data-a="del" data-i="${i}" aria-label="Delete">&#10005;</button></span></li>`).join('')
   ||'<li><span class="note">This list is empty. Add stations below or move some here.</span></li>';
  markCurrent();
}
async function loadStations(){const d=await api('/api/stations');lists=d.lists||[];if(tab>=lists.length)tab=0;renderStations()}
async function saveStations(){
  $('serr').hidden=true;
  try{await api('/api/stations',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({lists})})}
  catch(e){$('serr').textContent='Not saved: '+e.message;$('serr').hidden=false}
  await loadStations();
}
$('tabs').onclick=e=>{const b=e.target.closest('button');if(!b)return;
  if(b.dataset.t==='new'){const n=prompt('Name for the new list (e.g. Jazz, Aussie, Chill)');if(!n||!n.trim())return;
    lists.push({name:n.trim().slice(0,40),stations:[]});tab=lists.length-1;saveStations();return}
  tab=+b.dataset.t;renderStations()};
$('listbar').onclick=e=>{const b=e.target.closest('button');if(!b)return;const a=b.dataset.l;
  if(a==='ren'){const n=prompt('Rename list',cur().name);if(n&&n.trim()){cur().name=n.trim().slice(0,40);saveStations()}}
  else if(a==='left'&&tab>0){[lists[tab-1],lists[tab]]=[lists[tab],lists[tab-1]];tab--;saveStations()}
  else if(a==='right'&&tab<lists.length-1){[lists[tab+1],lists[tab]]=[lists[tab],lists[tab+1]];tab++;saveStations()}
  else if(a==='del'){const n=cur().stations.length;
    if(n&&!b.dataset.confirm){b.dataset.confirm=1;b.textContent=`Delete ${n} station${n>1?'s':''}?`;setTimeout(()=>{b.textContent='Delete list';delete b.dataset.confirm},3000);return}
    lists.splice(tab,1);tab=Math.max(0,tab-1);saveStations()}};
$('stations').onclick=e=>{
  const b=e.target.closest('button');if(!b)return;const i=+b.dataset.i,s=cur().stations;
  if(b.dataset.a==='play')cmd('play',i,'&l='+tab);
  else if(b.dataset.a==='up'&&i>0){[s[i-1],s[i]]=[s[i],s[i-1]];saveStations()}
  else if(b.dataset.a==='down'&&i<s.length-1){[s[i+1],s[i]]=[s[i],s[i+1]];saveStations()}
  else if(b.dataset.a==='del'){
    if(b.dataset.confirm){s.splice(i,1);saveStations()}
    else{b.dataset.confirm=1;b.textContent='Sure?';setTimeout(()=>{b.textContent='✕';delete b.dataset.confirm},2500)}}
};
$('stations').onchange=e=>{const sel=e.target.closest('select');if(!sel||sel.value==='')return;
  const [s]=cur().stations.splice(+sel.dataset.i,1);lists[+sel.value].stations.push(s);saveStations()};
$('addf').onsubmit=e=>{e.preventDefault();const u=$('au').value.trim();if(!u)return;
  cur().stations.push({name:$('an').value.trim()||u,url:u});$('an').value=$('au').value='';saveStations()};

// ---- radio-browser.info (runs in your browser, not on the radio) ----
const RB=['de1','de2','fi1','nl1'];
async function rb(path){for(const h of RB){try{const r=await fetch(`https://${h}.api.radio-browser.info${path}`);if(r.ok)return r.json()}catch(e){}}throw new Error('directory unreachable')}
let results=[];
async function find(order,label,name=''){
  const c=$('sc').value.trim().toUpperCase();
  $('sinfo').textContent=label+'…';$('results').innerHTML='';
  try{
    let p=`/json/stations/search?limit=40&hidebroken=true&order=${order}&reverse=true&name=${encodeURIComponent(name)}`;
    if(c)p+=`&countrycode=${encodeURIComponent(c)}`;
    results=(await rb(p)).filter(r=>r.url_resolved&&!r.hls);
    $('sinfo').textContent=results.length?`${label}: ${results.length} station${results.length>1?'s':''}${c?' in '+c:''}. "Try" plays without saving; "Add" saves to "${cur().name}".`:'Nothing found.';
    $('results').innerHTML=results.map((r,i)=>{const ok=/mp3|aac|flac|opus|ogg/i.test(r.codec);
      return `<li><span class="nm">${esc(r.name.trim())}<small>${esc([r.codec,r.bitrate?r.bitrate+'k':'',r.countrycode,r.tags.split(',').slice(0,3).join(' ')].filter(Boolean).join(' · '))}${ok?'':' · may not play'}</small></span>
      <span class="acts"><button class="sm" data-try="${i}">Try</button><button class="sm" data-r="${i}">Add</button></span></li>`}).join('');
  }catch(err){$('sinfo').textContent='Search failed: '+err.message+' (the phone needs internet access).'}
}
$('sf').onsubmit=e=>{e.preventDefault();find('clickcount','Searching',$('sq').value.trim())};
$('hot').onclick=()=>find('clicktrend',"What's hot");
$('top').onclick=()=>find('clickcount','Most played');
$('rnd').onclick=()=>find('random','Random picks');
$('results').onclick=e=>{const b=e.target.closest('button');if(!b)return;
  if(b.dataset.try!==undefined){const r=results[+b.dataset.try];
    api('/api/try',{method:'POST',body:new URLSearchParams({name:r.name.trim().slice(0,90),url:r.url_resolved})}).catch(()=>{});
    fetch(`https://de1.api.radio-browser.info/json/url/${r.stationuuid}`).catch(()=>{});return}   // counts a click, as the directory asks
  const r=results[+b.dataset.r];
  cur().stations.push({name:r.name.trim().slice(0,90),url:r.url_resolved});b.textContent='Added';b.disabled=true;saveStations()};

// ---- wifi ----
async function scan(){
  $('scan').disabled=true;$('scan').textContent='Scanning…';
  try{for(let i=0;i<20;i++){const d=await api('/api/scan');
      if(d.networks){d.networks.sort((a,b)=>b.rssi-a.rssi);
        $('wl').innerHTML='<option value="">Choose a network…</option>'+d.networks.map(n=>`<option value="${esc(n.ssid)}">${esc(n.ssid)} (${n.rssi} dBm${n.open?', open':''})</option>`).join('');
        $('wl').hidden=!d.networks.length;break}
      await new Promise(r=>setTimeout(r,700))}}
  catch(e){}
  $('scan').disabled=false;$('scan').textContent='Scan';
}
$('scan').onclick=scan;
$('wl').onchange=()=>{if($('wl').value){$('ws').value=$('wl').value;$('wp').focus()}};
$('wf').onsubmit=async e=>{e.preventDefault();if(!$('ws').value.trim())return;
  const f=new URLSearchParams({ssid:$('ws').value.trim(),pass:$('wp').value});
  await api('/api/wifi',{method:'POST',body:f}).catch(()=>{});
  document.querySelector('#wifisec .note').textContent=$('info').textContent='Saved. The radio is restarting to join "'+$('ws').value.trim()+'". Reconnect this phone to your normal WiFi and open http://retroradio.local (the radio shows the address once it\'s online).'};

loadStations().then(poll);
</script></body></html>)HTML";
