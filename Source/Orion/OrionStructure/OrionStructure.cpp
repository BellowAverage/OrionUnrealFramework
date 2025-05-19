// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionStructure.h"
#include "Orion/OrionPlayerController/OrionPlayerController.h"
#include "Orion/OrionGameInstance/OrionBuildingManager.h"

AOrionStructure::AOrionStructure()
{
	PrimaryActorTick.bCanEverTick = false;
	StructureMesh = nullptr;
}


void AOrionStructure::BeginPlay()
{
	Super::BeginPlay();


	if (!StructureMesh)
	{
		TArray<UActorComponent*> Tagged =
			GetComponentsByTag(UStaticMeshComponent::StaticClass(),
			                   FName("StructureMesh"));

		if (Tagged.Num() > 0)
		{
			StructureMesh = Cast<UStaticMeshComponent>(Tagged[0]);
		}
	}


	AOrionPlayerController* OrionPC =
		Cast<AOrionPlayerController>(GetWorld()->GetFirstPlayerController());

	if (OrionPC && OrionPC->bIsSpawnPreviewStructure)
	{
		OrionPC->bIsSpawnPreviewStructure = false;
		return;
	}

	if (BuildingManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOrionBuildingManager>() : nullptr;
		!BuildingManager)
	{
		UE_LOG(LogTemp, Error, TEXT("BuildingManager not found!"));
	}
}

void AOrionStructure::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AOrionStructure::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UOrionBuildingManager* BM = GetGameInstance()->GetSubsystem<UOrionBuildingManager>())
	{
		BM->RemoveSocketRegistration<AOrionStructure>(*this);
	}

	Super::EndPlay(EndPlayReason);
}
