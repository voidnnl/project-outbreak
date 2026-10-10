#include "OCPlayerController.h"
#include "OCGameMode.h"
#include "OCGodPawn.h"
#include "OCSimulationBridge.h"
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
    // A HUD widget can poll the drag endpoints to draw the wall preview ghost.
    // (Preview rendering is a content task; the drag state lives here.)
}

void AOCPlayerController::OnBuildCancel(const FInputActionValue&) {
    CancelBuildMode();
}
