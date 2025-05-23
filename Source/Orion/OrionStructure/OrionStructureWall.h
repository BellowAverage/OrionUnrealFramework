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
	AOrionStructureWall();

	virtual void BeginPlay() override;
};
