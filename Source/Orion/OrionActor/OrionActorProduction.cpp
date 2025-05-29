// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionActorProduction.h"


AOrionActorProduction::AOrionActorProduction()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AOrionActorProduction::BeginPlay()
{
	Super::BeginPlay();

	if (!InventoryComp)
	{
		return;
	}

	if (ProductionCategory == EProductionCategory::Bullets)
	{
		AvailableInventoryMap = {{2, 100}, {3, 2000}}; // 原料 2，上限 100；成品 3，上限 2000
		InventoryComp->AvailableInventoryMap = AvailableInventoryMap;
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
