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
#status{grid-template-columns:1fr 1fr}
#pad{position:relative;width:100%;aspect-ratio:1.6;background:radial-gradient(circle,#24311f,#0b0f0c 70%);border:1px solid var(--line);border-radius:14px;touch-action:none;margin:6px 0 10px}
#dot{position:absolute;width:34px;height:34px;margin:-17px 0 0 -17px;border-radius:50%;background:#111;border:6px solid var(--acc);left:50%;top:50%;pointer-events:none}
.pp{opacity:.55}.pp.on{opacity:1}#pnote{color:var(--mute);font-size:12px}
progress{width:100%;height:10px;accent-color:var(--acc)}
</style></head><body>
<header><main style="padding:0"><h1>Creeper<span>Eyes</span></h1><div class="sub" id="net">Changes apply instantly and are saved automatically.</div></main></header>
<main>
<section><h2>Live</h2>
 <div id="status"><div class="stat"><span>Monster</span><b id="st">…</b></div><div class="stat"><span>Radar sees</span><b id="rd">…</b></div>
  <div class="stat"><span>Visitors tonight</span><b id="vt">…</b></div><div class="stat"><span>All-time</span><b id="va">…</b></div></div>
 <div class="btns" style="margin-top:10px"><button onclick="cmd('wake')">Wake up</button><button onclick="cmd('sleep')">Go to sleep</button><button onclick="resetVisits()">Reset tonight</button></div>
</section>

<section class="pp" id="pup"><h2>Puppet mode</h2>
 <div id="pnote">Drag on the pad to steer his eyes. He goes back to normal 90 s after your last touch, or when you tap Release.</div>
 <div id="pad"><div id="dot"></div></div>
 <div class="btns">
  <button onclick="pa(1)">Wink L</button><button onclick="pa(2)">Wink R</button><button onclick="pa(3)">Slow blink</button><button onclick="pa(4)">Eyes shut/open</button>
  <button onclick="pa(5)">Cross-eyed</button><button onclick="pa(6)">Eye roll</button><button onclick="pa(7)">Look around</button>
 </div>
 <div class="btns" style="margin-top:8px">
  <button onclick="pa(101)">Groan</button><button onclick="pa(102)">Growl</button><button onclick="pa(107)">Roar</button><button onclick="pa(106)">Mmm…</button><button onclick="pa(110)">Chuckle</button><button onclick="pa(108)">Snore</button>
  <button class="pri" onclick="pa(20)">Release</button>
 </div>
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
 <div class="btns"><button onclick="cmd('sound:1')">Groan</button><button onclick="cmd('sound:2')">Growl 1</button><button onclick="cmd('sound:3')">Growl 2</button><button onclick="cmd('sound:6')">Mmm…</button><button onclick="cmd('sound:7')">Roar</button><button onclick="cmd('sound:9')">Hmm?</button><button onclick="cmd('sound:10')">Chuckle</button><button onclick="cmd('sound:8')">Snore</button></div>
</section>

<section><h2>Behaviour</h2>
 <div class="row"><label>Kid-friendly mode<small>Softer sounds, no scares, friendlier eyes, quieter</small></label><input type="checkbox" class="sw" data-k="kidMode"></div>
 <div class="row"><label>New eyes for each visitor<small>Random eye style every time he wakes up</small></label><input type="checkbox" class="sw" data-k="randomEyes"></div>
 <div class="row"><label>Transformations<small>Turns into dragon eyes if someone lingers</small></label><input type="checkbox" class="sw" data-k="transforms"></div>
 <div class="row"><label>Jump scares<small>Roars when someone rushes at him</small></label><input type="checkbox" class="sw" data-k="jumpScares"></div>
 <div class="row"><label>Antics<small>Winks, eye rolls, cross-eyes, nodding off</small></label><input type="checkbox" class="sw" data-k="antics"></div>
 <div class="row"><label>Snores while asleep</label><input type="checkbox" class="sw" data-k="sleepSounds"></div>
 <div class="row"><label>Power saver<small>Dims the eyes after 20 s asleep</small></label><input type="checkbox" class="sw" data-k="powerSaver"></div>
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

<section><h2>Firmware update</h2>
 <div class="row"><label>Installed<small id="fw">…</small></label></div>
 <div class="row"><label>Update file<small>creepereyes-update.bin, downloaded to this phone beforehand</small></label><input type="file" id="fwf" accept=".bin" style="max-width:150px"></div>
 <progress id="fwp" value="0" max="100" hidden></progress>
 <div class="btns" style="margin-top:8px"><button class="pri" onclick="doUpdate()">Install update</button></div>
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
 if(s.fw)document.getElementById('fw').textContent='Built '+s.fw;
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
const $1=id=>document.getElementById(id);
async function poll(){try{const s=await (await fetch('/api/status')).json();
 $1('st').textContent=s.state+(s.saving?' (power saver)':'');
 $1('rd').textContent=!s.radar?'not connected':s.person?(s.dist/1000).toFixed(2)+' m'+(s.people>1?' ('+s.people+' people)':''):'nobody';
 $1('vt').textContent=s.visitsTonight;$1('va').textContent=s.visitsTotal;
 $1('pup').classList.toggle('on',!!s.puppet)}catch(e){}
 setTimeout(poll,700)}
async function resetVisits(){await post('/api/visits/reset',{});toast('Reset')}
function pa(a){post('/api/puppet',{a});$1('pup').classList.add('on')}
(function(){const pad=$1('pad'),dot=$1('dot');let last=0,pend=null,busy=false;
 function sendXY(){if(busy||!pend)return;busy=true;const b=pend;pend=null;post('/api/puppet',b).finally(()=>{busy=false;setTimeout(sendXY,60)})}
 function mv(e){const r=pad.getBoundingClientRect();let fx=(e.clientX-r.left)/r.width,fy=(e.clientY-r.top)/r.height;
  fx=Math.min(1,Math.max(0,fx));fy=Math.min(1,Math.max(0,fy));dot.style.left=fx*100+'%';dot.style.top=fy*100+'%';
  pend={x:Math.round(fx*1023),y:Math.round((1-fy)*1023)};sendXY();$1('pup').classList.add('on')}
 pad.addEventListener('pointerdown',e=>{pad.setPointerCapture(e.pointerId);mv(e)});
 pad.addEventListener('pointermove',e=>{if(e.buttons||e.pointerType=='touch')mv(e)});})();
function doUpdate(){const f=$1('fwf').files[0];if(!f){toast('Pick the file first');return}
 if(!confirm('Install '+f.name+'? The eyes pause for about a minute, then he restarts.'))return;
 const fd=new FormData();fd.append('firmware',f,f.name);const x=new XMLHttpRequest(),p=$1('fwp');p.hidden=false;
 x.upload.onprogress=e=>{if(e.lengthComputable)p.value=e.loaded*100/e.total};
 x.onload=()=>{if(x.status==200){toast('Updated! Restarting…');setTimeout(()=>location.reload(),9000)}else{let m='Update failed';try{m=JSON.parse(x.responseText).error}catch(e){}alert(m);p.hidden=true}};
 x.onerror=()=>{alert('Connection lost during the update. Reconnect to his Wi-Fi and try again.');p.hidden=true};
 x.open('POST','/update');x.send(fd)}
fetch('/api/settings').then(r=>r.json()).then(fill);poll();
</script></body></html>)HTML";
