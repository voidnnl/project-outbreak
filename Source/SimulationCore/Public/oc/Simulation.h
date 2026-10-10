// Outbreak Commander - Simulation Core
// FSimulation: the deterministic, engine-agnostic game world.
//
// Phase 1 (prototype): one procedural city, infection spread, walls, power
// plant, medical center, simple zombie AI.
// Phase 2-3 (added): military (squads, vehicles, airstrikes, nuclear option),
// all five zombie types + hordes + noise, tech tree, items & crafting,
// district power/water networks, day/night + weather, emergency policies,
// scenarios & custom rules, international aid/trade/evacuation, the vaccine
// end-game, save/load, command-log replay, versus-mode hooks, and the
// after-action report generator.
//
// Everything the player can do is a method on this class. The UE layer only
// forwards input here and renders the resulting state, which keeps the game
// logic testable headless.
#pragma once

#include "oc/DataTables.h"
#include "oc/SimTypes.h"

namespace oc {

struct FTile {
    float Contamination = 0.f;  // 0..1, decays over time, infects citizens
    i32 District = 0;
    bool bRoad = false;
};

struct FCitizen {
    i32 Id = 0;
    i32 HomeId = -1;            // building id, -1 = homeless
    float X = 0.f, Y = 0.f;     // tile-space position
    ECitizenState State = ECitizenState::Healthy;
    bool bLatentInfection = false;
    bool bVaccinated = false;   // immune: cannot be infected
    bool bFemale = false;
    std::string Name;           // "First Last"
    std::string TypeId = "civilian";  // CitizenTypes.csv; assigned without extra RNG
    float InfectionSecondsLeft = 0.f;
    float Panic = 0.f;          // 0..1
    float Hunger = 1.f;         // 0..1
    float Thirst = 1.f;         // 0..1
    float Health = 1.f;         // 0..1
    float CorpseRiseSecondsLeft = 0.f;
    float TargetX = 0.f, TargetY = 0.f;
    bool bHasTarget = false;
};

struct FZombie {
    i32 Id = 0;
    std::string TypeId = "walker";
    float X = 0.f, Y = 0.f;
    float Hp = 1.f;
    EZombieState State = EZombieState::Wandering;
    i32 TargetCitizen = -1;
    float AttackCooldown = 0.f;
    float WanderTimer = 0.f;
    float DirX = 1.f, DirY = 0.f;
    // Noise investigation (gunfire attraction) and target memory.
    float ChaseX = -1.f, ChaseY = -1.f;
    float ChaseTimer = 0.f;
    // Wall sliding (going around instead of only attacking the wall).
    float SlideTimer = 0.f;
    float SlideDirX = 0.f, SlideDirY = 0.f;
    // Hit-and-run retreat after biting.
    float RetreatTimer = 0.f;
    // Death-alert speed boost (a zombie died nearby).
    float AlertTimer = 0.f;
};

// A transient noise ping: a zombie bite or a death alert. Zombies within the
// radius investigate (or, for alerts, converge on the location and speed up).
struct FNoisePing {
    float X = 0, Y = 0;
    float Radius = 10;
    float Timer = 1.5f;
    bool bAlert = false;  // death alert: attract + speed boost
};

struct FBuilding {
    i32 Id = 0;
    std::string DefId;
    i32 X = 0, Y = 0, W = 1, H = 1;  // footprint in tiles
    float Hp = 1.f;
    bool bUnderConstruction = true;
    float BuildProgress = 0.f;
    bool bPowered = false;
    bool bFuelStarved = false;
    bool bDemolished = false;  // tombstone; indices stay stable
    i32 Level = 1;
};

struct FWall {
    i32 Id = 0;
    float X1 = 0.f, Y1 = 0.f, X2 = 0.f, Y2 = 0.f;  // tile-center to tile-center
    float Hp = 1.f;
    float MaxHp = 1.f;
    bool bUnderConstruction = true;
    float BuildProgress = 0.f;
    bool bCorner = false;  // auto-detected on turns of a wall line
    bool bGate = false;    // converted crossing; weaker HP
};

struct FResources {
    double Money = 0, Food = 0, Water = 0, Fuel = 0;
    double Materials = 0, Medicine = 0, Ammo = 0, Research = 0;
    double PowerGeneration = 0, PowerConsumption = 0;
};

// Resource slot accessor by resource id (Resources.csv). Returns nullptr for
// unknown ids. Lets recipes and systems treat items and resources uniformly.
inline double* ResourceSlot(FResources& R, const std::string& Id) {
    if (Id == "money") return &R.Money;
    if (Id == "food") return &R.Food;
    if (Id == "water") return &R.Water;
    if (Id == "fuel") return &R.Fuel;
    if (Id == "materials") return &R.Materials;
    if (Id == "medicine") return &R.Medicine;
    if (Id == "ammo") return &R.Ammo;
    if (Id == "research") return &R.Research;
    return nullptr;
}
inline const double* ResourceSlot(const FResources& R, const std::string& Id) {
    if (Id == "money") return &R.Money;
    if (Id == "food") return &R.Food;
    if (Id == "water") return &R.Water;
    if (Id == "fuel") return &R.Fuel;
    if (Id == "materials") return &R.Materials;
    if (Id == "medicine") return &R.Medicine;
    if (Id == "ammo") return &R.Ammo;
    if (Id == "research") return &R.Research;
    return nullptr;
}

class FSimulation {
public:
    FSimulation() = default;

    // --- lifecycle ---------------------------------------------------------
    bool LoadData(const std::string& DataDir);
    void NewGame(const FSimConfig& Config);  // uses Config.ScenarioId
    void Step(float DeltaSeconds);            // advances in fixed SimTickSeconds steps

    // --- player commands: construction --------------------------------------
    i32 PlaceBuilding(const std::string& DefId, i32 X, i32 Y);   // building id or -1
    i32 BuildWallLine(i32 X1, i32 Y1, i32 X2, i32 Y2);           // segments built, 0 none, -1 invalid
    bool DemolishBuilding(i32 BuildingId);
    i32 BuildPowerLine(i32 DistrictA, i32 DistrictB);            // line id or -1
    i32 BuildWaterPipe(i32 DistrictA, i32 DistrictB);            // pipe id or -1
    bool DemolishPowerLine(i32 DistrictA, i32 DistrictB);        // cut a district off the grid
    bool DemolishWaterPipe(i32 DistrictA, i32 DistrictB);

    // --- player commands: military ------------------------------------------
    i32 RecruitSquad(const std::string& UnitTypeId, i32 X, i32 Y);  // squad id or -1
    bool IssueOrder(i32 SquadId, ESquadOrder Order, i32 TargetX = -1, i32 TargetY = -1);
    bool DisbandSquad(i32 SquadId);
    i32 DeployVehicle(const std::string& TypeId, i32 X, i32 Y);      // vehicle id or -1
    bool OrderVehicleMove(i32 VehicleId, i32 X, i32 Y);
    bool EmbarkSquad(i32 SquadId, i32 VehicleId);
    bool DisembarkSquad(i32 SquadId);
    bool CallAirstrike(i32 X, i32 Y);
    bool CallNuke(i32 X, i32 Y);

    // --- player commands: research, crafting, policies, international -------
    bool StartResearch(const std::string& TechId);
    bool QueueRecipe(const std::string& RecipeId);
    void SetPolicy(EPolicy Policy, bool bEnabled);
    bool RequestAid();
    bool SendTradeCaravan();
    bool CallEvacuationFlight();

    // --- versus mode (one player commands the virus) ------------------------
    i32 VirusSpawnHorde(i32 X, i32 Y, const std::string& TypeId, i32 Count);
    bool VirusInfectTile(i32 X, i32 Y);
    bool VirusContaminate(i32 X, i32 Y);
    bool VirusBoostHorde();
    i32 VirusMutate(const std::string& FromType, const std::string& ToType, i32 Count);

    // --- campaign / tutorial / buildings / zones ----------------------------
    bool StartCampaign(const std::string& CampaignId);
    bool UpgradeBuilding(i32 BuildingId);
    bool BuildWallGate(i32 WallId);
    bool DesignateQuarantineZone(i32 District, bool bEnabled);
    bool ApplyDifficulty(const std::string& DifficultyId);
    void SetLanguage(const std::string& Lang);
    void SetLocalRole(ECoopRole Role);
    bool DismissHint();

    // --- queries: campaign, heatmap, audio, localization, roles --------------
    bool CanRoleIssue(ECoopRole Role, const std::string& Verb) const;
    const std::vector<FObjective>& Objectives() const { return ObjectiveList; }
    const std::vector<FAchievementUnlock>& Achievements() const { return AchievementList; }
    const std::vector<FHint>& Hints() const { return HintList; }
    const std::vector<char>& QuarantineDistricts() const { return QuarantineFlags; }
    std::string CurrentSeason() const { return CurrentSeasonId; }
    EMusicState MusicState() const;
    std::string Localize(const std::string& Key) const;
    float InfectionHeatAt(i32 X, i32 Y) const;
    void FillHeatmap(std::vector<float>& Out, i32 Downsample = 2) const;
    i32 ComputeLeaderboardScore() const;
    double StorageCap() const;
    float SeasonFoodMult() const;
    float SeasonZombieMult() const;
    float SeasonSolarMult() const;
    float SeasonMoralePerDay() const;
    bool FindPath(i32 X1, i32 Y1, i32 X2, i32 Y2, std::vector<std::pair<i32, i32>>& Out) const;

    // --- queries -------------------------------------------------------------
    const FDataTables& Data() const { return Tables; }
    const FSimConfig& GetConfig() const { return Config; }
    const FResources& GetResources() const { return Resources; }
    const FStats& GetStats() const { return Stats; }
    EGameResult GetResult() const { return Result; }
    const std::string& GetResultReason() const { return ResultReason; }
    float GetTimeSeconds() const { return TimeSeconds; }
    i32 MapWidth() const { return Width; }
    i32 MapHeight() const { return Height; }
    i32 DistrictCount() const { return DistrictCols * DistrictRows; }
    i32 DistrictAt(i32 X, i32 Y) const;
    i32 DistrictOfBuilding(i32 BuildingId) const;
    bool IsDistrictPowered(i32 District) const;
    bool IsDistrictWatered(i32 District) const;
    const std::vector<FCitizen>& Citizens() const { return CitizenList; }
    const std::vector<FZombie>& Zombies() const { return ZombieList; }
    const std::vector<FBuilding>& Buildings() const { return BuildingList; }
    const std::vector<FWall>& Walls() const { return WallList; }
    const std::vector<FSquad>& Squads() const { return SquadList; }
    const std::vector<FVehicle>& Vehicles() const { return VehicleList; }
    const std::vector<FNoisePing>& NoisePings() const { return NoisePingList; }
    const std::vector<FCraftJob>& CraftJobs() const { return CraftJobsList; }
    const std::vector<FNetworkLink>& PowerLines() const { return PowerLineList; }
    const std::vector<FNetworkLink>& WaterPipes() const { return WaterPipeList; }
    const std::deque<FSimEvent>& Events() const { return EventLog; }
    const std::vector<FTile>& Tiles() const { return TileGrid; }
    const std::vector<FWorldCity>& WorldCities() const { return Tables.WorldCities; }
    const FNotable& Notable() const { return NotableEvents; }
    bool IsInside(i32 X, i32 Y) const { return X >= 0 && Y >= 0 && X < Width && Y < Height; }
    const FTile& TileAt(i32 X, i32 Y) const;
    float BalanceF(const std::string& Key, float Fallback) const { return Tables.Balance(Key, Fallback); }
    u64 StateHash() const;  // determinism check for tests/replays

    // --- tech / unlocks / modifiers ------------------------------------------
    bool IsTechResearched(const std::string& TechId) const;
    bool IsUnlocked(const std::string& Feature) const;  // unlock_* flags
    float Modifier(const std::string& Key) const;        // tech multipliers, default 1
    const std::string* ActiveResearch() const { return ActiveTechId.empty() ? nullptr : &ActiveTechId; }
    float ActiveResearchProgress() const { return ResearchProgress; }

    // --- environment -----------------------------------------------------------
    std::string CurrentWeather() const { return CurrentWeatherId; }
    bool IsNight() const;
    float DayFraction() const;  // 0..1 within the day
    bool HasPolicy(EPolicy Policy) const { return Policies.count(Policy) && Policies.at(Policy); }
    std::string CitizenName(i32 CitizenId) const;

    // --- items -----------------------------------------------------------------
    i32 ItemCount(const std::string& ItemId) const;
    const std::vector<FItemStack>& Inventory() const { return InventoryList; }

    // --- international ----------------------------------------------------------
    i32 EvacuatedTotal() const { return Evacuated; }

    // --- save / load / replay -----------------------------------------------------
    bool SaveToFile(const std::string& Path) const;
    bool LoadFromFile(const std::string& Path);
    void SetCommandLogEnabled(bool bEnabled) { bCommandLogEnabled = bEnabled; }
    const std::vector<FCommand>& CommandLog() const { return CommandLogList; }
    bool ApplyCommand(const FCommand& Cmd);  // used by replays

    // --- after-action report (the end-of-game artifact) -----------------------------
    std::string GenerateAfterActionReport() const;

    // --- debug / cheat hooks (also used by unit tests) ------------------------------
    FResources& DebugResources() { return Resources; }
    u64 DebugRngState() const { return Rng.State; }
    // Extra sim internals for determinism debugging (tests only).
    float DebugWeatherDaysLeft() const { return WeatherDaysLeft; }
    float DebugHordeTimerDays() const { return HordeTimerDays; }
    double DebugTreatmentsAccumulator() const { return TreatmentsAccumulator; }
    float DebugQuietDays() const { return QuietDays; }
    float DebugNewsTimerDays() const { return NewsTimerDays; }
    float DebugSquadNoiseTimer(i32 SquadId) const {
        return (SquadId >= 0 && SquadId < static_cast<i32>(SquadList.size()))
            ? SquadList[SquadId].NoiseTimer : -1.f;
    }
    float DebugSquadTargetX(i32 SquadId) const {
        return (SquadId >= 0 && SquadId < static_cast<i32>(SquadList.size()))
            ? SquadList[SquadId].TargetX : -1.f;
    }
    float DebugCitizenPanicSum() const {
        float S = 0;
        for (const auto& C : CitizenList) S += C.Panic;
        return S;
    }
    float DebugContaminationSum() const {
        float S = 0;
        for (const auto& T : TileGrid) S += T.Contamination;
        return S;
    }
    float DebugZombieWanderSum() const {
        float S = 0;
        for (const auto& Z : ZombieList) S += Z.WanderTimer;
        return S;
    }
    void DebugInfect(i32 CitizenId);
    i32 DebugSpawnZombie(const std::string& TypeId, float X, float Y);  // zombie id or -1
    void DebugTeleportCitizen(i32 CitizenId, float X, float Y);
    i32 DebugAddObjective(const std::string& Id, const std::string& Kind, const std::string& Target, i32 Count);
    void DebugUnlockAchievement(const std::string& Id);
    void DebugSetSeason(const std::string& SeasonId);

private:
    void RunTick(float Dt);
    void TickBuildings(float Dt);
    void TickEconomy(float Dt);
    void TickCitizens(float Dt);
    void TickZombies(float Dt);
    void TickMilitary(float Dt);
    void TickHordes(float Dt);
    void TickNoisePings(float Dt);
    void TickWeather(float Dt);
    void TickVaccination(float Dt);
    void TickCrafting(float Dt);
    void TickInternational(float Dt);
    void TickSeasons(float Dt);
    void TickCampaign(float Dt);
    void TickAchievements();
    void TickHints(float Dt);
    void TickDemoMode(float Dt);
    void TickAdaptiveDifficulty();
    void UpdateDistrictNetworks();
    void UpdateContamination(float Dt);
    void TickMorale(float Dt);
    void UpdateStats();
    void CheckGameResult(float Dt);

    void InfectCitizen(i32 CitizenId, const char* Cause);
    void TurnCitizen(i32 CitizenId);
    void KillCitizen(i32 CitizenId, const char* Cause);
    void CureCitizen(i32 CitizenId);
    void VaccinateCitizen(i32 CitizenId);
    void Log(EEventType Type, const std::string& Msg);
    void LogCommand(const std::string& Verb, std::vector<std::string> Args);
    i32 FindWallBlocking(float X1, float Y1, float X2, float Y2) const;  // wall index or -1
    bool TilesOverlapBuilding(i32 X, i32 Y, i32 W, i32 H) const;
    bool SegmentCrossesBuilding(float X1, float Y1, float X2, float Y2) const;
    void GenerateCity();
    void AssignHomes();
    i32 NearestZombie(float X, float Y, float MaxDist) const;  // zombie id or -1
    i32 CountAliveCitizens() const;
    i32 CountActivePoweredByFunction(const std::string& Function) const;
    i32 AddNetworkLink(std::vector<FNetworkLink>& List, i32 A, i32 B);
    float ZombieSpeedScale() const;    // scenario + weather + day/night
    float InfectionScale() const;      // scenario + policies
    void ApplyScenario(const FScenarioDef& S);
    void ApplyTechEffect(const FTechDef& T);
    void RecomputeTechEffects();
    void SpawnHordeWave(i32 Count, const char* Origin);
    i32 SpawnZombieOfType(const std::string& TypeId, float X, float Y);
    bool ConsumeAmmo(double Amount);
    bool AddItem(const std::string& ItemId, i32 Count);
    bool RemoveItem(const std::string& ItemId, i32 Count);
    std::string RandomWorldCity();  // not const: draws from the sim RNG
    std::string SquadName();
    void KillZombie(i32 ZombieIndex, i32 SquadId);  // credit + removal
    void DamageSquad(FSquad& Squad, float Amount, i32 ZombieSource);
    void PendingStrikeTick(float Dt);  // airstrike/nuke delays
    void PushHint(const std::string& HintId);
    void ActivateCampaignObjectives();
    void EvaluateObjective(FObjective& O);
    bool IsDistrictQuarantined(i32 District) const;
    void ClampResourcesToStorage();
    void MarkWallCorners(size_t FromIndex);

    FDataTables Tables;
    FSimConfig Config;
    FRng Rng;

    i32 Width = 0, Height = 0;
    i32 DistrictCols = 1, DistrictRows = 1;
    std::vector<FTile> TileGrid;
    std::vector<FBuilding> BuildingList;  // never erased (tombstones), index == Id
    std::vector<FWall> WallList;
    std::vector<FCitizen> CitizenList;    // never erased, index == Id
    std::vector<FZombie> ZombieList;
    std::vector<FSquad> SquadList;
    std::vector<FVehicle> VehicleList;
    std::vector<FNoisePing> NoisePingList;
    std::vector<FCraftJob> CraftJobsList;
    std::vector<FNetworkLink> PowerLineList;
    std::vector<FNetworkLink> WaterPipeList;
    std::vector<FItemStack> InventoryList;
    FResources Resources;
    FStats Stats;
    std::deque<FSimEvent> EventLog;
    FNotable NotableEvents;
    std::vector<FCommand> CommandLogList;
    bool bCommandLogEnabled = false;

    float TimeSeconds = 0.f;
    float TickAccumulator = 0.f;
    float Morale = 70.f;
    float QuietDays = 0.f;
    double TreatmentsAccumulator = 0.0;
    bool bFoodShortageTick = false;
    bool bWaterShortageTick = false;

    // scenario multipliers
    float ScenarioZombieSpeed = 1.f;
    float ScenarioInfection = 1.f;
    float ScenarioScarcity = 1.f;

    // per-district network state (computed in UpdateDistrictNetworks)
    std::vector<bool> DistrictPowered;
    std::vector<bool> DistrictWatered;
    std::vector<float> DistrictPowerGen, DistrictPowerCons;
    std::vector<float> DistrictWaterProd, DistrictWaterCons;

    // military
    i32 NextSquadId = 0, NextVehicleId = 0, NextCraftJobId = 0, NextLineId = 0;
    i32 SquadNameCounter = 0;
    i32 TotalZombiesKilled = 0;
    float AirstrikeCooldownDays = 0.f;
    float AirstrikePendingX = -1.f, AirstrikePendingY = -1.f, AirstrikePendingTimer = 0.f;
    bool bAirstrikePending = false;
    float NukePendingX = -1.f, NukePendingY = -1.f, NukePendingTimer = 0.f;
    bool bNukePending = false;
    bool bNuked = false;

    // tech
    std::vector<std::string> ResearchedTechs;
    std::unordered_map<std::string, float> TechModifiers;      // multiplicative, default 1
    std::unordered_map<std::string, bool> Unlocks;             // unlock_* flags
    std::string ActiveTechId;
    float ResearchProgress = 0.f;
    bool bVaccineDeployed = false;

    // environment
    std::string CurrentWeatherId = "clear";
    float WeatherDaysLeft = 1.f;
    std::unordered_map<EPolicy, bool> Policies;

    // horde spawner
    float HordeTimerDays = 0.f;

    // vaccination campaign accumulators (fractional doses per tick)
    double VaccinateAccumulator = 0.0;
    double VaccineCureAccumulator = 0.0;

    // nuclear fallout zones (persistent contamination, slow decay)
    std::vector<char> TileIsFallout;

    // international
    float AidCooldownDays = 0.f;
    float TradeCooldownDays = 0.f;
    float NewsTimerDays = 0.f;
    float EvacCooldownDays = 0.f;
    i32 AidPackages = 0;
    i32 Evacuated = 0;
    i32 CitizensVaccinated = 0;
    std::string LastNewsCity;

    // voting... (misc)
    i32 NextCitizenId = 0, NextZombieId = 0, NextBuildingId = 0, NextWallId = 0;

    EGameResult Result = EGameResult::Ongoing;
    std::string ResultReason;

    // campaign / seasons / versus extras / zones (Phase 2-5)
    std::vector<FObjective> ObjectiveList;
    std::vector<FAchievementUnlock> AchievementList;
    std::vector<FHint> HintList;
    std::vector<char> QuarantineFlags;
    std::string CurrentSeasonId = "spring";
    float SeasonDaysLeft = 90.f;
    float DifficultyBias = 1.f;
    float VirusBoostTimer = 0.f;
    std::string LastHintTrigger;
    std::vector<std::string> ShownHintIds;
};

} // namespace oc
