#pragma once

#include<CoreMinimal.h>
#include<EOrionAction.generated.h>


UENUM(BlueprintType)
enum class EOrionAction : uint8
{
	AttackOnChara UMETA(DisplayName = "Attack On Character"),
	InteractWithActor UMETA(DisplayName = "Interact With Actor"),
	InteractWithProduction UMETA(DisplayName = "Interact With Production"),
	InteractWithStorage UMETA(DisplayName = "Interact With Storage"),
	MoveToLocation UMETA(DisplayName = "Move To Location"),
	CollectCargo UMETA(DisplayName = "Collect Cargo"),
	CollectBullets UMETA(DisplayName = "Collect Bullets"),
	Undefined UMETA(DisplayName = "Undefined"),
};

USTRUCT()
struct FOrionActionParams
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	EOrionAction OrionActionType = EOrionAction::Undefined;

	UPROPERTY(SaveGame)
	FVector TargetLocation = FVector::ZeroVector;
	UPROPERTY(SaveGame)
	FGuid TargetActorId;
	UPROPERTY(SaveGame)
	FVector HitOffset = FVector::ZeroVector;
	UPROPERTY(SaveGame)
	int32 Quantity = 0;


	friend bool operator==(const FOrionActionParams& A,
	                       const FOrionActionParams& B)
	{
		return A.OrionActionType == B.OrionActionType &&
			A.TargetLocation.Equals(B.TargetLocation, 1e-4f) &&
			A.TargetActorId == B.TargetActorId &&
			A.HitOffset.Equals(B.HitOffset, 1e-4f) &&
			A.Quantity == B.Quantity;
	}

	friend bool operator!=(const FOrionActionParams& A,
	                       const FOrionActionParams& B)
	{
		return !(A == B);
	}
};

USTRUCT(BlueprintType)
struct FOrionCharaSerializable
{
	GENERATED_BODY()
	/* Basics */

	UPROPERTY(SaveGame)
	FGuid CharaGameId;

	UPROPERTY(SaveGame)
	FVector CharaLocation;

	UPROPERTY(SaveGame)
	FRotator CharaRotation;

	/* Procedural Actions */
	UPROPERTY(SaveGame)
	TArray<FOrionActionParams> SerializedProcActions;

	UPROPERTY(SaveGame)
	TArray<uint8> SerializedBytes;
};
