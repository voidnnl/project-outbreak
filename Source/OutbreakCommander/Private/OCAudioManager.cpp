#include "OCAudioManager.h"
#include "OCHUD.h"
#include "OCSimulationBridge.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UOCAudioManager::UOCAudioManager() {
    PrimaryComponentTick.bCanEverTick = true;
}

void UOCAudioManager::BindBridge(UOCSimulationBridge* InBridge) { Bridge = InBridge; }

void UOCAudioManager::SetVolumes(float Master, float Sfx, float Music) {
    MasterVolume = Master; SfxVolume = Sfx; MusicVolume = Music;
    if (MusicComp) MusicComp->SetVolumeMultiplier(MasterVolume * MusicVolume);
}

void UOCAudioManager::PlayUISound(const FString& Cue) {
    (void)Cue;
    if (SfxUIClick)
        UGameplayStatics::PlaySound2D(this, SfxUIClick, MasterVolume * SfxVolume);
}

void UOCAudioManager::ApplyMusicState(const FString& State) {
    if (State == CurrentState) return;
    CurrentState = State;
    USoundBase* Cue = MusicCalm;
    if (State == TEXT("combat")) Cue = MusicCombat;
    else if (State == TEXT("dire")) Cue = MusicDire;
    if (!Cue) return;
    if (!MusicComp) {
        MusicComp = NewObject<UAudioComponent>(GetOwner());
        MusicComp->RegisterComponent();
        MusicComp->bAutoDestroy = false;
        MusicComp->bIsUISound = false;
    }
    MusicComp->SetSound(Cue);
    MusicComp->SetVolumeMultiplier(MasterVolume * MusicVolume);
    MusicComp->FadeIn(CrossfadeSeconds, MasterVolume * MusicVolume);
}

void UOCAudioManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!Bridge) return;
    FString State = TEXT("calm");
    switch (Bridge->GetSim().MusicState()) {
    case oc::EMusicState::Combat: State = TEXT("combat"); break;
    case oc::EMusicState::Dire: State = TEXT("dire"); break;
    default: break;
    }
    ApplyMusicState(State);
}
