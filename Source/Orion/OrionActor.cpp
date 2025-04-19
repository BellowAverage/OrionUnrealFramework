// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionActor.h"

// Sets default values
AOrionActor::AOrionActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	MaxHealth = 30;
	CurrHealth = MaxHealth;

	this->SetCanBeDamaged(true); // 允许被伤害

	ActorStatus = EActorStatus::Interactable;
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

float AOrionActor::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	CurrHealth -= DamageAmount;
	if (CurrHealth <= 0)
	{
		Die();
	}
	return DamageAmount;
}

void AOrionActor::Die()
{
	// Handle death logic here
	UE_LOG(LogTemp, Log, TEXT("AOrionActor has died."));

	this->ActorStatus = EActorStatus::NotInteractable;

    SpawnDeathEffect(GetActorLocation());
}

void AOrionActor::SpawnDeathEffect_Implementation(FVector DeathLocation)
{
	if (DeathEffectClass)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.Instigator = nullptr; // GameMode 通常没有 Instigator
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			AActor* DeathEffect = World->SpawnActor<AActor>(DeathEffectClass, DeathLocation, FRotator::ZeroRotator, SpawnParams);
			if (DeathEffect)
			{
				UE_LOG(LogTemp, Log, TEXT("Death effect spawned at location: %s"), *DeathLocation.ToString());
			}
		}
	}

	this->Destroy(); // Destroy the actor after spawning the effect

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
