// Outbreak Commander - Simulation Core
// FSimulation: lifecycle, world generation, player commands, queries.
// Per-system tick logic lives in SimulationStep.cpp.
#include "oc/Simulation.h"

#include <cmath>
#include <cstdio>

namespace oc {

bool FSimulation::LoadData(const std::string& DataDir) {
    return Tables.LoadFromDirectory(DataDir);
}

void FSimulation::NewGame(const FSimConfig& InConfig) {
    Config = InConfig;
    Rng = FRng(Config.Seed);
    Width = Config.MapWidth;
    Height = Config.MapHeight;

    // Custom rule overrides (brief section 11) apply on top of the CSV data.
    for (const auto& O : Config.RuleOverrides) Tables.BalanceValues[O.first] = O.second;

    // Scenario preset: difficulty, city name, outbreak multipliers.
    ScenarioZombieSpeed = 1.f;
    ScenarioInfection = 1.f;
    ScenarioScarcity = 1.f;
    if (const FScenarioDef* Scenario = Tables.FindScenario(Config.ScenarioId)) {
        ApplyScenario(*Scenario);
        if (Scenario->InitialPopulation > 0) Config.InitialPopulation = Scenario->InitialPopulation;
    } else if (Config.CityName.empty()) {
        Config.CityName = "Kingsport";
    }

    TileGrid.assign(static_cast<size_t>(Width) * Height, FTile{});
    TileIsFallout.assign(static_cast<size_t>(Width) * Height, 0);
    BuildingList.clear();
    WallList.clear();
    CitizenList.clear();
    ZombieList.clear();
    EventLog.clear();
    SquadList.clear();
    VehicleList.clear();
    CraftJobsList.clear();
    PowerLineList.clear();
    WaterPipeList.clear();
    InventoryList.clear();
    ResearchedTechs.clear();
    TechModifiers.clear();
    Unlocks.clear();
    ActiveTechId.clear();
    ResearchProgress = 0.f;
    bVaccineDeployed = false;
    Policies.clear();
    CurrentWeatherId = "clear";
    WeatherDaysLeft = 1.f;
    HordeTimerDays = 0.f;  // first automatic horde arrives after HordeIntervalDays
    AidCooldownDays = TradeCooldownDays = NewsTimerDays = EvacCooldownDays = 0.f;
    AidPackages = Evacuated = CitizensVaccinated = 0;
    LastNewsCity.clear();
    NextSquadId = NextVehicleId = NextCraftJobId = NextLineId = 0;
    SquadNameCounter = 0;
    TotalZombiesKilled = 0;
    AirstrikeCooldownDays = 0.f;
    AirstrikePendingX = AirstrikePendingY = -1.f;
    AirstrikePendingTimer = 0.f;
    bAirstrikePending = false;
    NukePendingX = NukePendingY = -1.f;
    NukePendingTimer = 0.f;
    bNukePending = false;
    bNuked = false;
    NotableEvents = FNotable{};
    CommandLogList.clear();
    ObjectiveList.clear();
    AchievementList.clear();
    HintList.clear();
    QuarantineFlags.clear();
    CurrentSeasonId = "spring";
    SeasonDaysLeft = BalanceF("SeasonLengthDays", 90);
    DifficultyBias = 1.f;
    VirusBoostTimer = 0.f;
    LastHintTrigger.clear();
    ShownHintIds.clear();
    VaccinateAccumulator = 0.0;
    VaccineCureAccumulator = 0.0;

    TimeSeconds = 0.f;
    TickAccumulator = 0.f;
    QuietDays = 0.f;
    TreatmentsAccumulator = 0.0;
    bFoodShortageTick = false;
    bWaterShortageTick = false;
    NextCitizenId = NextZombieId = NextBuildingId = NextWallId = 0;
    Result = EGameResult::Ongoing;
    ResultReason.clear();

    // Districts (power/water network granularity).
    const i32 DistrictSize = std::max(1, static_cast<i32>(BalanceF("DistrictSizeTiles", 24)));
    DistrictCols = std::max(1, Width / DistrictSize);
    DistrictRows = std::max(1, Height / DistrictSize);
    for (i32 Y = 0; Y < Height; ++Y)
        for (i32 X = 0; X < Width; ++X)
            TileGrid[static_cast<size_t>(Y) * Width + X].District = (X / DistrictSize) + (Y / DistrictSize) * DistrictCols;
    DistrictPowered.assign(DistrictCount(), true);
    DistrictWatered.assign(DistrictCount(), true);
    DistrictPowerGen.assign(DistrictCount(), 0.f);
    DistrictPowerCons.assign(DistrictCount(), 0.f);
    DistrictWaterProd.assign(DistrictCount(), 0.f);
    DistrictWaterCons.assign(DistrictCount(), 0.f);

    // Starting stockpiles come from Balance.csv (scarcity-scaled), not code.
    Resources = FResources{};
    Resources.Money = BalanceF("StartingMoney", 2000);
    Resources.Food = BalanceF("StartingFood", 1500) / ScenarioScarcity;
    Resources.Water = BalanceF("StartingWater", 1500) / ScenarioScarcity;
    Resources.Fuel = BalanceF("StartingFuel", 200) / ScenarioScarcity;
    Resources.Materials = BalanceF("StartingMaterials", 800) / ScenarioScarcity;
    Resources.Medicine = BalanceF("StartingMedicine", 30) / ScenarioScarcity;
    Resources.Ammo = BalanceF("StartingAmmo", 0);

    GenerateCity();

    // Spawn citizens with names.
    const i32 Pop = std::max(0, Config.InitialPopulation);
    CitizenList.reserve(Pop);
    for (i32 i = 0; i < Pop; ++i) {
        FCitizen C;
        C.Id = NextCitizenId++;
        C.X = Rng.NextF(2.f, static_cast<float>(Width) - 2.f);
        C.Y = Rng.NextF(2.f, static_cast<float>(Height) - 2.f);
        C.bFemale = Rng.NextF01() < 0.5f;
        const auto& Firsts = C.bFemale ? Tables.FirstFemaleNames : Tables.FirstMaleNames;
        const std::string First = Firsts.empty() ? "Citizen" : Firsts[Rng.NextInt(0, static_cast<i32>(Firsts.size()) - 1)];
        const std::string Last = Tables.LastNames.empty() ? "Doe" : Tables.LastNames[Rng.NextInt(0, static_cast<i32>(Tables.LastNames.size()) - 1)];
        C.Name = First + " " + Last;
        if (!Tables.CitizenTypes.empty())
            C.TypeId = Tables.CitizenTypes[static_cast<size_t>(C.Id) % Tables.CitizenTypes.size()].Id;
        CitizenList.push_back(C);
    }
    AssignHomes();

    // Starter infrastructure grid: every district is connected so the city
    // starts powered and watered. The player extends/repairs this network.
    for (i32 D = 0; D < DistrictCount(); ++D) {
        const i32 Col = D % DistrictCols, Row = D / DistrictCols;
        if (Col + 1 < DistrictCols) {
            AddNetworkLink(PowerLineList, D, D + 1);
            AddNetworkLink(WaterPipeList, D, D + 1);
        }
        if (Row + 1 < DistrictRows) {
            AddNetworkLink(PowerLineList, D, D + DistrictCols);
            AddNetworkLink(WaterPipeList, D, D + DistrictCols);
        }
    }

    // Patient Zero starts the outbreak (brief section 2).
    if (Config.bStartWithPatientZero && !CitizenList.empty()) {
        FCitizen& P0 = CitizenList[0];
        P0.bLatentInfection = true;
        P0.State = ECitizenState::Infected;
        P0.InfectionSecondsLeft = BalanceF("PatientZeroIncubationDays", 0.1f) * BalanceF("SecondsPerDay", 60.f);
        NotableEvents.PatientZeroId = P0.Id;
        Log(EEventType::CitizenInfected,
            "Patient Zero (" + P0.Name + ") has been infected. The outbreak has begun.");
    }

    // Scenario "start hordes": waves already en route at game start.
    if (const FScenarioDef* Scenario = Tables.FindScenario(Config.ScenarioId)) {
        for (i32 i = 0; i < Scenario->StartHordes; ++i)
            SpawnHordeWave(static_cast<i32>(BalanceF("HordeBaseSize", 3)) + 2, "the region");
    }

    Morale = BalanceF("MoraleBaseline", 70.f);
    QuarantineFlags.assign(DistrictCount(), 0);
    if (Config.bTutorial && Config.CampaignId.empty()) Config.CampaignId = "tutorial";
    if (!Config.CampaignId.empty()) {
        ActivateCampaignObjectives();
        PushHint("hint_camera");
    }
    UpdateDistrictNetworks();
    UpdateStats();
    Log(EEventType::Milestone, "New game: " + Config.CityName + ". Contain the outbreak.");
}

void FSimulation::Step(float DeltaSeconds) {
    if (Result != EGameResult::Ongoing) return;  // freeze on game over
    const float Tick = BalanceF("SimTickSeconds", 0.25f);
    // Cap the backlog so a long frame hitch can't spiral (max 400 ticks/call).
    TickAccumulator = std::min(TickAccumulator + DeltaSeconds, Tick * 400.f);
    while (TickAccumulator >= Tick && Result == EGameResult::Ongoing) {
        RunTick(Tick);
        TickAccumulator -= Tick;
    }
}

// ---------------------------------------------------------------------------
// Player commands
// ---------------------------------------------------------------------------

i32 FSimulation::PlaceBuilding(const std::string& DefId, i32 X, i32 Y) {
    if (Result != EGameResult::Ongoing) return -1;
    const FBuildingDef* Def = Tables.FindBuilding(DefId);
    if (!Def) { Log(EEventType::Milestone, "Unknown building: " + DefId); return -1; }
    if (!IsInside(X, Y) || !IsInside(X + Def->Width - 1, Y + Def->Height - 1)) return -1;
    if (TilesOverlapBuilding(X, Y, Def->Width, Def->Height)) return -1;
    if (Resources.Money < Def->CostMoney || Resources.Materials < Def->CostMaterials) return -1;

    Resources.Money -= Def->CostMoney;
    Resources.Materials -= Def->CostMaterials;

    FBuilding B;
    B.Id = NextBuildingId++;
    B.DefId = DefId;
    B.X = X; B.Y = Y; B.W = Def->Width; B.H = Def->Height;
    B.Hp = static_cast<float>(Def->MaxHp);
    B.bUnderConstruction = true;
    B.BuildProgress = 0.f;
    BuildingList.push_back(B);

    Log(EEventType::BuildingPlaced, "Construction started: " + Def->Name);
    LogCommand("PlaceBuilding", {DefId, std::to_string(X), std::to_string(Y)});
    return B.Id;
}

i32 FSimulation::BuildWallLine(i32 X1, i32 Y1, i32 X2, i32 Y2) {
    if (Result != EGameResult::Ongoing) return -1;
    if (!IsInside(X1, Y1) || !IsInside(X2, Y2)) return -1;

    // Walk an 8-connected line between tile centers (Bresenham-style).
    const i32 Steps = std::max(std::abs(X2 - X1), std::abs(Y2 - Y1));
    if (Steps == 0) return 0;
    if (Steps > static_cast<i32>(BalanceF("WallMaxSegmentsPerDrag", 64))) return -1;

    struct FSeg { i32 Ax, Ay, Bx, By; };
    std::vector<FSeg> Segments;
    auto Key = [](i32 Ax, i32 Ay, i32 Bx, i32 By) {
        // order-independent key so duplicate/reversed segments dedupe
        if (std::tie(Ax, Ay) > std::tie(Bx, By)) std::swap(Ax, Ay), std::swap(Bx, By);
        return std::to_string(Ax) + "," + std::to_string(Ay) + ";" + std::to_string(Bx) + "," + std::to_string(By);
    };
    std::unordered_map<std::string, bool> Seen;
    for (i32 i = 0; i < Steps; ++i) {
        const i32 Ax = X1 + (X2 - X1) * i / Steps;
        const i32 Ay = Y1 + (Y2 - Y1) * i / Steps;
        const i32 Bx = X1 + (X2 - X1) * (i + 1) / Steps;
        const i32 By = Y1 + (Y2 - Y1) * (i + 1) / Steps;
        if (Ax == Bx && Ay == By) continue;
        const std::string K = Key(Ax, Ay, Bx, By);
        if (Seen.count(K)) continue;
        Seen[K] = true;
        Segments.push_back({Ax, Ay, Bx, By});
    }
    // Filter out segments that already exist so re-dragging a line is free.
    auto WallExists = [this](const FSeg& S) {
        for (const FWall& W : WallList) {
            const bool Same = (W.X1 == S.Ax && W.Y1 == S.Ay && W.X2 == S.Bx && W.Y2 == S.By) ||
                              (W.X1 == S.Bx && W.Y1 == S.By && W.X2 == S.Ax && W.Y2 == S.Ay);
            if (Same) return true;
        }
        return false;
    };
    std::vector<FSeg> NewSegments;
    for (const FSeg& S : Segments)
        if (!WallExists(S)) NewSegments.push_back(S);
    if (NewSegments.empty()) return 0;

    // Reject the whole line atomically if any new segment crosses a building.
    for (const FSeg& S : NewSegments) {
        if (SegmentCrossesBuilding(static_cast<float>(S.Ax), static_cast<float>(S.Ay),
                                   static_cast<float>(S.Bx), static_cast<float>(S.By))) {
            Log(EEventType::Milestone, "Cannot build wall: path crosses a building.");
            return -1;
        }
    }

    const double Cost = static_cast<double>(NewSegments.size()) * BalanceF("WallCostMoneyPerSegment", 50);
    if (Resources.Money < Cost) return -1;
    Resources.Money -= Cost;

    // Wall HP is data-driven and modified by the Reinforced Walls tech.
    const float MaxHp = BalanceF("WallMaxHp", 100) * Modifier("wall_hp");
    const float BuildTime = BalanceF("WallBuildTimeSeconds", 4) /
                            (BalanceF("ConstructionSpeedMultiplier", 1) * Modifier("construction_speed"));
    for (const FSeg& S : NewSegments) {
        FWall W;
        W.Id = NextWallId++;
        W.X1 = static_cast<float>(S.Ax); W.Y1 = static_cast<float>(S.Ay);
        W.X2 = static_cast<float>(S.Bx); W.Y2 = static_cast<float>(S.By);
        W.Hp = MaxHp; W.MaxHp = MaxHp;
        W.bUnderConstruction = BuildTime > 0.f;
        W.BuildProgress = 0.f;
        WallList.push_back(W);
    }
    MarkWallCorners(WallList.size() - NewSegments.size());
    Log(EEventType::WallBuilt, "Wall line ordered: " + std::to_string(NewSegments.size()) + " segments.");
    LogCommand("BuildWallLine", {std::to_string(X1), std::to_string(Y1), std::to_string(X2), std::to_string(Y2)});
    return static_cast<i32>(NewSegments.size());
}

bool FSimulation::DemolishBuilding(i32 BuildingId) {
    if (Result != EGameResult::Ongoing) return false;
    if (BuildingId < 0 || BuildingId >= static_cast<i32>(BuildingList.size())) return false;
    FBuilding& B = BuildingList[BuildingId];
    if (B.bDemolished) return false;
    const FBuildingDef* Def = Tables.FindBuilding(B.DefId);
    B.bDemolished = true;
    if (Def) {
        const float Refund = BalanceF("DemolishRefundPercent", 0.5f);
        Resources.Money += Def->CostMoney * Refund;
        Resources.Materials += Def->CostMaterials * Refund;
    }
    Log(EEventType::BuildingDestroyed, "Building demolished: " + B.DefId);
    AssignHomes();
    LogCommand("DemolishBuilding", {std::to_string(BuildingId)});
    return true;
}

// ---------------------------------------------------------------------------
// World generation
// ---------------------------------------------------------------------------

void FSimulation::GenerateCity() {
    // Roads on a regular grid, districts as 24x24 tile blocks.
    for (i32 Y = 0; Y < Height; ++Y) {
        for (i32 X = 0; X < Width; ++X) {
            FTile& T = TileGrid[static_cast<size_t>(Y) * Width + X];
            T.bRoad = (X % 8 == 0) || (Y % 8 == 0);
            T.District = ((X / 24) + (Y / 24) * 4) % 4;
        }
    }

    // Fixed starting infrastructure so the prototype is immediately playable:
    // one power plant, one medical center, one farm, one water plant.
    struct FStart { const char* Def; i32 X, Y; };
    const FStart Starts[] = {
        {"power_plant", 4, 4},
        {"medical_center", 12, 6},
        {"farm", 6, 14},
        {"water_purification", 16, 12},
    };
    for (const FStart& S : Starts) {
        const FBuildingDef* Def = Tables.FindBuilding(S.Def);
        if (!Def) continue;
        FBuilding B;
        B.Id = NextBuildingId++;
        B.DefId = S.Def;
        B.X = S.X; B.Y = S.Y; B.W = Def->Width; B.H = Def->Height;
        B.Hp = static_cast<float>(Def->MaxHp);
        B.bUnderConstruction = false;  // pre-built
        B.BuildProgress = Def->BuildTimeSeconds;
        BuildingList.push_back(B);
    }

    // Scatter houses until there is enough housing (with headroom).
    const i32 Pop = std::max(0, Config.InitialPopulation);
    i32 Housing = 0;
    for (const FBuilding& B : BuildingList) {
        const FBuildingDef* Def = Tables.FindBuilding(B.DefId);
        if (Def) Housing += Def->Housing;
    }
    i32 Attempts = 0;
    const FBuildingDef* House = Tables.FindBuilding("house");
    while (Housing < Pop + Pop / 8 && House && Attempts < 2000) {
        ++Attempts;
        const i32 X = Rng.NextInt(1, Width - House->Width - 2);
        const i32 Y = Rng.NextInt(1, Height - House->Height - 2);
        if (TilesOverlapBuilding(X, Y, House->Width, House->Height)) continue;
        FBuilding B;
        B.Id = NextBuildingId++;
        B.DefId = "house";
        B.X = X; B.Y = Y; B.W = House->Width; B.H = House->Height;
        B.Hp = static_cast<float>(House->MaxHp);
        B.bUnderConstruction = false;
        B.BuildProgress = House->BuildTimeSeconds;
        BuildingList.push_back(B);
        Housing += House->Housing;
    }
}

void FSimulation::AssignHomes() {
    for (FCitizen& C : CitizenList) {
        if (C.State == ECitizenState::Dead || C.State == ECitizenState::Turned) continue;
        C.HomeId = -1;
    }
    // Gather free housing capacity.
    std::vector<std::pair<i32, i32>> Slots;  // (building id, capacity)
    for (const FBuilding& B : BuildingList) {
        if (B.bDemolished || B.bUnderConstruction) continue;
        const FBuildingDef* Def = Tables.FindBuilding(B.DefId);
        if (Def && Def->Housing > 0) Slots.emplace_back(B.Id, Def->Housing);
    }
    std::vector<i32> Used(Slots.size(), 0);
    for (FCitizen& C : CitizenList) {
        if (C.State == ECitizenState::Dead || C.State == ECitizenState::Turned) continue;
        for (size_t i = 0; i < Slots.size(); ++i) {
            if (Used[i] < Slots[i].second) {
                C.HomeId = Slots[i].first;
                ++Used[i];
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Geometry helpers
// ---------------------------------------------------------------------------

const FTile& FSimulation::TileAt(i32 X, i32 Y) const {
    static const FTile Dummy;
    if (!IsInside(X, Y)) return Dummy;
    return TileGrid[static_cast<size_t>(Y) * Width + X];
}

bool FSimulation::TilesOverlapBuilding(i32 X, i32 Y, i32 W, i32 H) const {
    for (const FBuilding& B : BuildingList) {
        if (B.bDemolished) continue;
        const bool Overlap = X < B.X + B.W && X + W > B.X && Y < B.Y + B.H && Y + H > B.Y;
        if (Overlap) return true;
    }
    return false;
}

bool FSimulation::SegmentCrossesBuilding(float X1, float Y1, float X2, float Y2) const {
    const float Len = std::hypot(X2 - X1, Y2 - Y1);
    const i32 Samples = std::max(1, static_cast<i32>(Len * 4));
    for (i32 i = 0; i <= Samples; ++i) {
        const float T = static_cast<float>(i) / Samples;
        const i32 Tx = static_cast<i32>(std::lround(X1 + (X2 - X1) * T));
        const i32 Ty = static_cast<i32>(std::lround(Y1 + (Y2 - Y1) * T));
        for (const FBuilding& B : BuildingList) {
            if (B.bDemolished) continue;
            if (Tx >= B.X && Tx < B.X + B.W && Ty >= B.Y && Ty < B.Y + B.H) return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Debug hooks
// ---------------------------------------------------------------------------

void FSimulation::DebugInfect(i32 CitizenId) {
    InfectCitizen(CitizenId, "debug");
}

i32 FSimulation::DebugSpawnZombie(const std::string& TypeId, float X, float Y) {
    const FZombieTypeDef* Def = Tables.FindZombieType(TypeId);
    if (!Def) return -1;
    FZombie Z;
    Z.Id = NextZombieId++;
    Z.TypeId = TypeId;
    Z.X = std::clamp(X, 0.5f, static_cast<float>(Width) - 1.5f);
    Z.Y = std::clamp(Y, 0.5f, static_cast<float>(Height) - 1.5f);
    Z.Hp = Def->Hp;
    ZombieList.push_back(Z);
    return Z.Id;
}

void FSimulation::DebugTeleportCitizen(i32 CitizenId, float X, float Y) {
    if (CitizenId < 0 || CitizenId >= static_cast<i32>(CitizenList.size())) return;
    FCitizen& C = CitizenList[CitizenId];
    C.X = std::clamp(X, 0.5f, static_cast<float>(Width) - 1.5f);
    C.Y = std::clamp(Y, 0.5f, static_cast<float>(Height) - 1.5f);
    C.bHasTarget = false;
}

// ---------------------------------------------------------------------------
// Misc queries
// ---------------------------------------------------------------------------

i32 FSimulation::CountAliveCitizens() const {
    i32 N = 0;
    for (const FCitizen& C : CitizenList)
        if (C.State == ECitizenState::Healthy || C.State == ECitizenState::Infected ||
            C.State == ECitizenState::Panicked)
            ++N;
    return N;
}

i32 FSimulation::NearestZombie(float X, float Y, float MaxDist) const {
    // Returns the INDEX into ZombieList. Zombies are erased when killed, so a
    // zombie's Id is NOT its index; callers use the result to index the list
    // (returning the Id here caused wrong-target reads and out-of-bounds UB).
    i32 Best = -1;
    float BestD = MaxDist;
    for (size_t i = 0; i < ZombieList.size(); ++i) {
        const float D = std::hypot(ZombieList[i].X - X, ZombieList[i].Y - Y);
        if (D < BestD) { BestD = D; Best = static_cast<i32>(i); }
    }
    return Best;
}

u64 FSimulation::StateHash() const {
    // FNV-1a over the FULL meaningful state (including RNG state and timers);
    // used to prove determinism across tests, save/load, and replays.
    u64 H = 1469598103934665603ull;
    auto Mix = [&H](u64 V) { H ^= V; H *= 1099511628211ull; };
    auto MixF = [&H](float V) { H ^= static_cast<u64>(std::lround(V * 1000.f)); H *= 1099511628211ull; };
    MixF(TimeSeconds);
    MixF(TickAccumulator);
    Mix(Rng.State);
    Mix(static_cast<u64>(Resources.Money * 100) ^ static_cast<u64>(Resources.Food * 100) << 8);
    Mix(static_cast<u64>(ZombieList.size()) * 131 + CitizenList.size());
    for (const FCitizen& C : CitizenList) {
        Mix(static_cast<u64>(C.State) * 1000003ull + static_cast<u64>(std::lround(C.X * 16)) * 31 +
            static_cast<u64>(std::lround(C.Y * 16)) + (C.bLatentInfection ? 7ull : 0ull));
        MixF(C.Panic); MixF(C.Hunger); MixF(C.Thirst); MixF(C.Health);
        MixF(C.InfectionSecondsLeft); MixF(C.TargetX); MixF(C.TargetY);
        Mix(C.bHasTarget ? 1 : 0); Mix(C.bVaccinated ? 1 : 0); Mix(static_cast<u64>(C.HomeId));
    }
    for (const FZombie& Z : ZombieList) {
        Mix(static_cast<u64>(std::lround(Z.X * 16)) * 17 + static_cast<u64>(std::lround(Z.Y * 16)) * 3 +
            static_cast<u64>(std::lround(Z.Hp)));
        MixF(Z.WanderTimer); MixF(Z.ChaseX); MixF(Z.ChaseY); MixF(Z.ChaseTimer);
        MixF(Z.AttackCooldown); Mix(static_cast<u64>(Z.TargetCitizen));
        MixF(Z.SlideTimer); MixF(Z.RetreatTimer); MixF(Z.AlertTimer);
    }
    for (const FNoisePing& P : NoisePingList) {
        MixF(P.X); MixF(P.Y); MixF(P.Radius); MixF(P.Timer);
        Mix(P.bAlert ? 1 : 0);
    }
    for (const FWall& W : WallList) {
        Mix(static_cast<u64>(W.Id) + static_cast<u64>(std::lround(W.Hp)) * 101);
        MixF(W.BuildProgress); Mix(W.bUnderConstruction ? 1 : 0);
    }
    MixF(Morale);
    // Extended state (military / tech / environment / international).
    for (const FBuilding& B : BuildingList) {
        Mix(static_cast<u64>(B.Id) + (B.bDemolished ? 1000000ull : 0ull));
        if (!B.bDemolished) {
            MixF(B.Hp); MixF(B.BuildProgress);
            Mix(B.bPowered ? 1 : 0); Mix(B.bFuelStarved ? 1 : 0); Mix(B.bUnderConstruction ? 1 : 0);
            Mix(static_cast<u64>(B.Level));
        }
    }
    for (const FSquad& S : SquadList) {
        if (S.bWiped) continue;
        Mix(static_cast<u64>(S.Id) * 31 + static_cast<u64>(S.Kills) * 3 + static_cast<u64>(std::lround(S.Hp)));
        MixF(S.X); MixF(S.Y); MixF(S.TrainSecondsLeft); MixF(S.TargetX); MixF(S.TargetY);
        MixF(S.NoiseTimer); Mix(static_cast<u64>(S.Order)); Mix(static_cast<u64>(S.State));
        Mix(static_cast<u64>(S.Size));
    }
    for (const FVehicle& V : VehicleList) {
        if (V.bDestroyed) continue;
        Mix(static_cast<u64>(V.Id) * 17 + static_cast<u64>(std::lround(V.Hp)));
        MixF(V.X); MixF(V.Y); MixF(V.TargetX); MixF(V.TargetY);
        Mix(V.bHasTarget ? 1 : 0); Mix(static_cast<u64>(V.SquadId));
    }
    Mix(static_cast<u64>(TotalZombiesKilled) * 131 + static_cast<u64>(ResearchedTechs.size()) * 17 +
        static_cast<u64>(std::lround(ResearchProgress * 100)));
    Mix(static_cast<u64>(std::lround(Resources.Ammo * 100)) ^ static_cast<u64>(std::lround(Resources.Research * 100)) << 8);
    Mix(static_cast<u64>(Resources.Water * 100) ^ static_cast<u64>(Resources.Fuel * 100) << 8);
    Mix(static_cast<u64>(Resources.Materials * 100) ^ static_cast<u64>(Resources.Medicine * 100) << 8);
    Mix(static_cast<u64>(Resources.PowerGeneration * 100) ^ static_cast<u64>(Resources.PowerConsumption * 100) << 8);
    for (const FItemStack& I : InventoryList)
        Mix(static_cast<u64>(std::lround(I.Durability * 100)) + static_cast<u64>(I.Count) * 131 +
            [&] { u64 X = 0; for (char C : I.ItemId) X = X * 31 + static_cast<u64>(C); return X; }());
    Mix(static_cast<u64>(Evacuated) * 13 + static_cast<u64>(CitizensVaccinated) * 7 +
        static_cast<u64>(AidPackages) * 11);
    for (const char C : CurrentWeatherId) Mix(static_cast<u64>(C));
    MixF(WeatherDaysLeft); MixF(HordeTimerDays); MixF(NewsTimerDays); MixF(QuietDays);
    MixF(AidCooldownDays); MixF(TradeCooldownDays); MixF(EvacCooldownDays);
    MixF(TreatmentsAccumulator); MixF(VaccinateAccumulator); MixF(VaccineCureAccumulator);
    for (const auto& P : Policies) Mix(static_cast<u64>(P.first) * 17 + (P.second ? 1 : 0));
    for (const auto& T : ResearchedTechs) for (char C : T) Mix(static_cast<u64>(C));
    for (const char C : ActiveTechId) Mix(static_cast<u64>(C));
    Mix(bFoodShortageTick ? 1 : 0); Mix(bWaterShortageTick ? 1 : 0);
    Mix(static_cast<u64>(bVaccineDeployed) * 1000003 + static_cast<u64>(bNuked) * 999983);
    Mix(static_cast<u64>(SquadNameCounter) + static_cast<u64>(NextLineId) * 31 +
        static_cast<u64>(NextCraftJobId) * 17);
    for (const FCraftJob& J : CraftJobsList)
        Mix(static_cast<u64>(J.Id) + static_cast<u64>(std::lround(J.Progress * 100)) * 13);
    Mix(static_cast<u64>(NotableEvents.PatientZeroId) + static_cast<u64>(NotableEvents.FirstTurnedCitizenId) * 7 +
        static_cast<u64>(NotableEvents.FirstCuredCitizenId) * 11);
    for (const FTile& T : TileGrid) MixF(T.Contamination);
    for (char F : TileIsFallout) Mix(static_cast<u64>(F));
    MixF(SeasonDaysLeft); MixF(DifficultyBias); MixF(VirusBoostTimer);
    for (char C : CurrentSeasonId) Mix(static_cast<u64>(C));
    Mix(static_cast<u64>(ObjectiveList.size()) * 13 + static_cast<u64>(AchievementList.size()) * 17);
    for (const FObjective& O : ObjectiveList)
        Mix(static_cast<u64>(O.State) * 31 + static_cast<u64>(O.Progress));
    for (char Q : QuarantineFlags) Mix(static_cast<u64>(Q));
    for (const FWall& W : WallList) Mix((W.bGate ? 3ull : 0ull) + (W.bCorner ? 5ull : 0ull));
    return H;
}

void FSimulation::Log(EEventType Type, const std::string& Msg) {
    EventLog.push_back({Type, Msg, TimeSeconds});
    const size_t MaxEvents = static_cast<size_t>(BalanceF("MaxEventsLogged", 200));
    while (EventLog.size() > MaxEvents) EventLog.pop_front();
}

void FSimulation::LogCommand(const std::string& Verb, std::vector<std::string> Args) {
    if (!bCommandLogEnabled) return;
    CommandLogList.push_back({TimeSeconds, Verb, std::move(Args)});
}

// ---------------------------------------------------------------------------
// Districts & infrastructure networks
// ---------------------------------------------------------------------------

i32 FSimulation::DistrictAt(i32 X, i32 Y) const {
    if (!IsInside(X, Y)) return 0;
    const i32 DistrictSize = std::max(1, static_cast<i32>(BalanceF("DistrictSizeTiles", 24)));
    return (X / DistrictSize) + (Y / DistrictSize) * DistrictCols;
}

i32 FSimulation::DistrictOfBuilding(i32 BuildingId) const {
    if (BuildingId < 0 || BuildingId >= static_cast<i32>(BuildingList.size())) return 0;
    const FBuilding& B = BuildingList[BuildingId];
    return DistrictAt(B.X, B.Y);
}

bool FSimulation::IsDistrictPowered(i32 District) const {
    return District >= 0 && District < static_cast<i32>(DistrictPowered.size()) && DistrictPowered[District];
}

bool FSimulation::IsDistrictWatered(i32 District) const {
    return District >= 0 && District < static_cast<i32>(DistrictWatered.size()) && DistrictWatered[District];
}

i32 FSimulation::AddNetworkLink(std::vector<FNetworkLink>& List, i32 A, i32 B) {
    if (A == B || A < 0 || B < 0 || A >= DistrictCount() || B >= DistrictCount()) return -1;
    for (const FNetworkLink& L : List)
        if ((L.DistrictA == A && L.DistrictB == B) || (L.DistrictA == B && L.DistrictB == A)) return -1;
    FNetworkLink L;
    L.Id = NextLineId++;
    L.DistrictA = A;
    L.DistrictB = B;
    List.push_back(L);
    return L.Id;
}

i32 FSimulation::BuildPowerLine(i32 DistrictA, i32 DistrictB) {
    if (Result != EGameResult::Ongoing) return -1;
    const double Cost = BalanceF("PowerLineCostMaterials", 50);
    if (Resources.Materials < Cost) return -1;
    const i32 Id = AddNetworkLink(PowerLineList, DistrictA, DistrictB);
    if (Id < 0) return -1;
    Resources.Materials -= Cost;
    Log(EEventType::Milestone, "Power line built between districts " + std::to_string(DistrictA) +
        " and " + std::to_string(DistrictB) + ".");
    LogCommand("BuildPowerLine", {std::to_string(DistrictA), std::to_string(DistrictB)});
    return Id;
}

i32 FSimulation::BuildWaterPipe(i32 DistrictA, i32 DistrictB) {
    if (Result != EGameResult::Ongoing) return -1;
    const double Cost = BalanceF("WaterPipeCostMaterials", 50);
    if (Resources.Materials < Cost) return -1;
    const i32 Id = AddNetworkLink(WaterPipeList, DistrictA, DistrictB);
    if (Id < 0) return -1;
    Resources.Materials -= Cost;
    Log(EEventType::Milestone, "Water pipe built between districts " + std::to_string(DistrictA) +
        " and " + std::to_string(DistrictB) + ".");
    LogCommand("BuildWaterPipe", {std::to_string(DistrictA), std::to_string(DistrictB)});
    return Id;
}

static bool RemoveNetworkLink(std::vector<oc::FNetworkLink>& List, oc::i32 A, oc::i32 B) {
    for (auto It = List.begin(); It != List.end(); ++It) {
        if ((It->DistrictA == A && It->DistrictB == B) || (It->DistrictA == B && It->DistrictB == A)) {
            List.erase(It);
            return true;
        }
    }
    return false;
}

bool FSimulation::DemolishPowerLine(i32 DistrictA, i32 DistrictB) {
    if (RemoveNetworkLink(PowerLineList, DistrictA, DistrictB)) {
        Log(EEventType::Milestone, "Power line cut between districts " + std::to_string(DistrictA) +
            " and " + std::to_string(DistrictB) + ".");
        LogCommand("DemolishPowerLine", {std::to_string(DistrictA), std::to_string(DistrictB)});
        return true;
    }
    return false;
}

bool FSimulation::DemolishWaterPipe(i32 DistrictA, i32 DistrictB) {
    if (RemoveNetworkLink(WaterPipeList, DistrictA, DistrictB)) {
        Log(EEventType::Milestone, "Water pipe cut between districts " + std::to_string(DistrictA) +
            " and " + std::to_string(DistrictB) + ".");
        LogCommand("DemolishWaterPipe", {std::to_string(DistrictA), std::to_string(DistrictB)});
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Scenario, tech tree, modifiers
// ---------------------------------------------------------------------------

void FSimulation::ApplyScenario(const FScenarioDef& S) {
    ScenarioZombieSpeed = S.ZombieSpeedMultiplier;
    ScenarioInfection = S.InfectionRateMultiplier;
    ScenarioScarcity = S.ResourceScarcityMultiplier;
    if (!S.CityName.empty()) Config.CityName = S.CityName;
    Tables.BalanceValues["PatientZeroIncubationDays"] = S.PatientZeroIncubationDays;
}

bool FSimulation::StartResearch(const std::string& TechId) {
    if (Result != EGameResult::Ongoing) return false;
    const FTechDef* Def = Tables.FindTech(TechId);
    if (!Def) return false;
    if (IsTechResearched(TechId)) return false;
    if (!ActiveTechId.empty()) return false;  // one project at a time
    for (const auto& P : Def->Prerequisites) {
        if (!IsTechResearched(P)) {
            Log(EEventType::Milestone, "Research blocked: '" + Def->Name + "' requires '" + P + "'.");
            return false;
        }
    }
    ActiveTechId = TechId;
    Log(EEventType::Milestone, "Research started: " + Def->Name);
    LogCommand("StartResearch", {TechId});
    return true;
}

bool FSimulation::IsTechResearched(const std::string& TechId) const {
    return std::find(ResearchedTechs.begin(), ResearchedTechs.end(), TechId) != ResearchedTechs.end();
}

bool FSimulation::IsUnlocked(const std::string& Feature) const {
    auto It = Unlocks.find(Feature);
    return It != Unlocks.end() && It->second;
}

float FSimulation::Modifier(const std::string& Key) const {
    auto It = TechModifiers.find(Key);
    return It != TechModifiers.end() ? It->second : 1.f;
}

void FSimulation::ApplyTechEffect(const FTechDef& T) {
    if (T.EffectKey == "deploy_vaccine") {
        bVaccineDeployed = true;
        NotableEvents.VaccineDeployedDay = TimeSeconds / BalanceF("SecondsPerDay", 60.f);
        Log(EEventType::VaccineDeployed,
            "VACCINE DEPLOYED: the vaccination campaign begins. The endgame is in sight.");
    } else if (T.EffectKey.rfind("unlock_", 0) == 0) {
        Unlocks[T.EffectKey] = true;
    } else {
        // Multiplicative modifier, defaulting to 1.0 (unordered_map::operator[]
        // would value-initialize to 0.0 and zero out the modifier).
        auto It = TechModifiers.find(T.EffectKey);
        if (It != TechModifiers.end()) It->second *= T.EffectValue;
        else TechModifiers.emplace(T.EffectKey, T.EffectValue);
    }
}

void FSimulation::RecomputeTechEffects() {
    TechModifiers.clear();
    Unlocks.clear();
    bVaccineDeployed = false;
    for (const auto& Id : ResearchedTechs)
        if (const FTechDef* T = Tables.FindTech(Id)) ApplyTechEffect(*T);
}

// ---------------------------------------------------------------------------
// Items & crafting
// ---------------------------------------------------------------------------

i32 FSimulation::ItemCount(const std::string& ItemId) const {
    for (const auto& S : InventoryList)
        if (S.ItemId == ItemId) return S.Count;
    return 0;
}

bool FSimulation::AddItem(const std::string& ItemId, i32 Count) {
    if (Count <= 0) return false;
    if (!Tables.FindItem(ItemId)) return false;
    for (auto& S : InventoryList) {
        if (S.ItemId == ItemId) {
            S.Count += Count;
            S.Durability = 1.f;
            return true;
        }
    }
    FItemStack S;
    S.ItemId = ItemId;
    S.Count = Count;
    S.Durability = 1.f;
    InventoryList.push_back(S);
    return true;
}

bool FSimulation::RemoveItem(const std::string& ItemId, i32 Count) {
    for (auto It = InventoryList.begin(); It != InventoryList.end(); ++It) {
        if (It->ItemId == ItemId) {
            if (It->Count < Count) return false;
            It->Count -= Count;
            if (It->Count == 0) InventoryList.erase(It);
            return true;
        }
    }
    return false;
}

bool FSimulation::QueueRecipe(const std::string& RecipeId) {
    if (Result != EGameResult::Ongoing) return false;
    const FRecipeDef* Def = Tables.FindRecipe(RecipeId);
    if (!Def) return false;
    // Find a powered workshop.
    i32 WorkshopId = -1;
    for (const FBuilding& B : BuildingList) {
        if (B.bDemolished || B.bUnderConstruction || !B.bPowered) continue;
        const FBuildingDef* Bd = Tables.FindBuilding(B.DefId);
        if (Bd && Bd->Function == "crafting") { WorkshopId = B.Id; break; }
    }
    if (WorkshopId < 0) {
        Log(EEventType::Milestone, "No powered workshop available.");
        return false;
    }
    // Check inputs (items and resources).
    for (const auto& In : Def->Inputs) {
        if (Tables.FindItem(In.first)) {
            if (ItemCount(In.first) < In.second) return false;
        } else if (const double* Slot = ResourceSlot(Resources, In.first)) {
            if (*Slot < In.second) return false;
        } else return false;
    }
    // Consume inputs.
    for (const auto& In : Def->Inputs) {
        if (Tables.FindItem(In.first)) RemoveItem(In.first, In.second);
        else if (double* Slot = ResourceSlot(Resources, In.first)) *Slot -= In.second;
    }
    FCraftJob J;
    J.Id = NextCraftJobId++;
    J.RecipeId = RecipeId;
    J.WorkshopId = WorkshopId;
    J.Progress = 0.f;
    J.Duration = Def->TimeSeconds;
    CraftJobsList.push_back(J);
    Log(EEventType::Milestone, "Crafting started: " + Def->Name);
    LogCommand("QueueRecipe", {RecipeId});
    return true;
}

// ---------------------------------------------------------------------------
// Emergency policies
// ---------------------------------------------------------------------------

void FSimulation::SetPolicy(EPolicy Policy, bool bEnabled) {
    Policies[Policy] = bEnabled;
    const char* Name = Policy == EPolicy::Curfew ? "Curfew"
        : Policy == EPolicy::Quarantine ? "Quarantine" : "Martial law";
    Log(EEventType::PolicyChanged, std::string(bEnabled ? "Policy enacted: " : "Policy lifted: ") + Name + ".");
    LogCommand("SetPolicy", {std::to_string(static_cast<int>(Policy)), bEnabled ? "1" : "0"});
}

// ---------------------------------------------------------------------------
// International layer (aid, trade, evacuation, news)
// ---------------------------------------------------------------------------

std::string FSimulation::RandomWorldCity() {
    if (Tables.WorldCities.empty()) return "the international community";
    const FWorldCity& C = Tables.WorldCities[Rng.NextInt(0, static_cast<i32>(Tables.WorldCities.size()) - 1)];
    return C.City + ", " + C.Country;
}

bool FSimulation::RequestAid() {
    if (Result != EGameResult::Ongoing) return false;
    if (bNuked) {
        Log(EEventType::Milestone, "Aid request refused: the world condemns the atrocity.");
        return false;
    }
    if (AidCooldownDays > 0.f) return false;
    const double Amount = BalanceF("AidAmountMoney", 1500);
    Resources.Money += Amount;
    ++AidPackages;
    AidCooldownDays = BalanceF("AidRequestCooldownDays", 5);
    Log(EEventType::AidReceived, "International aid arrived from " + RandomWorldCity() + ": $" +
        std::to_string(static_cast<i32>(Amount)) + ".");
    LogCommand("RequestAid", {});
    return true;
}

bool FSimulation::SendTradeCaravan() {
    if (Result != EGameResult::Ongoing) return false;
    if (TradeCooldownDays > 0.f) return false;
    const double Cost = BalanceF("TradeCaravanMaterialsCost", 500);
    const double Gain = BalanceF("TradeCaravanMoneyGain", 1500);
    if (Resources.Materials < Cost) return false;
    Resources.Materials -= Cost;
    Resources.Money += Gain;
    TradeCooldownDays = BalanceF("TradeCaravanCooldownDays", 3);
    if (Rng.NextF01() < 0.4f) AddItem("medkit", 1);  // caravans sometimes bring supplies
    Log(EEventType::TradeComplete, "Trade caravan returned from " + RandomWorldCity() + ": +$" +
        std::to_string(static_cast<i32>(Gain)) + ".");
    LogCommand("SendTradeCaravan", {});
    return true;
}

bool FSimulation::CallEvacuationFlight() {
    if (Result != EGameResult::Ongoing) return false;
    if (EvacCooldownDays > 0.f) return false;
    const double Cost = BalanceF("EvacuationFlightCostMoney", 5000);
    if (Resources.Money < Cost) return false;
    const i32 Capacity = static_cast<i32>(BalanceF("EvacuationFlightCapacity", 50));
    Resources.Money -= Cost;
    i32 Taken = 0;
    for (FCitizen& C : CitizenList) {
        if (Taken >= Capacity) break;
        if (C.bLatentInfection) continue;  // never fly out the infected
        if (C.State == ECitizenState::Healthy || C.State == ECitizenState::Panicked) {
            C.State = ECitizenState::Evacuated;
            ++Taken;
        }
    }
    Evacuated += Taken;
    EvacCooldownDays = BalanceF("EvacuationFlightCooldownDays", 2);
    Log(EEventType::EvacuationDeparted, "Evacuation flight departed for " + RandomWorldCity() +
        " with " + std::to_string(Taken) + " survivors.");
    LogCommand("CallEvacuationFlight", {});
    return true;
}

// ---------------------------------------------------------------------------
// Versus mode: the virus player's commands
// ---------------------------------------------------------------------------

i32 FSimulation::VirusSpawnHorde(i32 X, i32 Y, const std::string& TypeId, i32 Count) {
    if (!Config.bVersusMode) return -1;
    i32 Spawned = 0;
    for (i32 i = 0; i < Count; ++i)
        if (SpawnZombieOfType(TypeId, X + Rng.NextF(-2.f, 2.f), Y + Rng.NextF(-2.f, 2.f)) >= 0) ++Spawned;
    Log(EEventType::HordeIncoming,
        "The virus released a horde of " + std::to_string(Spawned) + " " + TypeId + "s.");
    LogCommand("VirusSpawnHorde", {std::to_string(X), std::to_string(Y), TypeId, std::to_string(Count)});
    return Spawned;
}

// ---------------------------------------------------------------------------
// Spawning & combat helpers
// ---------------------------------------------------------------------------

i32 FSimulation::SpawnZombieOfType(const std::string& TypeId, float X, float Y) {
    const FZombieTypeDef* Def = Tables.FindZombieType(TypeId);
    if (!Def) return -1;
    FZombie Z;
    Z.Id = NextZombieId++;
    Z.TypeId = TypeId;
    Z.X = std::clamp(X, 0.5f, static_cast<float>(Width) - 1.5f);
    Z.Y = std::clamp(Y, 0.5f, static_cast<float>(Height) - 1.5f);
    Z.Hp = Def->Hp;
    ZombieList.push_back(Z);
    return Z.Id;
}

void FSimulation::SpawnHordeWave(i32 Count, const char* Origin) {
    if (Count <= 0) return;
    // Composition escalates with the day (data-driven unlock days).
    const float Day = TimeSeconds / BalanceF("SecondsPerDay", 60.f);
    i32 Runners = 0, Tanks = 0, Spitters = 0, Mutants = 0;
    if (Day >= BalanceF("RunnerUnlockDay", 5)) Runners = Count / 3;
    if (Day >= BalanceF("TankUnlockDay", 10)) Tanks = Count / 6;
    if (Day >= BalanceF("SpitterUnlockDay", 15)) Spitters = Count / 8;
    if (Day >= BalanceF("MutantUnlockDay", 25) && Rng.NextF01() < BalanceF("MutantChancePerWave", 0.25f))
        Mutants = 1;
    const i32 Walkers = std::max(1, Count - Runners - Tanks - Spitters - Mutants);

    auto SpawnAt = [this](const std::string& Type, i32 N) {
        for (i32 i = 0; i < N; ++i) {
            float X, Y;
            switch (Rng.NextInt(0, 3)) {
            case 0: X = Rng.NextF(1.f, Width - 1.f); Y = 1.f; break;
            case 1: X = Rng.NextF(1.f, Width - 1.f); Y = Height - 2.f; break;
            case 2: X = 1.f; Y = Rng.NextF(1.f, Height - 1.f); break;
            default: X = Width - 2.f; Y = Rng.NextF(1.f, Height - 1.f); break;
            }
            SpawnZombieOfType(Type, X, Y);
        }
    };
    SpawnAt("walker", Walkers);
    SpawnAt("runner", Runners);
    SpawnAt("tank", Tanks);
    SpawnAt("spitter", Spitters);
    SpawnAt("mutant", Mutants);
    Log(EEventType::HordeIncoming, "A horde of " + std::to_string(Count) + " zombies approaches from " + Origin + "!");
}

bool FSimulation::ConsumeAmmo(double Amount) {
    if (Amount <= 0.0) return true;
    if (Resources.Ammo < Amount) return false;
    Resources.Ammo -= Amount;
    return true;
}

std::string FSimulation::SquadName() {
    if (Tables.SquadNames.empty()) return "Squad-" + std::to_string(SquadNameCounter + 1);
    const std::string& Word = Tables.SquadNames[SquadNameCounter % Tables.SquadNames.size()];
    ++SquadNameCounter;
    return Word + "-" + std::to_string(SquadNameCounter);
}

float FSimulation::ZombieSpeedScale() const {
    float Scale = ScenarioZombieSpeed;
    if (const FWeatherDef* W = Tables.FindWeather(CurrentWeatherId)) Scale *= W->ZombieActivityMultiplier;
    Scale *= IsNight() ? BalanceF("ZombieNightActivityMultiplier", 1.5f)
                       : BalanceF("ZombieDayActivityMultiplier", 0.7f);
    Scale *= SeasonZombieMult();
    return Scale;
}

float FSimulation::InfectionScale() const {
    float Scale = ScenarioInfection;
    if (HasPolicy(EPolicy::Curfew)) Scale *= BalanceF("CurfewInfectionMultiplier", 0.5f);
    if (HasPolicy(EPolicy::Quarantine)) Scale *= BalanceF("QuarantineInfectionMultiplier", 0.7f);
    return Scale;
}

bool FSimulation::IsNight() const {
    const float Frac = DayFraction();
    return Frac >= BalanceF("NightStartFraction", 0.8f) || Frac < BalanceF("NightEndFraction", 0.2f);
}

float FSimulation::DayFraction() const {
    const float SecPerDay = BalanceF("SecondsPerDay", 60.f);
    return std::fmod(TimeSeconds, SecPerDay) / SecPerDay;
}

std::string FSimulation::CitizenName(i32 CitizenId) const {
    if (CitizenId < 0 || CitizenId >= static_cast<i32>(CitizenList.size())) return "?";
    return CitizenList[CitizenId].Name;
}

void FSimulation::KillZombie(i32 ZombieIndex, i32 SquadId) {
    if (ZombieIndex < 0 || ZombieIndex >= static_cast<i32>(ZombieList.size())) return;
    const FZombie Z = ZombieList[ZombieIndex];  // copy before erase
    ZombieList.erase(ZombieList.begin() + ZombieIndex);
    ++TotalZombiesKilled;
    // Death alert: nearby zombies converge on the kill (and speed up).
    FNoisePing Ping;
    Ping.X = Z.X;
    Ping.Y = Z.Y;
    Ping.Radius = BalanceF("DeathAlertRadiusTiles", 10);
    Ping.Timer = 1.5f;
    Ping.bAlert = true;
    NoisePingList.push_back(Ping);
    if (SquadId >= 0 && SquadId < static_cast<i32>(SquadList.size()) && !SquadList[SquadId].bWiped) {
        ++SquadList[SquadId].Kills;
        if (NotableEvents.FirstZombieKilledSquadId < 0) NotableEvents.FirstZombieKilledSquadId = SquadId;
    }
    const FZombieTypeDef* Def = Tables.FindZombieType(Z.TypeId);
    if (Def && Def->BountyMoney > 0) Resources.Money += Def->BountyMoney;
    Log(EEventType::ZombieKilled, "A " + Z.TypeId + " was neutralized.");
}

void FSimulation::DamageSquad(FSquad& Squad, float Amount, i32 ZombieSource) {
    (void)ZombieSource;  // reserved for per-zombie kill attribution
    if (Squad.bWiped) return;
    Squad.Hp -= Amount;
    if (Squad.Hp <= 0.f) {
        Squad.Hp = 0.f;
        Squad.bWiped = true;
        Morale -= BalanceF("MoraleTurnPenalty", 2) * 2.f;
        Log(EEventType::SquadWiped, Squad.Name + " was wiped out.");
    }
}

void FSimulation::VaccinateCitizen(i32 CitizenId) {
    if (CitizenId < 0 || CitizenId >= static_cast<i32>(CitizenList.size())) return;
    FCitizen& C = CitizenList[CitizenId];
    if (C.State == ECitizenState::Dead || C.State == ECitizenState::Turned ||
        C.State == ECitizenState::Evacuated)
        return;
    if (C.bVaccinated) return;
    C.bVaccinated = true;
    ++CitizensVaccinated;
    Log(EEventType::CitizenVaccinated, C.Name + " was vaccinated.");
}

} // namespace oc
