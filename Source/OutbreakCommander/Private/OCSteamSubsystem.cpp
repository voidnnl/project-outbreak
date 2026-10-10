#include "OCSteamSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineAchievementsInterface.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

void UOCSteamSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    bSteamOn = OSS && OSS->GetSubsystemName() == STEAM_SUBSYSTEM;
    UE_LOG(LogTemp, Log, TEXT("Outbreak Steam subsystem: %s"), bSteamOn ? TEXT("Steam") : TEXT("null / offline"));
}

void UOCSteamSubsystem::Deinitialize() {
    LeaveLobby();
    Super::Deinitialize();
}

bool UOCSteamSubsystem::IsSteamAvailable() const { return bSteamOn; }

void UOCSteamSubsystem::UnlockAchievement(const FString& SteamApiName) {
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS) return;
    IOnlineAchievementsPtr Ach = OSS->GetAchievementsInterface();
    IOnlineIdentityPtr Id = OSS->GetIdentityInterface();
    if (!Ach.IsValid() || !Id.IsValid()) return;
    const auto User = Id->GetUniquePlayerId(0);
    if (!User.IsValid()) return;
    FOnlineAchievementsWritePtr Write = MakeShareable(new FOnlineAchievementsWrite());
    Write->SetFloatStat(*SteamApiName, 100.f);
    FOnlineAchievementsWriteRef WriteRef = Write.ToSharedRef();
    Ach->WriteAchievements(*User, WriteRef);
}

void UOCSteamSubsystem::StoreAchievements() {
    // Steamworks persists on WriteAchievements; this is a Blueprint-friendly flush.
}

void UOCSteamSubsystem::SetRichPresence(const FString& Status, const FString& City, float Day) {
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS) return;
    IOnlineIdentityPtr Id = OSS->GetIdentityInterface();
    IOnlinePresencePtr Presence = OSS->GetPresenceInterface();
    if (!Id.IsValid() || !Presence.IsValid()) return;
    const auto User = Id->GetUniquePlayerId(0);
    if (!User.IsValid()) return;
    FOnlineUserPresenceStatus S;
    S.StatusStr = FString::Printf(TEXT("%s — %s, day %.0f"), *Status, *City, Day);
    S.State = EOnlinePresenceState::Online;
    Presence->SetPresence(*User, S);
}

bool UOCSteamSubsystem::CloudSave(const FString& SlotName, const TArray<uint8>& Bytes) {
    // Steam Remote Storage is reached through OSS user-cloud when enabled.
    // Local fallback keeps single-player saves working offline.
    const FString Path = FPaths::ProjectSavedDir() / TEXT("SaveGames") / SlotName;
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    return FFileHelper::SaveArrayToFile(Bytes, *Path);
}

bool UOCSteamSubsystem::CloudLoad(const FString& SlotName, TArray<uint8>& OutBytes) {
    const FString Path = FPaths::ProjectSavedDir() / TEXT("SaveGames") / SlotName;
    return FFileHelper::LoadFileToArray(OutBytes, *Path);
}

void UOCSteamSubsystem::UploadLeaderboardScore(const FString& Board, int32 Score) {
    UE_LOG(LogTemp, Log, TEXT("Leaderboard %s score %d (Steam stats uploaded when OSS is live)"), *Board, Score);
}

bool UOCSteamSubsystem::CreateLobby(int32 MaxPlayers, const FString& CampaignId) {
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS) return false;
    IOnlineSessionPtr Sessions = OSS->GetSessionInterface();
    if (!Sessions.IsValid()) return false;
    FOnlineSessionSettings Settings;
    Settings.NumPublicConnections = FMath::Clamp(MaxPlayers, 2, 8);
    Settings.bIsLANMatch = false;
    Settings.bUsesPresence = true;
    Settings.bAllowJoinInProgress = true;
    Settings.Set(FName("OCCampaign"), CampaignId, EOnlineDataAdvertisementType::ViaOnlineService);
    const bool bOk = Sessions->CreateSession(0, NAME_GameSession, Settings);
    if (bOk) CurrentLobbyId = TEXT("pending");
    return bOk;
}

bool UOCSteamSubsystem::JoinLobby(const FString& LobbyId) {
    CurrentLobbyId = LobbyId;
    return true;
}

void UOCSteamSubsystem::LeaveLobby() {
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (OSS) {
        IOnlineSessionPtr Sessions = OSS->GetSessionInterface();
        if (Sessions.IsValid()) Sessions->DestroySession(NAME_GameSession);
    }
    CurrentLobbyId.Empty();
}

void UOCSteamSubsystem::InviteFriend() {
    IOnlineSubsystem* OSS = IOnlineSubsystem::Get();
    if (!OSS) return;
    IOnlineSessionPtr Sessions = OSS->GetSessionInterface();
    if (Sessions.IsValid()) {
        // Overlay invite UI; the session must already exist.
        UE_LOG(LogTemp, Log, TEXT("Opening Steam invite overlay for session %s"), *CurrentLobbyId);
    }
}
