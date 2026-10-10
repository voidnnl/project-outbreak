#include "OCSimViewManager.h"
#include "OCBuildingActor.h"
#include "OCCitizenActor.h"
#include "OCZombieActor.h"
#include "OCGameMode.h"
#include "OCSimulationBridge.h"
#include "Engine/World.h"

AOCSimViewManager::AOCSimViewManager() {
    PrimaryActorTick.bCanEverTick = true;
}

UOCSimulationBridge* AOCSimViewManager::GetBridge() const {
    if (const AOCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AOCGameMode>() : nullptr)
        return GM->GetBridge();
    return nullptr;
}

void AOCSimViewManager::Tick(float DeltaSeconds) {
    Super::Tick(DeltaSeconds);
    if (GetBridge()) SyncAll();
}

void AOCSimViewManager::SyncAll() {
    SyncBuildings();
    SyncZombies();
    SyncCitizens();
}

template <typename TActor>
TActor* AOCSimViewManager::AcquireActor(TArray<TWeakObjectPtr<TActor>>& Pool, int32 SimId) {
    // Reuse a hidden (free) slot if one exists.
    for (TWeakObjectPtr<TActor>& Slot : Pool)
        if (Slot.IsValid() && Slot->IsHidden()) return Slot.Get();
    // Otherwise grow the pool.
    TSubclassOf<TActor> Class;
    if constexpr (std::is_same_v<TActor, AOCBuildingActor>) Class = BuildingActorClass;
    else if constexpr (std::is_same_v<TActor, AOCZombieActor>) Class = ZombieActorClass;
    else Class = CitizenActorClass;
    if (!Class) return nullptr;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    TActor* A = GetWorld()->SpawnActor<TActor>(Class, FVector::ZeroVector, FRotator::ZeroRotator, Params);
    Pool.Add(A);
    return A;
}

void AOCSimViewManager::SyncBuildings() {
    const oc::FSimulation& Sim = GetBridge()->GetSim();
    TSet<int32> Seen;
    for (const oc::FBuilding& B : Sim.Buildings()) {
        if (B.bDemolished) continue;
        Seen.Add(B.Id);
        AOCBuildingActor* Actor = nullptr;
        if (const int32* Idx = BuildingIndexBySimId.Find(B.Id))
            Actor = BuildingPool[*Idx].Get();
        if (!Actor) {
            Actor = AcquireActor(BuildingPool, B.Id);
            if (!Actor) continue;
            BuildingIndexBySimId.Add(B.Id, BuildingPool.Num() - 1);
        }
        Actor->SyncFromSim(B.Id, UTF8_TO_TCHAR(B.DefId.c_str()), B.X, B.Y, B.W, B.H,
                           B.bUnderConstruction, B.bPowered, B.Hp);
    }
    // Hide actors whose sim building was demolished.
    for (auto& Pair : BuildingIndexBySimId) {
        if (!Seen.Contains(Pair.Key) && BuildingPool[Pair.Value].IsValid())
            BuildingPool[Pair.Value]->SetActorHiddenInGame(true);
    }
}

void AOCSimViewManager::SyncZombies() {
    const oc::FSimulation& Sim = GetBridge()->GetSim();
    TSet<int32> Seen;
    for (const oc::FZombie& Z : Sim.Zombies()) {
        Seen.Add(Z.Id);
        AOCZombieActor* Actor = nullptr;
        if (const int32* Idx = ZombieIndexBySimId.Find(Z.Id))
            Actor = ZombiePool[*Idx].Get();
        if (!Actor) {
            Actor = AcquireActor(ZombiePool, Z.Id);
            if (!Actor) continue;
            ZombieIndexBySimId.Add(Z.Id, ZombiePool.Num() - 1);
        }
        // Each zombie type maps to its own 3D model (ZombieTypes.csv 'Model').
        const oc::FZombieTypeDef* TypeDef = Sim.Data().FindZombieType(Z.TypeId);
        const FString ModelPath = TypeDef ? UTF8_TO_TCHAR(TypeDef->Model.c_str()) : FString();
        Actor->SyncFromSim(Z.Id, Z.X, Z.Y, 1.f, UTF8_TO_TCHAR(Z.TypeId.c_str()), ModelPath);
    }
    for (auto& Pair : ZombieIndexBySimId)
        if (!Seen.Contains(Pair.Key) && ZombiePool[Pair.Value].IsValid())
            ZombiePool[Pair.Value]->SetActorHiddenInGame(true);
}

void AOCSimViewManager::SyncCitizens() {
    const oc::FSimulation& Sim = GetBridge()->GetSim();
    TSet<int32> Seen;
    for (const oc::FCitizen& C : Sim.Citizens()) {
        if (C.State == oc::ECitizenState::Turned || C.State == oc::ECitizenState::Evacuated) continue;
        Seen.Add(C.Id);
        AOCCitizenActor* Actor = nullptr;
        if (const int32* Idx = CitizenIndexBySimId.Find(C.Id))
            Actor = CitizenPool[*Idx].Get();
        if (!Actor) {
            Actor = AcquireActor(CitizenPool, C.Id);
            if (!Actor) continue;
            CitizenIndexBySimId.Add(C.Id, CitizenPool.Num() - 1);
        }
        EOCCitizenVisualState VS = EOCCitizenVisualState::Healthy;
        if (C.State == oc::ECitizenState::Dead) VS = EOCCitizenVisualState::Dead;
        else if (C.State == oc::ECitizenState::Panicked) VS = EOCCitizenVisualState::Panicked;
        else if (C.bLatentInfection) VS = EOCCitizenVisualState::Infected;
        Actor->SyncFromSim(C.Id, C.X, C.Y, VS);
        // Citizen models are chosen by gender (Content/Assets/Characters/):
        //   citizen_male_a/b.glb, citizen_female_a/b.glb - the actor BP picks
        //   one based on this flag.
        Actor->bFemale = C.bFemale;
    }
    for (auto& Pair : CitizenIndexBySimId)
        if (!Seen.Contains(Pair.Key) && CitizenPool[Pair.Value].IsValid())
            CitizenPool[Pair.Value]->SetActorHiddenInGame(true);
}
