// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Orion/OrionSaveGame/OrionSaveGame.h"
#include "OrionBuildingManager.generated.h"

class AOrionStructure;

USTRUCT()
struct FFoundationSocket
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsOccupied = false;

	UPROPERTY()
	FVector SocketLocation;

	UPROPERTY()
	FRotator SocketRotation;

	FFoundationSocket()
		: bIsOccupied(false)
		  , SocketLocation(FVector::ZeroVector)
		  , SocketRotation(FRotator::ZeroRotator)
	{
	}

	FFoundationSocket(
		bool InbIsOccupied,
		FVector InSocketLocation,
		FRotator InSocketRotator)
		: bIsOccupied(InbIsOccupied)
		  , SocketLocation(InSocketLocation)
		  , SocketRotation(InSocketRotator)
	{
	}
};

USTRUCT()
struct FWallSocket
{
	GENERATED_BODY()

	UPROPERTY()
	bool bIsOccupied = false;

	UPROPERTY()
	FVector SocketLocation;

	UPROPERTY()
	FRotator SocketRotation;

	FWallSocket()
		: bIsOccupied(false)
		  , SocketLocation(FVector::ZeroVector)
		  , SocketRotation(FRotator::ZeroRotator)
	{
	}

	FWallSocket(
		bool InbIsOccupied,
		FVector InSocketLocation,
		FRotator InSocketRotator)
		: bIsOccupied(InbIsOccupied)
		  , SocketLocation(InSocketLocation)
		  , SocketRotation(InSocketRotator)
	{
	}
};

UENUM()
enum class EOrionStructure : uint8
{
	Foundation,
	Wall
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
	                   AActor* InOwner)
		: Location(InLoc)
		  , Rotation(InRot)
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
	UPROPERTY()
	TArray<FOrionGlobalSocket> SocketsRaw;

	/* 参考池：不允许重复，用于吸附 & Debug，随时由 Raw 重建 */
	UPROPERTY()
	TArray<FOrionGlobalSocket> SocketsUnique;


	/* Save & Load */

	/* ========== ① 收集当前世界里的所有建筑 ========== */
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

	/* ========== ② 彻底重置 Socket 池（读档前调用） ========== */
	void ResetAllSockets(const UWorld* World)
	{
		SocketsRaw.Empty();
		SocketsUnique.Empty();
		UKismetSystemLibrary::FlushPersistentDebugLines(World);
	}

	static constexpr float MergeTolSqr = 5.f * 5.f;

	virtual void Initialize(FSubsystemCollectionBase&) override
	{
	}

	virtual void Deinitialize() override
	{
	}

	template <typename TOrionStructure>
	static void ConfirmPlaceStructure(
		TSubclassOf<TOrionStructure> BPOrionStructure,
		TOrionStructure*& PreviewStructurePtr,
		bool& bStructureSnapped,
		const FTransform& SpawnTransform)
	{
		if (!PreviewStructurePtr)
		{
			return;
		}

		if (PreviewStructurePtr->bForceSnapOnGrid && !bStructureSnapped)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("[Building] Structure must snap to socket before placement."));
			return;
		}

		UWorld* World = PreviewStructurePtr->GetWorld();
		if (!World)
		{
			return;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (bStructureSnapped)
		{
			World->SpawnActor<TOrionStructure>(
				BPOrionStructure, SpawnTransform, SpawnParams);
		}
		else
		{
			World->SpawnActor<TOrionStructure>(
				BPOrionStructure,
				PreviewStructurePtr->GetActorLocation(),
				PreviewStructurePtr->GetActorRotation(),
				SpawnParams);
		}

		PreviewStructurePtr->Destroy();
		PreviewStructurePtr = nullptr;
		bStructureSnapped = false;
	}

	const TArray<FOrionGlobalSocket>& GetSnapSockets() const { return SocketsUnique; }

	bool IsSocketFree(const FVector& Loc, EOrionStructure Kind) const
	{
		for (const FOrionGlobalSocket& S : SocketsUnique)
		{
			if (S.Kind == Kind && !S.bOccupied &&
				FVector::DistSquared(S.Location, Loc) < MergeTolSqr)
			{
				return true;
			}
		}
		return false;
	}

	void RegisterSocket(const FVector& Loc,
	                    const FRotator& Rot,
	                    EOrionStructure Kind,
	                    bool bOccupied,
	                    const UWorld* World,
	                    AActor* Owner)
	{
		if (!World)
		{
			return;
		}

		SocketsRaw.Emplace(Loc, Rot, Kind, bOccupied, Owner);
		RebuildUnique();
		RefreshDebug(World);
	}

	template <typename TStruct>
	void RemoveSocketRegistration(TStruct& Ref)
	{
		const UWorld* World = Ref.GetWorld();

		for (int32 i = SocketsRaw.Num() - 1; i >= 0; --i)
		{
			if (SocketsRaw[i].Owner.Get() == &Ref)
			{
				SocketsRaw.RemoveAt(i, 1, EAllowShrinking::No);
			}
		}

		RebuildUnique();
		if (World) { RefreshDebug(World); }
	}

private:
	void RebuildUnique()
	{
		SocketsUnique.Empty();

		for (const FOrionGlobalSocket& Src : SocketsRaw)
		{
			// 查是否已存在“同位置 + 同Kind”的条目
			int32 FoundIdx = INDEX_NONE;
			for (int32 i = 0; i < SocketsUnique.Num(); ++i)
			{
				if (SocketsUnique[i].Kind == Src.Kind &&
					FVector::DistSquared(SocketsUnique[i].Location, Src.Location) < MergeTolSqr)
				{
					FoundIdx = i;
					break;
				}
			}

			if (FoundIdx == INDEX_NONE)
			{
				SocketsUnique.Add(Src);
			}
			else
			{
				// 同一点已有条目：若现有为空闲而新条目为占用，则替换
				if (!SocketsUnique[FoundIdx].bOccupied && Src.bOccupied)
				{
					SocketsUnique[FoundIdx] = Src;
				}
			}
		}
	}

	void RefreshDebug(const UWorld* World)
	{
		UKismetSystemLibrary::FlushPersistentDebugLines(World);

		for (const FOrionGlobalSocket& S : SocketsUnique)
		{
			constexpr float AxisLen = 40.f;
			DrawDebugCoordinateSystem(
				World, S.Location, S.Rotation,
				AxisLen, /*bPersist=*/true, -1.f, 0,
				S.bOccupied ? 5.f : 2.f);
		}
	}
};
