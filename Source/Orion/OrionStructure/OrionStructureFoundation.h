// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "OrionStructure.h"
#include "OrionStructureFoundation.generated.h"

/**
 * 
 */

UENUM(Blueprintable)
enum class EOrionFoundationType : uint8
{
	BasicSquare,
	BasicTriangle,
};


UCLASS()
class ORION_API AOrionStructureFoundation : public AOrionStructure
{
	GENERATED_BODY()

public:
	AOrionStructureFoundation();
	//virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Basics")
	EOrionFoundationType FoundationType = EOrionFoundationType::BasicSquare;
};
