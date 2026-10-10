# In-editor checklist — Outbreak Commander (UE 5.4)

The simulation, C++ APIs, data, audio files, and fonts are in the repo. Unreal `.uasset` files cannot be authored without the editor. Do this once on a machine with **UE 5.4**.

Estimated time: a focused art/audio/UI pass, not a second engineering project. Every C++ class below already exists and is BlueprintCallable.

## 0. Open the project

1. Epic Launcher → Unreal Engine **5.4**.
2. Right-click `OutbreakCommander.uproject` → Generate Visual Studio project files.
3. Open the `.uproject`. First compile of `SimulationCore` + `OutbreakCommander` should succeed (no missing modules).

## 1. Input assets (`Content/Input/`)

Create Enhanced Input assets matching `Content/Data/InputBindings.csv`:

| Asset | Type |
|---|---|
| `IMC_Default` | Mapping context (keyboard + mouse) |
| `IMC_Gamepad` | Mapping context (gamepad / Steam Input) |
| `IA_PanX`, `IA_PanY`, `IA_Zoom`, `IA_Rotate` | Axis1D |
| `IA_BuildConfirm`, `IA_BuildCancel`, `IA_Pause`, `IA_SpeedUp` | Digital |
| `IA_CameraPreset1` … `IA_CameraPreset4` | Digital |
| `IA_Select`, `IA_Command` | Digital |

Assign them on `AOCPlayerController` defaults (`IMC_Default`, `IMC_Gamepad`, each `IA_*`).

## 2. Map (`Content/Maps/MAP_PrototypeCity`)

1. New level, **World Partition** enabled, 256 m grid.
2. Landscape or a 960×960 m Nanite ground plane (96 tiles × 10 m). Collision: `BlockAll`.
3. Directional light + sky atmosphere + exponential height fog + sky light (Lumen).
4. Place `AOCSimViewManager`, a post-process volume (Lumen GI, volumetric fog).
5. Game Mode override: `AOCGameMode` (or `BP_OCGameMode` child). Default pawn `AOCGodPawn`, controller `AOCPlayerController`.
6. Save as `/Game/Maps/MAP_PrototypeCity` (already referenced in `DefaultEngine.ini`).
7. Optional: `MAP_PortHope`, `MAP_GrayHarbor`, `MAP_Countryside` with the same actors (World Partition streaming).

Lighting: enable Lumen, Nanite, virtual shadow maps (already in `DefaultEngine.ini`). Recapture sky light. Play-in-editor should tick the sim (HUD numbers move).

## 3. Blueprints

| Blueprint | Parent | Purpose |
|---|---|---|
| `BP_OCGameMode` | `AOCGameMode` | Set `DataDirectory` to the cooked `Content/Data` path (`FPaths::ProjectContentDir()/Data` at runtime is fine if you copy CSVs to Content) |
| `BP_OCGodPawn` | `AOCGodPawn` | Optional camera shake off when `bReducedMotion` |
| `BP_OCZombie` | `AOCZombieActor` | Switch static/skeletal mesh from `ModelPath` |
| `BP_OCCitizen` | `AOCCitizenActor` | Male/female mesh from `bFemale`; tint by visual state |
| `BP_OCBuilding` | `AOCBuildingActor` | Per-`DefId` mesh (house, farm, plant, …) |
| `BP_OCSquad` | `AOCSquadActor` | |
| `BP_OCVehicle` | `AOCVehicleActor` | |
| `BP_OCWall` | `AOCWallActor` | Segment mesh; gate / corner variants |
| `WBP_OCHUD` | `UOCHUDWidget` | Resource bar, minimap, alert feed, heatmap, weather/season, objectives, day/night |
| `WBP_BuildMenu` | `UUserWidget` | Buttons call `SetBuildMode` / `PlaceBuilding` |
| `WBP_Research` | `UUserWidget` | Tech tree; `StartResearch` |
| `WBP_Crafting` | `UUserWidget` | `QueueRecipe` |
| `WBP_Military` | `UUserWidget` | Squad list, `IssueOrder`, vehicles, airstrike/nuke |
| `WBP_Policies` | `UUserWidget` | Curfew / quarantine / martial law |
| `WBP_International` | `UUserWidget` | Aid / trade / evacuate |
| `WBP_Pause` | `UUserWidget` | Save / load / settings / quit |
| `WBP_Settings` | `UUserWidget` | Bound to `UOCGameUserSettings` |
| `WBP_MainMenu` | `UUserWidget` | Campaign / sandbox / co-op / versus / tutorial / load |
| `WBP_AfterAction` | `UUserWidget` | Shows `GenerateAfterActionReport()` |

Create `BP_OCGameMode` and set it in World Settings. HUD: add `WBP_OCHUD` in the player controller `BeginPlay`.

**CSV cooking:** add `Content/Data/*.csv` to Additional Non-Asset Directories to Copy in Project Settings → Packaging, **or** copy them next to the executable. The bridge's `DataDirectory` must point at that folder.

## 4. Meshes (`Content/Assets`)

Import the glTF/GLB files (they are CC0):

- Zombies: `Monsters/monster_*.gltf` — assign per `ZombieTypes.csv` `Model`.
- Citizens: `Characters/citizen_{male,female}_{a,b}.glb`.
- Props: `Props/block_*.gltf` as greybox buildings; `decor_tree.gltf` for parks.
- Walls: `block_brick.gltf` / `block_stone.gltf`; scale to 10 m segments.

Enable Nanite on static meshes. Collision: simple box per building footprint.

## 5. Audio (`Content/Audio`)

Import WAV/MP3. On the Game Mode (or a placed actor) add `UOCAudioManager` and assign:

- `MusicCalm` → `Music/ambient_calm.mp3`
- `MusicCombat` → `Music/ambient_combat.mp3`
- `MusicDire` → `Music/ambient_dire.mp3`
- `SfxZombieHurt/Death` → `SFX/zombie_*`
- `SfxGun` → `SFX/gun_*`
- `SfxUIClick` → `SFX/impact_hit.wav`

The manager already crossfades on `MusicState()`.

## 6. Fonts & localization

Import `Content/Fonts/ChakraPetch-*.ttf` (HUD) and `Orbitron-Variable.ttf` (title). Create a `Font` asset. Localization: the sim already serves EN/DE/FA/AR via `Localize(key)`. For UMG, call `UOCHUDWidget::Localize`. Enable RTL on Arabic/Persian by setting widget `FlowDirection` from `Language` (`ar`/`fa`).

## 7. Accessibility

`UOCGameUserSettings`: colorblind (post-process hue shift material, 3 LUTs), text scale (UMG property), UI scale, reduced motion (disable camera lag on `AOCGodPawn`), subtitles (already default on). Steam Deck preset: `ApplySteamDeckDefaults()`.

## 8. PIE verification (definition of the vertical slice)

- [ ] New Game (tutorial) runs, hints appear, farm + militia objectives complete.
- [ ] Drag-to-build walls show a ghost (`GetWallDragPreview`) and place segments with auto-corners.
- [ ] Resource bar, minimap, alert feed, heatmap overlay, day/night, weather, season, objectives all update.
- [ ] Camera: WASD, edge scroll, zoom, rotate, presets 1–4, gamepad.
- [ ] Save slot + autosave round-trip.
- [ ] Settings: quality, audio, language (EN/DE/FA/AR), colorblind, text scale.
- [ ] 60 FPS @ 1080p High in PIE on an RTX 3060-class GPU (stat unit / stat fps).
- [ ] Adaptive music moves calm → combat when zombies appear.

## 9. Packaging

Project Settings → Packaging: Shipping, compress pak, include `Content/Data`, `Content/Assets`, `Content/Audio`, `Content/Fonts`. Platforms: Windows 64, Linux. Then follow `docs/07-steam-upload.md`.
