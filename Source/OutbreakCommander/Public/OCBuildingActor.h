// AOCBuildingActor: visual representation of one sim building. The view
// manager pools these and calls SyncFromSim every frame. Color/material params
// communicate powered / under-construction / demolished states for readability
// at every zoom level (brief section 3).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OCBuildingActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class OUTBREAKCOMMANDER_API AOCBuildingActor : public AActor {
    GENERATED_BODY()

public:
    AOCBuildingActor();

    // SimBuildingId ties this actor to the sim entity (-1 = free pooled actor).
    UFUNCTION(BlueprintCallable, Category = "Outbreak|Building")
    void SyncFromSim(int32 InSimId, const FString& DefId, int32 X, int32 Y, int32 W, int32 H,
                     bool bUnderConstruction, bool bPowered, float HpPct);

    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(EditAnywhere, Category = "Outbreak|Building") FName PoweredParam = TEXT("Powered");
    UPROPERTY(EditAnywhere, Category = "Outbreak|Building") FName ConstructionParam = TEXT("UnderConstruction");
};
