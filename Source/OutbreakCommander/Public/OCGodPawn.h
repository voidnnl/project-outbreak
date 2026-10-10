// AOCGodPawn: the god-view camera. High-angle spring-arm camera with smooth
// zoom (district level -> street level), yaw rotation, and panning handled by
// the player controller. Visual readability at every zoom level is a core
// requirement (brief section 3), so zoom clamps are exposed for tuning.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "OCGodPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;

UCLASS()
class OUTBREAKCOMMANDER_API AOCGodPawn : public APawn {
    GENERATED_BODY()

public:
    AOCGodPawn();

    virtual void Tick(float DeltaSeconds) override;

    // Axis in [-1, 1]; positive zooms out. Zoom is smoothed in Tick.
    UFUNCTION(BlueprintCallable, Category = "Outbreak|Camera")
    void Zoom(float Axis);

    // Axis in [-1, 1]; positive rotates clockwise (yaw).
    UFUNCTION(BlueprintCallable, Category = "Outbreak|Camera")
    void Rotate(float Axis);

    // Pans the camera in world X/Y (tiles). Called by the controller (keys or
    // edge scrolling) and by drag-panning with the middle mouse button.
    UFUNCTION(BlueprintCallable, Category = "Outbreak|Camera")
    void Pan(FVector2D Axis);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Camera")
    void SetTargetArmLength(float Length);

    // Camera presets 1-4 (district overview / medical / farms / walls).
    UFUNCTION(BlueprintCallable, Category = "Outbreak|Camera")
    void SnapToPreset(int32 PresetIndex);

    UFUNCTION(BlueprintCallable, Category = "Outbreak|Camera")
    void FocusOnTile(int32 X, int32 Y, float ArmLength = 4000.f);

    UPROPERTY(EditAnywhere, Category = "Outbreak|Camera") float MinArmLength = 800.f;
    UPROPERTY(EditAnywhere, Category = "Outbreak|Camera") float MaxArmLength = 24000.f;
    UPROPERTY(EditAnywhere, Category = "Outbreak|Camera") float ZoomSpeed = 2400.f;
    UPROPERTY(EditAnywhere, Category = "Outbreak|Camera") float PanSpeed = 45.f;
    UPROPERTY(EditAnywhere, Category = "Outbreak|Camera") float RotationSpeed = 90.f;
    // One tile is 10m in the world; the sim grid maps 1:1 onto tiles.
    UPROPERTY(EditAnywhere, Category = "Outbreak|Camera") float TileSizeMeters = 10.f;

private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> SpringArm;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;

    float TargetArmLength = 6000.f;
    float ZoomInput = 0.f;
    float RotateInput = 0.f;
    FVector2D PanInput = FVector2D::ZeroVector;
};
