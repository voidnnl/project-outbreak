// AOCGameState: replication-friendly home for the sim bridge pointer and the
// delegates the HUD binds to. (Multiplayer replication of snapshots is a
// Phase 3 task; in Phase 1 everything runs on the server/listen host.)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "OCSimulationBridge.h"
#include "OCGameState.generated.h"

UCLASS()
class OUTBREAKCOMMANDER_API AOCGameState : public AGameStateBase {
    GENERATED_BODY()

public:
    // Binds bridge delegates so widgets can subscribe to game state updates.
    void SetBridge(UOCSimulationBridge* InBridge);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    UOCSimulationBridge* GetBridge() const { return SimBridge; }

    UPROPERTY(BlueprintAssignable, Category = "Outbreak|Sim") FOnStatsUpdated OnStatsUpdated;
    UPROPERTY(BlueprintAssignable, Category = "Outbreak|Sim") FOnResourcesUpdated OnResourcesUpdated;
    UPROPERTY(BlueprintAssignable, Category = "Outbreak|Sim") FOnSimEvent OnSimEvent;
    UPROPERTY(BlueprintAssignable, Category = "Outbreak|Sim") FOnGameResult OnGameResult;

private:
    // Re-broadcast bridge delegates for widgets.
    UFUNCTION() void HandleStatsUpdated(const FOCStatsSnapshot& Stats) { OnStatsUpdated.Broadcast(Stats); }
    UFUNCTION() void HandleResourcesUpdated(const FOCResourcesSnapshot& R) { OnResourcesUpdated.Broadcast(R); }
    UFUNCTION() void HandleSimEvent(int32 EventType, const FString& Message) { OnSimEvent.Broadcast(EventType, Message); }
    UFUNCTION() void HandleGameResult(EOCGameResult Result) { OnGameResult.Broadcast(Result); }

    UPROPERTY() TObjectPtr<UOCSimulationBridge> SimBridge;
};
