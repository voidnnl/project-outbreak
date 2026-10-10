// UOCSteamSubsystem: Steamworks via OnlineSubsystemSteam.
// Achievements, cloud saves, rich presence, lobbies, leaderboards.
// App ID is 480 (Spacewar) until a real Steam AppID is assigned — see docs/11-assumptions.md.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OCSteamSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FOCLobbyInfo {
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Steam") FString LobbyId;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Steam") FString OwnerName;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Steam") int32 Players = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Steam") int32 MaxPlayers = 8;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Steam") FString MapName;
};

UCLASS()
class OUTBREAKCOMMANDER_API UOCSteamSubsystem : public UGameInstanceSubsystem {
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Steam")
    bool IsSteamAvailable() const;

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Steam")
    void UnlockAchievement(const FString& SteamApiName);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Steam")
    void StoreAchievements();

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Steam")
    void SetRichPresence(const FString& Status, const FString& City, float Day);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Steam")
    bool CloudSave(const FString& SlotName, const TArray<uint8>& Bytes);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Steam")
    bool CloudLoad(const FString& SlotName, TArray<uint8>& OutBytes);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Steam")
    void UploadLeaderboardScore(const FString& Board, int32 Score);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Steam")
    bool CreateLobby(int32 MaxPlayers, const FString& CampaignId);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Steam")
    bool JoinLobby(const FString& LobbyId);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Steam")
    void LeaveLobby();

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Steam")
    TArray<FOCLobbyInfo> GetLobbies() const { return CachedLobbies; }

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Steam")
    void InviteFriend();

    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Steam") FString CurrentLobbyId;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Steam") bool bSteamOn = false;

private:
    TArray<FOCLobbyInfo> CachedLobbies;
};
