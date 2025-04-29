// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../OrionActor.h"
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	EProductionCategory ProductionCategory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	int ProductionTimeCost;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	float ProductionProgress;

	UFUNCTION(BlueprintCallable, Category = "Basics")
	void ProductionProgressUpdate(float DeltaTime);
};
