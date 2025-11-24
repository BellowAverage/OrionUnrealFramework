// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OrionActor.h"
#include "OrionActorProduction.generated.h"

/**
 * 
 */

UENUM(BlueprintType)
enum class EProductionCategory : uint8
{
	None UMETA(DisplayName = "None"),
	Bullets UMETA(DisplayName = "Bullets"),
};

UCLASS()
class ORION_API AOrionActorProduction : public AOrionActor
{
	GENERATED_BODY()

public:
	AOrionActorProduction();

	virtual TArray<FString> TickShowHoveringInfo() override;

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Config (Non-null)")
	EProductionCategory ProductionCategory = EProductionCategory::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Config (Non-null)")
	int32 ProductionTimeCost = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Config (Non-null)")
	float ProductionProgress = 0.f;

	UFUNCTION(BlueprintCallable)
	void ProductionProgressUpdate(float DeltaTime);
};
