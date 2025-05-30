﻿#pragma once
#include "CoreMinimal.h"
#include "OrionDataItem.generated.h"

USTRUCT(BlueprintType)
struct FOrionDataItem
{
	GENERATED_BODY()

	/** Unique ID */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	int32 ItemId = 0;

	/** Internal name */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FName Name;

	/** English display name */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	/** Chinese display name */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText ChineseDisplayName;

	/** Standard price */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	float PriceSTD = 0.f;

	/** Standard production/processing time (seconds) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	float ProductionTimeCostSTD = 0.f;
};
