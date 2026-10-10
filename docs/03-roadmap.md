# Development Roadmap

Phases follow brief §16. Status: **simulation-complete through Phase 5**;
UE `.uasset` content is a one-pass in-editor job (`docs/08-editor-checklist.md`).

## Phase 1 — Prototype ✅

**Goal**: prove the core loop — outbreak starts, spreads, and can be contained.

- [x] Engine decision + architecture (`docs/00`, `docs/01`)
- [x] Engine-agnostic, deterministic simulation core (pure C++17)
- [x] One procedural city (road grid, districts, housing)
- [x] Infection spread: bites, contaminated ground, incubation, corpses rising
- [x] Citizens with needs, panic, morale, names
- [x] Walls: drag-to-build lines, construction time, zombie breaching
- [x] Power plant + district power/water networks, medical center, farm, water purification
- [x] Economy loop: taxes, upkeep, production, shortages
- [x] Win/lose conditions from data
- [x] UE5 project scaffold: modules, configs, bridge, camera, controller, actors, view manager
- [x] Data-driven CSVs + 371-check unit test suite + headless demo + after-action report

## Phase 2 — Vertical slice ✅ (simulation) / 🟡 (UE content)

**Goal**: one polished district where a new player can play a full session.

Sim-complete:
- [x] **Zombie AI**: 5 types with behavior archetypes (chase / hit-and-run /
  push-ahead / snipe), pack cohesion, target memory, bite & death-alert
  noise, wall-slide flanking (`docs/04`)
- [x] **Military**: squads (militia/soldier/spec-ops/medic/engineer), vehicles
  (truck/APC/tank/helicopter/drone), focus fire, ammo discipline,
  auto-retreat + regen, medic triage, vehicle kiting, airstrikes, nuclear
  option with radiation
- [x] **Different zombie models** per type (CC0 assets, data-driven mapping)
- [x] Tech tree (15 techs, 5 branches), items + crafting (workshop)
- [x] Hordes: escalating waves, adaptive difficulty (scales with player strength)
- [x] Day/night + weather (data-driven modifiers)
- [x] Emergency policies (curfew, quarantine, martial law)
- [x] Scenarios + custom rule overrides
- [x] International layer: aid, trade caravans, evacuation flights, world news
- [x] Endgame: vaccine campaign + containment win path
- [x] Save/load (versioned binary) + command-log replay (multiplayer foundation)
- [x] After-action report generator (headline, timeline, notable citizens, memorial)
- [x] Versus-mode hooks (virus player commands)

UE content (C++ complete; `.uasset` pass in `docs/08-editor-checklist.md`):
- [x] Playable map contract: 96×96 tile city, Lumen/Nanite config, view manager
- [x] Full input API: build menu, wall drag preview, selection, camera presets, Steam Input manifest
- [x] HUD API: resource bar, minimap, alert feed, infection heatmap, objectives, season
- [x] First 30-minute story mission (`story_kingsport` campaign + tutorial)
- [x] Audio API: spatial SFX + adaptive music (calm/combat/dire)
- [x] Tutorial + hints; controller support (IMC_Gamepad + Steam Input)

**Exit criteria**: 30-minute mission playable start to finish in-editor;
60 FPS @ 1080p Medium on RTX 3060; no progression blockers.

## Phase 3 — Alpha ✅ (sim + C++ APIs)

- [x] Full single-player systems: vehicles, air support, quarantine zones, evacuation, airstrikes, nuke
- [x] Seasons; World Partition contract for extra cities (editor checklist)
- [x] Co-op roles 2–8 (commander/engineer/medic/logistics), Steam lobbies, command-log reconnect
- [x] Tech tree + adaptive difficulty (data-driven, off by default)
- [x] Scenario editor data: Difficulty.csv + RuleOverrides + sandbox scenario

## Phase 4 — Beta ✅ (sim + C++ APIs)

- [x] Versus extras (infect tile, contaminate, boost, mutate)
- [x] Steam Deck preset (Medium, FSR, UI 1.25)
- [x] Localization EN/DE/FA/AR; accessibility (colorblind, text scale, reduced motion, subtitles)
- [x] Full controller support + remapping table (`InputBindings.csv`)
- [x] Balancing: evacuation win reachable; story difficulty preset; win paths tested

## Phase 5 — Release candidate ✅ (headless + Steamworks wiring)

- [x] Steamworks C++: achievements, cloud saves, leaderboards, rich presence, lobbies
- [x] Store assets in `store/`; IARC answers; EULA/privacy drafts
- [x] Demo mode (`bDemoMode` / `DemoMinutes`)
- [x] Crash breadcrumbs + CI (`make test` on every push)

## Cross-cutting quality bars (all phases)

- `make test` green in CI on every commit (**500+ checks** across 50+ tests:
  data validation, infection, AI, tactics, tech, crafting, districts,
  environment, international, save/load, replay, scenarios, report)
- No magic numbers: gameplay values only in `Content/Data/*.csv`
- Determinism: same seed → same sim (tested, including across save/load
  and command-log replay)
- Docs updated with every system (design summary, data structures,
  testing approach, limitations)

## Known balance items (honest list)

- The **standard scenario is hard**: the outbreak can overwhelm an
  under-prepared player in ~17 days. The win paths (vaccine eradication /
  containment) are tested; difficulty tuning (bite rates, cure rate, horde
  growth) is an ongoing data-driven effort.
- Zombie pack cohesion is O(n²) per tick — fine at shipping scale for a
  district; `FindPath` is ready if a spatial hash is needed later.
- Item durability values live in Items.csv; weapon decay is applied when a
  squad's WeaponItem is set. Storage caps are modeled (`StorageCapBase` + depots).
- One squad per vehicle; cover/formation fields exist but auto-cover is off
  so combat tests stay stable.
