// AOCGameMode: owns the simulation bridge, starts new games, ticks the sim.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OCGameMode.generated.h"

class UOCSimulationBridge;

UCLASS()
class OUTBREAKCOMMANDER_API AOCGameMode : public AGameModeBase {
    GENERATED_BODY()

public:
    AOCGameMode();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    // Filesystem path to the CSV data directory (Content/Data in dev builds).
    UPROPERTY(EditDefaultsOnly, Category = "Outbreak|Sim") FString DataDirectory = TEXT("Content/Data");

    UPROPERTY(EditDefaultsOnly, Category = "Outbreak|Sim") int32 StartSeed = 20261009;
    UPROPERTY(EditDefaultsOnly, Category = "Outbreak|Sim") int32 StartMapWidth = 96;
    UPROPERTY(EditDefaultsOnly, Category = "Outbreak|Sim") int32 StartMapHeight = 96;
    UPROPERTY(EditDefaultsOnly, Category = "Outbreak|Sim") int32 StartPopulation = 150;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    UOCSimulationBridge* GetBridge() const { return Bridge; }

private:
    UPROPERTY() TObjectPtr<UOCSimulationBridge> Bridge;
};
