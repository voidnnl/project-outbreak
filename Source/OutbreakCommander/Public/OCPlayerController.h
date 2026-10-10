// AOCPlayerController: RTS-style input. Camera pan/rotate/zoom (with edge
// scrolling), build-mode placement, and drag-to-build wall lines forwarded to
// the simulation bridge. Tile<->world conversion assumes 1 tile = 10m and the
// sim origin at the world origin (see AOCGodPawn::TileSizeMeters).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "OCPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UOCSimulationBridge;

UENUM(BlueprintType)
enum class EOCBuildMode : uint8 { None, Building, Wall };

UCLASS()
class OUTBREAKCOMMANDER_API AOCPlayerController : public APlayerController {
    GENERATED_BODY()

public:
    AOCPlayerController();

    virtual void SetupInputComponent() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void BeginPlay() override;

    // --- build mode (driven by UI buttons or hotkeys) ---
    UFUNCTION(BlueprintCallable, Category = "Outbreak|Build")
    void SetBuildMode(EOCBuildMode Mode, const FString& BuildingDefId = TEXT(""));

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Build")
    EOCBuildMode GetBuildMode() const { return BuildMode; }

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Build")
    void CancelBuildMode();

    // Projects a screen position onto the sim ground plane; returns the tile.
    UFUNCTION(BlueprintCallable, Category = "Outbreak|Build")
    bool ScreenToTile(FVector2D ScreenPos, FIntPoint& OutTile) const;

    UPROPERTY(EditAnywhere, Category = "Outbreak|Input") float EdgeScrollSpeed = 60.f;
    UPROPERTY(EditAnywhere, Category = "Outbreak|Input") int32 EdgeScrollMargin = 12;

private:
    void OnPanX(const FInputActionValue& Value);
    void OnPanY(const FInputActionValue& Value);
    void OnZoom(const FInputActionValue& Value);
    void OnRotate(const FInputActionValue& Value);
    void OnBuildConfirmStarted(const FInputActionValue& Value);
    void OnBuildConfirmReleased(const FInputActionValue& Value);
    void OnBuildCancel(const FInputActionValue& Value);
    void OnPause(const FInputActionValue& Value);

    void UpdateEdgeScrolling(float DeltaSeconds);
    void UpdateWallDrag();
    UOCSimulationBridge* GetBridge() const;

    UPROPERTY(EditDefaultsOnly, Category = "Outbreak|Input") TObjectPtr<UInputMappingContext> IMC_Default = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Outbreak|Input") TObjectPtr<UInputAction> IA_PanX = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Outbreak|Input") TObjectPtr<UInputAction> IA_PanY = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Outbreak|Input") TObjectPtr<UInputAction> IA_Zoom = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Outbreak|Input") TObjectPtr<UInputAction> IA_Rotate = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Outbreak|Input") TObjectPtr<UInputAction> IA_BuildConfirm = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Outbreak|Input") TObjectPtr<UInputAction> IA_BuildCancel = nullptr;
    UPROPERTY(EditDefaultsOnly, Category = "Outbreak|Input") TObjectPtr<UInputAction> IA_Pause = nullptr;

    EOCBuildMode BuildMode = EOCBuildMode::None;
    FString PendingBuildingDefId;

    // Wall drag state.
    bool bDraggingWall = false;
    FIntPoint DragStartTile = FIntPoint::ZeroValue;
};
