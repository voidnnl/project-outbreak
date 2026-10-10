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
};
