// UOCAudioManager: spatial SFX + adaptive music (calm / combat / dire).
// Cue paths point at Content/Audio; the editor checklist assigns the assets.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OCAudioManager.generated.h"

class UAudioComponent;
class USoundBase;
class UOCSimulationBridge;

UCLASS(ClassGroup = (Outbreak), meta = (BlueprintSpawnableComponent))
class OUTBREAKCOMMANDER_API UOCAudioManager : public UActorComponent {
    GENERATED_BODY()

public:
    UOCAudioManager();
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Audio")
    void BindBridge(UOCSimulationBridge* InBridge);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Audio")
    void PlayUISound(const FString& Cue);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Audio")
    void SetVolumes(float Master, float Sfx, float Music);

    UPROPERTY(EditAnywhere, Category = "Outbreak|Audio") TObjectPtr<USoundBase> MusicCalm;
    UPROPERTY(EditAnywhere, Category = "Outbreak|Audio") TObjectPtr<USoundBase> MusicCombat;
    UPROPERTY(EditAnywhere, Category = "Outbreak|Audio") TObjectPtr<USoundBase> MusicDire;
    UPROPERTY(EditAnywhere, Category = "Outbreak|Audio") TObjectPtr<USoundBase> SfxZombieHurt;
    UPROPERTY(EditAnywhere, Category = "Outbreak|Audio") TObjectPtr<USoundBase> SfxZombieDeath;
    UPROPERTY(EditAnywhere, Category = "Outbreak|Audio") TObjectPtr<USoundBase> SfxGun;
    UPROPERTY(EditAnywhere, Category = "Outbreak|Audio") TObjectPtr<USoundBase> SfxUIClick;
    UPROPERTY(EditAnywhere, Category = "Outbreak|Audio") float CrossfadeSeconds = 2.f;

private:
    void ApplyMusicState(const FString& State);

    UPROPERTY() TObjectPtr<UOCSimulationBridge> Bridge;
    UPROPERTY() TObjectPtr<UAudioComponent> MusicComp;
    FString CurrentState;
    float MasterVolume = 1.f, SfxVolume = 1.f, MusicVolume = 0.7f;
};
