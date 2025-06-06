// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionStructure.h"

#include "Orion/OrionComponents/OrionStructureComponent.h"
#include "Orion/OrionPlayerController/OrionPlayerController.h"

AOrionStructure::AOrionStructure()
{
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
