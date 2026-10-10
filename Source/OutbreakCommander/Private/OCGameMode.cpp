#include "OCGameMode.h"
#include "OCGameState.h"
#include "OCGameInstance.h"
#include "OCPlayerController.h"
#include "OCGodPawn.h"
#include "OCSimulationBridge.h"
#include "oc/Simulation.h"
#include "Engine/World.h"

AOCGameMode::AOCGameMode() {
    DefaultPawnClass = AOCGodPawn::StaticClass();
    PlayerControllerClass = AOCPlayerController::StaticClass();
    GameStateClass = AOCGameState::StaticClass();
    bUseSeamlessTravel = false;
}

void AOCGameMode::BeginPlay() {
    Super::BeginPlay();

    Bridge = NewObject<UOCSimulationBridge>(this);
    if (!Bridge->Initialize(DataDirectory)) {
        UE_LOG(LogTemp, Error, TEXT("Outbreak: failed to load data from %s"), *DataDirectory);
        return;
    }
    oc::FSimConfig Cfg;
    Cfg.Seed = static_cast<oc::u64>(StartSeed);
    Cfg.MapWidth = StartMapWidth;
    Cfg.MapHeight = StartMapHeight;
    Cfg.InitialPopulation = StartPopulation;
    if (UOCGameInstance* GI = GetGameInstance<UOCGameInstance>()) {
        Cfg.Seed = static_cast<oc::u64>(GI->PendingSeed);
        Cfg.ScenarioId = TCHAR_TO_UTF8(*GI->PendingScenarioId);
        Cfg.CampaignId = TCHAR_TO_UTF8(*GI->PendingCampaignId);
        Cfg.DifficultyId = TCHAR_TO_UTF8(*GI->PendingDifficulty);
        Cfg.Language = TCHAR_TO_UTF8(*GI->Language);
        Cfg.bVersusMode = GI->bPendingVersus;
        Cfg.bDemoMode = GI->bPendingDemo;
        Cfg.bTutorial = GI->bPendingTutorial;
        Cfg.MaxPlayers = GI->bPendingCoop ? 8 : 1;
        Bridge->SetLanguage(GI->Language);
    }
    Bridge->NewGameEx(Cfg);

    if (AOCGameState* GS = GetGameState<AOCGameState>()) {
        GS->SetBridge(Bridge);
    }
}

void AOCGameMode::Tick(float DeltaSeconds) {
    Super::Tick(DeltaSeconds);
    if (Bridge) Bridge->Tick(DeltaSeconds);
}
