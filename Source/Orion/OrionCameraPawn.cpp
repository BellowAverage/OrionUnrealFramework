#include "OrionCameraPawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "OrionPlayerController.h"

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

	// 1) 根组件
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// 2) 伸缩臂
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(SceneRoot);
	SpringArm->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	SpringArm->bDoCollisionTest = false;
	SpringArm->TargetArmLength = TargetArmLength;

	// 3) 相机
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);

	// 4) 浮动移动组件
	Movement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingPawnMovement"));
	Movement->Acceleration = 8000.f;
	Movement->Deceleration = 8000.f;
	Movement->MaxSpeed = 3000.f;

	//bAddDefaultMovementBindings = false;   // 我们手动绑定 WASD
}

void AOrionCameraPawn::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;
		PC->bEnableClickEvents = true;
		PC->bEnableMouseOverEvents = true;


		// --- 新增：设置 InputMode 为 GameAndUI，同时不锁定鼠标到视口 ---
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);

		// --- 新增：禁用 Viewport 的自动捕获（项目默认是 CaptureDuringMouseDown） ---
		//if (GetWorld()->GetGameViewport())
		//{
		//	GetWorld()->GetGameViewport()->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
		//}
	}
}

void AOrionCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateTimeDilationCompensation();

	// 平滑插值到目标长度
	const float Current = SpringArm->TargetArmLength;
	const float NewLength = FMath::FInterpTo(Current, TargetArmLength, DeltaSeconds, ZoomInterpSpeed);
	SpringArm->TargetArmLength = NewLength;


	// 1. 若目标失效则自动脱锁
	StopFollowIfTargetInvalid();

	// 2. 执行位置跟随
	if (bIsFollowing && FollowTarget)
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
			CustomTimeDilation = Wanted; // 抵消全局 Time Dilation
		}
	}
}

/* ------------ 移动 ------------ */
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

/* ------------ 缩放 ------------ */
void AOrionCameraPawn::Zoom(float AxisValue)
{
	if (!FMath::IsNearlyZero(AxisValue))
	{
		TargetArmLength = FMath::Clamp(TargetArmLength - AxisValue * ZoomStep,
		                               MinArmLength, MaxArmLength);
	}
}

/* ------------ 旋转 ------------ */
void AOrionCameraPawn::StartRotate()
{
	bRotatingCamera = true;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void AOrionCameraPawn::StopRotate()
{
	bRotatingCamera = false;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;
		PC->SetInputMode(FInputModeGameAndUI());
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
	if (bIsFollowing) // 已锁定 → 解除
	{
		bIsFollowing = false;
		FollowTarget = nullptr;
		return;
	}

	// 尚未锁定：检查当前是否只有 1 个选中的角色
	if (AOrionPlayerController* PC = Cast<AOrionPlayerController>(GetController()))
	{
		if (PC->OrionCharaSelection.size() == 1)
		{
			FollowTarget = PC->OrionCharaSelection[0];
			if (IsValid(FollowTarget))
			{
				bIsFollowing = true;
			}
		}
		else if (PC->OrionPawnSelection.size() == 1)
		{
			FollowTarget = PC->OrionPawnSelection[0];
			if (IsValid(FollowTarget))
			{
				bIsFollowing = true;
			}
		}
	}
}

void AOrionCameraPawn::StopFollowIfTargetInvalid()
{
	if (bIsFollowing && (!FollowTarget))
	{
		bIsFollowing = false;
		FollowTarget = nullptr;
	}
}
