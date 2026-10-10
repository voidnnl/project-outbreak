// Outbreak Commander - Simulation Core
// Save/load (versioned binary snapshots) and command-log replay.
// The deterministic sim makes replay trivial: same seed + same commands =
// same game. This is also the foundation for multiplayer sync and replays.
#include "oc/Simulation.h"

#include <cstring>
#include <fstream>

namespace oc {

namespace {

constexpr char SAVE_MAGIC[4] = {'O', 'C', 'S', 'V'};

struct FByteWriter {
    std::string Buf;
    void Bytes(const void* P, size_t N) { Buf.append(static_cast<const char*>(P), N); }
    void U8(std::uint8_t V) { Bytes(&V, 1); }
    void U64(u64 V) { Bytes(&V, 8); }
    void I32(i32 V) { Bytes(&V, 4); }
    void F32(float V) { Bytes(&V, 4); }
    void F64(double V) { Bytes(&V, 8); }
    void Bool(bool V) { U8(V ? 1 : 0); }
    void Str(const std::string& S) { U64(S.size()); Bytes(S.data(), S.size()); }
    template <typename T> void PodVec(const std::vector<T>& V) {
        U64(V.size());
        if (!V.empty()) Bytes(V.data(), V.size() * sizeof(T));
    }
    void BoolVec(const std::vector<bool>& V) {
        U64(V.size());
        for (bool B : V) U8(B ? 1 : 0);
    }
};

struct FByteReader {
    const char* P = nullptr;
    size_t Left = 0;
    bool bOk = true;
    void Bytes(void* Dst, size_t N) {
        if (!bOk || Left < N) { bOk = false; return; }
        std::memcpy(Dst, P, N);
        P += N;
        Left -= N;
    }
    std::uint8_t U8() { std::uint8_t V = 0; Bytes(&V, 1); return V; }
    u64 U64() { u64 V = 0; Bytes(&V, 8); return V; }
    i32 I32() { i32 V = 0; Bytes(&V, 4); return V; }
    float F32() { float V = 0; Bytes(&V, 4); return V; }
    double F64() { double V = 0; Bytes(&V, 8); return V; }
    bool Bool() { return U8() != 0; }
    std::string Str() {
        const u64 N = U64();
        std::string S;
        if (N > 0 && bOk) {
            if (N > Left) { bOk = false; return S; }
            S.resize(N);
            Bytes(&S[0], N);
        }
        return S;
    }
    template <typename T> void PodVec(std::vector<T>& V) {
        const u64 N = U64();
        V.resize(N);
        if (N > 0) Bytes(V.data(), N * sizeof(T));
    }
    void BoolVec(std::vector<bool>& V) {
        const u64 N = U64();
        V.assign(N, false);
        for (u64 i = 0; i < N; ++i) V[i] = U8() != 0;
    }
};

} // namespace

bool FSimulation::SaveToFile(const std::string& Path) const {
    FByteWriter W;
    W.Bytes(SAVE_MAGIC, 4);
    W.I32(static_cast<i32>(BalanceF("SaveVersion", 1)));

    // Config.
    W.U64(Config.Seed);
    W.I32(Config.MapWidth); W.I32(Config.MapHeight); W.I32(Config.InitialPopulation);
    W.Bool(Config.bStartWithPatientZero); W.Bool(Config.bVersusMode);
    W.Str(Config.ScenarioId); W.Str(Config.CityName);
    W.U64(Config.RuleOverrides.size());
    for (const auto& O : Config.RuleOverrides) { W.Str(O.first); W.F32(O.second); }

    W.U64(Rng.State);
    W.F32(TimeSeconds); W.F32(TickAccumulator); W.F32(Morale); W.F32(QuietDays);
    W.F64(TreatmentsAccumulator);
    W.Bool(bFoodShortageTick); W.Bool(bWaterShortageTick);
    W.F32(ScenarioZombieSpeed); W.F32(ScenarioInfection); W.F32(ScenarioScarcity);

    // Resources.
    W.F64(Resources.Money); W.F64(Resources.Food); W.F64(Resources.Water); W.F64(Resources.Fuel);
    W.F64(Resources.Materials); W.F64(Resources.Medicine); W.F64(Resources.Ammo); W.F64(Resources.Research);
    W.F64(Resources.PowerGeneration); W.F64(Resources.PowerConsumption);

    // Notable events + result.
    W.I32(NotableEvents.PatientZeroId); W.I32(NotableEvents.FirstTurnedCitizenId);
    W.I32(NotableEvents.FirstCuredCitizenId); W.I32(NotableEvents.FirstZombieKilledSquadId);
    W.I32(NotableEvents.TopSquadId);
    W.F32(NotableEvents.VaccineDeployedDay); W.F32(NotableEvents.NukeDay);
    W.I32(static_cast<i32>(Result)); W.Str(ResultReason);

    // Map.
    W.I32(Width); W.I32(Height); W.I32(DistrictCols); W.I32(DistrictRows);
    W.U64(TileGrid.size());
    for (const FTile& T : TileGrid) { W.F32(T.Contamination); W.Bool(T.bRoad); W.I32(T.District); }
    W.PodVec(TileIsFallout);
    W.BoolVec(DistrictPowered);
    W.BoolVec(DistrictWatered);
    W.PodVec(DistrictPowerGen); W.PodVec(DistrictPowerCons);
    W.PodVec(DistrictWaterProd); W.PodVec(DistrictWaterCons);

    // Buildings.
    W.U64(BuildingList.size());
    for (const FBuilding& B : BuildingList) {
        W.I32(B.Id); W.Str(B.DefId);
        W.I32(B.X); W.I32(B.Y); W.I32(B.W); W.I32(B.H);
        W.F32(B.Hp);
        W.Bool(B.bUnderConstruction); W.Bool(B.bPowered); W.Bool(B.bFuelStarved); W.Bool(B.bDemolished);
        W.I32(B.Level);
        W.F32(B.BuildProgress);  // construction progress (matters for buildings under construction)
    }
    // Walls (POD).
    W.PodVec(WallList);

    // Citizens.
    W.U64(CitizenList.size());
    for (const FCitizen& C : CitizenList) {
        W.I32(C.Id); W.I32(C.HomeId);
        W.F32(C.X); W.F32(C.Y);
        W.U8(static_cast<std::uint8_t>(C.State));
        W.Bool(C.bLatentInfection); W.Bool(C.bVaccinated); W.Bool(C.bFemale);
        W.Str(C.Name);
        W.F32(C.InfectionSecondsLeft); W.F32(C.Panic); W.F32(C.Hunger); W.F32(C.Thirst);
        W.F32(C.Health); W.F32(C.CorpseRiseSecondsLeft);
        W.F32(C.TargetX); W.F32(C.TargetY); W.Bool(C.bHasTarget);
    }

    // Zombies.
    W.U64(ZombieList.size());
    for (const FZombie& Z : ZombieList) {
        W.I32(Z.Id); W.Str(Z.TypeId);
        W.F32(Z.X); W.F32(Z.Y); W.F32(Z.Hp);
        W.U8(static_cast<std::uint8_t>(Z.State));
        W.I32(Z.TargetCitizen);
        W.F32(Z.AttackCooldown); W.F32(Z.WanderTimer); W.F32(Z.DirX); W.F32(Z.DirY);
        W.F32(Z.ChaseX); W.F32(Z.ChaseY); W.F32(Z.ChaseTimer);
        W.F32(Z.SlideTimer); W.F32(Z.SlideDirX); W.F32(Z.SlideDirY);
        W.F32(Z.RetreatTimer); W.F32(Z.AlertTimer);
    }

    // Squads.
    W.U64(SquadList.size());
    for (const FSquad& S : SquadList) {
        W.I32(S.Id); W.Str(S.UnitTypeId); W.Str(S.Name);
        W.I32(S.Size); W.I32(S.MaxSize);
        W.F32(S.X); W.F32(S.Y);
        W.U8(static_cast<std::uint8_t>(S.Order));
        W.U8(static_cast<std::uint8_t>(S.State));
        W.F32(S.TrainSecondsLeft); W.F32(S.TargetX); W.F32(S.TargetY);
        W.F32(S.Hp); W.F32(S.PerSoldierHp);
        W.I32(S.Kills); W.F32(S.NoiseTimer); W.Bool(S.bWiped);
    }

    // Vehicles.
    W.U64(VehicleList.size());
    for (const FVehicle& V : VehicleList) {
        W.I32(V.Id); W.Str(V.TypeId);
        W.F32(V.X); W.F32(V.Y); W.F32(V.Hp); W.F32(V.MaxHp);
        W.I32(V.SquadId);
        W.F32(V.TargetX); W.F32(V.TargetY);
        W.Bool(V.bHasTarget); W.Bool(V.bDestroyed);
    }

    // Crafting, networks, inventory.
    W.U64(CraftJobsList.size());
    for (const FCraftJob& J : CraftJobsList) {
        W.I32(J.Id); W.Str(J.RecipeId); W.I32(J.WorkshopId); W.F32(J.Progress); W.F32(J.Duration);
    }
    W.PodVec(PowerLineList);
    W.PodVec(WaterPipeList);
    W.U64(InventoryList.size());
    for (const FItemStack& I : InventoryList) {
        W.Str(I.ItemId); W.I32(I.Count); W.F32(I.Durability);
    }

    // Tech.
    W.U64(ResearchedTechs.size());
    for (const auto& T : ResearchedTechs) W.Str(T);
    W.Str(ActiveTechId); W.F32(ResearchProgress); W.Bool(bVaccineDeployed);

    // Policies.
    W.U64(Policies.size());
    for (const auto& P : Policies) { W.U8(static_cast<std::uint8_t>(P.first)); W.Bool(P.second); }

    // Environment.
    W.Str(CurrentWeatherId); W.F32(WeatherDaysLeft); W.F32(HordeTimerDays);

    // International.
    W.F32(AidCooldownDays); W.F32(TradeCooldownDays); W.F32(NewsTimerDays); W.F32(EvacCooldownDays);
    W.I32(AidPackages); W.I32(Evacuated); W.I32(CitizensVaccinated);
    W.Str(LastNewsCity);

    // Counters.
    W.I32(NextCitizenId); W.I32(NextZombieId); W.I32(NextBuildingId); W.I32(NextWallId);
    W.I32(NextSquadId); W.I32(NextVehicleId); W.I32(NextCraftJobId); W.I32(NextLineId);
    W.I32(SquadNameCounter); W.I32(TotalZombiesKilled);

    // Strikes.
    W.F32(AirstrikeCooldownDays);
    W.Bool(bAirstrikePending); W.F32(AirstrikePendingX); W.F32(AirstrikePendingY); W.F32(AirstrikePendingTimer);
    W.Bool(bNukePending); W.F32(NukePendingX); W.F32(NukePendingY); W.F32(NukePendingTimer);
    W.Bool(bNuked);
    W.F64(VaccinateAccumulator); W.F64(VaccineCureAccumulator);

    // Event log.
    W.U64(EventLog.size());
    for (const FSimEvent& E : EventLog) {
        W.U8(static_cast<std::uint8_t>(E.Type)); W.Str(E.Message); W.F32(E.TimeSeconds);
    }

    // Noise pings.
    W.U64(NoisePingList.size());
    for (const FNoisePing& P : NoisePingList) {
        W.F32(P.X); W.F32(P.Y); W.F32(P.Radius); W.F32(P.Timer); W.Bool(P.bAlert);
    }

    std::ofstream F(Path, std::ios::binary | std::ios::trunc);
    if (!F) return false;
    F.write(W.Buf.data(), static_cast<std::streamsize>(W.Buf.size()));
    return F.good();
}

bool FSimulation::LoadFromFile(const std::string& Path) {
    std::ifstream F(Path, std::ios::binary);
    if (!F) return false;
    std::string Data((std::istreambuf_iterator<char>(F)), std::istreambuf_iterator<char>());
    if (Data.size() < 8) return false;

    FByteReader R;
    R.P = Data.data();
    R.Left = Data.size();

    char Magic[4];
    R.Bytes(Magic, 4);
    if (std::memcmp(Magic, SAVE_MAGIC, 4) != 0) return false;
    const i32 Version = R.I32();
    if (Version != static_cast<i32>(BalanceF("SaveVersion", 1))) return false;
    if (!R.bOk) return false;

    // Config.
    Config.Seed = R.U64();
    Config.MapWidth = R.I32(); Config.MapHeight = R.I32(); Config.InitialPopulation = R.I32();
    Config.bStartWithPatientZero = R.Bool(); Config.bVersusMode = R.Bool();
    Config.ScenarioId = R.Str(); Config.CityName = R.Str();
    const u64 OverrideCount = R.U64();
    Config.RuleOverrides.clear();
    for (u64 i = 0; i < OverrideCount; ++i) {
        const std::string K = R.Str();
        const float V = R.F32();
        Config.RuleOverrides[K] = V;
        Tables.BalanceValues[K] = V;  // re-apply custom rules
    }

    Rng.State = R.U64();
    TimeSeconds = R.F32(); TickAccumulator = R.F32(); Morale = R.F32(); QuietDays = R.F32();
    TreatmentsAccumulator = R.F64();
    bFoodShortageTick = R.Bool(); bWaterShortageTick = R.Bool();
    ScenarioZombieSpeed = R.F32(); ScenarioInfection = R.F32(); ScenarioScarcity = R.F32();

    Resources.Money = R.F64(); Resources.Food = R.F64(); Resources.Water = R.F64(); Resources.Fuel = R.F64();
    Resources.Materials = R.F64(); Resources.Medicine = R.F64(); Resources.Ammo = R.F64();
    Resources.Research = R.F64();
    Resources.PowerGeneration = R.F64(); Resources.PowerConsumption = R.F64();

    NotableEvents.PatientZeroId = R.I32(); NotableEvents.FirstTurnedCitizenId = R.I32();
    NotableEvents.FirstCuredCitizenId = R.I32(); NotableEvents.FirstZombieKilledSquadId = R.I32();
    NotableEvents.TopSquadId = R.I32();
    NotableEvents.VaccineDeployedDay = R.F32(); NotableEvents.NukeDay = R.F32();
    Result = static_cast<EGameResult>(R.I32()); ResultReason = R.Str();

    Width = R.I32(); Height = R.I32(); DistrictCols = R.I32(); DistrictRows = R.I32();
    const u64 TileCount = R.U64();
    TileGrid.resize(TileCount);
    for (FTile& T : TileGrid) { T.Contamination = R.F32(); T.bRoad = R.Bool(); T.District = R.I32(); }
    R.PodVec(TileIsFallout);
    R.BoolVec(DistrictPowered);
    R.BoolVec(DistrictWatered);
    R.PodVec(DistrictPowerGen); R.PodVec(DistrictPowerCons);
    R.PodVec(DistrictWaterProd); R.PodVec(DistrictWaterCons);

    const u64 BuildingCount = R.U64();
    BuildingList.resize(BuildingCount);
    for (FBuilding& B : BuildingList) {
        B.Id = R.I32(); B.DefId = R.Str();
        B.X = R.I32(); B.Y = R.I32(); B.W = R.I32(); B.H = R.I32();
        B.Hp = R.F32();
        B.bUnderConstruction = R.Bool(); B.bPowered = R.Bool(); B.bFuelStarved = R.Bool();
        B.bDemolished = R.Bool();
        B.Level = R.I32();
        B.BuildProgress = R.F32();
    }
    R.PodVec(WallList);

    const u64 CitizenCount = R.U64();
    CitizenList.resize(CitizenCount);
    for (FCitizen& C : CitizenList) {
        C.Id = R.I32(); C.HomeId = R.I32();
        C.X = R.F32(); C.Y = R.F32();
        C.State = static_cast<ECitizenState>(R.U8());
        C.bLatentInfection = R.Bool(); C.bVaccinated = R.Bool(); C.bFemale = R.Bool();
        C.Name = R.Str();
        C.InfectionSecondsLeft = R.F32(); C.Panic = R.F32(); C.Hunger = R.F32(); C.Thirst = R.F32();
        C.Health = R.F32(); C.CorpseRiseSecondsLeft = R.F32();
        C.TargetX = R.F32(); C.TargetY = R.F32(); C.bHasTarget = R.Bool();
    }

    const u64 ZombieCount = R.U64();
    ZombieList.resize(ZombieCount);
    for (FZombie& Z : ZombieList) {
        Z.Id = R.I32(); Z.TypeId = R.Str();
        Z.X = R.F32(); Z.Y = R.F32(); Z.Hp = R.F32();
        Z.State = static_cast<EZombieState>(R.U8());
        Z.TargetCitizen = R.I32();
        Z.AttackCooldown = R.F32(); Z.WanderTimer = R.F32(); Z.DirX = R.F32(); Z.DirY = R.F32();
        Z.ChaseX = R.F32(); Z.ChaseY = R.F32(); Z.ChaseTimer = R.F32();
        Z.SlideTimer = R.F32(); Z.SlideDirX = R.F32(); Z.SlideDirY = R.F32();
        Z.RetreatTimer = R.F32(); Z.AlertTimer = R.F32();
    }

    const u64 SquadCount = R.U64();
    SquadList.resize(SquadCount);
    for (FSquad& S : SquadList) {
        S.Id = R.I32(); S.UnitTypeId = R.Str(); S.Name = R.Str();
        S.Size = R.I32(); S.MaxSize = R.I32();
        S.X = R.F32(); S.Y = R.F32();
        S.Order = static_cast<ESquadOrder>(R.U8());
        S.State = static_cast<ESquadState>(R.U8());
        S.TrainSecondsLeft = R.F32(); S.TargetX = R.F32(); S.TargetY = R.F32();
        S.Hp = R.F32(); S.PerSoldierHp = R.F32();
        S.Kills = R.I32(); S.NoiseTimer = R.F32(); S.bWiped = R.Bool();
    }

    const u64 VehicleCount = R.U64();
    VehicleList.resize(VehicleCount);
    for (FVehicle& V : VehicleList) {
        V.Id = R.I32(); V.TypeId = R.Str();
        V.X = R.F32(); V.Y = R.F32(); V.Hp = R.F32(); V.MaxHp = R.F32();
        V.SquadId = R.I32();
        V.TargetX = R.F32(); V.TargetY = R.F32();
        V.bHasTarget = R.Bool(); V.bDestroyed = R.Bool();
    }

    const u64 JobCount = R.U64();
    CraftJobsList.resize(JobCount);
    for (FCraftJob& J : CraftJobsList) {
        J.Id = R.I32(); J.RecipeId = R.Str(); J.WorkshopId = R.I32();
        J.Progress = R.F32(); J.Duration = R.F32();
    }
    R.PodVec(PowerLineList);
    R.PodVec(WaterPipeList);
    const u64 ItemCount = R.U64();
    InventoryList.resize(ItemCount);
    for (FItemStack& I : InventoryList) {
        I.ItemId = R.Str(); I.Count = R.I32(); I.Durability = R.F32();
    }

    const u64 TechCount = R.U64();
    ResearchedTechs.resize(TechCount);
    for (auto& T : ResearchedTechs) T = R.Str();
    ActiveTechId = R.Str(); ResearchProgress = R.F32(); bVaccineDeployed = R.Bool();
    RecomputeTechEffects();  // rebuild modifiers/unlocks from the researched set

    const u64 PolicyCount = R.U64();
    Policies.clear();
    for (u64 i = 0; i < PolicyCount; ++i)
        Policies[static_cast<EPolicy>(R.U8())] = R.Bool();

    CurrentWeatherId = R.Str(); WeatherDaysLeft = R.F32(); HordeTimerDays = R.F32();

    AidCooldownDays = R.F32(); TradeCooldownDays = R.F32(); NewsTimerDays = R.F32();
    EvacCooldownDays = R.F32();
    AidPackages = R.I32(); Evacuated = R.I32(); CitizensVaccinated = R.I32();
    LastNewsCity = R.Str();

    NextCitizenId = R.I32(); NextZombieId = R.I32(); NextBuildingId = R.I32(); NextWallId = R.I32();
    NextSquadId = R.I32(); NextVehicleId = R.I32(); NextCraftJobId = R.I32(); NextLineId = R.I32();
    SquadNameCounter = R.I32(); TotalZombiesKilled = R.I32();

    AirstrikeCooldownDays = R.F32();
    bAirstrikePending = R.Bool(); AirstrikePendingX = R.F32(); AirstrikePendingY = R.F32();
    AirstrikePendingTimer = R.F32();
    bNukePending = R.Bool(); NukePendingX = R.F32(); NukePendingY = R.F32(); NukePendingTimer = R.F32();
    bNuked = R.Bool();
    VaccinateAccumulator = R.F64(); VaccineCureAccumulator = R.F64();

    const u64 EventCount = R.U64();
    EventLog.clear();
    for (u64 i = 0; i < EventCount; ++i) {
        FSimEvent E;
        E.Type = static_cast<EEventType>(R.U8());
        E.Message = R.Str();
        E.TimeSeconds = R.F32();
        EventLog.push_back(E);
    }

    const u64 PingCount = R.U64();
    NoisePingList.clear();
    for (u64 i = 0; i < PingCount; ++i) {
        FNoisePing P;
        P.X = R.F32(); P.Y = R.F32(); P.Radius = R.F32(); P.Timer = R.F32(); P.bAlert = R.Bool();
        NoisePingList.push_back(P);
    }

    if (!R.bOk) return false;
    bCommandLogEnabled = false;  // never log state restored from disk
    UpdateStats();
    return true;
}

// ---------------------------------------------------------------------------
// Command replay (multiplayer foundation + deterministic replays)
// ---------------------------------------------------------------------------

bool FSimulation::ApplyCommand(const FCommand& Cmd) {
    const auto& A = Cmd.Args;
    auto I = [&A](size_t Idx, i32 Def = 0) {
        return Idx < A.size() ? std::stoi(A[Idx]) : Def;
    };
    if (Cmd.Verb == "PlaceBuilding" && A.size() >= 3) return PlaceBuilding(A[0], I(1), I(2)) >= 0;
    if (Cmd.Verb == "BuildWallLine" && A.size() >= 4) return BuildWallLine(I(0), I(1), I(2), I(3)) >= 0;
    if (Cmd.Verb == "DemolishBuilding" && A.size() >= 1) return DemolishBuilding(I(0));
    if (Cmd.Verb == "BuildPowerLine" && A.size() >= 2) return BuildPowerLine(I(0), I(1)) >= 0;
    if (Cmd.Verb == "BuildWaterPipe" && A.size() >= 2) return BuildWaterPipe(I(0), I(1)) >= 0;
    if (Cmd.Verb == "DemolishPowerLine" && A.size() >= 2) return DemolishPowerLine(I(0), I(1));
    if (Cmd.Verb == "DemolishWaterPipe" && A.size() >= 2) return DemolishWaterPipe(I(0), I(1));
    if (Cmd.Verb == "RecruitSquad" && A.size() >= 3) return RecruitSquad(A[0], I(1), I(2)) >= 0;
    if (Cmd.Verb == "IssueOrder" && A.size() >= 2)
        return IssueOrder(I(0), static_cast<ESquadOrder>(I(1)), I(2, -1), I(3, -1));
    if (Cmd.Verb == "DisbandSquad" && A.size() >= 1) return DisbandSquad(I(0));
    if (Cmd.Verb == "DeployVehicle" && A.size() >= 3) return DeployVehicle(A[0], I(1), I(2)) >= 0;
    if (Cmd.Verb == "OrderVehicleMove" && A.size() >= 3) return OrderVehicleMove(I(0), I(1), I(2));
    if (Cmd.Verb == "EmbarkSquad" && A.size() >= 2) return EmbarkSquad(I(0), I(1));
    if (Cmd.Verb == "DisembarkSquad" && A.size() >= 1) return DisembarkSquad(I(0));
    if (Cmd.Verb == "CallAirstrike" && A.size() >= 2) return CallAirstrike(I(0), I(1));
    if (Cmd.Verb == "CallNuke" && A.size() >= 2) return CallNuke(I(0), I(1));
    if (Cmd.Verb == "StartResearch" && A.size() >= 1) return StartResearch(A[0]);
    if (Cmd.Verb == "QueueRecipe" && A.size() >= 1) return QueueRecipe(A[0]);
    if (Cmd.Verb == "SetPolicy" && A.size() >= 2) { SetPolicy(static_cast<EPolicy>(I(0)), I(1) != 0); return true; }
    if (Cmd.Verb == "RequestAid") return RequestAid();
    if (Cmd.Verb == "SendTradeCaravan") return SendTradeCaravan();
    if (Cmd.Verb == "CallEvacuationFlight") return CallEvacuationFlight();
    if (Cmd.Verb == "VirusSpawnHorde" && A.size() >= 4)
        return VirusSpawnHorde(I(0), I(1), A[2], I(3)) >= 0;
    return false;
}

} // namespace oc
