# Architecture

## Layer model

```
┌──────────────────────────────────────────────────────────────┐
│  PRESENTATION (UE5, module "OutbreakCommander")              │
│  Input (AOCPlayerController, Enhanced Input, controller)     │
│  Camera (AOCGodPawn: god-view zoom/rotate/pan/edge-scroll)   │
│  Actors (buildings, zombies, citizens, squads — pooled)     │
│  HUD / UI (UMG widgets bound to snapshots + event feed)      │
│  Audio (SFX + adaptive music), VFX (Niagara), Lumen/Nanite   │
├──────────────────────────────────────────────────────────────┤
│  BRIDGE (UOCSimulationBridge — thin UObject)                 │
│  owns oc::FSimulation; commands in, snapshots/events out     │
├──────────────────────────────────────────────────────────────┤
│  SIMULATION CORE (module "SimulationCore" — pure C++17)      │
│  deterministic, fixed-timestep, zero engine dependencies     │
│  compiled standalone for tests/CI + inside the game          │
├──────────────────────────────────────────────────────────────┤
│  DATA (Content/Data/*.csv — single source of truth)          │
│  Balance / Resources / Buildings / ZombieTypes / Soldiers    │
│  / Vehicles / TechTree / Items / Recipes / Scenarios         │
│  / Weather / Names / WorldCities                             │
└──────────────────────────────────────────────────────────────┘
```

## Why this split

1. **Testability**: the sim core compiles with a bare `g++` and runs **371
   unit checks in ~15s** (`make test`). Core systems (economy, infection,
   AI, tactics, tech, save/load, replays) are tested without an engine.
2. **Determinism**: all randomness flows through one seeded RNG
   (`oc::FRng`, xorshift64*). Same seed → identical outbreak. Foundation
   for replays, save/load verification, and lockstep multiplayer.
3. **Portability**: if the engine decision ever changes, the game logic
   moves with us (see `docs/00-engine-decision.md`).
4. **Performance**: the sim ticks at a fixed 0.25 s step, independent of
   frame rate. The UE layer only reads snapshots once per frame.

## Module map

| Path | Contents |
|---|---|
| `Source/SimulationCore/Public/oc/` | `SimTypes.h`, `DataTables.h`, `Simulation.h` — the whole public API |
| `Source/SimulationCore/Private/` | `DataTables.cpp` (CSV loader/validator), `Simulation.cpp` (lifecycle, world gen, commands, spawning), `SimulationStep.cpp` (per-tick systems), `SimulationMilitary.cpp` (military commands + strikes), `SimulationIO.cpp` (save/load + command replay), `SimulationReport.cpp` (after-action report) |
| `Source/OutbreakCommander/` | UE presentation: game mode/state/instance, bridge, god pawn, controller, actors, view manager |
| `Content/Data/` | CSV data tables (shared with UE DataTables) |
| `Content/Assets/`, `Content/Audio/`, `Content/Fonts/` | CC0/OFL assets (see `Content/Assets/ATTRIBUTION.md`) |
| `tests/` | Self-contained unit test runner (500+ checks) |
| `tools/` | `sim_cli.cpp` (full-game headless demo + report), `fetch_assets.py` (re-downloads all assets) |
| `docs/` | Design docs (engine, architecture, data, AI/tactics, roadmap) |

## Tick flow

```
UE frame (AOCGameMode::Tick, real dt)
  └─ UOCSimulationBridge::Tick(dt)
       └─ oc::FSimulation::Step(dt)          // fixed 0.25 s ticks
            └─ RunTick(dt)                   // one simulation tick
                 ├─ TickBuildings             // construction progress
                 ├─ UpdateDistrictNetworks    // power/water per district (union-find)
                 ├─ TickEconomy               // taxes, upkeep, production, cures, research
                 ├─ TickCitizens              // needs, infection, panic, movement
                 ├─ TickZombies               // AI: senses, behaviors, pack cohesion, memory, flanking
                 ├─ TickMilitary              // squads/vehicles: focus fire, tactics, combat
                 ├─ TickHordes                // escalating waves + adaptive difficulty
                 ├─ TickNoisePings            // bite/death-alert pings
                 ├─ TickWeather               // day/night + weather changes
                 ├─ TickVaccination           // vaccine campaign (endgame)
                 ├─ TickCrafting              // workshop jobs
                 ├─ TickInternational         // aid/trade cooldowns + world news
                 ├─ PendingStrikeTick         // airstrike/nuke delays
                 ├─ UpdateContamination       // ground contamination spread/decay
                 ├─ TickSeasons / TickCampaign / TickHints / TickDemoMode
                 ├─ TickMorale
                 ├─ UpdateStats
                 ├─ CheckGameResult           // win (eradication/containment/evac) / lose
                 └─ TickAchievements / ClampResourcesToStorage
       └─ broadcast new events + snapshots    // HUD/alerts
```

## System summary (design, data structures, testing, limitations)

| System | Data structures | Key classes/functions | Testing | Known limitations |
|---|---|---|---|---|
| Infection | `FCitizen` (state machine, latent timer), tile contamination | `TickCitizens`, `InfectCitizen`, `TurnCitizen`, `CureCitizen` | TestInfectionProgression, TestOutbreakSpreads | No citizen-to-citizen contact infection model beyond bites/tiles |
| Zombie AI | `FZombie` + `FNoisePing`, per-type `Behavior` | `TickZombies`, `TickNoisePings` | 9 TestZombie* tests | No pathfinding; pack cohesion O(n²) |
| Military | `FSquad`, `FVehicle`, orders, ammo | `TickMilitary`, `RecruitSquad`, `IssueOrder`, vehicles, airstrike/nuke | TestMilitary*, TestSquad*, TestAirstrike, TestNuke | One squad per vehicle; no cover system |
| Economy | `FResources`, buildings with jobs/housing | `TickEconomy` (taxes, upkeep, production) | TestEconomy | Income = min(workers, jobs) × tax × morale |
| Power/water | districts + `FNetworkLink` (power lines/pipes), union-find components | `UpdateDistrictNetworks` | TestDistrictNetworks, TestPowerGrid | Districts are 24×24 tiles; no per-building lines |
| Tech tree | `FTechDef`, researched set, modifiers, unlocks | `StartResearch`, `ApplyTechEffect` | TestTechTree, TestVaccineWin | One project at a time |
| Items/crafting | `FItemStack`, `FCraftJob`, recipes (items+resources) | `QueueRecipe`, `TickCrafting` | TestCrafting | No storage caps; durability is placeholder |
| Environment | weather table, day/night fraction | `TickWeather`, `ZombieSpeedScale` | TestDayNightAndWeather | Weather is global; no per-district weather |
| International | world cities (245 real capitals), cooldowns | `RequestAid`, `SendTradeCaravan`, `CallEvacuationFlight`, news | TestInternational | News is cosmetic ±morale |
| Scenarios/rules | `FScenarioDef`, `RuleOverrides` | `ApplyScenario`, NewGame | TestScenarios | Sandbox overrides are Balance keys only |
| Save/load | versioned binary snapshot | `SaveToFile`, `LoadFromFile` | TestSaveLoad | One save format version |
| Replay | command log (`FCommand`) + `ApplyCommand` | `LogCommand`, `ApplyCommand` | TestCommandLogReplay | Replay = same tick alignment |
| Endgame | vaccine campaign, evacuation, containment win | `TickVaccination`, `CheckGameResult` | TestVaccineWin, TestWinCondition | Containment requires ALL survivors immune |
| Report | generated markdown (headline, timeline, memorial) | `GenerateAfterActionReport` | TestAfterActionReport | Report is generated at game end only |

## Multiplayer readiness (Phase 3)

The core is already shaped for client-server play: an authoritative server
runs `FSimulation`; clients send *commands* (PlaceBuilding, BuildWallLine,
RecruitSquad, ...) and receive *snapshots + events*. Because the sim is
deterministic and seeded, the **command log** (`SetCommandLogEnabled` +
`ApplyCommand`) is a ready-made replay/lockstep mechanism (tested: same seed
+ same commands = identical sim). Save/load is the same serialization path.

## Coding standards

- Sim core: pure C++17, no UE includes, `oc::` namespace, `-Wall -Wextra` clean.
- UE layer: UE5 naming (A/U/F prefixes, `TObjectPtr`, `UFUNCTION`/`UPROPERTY`).
- All gameplay values in `Content/Data/*.csv` — **no magic numbers in code**.
- One system per file section; comments explain *why*, not *what*.
- The data loader validates everything at startup (required columns, unique
  ids, positive stats, known references, model files exist) — bad data fails
  the build in tests, never ships.
