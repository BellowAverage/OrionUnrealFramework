// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionActor.h"
#include "Orion/OrionComponents/OrionInventoryComponent.h"
#include "Orion/OrionComponents/OrionStructureComponent.h"

AOrionActor::AOrionActor()
{
	// Set Attributes

	PrimaryActorTick.bCanEverTick = true;
	SetCanBeDamaged(true);

	// Acquire Components

	// 1. The root mesh component representing the visual appearance of the actor.
	RootStaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootStaticMeshComp->ComponentTags.Add(FName(TEXT("StructureMesh")));

	RootComponent = RootStaticMeshComp; // Supper::RootComponent


	// 2. Defines the ranges within which other actors can interact with this actor.
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->SetupAttachment(RootComponent);

	// 3. Game logics supporting components.
	InventoryComp = CreateDefaultSubobject<UOrionInventoryComponent>(TEXT("InventoryComp"));
	StructureComponent = CreateDefaultSubobject<UOrionStructureComponent>(TEXT("StructureComponent"));
}

TArray<FString> AOrionActor::TickShowHoveringInfo()
{
	return TArray<FString>{ FString::Printf(TEXT("Name: %s"), *GetName()),
		FString::Printf(TEXT("CurrHealth: %d"), CurrHealth) };
}

void AOrionActor::BeginPlay()
{
	Super::BeginPlay();

	InitSerializable(ActorSerializable); // Distribute the sole identifier of this game object.

	if (InventoryComp)
	{
		InventoryComp->AvailableInventoryMap = AvailableInventoryMap;
		InventoryComp->ModifyItemQuantity(2, +2);
	}
}

void AOrionActor::InitSerializable(const FSerializable& /*In*/)
{
	if (!ActorSerializable.GameId.IsValid())
	{
		ActorSerializable.GameId = FGuid::NewGuid();
	}
}


void AOrionActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

float AOrionActor::TakeDamage(float DamageAmount,
                              const FDamageEvent& /*Event*/,
                              AController* /*Instigator*/,
                              AActor* /*Causer*/)
{
	CurrHealth -= FMath::RoundToInt(DamageAmount);
	if (CurrHealth <= 0)
	{
		Die();
	}
	return DamageAmount;
}

void AOrionActor::Die()
{
	UE_LOG(LogTemp, Log, TEXT("OrionActor::Die: %s died."), *GetName());

	ActorStatus = EActorStatus::NotInteractable;
	SpawnDeathEffect(GetActorLocation());
}


void AOrionActor::SpawnDeathEffect_Implementation(FVector Location)
{
	if (!DeathEffectClass)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		World->SpawnActor<AActor>(DeathEffectClass, Location, FRotator::ZeroRotator, P);
	}

	/* 让 Mesh 落地 */
	TArray<UStaticMeshComponent*> Meshes;
	GetComponents<UStaticMeshComponent>(Meshes);
	for (auto* M : Meshes)
	{
		if (M)
		{
			M->SetSimulatePhysics(true);
			M->SetEnableGravity(true);
		}
	}

	/* 5 秒后自毁 */
	GetWorldTimerManager().SetTimer(DeathDestroyTimerHandle,
	                                this,
	                                &AOrionActor::HandleDelayedDestroy,
	                                5.f,
	                                false);
}

void AOrionActor::HandleDelayedDestroy()
{
	Destroy();
}

