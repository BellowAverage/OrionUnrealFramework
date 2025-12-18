// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionActor.h"
#include "Orion/OrionComponents/OrionInventoryComponent.h"
#include "Orion/OrionComponents/OrionStructureComponent.h"
#include "Orion/OrionComponents/OrionAttributeComponent.h"

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
	AttributeComp = CreateDefaultSubobject<UOrionAttributeComponent>(TEXT("AttributeComp"));
}

TArray<FString> AOrionActor::TickShowHoveringInfo()
{
	float CurrentHealth = AttributeComp ? AttributeComp->Health : 0.0f;
	return TArray<FString>{ FString::Printf(TEXT("Name: %s"), *GetName()),
		FString::Printf(TEXT("CurrHealth: %.0f"), CurrentHealth) };
}

void AOrionActor::BeginPlay()
{
	Super::BeginPlay();

	InitSerializable(ActorSerializable); // Distribute the sole identifier of this game object.

	if (InventoryComp)
	{
		InventoryComp->AvailableInventoryMap = AvailableInventoryMap;
		InventoryComp->ModifyItemQuantity(2, +50);
	}

	if (AttributeComp)
	{
		AttributeComp->MaxHealth = MaxHealth;
		AttributeComp->SetHealth(MaxHealth);
		AttributeComp->OnHealthZero.AddDynamic(this, &AOrionActor::HandleHealthZero);
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
                              AController* InstigatorController,
                              AActor* /*Causer*/)
{
	if (AttributeComp)
	{
		AttributeComp->ReceiveDamage(DamageAmount, InstigatorController ? InstigatorController->GetPawn() : nullptr);
	}
	return DamageAmount;
}

void AOrionActor::HandleHealthZero(AActor* InstigatorActor)
{
	Die();
}

void AOrionActor::Die()
{
	UE_LOG(LogTemp, Log, TEXT("OrionActor::Die: %s died."), *GetName());

	ActorStatus = EActorStatus::NotInteractable;
	SpawnDeathEffect(GetActorLocation());
}


void AOrionActor::SpawnDeathEffect_Implementation(FVector Location)
{
	UE_LOG(LogTemp, Log, TEXT("OrionActor::SpawnDeathEffect_Implementation: Spawning death effect at location %s."), *Location.ToString());

	if (UWorld* World = GetWorld(); DeathEffectClass)
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

