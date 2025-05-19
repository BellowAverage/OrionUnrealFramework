// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionActorProduction.h"


AOrionActorProduction::AOrionActorProduction()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AOrionActorProduction::BeginPlay()
{
	Super::BeginPlay();

	if (InventoryComp)
	{
		//InventoryComp->ModifyItemQuantity(2, 10);

		if (ProductionCategory == EProductionCategory::Bullets)
		{
			InventoryComp->AvailableInventoryMap.Add(2, 100);
			InventoryComp->AvailableInventoryMap.Add(3, 2000);
			UE_LOG(LogTemp, Log,
			       TEXT("[Production] Set capacity for Bullets: Item2=100, Item3=10"));
		}
	}
}


void AOrionActorProduction::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ProductionProgressUpdate(DeltaTime);

	if (ProductionCategory == EProductionCategory::Bullets && InventoryComp->FullInventoryStatus.FindRef(3))
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
	if (CurrWorkers < 1)
	{
		return;
	}

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
