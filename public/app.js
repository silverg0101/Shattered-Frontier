const $ = id => document.getElementById(id);
const canvas = $('world'), ctx = canvas.getContext('2d');
let me, state, mode = 'gather', busy = false;
const colors = { grass: '#416346', earth: '#8b7451', water: '#355e69', tree: '#38573d', rock: '#51634c', wall: '#846442' };
async function api(path, data) {
  const response = await fetch('/api/' + path, data ? { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(data) } : {});
  const body = await response.json(); if (!response.ok) throw new Error(body.error); return body;
}
function log(message) { $('log').textContent = message; }
function stats() {
  $('join').hidden = true; $('name').textContent = me.name;
  $('stats').replaceChildren();
  for (const [key, value] of Object.entries(me.inventory)) { const row = document.createElement('div'); row.className = 'row'; const label = document.createElement('span'); label.textContent = key[0].toUpperCase() + key.slice(1); const count = document.createElement('strong'); count.textContent = value; row.append(label, count); $('stats').append(row); }
  for (const [key, value] of Object.entries(me.skills)) { const row = document.createElement('div'); row.className = 'skill'; row.textContent = key[0].toUpperCase() + key.slice(1) + ' · ' + value; $('stats').append(row); }
  $('progress').style.width = Math.min(100, me.skills.building / 3 * 100) + '%';
  $('quest-status').textContent = me.questDone ? 'Complete · You helped found the frontier.' : Math.min(3, me.skills.building) + ' / 3 walls placed';
}
function draw() {
  if (!state) return;
  const s = 24; ctx.clearRect(0, 0, 768, 768);
  for (const t of state.tiles) {
    const x = t.x * s, y = t.y * s; ctx.fillStyle = colors[t.kind]; ctx.fillRect(x, y, s, s);
    if (t.kind === 'grass') { ctx.fillStyle = '#54744d'; ctx.fillRect(x + (t.x * 7 % 18), y + (t.y * 11 % 18), 2, 3); }
    if (t.kind === 'tree') { ctx.fillStyle = '#6b563c'; ctx.fillRect(x + 10, y + 12, 4, 11); ctx.fillStyle = '#213f30'; ctx.beginPath(); ctx.arc(x + 12, y + 9, 9, 0, Math.PI * 2); ctx.fill(); ctx.fillStyle = '#54754a'; ctx.beginPath(); ctx.arc(x + 10, y + 6, 5, 0, Math.PI * 2); ctx.fill(); }
    if (t.kind === 'rock') { ctx.fillStyle = '#9caa98'; ctx.beginPath(); ctx.moveTo(x+4,y+18);ctx.lineTo(x+8,y+6);ctx.lineTo(x+17,y+5);ctx.lineTo(x+22,y+17);ctx.closePath();ctx.fill(); }
    if (t.kind === 'wall') { ctx.strokeStyle = t.owner === me?.id ? '#dfcc8c' : '#b99471'; ctx.strokeRect(x+2,y+2,20,20);ctx.beginPath();ctx.moveTo(x+12,y+2);ctx.lineTo(x+12,y+22);ctx.stroke(); }
  }
  ctx.fillStyle = '#e1c777';ctx.beginPath();ctx.moveTo(396,386);ctx.lineTo(404,396);ctx.lineTo(396,406);ctx.lineTo(388,396);ctx.closePath();ctx.fill();
  for (const p of state.players) { ctx.fillStyle = p.id === me?.id ? '#f2e7bc' : '#82d5d6'; ctx.strokeStyle='#132b23';ctx.lineWidth=2;ctx.beginPath();ctx.arc(p.x*s+12,p.y*s+12,7,0,Math.PI*2);ctx.fill();ctx.stroke();ctx.font='11px Segoe UI';ctx.textAlign='center';ctx.fillStyle='#fff';ctx.fillText(p.name,p.x*s+12,p.y*s-3); }
}
async function action(type, x, y) {
  if (!me || busy) return;
  busy = true;
  try { const result = await api('action', { type, x, y }); me = result.player; stats(); draw(); log(result.message); } catch (e) { log(e.message); } finally { busy = false; }
}
function connect() {
  stats(); const events = new EventSource('/api/events');
  events.onmessage = event => { state = JSON.parse(event.data); $('connection').textContent = '● Connected · ' + state.players.length + ' settlers'; draw(); };
  events.onerror = () => { $('connection').textContent = 'Reconnecting…'; };
}
$('join').onsubmit = async event => { event.preventDefault(); try { me = await api('join', { name: $('player-name').value }); connect(); } catch (e) { log(e.message); } };
$('tools').onclick = event => { const button = event.target.closest('button'); if (!button) return; mode = button.dataset.mode; document.querySelectorAll('[data-mode]').forEach(b => b.classList.toggle('active', b === button)); };
canvas.onclick = event => { const r = canvas.getBoundingClientRect(); action(mode, Math.floor((event.clientX-r.left)/r.width*32), Math.floor((event.clientY-r.top)/r.height*32)); };
document.addEventListener('keydown', event => {
  if (['INPUT','TEXTAREA','BUTTON'].includes(document.activeElement.tagName) || !me) return;
  const directions = { w:[0,-1],ArrowUp:[0,-1],s:[0,1],ArrowDown:[0,1],a:[-1,0],ArrowLeft:[-1,0],d:[1,0],ArrowRight:[1,0] };
  if (directions[event.key]) { event.preventDefault(); const [x,y]=directions[event.key]; action('move',me.x+x,me.y+y); }
  if ('1234'.includes(event.key)) document.querySelectorAll('[data-mode]')[Number(event.key)-1]?.click();
});
for (const type of ['craft','trade','quest']) $(type).onclick = () => action(type);
try { me = await api('me'); connect(); } catch { $('connection').textContent = 'Ready to join'; }
