// Fill out your copyright notice in the Description page of Project Settings.


#include "Orion/OrionStructure/OrionStructureWall.h"
#include "Orion/OrionStructure/OrionStructureFoundation.h"


AOrionStructureWall::AOrionStructureWall()
{
	if (!StructureComponent)
	{
		return;
	}

	//StructureComponent->OrionStructureType = EOrionStructure::Wall;
}

void AOrionStructureWall::BeginPlay()
{
	Super::BeginPlay();
}
