// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionStructure.h"

#include "Orion/OrionComponents/OrionStructureComponent.h"

AOrionStructure::AOrionStructure()
{
	AttributeComp = CreateDefaultSubobject<UOrionAttributeComponent>(TEXT("AttributeComp"));

	if (UOrionStructureComponent* FoundStructureComp = FindComponentByClass<UOrionStructureComponent>())
	{
		StructureComponent = FoundStructureComp;
	}
	else
	{
		/*StructureComponent = CreateDefaultSubobject<UOrionStructureComponent>(TEXT("StructureComponent"));*/
		return;
	}


	PrimaryActorTick.bCanEverTick = false;
}


void AOrionStructure::BeginPlay()
{
	Super::BeginPlay();

	if (AttributeComp)
	{
		AttributeComp->MaxHealth = MaxHealth;
		AttributeComp->SetHealth(MaxHealth);
		AttributeComp->OnHealthZero.AddDynamic(this, &AOrionStructure::HandleHealthZero);
	}

	/*const_cast<AOrionPlayerController*>(PC)->bIsSpawnPreviewStructure = false;*/

	/* Preview Structure */

	if (StructureComponent && StructureComponent->BIsPreviewStructure)
	{
		TArray<UPrimitiveComponent*> PrimComps;
		GetComponents<UPrimitiveComponent>(PrimComps);

		for (UPrimitiveComponent* Prim : PrimComps)
		{
			if (!Prim) { continue; }

			Prim->SetGenerateOverlapEvents(false);
			Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 完全禁用碰撞（可选）
		}
	}
}

void AOrionStructure::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AOrionStructure::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

float AOrionStructure::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator,
                                  AActor* DamageCauser)
{
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (AttributeComp)
	{
		AttributeComp->ReceiveDamage(ActualDamage, EventInstigator ? EventInstigator->GetPawn() : DamageCauser);
	}
	return ActualDamage;
}

void AOrionStructure::HandleHealthZero(AActor* InstigatorActor)
{
	Die();
}

void AOrionStructure::Die()
{
	UE_LOG(LogTemp, Log, TEXT("Structure %s Destroyed"), *GetName());
	Destroy();
}

TArray<FString> AOrionStructure::TickShowHoveringInfo()
{
	TArray<FString> Info;

	const UOrionAttributeComponent* Attr = FindComponentByClass<UOrionAttributeComponent>();
	if (Attr)
	{
		Info.Add(FString::Printf(TEXT("Health: %.0f / %.0f"), Attr->Health, Attr->MaxHealth));
	}
	else
	{
		Info.Add(TEXT("Health: N/A (no AttributeComp)"));
	}

	return Info;
}
