// UOCCrashReporter: writes a crash/assert breadcrumb file the next boot can
// upload. Wire a Sentry DSN at packaging time (docs/07-steam-upload.md).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OCCrashReporter.generated.h"

UCLASS()
class OUTBREAKCOMMANDER_API UOCCrashReporter : public UGameInstanceSubsystem {
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Crash")
    void Breadcrumb(const FString& Message);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Crash")
    void NotifyGameResult(const FString& Result, const FString& Reason);

    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Crash") FString LastBreadcrumb;
};
