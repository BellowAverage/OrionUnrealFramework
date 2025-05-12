// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionStructure.h"

AOrionStructure::AOrionStructure()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AOrionStructure::BeginPlay()
{
	Super::BeginPlay();

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
