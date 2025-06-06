// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionActor.h"
#include "Orion/OrionComponents/OrionInventoryComponent.h"
#include "Orion/OrionComponents/OrionStructureComponent.h"

AOrionActor::AOrionActor()
{
	/*PrimaryActorTick.bCanEverTick = true;*/

	/*

	RootStaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));

	RootComponent = RootStaticMeshComp;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));

	*/


	/*InventoryComp = CreateDefaultSubobject<UOrionInventoryComponent>(TEXT("InventoryComp"));


	MaxHealth = 30;
	CurrHealth = MaxHealth;

	this->SetCanBeDamaged(true);

	ActorStatus = EActorStatus::Interactable;*/


	PrimaryActorTick.bCanEverTick = true;


	RootStaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootStaticMeshComp->ComponentTags.Add(FName(TEXT("StructureMesh")));

	RootComponent = RootStaticMeshComp;


	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->SetupAttachment(RootComponent);


	InventoryComp = CreateDefaultSubobject<UOrionInventoryComponent>(TEXT("InventoryComp"));

	StructureComponent = CreateDefaultSubobject<UOrionStructureComponent>(TEXT("StructureComponent"));


	SetCanBeDamaged(true);
}

void AOrionActor::BeginPlay()
{
	Super::BeginPlay();

	/*InitSerializable(ActorSerializable);

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

	InventoryComp->ModifyItemQuantity(2, +2);*/

	InitSerializable(ActorSerializable);

	/* 加载后 AvailableInventoryMap 会被反序列化覆盖 */
	if (InventoryComp)
	{
		InventoryComp->AvailableInventoryMap = AvailableInventoryMap;
		InventoryComp->ModifyItemQuantity(2, +2); // 示例逻辑
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
	UE_LOG(LogTemp, Log, TEXT("%s died."), *GetName());

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
