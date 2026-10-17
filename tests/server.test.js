import test from 'node:test';
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { once } from 'node:events';

test('HTTP sessions, live world updates and SQLite survive a process restart', async () => {
  const dir = mkdtempSync(join(tmpdir(), 'frontier-test-'));
  const port = 18000 + Math.floor(Math.random() * 10000);
  const base = `http://localhost:${port}`;
  let child;
  async function start() {
    child = spawn(process.execPath, ['server/index.js'], { env: { ...process.env, PORT: String(port), WORLD_DB: join(dir, 'test.sqlite') }, stdio: ['ignore', 'pipe', 'pipe'] });
    await Promise.race([once(child.stdout, 'data'), once(child, 'exit').then(() => { throw new Error('Server exited before startup'); }), new Promise((_, reject) => setTimeout(() => reject(new Error('Startup timeout')), 5000).unref())]);
  }
  async function stop() { const exited = once(child, 'exit'); child.kill(); await exited; child = null; }
  try {
    await start();
    assert.equal((await fetch(base)).status, 200);
    const joined = await fetch(base + '/api/join', { method: 'POST', body: JSON.stringify({ name: 'Ada' }) });
    const cookie = joined.headers.get('set-cookie').split(';')[0]; const original = await joined.json();
    const second = await fetch(base + '/api/join', { method: 'POST', body: JSON.stringify({ name: 'Ben' }) });
    const secondCookie = second.headers.get('set-cookie').split(';')[0];
    const controller = new AbortController();
    const events = await fetch(base + '/api/events', { headers: { cookie: secondCookie }, signal: controller.signal });
    const reader = events.body.getReader(); await reader.read();
    const dig = await fetch(base + '/api/action', { method: 'POST', headers: { cookie }, body: JSON.stringify({ type: 'dig', x: 17, y: 16 }) });
    assert.equal((await dig.json()).player.inventory.stone, 1);
    const update = new TextDecoder().decode((await reader.read()).value);
    assert.match(update, /"kind":"earth"/); controller.abort();
    await stop(); await start();
    const resumed = await (await fetch(base + '/api/me', { headers: { cookie } })).json();
    assert.equal(resumed.id, original.id); assert.equal(resumed.inventory.stone, 1);
    const restored = await fetch(base + '/api/events', { headers: { cookie } });
    const restoredReader = restored.body.getReader(); const saved = new TextDecoder().decode((await restoredReader.read()).value);
    assert.match(saved, /"x":17,"y":16,"kind":"earth"/); await restoredReader.cancel();
  } finally { if (child) await stop(); rmSync(dir, { recursive: true, force: true }); }
});
