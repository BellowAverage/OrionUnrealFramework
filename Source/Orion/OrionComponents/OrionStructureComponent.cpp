// Fill out your copyright notice in the Description page of Project Settings.
#include "Orion/OrionComponents/OrionStructureComponent.h"
#include "Orion/OrionGlobals/EOrionStructure.h"
#include "Orion/OrionPlayerController/OrionPlayerController.h"

// Sets default values for this component's properties
UOrionStructureComponent::UOrionStructureComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	StructureMesh = nullptr;

	// ...
}

void UOrionStructureComponent::BeginPlay()
{
	Super::BeginPlay();

	if (OrionStructureType == EOrionStructure::None)
	{
		UE_LOG(LogTemp, Error, TEXT("[StructureComponent] StructureType is None!"));
		return;
	}

	BuildingManager = GetWorld()->GetGameInstance()
		                  ? GetWorld()->GetGameInstance()->GetSubsystem<UOrionBuildingManager>()
		                  : nullptr;

	if (!BuildingManager)
	{
		UE_LOG(LogTemp, Error, TEXT("[StructureComponent] BuildingManager not found!"));
		return;
	}

	if (!StructureMesh)
	{
		TArray<UActorComponent*> Tagged =
			GetOwner()->GetComponentsByTag(UStaticMeshComponent::StaticClass(),
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

	if (bAutoRegisterSockets && BuildingManager)
	{
		RegisterAllSockets();
	}
}

void UOrionStructureComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BuildingManager)
	{
		if (UOrionBuildingManager* SafeInitializedBuildingManager =
			GetOwner()->GetGameInstance()->GetSubsystem<UOrionBuildingManager>())
		{
			SafeInitializedBuildingManager->RemoveSocketRegistration(*GetOwner());
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UOrionStructureComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UOrionStructureComponent::RegisterAllSockets() const
{
	if (!StructureMesh || !BuildingManager) { return; }

	const FVector StructureLocation = StructureMesh->GetComponentLocation();
	const FRotator StructureRotation = StructureMesh->GetComponentRotation();
	const FVector StructureBound = FVector(50.f, 50.f, 10.f);

	/*========= Square Foundation =========*/
	if (OrionStructureType == EOrionStructure::BasicSquareFoundation)
	{
		// ─ 自身（占用）
		BuildingManager->RegisterSocket(StructureLocation, StructureRotation, EOrionStructure::BasicSquareFoundation,
		                                true, GetWorld(), GetOwner());

		// ─ 地基-对-地基（空闲）
		static const FVector Offsets[4] = {
			{StructureBound.X * 2, 0.f, 0.f},
			{0.f, -StructureBound.Y * 2, 0.f},
			{-StructureBound.X * 2, 0.f, 0.f},
			{0.f, StructureBound.Y * 2, 0.f}
		};
		for (const FVector& Off : Offsets)
		{
			BuildingManager->RegisterSocket(StructureLocation + StructureRotation.RotateVector(Off), StructureRotation,
			                                EOrionStructure::BasicSquareFoundation, false, GetWorld(), GetOwner());
		}

		// ─ 地基-对-墙（空闲）
		static const FVector WallPosOff[4] = {
			{StructureBound.X, 0.f, StructureBound.Z + 150.f},
			{0.f, StructureBound.Y, StructureBound.Z + 150.f},
			{-StructureBound.X, 0.f, StructureBound.Z + 150.f},
			{0.f, -StructureBound.Y, StructureBound.Z + 150.f}
		};
		static const FRotator WallRotOff[4] = {
			{0, 0, 0},
			{0, 90, 0},
			{0, 180, 0},
			{0, 270, 0}
		};
		for (int32 i = 0; i < 4; ++i)
		{
			BuildingManager->RegisterSocket(
				StructureLocation + StructureRotation.RotateVector(WallPosOff[i]), StructureRotation + WallRotOff[i],
				EOrionStructure::Wall, false, GetWorld(), GetOwner());
		}

		// Square Foundation - 对 - Triangle Foundation
		{
			static const FVector TriPosOff[4] = {
				{StructureBound.X + StructureBound.X * 0.866 / 2 + 7.5f, 0.f, 0.f}, // +X 边中点
				{-StructureBound.X - StructureBound.X * 0.866 / 2 - 7.5f, 0.f, 0.f}, // -X
				{0.f, StructureBound.Y + StructureBound.Y * 0.866 / 2 + 7.5f, 0.f}, // +Y
				{0.f, -StructureBound.Y - StructureBound.Y * 0.866 / 2 - 7.5f, 0.f} // -Y
			};
			static constexpr float TriYaw[4] = {180.f, 0.f, 270.f, 90.f}; // 让三角边法线指向方形

			for (int32 i = 0; i < 4; ++i)
			{
				BuildingManager->RegisterSocket(
					StructureLocation + StructureRotation.RotateVector(TriPosOff[i]),
					StructureRotation + FRotator(0.f, TriYaw[i], 0.f),
					EOrionStructure::BasicTriangleFoundation,
					/*bOccupied=*/false, GetWorld(), GetOwner());
			}
		}
	}

	/*========= Triangle Foundation =========*/
	else if (OrionStructureType == EOrionStructure::BasicTriangleFoundation)
	{
		//const float EdgeLength = StructureBound.Y * 2.f; // 等边三角形的边长
		constexpr float EdgeLength = 100.f; // 等边三角形的边长
		const float HalfMeshHeight = StructureBound.Z;
		const float HalfSquareRootThree = FMath::Sqrt(3.f) / 2.f;
		const float HalfSquareRootTwo = FMath::Sqrt(3.f) / 2.f;

		const FVector AdjTriangleOffset[3] = {
			{EdgeLength / (HalfSquareRootThree * 2.f), 0.f, 0.f},
			{-EdgeLength / (4.f * HalfSquareRootThree), EdgeLength * 0.5f, 0.f},
			{-EdgeLength / (4.f * HalfSquareRootThree), -EdgeLength * 0.5f, 0.f}
		};

		const FVector AdjSquareOffset[3] = {
			{(HalfSquareRootThree * 2.f + 1.f) / (2.f * 2.f * HalfSquareRootThree) * EdgeLength, 0.f, 0.f},
			{
				-(0.25f + HalfSquareRootThree * 2.f / 12.f) * EdgeLength,
				-(0.25f + HalfSquareRootThree * 2.f / 4.f) * EdgeLength, 0.f
			},

			{
				-(0.25f + HalfSquareRootThree * 2.f / 12.f) * EdgeLength,
				(0.25f + HalfSquareRootThree * 2.f / 4.f) * EdgeLength, 0.f
			},

		};

		/*── 1) 自身（占用） ─────────────────────────────────────*/
		BuildingManager->RegisterSocket(
			StructureLocation, StructureRotation,
			EOrionStructure::BasicTriangleFoundation,
			/*bOccupied=*/true, GetWorld(), GetOwner());

		/*── 2) 三角 ↔ 三角 / ↔ 方形（空闲） ─────────────────────*/
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector WorldTrianglePos = StructureLocation + StructureRotation.RotateVector(AdjTriangleOffset[i]);
			//const FVector WorldTrianglePos = StructureLocation + AdjTriangleOffset[i];

			// （a）同类三角
			constexpr float TriangleRotationOffset[3] = {180.f, 180.f, 180.f};
			BuildingManager->RegisterSocket(
				WorldTrianglePos,
				StructureRotation + FRotator(0.f, TriangleRotationOffset[i], 0.f),
				EOrionStructure::BasicTriangleFoundation,
				/*bOccupied=*/false, GetWorld(), GetOwner());

			const FVector WorldSquarePos = StructureLocation + StructureRotation.RotateVector(AdjSquareOffset[i]);

			// （b）邻接方形
			constexpr float SquareRotationOffset[3] = {0.f, 60.f, 120.f};
			BuildingManager->RegisterSocket(
				WorldSquarePos,
				StructureRotation + FRotator(0.f, SquareRotationOffset[i], 0.f),
				EOrionStructure::BasicSquareFoundation,
				/*bOccupied=*/false, GetWorld(), GetOwner());
		}

		const FVector TriangleEdgeMidpoints[3] = {
			{EdgeLength * (1.f / (4.f * HalfSquareRootThree)), 0.f, 0.f},
			{-1.f / (4.f * HalfSquareRootThree * 2.f) * EdgeLength, 0.25f * EdgeLength, 0.f},
			{-1.f / (4.f * HalfSquareRootThree * 2.f) * EdgeLength, -0.25f * EdgeLength, 0.f},

		};

		/*── 3) Triangle → Wall（Z ↑150，空闲） ──────────────────*/
		for (int32 i = 0; i < 3; ++i)
		{
			constexpr float TriangleEdgeWallRotation[3] = {180.f, 300.f, 60.f};
			const FVector WorldPos =
				StructureLocation + StructureRotation.RotateVector(
					TriangleEdgeMidpoints[i] + FVector(0.f, 0.f, HalfMeshHeight + 150.f));

			BuildingManager->RegisterSocket(
				WorldPos,
				StructureRotation + FRotator(0.f, TriangleEdgeWallRotation[i], 0.f),
				EOrionStructure::Wall,
				/*bOccupied=*/false, GetWorld(), GetOwner());
		}
	}

	/*========= Wall =========*/
	else if (OrionStructureType == EOrionStructure::Wall)
	{
		// ─ 自身（占用）
		BuildingManager->RegisterSocket(StructureLocation, StructureRotation, EOrionStructure::Wall,
		                                true, GetWorld(), GetOwner());

		// ─ 向上延伸（空闲）
		FVector Up = StructureLocation;
		Up.Z += StructureBound.Z * 2.f;
		BuildingManager->RegisterSocket(Up, StructureRotation, EOrionStructure::Wall,
		                                false, GetWorld(), GetOwner());
	}
}
