// AOCCitizenActor: visual representation of one sim citizen. Pooled by the
// view manager; color encodes state (healthy / infected / panicked) so the
// infection heatmap is readable at a glance (brief sections 3 and 13).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OCCitizenActor.generated.h"

class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EOCCitizenVisualState : uint8 { Healthy, Infected, Panicked, Dead };

UCLASS()
class OUTBREAKCOMMANDER_API AOCCitizenActor : public AActor {
    GENERATED_BODY()

public:
    AOCCitizenActor();

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Citizen")
    void SyncFromSim(int32 InSimId, float X, float Y, EOCCitizenVisualState State);

    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;

    // Drives which character model the BP assigns (male/female variants).
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Citizen") bool bFemale = false;
};
