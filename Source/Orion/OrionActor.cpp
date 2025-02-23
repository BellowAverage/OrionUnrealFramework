// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionActor.h"

// Sets default values
AOrionActor::AOrionActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AOrionActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AOrionActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ProductionProgressUpdate(DeltaTime);
}



FString AOrionActor::GetInteractType()
{
	return InteractType;
}

void AOrionActor::ProductionProgressUpdate(float DeltaTime)
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
        CurrInventory += 1;

        // Subtract 100 from the progress, retaining any leftover progress.
        ProductionProgress -= 100.0f;
    }
}

int AOrionActor::GetOutItemId() const
{
	return OutItemId;
}

int AOrionActor::GetInItemId() const
{
	return InItemId;
}

int AOrionActor::GetProductionTimeCost() const
{
	return ProductionTimeCost;
}

int AOrionActor::GetMaxWorkers() const
{
    return MaxWorkers;
}

int AOrionActor::GetCurrWorkers() const
{
    return CurrWorkers;
}

float AOrionActor::GetProductionProgress() const
{
    return ProductionProgress;
}

int AOrionActor::GetCurrInventory() const
{
    return CurrInventory;
}