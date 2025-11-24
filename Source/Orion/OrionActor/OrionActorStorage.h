// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OrionActor.h"
#include "OrionActorStorage.generated.h"

UENUM(BlueprintType)
enum class EStorageCategory : uint8
{
	None UMETA(DisplayName = "None"),
	WoodStorage UMETA(DisplayName = "Wood Storage"),
	StoneStorage UMETA(DisplayName = "Stone Storage"),
	CopperStorage UMETA(DisplayName = "Copper Storage"),
	GoldStorage UMETA(DisplayName = "Gold Storage"),
	SilverStorage UMETA(DisplayName = "Silver Storage"),
};


/**
 * 
 */
UCLASS()
class ORION_API AOrionActorStorage : public AOrionActor
{
	GENERATED_BODY()

public:
	AOrionActorStorage();

	virtual TArray<FString> TickShowHoveringInfo() override;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category = "Basics")
	EStorageCategory StorageCategory = EStorageCategory::None;
};
