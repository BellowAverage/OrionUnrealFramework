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

	FVector2D MoveInput;

protected:
	virtual void BeginPlay() override;

private:
	void UpdateTimeDilationCompensation();
	void SimulateInitialRotateKick();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USpringArmComponent* SpringArm = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UCameraComponent* Camera = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UFloatingPawnMovement* Movement = nullptr;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void Zoom(float AxisValue);

	void StartRotate();
	void StopRotate();
	void CameraYaw(float Value);
	void CameraPitch(float Value);

	bool bRotatingCamera = false;

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

	UPROPERTY(EditAnywhere, Category = "Follow")
	float FollowInterpSpeed = 10.f;

	UPROPERTY(EditAnywhere, Category = "Follow")
	FVector FollowOffset = FVector(0.f, 0.f, 0.f);

	AActor* FollowTarget = nullptr;

	bool IsFollowing = false;

	void ToggleFollow();
	void StopFollowIfTargetInvalid();
};
