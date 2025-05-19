// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Orion/OrionStructure/OrionStructure.h"
#include "OrionStructureWall.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class ORION_API AOrionStructureWall : public AOrionStructure
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	/* Basics */

	bool bForceSnapOnGrid = true;

	/* Sockets */

	TArray<FWallSocket> WallSockets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	TSubclassOf<AOrionStructureWall> BlueprintWallInstance;

	UFUNCTION(BlueprintCallable, Category = "Basics")
	static EOrionStructure GetOrionStructureCategory()
	{
		return EOrionStructure::Wall;
	}
};
