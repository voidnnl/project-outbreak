#include "OCGameInstance.h"
#include "Engine/Engine.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"

void UOCGameInstance::RefreshSaveSlots() {
    SaveSlots.Reset();
    const FString SaveDir = FPaths::ProjectSavedDir() / TEXT("SaveGames");
    IPlatformFile& PFile = FPlatformFileManager::Get().GetPlatformFile();
    PFile.IterateDirectory(*SaveDir, [this](const TCHAR* Path, bool bIsDir) {
        if (!bIsDir && FPaths::GetExtension(Path) == TEXT("sav"))
            SaveSlots.Add(FPaths::GetCleanFilename(Path));
        return true;
    });
}
