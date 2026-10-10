// AOCNetGameMode: authoritative listen-server for co-op 2-8 and versus.
// Clients send commands; the server ticks one FSimulation and replicates snapshots.
#pragma once

#include "CoreMinimal.h"
#include "OCGameMode.h"
#include "OCTypes.h"
#include "OCNetGameMode.generated.h"

UENUM(BlueprintType)
enum class EOCNetRole : uint8 { Commander, Engineer, Medic, Logistics, Virus };

UCLASS()
class OUTBREAKCOMMANDER_API AOCNetGameMode : public AOCGameMode {
    GENERATED_BODY()

public:
    AOCNetGameMode();

    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Net")
    EOCNetRole RoleForPlayer(int32 PlayerIndex) const;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Net")
    int32 ConnectedPlayers() const { return PlayerCount; }

    UPROPERTY(EditDefaultsOnly, Category = "Outbreak|Net") int32 MaxCoopPlayers = 8;
    UPROPERTY(EditDefaultsOnly, Category = "Outbreak|Net") bool bVersus = false;
    UPROPERTY(EditDefaultsOnly, Category = "Outbreak|Net") FString CampaignId;

private:
    int32 PlayerCount = 0;
};
