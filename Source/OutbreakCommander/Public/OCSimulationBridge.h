// UOCSimulationBridge: the single owner of the oc::FSimulation inside UE.
// The game mode ticks it; controllers send player commands through it; the
// view manager and HUD read snapshots from it. All gameplay rules stay in the
// engine-agnostic core, so this class is intentionally thin.
#pragma once

#include "CoreMinimal.h"
#include "OCTypes.h"
#include "oc/Simulation.h"
#include "UObject/Object.h"
#include "OCSimulationBridge.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSimEvent, int32, EventType, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStatsUpdated, const FOCStatsSnapshot&, Stats);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResourcesUpdated, const FOCResourcesSnapshot&, Resources);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameResult, EOCGameResult, Result);

UCLASS(BlueprintType)
class OUTBREAKCOMMANDER_API UOCSimulationBridge : public UObject {
    GENERATED_BODY()

public:
    // Loads the CSV data tables. DataDir is a filesystem path (Content/Data).
    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool Initialize(const FString& DataDir);

    // Starts a new game. ScenarioId selects a preset from Scenarios.csv
    // ("standard", "virulent", "blackout", "overrun", "sandbox", ...).
    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    void NewGame(int32 Seed, int32 MapWidth, int32 MapHeight, int32 Population,
                 const FString& ScenarioId = TEXT("standard"));

    // Called by the game mode every frame with real delta seconds.
    void Tick(float DeltaSeconds);

    // --- construction ---
    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    int32 PlaceBuilding(const FString& DefId, int32 X, int32 Y);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    int32 BuildWallLine(int32 X1, int32 Y1, int32 X2, int32 Y2);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool DemolishBuilding(int32 BuildingId);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    int32 BuildPowerLine(int32 DistrictA, int32 DistrictB);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    int32 BuildWaterPipe(int32 DistrictA, int32 DistrictB);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool DemolishPowerLine(int32 DistrictA, int32 DistrictB);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool DemolishWaterPipe(int32 DistrictA, int32 DistrictB);

    // --- military ---
    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    int32 RecruitSquad(const FString& UnitTypeId, int32 X, int32 Y);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool IssueOrder(int32 SquadId, EOCSquadOrder Order, int32 TargetX, int32 TargetY);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool DisbandSquad(int32 SquadId);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    int32 DeployVehicle(const FString& TypeId, int32 X, int32 Y);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool OrderVehicleMove(int32 VehicleId, int32 X, int32 Y);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool EmbarkSquad(int32 SquadId, int32 VehicleId);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool DisembarkSquad(int32 SquadId);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool CallAirstrike(int32 X, int32 Y);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool CallNuke(int32 X, int32 Y);

    // --- research, crafting, policies, international ---
    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool StartResearch(const FString& TechId);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool QueueRecipe(const FString& RecipeId);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    void SetPolicy(EOCPolicy Policy, bool bEnabled);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool RequestAid();

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool SendTradeCaravan();

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool CallEvacuationFlight();

    // --- save / load / report ---
    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool SaveGame(const FString& Path);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool LoadGame(const FString& Path);

    // The procedural end-of-game document (markdown).
    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    FString GenerateAfterActionReport();

    // --- queries ---
    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    FOCStatsSnapshot GetStatsSnapshot() const;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    FOCResourcesSnapshot GetResourcesSnapshot() const;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    FOCEnvironmentInfo GetEnvironmentInfo() const;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    TArray<FOCSquadInfo> GetSquads() const;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    TArray<FOCVehicleInfo> GetVehicles() const;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    EOCGameResult GetGameResult() const;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    FString GetResultReason() const { return UTF8_TO_TCHAR(Sim.GetResultReason().c_str()); }

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool IsTechResearched(const FString& TechId) const;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    bool IsDistrictPowered(int32 District) const { return Sim.IsDistrictPowered(District); }

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Sim")
    int32 DistrictCount() const { return Sim.DistrictCount(); }

    // Read-only access for the view manager (spawning/updating actors).
    const oc::FSimulation& GetSim() const { return Sim; }

    UPROPERTY(BlueprintAssignable, Category = "Outbreak|Sim") FOnSimEvent OnSimEvent;
    UPROPERTY(BlueprintAssignable, Category = "Outbreak|Sim") FOnStatsUpdated OnStatsUpdated;
    UPROPERTY(BlueprintAssignable, Category = "Outbreak|Sim") FOnResourcesUpdated OnResourcesUpdated;
    UPROPERTY(BlueprintAssignable, Category = "Outbreak|Sim") FOnGameResult OnGameResult;

private:
    void BroadcastNewEvents();
    void BroadcastResultIfChanged();

    oc::FSimulation Sim;
    bool bInitialized = false;
    size_t LastEventCount = 0;
    EOCGameResult LastResult = EOCGameResult::Ongoing;
};
