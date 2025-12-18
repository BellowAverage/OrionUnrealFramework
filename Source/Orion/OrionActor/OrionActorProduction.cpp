// Fill out your copyright notice in the Description page of Project Settings.

#include "OrionActorProduction.h"
#include "Orion/OrionComponents/OrionAttributeComponent.h"

AOrionActorProduction::AOrionActorProduction()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AOrionActorProduction::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!InventoryComp)
	{
		return;
	}
	if (ProductionCategory == EProductionCategory::Bullets)
	{
		AvailableInventoryMap = { {2, 100}, {3, 2000} }; // 原料 2，上限 100；成品 3，上限 2000
		InventoryComp->AvailableInventoryMap = AvailableInventoryMap;
	}
}

void AOrionActorProduction::BeginPlay()
{
	Super::BeginPlay();

	if (!InventoryComp)
	{
		return;
	}

}

void AOrionActorProduction::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ProductionProgressUpdate(DeltaTime);

	if (ProductionCategory == EProductionCategory::Bullets &&
		InventoryComp && InventoryComp->FullInventoryStatus.FindRef(3))
	{
		ActorStatus = EActorStatus::NotInteractable;
	}
	else
	{
		ActorStatus = EActorStatus::Interactable;
	}
}

void AOrionActorProduction::ProductionProgressUpdate(float DeltaTime)
{
	// Only update production if there's at least one worker.
	if (CurrWorkers < 1 || !InventoryComp) { return; }

	// Calculate the progress increment.
	// For one worker, the progress rate is 100 / ProductionTimeCost per second.
	// With multiple workers, the production speeds up proportionally.
	float ProgressIncrement = (100.0f / ProductionTimeCost) * CurrWorkers * DeltaTime;

	if (ProductionCategory == EProductionCategory::Bullets && InventoryComp->InventoryMap.FindRef(2) < 2)
	{
		return;
	}

	// Accumulate the progress.
	ProductionProgress += ProgressIncrement;

	// When production progress reaches or exceeds 100, consider production complete.
	while (ProductionProgress >= 100.0f)
	{
		// Production cycle complete, add one item to inventory.

		if (ProductionCategory == EProductionCategory::Bullets)
		{
			InventoryComp->ModifyItemQuantity(2, -2);
			InventoryComp->ModifyItemQuantity(3, 50);
		}


		// Subtract 100 from the progress, retaining any leftover progress.
		ProductionProgress -= 100.0f;
	}
}

TArray<FString> AOrionActorProduction::TickShowHoveringInfo()
{
	TArray<FString> Lines;

	const float Progress = ProductionProgress;
	FString Bar;

	constexpr int32 TotalBars = 20; // Segments number
	const int32 FilledBars = FMath::RoundToInt((Progress / 100.0f) * TotalBars);

	for (int32 i = 0; i < TotalBars; ++i)
	{
		Bar += (i < FilledBars) ? TEXT("#") : TEXT("-");
	}

	Lines.Add(FString::Printf(TEXT("Name: %s"), *GetName()));
	Lines.Add(FString::Printf(TEXT("CurrNumOfWorkers: %d"), CurrWorkers));
	Lines.Add(FString::Printf(TEXT("ProductionProgress: [%s] %.1f%%"), *Bar, Progress));

	float CurrentHealth = AttributeComp ? AttributeComp->Health : 0.0f;
	Lines.Add(FString::Printf(TEXT("CurrHealth: %.0f"), CurrentHealth));

	return Lines;
}
