# Autonomous assumptions — Outbreak Commander 1.0

Decisions made without blocking the user, 2026-10-10.

## Engine / environment

1. **UE 5.4 editor is not available in this environment.** Everything that can be built headless is built and tested (`make test`, `make demo`). The UE layer is complete C++ (HUD, settings, Steam, audio, net, crash reporter, camera presets, controller). `.uasset` creation is specified in `docs/08-editor-checklist.md` as a one-pass editor job, not leftover design.
2. **Steam AppID 480 (Spacewar)** is the development ID. Replace before store launch. Achievements, cloud, and Steam Input are wired against 480.
3. **Crash backend.** Breadcrumbs go to `Saved/Crashes/`. A Sentry DSN is configured at packaging time; none is committed.
4. **Upscaling plugins** (DLSS/FSR/XeSS) are selected in settings; they require the vendor plugins enabled in a full UE install. TSR is the default (already on in `DefaultEngine.ini`).

## Design

5. **No direct zombie combat for the player.** Military is ordered, never clicked-to-attack as an FPS. Versus-mode virus commands are the only "play as the horde" tools.
6. **Spring modifiers are identity (1.0 / 0.0)** so existing short tests and the prototype balance stay stable. Summer/autumn/winter change long games.
7. **Adaptive difficulty is off by default** (`AdaptiveDifficultyEnabled=0`) for the same reason; campaign/hard presets can turn it on.
8. **Pathfinding is opt-in** (`PathfindingEnabled=0`). Default zombie motion remains wall-slide steering (tested). `FindPath` is available for orders and future AI.
9. **Citizen type needs are off by default.** Types are assigned deterministically (`Id % N`) with no extra RNG, so NewGame is bit-identical to the previous seed aside from the new fields themselves.
10. **Evacuation win target is 80** (was 500, unreachable on a 150-pop map). Containment and eradication paths unchanged.
11. **SaveVersion is 2.** v1 saves from the previous session will not load; this is the 1.0 format.
12. **Campaign win-on-objectives is on.** Completing every *required* objective wins. Optional objectives (e.g. start triage in the story mission) do not block.
13. **Co-op roles do not block the sim.** `CanRoleIssue` is a query for the UI/net layer so command-log replay stays faithful. The listen server still applies every accepted command.
14. **Demo length is 30 real minutes at 1x** (`DemoMinutes=30`, `SecondsPerDay=60` → 30 in-game days).
15. **One squad per vehicle, no cover system by default.** `CoverBuildingId` exists for a later formation pass; squads do not auto-take cover (would change combat tests).
16. **Storage cap is 1,000,000** plus depots, so prototype economies never clamp.
17. **Languages:** English (source), German, Persian, Arabic. UI strings live in `Content/Data/Localization.csv`. RTL is an editor widget setting (`docs/08`).
18. **Company identity.** "Outbreak Commander Team", privacy URL `https://outbreakcommander.example/privacy` — replace with the real domain before store launch.
19. **Age rating.** IARC answers in `docs/legal/IARC.md` assume Teen / PEGI 16. Submit through Steam's IARC flow; do not invent a certificate number.
20. **Trailer.** Key art and a voice-over script ship in `store/`. The in-engine trailer is captured in-editor (sequencer) per the checklist; this environment cannot cook a video.

## Balance

21. The **standard scenario remains hard** (headless demo still loses around day 20 without a perfect player). Win paths are tested (`TestWinCondition`, `TestVaccineWin`, `TestEvacuationWin`). Story difficulty preset (`Difficulty.csv` `story`) exists for the first playthrough.
22. Magic numbers introduced for new systems went into `Balance.csv`, never into code.

## Legal

23. Third-party assets are CC0/OFL as already attributed. No new third-party code was added except Unreal Online Subsystem headers (Epic). EULA + Privacy are drafts for counsel review (`docs/legal/`).
