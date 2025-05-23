#pragma once
#include "CoreMinimal.h"
#include "Orion/OrionGlobals/EOrionStructure.h"
#include "OrionDataBuilding.generated.h"

USTRUCT(BlueprintType)
struct FOrionDataBuilding
{
	GENERATED_BODY()

	int32 BuildingId; // Building ID

	FName BuildingDisplayName;

	FString BuildingImageReference;

	FString BuildingBlueprintReference;

	EOrionStructure BuildingPlacingRule = EOrionStructure::None;
};
