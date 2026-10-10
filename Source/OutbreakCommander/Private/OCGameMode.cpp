#include "OCGameMode.h"
#include "OCGameState.h"
#include "OCPlayerController.h"
#include "OCGodPawn.h"
#include "OCSimulationBridge.h"
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
    Bridge->NewGame(StartSeed, StartMapWidth, StartMapHeight, StartPopulation);

    if (AOCGameState* GS = GetGameState<AOCGameState>()) {
        GS->SetBridge(Bridge);
    }
}

void AOCGameMode::Tick(float DeltaSeconds) {
    Super::Tick(DeltaSeconds);
    if (Bridge) Bridge->Tick(DeltaSeconds);
}
