import test from 'node:test';
import assert from 'node:assert/strict';
import { newWorld, addPlayer, act } from '../server/world.js';

function fixture() { const w = newWorld(); addPlayer(w, 'a', 'Ada'); addPlayer(w, 'b', 'Ben'); return w; }
test('gather, craft, build and reclaim conserve materials and enforce ownership', () => {
  const w = fixture(), p = w.players.a;
  const tile = w.tiles.find(t => t.x === 17 && t.y === 16); tile.kind = 'tree';
  act(w, 'a', { type: 'gather', x: 17, y: 16 });
  act(w, 'a', { type: 'craft' });
  act(w, 'a', { type: 'build', x: 17, y: 16 });
  assert.equal(tile.owner, 'a'); assert.equal(p.inventory.wood, 1); assert.equal(p.inventory.plank, 0);
  assert.throws(() => act(w, 'b', { type: 'remove', x: 17, y: 16 }), /own/);
  act(w, 'a', { type: 'remove', x: 17, y: 16 });
  assert.equal(p.inventory.plank, 1); assert.equal(tile.kind, 'grass');
});
test('invalid actions cannot grant resources or change world revision', () => {
  const w = fixture(), before = JSON.stringify(w);
  for (const command of [{ type: 'craft' }, { type: 'move', x: 25, y: 25 }, { type: 'gather', x: 0, y: 0 }, { type: 'quest' }, { type: 'build', x: 17, y: 16 }]) assert.throws(() => act(w, 'a', command));
  assert.equal(JSON.stringify(w), before);
});
test('two players cannot harvest the same resource twice', () => {
  const w = fixture(); w.tiles.find(t => t.x === 17 && t.y === 16).kind = 'rock';
  act(w, 'a', { type: 'gather', x: 17, y: 16 });
  assert.throws(() => act(w, 'b', { type: 'gather', x: 17, y: 16 }));
  assert.equal(w.players.a.inventory.stone + w.players.b.inventory.stone, 3);
});
test('quest reward can only be claimed once and trading requires the beacon', () => {
  const w = fixture(), p = w.players.a; p.skills.building = 3;
  act(w, 'a', { type: 'quest' }); assert.throws(() => act(w, 'a', { type: 'quest' }));
  assert.equal(p.inventory.coin, 10); p.x = 0; p.y = 0; p.inventory.stone = 3;
  assert.throws(() => act(w, 'a', { type: 'trade' }), /beacon/); assert.equal(p.inventory.stone, 3);
});
