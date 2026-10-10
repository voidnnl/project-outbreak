#include "OCGodPawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

AOCGodPawn::AOCGodPawn() {
    PrimaryActorTick.bCanEverTick = true;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // God view: camera looks down at ~45 degrees, spring arm length = zoom.
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(Root);
    SpringArm->TargetArmLength = TargetArmLength;
    SpringArm->SetRelativeRotation(FRotator(-55.f, 0.f, 0.f));
    SpringArm->bDoCollisionTest = false;
    SpringArm->bEnableCameraLag = true;
    SpringArm->CameraLagSpeed = 12.f;
    SpringArm->bUsePawnControlRotation = false;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    Camera->bUsePawnControlRotation = false;
}

void AOCGodPawn::Tick(float DeltaSeconds) {
    Super::Tick(DeltaSeconds);

    // Smooth zoom toward the target arm length.
    const float NewLength = FMath::Clamp(
        SpringArm->TargetArmLength - ZoomInput * ZoomSpeed * DeltaSeconds,
        MinArmLength, MaxArmLength);
    TargetArmLength = NewLength;
    SpringArm->TargetArmLength = NewLength;
    ZoomInput = 0.f;

    // Smooth yaw rotation.
    if (!FMath::IsNearlyZero(RotateInput)) {
        FRotator R = SpringArm->GetRelativeRotation();
        R.Yaw = FMath::Fmod(R.Yaw + RotateInput * RotationSpeed * DeltaSeconds, 360.f);
        SpringArm->SetRelativeRotation(R);
        RotateInput = 0.f;
    }

    // Pan in world space, scaled by zoom so panning feels consistent at every
    // distance (brief section 3: smooth pan at every zoom level).
    if (!PanInput.IsNearlyZero()) {
        const float ZoomFactor = SpringArm->TargetArmLength / 6000.f;
        const FRotator YawRot(0.f, SpringArm->GetRelativeRotation().Yaw, 0.f);
        const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
        const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
        const FVector Delta = (Forward * PanInput.Y + Right * PanInput.X) *
                               PanSpeed * ZoomFactor * TileSizeMeters * DeltaSeconds;
        AddActorWorldOffset(Delta, false);
        PanInput = FVector2D::ZeroVector;
    }
}

void AOCGodPawn::Zoom(float Axis) { ZoomInput += Axis; }

void AOCGodPawn::Rotate(float Axis) { RotateInput += Axis; }

void AOCGodPawn::Pan(FVector2D Axis) { PanInput += Axis; }

void AOCGodPawn::SetTargetArmLength(float Length) {
    TargetArmLength = FMath::Clamp(Length, MinArmLength, MaxArmLength);
}
