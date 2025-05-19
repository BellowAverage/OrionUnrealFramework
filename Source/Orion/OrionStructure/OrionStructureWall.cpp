// Fill out your copyright notice in the Description page of Project Settings.


#include "Orion/OrionStructure/OrionStructureWall.h"
#include "Orion/OrionStructure/OrionStructureFoundation.h"
//#include "EngineUtils.h"
//#include "Kismet/KismetSystemLibrary.h"


void AOrionStructureWall::BeginPlay()
{
	Super::BeginPlay();

	/* 0) 检查组件与管理器 ----------------------------------------- */
	if (!StructureMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("[Wall] StructureMesh component not found!"));
		return;
	}

	UOrionBuildingManager* BM =
		GetGameInstance() ? GetGameInstance()->GetSubsystem<UOrionBuildingManager>() : nullptr;

	if (!BM)
	{
		UE_LOG(LogTemp, Error, TEXT("[Wall] BuildingManager not found!"));
		return;
	}

	const float SizeZ = StructureMesh->Bounds.BoxExtent.Z;

	/* 1) 自身 Socket（占用） ------------------------------------- */
	BM->RegisterSocket(
		StructureMesh->GetComponentLocation(),
		StructureMesh->GetComponentRotation(),
		EOrionStructure::Wall,
		/*bOccupied=*/true,
		GetWorld(), this);

	/* 2) 向上延伸接口（空闲） ------------------------------------ */
	const FVector UpLoc = StructureMesh->GetComponentLocation() +
		FVector(0.f, 0.f, SizeZ * 2.f);
	const FRotator UpRot = StructureMesh->GetComponentRotation();

	BM->RegisterSocket(
		UpLoc, UpRot,
		EOrionStructure::Wall,
		/*bOccupied=*/false,
		GetWorld(), this);

	/* 3) （可选）检测下层支撑 ----------------------------------- */
	// 若需要确定支撑合法性，可在此使用 LineTrace，
	// 不影响 Socket 注册逻辑，因此此处略。
}
