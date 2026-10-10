#include "OCZombieActor.h"
#include "Components/StaticMeshComponent.h"

AOCZombieActor::AOCZombieActor() {
    PrimaryActorTick.bCanEverTick = false;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
}

void AOCZombieActor::SyncFromSim(int32 InSimId, float X, float Y, float HpPct, const FString& TypeId,
                                 const FString& InModelPath) {
    constexpr float TileMeters = 10.f;
    SetActorLocation(FVector(X * TileMeters, Y * TileMeters, 50.f));
    SetActorHiddenInGame(false);
    ModelPath = InModelPath;
    // The actual mesh swap is a content task: assign the model asset matching
    // ModelPath (e.g. /Game/Assets/Monsters/monster_demon) in the BP subclass.
}
