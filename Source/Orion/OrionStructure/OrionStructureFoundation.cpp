// Fill out your copyright notice in the Description page of Project Settings.


#include "OrionStructureFoundation.h"
#include "OrionStructureWall.h"

AOrionStructureFoundation::AOrionStructureFoundation()
{
}

void AOrionStructureFoundation::BeginPlay()
{
	Super::BeginPlay();

	/* 0) 基础检测 -------------------------------------------------- */
	if (!StructureMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[Foundation] StructureMesh component not found!"));
		return;
	}

	UOrionBuildingManager* BM =
		GetGameInstance() ? GetGameInstance()->GetSubsystem<UOrionBuildingManager>() : nullptr;

	if (!BM)
	{
		UE_LOG(LogTemp, Error, TEXT("[Foundation] BuildingManager not found!"));
		return;
	}

	/* 1) 计算尺寸 -------------------------------------------------- */
	const float SizeX = StructureMesh->Bounds.BoxExtent.X;
	const float SizeY = StructureMesh->Bounds.BoxExtent.Y;
	const float SizeZ = StructureMesh->Bounds.BoxExtent.Z;

	/* 2) 登记自身 Socket（占用） ----------------------------------- */
	BM->RegisterSocket(
		StructureMesh->GetComponentLocation(),
		StructureMesh->GetComponentRotation(),
		EOrionStructure::Foundation,
		/*bOccupied=*/true,
		GetWorld(), this);

	/* 3) 地基-对-地基接口（空闲） ----------------------------------- */
	static const TArray<FVector> NeighborOffsets = {
		{SizeX * 2.f, 0.f, 0.f}, // East
		{0.f, -SizeY * 2.f, 0.f}, // South
		{-SizeX * 2.f, 0.f, 0.f}, // West
		{0.f, SizeY * 2.f, 0.f} // North
	};

	for (const FVector& Off : NeighborOffsets)
	{
		const FVector Pos = StructureMesh->GetComponentLocation() + Off;
		const FRotator Rot = StructureMesh->GetComponentRotation();

		BM->RegisterSocket(
			Pos, Rot,
			EOrionStructure::Foundation,
			/*bOccupied=*/false,
			GetWorld(), this);
	}

	/* 4) 地基-对-墙接口（空闲） ------------------------------------ */
	static const TArray<FVector> WallOffsetsLocation = {
		{SizeX, 0.f, SizeZ + 150.f},
		{0.f, SizeY, SizeZ + 150.f},
		{-SizeX, 0.f, SizeZ + 150.f},
		{0.f, -SizeY, SizeZ + 150.f}
	};
	static const TArray<FRotator> WallOffsetsRotation = {
		{0.f, 0.f, 0.f},
		{0.f, 90.f, 0.f},
		{0.f, 180.f, 0.f},
		{0.f, 270.f, 0.f}
	};

	for (int32 i = 0; i < WallOffsetsLocation.Num(); ++i)
	{
		const FVector Pos = StructureMesh->GetComponentLocation() + WallOffsetsLocation[i];
		const FRotator Rot = StructureMesh->GetComponentRotation() + WallOffsetsRotation[i];

		BM->RegisterSocket(
			Pos, Rot,
			EOrionStructure::Wall,
			/*bOccupied=*/false,
			GetWorld(), this);
	}
}
