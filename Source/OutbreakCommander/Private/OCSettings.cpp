#include "OCSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"

UOCGameUserSettings* UOCGameUserSettings::GetOCSettings() {
    return Cast<UOCGameUserSettings>(GEngine ? GEngine->GetGameUserSettings() : nullptr);
}

void UOCGameUserSettings::ApplyQualityPreset(EOCQualityPreset Preset) {
    Quality = Preset;
    int32 Level = 2;
    switch (Preset) {
    case EOCQualityPreset::Low: Level = 0; break;
    case EOCQualityPreset::Medium: Level = 1; break;
    case EOCQualityPreset::High: Level = 2; break;
    case EOCQualityPreset::Epic: Level = 3; break;
    case EOCQualityPreset::SteamDeck: Level = 1; UIScale = 1.25f; Upscale = EOCUpscaleMode::FSR; break;
    }
    SetOverallScalabilityLevel(Level);
    ApplyOutbreakSettings();
}

void UOCGameUserSettings::ApplySteamDeckDefaults() {
    ApplyQualityPreset(EOCQualityPreset::SteamDeck);
    bVSync = true;
    SetFrameRateLimit(60.f);
    UIScale = 1.25f;
    TextScale = 1.1f;
    ApplyOutbreakSettings();
}

void UOCGameUserSettings::ApplyOutbreakSettings() {
    SetVSyncEnabled(bVSync);
    ApplySettings(false);
    if (GEngine) {
        FString Cmd;
        switch (Upscale) {
        case EOCUpscaleMode::DLSS: Cmd = TEXT("r.NGX.DLSS.Enable 1"); break;
        case EOCUpscaleMode::FSR: Cmd = TEXT("r.FidelityFX.FSR.Enabled 1"); break;
        case EOCUpscaleMode::XeSS: Cmd = TEXT("r.XeSS.Enabled 1"); break;
        case EOCUpscaleMode::Off: Cmd = TEXT("r.AntiAliasingMethod 0"); break;
        default: Cmd = TEXT("r.AntiAliasingMethod 4"); break;  // TSR
        }
        GEngine->Exec(nullptr, *Cmd);
        GEngine->Exec(nullptr, *FString::Printf(TEXT("sl.TextScale %f"), TextScale));
    }
}
