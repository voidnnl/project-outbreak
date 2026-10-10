#include "OCCrashReporter.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/DateTime.h"

void UOCCrashReporter::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);
    const FString CrashDir = FPaths::ProjectSavedDir() / TEXT("Crashes");
    IFileManager::Get().MakeDirectory(*CrashDir, true);
    Breadcrumb(TEXT("session start"));
}

void UOCCrashReporter::Breadcrumb(const FString& Message) {
    LastBreadcrumb = FString::Printf(TEXT("%s %s"), *FDateTime::UtcNow().ToIso8601(), *Message);
    const FString Path = FPaths::ProjectSavedDir() / TEXT("Crashes") / TEXT("breadcrumbs.log");
    FFileHelper::SaveStringToFile(LastBreadcrumb + TEXT("\n"), *Path, FFileHelper::EEncodingOptions::AutoDetect,
                                  &IFileManager::Get(), FILEWRITE_Append);
}

void UOCCrashReporter::NotifyGameResult(const FString& Result, const FString& Reason) {
    Breadcrumb(FString::Printf(TEXT("game %s: %s"), *Result, *Reason));
}
