// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionStructure.h"
#include "Orion/OrionPlayerController/OrionPlayerController.h"
//#include "Kismet/KismetSystemLibrary.h"
#include "Orion/OrionGameInstance/OrionBuildingManager.h"

AOrionStructure::AOrionStructure()
{
	PrimaryActorTick.bCanEverTick = false;
	StructureMesh = nullptr;
}


void AOrionStructure::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	AOrionPlayerController* OrionPC = Cast<AOrionPlayerController>(PC);

	if (OrionPC->bIsSpawnPreviewStructure)
	{
		OrionPC->bIsSpawnPreviewStructure = false;
		return;
	}

	if (BuildingManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOrionBuildingManager>() : nullptr;
		!BuildingManager)
	{
		UE_LOG(LogTemp, Error, TEXT("BuildingManager not found!"));
		return;
	}


	TArray<UActorComponent*> TaggedComps =
		GetComponentsByTag(
			UStaticMeshComponent::StaticClass(),
			FName("StructureMesh")
		);

	if (TaggedComps.Num() > 0)
	{
		StructureMesh = Cast<UStaticMeshComponent>(TaggedComps[0]);
	}

	if (!StructureMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("No StructureMesh Found. "));
	}
}

void AOrionStructure::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AOrionStructure::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UOrionBuildingManager* NewBuildingManager = nullptr;

	if (NewBuildingManager = GetGameInstance() ? GetGameInstance()->GetSubsystem<UOrionBuildingManager>() : nullptr;
		!NewBuildingManager)
	{
		UE_LOG(LogTemp, Error, TEXT("NewBuildingManager not found!"));
		return;
	}
	NewBuildingManager->RemoveSocketRegistration<AOrionStructure>(*this);


	// 2) 再交给父类做销毁流程
	Super::EndPlay(EndPlayReason);
}
