# Steam upload guide — Outbreak Commander 1.0

AppID **480 (Spacewar)** is the development stand-in. Replace with the real AppID before going live (`docs/11-assumptions.md`).

## 0. One-time partner setup

1. Steamworks partner account, sign the NDA, complete tax/banking.
2. Create the app: type **Game**, name **Outbreak Commander**.
3. Create depots: `Windows 64` (481), `Linux 64` (482). Steam Deck uses the Linux depot + Proton as fallback.
4. Enable packages: standard, demo (`Outbreak Commander Demo`).
5. Copy achievement IDs from `Content/Data/Achievements.csv` (`SteamApiName` column) into Steamworks → Stats & Achievements. They already match `Config/DefaultEngine.ini`.
6. Enable: Cloud (quota 100 MB), Leaderboards (`OC_DaysSurvived`, `OC_ZombiesKilled`, `OC_Score`), Rich Presence, Steam Input (upload `tools/steampipe/steam_input_manifest.vdf`).
7. Set `SteamAppId` / `SteamDevAppId` in `Config/DefaultEngine.ini` to the real AppID.

## 1. Build (Windows)

```
# From a machine with UE 5.4 and Visual Studio 2022
RunUAT.bat BuildCookRun -project=OutbreakCommander.uproject ^
  -platform=Win64 -clientconfig=Shipping -cook -stage -pak -archive ^
  -archivedirectory=Saved/StagedBuilds/Windows
```

Linux (Steam Deck):

```
RunUAT.sh BuildCookRun -project=OutbreakCommander.uproject \
  -platform=Linux -clientconfig=Shipping -cook -stage -pak -archive \
  -archivedirectory=Saved/StagedBuilds/Linux
```

Headless verification (any machine with g++):

```
make test && make demo
```

## 2. SteamPipe

```
cd tools/steampipe
# Edit app_build.vdf: AppID, ContentRoot, Desc
steamcmd +login <build_account> \
  +run_app_build app_build.vdf \
  +quit
```

The build lands on the **default** branch, not live. Set `"SetLive"` only after QA.

## 3. Branches

| Branch | Purpose |
|---|---|
| `default` | Staging; QA plays this |
| `demo` | 30-minute Next Fest build (`bPendingDemo = true`) |
| `beta` | Public beta |
| `public` | Live (set live in Steamworks after sign-off) |

## 4. Store page

Upload from `store/`:

| Asset | File | Size |
|---|---|---|
| Header capsule | `store/capsule_header.png` | 920×430 (source 1840×860) |
| Main capsule | `store/capsule_main.png` | 616×353 |
| Small capsule | `store/capsule_small.png` | 231×87 |
| Hero | `store/hero.png` | 3840×1240 |
| Library capsule | `store/library_capsule.png` | 600×900 |
| Page background | `store/page_background.png` | 1438×810 |
| Screenshots | `store/screenshot_*.png` | 1920×1080 |
| Logo | `store/logo.png` | transparent |

Copy `store/STORE_DESCRIPTION.md` into the short + long descriptions. System requirements are in that file.

## 5. Demo (Next Fest)

Package with `UOCGameInstance::bPendingDemo = true` (the sim freezes after `DemoMinutes` from Balance.csv, default 30 real minutes at 1x). Depot the demo as a separate app pointing at the same executable with `-demo`.

## 6. Release checklist

- [ ] `make test` green on CI for the tagged commit
- [ ] Save compatibility: a 1.0 save loads (SaveVersion 2)
- [ ] Achievements fire in a Spacewar session
- [ ] Cloud save round-trips
- [ ] Steam Deck: 30 minutes on device, UI scale 1.25, FSR, 30–40 FPS target at Medium
- [ ] IARC filled (`docs/legal/IARC.md`)
- [ ] EULA + Privacy URLs live
- [ ] Capsules / hero / 5 screenshots / trailer uploaded
- [ ] Build set live on `public`

## 7. Crash reporting

Set `SENTRY_DSN` in the packaged environment (or `DefaultGame.ini` `[Crash] Dsn=`). `UOCCrashReporter` writes breadcrumbs to `Saved/Crashes/`. UE's Crashpad (Crash Report Client) is enabled for Shipping; point it at your backend in Project Settings → Crash Reporter.
