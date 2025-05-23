// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionActor.h"
#include "Orion/OrionComponents/OrionInventoryComponent.h"

AOrionActor::AOrionActor()
{
	PrimaryActorTick.bCanEverTick = true;

	/*

	RootStaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));

	RootComponent = RootStaticMeshComp;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));

	*/


	InventoryComp = CreateDefaultSubobject<UOrionInventoryComponent>(TEXT("InventoryComp"));


	MaxHealth = 30;
	CurrHealth = MaxHealth;

	this->SetCanBeDamaged(true);

	ActorStatus = EActorStatus::Interactable;
}

void AOrionActor::BeginPlay()
{
	Super::BeginPlay();

	InventoryComp = FindComponentByClass<UOrionInventoryComponent>();

	CollisionSphere = FindComponentByClass<USphereComponent>();

	if (InventoryComp)
	{
		InventoryComp->AvailableInventoryMap.Empty();
		InventoryComp->AvailableInventoryMap = AvailableInventoryMap;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("%s missing UOrionInventoryComponent!"), *GetName());
	}

	InventoryComp->ModifyItemQuantity(2, +2);
}

void AOrionActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

float AOrionActor::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
                              AActor* DamageCauser)
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
		if (UWorld* World = GetWorld())
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.Instigator = nullptr;
			SpawnParams.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			AActor* DeathEffect = World->SpawnActor<AActor>(DeathEffectClass, DeathLocation, FRotator::ZeroRotator,
			                                                SpawnParams);
			if (DeathEffect)
			{
				UE_LOG(LogTemp, Log, TEXT("Death effect spawned at location: %s"), *DeathLocation.ToString());
			}
		}
	}

	TArray<UStaticMeshComponent*> MeshComps;
	GetComponents<UStaticMeshComponent>(MeshComps);
	for (UStaticMeshComponent* MeshComp : MeshComps)
	{
		if (!MeshComp)
		{
			continue;
		}

		MeshComp->SetEnableGravity(true);
		MeshComp->SetSimulatePhysics(true);
	}

	// 3) Call HandleDelayedDestroy() after 5 seconds to destroy itself
	GetWorldTimerManager()
		.SetTimer(DeathDestroyTimerHandle,
		          this,
		          &AOrionActor::HandleDelayedDestroy,
		          5.0f,
		          false);
}

void AOrionActor::HandleDelayedDestroy()
{
	Destroy();
}

FString AOrionActor::GetInteractType()
{
	return InteractType;
}
