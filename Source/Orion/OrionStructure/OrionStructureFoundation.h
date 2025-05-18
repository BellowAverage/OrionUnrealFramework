// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OrionStructure.h"
#include "OrionStructureFoundation.generated.h"

/**
 * 
 */


UCLASS()
class ORION_API AOrionStructureFoundation : public AOrionStructure
{
	GENERATED_BODY()

public:
	AOrionStructureFoundation();

	virtual void BeginPlay() override;

	/* Basics */

	bool bForceSnapOnGrid = false;

	/* Sockets */
	TArray<FFoundationSocket> FoundationSockets;

	TArray<FWallSocket> WallSockets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	TSubclassOf<AOrionStructureFoundation> BlueprintFoundationInstance;

	UFUNCTION(BlueprintCallable, Category = "Basics")
	static EOrionStructure GetOrionStructureCategory()
	{
		return EOrionStructure::Foundation;
	}
};
