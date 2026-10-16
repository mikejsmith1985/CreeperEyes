// The settings page served at http://192.168.4.1/ . Fully self-contained:
// phones on the monster's Wi-Fi have no internet, so no external fonts or scripts.
#include <Arduino.h>

extern const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes"><meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent"><meta name="apple-mobile-web-app-title" content="Creeper">
<meta name="theme-color" content="#0e120f"><link rel="apple-touch-icon" href="/apple-touch-icon.png">
<title>CreeperEyes</title>
<style>
:root{--bg:#0e120f;--card:#171d18;--line:#2a332b;--text:#e6ece4;--mute:#93a08f;--acc:#8fd16a;--warn:#e0a84a}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:16px/1.4 system-ui,-apple-system,Segoe UI,Roboto,sans-serif}
header{padding:calc(20px + env(safe-area-inset-top)) 16px 8px}h1{margin:0;font-size:22px;letter-spacing:.5px}h1 span{color:var(--acc)}
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
button:active{transform:scale(.93)}button.busy{opacity:.6}button.ok{background:#3f6b2a!important;border-color:var(--acc)!important;color:#fff!important}button.err{background:#7a2a22!important;border-color:#e05a4a!important}
button.pri{background:var(--acc);color:#10200a;border-color:var(--acc);font-weight:600}button.warn{color:var(--warn)}
#status{display:grid;grid-template-columns:1fr 1fr;gap:8px}.stat{background:#0b0f0c;border-radius:10px;padding:8px 10px}.stat b{display:block;font-size:18px}
.stat span{color:var(--mute);font-size:12px}#toast{position:fixed;z-index:9;left:50%;bottom:calc(18px + env(safe-area-inset-bottom));transform:translateX(-50%);background:var(--acc);color:#10200a;padding:8px 14px;border-radius:20px;font-weight:600;opacity:0;transition:.3s}
#toast.on{opacity:1}
.note{color:var(--mute);font-size:12px;margin:6px 0}
.srow{display:flex;gap:6px;align-items:center;margin:6px 0}.srow input{min-width:0}.srow .nm{flex:1}.srow .tn{width:64px}
.srow button{padding:8px 10px}.rrow{display:flex;gap:8px;align-items:center;margin:6px 0}.rrow label{flex:1;font-size:14px}.rrow input{width:130px}
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
  <div class="stat" style="grid-column:1/3"><span>What the radar sees (top view, bust at the bottom; green dot = where he's looking)</span><canvas id="rv" width="600" height="300" style="width:100%;margin-top:6px;border-radius:8px;background:#0b0f0c"></canvas></div>
  <div class="stat"><span>Visitors tonight</span><b id="vt">…</b></div><div class="stat"><span>All-time</span><b id="va">…</b></div></div>
 <div class="btns" style="margin-top:10px"><button onclick="cmd(this,'wake')">Wake up</button><button onclick="cmd(this,'sleep')">Go to sleep</button><button onclick="resetVisits(this)">Reset tonight</button></div>
</section>

<section class="pp" id="pup"><h2>Puppet mode</h2>
 <div id="pnote">Drag on the pad to steer his eyes. He goes back to following people 30 s after your last touch, or when you tap Release. The other buttons play straight away without stopping him following people.</div>
 <div id="pad"><div id="dot"></div></div>
 <div class="btns">
  <button onclick="pa(this,1)">Wink L</button><button onclick="pa(this,2)">Wink R</button><button onclick="pa(this,3)">Slow blink</button><button onclick="pa(this,4)">Eyes shut/open</button>
  <button onclick="pa(this,5)">Cross-eyed</button><button onclick="pa(this,6)">Eye roll</button><button onclick="pa(this,7)">Look around</button>
 </div>
 <div class="btns" style="margin-top:8px">
  <span class="sndbtns" style="display:contents"></span>
  <button class="pri" onclick="pa(this,20)">Release</button>
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
 <div class="btns sndbtns"></div>
 <div class="btns" style="margin-top:10px"><button onclick="toggleEd()" id="edbtn">Edit sounds</button></div>
 <div id="sed" hidden>
  <p class="note" id="sfiles"></p>
  <h3 class="eyett">Buttons</h3>
  <div id="srows"></div>
  <div class="btns" style="margin:8px 0 14px"><button onclick="addRow()">+ Add button</button></div>
  <h3 class="eyett">What he plays on his own</h3>
  <p class="note">File numbers separated by commas; he picks one at random. Leave a box empty to keep him quiet for that moment.</p>
  <div id="sroles"></div>
  <div class="btns" style="margin-top:10px"><button class="pri" onclick="saveSounds(this)">Save sounds</button><button class="warn" onclick="resetSounds(this)">Back to the original sounds</button></div>
 </div>
</section>

<section><h2>Behaviour</h2>
 <div class="row"><label>Kid-friendly mode<small>Softer sounds, no scares, friendlier eyes, quieter</small></label><input type="checkbox" class="sw" data-k="kidMode"></div>
 <div class="row"><label>New eyes for each visitor<small>Random eye style every time he wakes up</small></label><input type="checkbox" class="sw" data-k="randomEyes"></div>
 <div class="row"><label>Transformations<small>Turns into dragon eyes if someone lingers</small></label><input type="checkbox" class="sw" data-k="transforms"></div>
 <div class="row"><label>Jump scares<small>Roars when someone rushes at him</small></label><input type="checkbox" class="sw" data-k="jumpScares"></div>
 <div class="row"><label>Antics<small>Winks, eye rolls, cross-eyes, nodding off</small></label><input type="checkbox" class="sw" data-k="antics"></div>
 <div class="row"><label>Snores while asleep</label><input type="checkbox" class="sw" data-k="sleepSounds"></div>
 <div class="row"><label>Power saver<small>Dims the eyes after 20 s asleep</small></label><input type="checkbox" class="sw" data-k="powerSaver"></div>
 <div class="row"><label>Follow strength<small>How hard his eyes swing towards people. 1 = realistic, 5 = cartoon</small></label><input type="range" min="1" max="5" step="1" data-k="followGain" data-u="x"><output></output></div>
 <div class="row"><label>Detection range<small>Ignores anyone further away (2 m suits the back of a car)</small></label><input type="range" min="500" max="5000" step="100" data-k="rangeMm" data-u="m"><output></output></div>
 <div class="row"><label>Wakes up within</label><input type="range" min="300" max="3000" step="100" data-k="wakeDistMm" data-u="m"><output></output></div>
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
 <div class="row"><label>Update file<small id="fwfile">creepereyes-v….bin, downloaded to this phone beforehand</small></label><input type="file" id="fwf" accept=".bin" style="max-width:150px" onchange="pickFw()"></div>
 <progress id="fwp" value="0" max="100" hidden></progress>
 <div id="fwmsg" style="margin-top:8px;font-weight:600"></div>
 <div class="btns" style="margin-top:8px"><button class="pri" onclick="doUpdate()">Install update</button></div>
</section>
</main>
<div id="toast">Saved</div>
<script>
const $=s=>document.querySelectorAll(s);let t,S={};
function toast(m){const e=document.getElementById('toast');e.textContent=m||'Saved';e.classList.add('on');clearTimeout(t);t=setTimeout(()=>e.classList.remove('on'),1400)}
function fmt(el){const u=el.dataset.u,v=+el.value;return u=='m'?(v/1000).toFixed(v<1000?2:1)+' m':u=='s'?v+' s':u=='x'?'×'+v:v}
function key(el){const b=el.closest('.eyebox');return el.dataset.k+(b?b.dataset.eye:'')}
function fill(s){S=s;
 if(s.styles)$('select.styles').forEach(sel=>{if(sel.options.length!=s.styles.length)sel.innerHTML=s.styles.map((n,i)=>'<option value="'+i+'">'+n+'</option>').join('')});
 $('[data-k]').forEach(el=>{const v=s[key(el)];if(v===undefined)return;
  if(el.type=='checkbox')el.checked=!!v;else el.value=v;const o=el.parentElement.querySelector('output');if(o)o.textContent=fmt(el)});
 document.body.classList.toggle('linked',!!s.linkEyes);
 if(s.fw)document.getElementById('fw').textContent=fwText(s);
 if(s.ssid)document.getElementById('net').textContent='Connected to '+s.ssid+' · changes apply instantly and save automatically.'}
async function post(u,b){const r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(b)});return r.json()}
const timers={};
function val(el){return el.type=='checkbox'?(el.checked?1:0):el.value}
function send(el){const k=key(el),v=val(el),b={[k]:v};
 if(el.closest('.eyebox')&&S.linkEyes){b[el.dataset.k+'L']=v;b[el.dataset.k+'R']=v}
 if(k=='linkEyes'&&v==1)['style','irisMode','irisColor','eyelidColor'].forEach(n=>b[n+'R']=S[n+'L']);
 clearTimeout(timers[k]);timers[k]=setTimeout(async()=>{fill(await post('/api/set',b));toast()},150)}
$('[data-k]').forEach(el=>{const h=()=>{const o=el.parentElement.querySelector('output');if(o)o.textContent=fmt(el);send(el)};el.addEventListener('input',h);if(el.tagName=='SELECT'||el.type=='checkbox'||el.type=='color')el.addEventListener('change',h)});
// Every button lights green when the monster confirms it, red if it didn't get through.
async function act(b,url,body,msg){if(b){b.classList.remove('ok','err');b.classList.add('busy')}
 try{const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(body)});
  if(!r.ok){let m='Not received \u2013 check Wi-Fi';try{m=(await r.json()).error||m}catch(e){}throw m}const j=await r.json();if(b){b.classList.remove('busy');b.classList.add('ok');setTimeout(()=>b.classList.remove('ok'),900)}
  toast((msg||(b?b.textContent:'OK'))+' \u2713');return j}
 catch(e){if(b){b.classList.remove('busy');b.classList.add('err');setTimeout(()=>b.classList.remove('err'),1500)}toast(typeof e=='string'?e:'Not received \u2013 check Wi-Fi');}}
function cmd(b,c){return act(b,'/api/cmd',{c})}
async function setPw(){const p=document.getElementById('pw').value;if(p.length<8){toast('8+ characters');return}
 await post('/api/set',{wifiPass:p});toast('Password saved');document.getElementById('pw').value=''}
async function resetAll(){if(!confirm('Reset all settings (except Wi-Fi password)?'))return;fill(await post('/api/reset',{}));toast('Reset')}
const $1=id=>document.getElementById(id);
// ---------- editable sounds ----------
let SC=null;const ROLES=[['wake','Waking up'],['growl','Random growls'],['lean','Someone leans in'],['roar','Jump scare / transformation'],['snore','Snoring while asleep'],['hmm','Friendly "hmm?" (kid mode)'],['chuckle','Chuckle (loses staring contest)']];
const esc=t=>String(t).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
function drawSoundButtons(){document.querySelectorAll('.sndbtns').forEach(d=>{d.innerHTML=SC.buttons.map(b=>'<button onclick="cmd(this,\'sound:'+b.t+'\')">'+esc(b.n)+'</button>').join('')})}
function drawEditor(){$1('sfiles').textContent=SC.files?('The card has '+SC.files+' files: number '+'1 = 0001.mp3, 2 = 0002.mp3 and so on (folder "mp3").'):'File 1 = 0001.mp3, 2 = 0002.mp3 and so on, in the card\'s "mp3" folder.';
 $1('srows').innerHTML=SC.buttons.map((b,i)=>'<div class="srow"><input class="nm" maxlength="19" value="'+esc(b.n)+'" oninput="SC.buttons['+i+'].n=this.value" placeholder="Button name"><input class="tn" type="number" min="1" max="2999" value="'+b.t+'" oninput="SC.buttons['+i+'].t=+this.value"><button onclick="cmd(this,\'sound:\'+SC.buttons['+i+'].t)">\u25B6</button><button class="warn" onclick="SC.buttons.splice('+i+',1);drawEditor()">\u2715</button></div>').join('');
 $1('sroles').innerHTML=ROLES.map(([k,l])=>'<div class="rrow"><label>'+l+'</label><input inputmode="numeric" value="'+(SC.roles[k]||[]).join(', ')+'" oninput="SC.roles[\''+k+'\']=this.value.split(/[^0-9]+/).filter(x=>x).map(Number)"></div>').join('')}
function addRow(){if(SC.buttons.length>=16){toast('Up to 16 buttons');return}const t=(SC.buttons.reduce((m,b)=>Math.max(m,b.t),0)||0)+1;SC.buttons.push({n:'New sound',t});drawEditor()}
function toggleEd(){const e=$1('sed');e.hidden=!e.hidden;$1('edbtn').textContent=e.hidden?'Edit sounds':'Close editor';if(!e.hidden)drawEditor()}
async function loadSounds(){try{SC=await (await fetch('/api/sounds')).json();drawSoundButtons()}catch(e){}}
async function saveSounds(b){const j=await act(b,'/api/sounds',{cfg:JSON.stringify({buttons:SC.buttons,roles:SC.roles})},'Sounds saved');if(j){SC=j;drawSoundButtons();drawEditor()}}
async function resetSounds(b){if(!confirm('Put the original buttons and sounds back?'))return;const j=await act(b,'/api/sounds/reset',{},'Original sounds restored');if(j){SC=j;drawSoundButtons();drawEditor()}}
async function poll(){try{const s=await (await fetch('/api/status')).json();
 $1('st').textContent=s.state+(s.saving?' (power saver)':'');
 $1('rd').textContent=!s.radar?'not connected':s.person?(s.dist/1000).toFixed(2)+' m'+(s.people>1?' ('+s.people+' people)':''):'nobody';
 $1('vt').textContent=s.visitsTonight;drawRadar(s);$1('va').textContent=s.visitsTotal;
 $1('pup').classList.toggle('on',!!s.puppet)}catch(e){}
 setTimeout(poll,700)}
function drawRadar(s){const c=$1('rv'),g=c.getContext('2d'),W=c.width,H=c.height,R=Math.max(1500,(S.rangeMm||2000)+500),sc=H/R;g.clearRect(0,0,W,H);
 g.strokeStyle='#2a332b';g.fillStyle='#93a08f';g.font='20px system-ui';
 for(let m=0.5;m*1000<R;m+=0.5){g.beginPath();g.arc(W/2,H,m*1000*sc,Math.PI,2*Math.PI);g.stroke();g.fillText(m+' m',W/2+4,H-m*1000*sc+20)}
 g.strokeStyle='#8fd16a';g.setLineDash([8,6]);g.beginPath();g.arc(W/2,H,(S.rangeMm||2000)*sc,Math.PI,2*Math.PI);g.stroke();g.setLineDash([]);
 g.strokeStyle='#3d5a32';g.beginPath();g.moveTo(W/2-1200*sc,H);g.lineTo(W/2-1200*sc,0);g.moveTo(W/2+1200*sc,H);g.lineTo(W/2+1200*sc,0);g.stroke();
 if(!s.radar){g.fillStyle='#e0a84a';g.fillText('radar not connected',W/2-90,H/2);return}
 (s.t||[]).forEach(p=>{g.fillStyle=p[2]?'#e6ece4':'#555';g.beginPath();g.arc(W/2+p[0]*sc,H-p[1]*sc,11,0,7);g.fill()});
 const a=(s.gx-512)/470*0.79*(S.flipGazeX?-1:1),L=H*0.9,ex=W/2+Math.sin(a)*L,ey=H-Math.cos(a)*L;
 g.strokeStyle='#8fd16a';g.lineWidth=3;g.beginPath();g.moveTo(W/2,H);g.lineTo(ex,ey);g.stroke();g.lineWidth=1;
 g.fillStyle='#8fd16a';g.beginPath();g.arc(ex,ey,8,0,7);g.fill()}
function resetVisits(b){return act(b,'/api/visits/reset',{},'Visitors reset')}
function pa(b,a){$1('pup').classList.add('on');return act(b,'/api/puppet',{a})}
(function(){const pad=$1('pad'),dot=$1('dot');let last=0,pend=null,busy=false;
 function sendXY(){if(busy||!pend)return;busy=true;const b=pend;pend=null;post('/api/puppet',b).finally(()=>{busy=false;setTimeout(sendXY,60)})}
 function mv(e){const r=pad.getBoundingClientRect();let fx=(e.clientX-r.left)/r.width,fy=(e.clientY-r.top)/r.height;
  fx=Math.min(1,Math.max(0,fx));fy=Math.min(1,Math.max(0,fy));dot.style.left=fx*100+'%';dot.style.top=fy*100+'%';
  pend={x:Math.round(fx*1023),y:Math.round((1-fy)*1023)};sendXY();$1('pup').classList.add('on')}
 pad.addEventListener('pointerdown',e=>{pad.setPointerCapture(e.pointerId);mv(e)});
 pad.addEventListener('pointermove',e=>{if(e.buttons||e.pointerType=='touch')mv(e)});})();
function fwText(s){if(!s.fwEpoch)return 'Built '+s.fw;const d=new Date(s.fwEpoch*1000);
 return 'v'+s.fw+' · built '+d.toLocaleDateString([], {month:'short',day:'numeric'})+' '+d.toLocaleTimeString([], {hour:'numeric',minute:'2-digit'})}
let fileVer=null;
function pickFw(){const f=$1('fwf').files[0];fileVer=null;if(!f)return;const r=new FileReader();
 r.onload=()=>{const b=new Uint8Array(r.result),tag=[67,69,86,69,82,58];let v=null;
  for(let i=0;i<b.length-6&&!v;i++){let k=0;while(k<6&&b[i+k]==tag[k])k++;if(k==6){let j=i+6,t='';while(j<b.length&&b[j]!=59&&t.length<20)t+=String.fromCharCode(b[j++]);v=t}}
  fileVer=v;const m=$1('fwmsg');
  if(b[0]!=0xE9){m.style.color='#e07a5f';m.textContent='That file is not CreeperEyes firmware.'}
  else if(!v){m.style.color='#e0b15f';m.textContent='This file has no version stamp (older build).'}
  else if(v==S.fw){m.style.color='#e0b15f';m.textContent='This file is v'+v+' – the same version he already has.'}
  else{m.style.color='#8fd16a';m.textContent='This file is v'+v+'. He has '+(/^[0-9]/.test(S.fw||'')?'v'+S.fw:'an older unversioned build')+'. Tap Install update.'}};
 r.readAsArrayBuffer(f)}
function doUpdate(){const f=$1('fwf').files[0];if(!f){toast('Pick the file first');return}
 const from=S.fw,m=$1('fwmsg'),fromTxt=/^[0-9]/.test(from||'')?'v'+from:'the old build';
 if(!confirm('Install '+(fileVer?'v'+fileVer:f.name)+'? The eyes pause for about a minute, then he restarts.'))return;
 const fd=new FormData();fd.append('firmware',f,f.name);const x=new XMLHttpRequest(),p=$1('fwp');p.hidden=false;
 m.style.color='';m.textContent='Uploading…';
 x.upload.onprogress=e=>{if(e.lengthComputable)p.value=e.loaded*100/e.total};
 x.onload=()=>{if(x.status!=200){let e='Update failed';try{e=JSON.parse(x.responseText).error}catch(_){}m.style.color='#e07a5f';m.textContent=e;p.hidden=true;return}
  m.textContent='Written. Waiting for him to restart…';const t0=Date.now();
  const check=async()=>{try{const s=await (await fetch('/api/settings',{cache:'no-store'})).json();
    if(s.fw&&s.fw!==from||(fileVer&&s.fw===fileVer)){fill(s);p.hidden=true;
     if(!fileVer||s.fw===fileVer){m.style.color='#8fd16a';m.textContent='✓ Updated: '+fromTxt+' → v'+s.fw}
     else{m.style.color='#e0b15f';m.textContent='He restarted on v'+s.fw+' (the file said v'+fileVer+').'}
     return}}catch(_){}
   if(Date.now()-t0>90000){p.hidden=true;m.style.color='#e07a5f';m.textContent='✗ He is still on '+fromTxt+' after 90 s. Reconnect to his Wi-Fi, reload this page and check the version.';return}
   setTimeout(check,2000)};
  setTimeout(check,4000)};
 x.onerror=()=>{m.style.color='#e07a5f';m.textContent='Connection lost during the upload. Reconnect to his Wi-Fi and try again.';p.hidden=true};
 x.open('POST','/update');x.send(fd)}
fetch('/api/settings').then(r=>r.json()).then(fill);loadSounds();poll();
</script></body></html>)HTML";
