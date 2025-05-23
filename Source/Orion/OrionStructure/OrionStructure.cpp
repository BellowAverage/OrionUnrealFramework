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


	if (const auto* PC = Cast<AOrionPlayerController>(GetWorld()->GetFirstPlayerController()))
	{
		if (PC->bIsSpawnPreviewStructure)
		{
			const_cast<AOrionPlayerController*>(PC)->bIsSpawnPreviewStructure = false;
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
