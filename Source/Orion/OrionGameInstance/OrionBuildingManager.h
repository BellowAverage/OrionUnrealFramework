// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Orion/OrionSaveGame/OrionSaveGame.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "UObject/SoftObjectPath.h"


class AOrionStructure;
class AActor;
class UOrionStructureComponent;
class FBuildingObjectsPool;

#include "OrionBuildingManager.generated.h"

UENUM()
enum class EOrionStructure : uint8
{
	None,
	BasicSquareFoundation,
	BasicTriangleFoundation,
	Wall,
	DoubleWall,
	BasicRoof,
	InclinedRoof,
};

USTRUCT(BlueprintType)
struct FOrionDataBuilding
{
	GENERATED_BODY()

	int32 BuildingId;

	FName BuildingDisplayName;

	FString BuildingImageReference;

	FString BuildingBlueprintReference;

	EOrionStructure BuildingPlacingRule = EOrionStructure::None;
};

// DataTable 行结构（用于编辑器配置）
USTRUCT(BlueprintType)
struct FOrionDataBuildingRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Data")
	int32 BuildingId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Data")
	FName BuildingDisplayName;

	// 使用 TSoftObjectPtr 以便在编辑器中直接选择图片资产
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Data", meta = (AllowedClasses = "Texture2D"))
	TSoftObjectPtr<UTexture2D> BuildingImageReference;

	// 使用 TSoftClassPtr 以便在编辑器中直接选择蓝图类资产
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Data", meta = (AllowedClasses = "Actor"))
	TSoftClassPtr<AActor> BuildingBlueprintReference;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building Data")
	EOrionStructure BuildingPlacingRule = EOrionStructure::None;

	// 转换为 FOrionDataBuilding（将软引用转换为字符串路径）
	FOrionDataBuilding ToDataBuilding() const
	{
		FOrionDataBuilding Result;
		Result.BuildingId = BuildingId;
		Result.BuildingDisplayName = BuildingDisplayName;
		// 将软引用转换为字符串路径
		Result.BuildingImageReference = BuildingImageReference.ToSoftObjectPath().ToString();
		Result.BuildingBlueprintReference = BuildingBlueprintReference.ToSoftObjectPath().ToString();
		Result.BuildingPlacingRule = BuildingPlacingRule;
		return Result;
	}
};


USTRUCT()
struct FOrionGlobalSocket
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Location;

	UPROPERTY()
	FRotator Rotation;

	UPROPERTY()
	FVector Scale;

	UPROPERTY()
	EOrionStructure Kind;

	UPROPERTY()
	bool bOccupied = false;

	UPROPERTY()
	TWeakObjectPtr<AActor> Owner;

	FOrionGlobalSocket() = default;

	FOrionGlobalSocket(const FVector& InLoc,
	                   const FRotator& InRot,
	                   const EOrionStructure InKind,
	                   const bool bInOccupied,
	                   AActor* InOwner, const FVector& Scale = FVector(1.0f, 1.0f, 1.0f))
		: Location(InLoc)
		  , Rotation(InRot)
		  , Scale(Scale)
		  , Kind(InKind)
		  , bOccupied(bInOccupied)
		  , Owner(InOwner)
	{
	}
};

	// [UHT Compatible] Struct wrapper: UHT does not support TMap Value directly being TArray
USTRUCT()
struct FOrionSocketBucket
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FOrionGlobalSocket> Sockets;
};

/**
 * 
 */
UCLASS()
class ORION_API UOrionBuildingManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UOrionBuildingManager();

	// 数据获取方法：优先使用 DataTable，如果未指定则使用硬编码数据
	const TArray<FOrionDataBuilding>& GetOrionDataBuildings() const;
	const TMap<int32, FOrionDataBuilding>& GetOrionDataBuildingsMap() const;

	// 向后兼容：保留静态访问方式（已废弃，建议使用 GetOrionDataBuildings）
	UE_DEPRECATED(5.0, "Use GetOrionDataBuildings() instead")
	static const TArray<FOrionDataBuilding> OrionDataBuildings;
	
	UE_DEPRECATED(5.0, "Use GetOrionDataBuildingsMap() instead")
	static const TMap<int32, FOrionDataBuilding> OrionDataBuildingsMap;

	static const TMap<EOrionStructure, FVector> StructureOriginalScaleMap;


	// [Deprecated] Old system removed, replaced by spatial grid
	// TArray<FOrionGlobalSocket> SocketsRaw;
	// TArray<FOrionGlobalSocket> SocketsUnique;


	/* Save & Load */

	/* ========== ① Collect all buildings in the current world ========== */
	template <typename TStructureIterator = AOrionStructure>
	void CollectStructureRecords(TArray<FOrionStructureRecord>& Out) const
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}

		for (TActorIterator<TStructureIterator> It(World); It; ++It)
		{
			const AActor* Act = *It;
			FString Path = Act->GetClass()->GetPathName();
			Out.Emplace(Path, Act->GetActorTransform());
		}
	}

	/* ========== ② Completely reset Socket pool (called before loading) ========== */
	void ResetAllSockets(const UWorld* World);
	bool DelaySpawnNewStructure(TSubclassOf<AActor> BPClass, UWorld* World, const FTransform& TargetTransform,
	                                   bool& DelaySpawnNewStructureRes, bool bSnapped = false, AActor* ParentActor = nullptr);

	static constexpr float MergeTolSqr = 2.f * 2.f;

	bool ConfirmPlaceStructure(
		TSubclassOf<AActor> BPClass,
		AActor* PreviewPtr,  // Read-only: Only used to get Transform, not destroyed
		bool bSnapped,
		const FTransform& SnapTransform,
		AActor* ParentActor = nullptr);


	const TArray<FOrionGlobalSocket>& GetSnapSocketsByKind(const EOrionStructure Kind) const;

	// [Core] Register socket (Directly into storage, using new grid system)
	void RegisterSocket(const FVector& Loc,
	                    const FRotator& Rot,
	                    const EOrionStructure Kind,
	                    bool IsOccupied,
	                    const UWorld* World,
	                    AActor* Owner, const FVector& Scale = FVector(1.0f, 1.0f, 1.0f));

	// [Core] Unregister socket (Remove by Owner)
	void UnregisterSockets(AActor* Owner);

	// [Query] Core query + Filter logic
	// Returns whether found, and fills OutSocket with the best socket
	bool FindNearestSocket(const FVector& QueryPos, float SearchRadius, EOrionStructure Type, FOrionGlobalSocket& OutSocket) const;

	// [New] Get all physically touching building components around specified actor (No connection established, returns list only)
	TArray<UOrionStructureComponent*> GetConnectedNeighbors(AActor* CenterStructure, bool bDebug = false) const;

	TMap<EOrionStructure, TArray<FOrionGlobalSocket>> SocketsByKind;

	bool BEnableDebugLine = true;

	TUniquePtr<FBuildingObjectsPool> BuildingObjectsPool;

private:
	// [Grid] Grid size (e.g. 500cm)
	static constexpr float GridCellSize = 500.f;
	
	// [Grid] Core storage: Spatial Hash
	// Key: Compressed Coordinate Hash
	// Value: All sockets in this cell (No deduplication, no overwriting)
	// [UHT Compatible] Use FOrionSocketBucket wrapper for TArray, as UHT doesn't support nested containers
	UPROPERTY()
	TMap<int64, FOrionSocketBucket> SpatialGrid;

	// [Grid] Calculate Key
	static int64 GetGridKey(const FVector& Location);

	void OnWorldInitializedActors(const FActorsInitializedParams& ActorsInitializedParams);
	virtual void Initialize(FSubsystemCollectionBase&) override;

	// 数据加载方法
	void LoadBuildingDataFromDataTable();
	const TArray<FOrionDataBuilding>& GetHardcodedBuildings() const;

	// DataTable 引用（从 GameInstance 获取）
	UPROPERTY()
	TObjectPtr<UDataTable> BuildingDataTable = nullptr;

	// 缓存的数据（运行时使用）
	mutable TArray<FOrionDataBuilding> CachedBuildingData;
	mutable TMap<int32, FOrionDataBuilding> CachedBuildingDataMap;
	mutable bool bDataLoaded = false;

	// virtual void Deinitialize() override;

};

class FBuildingObjectsPool
{
private:
	TWeakObjectPtr<UOrionBuildingManager> Owner;

	TMap<int32, AActor*> Pool;

	UOrionBuildingManager* GetOwner() const { return Owner.Get(); }

public:
	FBuildingObjectsPool(const FBuildingObjectsPool&) = delete;
	FBuildingObjectsPool& operator=(const FBuildingObjectsPool&) = delete;

	explicit FBuildingObjectsPool(UOrionBuildingManager* InOwner)
	{
		Owner = InOwner;
		InitPreviewStructures(InOwner->GetWorld());
	}

	~FBuildingObjectsPool()
	{
		Pool.Empty();
	}

	void InitPreviewStructures(UWorld* World)
	{
		if (!World || !GetOwner()) return;

		for (auto& Each : GetOwner()->GetOrionDataBuildingsMap())
		{
			const int32 BuildingId = Each.Value.BuildingId;

			if (Pool.Contains(BuildingId)) continue;

			// 最好在数据资产里直接存 TSubclassOf<AActor>，而不是存字符串路径来 LoadClass
			// LoadClass 是阻塞加载，如果资产多会卡顿
			UClass* StructureBPSubclass = LoadClass<AActor>(nullptr, *Each.Value.BuildingBlueprintReference);

			if (!StructureBPSubclass)
			{
				UE_LOG(LogTemp, Warning, TEXT("FBuildingObjectsPool::InitPreviewStructures: Failed to load class for ID: %d"), BuildingId);
				continue;
			}

			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AActor* PreviewActor = World->SpawnActor<AActor>(StructureBPSubclass, FVector::ZeroVector, FRotator::ZeroRotator, Params);

			if (PreviewActor)
			{
				DeactivateActor(PreviewActor);

				// 改名为 Preview_XX 方便在 Outliner 调试
				PreviewActor->SetActorLabel(FString::Printf(TEXT("Preview_%d"), BuildingId));

				Pool.Add(BuildingId, PreviewActor);
			}
		}
	}

	AActor* AcquirePreviewActor(int32 BuildingId)
	{
		if (AActor** FoundActorPtr = Pool.Find(BuildingId))
		{
			if (AActor* Actor = *FoundActorPtr; IsValid(Actor))
			{
				Actor->SetActorHiddenInGame(false);
				Actor->SetActorEnableCollision(false); 
				Actor->SetActorTickEnabled(false);

				return Actor;
			}
		}
		return nullptr;
	}

	void ReleasePreviewActor(const int32 BuildingId)
	{
		if (AActor** FoundActorPtr = Pool.Find(BuildingId))
		{
			DeactivateActor(*FoundActorPtr);
		}
	}

	void ReleaseAll()
	{
		for (const auto& Pair : Pool)
		{
			DeactivateActor(Pair.Value);
		}
	}

private:
	static void DeactivateActor(AActor* Actor)
	{
		if (IsValid(Actor))
		{
			Actor->SetActorHiddenInGame(true); 
			Actor->SetActorEnableCollision(false);
			Actor->SetActorTickEnabled(false);

			Actor->SetActorLocation(FVector(0, 0, -10000));
		}
	}
};