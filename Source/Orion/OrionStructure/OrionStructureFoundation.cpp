// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionStructureFoundation.h"
#include "OrionStructureWall.h"
//#include "EngineUtils.h"
//#include "Kismet/KismetSystemLibrary.h"

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
		FoundationSockets.Add(FFoundationSocket(false, SocketLocation, SocketRotation));
	}


	static const TArray<FVector> WallOffsetsLocation = {
		FVector(StructureSizeX, 0.0f, StructureSizeZ + 150.f),
		FVector(0, StructureSizeY, StructureSizeZ + 150.f),
		FVector(-StructureSizeX, 0.0f, StructureSizeZ + 150.f),

		FVector(0, -StructureSizeY, StructureSizeZ + 150.f),
	};

	static const TArray<FRotator> WallOffsetsRotation = {
		FRotator(0.f, 0.f, 0.f),
		FRotator(0.f, 90.f, 0.f),
		FRotator(0.f, 180.f, 0.f),
		FRotator(0.f, 270.f, 0.f),
	};

	for (int32 i = 0; i < WallOffsetsLocation.Num(); ++i)
	{
		const FVector SocketLocation = StructureMesh->GetComponentLocation() + WallOffsetsLocation[i];
		const FRotator SocketRotation = StructureMesh->GetComponentRotation() + WallOffsetsRotation[i];
		WallSockets.Add(FWallSocket(false, SocketLocation, SocketRotation));
	}

	/* ① 自身 socket —— 视为“占用” */
	BuildingManager->RegisterSocket(StructureMesh->GetComponentLocation(),
	                                StructureMesh->GetComponentRotation(),
	                                EOrionStructure::Foundation, /*bOccupied=*/true, GetWorld(), this);

	/* ② 地基‑对‑地基接口 —— 视为“空闲” */
	for (const auto& S : FoundationSockets)
	{
		BuildingManager->RegisterSocket(S.SocketLocation, S.SocketRotation,
		                                EOrionStructure::Foundation, /*bOccupied=*/false, GetWorld(), this);
	}

	/* ③ 地基‑对‑墙接口 —— 视为“空闲”（⚠️ 新增） */
	for (const auto& S : WallSockets)
	{
		BuildingManager->RegisterSocket(S.SocketLocation, S.SocketRotation,
		                                EOrionStructure::Wall, /*bOccupied=*/false, GetWorld(), this);
	}
}
