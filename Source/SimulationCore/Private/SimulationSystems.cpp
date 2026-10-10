// Outbreak Commander - Simulation Core
// Campaign, tutorial, seasons, achievements, versus extras, heatmap, roles,
// localization queries, pathfinding, storage, upgrades, quarantine zones.
#include "oc/Simulation.h"

#include <cmath>
#include <functional>
#include <queue>
#include <unordered_set>

namespace oc {

namespace {

const char* RoleId(ECoopRole R) {
    switch (R) {
    case ECoopRole::Engineer: return "engineer";
    case ECoopRole::Medic: return "medic";
    case ECoopRole::Logistics: return "logistics";
    case ECoopRole::Virus: return "virus";
    default: return "commander";
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Roles, language, difficulty
// ---------------------------------------------------------------------------

bool FSimulation::CanRoleIssue(ECoopRole Role, const std::string& Verb) const {
    const FCoopRoleDef* Def = Tables.FindCoopRole(RoleId(Role));
    if (!Def) return Role == ECoopRole::Commander;
    for (const auto& V : Def->AllowedVerbs)
        if (V == "*" || V == Verb) return true;
    return false;
}

void FSimulation::SetLanguage(const std::string& Lang) {
    Config.Language = Lang.empty() ? "en" : Lang;
}

void FSimulation::SetLocalRole(ECoopRole Role) { Config.LocalRole = Role; }

std::string FSimulation::Localize(const std::string& Key) const {
    return Tables.Localize(Key, Config.Language);
}

bool FSimulation::ApplyDifficulty(const std::string& DifficultyId) {
    const FDifficultyDef* D = Tables.FindDifficulty(DifficultyId);
    if (!D) return false;
    Config.DifficultyId = DifficultyId;
    // Difficulty multiplies the scenario multipliers (does not replace them).
    ScenarioZombieSpeed *= D->ZombieSpeedMultiplier;
    ScenarioInfection *= D->InfectionRateMultiplier;
    ScenarioScarcity *= D->ResourceScarcityMultiplier;
    Tables.BalanceValues["HordeBaseSize"] = static_cast<float>(D->HordeBaseSize);
    return true;
}

// ---------------------------------------------------------------------------
// Campaign / tutorial / objectives
// ---------------------------------------------------------------------------

bool FSimulation::StartCampaign(const std::string& CampaignId) {
    const FCampaignDef* C = Tables.FindCampaign(CampaignId);
    if (!C) return false;
    Config.CampaignId = CampaignId;
    Config.bTutorial = C->bTutorial;
    if (C->DemoMinutes > 0.f && Config.bDemoMode)
        Tables.BalanceValues["DemoMinutes"] = C->DemoMinutes;
    ActivateCampaignObjectives();
    PushHint("hint_camera");
    Log(EEventType::Milestone, "Campaign started: " + C->Name);
    LogCommand("StartCampaign", {CampaignId});
    return true;
}

void FSimulation::ActivateCampaignObjectives() {
    ObjectiveList.clear();
    if (Config.CampaignId.empty()) return;
    for (const FObjectiveDef& D : Tables.ObjectiveDefs) {
        if (D.CampaignId != Config.CampaignId) continue;
        FObjective O;
        O.Id = D.Id; O.Title = D.Title; O.Description = D.Description;
        O.Kind = D.Kind; O.Target = D.Target; O.HintKey = D.HintKey;
        O.TargetCount = D.TargetCount; O.UnlockDay = D.UnlockDay;
        O.bRequired = D.bRequired;
        O.State = D.UnlockDay <= 0.f ? EObjectiveState::Active : EObjectiveState::Hidden;
        if (O.State == EObjectiveState::Active && !O.HintKey.empty()) PushHint(O.HintKey);
        ObjectiveList.push_back(O);
    }
}

void FSimulation::EvaluateObjective(FObjective& O) {
    if (O.State != EObjectiveState::Active) return;
    i32 Progress = 0;
    if (O.Kind == "start") {
        Progress = 1;
    } else if (O.Kind == "build") {
        for (const FBuilding& B : BuildingList)
            if (!B.bDemolished && !B.bUnderConstruction && B.DefId == O.Target) ++Progress;
    } else if (O.Kind == "recruit") {
        for (const FSquad& S : SquadList)
            if (!S.bWiped && S.UnitTypeId == O.Target) ++Progress;
    } else if (O.Kind == "research") {
        Progress = IsTechResearched(O.Target) ? 1 : 0;
    } else if (O.Kind == "survive_days") {
        Progress = static_cast<i32>(Stats.Day);
    } else if (O.Kind == "kill") {
        Progress = TotalZombiesKilled;
    } else if (O.Kind == "vaccinate") {
        Progress = CitizensVaccinated;
    } else if (O.Kind == "evacuate") {
        Progress = Evacuated;
    } else if (O.Kind == "wall") {
        i32 Segs = 0;
        for (const FWall& W : WallList) if (!W.bUnderConstruction) ++Segs;
        Progress = Segs;
    } else if (O.Kind == "policy") {
        if (O.Target == "quarantine") Progress = HasPolicy(EPolicy::Quarantine) ? 1 : 0;
        else if (O.Target == "curfew") Progress = HasPolicy(EPolicy::Curfew) ? 1 : 0;
        else if (O.Target == "martiallaw") Progress = HasPolicy(EPolicy::MartialLaw) ? 1 : 0;
    } else if (O.Kind == "power") {
        Progress = 0;
        for (i32 D = 0; D < DistrictCount(); ++D) if (IsDistrictPowered(D)) ++Progress;
        if (Progress > 0) Progress = 1;
    } else if (O.Kind == "hold") {
        Progress = (Stats.InfectedFraction < 0.5f) ? static_cast<i32>(Stats.Day) : 0;
    }
    O.Progress = Progress;
    if (O.Progress >= O.TargetCount) {
        O.State = EObjectiveState::Complete;
        O.Progress = O.TargetCount;
        Log(EEventType::ObjectiveComplete, "Objective complete: " + O.Title);
        if (!O.HintKey.empty()) { /* already shown on activate */ }
    }
}

void FSimulation::TickCampaign(float Dt) {
    (void)Dt;
    if (ObjectiveList.empty()) return;
    const float Day = TimeSeconds / BalanceF("SecondsPerDay", 60.f);
    for (FObjective& O : ObjectiveList) {
        if (O.State == EObjectiveState::Hidden && Day >= O.UnlockDay) {
            O.State = EObjectiveState::Active;
            if (!O.HintKey.empty()) PushHint(O.HintKey);
        }
        EvaluateObjective(O);
    }
    if (Result != EGameResult::Ongoing) return;
    if (BalanceF("CampaignWinOnObjectives", 1) < 0.5f) return;
    bool bAnyRequired = false, bAllRequired = true;
    for (const FObjective& O : ObjectiveList) {
        if (!O.bRequired) continue;
        bAnyRequired = true;
        if (O.State != EObjectiveState::Complete) bAllRequired = false;
    }
    if (bAnyRequired && bAllRequired) {
        Result = EGameResult::Won;
        ResultReason = "Campaign objectives complete.";
        Log(EEventType::GameWon, ResultReason);
    }
}

i32 FSimulation::DebugAddObjective(const std::string& Id, const std::string& Kind,
                                   const std::string& Target, i32 Count) {
    FObjective O;
    O.Id = Id; O.Title = Id; O.Kind = Kind; O.Target = Target;
    O.TargetCount = Count; O.State = EObjectiveState::Active; O.bRequired = true;
    ObjectiveList.push_back(O);
    return static_cast<i32>(ObjectiveList.size()) - 1;
}

// ---------------------------------------------------------------------------
// Hints
// ---------------------------------------------------------------------------

void FSimulation::PushHint(const std::string& HintId) {
    if (HintId.empty()) return;
    for (const auto& Id : ShownHintIds) if (Id == HintId) return;
    const FHintDef* D = Tables.FindHint(HintId);
    if (!D) return;
    LastHintTrigger = HintId;
    ShownHintIds.push_back(HintId);
    FHint H;
    H.Id = D->Id; H.Title = D->Title; H.Body = D->Body;
    H.SecondsLeft = BalanceF("HintDisplaySeconds", 8);
    HintList.push_back(H);
    Log(EEventType::HintShown, H.Title + " — " + H.Body);
}

bool FSimulation::DismissHint() {
    if (HintList.empty()) return false;
    HintList.erase(HintList.begin());
    return true;
}

void FSimulation::TickHints(float Dt) {
    for (FHint& H : HintList) H.SecondsLeft -= Dt;
    for (i32 i = static_cast<i32>(HintList.size()) - 1; i >= 0; --i)
        if (HintList[i].SecondsLeft <= 0.f) HintList.erase(HintList.begin() + i);

    // Contextual triggers (each fires at most once via LastHintTrigger).
    if (Stats.ZombiesAlive > 0) PushHint("hint_military");
    if (Stats.CitizensInfected > 1) PushHint("hint_policy");
    if (Stats.bNight) PushHint("hint_survive");
    if (Resources.Ammo < BalanceF("LowAmmoThreshold", 50) && Stats.ActiveSquads > 0)
        PushHint("hint_ammo");
    if (bFoodShortageTick) PushHint("hint_food");
    if (Stats.bAnyPowerOutage) PushHint("hint_power");
}

// ---------------------------------------------------------------------------
// Achievements
// ---------------------------------------------------------------------------

void FSimulation::DebugUnlockAchievement(const std::string& Id) {
    for (FAchievementUnlock& A : AchievementList)
        if (A.Id == Id) { A.bUnlocked = true; A.UnlockDay = Stats.Day; return; }
    FAchievementUnlock A;
    A.Id = Id; A.bUnlocked = true; A.UnlockDay = Stats.Day;
    AchievementList.push_back(A);
}

void FSimulation::TickAchievements() {
    auto Unlocked = [&](const std::string& Id) {
        for (const auto& A : AchievementList) if (A.Id == Id && A.bUnlocked) return true;
        return false;
    };
    auto Unlock = [&](const FAchievementDef& D) {
        if (Unlocked(D.Id)) return;
        FAchievementUnlock A;
        A.Id = D.Id; A.bUnlocked = true; A.UnlockDay = Stats.Day;
        AchievementList.push_back(A);
        Log(EEventType::AchievementUnlocked, "Achievement: " + D.Name);
    };
    i32 BuildingsDone = 0;
    for (const FBuilding& B : BuildingList)
        if (!B.bDemolished && !B.bUnderConstruction) ++BuildingsDone;
    i32 WallsDone = static_cast<i32>(WallList.size());
    i32 SquadsRecruited = NextSquadId;

    for (const FAchievementDef& D : Tables.AchievementDefs) {
        if (Unlocked(D.Id)) continue;
        bool bHit = false;
        if (D.Kind == "kills") bHit = TotalZombiesKilled >= D.TargetCount;
        else if (D.Kind == "days") bHit = Stats.Day >= static_cast<float>(D.TargetCount);
        else if (D.Kind == "buildings") bHit = BuildingsDone >= D.TargetCount;
        else if (D.Kind == "walls") bHit = WallsDone >= D.TargetCount;
        else if (D.Kind == "squads") bHit = SquadsRecruited >= D.TargetCount;
        else if (D.Kind == "tech") bHit = IsTechResearched(D.Target);
        else if (D.Kind == "aid") bHit = AidPackages >= D.TargetCount;
        else if (D.Kind == "evacuate") bHit = Evacuated >= D.TargetCount;
        else if (D.Kind == "campaign") bHit = Config.CampaignId == D.Target && Result == EGameResult::Won;
        else if (D.Kind == "win") {
            if (Result == EGameResult::Won) {
                if (D.Target == "eradication")
                    bHit = ResultReason.find("eradicated") != std::string::npos;
                else if (D.Target == "containment")
                    bHit = ResultReason.find("contained") != std::string::npos;
                else bHit = true;
            }
        } else if (D.Kind == "win_no_nuke") {
            bHit = Result == EGameResult::Won && !bNuked;
        }
        if (bHit) Unlock(D);
    }
}

// ---------------------------------------------------------------------------
// Seasons
// ---------------------------------------------------------------------------

void FSimulation::DebugSetSeason(const std::string& SeasonId) {
    if (!Tables.FindSeason(SeasonId)) return;
    CurrentSeasonId = SeasonId;
    SeasonDaysLeft = BalanceF("SeasonLengthDays", 90);
}

void FSimulation::TickSeasons(float Dt) {
    if (VirusBoostTimer > 0.f) VirusBoostTimer = std::max(0.f, VirusBoostTimer - Dt);
    SeasonDaysLeft -= Dt / BalanceF("SecondsPerDay", 60.f);
    if (SeasonDaysLeft > 0.f || Tables.Seasons.empty()) return;
    // Cycle without extra RNG so short games (still in spring) are unchanged.
    i32 Idx = 0;
    for (i32 i = 0; i < static_cast<i32>(Tables.Seasons.size()); ++i)
        if (Tables.Seasons[i].Id == CurrentSeasonId) { Idx = i; break; }
    Idx = (Idx + 1) % static_cast<i32>(Tables.Seasons.size());
    CurrentSeasonId = Tables.Seasons[Idx].Id;
    SeasonDaysLeft = BalanceF("SeasonLengthDays", 90);
    const FSeasonDef* S = Tables.FindSeason(CurrentSeasonId);
    Log(EEventType::SeasonChanged, std::string("Season changed: ") + (S ? S->Name : CurrentSeasonId));
}

float FSimulation::SeasonFoodMult() const {
    const FSeasonDef* S = Tables.FindSeason(CurrentSeasonId);
    return S ? S->FoodYieldMultiplier : 1.f;
}
float FSimulation::SeasonZombieMult() const {
    const FSeasonDef* S = Tables.FindSeason(CurrentSeasonId);
    float M = S ? S->ZombieActivityMultiplier : 1.f;
    if (VirusBoostTimer > 0.f) M *= BalanceF("VirusBoostSpeedMultiplier", 1.5f);
    return M;
}
float FSimulation::SeasonSolarMult() const {
    const FSeasonDef* S = Tables.FindSeason(CurrentSeasonId);
    return S ? S->SolarPowerMultiplier : 1.f;
}
float FSimulation::SeasonMoralePerDay() const {
    const FSeasonDef* S = Tables.FindSeason(CurrentSeasonId);
    return S ? S->MoraleModifierPerDay : 0.f;
}

// ---------------------------------------------------------------------------
// Versus extras
// ---------------------------------------------------------------------------

bool FSimulation::VirusInfectTile(i32 X, i32 Y) {
    if (!Config.bVersusMode || !IsInside(X, Y)) return false;
    TileGrid[static_cast<size_t>(Y) * Width + X].Contamination =
        std::min(1.f, TileGrid[static_cast<size_t>(Y) * Width + X].Contamination +
                          BalanceF("VirusInfectTileAmount", 1.f));
    Log(EEventType::Milestone, "The virus saturated a tile.");
    LogCommand("VirusInfectTile", {std::to_string(X), std::to_string(Y)});
    return true;
}

bool FSimulation::VirusContaminate(i32 X, i32 Y) {
    if (!Config.bVersusMode) return false;
    const i32 R = static_cast<i32>(BalanceF("VirusContaminateRadius", 4));
    const float Amt = BalanceF("VirusContaminateAmount", 0.6f);
    for (i32 Dy = -R; Dy <= R; ++Dy)
        for (i32 Dx = -R; Dx <= R; ++Dx) {
            const i32 Tx = X + Dx, Ty = Y + Dy;
            if (!IsInside(Tx, Ty)) continue;
            if (Dx * Dx + Dy * Dy > R * R) continue;
            auto& T = TileGrid[static_cast<size_t>(Ty) * Width + Tx];
            T.Contamination = std::min(1.f, T.Contamination + Amt);
        }
    Log(EEventType::Milestone, "The virus contaminated an area.");
    LogCommand("VirusContaminate", {std::to_string(X), std::to_string(Y)});
    return true;
}

bool FSimulation::VirusBoostHorde() {
    if (!Config.bVersusMode) return false;
    VirusBoostTimer = BalanceF("VirusBoostDurationSeconds", 20);
    Log(EEventType::Milestone, "The horde surges.");
    LogCommand("VirusBoostHorde", {});
    return true;
}

i32 FSimulation::VirusMutate(const std::string& FromType, const std::string& ToType, i32 Count) {
    if (!Config.bVersusMode) return -1;
    if (!Tables.FindZombieType(ToType)) return -1;
    i32 Mutated = 0;
    for (FZombie& Z : ZombieList) {
        if (Mutated >= Count) break;
        if (Z.TypeId != FromType) continue;
        const FZombieTypeDef* Def = Tables.FindZombieType(ToType);
        Z.TypeId = ToType;
        Z.Hp = Def->Hp;
        ++Mutated;
    }
    Log(EEventType::Milestone, "The virus mutated " + std::to_string(Mutated) + " " + FromType + "s.");
    LogCommand("VirusMutate", {FromType, ToType, std::to_string(Count)});
    return Mutated;
}

// ---------------------------------------------------------------------------
// Buildings: upgrades, gates, corners, quarantine, storage
// ---------------------------------------------------------------------------

bool FSimulation::UpgradeBuilding(i32 BuildingId) {
    if (Result != EGameResult::Ongoing) return false;
    if (BuildingId < 0 || BuildingId >= static_cast<i32>(BuildingList.size())) return false;
    FBuilding& B = BuildingList[BuildingId];
    if (B.bDemolished || B.bUnderConstruction) return false;
    const i32 MaxLevel = static_cast<i32>(BalanceF("BuildingMaxLevel", 3));
    if (B.Level >= MaxLevel) return false;
    const FBuildingDef* Def = Tables.FindBuilding(B.DefId);
    if (!Def) return false;
    const float Mult = BalanceF("UpgradeCostMultiplier", 0.75f);
    const double Money = Def->CostMoney * Mult;
    const double Mats = Def->CostMaterials * Mult;
    if (Resources.Money < Money || Resources.Materials < Mats) return false;
    Resources.Money -= Money;
    Resources.Materials -= Mats;
    ++B.Level;
    B.Hp = static_cast<float>(Def->MaxHp) * (1.f + (B.Level - 1) * BalanceF("UpgradeProductionBonus", 0.25f));
    Log(EEventType::BuildingUpgraded, Def->Name + " upgraded to level " + std::to_string(B.Level) + ".");
    LogCommand("UpgradeBuilding", {std::to_string(BuildingId)});
    return true;
}

bool FSimulation::BuildWallGate(i32 WallId) {
    if (Result != EGameResult::Ongoing) return false;
    if (WallId < 0 || WallId >= static_cast<i32>(WallList.size())) return false;
    FWall& W = WallList[WallId];
    if (W.bGate) return false;
    const double Cost = BalanceF("GateCostMoney", 200);
    if (Resources.Money < Cost) return false;
    Resources.Money -= Cost;
    W.bGate = true;
    W.MaxHp *= BalanceF("GateHpFraction", 0.6f);
    W.Hp = std::min(W.Hp, W.MaxHp);
    Log(EEventType::GateBuilt, "A gate was cut into the wall.");
    LogCommand("BuildWallGate", {std::to_string(WallId)});
    return true;
}

void FSimulation::MarkWallCorners(size_t FromIndex) {
    if (WallList.size() < 2) return;
    const size_t Start = FromIndex > 0 ? FromIndex : 1;
    for (size_t i = Start; i < WallList.size(); ++i) {
        const FWall& A = WallList[i - 1];
        FWall& B = WallList[i];
        const float Adx = A.X2 - A.X1, Ady = A.Y2 - A.Y1;
        const float Bdx = B.X2 - B.X1, Bdy = B.Y2 - B.Y1;
        // Perpendicular consecutive segments (including the join with a previous drag) are corners.
        if (std::fabs(Adx * Bdx + Ady * Bdy) < 0.1f) B.bCorner = true;
    }
}

bool FSimulation::DesignateQuarantineZone(i32 District, bool bEnabled) {
    if (District < 0 || District >= DistrictCount()) return false;
    if (QuarantineFlags.size() != static_cast<size_t>(DistrictCount()))
        QuarantineFlags.assign(DistrictCount(), 0);
    QuarantineFlags[District] = bEnabled ? 1 : 0;
    UpdateStats();
    Log(EEventType::QuarantineDesignated,
        std::string(bEnabled ? "Quarantine zone designated: district " : "Quarantine zone lifted: district ") +
            std::to_string(District));
    LogCommand("DesignateQuarantineZone", {std::to_string(District), bEnabled ? "1" : "0"});
    return true;
}

bool FSimulation::IsDistrictQuarantined(i32 District) const {
    return District >= 0 && District < static_cast<i32>(QuarantineFlags.size()) &&
           QuarantineFlags[District] != 0;
}

double FSimulation::StorageCap() const {
    double Cap = BalanceF("StorageCapBase", 1000000);
    const double Per = BalanceF("StorageCapPerDepot", 5000);
    for (const FBuilding& B : BuildingList) {
        if (B.bDemolished || B.bUnderConstruction || !B.bPowered) continue;
        const FBuildingDef* Def = Tables.FindBuilding(B.DefId);
        if (Def && Def->Function == "storage") Cap += Per * B.Level;
    }
    return Cap;
}

void FSimulation::ClampResourcesToStorage() {
    const double Cap = StorageCap();
    auto Clamp = [Cap](double& V) { if (V > Cap) V = Cap; };
    Clamp(Resources.Food); Clamp(Resources.Water); Clamp(Resources.Fuel);
    Clamp(Resources.Materials); Clamp(Resources.Medicine); Clamp(Resources.Ammo);
}

// ---------------------------------------------------------------------------
// Heatmap, music, leaderboard, demo, adaptive difficulty
// ---------------------------------------------------------------------------

float FSimulation::InfectionHeatAt(i32 X, i32 Y) const {
    if (!IsInside(X, Y)) return 0.f;
    float H = TileAt(X, Y).Contamination * BalanceF("HeatmapContaminationWeight", 0.2f);
    const float Zw = BalanceF("HeatmapZombieWeight", 0.4f);
    const float Iw = BalanceF("HeatmapInfectedWeight", 0.4f);
    for (const FZombie& Z : ZombieList) {
        const float D = std::hypot(Z.X - X, Z.Y - Y);
        if (D < 8.f) H += Zw * (1.f - D / 8.f);
    }
    for (const FCitizen& C : CitizenList) {
        if (!C.bLatentInfection) continue;
        const float D = std::hypot(C.X - X, C.Y - Y);
        if (D < 8.f) H += Iw * (1.f - D / 8.f);
    }
    return std::min(1.f, H);
}

void FSimulation::FillHeatmap(std::vector<float>& Out, i32 Downsample) const {
    const i32 Step = std::max(1, Downsample);
    const i32 W = (Width + Step - 1) / Step;
    const i32 H = (Height + Step - 1) / Step;
    Out.assign(static_cast<size_t>(W * H), 0.f);
    for (i32 Y = 0; Y < Height; Y += Step)
        for (i32 X = 0; X < Width; X += Step)
            Out[static_cast<size_t>((Y / Step) * W + (X / Step))] = InfectionHeatAt(X, Y);
}

EMusicState FSimulation::MusicState() const {
    if (Stats.InfectedFraction >= BalanceF("MusicDireInfectedFraction", 0.4f) ||
        Stats.ZombiesAlive >= 40)
        return EMusicState::Dire;
    if (Stats.ZombiesAlive >= static_cast<i32>(BalanceF("MusicCombatZombieThreshold", 5)) ||
        (Stats.ActiveSquads > 0 && Stats.ZombiesAlive > 0))
        return EMusicState::Combat;
    return EMusicState::Calm;
}

i32 FSimulation::ComputeLeaderboardScore() const {
    i32 Score = Stats.CitizensAlive * static_cast<i32>(BalanceF("LeaderboardSurvivorPoints", 10));
    Score += TotalZombiesKilled * static_cast<i32>(BalanceF("LeaderboardKillPoints", 2));
    Score += static_cast<i32>(Stats.Day) * static_cast<i32>(BalanceF("LeaderboardDayPoints", 100));
    Score += Stats.TechsResearched * static_cast<i32>(BalanceF("LeaderboardTechPoints", 50));
    if (Result == EGameResult::Won) Score += 5000;
    if (bNuked) Score -= 1000;
    return std::max(0, Score);
}

void FSimulation::TickDemoMode(float Dt) {
    (void)Dt;
    if (!Config.bDemoMode || Result != EGameResult::Ongoing) return;
    const float LimitDays = BalanceF("DemoMinutes", 30) * 60.f / BalanceF("SecondsPerDay", 60.f);
    if (Stats.Day >= LimitDays) {
        Result = EGameResult::Won;
        ResultReason = "Demo complete.";
        Log(EEventType::DemoEnded, ResultReason);
    }
}

void FSimulation::TickAdaptiveDifficulty() {
    if (BalanceF("AdaptiveDifficultyEnabled", 0) < 0.5f) return;
    const float Step = BalanceF("AdaptiveDifficultyStep", 0.05f);
    if (Stats.InfectedFraction > 0.35f) DifficultyBias -= Step;
    else if (Stats.InfectedFraction < 0.1f && Stats.ActiveSquads > 2) DifficultyBias += Step;
    DifficultyBias = std::max(BalanceF("AdaptiveDifficultyMin", 0.5f),
                              std::min(BalanceF("AdaptiveDifficultyMax", 2.f), DifficultyBias));
}

// ---------------------------------------------------------------------------
// A* pathfinding (opt-in; default off so wall-slide tests stay stable)
// ---------------------------------------------------------------------------

void FSimulation::WearItem(const std::string& ItemId, float Amount) {
    if (ItemId.empty() || Amount <= 0.f) return;
    for (auto It = InventoryList.begin(); It != InventoryList.end(); ++It) {
        if (It->ItemId != ItemId) continue;
        const FItemDef* Def = Tables.FindItem(ItemId);
        const float MaxD = (Def && Def->Durability > 0) ? static_cast<float>(Def->Durability) : 100.f;
        It->Durability -= Amount / MaxD;
        if (It->Durability > 0.f) return;
        --It->Count;
        It->Durability = 1.f;
        if (It->Count <= 0) InventoryList.erase(It);
        return;
    }
}

bool FSimulation::FindPath(i32 X1, i32 Y1, i32 X2, i32 Y2, std::vector<std::pair<i32, i32>>& Out) const {
    Out.clear();
    if (!IsInside(X1, Y1) || !IsInside(X2, Y2)) return false;
    const i32 N = Width * Height;
    std::vector<i32> Came(N, -1);
    std::vector<i32> G(N, 1 << 30);
    auto Idx = [&](i32 X, i32 Y) { return Y * Width + X; };
    auto Passable = [&](i32 X, i32 Y) {
        if (!IsInside(X, Y)) return false;
        // Buildings block; finished (non-gate) walls block the tile they sit on.
        for (const FBuilding& B : BuildingList) {
            if (B.bDemolished || B.bUnderConstruction) continue;
            if (X >= B.X && X < B.X + B.W && Y >= B.Y && Y < B.Y + B.H) return false;
        }
        return true;
    };
    using Node = std::pair<i32, i32>;  // (f, idx)
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> Open;
    const i32 Start = Idx(X1, Y1), Goal = Idx(X2, Y2);
    G[Start] = 0;
    Open.push({std::abs(X2 - X1) + std::abs(Y2 - Y1), Start});
    const i32 Dx[4] = {1, -1, 0, 0};
    const i32 Dy[4] = {0, 0, 1, -1};
    i32 Guard = 0;
    while (!Open.empty() && Guard++ < N * 4) {
        const i32 Cur = Open.top().second; Open.pop();
        if (Cur == Goal) break;
        const i32 Cx = Cur % Width, Cy = Cur / Width;
        for (i32 k = 0; k < 4; ++k) {
            const i32 Nx = Cx + Dx[k], Ny = Cy + Dy[k];
            if (!Passable(Nx, Ny) && !(Nx == X2 && Ny == Y2)) continue;
            const i32 Ni = Idx(Nx, Ny);
            const i32 Tent = G[Cur] + 1;
            if (Tent < G[Ni]) {
                G[Ni] = Tent;
                Came[Ni] = Cur;
                Open.push({Tent + std::abs(X2 - Nx) + std::abs(Y2 - Ny), Ni});
            }
        }
    }
    if (Came[Goal] < 0 && Start != Goal) return false;
    std::vector<std::pair<i32, i32>> Rev;
    for (i32 C = Goal; C >= 0; C = Came[C]) {
        Rev.push_back({C % Width, C / Width});
        if (C == Start) break;
    }
    Out.assign(Rev.rbegin(), Rev.rend());
    return !Out.empty();
}

} // namespace oc
