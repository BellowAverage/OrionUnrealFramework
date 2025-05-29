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

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Basics")
	EProductionCategory ProductionCategory = EProductionCategory::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Basics")
	int32 ProductionTimeCost = 15;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Basics")
	float ProductionProgress = 0.f;

	UFUNCTION(BlueprintCallable, Category = "Basics")
	void ProductionProgressUpdate(float DeltaTime);
};
