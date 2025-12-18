// Fill out your copyright notice in the Description page of Project Settings.


#include "Orion/OrionGameInstance/OrionBuildingManager.h"
#include "Orion/OrionGameInstance/OrionGameInstance.h"
#include "Orion/OrionComponents/OrionStructureComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/DataTable.h"

// 硬编码数据（作为后备，当 DataTable 未指定时使用）
const TArray<FOrionDataBuilding>& UOrionBuildingManager::GetHardcodedBuildings() const
{
	static const TArray<FOrionDataBuilding> HardcodedBuildings = {
	{
		1, FName(TEXT("SquareFoundation")),
		FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/SquareFoundationSnapshot.SquareFoundationSnapshot")),
		FString(TEXT(
			"/Game/_Orion/Blueprints/Buildings/BP_OrionStructureSquareFoundation.BP_OrionStructureSquareFoundation_C")),
		EOrionStructure::BasicSquareFoundation
	},
	{
		2, FName(TEXT("TriangleFoundation")),
		FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/TriangleFoundationSnapshot.TriangleFoundationSnapshot")),
		FString(TEXT(
			"/Game/_Orion/Blueprints/Buildings/BP_OrionStructureTriangleFoundation.BP_OrionStructureTriangleFoundation_C")),
		EOrionStructure::BasicTriangleFoundation
	},
	{
		3, FName(TEXT("Wall")),
		FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/WallSnapshot.WallSnapshot")),
		FString(TEXT("/Game/_Orion/Blueprints/Buildings/BP_OrionStructureWall.BP_OrionStructureWall_C")),
		EOrionStructure::Wall
	},
	{
		4, FName(TEXT("DoubleWallDoor")),
		FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/DoubleWallSnapshot.DoubleWallSnapshot")),
		FString(TEXT("/Game/_Orion/Blueprints/Buildings/BP_OrionStructureDoubleWall.BP_OrionStructureDoubleWall_C")),
		EOrionStructure::DoubleWall
	},
	/*{
		5, FName(TEXT("BasicRoof")),
		FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/BasicRoofSnapShot.BasicRoofSnapShot")),
		FString(TEXT("/Game/_Orion/Blueprints/Buildings/BP_OrionStructureBasicRoof.BP_OrionStructureBasicRoof_C")),
		EOrionStructure::BasicRoof
	},*/
	{
		6, FName(TEXT("OrionActorOre")),
		FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/OrionActorOreSnapshot.OrionActorOreSnapshot")),
		FString(TEXT("/Game/_Orion/Blueprints/BP_OrionActorOre.BP_OrionActorOre_C")),
		EOrionStructure::None
	},
	{
		7, FName(TEXT("OrionActorProduction")),
		FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/OrionActorProductionSnapshot.OrionActorProductionSnapshot")),
		FString(TEXT("/Game/_Orion/Blueprints/BP_OrionActorProduction.BP_OrionActorProduction_C")),
		EOrionStructure::None
	},
	{
		8, FName(TEXT("OrionActorStorage")),
		FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/OrionActorStorageSnapshot.OrionActorStorageSnapshot")),
		FString(TEXT("/Game/_Orion/Blueprints/BP_OrionActorStorage.BP_OrionActorStorage_C")),
		EOrionStructure::None
	},
	{
		9, FName(TEXT("InclinedRoof")),
		FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/InclinedRoofSnapshot.InclinedRoofSnapshot")),
		FString(TEXT("/Game/_Orion/Blueprints/Buildings/BP_OrionStructureInclinedRoof.BP_OrionStructureInclinedRoof_C")),
		EOrionStructure::InclinedRoof
	},
		{
		10, FName(TEXT("OrionContainer")),
		FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/OrionContainerSnapshot.OrionContainerSnapshot")),
		FString(TEXT("/Game/_Orion/Blueprints/Props/BP_OrionContainer.BP_OrionContainer_C")),
		EOrionStructure::None
	},
	};
	return HardcodedBuildings;
}

// 向后兼容的静态成员（已废弃，仅用于向后兼容）
const TArray<FOrionDataBuilding> UOrionBuildingManager::OrionDataBuildings = []()
{
	// 注意：这个静态成员在类实例化之前初始化，无法访问 DataTable
	// 因此只返回硬编码数据，实际使用应通过 GetOrionDataBuildings() 方法
	static const TArray<FOrionDataBuilding> StaticBuildings = {
		{
			1, FName(TEXT("SquareFoundation")),
			FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/SquareFoundationSnapshot.SquareFoundationSnapshot")),
			FString(TEXT(
				"/Game/_Orion/Blueprints/Buildings/BP_OrionStructureSquareFoundation.BP_OrionStructureSquareFoundation_C")),
			EOrionStructure::BasicSquareFoundation
		},
		{
			2, FName(TEXT("TriangleFoundation")),
			FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/TriangleFoundationSnapshot.TriangleFoundationSnapshot")),
			FString(TEXT(
				"/Game/_Orion/Blueprints/Buildings/BP_OrionStructureTriangleFoundation.BP_OrionStructureTriangleFoundation_C")),
			EOrionStructure::BasicTriangleFoundation
		},
		{
			3, FName(TEXT("Wall")),
			FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/WallSnapshot.WallSnapshot")),
			FString(TEXT("/Game/_Orion/Blueprints/Buildings/BP_OrionStructureWall.BP_OrionStructureWall_C")),
			EOrionStructure::Wall
		},
		{
			4, FName(TEXT("DoubleWallDoor")),
			FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/DoubleWallSnapshot.DoubleWallSnapshot")),
			FString(TEXT("/Game/_Orion/Blueprints/Buildings/BP_OrionStructureDoubleWall.BP_OrionStructureDoubleWall_C")),
			EOrionStructure::DoubleWall
		},
		{
			6, FName(TEXT("OrionActorOre")),
			FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/OrionActorOreSnapshot.OrionActorOreSnapshot")),
			FString(TEXT("/Game/_Orion/Blueprints/BP_OrionActorOre.BP_OrionActorOre_C")),
			EOrionStructure::None
		},
		{
			7, FName(TEXT("OrionActorProduction")),
			FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/OrionActorProductionSnapshot.OrionActorProductionSnapshot")),
			FString(TEXT("/Game/_Orion/Blueprints/BP_OrionActorProduction.BP_OrionActorProduction_C")),
			EOrionStructure::None
		},
		{
			8, FName(TEXT("OrionActorStorage")),
			FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/OrionActorStorageSnapshot.OrionActorStorageSnapshot")),
			FString(TEXT("/Game/_Orion/Blueprints/BP_OrionActorStorage.BP_OrionActorStorage_C")),
			EOrionStructure::None
		},
		{
			9, FName(TEXT("InclinedRoof")),
			FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/InclinedRoofSnapshot.InclinedRoofSnapshot")),
			FString(TEXT("/Game/_Orion/Blueprints/Buildings/BP_OrionStructureInclinedRoof.BP_OrionStructureInclinedRoof_C")),
			EOrionStructure::InclinedRoof
		},
		{
			10, FName(TEXT("OrionContainer")),
			FString(TEXT("/Game/_Orion/UI/BuildingSnapshots/OrionContainerSnapshot.OrionContainerSnapshot")),
			FString(TEXT("/Game/_Orion/Blueprints/Props/BP_OrionContainer.BP_OrionContainer_C")),
			EOrionStructure::None
		},
	};
	return StaticBuildings;
}();

const TMap<int32, FOrionDataBuilding> UOrionBuildingManager::OrionDataBuildingsMap = []()
{
	TMap<int32, FOrionDataBuilding> Map;
	for (const FOrionDataBuilding& Data : OrionDataBuildings)
	{
		Map.Add(Data.BuildingId, Data);
	}
	return Map;
}();

const TMap<EOrionStructure, FVector> UOrionBuildingManager::StructureOriginalScaleMap = {
	{EOrionStructure::BasicSquareFoundation, FVector(1.25f, 1.25f, 1.0f)},
	{EOrionStructure::BasicTriangleFoundation, FVector(1.25f, 1.25f, 1.25f)},
	{EOrionStructure::Wall, FVector(1.0f, 1.25f, 1.0f)},
	{EOrionStructure::DoubleWall, FVector(1.0f, 1.25f, 1.0f)},
	{EOrionStructure::BasicRoof, FVector(1.25f, 1.25f, 1.0f)},
	{EOrionStructure::InclinedRoof, FVector(1.25f, 1.25f, 1.0f)},
};

UOrionBuildingManager::UOrionBuildingManager()
{

}

void UOrionBuildingManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 从 GameInstance 获取 DataTable 配置
	if (UOrionGameInstance* GameInstance = Cast<UOrionGameInstance>(GetGameInstance()))
	{
		BuildingDataTable = GameInstance->BuildingDataTable;
	}

	// 加载建筑数据（优先从 DataTable，否则使用硬编码数据）
	LoadBuildingDataFromDataTable();

	FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UOrionBuildingManager::OnWorldInitializedActors);
}

void UOrionBuildingManager::OnWorldInitializedActors(const FActorsInitializedParams& ActorsInitializedParams)
{
	if (!GetWorld())
	{
		UE_LOG(LogTemp, Error, TEXT("UOrionBuildingManager::OnWorldInitializedActors: World is null."));
		return;
	}

	BuildingObjectsPool = MakeUnique<FBuildingObjectsPool>(this);
}

void UOrionBuildingManager::ResetAllSockets(const UWorld* World)
{
	SpatialGrid.Empty();
	SocketsByKind.Empty();
	UKismetSystemLibrary::FlushPersistentDebugLines(World);
}

// Get Grid Key (Simple int32 merge to int64)
int64 UOrionBuildingManager::GetGridKey(const FVector& Location)
{
	const int32 X = FMath::FloorToInt(Location.X / GridCellSize);
	const int32 Y = FMath::FloorToInt(Location.Y / GridCellSize);
	return (static_cast<int64>(X) << 32) | static_cast<uint32>(Y); // High 32 bits store X, Low 32 bits store Y
}


bool UOrionBuildingManager::ConfirmPlaceStructure(
	TSubclassOf<AActor> BPClass,
	AActor* PreviewPtr,  // Read-only: Only used to get Transform, not destroyed
	bool bSnapped,
	const FTransform& SnapTransform,
	AActor* ParentActor)
{
	if (!PreviewPtr || !BPClass) { return false; }

	/* 1. Preview actor must have structure component */
	if (UOrionStructureComponent* Comp =
			PreviewPtr->FindComponentByClass<UOrionStructureComponent>();
		!Comp)
	{
		return false;
	}

	if (const UOrionStructureComponent* Comp =
		PreviewPtr->FindComponentByClass<UOrionStructureComponent>())
	{
		if (Comp->bForceSnapOnGrid && !bSnapped)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("[Building] Structure must snap to socket before placement."));
			return false;
		}
	}

	UWorld* World = PreviewPtr->GetWorld();
	if (!World) { return false; }

	/* 2. Calculate final target transform */
	// If snapped, use SnapTransform; if not snapped, use PreviewPtr's current Transform
	const FTransform TargetTransform =
		bSnapped
			? SnapTransform
			: PreviewPtr->GetActorTransform();

	/*const FVector PreviewScale = PreviewPtr->GetActorScale3D();
	UE_LOG(LogTemp, Log, TEXT("[Building] PreviewPtr->GetActorScale3D() = (%.3f, %.3f, %.3f)"), PreviewScale.X,
	       PreviewScale.Y, PreviewScale.Z);
	const FVector SnapLoc = SnapTransform.GetLocation();
	const FRotator SnapRot = SnapTransform.GetRotation().Rotator();
	const FVector SnapScale = SnapTransform.GetScale3D();
	UE_LOG(LogTemp, Log,
	       TEXT("[Building] SnapTransform: Loc=(%.3f, %.3f, %.3f) Rot=(%.3f, %.3f, %.3f) Scale=(%.3f, %.3f, %.3f)"),
	       SnapLoc.X, SnapLoc.Y, SnapLoc.Z,
	       SnapRot.Pitch, SnapRot.Yaw, SnapRot.Roll,
	       SnapScale.X, SnapScale.Y, SnapScale.Z);*/


	/* 3. Placeholder collision check (with tolerance + debug draw) ------------------ */
	bool bBlocked = false;

	if (const UPrimitiveComponent* RootPrim =
		Cast<UPrimitiveComponent>(PreviewPtr->GetRootComponent()))
	{
		const FBoxSphereBounds Bounds = RootPrim->CalcBounds(TargetTransform);
		const FVector ExtentFull = Bounds.BoxExtent;
		const FVector Center = Bounds.Origin;

		/* -- 3-A. Light tolerance: shrink box by 2 cm -- */
		constexpr float ToleranceCm = 100.f; // <- Tolerance
		const FVector ExtentLoose = (ExtentFull - FVector(ToleranceCm))
			.ComponentMax(FVector(1.f)); // Prevent negative value

		/* -- 3-B. Strict check first, then double check with loose box -- */
		FCollisionShape ShapeStrict = FCollisionShape::MakeBox(ExtentFull);
		FCollisionShape ShapeLoose = FCollisionShape::MakeBox(ExtentLoose);

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlacementTest), /*TraceComplex=*/false);
		QueryParams.AddIgnoredActor(PreviewPtr);

		const bool bBlockedStrict = World->OverlapBlockingTestByChannel(
			Center, TargetTransform.GetRotation(), ECC_WorldStatic, ShapeStrict, QueryParams);

		const bool bBlockedLoose = World->OverlapBlockingTestByChannel(
			Center, TargetTransform.GetRotation(), ECC_WorldStatic, ShapeLoose, QueryParams);

		/* -- 3-C. Only consider blocked if both "Strict" and "Loose" are blocked -- */
		bBlocked = bBlockedStrict && bBlockedLoose;

		/* -- 3-D. Debug Output -- */
		UE_LOG(LogTemp, Verbose,
		       TEXT(
			       "[Building][Debug] Center=(%.1f,%.1f,%.1f)  ExtentFull=(%.1f,%.1f,%.1f)  BlockedStrict=%d  BlockedLoose=%d"
		       ),
		       Center.X, Center.Y, Center.Z,
		       ExtentFull.X, ExtentFull.Y, ExtentFull.Z,
		       bBlockedStrict, bBlockedLoose);

		DrawDebugBox(World, Center, ExtentFull,
		             TargetTransform.GetRotation(),
		             bBlocked ? FColor::Red : FColor::Green,
		             /*PersistentLines=*/false, /*LifeTime=*/2.f);
	}

	if (bBlocked)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("[Building] Placement blocked - cannot spawn structure here."));
		return false;
	}

	// -----------------------------------------------------------------------
	// 【Core Change】: Removed PreviewPtr->Destroy() and PreviewPtr = nullptr;
	// Preview actor is now safe, we don't touch it.
	// -----------------------------------------------------------------------

	/* 4. Directly spawn new "Real" building */
	// Note: No need to pass bSnapped to Spawn function anymore, as Transform is already determined
	bool bSpawnSuccess = false;
	
	// Reuse existing spawn logic, but slightly adjust parameter passing
	// We pass bSnapped state for subsequent logic (like socket logic)
	DelaySpawnNewStructure(BPClass, World, TargetTransform, bSpawnSuccess, bSnapped, ParentActor);

	return bSpawnSuccess;
}


bool UOrionBuildingManager::DelaySpawnNewStructure(const TSubclassOf<AActor> BPClass, UWorld* World,
                                                   const FTransform& TargetTransform, bool& DelaySpawnNewStructureRes, bool bSnapped, AActor* ParentActor)
{
	/* 4. Officially Spawn Actor --------------------------------------- */
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (AActor* NewlySpawnedActor = World->SpawnActor<AActor>(BPClass, TargetTransform, P); !NewlySpawnedActor)
	{
		UE_LOG(LogTemp, Error,
		       TEXT("[Building] SpawnActor failed (class: %s)."), *BPClass->GetName());
		DelaySpawnNewStructureRes = false;
		return true;
	}
	else
	{
		/* Here the actor has done spawning, which means transforms made bellow won't affect socket registration */
		NewlySpawnedActor->SetActorScale3D(TargetTransform.GetScale3D());
		
		// Set entity properties (Entity needs collision and visibility)
		NewlySpawnedActor->SetActorEnableCollision(true); // Entity must have collision
		NewlySpawnedActor->SetActorHiddenInGame(false);   // Entity must be visible


		if (UOrionStructureComponent* StructureComp = NewlySpawnedActor->FindComponentByClass<UOrionStructureComponent>())
		{
			StructureComp->BIsPreviewStructure = false; // This is a real entity

			// [Fix] Force re-register sockets
			// Reason: BeginPlay might run before Transform update during SpawnActor, causing sockets to register at (0,0,0)
			// Here we ensure Actor is in correct position, unregister old ones (if any), then register new ones.
			this->UnregisterSockets(NewlySpawnedActor);
			StructureComp->SocketsRegistryHandler();


			// Calculate stability, triggers Destroy() automatically if 0
			StructureComp->UpdateStability();

			// [Fix] Check if object is alive (IsValid checks if marked PendingKill)
			// And check if stability is greater than 0
			if (IsValid(NewlySpawnedActor) && StructureComp->CurrentStability > 0.0f)
			{
				DelaySpawnNewStructureRes = true; // Placement successful and alive
			}
			else
			{
				// Object unstable, collapse triggered (also considered placement successful as return true)
				UE_LOG(LogTemp, Warning, TEXT("[Building] Structure placed but collapsed immediately due to lack of support."));
				DelaySpawnNewStructureRes = true;
			}
		}
		else
		{
			// No component, treat as normal object placement success
			DelaySpawnNewStructureRes = true;
		}
	}

	return true; // DelaySpawnNewStructure execution itself had no errors
}

// Core: Query + Filter
bool UOrionBuildingManager::FindNearestSocket(const FVector& QueryPos, const float SearchRadius, const EOrionStructure Type, FOrionGlobalSocket& OutSocket) const
{
	float MinDistSqr = SearchRadius * SearchRadius;
	constexpr float BlockThresholdSqr = 5.0f * 5.0f; // Blocked if Occupied within 5cm
	const FOrionGlobalSocket* BestCandidate = nullptr;
	
	// 1. Determine query range (3x3 Grid)
	const int64 CenterKey = GetGridKey(QueryPos);
	const int32 CenterX = CenterKey >> 32;
	const int32 CenterY = static_cast<int32>(CenterKey);
	
	// List 1: Candidates (Type matches, I can snap to)
	TArray<const FOrionGlobalSocket*> LocalCandidates;
	// List 2: Potential blockers (All sockets in area, regardless of type)
	TArray<const FOrionGlobalSocket*> AllNearbySockets; 
	
	for (int32 x = -1; x <= 1; ++x)
	{
		for (int32 y = -1; y <= 1; ++y)
		{
			const int64 Key = (static_cast<int64>(CenterX + x) << 32) | static_cast<uint32>(CenterY + y);
			if (const FOrionSocketBucket* Bucket = SpatialGrid.Find(Key))
			{
				for (const FOrionGlobalSocket& S : Bucket->Sockets)
				{
					// Collect all sockets for blockage detection
					AllNearbySockets.Add(&S);

					// Only collect matching types for snapping
					if (S.Kind == Type) 
					{
						LocalCandidates.Add(&S);
					}
				}
			}
		}
	}
	
	// 2. Iterate candidates, find nearest & unblocked
	for (const FOrionGlobalSocket* S : LocalCandidates)
	{
		// Distance pre-filter
		const float DistSqr = FVector::DistSquared(S->Location, QueryPos);
		if (DistSqr > MinDistSqr) continue;
		
		// === The Filter Logic ===
		// Check if any "Occupied" socket exists near this location (S->Location)?
		// If so, it means although there is a free socket (from foundation), it's blocked by a wall
		bool bIsBlocked = false;
		
		// If socket itself is Occupied, it definitely can't be used
		if (S->bOccupied) 
		{
			bIsBlocked = true;
		}
		else 
		{
			// [Fix] Look for blockers in AllNearbySockets (Full Set), not just in Candidates
			for (const FOrionGlobalSocket* Blocker : AllNearbySockets)
			{
				if (Blocker->bOccupied && 
					FVector::DistSquared(Blocker->Location, S->Location) < BlockThresholdSqr)
				{
					bIsBlocked = true; // Blocker found
					break;
				}
			}
		}
		
		// 3. If unblocked and closer, select it
		// [Safe] Also check if Owner is still valid
		if (!bIsBlocked && S->Owner.IsValid())
		{
			MinDistSqr = DistSqr;
			BestCandidate = const_cast<FOrionGlobalSocket*>(S); // Cast away const
		}
	}
	
	if (BestCandidate && BestCandidate->Owner.IsValid())
	{
		OutSocket = *BestCandidate;
		return true;
	}
	return false;
}

const TArray<FOrionGlobalSocket>& UOrionBuildingManager::GetSnapSocketsByKind(const EOrionStructure Kind) const
{
	if (const TArray<FOrionGlobalSocket>* Bucket = SocketsByKind.Find(Kind))
	{
		return *Bucket;
	}
	static const TArray<FOrionGlobalSocket> Empty;
	return Empty;
}

void UOrionBuildingManager::RegisterSocket(const FVector& Loc, const FRotator& Rot, const EOrionStructure Kind,
                                           const bool IsOccupied, const UWorld* World, AActor* Owner, const FVector& Scale)
{
	if (!World)
	{
		return;
	}

	// [Grid] Register directly to grid, no deduplication
	const FOrionGlobalSocket NewSocket(Loc, Rot, Kind, IsOccupied, Owner, Scale);
	const int64 Key = GetGridKey(Loc);
	SpatialGrid.FindOrAdd(Key).Sockets.Add(NewSocket);

	// Compatible with old system: Update SocketsByKind simultaneously (Used for GetSnapSocketsByKind)
	SocketsByKind.FindOrAdd(Kind).Add(NewSocket);

	// Special handling: Wall also registers DoubleWall, SquareFoundation also registers BasicRoof
	if (Kind == EOrionStructure::Wall)
	{
		const FOrionGlobalSocket DoubleWallSocket(Loc, Rot, EOrionStructure::DoubleWall, IsOccupied, Owner, Scale);
		SpatialGrid.FindOrAdd(Key).Sockets.Add(DoubleWallSocket);
		SocketsByKind.FindOrAdd(EOrionStructure::DoubleWall).Add(DoubleWallSocket);
	}
	else if (Kind == EOrionStructure::BasicSquareFoundation)
	{
		const FOrionGlobalSocket RoofSocket(Loc, Rot, EOrionStructure::BasicRoof, IsOccupied, Owner, Scale);
		SpatialGrid.FindOrAdd(Key).Sockets.Add(RoofSocket);
		SocketsByKind.FindOrAdd(EOrionStructure::BasicRoof).Add(RoofSocket);
	}
}

// Unregister: Find grid cell where Actor is located, remove sockets belonging to it
void UOrionBuildingManager::UnregisterSockets(AActor* Owner)
{
	if (!Owner) return;
	
	const UWorld* World = Owner->GetWorld();
	
	// [Fix] Deprecate Bounds-based cleanup, use full grid scan to prevent residuals due to Bounds calculation errors
	// This ensures absolute cleanliness, preventing ghost sockets
	TArray<int64> KeysToRemove;

	for (auto& Elem : SpatialGrid)
	{
		FOrionSocketBucket& Bucket = Elem.Value;
		
		int32 RemovedNum = Bucket.Sockets.RemoveAll([Owner](const FOrionGlobalSocket& S) {
			return S.Owner.Get() == Owner;
		});

		// If bucket is empty, mark Key for removal
		if (Bucket.Sockets.Num() == 0)
		{
			KeysToRemove.Add(Elem.Key);
		}
	}

	// Clean up empty buckets
	for (const int64 Key : KeysToRemove)
	{
		SpatialGrid.Remove(Key);
	}
	
	// Update SocketsByKind simultaneously (Old system compatibility)
	TArray<EOrionStructure> KindsToRemove;
	for (auto& Elem : SocketsByKind)
	{
		Elem.Value.RemoveAll([Owner](const FOrionGlobalSocket& S) {
			return S.Owner.Get() == Owner;
		});
		
		if (Elem.Value.Num() == 0)
		{
			KindsToRemove.Add(Elem.Key);
		}
	}
	
	for (const EOrionStructure Kind : KindsToRemove)
	{
		SocketsByKind.Remove(Kind);
	}
}

// [New] Get surrounding neighbors (Using physical overlap detection, bypassing Pivot offset issues)
TArray<UOrionStructureComponent*> UOrionBuildingManager::GetConnectedNeighbors(AActor* CenterStructure, bool bDebug) const
{
	TArray<UOrionStructureComponent*> Result;
	if (!CenterStructure) return Result;

	UWorld* World = GetWorld();
	if (!World) return Result;

	// -------------------------------------------------------
	// 1. Get StructureMesh (This is the most accurate geometry source)
	// -------------------------------------------------------
	UStaticMeshComponent* MeshComp = nullptr;
	if (auto* StructComp = CenterStructure->FindComponentByClass<UOrionStructureComponent>())
	{
		MeshComp = StructComp->StructureMesh;
	}
	// Double insurance: If no cache, try direct lookup
	if (!MeshComp) MeshComp = CenterStructure->FindComponentByClass<UStaticMeshComponent>();
	if (!MeshComp) return Result;

	// -------------------------------------------------------
	// 2. Calculate precise OABB (Oriented Bounding Box)
	// -------------------------------------------------------
	FVector LocalMin, LocalMax;
	MeshComp->GetLocalBounds(LocalMin, LocalMax);

	// Calculate geometric center in local coordinate system (Auto-correct Pivot offset)
	FVector LocalCenter = (LocalMin + LocalMax) * 0.5f;
	FVector LocalExtent = (LocalMax - LocalMin) * 0.5f;

	// Transform center point and rotation to world space
	FTransform CompTransform = MeshComp->GetComponentTransform();
	FVector WorldCenter = CompTransform.TransformPosition(LocalCenter);
	FQuat WorldRotation = CompTransform.GetRotation();

	// Calculate extended half-extent (Apply scale + 0.05m tolerance)
	// Note: GetLocalBounds returns unscaled size, must multiply by Scale
	FVector SearchExtent = LocalExtent * CompTransform.GetScale3D();
	SearchExtent += FVector(5.0f); // Extend 5cm in xyz

	// -------------------------------------------------------
	// 3. Execute physical overlap detection
	// -------------------------------------------------------
	TArray<FOverlapResult> Overlaps;
	FCollisionShape BoxShape = FCollisionShape::MakeBox(SearchExtent);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(CenterStructure); // Ignore self
	Params.bTraceComplex = false;            // Simple collision detection is more efficient

	// Use WorldStatic channel (Buildings are usually WorldStatic)
	World->OverlapMultiByChannel(
		Overlaps,
		WorldCenter,
		WorldRotation,
		ECC_WorldStatic,
		BoxShape,
		Params
	);

	// -------------------------------------------------------
	// 4. Debug Visualization
	// -------------------------------------------------------
	if (bDebug)
	{
		// Draw purple "Search Box": Show actual detection range (Lasts 0.1s, fits Tick)
		DrawDebugBox(World, WorldCenter, SearchExtent, WorldRotation, FColor::Purple, false, 0.1f, 0, 2.0f);
	
		// Draw small coordinate axis at geometric center, confirm center calculation correctness
		DrawDebugCoordinateSystem(World, WorldCenter, WorldRotation.Rotator(), 50.0f, false, 0.1f, 0, 1.0f);
	}

	// -------------------------------------------------------
	// 5. Filter results and "Highlight" neighbors
	// -------------------------------------------------------
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor || HitActor == CenterStructure) continue;

		// Confirm hit is a building with OrionStructureComponent
		if (UOrionStructureComponent* NeighborComp = HitActor->FindComponentByClass<UOrionStructureComponent>())
		{
			Result.AddUnique(NeighborComp);

			if (bDebug)
			{
				// [Highlight] Detected neighbor: Draw yellow box enclosing it
				if (UStaticMeshComponent* NeighborMesh = HitActor->FindComponentByClass<UStaticMeshComponent>())
				{
					FVector N_Min, N_Max;
					NeighborMesh->GetLocalBounds(N_Min, N_Max);
					FVector N_Center = NeighborMesh->GetComponentTransform().TransformPosition((N_Min + N_Max) * 0.5f);
					FVector N_Extent = (N_Max - N_Min) * 0.5f * NeighborMesh->GetComponentScale();
				
					// Draw yellow box indicating "Connected"
					DrawDebugBox(World, N_Center, N_Extent, NeighborMesh->GetComponentQuat(), FColor::Yellow, false, 0.1f, 0, 3.0f);
				}
			}
		}
	}

	// Print log for debugging
	if (bDebug && Result.Num() > 0)
	{
		// Limit log frequency to prevent Tick spam, simple output here
		// UE_LOG(LogTemp, Log, TEXT("[Structure] %s found %d neighbors via Overlap."), *CenterStructure->GetName(), Result.Num());
	}

	return Result;
}

// 从 DataTable 加载建筑数据
void UOrionBuildingManager::LoadBuildingDataFromDataTable()
{
	CachedBuildingData.Empty();
	CachedBuildingDataMap.Empty();

	// 如果指定了 DataTable，优先使用 DataTable 数据
	if (BuildingDataTable)
	{
		TArray<FOrionDataBuildingRow*> Rows;
		BuildingDataTable->GetAllRows<FOrionDataBuildingRow>(TEXT("LoadBuildingDataFromDataTable"), Rows);

		if (Rows.Num() > 0)
		{
			for (const FOrionDataBuildingRow* Row : Rows)
			{
				if (Row)
				{
					FOrionDataBuilding BuildingData = Row->ToDataBuilding();
					CachedBuildingData.Add(BuildingData);
					CachedBuildingDataMap.Add(BuildingData.BuildingId, BuildingData);
				}
			}

			UE_LOG(LogTemp, Log, TEXT("[BuildingManager] Loaded %d buildings from DataTable: %s"), 
				CachedBuildingData.Num(), *BuildingDataTable->GetName());
			bDataLoaded = true;
			return;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[BuildingManager] DataTable %s is empty, falling back to hardcoded data."), 
				*BuildingDataTable->GetName());
		}
	}

	// 如果未指定 DataTable 或 DataTable 为空，使用硬编码数据
	const TArray<FOrionDataBuilding>& HardcodedData = GetHardcodedBuildings();
	CachedBuildingData = HardcodedData;
	
	for (const FOrionDataBuilding& Data : HardcodedData)
	{
		CachedBuildingDataMap.Add(Data.BuildingId, Data);
	}

	UE_LOG(LogTemp, Log, TEXT("[BuildingManager] Using hardcoded building data (%d buildings)."), HardcodedData.Num());
	bDataLoaded = true;
}

// 获取建筑数据数组
const TArray<FOrionDataBuilding>& UOrionBuildingManager::GetOrionDataBuildings() const
{
	if (!bDataLoaded)
	{
		// 如果数据未加载，立即加载（这种情况不应该发生，但作为安全措施）
		const_cast<UOrionBuildingManager*>(this)->LoadBuildingDataFromDataTable();
	}
	return CachedBuildingData;
}

// 获取建筑数据映射
const TMap<int32, FOrionDataBuilding>& UOrionBuildingManager::GetOrionDataBuildingsMap() const
{
	if (!bDataLoaded)
	{
		// 如果数据未加载，立即加载（这种情况不应该发生，但作为安全措施）
		const_cast<UOrionBuildingManager*>(this)->LoadBuildingDataFromDataTable();
	}
	return CachedBuildingDataMap;
}
