#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OCSquadActor.generated.h"

UCLASS()
class OUTBREAKCOMMANDER_API AOCSquadActor : public AActor {
    GENERATED_BODY()
public:
    AOCSquadActor();
    UFUNCTION(BlueprintCallable, Category = "Outbreak|View")
    void SyncFromSim(int32 InSimId, const FString& UnitType, float X, float Y, float HpPct, bool bActive);
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
};

UCLASS()
class OUTBREAKCOMMANDER_API AOCVehicleActor : public AActor {
    GENERATED_BODY()
public:
    AOCVehicleActor();
    UFUNCTION(BlueprintCallable, Category = "Outbreak|View")
    void SyncFromSim(int32 InSimId, const FString& TypeId, float X, float Y, float HpPct);
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
};

UCLASS()
class OUTBREAKCOMMANDER_API AOCWallActor : public AActor {
    GENERATED_BODY()
public:
    AOCWallActor();
    UFUNCTION(BlueprintCallable, Category = "Outbreak|View")
    void SyncFromSim(int32 InSimId, float X1, float Y1, float X2, float Y2, float HpPct, bool bGate, bool bCorner, bool bUnderConstruction);
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
};
