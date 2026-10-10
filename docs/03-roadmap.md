# Development Roadmap

Phases follow brief §16. Status: **Phase 2 (vertical slice) systems are
simulation-complete**; UE content (maps, widgets, audio mixing) is next.

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

UE content (next, in-editor):
- [ ] Playable map: graybox → art pass on one district; Lumen/Nanite showcase
- [ ] Full input: build menu, wall drag with preview ghost, selection
- [ ] HUD: resource bar, minimap, alert feed, infection heatmap overlay
- [ ] First 30-minute story mission (scripted outbreak scenario)
- [ ] Audio pass: spatial SFX + adaptive music (assets ready in Content/Audio)
- [ ] Tutorial + tooltips; controller support (Steam Input)

**Exit criteria**: 30-minute mission playable start to finish in-editor;
60 FPS @ 1080p Medium on RTX 3060; no progression blockers.

## Phase 3 — Alpha

- [ ] Full single-player systems: vehicles in-game, air support, strategic
  tools (quarantine zones, evacuation, airstrikes, controlled bombing)
- [ ] Multiple cities + countryside; World Partition streaming, seasons
- [ ] Co-op multiplayer (2–4 players; roles: commander/engineer/medic/
  logistics), server browser, invites, reconnection (command log ready)
- [ ] Complete tech tree playthrough + adaptive difficulty tuning
- [ ] Scenario editor / custom rules UI

## Phase 4 — Beta

- [ ] All content; versus mode (one player spreads the virus)
- [ ] Optimization: 60 FPS stable, streaming, Steam Deck verified
- [ ] Localization (EN first; Persian, Arabic, German), accessibility
  (colorblind modes, text scaling, reduced motion)
- [ ] Full controller support + remapping
- [ ] Balancing from telemetry (opt-in) and playtests

## Phase 5 — Release candidate

- [ ] Steamworks: achievements, cloud saves, leaderboards, rich presence
- [ ] Store assets: capsules, hero, screenshots, trailer; age rating
- [ ] EULA/privacy; third-party asset legal review (attribution ready)
- [ ] Demo build; min/recommended specs tested; QA pass
- [ ] Crash reporting + CI builds

## Cross-cutting quality bars (all phases)

- `make test` green in CI on every commit (**371 checks** across 34 tests:
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
- Zombie pack cohesion is O(n²) per tick — fine at prototype scale, spatial
  hash planned for Phase 3.
- Items have placeholder durability; storage caps are not modeled.
- One squad per vehicle; no cover/formation system yet.
