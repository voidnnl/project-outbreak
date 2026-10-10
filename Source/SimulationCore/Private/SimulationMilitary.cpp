// Outbreak Commander - Simulation Core
// Military player commands: squads, vehicles, airstrikes, and the nuclear
// option. Per-tick combat logic lives in SimulationStep.cpp (TickMilitary).
#include "oc/Simulation.h"

#include <cmath>

namespace oc {

i32 FSimulation::RecruitSquad(const std::string& UnitTypeId, i32 X, i32 Y) {
    if (Result != EGameResult::Ongoing) return -1;
    const FSoldierDef* Def = Tables.FindSoldier(UnitTypeId);
    if (!Def) return -1;
    if (Def->Unlock != "none" && !IsUnlocked(Def->Unlock)) {
        Log(EEventType::Milestone, "Recruitment blocked: " + Def->Name + " requires '" + Def->Unlock + "' tech.");
        return -1;
    }
    if (!IsInside(X, Y)) return -1;
    if (Resources.Money < Def->CostMoney || Resources.Ammo < Def->CostAmmo) return -1;
    Resources.Money -= Def->CostMoney;
    Resources.Ammo -= Def->CostAmmo;

    FSquad S;
    S.Id = NextSquadId++;
    S.UnitTypeId = UnitTypeId;
    S.Name = SquadName();
    S.Size = Def->SquadSize;
    S.MaxSize = Def->SquadSize;
    S.X = static_cast<float>(X);
    S.Y = static_cast<float>(Y);
    S.Order = ESquadOrder::Guard;
    S.TargetX = S.X;
    S.TargetY = S.Y;
    S.State = ESquadState::Training;
    S.TrainSecondsLeft = Def->TrainingSeconds;
    S.Hp = Def->Hp * Def->SquadSize;
    S.PerSoldierHp = Def->Hp;
    SquadList.push_back(S);

    Log(EEventType::SquadRecruited, S.Name + " (" + Def->Name + ") recruited. Training for " +
        std::to_string(static_cast<i32>(Def->TrainingSeconds)) + "s.");
    LogCommand("RecruitSquad", {UnitTypeId, std::to_string(X), std::to_string(Y)});
    return S.Id;
}

bool FSimulation::IssueOrder(i32 SquadId, ESquadOrder Order, i32 TargetX, i32 TargetY) {
    if (SquadId < 0 || SquadId >= static_cast<i32>(SquadList.size())) return false;
    FSquad& S = SquadList[SquadId];
    if (S.bWiped) return false;
    S.Order = Order;
    if (TargetX >= 0 && TargetY >= 0) {
        S.TargetX = static_cast<float>(TargetX);
        S.TargetY = static_cast<float>(TargetY);
    }
    LogCommand("IssueOrder", {std::to_string(SquadId), std::to_string(static_cast<int>(Order)),
                              std::to_string(TargetX), std::to_string(TargetY)});
    return true;
}

bool FSimulation::DisbandSquad(i32 SquadId) {
    if (SquadId < 0 || SquadId >= static_cast<i32>(SquadList.size())) return false;
    FSquad& S = SquadList[SquadId];
    if (S.bWiped) return false;
    // Disembark from any vehicle first.
    for (FVehicle& V : VehicleList)
        if (V.SquadId == SquadId) V.SquadId = -1;
    S.bWiped = true;
    Log(EEventType::Milestone, S.Name + " was disbanded.");
    LogCommand("DisbandSquad", {std::to_string(SquadId)});
    return true;
}

i32 FSimulation::DeployVehicle(const std::string& TypeId, i32 X, i32 Y) {
    if (Result != EGameResult::Ongoing) return -1;
    const FVehicleDef* Def = Tables.FindVehicle(TypeId);
    if (!Def) return -1;
    if (Def->Unlock != "none" && !IsUnlocked(Def->Unlock)) {
        Log(EEventType::Milestone, "Deployment blocked: " + Def->Name + " requires '" + Def->Unlock + "' tech.");
        return -1;
    }
    if (!IsInside(X, Y)) return -1;
    if (Resources.Money < Def->CostMoney || Resources.Materials < Def->CostMaterials ||
        Resources.Fuel < Def->CostFuel)
        return -1;
    Resources.Money -= Def->CostMoney;
    Resources.Materials -= Def->CostMaterials;
    Resources.Fuel -= Def->CostFuel;

    FVehicle V;
    V.Id = NextVehicleId++;
    V.TypeId = TypeId;
    V.X = static_cast<float>(X);
    V.Y = static_cast<float>(Y);
    V.Hp = Def->Hp;
    V.MaxHp = Def->Hp;
    VehicleList.push_back(V);
    Log(EEventType::Milestone, Def->Name + " deployed.");
    LogCommand("DeployVehicle", {TypeId, std::to_string(X), std::to_string(Y)});
    return V.Id;
}

bool FSimulation::OrderVehicleMove(i32 VehicleId, i32 X, i32 Y) {
    if (VehicleId < 0 || VehicleId >= static_cast<i32>(VehicleList.size())) return false;
    FVehicle& V = VehicleList[VehicleId];
    if (V.bDestroyed) return false;
    V.TargetX = static_cast<float>(X);
    V.TargetY = static_cast<float>(Y);
    V.bHasTarget = true;
    LogCommand("OrderVehicleMove", {std::to_string(VehicleId), std::to_string(X), std::to_string(Y)});
    return true;
}

bool FSimulation::EmbarkSquad(i32 SquadId, i32 VehicleId) {
    if (SquadId < 0 || SquadId >= static_cast<i32>(SquadList.size())) return false;
    if (VehicleId < 0 || VehicleId >= static_cast<i32>(VehicleList.size())) return false;
    FSquad& S = SquadList[SquadId];
    FVehicle& V = VehicleList[VehicleId];
    if (S.bWiped || V.bDestroyed) return false;
    if (V.SquadId != -1) return false;  // one squad per vehicle (prototype simplification)
    V.SquadId = SquadId;
    S.X = V.X;
    S.Y = V.Y;
    Log(EEventType::Milestone, S.Name + " embarked " + V.TypeId + ".");
    LogCommand("EmbarkSquad", {std::to_string(SquadId), std::to_string(VehicleId)});
    return true;
}

bool FSimulation::DisembarkSquad(i32 SquadId) {
    for (FVehicle& V : VehicleList) {
        if (V.SquadId == SquadId) {
            V.SquadId = -1;
            LogCommand("DisembarkSquad", {std::to_string(SquadId)});
            return true;
        }
    }
    return false;
}

bool FSimulation::CallAirstrike(i32 X, i32 Y) {
    if (Result != EGameResult::Ongoing) return false;
    if (AirstrikeCooldownDays > 0.f || bAirstrikePending) return false;
    const double Cost = BalanceF("AirstrikeCostMoney", 2000);
    if (Resources.Money < Cost) return false;
    Resources.Money -= Cost;
    bAirstrikePending = true;
    AirstrikePendingX = static_cast<float>(X);
    AirstrikePendingY = static_cast<float>(Y);
    AirstrikePendingTimer = BalanceF("AirstrikeDelaySeconds", 3);
    AirstrikeCooldownDays = BalanceF("AirstrikeCooldownDays", 1);
    Log(EEventType::AirstrikeCalled, "Airstrike authorized on (" + std::to_string(X) + ", " +
        std::to_string(Y) + "). Impact imminent.");
    LogCommand("CallAirstrike", {std::to_string(X), std::to_string(Y)});
    return true;
}

bool FSimulation::CallNuke(i32 X, i32 Y) {
    if (Result != EGameResult::Ongoing) return false;
    if (bNukePending || bNuked) return false;  // one nuclear option, ever
    const double Cost = BalanceF("NukeCostMoney", 50000);
    if (Resources.Money < Cost) return false;
    Resources.Money -= Cost;
    bNukePending = true;
    NukePendingX = static_cast<float>(X);
    NukePendingY = static_cast<float>(Y);
    NukePendingTimer = BalanceF("NukeDelaySeconds", 10);
    Log(EEventType::NukeCalled, "NUCLEAR OPTION AUTHORIZED on (" + std::to_string(X) + ", " +
        std::to_string(Y) + "). Detonation in " +
        std::to_string(static_cast<i32>(BalanceF("NukeDelaySeconds", 10))) + " seconds.");
    LogCommand("CallNuke", {std::to_string(X), std::to_string(Y)});
    return true;
}

// ---------------------------------------------------------------------------
// Pending strike resolution (airstrike / nuke delays)
// ---------------------------------------------------------------------------

void FSimulation::PendingStrikeTick(float Dt) {
    if (bAirstrikePending) {
        AirstrikePendingTimer -= Dt;
        if (AirstrikePendingTimer <= 0.f) {
            bAirstrikePending = false;
            const float Radius = BalanceF("AirstrikeRadiusTiles", 6);
            const float Damage = BalanceF("AirstrikeDamage", 150);
            for (i32 i = static_cast<i32>(ZombieList.size()) - 1; i >= 0; --i) {
                const FZombie& Z = ZombieList[i];
                if (std::hypot(Z.X - AirstrikePendingX, Z.Y - AirstrikePendingY) <= Radius) {
                    if (Z.Hp <= Damage) KillZombie(i, -1);
                    else ZombieList[i].Hp -= Damage;
                }
            }
            for (i32 i = static_cast<i32>(WallList.size()) - 1; i >= 0; --i) {
                const FWall& W = WallList[i];
                if (std::hypot((W.X1 + W.X2) * 0.5f - AirstrikePendingX,
                               (W.Y1 + W.Y2) * 0.5f - AirstrikePendingY) <= Radius)
                    WallList.erase(WallList.begin() + i);
            }
            for (FBuilding& B : BuildingList) {
                if (B.bDemolished) continue;
                if (std::hypot(B.X + B.W * 0.5f - AirstrikePendingX,
                               B.Y + B.H * 0.5f - AirstrikePendingY) <= Radius) {
                    B.Hp -= Damage * 2.f;
                    if (B.Hp <= 0.f) {
                        B.bDemolished = true;
                        Log(EEventType::BuildingDestroyed, "A building was destroyed by an airstrike.");
                        AssignHomes();
                    }
                }
            }
            Log(EEventType::Milestone, "Airstrike impact at (" +
                std::to_string(static_cast<i32>(AirstrikePendingX)) + ", " +
                std::to_string(static_cast<i32>(AirstrikePendingY)) + ").");
        }
    }

    if (bNukePending) {
        NukePendingTimer -= Dt;
        if (NukePendingTimer <= 0.f) {
            bNukePending = false;
            bNuked = true;
            NotableEvents.NukeDay = TimeSeconds / BalanceF("SecondsPerDay", 60.f);
            const float Radius = BalanceF("NukeRadiusTiles", 20);
            // Fallout: persistent contamination that decays very slowly.
            for (i32 Y = 0; Y < Height; ++Y)
                for (i32 X = 0; X < Width; ++X)
                    if (std::hypot(static_cast<float>(X) - NukePendingX,
                                   static_cast<float>(Y) - NukePendingY) <= Radius) {
                        TileGrid[static_cast<size_t>(Y) * Width + X].Contamination = 1.f;
                        TileIsFallout[static_cast<size_t>(Y) * Width + X] = 1;
                    }
            for (i32 i = static_cast<i32>(ZombieList.size()) - 1; i >= 0; --i)
                if (std::hypot(ZombieList[i].X - NukePendingX, ZombieList[i].Y - NukePendingY) <= Radius)
                    KillZombie(i, -1);
            for (FCitizen& C : CitizenList)
                if (C.State != ECitizenState::Dead && C.State != ECitizenState::Turned &&
                    C.State != ECitizenState::Evacuated &&
                    std::hypot(C.X - NukePendingX, C.Y - NukePendingY) <= Radius)
                    KillCitizen(C.Id, "radiation");
            for (i32 i = static_cast<i32>(WallList.size()) - 1; i >= 0; --i) {
                const FWall& W = WallList[i];
                if (std::hypot((W.X1 + W.X2) * 0.5f - NukePendingX,
                               (W.Y1 + W.Y2) * 0.5f - NukePendingY) <= Radius)
                    WallList.erase(WallList.begin() + i);
            }
            for (FBuilding& B : BuildingList) {
                if (B.bDemolished) continue;
                if (std::hypot(B.X + B.W * 0.5f - NukePendingX,
                               B.Y + B.H * 0.5f - NukePendingY) <= Radius) {
                    B.bDemolished = true;
                    Log(EEventType::BuildingDestroyed, "A building was vaporized by the nuclear blast.");
                }
            }
            AssignHomes();
            Morale -= BalanceF("NukeMoralePenalty", 50);
            AidCooldownDays = 1e9f;  // the world condemns the atrocity: aid stops
            Log(EEventType::NukeDetonated, "NUCLEAR DETONATION at (" +
                std::to_string(static_cast<i32>(NukePendingX)) + ", " +
                std::to_string(static_cast<i32>(NukePendingY)) + "). The city will never be the same.");
        }
    }
}

} // namespace oc
