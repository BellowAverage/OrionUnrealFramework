// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionActorOre.h"

void AOrionActorOre::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	ProductionProgressUpdate(DeltaTime);
}

void AOrionActorOre::ProductionProgressUpdate(float DeltaTime)
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

	// Accumulate the progress.
	ProductionProgress += ProgressIncrement;

	// When production progress reaches or exceeds 100, consider production complete.
	while (ProductionProgress >= 100.0f)
	{
		// Production cycle complete, add one item to inventory.
		InventoryComp->ModifyItemQuantity(2, 1);

		// Subtract 100 from the progress, retaining any leftover progress.
		ProductionProgress -= 100.0f;
	}
}
