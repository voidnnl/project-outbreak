// AOCSimViewManager: keeps the actor world in sync with the simulation.
// Every frame it diffs sim entities against pooled actors and spawns,
// updates, or despawns accordingly. This is the ONLY place that reads sim
// entity positions directly (besides the HUD reading snapshots), which keeps
// the presentation layer thin and the sim authoritative.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OCSimViewManager.generated.h"

class UOCSimulationBridge;
class AOCBuildingActor;
class AOCZombieActor;
class AOCCitizenActor;

UCLASS()
class OUTBREAKCOMMANDER_API AOCSimViewManager : public AActor {
    GENERATED_BODY()

public:
    AOCSimViewManager();

    virtual void Tick(float DeltaSeconds) override;

    // Pool sizes; zombies/citizens scale with the outbreak, so pools grow.
    UPROPERTY(EditAnywhere, Category = "Outbreak|View") int32 InitialBuildingPool = 256;
    UPROPERTY(EditAnywhere, Category = "Outbreak|View") int32 InitialZombiePool = 512;
    UPROPERTY(EditAnywhere, Category = "Outbreak|View") int32 InitialCitizenPool = 1024;

    UPROPERTY(EditAnywhere, Category = "Outbreak|View") TSubclassOf<AOCBuildingActor> BuildingActorClass;
    UPROPERTY(EditAnywhere, Category = "Outbreak|View") TSubclassOf<AOCZombieActor> ZombieActorClass;
    UPROPERTY(EditAnywhere, Category = "Outbreak|View") TSubclassOf<AOCCitizenActor> CitizenActorClass;

private:
    void SyncAll();
    void SyncBuildings();
    void SyncZombies();
    void SyncCitizens();
    UOCSimulationBridge* GetBridge() const;

    template <typename TActor>
    TActor* AcquireActor(TArray<TWeakObjectPtr<TActor>>& Pool, int32 SimId);

    UPROPERTY() TArray<TWeakObjectPtr<AOCBuildingActor>> BuildingPool;
    UPROPERTY() TArray<TWeakObjectPtr<AOCZombieActor>> ZombiePool;
    UPROPERTY() TArray<TWeakObjectPtr<AOCCitizenActor>> CitizenPool;

    // sim id -> pooled actor (index into the pools)
    UPROPERTY() TMap<int32, int32> BuildingIndexBySimId;
    UPROPERTY() TMap<int32, int32> ZombieIndexBySimId;
    UPROPERTY() TMap<int32, int32> CitizenIndexBySimId;
};
