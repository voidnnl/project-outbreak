# Runbook — Outbreak Commander

## Headless (no Unreal)

```
make test    # unit suite (500+ checks)
make demo    # 120-day scripted playthrough + after-action report
make clean
```

Requires g++ with C++17. Data path defaults to `Content/Data`.

CI: `.github/workflows/ci.yml` runs both targets on every push.

## Unreal

See `docs/08-editor-checklist.md`. Play-in-editor uses `MAP_PrototypeCity` and `AOCGameMode`, which loads CSVs and starts a game from `UOCGameInstance` pending fields (campaign, difficulty, language, demo, versus, co-op).

## Saves

Binary snapshots, magic `OCSV`, `SaveVersion` 2 (`Balance.csv`). Path: `Saved/SaveGames/<slot>.sav`. Autosave interval: `AutosaveIntervalDays`. Cloud: `UOCSteamSubsystem::CloudSave`.

## Multiplayer

Listen server, `AOCNetGameMode`, max 8. Roles cycle commander → engineer → medic → logistics. Versus: player 1 is the virus. Reconnect = replay the command log on the authoritative sim.

## Localization

`Content/Data/Localization.csv` keys. `FSimulation::Localize` / `UOCSimulationBridge::Localize`. Languages: `en`, `de`, `fa`, `ar`.

## Telemetry / crash

Opt-in. Breadcrumbs: `Saved/Crashes/breadcrumbs.log`. Shipping builds use UE Crashpad; set the DSN at package time.

## Steam Deck

`UOCGameUserSettings::ApplySteamDeckDefaults()` — Medium, FSR, UI scale 1.25, 60 Hz cap. Steam Input manifest: `tools/steampipe/steam_input_manifest.vdf`.
