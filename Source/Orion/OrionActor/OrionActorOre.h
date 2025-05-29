﻿// Fill out your copyright notice in the Description page of Project Settings.

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

	/* --- 配置 --- */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Basics")
	EOreCategory OreCategory = EOreCategory::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Basics")
	int32 ProductionTimeCost = 20; // 秒

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Basics")
	float ProductionProgress = 0.f; // 0-100

	/* --- 逻辑 --- */
	UFUNCTION(BlueprintCallable, Category = "Basics")
	void ProductionProgressUpdate(float DeltaTime);
};
