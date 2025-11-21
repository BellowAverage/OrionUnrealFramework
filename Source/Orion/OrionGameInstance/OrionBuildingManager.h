// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Orion/OrionSaveGame/OrionSaveGame.h"


class AOrionStructure;
class UOrionStructureComponent;

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

/**
 * 
 */
UCLASS()
class ORION_API UOrionBuildingManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UOrionBuildingManager();

	static const TArray<FOrionDataBuilding> OrionDataBuildings;
	static const TMap<int32, FOrionDataBuilding> OrionDataBuildingsMap;

	static const TMap<EOrionStructure, FVector> StructureOriginalScaleMap;


	UPROPERTY()
	TArray<FOrionGlobalSocket> SocketsRaw;

	/* Reference pool: no duplicates allowed, used for snapping & Debug, rebuilt from Raw at any time */
	UPROPERTY()
	TArray<FOrionGlobalSocket> SocketsUnique;


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

	/*void UOrionBuildingManager::CollectStructureRecords(
		TArray<FOrionStructureRecord>& OutRecords) const
	{
		OutRecords.Empty();

		for (TActorIterator<AOrionStructure> It(GetWorld()); It; ++It)
		{
			AOrionStructure* Struct = *It;
			if (!Struct)
			{
				continue;
			}

			FOrionStructureRecord Rec;

			/* 路径要带 “_C” 才能 LoadClass #1#
			Rec.ClassPath = Struct->GetClass()->GetPathName();

			Rec.Transform = Struct->GetActorTransform();

			OutRecords.Add(MoveTemp(Rec));
		}
	}*/

	/* ========== ② Completely reset Socket pool (called before loading) ========== */
	void ResetAllSockets(const UWorld* World);
	static bool DelaySpawnNewStructure(TSubclassOf<AActor> BPClass, UWorld* World, FTransform TargetTransform,
	                                   bool& bValue);

	static constexpr float MergeTolSqr = 5.f * 5.f;

	/*virtual void Initialize(FSubsystemCollectionBase&) override
	{
	}

	virtual void Deinitialize() override
	{
	}*/

	static bool ConfirmPlaceStructure(
		TSubclassOf<AActor> BPClass,
		AActor*& PreviewPtr,
		bool& bSnapped,
		const FTransform& SnapTransform);


	TMap<EOrionStructure, TArray<FOrionGlobalSocket>> SocketsByKind;

	const TArray<FOrionGlobalSocket>& GetSnapSocketsByKind(const EOrionStructure Kind) const;

	/* Deprecated Due to Low Performance */
	//const TArray<FOrionGlobalSocket>& GetSnapSockets() const { return SocketsUnique; }

	bool IsSocketFree(const FVector& Loc, const EOrionStructure Kind) const;

	void RegisterSocket(const FVector& Loc,
	                    const FRotator& Rot,
	                    const EOrionStructure Kind,
	                    bool bOccupied,
	                    const UWorld* World,
	                    AActor* Owner, const FVector& Scale = FVector(1.0f, 1.0f, 1.0f));


	void RebuildUniqueForKind(EOrionStructure Kind);

	void RemoveSocketRegistration(const AActor& Ref);

	void AddUniqueSocket(const FOrionGlobalSocket& New);

	void RemoveUniqueSocket(const FOrionGlobalSocket& Old);

	/* Deprecated Due to Low Performance */
	void RebuildUnique();

	bool BEnableDebugLine = true;

	void RefreshDebug(const UWorld* World);
};
