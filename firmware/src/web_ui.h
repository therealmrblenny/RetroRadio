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
h2{margin:0;font:600 11px var(--sans);letter-spacing:.14em;text-transform:uppercase;color:var(--muted)}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
button{font:500 14px var(--sans);color:var(--fg);background:#161c26;border:1px solid var(--line);border-radius:8px;padding:9px 13px;cursor:pointer;min-height:40px}
button:hover{border-color:var(--led)}button:active{background:var(--led-dim)}
button.on{background:var(--led-dim);border-color:var(--led);color:#fff}
button.sm{padding:5px 9px;min-height:32px;font-size:13px}
button:focus-visible,input:focus-visible{outline:2px solid var(--led);outline-offset:2px}
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
.knob{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
.note{font-size:13px;color:var(--muted);margin:0}
.err{color:var(--bad)}
@media(max-width:420px){.knob{grid-template-columns:repeat(2,1fr)}}
</style></head><body><div class="wrap">

<div class="face">
  <canvas id="mx" width="644" height="164" aria-label="LED display mirror"></canvas>
  <div class="np"><div class="st" id="st">&nbsp;</div><div class="ti" id="ti">&nbsp;</div></div>
  <div class="meta"><span id="codec"></span><span>VOL <b id="volv">-</b></span><span id="net"></span></div>
</div>

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
  <p class="note">Same as the real knob: turn = volume, press = browse/tune, hold = standby.</p>
  <div class="knob">
    <button data-k="ccw">&#8634; Turn left</button><button data-k="cw">Turn right &#8635;</button>
    <button data-k="press">Press</button><button data-k="long">Hold</button>
  </div>
</section>

<section>
  <h2>Stations</h2>
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
  <form class="row" id="sf">
    <input type="search" id="sq" placeholder="Search by name, e.g. triple j, jazz, BBC" aria-label="Search stations">
    <input type="text" id="sc" placeholder="Country code" value="" style="flex:0 0 120px" aria-label="Country code">
    <button type="submit">Search</button>
  </form>
  <p class="note" id="sinfo">From the radio-browser.info directory. MP3 and AAC streams work best.</p>
  <ul class="list" id="results"></ul>
</section>

<section>
  <h2>Settings</h2>
  <div class="row"><label class="lbl" for="br">Brightness</label><input type="range" id="br" min="0" max="7"></div>
  <form class="row" id="wf">
    <input type="text" id="ws" placeholder="WiFi name" aria-label="WiFi name" autocomplete="off">
    <input type="password" id="wp" placeholder="Password" aria-label="WiFi password">
    <button type="submit">Save &amp; restart</button>
  </form>
  <p class="note" id="info"></p>
</section>

</div><script>
const $=id=>document.getElementById(id);
const api=(p,o)=>fetch(p,o).then(r=>{if(!r.ok)return r.text().then(t=>{throw new Error(t||r.status)});return r.headers.get('content-type')?.includes('json')?r.json():r.text()});
const cmd=(c,v)=>api('/api/cmd?c='+c+(v!==undefined?'&v='+encodeURIComponent(v):''),{method:'POST'}).catch(()=>{});
let st={},stations=[],volDrag=false,brDrag=false;

// ---- LED marquee mirror ----
const cv=$('mx'),cx=cv.getContext('2d'),P=20,R=7.2;
function drawFb(hex){
  cx.fillStyle='#000';cx.fillRect(0,0,cv.width,cv.height);
  for(let x=0;x<32;x++){const c=parseInt(hex.substr(x*2,2),16)||0;
    for(let y=0;y<8;y++){const on=(c>>y)&1,px=x*P+12,py=y*P+12;
      cx.beginPath();cx.arc(px,py,R,0,7);
      if(on){cx.shadowColor='#4d8dff';cx.shadowBlur=14;cx.fillStyle='#8fb8ff';}else{cx.shadowBlur=0;cx.fillStyle='#0b1320';}
      cx.fill();}}
  cx.shadowBlur=0;
}

// ---- state polling ----
async function poll(){
  if(document.hidden){setTimeout(poll,1000);return}
  try{
    st=await api('/api/state');
    drawFb(st.fb);
    $('st').textContent=st.name||'—';
    $('ti').textContent=st.mode==='standby'?'Standby':(st.title||(st.playing&&!st.running?'Tuning…':' '));
    $('codec').textContent=st.codec?st.codec+(st.bitrate?' '+Math.round(st.bitrate/1000)+'k':''):'';
    $('volv').textContent=st.vol;
    $('net').textContent=st.net==='online'?('WiFi '+st.rssi+' dBm'):st.net;
    if(!volDrag)$('vol').value=st.vol;
    if(!brDrag)$('br').value=st.bright;
    $('vol').max=st.volMax;
    $('sb').classList.toggle('on',st.mode==='standby');
    $('sb').textContent=st.mode==='standby'?'Wake':'Standby';
    $('info').textContent=`IP ${st.ip} · WiFi "${st.ssid}" · free heap ${Math.round(st.heap/1024)} kB · PSRAM ${Math.round(st.psram/1024)} kB · up ${Math.round(st.uptime/60)} min · fw ${st.fw}`;
    document.querySelectorAll('#stations li').forEach((li,i)=>li.classList.toggle('cur',i===st.idx));
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
document.querySelectorAll('[data-k]').forEach(b=>b.onclick=()=>cmd('key',b.dataset.k));

// ---- stations ----
function esc(s){return s.replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
function renderStations(){
  $('stations').innerHTML=stations.map((s,i)=>`<li${i===st.idx?' class="cur"':''}><span class="n">${i+1}</span>
   <span class="nm">${esc(s.name)}<small>${esc(s.url)}</small></span>
   <button class="sm" data-a="play" data-i="${i}" aria-label="Play">&#9654;</button>
   <button class="sm" data-a="up" data-i="${i}" aria-label="Move up">&#9650;</button>
   <button class="sm" data-a="down" data-i="${i}" aria-label="Move down">&#9660;</button>
   <button class="sm" data-a="del" data-i="${i}" aria-label="Delete">&#10005;</button></li>`).join('');
}
async function loadStations(){stations=await api('/api/stations');renderStations()}
async function saveStations(){
  $('serr').hidden=true;
  try{await api('/api/stations',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(stations)})}
  catch(e){$('serr').textContent='Not saved: '+e.message;$('serr').hidden=false}
  await loadStations();
}
$('stations').onclick=e=>{
  const b=e.target.closest('button');if(!b)return;const i=+b.dataset.i;
  if(b.dataset.a==='play')cmd('play',i);
  else if(b.dataset.a==='up'&&i>0){[stations[i-1],stations[i]]=[stations[i],stations[i-1]];saveStations()}
  else if(b.dataset.a==='down'&&i<stations.length-1){[stations[i+1],stations[i]]=[stations[i],stations[i+1]];saveStations()}
  else if(b.dataset.a==='del'&&stations.length>1){
    if(b.dataset.confirm){stations.splice(i,1);saveStations()}
    else{b.dataset.confirm=1;b.textContent='Sure?';setTimeout(()=>{b.textContent='✕';delete b.dataset.confirm},2500)}}
};
$('addf').onsubmit=e=>{e.preventDefault();const u=$('au').value.trim();if(!u)return;
  stations.push({name:$('an').value.trim()||u,url:u});$('an').value=$('au').value='';saveStations()};

// ---- radio-browser.info search (runs in your browser, not on the radio) ----
const RB=['de1','de2','fi1','nl1'];
async function rb(path){for(const h of RB){try{const r=await fetch(`https://${h}.api.radio-browser.info${path}`);if(r.ok)return r.json()}catch(e){}}throw new Error('directory unreachable')}
let results=[];
$('sf').onsubmit=async e=>{e.preventDefault();
  const q=$('sq').value.trim(),c=$('sc').value.trim().toUpperCase();
  $('sinfo').textContent='Searching…';$('results').innerHTML='';
  try{
    let p=`/json/stations/search?limit=40&hidebroken=true&order=clickcount&reverse=true&name=${encodeURIComponent(q)}`;
    if(c)p+=`&countrycode=${encodeURIComponent(c)}`;
    results=(await rb(p)).filter(r=>r.url_resolved);
    $('sinfo').textContent=results.length?`${results.length} found. MP3 and AAC streams work best.`:'Nothing found.';
    $('results').innerHTML=results.map((r,i)=>{const ok=/mp3|aac|flac|opus|ogg/i.test(r.codec)&&!r.hls;
      return `<li><span class="nm">${esc(r.name.trim())}<small>${esc([r.codec,r.bitrate?r.bitrate+'k':'',r.countrycode,r.tags.split(',').slice(0,3).join(' ')].filter(Boolean).join(' · '))}${ok?'':' · may not play'}</small></span>
      <button class="sm" data-r="${i}">Add</button></li>`}).join('');
  }catch(err){$('sinfo').textContent='Search failed: '+err.message+' (the phone needs internet access).'}
};
$('results').onclick=e=>{const b=e.target.closest('button');if(!b)return;const r=results[+b.dataset.r];
  stations.push({name:r.name.trim().slice(0,90),url:r.url_resolved});b.textContent='Added';b.disabled=true;saveStations()};

// ---- wifi ----
$('wf').onsubmit=async e=>{e.preventDefault();if(!$('ws').value.trim())return;
  const f=new URLSearchParams({ssid:$('ws').value.trim(),pass:$('wp').value});
  await api('/api/wifi',{method:'POST',body:f}).catch(()=>{});
  $('info').textContent='Saved. The radio is restarting and will join "'+$('ws').value.trim()+'".'};

loadStations().then(poll);
</script></body></html>)HTML";
