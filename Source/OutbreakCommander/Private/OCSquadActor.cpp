#include "OCSquadActor.h"

AOCSquadActor::AOCSquadActor() {
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}
void AOCSquadActor::SyncFromSim(int32, const FString&, float X, float Y, float, bool) {
    constexpr float TileMeters = 10.f;
    SetActorLocation(FVector(X * TileMeters, Y * TileMeters, 20.f));
    SetActorHiddenInGame(false);
}

AOCVehicleActor::AOCVehicleActor() {
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}
void AOCVehicleActor::SyncFromSim(int32, const FString&, float X, float Y, float) {
    constexpr float TileMeters = 10.f;
    SetActorLocation(FVector(X * TileMeters, Y * TileMeters, 30.f));
    SetActorHiddenInGame(false);
}

AOCWallActor::AOCWallActor() {
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}
void AOCWallActor::SyncFromSim(int32, float X1, float Y1, float X2, float Y2, float, bool, bool, bool bUnderConstruction) {
    constexpr float TileMeters = 10.f;
    const FVector A(X1 * TileMeters, Y1 * TileMeters, 40.f);
    const FVector B(X2 * TileMeters, Y2 * TileMeters, 40.f);
    SetActorLocation((A + B) * 0.5f);
    SetActorHiddenInGame(bUnderConstruction);
}
