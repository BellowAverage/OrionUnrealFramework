#pragma once

#include "CoreMinimal.h"
#include "OrionWeaponData.generated.h"

UENUM(BlueprintType)
enum class EOrionWeaponType : uint8
{
	Rifle UMETA(DisplayName = "Rifle"),
	Shotgun UMETA(DisplayName = "Shotgun")
};

USTRUCT(BlueprintType)
struct FOrionWeaponParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool IsWeaponLongRange = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float FireRange = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackFrequencyLongRange = 0.231f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float AttackTriggerTimeLongRange = 1.60f;
	
	// Shotgun params
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	int32 PelletsCount = 1;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float SpreadAngle = 1.0f;
};




