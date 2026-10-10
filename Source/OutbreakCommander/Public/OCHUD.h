// UOCHUDWidget: the in-game HUD. Bound to sim snapshots; no gameplay rules.
// Create WBP_OCHUD in the editor with this as the parent class (see docs/08-editor-checklist.md).
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OCTypes.h"
#include "OCHUD.generated.h"

class UOCSimulationBridge;

USTRUCT(BlueprintType)
struct FOCObjectiveView {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|HUD") FString Id;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|HUD") FString Title;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|HUD") FString Description;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|HUD") int32 Progress = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|HUD") int32 Target = 1;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|HUD") bool bComplete = false;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|HUD") bool bActive = false;
};

USTRUCT(BlueprintType)
struct FOCHintView {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|HUD") FString Title;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|HUD") FString Body;
};

USTRUCT(BlueprintType)
struct FOCHeatmapCell {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|HUD") int32 X = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|HUD") int32 Y = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|HUD") float Heat = 0.f;
};

UCLASS(BlueprintType)
class OUTBREAKCOMMANDER_API UOCHUDWidget : public UUserWidget {
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Outbreak|HUD")
    void BindBridge(UOCSimulationBridge* InBridge);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|HUD")
    FOCStatsSnapshot GetStats() const;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|HUD")
    FOCResourcesSnapshot GetResources() const;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|HUD")
    TArray<FOCObjectiveView> GetObjectives() const;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|HUD")
    TArray<FOCHintView> GetHints() const;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|HUD")
    TArray<FOCHeatmapCell> GetHeatmap(int32 Downsample = 4) const;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|HUD")
    FString Localize(const FString& Key) const;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|HUD")
    FString GetMusicState() const;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|HUD")
    FString GetSeason() const;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|HUD")
    bool DismissHint();

    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|HUD") bool bShowHeatmap = false;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|HUD") bool bShowMinimap = true;

protected:
    UPROPERTY() TObjectPtr<UOCSimulationBridge> Bridge;
};
