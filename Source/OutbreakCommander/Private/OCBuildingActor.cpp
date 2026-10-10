#include "OCBuildingActor.h"
#include "Components/StaticMeshComponent.h"

AOCBuildingActor::AOCBuildingActor() {
    PrimaryActorTick.bCanEverTick = false;
    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
}

void AOCBuildingActor::SyncFromSim(int32 InSimId, const FString& DefId, int32 X, int32 Y, int32 W, int32 H,
                                   bool bUnderConstruction, bool bPowered, float HpPct) {
    constexpr float TileMeters = 10.f;
    SetActorLocation(FVector((X + W * 0.5f) * TileMeters, (Y + H * 0.5f) * TileMeters, 0.f));
    SetActorScale3D(FVector(W * TileMeters / 100.f, H * TileMeters / 100.f, 1.f));  // assumes 1m cube base mesh

    if (UMaterialInstanceDynamic* Dyn = Mesh->CreateAndSetMaterialInstanceDynamic(0)) {
        Dyn->SetScalarParameterValue(PoweredParam, bPowered ? 1.f : 0.f);
        Dyn->SetScalarParameterValue(ConstructionParam, bUnderConstruction ? 1.f : 0.f);
    }
    SetActorHiddenInGame(bUnderConstruction && HpPct <= 0.f);
}
