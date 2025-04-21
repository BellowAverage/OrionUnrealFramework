#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "OrionCameraPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UFloatingPawnMovement;

UCLASS()
class ORION_API AOrionCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	AOrionCameraPawn();

	virtual void Tick(float DeltaSeconds) override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	virtual void BeginPlay() override;

private:
	/* ========= 组件（Component 指针不能互相赋值）========== */
	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USpringArmComponent* SpringArm = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UCameraComponent* Camera = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UFloatingPawnMovement* Movement = nullptr;

	/* ========= 输入响应 ========= */
	void MoveForward(float Value);
	void MoveRight(float Value);
	void Zoom(float AxisValue);

	void StartRotate();       // 中键按下
	void StopRotate();        // 中键松开
	void CameraYaw(float Value);
	void CameraPitch(float Value);

	/* ========= 运行时状态 ========= */
	bool bRotatingCamera = false;

	/* ========= 缩放/旋转参数 ========= */
	UPROPERTY(EditAnywhere, Category = "Basics")
	float MinArmLength = -6000.f;

	UPROPERTY(EditAnywhere, Category = "Basics")
	float MaxArmLength = 5000.f;

	UPROPERTY(EditAnywhere, Category = "Basics")
	float ZoomStep = 250.f;

	UPROPERTY(EditAnywhere, Category = "Basics")
	float ZoomInterpSpeed = 10.f;

	UPROPERTY(EditAnywhere, Category = "Basics")
	float TargetArmLength = 500.f;

	UPROPERTY(EditAnywhere, Category = "Basics")
	float RotateSpeed = 1.0f;

	/* ========= 锁定镜头 ========= */
	UPROPERTY(EditAnywhere, Category = "Follow")
	float FollowInterpSpeed = 10.f;

	UPROPERTY(EditAnywhere, Category = "Follow")
	FVector FollowOffset = FVector(0.f, 0.f, 0.f);

	UPROPERTY()
	AActor* FollowTarget = nullptr;

	bool bIsFollowing = false;

	void ToggleFollow();
	void StopFollowIfTargetInvalid();
};
