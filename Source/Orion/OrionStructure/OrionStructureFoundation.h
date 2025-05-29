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
	//virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;
};
