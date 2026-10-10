# Data Files: the single source of truth

Every gameplay value — costs, rates, timers, stats, AI behavior, models —
lives in CSV files under `Content/Data/`. **Nothing is hard-coded.** The same
files are consumed two ways:

1. **SimulationCore** parses them directly (headless tests, CI, the CLI demo).
2. **Unreal Engine** imports them as DataTable assets using the row structs
   in `Source/OutbreakCommander/Public/OCDataTables.h`.

The unit tests fail the build if any CSV is missing, malformed, duplicated,
has unknown references, or points at a missing asset — bad data can never
ship.

## Files

### `Balance.csv` — global tuning (key/value, ~120 keys)
Columns: `Key,Value,Description`. Code reads via `BalanceF("Key", fallback)`.
Groups: simulation pacing, starting stockpiles, infection, morale, economy,
walls, power, citizens, zombies, **zombie AI** (pack radius, search seconds,
bite noise, wall slide, death alerts, hit-and-run), **military tactics**
(regen, retreat/resume fractions, low-ammo threshold), research, crafting,
airstrike/nuke, vaccination, evacuation, aid/trade/news, policies, save
version, report limits, districts.

### `Resources.csv` — resource catalog (9: money, food, water, power, fuel, materials, medicine, ammo, research)

### `Buildings.csv` — 12 building definitions
Columns: `Id,Name,CostMoney,CostMaterials,PowerGeneration,PowerConsumption,WaterProduction,WaterConsumption,FoodProduction,FuelConsumption,Function,Housing,Jobs,BuildTimeSeconds,MaxHp,Width,Height,Description`
`Function`: housing | power | medical | food | water | storage | military | crafting | research.

### `ZombieTypes.csv` — 5 zombie types with AI behavior + 3D model
Columns: `Id,Name,Hp,SpeedTilesPerSec,DamagePerSec,BiteChancePerSec,SightRadiusTiles,SmellRadiusTiles,BountyMoney,Behavior,Model,Description`
- `Behavior`: `chase | hitandrun | pushahead | snipe` (see `docs/04-zombie-ai-and-tactics.md`)
- `Model`: asset under `Content/Assets/` (validated at load time)

### `Soldiers.csv` — 5 military unit types
Columns: `Id,Name,CostMoney,CostAmmo,FoodPerDay,Hp,DamagePerSec,AmmoPerSec,RangeTiles,SpeedTilesPerSec,VisionTiles,SquadSize,TrainingSeconds,WeaponItem,Unlock,Description`
`Unlock`: tech id required, or `none`.

### `Vehicles.csv` — 5 vehicles (truck, APC, tank, helicopter, drone)
Columns: `Id,Name,CostMoney,CostMaterials,CostFuel,Hp,SpeedTilesPerSec,DamagePerSec,AmmoPerSec,RangeTiles,Seats,Unlock,Description`

### `TechTree.csv` — 15 technologies, 5 branches (medical, military, engineering, agriculture, communications)
Columns: `Id,Name,Branch,CostResearch,Prerequisites,EffectKey,EffectValue,Description`
`EffectKey`: a modifier (`medical_cure_rate`, `military_damage`, `wall_hp`,
`construction_speed`, `farm_yield`, `water_yield`, `research_rate`,
`solar_day_power`), an `unlock_*` flag, or `deploy_vaccine`.

### `Items.csv` — 11 items (weapons, armor, medical, tools, electronics)
Columns: `Id,Name,Category,Rarity,Durability,Description`

### `Recipes.csv` — 8 crafting recipes
Columns: `Id,Name,Inputs,Outputs,TimeSeconds,Description`
Inputs/Outputs: `id:count;id:count` where ids are items OR resources.

### `Scenarios.csv` — 6 scenario presets (calm, standard, virulent, blackout, overrun, sandbox)
Columns: `Id,Name,CityName,Description,InitialPopulation,PatientZeroIncubationDays,ZombieSpeedMultiplier,InfectionRateMultiplier,ResourceScarcityMultiplier,StartHordes`

### `Weather.csv` — 5 weather profiles (clear, rain, fog, storm, snow)
Columns: `Id,Name,ZombieActivityMultiplier,SolarPowerMultiplier,MoraleModifierPerDay,Description`

### `Names.csv` — citizen first/last names + NATO squad names
Columns: `Type,Name` with types `first_male`, `first_female`, `last`, `squad`.

### `WorldCities.csv` — 245 real capital cities (from public datasets)
Columns: `City,Country,Latitude,Longitude,Population` (national population
as a metro-scale proxy). Powers international aid, trade, news, and
evacuation flavor. Source: `github.com/mledoze/countries` +
`github.com/dr5hn/countries-states-cities-database` (see
`tools/fetch_assets.py`).

## Rules for editing data

1. **Never hard-code a value that belongs in data.**
2. **Keep ids stable** (referenced by saves, mods, Workshop content).
3. **Run `make test` after data changes** — the suite validates the CSVs and
   re-checks balance-sensitive behaviors.
4. **One concern per file.** New tunables go in `Balance.csv`.
5. CSV quoting: fields with commas must be double-quoted; `""` escapes.

## Planned Phase 2+ tables (same pattern)

- `CitizenTypes.csv` — citizen archetypes (needs profiles, wages)
- `Squads.csv` / `Vehicles.csv` (extend) — more unit/vehicle variety
- `Localization.csv` — string keys for the localization system (brief §13)
