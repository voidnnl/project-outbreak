#include "OCNetGameMode.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

AOCNetGameMode::AOCNetGameMode() {
    bUseSeamlessTravel = true;
}

void AOCNetGameMode::PostLogin(APlayerController* NewPlayer) {
    Super::PostLogin(NewPlayer);
    ++PlayerCount;
    UE_LOG(LogTemp, Log, TEXT("Outbreak co-op: player %d joined as %d"), PlayerCount, (int32)RoleForPlayer(PlayerCount - 1));
}

void AOCNetGameMode::Logout(AController* Exiting) {
    Super::Logout(Exiting);
    PlayerCount = FMath::Max(0, PlayerCount - 1);
}

EOCNetRole AOCNetGameMode::RoleForPlayer(int32 PlayerIndex) const {
    if (bVersus && PlayerIndex == 1) return EOCNetRole::Virus;
    static const EOCNetRole Cycle[] = {
        EOCNetRole::Commander, EOCNetRole::Engineer, EOCNetRole::Medic, EOCNetRole::Logistics
    };
    return Cycle[FMath::Clamp(PlayerIndex, 0, 3) % 4];
}
