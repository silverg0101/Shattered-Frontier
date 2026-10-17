Project Concept Summary
Working Title: Legends of the Shattered Frontier

A next-generation sandbox MMORPG that combines:

Ultima Online style player freedom, player-driven economy, housing, criminal systems, and sandbox progression.
World of Warcraft style questing, dungeon content, raids, faction conflicts, and accessible MMO gameplay.
Minecraft style voxel-inspired building, terrain modification, resource gathering, crafting, and environmental destruction.
Survival / Persistent World Games (Valheim, Rust, Enshrouded, Ark) style exploration, base building, territorial control, and world-changing events.

The world is fully persistent and exists even when players are offline.

The entire project is developed using AI agents (Claude, Codex, GPT, etc.) for:

Source code generation
Art creation
Sound effects
Music generation
Quest writing
Lore generation
NPC dialog
3D modeling
Animation
UI assets
Testing automation
Documentation

Human involvement becomes primarily project direction, validation, and refinement.

Core Vision

Create a living world where:

Every tree can be harvested.
Every mountain can be mined.
Buildings can be constructed almost anywhere.
Terrain can be modified.
Cities can be founded by players.
Entire economies emerge from player interaction.
Exploration continuously expands the available world.
Future content unlocks naturally through gameplay rather than requiring server wipes.

The game should feel like:

"What if Ultima Online's sandbox, WoW's polish and progression, and Minecraft's building systems existed in one persistent MMO world."

Initial World Design
Launch World
Four Biomes
1. Greenlands
Starter region
Farming
Early quests
Safe zones
NPC cities
2. Frozen North
Ice caves
Rare ores
Harsh weather
3. Desert Wastes
Ancient ruins
Dungeons
High-tier resources
4. Corrupted Frontier
Dangerous endgame region
PvP-enabled territories
Raid content
World Expansion Strategy

Instead of increasing map size immediately:

Exploration Gates

Players discover:

Ancient portals
Ocean routes
Airship routes
Underground tunnels

Each unlocks new lands.

Expansion Packs

Examples:

Jungle Continent
Volcanic Realm
Floating Isles
Underworld
Celestial Planes

This allows essentially endless expansion while maintaining performance.

Gameplay Pillars
1. Character Progression

Ultima-style skill advancement:

Rather than fixed classes:

Mining
Blacksmithing
Archery
Swordsmanship
Magic
Alchemy
Engineering
Farming
Cooking
Fishing
Trading


Players improve through use.

Examples:

Mine ore → increase mining skill
Craft swords → improve blacksmithing
Cast spells → improve magic

Players create unique builds.

2. Building System

Minecraft-inspired freedom but MMO-scale.

Build Modes
Personal homes
Villages
Guild settlements
Player cities
Fortresses
Kingdom capitals

Supports:

Terraforming
Roads
Bridges
Tunnels
Towers
Castles
3. Destructible Environments

The world is not static.

Examples:

Trees chopped down
Mountains mined
Buildings damaged
Siege warfare
Dynamic caves

The server continuously regenerates select resources while preserving player structures.

4. Economy

Primarily player-driven.

Players Produce
Weapons
Armor
Potions
Food
Building materials
Ships
Vehicles
NPCs Provide
Currency sinks
Quest rewards
Resource balancing

Goal:

80%+ of trade occurs between players.
5. PvE
Content
Dynamic events
World bosses
Dungeons
Raids
Expeditions

AI systems create procedurally generated:

Quests
Events
Encounters

Reducing content bottlenecks.

6. PvP

Optional by region.

Systems
Arena
Battlegrounds
Open-world conflict
Territory warfare
Guild wars
Siege mechanics

Safe zones remain available.

Technical Architecture
Recommended Engine
Unreal Engine 5

Best choice today because of:

Nanite
Lumen
Massive worlds
Dedicated server support
High-end graphics
Mature MMO ecosystem

Alternative:

Godot

Pros:

Open source
No royalties
AI-generated code friendlier

Cons:

Large-scale MMO tooling less mature

Recommendation: Unreal Engine 5

Backend Architecture
World Server Cluster

Components:

Gateway Servers
Authentication
Login
Session routing
Simulation Servers
World state
Physics
AI
Database Servers
Character data
Inventory
Housing
Economy
Proxy Layer
Load balancing
Regional routing
Persistence Requirements

Must save:

Terrain changes
Buildings
Guild structures
Character progression
Inventories
Economies

Potential storage:

PostgreSQL
Redis
Object Storage
AI Development Pipeline
Coding

Agents:

Claude
Codex
GPT

Used for:

Gameplay systems
Server systems
Tooling
Bug fixes
Art Generation

Tools:

Flux
Stable Diffusion
Ideogram
Midjourney (optional cost)

Used for:

Concepts
Texture generation
UI
3D Asset Creation

Tools:

Blender
TripoSR
Hunyuan3D
Meshy
Rodin

Generate:

Buildings
Characters
Props
Creatures
Animation

Tools:

Cascadeur
Mixamo
AI motion generators

Generate:

Combat animations
Movement cycles
Emotes
MMO Scale Targets
Phase 1

Single World

2,000 concurrent players
20,000 registered players
Phase 2

Regional Shards

10,000+ concurrent players
Multiple continents
Phase 3

Mega World

100,000+ active users
Dynamic world expansion
Estimated Infrastructure
Prototype

Monthly:

$50-$300

Includes:

Development servers
Databases
Build pipelines
Alpha

Monthly:

$500-$2,000

Includes:

Dedicated world servers
AI pipelines
Testing infrastructure
Live Service Launch

Monthly:

$5,000-$50,000+

Depends entirely on:

Player count
Geographic regions
Bandwidth
Persistence requirements
Initial Cost Summary
Free / Open Source
Blender
Godot
PostgreSQL
Redis
Kubernetes
Stable Diffusion ecosystem
One-Time Costs
Unreal Engine: Free to start
Marketplace assets: Optional
Domain name: ~$10-$20/year
Ongoing Costs
Hosting
Object storage
Build infrastructure
AI inference/GPU compute
MVP (First Playable Version)

The AI-agent-generated MVP should include:

One biome
Multiplayer support (100-200 players)
Resource gathering
Crafting
Building system
Terrain modification
Player housing
Skill progression
Basic economy
Persistent database

Once stable, expand to:

Four-biome world
Guild systems
Dungeons
Territories
Full MMO infrastructure

This staged approach dramatically reduces risk and gives the AI agent workflow a realistic chance of producing a playable MMO before attempting a WoW-scale launch.