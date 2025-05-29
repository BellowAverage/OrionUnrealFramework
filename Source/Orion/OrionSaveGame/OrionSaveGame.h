// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Orion/OrionActor/OrionActor.h"
#include "Orion/OrionGlobals/EOrionAction.h"
#include "OrionSaveGame.generated.h"

USTRUCT()
struct FOrionStructureRecord
{
	GENERATED_BODY()

	/** Blueprint or C++ class full path (example: /Game/BP/BP_OrionFoundation.BP_OrionFoundation_C) */
	UPROPERTY(SaveGame)
	FString ClassPath;

	UPROPERTY(SaveGame)
	FTransform Transform;

	FOrionStructureRecord() = default;

	FOrionStructureRecord(const FString& InPath, const FTransform& InTM)
		: ClassPath(InPath), Transform(InTM)
	{
	}
};

UCLASS()
class ORION_API UOrionSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame)
	TArray<FOrionStructureRecord> SavedStructures;

	UPROPERTY(SaveGame)
	TArray<FOrionCharaSerializable> SavedCharacters;

	UPROPERTY(SaveGame)
	TArray<FOrionActorFullRecord> SavedActors;

	UPROPERTY(SaveGame)
	TArray<FOrionInventorySerializable> SavedInventories;
};
