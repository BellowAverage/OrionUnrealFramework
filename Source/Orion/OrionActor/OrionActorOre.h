// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OrionActor.h"
#include "OrionActorOre.generated.h"

/**
 * 
 */

UENUM(BlueprintType)
enum class EOreCategory : uint8
{
	None UMETA(DisplayName = "None"),
	StoneOre UMETA(DisplayName = "Stone Ore"),
	CopperOre UMETA(DisplayName = "Copper Ore"),
	GoldOre UMETA(DisplayName = "Gold Ore"),
	SilverOre UMETA(DisplayName = "Silver Ore"),
};

UCLASS()
class ORION_API AOrionActorOre : public AOrionActor
{
	GENERATED_BODY()

public:
	AOrionActorOre();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	EOreCategory OreCategory;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	int ProductionTimeCost;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	float ProductionProgress;

	UFUNCTION(BlueprintCallable, Category = "Basics")
	void ProductionProgressUpdate(float DeltaTime);
};
