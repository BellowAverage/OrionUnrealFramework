// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionStructureFoundation.h"
#include "OrionStructureWall.h"

AOrionStructureFoundation::AOrionStructureFoundation()
{
	if (!StructureComponent)
	{
		return;
	}

	/*switch (FoundationType)
	{
	case EOrionFoundationType::BasicSquare:
		StructureComponent->OrionStructureType = EOrionStructure::BasicSquareFoundation;
		break;
	case EOrionFoundationType::BasicTriangle:
		StructureComponent->OrionStructureType = EOrionStructure::BasicTriangleFoundation;
		break;
	default:
		UE_LOG(LogTemp, Error, TEXT("[Foundation] Unknown Foundation Type!"));
		break;
	}*/
}


void AOrionStructureFoundation::BeginPlay()
{
	Super::BeginPlay();
}
