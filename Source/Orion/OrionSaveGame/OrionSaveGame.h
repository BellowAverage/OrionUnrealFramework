// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "OrionSaveGame.generated.h"


USTRUCT()
struct FOrionStructureRecord
{
	GENERATED_BODY()

	/** Blueprint or C++ class full path (example: /Game/BP/BP_OrionFoundation.BP_OrionFoundation_C) */
	UPROPERTY() FString ClassPath;

	UPROPERTY() FTransform Transform;

	FOrionStructureRecord() = default;
	FOrionStructureRecord(const FString& InPath, const FTransform& InTM)
		: ClassPath(InPath), Transform(InTM) {
	}
};



UCLASS()
class ORION_API UOrionSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() TArray<FOrionStructureRecord> SavedStructures;
	
};
