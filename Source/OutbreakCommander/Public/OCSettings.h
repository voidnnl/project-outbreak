// UOCGameUserSettings: graphics, audio, input, accessibility, language.
// All values are user preferences (not gameplay). Gameplay values stay in CSV.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "OCSettings.generated.h"

UENUM(BlueprintType)
enum class EOCColorblindMode : uint8 { None, Protanopia, Deuteranopia, Tritanopia };

UENUM(BlueprintType)
enum class EOCUpscaleMode : uint8 { TSR, DLSS, FSR, XeSS, Off };

UENUM(BlueprintType)
enum class EOCQualityPreset : uint8 { Low, Medium, High, Epic, SteamDeck };

UCLASS(BlueprintType)
class OUTBREAKCOMMANDER_API UOCGameUserSettings : public UGameUserSettings {
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Outbreak|Settings")
    static UOCGameUserSettings* GetOCSettings();

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Settings")
    void ApplyOutbreakSettings();

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Settings")
    void ApplyQualityPreset(EOCQualityPreset Preset);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Settings")
    void ApplySteamDeckDefaults();

    UPROPERTY(Config, BlueprintReadWrite, Category = "Outbreak|Audio") float MasterVolume = 1.f;
    UPROPERTY(Config, BlueprintReadWrite, Category = "Outbreak|Audio") float SfxVolume = 1.f;
    UPROPERTY(Config, BlueprintReadWrite, Category = "Outbreak|Audio") float MusicVolume = 0.7f;
    UPROPERTY(Config, BlueprintReadWrite, Category = "Outbreak|Audio") bool bSubtitles = true;

    UPROPERTY(Config, BlueprintReadWrite, Category = "Outbreak|Access") EOCColorblindMode Colorblind = EOCColorblindMode::None;
    UPROPERTY(Config, BlueprintReadWrite, Category = "Outbreak|Access") float TextScale = 1.f;
    UPROPERTY(Config, BlueprintReadWrite, Category = "Outbreak|Access") float UIScale = 1.f;
    UPROPERTY(Config, BlueprintReadWrite, Category = "Outbreak|Access") bool bReducedMotion = false;
    UPROPERTY(Config, BlueprintReadWrite, Category = "Outbreak|Access") bool bScreenReaderPrompts = false;

    UPROPERTY(Config, BlueprintReadWrite, Category = "Outbreak|Graphics") EOCUpscaleMode Upscale = EOCUpscaleMode::TSR;
    UPROPERTY(Config, BlueprintReadWrite, Category = "Outbreak|Graphics") EOCQualityPreset Quality = EOCQualityPreset::High;
    UPROPERTY(Config, BlueprintReadWrite, Category = "Outbreak|Graphics") bool bVSync = true;
    UPROPERTY(Config, BlueprintReadWrite, Category = "Outbreak|Graphics") bool bEdgeScroll = true;

    UPROPERTY(Config, BlueprintReadWrite, Category = "Outbreak|Language") FString Language = TEXT("en");
};
