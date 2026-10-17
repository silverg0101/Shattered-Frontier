export const SIZE = 32;
export function newWorld() {
  const tiles = [];
  for (let y = 0; y < SIZE; y++) for (let x = 0; x < SIZE; x++) {
    const n = (x * 73 + y * 137 + x * y * 17) % 101;
    tiles.push({ x, y, kind: x > 12 && x < 20 && y > 12 && y < 20 ? 'grass' : n < 7 ? 'water' : n < 32 ? 'tree' : n < 46 ? 'rock' : 'grass', owner: null });
  }
  return { tiles, players: {}, revision: 0 };
}
export function addPlayer(world, id, name) {
  world.players[id] = { id, name, x: 16, y: 16, inventory: { wood: 0, stone: 0, plank: 0, coin: 0 }, skills: { gathering: 0, carpentry: 0, building: 0 }, questDone: false };
}
export function act(world, id, command) {
  const p = world.players[id];
  if (!p) throw new Error('Session expired. Join again.');
  const tile = world.tiles.find(t => t.x === command.x && t.y === command.y);
  const near = tile && Math.abs(p.x - tile.x) + Math.abs(p.y - tile.y) <= 1;
  const spend = (item, amount) => {
    if (p.inventory[item] < amount) throw new Error(`Need ${amount} ${item}.`);
    p.inventory[item] -= amount;
  };
  let message;
  switch (command.type) {
    case 'move':
      if (!near || tile.kind === 'water' || tile.kind === 'wall' || tile.kind === 'tree' || tile.kind === 'rock') throw new Error('That path is blocked.');
      p.x = tile.x; p.y = tile.y; message = 'Exploring the Greenlands.'; break;
    case 'gather':
      if (!near || !['tree', 'rock'].includes(tile.kind)) throw new Error('Stand beside a tree or rock.');
      p.inventory[tile.kind === 'tree' ? 'wood' : 'stone'] += 3;
      tile.kind = 'grass'; p.skills.gathering++; message = 'Gathered 3 resources. Gathering +1.'; break;
    case 'craft':
      spend('wood', 2); p.inventory.plank++; p.skills.carpentry++; message = 'Crafted a plank. Carpentry +1.'; break;
    case 'build':
      if (!near || tile.kind !== 'grass' || world.players && Object.values(world.players).some(other => other.x === tile.x && other.y === tile.y)) throw new Error('Build on empty grass beside you.');
      spend('plank', 1); tile.kind = 'wall'; tile.owner = id; p.skills.building++; message = 'Placed a claimed timber wall. Building +1.'; break;
    case 'remove':
      if (!near || tile.kind !== 'wall' || tile.owner !== id) throw new Error('You can reclaim only your own adjacent wall.');
      tile.kind = 'grass'; tile.owner = null; p.inventory.plank++; message = 'Reclaimed wall.'; break;
    case 'dig':
      if (!near || tile.kind !== 'grass') throw new Error('Dig adjacent grass.');
      tile.kind = 'earth'; p.inventory.stone++; p.skills.gathering++; message = 'Terrain excavated. Found 1 stone.'; break;
    case 'trade':
      if (Math.abs(p.x - 16) + Math.abs(p.y - 16) > 2) throw new Error('Return to the settlement beacon to trade.');
      spend('stone', 3); p.inventory.coin += 2; message = 'Sold 3 stone for 2 coins.'; break;
    case 'quest':
      if (Math.abs(p.x - 16) + Math.abs(p.y - 16) > 2) throw new Error('Return to the settlement beacon.');
      if (p.questDone || p.skills.building < 3) throw new Error('Place three walls, then return for your reward.');
      p.questDone = true; p.inventory.coin += 10; message = 'Frontier Founder complete! Earned 10 coins.'; break;
    default: throw new Error('Unknown action.');
  }
  world.revision++;
  return message;
}
