// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionStructureFoundation.h"

AOrionStructureFoundation::AOrionStructureFoundation()
{
}

void AOrionStructureFoundation::BeginPlay()
{
	Super::BeginPlay();

	if (!StructureMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("StructureMesh component not found!"));
		return;
	}

	const float StructureSizeX = StructureMesh->Bounds.BoxExtent.X;
	const float StructureSizeY = StructureMesh->Bounds.BoxExtent.Y;
	const float StructureSizeZ = StructureMesh->Bounds.BoxExtent.Z;

	static const TArray<FVector> Offsets = {
		FVector(StructureSizeX * 2.0f, 0.f, 0.f), // East
		FVector(0.f, -StructureSizeY * 2.0f, 0.f), // South
		FVector(-StructureSizeX * 2.0f, 0.f, 0.f), // West
		FVector(0.f, StructureSizeY * 2.0f, 0.f), // North
	};

	for (int32 i = 0; i < Offsets.Num(); ++i)
	{
		const FVector SocketLocation = StructureMesh->GetComponentLocation() + Offsets[i];
		const FRotator SocketRotation = StructureMesh->GetComponentRotation();
		FoundationSockets.Add(FFoundationSocket(this, false, SocketLocation, SocketRotation));
	}
}

void AOrionStructureFoundation::PlaceFoundationStructure(int32 InSocketIndex,
                                                         FRotator& InRotationOffset)
{
	if (!StructureMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("StructureMesh component not found!"));
		return;
	}

	if (InSocketIndex < 0 || InSocketIndex >= FoundationSockets.Num())
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid socket index or mesh!"));
		return;
	}

	if (!BlueprintFoundationInstance)
	{
		UE_LOG(LogTemp, Error, TEXT("BlueprintFoundationInstance is null!"));
		return;
	}

	FFoundationSocket& ModifyingSocket = FoundationSockets[InSocketIndex];
	if (ModifyingSocket.bIsOccupied)
	{
		UE_LOG(LogTemp, Warning, TEXT("Socket already occupied"));
		return;
	}

	const FVector SpawnLocation = ModifyingSocket.SocketLocation;
	const FRotator SpawnRotation = ModifyingSocket.SocketRotation + InRotationOffset;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AOrionStructureFoundation* NewFoundation =
		GetWorld()->SpawnActor<AOrionStructureFoundation>(
			BlueprintFoundationInstance, SpawnLocation, SpawnRotation, Params);

	if (!NewFoundation)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to spawn new foundation"));
		return;
	}

	const int32 OppositeIdx = (InSocketIndex + 2) % 4;

	ModifyingSocket.bIsOccupied = true;
	ModifyingSocket.SocketStructurePtr = NewFoundation;

	NewFoundation->FoundationSockets[OppositeIdx].bIsOccupied = true;
	NewFoundation->FoundationSockets[OppositeIdx].SocketStructurePtr = this;
}
