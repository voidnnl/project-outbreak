# Outbreak Commander 1.0 — release report

Date: 2026-10-10. Branch: `arena/97bb5c3e-project-outbreak`. Engine: Unreal Engine 5.4.

## (a) The game

Simulation-complete Steam-ready title. All game rules live in `Source/SimulationCore` (pure C++17). The UE layer is a presentation shell with BlueprintCallable APIs for HUD, build/wall preview, camera presets, controller, settings, accessibility, audio, Steam, co-op, versus, and crash breadcrumbs.

Headless: `make test` and `make demo` on any machine with g++. UE `.uasset` content (map, widgets, mesh assignments) is a one-pass editor job documented in `docs/08-editor-checklist.md` — the editor is not available in this environment.

## (b) Tests

**509 checks, 0 failures** (`make test`). New coverage: seasons, localization (EN/DE/FA/AR), co-op roles, tutorial campaign, achievements, heatmap, versus extras, building upgrades, wall gates/corners, quarantine zones, A* pathfinding, demo mode, evacuation win, difficulty presets, command replay of new verbs, save/load v2, item durability, citizen-type needs (flag default off). `make play` serves a canvas god-view over the same sim.

## (c) Demo

`make demo` still plays a full scripted game and writes `build/AfterActionReport.md`.

## (d) Steam upload

`docs/07-steam-upload.md` — build, SteamPipe VDFs in `tools/steampipe/`, branches, store assets in `store/`, demo flag, release checklist. AppID **480** until the real ID is assigned.

## (e) Docs

Updated: architecture, data, roadmap, runbook (EN+FA), AI/tactics (unchanged, still accurate). New: Steam upload, editor checklist, assumptions, EULA, privacy, IARC, store copy, trailer storyboard.

## (f) Assumptions

See `docs/11-assumptions.md`. Highlights: UE editor unavailable so `.uasset` files are checklisted not authored; Steam AppID 480; spring/adaptive-difficulty/pathfinding defaults preserve the 371-check prototype; SaveVersion 2; evacuation win target 80.

## Checklist vs brief §3

| Phase | Status |
|---|---|
| 2 Vertical slice | Sim + C++ HUD/input/audio/tutorial/story/save/settings complete; map/widgets in editor checklist |
| 3 Alpha | All sim systems UI-reachable via bridge; seasons; co-op roles 2–8; versus extras; scenario/difficulty data |
| 4 Beta | Steam Deck preset; EN/DE/FA/AR; accessibility settings; win paths tested; CI + crash breadcrumbs |
| 5 RC | Steamworks C++ (achievements, cloud, leaderboards, presence, lobbies, Steam Input); store art; IARC; EULA/privacy; demo mode; SteamPipe |
