// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"
#include "Orion/OrionComponents/OrionStructureComponent.h"
#include "Orion/OrionGameInstance/OrionBuildingManager.h"
#include "OrionStructure.generated.h"


UCLASS()
class ORION_API AOrionStructure : public AActor
{
	GENERATED_BODY()

public:
	AOrionStructure();

	virtual void BeginPlay() override;

	virtual void Tick(float DeltaTime) override;

	/* Basics */

	bool bForceSnapOnGrid = false;

	UPROPERTY()
	UOrionBuildingManager* BuildingManager = nullptr;

	UPROPERTY()
	UOrionStructureComponent* StructureComponent = nullptr;


	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
