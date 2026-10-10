#include "OCSimulationBridge.h"

DEFINE_LOG_CATEGORY_STATIC(LogOCSim, Log, All);

bool UOCSimulationBridge::Initialize(const FString& DataDir) {
    const std::string Dir(TCHAR_TO_UTF8(*DataDir));
    if (!Sim.LoadData(Dir)) {
        for (const auto& Err : Sim.Data().GetErrors())
            UE_LOG(LogOCSim, Error, TEXT("Data error: %s"), UTF8_TO_TCHAR(Err.c_str()));
        return false;
    }
    bInitialized = true;
    return true;
}

void UOCSimulationBridge::NewGame(int32 Seed, int32 MapWidth, int32 MapHeight, int32 Population,
                                  const FString& ScenarioId) {
    if (!bInitialized) return;
    oc::FSimConfig Cfg;
    Cfg.Seed = static_cast<oc::u64>(Seed);
    Cfg.MapWidth = MapWidth;
    Cfg.MapHeight = MapHeight;
    Cfg.InitialPopulation = Population;
    Cfg.ScenarioId = TCHAR_TO_UTF8(*ScenarioId);
    Sim.NewGame(Cfg);
    LastEventCount = 0;
    LastResult = EOCGameResult::Ongoing;
    BroadcastNewEvents();
    OnStatsUpdated.Broadcast(GetStatsSnapshot());
    OnResourcesUpdated.Broadcast(GetResourcesSnapshot());
}

void UOCSimulationBridge::Tick(float DeltaSeconds) {
    if (!bInitialized) return;
    Sim.Step(DeltaSeconds);
    BroadcastNewEvents();
    OnStatsUpdated.Broadcast(GetStatsSnapshot());
    OnResourcesUpdated.Broadcast(GetResourcesSnapshot());
    BroadcastResultIfChanged();
}

// --- construction ---

int32 UOCSimulationBridge::PlaceBuilding(const FString& DefId, int32 X, int32 Y) {
    return Sim.PlaceBuilding(TCHAR_TO_UTF8(*DefId), X, Y);
}

int32 UOCSimulationBridge::BuildWallLine(int32 X1, int32 Y1, int32 X2, int32 Y2) {
    return Sim.BuildWallLine(X1, Y1, X2, Y2);
}

bool UOCSimulationBridge::DemolishBuilding(int32 BuildingId) {
    return Sim.DemolishBuilding(BuildingId);
}

int32 UOCSimulationBridge::BuildPowerLine(int32 DistrictA, int32 DistrictB) {
    return Sim.BuildPowerLine(DistrictA, DistrictB);
}

int32 UOCSimulationBridge::BuildWaterPipe(int32 DistrictA, int32 DistrictB) {
    return Sim.BuildWaterPipe(DistrictA, DistrictB);
}

bool UOCSimulationBridge::DemolishPowerLine(int32 DistrictA, int32 DistrictB) {
    return Sim.DemolishPowerLine(DistrictA, DistrictB);
}

bool UOCSimulationBridge::DemolishWaterPipe(int32 DistrictA, int32 DistrictB) {
    return Sim.DemolishWaterPipe(DistrictA, DistrictB);
}

// --- military ---

int32 UOCSimulationBridge::RecruitSquad(const FString& UnitTypeId, int32 X, int32 Y) {
    return Sim.RecruitSquad(TCHAR_TO_UTF8(*UnitTypeId), X, Y);
}

bool UOCSimulationBridge::IssueOrder(int32 SquadId, EOCSquadOrder Order, int32 TargetX, int32 TargetY) {
    return Sim.IssueOrder(SquadId, static_cast<oc::ESquadOrder>(Order), TargetX, TargetY);
}

bool UOCSimulationBridge::DisbandSquad(int32 SquadId) { return Sim.DisbandSquad(SquadId); }

int32 UOCSimulationBridge::DeployVehicle(const FString& TypeId, int32 X, int32 Y) {
    return Sim.DeployVehicle(TCHAR_TO_UTF8(*TypeId), X, Y);
}

bool UOCSimulationBridge::OrderVehicleMove(int32 VehicleId, int32 X, int32 Y) {
    return Sim.OrderVehicleMove(VehicleId, X, Y);
}

bool UOCSimulationBridge::EmbarkSquad(int32 SquadId, int32 VehicleId) {
    return Sim.EmbarkSquad(SquadId, VehicleId);
}

bool UOCSimulationBridge::DisembarkSquad(int32 SquadId) { return Sim.DisembarkSquad(SquadId); }

bool UOCSimulationBridge::CallAirstrike(int32 X, int32 Y) { return Sim.CallAirstrike(X, Y); }

bool UOCSimulationBridge::CallNuke(int32 X, int32 Y) { return Sim.CallNuke(X, Y); }

// --- research, crafting, policies, international ---

bool UOCSimulationBridge::StartResearch(const FString& TechId) {
    return Sim.StartResearch(TCHAR_TO_UTF8(*TechId));
}

bool UOCSimulationBridge::QueueRecipe(const FString& RecipeId) {
    return Sim.QueueRecipe(TCHAR_TO_UTF8(*RecipeId));
}

void UOCSimulationBridge::SetPolicy(EOCPolicy Policy, bool bEnabled) {
    Sim.SetPolicy(static_cast<oc::EPolicy>(Policy), bEnabled);
}

bool UOCSimulationBridge::RequestAid() { return Sim.RequestAid(); }

bool UOCSimulationBridge::SendTradeCaravan() { return Sim.SendTradeCaravan(); }

bool UOCSimulationBridge::CallEvacuationFlight() { return Sim.CallEvacuationFlight(); }

bool UOCSimulationBridge::StartCampaign(const FString& CampaignId) {
    return Sim.StartCampaign(TCHAR_TO_UTF8(*CampaignId));
}
bool UOCSimulationBridge::UpgradeBuilding(int32 BuildingId) { return Sim.UpgradeBuilding(BuildingId); }
bool UOCSimulationBridge::BuildWallGate(int32 WallId) { return Sim.BuildWallGate(WallId); }
bool UOCSimulationBridge::DesignateQuarantineZone(int32 District, bool bEnabled) {
    return Sim.DesignateQuarantineZone(District, bEnabled);
}
bool UOCSimulationBridge::ApplyDifficulty(const FString& DifficultyId) {
    return Sim.ApplyDifficulty(TCHAR_TO_UTF8(*DifficultyId));
}
void UOCSimulationBridge::SetLanguage(const FString& Lang) { Sim.SetLanguage(TCHAR_TO_UTF8(*Lang)); }
bool UOCSimulationBridge::DismissHint() { return Sim.DismissHint(); }
bool UOCSimulationBridge::VirusInfectTile(int32 X, int32 Y) { return Sim.VirusInfectTile(X, Y); }
bool UOCSimulationBridge::VirusContaminate(int32 X, int32 Y) { return Sim.VirusContaminate(X, Y); }
bool UOCSimulationBridge::VirusBoostHorde() { return Sim.VirusBoostHorde(); }
int32 UOCSimulationBridge::VirusMutate(const FString& FromType, const FString& ToType, int32 Count) {
    return Sim.VirusMutate(TCHAR_TO_UTF8(*FromType), TCHAR_TO_UTF8(*ToType), Count);
}
int32 UOCSimulationBridge::GetLeaderboardScore() const { return Sim.ComputeLeaderboardScore(); }
FString UOCSimulationBridge::Localize(const FString& Key) const {
    return UTF8_TO_TCHAR(Sim.Localize(TCHAR_TO_UTF8(*Key)).c_str());
}
FString UOCSimulationBridge::GetSeason() const { return UTF8_TO_TCHAR(Sim.CurrentSeason().c_str()); }
FString UOCSimulationBridge::GetMusicState() const {
    switch (Sim.MusicState()) {
    case oc::EMusicState::Combat: return TEXT("combat");
    case oc::EMusicState::Dire: return TEXT("dire");
    default: return TEXT("calm");
    }
}
void UOCSimulationBridge::NewGameEx(const oc::FSimConfig& Cfg) {
    if (!bInitialized) return;
    Sim.NewGame(Cfg);
    LastEventCount = 0;
    LastResult = EOCGameResult::Ongoing;
    BroadcastNewEvents();
    OnStatsUpdated.Broadcast(GetStatsSnapshot());
    OnResourcesUpdated.Broadcast(GetResourcesSnapshot());
}

// --- save / load / report ---

bool UOCSimulationBridge::SaveGame(const FString& Path) {
    return Sim.SaveToFile(TCHAR_TO_UTF8(*Path));
}

bool UOCSimulationBridge::LoadGame(const FString& Path) {
    const bool bOk = Sim.LoadFromFile(TCHAR_TO_UTF8(*Path));
    if (bOk) {
        LastEventCount = 0;
        LastResult = EOCGameResult::Ongoing;
        BroadcastNewEvents();
        OnStatsUpdated.Broadcast(GetStatsSnapshot());
        OnResourcesUpdated.Broadcast(GetResourcesSnapshot());
    }
    return bOk;
}

FString UOCSimulationBridge::GenerateAfterActionReport() {
    return UTF8_TO_TCHAR(Sim.GenerateAfterActionReport().c_str());
}

// --- queries ---

FOCStatsSnapshot UOCSimulationBridge::GetStatsSnapshot() const {
    const oc::FStats& S = Sim.GetStats();
    FOCStatsSnapshot Out;
    Out.CitizensAlive = S.CitizensAlive;
    Out.CitizensHealthy = S.CitizensHealthy;
    Out.CitizensInfected = S.CitizensInfected;
    Out.CitizensPanicked = S.CitizensPanicked;
    Out.CitizensDead = S.CitizensDead;
    Out.CitizensTurned = S.CitizensTurned;
    Out.ZombiesAlive = S.ZombiesAlive;
    Out.ZombiesKilled = S.ZombiesKilled;
    Out.InitialPopulation = S.InitialPopulation;
    Out.InfectedFraction = S.InfectedFraction;
    Out.Morale = S.Morale;
    Out.Day = S.Day;
    Out.HomelessCitizens = S.HomelessCitizens;
    Out.bAnyPowerOutage = S.bAnyPowerOutage;
    Out.ActiveSquads = S.ActiveSquads;
    Out.Vehicles = S.Vehicles;
    Out.TechsResearched = S.TechsResearched;
    Out.Evacuated = S.Evacuated;
    Out.bVaccineDeployed = S.bVaccineDeployed;
    Out.bNuked = S.bNuked;
    Out.Weather = UTF8_TO_TCHAR(S.Weather.c_str());
    Out.bNight = S.bNight;
    return Out;
}

FOCResourcesSnapshot UOCSimulationBridge::GetResourcesSnapshot() const {
    const oc::FResources& R = Sim.GetResources();
    FOCResourcesSnapshot Out;
    Out.Money = R.Money;
    Out.Food = R.Food;
    Out.Water = R.Water;
    Out.Fuel = R.Fuel;
    Out.Materials = R.Materials;
    Out.Medicine = R.Medicine;
    Out.Ammo = R.Ammo;
    Out.Research = R.Research;
    Out.PowerGeneration = R.PowerGeneration;
    Out.PowerConsumption = R.PowerConsumption;
    return Out;
}

FOCEnvironmentInfo UOCSimulationBridge::GetEnvironmentInfo() const {
    const oc::FStats& S = Sim.GetStats();
    FOCEnvironmentInfo Out;
    Out.Weather = UTF8_TO_TCHAR(S.Weather.c_str());
    Out.bNight = S.bNight;
    Out.Day = S.Day;
    Out.bVaccineDeployed = S.bVaccineDeployed;
    Out.bNuked = S.bNuked;
    Out.Evacuated = S.Evacuated;
    Out.TechsResearched = S.TechsResearched;
    Out.CityName = UTF8_TO_TCHAR(Sim.GetConfig().CityName.c_str());
    return Out;
}

TArray<FOCSquadInfo> UOCSimulationBridge::GetSquads() const {
    TArray<FOCSquadInfo> Out;
    for (const oc::FSquad& S : Sim.Squads()) {
        if (S.bWiped) continue;
        FOCSquadInfo Info;
        Info.Id = S.Id;
        Info.UnitTypeId = UTF8_TO_TCHAR(S.UnitTypeId.c_str());
        Info.Name = UTF8_TO_TCHAR(S.Name.c_str());
        Info.Size = S.Size;
        Info.X = S.X;
        Info.Y = S.Y;
        Info.Order = static_cast<EOCSquadOrder>(S.Order);
        Info.bActive = S.State == oc::ESquadState::Active;
        Info.Hp = S.Hp;
        Info.Kills = S.Kills;
        Out.Add(Info);
    }
    return Out;
}

TArray<FOCVehicleInfo> UOCSimulationBridge::GetVehicles() const {
    TArray<FOCVehicleInfo> Out;
    for (const oc::FVehicle& V : Sim.Vehicles()) {
        if (V.bDestroyed) continue;
        FOCVehicleInfo Info;
        Info.Id = V.Id;
        Info.TypeId = UTF8_TO_TCHAR(V.TypeId.c_str());
        Info.X = V.X;
        Info.Y = V.Y;
        Info.Hp = V.Hp;
        Info.bDestroyed = V.bDestroyed;
        Out.Add(Info);
    }
    return Out;
}

EOCGameResult UOCSimulationBridge::GetGameResult() const {
    switch (Sim.GetResult()) {
    case oc::EGameResult::Won: return EOCGameResult::Won;
    case oc::EGameResult::Lost: return EOCGameResult::Lost;
    default: return EOCGameResult::Ongoing;
    }
}

bool UOCSimulationBridge::IsTechResearched(const FString& TechId) const {
    return Sim.IsTechResearched(TCHAR_TO_UTF8(*TechId));
}

void UOCSimulationBridge::BroadcastNewEvents() {
    const auto& Events = Sim.Events();
    if (Events.size() <= LastEventCount) return;
    for (size_t i = LastEventCount; i < Events.size(); ++i)
        OnSimEvent.Broadcast(static_cast<int32>(Events[i].Type), UTF8_TO_TCHAR(Events[i].Message.c_str()));
    LastEventCount = Events.size();
}

void UOCSimulationBridge::BroadcastResultIfChanged() {
    const EOCGameResult R = GetGameResult();
    if (R != LastResult) {
        LastResult = R;
        OnGameResult.Broadcast(R);
    }
}
