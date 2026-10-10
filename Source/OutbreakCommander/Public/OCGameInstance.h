// UOCGameInstance: project-level game instance. Owns cross-map state:
// save slots metadata, pending co-op session info, and (later) Steam cloud
// save handles. Phase 1 keeps it minimal.
#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "OCGameInstance.generated.h"

UCLASS()
class OUTBREAKCOMMANDER_API UOCGameInstance : public UGameInstance {
    GENERATED_BODY()

public:
    // Save slot names (manual slots + autosave). Cloud sync via Steamworks is
    // a Phase 5 task; slots are local files for now.
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Save") TArray<FString> SaveSlots;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Save")
    void RefreshSaveSlots();

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Save")
    FString SlotPath(const FString& SlotName) const;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Save")
    FString AutosavePath() const { return SlotPath(TEXT("autosave.sav")); }

    UPROPERTY(BlueprintReadWrite, Category = "Outbreak|Session") FString PendingCampaignId;
    UPROPERTY(BlueprintReadWrite, Category = "Outbreak|Session") FString PendingScenarioId = TEXT("standard");
    UPROPERTY(BlueprintReadWrite, Category = "Outbreak|Session") FString PendingDifficulty = TEXT("normal");
    UPROPERTY(BlueprintReadWrite, Category = "Outbreak|Session") FString Language = TEXT("en");
    UPROPERTY(BlueprintReadWrite, Category = "Outbreak|Session") int32 PendingSeed = 20261009;
    UPROPERTY(BlueprintReadWrite, Category = "Outbreak|Session") bool bPendingCoop = false;
    UPROPERTY(BlueprintReadWrite, Category = "Outbreak|Session") bool bPendingVersus = false;
    UPROPERTY(BlueprintReadWrite, Category = "Outbreak|Session") bool bPendingDemo = false;
    UPROPERTY(BlueprintReadWrite, Category = "Outbreak|Session") bool bPendingTutorial = false;
};
