// The settings page served at http://192.168.4.1/ . Fully self-contained:
// phones on the monster's Wi-Fi have no internet, so no external fonts or scripts.
#include <Arduino.h>

extern const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>CreeperEyes</title>
<style>
:root{--bg:#0e120f;--card:#171d18;--line:#2a332b;--text:#e6ece4;--mute:#93a08f;--acc:#8fd16a;--warn:#e0a84a}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:16px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{padding:20px 16px 8px}h1{margin:0;font-size:22px;letter-spacing:.5px}h1 span{color:var(--acc)}
.sub{color:var(--mute);font-size:13px;margin-top:2px}
main{max-width:560px;margin:0 auto;padding:0 16px 40px}
section{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px 16px;margin:14px 0}
h2{font-size:13px;text-transform:uppercase;letter-spacing:1.2px;color:var(--mute);margin:0 0 10px;font-weight:600}
.row{display:flex;align-items:center;justify-content:space-between;gap:12px;padding:9px 0;border-top:1px solid var(--line)}
.row:first-of-type{border-top:0}.row label{flex:1}.row small{display:block;color:var(--mute);font-size:12px}
input[type=range]{width:150px;accent-color:var(--acc)}output{min-width:52px;text-align:right;color:var(--acc);font-variant-numeric:tabular-nums}
input[type=color]{width:46px;height:34px;border:1px solid var(--line);border-radius:8px;background:none;padding:2px}
select,input[type=password],input[type=text]{background:#0b0f0c;color:var(--text);border:1px solid var(--line);border-radius:8px;padding:8px 10px;font:inherit}
.sw{appearance:none;width:46px;height:26px;border-radius:13px;background:#333;position:relative;cursor:pointer;transition:.2s}
.sw:checked{background:var(--acc)}.sw:before{content:"";position:absolute;width:20px;height:20px;border-radius:50%;background:#fff;top:3px;left:3px;transition:.2s}.sw:checked:before{left:23px}
.btns{display:flex;flex-wrap:wrap;gap:8px}button{background:#223024;color:var(--text);border:1px solid var(--line);border-radius:10px;padding:10px 14px;font:inherit;cursor:pointer}
button.pri{background:var(--acc);color:#10200a;border-color:var(--acc);font-weight:600}button.warn{color:var(--warn)}
#status{display:grid;grid-template-columns:1fr 1fr;gap:8px}.stat{background:#0b0f0c;border-radius:10px;padding:8px 10px}.stat b{display:block;font-size:18px}
.stat span{color:var(--mute);font-size:12px}#toast{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);background:var(--acc);color:#10200a;padding:8px 14px;border-radius:20px;font-weight:600;opacity:0;transition:.3s}
#toast.on{opacity:1}
.eyebox{border-top:1px solid var(--line);margin-top:4px;padding-top:6px}.eyett{font-size:14px;margin:6px 0 0;color:var(--acc)}
body.linked .eyebox[data-eye=R]{display:none}body.linked .eyett{display:none}
</style></head><body>
<header><main style="padding:0"><h1>Creeper<span>Eyes</span></h1><div class="sub" id="net">Changes apply instantly and are saved automatically.</div></main></header>
<main>
<section><h2>Live</h2>
 <div id="status"><div class="stat"><span>Monster</span><b id="st">…</b></div><div class="stat"><span>Radar sees</span><b id="rd">…</b></div></div>
 <div class="btns" style="margin-top:10px"><button onclick="cmd('wake')">Wake up</button><button onclick="cmd('sleep')">Go to sleep</button></div>
</section>

<section><h2>Look</h2>
 <div class="row"><label>Same look for both eyes<small>Turn off to style each eye on its own</small></label><input type="checkbox" class="sw" data-k="linkEyes" id="link"></div>
 <div class="eyebox" data-eye="L"><h3 class="eyett">Left eye</h3>
  <div class="row"><label>Eye style</label><select data-k="style" class="styles"></select></div>
  <div class="row"><label>Iris colour<small>Off keeps the eye's own colours</small></label><input type="checkbox" class="sw" data-k="irisMode"><input type="color" data-k="irisColor"></div>
  <div class="row"><label>Eyelid colour<small>Match it to your printed skin</small></label><input type="color" data-k="eyelidColor"></div>
 </div>
 <div class="eyebox" data-eye="R"><h3 class="eyett">Right eye</h3>
  <div class="row"><label>Eye style</label><select data-k="style" class="styles"></select></div>
  <div class="row"><label>Iris colour<small>Off keeps the eye's own colours</small></label><input type="checkbox" class="sw" data-k="irisMode"><input type="color" data-k="irisColor"></div>
  <div class="row"><label>Eyelid colour<small>Match it to your printed skin</small></label><input type="color" data-k="eyelidColor"></div>
 </div>
 <div class="row"><label>Brightness</label><input type="range" min="1" max="15" data-k="brightness"><output></output></div>
 <div class="row"><label>Blinking</label><input type="checkbox" class="sw" data-k="autoBlink"></div>
</section>

<section><h2>Sound</h2>
 <div class="row"><label>Volume</label><input type="range" min="0" max="30" data-k="volume"><output></output></div>
 <div class="btns"><button onclick="cmd('sound:1')">Groan</button><button onclick="cmd('sound:2')">Growl 1</button><button onclick="cmd('sound:3')">Growl 2</button><button onclick="cmd('sound:6')">Mmm…</button></div>
</section>

<section><h2>Behaviour</h2>
 <div class="row"><label>Wakes up within</label><input type="range" min="500" max="5000" step="100" data-k="wakeDistMm" data-u="m"><output></output></div>
 <div class="row"><label>"Leaning in" within</label><input type="range" min="150" max="1200" step="50" data-k="leanDistMm" data-u="m"><output></output></div>
 <div class="row"><label>Falls asleep after</label><input type="range" min="2" max="60" data-k="sleepAfterS" data-u="s"><output></output></div>
 <div class="row"><label>Flip left/right<small>If the eyes look away from people</small></label><input type="checkbox" class="sw" data-k="flipGazeX"></div>
 <div class="row"><label>Flip up/down<small>If he looks up at close kids</small></label><input type="checkbox" class="sw" data-k="flipGazeY"></div>
</section>

<section><h2>Wi-Fi</h2>
 <div class="row"><label>New password<small>8+ characters. Reconnect after restarting the monster.</small></label><input type="password" id="pw" placeholder="unchanged" size="12"></div>
 <div class="btns"><button onclick="setPw()">Change password</button><button class="warn" onclick="resetAll()">Reset to defaults</button></div>
</section>
</main>
<div id="toast">Saved</div>
<script>
const $=s=>document.querySelectorAll(s);let t,S={};
function toast(m){const e=document.getElementById('toast');e.textContent=m||'Saved';e.classList.add('on');clearTimeout(t);t=setTimeout(()=>e.classList.remove('on'),1200)}
function fmt(el){const u=el.dataset.u,v=+el.value;return u=='m'?(v/1000).toFixed(v<1000?2:1)+' m':u=='s'?v+' s':v}
function key(el){const b=el.closest('.eyebox');return el.dataset.k+(b?b.dataset.eye:'')}
function fill(s){S=s;
 if(s.styles)$('select.styles').forEach(sel=>{if(sel.options.length!=s.styles.length)sel.innerHTML=s.styles.map((n,i)=>'<option value="'+i+'">'+n+'</option>').join('')});
 $('[data-k]').forEach(el=>{const v=s[key(el)];if(v===undefined)return;
  if(el.type=='checkbox')el.checked=!!v;else el.value=v;const o=el.parentElement.querySelector('output');if(o)o.textContent=fmt(el)});
 document.body.classList.toggle('linked',!!s.linkEyes);
 if(s.ssid)document.getElementById('net').textContent='Connected to '+s.ssid+' · changes apply instantly and save automatically.'}
async function post(u,b){const r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(b)});return r.json()}
const timers={};
function val(el){return el.type=='checkbox'?(el.checked?1:0):el.value}
function send(el){const k=key(el),v=val(el),b={[k]:v};
 if(el.closest('.eyebox')&&S.linkEyes){b[el.dataset.k+'L']=v;b[el.dataset.k+'R']=v}
 if(k=='linkEyes'&&v==1)['style','irisMode','irisColor','eyelidColor'].forEach(n=>b[n+'R']=S[n+'L']);
 clearTimeout(timers[k]);timers[k]=setTimeout(async()=>{fill(await post('/api/set',b));toast()},150)}
$('[data-k]').forEach(el=>{el.addEventListener('input',()=>{const o=el.parentElement.querySelector('output');if(o)o.textContent=fmt(el);send(el)})});
async function cmd(c){await post('/api/cmd',{c});toast(c.startsWith('sound')?'Playing':'OK')}
async function setPw(){const p=document.getElementById('pw').value;if(p.length<8){toast('8+ characters');return}
 await post('/api/set',{wifiPass:p});toast('Password saved');document.getElementById('pw').value=''}
async function resetAll(){if(!confirm('Reset all settings (except Wi-Fi password)?'))return;fill(await post('/api/reset',{}));toast('Reset')}
async function poll(){try{const s=await (await fetch('/api/status')).json();
 document.getElementById('st').textContent=s.state;
 document.getElementById('rd').textContent=!s.radar?'not connected':s.person?(s.dist/1000).toFixed(2)+' m':'nobody'}catch(e){}
 setTimeout(poll,700)}
fetch('/api/settings').then(r=>r.json()).then(fill);poll();
</script></body></html>)HTML";
