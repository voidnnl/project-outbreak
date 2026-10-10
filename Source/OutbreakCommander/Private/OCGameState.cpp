#include "OCGameState.h"

void AOCGameState::SetBridge(UOCSimulationBridge* InBridge) {
    SimBridge = InBridge;
    if (!SimBridge) return;

    SimBridge->OnStatsUpdated.AddDynamic(this, &AOCGameState::HandleStatsUpdated);
    SimBridge->OnResourcesUpdated.AddDynamic(this, &AOCGameState::HandleResourcesUpdated);
    SimBridge->OnSimEvent.AddDynamic(this, &AOCGameState::HandleSimEvent);
    SimBridge->OnGameResult.AddDynamic(this, &AOCGameState::HandleGameResult);
}
