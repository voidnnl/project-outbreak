# Outbreak Commander

A 3D city-management and zombie-survival game seen from a god-like elevated view. You don't fight every zombie — you command the city: its resources, military, and infrastructure — to contain a virus and save humanity.

- **Engine**: Unreal Engine 5 (see `docs/00-engine-decision.md`)
- **Architecture**: engine-agnostic deterministic simulation core (pure C++17) + thin UE5 presentation layer (see `docs/01-architecture.md`)
- **Zombie AI & military tactics**: behavior archetypes, pack cohesion, flanking, focus fire, self-preservation (see `docs/04-zombie-ai-and-tactics.md`)
- **Data**: all gameplay values in editable CSVs under `Content/Data/` (see `docs/02-data-files.md`)
- **Roadmap**: phased development plan (see `docs/03-roadmap.md`)

## Quick start (no Unreal Engine needed)

The simulation core compiles and runs with a bare C++17 compiler:

```bash
make test   # build + run the unit test suite (500+ checks)
make demo   # play a full scripted 120-day game headless; prints the
            # procedural AFTER-ACTION REPORT (also saved to build/AfterActionReport.md)
make play   # playable god-view in the browser (binds 0.0.0.0:8080, real sim core)
```

## What's in the game (simulation-complete)

- **Infection**: bites, contaminated ground, incubation, corpses rising; citizens have needs, panic, morale, and names
- **Zombie AI** (5 types, each with a behavior archetype and its own 3D model): walkers hunt in packs, runners strike and retreat, tanks push walls, spitters snipe from range, mutants lead the horde. Hordes have pack cohesion, target memory, noise attraction (gunfire, bites, death alerts), and wall-slide flanking
- **Military**: squads (militia, soldiers, spec ops, medics, engineers), vehicles (truck, APC, tank, helicopter, drone), focus fire, ammo discipline, auto-retreat + regeneration, medic triage, vehicle kiting, airstrikes, and the nuclear option (with permanent radiation)
- **Economy**: money, food, water, power, fuel, materials, medicine, ammo, research; taxes, upkeep, production, shortages
- **Infrastructure**: drag-to-build walls; district power grids and water pipes (build/cut lines; isolated districts go dark/dry)
- **Tech tree**: 15 technologies across medical/military/engineering/agriculture/communications branches; items + crafting at workshops
- **Environment**: day/night cycle and weather (clear/rain/fog/storm/snow) that scale zombie activity, solar power, and morale
- **Emergency policies**: curfew, quarantine, martial law
- **International**: aid packages, trade caravans, evacuation flights, world news (245 real capital cities)
- **Scenarios & custom rules**: 6 presets + per-game rule overrides (difficulty)
- **Endgame**: research the vaccine, vaccinate the city, win by eradication, containment, or evacuation
- **Campaign**: tutorial (First Light) + story missions (Patient Zero, Virulent Night, Blackout) with objectives, hints, and a 30-minute demo slice
- **Co-op roles** (commander / engineer / medic / logistics) and **versus** virus commands (infect, contaminate, boost, mutate)
- **Seasons**, building upgrades, wall gates/corners, quarantine zones, achievements, localization (EN/DE/FA/AR)
- **Save/load** (versioned binary snapshots, v2) and **command-log replay** (the deterministic multiplayer foundation)
- **Steam**: achievements, cloud saves, rich presence, lobbies, Steam Input, leaderboards (AppID 480 until assigned)
- **After-action report**: when the game ends, the sim writes a newspaper-style report — generated headline, situation summary, key figures, timeline of decisive events, notable citizens (Patient Zero, first to turn, first cured), the top squad, and a memorial for the fallen

## Repository layout

```
OutbreakCommander.uproject   UE5 project file
Config/                      Engine/game/input config (Lumen, Nanite, maps, key bindings)
Content/Data/                CSV data tables (Balance, Resources, Buildings, ZombieTypes,
                             Soldiers, Vehicles, TechTree, Items, Recipes, Scenarios,
                             Weather, Names, WorldCities, plus Campaign, Objectives, Hints,
                             Achievements, Seasons, CitizenTypes, CoopRoles, Localization,
                             Difficulty, InputBindings)
Content/Assets/              CC0 3D models (monsters, characters, props) + ATTRIBUTION.md
Content/Audio/               CC0 SFX (groans, gunfire, footsteps) + adaptive music
Content/Fonts/               OFL fonts (Chakra Petch HUD, Orbitron title)
Source/SimulationCore/       Pure C++17 game simulation (deterministic, unit-tested)
Source/OutbreakCommander/    UE5 presentation layer (camera, input, actors, HUD bridge)
tests/                       Unit tests for the sim core (self-contained runner, 500+ checks)
Content/Web/                 Canvas god-view served by `make play` (no Unreal required)
tools/                       sim_cli.cpp (full-game demo), sim_server.cpp (god-view HTTP), fetch_assets.py, steampipe/
docs/                        Engine, architecture, data, AI, roadmap, runbook, Steam, editor checklist, legal
store/                       Steam capsules, hero, screenshots, store copy, trailer storyboard
```

## Opening in Unreal Engine

1. Install Unreal Engine 5.4 (Epic Games Launcher).
2. Right-click `OutbreakCommander.uproject` → "Generate Visual Studio project files" (or double-click to open).
3. Create the referenced content assets (input actions/mapping context under `Content/Input`, the `MAP_PrototypeCity` level, `BP_OCGameMode`, widget blueprints, and the mesh assignments for the zombie/citizen models listed in `ZombieTypes.csv`) — these are `.uasset` files and are created in the editor.
4. The game mode loads `Content/Data` CSVs at runtime and starts a new simulation; the view manager spawns actors to match (zombie actors pick their model from the type's `Model` column).

## Design pillars

1. **Emergent outbreaks** — the virus spreads through bites, contamination, panic, and infrastructure failures; every playthrough tells a different story.
2. **Command, don't grind** — you manage systems (power, water, food, medicine, military), not individual zombies.
3. **Player freedom** — build any layout, try any strategy; all values are data-tunable.
4. **Testable core** — game logic runs headless with a fixed timestep and a seeded RNG, so it's unit-testable and replayable.

## License

Proprietary — see `Config/DefaultGame.ini` licensing fields. All bundled third-party assets are CC0/OFL; see `Content/Assets/ATTRIBUTION.md`. Legal review is required before release (brief §15).
