#include "OCPlayerController.h"
#include "OCGameMode.h"
#include "OCGodPawn.h"
#include "OCSimulationBridge.h"
#include "oc/Simulation.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerState.h"

AOCPlayerController::AOCPlayerController() {
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    DefaultMouseCursor = EMouseCursor::Crosshairs;
}

void AOCPlayerController::BeginPlay() {
    Super::BeginPlay();
    if (ULocalPlayer* LP = GetLocalPlayer()) {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>()) {
            if (IMC_Default) Subsystem->AddMappingContext(IMC_Default, 0);
            if (IMC_Gamepad) Subsystem->AddMappingContext(IMC_Gamepad, 1);
        }
    }
}

void AOCPlayerController::SetupInputComponent() {
    Super::SetupInputComponent();
    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent)) {
        if (IA_PanX) EIC->BindAction(IA_PanX, ETriggerEvent::Triggered, this, &AOCPlayerController::OnPanX);
        if (IA_PanY) EIC->BindAction(IA_PanY, ETriggerEvent::Triggered, this, &AOCPlayerController::OnPanY);
        if (IA_Zoom) EIC->BindAction(IA_Zoom, ETriggerEvent::Triggered, this, &AOCPlayerController::OnZoom);
        if (IA_Rotate) EIC->BindAction(IA_Rotate, ETriggerEvent::Triggered, this, &AOCPlayerController::OnRotate);
        if (IA_BuildConfirm) {
            EIC->BindAction(IA_BuildConfirm, ETriggerEvent::Started, this, &AOCPlayerController::OnBuildConfirmStarted);
            EIC->BindAction(IA_BuildConfirm, ETriggerEvent::Completed, this, &AOCPlayerController::OnBuildConfirmReleased);
        }
        if (IA_BuildCancel) EIC->BindAction(IA_BuildCancel, ETriggerEvent::Started, this, &AOCPlayerController::OnBuildCancel);
        if (IA_Pause) EIC->BindAction(IA_Pause, ETriggerEvent::Started, this, &AOCPlayerController::OnPause);
    }
}

void AOCPlayerController::SnapCameraPreset(int32 PresetIndex) {
    if (APawn* P = GetPawn()) if (AOCGodPawn* God = Cast<AOCGodPawn>(P)) God->SnapToPreset(PresetIndex);
}

void AOCPlayerController::CycleSpeed() {
    static const float Speeds[] = {1.f, 2.f, 4.f};
    SpeedIndex = (SpeedIndex + 1) % 3;
    SimSpeed = Speeds[SpeedIndex];
}

bool AOCPlayerController::GetWallDragPreview(FIntPoint& OutStart, FIntPoint& OutEnd) const {
    if (!bDraggingWall) return false;
    OutStart = DragStartTile;
    OutEnd = DragEndTile;
    return true;
}

void AOCPlayerController::SelectAtScreen(FVector2D ScreenPos) {
    FIntPoint Tile;
    if (!ScreenToTile(ScreenPos, Tile)) return;
    SelectedSquadId = -1;
    SelectedBuildingId = -1;
    if (UOCSimulationBridge* Bridge = GetBridge()) {
        const oc::FSimulation& Sim = Bridge->GetSim();
        for (const oc::FSquad& S : Sim.Squads()) {
            if (S.bWiped) continue;
            if (FMath::Abs(S.X - Tile.X) < 1.5f && FMath::Abs(S.Y - Tile.Y) < 1.5f) {
                SelectedSquadId = S.Id;
                return;
            }
        }
        for (const oc::FBuilding& B : Sim.Buildings()) {
            if (B.bDemolished) continue;
            if (Tile.X >= B.X && Tile.X < B.X + B.W && Tile.Y >= B.Y && Tile.Y < B.Y + B.H) {
                SelectedBuildingId = B.Id;
                return;
            }
        }
    }
}

void AOCPlayerController::Tick(float DeltaSeconds) {
    Super::Tick(DeltaSeconds);
    UpdateEdgeScrolling(DeltaSeconds);
    UpdateWallDrag();
}

UOCSimulationBridge* AOCPlayerController::GetBridge() const {
    if (const AOCGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<AOCGameMode>() : nullptr)
        return GM->GetBridge();
    return nullptr;
}

// --- camera input ---

void AOCPlayerController::OnPanX(const FInputActionValue& Value) {
    if (APawn* P = GetPawn()) if (AOCGodPawn* God = Cast<AOCGodPawn>(P)) God->Pan(FVector2D(Value.Get<float>(), 0.f));
}
void AOCPlayerController::OnPanY(const FInputActionValue& Value) {
    if (APawn* P = GetPawn()) if (AOCGodPawn* God = Cast<AOCGodPawn>(P)) God->Pan(FVector2D(0.f, Value.Get<float>()));
}
void AOCPlayerController::OnZoom(const FInputActionValue& Value) {
    if (APawn* P = GetPawn()) if (AOCGodPawn* God = Cast<AOCGodPawn>(P)) God->Zoom(Value.Get<float>());
}
void AOCPlayerController::OnRotate(const FInputActionValue& Value) {
    if (APawn* P = GetPawn()) if (AOCGodPawn* God = Cast<AOCGodPawn>(P)) God->Rotate(Value.Get<float>());
}

void AOCPlayerController::OnPause(const FInputActionValue&) {
    SetPause(!IsPaused());
}

// --- edge scrolling (brief section 3) ---

void AOCPlayerController::UpdateEdgeScrolling(float DeltaSeconds) {
    if (IsPaused()) return;
    FVector2D MousePos;
    if (!GetMousePosition(MousePos.X, MousePos.Y)) return;
    const FVector2D ViewportSize = FVector2D(GEngine->GameViewport->Viewport->GetSizeXY());
    if (ViewportSize.X <= 0.f) return;

    FVector2D Dir = FVector2D::ZeroVector;
    if (MousePos.X <= EdgeScrollMargin) Dir.X = -1.f;
    else if (MousePos.X >= ViewportSize.X - EdgeScrollMargin) Dir.X = 1.f;
    if (MousePos.Y <= EdgeScrollMargin) Dir.Y = -1.f;
    else if (MousePos.Y >= ViewportSize.Y - EdgeScrollMargin) Dir.Y = 1.f;

    if (!Dir.IsNearlyZero()) {
        if (APawn* P = GetPawn()) if (AOCGodPawn* God = Cast<AOCGodPawn>(P))
            God->Pan(Dir * EdgeScrollSpeed * DeltaSeconds);
    }
}

// --- build mode / wall drag ---

void AOCPlayerController::SetBuildMode(EOCBuildMode Mode, const FString& BuildingDefId) {
    BuildMode = Mode;
    PendingBuildingDefId = BuildingDefId;
    bDraggingWall = false;
}

void AOCPlayerController::CancelBuildMode() {
    BuildMode = EOCBuildMode::None;
    bDraggingWall = false;
}

bool AOCPlayerController::ScreenToTile(FVector2D ScreenPos, FIntPoint& OutTile) const {
    FVector WorldPos, WorldDir;
    if (!DeprojectScreenPositionToWorld(ScreenPos.X, ScreenPos.Y, WorldPos, WorldDir)) return false;
    // Intersect the camera ray with the ground plane z = 0.
    if (FMath::IsNearlyZero(WorldDir.Z)) return false;
    const float T = -WorldPos.Z / WorldDir.Z;
    if (T < 0.f) return false;
    const FVector Hit = WorldPos + WorldDir * T;
    constexpr float TileMeters = 10.f;  // keep in sync with AOCGodPawn::TileSizeMeters
    OutTile = FIntPoint(FMath::FloorToInt(Hit.X / TileMeters), FMath::FloorToInt(Hit.Y / TileMeters));
    return true;
}

void AOCPlayerController::OnBuildConfirmStarted(const FInputActionValue&) {
    if (BuildMode == EOCBuildMode::None) return;
    FVector2D MousePos;
    if (!GetMousePosition(MousePos.X, MousePos.Y)) return;
    FIntPoint Tile;
    if (!ScreenToTile(MousePos, Tile)) return;

    if (BuildMode == EOCBuildMode::Wall) {
        bDraggingWall = true;
        DragStartTile = Tile;
    } else if (BuildMode == EOCBuildMode::Building) {
        if (UOCSimulationBridge* Bridge = GetBridge())
            Bridge->PlaceBuilding(PendingBuildingDefId, Tile.X, Tile.Y);
    }
}

void AOCPlayerController::OnBuildConfirmReleased(const FInputActionValue&) {
    if (!bDraggingWall) return;
    bDraggingWall = false;
    FVector2D MousePos;
    if (!GetMousePosition(MousePos.X, MousePos.Y)) return;
    FIntPoint Tile;
    if (!ScreenToTile(MousePos, Tile)) return;
    if (UOCSimulationBridge* Bridge = GetBridge())
        Bridge->BuildWallLine(DragStartTile.X, DragStartTile.Y, Tile.X, Tile.Y);
}

void AOCPlayerController::UpdateWallDrag() {
    if (!bDraggingWall) return;
    FVector2D MousePos;
    if (!GetMousePosition(MousePos.X, MousePos.Y)) return;
    ScreenToTile(MousePos, DragEndTile);
}

void AOCPlayerController::OnBuildCancel(const FInputActionValue&) {
    CancelBuildMode();
}
