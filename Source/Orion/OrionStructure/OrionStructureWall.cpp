// Fill out your copyright notice in the Description page of Project Settings.


#include "Orion/OrionStructure/OrionStructureWall.h"
#include "Orion/OrionStructure/OrionStructureFoundation.h"
//#include "EngineUtils.h"
//#include "Kismet/KismetSystemLibrary.h"


void AOrionStructureWall::BeginPlay()
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
		FVector(0.f, 0.f, StructureSizeZ * 2.f), // Up
	};

	for (int32 i = 0; i < Offsets.Num(); ++i)
	{
		const FVector SocketLocation = StructureMesh->GetComponentLocation() + Offsets[i];
		const FRotator SocketRotation = StructureMesh->GetComponentRotation();
		WallSockets.Add(FWallSocket(false, SocketLocation, SocketRotation));
	}

	AActor* Support = nullptr;
	{
		// ① 射线向下检查 10cm 以内是否碰到 Foundation 或 Wall
		FHitResult Hit;
		FVector Start = StructureMesh->GetComponentLocation();
		FVector End = Start - FVector(0, 0, 10.f);

		if (GetWorld()->LineTraceSingleByChannel(
			Hit, Start, End, ECC_WorldStatic))
		{
			Support = Hit.GetActor(); // 可能是 Foundation 或下层 Wall
		}
	}

	/* ① 自身 socket —— 视为“占用” */
	BuildingManager->RegisterSocket(
		StructureMesh->GetComponentLocation(),
		StructureMesh->GetComponentRotation(),
		EOrionStructure::Wall,
		/*bOccupied=*/true,
		GetWorld(), this);

	/* ② 普通接口 socket —— 视为“空闲” */
	for (const auto& S : WallSockets)
	{
		BuildingManager->RegisterSocket(
			S.SocketLocation,
			S.SocketRotation,
			EOrionStructure::Wall,
			/*bOccupied=*/false,
			GetWorld(), this);
	}
}
