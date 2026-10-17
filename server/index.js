import http from 'node:http';
import { readFileSync } from 'node:fs';
import { mkdirSync } from 'node:fs';
import { DatabaseSync } from 'node:sqlite';
import { randomBytes, randomUUID } from 'node:crypto';
import { fileURLToPath } from 'node:url';
import { newWorld, addPlayer, act } from './world.js';

mkdirSync(new URL('../data/', import.meta.url), { recursive: true });
const db = new DatabaseSync(process.env.WORLD_DB || fileURLToPath(new URL('../data/world.sqlite', import.meta.url)));
db.exec('PRAGMA journal_mode=WAL; CREATE TABLE IF NOT EXISTS snapshot (id INTEGER PRIMARY KEY, body TEXT NOT NULL); CREATE TABLE IF NOT EXISTS sessions (token TEXT PRIMARY KEY, player TEXT NOT NULL)');
let world = JSON.parse(db.prepare('SELECT body FROM snapshot WHERE id=1').get()?.body || 'null') || newWorld();
const save = () => db.prepare('INSERT OR REPLACE INTO snapshot VALUES (1, ?)').run(JSON.stringify(world));
save();
const clients = new Set();
const publicState = () => JSON.stringify({ tiles: world.tiles, players: Object.values(world.players).map(({ id, name, x, y }) => ({ id, name, x, y })), revision: world.revision });
const broadcast = () => { const data = `data: ${publicState()}\n\n`; for (const c of clients) c.write(data); };
const files = { '/': ['index.html', 'text/html'], '/app.js': ['app.js', 'text/javascript'], '/style.css': ['style.css', 'text/css'] };
const server = http.createServer(async (req, res) => {
  const path = new URL(req.url, 'http://localhost').pathname;
  const json = (code, value) => { res.writeHead(code, { 'Content-Type': 'application/json' }); res.end(JSON.stringify(value)); };
  try {
    if (req.method === 'GET' && files[path]) {
      const [file, type] = files[path]; res.writeHead(200, { 'Content-Type': type }); res.end(readFileSync(new URL(`../public/${file}`, import.meta.url))); return;
    }
    const token = req.headers.cookie?.split('; ').find(v => v.startsWith('frontier='))?.slice(9);
    const id = token && db.prepare('SELECT player FROM sessions WHERE token=?').get(token)?.player;
    if (req.method === 'GET' && path === '/api/events') {
      if (!id) return json(401, { error: 'Join first.' });
      res.writeHead(200, { 'Content-Type': 'text/event-stream', 'Cache-Control': 'no-cache', Connection: 'keep-alive' });
      res.write(`data: ${publicState()}\n\n`); clients.add(res); req.on('close', () => clients.delete(res)); return;
    }
    if (req.method === 'GET' && path === '/api/me') return json(id ? 200 : 401, id ? world.players[id] : { error: 'Join first.' });
    if (req.method !== 'POST') return json(404, { error: 'Not found.' });
    let body = ''; for await (const chunk of req) { body += chunk; if (body.length > 4096) throw new Error('Request too large.'); }
    const command = JSON.parse(body);
    if (path === '/api/join') {
      if (id) return json(200, world.players[id]);
      const name = String(command.name || 'Wayfarer').trim().slice(0, 24) || 'Wayfarer';
      const playerId = randomUUID(); const session = randomBytes(32).toString('hex');
      addPlayer(world, playerId, name); save(); db.prepare('INSERT INTO sessions VALUES (?, ?)').run(session, playerId);
      res.setHeader('Set-Cookie', `frontier=${session}; HttpOnly; SameSite=Strict; Path=/; Max-Age=31536000`);
      broadcast(); return json(200, world.players[playerId]);
    }
    if (!id) return json(401, { error: 'Join first.' });
    if (path !== '/api/action') return json(404, { error: 'Not found.' });
    const message = act(world, id, command); save(); broadcast(); json(200, { player: world.players[id], message });
  } catch (error) { json(400, { error: error.message }); }
});
const heartbeat = setInterval(() => { for (const c of clients) c.write(': heartbeat\n\n'); }, 20000);
server.listen(Number(process.env.PORT || 3000), '127.0.0.1', () => console.log('Shattered Frontier: http://localhost:' + (process.env.PORT || 3000)));
process.on('SIGINT', () => { clearInterval(heartbeat); for (const c of clients) c.end(); server.close(() => db.close()); });
