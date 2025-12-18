#include "OrionCameraPawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Orion/OrionPlayerController/OrionPlayerController.h"
#include "TimerManager.h"
#include "Orion/OrionChara/OrionChara.h"

void AOrionCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	check(PlayerInputComponent);

	PlayerInputComponent->BindAxis(TEXT("CameraMoveForward"), this, &AOrionCameraPawn::MoveForward);
	PlayerInputComponent->BindAxis(TEXT("CameraMoveRight"), this, &AOrionCameraPawn::MoveRight);
	PlayerInputComponent->BindAxis(TEXT("CameraZoom"), this, &AOrionCameraPawn::Zoom);

	PlayerInputComponent->BindAxis(TEXT("CameraTurn"), this, &AOrionCameraPawn::CameraYaw);
	PlayerInputComponent->BindAxis(TEXT("CameraLookUp"), this, &AOrionCameraPawn::CameraPitch);

	PlayerInputComponent->BindAction(TEXT("CameraRotate"), IE_Pressed, this, &AOrionCameraPawn::StartRotate);
	PlayerInputComponent->BindAction(TEXT("CameraRotate"), IE_Released, this, &AOrionCameraPawn::StopRotate);

	PlayerInputComponent->BindAction("CameraToggleFollow", IE_Pressed,
	                                 this, &AOrionCameraPawn::ToggleFollow);
}

AOrionCameraPawn::AOrionCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	PrimaryActorTick.bTickEvenWhenPaused = true;

	// 1) Root component
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// 2) Spring arm
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(SceneRoot);
	SpringArm->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	SpringArm->bDoCollisionTest = false;
	SpringArm->TargetArmLength = TargetArmLength;

	// 3) Camera
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);

	// 4) Floating movement component
	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingPawnMovement"));
	Movement->Acceleration = 8000.f;
	Movement->Deceleration = 8000.f;
	Movement->MaxSpeed = 3000.f;

	//bAddDefaultMovementBindings = false;   // We manually bind WASD
}

void AOrionCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;

		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);

		if (UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport())
		{
			ViewportClient->SetMouseCaptureMode(EMouseCaptureMode::CapturePermanently);
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &AOrionCameraPawn::SimulateInitialRotateKick);
	}
}

void AOrionCameraPawn::SimulateInitialRotateKick()
{
	// Trigger middle button press/release logic once to ensure initial mouse state matches manual drag
	StartRotate();
	StopRotate();
}

void AOrionCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateTimeDilationCompensation();

	// Smoothly interpolate to target length
	const float Current = SpringArm->TargetArmLength;
	const float NewLength = FMath::FInterpTo(Current, TargetArmLength, DeltaSeconds, ZoomInterpSpeed);
	SpringArm->TargetArmLength = NewLength;


	// 1. If target is invalid, automatically unlock
	StopFollowIfTargetInvalid();

	// 2. Execute position following
	if (IsFollowing && FollowTarget)
	{
		const FVector Desired =
			FollowTarget->GetActorLocation() + FollowOffset;

		SetActorLocation(
			FMath::VInterpTo(GetActorLocation(),
			                 Desired,
			                 DeltaSeconds,
			                 FollowInterpSpeed));
	}
}

void AOrionCameraPawn::UpdateTimeDilationCompensation()
{
	if (UWorld* World = GetWorld())
	{
		const float GlobalDil = World->GetWorldSettings()->GetEffectiveTimeDilation();
		const float Wanted = (GlobalDil > KINDA_SMALL_NUMBER) ? 1.f / GlobalDil : 1.f;

		if (!FMath::IsNearlyEqual(CustomTimeDilation, Wanted))
		{
			CustomTimeDilation = Wanted; // Compensate for global Time Dilation
		}
	}
}

/* ------------ Movement ------------ */
void AOrionCameraPawn::MoveForward(float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AOrionCameraPawn::MoveRight(float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		AddMovementInput(GetActorRightVector(), Value);
	}
}

/* ------------ Zoom ------------ */
void AOrionCameraPawn::Zoom(float AxisValue)
{
	if (!FMath::IsNearlyZero(AxisValue))
	{
		TargetArmLength = FMath::Clamp(TargetArmLength - AxisValue * ZoomStep,
		                               MinArmLength, MaxArmLength);
	}
}

/* ------------ Rotation ------------ */
void AOrionCameraPawn::StartRotate()
{
	bRotatingCamera = true;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;

		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);

		if (UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport())
		{
			ViewportClient->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown);
		}
	}
}

void AOrionCameraPawn::StopRotate()
{
	bRotatingCamera = false;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;

		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(InputMode);

		if (UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport())
		{
			ViewportClient->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown);
		}
	}
}

void AOrionCameraPawn::CameraYaw(float Value)
{
	if (bRotatingCamera && !FMath::IsNearlyZero(Value))
	{
		AddActorWorldRotation(FRotator(0.f, Value * RotateSpeed, 0.f));
	}
}

void AOrionCameraPawn::CameraPitch(float Value)
{
	Value = -Value;

	if (bRotatingCamera && !FMath::IsNearlyZero(Value))
	{
		const FRotator CurRel = SpringArm->GetRelativeRotation();
		const float NewPitch = FMath::Clamp(CurRel.Pitch + Value * RotateSpeed, -89.f, -10.f);
		SpringArm->SetRelativeRotation(FRotator(NewPitch, CurRel.Yaw, CurRel.Roll));
	}
}

void AOrionCameraPawn::ToggleFollow()
{
	if (IsFollowing) // Already locked → unlock
	{
		IsFollowing = false;
		FollowTarget = nullptr;
		return;
	}

	// Not locked yet: check if there is only 1 selected character
	if (AOrionPlayerController* PC = Cast<AOrionPlayerController>(GetController()))
	{
		if (PC->OrionCharaSelection.Num() == 1)
		{
			FollowTarget = Cast<AActor>(PC->OrionCharaSelection[0]);
			if (IsValid(FollowTarget))
			{
				IsFollowing = true;
			}
		}
		/*else if (PC->OrionPawnSelection.Num() == 1)
		{
			FollowTarget = PC->OrionPawnSelection[0];
			if (IsValid(FollowTarget))
			{
				IsFollowing = true;
			}
		}*/
	}
}

void AOrionCameraPawn::StopFollowIfTargetInvalid()
{
	if (IsFollowing && (!FollowTarget))
	{
		IsFollowing = false;
		FollowTarget = nullptr;
	}
}
