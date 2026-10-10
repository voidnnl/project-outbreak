// AOCZombieActor: visual representation of one sim zombie. Pooled by the view
// manager. Each zombie TYPE maps to a different 3D model (ZombieTypes.csv
// 'Model' column -> Content/Assets/Monsters/), so walkers, runners, tanks,
// spitters and mutants are visually distinct.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OCZombieActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class OUTBREAKCOMMANDER_API AOCZombieActor : public AActor {
    GENERATED_BODY()

public:
    AOCZombieActor();

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Zombie")
    void SyncFromSim(int32 InSimId, float X, float Y, float HpPct, const FString& TypeId,
                     const FString& ModelPath);

    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Mesh;

    // Asset path of the model for this zombie type (Content/Assets/<path>).
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Zombie") FString ModelPath;
};
