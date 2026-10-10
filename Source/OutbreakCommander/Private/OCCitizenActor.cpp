#include "OCCitizenActor.h"
#include "Components/StaticMeshComponent.h"

AOCCitizenActor::AOCCitizenActor() {
    PrimaryActorTick.bCanEverTick = false;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
}

void AOCCitizenActor::SyncFromSim(int32 InSimId, float X, float Y, EOCCitizenVisualState State) {
    constexpr float TileMeters = 10.f;
    SetActorLocation(FVector(X * TileMeters, Y * TileMeters, 0.f));
    SetActorHiddenInGame(State == EOCCitizenVisualState::Dead);
    // Tint by state via a dynamic material (content task): green/red/amber.
}
