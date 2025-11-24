// Fill out your copyright notice in the Description page of Project Settings.
#include "Orion/OrionComponents/OrionStructureComponent.h"
#include "Orion/OrionGameInstance/OrionBuildingManager.h"

// #include "Components/ArrowComponent.h"
// #include "Components/BoxComponent.h"
class UPrimitiveComponent;


#define STRUCTURE_THICKNESS_BASE 10.f
#define STRUCTURE_LENGTH_BASE 50.f * 1.25f
#define STRUCTURE_HEIGHT_BASE 150.f

const TMap<EOrionStructure, FVector> UOrionStructureComponent::StructureBoundMap =
{
	// ---------- X (half) Y (half) Z (half) -------------
	{EOrionStructure::BasicSquareFoundation, {STRUCTURE_LENGTH_BASE, STRUCTURE_LENGTH_BASE, STRUCTURE_THICKNESS_BASE}},
	{
		EOrionStructure::BasicTriangleFoundation,
		{STRUCTURE_LENGTH_BASE * (FMath::Sqrt(3.f) / 2), STRUCTURE_LENGTH_BASE, STRUCTURE_THICKNESS_BASE}
	},
	{EOrionStructure::Wall, {STRUCTURE_THICKNESS_BASE, STRUCTURE_LENGTH_BASE, STRUCTURE_HEIGHT_BASE}},
	{EOrionStructure::DoubleWall, {STRUCTURE_THICKNESS_BASE, 2.f * STRUCTURE_LENGTH_BASE, STRUCTURE_HEIGHT_BASE}},
	{EOrionStructure::BasicRoof, {STRUCTURE_LENGTH_BASE,STRUCTURE_LENGTH_BASE, STRUCTURE_LENGTH_BASE}},
};


// Sets default values for this component's properties
UOrionStructureComponent::UOrionStructureComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	StructureMesh = nullptr;
	bAutoRegisterSockets = true;
	
	// [Fix] 默认为 false (实体)。只有 Controller 生成幽灵时才需手动设为 true。
	BIsPreviewStructure = false;
	
	bIsAdjustable = false;

	// 根据类型设置默认衰减，也可以在蓝图里配
	StabilityDecay = 0.1f; 
}


void UOrionStructureComponent::BeginPlay()
{
	Super::BeginPlay();

	BuildingManager = GetWorld()->GetGameInstance()->GetSubsystem<UOrionBuildingManager>();

	checkf(BuildingManager, TEXT("OrionStructureComponent::BeginPlay: Unable to find Building Manager Reference. "))


	if (TArray<UActorComponent*> StructureMeshActorComponents = GetOwner()->GetComponentsByTag(UStaticMeshComponent::StaticClass(), FName("StructureMesh")); StructureMeshActorComponents.Num() > 0)
	{
		StructureMesh = Cast<UStaticMeshComponent>(StructureMeshActorComponents[0]);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("OrionStructureComponent::BeginPlay: Unable to get StructureMesh StaticMeshComponent. "));
	}


	// 根据类型微调 Decay (如果蓝图没设的话)
	if (OrionStructureType == EOrionStructure::Wall || OrionStructureType == EOrionStructure::DoubleWall)
		StabilityDecay = 0.1f; // 墙壁很硬
	else 
		StabilityDecay = 0.25f; // 屋顶/地板比较脆

	if (bAutoRegisterSockets && !BIsPreviewStructure)
	{
		SocketsRegistryHandler();
		// 注意：BeginPlay 时不需要手动调 UpdateStability，
		// 因为 BuildingManager::DelaySpawnNewStructure 里会调。
	}
}


// ---------------------------------------------------------
// [Core] 稳定性算法
// ---------------------------------------------------------	 
void UOrionStructureComponent::UpdateStability()
{
	if (!BuildingManager) return;

	float NewStability = 0.0f;

	// 1. 接地检测 (Anchor = 100%)
	if (CheckIsTouchingGround())
	{
		NewStability = 1.0f;
	}
	else
	{
		// 2. 获取所有物理连接的邻居
		TArray<UOrionStructureComponent*> Neighbors = BuildingManager->GetConnectedNeighbors(GetOwner());
		
		float MaxNeighborStability = 0.0f;
		UOrionStructureComponent* BestNeighbor = nullptr;

		for (const UOrionStructureComponent* Neighbor : Neighbors)
		{
			if (Neighbor->CurrentStability > MaxNeighborStability)
			{
				MaxNeighborStability = Neighbor->CurrentStability;
				BestNeighbor = const_cast<UOrionStructureComponent*>(Neighbor);
			}
		}

		// [Debug] 可视化支撑关系：画一条线指向支撑我的那个邻居
		if (BestNeighbor && GetWorld())
		{
			DrawDebugLine(GetWorld(), GetOwner()->GetActorLocation(), BestNeighbor->GetOwner()->GetActorLocation(), 
				FColor::Cyan, false, 0.1f, 0, 5.0f);
		}

		// 3. 计算：最强邻居 - 衰减
		NewStability = MaxNeighborStability - StabilityDecay;
	}

	// 4. 钳制范围
	if (NewStability < 0.0f) NewStability = 0.0f;

	// 5. 只有数值变化了才处理 (防止无限递归)
	if (!FMath::IsNearlyEqual(CurrentStability, NewStability, 0.001f))
	{
		CurrentStability = NewStability;

		UE_LOG(LogTemp, Log, TEXT("[Stability] %s value updated: %.2f"), *GetOwner()->GetName(), CurrentStability);

		// [Debug] 持久化显示稳定性数值 (事件触发)
		if (GetWorld())
		{
			// 使用 -1.0f (Infinite duration) 持久显示
			DrawDebugString(GetWorld(), FVector(0, 0, 100), 
				FString::Printf(TEXT("%.2f"), CurrentStability), 
				GetOwner(), FColor::Blue, -1.0f, /*bDrawShadow=*/true, /*FontScale=*/1.0f);
		}

		// 6. 崩塌检测
		if (CurrentStability <= 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Stability] %s unstable! Collapsing..."), *GetOwner()->GetName());

			GetOwner()->Destroy();
		}
		else
		{
			// 7. 传播：通知邻居重新计算
			for (TArray<UOrionStructureComponent*> Neighbors = BuildingManager->GetConnectedNeighbors(GetOwner()); UOrionStructureComponent* Neighbor : Neighbors)
			{
				// 递归调用
				Neighbor->UpdateStability();
			}
		}
	}
}

void UOrionStructureComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (EndPlayReason == EEndPlayReason::Destroyed && !BIsPreviewStructure)
	{
		// 1. 获取邻居 (必须在 Unregister 之前获取，否则找不到)
		// 注意：此时我已经不能提供支撑了，但邻居还不知道
		TArray<UOrionStructureComponent*> Neighbors;
		if (BuildingManager)
		{
			Neighbors = BuildingManager->GetConnectedNeighbors(GetOwner());
			
			// 2. 立即从 Grid 移除我的插槽
			// 这样邻居在下一行重算时，就不会再扫描到我了
			BuildingManager->UnregisterSockets(GetOwner());
		}

		// [Fix] 关键：在通知邻居前，先将自己的稳定性归零！
		// 原因：因为使用的是物理重叠检测，邻居在 UpdateStability 时可能仍然能检测到我（物理碰撞体还没销毁）。
		// 如果我不归零，邻居会读到我旧的高稳定性数值，从而误判为"支撑依旧存在"，导致不进行连锁更新。
		CurrentStability = 0.0f;

		// 3. 强制邻居重算
		for (UOrionStructureComponent* Neighbor : Neighbors)
		{
			if (Neighbor && Neighbor->IsValidLowLevel())
			{
				// 邻居会发现少了一个支撑 (我)，从而算出更低的值
				Neighbor->UpdateStability();
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UOrionStructureComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UOrionStructureComponent::SocketsRegistryHandler() const
{
	const FVector StructureLocation = StructureMesh->GetComponentLocation();
	const FRotator StructureRotation = StructureMesh->GetComponentRotation();

	switch (OrionStructureType)
	{
	case EOrionStructure::BasicSquareFoundation: RegisterSquareFoundationSockets(StructureLocation, StructureRotation);
		break;
	case EOrionStructure::BasicRoof: RegisterSquareFoundationSockets(StructureLocation, StructureRotation);
		break;
	case EOrionStructure::BasicTriangleFoundation: RegisterTriangleFoundationSockets(
		StructureLocation, StructureRotation);
		break;
	case EOrionStructure::Wall: RegisterWallSockets(StructureLocation, StructureRotation);
		break;
	case EOrionStructure::DoubleWall: RegisterDoubleWallSockets(StructureLocation, StructureRotation);
		break;
	default: break;
	}
}

void UOrionStructureComponent::RegisterSquareFoundationSockets(const FVector& StructureLocation,
                                                               const FRotator& StructureRotation) const
{
	const FVector SquareFoundationBound = GetStructureBounds(EOrionStructure::BasicSquareFoundation);
	const FVector WallBound = GetStructureBounds(EOrionStructure::Wall);

	BuildingManager->RegisterSocket(StructureLocation, StructureRotation, EOrionStructure::BasicSquareFoundation,
	                                true, GetWorld(), GetOwner());

	/* SquareFoundation -> SquareFoundation */
	static const FVector SquareFoundation2SquareFoundationLocationOffset[4] = {
		{SquareFoundationBound.X * 2, 0.f, 0.f},
		{0.f, -SquareFoundationBound.Y * 2, 0.f},
		{-SquareFoundationBound.X * 2, 0.f, 0.f},
		{0.f, SquareFoundationBound.Y * 2, 0.f}
	};
	for (const FVector& Offset : SquareFoundation2SquareFoundationLocationOffset)
	{
		BuildingManager->RegisterSocket(StructureLocation + StructureRotation.RotateVector(Offset), StructureRotation,
		                                EOrionStructure::BasicSquareFoundation, false, GetWorld(), GetOwner());
	}

	/* SquareFoundation -> TriangleFoundation */
	{
		static const FVector SquareFoundation2TriFoundationLocationOffset[4] = {
			{SquareFoundationBound.X + SquareFoundationBound.X * (1 / FMath::Sqrt(3.f)), 0.f, 0.f},
			{-SquareFoundationBound.X - SquareFoundationBound.X * (1 / FMath::Sqrt(3.f)), 0.f, 0.f},
			{0.f, SquareFoundationBound.Y + SquareFoundationBound.Y * (1 / FMath::Sqrt(3.f)), 0.f},
			{0.f, -SquareFoundationBound.Y - SquareFoundationBound.Y * (1 / FMath::Sqrt(3.f)), 0.f}
		};
		static constexpr float SquareFoundation2TriFoundationRotationOffset[4] = {180.f, 0.f, 270.f, 90.f};

		for (int32 i = 0; i < 4; ++i)
		{
			BuildingManager->RegisterSocket(
				StructureLocation + StructureRotation.RotateVector(SquareFoundation2TriFoundationLocationOffset[i]),
				StructureRotation + FRotator(0.f, SquareFoundation2TriFoundationRotationOffset[i], 0.f),
				EOrionStructure::BasicTriangleFoundation,
				/*bOccupied=*/false, GetWorld(), GetOwner());
		}
	}

	/* SquareFoundation -> Wall */
	static const FVector SquareFoundation2WallLocationOffset[4] = {
		{SquareFoundationBound.X, 0.f, SquareFoundationBound.Z + WallBound.Z},
		{0.f, SquareFoundationBound.Y, SquareFoundationBound.Z + WallBound.Z},
		{-SquareFoundationBound.X, 0.f, SquareFoundationBound.Z + WallBound.Z},
		{0.f, -SquareFoundationBound.Y, SquareFoundationBound.Z + WallBound.Z}
	};
	static const FRotator SquareFoundation2WallRotationOffset[4] = {
		{0, 0, 0},
		{0, 90, 0},
		{0, 180, 0},
		{0, 270, 0}
	};
	static const FVector SquareFoundation2WallScaleOffset[4] = {
		{1.0f, 1.0f, 1.0f},
		{1.0f, 1.0f, 1.0f},
		{1.0f, 1.0f, 1.0f},
		{1.0f, 1.0f, 1.0f},

	};
	for (int32 i = 0; i < 4; ++i)
	{
		BuildingManager->RegisterSocket(
			StructureLocation + StructureRotation.RotateVector(SquareFoundation2WallLocationOffset[i]),
			StructureRotation + SquareFoundation2WallRotationOffset[i],
			EOrionStructure::Wall, false, GetWorld(), GetOwner(), SquareFoundation2WallScaleOffset[i]);
	}
}

void UOrionStructureComponent::RegisterTriangleFoundationSockets(const FVector& StructureLocation,
                                                                 const FRotator& StructureRotation) const
{
	const float TriEdgeLength = 2.f * GetStructureBounds(EOrionStructure::BasicTriangleFoundation).Y;
	const FVector TriFoundationBound = GetStructureBounds(EOrionStructure::BasicTriangleFoundation);
	const FVector WallBound = GetStructureBounds(EOrionStructure::Wall);

	BuildingManager->RegisterSocket(
		StructureLocation, StructureRotation,
		EOrionStructure::BasicTriangleFoundation,
		/*bOccupied=*/true, GetWorld(), GetOwner());

	/* TriangleFoundation -> TriangleFoundation */
	const FVector TriFoundation2TriFoundationLocationOffset[3] = {
		{TriEdgeLength / FMath::Sqrt(3.0f), 0.f, 0.f},
		{-TriEdgeLength / (2.f * FMath::Sqrt(3.0f)), TriEdgeLength * 0.5f, 0.f},
		{-TriEdgeLength / (2.f * FMath::Sqrt(3.0f)), -TriEdgeLength * 0.5f, 0.f}
	};

	for (int32 i = 0; i < 3; ++i)
	{
		constexpr float TriFoundation2TriFoundationRotationOffset[3] = {180.f, 180.f, 180.f};
		const FVector EachTF2TFLocationOffset = StructureLocation + StructureRotation.RotateVector(
			TriFoundation2TriFoundationLocationOffset[i]);


		BuildingManager->RegisterSocket(
			EachTF2TFLocationOffset,
			StructureRotation + FRotator(0.f, TriFoundation2TriFoundationRotationOffset[i], 0.f),
			EOrionStructure::BasicTriangleFoundation,
			/*bOccupied=*/false, GetWorld(), GetOwner());
	}

	/* TriangleFoundation -> SquareFoundation */
	const FVector TriFoundation2SquareFoundationLocationOffset[3] = {
		{(FMath::Sqrt(3.0f) + 1.f) / (2.f * FMath::Sqrt(3.0f)) * TriEdgeLength, 0.f, 0.f},
		{
			-(0.25f + FMath::Sqrt(3.0f) / 12.f) * TriEdgeLength,
			-(0.25f + FMath::Sqrt(3.0f) / 4.f) * TriEdgeLength, 0.f
		},

		{
			-(0.25f + FMath::Sqrt(3.0f) / 12.f) * TriEdgeLength,
			(0.25f + FMath::Sqrt(3.0f) / 4.f) * TriEdgeLength, 0.f
		},

	};

	for (int32 i = 0; i < 3; ++i)
	{
		const FVector EachTF2SFLocationOffset = StructureLocation + StructureRotation.RotateVector(
			TriFoundation2SquareFoundationLocationOffset[i]);

		constexpr float TriFoundation2SquareFoundationRotationOffset[3] = {0.f, 60.f, 120.f};
		BuildingManager->RegisterSocket(
			EachTF2SFLocationOffset,
			StructureRotation + FRotator(0.f, TriFoundation2SquareFoundationRotationOffset[i], 0.f),
			EOrionStructure::BasicSquareFoundation,
			/*bOccupied=*/false, GetWorld(), GetOwner());
	}

	const FVector TriangleEdgeMidpoints[3] = {
		{TriEdgeLength * (1.f / (4.f * (FMath::Sqrt(3.0f) / 2.f))), 0.f, 0.f},
		{-1.f / (4.f * FMath::Sqrt(3.0f)) * TriEdgeLength, 0.25f * TriEdgeLength, 0.f},
		{-1.f / (4.f * FMath::Sqrt(3.0f)) * TriEdgeLength, -0.25f * TriEdgeLength, 0.f},

	};

	/* TriangleFoundation -> Wall */
	for (int32 i = 0; i < 3; ++i)
	{
		const FVector TriFoundation2WallLocationOffset =
			StructureLocation + StructureRotation.RotateVector(
				TriangleEdgeMidpoints[i] + FVector(0.f, 0.f, TriFoundationBound.Z + WallBound.Z));
		constexpr float TriFoundation2WallRotationOffset[3] = {180.f, 300.f, 60.f};

		BuildingManager->RegisterSocket(
			TriFoundation2WallLocationOffset,
			StructureRotation + FRotator(0.f, TriFoundation2WallRotationOffset[i], 0.f),
			EOrionStructure::Wall,
			/*bOccupied=*/false, GetWorld(), GetOwner());
	}
}

void UOrionStructureComponent::RegisterWallSockets(const FVector& StructureLocation,
                                                   const FRotator& StructureRotation) const
{
	const FVector WallBound = GetStructureBounds(EOrionStructure::Wall);
	const FVector TriangleFoundationBound = GetStructureBounds(EOrionStructure::BasicTriangleFoundation);
	const float TriEdgeLength = 2.f * GetStructureBounds(EOrionStructure::BasicTriangleFoundation).Y;

	BuildingManager->RegisterSocket(StructureLocation, StructureRotation, EOrionStructure::Wall,
	                                true, GetWorld(), GetOwner());

	/* Wall -> Wall */
	const FVector Wall2WallLocationOffset = StructureLocation + FVector(0.f, 0.f, WallBound.Z * 2.f);

	BuildingManager->RegisterSocket(Wall2WallLocationOffset, StructureRotation, EOrionStructure::Wall,
	                                false, GetWorld(), GetOwner());

	/* Wall -> Floor (SquareFoundation) */
	const FTransform ParentTM(StructureRotation, StructureLocation);

	const FVector FrontLocal(-WallBound.Y, 0.f, WallBound.Z);
	const FVector RearLocal(WallBound.Y, 0.f, WallBound.Z);

	const FVector FrontWorld = ParentTM.TransformPositionNoScale(FrontLocal);
	const FVector RearWorld = ParentTM.TransformPositionNoScale(RearLocal);

	BuildingManager->RegisterSocket(
		FrontWorld, StructureRotation,
		EOrionStructure::BasicSquareFoundation,
		false, GetWorld(), GetOwner());

	BuildingManager->RegisterSocket(
		RearWorld, StructureRotation,
		EOrionStructure::BasicSquareFoundation,
		false, GetWorld(), GetOwner());

	/* Wall -> TriangleFloor (TriangleFoundation) */
	const FVector FrontLocalTri(TriEdgeLength / (2.0 * FMath::Sqrt(3.f)), 0.f, WallBound.Z);
	const FVector RearLocalTri(-TriEdgeLength / (2.0 * FMath::Sqrt(3.f)), 0.f, WallBound.Z);

	const FVector FrontWorldTri = ParentTM.TransformPositionNoScale(FrontLocalTri);
	const FVector RearWorldTri = ParentTM.TransformPositionNoScale(RearLocalTri);

	BuildingManager->RegisterSocket(
		FrontWorldTri, StructureRotation + FRotator(0.f, 180.f, 0.f),
		EOrionStructure::BasicTriangleFoundation,
		false, GetWorld(), GetOwner());

	BuildingManager->RegisterSocket(
		RearWorldTri, StructureRotation,
		EOrionStructure::BasicTriangleFoundation,
		false, GetWorld(), GetOwner());
}


void UOrionStructureComponent::RegisterDoubleWallSockets(const FVector& StructureLocation,
                                                         const FRotator& StructureRotation) const
{
	{
		const FVector DoubleWallBound = GetStructureBounds(EOrionStructure::DoubleWall);
		const FVector SquareFoundationBound = GetStructureBounds(EOrionStructure::BasicSquareFoundation);

		/* DoubleWall -> Wall */
		const FVector DoubleWall2WallLocationOffset[2] = {
			StructureLocation + FVector(0.f, 0.f, DoubleWallBound.Z * 2.f),
			StructureLocation + FVector(0.f, DoubleWallBound.Y, DoubleWallBound.Z * 2.f)
		};

		for (int32 i = 0; i < 2; ++i)
		{
			BuildingManager->RegisterSocket(
				DoubleWall2WallLocationOffset[i],
				StructureRotation,
				EOrionStructure::Wall,
				false,
				GetWorld(),
				GetOwner()
			);
		}

		/* DoubleWall -> SquareRoof (SquareFoundation) */
		const FVector DoubleWall2SquareFoundationOffset[4] = {
			StructureLocation + FVector(SquareFoundationBound.X, 0.f, DoubleWallBound.Z),
			StructureLocation + FVector(-SquareFoundationBound.X, 0.f, DoubleWallBound.Z),
			StructureLocation + FVector(SquareFoundationBound.X, DoubleWallBound.Y, DoubleWallBound.Z),
			StructureLocation + FVector(-SquareFoundationBound.X, DoubleWallBound.Y, DoubleWallBound.Z),
		};

		for (int32 i = 0; i < 4; ++i)
		{
			BuildingManager->RegisterSocket(
				DoubleWall2SquareFoundationOffset[i],
				StructureRotation,
				EOrionStructure::BasicSquareFoundation,
				false,
				GetWorld(),
				GetOwner()
			);
		}
	}
}

FVector UOrionStructureComponent::GetStructureBounds(const EOrionStructure Type)
{
	if (const FVector* BoundVector = StructureBoundMap.Find(Type))
	{
		return *BoundVector;
	}
	UE_LOG(LogTemp, Error, TEXT("[Error] StructureBounds not found for type %d! Returning ZeroVector. This will cause sockets to spawn at actor center!"), (int32)Type);
	return FVector::ZeroVector;
}

bool UOrionStructureComponent::CheckIsTouchingGround() const
{
	AActor* Owner = GetOwner();
	if (!Owner) return false;

	FVector Start = Owner->GetActorLocation();
	// 向下探测：长度根据地基厚度调整，通常 100cm 足够穿透到地面
	FVector End = Start - FVector(0.f, 0.f, 100.f);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Owner); // 忽略自己
	Params.bTraceComplex = false;

	UWorld* World = GetWorld();
	if (!World) return false;

	// 使用 WorldStatic 通道进行检测（地形通常是 WorldStatic）
	bool bHit = World->LineTraceSingleByChannel(
		Hit,
		Start,
		End,
		ECC_WorldStatic,
		Params
	);

	if (bHit)
	{
		// 关键判断：虽然撞到了东西，但如果撞到的是"另一个建筑"，说明我在二楼 -> 不是锚点
		if (Hit.GetActor() && Hit.GetActor()->FindComponentByClass<UOrionStructureComponent>())
		{
			return false;
		}

		// 撞到了非建筑物体（通常是 Landscape） -> 是锚点
		return true;
	}

	return false; // 悬空 -> 不是锚点
}
