#include "OCHUD.h"
#include "OCSimulationBridge.h"
#include "oc/Simulation.h"

void UOCHUDWidget::BindBridge(UOCSimulationBridge* InBridge) { Bridge = InBridge; }

FOCStatsSnapshot UOCHUDWidget::GetStats() const {
    return Bridge ? Bridge->GetStatsSnapshot() : FOCStatsSnapshot{};
}
FOCResourcesSnapshot UOCHUDWidget::GetResources() const {
    return Bridge ? Bridge->GetResourcesSnapshot() : FOCResourcesSnapshot{};
}

TArray<FOCObjectiveView> UOCHUDWidget::GetObjectives() const {
    TArray<FOCObjectiveView> Out;
    if (!Bridge) return Out;
    for (const oc::FObjective& O : Bridge->GetSim().Objectives()) {
        FOCObjectiveView V;
        V.Id = UTF8_TO_TCHAR(O.Id.c_str());
        V.Title = UTF8_TO_TCHAR(O.Title.c_str());
        V.Description = UTF8_TO_TCHAR(O.Description.c_str());
        V.Progress = O.Progress;
        V.Target = O.TargetCount;
        V.bComplete = O.State == oc::EObjectiveState::Complete;
        V.bActive = O.State == oc::EObjectiveState::Active;
        Out.Add(V);
    }
    return Out;
}

TArray<FOCHintView> UOCHUDWidget::GetHints() const {
    TArray<FOCHintView> Out;
    if (!Bridge) return Out;
    for (const oc::FHint& H : Bridge->GetSim().Hints()) {
        FOCHintView V;
        V.Title = UTF8_TO_TCHAR(H.Title.c_str());
        V.Body = UTF8_TO_TCHAR(H.Body.c_str());
        Out.Add(V);
    }
    return Out;
}

TArray<FOCHeatmapCell> UOCHUDWidget::GetHeatmap(int32 Downsample) const {
    TArray<FOCHeatmapCell> Out;
    if (!Bridge) return Out;
    std::vector<float> Heat;
    Bridge->GetSim().FillHeatmap(Heat, Downsample);
    const int32 Step = FMath::Max(1, Downsample);
    const int32 W = (Bridge->GetSim().MapWidth() + Step - 1) / Step;
    const int32 H = (Bridge->GetSim().MapHeight() + Step - 1) / Step;
    for (int32 Y = 0; Y < H; ++Y)
        for (int32 X = 0; X < W; ++X) {
            FOCHeatmapCell C;
            C.X = X * Step; C.Y = Y * Step;
            C.Heat = Heat[static_cast<size_t>(Y * W + X)];
            Out.Add(C);
        }
    return Out;
}

FString UOCHUDWidget::Localize(const FString& Key) const {
    if (!Bridge) return Key;
    return UTF8_TO_TCHAR(Bridge->GetSim().Localize(TCHAR_TO_UTF8(*Key)).c_str());
}

FString UOCHUDWidget::GetMusicState() const {
    if (!Bridge) return TEXT("calm");
    switch (Bridge->GetSim().MusicState()) {
    case oc::EMusicState::Combat: return TEXT("combat");
    case oc::EMusicState::Dire: return TEXT("dire");
    default: return TEXT("calm");
    }
}

FString UOCHUDWidget::GetSeason() const {
    return Bridge ? UTF8_TO_TCHAR(Bridge->GetSim().CurrentSeason().c_str()) : TEXT("spring");
}

bool UOCHUDWidget::DismissHint() { return Bridge ? Bridge->DismissHint() : false; }
