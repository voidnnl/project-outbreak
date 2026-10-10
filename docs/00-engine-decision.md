# Engine Decision: Unreal Engine 5

**Decision: Unreal Engine 5 (target 5.4 LTS), C++, with an engine-agnostic simulation core.**

## Why UE5

| Requirement (brief section) | UE5 answer |
|---|---|
| Large seamless open world, no loading screens (§10) | World Partition + streaming; Nanite removes mesh-LOD authoring for big maps |
| PBR, 4K textures, dynamic GI, real-time reflections, volumetric fog (§12) | Lumen (dynamic GI + reflections), Nanite, volumetric fog, cinematic post-process — all first-party |
| Destruction, fire/smoke/explosion particles (§5, §12) | Chaos destruction + Niagara |
| Stable 60 FPS at 1080p on RTX 3060; Steam Deck playable (§12) | Mature scalability presets, DLSS/FSR/XeSS plugins, extensive profiling tools (Insights, stat commands) |
| Authoritative client-server multiplayer, Steam (§9, §14) | Built-in replication + OnlineSubsystemSteam + Steam Input |
| Steam launch: achievements, cloud saves, leaderboards, rich presence (§15) | Steamworks via OnlineSubsystemSteam |
| Data-driven tuning (§17: values in editable files) | DataTables + CSV import pipeline; our sim core reads the same CSVs headless |
| 10k+ agents (zombies, citizens) simulating every frame | C++ core with fixed-timestep sim; MassEntity/AI available later if needed |

## Why not Unity HDRP

- **World streaming**: HDRP has no production-grade equivalent to World Partition for seamless multi-city worlds; stitching scenes is manual and painful.
- **Simulation performance**: a 10k-agent infection/economy sim wants plain C++ without per-frame GC pressure; Unity would push us to DOTS (a second, parallel ECS stack to learn and maintain).
- **Cinematic GI**: Lumen is a generation ahead of HDRP's real-time GI for large outdoor scenes.
- Trade-off accepted: UE5 has a steeper learning curve and longer build times; the pure-C++ sim core mitigates the biggest risk (game logic locked inside the engine) — the entire game simulation compiles and runs without UE.

## The hedge that makes this safe

**All game logic lives in `Source/SimulationCore`: pure C++17, zero engine dependencies, compiled and unit-tested headless (`make test`).** The UE layer is a thin presentation shell (input → commands, sim state → actors/UI). If the engine choice ever had to change, the sim core — the actual game — moves with us. It also gives us:

- deterministic, seed-based simulation (same seed → same outbreak), which is a prerequisite for replays, lockstep multiplayer, and save/load verification;
- CI unit tests for economy, infection, and AI (brief §14 asks for exactly this) that run in seconds without a GPU or engine;
- a scriptable headless demo (`make demo`) for balance tuning.

## Engine version and settings

- **UE 5.4** (LTS branch): stable Nanite/Lumen, Enhanced Input, World Partition. Config in `Config/DefaultEngine.ini` (Lumen GI method 1, Nanite project-enabled, virtual shadow maps, TSR).
- Rendering targets: 60 FPS @ 1080p High on RTX 3060-class; Steam Deck verified target at reduced presets (Medium, FSR quality).
- Upscaling: DLSS / FSR / XeSS selectable in the graphics menu (platform plugins; per-platform defaults in `Config/DefaultPlatform.ini` at packaging time).

## Known trade-offs / costs

- **Iteration speed**: UE5 edit-compile cycles are slower than Unity; mitigated by keeping logic in the sim core (fast `g++` loop) and UE code thin.
- **Team skills**: C++/UE experience required; the sim core is plain C++ and can be worked on by anyone with a compiler.
- **Build size/cook times**: acceptable for a mid-size commercial title; monitored in CI.
- **Console certification**: UE5 is the industry default for console ports; no action needed now.
