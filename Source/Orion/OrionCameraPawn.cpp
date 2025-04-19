#include "OrionCameraPawn.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"

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
}

AOrionCameraPawn::AOrionCameraPawn()
{
	PrimaryActorTick.bCanEverTick = true;

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
	}
}

void AOrionCameraPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 平滑插值到目标长度
	const float Current = SpringArm->TargetArmLength;
	const float NewLength = FMath::FInterpTo(Current, TargetArmLength, DeltaSeconds, ZoomInterpSpeed);
	SpringArm->TargetArmLength = NewLength;
}

/* ------------ 移动 ------------ */
void AOrionCameraPawn::MoveForward(float Value)
{
	UE_LOG(LogTemp, Warning, TEXT("MoveForward: %f"), Value);

	if (!FMath::IsNearlyZero(Value))
	{
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void AOrionCameraPawn::MoveRight(float Value)
{

	UE_LOG(LogTemp, Warning, TEXT("MoveRight: %f"), Value);

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
